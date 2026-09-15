#!/usr/bin/env python3
"""
split_sheet.py — Auto-split a sprite sheet into individual transparent PNGs.

What it does:
  1. Detects the background color (samples the 4 corners; must agree).
  2. Keys that color out to full transparency (with a tolerance, so slightly
     anti-aliased edge pixels near the background still get removed).
  3. Finds connected components of remaining (non-background) pixels —
     i.e. each separate sprite blob on the sheet.
  4. Crops each blob to its EXACT tight bounding box: no row/col of fully
     transparent pixels left on any edge, and no opaque pixel cut off.
  5. Filters out tiny noise blobs (stray anti-aliasing specks) below
     --min-area pixels.
  6. Saves each sprite as its own RGBA PNG, in reading order (top-to-bottom,
     left-to-right), plus a contact-sheet preview so you can sanity check
     the split before feeding sprites into your RGB565 pipeline.

Usage:
  python3 split_sheet.py sheet.png --out out_dir
  python3 split_sheet.py sheet.png --out out_dir --tolerance 40 --min-area 20
  python3 split_sheet.py sheet.png --out out_dir --prefix tsunomon
"""

import argparse
import os
import sys
from PIL import Image

try:
    import numpy as np
except ImportError:
    print("This script requires numpy. Install with: pip install numpy --break-system-packages")
    sys.exit(1)


def _exact_color_components(arr, color, tol=6):
    """Connected components of pixels matching `color` almost exactly."""
    diff = np.abs(arr[:, :, :3].astype(int) - np.array(color).astype(int))
    mask = (diff.sum(axis=2) <= tol).astype(np.uint8) * 255
    return find_components(mask)


def detect_background_colors(arr, min_fill_fraction=0.03, top_n=12):
    """
    Look at the most common colors on the sheet, but don't just trust total
    pixel count — a recurring SPRITE color (black outline, a repeated body
    color, a drop-shadow color used under every frame) can also have high
    total coverage while being scattered across many small-to-medium,
    SEPARATE blobs (one per sprite instance).
    A true background FILL color instead forms at least one blob that
    covers a large FRACTION OF THE WHOLE SHEET (the outer margin, or a
    tile's box background) — typically 30-90%+ of the image — whereas even
    a fairly big single sprite element (a whole body, a shadow oval) is
    usually well under a few percent of the total sheet area. Using a
    fraction (not a fixed pixel count) keeps this reliable across sheets of
    very different resolutions.
    """
    h, w = arr.shape[0], arr.shape[1]
    total = h * w
    flat = arr.reshape(-1, 3)
    colors, counts = np.unique(flat, axis=0, return_counts=True)
    order = np.argsort(-counts)[:top_n]

    bg_colors = []
    for idx in order:
        color = tuple(int(c) for c in colors[idx])
        comps = _exact_color_components(arr, color)
        if not comps:
            continue
        largest = max(c[4] for c in comps)
        if largest / total >= min_fill_fraction:
            bg_colors.append(color)

    corner = tuple(int(c) for c in arr[0, 0])
    if corner not in bg_colors:
        bg_colors.append(corner)
    return bg_colors


def key_out_background(arr, bg_colors, tolerance):
    """Return an alpha mask: 255 = keep (sprite), 0 = background (any bg color)."""
    h, w = arr.shape[0], arr.shape[1]
    mask = np.ones((h, w), dtype=bool)
    for bg_color in bg_colors:
        diff = np.abs(arr[:, :, :3].astype(int) - np.array(bg_color).astype(int))
        dist = diff.sum(axis=2)
        mask &= (dist > tolerance)
    return (mask.astype(np.uint8) * 255)


