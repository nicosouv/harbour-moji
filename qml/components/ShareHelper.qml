import QtQuick 2.0

// Handing text to whatever the user has installed that accepts it.
//
// Not visual, and in components/ rather than in a page, because two places need
// it: the result page's share button and the cover's share action. A copy in each
// is a copy that drifts.
//
// Deliberately not a singleton and not in the Mochi module. Mochi is a design
// language - colours, panels, motion - and sharing is not design. A singleton
// would also reintroduce the trap qml/Mochi/README.md describes: a directory
// import and a module import are separate identities to the engine, so the same
// singleton can exist twice.
QtObject {
    id: helper

    // Emitted instead of showing anything, so the caller decides how to say it -
    // the result page has a Banner, the cover has nowhere to put a message.
    signal failed(string reason)

    function share(text, title) {
        if (text === "") {
            helper.failed(qsTr("There is no text to share"))
            return false
        }

        // Built from a string at the moment it is used, rather than imported at
        // the top of the file.
        //
        // An import names a module the device may not have, and a QML file that
        // imports something missing does not lose that one feature - the whole
        // file fails to load and the page comes up blank, with the reason in the
        // journal. That has cost this project a release once already
        // (Image.autoTransform under the wrong QtQuick import), so a component
        // that is not certain to be there is built where a failure can be caught
        // and said out loud.
        var action = null
        try {
            action = Qt.createQmlObject(
                'import QtQuick 2.0; import Sailfish.Share 1.0; ShareAction { }',
                helper, "shareAction")
        } catch (e) {
            action = null
        }

        if (action === null) {
            helper.failed(qsTr("Sharing is not available on this device"))
            return false
        }

        action.resources = [ { "type": "text/plain", "data": text } ]
        action.title = title
        action.trigger()
        action.destroy()
        return true
    }
}
