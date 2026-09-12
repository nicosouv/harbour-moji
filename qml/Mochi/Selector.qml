import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// Mojo's ListSelector: a row showing a label and its current value, which opens
// a list of the alternatives when tapped, with the current one marked.
//
// Onyx put that list in a popup positioned *over* the trigger, so the selected
// row landed on top of the button it came from. That is the more authentic
// gesture, and it is also the one that breaks inside a Flickable, where an
// overlay is clipped by the viewport unless it is reparented to the page. This
// reveals the list in a Drawer instead: the same information, the same short
// decelerating motion, and it survives being put inside a GroupPanel, which is
// where a selector actually goes.
//
// options is a plain array of strings, deliberately not a ListModel: reading a
// model in a binding means get(), and a get() inside a binding is the crash
// scripts/check_qml.py exists to prevent.
//
//   Selector {
//       width: parent.width
//       label: "Sync interval"
//       options: ["30 seconds", "2 minutes", "Manual"]
//       currentIndex: 1
//   }
Column {
    id: root

    property string label
    property var options: []
    property int currentIndex: 0

    readonly property string currentText:
        (options && options.length > currentIndex && currentIndex >= 0)
            ? options[currentIndex] : ""

    property bool expanded: false

    // Takes part in GroupPanel's :first-child/:last-child rounding. The bottom
    // corner moves: it belongs to the header while collapsed and to the last
    // option once the drawer is open.
    readonly property bool mochiRow: true
    property bool roundTop: false
    property bool roundBottom: false

    MouseArea {
        id: header

        width: root.width
        height: Theme.itemSizeSmall

        onClicked: root.expanded = !root.expanded

        PanelBox {
            anchors.fill: parent
            roundTop: root.roundTop
            roundBottom: root.roundBottom && !root.expanded
            radius: Tokens.panelRadius
            color: Tokens.pressedColor
            visible: header.pressed
        }

        Label {
            anchors {
                left: parent.left
                leftMargin: Theme.paddingLarge
                verticalCenter: parent.verticalCenter
            }
            text: root.label
            font.pixelSize: Theme.fontSizeSmall
            color: header.pressed ? Tokens.accentColor : Tokens.primaryColor
        }

        Row {
            anchors {
                right: parent.right
                rightMargin: Theme.paddingLarge
                verticalCenter: parent.verticalCenter
            }
            spacing: Theme.paddingMedium

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: root.currentText
                font.pixelSize: Theme.fontSizeSmall
                color: Tokens.accentColor
            }

            // A chevron built as an equal-armed L rotated 45 degrees, which
            // points down; rotating it to 135 points it up. Animating the
            // rotation rather than swapping two glyphs is what makes the
            // control feel like it opened rather than changed.
            Item {
                id: chevron

                anchors.verticalCenter: parent.verticalCenter
                width: Theme.fontSizeSmall * 0.5
                height: width

                readonly property real thickness: Math.max(Tokens.hairline,
                                                           width * 0.18)

                rotation: root.expanded ? 135 : 45

                Behavior on rotation {
                    NumberAnimation {
                        duration: Tokens.durBase
                        easing.type: Tokens.easingType
                    }
                }

                Rectangle {
                    x: 0
                    y: 0
                    width: chevron.thickness
                    height: parent.height
                    color: Tokens.accentColor
                }

                Rectangle {
                    x: 0
                    y: parent.height - chevron.thickness
                    width: parent.width
                    height: chevron.thickness
                    color: Tokens.accentColor
                }
            }
        }
    }

    Drawer {
        width: root.width
        open: root.expanded

        Repeater {
            model: root.options

            delegate: MouseArea {
                id: option

                width: root.width
                height: Theme.itemSizeSmall

                // modelData, not model: the model is an array of strings, so
                // each delegate gets the string itself and there are no roles.
                readonly property string optionText: modelData
                readonly property bool selected: index === root.currentIndex
                readonly property bool lastOption: index === root.options.length - 1

                onClicked: {
                    root.currentIndex = index
                    root.expanded = false
                }

                PanelBox {
                    anchors.fill: parent
                    roundBottom: root.roundBottom && option.lastOption
                    radius: Tokens.panelRadius
                    color: Tokens.selectionColor
                    visible: option.selected
                }

                PanelBox {
                    anchors.fill: parent
                    roundBottom: root.roundBottom && option.lastOption
                    radius: Tokens.panelRadius
                    color: Tokens.pressedColor
                    visible: option.pressed
                }

                Label {
                    anchors {
                        left: parent.left
                        leftMargin: Theme.paddingLarge * 2
                        verticalCenter: parent.verticalCenter
                    }
                    text: option.optionText
                    font.pixelSize: Theme.fontSizeSmall
                    color: option.selected ? Tokens.accentColor
                                           : Tokens.primaryColor
                }
            }
        }
    }
}
