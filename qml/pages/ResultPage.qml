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

    // Saying which unit is selected is what makes the second tap understandable.
    // Without it the box just gets bigger and the user has to infer the rule.
    function scopeName(scope) {
        if (scope === ocr.scopeLine()) return qsTr("Line")
        if (scope === ocr.scopeParagraph()) return qsTr("Paragraph")
        if (scope === ocr.scopeBlock()) return qsTr("Block")
        return qsTr("Word")
    }

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
                onClicked: {
                    Clipboard.text = ocr.text
                    banner.show(qsTr("All text copied"))
                }
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

                // Where the text is. Without this the photo looks inert and
                // nothing suggests it can be touched; with it, the page is
                // visibly understood before anything is tapped.
                //
                // Per line, not per word: a page holds a couple of thousand words
                // and a few dozen lines, and a Repeater over the former stutters.
                Repeater {
                    model: page.selection.valid === true ? [] : ocr.lines

                    delegate: Rectangle {
                        x: canvas.offsetX + modelData.x * canvas.ratio
                        y: modelData.y * canvas.ratio
                        width: modelData.width * canvas.ratio
                        height: modelData.height * canvas.ratio
                        radius: Tokens.hairline * 2

                        // Weak lines are tinted, strong ones barely marked. This
                        // is the confidence map: it points at the parts worth
                        // reading twice without putting a number on anything.
                        color: modelData.confidence < 70
                               ? Theme.rgba(Theme.errorColor, 0.20)
                               : Theme.rgba(Tokens.accentColor, 0.13)

                        opacity: ocr.busy ? 0 : 1
                        Behavior on opacity {
                            NumberAnimation {
                                duration: Tokens.durSlow
                                easing.type: Tokens.easingType
                            }
                        }
                    }
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

            // Mean confidence, stated plainly rather than as a bar. Below about
            // 70 the recogniser is usually wrong rather than slightly wrong, and
            // the useful advice at that point is to retake the photo - so that is
            // what it says instead of showing a number and leaving the user to
            // interpret it.
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: ocr.wordCount > 0
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: ocr.confidence < 70 ? Theme.errorColor : Tokens.secondaryColor
                text: ocr.confidence < 70
                      ? qsTr("Low confidence (%1%). Try again with more light, or fill the frame with the page.").arg(Math.round(ocr.confidence))
                      : qsTr("Confidence %1%").arg(Math.round(ocr.confidence))
            }

            GroupPanel {
                width: parent.width
                title: qsTr("Selection")
                visible: page.selection.valid === true

                PanelRow {
                    width: parent.width
                    title: page.selection.text || ""
                    detail: page.currentScope === ocr.scopeBlock()
                            ? qsTr("%1 — tap here to copy").arg(page.scopeName(page.currentScope))
                            : qsTr("%1 — tap the photo again to widen, or tap here to copy").arg(page.scopeName(page.currentScope))
                    glyph: "\""
                    onClicked: {
                        Clipboard.text = page.selection.text
                        banner.show(qsTr("Copied"))
                    }
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

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !ocr.busy && ocr.wordCount === 0 && ocr.lastError === ""
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Tokens.secondaryColor
                text: qsTr("No text found in this photo. More light and a closer frame usually fix it — or check the language in Settings.")
            }

            GroupPanel {
                width: parent.width
                title: qsTr("Found")
                visible: ocr.fields.length > 0

                Repeater {
                    model: ocr.fields

                    delegate: PanelRow {
                        width: parent.width

                        title: modelData.value
                        // The checksum verdict is the whole point, so it is said
                        // in words rather than shown as a colour someone has to
                        // learn. A field with nothing to check says neither.
                        detail: modelData.checkable
                                ? (modelData.checksumValid
                                   ? qsTr("%1 — checksum verified").arg(modelData.kindName)
                                   : qsTr("%1 — checksum does not match, read it again").arg(modelData.kindName))
                                : modelData.kindName

                        glyph: modelData.checkable
                               ? (modelData.checksumValid ? "\u2713" : "!")
                               : "\u00b7"

                        onClicked: {
                            Clipboard.text = modelData.value
                            banner.show(qsTr("Copied"))
                        }
                    }
                }
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

    Banner {
        id: banner
    }
}
