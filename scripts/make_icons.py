#!/usr/bin/env python3
"""Produces the app icon at every size the RPM ships.

    python3 scripts/make_icons.py

Resizes icons/harbour-moji.png - the master artwork, committed with its alpha
channel - down to icons/{86,108,128,172}x.../harbour-moji.png.

The master was cut out of a design mockup that showed the badge sitting on a
photograph of a desk. To redo that, or to do it again from a new mockup:

    python3 scripts/make_icons.py --extract ~/Downloads/mockup.jpg

Everything the extraction needs is measured from the image rather than eyeballed:
the badge is found by looking for the only strongly teal region, and the corner
radius is derived from how far along its diagonal each corner becomes solid. Three
corners are round and the top right is square, which is the Sailfish silhouette.

Two details that matter, and are the reason this is a script and not a one-off
crop in an image editor:

  - The mask is inset by a few pixels. The badge in the mockup has a drop shadow
    and an antialiased rim where the desk showed through it, and cutting exactly
    on the outline keeps a dirty brown fringe that is invisible at 407px and
    obvious once the icon is on a dark ambience.
  - The saturation is put back. The badge is semi-transparent in the mockup, so
    the wood underneath washes it out; what gets extracted is the colour the photo
    left behind, not the colour it was drawn as.

No upscaling happens anywhere: the master is 407px and the largest shipped icon is
172px, so every size is a downsample.
"""

import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageEnhance
except ImportError:
    sys.exit("needs Pillow: pip install Pillow")

ROOT = Path(__file__).resolve().parent.parent
MASTER_PATH = ROOT / "icons" / "harbour-moji.png"
SIZES = (86, 108, 128, 172)

# Cut this far inside the outline, past the mockup's shadow and the rim where the
# desk bled through.
INSET = 3.0

# The mockup's badge is semi-transparent over light wood; this puts back roughly
# what that took out.
SATURATION = 1.45

# The mask is drawn at this multiple and downsampled, so the curve is antialiased
# rather than stepped.
SUPERSAMPLE = 8


def find_badge(image):
    """Bounding box of the badge: the only strongly teal thing in the mockup."""
    width, height = image.size
    pixels = image.load()

    min_x, min_y, max_x, max_y = width, height, -1, -1
    for y in range(0, height, 2):
        for x in range(0, width, 2):
            r, g, b = pixels[x, y]
            if g > 90 and g - r > 22 and b > 70 and abs(g - b) < 90:
                min_x, max_x = min(min_x, x), max(max_x, x)
                min_y, max_y = min(min_y, y), max(max_y, y)

    if max_x < 0:
        sys.exit("no teal badge found in that image")
    return (min_x, min_y, max_x + 1, max_y + 1)


def corner_radius(badge):
    """Derived from how far along its diagonal the top-left corner fills in.

    A rounded corner's arc stands off its own corner by r * (1 - 1/sqrt(2)), so
    measuring that distance gives r without having to guess at it.
    """
    pixels = badge.load()
    for distance in range(200):
        r, g, b = pixels[distance, distance]
        if (b - r) > -15:
            return distance / (1 - 2 ** -0.5)
    sys.exit("could not measure the corner radius")


def build_mask(size, radius):
    width, height = size
    scale = SUPERSAMPLE
    mask = Image.new("L", (width * scale, height * scale), 0)
    draw = ImageDraw.Draw(mask)

    inset = INSET * scale
    draw.rounded_rectangle((inset, inset,
                            width * scale - 1 - inset, height * scale - 1 - inset),
                           radius=radius * scale, fill=255)

    # Square the top-right corner back in: the badge is solid right into it.
    draw.rounded_rectangle((width * scale - radius * scale, inset,
                            width * scale - 1 - inset, radius * scale),
                           radius=width * scale * 0.02, fill=255)

    return mask.resize((width, height), Image.LANCZOS)


def extract(mockup_path):
    source = Image.open(mockup_path).convert("RGB")
    box = find_badge(source)
    badge = source.crop(box)
    print(f"badge found at {box}, {badge.size[0]}x{badge.size[1]}")

    radius = corner_radius(badge)
    print(f"corner radius measured at {radius:.0f}px")

    master = ImageEnhance.Color(badge).enhance(SATURATION).convert("RGBA")
    master.putalpha(build_mask(badge.size, radius))

    MASTER_PATH.parent.mkdir(parents=True, exist_ok=True)
    master.save(MASTER_PATH)
    print(f"wrote {MASTER_PATH.relative_to(ROOT)}")
    return master


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--extract":
        if len(sys.argv) < 3:
            sys.exit("usage: make_icons.py --extract <mockup image>")
        master = extract(sys.argv[2])
    else:
        if not MASTER_PATH.exists():
            sys.exit(f"{MASTER_PATH} is missing; run with --extract <mockup>")
        master = Image.open(MASTER_PATH).convert("RGBA")

    for size in SIZES:
        if size > master.size[0]:
            sys.exit(f"master is only {master.size[0]}px; cannot make {size}px")
        directory = ROOT / "icons" / f"{size}x{size}"
        directory.mkdir(parents=True, exist_ok=True)
        path = directory / "harbour-moji.png"
        master.resize((size, size), Image.LANCZOS).save(path)
        print(f"wrote {path.relative_to(ROOT)}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
