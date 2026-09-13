import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// A reading whose photo has since been deleted.
//
// The text was kept, so it is still worth showing - but nothing here can offer to
// read it again, and pretending otherwise would be the app promising something it
// cannot do.
Page {
    id: page

    property string title
    property string body

    allowedOrientations: defaultAllowedOrientations

    Rectangle {
        anchors.fill: parent
        color: Tokens.pageColor
        visible: !Tokens.ambient
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge * 2

        VerticalScrollDecorator { }

        PullDownMenu {
            MenuItem {
                text: qsTr("Copy all text")
                onClicked: {
                    Clipboard.text = page.body
                    banner.show(qsTr("All text copied"))
                }
            }
        }

        Column {
            id: column

            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: page.title
                description: qsTr("Photo no longer on the device")
            }

            GroupPanel {
                width: parent.width
                title: qsTr("All text")

                Item {
                    width: parent.width
                    height: stored.height + Theme.paddingLarge * 2

                    TextEdit {
                        id: stored

                        anchors {
                            left: parent.left
                            right: parent.right
                            leftMargin: Theme.paddingLarge
                            rightMargin: Theme.paddingLarge
                            verticalCenter: parent.verticalCenter
                        }

                        readOnly: true
                        selectByMouse: true
                        persistentSelection: true
                        textFormat: TextEdit.PlainText

                        text: page.body
                        wrapMode: TextEdit.Wrap
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Tokens.primaryColor
                        selectionColor: Tokens.selectionColor
                        selectedTextColor: Tokens.primaryColor
                    }
                }
            }
        }
    }

    Banner {
        id: banner
    }
}
