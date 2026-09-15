import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0
import "pages"
import "cover"
import "components"

ApplicationWindow {
    id: root

    initialPage: Component { MainPage { } }
    allowedOrientations: defaultAllowedOrientations

    // Without this the app shows Sailfish's default cover - the icon and the
    // name - which on this platform reads as unfinished. The cover has three
    // states because the app does, and the one that earns it is "reading":
    // recognition takes a second or three on a photograph and longer on a
    // document, so somebody who minimised mid-read can see whether it is done.
    cover: Component {
        CoverPage {
            onCameraRequested: {
                root.activate()
                root.openCamera()
            }
            onShareRequested: {
                root.activate()
                sharer.share(ocr.editedText, qsTr("Recognised text"))
            }
        }
    }

    ShareHelper {
        id: sharer
    }

    // Opens the viewfinder over whatever is on the stack, and puts the result
    // where the main page would have put it.
    //
    // Pushed rather than replacing the stack: someone who taps the cover's camera
    // while a result is open has not asked to lose it.
    function openCamera() {
        var camera = pageStack.push(Qt.resolvedUrl("pages/CameraPage.qml"))
        camera.captured.connect(function (path) {
            pageStack.completeAnimation()
            pageStack.replace(Qt.resolvedUrl("pages/ResultPage.qml"),
                              { imageUrl: "file://" + path })
        })
    }

    // The stored theme drives Mochi's singleton. Done with an explicit handler
    // rather than a Binding on the singleton: the value has to be pushed once at
    // startup too, and one mechanism doing both is easier to be sure of than a
    // declarative binding plus a special case for the first frame.
    Component.onCompleted: Tokens.mode = settings.theme

    Connections {
        target: settings
        onThemeChanged: Tokens.mode = settings.theme
    }
}
