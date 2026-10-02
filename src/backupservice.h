#ifndef BACKUPSERVICE_H
#define BACKUPSERVICE_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QSettings>
#include <QNetworkReply>
#include <QFile>
#include "qwebdav.h"
#include "qwebdavdirparser.h"

class BackupService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Stage stage READ getStage NOTIFY stageChanged)
public:
    enum Stage { Idle, Preparing, Uploading, Assembling, Pruning };
    Q_ENUM(Stage)

    explicit BackupService(QObject *parent = nullptr);

    Q_INVOKABLE void backup(QString profile);

    Stage getStage() const;

signals:
    void backupProgress(qint64 bytesSent, qint64 bytesTotal, qint64 etaMs);
    void backupFinished(bool success, const QString &error);
    void stageChanged(Stage stage);

private slots:
    void onReadStarted();
    void onSizeFinished();
    void onTarError(QProcess::ProcessError error);
    void onProgressTick();
    void onMkcolFinished();
    void onSplitFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onChunkUploadFinished();
    void onMoveFinished();
    void onBackupListFinished();
    void onOldBackupRemoved();
    void tryUploadNextChunk();
    void onRsyncOutput();
    void onRsyncFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    qint64 directorySize(const QString &path) const;
    qint64 blockDeviceSize(const QString &path) const;
    qint64 processBytesRead(qint64 pid) const;
    void buildSourceStream();
    QString profileSlug() const;
    QString davPath(const QString &path) const;
    QString davUrl(const QString &path) const;
    void setupWebdav();
    void startUpload();
    void startChunkedTransfer();
    void finishChunksAndAssemble();
    void pruneOldBackups();
    void finishUpload(bool success, const QString &error);
    void setStage(Stage stage);
    bool validateWebDavSettings();
    bool validateSourceSettings();
    bool validateRsyncSettings();
    void startRsync();

private:
    QProcess *_sizeProcess = nullptr;
    QProcess *_readProcess = nullptr;
    QProcess *_splitProcess = nullptr;
    QProcess *_rsyncProcess = nullptr;
    QByteArray _rsyncOutput;
    QWebdav *_webdav = nullptr;
    QNetworkReply *_mkcolReply = nullptr;
    QNetworkReply *_chunkUploadReply = nullptr;
    QNetworkReply *_moveReply = nullptr;
    QWebdavDirParser *_dirParser = nullptr;
    QFile *_chunkFile = nullptr;
    QSettings *_settings = nullptr;
    QTimer *_progressTimer = nullptr;
    QTimer *_chunkPollTimer = nullptr;
    QString _stagingDir;
    QString _destinationUrl;
    QString _uploadsBaseUrl;
    QString _backupDir;
    QString _backupPrefix;
    QString _backupExtension;
    int _nextChunkIndex = 0;
    int _pendingRemovals = 0;
    int _chunkRetries = 0;
    qint64 _retryAfterMs = 0;
    qint64 _uploadedBytesTotal = 0;
    bool _splitFinished = false;
    bool _splitPaused = false;
    qint64 _totalBytes = 0;
    qint64 _bytesRead = 0;
    qint64 _lastEtaMs = -1;
    Stage _stage = Idle;
};

#endif // BACKUPSERVICE_H
