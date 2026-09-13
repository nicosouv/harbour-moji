import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// The polite notification: a dark rounded bar that rises from the bottom, says
// one thing, and leaves.
//
// This was webOS's most copied invention, and the reason is that it informs
// without taking the screen: nothing is blocked, nothing has to be dismissed, and
// whatever the user was doing carries on underneath. That makes it the right
// answer for "copied", "saved", "nothing found there" - the whole class of message
// an app is tempted to put in a dialog.
//
// Dark in every theme, ambience included. It reads as the system speaking rather
// than as part of the page, and a banner tinted to match the page it floats over
// stops looking like a separate voice.
//
// Anchor it to the Page, not to a Flickable's content, or it will scroll away:
//
//   Banner { id: banner }
//   ...
//   onClicked: banner.show(qsTr("Copied"))
Item {
    id: root

    property int duration: 3500

    readonly property bool raised: bar.y < root.height

    function show(message) {
        label.text = message
        bar.raised = true
        hideTimer.restart()
    }

    function hide() {
        hideTimer.stop()
        bar.raised = false
    }

    anchors.fill: parent

    // Nothing under the banner should stop responding just because it is on
    // screen; only the bar itself takes touches.
    enabled: false

    Rectangle {
        id: bar

        property bool raised: false

        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 2 * Theme.horizontalPageMargin
        height: Math.max(Theme.itemSizeSmall,
                         label.paintedHeight + 2 * Theme.paddingLarge)
        radius: Tokens.panelRadius
        color: Tokens.bannerColor

        y: raised ? parent.height - height - Theme.paddingLarge : parent.height

        // 250ms: long enough to be seen arriving, short enough not to be waited
        // for. Decelerating, and with no overshoot - a banner that bounces reads
        // as a toy.
        Behavior on y {
            NumberAnimation {
                duration: Tokens.durSlow
                easing.type: Tokens.easingType
            }
        }

        Label {
            id: label

            anchors {
                left: parent.left
                right: parent.right
                leftMargin: Theme.paddingLarge
                rightMargin: Theme.paddingLarge
                verticalCenter: parent.verticalCenter
            }

            wrapMode: Text.Wrap
            maximumLineCount: 3
            font.pixelSize: Theme.fontSizeSmall
            color: Tokens.bannerTextColor
        }

        MouseArea {
            anchors.fill: parent
            enabled: bar.raised
            onClicked: root.hide()
        }
    }

    Timer {
        id: hideTimer

        interval: root.duration
        onTriggered: bar.raised = false
    }
}
