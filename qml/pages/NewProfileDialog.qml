import QtQuick 2.0
import Sailfish.Silica 1.0

Dialog {
    id: dialog

    allowedOrientations: Orientation.All

    property alias name: nameField.text

    canAccept: nameField.text.trim() !== ""

    Column {
        width: parent.width

        DialogHeader {
            acceptText: qsTr("Create")
        }

        TextField {
            id: nameField

            label: qsTr("Profile name")
            placeholderText: label
            focus: true

            EnterKey.enabled: dialog.canAccept
            EnterKey.onClicked: dialog.accept()
        }
    }
}
