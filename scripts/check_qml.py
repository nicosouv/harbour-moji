#!/usr/bin/env python3
"""Static checks on the QML sources.

Runs anywhere, no Qt needed. These are the Silica traps that cost an
afternoon each and that qmllint has nothing to say about: they all build,
load and ship, and then behave wrongly at runtime.

qml/Mochi/ is checked along with everything else. It is a shared module, so a
trap introduced there would be inherited by every app that vendors it.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
QML_DIRS = [ROOT / "qml"]

# Start of a property binding: "property int foo: ..." or "readonly property ..."
BINDING_START = re.compile(r"^\s*(readonly\s+)?property\s+\w+(<[^>]+>)?\s*\w+\s*:")
# Start of a function body, where the same call is harmless
FUNCTION_START = re.compile(r"^\s*function\s+\w+\s*\(")


def qml_files():
    for directory in QML_DIRS:
        yield from sorted(directory.rglob("*.qml"))


def strip_comments(line):
    return re.sub(r"//.*$", "", line)


def check_get_in_binding(path, lines):
    """A model's get() inside a property binding.

    The wrapper object get() returns is owned by the model, but a binding
    that reads it keeps a dependency guard on it. When the component is
    destroyed the binding's destructor can touch that wrapper after the
    model has released it, which aborts the process with "pure virtual
    method called". Compute it in a function called from a signal handler
    instead - CoverPage.refresh() is the shape to copy.
    """
    findings = []
    in_binding = False
    depth = 0
    binding_line = 0

    for number, raw in enumerate(lines, start=1):
        line = strip_comments(raw)

        if not in_binding:
            if BINDING_START.search(line) and not FUNCTION_START.search(line):
                in_binding = True
                binding_line = number
                depth = line.count("{") - line.count("}")
                if depth <= 0 and ".get(" not in line:
                    in_binding = False
                elif ".get(" in line:
                    findings.append((binding_line, raw.strip()))
                    in_binding = depth > 0
            continue

        if ".get(" in line:
            findings.append((binding_line, line.strip()))
        depth += line.count("{") - line.count("}")
        if depth <= 0:
            in_binding = False

    return findings


# Properties every Item already has. Declaring one of these shadows the
# built-in, and the shadow only reaches the component's own root scope.
ITEM_PROPERTIES = {
    "clip", "state", "states", "opacity", "visible", "enabled", "focus",
    "rotation", "scale", "smooth", "antialiasing", "parent", "children",
    "data", "transform", "x", "y", "z", "width", "height",
    "implicitWidth", "implicitHeight", "baselineOffset", "activeFocus",
}

PROPERTY_DECL = re.compile(
    r"^\s*(?:readonly\s+)?property\s+"
    r"(?:var|int|real|double|bool|string|url|color|list<[^>]+>|[A-Z]\w*)\s+(\w+)\s*[:;]?")


def check_shadowed_item_property(path, lines):
    """A property named like one Item already has.

    The declaration shadows the built-in, but only in the component's own
    root scope. Inside any nested Item, a bare name resolves to that Item's
    inherited property instead, silently and with a plausible type - so the
    value reads back as something entirely different depending on where it
    is read from, and nothing logs.
    """
    findings = []
    for number, raw in enumerate(lines, start=1):
        line = strip_comments(raw)
        match = PROPERTY_DECL.search(line)
        if match and match.group(1) in ITEM_PROPERTIES:
            findings.append((number, line.strip()))
    return findings


# Types Silica and QtQuick already define. A file in qml/components/ named
# after one of these shadows it in every file that imports that directory.
PLATFORM_TYPES = {
    # Silica
    "SectionHeader", "PageHeader", "Page", "Button", "Label", "TextField",
    "TextArea", "Slider", "Switch", "TextSwitch", "ComboBox", "ContextMenu",
    "MenuItem", "Dialog", "DialogHeader", "ViewPlaceholder", "SearchField",
    "IconButton", "BackgroundItem", "ListItem", "RemorseItem", "RemorsePopup",
    "Separator", "SilicaListView", "SilicaGridView", "SilicaFlickable",
    "PullDownMenu", "PushUpMenu", "BusyIndicator", "GlassItem", "DockedPanel",
    "ProgressBar", "ValueButton", "Icon", "Theme", "Formatter", "DetailItem",
    "PasswordField", "CoverBackground", "CoverAction", "CoverActionList",
    # QtQuick
    "Item", "Rectangle", "Image", "Text", "Row", "Column", "Grid", "Flow",
    "Repeater", "Loader", "Timer", "Connections", "Component", "MouseArea",
    "Flickable", "ListView", "GridView", "ListModel", "Animation", "Gradient",
}


def check_shadowed_platform_type(path, lines):
    """A component named after a type Silica or QtQuick already defines.

    The local one wins in every file that imports its directory, silently
    and at load time. Any page that used the platform type and relied on a
    property the local one lacks then fails to load at all.
    """
    if path.parent.name != "components" or path.stem not in PLATFORM_TYPES:
        return []
    return [(1, f"{path.stem} is already a Silica or QtQuick type")]


def check_richtext(path, lines):
    """Text rendered as rich text.

    Every string this app displays may be OCR output, and OCR output is
    whatever was in front of the camera. A Label or TextEdit whose textFormat
    admits markup - RichText, StyledText, or AutoText, which decides for
    itself - will interpret markup that was photographed - it will follow an <img> tag in a picture of a page, which
    turns reading a document into a network request, and it will let a
    photographed <a href> become a tappable link. Render recognised text as
    plain text.
    """
    findings = []
    for number, raw in enumerate(lines, start=1):
        line = strip_comments(raw)
        if any(token in line for token in ("Text.RichText", "Text.StyledText",
                                           "TextEdit.RichText", "TextEdit.AutoText",
                                           "Text.AutoText")):
            findings.append((number, line.strip()))
    return findings


# Sizing a parent from what a child ended up painting.
SIZE_BINDING = re.compile(r"^\s*(?:implicit)?(?:width|height)\s*:")
PAINTED = re.compile(r"\bpainted(?:Width|Height)\b")


def check_painted_size_binding(path, lines):
    """A width or height bound to a child's paintedWidth/paintedHeight.

    An Image anchored to fill its parent takes its size from that parent, so
    its paintedWidth and paintedHeight are downstream of it. Sizing the parent
    from them closes the circle. Qt prints "Binding loop detected" once and
    then carries on re-evaluating the layout, which on a device reads as the
    page having frozen rather than as anything being wrong - so the warning
    scrolls past in a log nobody is reading and the bug ships.

    Take the proportions from the image instead: implicitWidth and
    implicitHeight are what the loader decoded and depend on nothing in the
    layout.
    """
    findings = []
    for number, raw in enumerate(lines, start=1):
        line = strip_comments(raw)
        if SIZE_BINDING.search(line) and PAINTED.search(line):
            findings.append((number, line.strip()))
    return findings


# Properties that need a newer QtQuick than the ubiquitous "import QtQuick 2.0".
#
# Getting this wrong does not degrade gracefully: the property is simply absent
# and the entire file fails to load, so the page comes up blank with the reason
# only in the journal.
QTQUICK_IMPORT = re.compile(r"^\s*import\s+QtQuick\s+2\.(\d+)")

VERSIONED_PROPERTIES = {
    "autoTransform": 5,   # Image.autoTransform, Qt 5.5 / QtQuick 2.5
}


def check_qtquick_version(path, lines):
    """A property that needs a newer QtQuick than the file imports.

    A QML import names the API version being asked for, not the one the device
    has. Sailfish ships Qt 5.6, so QtQuick 2.5 resolves happily - but a file
    that says "import QtQuick 2.0" and then sets Image.autoTransform does not
    merely lose that property. The whole file fails to load, the page is blank,
    and the only explanation is a line in the journal.
    """
    imported = None
    for raw in lines:
        match = QTQUICK_IMPORT.search(strip_comments(raw))
        if match:
            imported = int(match.group(1))
            break

    if imported is None:
        return []

    findings = []
    for number, raw in enumerate(lines, start=1):
        line = strip_comments(raw)
        for name, needed in VERSIONED_PROPERTIES.items():
            if re.search(r"\b" + name + r"\s*:", line) and imported < needed:
                findings.append((number, f"{line.strip()}  (needs QtQuick 2.{needed}, "
                                         f"file imports 2.{imported})"))
    return findings


# A size binding that names another item's opposite dimension.
SIZE_PROP = re.compile(r"^\s*(width|height)\s*:\s*(.*)$")
ID_DECL = re.compile(r"^\s*id:\s*(\w+)\s*$")
CROSS_REF = re.compile(r"\b(\w+)\.(width|height)\b")


def _size_graph(lines):
    """Which item's size binding reads which other item's size.

    Blocks are followed by counting braces, which is enough for QML written
    the way this project writes it: one brace per line end. A binding's
    continuation lines are taken as the more-indented lines that follow it.
    """
    stack = [{"id": None}]
    edges = {}          # id -> {(other_id, other_prop): (prop, line_no, text)}
    pending = None      # (owner_id, prop, line_no, text, indent)

    for number, raw in enumerate(lines, start=1):
        line = strip_comments(raw)
        if not line.strip():
            continue
        indent = len(line) - len(line.lstrip())

        if pending and indent <= pending[4] and not SIZE_PROP.match(line):
            pending = None

        match = ID_DECL.match(line)
        if match:
            stack[-1]["id"] = match.group(1)

        match = SIZE_PROP.match(line)
        if match and stack[-1]["id"]:
            pending = (stack[-1]["id"], match.group(1), number,
                       line.strip(), indent)

        if pending:
            owner, prop, number0, text, _ = pending
            for other, other_prop in CROSS_REF.findall(line):
                if other == owner:
                    continue
                edges.setdefault(owner, {})[(other, other_prop)] = (prop, number0, text)

        opened = line.count("{")
        closed = line.count("}")
        for _ in range(opened):
            stack.append({"id": None})
        for _ in range(closed):
            if len(stack) > 1:
                stack.pop()
            pending = None

    return edges


def check_size_cycle(path, lines):
    """Two items sizing each other across the two dimensions.

    An item whose height is a child's width, inside a child whose width is
    that item's height, is a binding loop. Qt reports it once and then goes
    on re-evaluating the layout, so on a device the page does not warn - it
    stops. The one that shipped was in ResultPage: the frame reserved
    canvas.width for a quarter-turned photo while the canvas took its width
    from frame.height.

    Break it by deriving both from something outside the pair - the width the
    page gives the frame, and the photo's own proportions.
    """
    edges = _size_graph(lines)
    findings = []
    seen = set()
    for owner, refs in edges.items():
        for (other, other_prop), (prop, number, text) in refs.items():
            back = edges.get(other, {}).get((owner, prop))
            if back and back[0] == other_prop:
                key = tuple(sorted([owner, other]))
                if key in seen:
                    continue
                seen.add(key)
                findings.append((number, f"{text}  (and {other}.{other_prop} "
                                         f"reads {owner}.{prop})"))
    return findings


CHECKS = [
    ("model get() inside a property binding", check_get_in_binding),
    ("property shadows one Item already has", check_shadowed_item_property),
    ("component name shadows a platform type", check_shadowed_platform_type),
    ("recognised text rendered as rich text", check_richtext),
    ("size bound to a child's painted size", check_painted_size_binding),
    ("property needs a newer QtQuick than imported", check_qtquick_version),
    ("two items sizing each other across dimensions", check_size_cycle),
]


def main():
    failures = 0
    checked = 0

    for path in qml_files():
        checked += 1
        lines = path.read_text(encoding="utf-8").splitlines()
        for title, check in CHECKS:
            for number, snippet in check(path, lines):
                rel = path.relative_to(ROOT)
                print(f"{rel}:{number}: {title}")
                print(f"    {snippet}")
                print(f"    {check.__doc__.strip().splitlines()[0]}")
                failures += 1

    print(f"\nchecked {checked} QML files, {failures} problem(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
