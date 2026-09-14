import QtQuick 2.0
import QtMultimedia 5.0
import Sailfish.Silica 1.0
import Mochi 1.0

// Photographing a page, rather than picking one already taken.
//
// Deliberately not a general camera app: no zoom, no filters, no gallery strip.
// One button, and a frame guide to hold the phone square to the page - because
// the single biggest thing a user can do for recognition quality is to fill the
// frame and keep the paper parallel, and nothing in software recovers as much as
// that does.
Page {
    id: page

    // Emitted rather than pushed from here, so the page that opened the camera
    // decides what happens next. That keeps this file free of any knowledge of
    // the recognition flow.
    signal captured(string path)

    allowedOrientations: defaultAllowedOrientations

    // The viewfinder must not keep the sensor powered while the page is off the
    // stack; on a phone that is a visible battery cost and, on some devices, a
    // camera that stays locked for the next app that wants it.
    property bool active: status === PageStatus.Active

    // How far the frame about to be taken has to be turned clockwise to be the
    // right way up.
    //
    // The camera on this device writes an EXIF orientation of 1 on every photo it
    // takes - the five that were measured are all 8192x6144, the sensor's own
    // landscape frame, tagged as needing no rotation whichever way the phone was
    // held. So nothing downstream can know: ImagePrep::loadUpright applies the tag
    // faithfully and the tag says nothing, which is why a page photographed in
    // portrait arrived on its side.
    //
    // Portrait is anchored to a measurement: a magazine page shot in portrait read
    // correctly only after a quarter turn clockwise. The other three follow from
    // it by quarter turns, and are not measured.
    //
    // Nothing depends on this being right. The recogniser searches all four
    // angles and the result page turns the view to whichever one it read, so a tag
    // that comes out backwards costs a wrong thumbnail in the gallery and nothing
    // in this app. It is written because a photo that knows which way up it is, is
    // a better photo.
    function captureRotation() {
        switch (page.orientation) {
        case Orientation.Landscape:         return 0
        case Orientation.PortraitInverted:  return 270
        case Orientation.LandscapeInverted: return 180
        default:                            return 90
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    Camera {
        id: camera

        captureMode: Camera.CaptureStillImage
        cameraState: page.active ? Camera.ActiveState : Camera.UnloadedState

        // Continuous rather than a tap-to-focus ritual: text fills the frame, so
        // there is rarely a subject to disambiguate, and asking the user to focus
        // before every shot is one step too many for something they will do
        // twenty times in a row.
        focus {
            focusMode: Camera.FocusContinuous
            focusPointMode: Camera.FocusPointAuto
        }

        imageCapture {
            onImageSaved: page.captured(path)
            onCaptureFailed: {
                errorLabel.text = qsTr("The photo could not be taken.")
            }
        }

        // Written in JavaScript, at the moment of capture, rather than bound here.
        //
        // Bound, it would be a declarative reference to a property this device's
        // QtMultimedia may not have, and a QML file that names a property that is
        // not there does not lose the property - it fails to load, and the camera
        // page comes up black with the reason in the journal. Assigned from a
        // function, the worst case is one logged line and a photo tagged the way
        // it is tagged today. Nothing here can be tried on a device before a tag,
        // so the failure mode is chosen rather than discovered.
        function applyOrientation() {
            if (camera.metaData) {
                camera.metaData.orientation = page.captureRotation()
            }
        }
    }

    VideoOutput {
        anchors.fill: parent
        source: camera
        fillMode: VideoOutput.PreserveAspectFit
        // Portrait and Landscape are as they were, because they are the two that
        // are known to look right on a device. The inverted pair used to fall into
        // the Landscape case and be a half turn out; they are now their own.
        orientation: {
            switch (page.orientation) {
            case Orientation.Landscape:         return 90
            case Orientation.PortraitInverted:  return 180
            case Orientation.LandscapeInverted: return 270
            default:                            return 0
            }
        }
    }

    // A frame guide, not a crop: nothing is cut to it. It is there because a page
    // held inside it is a page held square, and that is worth more than any
    // preprocessing.
    Item {
        anchors {
            fill: parent
            margins: Theme.paddingLarge * 2
        }

        Repeater {
            model: 4

            delegate: Item {
                readonly property bool atRight: index === 1 || index === 3
                readonly property bool atBottom: index >= 2

                width: Theme.itemSizeSmall
                height: Theme.itemSizeSmall
                x: atRight ? parent.width - width : 0
                y: atBottom ? parent.height - height : 0

                Rectangle {
                    width: parent.width
                    height: Tokens.hairline * 2
                    color: Tokens.onColor
                    y: parent.atBottom ? parent.height - height : 0
                }

                Rectangle {
                    width: Tokens.hairline * 2
                    height: parent.height
                    color: Tokens.onColor
                    x: parent.atRight ? parent.width - width : 0
                }
            }
        }
    }

    Label {
        id: errorLabel

        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: shutter.top
            bottomMargin: Theme.paddingLarge
        }
        color: Theme.errorColor
        font.pixelSize: Theme.fontSizeSmall
    }

    Rectangle {
        id: shutter

        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: Theme.paddingLarge * 2
        }

        width: Theme.itemSizeLarge
        height: width
        radius: width / 2
        color: shutterArea.pressed ? Tokens.onColor : "#FFFFFF"
        opacity: camera.cameraState === Camera.ActiveState ? 1.0
                                                           : Tokens.disabledOpacity

        Behavior on color {
            ColorAnimation {
                duration: Tokens.durFast
                easing.type: Tokens.easingType
            }
        }

        MouseArea {
            id: shutterArea

            anchors.fill: parent
            enabled: camera.cameraState === Camera.ActiveState
            onClicked: {
                errorLabel.text = ""
                camera.applyOrientation()
                camera.imageCapture.capture()
            }
        }
    }

    Label {
        anchors {
            horizontalCenter: parent.horizontalCenter
            top: parent.top
            topMargin: Theme.paddingLarge * 2
        }
        width: parent.width - Theme.horizontalPageMargin * 2
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: "#FFFFFF"
        font.pixelSize: Theme.fontSizeExtraSmall
        text: qsTr("Fill the frame with the page, and hold the phone parallel to it.")
    }
}
