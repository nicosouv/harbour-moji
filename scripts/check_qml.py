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
    whatever was in front of the camera. A Label with textFormat set to
    Text.RichText or Text.StyledText will interpret markup that was
    photographed - it will follow an <img> tag in a picture of a page, which
    turns reading a document into a network request, and it will let a
    photographed <a href> become a tappable link. Render recognised text as
    plain text.
    """
    findings = []
    for number, raw in enumerate(lines, start=1):
        line = strip_comments(raw)
        if "Text.RichText" in line or "Text.StyledText" in line:
            findings.append((number, line.strip()))
    return findings


CHECKS = [
    ("model get() inside a property binding", check_get_in_binding),
    ("property shadows one Item already has", check_shadowed_item_property),
    ("component name shadows a platform type", check_shadowed_platform_type),
    ("recognised text rendered as rich text", check_richtext),
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
