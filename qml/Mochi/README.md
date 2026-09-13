# Mochi

A small QML module for Sailfish OS, borrowing from the webOS Mojo widget
vocabulary and the Mochi design language: grouped list panels, quiet hairlines
with a barely perceptible curve, short decelerating motion.

Targets the system Qt 5.6 that Sailfish ships. No Qt Quick Shapes, no
Controls 2, no per-delegate Canvas.

## Install

As integrated here:

```
qml/
  harbour-imtrix.qml
  Mochi/
    qmldir
    Tokens.qml          singleton: palette, geometry, motion
    PanelBox.qml        internal: a rectangle rounded at one end
    CurvedSeparator.qml
    GroupPanel.qml
    PanelRow.qml
    Drawer.qml
    ToggleSwitch.qml
    CheckBox.qml
    Selector.qml
    Segmented.qml
  pages/
```

```qml
import Mochi 1.0
```

`main.cpp` puts `qml/` on the engine's import path, so that one spelling works
from every depth. The alternative the module was written for - relying on the
implicit path, which is the importing file's own directory, and saying
`import "../Mochi"` from `pages/` - is not merely uglier here: a directory
import and a module import are separate identities to the engine, so `Tokens`
would be instantiated twice and setting `mode` through one would leave the
components reading the other. Both spellings in one app is the version of that
bug that takes an afternoon to find.

For the same reason the module's own files import themselves by module name
rather than `import "."`.

`CMakeLists.txt` installs `qml/` as a directory, so no file list needs
touching when the module grows. `*.md` is excluded, which is why this file does
not reach the RPM.

## Usage

```qml
SilicaFlickable {
    Column {
        width: parent.width
        spacing: Theme.paddingLarge

        GroupPanel {
            width: parent.width
            title: "UNREAD"

            Repeater {
                model: roomList
                delegate: PanelRow {
                    width: parent.width
                    title: model.displayName
                    detail: model.lastMessage
                    badge: model.notificationCount
                }
            }

            // Mojo's create action: the last row of the group, in the avatar
            // column, instead of a floating button.
            PanelRow {
                width: parent.width
                title: "Join a room"
                glyph: "+"
                accent: true
            }
        }
    }
}
```

**Where an action goes.** Mojo put the create action *in* the list, as its last
row, rather than behind a floating button — and the same reasoning puts every
other verb there too. A pulley carries what is global to the app: settings, about.
A page's own actions are rows in a group, where they are visible without being
discovered.

Filling a pulley with a page's verbs is the Silica habit this module exists to
replace, and it is an easy one to fall back into, because a pulley is the quickest
place to put something.

`PanelRow` is not from the original drop — `GroupPanel` has no use without a row,
and every consumer inventing its own is how a design language stops being one.

Note what the delegate is **not** doing: no wrapper, no separator, no index
threaded through by hand. Onyx gave each row a bottom border and the last row
none, so the divider belongs to the row; and `GroupPanel` walks its children the
way `:first-child`/`:last-child` did, handing each row its rounding and its
separator phase. Position is measured over every child that occupies space, so a
group whose last child is a `Selector` rounds the `Selector`.

`PanelRow` is a `MouseArea`, so `onClicked` works and `pressed` drives the
highlight. Deliberately not a Silica `BackgroundItem`: a row at either end of a
group has to round its highlight to the panel's corner, and Silica's highlight is
not addressable.

## Controls

`ToggleSwitch`, `CheckBox`, `Selector`, `Segmented` and `Drawer` are rebuilt from
Onyx's own stylesheets rather than from recollection, which changed four things
a from-memory version gets wrong:

| | Onyx's value | why it matters |
|---|---|---|
| Track | 32px high, min-width 64 | a 2:1 track, not a square |
| Knob | 30px, 1px margin, floats to the far side | the label sits where the knob is not |
| Corners | `border-radius: 3px` on that 32px track | **a full pill reads as iOS** and loses the homage |
| On / off | `#8BBA3D` / `#B1B1B1`, knob `#F6F6F6` | that green is the single most recognisable value |
| Label | uppercase, bold, inside the track | `onContent: 'On'` / `offContent: 'Off'` |

`onyx.ToggleButton` handles `ondragstart/ondrag/ondragfinish`, and Mojo's own
docs say a toggle "will switch between two states each time it is tapped **or
swiped** in the direction opposite its current state" — so `ToggleSwitch`
answers a drag as well as a tap.

