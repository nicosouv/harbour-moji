import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import Mochi 1.0

// Two ways in: photograph a page, or pick one already on the device.
//
// Mojo put the action in the list as a row rather than behind a floating button,
// and the pulley carries what is global - settings, about. That split is the rule
// this app follows: rows do the thing the group is about, the pulley does the rest.
Page {
    id: page

    allowedOrientations: defaultAllowedOrientations

    function read(path) {
        pageStack.push(Qt.resolvedUrl("ResultPage.qml"),
                       { imageUrl: "file://" + path })
    }

    // A picked document, which may be a PDF and may be an image someone keeps in
    // Documents rather than in the gallery.
    //
    // A PDF has no pixels, so one page of it is rendered to an image first and
    // everything downstream carries on unchanged - the result page, the
    // searchable PDF export, the redaction and the history all work on an image
    // file and none of them has to learn what a PDF is.
    function openDocument(path) {
        var url = "file://" + path

        if (!pdf.isPdf(url)) {
            // Not a PDF: the picker lists images kept in Documents too, and
            // reading one needs nothing special.
            pageStack.completeAnimation()
            pageStack.replace(Qt.resolvedUrl("ResultPage.qml"), { imageUrl: url })
            return
        }

        var pages = pdf.pageCount(url)
        if (pages < 1) {
            pageStack.completeAnimation()
            pageStack.pop()
            banner.show(pdf.lastError())
            return
        }

        if (pages === 1) {
            page.readPdfPage(url, 1, true)
            return
        }

        // More than one page, so ask. Replace rather than push: Back from the
        // chooser should return here, not to the picker.
        pageStack.completeAnimation()
        var chooser = pageStack.replace(Qt.resolvedUrl("PdfPagePage.qml"),
                                        { documentUrl: url, pageCount: pages })
        chooser.chosen.connect(function (number) {
            page.readPdfPage(url, number, true)
        })
    }

    function readPdfPage(url, number, replace) {
        var rendered = pdf.renderPage(url, number)
        if (rendered == "") {
            banner.show(pdf.lastError())
            return
        }

        // A PDF that was exported rather than scanned carries its text exactly,
        // and recognising a picture of that text can only be worse. Say so; do
        // not decide - a page can carry a text layer over half of what is on it,
        // and the reading is still the thing that was asked for.
        var existing = pdf.embeddedText(url, number)
        if (existing !== "") {
            banner.show(qsTr("This page already carries text. Reading it as a picture anyway."))
        }

        pageStack.completeAnimation()
        if (replace) {
            pageStack.replace(Qt.resolvedUrl("ResultPage.qml"), { imageUrl: rendered })
        } else {
            pageStack.push(Qt.resolvedUrl("ResultPage.qml"), { imageUrl: rendered })
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Tokens.pageColor
        visible: !Tokens.ambient
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge * 2

        VerticalScrollDecorator { }

        PullDownMenu {
            MenuItem {
                text: qsTr("About")
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
        }

        Column {
            id: content

            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Moji OCR")
            }

            GroupPanel {
                width: parent.width
                title: qsTr("Read")

                PanelRow {
                    width: parent.width
                    title: qsTr("Take a photo")
                    detail: qsTr("Point the camera at a page")
                    accent: true
                    glyph: "●"
                    onClicked: {
                        var camera = pageStack.push(Qt.resolvedUrl("CameraPage.qml"))
                        camera.captured.connect(function (path) {
                            // Replace rather than push: coming back from the
                            // result should return here, not to a viewfinder that
                            // would restart the camera behind the page.
                            //
                            // completeAnimation first, or the replace lands while
                            // the push that opened the camera is still animating
                            // and Silica refuses it with "cannot pop while
                            // transition is in progress".
                            pageStack.completeAnimation()
                            pageStack.replace(Qt.resolvedUrl("ResultPage.qml"),
                                              { imageUrl: "file://" + path })
                        })
                    }
                }

                PanelRow {
                    width: parent.width
                    title: qsTr("Choose a photo")
                    detail: qsTr("From the gallery")
                    accent: true
                    glyph: "+"
                    onClicked: pageStack.push(imagePicker)
                }

                PanelRow {
                    width: parent.width
                    title: qsTr("Choose a document")
                    detail: qsTr("A PDF, or an image kept in Documents")
                    accent: true
                    glyph: "▤"
                    onClicked: pageStack.push(documentPicker)
                }
            }

            GroupPanel {
                width: parent.width
                title: qsTr("Recent")
                visible: history.count > 0

                Repeater {
                    model: history.entries

                    delegate: PanelRow {
                        width: parent.width

                        // What the page says, not what the camera called the
                        // file: a document is recognised by its first line far
                        // faster than by IMG_0042.
                        title: modelData.summary
                        detail: modelData.imageExists
                                ? qsTr("%1 words").arg(modelData.wordCount)
                                : qsTr("%1 words — photo no longer on the device")
                                  .arg(modelData.wordCount)
                        glyph: modelData.imageExists ? "\u25a4" : "\u2717"

                        onClicked: {
                            if (modelData.imageExists) {
                                // With the text that was kept: reading the photo
                                // again takes seconds and can come out differently,
                                // and nothing about tapping a past reading asks for
                                // that. "Read again" is on the page for when it is
                                // wanted.
                                pageStack.push(Qt.resolvedUrl("ResultPage.qml"),
                                               { imageUrl: "file://" + modelData.imagePath,
                                                 storedText: history.textOf(modelData.id) })
                            } else {
                                // The photo is gone, but the text was kept. Show
                                // that rather than offering a reading that cannot
                                // happen.
                                pageStack.push(Qt.resolvedUrl("StoredTextPage.qml"),
                                               { title: modelData.summary,
                                                 body: history.textOf(modelData.id) })
                            }
                        }
                    }
                }

                // The group's own action, as its last row. Mojo put "add an item"
                // there rather than behind a floating button; the same reasoning
                // puts "clear these" there rather than in a menu.
                PanelRow {
                    width: parent.width
                    title: qsTr("Clear history")
                    glyph: "\u2715"
                    onClicked: remorse.execute(qsTr("Clearing history"),
                                               function () { history.forgetAll() })
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Tokens.secondaryColor
                text: qsTr("Everything happens on this device. Moji has no network permission at all.")
            }
        }
    }

    RemorsePopup {
        id: remorse
    }

    Banner {
        id: banner
    }

    Component {
        id: documentPicker

        // FilePickerPage, not DocumentPickerPage: the latter lists what the
        // tracker index calls a document, which is a narrower set than "the
        // things in my Documents folder" and leaves out an image kept there.
        // The filter is on the name because that is what the picker offers.
        FilePickerPage {
            title: qsTr("Choose a document")
            nameFilters: [ "*.pdf", "*.jpg", "*.jpeg", "*.png", "*.tif", "*.tiff" ]

            onSelectedContentPropertiesChanged: {
                if (selectedContentProperties.filePath) {
                    page.openDocument(selectedContentProperties.filePath)
                }
            }
        }
    }

    Component {
        id: imagePicker

        ImagePickerPage {
            onSelectedContentPropertiesChanged: {
                if (selectedContentProperties.filePath) {
                    // The picker is still on the stack at this point; replacing it
                    // means Back from the result returns to the main page rather
                    // than to the gallery.
                    //
                    // The selection arrives while the picker's own transition is
                    // still running, which is what logs "cannot pop while
                    // transition is in progress" and can drop the replace.
                    pageStack.completeAnimation()
                    pageStack.replace(Qt.resolvedUrl("ResultPage.qml"),
                                      { imageUrl: "file://"
                                                  + selectedContentProperties.filePath })
                }
            }
        }
    }
}
