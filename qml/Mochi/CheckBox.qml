import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// A checkbox in the webOS idiom: a barely-rounded square that fills, with the
// tick scaling in rather than appearing.
//
// Onyx drew this with a two-state sprite (32x32, the checked frame stacked
// underneath), so there is no vector to copy - only the box metrics, which are
// the same 32px as the toggle track. The tick below is therefore an
// interpretation, kept to the same weight the sprite had.
//
// The tick is an L - a short vertical bar and a long horizontal one meeting at
// the bottom left - rotated -45 degrees. That is exactly a checkmark, and it
// avoids positioning two independently rotated strokes so their ends meet, which
// is where hand-built ticks usually go wrong.
//
// In ambience mode this renders as a Silica Switch, not as a checkbox. Sailfish
// has no standalone checkbox and does not want one: a boolean is a switch there,
// and the only place a tick appears is inside TextSwitch, which is a whole row.
// Drawing a checkbox that exists nowhere else on the phone would be more foreign
// than the switch that does.
Item {
    id: root

    property bool checked: false

    // Filled with the switch green so a form reads as one control family. Only
    // consulted by the Mochi themes; the native variant is Silica's own.
    property color boxColor: Tokens.onColor
    property color tickColor: "#FFFFFF"

    implicitWidth: variant.item ? variant.item.width : Tokens.controlHeight
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
            automaticCheck: false
            checked: root.checked
            enabled: root.enabled
            onClicked: root.checked = !root.checked
        }
    }

    Component {
        id: mochiVariant

        Rectangle {
            id: box

            width: Tokens.controlHeight
            height: Tokens.controlHeight
            radius: Tokens.controlRadius

            color: root.checked ? root.boxColor : "transparent"
            border.width: root.checked ? 0 : Tokens.hairline
            border.color: Tokens.separatorColor

            Behavior on color {
                ColorAnimation {
                    duration: Tokens.durFast
                    easing.type: Tokens.easingType
                }
            }

            Item {
                id: tick

                anchors.centerIn: parent
                width: Tokens.controlHeight * 0.52
                height: Tokens.controlHeight * 0.30
                rotation: -45

                readonly property real thickness:
                    Math.max(Tokens.hairline * 2, Tokens.controlHeight * 0.13)

                opacity: root.checked ? 1.0 : 0.0
                scale: root.checked ? 1.0 : 0.6

                Behavior on opacity {
                    NumberAnimation {
                        duration: Tokens.durFast
                        easing.type: Tokens.easingType
                    }
                }

                // No overshoot: webOS motion decelerates into place and stops.
                Behavior on scale {
                    NumberAnimation {
                        duration: Tokens.durFast
                        easing.type: Tokens.easingType
                    }
                }

                // The short arm, pointing down.
                Rectangle {
                    x: 0
                    y: 0
                    width: tick.thickness
                    height: parent.height
                    color: root.tickColor
                }

                // The long arm, pointing right, sharing the bottom-left corner.
                Rectangle {
                    x: 0
                    y: parent.height - tick.thickness
                    width: parent.width
                    height: tick.thickness
                    color: root.tickColor
                }
            }

            MouseArea {
                anchors.fill: parent
                enabled: root.enabled
                onClicked: root.checked = !root.checked
            }
        }
    }
}
