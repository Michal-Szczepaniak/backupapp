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

Page {
    id: page

    allowedOrientations: Orientation.All

    property real rootFreeBytes: storageInfo.freeBytes("/")

    onStatusChanged: {
        if (status === PageStatus.Active) {
            profilesRepeater.model = settings.getProfiles()
            rootFreeBytes = storageInfo.freeBytes("/")
        }
    }

    StorageInfoProvider {
        id: storageInfo
    }

    KeepAlive {
        id: keepAlive

        enabled: backupService.stage !== BackupService.Idle
    }

    Connections {
        target: backupService

        function formatEta(etaMs) {
            if (etaMs < 0)
                return qsTr("calculating…")

            var totalSeconds = Math.round(etaMs / 1000)
            var minutes = Math.floor(totalSeconds / 60)
            var seconds = totalSeconds % 60

            return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
        }

        onBackupProgress: {
            var percent = ((bytesSent / bytesTotal) * 100).toFixed(1)

            info.text = qsTr("%1% · ETA %2").arg(percent).arg(formatEta(etaMs))
            progressBar.value = (bytesSent/bytesTotal)*100;
            progressBar.label = qsTr("%1%").arg(((bytesSent / bytesTotal) * 100).toFixed(2));
        }
        onBackupFinished: {
            info.text = success ? "Success" : ("Failure" + error)

            if (success) {
                progressBar.value = 100
                progressBar.label = qsTr("100%")
            }
        }
        onStageChanged: {
            if (stage === BackupService.Preparing)
                info.text = qsTr("Preparing…")
            else if (stage === BackupService.Uploading)
                info.text = qsTr("Backing up…")
            else if (stage === BackupService.Assembling)
                info.text = qsTr("Assembling on server…")
            else if (stage === BackupService.Pruning)
                info.text = qsTr("Removing old backups…")
        }
    }

    SilicaFlickable {
        anchors.fill: parent

        PullDownMenu {
            MenuItem {
                text: qsTr("About")
                onClicked: pageStack.animatorPush(Qt.resolvedUrl("About.qml"))
            }

            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.animatorPush(Qt.resolvedUrl("Settings.qml"))
            }

            MenuItem {
                text: qsTr("Restore")
                onClicked: pageStack.animatorPush(Qt.resolvedUrl("Restore.qml"))
            }
        }

        contentHeight: column.height

        Column {
            id: column

            width: page.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Backupapp")
            }

            Label {
                visible: page.rootFreeBytes >= 0 && page.rootFreeBytes < 5 * 1024 * 1024 * 1024
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - Theme.horizontalPageMargin*2
                wrapMode: Text.Wrap
                color: Theme.errorColor
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("Only %1 GB free on the root partition. A full system restore needs room for a second copy of the system, so you may not be able to restore this backup.")
                      .arg((page.rootFreeBytes / (1024 * 1024 * 1024)).toFixed(1))
            }

            ComboBox {
                id: profileComboBox

                anchors.horizontalCenter: parent.horizontalCenter
                label: qsTr("Profile")

                menu: ContextMenu {
                    Repeater {
                        id: profilesRepeater

                        model: settings.getProfiles()

                        MenuItem {
                            text: modelData.name

                            property string fileName: modelData.file
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
                visible: value !== 0
                anchors.horizontalCenter: parent.horizontalCenter
                minimumValue: 0
                maximumValue: 100
                indeterminate: backupService.stage === BackupService.Assembling || backupService.stage === BackupService.Pruning
            }

            Button {
                text: qsTr("Backup now!")
                onClicked: backupService.backup(profileComboBox.currentItem.fileName)
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }
}
