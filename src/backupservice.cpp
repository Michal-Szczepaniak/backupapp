#include "backupservice.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>
#include <csignal>

BackupService::BackupService(QObject *parent) : QObject(parent)
{
}

qint64 BackupService::directorySize(const QString &path) const
{
    qint64 total = 0;

    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }

    return total;
}

qint64 BackupService::blockDeviceSize(const QString &path) const
{
    QProcess process;
    process.start(QStringLiteral("blockdev"), {QStringLiteral("--getsize64"), path});
    process.waitForFinished();

    return process.readAllStandardOutput().trimmed().toLongLong();
}

qint64 BackupService::processBytesRead(qint64 pid) const
{
    QFile file(QStringLiteral("/proc/%1/io").arg(pid));

    if (!file.open(QIODevice::ReadOnly))
        return -1;

    const QList<QByteArray> lines = file.readAll().split('\n');

    for (const QByteArray &line : lines) {
        if (line.startsWith("rchar:")) {
            return line.mid(6).trimmed().toLongLong();
        }
    }

    return -1;
}

void BackupService::buildSourceStream()
{
    setStage(Preparing);

    QString sourceType = _settings->value("sourceType", "").toString();

    if (sourceType == "directory") {
        QStringList directories = _settings->value("directory", "").toStringList();

        _totalBytes = 0;
        _bytesRead = 0;
        _lastEtaMs = -1;

        _sizeProcess = new QProcess(this);
        _sizeProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
        _sizeProcess->setProgram(QStringLiteral("du"));
        _sizeProcess->setArguments(QStringList{QStringLiteral("-scb")} + directories);

        connect(_sizeProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                this, &BackupService::onSizeFinished);

        connect(_sizeProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart)
                finishUpload(false, _sizeProcess->errorString());
        });

        _sizeProcess->start();

        return;
    } else if (sourceType == "block") {
        const QString devicePath = _settings->value("block").toString();

        _totalBytes = blockDeviceSize(devicePath);
        _bytesRead = 0;
        _lastEtaMs = -1;

        _readProcess = new QProcess(this);
        _readProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
        _readProcess->setProgram(QStringLiteral("gzip"));
        _readProcess->setArguments({
            QStringLiteral("-c"),
            devicePath
        });
    }

    startUpload();
}

QString BackupService::profileSlug() const
{
    QString slug = _settings->value("name").toString().toLower();

    slug.replace(QLatin1Char(' '), QLatin1Char('_'));
    slug.remove(QRegularExpression(QStringLiteral("[^a-z0-9_]")));

    if (slug.isEmpty())
        slug = QStringLiteral("profile");

    return slug;
}

QString BackupService::davPath(const QString &path) const
{
    QString davRoot = _settings->value("webdavRoot").toString();

    if (davRoot.isEmpty())
        davRoot = QStringLiteral("/remote.php/dav");

    return QDir::cleanPath("/" + davRoot + "/" + path);
}

QString BackupService::davUrl(const QString &path) const
{
    const QString scheme = _settings->value("webdavType").toString().toLower() == "https" ? QStringLiteral("https") : QStringLiteral("http");

    return scheme + "://" + _settings->value("webdavHost").toString() + davPath(path);
}

void BackupService::setupWebdav()
{
    const QWebdav::QWebdavConnectionType type = _settings->value("webdavType").toString().toLower() == "https" ? QWebdav::HTTPS : QWebdav::HTTP;
    const QString host = _settings->value("webdavHost").toString();
    const QString user = _settings->value("webdavUser").toString();
    const QString password = _settings->value("webdavPassword").toString();
    const QString filesRoot = davPath("/files/" + _settings->value("webdavUserId").toString());

    if (_webdav) {
        _webdav->deleteLater();
    }

    _webdav = new QWebdav(this);
    _webdav->setConnectionSettings(type, host, filesRoot, user, password);
}

