#include "restoreservice.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <cstdio>
#include <unistd.h>

RestoreService::RestoreService(QObject *parent) : QObject(parent)
{
    connect(&_parser, &QWebdavDirParser::finished, this, &RestoreService::onGotFilesList);
    connect(&_parser, &QWebdavDirParser::errorChanged, this, &RestoreService::error);
    connect(this, &RestoreService::error, [&](){ setStage(Error); });
}

void RestoreService::restore(QString profile, QString backupFile)
{
    if (profile.isEmpty() || backupFile.isEmpty() || _stage == Extracting) return;

    if (_settings) {
        delete _settings;
    }

    _settings = new QSettings("/etc/backupapp/" + profile, QSettings::IniFormat);

    bool isWebdav = _settings->value("destinationType", "") == "webdav";
    if (!isWebdav) {
        emit error(tr("Only restore from webdav is currently supported!"));
        return;
    }

    if (!validateWebDavSettings() || !validateSourceSettings()) {
        return;
    }

    if (_settings->value("sourceType", "") != "directory") {
        emit error(tr("Only directory type backups are supported!"));
        return;
    }

    setupWebdav();

    _fullBackup = _settings->value("fullSystemBackup", false).toBool();
    _backupFile = QDir::cleanPath("/" + _settings->value("webdavPath").toString()) + "/" + backupFile;

    setStage(Extracting);

    if (_fullBackup) {
        restoreFullBackup();
    } else {
        restorePartialBackup();
    }
}

void RestoreService::getBackups(QString profile)
{
    if (_settings) {
        delete _settings;
    }

    _settings = new QSettings("/etc/backupapp/" + profile, QSettings::IniFormat);

    bool isWebdav = _settings->value("destinationType", "") == "webdav";
    if (!isWebdav) {
        emit error(tr("Only restore from webdav is currently supported!"));
        return;
    }

    if (!validateWebDavSettings() || !validateSourceSettings()) {
        return;
    }

    setupWebdav();

    const QString backupDir = QDir::cleanPath("/" + _settings->value("webdavPath").toString()) + "/";

    _parser.listDirectory(_webdav, backupDir, 1);
}

float RestoreService::getRestoreProgress() const
{
    return _restoreProgress;
}

RestoreService::Stage RestoreService::getStage() const
{
    return _stage;
}

void RestoreService::setStage(Stage stage)
{
    _stage = stage;

    emit stageChanged(_stage);
}

void RestoreService::onGotFilesList()
{
    qDebug();

    QList<QWebdavItem> files = _parser.getList();
    QStringList backupFiles;

    for (QWebdavItem &file : files) {
        if (file.name().startsWith(getBackupPrefix()) && file.name().endsWith(".tar.gz")) {
            qDebug() << file.name();
            backupFiles += file.name();
        }
    }

    backupFiles.sort();
    std::reverse(backupFiles.begin(), backupFiles.end());

    emit gotBackupFilesList(backupFiles);
}

void RestoreService::onRestoreFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    const bool success = exitStatus == QProcess::NormalExit && exitCode == 0;

    if (!success) {
        emit error(tr("tar exited with code %1").arg(exitCode));

        if (!_backupFileReply) return;

        disconnect(_backupFileReply, nullptr, this, nullptr);
        _backupFileReply->abort();
        _backupFileReply->deleteLater();
        _backupFileReply = nullptr;

        return;
    } else {
        if (_fullBackup)
            activateFullRestore();

        if (_stage != Error && _stage != RebootRequired)
            setStage(Finished);
    }
}

void RestoreService::onTarError(QProcess::ProcessError error)
{
    if (error != QProcess::ProcessError::FailedToStart) return;

    qDebug() << error;

    emit this->error(_restoreProcess->errorString());

    if (!_backupFileReply) return;

    disconnect(_backupFileReply, nullptr, this, nullptr);
    _backupFileReply->abort();
    _backupFileReply->deleteLater();
    _backupFileReply = nullptr;
}

