// 2.5, not 2.0: Image.autoTransform arrived with Qt 5.5 and a QML import states
// the version whose API it is asking for, not the one the device happens to have.
// Sailfish ships Qt 5.6, so 2.5 resolves - but importing 2.0 and using a 2.5
// property fails the whole file at load with "is not available in QtQuick 2.0".
import QtQuick 2.5
import Sailfish.Silica 1.0
import Mochi 1.0
import "../components"

// The photo, with the recognised text available by touching it.
//
// Tapping a word selects that word; tapping inside what is already selected
// widens it to the line, then the paragraph, then the block. That is the whole
// gesture, and it works because the recogniser reported a real structure rather
// than a flat list of strings - see src/textlayout.h.
Page {
    id: page

    property url imageUrl

    // Set when this page is one page of a PDF, so the next one is a tap away
    // instead of a walk back to the main page and through the picker again -
    // which is what reading a two-page document used to cost.
    property url documentUrl
    property int documentPage: 0
    property int documentPages: 0

    readonly property bool hasNextPage:
        documentPages > 1 && documentPage < documentPages

    // Set when the page is opened from the history: the text was kept, so showing
    // it costs nothing while recognising the photo again costs seconds and can
    // come out differently. The photo is still shown, and "Read again" is there
    // for anyone who wants a fresh reading - but it is asked for, not assumed.
    property string storedText: ""
    readonly property bool showingStored: storedText !== "" && ocr.wordCount === 0

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
    //
    // Set from the reading when one arrives, because the recogniser is the only
    // thing in the app that knows which way up the photograph is. The Sailfish
    // camera tags every frame it writes as needing no rotation - the sensor's
    // landscape frame, however the phone was held - so a page photographed in
    // portrait arrives on its side and stays there. The text came out right
    // anyway, the picture above it did not, and that is the whole of the
    // complaint. A quarter turn of the view costs nothing and fixes it.
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

    function runAction(id) {
        if (id === "area") {
            page.marking = !page.marking
            page.markedRegion = Qt.rect(0, 0, 0, 0)
            if (page.marking) {
                banner.show(qsTr("Drag a box around the part you want"))
            }
        } else if (id === "rotate") {
            page.viewRotation = (page.viewRotation + 90) % 360
        } else if (id === "pdf") {
            var name = page.imageUrl.toString().split("/").pop()
                           .replace(/\.[^.]+$/, "") + ".pdf"
            var target = StandardPaths.download + "/" + name
            if (ocr.exportPdf(page.imageUrl, target.replace("file://", ""))) {
                banner.show(qsTr("Saved to Downloads as %1").arg(name))
            } else {
                banner.show(qsTr("Could not save the PDF"))
            }
        } else if (id === "copy") {
            Clipboard.text = page.showingStored ? page.storedText : ocr.editedText
            banner.show(qsTr("All text copied"))
        } else if (id === "share") {
            // The reason most people photograph a page is to send what is on it
            // somewhere else, and until now the only way out was the clipboard
            // and another app. The Sharing permission has been in the sandbox
            // since the first release with nothing asking for it.
            sharer.share(page.showingStored ? page.storedText : ocr.editedText,
                         qsTr("Recognised text"))
        } else if (id === "nextpage") {
            page.readNextPage()
        } else if (id === "again") {
            page.readWhole()
        } else if (id === "hide") {
            var safeName = page.imageUrl.toString().split("/").pop()
                               .replace(/\.[^.]+$/, "") + "-hidden.jpg"
            var safeWhere = StandardPaths.download + "/" + safeName
            if (ocr.exportRedacted(page.imageUrl, safeWhere.replace("file://", ""))) {
                banner.show(qsTr("Saved to Downloads as %1").arg(safeName))
            } else {
                banner.show(qsTr("Could not save the copy"))
            }
        }
    }

    // The next page of the same document, in place of this one.
    //
    // Replaced rather than pushed: a forty-page document would otherwise leave
    // forty result pages on the stack, each holding a photograph.
    function readNextPage() {
        var next = page.documentPage + 1
        var rendered = pdf.renderPage(page.documentUrl, next)
        if (rendered == "") {
            banner.show(pdf.lastError())
            return
        }

        pageStack.completeAnimation()
        pageStack.replace(Qt.resolvedUrl("ResultPage.qml"),
                          { imageUrl: rendered,
                            documentUrl: page.documentUrl,
                            documentPage: next,
                            documentPages: page.documentPages })
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
            // Show it the way it was read. Assigning rather than binding, so the
            // rotate button still wins afterwards: a binding would snap the view
            // back on the next reading, and someone who has just turned the page
            // by hand has said what they want.
            page.viewRotation = ocr.orientation

            history.remember(page.imageUrl.toString().replace("file://", ""),
                             settings.tesseractLanguages,
                             ocr.wordCount, ocr.confidence, ocr.editedText)
        }
    }

    Component.onCompleted: {
        if (imageUrl != "" && storedText === "") {
            readWhole()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Tokens.pageColor
        visible: !Tokens.ambient
    }

    SilicaFlickable {
        id: flickable

        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge * 2

        // A Flickable claims any drag that starts inside it, which is right for a
        // page of text and exactly wrong while someone is trying to draw a box on
        // the photo. Marking an area turns the scrolling off for the duration -
        // preventStealing on the handler alone is not enough, because the
        // Flickable takes the gesture before the handler is ever asked.
        interactive: !page.marking

        VerticalScrollDecorator { }

        // Only what is global to the app. Every action that belongs to *this*
        // page is a row in the Actions group below, which is Mojo's rule and the
        // reason Mochi exists: the create action lived as the last row of a list,
        // not behind a floating button or a menu you have to know about. A pulley
        // stuffed with the page's own verbs is the Silica habit Mochi replaces.
        PullDownMenu {
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
        }

        // Tapping anywhere that is not the text closes the keyboard.
        //
        // Declared before the Column, so it sits underneath every other item in
        // the flickable: a tap on the photo, on one of the action buttons or on a
        // row is taken by that item and never arrives here. Only a tap that hit
        // nothing at all falls through.
        //
        // Enabled only while the field actually has focus, so it is inert the rest
        // of the time and cannot swallow anything. Without it the keyboard stays
        // up over the middle of the page with no obvious way to put it away -
        // Silica has no "done" affordance for a TextEdit, because a text field on
        // this platform is normally the whole point of the page, and here it is
        // one panel among several.
        MouseArea {
            anchors.fill: column
            enabled: rawText.activeFocus

            onClicked: {
                // Clearing focus is what actually dismisses it; hiding the panel
                // as well is belt and braces for the case where the focus scope
                // holds on, which cannot be tried from here.
                rawText.focus = false
                Qt.inputMethod.hide()
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
                             : (page.documentPages > 1
                                ? qsTr("Page %1 of %2 — %3 words")
                                  .arg(page.documentPage).arg(page.documentPages)
                                  .arg(ocr.wordCount)
                                : (ocr.wordCount > 0
                                   ? qsTr("%1 words").arg(ocr.wordCount)
                                   : (page.showingStored
                                      ? qsTr("Read earlier")
                                      : ocr.lastError)))
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

                // How wide the canvas is, derived from this item's *width* and the
                // photo's proportions - never from this item's height.
                //
                // The height used to be canvas.width while canvas.width was
                // frame.height, which is a binding loop: Qt says so once and then
                // re-evaluates the layout forever, which on a device does not look
                // like a warning, it looks like the page has stopped. It was only
                // reachable by tapping "rotate the view", which is presumably why
                // it survived; the view now turns itself to whatever way up the
                // page was read, so it would have been on the ordinary path.
                //
                // Turned a quarter, the canvas lies on its side: its own height is
                // what has to fit across the frame, so its width is the frame's
                // width divided by the aspect rather than multiplied by it.
                readonly property real canvasWidth: quarterTurned
                        ? (canvas.aspect > 0 ? width / canvas.aspect : width)
                        : width

                height: quarterTurned ? canvasWidth : canvasWidth * canvas.aspect

                Item {
                    id: canvas

                    anchors.centerIn: parent
                    width: frame.canvasWidth

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

                        // Belt and braces: refuse to hand the gesture back once it
                        // has started, whatever is above.
                        preventStealing: true

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

            // A bar, not five rows with subtitles. The rows read well on their own
            // and take more of the screen than the photograph they act on, which
            // inverts what the page is about - Mojo's toolbars were a strip of
            // glyphs at the edge for exactly this reason.
            //
            // Still not the pulley: these are visible, and every one of them says
            // what it is before it does it.
            //
            // Two taps, not one. A row of six glyphs is unreadable until you have
            // learnt it, and the way to learn it used to be a press-and-hold -
            // which is a gesture you have to already know about to discover what
            // the button does. So the first tap names the verb and arms it, and
            // the second runs it. Nothing fires from a tap that was only a
            // question, which matters most for the two verbs that write a file.
            Item {
                id: actionBar

                width: parent.width
                height: actions.height
                visible: ocr.wordCount > 0 || ocr.lastError !== ""
                         || page.showingStored

                // Which verb is waiting for its second tap, held by id and not by
                // index: the bar grows a button when the page turns out to have a
                // table, and another when it has a number worth hiding, so the
                // index armed a moment ago can be a different verb now.
                property string armedId: ""

                // Disarmed after a few seconds. A button left armed is a button
                // that fires from what its owner thinks is a first tap, and the
                // one that exports a PDF should not be reachable that way.
                Timer {
                    id: disarm

                    interval: 4000
                    onTriggered: actionBar.armedId = ""
                }

                function tap(id) {
                    if (actionBar.armedId === id) {
                        actionBar.armedId = ""
                        disarm.stop()
                        page.runAction(id)
                    } else {
                        actionBar.armedId = id
                        disarm.restart()
                    }
                }

                // A Grid that wraps, not a Row.
                //
                // A Row centred with no width constraint does not clip, it
                // overflows both edges, and the buttons at the ends go off the
                // screen where nothing can reach them. That shipped: on a 1032px
                // screen at pixelRatio 1.5 a button is 150 and the gap 36, so five
                // verbs fit in 894 and six need 1080. Adding "share" in v0.1.18
                // crossed the line; a PDF page carrying an account number wants
                // eight, which is 1452 and loses one at each end entirely.
                //
                // Columns are counted from the width actually available rather
                // than fixed, so a wider screen uses one row and a narrower one
                // uses three without either being told about the other.
                Grid {
                    id: actions

                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: Theme.paddingLarge

                    readonly property real cellSize: Theme.itemSizeSmall
                    columns: Math.max(1, Math.min(
                        shown.length,
                        Math.floor((actionBar.width + spacing) / (cellSize + spacing))))

                    property var verbs: [
                        { glyph: "\u2b1a", name: qsTr("Read only an area"), id: "area" },
                        { glyph: "\u21bb", name: qsTr("Rotate the view"),   id: "rotate" },
                        { glyph: "\u21e9", name: qsTr("Save as PDF"),       id: "pdf" },
                        { glyph: "\u29c9", name: qsTr("Copy all text"),     id: "copy" },
                        { glyph: "\u27a6", name: qsTr("Share the text"),    id: "share" },
                        { glyph: "\u21ba", name: qsTr("Read again"),        id: "again" }
                    ]

                    // Only for a document that has a next page, for the same
                    // reason the redaction button is conditional.
                    property var nextPageVerb: ({ glyph: "\u21e5",
                                                  name: qsTr("Read the next page"),
                                                  id: "nextpage" })

                    // Offered only when there is something to hide: a page with no
                    // account number must not offer to blank one, and a button
                    // that is usually greyed out is just clutter.
                    property var hideVerb: ({ glyph: "\u2588",
                                              name: qsTr("Hide the private numbers"),
                                              id: "hide" })

                    // Named once and read by both the Repeater and the tooltip,
                    // so the two cannot disagree about which button is where.
                    property var shown: {
                        var list = actions.verbs.slice()
                        if (page.hasNextPage) {
                            list.push(actions.nextPageVerb)
                        }
                        if (ocr.sensitiveCount > 0) {
                            list.push(actions.hideVerb)
                        }
                        return list
                    }

                    Repeater {
                        model: actions.shown

                        delegate: MouseArea {
                            id: verb

                            width: Theme.itemSizeSmall
                            height: Theme.itemSizeSmall

                            readonly property bool armed:
                                actionBar.armedId === modelData.id

                            // Lit for the verb that is on, as well as for the verb
                            // that is armed: "read only an area" stays on between
                            // taps and has to look it.
                            readonly property bool lit:
                                armed || (modelData.id === "area" && page.marking)

                            onClicked: actionBar.tap(modelData.id)

                            Rectangle {
                                anchors.fill: parent
                                radius: Tokens.controlRadius
                                color: verb.pressed ? Tokens.pressedColor
                                     : verb.armed ? Tokens.selectionColor
                                                  : Tokens.panelColor
                                border.width: Tokens.hairline
                                border.color: verb.lit ? Tokens.onColor
                                                       : Tokens.separatorColor

                                Label {
                                    anchors.centerIn: parent
                                    text: modelData.glyph
                                    font.pixelSize: Theme.fontSizeLarge
                                    color: verb.lit ? Tokens.onColor
                                                    : Tokens.accentColor
                                }
                            }
                        }
                    }
                }

                // The tooltip, over the button it belongs to rather than at the
                // bottom of the screen where the Banner lives: a message that has
                // to be traced back to one of six identical squares is not a
                // tooltip. It sits above the bar, over the foot of the photograph,
                // and no page here clips, so it is free to leave the row.
                Rectangle {
                    id: tip

                    readonly property int slot: {
                        for (var i = 0; i < actions.shown.length; ++i) {
                            if (actions.shown[i].id === actionBar.armedId) {
                                return i
                            }
                        }
                        return -1
                    }

                    // Measured from the grid's own geometry rather than by
                    // reaching into a delegate: the buttons are one fixed size at
                    // one fixed spacing, so where the nth one sits is arithmetic -
                    // now in two dimensions, because the grid wraps.
                    readonly property int slotColumn:
                        actions.columns > 0 ? tip.slot % actions.columns : 0
                    readonly property int slotRow:
                        actions.columns > 0 ? Math.floor(tip.slot / actions.columns) : 0

                    readonly property real slotCentre:
                        actions.x + tip.slotColumn * (actions.cellSize + actions.spacing)
                        + actions.cellSize / 2

                    // The top of the row the armed button is on, so a tooltip for
                    // a button on the second row does not float above the first.
                    readonly property real slotTop:
                        tip.slotRow * (actions.cellSize + actions.spacing)

                    visible: slot >= 0
                    width: Math.min(actionBar.width,
                                    tipColumn.width + 2 * Theme.paddingLarge)
                    height: tipColumn.height + 2 * Theme.paddingMedium
                    radius: Tokens.panelRadius
                    color: Tokens.bannerColor

                    // Clamped into the page: the first and last buttons are near
                    // the edges and a bubble centred on them would hang off.
                    x: Math.max(0, Math.min(actionBar.width - width,
                                            slotCentre - width / 2))
                    y: slotTop - height - Theme.paddingSmall

                    Column {
                        id: tipColumn

                        anchors.centerIn: parent
                        spacing: Theme.paddingSmall / 2

                        Label {
                            text: tip.slot >= 0 ? actions.shown[tip.slot].name : ""
                            font.pixelSize: Theme.fontSizeSmall
                            color: Tokens.bannerTextColor
                        }

                        Label {
                            text: qsTr("Tap again to do it")
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Tokens.bannerTextColor
                            opacity: Tokens.disabledOpacity
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
                visible: ocr.wordCount > 0
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Tokens.secondaryColor
                text: qsTr("The text below can be edited.")
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
                visible: ocr.sensitiveCount > 0
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Tokens.secondaryColor
                text: qsTr("This page carries account or card numbers. The black square saves a copy with them painted out.")
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
                title: page.onlyBlock >= 0
                       ? qsTr("Selected block")
                       : (ocr.edited ? qsTr("All text (edited)") : qsTr("All text"))
                visible: ocr.wordCount > 0 || page.showingStored

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

                        // Editable, and selectable. Correcting one word by tapping
                        // it on the photo writes back to that word, so the boxes
                        // and the confidence follow; editing here cannot do that -
                        // there is no telling which word a new sentence belongs to
                        // - so it amends the text alone. What is copied, shared and
                        // remembered comes from here; what is drawn on the photo
                        // still comes from the words.
                        readOnly: page.onlyBlock >= 0
                        selectByMouse: true
                        persistentSelection: true
                        inputMethodHints: Qt.ImhNoAutoUppercase

                        onTextChanged: {
                            if (page.onlyBlock < 0) {
                                ocr.editedText = text
                            }
                        }

                        // Plain text, never RichText. This string is whatever was
                        // in front of the camera, so rendering it as markup would
                        // let a photographed <img> tag decide what this device
                        // fetches. scripts/check_qml.py fails the build on it.
                        textFormat: TextEdit.PlainText

                        // Amended text when the whole page is shown; the block's
                        // own text when one is picked out, and then read-only,
                        // because writing a block back is a different problem.
                        text: page.showingStored
                              ? page.storedText
                              : (page.onlyBlock < 0
                                 ? ocr.editedText
                                 : ocr.textOfBlock(page.onlyBlock))
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

    // Shared with the cover, so the two cannot drift apart. It says nothing
    // itself; the page decides where a failure is shown, and here that is the
    // banner.
    ShareHelper {
        id: sharer

        onFailed: banner.show(reason)
    }
}
