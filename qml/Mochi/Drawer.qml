import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// The webOS drawer: content that is revealed by growing the container rather
// than by moving the content. Onyx had this as a first-class control, and it is
// how webOS expanded a group, a submenu or a selector without ever covering
// what the user was looking at.
//
// Height is animated, not scale or opacity: everything below it is pushed down,
// which is the whole point - the reveal is part of the page's layout, so nothing
// floats over anything and no overlay plumbing is needed. That also makes it
// safe inside a Flickable, where a popup would be clipped.
//
// Children go straight into an internal Column:
//   Drawer {
//       width: parent.width
//       open: somethingTapped
//       Label { text: "revealed" }
//   }
Item {
    id: root

    property bool open: false

    default property alias content: column.data

    // Nothing must escape the collapsed box while it is animating shut.
    clip: true

    implicitHeight: open ? column.height : 0

    Behavior on implicitHeight {
        NumberAnimation {
            duration: Tokens.durBase
            easing.type: Tokens.easingType
        }
    }

    Column {
        id: column

        width: root.width

        // Bottom-anchored so the content slides up out of view as the drawer
        // closes, instead of being cut off from the bottom while standing still.
        y: root.height - height
    }
}
