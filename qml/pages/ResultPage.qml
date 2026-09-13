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
    property int onlyBlock: -1

    // Turned by hand, when the automatic choice got it wrong or the page is
    // simply easier to read the other way up. Applied to the view only: the
    // overlays are children of the same item, so they turn with the photo and
    // stay aligned without a coordinate being recomputed.
    property int viewRotation: 0

    // Dragging across the photo marks out a region to read instead of the whole
    // page. Tesseract separates columns well enough to pick one, but not when it
    // merges two into a single block - and then pointing at the part you meant is
    // the only way to say it.
    property bool marking: false
    property rect markedRegion: Qt.rect(0, 0, 0, 0)

    readonly property string primaryLanguage:
        settings.ocrLanguages.length > 0 ? settings.ocrLanguages[0] : "eng"

    readonly property var languageLabels: {
        var labels = []
        for (var i = 0; i < settings.installedLanguages.length; ++i) {
            labels.push(settings.languageName(settings.installedLanguages[i]))
        }
        return labels
    }

    allowedOrientations: defaultAllowedOrientations

    // Saying which unit is selected is what makes the second tap understandable.
    // Without it the box just gets bigger and the user has to infer the rule.
    function scopeName(scope) {
        if (scope === ocr.scopeLine()) return qsTr("Line")
        if (scope === ocr.scopeParagraph()) return qsTr("Paragraph")
        if (scope === ocr.scopeBlock()) return qsTr("Block")
        return qsTr("Word")
    }

    function readWhole() {
        ocr.recognise(page.imageUrl, settings.tesseractLanguages,
                      settings.autoRotate, settings.enhanceContrast)
    }

    function readRegion(region) {
        ocr.recogniseRegion(page.imageUrl, settings.tesseractLanguages,
                            settings.autoRotate, settings.enhanceContrast,
                            Math.round(region.x), Math.round(region.y),
                            Math.round(region.width), Math.round(region.height))
    }

    function editWord(index, current) {
        var dialog = pageStack.push(Qt.resolvedUrl("CorrectWordDialog.qml"),
                                    { wordIndex: index, wordText: current })
        dialog.accepted.connect(function () {
            ocr.correctWord(dialog.wordIndex, dialog.wordText)
            banner.show(qsTr("Corrected"))
        })
    }

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
            readWhole()
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
                text: qsTr("Save as searchable PDF")
                enabled: ocr.wordCount > 0
                onClicked: {
                    var name = page.imageUrl.toString().split("/").pop()
                                   .replace(/\.[^.]+$/, "") + ".pdf"
                    var target = StandardPaths.download + "/" + name
                    if (ocr.exportPdf(page.imageUrl, target.replace("file://", ""))) {
                        banner.show(qsTr("Saved to Downloads as %1").arg(name))
                    } else {
                        banner.show(qsTr("Could not save the PDF"))
                    }
                }
            }
            MenuItem {
                text: page.marking ? qsTr("Cancel area") : qsTr("Read only an area")
                enabled: !ocr.busy && ocr.imageSize.width > 0
                onClicked: {
                    page.marking = !page.marking
                    page.markedRegion = Qt.rect(0, 0, 0, 0)
                    if (page.marking) {
                        banner.show(qsTr("Drag across the part you want"))
                    }
                }
            }
            MenuItem {
                text: qsTr("Rotate the view")
                onClicked: page.viewRotation = (page.viewRotation + 90) % 360
            }
            MenuItem {
                text: qsTr("Read again")
                enabled: !ocr.busy
                onClicked: page.readWhole()
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

            // The language belongs here, not only in Settings: it is the biggest
            // single lever on quality, and the moment you discover it was wrong is
            // the moment you are looking at a bad result.
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
                            page.readWhole()
                        }
                    }
                }
            }

            // Holds the rotated canvas. A quarter turn swaps the canvas's width
            // and height, and a Column has to be told how much room that takes.
            Item {
                id: frame

                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin

                readonly property bool quarterTurned: page.viewRotation % 180 !== 0
                height: quarterTurned ? canvas.width : canvas.height

                Item {
                    id: canvas

                    anchors.centerIn: parent
                    width: frame.quarterTurned ? frame.height : frame.width

                    // Height from the image's own proportions, never from
                    // paintedHeight: that is what the Image drew, which depends on
                    // the Image's height, which - anchored to fill this - depends
                    // on this height. Qt calls that a binding loop, says so once,
                    // and then keeps re-evaluating the layout, which on a device
                    // looks like the page has frozen.
                    //
                    // implicitWidth/implicitHeight are the loaded image's own
                    // dimensions, outputs of the loader, so nothing feeds back.
                    readonly property real aspect: photo.implicitWidth > 0
                            ? photo.implicitHeight / photo.implicitWidth : 1

                    height: width * aspect

                    // The image fills the width exactly, so one image pixel is
                    // this many screen pixels and there is no letterboxing.
                    readonly property real ratio: ocr.imageSize.width > 0
                            ? width / ocr.imageSize.width : 1

                    readonly property real offsetX: 0

                    rotation: page.viewRotation

                    Behavior on rotation {
                        NumberAnimation {
                            duration: Tokens.durBase
                            easing.type: Tokens.easingType
                        }
                    }

                    Image {
                        id: photo

                        anchors.fill: parent
                        source: page.imageUrl
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true

                        // The camera tags a portrait photo rather than rotating
                        // its pixels, and QML ignores that tag unless asked.
                        // Without this the preview lies on its side while
                        // OcrEngine - which does apply the tag - reports boxes for
                        // the upright image, so every box lands wrong.
                        autoTransform: true

                        // Decoded at the size it is drawn, not the camera's: a 12
                        // megapixel photo at full resolution is ~48MB of pixels,
                        // which is how an image viewer gets itself killed.
                        sourceSize.width: page.width
                    }

                    // Where the text is. Without this the photo looks inert and
                    // nothing suggests it can be touched.
                    //
                    // Per line, not per word: a page holds a couple of thousand
                    // words and a few dozen lines, and a Repeater over the former
                    // stutters.
                    Repeater {
                        model: page.selection.valid === true ? [] : ocr.lines

                        delegate: Rectangle {
                            x: canvas.offsetX + modelData.x * canvas.ratio
                            y: modelData.y * canvas.ratio
                            width: modelData.width * canvas.ratio
                            height: modelData.height * canvas.ratio
                            radius: Tokens.hairline * 2

                            // Weak lines tinted, strong ones barely marked: the
                            // confidence map, pointing at what to read twice
                            // without putting a number on anything.
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

                    // The blocks, outlined so they can be picked. Only when there
                    // is a choice to make: one block is the whole photo, and
                    // outlining it says nothing.
                    Repeater {
                        model: (page.selection.valid === true || ocr.blocks.length < 2)
                               ? [] : ocr.blocks

                        delegate: Rectangle {
                            // Above the whole-photo tap handler, which is declared
                            // after these and would otherwise swallow every tap
                            // meant for a block or a doubtful word.
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
                                enabled: !page.marking
                                onClicked: {
                                    page.onlyBlock = page.onlyBlock === modelData.block
                                                     ? -1 : modelData.block
                                }
                            }
                        }
                    }

                    // The words the recogniser doubted, tinted and tappable. Only
                    // the doubtful ones: the rest are not worth an item each.
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
                                enabled: !page.marking
                                onClicked: page.editWord(modelData.index, modelData.text)
                            }
                        }
                    }

                    // The area being marked out, while a drag is in progress.
                    Rectangle {
                        z: 3
                        visible: page.marking && page.markedRegion.width > 0

                        x: canvas.offsetX + page.markedRegion.x * canvas.ratio
                        y: page.markedRegion.y * canvas.ratio
                        width: page.markedRegion.width * canvas.ratio
                        height: page.markedRegion.height * canvas.ratio

                        color: Theme.rgba(Tokens.onColor, 0.20)
                        border.width: Tokens.hairline * 2
                        border.color: Tokens.onColor
                    }

                    // The current selection.
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

                    // Marking out an area. Above everything while it is on, inert
                    // otherwise, so it never competes with the tap gestures.
                    MouseArea {
                        id: marker

                        z: 4
                        anchors.fill: parent
                        enabled: page.marking

                        property real startX: 0
                        property real startY: 0

                        onPressed: {
                            startX = (mouse.x - canvas.offsetX) / canvas.ratio
                            startY = mouse.y / canvas.ratio
                            page.markedRegion = Qt.rect(startX, startY, 0, 0)
                        }

                        onPositionChanged: {
                            var x = (mouse.x - canvas.offsetX) / canvas.ratio
                            var y = mouse.y / canvas.ratio
                            page.markedRegion = Qt.rect(Math.min(startX, x),
                                                        Math.min(startY, y),
                                                        Math.abs(x - startX),
                                                        Math.abs(y - startY))
                        }

                        onReleased: {
                            // A tap rather than a drag: too small to be an area,
                            // and reading it would return nothing and look broken.
                            if (page.markedRegion.width < 20
                                    || page.markedRegion.height < 20) {
                                page.markedRegion = Qt.rect(0, 0, 0, 0)
                                return
                            }
                            page.marking = false
                            page.onlyBlock = -1
                            page.selection = ({ valid: false })
                            page.readRegion(page.markedRegion)
                        }
                    }

                    // Tap to select. Declared last, so it sits under the overlays
                    // above, which raise themselves with z.
                    MouseArea {
                        anchors.fill: parent
                        enabled: ocr.wordCount > 0 && !page.marking

                        onClicked: {
                            var imageX = (mouse.x - canvas.offsetX) / canvas.ratio
                            var imageY = mouse.y / canvas.ratio

                            // Inside what is already selected means "give me more
                            // of it"; anywhere else starts again at a single word.
                            // Two taps in the same place therefore walk outwards,
                            // which is the entire interaction.
                            var inside = page.selection.valid === true
                                    && imageX >= page.selection.x
                                    && imageX <= page.selection.x + page.selection.width
                                    && imageY >= page.selection.y
                                    && imageY <= page.selection.y + page.selection.height

                            page.currentScope = inside ? ocr.growScope(page.currentScope)
                                                       : ocr.scopeWord()

                            // A fingertip is wider than a word box is tall, so a
                            // miss falls back to the nearest word within this much.
                            var tolerance = Math.round(Theme.itemSizeExtraSmall
                                                       / canvas.ratio)

                            page.selection = ocr.selectAt(Math.round(imageX),
                                                          Math.round(imageY),
                                                          page.currentScope,
                                                          tolerance)
                        }
                    }
                }
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: ocr.busy
                size: BusyIndicatorSize.Large
                // A Column still reserves space for an item that is merely not
                // running, which leaves a hole under the photo for the whole time
                // the result is on screen.
                visible: ocr.busy
            }

            // Mean confidence, stated plainly rather than as a bar. Below about 70
            // the recogniser is usually wrong rather than slightly wrong, and the
            // useful advice then is to retake the photo - so that is what it says.
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
                               ? (modelData.checksumValid ? "✓" : "!")
                               : "·"

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
                        // from and the checksums follow.
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

    Banner {
        id: banner
    }
}
