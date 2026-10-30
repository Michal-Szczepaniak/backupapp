/*
    Copyright (C) 2026 Michał Szczepaniak

    This file is part of backupapp.

    backupapp is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    backupapp is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with backupapp.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "restoreservice.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStorageInfo>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/mount.h>
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

    const QString destinationType = _settings->value("destinationType", "").toString();

    if (destinationType == "rsync") {
        if (!validateRsyncSettings() || !validateSourceSettings()) {
            return;
        }

        if (_settings->value("sourceType", "") != "directory") {
            emit error(tr("Only directory type backups are supported!"));
            return;
        }

        _fullBackup = _settings->value("fullSystemBackup", false).toBool();

        setStage(Extracting);

        restoreRsyncBackup();
    } else if (destinationType == "webdav") {
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
    } else {
        emit error(tr("Only restore from webdav and rsync is currently supported!"));
    }
}

void RestoreService::getBackups(QString profile)
{
    if (_settings) {
        delete _settings;
    }

    _settings = new QSettings("/etc/backupapp/" + profile, QSettings::IniFormat);

    const QString destinationType = _settings->value("destinationType", "").toString();

    if (destinationType == "rsync") {
        if (!validateRsyncSettings() || !validateSourceSettings()) {
            return;
        }

        emit gotBackupFilesList({_settings->value("rsyncPath").toString()});
    } else if (destinationType == "webdav") {
        if (!validateWebDavSettings() || !validateSourceSettings()) {
            return;
        }

        setupWebdav();

        const QString backupDir = QDir::cleanPath("/" + _settings->value("webdavPath").toString()) + "/";

        _parser.listDirectory(_webdav, backupDir, 1);
    } else {
        emit error(tr("Only restore from webdav and rsync is currently supported!"));
    }
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
    const bool unstaged = unstageHome();

    if (!success) {
        emit error(tr("tar exited with code %1").arg(exitCode));

        if (!_backupFileReply) return;

        disconnect(_backupFileReply, nullptr, this, nullptr);
        _backupFileReply->abort();
        _backupFileReply->deleteLater();
        _backupFileReply = nullptr;

        return;
    } else {
        if (!unstaged) {
            emit error(tr("Could not unmount /backup/home, remove it before rebooting"));
            return;
        }

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

    unstageHome();

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

bool RestoreService::validateRsyncSettings()
{
    bool result = _settings->contains("rsyncPath");

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
    QStringList members;

    for (const QString &directory : _settings->value("directory").toStringList())
        members += QDir::cleanPath("/" + directory.trimmed()).mid(1);

    _restoreProcess->setArguments(QStringList{
        QStringLiteral("-xzpf"),
        QStringLiteral("-"),
        QStringLiteral("-C"),
        QStringLiteral("/"),
        QStringLiteral("--xattrs"),
        QStringLiteral("--acls"),
        QStringLiteral("--numeric-owner"),
        } + members);

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

    if (!stageHome())
        return;

    if (_restoreProcess) {
        disconnect(_restoreProcess, nullptr, this, nullptr);
        _restoreProcess->kill();
        _restoreProcess->deleteLater();
    }

    _restoreProcess = new QProcess(this);
    _restoreProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    _restoreProcess->setProgram(QStringLiteral("tar"));
    QStringList members;

    for (const QString &directory : _settings->value("directory").toStringList())
        members += QDir::cleanPath("/" + directory.trimmed()).mid(1);

    _restoreProcess->setArguments(QStringList{
        QStringLiteral("-xzpf"),
        QStringLiteral("-"),
        QStringLiteral("-C"),
        targetDir,
        QStringLiteral("--xattrs"),
        QStringLiteral("--acls"),
        QStringLiteral("--numeric-owner"),
        } + members);

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

        unstageHome();

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
    const QString fstab = QStringLiteral("/etc/fstab");

    QDir().mkpath(restoredRoot + "/etc/oneshot.d/preinit");

    if (QFileInfo::exists(fstab)) {
        QFile::remove(restoredRoot + fstab);

        if (!QFile::copy(fstab, restoredRoot + fstab)) {
            emit error(tr("Could not keep %1").arg(fstab));
            return;
        }
    }

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

    if (_homeStaged) {
        const QString homeScript = QStringLiteral("backupapp-swap-home.sh");
        const QString homeService = QStringLiteral("backupapp-swap-home.service");
        const QString restoredScript = restoredRoot + "/usr/libexec/" + homeScript;
        const QString restoredService = restoredRoot + "/etc/systemd/system/" + homeService;
        const QStringList wantedBy = {QStringLiteral("multi-user.target")};

        QDir().mkpath(restoredRoot + "/usr/libexec");
        QDir().mkpath(restoredRoot + "/etc/systemd/system");

        QFile::remove(restoredScript);
        QFile::remove(restoredService);

        bool queued = QFile::copy("/usr/share/backupapp/" + homeScript, restoredScript)
                && QFile::setPermissions(restoredScript, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther)
                && QFile::copy("/usr/share/backupapp/" + homeService, restoredService);

        for (const QString &target : wantedBy) {
            const QString wantsDir = restoredRoot + "/etc/systemd/system/" + target + ".wants";

            QDir().mkpath(wantsDir);
            QFile::remove(wantsDir + "/" + homeService);

            queued = queued && QFile::link("/etc/systemd/system/" + homeService, wantsDir + "/" + homeService);
        }

        if (!queued) {
            emit error(tr("Could not queue %1").arg(homeService));
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

void RestoreService::restoreRsyncBackup()
{
    QString targetDir = QStringLiteral("/");

    if (_fullBackup) {
        targetDir = QStringLiteral("/backup");

        if (!QDir(targetDir).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot).isEmpty()) {
            emit error(tr("%1 is not empty, remove it before restoring").arg(targetDir));
            return;
        }

        if (!QDir().mkpath(targetDir)) {
            emit error(tr("Could not create %1").arg(targetDir));
            return;
        }

        if (!stageHome())
            return;
    }

    QString port = _settings->value("rsyncPort").toString();

    if (port.isEmpty())
        port = QStringLiteral("22");

    const QString ssh = QStringLiteral("ssh -p %1 -i %2 -o BatchMode=yes").arg(port, _settings->value("rsyncKey").toString());
    const QString host = _settings->value("rsyncHost").toString();
    const QString user = _settings->value("rsyncUser").toString();

    QString sourcePrefix = _settings->value("rsyncPath").toString() + "/.";

    if (!host.isEmpty())
        sourcePrefix.prepend(host + ":");

    if (!host.isEmpty() && !user.isEmpty())
        sourcePrefix.prepend(user + "@");

    QStringList arguments = {
        QStringLiteral("-aAXHR"),
        QStringLiteral("--numeric-ids"),
        QStringLiteral("--info=progress2"),
        QStringLiteral("--no-inc-recursive"),
    };

    if (!host.isEmpty())
        arguments += {QStringLiteral("-e"), ssh};

    arguments += _settings->value("rsyncRestoreOptions").toString().split(QLatin1Char(' '), QString::SkipEmptyParts);

    for (const QString &directory : _settings->value("directory").toStringList())
        arguments += sourcePrefix + QDir::cleanPath("/" + directory.trimmed());

    arguments += targetDir;

    qDebug() << "rsync" << arguments;

    if (_restoreProcess) {
        disconnect(_restoreProcess, nullptr, this, nullptr);
        _restoreProcess->kill();
        _restoreProcess->deleteLater();
    }

    _rsyncOutput.clear();

    _restoreProcess = new QProcess(this);
    _restoreProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    _restoreProcess->setProgram(QStringLiteral("rsync"));
    _restoreProcess->setArguments(arguments);

    connect(_restoreProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),this, &RestoreService::onRestoreFinished);
    connect(_restoreProcess, &QProcess::errorOccurred, this, &RestoreService::onTarError);
    connect(_restoreProcess, &QProcess::readyReadStandardOutput, this, &RestoreService::onRsyncOutput);

    _restoreProcess->start();
}

void RestoreService::onRsyncOutput()
{
    _rsyncOutput += _restoreProcess->readAllStandardOutput();

    const int lineEnd = qMax(_rsyncOutput.lastIndexOf('\r'), _rsyncOutput.lastIndexOf('\n'));

    if (lineEnd < 0)
        return;

    const QString lines = QString::fromUtf8(_rsyncOutput.left(lineEnd));
    _rsyncOutput.remove(0, lineEnd + 1);

    static const QRegularExpression progressPattern(QStringLiteral("(\\d+)%\\s+\\S+\\s+(\\d+):(\\d+):(\\d+)(\\s+\\(xfr#)?"));

    QRegularExpressionMatchIterator it = progressPattern.globalMatch(lines);
    QRegularExpressionMatch match;

    while (it.hasNext()) {
        match = it.next();

        if (match.captured(5).isEmpty()) {
            const qint64 etaSeconds = (match.captured(2).toLongLong() * 60 + match.captured(3).toLongLong()) * 60 + match.captured(4).toLongLong();
            emit restoreEtaChanged(etaSeconds * 1000);
        }
    }

    if (!match.hasMatch())
        return;

    _restoreProgress = match.captured(1).toFloat();

    emit restoreProgressChanged(_restoreProgress);
}

bool RestoreService::stageHome()
{
    _homeStaged = false;

    const QString home = QStringLiteral("/home");
    const QString stagingDir = QStringLiteral("/home/.backup");
    const QString mountPoint = QStringLiteral("/backup/home");

    bool homeListed = false;

    for (const QString &directory : _settings->value("directory").toStringList()) {
        if (QDir::cleanPath("/" + directory.trimmed()) == home)
            homeListed = true;
    }

    if (!homeListed || QStorageInfo(home).rootPath() != home)
        return true;

    if (!QDir(stagingDir).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot).isEmpty()) {
        emit error(tr("%1 is not empty, remove it before restoring").arg(stagingDir));
        return false;
    }

    if (!QDir().mkpath(stagingDir) || !QDir().mkpath(mountPoint)) {
        emit error(tr("Could not create %1").arg(stagingDir));
        return false;
    }

    if (::mount(stagingDir.toLocal8Bit().constData(), mountPoint.toLocal8Bit().constData(), nullptr, MS_BIND, nullptr) != 0) {
        const QString reason = QString::fromLocal8Bit(strerror(errno));

        QDir().rmdir(mountPoint);

        emit error(tr("Could not bind %1 to %2: %3").arg(stagingDir, mountPoint, reason));
        return false;
    }

    _homeStaged = true;

    return true;
}

bool RestoreService::unstageHome()
{
    if (!_homeStaged)
        return true;

    const QString mountPoint = QStringLiteral("/backup/home");

    ::umount2(mountPoint.toLocal8Bit().constData(), MNT_DETACH);

    QDir().rmdir(mountPoint);

    return !QFileInfo::exists(mountPoint);
}
