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

#include <QtQuick>
#include <sailfishapp.h>
#include "backupservice.h"
#include "restoreservice.h"
#include "settings.h"
#include "storageinfoprovider.h"
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
//        exit(1);
    }

    qmlRegisterUncreatableType<BackupService>("backupapp", 1, 0, "BackupService", QStringLiteral("Use the backupService context property"));
    qmlRegisterUncreatableType<RestoreService>("backupapp", 1, 0, "RestoreService", QStringLiteral("Use the restoreService context property"));
    qmlRegisterType<StorageInfoProvider>("backupapp", 1, 0, "StorageInfoProvider");

    view->rootContext()->setContextProperty("backupService", &backupService);
    view->rootContext()->setContextProperty("restoreService", &restoreService);
    view->rootContext()->setContextProperty("settings", &settings);

    view->setSource(SailfishApp::pathTo("qml/backupapp.qml"));
    view->show();

    int ret = app->exec();
    return ret;
}