bool RestoreService::validateWebDavSettings()
{
    bool result = _settings->contains("webdavHost") &&
           _settings->contains("webdavType") &&
           _settings->contains("webdavUser") &&
           _settings->contains("webdavPassword") &&
           _settings->contains("webdavUserId") &&
           _settings->contains("webdavPath");

    if (!result) {
        emit error(tr("Invalid profile configuration"));
    }

    return result;
}

bool RestoreService::validateSourceSettings()
{
    QString sourceType = _settings->value("sourceType", "").toString();

    if (sourceType == "directory" && !_settings->contains("directory")) {
        emit error("Missing directory settings");
        return false;
    }

    if (sourceType == "block" && !_settings->contains("block")) {
        emit error("Missing block settings");
        return false;
    }

    return true;
}

QString RestoreService::getBackupPrefix()
{
    QString slug = _settings->value("name").toString().toLower();

    slug.replace(QLatin1Char(' '), QLatin1Char('_'));
    slug.remove(QRegularExpression(QStringLiteral("[^a-z0-9_]")));

    if (slug.isEmpty())
        slug = QStringLiteral("profile");

    return "backup-" + slug + "-";
}

void RestoreService::setupWebdav()
{
    const QString scheme = _settings->value("webdavType").toString().toLower();
    const QString host = _settings->value("webdavHost").toString();
    const QString user = _settings->value("webdavUser").toString();
    const QString password = _settings->value("webdavPassword").toString();
    const QString userId = _settings->value("webdavUserId").toString();

    QString davRoot = _settings->value("webdavRoot").toString();

    if (davRoot.isEmpty())
        davRoot = "/remote.php/dav";

    if (!davRoot.startsWith("/"))
        davRoot.prepend("/");

    if (davRoot.endsWith("/"))
        davRoot.chop(1);

    const QString filesRoot = davRoot + "/files/" + userId;

    if (_webdav) {
        _webdav->deleteLater();
    }

    _webdav = new QWebdav(this);
    _webdav->setConnectionSettings(scheme == "https" ? QWebdav::HTTPS : QWebdav::HTTP, host, filesRoot, user, password);
}

void RestoreService::restorePartialBackup()
{
    if (_restoreProcess) {
        disconnect(_restoreProcess, nullptr, this, nullptr);
        _restoreProcess->kill();
        _restoreProcess->deleteLater();
    }

        _restoreProcess = new QProcess(this);
    _restoreProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    _restoreProcess->setProgram(QStringLiteral("tar"));
    _restoreProcess->setArguments({
        QStringLiteral("-xzpf"),
        QStringLiteral("-"),
        QStringLiteral("-C"),
        QStringLiteral("/"),
        QStringLiteral("--xattrs"),
        QStringLiteral("--acls"),
        QStringLiteral("--numeric-owner"),
        });

    connect(_restoreProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),this, &RestoreService::onRestoreFinished);
    connect(_restoreProcess, &QProcess::errorOccurred, this, &RestoreService::onTarError);
    connect(_restoreProcess, &QProcess::bytesWritten, this, &RestoreService::feedBackupData);

    _restoreProcess->start();

    if (_backupFileReply) {
        disconnect(_backupFileReply, nullptr, this, nullptr);
        _backupFileReply->abort();
        _backupFileReply->deleteLater();
    }

    _backupFileReply = _webdav->get(_backupFile);
    _backupFileReply->setReadBufferSize(4 * 1024 * 1024);
    _restoreTimer.start();

    connect(_backupFileReply, &QNetworkReply::readyRead, this, &RestoreService::feedBackupData);
    connect(_backupFileReply, &QNetworkReply::finished, this, &RestoreService::feedBackupData);
    connect(_backupFileReply, &QNetworkReply::downloadProgress, this, &RestoreService::restoreProgress);
}

