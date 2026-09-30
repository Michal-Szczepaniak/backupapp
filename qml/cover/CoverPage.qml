import QtQuick 2.0
import Sailfish.Silica 1.0
import backupapp 1.0

CoverBackground {
    id: cover

    property real progress: 0
    property string etaText: qsTr("calculating…")
    property bool restoring: restoreService.stage === RestoreService.Extracting
    property bool active: backupService.stage !== BackupService.Idle || restoring
    property bool busy: backupService.stage === BackupService.Assembling || backupService.stage === BackupService.Pruning

    function formatEta(etaMs) {
        if (etaMs < 0)
            return qsTr("calculating…")

        var totalSeconds = Math.round(etaMs / 1000)
        var minutes = Math.floor(totalSeconds / 60)
        var seconds = totalSeconds % 60

        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    Connections {
        target: backupService

        onBackupProgress: {
            cover.progress = bytesTotal > 0 ? bytesSent / bytesTotal : 0
            cover.etaText = formatEta(etaMs)
        }

        onBackupFinished: cover.progress = 0
        onStageChanged: if (stage === BackupService.Idle) cover.progress = 0
    }

    Connections {
        target: restoreService

        onRestoreProgressChanged: cover.progress = progress / 100
        onRestoreEtaChanged: cover.etaText = formatEta(etaMs)

        onStageChanged: {
            cover.progress = 0
            cover.etaText = qsTr("calculating…")
        }
    }

    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        spacing: Theme.paddingMedium

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: cover.restoring ? qsTr("Restoring") : qsTr("Backupapp")
            font.pixelSize: Theme.fontSizeMedium
            color: Theme.highlightColor
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: !cover.active ? qsTr("Idle") : cover.busy ? qsTr("Assembling…") : qsTr("%1%").arg((cover.progress * 100).toFixed(0))
            font.pixelSize: Theme.fontSizeExtraLarge
            color: Theme.primaryColor
        }

        ProgressBar {
            width: parent.width
            visible: cover.active
            minimumValue: 0
            maximumValue: 1
            value: cover.progress
            indeterminate: cover.busy
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: cover.active && !cover.busy
            text: qsTr("ETA %1").arg(cover.etaText)
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryColor
        }
    }
}
