import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent

        contentHeight: column.height

        Column {
            id: column

            width: page.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Settings")
            }

            Label {
                x: Theme.horizontalPageMargin
                text: qsTr("Edit /etc/backupapp/ to configure this app.")
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraLarge
            }
        }
    }
}