void RestoreService::restoreFullBackup()
{
    const QString targetDir = QStringLiteral("/backup");

    if (!QDir(targetDir).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot).isEmpty()) {
        emit error(tr("%1 is not empty, remove it before restoring").arg(targetDir));
        return;
    }

    if (!QDir().mkpath(targetDir)) {
        emit error(tr("Could not create %1").arg(targetDir));
        return;
    }

    if (_restoreProcess) {
        disconnect(_restoreProcess, nullptr, this, nullptr);
        _restoreProcess->kill();
        _restoreProcess->deleteLater();
    }

    _restoreProcess = new QProcess(this);
    _restoreProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    _restoreProcess->setProgram(QStringLiteral("tar"));
    _restoreProcess->setArguments({
        QStringLiteral("-xzpf"),
        QStringLiteral("-"),
        QStringLiteral("-C"),
        targetDir,
        QStringLiteral("--xattrs"),
        QStringLiteral("--acls"),
        QStringLiteral("--numeric-owner"),
        });

    connect(_restoreProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),this, &RestoreService::onRestoreFinished);
    connect(_restoreProcess, &QProcess::errorOccurred, this, &RestoreService::onTarError);
    connect(_restoreProcess, &QProcess::bytesWritten, this, &RestoreService::feedBackupData);

    _restoreProcess->start();

    if (_backupFileReply) {
        disconnect(_backupFileReply, nullptr, this, nullptr);
        _backupFileReply->abort();
        _backupFileReply->deleteLater();
    }

    _backupFileReply = _webdav->get(_backupFile);
    _backupFileReply->setReadBufferSize(4 * 1024 * 1024);
    _restoreTimer.start();

    connect(_backupFileReply, &QNetworkReply::readyRead, this, &RestoreService::feedBackupData);
    connect(_backupFileReply, &QNetworkReply::finished, this, &RestoreService::feedBackupData);
    connect(_backupFileReply, &QNetworkReply::downloadProgress, this, &RestoreService::restoreProgress);
}

void RestoreService::feedBackupData()
{
    if (_backupFileReply->isFinished() && _backupFileReply->error() != QNetworkReply::NoError) {
        disconnect(_restoreProcess, nullptr, this, nullptr);
        _restoreProcess->kill();
        _restoreProcess->deleteLater();
        _restoreProcess = nullptr;

        emit error(_backupFileReply->errorString());
        return;
    }

    while (_backupFileReply->bytesAvailable() > 0) {
        const qint64 room = 4 * 1024 * 1024 - _restoreProcess->bytesToWrite();

        if (room <= 0)
            return;

        _restoreProcess->write(_backupFileReply->read(qMin(room, _backupFileReply->bytesAvailable())));
    }

    if (_backupFileReply->isFinished() && _restoreProcess->bytesToWrite() == 0)
        _restoreProcess->closeWriteChannel();
}

void RestoreService::restoreProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    _restoreProgress = (float)bytesReceived / bytesTotal * 100;

    qint64 etaMs = -1;

    if (bytesReceived > 0 && bytesTotal > 0)
        etaMs = _restoreTimer.elapsed() * (bytesTotal - bytesReceived) / bytesReceived;

    emit restoreEtaChanged(etaMs);
    emit restoreProgressChanged(_restoreProgress);
}

void RestoreService::activateFullRestore()
{
    const QString restoredRoot = QStringLiteral("/backup");
    const QString swapJob = QStringLiteral("backupapp-swap-rootfs.sh");

    QDir().mkpath(restoredRoot + "/etc/oneshot.d/preinit");

    const QFileInfoList updates = QDir(restoredRoot + "/var/lib/platform-updates").entryInfoList(QDir::Files);

    for (const QFileInfo &update : updates) {
        if (!(update.permissions() & QFile::ExeOwner))
            continue;

        const QString link = restoredRoot + "/etc/oneshot.d/preinit/" + update.fileName();

        QFile::remove(link);

        if (!QFile::link("/var/lib/platform-updates/" + update.fileName(), link)) {
            emit error(tr("Could not queue %1").arg(update.fileName()));
            return;
        }
    }

    QDir().mkpath(QStringLiteral("/etc/oneshot.d/preinit"));

    const QString jobLink = "/etc/oneshot.d/preinit/" + swapJob;

    QFile::remove(jobLink);

    if (!QFile::link("/usr/lib/oneshot.d/" + swapJob, jobLink)) {
        emit error(tr("Could not queue %1").arg(swapJob));
        return;
    }

    ::sync();

    setStage(RebootRequired);
}
