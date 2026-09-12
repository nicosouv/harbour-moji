import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// A rectangle rounded on the top corners, the bottom corners, both or neither.
//
// Qt Quick's Rectangle has one radius for all four corners, and Qt 5.6 has no
// Shapes module to draw a path instead. The standard answer is to round all four
// and then square off the end that should not be: a rounded rectangle plus a
// plain patch, radius tall, over whichever edge needs to stay flat.
//
// This exists because Onyx's Groupbox puts the rounding on the rows rather than
// on the container - :first-child gets the top corners, :last-child the bottom -
// which is what lets a grouped list have rounded ends without clipping, and
// therefore without the square-corner artifact clipping produces.
//
// Module-private: declared "internal" in qmldir, so it is available to Mochi's
// own components and invisible to anything importing the module.
Item {
    id: root

    property bool roundTop: false
    property bool roundBottom: false
    property real radius: Tokens.panelRadius
    property color color: "transparent"

    Rectangle {
        anchors.fill: parent
        color: root.color
        radius: (root.roundTop || root.roundBottom) ? root.radius : 0
    }

    // Squares off the top when only the bottom is meant to be round.
    Rectangle {
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        height: Math.min(root.radius, parent.height)
        color: root.color
        visible: root.roundBottom && !root.roundTop
    }

    Rectangle {
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }
        height: Math.min(root.radius, parent.height)
        color: root.color
        visible: root.roundTop && !root.roundBottom
    }
}
