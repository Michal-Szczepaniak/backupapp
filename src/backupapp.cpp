#include <QtQuick>
#include <sailfishapp.h>
#include "backupservice.h"
#include "restoreservice.h"
#include "settings.h"
#include <unistd.h>
#include <sys/types.h>

int main(int argc, char *argv[])
{
    QCoreApplication::setSetuidAllowed(true);

    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    QSharedPointer<QQuickView> view(SailfishApp::createView());

    Settings settings;
    BackupService backupService;
    RestoreService restoreService;

    if (setuid(0)) {
        perror("setuid");
        exit(1);
    }

    qmlRegisterUncreatableType<BackupService>("backupapp", 1, 0, "BackupService", QStringLiteral("Use the backupService context property"));
    qmlRegisterUncreatableType<RestoreService>("backupapp", 1, 0, "RestoreService", QStringLiteral("Use the restoreService context property"));

    view->rootContext()->setContextProperty("backupService", &backupService);
    view->rootContext()->setContextProperty("restoreService", &restoreService);
    view->rootContext()->setContextProperty("settings", &settings);

    view->setSource(SailfishApp::pathTo("qml/backupapp.qml"));
    view->show();

    int ret = app->exec();
    return ret;
}
