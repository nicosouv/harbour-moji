import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// The webOS switch, rebuilt from Onyx's own CSS rather than from memory.
//
// Four details carry the recognition, and all four are easy to get wrong:
//
//   - The label lives *inside* the track, on the side the knob is not. Onyx
//     floats the knob right and the "on" label left, then swaps both. Only one
//     label is ever showing.
//   - The corners are barely rounded: 3px on a 32px track. A full pill reads as
//     iOS and throws the whole homage away.
//   - The track is green when on (#8BBA3D), grey when off. Not the platform
//     accent - this specific green is the thing people remember.
//   - It answers a swipe as well as a tap, in the direction opposite its
//     current state, which is what Mojo's own docs specify.
//
// Named ToggleSwitch, not Toggle or Switch: Silica already has a Switch.
Item {
    id: root

    property bool checked: false

    // Passed in rather than translated here: a module that calls qsTr() owes
    // five catalogue entries to every app that imports it.
    property string onText: "On"
    property string offText: "Off"

    readonly property real knobSize: Tokens.controlHeight - 2 * Tokens.controlMargin

    implicitHeight: Tokens.controlHeight
    implicitWidth: Tokens.controlHeight * 2

    opacity: enabled ? 1.0 : Tokens.disabledOpacity

    Rectangle {
        id: track

        anchors.fill: parent
        radius: Tokens.switchRadius
        color: root.checked ? Tokens.onColor : Tokens.offColor

        Behavior on color {
            ColorAnimation {
                duration: Tokens.durFast
                easing.type: Tokens.easingType
            }
        }

        // Onyx renders both labels and shows one. Same here: no reflow when the
        // state changes, so the knob is the only thing that moves.
        Label {
            anchors {
                left: parent.left
                leftMargin: Theme.paddingMedium
                verticalCenter: parent.verticalCenter
            }
            visible: root.checked && !Tokens.ambient
            text: root.onText
            font.pixelSize: Theme.fontSizeExtraSmall
            font.bold: true
            font.capitalization: Font.AllUppercase
            color: "#FFFFFF"
        }

        Label {
            anchors {
                right: parent.right
                rightMargin: Theme.paddingMedium
                verticalCenter: parent.verticalCenter
            }
            visible: !root.checked && !Tokens.ambient
            text: root.offText
            font.pixelSize: Theme.fontSizeExtraSmall
            font.bold: true
            font.capitalization: Font.AllUppercase
            color: "#FFFFFF"
        }

        Rectangle {
            id: knob

            y: Tokens.controlMargin
            width: root.knobSize
            height: root.knobSize
            radius: Tokens.switchRadius
            color: Tokens.knobColor

            x: root.checked ? parent.width - width - Tokens.controlMargin
                            : Tokens.controlMargin

            Behavior on x {
                NumberAnimation {
                    duration: Tokens.durFast
                    easing.type: Tokens.easingType
                }
            }
        }
    }

    MouseArea {
        id: area

        anchors.fill: parent
        enabled: root.enabled

        property real pressX: 0
        property bool dragged: false

        // Past this the gesture is a swipe and the release must not also
        // toggle, or a deliberate drag would land back where it started.
        readonly property real threshold: root.knobSize * 0.3

        onPressed: {
            pressX = mouseX
            dragged = false
        }

        onPositionChanged: {
            if (!pressed || Math.abs(mouseX - pressX) < threshold) {
                return
            }
            dragged = true
            // Dragging right means on, left means off - which is Mojo's "swiped
            // in the direction opposite its current state" seen from the other
            // side, and it also makes a drag that overshoots and comes back
            // settle correctly.
            root.checked = mouseX > pressX
        }

        onReleased: {
            if (!dragged) {
                root.checked = !root.checked
            }
        }
    }
}
