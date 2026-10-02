import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    allowedOrientations: Orientation.All

    property var filename

    SilicaFlickable {
        anchors.fill: parent

        contentHeight: column.height

        Column {
            id: column

            width: page.width
            spacing: Theme.paddingLarge

            PageHeader {
                id: header
                title: filename
            }

            TextField {
                text: settings.getStringValue(filename, "name")
                label: qsTr("Profile name")

                onTextChanged: settings.setValue(filename, "name", text)
            }

            ComboBox {
                id: sourceType

                label: qsTr("Source type")

                currentIndex: settings.getStringValue(filename, "sourceType") === "block"

                menu: ContextMenu {
                    MenuItem {
                        text: qsTr("Directory")
                    }
                    MenuItem {
                        text: qsTr("Block")
                    }
                }

                onCurrentIndexChanged: {
                    settings.setValue(filename, "sourceType", currentIndex === 0 ? "directory" : "block")
                }
            }

            TextSwitch {
                text: qsTr("Full system backup")
                description: qsTr("Restore extracts to /backup and swaps it in on next boot instead of extracting over /")

                checked: settings.getBoolValue(filename, "fullSystemBackup")
                onCheckedChanged: settings.setValue(filename, "fullSystemBackup", checked)
            }

            TextField {
                text: settings.getStringListValue(filename, "directory").join(",")
                labelVisible: text === ""
                description: qsTr("Comma separated list of directories to back up")

                onTextChanged: settings.setStringListValue(filename, "directory", text.split(","))
                visible: sourceType.currentIndex === 0
            }

            TextField {
                text: settings.getStringValue(filename, "block")
                label: qsTr("Block file to back up")

                onTextChanged: settings.setValue(filename, "block", text)
                visible: sourceType.currentIndex === 1
            }

            ComboBox {
                id: destinationType

                label: qsTr("Destination type")

                currentIndex: settings.getStringValue(filename, "destinationType") === "rsync"

                menu: ContextMenu {
                    MenuItem {
                        text: qsTr("WebDav")
                    }
                    MenuItem {
                        text: qsTr("rsync")
                        visible: sourceType.currentIndex === 0
                    }
                }

                onCurrentIndexChanged: {
                    settings.setValue(filename, "destinationType", currentIndex === 0 ? "webdav" : "rsync")
                }
            }

            ComboBox {
                label: qsTr("WebDav protocol")

                currentIndex: settings.getStringValue(filename, "webdavType") === "https"

                menu: ContextMenu {
                    MenuItem {
                        text: qsTr("http")
                    }
                    MenuItem {
                        text: qsTr("https")
                    }
                }

                onCurrentIndexChanged: {
                    settings.setValue(filename, "webdavType", currentIndex === 0 ? "http" : "https")
                }
                visible: destinationType.currentIndex === 0
            }

            TextField {
                text: settings.getStringValue(filename, "webdavHost")
                label: qsTr("WebDav server hostname")

                onTextChanged: settings.setValue(filename, "webdavHost", text)
                visible: destinationType.currentIndex === 0
            }

            TextField {
                text: settings.getStringValue(filename, "webdavRoot")
                labelVisible: text === ""
                description: qsTr("Nextcloud DAV root, empty defaults to /remote.php/dav")

                onTextChanged: settings.setValue(filename, "webdavRoot", text)
                visible: destinationType.currentIndex === 0
            }

            TextField {
                text: settings.getStringValue(filename, "webdavUser")
                label: qsTr("Login name")

                onTextChanged: settings.setValue(filename, "webdavUser", text)
                visible: destinationType.currentIndex === 0
            }

            PasswordField {
                text: settings.getStringValue(filename, "webdavPassword")
                label: qsTr("Password")

                onTextChanged: settings.setValue(filename, "webdavPassword", text)
                visible: destinationType.currentIndex === 0
            }

            TextField {
                text: settings.getStringValue(filename, "webdavUserId")
                label: qsTr("Nextcloud user ID used in DAV paths")

                onTextChanged: settings.setValue(filename, "webdavUserId", text)
                visible: destinationType.currentIndex === 0
            }

            TextField {
                text: settings.getStringValue(filename, "webdavPath")
                label: qsTr("Remote directory to upload backups to")

                onTextChanged: settings.setValue(filename, "webdavPath", text)
                visible: destinationType.currentIndex === 0
            }

            TextField {
                text: settings.getStringValue(filename, "keepBackups")
                label: qsTr("Number of backups to keep, empty or 0 keeps everything")
                inputMethodHints: Qt.ImhDigitsOnly

                onTextChanged: settings.setValue(filename, "keepBackups", text)
                visible: destinationType.currentIndex === 0
            }

            TextField {
                text: settings.getStringValue(filename, "rsyncHost")
                label: qsTr("SSH host")

                onTextChanged: settings.setValue(filename, "rsyncHost", text)
                visible: destinationType.currentIndex === 1
            }

            TextField {
                text: settings.getStringValue(filename, "rsyncUser")
                label: qsTr("SSH login name")

                onTextChanged: settings.setValue(filename, "rsyncUser", text)
                visible: destinationType.currentIndex === 1
            }

            TextField {
                text: settings.getStringValue(filename, "rsyncPort")
                label: qsTr("SSH port, empty defaults to 22")
                inputMethodHints: Qt.ImhDigitsOnly

                onTextChanged: settings.setValue(filename, "rsyncPort", text)
                visible: destinationType.currentIndex === 1
            }

            TextField {
                text: settings.getStringValue(filename, "rsyncPath")
                label: qsTr("Remote directory to sync into")

                onTextChanged: settings.setValue(filename, "rsyncPath", text)
                visible: destinationType.currentIndex === 1
            }

            TextField {
                text: settings.getStringValue(filename, "rsyncKey")
                label: qsTr("Private key without a passphrase")

                onTextChanged: settings.setValue(filename, "rsyncKey", text)
                visible: destinationType.currentIndex === 1
            }

            TextField {
                text: settings.getStringValue(filename, "rsyncBackupOptions")
                label: qsTr("Extra rsync arguments for backup")

                onTextChanged: settings.setValue(filename, "rsyncBackupOptions", text)
                visible: destinationType.currentIndex === 1
            }

            TextField {
                text: settings.getStringValue(filename, "rsyncRestoreOptions")
                label: qsTr("Extra rsync arguments for restore")

                onTextChanged: settings.setValue(filename, "rsyncRestoreOptions", text)
                visible: destinationType.currentIndex === 1
            }
        }
    }
}
