import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// Which page of a PDF to read.
//
// Shown only when there is a choice. A one-page document is read without asking,
// because asking a question with one answer is not a choice, it is a step.
//
// Numbered from 1, the way the document numbers them, and not from 0 the way
// Poppler does - the translation happens once, in PdfRender.
Page {
    id: page

    property url documentUrl
    property int pageCount: 0

    allowedOrientations: defaultAllowedOrientations

    // Emitted rather than pushed, so this file knows nothing about what reading a
    // page involves - the same split CameraPage uses.
    signal chosen(int pageNumber)

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
                title: qsTr("Choose a page")
                description: qsTr("%1 pages").arg(page.pageCount)
            }

            GroupPanel {
                width: parent.width
                title: qsTr("Pages")

                Repeater {
                    model: page.pageCount

                    delegate: MouseArea {
                        width: parent.width
                        height: Theme.itemSizeLarge

                        onClicked: page.chosen(index + 1)

                        Rectangle {
                            anchors.fill: parent
                            color: parent.pressed ? Tokens.pressedColor : "transparent"
                        }

                        // Rendered small, and only when the row is about to be
                        // seen. A forty-page document would otherwise render forty
                        // pages before showing anything.
                        Image {
                            id: thumb

                            anchors {
                                left: parent.left
                                leftMargin: Theme.paddingLarge
                                verticalCenter: parent.verticalCenter
                            }
                            height: parent.height - Theme.paddingMedium * 2
                            width: height * 0.75
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            source: pdf.thumbnail(page.documentUrl, index + 1,
                                                  Theme.itemSizeLarge)

                            Rectangle {
                                anchors.fill: parent
                                color: "transparent"
                                border.width: Tokens.hairline
                                border.color: Tokens.separatorColor
                            }
                        }

                        Label {
                            anchors {
                                left: thumb.right
                                leftMargin: Theme.paddingLarge
                                verticalCenter: parent.verticalCenter
                            }
                            // index is zero-based and a reader is not.
                            text: qsTr("Page %1").arg(index + 1)
                            font.pixelSize: Theme.fontSizeSmall
                            color: Tokens.primaryColor
                        }
                    }
                }
            }
        }
    }
}