void BackupService::onSizeFinished()
{
    const QList<QByteArray> lines = _sizeProcess->readAllStandardOutput().trimmed().split('\n');

    _totalBytes = lines.last().split('\t').first().toLongLong();

    _sizeProcess->deleteLater();
    _sizeProcess = nullptr;

    QStringList arguments = {
        QStringLiteral("-czf"),
        QStringLiteral("-"),
        QStringLiteral("--xattrs"), QStringLiteral("--acls"),
    };

    arguments += _settings->value("directory", "").toStringList();

    _readProcess = new QProcess(this);
    _readProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    _readProcess->setProgram(QStringLiteral("tar"));
    _readProcess->setArguments(arguments);

    startUpload();
}

void BackupService::startUpload()
{
    const QString userId = _settings->value("webdavUserId").toString();

    _backupPrefix = "backup-" + profileSlug() + "-";
    _backupDir = QDir::cleanPath("/" + _settings->value("webdavPath").toString());

    if (!_backupDir.endsWith("/"))
        _backupDir += "/";

    _backupExtension = _settings->value("sourceType").toString() == "block" ? QStringLiteral(".img.gz") : QStringLiteral(".tar.gz");

    const QString fileName = _backupPrefix + QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss") + _backupExtension;

    _uploadsBaseUrl = davUrl("/uploads/" + userId + "/backupapp-" + QString::number(QDateTime::currentMSecsSinceEpoch()));
    _destinationUrl = davUrl("/files/" + userId + _backupDir + fileName);

    _stagingDir = QStringLiteral("/tmp/backupapp-chunks");
    QDir().mkpath(_stagingDir);

    _nextChunkIndex = 0;
    _uploadedBytesTotal = 0;
    _splitFinished = false;
    _splitPaused = false;
    _chunkRetries = 0;
    _retryAfterMs = 0;

    setupWebdav();

    QNetworkRequest req;
    req.setUrl(QUrl(_uploadsBaseUrl));
    req.setRawHeader("Destination", _destinationUrl.toUtf8());

    qDebug() << "MKCOL" << req.url() << "Destination:" << _destinationUrl;

    _mkcolReply = _webdav->sendCustomRequest(req, "MKCOL");
    connect(_mkcolReply, &QNetworkReply::finished, this, &BackupService::onMkcolFinished);
}

void BackupService::onMkcolFinished()
{
    const bool success = _mkcolReply->error() == QNetworkReply::NoError;
    const QString errorString = _mkcolReply->errorString();
    const QVariant statusCode = _mkcolReply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    qDebug() << "MKCOL finished, status:" << statusCode << "error:" << errorString;

    _mkcolReply->deleteLater();
    _mkcolReply = nullptr;

    if (!success) {
        finishUpload(false, QStringLiteral("Could not create upload session: %1").arg(errorString));
        return;
    }

    startChunkedTransfer();
}

void BackupService::startChunkedTransfer()
{
    setStage(Uploading);

    _splitProcess = new QProcess(this);
    _splitProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    _splitProcess->setProgram(QStringLiteral("split"));
    _splitProcess->setArguments({
        QStringLiteral("-d"),
        QStringLiteral("-a"), QStringLiteral("4"),
        QStringLiteral("-b"), QStringLiteral("100M"),
        QStringLiteral("-"),
        _stagingDir + "/chunk-"
    });

    connect(_splitProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &BackupService::onSplitFinished);

    _readProcess->setStandardOutputProcess(_splitProcess);

    connect(_readProcess, &QProcess::started, this, &BackupService::onReadStarted);
    connect(_readProcess, &QProcess::errorOccurred, this, &BackupService::onTarError);

    _readProcess->start();
    _splitProcess->start();

    _chunkPollTimer = new QTimer(this);
    connect(_chunkPollTimer, &QTimer::timeout, this, &BackupService::tryUploadNextChunk);
    _chunkPollTimer->start(1000);
}

