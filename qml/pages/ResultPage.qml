import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// The photo, with the recognised text available by touching it.
//
// Tapping a word selects that word; tapping inside what is already selected
// widens it to the line, then the paragraph, then the block. That is the whole
// gesture, and it works because the recogniser reported a real structure rather
// than a flat list of strings - see src/textlayout.h.
Page {
    id: page

    property url imageUrl

    // Kept in the image's own coordinates, never the screen's, so it survives
    // rotation and zoom without being recomputed.
    property int currentScope: 0
    property var selection: ({ valid: false })

    allowedOrientations: defaultAllowedOrientations

    Component.onCompleted: {
        if (imageUrl != "") {
            ocr.recognise(imageUrl, settings.tesseractLanguages)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Tokens.pageColor
        visible: !Tokens.ambient
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge * 2

        VerticalScrollDecorator { }

        PullDownMenu {
            MenuItem {
                text: qsTr("Copy all text")
                enabled: ocr.wordCount > 0
                onClicked: Clipboard.text = ocr.text
            }
            MenuItem {
                text: qsTr("Read again")
                enabled: !ocr.busy
                onClicked: ocr.recognise(page.imageUrl, settings.tesseractLanguages)
            }
        }

        Column {
            id: column

            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Text")
                description: ocr.busy
                             ? qsTr("Reading…")
                             : (ocr.wordCount > 0
                                ? qsTr("%1 words").arg(ocr.wordCount)
                                : ocr.lastError)
            }

            Item {
                id: canvas

                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                height: photo.paintedHeight > 0 ? photo.paintedHeight : width

                // How many screen pixels one image pixel occupies. Everything
                // below converts through this and nothing hardcodes a size.
                readonly property real ratio:
                    (ocr.imageSize.width > 0 && photo.paintedWidth > 0)
                        ? photo.paintedWidth / ocr.imageSize.width : 1

                readonly property real offsetX: (width - photo.paintedWidth) / 2

                Image {
                    id: photo

                    anchors.fill: parent
                    source: page.imageUrl
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    // Decoded at the size it is drawn, not the camera's: a 12
                    // megapixel photo held at full resolution is ~48MB of pixels,
                    // which is how an image viewer gets itself killed on a phone.
                    sourceSize.width: page.width
                }

                // The current selection, drawn over the photo in image
                // coordinates scaled to the screen.
                Rectangle {
                    visible: page.selection.valid === true
                    color: Theme.rgba(Tokens.onColor, 0.28)
                    border.width: Tokens.hairline
                    border.color: Tokens.onColor
                    radius: Tokens.hairline * 2

                    x: canvas.offsetX + (page.selection.x || 0) * canvas.ratio
                    y: (page.selection.y || 0) * canvas.ratio
                    width: (page.selection.width || 0) * canvas.ratio
                    height: (page.selection.height || 0) * canvas.ratio

                    Behavior on x { NumberAnimation { duration: Tokens.durFast; easing.type: Tokens.easingType } }
                    Behavior on y { NumberAnimation { duration: Tokens.durFast; easing.type: Tokens.easingType } }
                    Behavior on width { NumberAnimation { duration: Tokens.durFast; easing.type: Tokens.easingType } }
                    Behavior on height { NumberAnimation { duration: Tokens.durFast; easing.type: Tokens.easingType } }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: ocr.wordCount > 0

                    onClicked: {
                        var imageX = (mouse.x - canvas.offsetX) / canvas.ratio
                        var imageY = mouse.y / canvas.ratio

                        // Inside what is already selected means "give me more of
                        // it"; anywhere else starts again at a single word. Two
                        // taps in the same place therefore walk outwards, which
                        // is the entire interaction.
                        var inside = page.selection.valid === true
                                && imageX >= page.selection.x
                                && imageX <= page.selection.x + page.selection.width
                                && imageY >= page.selection.y
                                && imageY <= page.selection.y + page.selection.height

                        page.currentScope = inside ? ocr.growScope(page.currentScope)
                                                   : ocr.scopeWord()

                        // A fingertip is wider than a word box is tall, so a miss
                        // falls back to the nearest word within this much.
                        var tolerance = Math.round(Theme.itemSizeExtraSmall
                                                   / canvas.ratio)

                        page.selection = ocr.selectAt(Math.round(imageX),
                                                      Math.round(imageY),
                                                      page.currentScope,
                                                      tolerance)
                    }
                }
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: ocr.busy
                size: BusyIndicatorSize.Large
            }

            GroupPanel {
                width: parent.width
                title: qsTr("Selection")
                visible: page.selection.valid === true

                PanelRow {
                    width: parent.width
                    title: page.selection.text || ""
                    detail: qsTr("Tap the photo again to widen, or tap here to copy")
                    glyph: "\""
                    onClicked: Clipboard.text = page.selection.text
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: ocr.wordCount > 0 && page.selection.valid !== true
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Tokens.secondaryColor
                text: qsTr("Tap a word in the photo. Tap it again to take the whole line, then the paragraph.")
            }

            GroupPanel {
                width: parent.width
                title: qsTr("All text")
                visible: ocr.wordCount > 0

                Item {
                    width: parent.width
                    height: rawText.height + Theme.paddingLarge * 2

                    Label {
                        id: rawText

                        anchors {
                            left: parent.left
                            right: parent.right
                            leftMargin: Theme.paddingLarge
                            rightMargin: Theme.paddingLarge
                            verticalCenter: parent.verticalCenter
                        }

                        // Plain text, never RichText. This string is whatever was
                        // in front of the camera, so rendering it as markup would
                        // let a photographed <img> tag decide what this device
                        // fetches. scripts/check_qml.py fails the build on it.
                        text: ocr.text
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Tokens.primaryColor
                    }
                }
            }
        }
    }
}
