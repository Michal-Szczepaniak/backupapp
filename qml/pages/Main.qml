import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.KeepAlive 1.2
import backupapp 1.0

Page {
    id: page

    allowedOrientations: Orientation.All

    onStatusChanged: {
        if (status === PageStatus.Active)
            profilesRepeater.model = settings.getProfiles()
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
