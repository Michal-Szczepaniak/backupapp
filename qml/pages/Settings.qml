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

Page {
    id: page

    allowedOrientations: Orientation.All

    property string newProfileFile: ""

    onStatusChanged: {
        if (status !== PageStatus.Active)
            return

        profilesList.model = settings.getProfiles()

        if (newProfileFile !== "") {
            pageStack.animatorPush(Qt.resolvedUrl("ProfileSettings.qml"), {filename: newProfileFile})
            newProfileFile = ""
        }
    }

    SilicaFlickable {
        anchors.fill: parent

        contentHeight: column.height

        PullDownMenu {
            MenuItem {
                text: qsTr("Add profile")
                onClicked: {
                    var dialog = pageStack.push(Qt.resolvedUrl("NewProfileDialog.qml"))

                    dialog.accepted.connect(function() {
                        newProfileFile = settings.createProfile(dialog.name.trim())
                    })
                }
            }
        }

        Column {
            id: column

            width: page.width
            spacing: Theme.paddingLarge

            PageHeader {
                id: header
                title: qsTr("Settings")
            }

            Label {
                id: infoLabel

                color: Theme.secondaryHighlightColor
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - Theme.horizontalPageMargin*2
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter

                text: qsTr("You can edit files in /etc/backupapp/ to configure this app.")
            }

            Item {
                height: Theme.itemSizeLarge
                width: 1
            }

            Repeater {
                id: profilesList

                model: settings.getProfiles()

                delegate: ListItem {

                    DetailItem {
                        id: detailItem
                        anchors.centerIn: parent

                        label: modelData.name
                        value: modelData.file
                    }

                    onClicked: {
                        pageStack.animatorPush(Qt.resolvedUrl("ProfileSettings.qml"), {filename: modelData.file})
                    }

                    menu: ContextMenu {
                        MenuItem {
                            text: qsTr("Delete")

                            onClicked: {
                                var file = modelData.file

                                remorseAction(qsTr("Deleting"), function() {
                                    settings.deleteProfile(file)
                                    profilesList.model = settings.getProfiles()
                                })
                            }
                        }
                    }
                }
            }
        }
    }
}
