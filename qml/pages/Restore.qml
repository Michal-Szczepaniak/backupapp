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

import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.KeepAlive 1.2
import backupapp 1.0
import Nemo.Notifications 1.0

Page {
    id: page

    allowedOrientations: Orientation.All

    KeepAlive {
        id: keepAlive

        enabled: restoreService.stage === RestoreService.Extracting
    }

    Notification {
         id: errorNotification

         summary: qsTr("Backupapp")
         replacesId: 1
    }

    Connections {
        target: restoreService

        onError: {
            info.text = message
            errorNotification.body = message
            errorNotification.publish()
        }

        onGotBackupFilesList: {
            backupFilesRepeater.model = files
        }

        property string etaText: qsTr("calculating…")

        onRestoreEtaChanged: {
            if (etaMs < 0) {
                etaText = qsTr("calculating…")
                return
            }

            var totalSeconds = Math.round(etaMs / 1000)
            var minutes = Math.floor(totalSeconds / 60)
            var seconds = totalSeconds % 60

            etaText = minutes + ":" + (seconds < 10 ? "0" : "") + seconds
        }

        onRestoreProgressChanged: {
            var percent = progress.toFixed(1)

            info.text = qsTr("%1% · ETA %2").arg(percent).arg(etaText)
        }

        onStageChanged: {
            if (stage === RestoreService.Extracting) {
                info.text = qsTr("Extracting…");
            } else if (stage === RestoreService.Finished) {
                info.text = qsTr("Finished");
            } else if (stage === RestoreService.RebootRequired) {
                info.text = qsTr("Finished, reboot to apply the restore");
            }
        }
    }

    SilicaFlickable {
        anchors.fill: parent

        contentHeight: column.height

        Column {
            id: column

            width: page.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Restore")
            }

            ComboBox {
                id: profileComboBox

                anchors.horizontalCenter: parent.horizontalCenter
                label: qsTr("Profile")

                menu: ContextMenu {
                    Repeater {
                        model: settings.getProfiles()

                        MenuItem {
                            text: modelData.name

                            property string fileName: modelData.file
                        }
                    }
                }

                onCurrentItemChanged: if (currentItem) restoreService.getBackups(currentItem.fileName)
            }

            ComboBox {
                id: backupFilesComboBox

                anchors.horizontalCenter: parent.horizontalCenter
                label: qsTr("Backup file")

                menu: ContextMenu {
                    Repeater {
                        id: backupFilesRepeater

                        MenuItem {
                            text: modelData
                        }
                    }
                }
            }

            Label {
                id: info
                text: ""
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - Theme.horizontalPageMargin*2
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            ProgressBar {
                id: progressBar
                width: parent.width
                value: restoreService.restoreProgress
                visible: value !== 0
                anchors.horizontalCenter: parent.horizontalCenter
                minimumValue: 0
                maximumValue: 100
            }

            Button {
                text: qsTr("Restore backup")
                onClicked: restoreService.restore(profileComboBox.currentItem.fileName, backupFilesComboBox.currentItem.text)
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }
}
