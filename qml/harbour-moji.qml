import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0
import "pages"

ApplicationWindow {
    id: root

    initialPage: Component { MainPage { } }
    allowedOrientations: defaultAllowedOrientations

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
