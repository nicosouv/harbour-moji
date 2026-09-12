import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import Mochi 1.0

// Two ways in: the camera, or a photo already on the device.
//
// Mojo put the action in the list as a row rather than behind a floating button,
// and the pulley carries what is global - settings, about. That split is the rule
// this app follows: rows do the thing the group is about, the pulley does the rest.
Page {
    id: page

    property string imagePath: ""

    allowedOrientations: defaultAllowedOrientations

    Rectangle {
        anchors.fill: parent
        color: Tokens.pageColor
        visible: !Tokens.ambient
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge * 2

        VerticalScrollDecorator { }

        PullDownMenu {
            MenuItem {
                text: qsTr("About")
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
        }

        Column {
            id: content

            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Moji OCR")
            }

            GroupPanel {
                width: parent.width
                title: qsTr("Read")

                PanelRow {
                    width: parent.width
                    title: qsTr("Choose a photo")
                    detail: qsTr("From the gallery")
                    accent: true
                    glyph: "+"
                    onClicked: pageStack.push(imagePicker)
                }
            }

            GroupPanel {
                width: parent.width
                title: qsTr("Selected")
                visible: page.imagePath !== ""

                PanelRow {
                    width: parent.width
                    // The file name only: a full path is unreadable at this size
                    // and the directory is not what the user is checking.
                    title: page.imagePath.substring(page.imagePath.lastIndexOf("/") + 1)
                    detail: qsTr("Recognition is not wired up yet")
                    glyph: "…"
                }
            }

            Image {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                visible: page.imagePath !== ""
                source: page.imagePath === "" ? "" : "file://" + page.imagePath

                // Decoded at the size it is drawn rather than at the camera's
                // resolution: a 12-megapixel photo held at full size is ~48MB of
                // pixels, which is how an image viewer gets itself killed on a
                // phone.
                sourceSize.width: page.width
            }
        }
    }

    Component {
        id: imagePicker

        ImagePickerPage {
            onSelectedContentPropertiesChanged: {
                page.imagePath = selectedContentProperties.filePath
            }
        }
    }
}
