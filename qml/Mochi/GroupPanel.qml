import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// A grouped list container, the structuring gesture of webOS Mojo: rows sit in
// a rounded panel on a slightly different surface, with a quiet label above.
// Hierarchy comes from the grouping, not from rules or bold headings.
//
// Mojo's own stylesheets built this in three levels - .palm-group framed it,
// .palm-list drove the spacing and separators, .palm-row wrapped each entry -
// and Onyx's Groupbox put the rounding on the rows themselves: a full border on
// every child, top corners on :first-child, bottom corners on :last-child, all
// four when a group holds a single row.
//
// That last part is the reason updateRows() exists. Rounding the container
// and clipping to it would be the obvious QML translation, but Qt Quick clips to
// the bounding box and never to the radius, so a pressed row at either end
// paints its highlight square into the corner. Rounding the rows instead removes
// the artifact rather than hiding it, and needs no mask and no extra texture.
//
// Children are laid out in a Column inside the panel:
//   GroupPanel {
//       width: parent.width
//       title: "Unread"
//       Repeater { model: unreadRooms; delegate: PanelRow {} }
//   }
Item {
    id: root

    property string title
    property real radius: Tokens.panelRadius
    property color panelColor: Tokens.panelColor

    // Rows go straight into the panel's own column.
    default property alias content: panelColumn.data

    // The panel is inset from the page edge; rows are not inset again.
    property real margin: Theme.horizontalPageMargin

    implicitHeight: layout.height

    // The QML equivalent of :first-child and :last-child, plus each row's
    // separator phase.
    //
    // Position is measured over every child that occupies space, not only over
    // the rows: a group whose last child is a Selector must round that, and a
    // group whose first child is a plain Item must round nothing. The zero-sized
    // ones are skipped because a Repeater is itself a child of the Column it
    // fills - counted, it would take first place and no row would ever round.
    //
    // Rows appear one at a time as a Repeater builds them, so this runs on every
    // change and converges rather than assuming one pass is enough.
    function updateRows() {
        var visible = []
        for (var i = 0; i < panelColumn.children.length; ++i) {
            var child = panelColumn.children[i]
            if (child.visible && (child.width > 0 || child.height > 0)) {
                visible.push(child)
            }
        }

        var seen = 0
        for (var j = 0; j < visible.length; ++j) {
            var item = visible[j]
            if (item.mochiRow !== true) {
                continue
            }
            item.roundTop = (j === 0)
            item.roundBottom = (j === visible.length - 1)
            // Guarded: Selector takes part in the rounding but has no separator
            // of its own, so it has no rowIndex to assign.
            if (item.rowIndex !== undefined) {
                item.rowIndex = seen
            }
            seen += 1
        }
    }

    Column {
        id: layout

        width: root.width
        spacing: Theme.paddingSmall

        Label {
            x: root.margin + Theme.paddingSmall
            width: root.width - 2 * root.margin - Theme.paddingSmall
            text: root.title
            visible: root.title.length > 0
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Tokens.secondaryColor
            truncationMode: TruncationMode.Fade
        }

        Rectangle {
            x: root.margin
            width: root.width - 2 * root.margin
            height: panelColumn.height
            radius: root.radius
            color: root.panelColor

            // No clip: the rows round their own ends, so nothing needs masking,
            // and clipping is what caused the square corner in the first place.

            Column {
                id: panelColumn

                width: parent.width

                // Repeaters add their items after this component is complete, so
                // recompute whenever the set of children changes rather than
                // only once at startup.
                onChildrenChanged: root.updateRows()
            }
        }
    }

    Component.onCompleted: updateRows()
}
