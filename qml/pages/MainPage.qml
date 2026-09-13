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
                            //
                            // completeAnimation first, or the replace lands while
                            // the push that opened the camera is still animating
                            // and Silica refuses it with "cannot pop while
                            // transition is in progress".
                            pageStack.completeAnimation()
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

            GroupPanel {
                width: parent.width
                title: qsTr("Recent")
                visible: history.count > 0

                Repeater {
                    model: history.entries

                    delegate: PanelRow {
                        width: parent.width

                        // What the page says, not what the camera called the
                        // file: a document is recognised by its first line far
                        // faster than by IMG_0042.
                        title: modelData.summary
                        detail: modelData.imageExists
                                ? qsTr("%1 words").arg(modelData.wordCount)
                                : qsTr("%1 words — photo no longer on the device")
                                  .arg(modelData.wordCount)
                        glyph: modelData.imageExists ? "\u25a4" : "\u2717"

                        onClicked: {
                            if (modelData.imageExists) {
                                // With the text that was kept: reading the photo
                                // again takes seconds and can come out differently,
                                // and nothing about tapping a past reading asks for
                                // that. "Read again" is on the page for when it is
                                // wanted.
                                pageStack.push(Qt.resolvedUrl("ResultPage.qml"),
                                               { imageUrl: "file://" + modelData.imagePath,
                                                 storedText: history.textOf(modelData.id) })
                            } else {
                                // The photo is gone, but the text was kept. Show
                                // that rather than offering a reading that cannot
                                // happen.
                                pageStack.push(Qt.resolvedUrl("StoredTextPage.qml"),
                                               { title: modelData.summary,
                                                 body: history.textOf(modelData.id) })
                            }
                        }
                    }
                }

                // The group's own action, as its last row. Mojo put "add an item"
                // there rather than behind a floating button; the same reasoning
                // puts "clear these" there rather than in a menu.
                PanelRow {
                    width: parent.width
                    title: qsTr("Clear history")
                    glyph: "\u2715"
                    onClicked: remorse.execute(qsTr("Clearing history"),
                                               function () { history.forgetAll() })
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

    RemorsePopup {
        id: remorse
    }

    Component {
        id: imagePicker

        ImagePickerPage {
            onSelectedContentPropertiesChanged: {
                if (selectedContentProperties.filePath) {
                    // The picker is still on the stack at this point; replacing it
                    // means Back from the result returns to the main page rather
                    // than to the gallery.
                    //
                    // The selection arrives while the picker's own transition is
                    // still running, which is what logs "cannot pop while
                    // transition is in progress" and can drop the replace.
                    pageStack.completeAnimation()
                    pageStack.replace(Qt.resolvedUrl("ResultPage.qml"),
                                      { imageUrl: "file://"
                                                  + selectedContentProperties.filePath })
                }
            }
        }
    }
}
