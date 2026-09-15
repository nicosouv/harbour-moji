import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

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
                title: qsTr("About Moji OCR")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Tokens.primaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("Reads text from photos, entirely on the device.")
            }

            GroupPanel {
                width: parent.width
                title: qsTr("Version")

                PanelRow {
                    width: parent.width
                    title: appVersion
                    glyph: "M"
                }
            }

            GroupPanel {
                width: parent.width
                title: qsTr("Privacy")

                PanelRow {
                    width: parent.width
                    title: qsTr("No network access")
                    detail: qsTr("Enforced by the sandbox, not just promised")
                    glyph: "!"
                }

                PanelRow {
                    width: parent.width
                    title: qsTr("Nothing leaves the device")
                    detail: qsTr("No account, no telemetry, no uploads")
                    glyph: "!"
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Tokens.secondaryColor
                text: qsTr("Moji (文字) is Japanese for a written character.")
            }

            // Where the look comes from. Kept to one sentence on purpose: the
            // full account is in qml/Mochi/README.md, and an About page is not
            // where a design language gets explained - it is where it gets
            // credited.
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Tokens.secondaryColor
                text: qsTr("The interface is Mochi, a small module built for this app: grouped panels, quiet hairlines and short decelerating motion, borrowed from webOS — Mojo's lists, Onyx's controls, Enyo's timing.")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Tokens.secondaryColor
                text: "github.com/nicosouv/harbour-moji"
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Tokens.secondaryColor
                text: qsTr("MIT licensed. Text recognition by Tesseract, Apache-2.0. PDF rendering by Poppler, GPL-2.0-or-later, which makes this binary GPL.")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Tokens.accentColor
                text: qsTr("Developed with ❤️ for Sailfish OS")
            }
        }
    }
}
