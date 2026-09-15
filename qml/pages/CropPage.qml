// 2.5 for Image.autoTransform, the same reason ResultPage says so.
import QtQuick 2.5
import Sailfish.Silica 1.0
import Mochi 1.0

// Marking the four corners of a page, so it can be pulled flat.
//
// The distortion this exists for is the one nothing else here can touch. The
// recogniser's own deskew straightens text that is rotated in the plane of the
// photograph; a page held at an angle to the camera converges instead - the far
// edge shorter than the near one - and no rotation fixes a trapezoid. Tesseract
// is trained on flatbed scans, where it never happens.
//
// A Dialog, so accepting and cancelling are the platform's own gesture and this
// file does not invent them.
Dialog {
    id: dialog

    property url imageUrl

    // The photo's own pixel size, needed to turn where a finger is into where a
    // pixel is. Taken from the recogniser, which has already loaded it upright.
    property size imageSize

    // Filled on accept, in the photo's coordinates, clockwise from the top left.
    property var corners: []

    allowedOrientations: defaultAllowedOrientations

    // Nothing to straighten if the marks have been dragged into a line or on top
    // of one another; the same judgement the C++ makes, so the button agrees with
    // what will happen.
    canAccept: canvas.ratio > 0

    onAccepted: {
        dialog.corners = [
            canvas.toImage(handles.itemAt(0)),
            canvas.toImage(handles.itemAt(1)),
            canvas.toImage(handles.itemAt(2)),
            canvas.toImage(handles.itemAt(3))
        ]
    }

    Rectangle {
        anchors.fill: parent
        color: Tokens.pageColor
        visible: !Tokens.ambient
    }

    Column {
        id: content

        width: parent.width

        DialogHeader {
            acceptText: qsTr("Straighten")
            cancelText: qsTr("Cancel")
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Tokens.secondaryColor
            text: qsTr("Drag the four marks onto the corners of the page.")
        }
    }

    Item {
        id: frame

        anchors {
            left: parent.left
            right: parent.right
            top: content.bottom
            bottom: parent.bottom
            margins: Theme.horizontalPageMargin
        }

        Item {
            id: canvas

            anchors.centerIn: parent

            // Proportions from the loaded image, never from what the Image
            // painted: sizing an item from its child's paintedHeight while the
            // child fills the item is the binding loop this project has met
            // twice. scripts/check_qml.py fails the build on the other spelling.
            readonly property real aspect: photo.implicitWidth > 0
                    ? photo.implicitHeight / photo.implicitWidth : 1

            width: Math.min(frame.width, frame.height / Math.max(aspect, 0.0001))
            height: width * aspect

            // Screen pixels per image pixel, which is the whole conversion.
            readonly property real ratio: dialog.imageSize.width > 0
                    ? width / dialog.imageSize.width : 0

            function toImage(handle) {
                return Qt.point(Math.round(handle.centreX / ratio),
                                Math.round(handle.centreY / ratio))
            }

            Image {
                id: photo

                anchors.fill: parent
                source: dialog.imageUrl
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                autoTransform: true
                sourceSize.width: dialog.width
            }

            // The quadrilateral as it stands, drawn as four edges so what will be
            // kept is visible rather than inferred from four dots.
            Canvas {
                id: outline

                anchors.fill: parent
                // Drawn once per change, not per frame; a Canvas repainting
                // continuously on a phone is a warm battery.
                renderTarget: Canvas.Image

                property int revision: 0

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    if (handles.count < 4) {
                        return
                    }
                    ctx.strokeStyle = Tokens.accentColor
                    ctx.lineWidth = Math.max(1, Tokens.hairline * 2)
                    ctx.beginPath()
                    for (var i = 0; i < 4; ++i) {
                        var h = handles.itemAt(i)
                        if (i === 0) {
                            ctx.moveTo(h.centreX, h.centreY)
                        } else {
                            ctx.lineTo(h.centreX, h.centreY)
                        }
                    }
                    ctx.closePath()
                    ctx.stroke()
                }

                onRevisionChanged: requestPaint()
            }

            Repeater {
                id: handles

                // Started at the corners of the photo, which is the answer for a
                // page that fills the frame and one drag away from the answer for
                // one that does not.
                model: [ { hx: 0, hy: 0 }, { hx: 1, hy: 0 },
                         { hx: 1, hy: 1 }, { hx: 0, hy: 1 } ]

                delegate: MouseArea {
                    id: handle

                    // The touch target is larger than the mark, because a
                    // fingertip is larger than a corner.
                    width: Theme.itemSizeSmall
                    height: width

                    readonly property real centreX: x + width / 2
                    readonly property real centreY: y + height / 2

                    x: modelData.hx * canvas.width - width / 2
                    y: modelData.hy * canvas.height - height / 2

                    drag.target: handle
                    drag.minimumX: -width / 2
                    drag.maximumX: canvas.width - width / 2
                    drag.minimumY: -height / 2
                    drag.maximumY: canvas.height - height / 2

                    // preventStealing, or the page under this takes the drag and
                    // the mark stays where it was while the view scrolls.
                    preventStealing: true

                    onXChanged: outline.revision++
                    onYChanged: outline.revision++

                    // A ring, not a disc: the point being aimed at has to stay
                    // visible while the finger is on it.
                    Rectangle {
                        anchors.centerIn: parent
                        width: Theme.itemSizeExtraSmall
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.width: Math.max(2, Tokens.hairline * 2)
                        border.color: handle.pressed ? Tokens.onColor
                                                     : Tokens.accentColor

                        Rectangle {
                            anchors.centerIn: parent
                            width: Math.max(2, Tokens.hairline * 2)
                            height: parent.width * 0.5
                            color: parent.border.color
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width * 0.5
                            height: Math.max(2, Tokens.hairline * 2)
                            color: parent.border.color
                        }
                    }
                }
            }
        }
    }
}
