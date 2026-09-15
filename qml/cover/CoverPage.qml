import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// The cover: what Moji looks like when it is minimised.
//
// Three states, because the app has three. Idle, when nothing has been read yet
// and there is only an identity to show. Reading, which is the state that earns
// this file - recognition takes a second or three on a photograph and longer on a
// document, so somebody who minimises mid-read wants to know whether it is still
// going. And read, showing what came out.
//
// Two actions, which is the platform's maximum: the camera, because photographing
// a page is the whole reason the app exists and this saves opening it first; and
// share, which is a complete route out to another app rather than the clipboard
// and a second journey.
CoverBackground {
    id: cover

    // Emitted, not acted on. An id is only visible inside the document that
    // declares it, so this file cannot call anything on the ApplicationWindow -
    // and reaching for it through some global would be the sort of coupling that
    // makes a cover impossible to move. harbour-moji.qml instantiates this and
    // connects both.
    signal cameraRequested()
    signal shareRequested()

    // Anyone can glance at a home screen, and this app knows better than most
    // which readings are worth not putting there: sensitiveCount is the same
    // count that decides whether the result page offers to black numbers out. A
    // page with an IBAN, a card number or a passport code on it says so instead
    // of showing itself.
    readonly property bool hasReading: ocr.wordCount > 0
    readonly property bool isPrivate: ocr.sensitiveCount > 0

    readonly property string firstLine:
        ocr.editedText.split("\n")[0].replace(/\s+/g, " ")

    Rectangle {
        anchors.fill: parent
        color: Tokens.pageColor
        // Ambience mode keeps Silica's own translucent cover, which is what makes
        // a minimised app look like part of the system rather than a sticker on
        // top of it.
        visible: !Tokens.ambient
    }

    // The character the app is named for, behind everything and barely there.
    // 文字 is Japanese for a written character; the About page says so, and a
    // watermark is the one place an app may be decorative without being noisy.
    Label {
        anchors {
            centerIn: parent
            verticalCenterOffset: -Theme.paddingLarge
        }
        text: "文"
        font.pixelSize: cover.height * 0.6
        color: Tokens.primaryColor
        opacity: cover.hasReading || ocr.busy ? 0.06 : 0.12
    }

    Column {
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: Theme.paddingMedium
        }
        spacing: Theme.paddingSmall

        Row {
            width: parent.width
            spacing: Theme.paddingSmall
            visible: cover.hasReading || ocr.busy

            Image {
                anchors.verticalCenter: parent.verticalCenter
                // The 86px icon is the smallest installed, and this draws it
                // smaller still - sourceSize keeps it from being decoded at full
                // size for a thumbnail the height of a line of small text.
                source: "/usr/share/icons/hicolor/86x86/apps/harbour-moji.png"
                width: Theme.fontSizeExtraSmall * 1.4
                height: width
                sourceSize.width: 86
                sourceSize.height: 86
                smooth: true
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - x
                text: qsTr("Moji OCR")
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Tokens.secondaryColor
                truncationMode: TruncationMode.Fade
            }
        }

        // Reading. A count that is still climbing is the one thing worth saying
        // while it happens.
        Label {
            width: parent.width
            text: qsTr("Reading…")
            font.pixelSize: Theme.fontSizeMedium
            color: Tokens.accentColor
            visible: ocr.busy
        }

        Label {
            width: parent.width
            text: qsTr("%1 words").arg(ocr.wordCount)
            font.pixelSize: Theme.fontSizeLarge
            color: Tokens.primaryColor
            visible: cover.hasReading && !ocr.busy
        }

        // Below about 70 the recogniser is usually wrong rather than slightly
        // wrong, which is worth knowing before opening the app to look.
        Label {
            width: parent.width
            text: qsTr("%1%").arg(Math.round(ocr.confidence))
            font.pixelSize: Theme.fontSizeExtraSmall
            color: ocr.confidence < 70 ? Theme.errorColor : Tokens.secondaryColor
            visible: cover.hasReading && !ocr.busy
        }
    }

    // What was read, or the fact that it is not for a home screen.
    Label {
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: Theme.paddingMedium
            bottomMargin: Theme.itemSizeSmall
        }

        visible: cover.hasReading && !ocr.busy
        wrapMode: Text.Wrap
        maximumLineCount: 3
        elide: Text.ElideRight
        font.pixelSize: Theme.fontSizeExtraSmall
        color: cover.isPrivate ? Tokens.secondaryColor : Tokens.primaryColor
        font.italic: cover.isPrivate

        // Plain text, always. This is recognised text - whatever was in front of
        // the camera - and scripts/check_qml.py fails the build on any other
        // textFormat for exactly that reason.
        textFormat: Text.PlainText
        text: cover.isPrivate ? qsTr("Contains private numbers")
                              : cover.firstLine
    }

    // Idle: nothing has been read, so there is nothing to report and the name
    // sits under the watermark instead.
    Row {
        anchors {
            horizontalCenter: parent.horizontalCenter
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: cover.height * 0.22
        }
        spacing: Theme.paddingSmall
        visible: !cover.hasReading && !ocr.busy

        Image {
            anchors.verticalCenter: parent.verticalCenter
            source: "/usr/share/icons/hicolor/86x86/apps/harbour-moji.png"
            width: Theme.fontSizeSmall * 1.4
            height: width
            sourceSize.width: 86
            sourceSize.height: 86
            smooth: true
        }

        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Moji OCR")
            font.pixelSize: Theme.fontSizeSmall
            color: Tokens.secondaryColor
        }
    }

    CoverActionList {
        id: actions

        CoverAction {
            // Straight to the viewfinder. The point of a cover action is to skip
            // the app, and every other way in starts on the main page.
            iconSource: "image://theme/icon-cover-camera"
            onTriggered: cover.cameraRequested()
        }

        CoverAction {
            iconSource: "image://theme/icon-cover-message"
            onTriggered: cover.shareRequested()
        }
    }
}
