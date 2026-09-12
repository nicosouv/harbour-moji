import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// A row for the inside of a GroupPanel: a glyph or avatar in the left column,
// a title, an optional second line, an optional count on the right.
//
// Not one of the three files the design drop delivered - GroupPanel has no use
// without a row, and every consumer writing its own would be how a design
// language stops being one.
//
// A MouseArea, not a Silica BackgroundItem. Onyx's Groupbox rounds :first-child's
// top corners and :last-child's bottom ones, so a row at either end of a group
// has to round its press highlight to match; Silica's highlight is not
// addressable, and clipping the panel instead squares the corner off, which is
// the artifact this replaces. Drawing it here also drops two guesses about
// Silica's API - whether BackgroundItem takes height or contentHeight, and
// whether it exposes highlighted.
//
// roundTop and roundBottom are set by the enclosing GroupPanel, which walks its
// children the way the CSS selectors did. Setting them by hand is only needed
// for a row that is not a direct child of one.
//
// The padding is the row's own, not the panel's: GroupPanel insets its panel
// from the page edge and deliberately does not inset its rows again, so a row
// that anchored to Theme.horizontalPageMargin would be indented twice.
//
// Set accent for an action row - Mojo's "add an item" line, which lived as the
// last row of the group rather than behind a floating button.
MouseArea {
    id: row

    property string title
    property string detail
    property int badge: 0

    // Defaults to the title's initial, the way a room with no avatar is drawn.
    property string glyph: title.length > 0 ? title.charAt(0).toUpperCase() : "?"

    property bool accent: false

    // An avatar to draw instead of the glyph. Left empty, the circle shows the
    // glyph, which is also the fallback while the image is still loading.
    property url avatar

    // How GroupPanel recognises a row it should round. Cheaper and looser than a
    // type check: anything that opts in by declaring it takes part.
    readonly property bool mochiRow: true

    property bool roundTop: false
    property bool roundBottom: false

    // Also assigned by GroupPanel, from the row's position in the group. It only
    // feeds the separator's phase, so consecutive rows bow differently - and
    // because it comes from the position rather than from a model index, it does
    // not have to be threaded through every delegate by hand.
    property int rowIndex: 0

    height: detail.length > 0 ? Theme.itemSizeMedium : Theme.itemSizeSmall

    PanelBox {
        anchors.fill: parent
        roundTop: row.roundTop
        roundBottom: row.roundBottom
        radius: Tokens.panelRadius
        color: Tokens.pressedColor
        visible: row.pressed
    }

    // Onyx gave every row a bottom border and the last row none, so the divider
    // belongs to the row rather than being interleaved between rows by the
    // caller. That is also what lets a Repeater's delegate be a PanelRow and
    // nothing else.
    CurvedSeparator {
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }
        index: row.rowIndex
        visible: !row.roundBottom
    }

    Row {
        anchors {
            left: parent.left
            right: parent.right
            leftMargin: Theme.paddingLarge
            rightMargin: Theme.paddingLarge
            verticalCenter: parent.verticalCenter
        }
        spacing: Theme.paddingMedium

        Item {
            width: Theme.iconSizeMedium
            height: Theme.iconSizeMedium
            anchors.verticalCenter: parent.verticalCenter

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                visible: avatarImage.status !== Image.Ready
                color: row.accent ? "transparent"
                                  : Theme.rgba(Tokens.accentColor, 0.15)

                // An action row reads as an outline, not as a filled avatar:
                // it is not standing in for a person.
                border.width: row.accent ? Tokens.hairline : 0
                border.color: Theme.rgba(Tokens.accentColor, 0.4)

                Label {
                    anchors.centerIn: parent
                    text: row.glyph
                    font.pixelSize: row.accent ? Theme.fontSizeLarge
                                               : Theme.fontSizeSmall
                    color: Tokens.accentColor
                }
            }

            Image {
                id: avatarImage

                anchors.fill: parent
                source: row.avatar
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: status === Image.Ready
                clip: true
            }
        }

        Column {
            width: parent.width - Theme.iconSizeMedium - countColumn.width
                   - 2 * Theme.paddingMedium
            anchors.verticalCenter: parent.verticalCenter

            Label {
                width: parent.width
                text: row.title
                font.pixelSize: Theme.fontSizeSmall
                color: (row.accent || row.pressed) ? Tokens.accentColor
                                                   : Tokens.primaryColor
                truncationMode: TruncationMode.Fade
            }

            Label {
                width: parent.width
                visible: row.detail.length > 0
                text: row.detail
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Tokens.secondaryColor
                truncationMode: TruncationMode.Fade
            }
        }

        Item {
            id: countColumn

            width: row.badge > 0 ? counter.width + Theme.paddingMedium : 0
            height: Theme.iconSizeMedium
            anchors.verticalCenter: parent.verticalCenter

            Rectangle {
                id: counter

                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(Theme.iconSizeExtraSmall,
                                counterLabel.width + Theme.paddingMedium)
                height: Theme.iconSizeExtraSmall
                radius: height / 2
                visible: row.badge > 0
                color: Tokens.accentColor

                Label {
                    id: counterLabel

                    anchors.centerIn: parent
                    text: row.badge > 99 ? "99+" : row.badge
                    font.pixelSize: Theme.fontSizeTiny
                    color: Tokens.ambient ? Theme.primaryColor : "#FFFFFF"
                }
            }
        }
    }
}
