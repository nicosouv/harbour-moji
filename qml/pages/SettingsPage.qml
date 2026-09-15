import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// Built entirely from Mochi: grouped panels instead of SectionHeaders, the webOS
// switch, the drawer-revealing selector. Mixing these with Silica's own TextSwitch
// and ComboBox on one page would read as two half-finished designs, so the page
// picks a side.
Page {
    id: page

    allowedOrientations: defaultAllowedOrientations

    // The theme names, in the order Tokens publishes them. Translated here rather
    // than in the module: a QML module that calls qsTr() decides the wording for
    // every app that imports it, and owes a catalogue entry to each.
    readonly property var themeLabels: [
        qsTr("Ambience"),
        qsTr("Mochi light"),
        qsTr("Mochi dark")
    ]

    // Read from the tessdata directory, never hardcoded: language packs are
    // separate RPMs, so the list has to come from what is installed rather than
    // from what this version was built knowing about.
    readonly property var languageCodes: settings.installedLanguages

    readonly property var languageLabels: {
        var labels = []
        for (var i = 0; i < languageCodes.length; ++i) {
            labels.push(settings.languageName(languageCodes[i]))
        }
        return labels
    }

    // The first entry is the language being read; English may ride along beside
    // it. That covers the case that actually happens - a French invoice with
    // English words in it - without making the user assemble combinations.
    readonly property string primaryLanguage:
        settings.ocrLanguages.length > 0 ? settings.ocrLanguages[0] : "eng"

    readonly property bool alsoEnglish:
        settings.ocrLanguages.indexOf("eng") > 0

    function applyLanguages(primary, withEnglish) {
        if (primary === "eng" || !withEnglish) {
            settings.ocrLanguages = [primary]
        } else {
            settings.ocrLanguages = [primary, "eng"]
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

        Column {
            id: content

            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Settings")
            }

            GroupPanel {
                width: parent.width
                title: qsTr("Appearance")

                // "Ambience" is first and is the default: an app that has not been
                // told otherwise should look like it belongs to the user's
                // Sailfish rather than to itself.
                Selector {
                    width: parent.width
                    label: qsTr("Theme")
                    options: page.themeLabels
                    currentIndex: Tokens.themes.indexOf(settings.theme)
                    onCurrentIndexChanged: settings.theme = Tokens.themes[currentIndex]
                }
            }

            GroupPanel {
                width: parent.width
                title: qsTr("Recognition")

                Selector {
                    width: parent.width
                    label: qsTr("Language")
                    options: page.languageLabels
                    currentIndex: {
                        var i = page.languageCodes.indexOf(page.primaryLanguage)
                        return i < 0 ? 0 : i
                    }
                    onCurrentIndexChanged: {
                        page.applyLanguages(page.languageCodes[currentIndex],
                                            page.alsoEnglish)
                    }
                }

                Item {
                    width: parent.width
                    height: Theme.itemSizeSmall
                    // Nothing to add when English is already the language being
                    // read, and offering it would suggest it does something.
                    visible: page.primaryLanguage !== "eng"

                    Label {
                        anchors {
                            left: parent.left
                            leftMargin: Theme.paddingLarge
                            right: englishBox.left
                            rightMargin: Theme.paddingMedium
                            verticalCenter: parent.verticalCenter
                        }
                        text: qsTr("Also read English")
                        font.pixelSize: Theme.fontSizeSmall
                        color: Tokens.primaryColor
                        truncationMode: TruncationMode.Fade
                    }

                    CheckBox {
                        id: englishBox

                        anchors {
                            right: parent.right
                            rightMargin: Theme.paddingLarge
                            verticalCenter: parent.verticalCenter
                        }
                        checked: page.alsoEnglish
                        onCheckedChanged: {
                            page.applyLanguages(page.primaryLanguage, checked)
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: Theme.itemSizeMedium

                    Column {
                        anchors {
                            left: parent.left
                            leftMargin: Theme.paddingLarge
                            right: contrastSwitch.left
                            rightMargin: Theme.paddingMedium
                            verticalCenter: parent.verticalCenter
                        }

                        Label {
                            width: parent.width
                            text: qsTr("Even out the lighting")
                            font.pixelSize: Theme.fontSizeSmall
                            color: Tokens.primaryColor
                            truncationMode: TruncationMode.Fade
                        }

                        Label {
                            width: parent.width
                            text: qsTr("Helps photographs; does nothing for a flat scan")
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Tokens.secondaryColor
                            wrapMode: Text.Wrap
                        }
                    }

                    ToggleSwitch {
                        id: contrastSwitch

                        anchors {
                            right: parent.right
                            rightMargin: Theme.paddingLarge
                            verticalCenter: parent.verticalCenter
                        }
                        checked: settings.enhanceContrast
                        onCheckedChanged: settings.enhanceContrast = checked
                    }
                }

                Item {
                    width: parent.width
                    height: Theme.itemSizeMedium

                    Column {
                        anchors {
                            left: parent.left
                            leftMargin: Theme.paddingLarge
                            right: rotateSwitch.left
                            rightMargin: Theme.paddingMedium
                            verticalCenter: parent.verticalCenter
                        }

                        Label {
                            width: parent.width
                            text: qsTr("Straighten photos")
                            font.pixelSize: Theme.fontSizeSmall
                            color: Tokens.primaryColor
                            truncationMode: TruncationMode.Fade
                        }

                        Label {
                            width: parent.width
                            text: qsTr("Detect the page orientation before reading")
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Tokens.secondaryColor
                            wrapMode: Text.Wrap
                        }
                    }

                    ToggleSwitch {
                        id: rotateSwitch

                        anchors {
                            right: parent.right
                            rightMargin: Theme.paddingLarge
                            verticalCenter: parent.verticalCenter
                        }
                        checked: settings.autoRotate
                        onCheckedChanged: settings.autoRotate = checked
                    }
                }
            }

            GroupPanel {
                width: parent.width
                title: qsTr("About")

                PanelRow {
                    width: parent.width
                    title: qsTr("Taking a readable photo")
                    detail: qsTr("What helps, and what cannot be recovered")
                    glyph: "!"
                    onClicked: pageStack.push(Qt.resolvedUrl("HintsPage.qml"))
                }

                PanelRow {
                    width: parent.width
                    title: qsTr("About Moji OCR")
                    glyph: "?"
                    onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
                }
            }

            // One string literal, not a concatenation: lupdate extracts the
            // literal inside qsTr() and would take only the first piece of a
            // "a" + "b", leaving the rest silently untranslated.
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Tokens.secondaryColor
                text: qsTr("Installed languages are listed above. Moji never connects to the network — the sandbox does not permit it.")
            }
        }
    }
}
