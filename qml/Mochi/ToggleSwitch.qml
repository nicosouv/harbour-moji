import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// A boolean switch, in whichever vocabulary the theme is speaking.
//
// In the Mochi themes this is the webOS switch, rebuilt from Onyx's own CSS
// rather than from memory. Four details carry the recognition, and all four are
// easy to get wrong:
//
//   - The label lives *inside* the track, on the side the knob is not. Onyx
//     floats the knob right and the "on" label left, then swaps both. Only one
//     label is ever showing.
//   - The corners are barely rounded: 3px on a 32px track. A full pill reads as
//     iOS and throws the whole homage away.
//   - The track is green when on (#8BBA3D), grey when off.
//   - It answers a swipe as well as a tap, in the direction opposite its current
//     state, which is what Mojo's own docs specify.
//
// In ambience mode it is a Silica Switch instead - the real one, not a repaint.
// Someone who left the theme on "ambience" is asking for an app that looks and
// behaves like the rest of their phone, and a lookalike gets the details wrong in
// ways that are felt rather than seen.
//
// Named ToggleSwitch, not Toggle or Switch: Silica already has a Switch, and a
// component in a module that shadows a platform type wins over it everywhere.
Item {
    id: root

    property bool checked: false

    // Passed in rather than translated here: a module that calls qsTr() owes a
    // catalogue entry to every app that imports it. Unused in ambience mode,
    // where Silica switches carry no label.
    property string onText: "On"
    property string offText: "Off"

    implicitWidth: variant.item ? variant.item.width : Tokens.controlHeight * 2
    implicitHeight: variant.item ? variant.item.height : Tokens.controlHeight

    opacity: enabled ? 1.0 : Tokens.disabledOpacity

    Loader {
        id: variant

        anchors.centerIn: parent
        sourceComponent: Tokens.ambient ? nativeVariant : mochiVariant
    }

    Component {
        id: nativeVariant

        Switch {
            // Silica would toggle itself and then the binding below would fight
            // it. Driving the value from one place keeps the two in step.
            automaticCheck: false
            checked: root.checked
            enabled: root.enabled
            onClicked: root.checked = !root.checked
        }
    }

    Component {
        id: mochiVariant

        // Sized from the tokens, never from root, or root's implicit size would
        // depend on this while this depended on root.
        Item {
            id: body

            width: Tokens.controlHeight * 2
            height: Tokens.controlHeight

            readonly property real knobSize: Tokens.controlHeight
                                             - 2 * Tokens.controlMargin

            Rectangle {
                id: track

                anchors.fill: parent
                radius: Tokens.controlRadius
                color: root.checked ? Tokens.onColor : Tokens.offColor

                Behavior on color {
                    ColorAnimation {
                        duration: Tokens.durFast
                        easing.type: Tokens.easingType
                    }
                }

                // Onyx renders both labels and shows one. Same here: no reflow
                // when the state changes, so the knob is the only thing moving.
                Label {
                    anchors {
                        left: parent.left
                        leftMargin: Theme.paddingMedium
                        verticalCenter: parent.verticalCenter
                    }
                    visible: root.checked
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
                    visible: !root.checked
                    text: root.offText
                    font.pixelSize: Theme.fontSizeExtraSmall
                    font.bold: true
                    font.capitalization: Font.AllUppercase
                    color: "#FFFFFF"
                }

                Rectangle {
                    id: knob

                    y: Tokens.controlMargin
                    width: body.knobSize
                    height: body.knobSize
                    radius: Tokens.controlRadius
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

                // Past this the gesture is a swipe, and the release must not also
                // toggle or a deliberate drag would land back where it started.
                readonly property real threshold: body.knobSize * 0.3

                onPressed: {
                    pressX = mouseX
                    dragged = false
                }

                onPositionChanged: {
                    if (!pressed || Math.abs(mouseX - pressX) < threshold) {
                        return
                    }
                    dragged = true
                    // Dragging right means on, left means off - Mojo's "swiped in
                    // the direction opposite its current state" seen from the
                    // other side, and it also makes a drag that overshoots and
                    // comes back settle correctly.
                    root.checked = mouseX > pressX
                }

                onReleased: {
                    if (!dragged) {
                        root.checked = !root.checked
                    }
                }
            }
        }
    }
}