void BackupService::tryUploadNextChunk()
{
    const int stagedCount = QDir(_stagingDir).entryList(QStringList{QStringLiteral("chunk-*")}, QDir::Files).count();

    if (!_splitFinished && !_splitPaused && stagedCount >= 3) {
        ::kill(static_cast<pid_t>(_splitProcess->processId()), SIGSTOP);
        _splitPaused = true;
    } else if (_splitPaused && stagedCount <= 1) {
        ::kill(static_cast<pid_t>(_splitProcess->processId()), SIGCONT);
        _splitPaused = false;
    }

    if (_chunkUploadReply)
        return;

    if (QDateTime::currentMSecsSinceEpoch() < _retryAfterMs)
        return;

    const QString currentPath = _stagingDir + "/chunk-" + QString::number(_nextChunkIndex).rightJustified(4, '0');
    const QString nextPath = _stagingDir + "/chunk-" + QString::number(_nextChunkIndex + 1).rightJustified(4, '0');

    if (!QFile::exists(currentPath)) {
        if (_splitFinished) {
            _chunkPollTimer->stop();
            finishChunksAndAssemble();
        }

        return;
    }

    if (!QFile::exists(nextPath) && !_splitFinished)
        return;

    _chunkFile = new QFile(currentPath, this);
    _chunkFile->open(QIODevice::ReadOnly);

    QNetworkRequest req;
    req.setUrl(QUrl(_uploadsBaseUrl + "/" + QString::number(_nextChunkIndex + 1)));
    req.setRawHeader("Destination", _destinationUrl.toUtf8());
    req.setHeader(QNetworkRequest::ContentLengthHeader, _chunkFile->size());

    qDebug() << "PUT chunk" << _nextChunkIndex << req.url() << "size:" << _chunkFile->size();

    _chunkUploadReply = _webdav->QNetworkAccessManager::put(req, _chunkFile);
    connect(_chunkUploadReply, &QNetworkReply::finished, this, &BackupService::onChunkUploadFinished);
}

void BackupService::onChunkUploadFinished()
{
    const bool success = _chunkUploadReply->error() == QNetworkReply::NoError;
    const QString errorString = _chunkUploadReply->errorString();
    const QString uploadedPath = _chunkFile->fileName();
    const QVariant statusCode = _chunkUploadReply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    qDebug() << "chunk" << _nextChunkIndex << "finished, status:" << statusCode << "error:" << errorString;

    _chunkUploadReply->deleteLater();
    _chunkUploadReply = nullptr;

    _chunkFile->close();
    _chunkFile->deleteLater();
    _chunkFile = nullptr;

    if (!success) {
        if (_chunkRetries < 3) {
            _chunkRetries++;
            _retryAfterMs = QDateTime::currentMSecsSinceEpoch() + _chunkRetries * 10000;

            qDebug() << "retrying chunk" << _nextChunkIndex << "in" << _chunkRetries * 10 << "s, attempt" << _chunkRetries;
            return;
        }

        finishUpload(false, errorString);
        return;
    }

    _chunkRetries = 0;
    _uploadedBytesTotal += QFileInfo(uploadedPath).size();

    QFile::remove(uploadedPath);
    _nextChunkIndex++;

    tryUploadNextChunk();
}

void BackupService::finishChunksAndAssemble()
{
    if (_moveReply)
        return;

    setStage(Assembling);

    QNetworkRequest req;
    req.setUrl(QUrl(_uploadsBaseUrl + "/.file"));
    req.setRawHeader("Destination", _destinationUrl.toUtf8());
    req.setRawHeader("OC-Total-Length", QString::number(_uploadedBytesTotal).toUtf8());

    qDebug() << "MOVE" << req.url() << "Destination:" << _destinationUrl << "OC-Total-Length:" << _uploadedBytesTotal;

    _moveReply = _webdav->sendCustomRequest(req, "MOVE");
    connect(_moveReply, &QNetworkReply::finished, this, &BackupService::onMoveFinished);
}

void BackupService::onMoveFinished()
{
    const bool success = _moveReply->error() == QNetworkReply::NoError;
    const QString errorString = _moveReply->errorString();
    const QVariant statusCode = _moveReply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    qDebug() << "MOVE finished, status:" << statusCode << "error:" << errorString;

    _moveReply->deleteLater();
    _moveReply = nullptr;

    if (!success) {
        finishUpload(false, errorString);
        return;
    }

    pruneOldBackups();
}

