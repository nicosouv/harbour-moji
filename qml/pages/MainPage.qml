import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import Mochi 1.0

// Two ways in: photograph a page, or pick one already on the device.
//
// Mojo put the action in the list as a row rather than behind a floating button,
// and the pulley carries what is global - settings, about. That split is the rule
// this app follows: rows do the thing the group is about, the pulley does the rest.
Page {
    id: page

    allowedOrientations: defaultAllowedOrientations

    function read(path) {
        pageStack.push(Qt.resolvedUrl("ResultPage.qml"),
                       { imageUrl: "file://" + path })
    }

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
                    title: qsTr("Take a photo")
                    detail: qsTr("Point the camera at a page")
                    accent: true
                    glyph: "●"
                    onClicked: {
                        var camera = pageStack.push(Qt.resolvedUrl("CameraPage.qml"))
                        camera.captured.connect(function (path) {
                            // Replace rather than push: coming back from the
                            // result should return here, not to a viewfinder that
                            // would restart the camera behind the page.
                            pageStack.replace(Qt.resolvedUrl("ResultPage.qml"),
                                              { imageUrl: "file://" + path })
                        })
                    }
                }

                PanelRow {
                    width: parent.width
                    title: qsTr("Choose a photo")
                    detail: qsTr("From the gallery")
                    accent: true
                    glyph: "+"
                    onClicked: pageStack.push(imagePicker)
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Tokens.secondaryColor
                text: qsTr("Everything happens on this device. Moji has no network permission at all.")
            }
        }
    }

    Component {
        id: imagePicker

        ImagePickerPage {
            onSelectedContentPropertiesChanged: {
                if (selectedContentProperties.filePath) {
                    // The picker is still on the stack at this point; replacing it
                    // means Back from the result returns to the main page rather
                    // than to the gallery.
                    pageStack.replace(Qt.resolvedUrl("ResultPage.qml"),
                                      { imageUrl: "file://"
                                                  + selectedContentProperties.filePath })
                }
            }
        }
    }
}
