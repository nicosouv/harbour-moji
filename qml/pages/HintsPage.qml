import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// How to take a photograph this app can read.
//
// Worth a page of its own because it is the largest lever there is, and it is the
// user's rather than the developer's. Measured on real photographs, framing and
// light moved the result far more than any setting in here: a four-times-larger
// recognition model won one case out of five, and sharpening the image lost four
// out of five. Filling the frame with a well-lit page wins every time.
//
// Every line below is something that was actually observed, not general advice.
Page {
    id: page

    allowedOrientations: defaultAllowedOrientations

    Rectangle {
        anchors.fill: parent
        color: Tokens.pageColor
        visible: !Tokens.ambient
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge * 2

        VerticalScrollDecorator { }

        Column {
            id: content

            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Taking a readable photo")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Tokens.primaryColor
                text: qsTr("How the photo is taken matters more than any setting on the previous page. These are the things that were measured to make the biggest difference.")
            }

            GroupPanel {
                width: parent.width
                title: qsTr("Do")

                PanelRow {
                    width: parent.width
                    title: qsTr("Fill the frame with the page")
                    detail: qsTr("Text too small in the frame is the commonest reason a page reads badly")
                    glyph: "⬚"
                }

                PanelRow {
                    width: parent.width
                    title: qsTr("Hold the phone parallel to the page")
                    detail: qsTr("A page at an angle converges, and no rotation fixes that — use “Straighten the page” if you cannot")
                    glyph: "◱"
                }

                PanelRow {
                    width: parent.width
                    title: qsTr("Give it light")
                    detail: qsTr("Every photo this app read badly was taken at night. The torch in the camera is there for that")
                    glyph: "☀"
                }

                PanelRow {
                    width: parent.width
                    title: qsTr("Set the right language first")
                    detail: qsTr("Reading French as English costs every accented word — it is on the result page as well as in Settings")
                    glyph: "A"
                }
            }

            GroupPanel {
                width: parent.width
                title: qsTr("Avoid")

                PanelRow {
                    width: parent.width
                    title: qsTr("Your own shadow")
                    detail: qsTr("Standing over a page puts a gradient across it; step aside or raise the light")
                    glyph: "◑"
                }

                PanelRow {
                    width: parent.width
                    title: qsTr("Reflections on glossy paper")
                    detail: qsTr("A highlight erases the words under it, and nothing can recover them")
                    glyph: "✦"
                }

                PanelRow {
                    width: parent.width
                    title: qsTr("Moving while the shutter is open")
                    detail: qsTr("Indoors the exposure is long. Blur cannot be sharpened back — that was measured too")
                    glyph: "〜"
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Tokens.secondaryColor
                text: qsTr("Printed pages read well. Street signs, handwriting and text over photographs read poorly, and that is a limit of recognition that runs on a phone rather than something to adjust.")
            }
        }
    }
}