void BackupService::pruneOldBackups()
{
    if (_settings->value("keepBackups", 0).toInt() <= 0) {
        finishUpload(true, QString());
        return;
    }

    setStage(Pruning);

    _dirParser = new QWebdavDirParser(this);
    connect(_dirParser, &QWebdavDirParser::finished, this, &BackupService::onBackupListFinished);
    connect(_dirParser, &QWebdavDirParser::errorChanged, this, [](const QString &error) { qDebug() << "listing backups failed:" << error; });

    qDebug() << "PROPFIND" << _backupDir;

    if (!_dirParser->listDirectory(_webdav, _backupDir, 1))
        finishUpload(true, QString());
}

void BackupService::onBackupListFinished()
{
    const int keep = _settings->value("keepBackups", 0).toInt();

    QStringList backups;

    for (const QWebdavItem &item : _dirParser->getList()) {
        if (!item.isDir() && item.name().startsWith(_backupPrefix) && item.name().endsWith(_backupExtension))
            backups << item.name();
    }

    _dirParser->deleteLater();
    _dirParser = nullptr;

    backups.sort();

    const QStringList toRemove = backups.mid(0, qMax(0, backups.size() - keep));

    if (toRemove.isEmpty()) {
        finishUpload(true, QString());
        return;
    }

    _pendingRemovals = toRemove.size();

    for (const QString &name : toRemove) {
        qDebug() << "DELETE" << _backupDir + name;

        QNetworkReply *reply = _webdav->remove(_backupDir + name);
        connect(reply, &QNetworkReply::finished, this, &BackupService::onOldBackupRemoved);
    }
}

void BackupService::onOldBackupRemoved()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());

    qDebug() << "DELETE finished, status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) << "error:" << reply->errorString();

    reply->deleteLater();

    if (--_pendingRemovals == 0)
        finishUpload(true, QString());
}

bool BackupService::validateWebDavSettings()
{
    return _settings->contains("webdavHost") &&
           _settings->contains("webdavType") &&
           _settings->contains("webdavUser") &&
           _settings->contains("webdavPassword") &&
           _settings->contains("webdavUserId") &&
           _settings->contains("webdavPath");
}

bool BackupService::validateSourceSettings()
{
    QString sourceType = _settings->value("sourceType", "").toString();

    if (sourceType == "directory" && !_settings->contains("directory")) {
        emit backupFinished(false, "Missing directory settings");
        return false;
    }

    if (sourceType == "block" && !_settings->contains("block")) {
        emit backupFinished(false, "Missing block settings");
        return false;
    }

    return true;
}

void BackupService::backup(QString profile)
{
    qDebug() << profile;

    if (_stage != Idle) {
        emit backupFinished(false, QStringLiteral("Backup already in progress"));
        return;
    }

    if (_settings) {
        delete _settings;
    }

    _settings = new QSettings("/etc/backupapp/" + profile, QSettings::IniFormat);

    QString sourceType = _settings->value("sourceType", "").toString();
    if (!QStringList{"directory", "block"}.contains(sourceType)) {
        emit backupFinished(false, "Invalid backup source type");
        return;
    }

    QString destinationType = _settings->value("destinationType", "").toString();
    if (!QStringList{"webdav"}.contains(destinationType)) {
        emit backupFinished(false, "Invalid backup destination type");
        return;
    }

    if (QStringList{"webdav"}.contains(destinationType)) {
        if (!validateWebDavSettings()) {
            emit backupFinished(false, "Missing webdav settings");
            return;
        }

        if (!validateSourceSettings()) {
            return;
        }

        buildSourceStream();
    }
}


BackupService::Stage BackupService::getStage() const
{
    return _stage;
}

void BackupService::setStage(Stage stage)
{
    if (_stage == stage)
        return;

    _stage = stage;

    emit stageChanged(_stage);
}

void BackupService::onReadStarted()
{
    _progressTimer = new QTimer(this);
    connect(_progressTimer, &QTimer::timeout, this, &BackupService::onProgressTick);
    _progressTimer->start(500);
}

