#!/usr/bin/env python3
"""
png_to_rgb565.py — Convert PNG images into RGB565 C arrays for the
ESP32S3_Tamagotchi project (Adafruit_GFX / drawRGBBitmap format).

The project stores every sprite and background as a flat, row-major
`const uint16_t NAME[] PROGMEM = { 0x.... };` array of 16-bit RGB565
values. `drawSprite()` treats 0x0000 (TFT_BLACK) as transparent, so
sprites use fit+center with transparent padding, while full-screen
backgrounds use fill+crop and keep every pixel opaque.

--------------------------------------------------------------------------
USAGE
--------------------------------------------------------------------------
  # A 48x48 pet frame (fit+center, alpha -> transparent black):
  python tools/png_to_rgb565.py sprite Assets/Digimons/terriermon/terriermon_walk1.png ^
      --name walk_0 --size 48

  # A 128x128 full-screen background (fill+crop, fully opaque):
  python tools/png_to_rgb565.py background Assets/Backgrounds/Data_forest.png ^
      --name background_data_forest --size 128

  # Write straight into Sprites.cpp instead of stdout:
  python tools/png_to_rgb565.py sprite foo.png --name walk_0 --size 48 ^
      --out src/Sprites.cpp --append

Notes:
  * "sprite" mode: keeps aspect ratio, centers on a transparent field,
    nearest-neighbor scaling (crisp pixel art), alpha < 128 -> 0x0000.
    Opaque pixels that would land on pure black are nudged to 0x0821 so
    they are not treated as transparent by drawSprite().
  * "background" mode: scales to COVER the square then center-crops,
    LANCZOS scaling (smooth photo/art), every pixel opaque.
  * The array is emitted `--size` values per row so it stays readable and
    matches the width the C code reads it back at (width == height == size).

To wire a new sprite/background into the firmware after generating it:
  1. Declare it in Sprites.h:   extern const uint16_t NAME[] PROGMEM;
  2. (sprites) point a frame table entry at it, or (bg) call
     tft.drawRGBBitmap(x, y, NAME, size, size) via DisplayManager.
"""

import argparse
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required:  pip install pillow")


def to_rgb565(r: int, g: int, b: int) -> int:
    """Pack 8-bit RGB into a 16-bit RGB565 value."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def convert_sprite(path: str, size: int) -> list[int]:
    """Fit+center, keep aspect ratio, alpha<128 -> transparent (0x0000)."""
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    scale = min(size / w, size / h)
    nw, nh = max(1, round(w * scale)), max(1, round(h * scale))
    im = im.resize((nw, nh), Image.NEAREST)  # crisp pixel art
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.paste(im, ((size - nw) // 2, (size - nh) // 2))
    px = canvas.load()
    vals = []
    for y in range(size):
        for x in range(size):
            r, g, b, a = px[x, y]
            if a < 128:
                vals.append(0x0000)               # transparent
            else:
                v = to_rgb565(r, g, b)
                vals.append(0x0821 if v == 0x0000 else v)  # avoid accidental transparency
    return vals


def convert_background(path: str, size: int) -> list[int]:
    """Fill+crop to a square, keep aspect ratio, every pixel opaque."""
    im = Image.open(path).convert("RGB")
    w, h = im.size
    scale = max(size / w, size / h)  # COVER
    nw, nh = round(w * scale), round(h * scale)
    im = im.resize((nw, nh), Image.LANCZOS)  # smooth for photos/art
    left, top = (nw - size) // 2, (nh - size) // 2
    im = im.crop((left, top, left + size, top + size))
    px = im.load()
    return [to_rgb565(*px[x, y]) for y in range(size) for x in range(size)]


def format_array(name: str, vals: list[int], per_row: int) -> str:
    rows = len(vals) // per_row
    lines = [f"const uint16_t {name}[] PROGMEM = {{"]
    for row in range(rows):
        chunk = vals[row * per_row:(row + 1) * per_row]
        comma = "," if row < rows - 1 else ""
        lines.append("  " + ", ".join(f"0x{v:04x}" for v in chunk) + comma)
    lines.append("};")
    return "\n".join(lines)


def main() -> None:
    ap = argparse.ArgumentParser(description="PNG -> RGB565 C array converter")
    ap.add_argument("mode", choices=["sprite", "background"],
                    help="sprite = fit+center+transparent; background = fill+crop+opaque")
    ap.add_argument("image", help="source PNG path")
    ap.add_argument("--name", required=True, help="C array variable name (e.g. walk_0)")
    ap.add_argument("--size", type=int, default=48,
                    help="square output size in px (sprites ~48, background 128)")
    ap.add_argument("--out", help="write to this file instead of stdout")
    ap.add_argument("--append", action="store_true",
                    help="append to --out instead of overwriting")
    args = ap.parse_args()

    if args.mode == "sprite":
        vals = convert_sprite(args.image, args.size)
    else:
        vals = convert_background(args.image, args.size)

    block = format_array(args.name, vals, per_row=args.size)
    header = (f"// {args.name}: {args.size}x{args.size} RGB565, "
              f"{args.mode} of {args.image}\n")
    text = header + block + "\n"

    if args.out:
        mode = "a" if args.append else "w"
        with open(args.out, mode, encoding="utf-8") as f:
            if args.append:
                f.write("\n")
            f.write(text)
        print(f"[ok] {args.name} ({args.size}x{args.size}, {len(vals)} values) "
              f"-> {args.out} ({'appended' if args.append else 'written'})")
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
