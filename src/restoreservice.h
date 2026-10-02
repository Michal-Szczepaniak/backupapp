#ifndef RESTORESERVICE_H
#define RESTORESERVICE_H

#include <QElapsedTimer>
#include <QObject>
#include <QSettings>
#include <qwebdav.h>
#include <qwebdavdirparser.h>

class RestoreService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(float restoreProgress READ getRestoreProgress NOTIFY restoreProgressChanged)
    Q_PROPERTY(Stage stage READ getStage NOTIFY stageChanged)
public:
    enum Stage { Idle, Extracting, Error, Finished, RebootRequired };
    Q_ENUM(Stage)

    explicit RestoreService(QObject *parent = nullptr);

    Q_INVOKABLE void restore(QString profile, QString backupFile);
    Q_INVOKABLE void getBackups(QString profile);

    float getRestoreProgress() const;
    Stage getStage() const;
    void setStage(Stage stage);

signals:
    void error(QString message);
    void gotBackupFilesList(QStringList files);
    void restoreProgressChanged(float progress);
    void restoreEtaChanged(qint64 etaMs);
    void stageChanged(Stage stage);

public slots:
    void onGotFilesList();
    void onRestoreFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onTarError(QProcess::ProcessError error);
    void feedBackupData();
    void restoreProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onRsyncOutput();

private:
    bool validateWebDavSettings();
    bool validateSourceSettings();
    bool validateRsyncSettings();
    QString getBackupPrefix();
    void setupWebdav();
    void restorePartialBackup();
    void restoreFullBackup();
    void activateFullRestore();
    void restoreRsyncBackup();

private:
    QSettings *_settings = nullptr;
    QWebdav *_webdav = nullptr;
    QWebdavDirParser _parser{};
    QProcess *_restoreProcess = nullptr;
    QString _backupFile{};
    bool _fullBackup = false;
    QNetworkReply *_backupFileReply = nullptr;
    QByteArray _rsyncOutput;
    float _restoreProgress = 0;
    QElapsedTimer _restoreTimer;
    Stage _stage = Idle;
};

#endif // RESTORESERVICE_H