void BackupService::onProgressTick()
{
    const qint64 rchar = processBytesRead(_readProcess->processId());
    if (rchar < 0)
        return;

    const qint64 lastBytesRead = _bytesRead;
    _bytesRead = qMin(rchar, _totalBytes);

    const qint64 delta = _bytesRead - lastBytesRead;
    const qint64 remaining = _totalBytes - _bytesRead;

    if (delta > 0)
        _lastEtaMs = remaining * 500 / delta;

    emit backupProgress(qMin(_bytesRead, _totalBytes * 99 / 100), _totalBytes, _lastEtaMs);
}

void BackupService::onSplitFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    const bool success = exitStatus == QProcess::NormalExit && exitCode == 0;

    if (!success) {
        finishUpload(false, QStringLiteral("split exited with code %1").arg(exitCode));
        return;
    }

    _splitFinished = true;
    tryUploadNextChunk();
}

void BackupService::finishUpload(bool success, const QString &error)
{
    setStage(Idle);

    if (_progressTimer) {
        _progressTimer->stop();
        _progressTimer->deleteLater();
        _progressTimer = nullptr;
    }

    if (_chunkPollTimer) {
        _chunkPollTimer->stop();
        _chunkPollTimer->deleteLater();
        _chunkPollTimer = nullptr;
    }

    if (_mkcolReply) {
        disconnect(_mkcolReply, nullptr, this, nullptr);
        _mkcolReply->abort();
        _mkcolReply->deleteLater();
        _mkcolReply = nullptr;
    }

    if (_chunkUploadReply) {
        disconnect(_chunkUploadReply, nullptr, this, nullptr);
        _chunkUploadReply->abort();
        _chunkUploadReply->deleteLater();
        _chunkUploadReply = nullptr;
    }

    if (_chunkFile) {
        _chunkFile->close();
        _chunkFile->deleteLater();
        _chunkFile = nullptr;
    }

    if (_moveReply) {
        disconnect(_moveReply, nullptr, this, nullptr);
        _moveReply->abort();
        _moveReply->deleteLater();
        _moveReply = nullptr;
    }

    if (_dirParser) {
        disconnect(_dirParser, nullptr, this, nullptr);
        _dirParser->abort();
        _dirParser->deleteLater();
        _dirParser = nullptr;
    }

    if (_splitProcess) {
        disconnect(_splitProcess, nullptr, this, nullptr);

        if (_splitPaused) {
            ::kill(static_cast<pid_t>(_splitProcess->processId()), SIGCONT);
            _splitPaused = false;
        }

        _splitProcess->kill();
        _splitProcess->waitForFinished(1000);
        _splitProcess->deleteLater();
        _splitProcess = nullptr;
    }

    if (_sizeProcess) {
        disconnect(_sizeProcess, nullptr, this, nullptr);

        _sizeProcess->kill();
        _sizeProcess->waitForFinished(1000);
        _sizeProcess->deleteLater();
        _sizeProcess = nullptr;
    }

    if (_readProcess) {
        disconnect(_readProcess, nullptr, this, nullptr);

        _readProcess->kill();
        _readProcess->waitForFinished(1000);
        _readProcess->deleteLater();
        _readProcess = nullptr;
    }

    if (_webdav) {
        if (success) {
            _webdav->deleteLater();
        } else {
            QNetworkRequest req;
            req.setUrl(QUrl(_uploadsBaseUrl + "/"));

            qDebug() << "DELETE upload session" << req.url();

            QWebdav *webdav = _webdav;
            QNetworkReply *reply = webdav->deleteResource(req);

            connect(reply, &QNetworkReply::finished, webdav, [webdav, reply]() {
                qDebug() << "DELETE upload session finished, status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

                reply->deleteLater();
                webdav->deleteLater();
            });
        }

        _webdav = nullptr;
    }

    QDir(_stagingDir).removeRecursively();

    emit backupFinished(success, error);
}

void BackupService::onTarError(QProcess::ProcessError error)
{
    qDebug() << error;

    finishUpload(false, _readProcess->errorString());
}
