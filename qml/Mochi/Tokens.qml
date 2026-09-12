pragma Singleton

import QtQuick 2.0
import Sailfish.Silica 1.0

// Single source of truth for the Mochi look. Components never hardcode a
// colour, radius or duration: they read it from here.
//
// Three themes:
//   "ambience"  - derive everything from the user's Sailfish ambience, so an app
//                 still feels native. Default.
//   "mochi"     - the webOS light palette: white panels on a warm grey page.
//                 For an app that owns its whole visual identity.
//   "mochiDark" - the same vocabulary in webOS's own dark surfaces. Onyx's
//                 Groupbox was #4C4C4C with white text, which is where the panel
//                 colour below comes from.
//
// Set it once, early, or bind it to a setting:
//   Component.onCompleted: Tokens.mode = "mochi"
//
// themes is the list an app's theme selector should offer. The labels are
// deliberately not here: a module that calls qsTr() owes five catalogue entries
// to every app that imports it, so naming the themes is the app's job.
QtObject {
    property string mode: "ambience"

    readonly property var themes: ["ambience", "mochi", "mochiDark"]

    // Compared for equality, not inequality. "mode !== 'mochi'" would have made
    // every theme added later behave as the ambience one.
    readonly property bool ambient: mode === "ambience"
    readonly property bool dark: mode === "mochiDark"

    // Surfaces. Page is the backdrop, panel is the grouped list container. In
    // ambience mode the page is transparent so the user's wallpaper shows.
    readonly property color pageColor: ambient ? "transparent"
                                              : (dark ? "#333333" : "#EDECE6")

    readonly property color panelColor: ambient
            ? Theme.rgba(Theme.highlightBackgroundColor, 0.08)
            : (dark ? "#4C4C4C" : "#FFFFFF")

    // Text.
    readonly property color primaryColor: ambient ? Theme.primaryColor
                                                  : (dark ? "#FFFFFF" : "#2C2C2A")

    readonly property color secondaryColor: ambient ? Theme.secondaryColor
                                                    : (dark ? "#B1B1B1" : "#85837B")

    readonly property color accentColor: ambient ? Theme.highlightColor
                                                 : (dark ? "#9FD0F5" : "#185FA5")

    // Hairlines. Deliberately weaker than Silica's own dividers: grouping
    // carries the hierarchy, the line only separates rows.
    readonly property color separatorColor: ambient
            ? Theme.rgba(Theme.primaryColor, 0.18)
            : (dark ? "#5E5E5E" : "#E0DED6")

    // Geometry. Always scale by pixelRatio or it breaks on dense screens.
    readonly property real panelRadius: 12 * Theme.pixelRatio
    readonly property real hairline: Math.max(1, Math.round(Theme.pixelRatio))

    // Peak deviation of a curved separator from its centre line, in px.
    // Above ~2px this stops reading as a hairline and becomes a pattern.
    readonly property real curveAmplitude: 1.5 * Theme.pixelRatio

    // Control geometry. Onyx's toggle track is 32px at 1x with a 30px knob, a
    // 1px margin and min-width 64 - so a 2:1 track holding an almost-square
    // knob, which is what gets reproduced here.
    //
    // The height comes from iconSizeMedium rather than the closer-sounding
    // iconSizeSmall for two reasons: no shipped file in this app references
    // iconSizeSmall, so it is an unverified bet on Silica's API and a wrong one
    // fails the whole file at load; and a track scaled straight from 32px is a
    // thin target for a fingertip - webOS 3.0.2's own release notes record the
    // ToggleButton being restyled for a larger tap area.
    readonly property real controlHeight: Theme.iconSizeMedium
    readonly property real controlMargin: Math.max(1, Math.round(Theme.pixelRatio))

    // 3-4px on a 32px track in Onyx: webOS toggles were rounded rectangles, not
    // pills. Keeping them square-ish is most of what makes them recognisable,
    // so this is deliberately far below controlHeight / 2.
    readonly property real controlRadius: 4 * Theme.pixelRatio

    // The webOS switch colours, kept literal in every theme. Everything else
    // here follows the ambience or the palette, but this green is the single most
    // recognisable value in the whole vocabulary - deriving it from the user's
    // ambience would throw away the only detail an ex-Pre owner is certain to
    // know.
    readonly property color onColor: "#8BBA3D"
    readonly property color offColor: "#B1B1B1"
    readonly property color knobColor: "#F6F6F6"

    // The track a Segmented control sits in.
    readonly property color trackColor: ambient
            ? Theme.rgba(Theme.primaryColor, 0.12)
            : (dark ? "#2B2B2B" : "#E1E1E1")

    // Onyx's picker marks the current row in a pale blue rather than with the
    // platform accent.
    readonly property color selectionColor: ambient
            ? Theme.rgba(Theme.highlightBackgroundColor, 0.35)
            : (dark ? "#22405C" : "#CDE7FE")

    // Press feedback. Mochi rows draw their own rather than inheriting
    // BackgroundItem's, because a row at the end of a group has to round the
    // highlight to the panel's corner and Silica's is not addressable.
    readonly property color pressedColor: ambient
            ? Theme.rgba(Theme.highlightBackgroundColor, 0.25)
            : (dark ? "#2A3A48" : "#DDEAF6")

    // The notification banner was always dark in webOS, in every theme: it reads
    // as the system speaking rather than as part of the page.
    readonly property color bannerColor: "#2C2C2A"
    readonly property color bannerTextColor: "#FFFFFF"

    readonly property real disabledOpacity: 0.4

    // Motion. Short and decelerating, never bouncing. durPage is enyo.Animator's
    // own default - 350ms with enyo.easing.cubicOut - which is where OutCubic
    // comes from in the first place; the shorter values are for controls, where
    // 350ms reads as sluggish under a fingertip.
    readonly property int durFast: 150
    readonly property int durBase: 200
    readonly property int durSlow: 250
    readonly property int durPage: 350
    readonly property int easingType: Easing.OutCubic
}
