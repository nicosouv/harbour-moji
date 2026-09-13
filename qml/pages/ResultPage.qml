// 2.5, not 2.0: Image.autoTransform arrived with Qt 5.5 and a QML import states
// the version whose API it is asking for, not the one the device happens to have.
// Sailfish ships Qt 5.6, so 2.5 resolves - but importing 2.0 and using a 2.5
// property fails the whole file at load with "is not available in QtQuick 2.0".
import QtQuick 2.5
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

    // Which block the text panel is restricted to, or -1 for the whole page.
    //
    // Photograph a leaflet and the column next door arrives too. Tesseract already
    // separated them; this simply lets the user say which one they meant, which is
    // cheaper and more accurate than cropping the photo and reading it again.
    property int onlyBlock: -1

    readonly property string primaryLanguage:
        settings.ocrLanguages.length > 0 ? settings.ocrLanguages[0] : "eng"

    readonly property var languageLabels: {
        var labels = []
        for (var i = 0; i < settings.installedLanguages.length; ++i) {
            labels.push(settings.languageName(settings.installedLanguages[i]))
        }
        return labels
    }

    // Saying which unit is selected is what makes the second tap understandable.
    // Without it the box just gets bigger and the user has to infer the rule.
    function scopeName(scope) {
        if (scope === ocr.scopeLine()) return qsTr("Line")
        if (scope === ocr.scopeParagraph()) return qsTr("Paragraph")
        if (scope === ocr.scopeBlock()) return qsTr("Block")
        return qsTr("Word")
    }

    allowedOrientations: defaultAllowedOrientations

    Connections {
        target: ocr
        onFinished: {
            history.remember(page.imageUrl.toString().replace("file://", ""),
                             settings.tesseractLanguages,
                             ocr.wordCount, ocr.confidence, ocr.text)
        }
    }

    Component.onCompleted: {
        if (imageUrl != "") {
            ocr.recognise(imageUrl, settings.tesseractLanguages, settings.autoRotate)
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
                onClicked: ocr.recognise(page.imageUrl, settings.tesseractLanguages,
                                         settings.autoRotate)
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

            GroupPanel {
                width: parent.width
                title: qsTr("Language")
                visible: ocr.wordCount > 0 || ocr.lastError !== ""

                Selector {
                    width: parent.width
                    label: qsTr("Read as")
                    options: page.languageLabels
                    currentIndex: {
                        var i = settings.installedLanguages.indexOf(page.primaryLanguage)
                        return i < 0 ? 0 : i
                    }
                    onCurrentIndexChanged: {
                        var code = settings.installedLanguages[currentIndex]
                        if (code && code !== page.primaryLanguage) {
                            settings.ocrLanguages = [code]
                            // Re-read straight away: nobody changes this setting
                            // for later, they change it because what is on screen
                            // came out wrong.
                            ocr.recognise(page.imageUrl, settings.tesseractLanguages,
                                          settings.autoRotate)
                        }
                    }
                }
            }

            Item {
                id: canvas

                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin

                // Height from the image's own proportions, never from
                // paintedHeight.
                //
                // paintedHeight is what the Image ended up drawing, which depends
                // on the Image's height, which - the Image being anchored to fill
                // this Item - depends on this height. Qt calls that a binding loop
                // and says so once; what it does not say is that it then keeps
                // re-evaluating the layout, which is what makes the page look
                // frozen rather than merely wrong.
                //
                // implicitWidth/implicitHeight are the loaded image's own
                // dimensions and are outputs of the loader, so nothing here feeds
                // back into them. sourceSize would do as well once loaded, but it
                // reports a zero height while only its width has been set, and a
                // zero aspect collapses the photo to nothing on the first frame.
                readonly property real aspect: photo.implicitWidth > 0
                        ? photo.implicitHeight / photo.implicitWidth : 1

                height: width * aspect

                // The image now fills the width exactly, so one image pixel is
                // this many screen pixels and there is no letterboxing to offset.
                readonly property real ratio: ocr.imageSize.width > 0
                        ? width / ocr.imageSize.width : 1

                readonly property real offsetX: 0

                Image {
                    id: photo

                    anchors.fill: parent
                    source: page.imageUrl
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true

                    // The camera tags a portrait photo rather than rotating its
                    // pixels, and QML ignores that tag unless asked. Without this
                    // the preview lies on its side while OcrEngine - which does
                    // apply the tag - reports boxes for the upright image, so
                    // every box lands in the wrong place.
                    autoTransform: true
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

                // The blocks, outlined so they can be picked. Only when there is
                // a choice to make: one block is the whole photo and outlining it
                // says nothing.
                Repeater {
                    model: (page.selection.valid === true || ocr.blocks.length < 2)
                           ? [] : ocr.blocks

                    delegate: Rectangle {
                        // Above the whole-photo tap handler, which is declared
                        // after these and would otherwise swallow every tap meant
                        // for a block or a doubtful word - which is exactly what
                        // made retyping a word impossible to discover.
                        z: 1

                        x: canvas.offsetX + modelData.x * canvas.ratio
                        y: modelData.y * canvas.ratio
                        width: modelData.width * canvas.ratio
                        height: modelData.height * canvas.ratio

                        color: page.onlyBlock === modelData.block
                               ? Theme.rgba(Tokens.onColor, 0.18) : "transparent"
                        border.width: Tokens.hairline * 2
                        border.color: page.onlyBlock === modelData.block
                                      ? Tokens.onColor
                                      : Theme.rgba(Tokens.accentColor, 0.55)
                        radius: Tokens.hairline * 3

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                page.onlyBlock = page.onlyBlock === modelData.block
                                                 ? -1 : modelData.block
                            }
                        }
                    }
                }

                // The words the recogniser doubted, tinted and tappable. Only the
                // doubtful ones: the rest are not worth an item each.
                Repeater {
                    model: page.selection.valid === true ? [] : ocr.uncertainWords

                    delegate: Rectangle {
                        z: 2   // above the block outlines as well as the photo

                        x: canvas.offsetX + modelData.x * canvas.ratio
                        y: modelData.y * canvas.ratio
                        width: modelData.width * canvas.ratio
                        height: modelData.height * canvas.ratio
                        radius: Tokens.hairline * 2

                        color: Theme.rgba(Theme.errorColor, 0.30)
                        border.width: Tokens.hairline
                        border.color: Theme.errorColor

                        MouseArea {
                            anchors.fill: parent
                            onClicked: page.editWord(modelData.index, modelData.text)
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
                // A Column still reserves space for an item that is merely not
                // running, which leaves a hole the height of the indicator under
                // the photo for the whole time the result is on screen.
                visible: ocr.busy
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
                visible: ocr.blocks.length > 1 && page.selection.valid !== true
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Tokens.secondaryColor
                text: page.onlyBlock >= 0
                      ? qsTr("Showing one block. Tap its outline again for the whole page.")
                      : qsTr("Outlined areas are separate blocks of text. Tap one to keep only it.")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: ocr.uncertainCount > 0 && page.selection.valid !== true
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.errorColor
                text: qsTr("Words marked in red were hard to read. Tap one to retype it.")
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
                title: page.onlyBlock >= 0 ? qsTr("Selected block") : qsTr("All text")
                visible: ocr.wordCount > 0

                Item {
                    width: parent.width
                    height: rawText.height + Theme.paddingLarge * 2

                    TextEdit {
                        id: rawText

                        anchors {
                            left: parent.left
                            right: parent.right
                            leftMargin: Theme.paddingLarge
                            rightMargin: Theme.paddingLarge
                            verticalCenter: parent.verticalCenter
                        }

                        // A TextEdit rather than a Label, so a part of the result
                        // can be selected and copied. Copying all of it is rarely
                        // what someone wants from a page of text.
                        //
                        // readOnly, because editing belongs to the correction
                        // flow, where a change is written back to the word it came
                        // from and the checksums follow. Typing into this box would
                        // change what is displayed and nothing else.
                        readOnly: true
                        selectByMouse: true
                        persistentSelection: true

                        // Plain text, never RichText. This string is whatever was
                        // in front of the camera, so rendering it as markup would
                        // let a photographed <img> tag decide what this device
                        // fetches. scripts/check_qml.py fails the build on it.
                        textFormat: TextEdit.PlainText

                        text: ocr.textOfBlock(page.onlyBlock)
                        wrapMode: TextEdit.Wrap
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Tokens.primaryColor
                        selectionColor: Tokens.selectionColor
                        selectedTextColor: Tokens.primaryColor
                    }
                }
            }
        }
    }

    function editWord(index, current) {
        var dialog = pageStack.push(Qt.resolvedUrl("CorrectWordDialog.qml"),
                                    { wordIndex: index, wordText: current })
        dialog.accepted.connect(function () {
            ocr.correctWord(dialog.wordIndex, dialog.wordText)
            banner.show(qsTr("Corrected"))
        })
    }

    Banner {
        id: banner
    }
}
