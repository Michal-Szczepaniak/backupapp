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

         summary: qsTr("Musikilo")
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
