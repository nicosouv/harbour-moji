import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// Mojo's RadioButton group: "each button as a labeled selection option in a
// horizontal array in which only one option can be selected at a time".
//
// The indicator slides between segments rather than blinking from one to the
// next. That is the single change that makes a segmented control feel like it
// belongs to this vocabulary: the movement tells you the two options are on one
// axis, which is the reason to use a segmented control instead of a list.
//
// Same barely-rounded corners and same green as ToggleSwitch, so a form built
// from Mochi controls reads as one family.
Item {
    id: root

    property var options: []
    property int currentIndex: 0

    readonly property real segmentWidth:
        (options && options.length > 0) ? width / options.length : width

    implicitHeight: Tokens.controlHeight
    implicitWidth: Tokens.controlHeight * 4

    opacity: enabled ? 1.0 : Tokens.disabledOpacity

    Rectangle {
        id: track

        anchors.fill: parent
        radius: Tokens.controlRadius
        color: Tokens.trackColor
    }

    Rectangle {
        id: indicator

        y: Tokens.controlMargin
        height: parent.height - 2 * Tokens.controlMargin
        width: root.segmentWidth - 2 * Tokens.controlMargin
        radius: Tokens.controlRadius
        color: Tokens.onColor

        x: Tokens.controlMargin + root.currentIndex * root.segmentWidth

        Behavior on x {
            NumberAnimation {
                duration: Tokens.durBase
                easing.type: Tokens.easingType
            }
        }
    }

    Row {
        anchors.fill: parent

        Repeater {
            model: root.options

            delegate: Item {
                id: segment

                width: root.segmentWidth
                height: root.height

                readonly property bool selected: index === root.currentIndex

                Label {
                    anchors.centerIn: parent
                    width: parent.width - Theme.paddingSmall * 2
                    horizontalAlignment: Text.AlignHCenter
                    truncationMode: TruncationMode.Fade

                    text: modelData
                    font.pixelSize: Theme.fontSizeExtraSmall
                    font.bold: segment.selected
                    font.capitalization: Font.AllUppercase

                    // White on the green indicator, and the ordinary secondary
                    // colour everywhere else.
                    color: segment.selected ? "#FFFFFF" : Tokens.secondaryColor

                    Behavior on color {
                        ColorAnimation {
                            duration: Tokens.durBase
                            easing.type: Tokens.easingType
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: root.enabled
                    onClicked: root.currentIndex = index
                }
            }
        }
    }
}