def find_components(mask):
    """
    Simple flood-fill connected-component labeling (4-connectivity) using
    only numpy + a manual stack, so we don't need scipy as a dependency.
    Returns a list of (y0,y1,x0,x1) tight bounding boxes, one per component.
    """
    h, w = mask.shape
    visited = np.zeros((h, w), dtype=bool)
    boxes = []

    ys, xs = np.nonzero(mask)
    nonzero_set = set(zip(ys.tolist(), xs.tolist()))

    for start in list(nonzero_set):
        sy, sx = start
        if visited[sy, sx]:
            continue
        # BFS/flood fill
        stack = [(sy, sx)]
        visited[sy, sx] = True
        min_y = max_y = sy
        min_x = max_x = sx
        comp_size = 0
        while stack:
            y, x = stack.pop()
            comp_size += 1
            if y < min_y: min_y = y
            if y > max_y: max_y = y
            if x < min_x: min_x = x
            if x > max_x: max_x = x
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                ny, nx = y + dy, x + dx
                if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not visited[ny, nx]:
                    visited[ny, nx] = True
                    stack.append((ny, nx))
        boxes.append((min_y, max_y, min_x, max_x, comp_size))
    return boxes


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sheet", help="Path to the sprite sheet PNG")
    ap.add_argument("--out", default="sprites_out", help="Output directory")
    ap.add_argument("--prefix", default="sprite", help="Filename prefix for output sprites")
    ap.add_argument("--tolerance", type=int, default=30,
                     help="Color-match tolerance for background keying (manhattan RGB distance, default 30)")
    ap.add_argument("--min-area", type=int, default=15,
                     help="Minimum pixel area to count as a real sprite, not noise (default 15)")
    ap.add_argument("--pad", type=int, default=0,
                     help="Optional transparent padding (px) to add around each tight crop (default 0)")
    ap.add_argument("--min-fill-fraction", type=float, default=0.03,
                     help="Min size of a color's largest single blob, as a FRACTION of the whole "
                          "sheet's area (0.03 = 3%%), to count it as a background fill rather than "
                          "a repeated sprite element like a body color or drop-shadow (default 0.03)")
    ap.add_argument("--exclude-box", action="append", default=[],
                     metavar="x0,y0,x1,y1",
                     help="Ignore everything inside this pixel region (e.g. a 'ripped by...' credit "
                          "watermark baked into the sheet). Repeatable for multiple regions.")
    args = ap.parse_args()

    exclude_boxes = []
    for box_str in args.exclude_box:
        x0, y0, x1, y1 = (int(v) for v in box_str.split(","))
        exclude_boxes.append((x0, y0, x1, y1))

    os.makedirs(args.out, exist_ok=True)

    im = Image.open(args.sheet).convert("RGB")
    arr = np.array(im)

    bg_colors = detect_background_colors(arr, min_fill_fraction=args.min_fill_fraction)
    print(f"Detected background color(s): {bg_colors}")

    mask = key_out_background(arr, bg_colors, args.tolerance)

    for (x0, y0, x1, y1) in exclude_boxes:
        mask[y0:y1 + 1, x0:x1 + 1] = 0

    boxes = find_components(mask)

    # Filter noise, sort in reading order (top-to-bottom, then left-to-right)
    boxes = [b for b in boxes if b[4] >= args.min_area]
    boxes.sort(key=lambda b: (b[0] // 20, b[2]))  # bucket rows loosely, then sort by x

    print(f"Found {len(boxes)} sprites (after filtering components < {args.min_area}px)")

    rgba = np.dstack([arr, mask])  # RGBA array, alpha=0 on background

    saved = []
    for i, (y0, y1, x0, x1, size) in enumerate(boxes):
        y0p = max(0, y0 - args.pad)
        y1p = min(arr.shape[0] - 1, y1 + args.pad)
        x0p = max(0, x0 - args.pad)
        x1p = min(arr.shape[1] - 1, x1 + args.pad)

        crop = rgba[y0p:y1p + 1, x0p:x1p + 1]
        out_im = Image.fromarray(crop, mode="RGBA")

        fname = f"{args.prefix}_{i:02d}.png"
        out_path = os.path.join(args.out, fname)
        out_im.save(out_path)
        saved.append((fname, out_im.size, size))
        print(f"  {fname}: {out_im.size[0]}x{out_im.size[1]} px  (area={size})")

    # Contact sheet preview: lay sprites out on a checkerboard so transparency is visible
    if saved:
        cell = max(max(w, h) for _, (w, h), _ in saved) + 4
        cols = min(8, len(saved))
        rows = (len(saved) + cols - 1) // cols
        preview = Image.new("RGBA", (cell * cols, cell * rows), (0, 0, 0, 0))
        # checkerboard background so alpha is visible
        checker = Image.new("RGBA", preview.size, (255, 255, 255, 255))
        cpx = checker.load()
        for yy in range(checker.size[1]):
            for xx in range(checker.size[0]):
                if ((xx // 8) + (yy // 8)) % 2 == 0:
                    cpx[xx, yy] = (200, 200, 200, 255)
        preview = Image.alpha_composite(checker, preview)

        for i, (fname, (w, h), _) in enumerate(saved):
            sprite = Image.open(os.path.join(args.out, fname))
            cx = (i % cols) * cell + (cell - w) // 2
            cy = (i // cols) * cell + (cell - h) // 2
            preview.paste(sprite, (cx, cy), sprite)
        preview_path = os.path.join(args.out, "_preview.png")
        preview.save(preview_path)
        print(f"\nPreview saved: {preview_path}")


if __name__ == "__main__":
    main()