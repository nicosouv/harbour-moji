import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// Retyping a word the recogniser got wrong.
//
// A Silica Dialog rather than anything of Mochi's own: this is a keyboard
// interaction, and the accept/cancel gesture at the top of a Sailfish dialog is
// muscle memory. Inventing a different one here would be the place where being
// different stops being charming.
Dialog {
    id: dialog

    // Set by the caller; read back after accepted.
    property int wordIndex: -1
    property string wordText

    canAccept: field.text.length > 0

    onAccepted: wordText = field.text

    Column {
        width: parent.width
        spacing: Theme.paddingLarge

        DialogHeader {
            acceptText: qsTr("Correct")
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Tokens.secondaryColor
            text: qsTr("This word was hard to read. What does it say?")
        }

        TextField {
            id: field

            width: parent.width
            text: dialog.wordText
            label: qsTr("Word")
            inputMethodHints: Qt.ImhNoPredictiveText

            // The recogniser's guess is usually close, so the whole of it is
            // selected: one keystroke replaces it, and the arrow keys still allow
            // fixing a single character.
            Component.onCompleted: {
                forceActiveFocus()
                selectAll()
            }

            EnterKey.iconSource: "image://theme/icon-m-enter-accept"
            EnterKey.onClicked: dialog.accept()
        }
    }
}