Motion comes from `enyo.Animator`'s defaults: **350ms, `enyo.easing.cubicOut`**.
That is where `Easing.OutCubic` comes from, and it is `Tokens.durPage`. Controls
use the shorter `durFast`/`durBase`, because 350ms under a fingertip reads as
sluggish.

`Selector` departs from Onyx deliberately. Onyx's picker is a popup positioned
*over* its trigger, so the selected row lands on the button it came from; that is
the better gesture and it is also the one that gets clipped inside a
`SilicaFlickable` unless it is reparented to the page. `Selector` reveals its
options in a `Drawer` instead — same information, same motion, and it survives
being placed inside a `GroupPanel`, which is where a selector actually goes.

`Tokens` is a singleton. Set the mode once, early:

```qml
Component.onCompleted: Tokens.mode = "mochi"   // or "mochiDark", or leave
                                              // it on "ambience"
```

## What to verify on device

Settings → Design → "Mochi preview" opens `pages/MochiTestPage.qml`, a harness
that exists only to answer these questions on a screen. It needs no account and
touches no model, so it works on a fresh install. Nothing depends on it:
deleting it and its one entry point in `SettingsPage.qml` removes the harness
completely.

Already checked off-device, in a container with plain Qt5 and a stubbed Silica:
every component instantiates, and `Tokens` resolves to a single shared instance
across the module boundary. What that cannot cover is below.

1. **The shader compiles.** `CurvedSeparator` uses a GLSL ES fragment shader,
   and no CI lane here has a GPU — the software renderer skips `ShaderEffect`
   rather than compiling it. If the log shows a compile error or rows render as
   solid blocks, the shader is the cause, not the layout. A Canvas drawn once in
   `onPaint` with `renderTarget: Canvas.Image` is the fallback; it costs a
   texture per row. Note the shader asks for `highp` in a fragment shader, which
   GLSL ES 1.00 only guarantees when `GL_FRAGMENT_PRECISION_HIGH` is defined; if
   it fails to compile, that is the first line to suspect.
2. **The hairline survives.** On a dense screen a sub-pixel stroke disappears.
   `Tokens.hairline` rounds `Theme.pixelRatio`, but confirm visually. The
   harness draws the same rule at 0.5, 1, 1.5, 2 and 4 px to calibrate this.
3. **The Silica API is as assumed.** The stub was written from memory, so it
   proves the module is internally consistent, not that Silica agrees. The
   surface is deliberately small: no `BackgroundItem` any more, and every `Theme`
   property used here is one this app's own shipped pages already use — checked by
   diffing the two sets, which is how `Theme.iconSizeSmall` got caught. What is
   left to confirm is `Label.truncationMode` and `font.capitalization`.
4. **A white panel against a dark ambience.** The harness switches between all
   three themes live, with the same `Segmented` an app's theme setting would use.
   If grouped panels hold up in ambience mode, the integration stays native; if
   not, `mochi` or `mochiDark` is the answer and the app owns its own identity.
5. **Scrolling stays smooth** with a few hundred rows. If it does not, the
   separator is the first thing to switch off to find out whether it is to
   blame. The harness is a `Column`, not a `ListView`, so it does not answer
   this — only a real room list does.

## Notes

- Amplitude above roughly 2px stops reading as a hairline and starts reading as
  decoration. `Tokens.curveAmplitude` is deliberately conservative.
- `phase` is derived from the delegate index, never random: a random phase
  shifts when the view recycles delegates and the list appears to twitch while
  scrolling.
- Keep motion at `Tokens.durFast`/`durBase` with `Easing.OutCubic`. The only
  place an overshoot belongs is end-of-scroll, and Silica already does that.
- `GroupPanel` is built the way Onyx built it: the rounding belongs to the rows,
  not to a clipped container. Clipping was the obvious QML translation and it is
  wrong — Qt Quick `clip` clips to the bounding box and never to the radius, so a
  pressed end row painted its highlight square into the corner. `PanelBox` rounds
  one end of a rectangle by patching over the other, which is the standard answer
  when there is no Shapes module.
- Every `Theme` property the module touches is one that already ships in this
  app's own pages, checked by diffing the two sets. That is not pedantry:
  assigning from a `Theme` property Silica does not have fails the entire file at
  load, and the first version of `Tokens` reached for `Theme.iconSizeSmall`,
  which nothing here proves exists.
- The notification banner and the grouped-list adoption in `RoomsPage` are not
  here yet. The banner is mocked up in the harness so it can be judged before it
  becomes API.
