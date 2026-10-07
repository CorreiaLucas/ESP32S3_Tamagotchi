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


def _find_sheet_in_folder(folder, prefix):
    """
    Locate the source sheet PNG inside `folder`. The PNG may be capitalized
    (`Kapurimon.png`) while the folder/prefix is lowercase, so match
    case-INSENSITIVELY and prefer an exact `<prefix>.png`. Skips the script's
    own `<prefix>_*.png` outputs and `_preview.png` so a re-run still finds the
    source. Raises SystemExit if none found.
    """
    if not os.path.isdir(folder):
        sys.exit(f"folder not found: {folder}")
    pngs = [f for f in os.listdir(folder) if f.lower().endswith(".png")]
    want = prefix.lower() + ".png"
    exact = [f for f in pngs if f.lower() == want]
    if exact:
        return os.path.join(folder, exact[0])
    candidates = [f for f in pngs
                  if not f.lower().startswith(prefix.lower() + "_")
                  and f.lower() != "_preview.png"]
    if not candidates:
        sys.exit(f"no source sheet PNG in {folder} (looked for {prefix}.png, any case)")
    print(f"note: using '{os.path.join(folder, candidates[0])}' (no exact {prefix}.png).")
    return os.path.join(folder, candidates[0])


def resolve_sheet_and_out(sheet, out, prefix, digimons_root="Assets/Digimons"):
    """
    Apply the project's convention so you can pass just --prefix:

      sheet (if omitted) -> <digimons_root>/<prefix>/<match>.png
      out   (if omitted) -> <digimons_root>/<prefix>   (derived sheet)
                            or the sheet's own folder   (explicit sheet)

    The folder is <prefix> as given (your folders are lowercase, e.g.
    "kapurimon"); the PNG inside may be capitalized ("Kapurimon.png"), matched
    case-insensitively. Returns (sheet_path, out_dir).
    """
    if sheet is None:
        folder = os.path.join(digimons_root, prefix)
        sheet = _find_sheet_in_folder(folder, prefix)
        if out is None:
            out = folder
    elif out is None:
        out = os.path.dirname(os.path.abspath(sheet)) or "."
    return sheet, out

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


def _alpha_mask(crop):
    """Boolean foreground mask (alpha > 0) for an RGBA crop array."""
    return crop[:, :, 3] > 0


def _mirror_similarity(a_crop, b_crop):
    """
    How close is `a_crop` to the HORIZONTAL MIRROR of `b_crop`, as an IoU of
    their alpha silhouettes (0..1). Both crops are resized onto a common grid
    first so differing tight-bbox sizes don't defeat the comparison. Used to
    (a) pair walk<->walkback (left/right facings) and (b) spot redundant mirror
    duplicates of a pose already kept.
    """
    am = _alpha_mask(a_crop)
    bm = _alpha_mask(b_crop)
    # Normalize both silhouettes to a 24x24 grid for a size-robust compare.
    def norm(m):
        im = Image.fromarray((m.astype(np.uint8) * 255), mode="L").resize((24, 24))
        return np.array(im) > 127
    an = norm(am)
    bn_mirror = norm(bm)[:, ::-1]   # flip the SECOND one horizontally
    inter = np.logical_and(an, bn_mirror).sum()
    union = np.logical_or(an, bn_mirror).sum()
    return (inter / union) if union else 0.0


def _squareness(w, h):
    """1.0 for a perfect square bbox, decreasing as it gets more oblong."""
    if w <= 0 or h <= 0:
        return 0.0
    return min(w, h) / max(w, h)


def _group_rows(blobs, row_tol_frac=0.6):
    """
    Cluster blobs into visual ROWS by vertical position. Two blobs share a row
    when their vertical centers are within `row_tol_frac` of the median blob
    height of each other. Returns a list of rows (each a list of blobs), ordered
    top-to-bottom, and each row's blobs ordered left-to-right.
    """
    if not blobs:
        return []
    heights = sorted(b["h"] for b in blobs)
    med_h = heights[len(heights) // 2] or 1
    tol = max(2, med_h * row_tol_frac)

    by_cy = sorted(blobs, key=lambda b: b["y0"] + b["h"] / 2.0)
    rows = []
    cur = [by_cy[0]]
    cur_cy = by_cy[0]["y0"] + by_cy[0]["h"] / 2.0
    for b in by_cy[1:]:
        cy = b["y0"] + b["h"] / 2.0
        if abs(cy - cur_cy) <= tol:
            cur.append(b)
            # running mean of the row's center keeps long rows stable
            cur_cy = sum(x["y0"] + x["h"] / 2.0 for x in cur) / len(cur)
        else:
            rows.append(cur)
            cur = [b]
            cur_cy = cy
    rows.append(cur)
    for r in rows:
        r.sort(key=lambda b: b["x0"])
    return rows


def _detect_sprite_band(rows, blobs, text_below_y=None, sprite_rows_n=None,
                        gap_factor=1.5):
    """
    Split rows into (sprite_rows, text_rows). These sheets stack sprite rows at
    the TOP, then a block of credit text (often shattered into many small
    per-word blobs) + a ripper avatar BELOW.

    Priority of cut strategies:
      1. EXPLICIT y cut (text_below_y): any row whose TOP is at/below this pixel
         is text. Deterministic -- use it when auto-detection misjudges.
      2. EXPLICIT row count (sprite_rows_n): keep exactly the top N rows.
      3. AUTO: cut at the first LARGE VERTICAL GAP between rows (> gap_factor x
         the median blob height). Sprite rows are stacked tightly; the credit
         block sits well below with a clear gap. We deliberately do NOT cut on a
         blob-size drop alone, because a legitimate second sprite row (e.g. the
         small map-sprite row under the big battle poses) is smaller than the
         first row and would be wrongly dropped.

    Returns (sprite_rows, text_rows).
    """
    if not rows:
        return [], []

    def row_bottom(r):
        return max(x["y0"] + x["h"] for x in r)

    def row_top(r):
        return min(x["y0"] for x in r)

    # 1. explicit pixel cut
    if text_below_y is not None:
        sprite, text = [], []
        for r in rows:
            (text if row_top(r) >= text_below_y else sprite).append(r)
        return sprite, text

    # 2. explicit row-count cut
    if sprite_rows_n is not None and sprite_rows_n > 0:
        return rows[:sprite_rows_n], rows[sprite_rows_n:]

    # 3. auto: first large vertical gap
    heights = sorted(b["h"] for b in blobs)
    med_h = heights[len(heights) // 2] or 1
    cut = len(rows)
    for i in range(1, len(rows)):
        gap = row_top(rows[i]) - row_bottom(rows[i - 1])
        if gap > med_h * gap_factor:
            cut = i
            break
    return rows[:cut], rows[cut:]


def _split_size_tiers(band):
    """
    Split band blobs into (large_tier, small_tier) by AREA. These DW sheets put
    a row of BIG battle poses on top and a row of SMALL map sprites below; the
    two form two clear area clusters. We sort by area and cut at the largest
    relative gap in the upper half (so a handful of big poses separate cleanly
    from the many small frames). If there's no strong gap (one uniform tier),
    everything is "small" (walk/walkback/happy) and large is empty.
    """
    if not band:
        return [], []
    by_area = sorted(band, key=lambda b: b["area"])
    areas = [b["area"] for b in by_area]
    n = len(areas)
    # Find the biggest ratio jump between consecutive areas, searched in the
    # upper 60% (the big-pose boundary sits well above the median).
    best_ratio, best_i = 1.0, None
    for i in range(max(1, int(n * 0.4)), n):
        prev = areas[i - 1] or 1
        ratio = areas[i] / prev
        if ratio > best_ratio:
            best_ratio, best_i = ratio, i
    # Require a real jump (big poses are ~2x+ the map sprites) to call it a
    # two-tier sheet; otherwise treat all as the small tier.
    if best_i is not None and best_ratio >= 1.8:
        small = by_area[:best_i]
        large = by_area[best_i:]
    else:
        small, large = by_area, []
    # Return each tier in READING order (row-major), not area order.
    large.sort(key=lambda b: (b["y0"], b["x0"]))
    small.sort(key=lambda b: (b["y0"], b["x0"]))
    return large, small


def auto_name_sprites(blobs, prefix, mirror_thresh=0.72, drop_text=True,
                      counts=None, order=("walkback", "walk", "happy"),
                      want_profile=True, pair_mirrors=True,
                      text_below_y=None, sprite_rows_n=None):
    """
    TWO-TIER, ROW-AWARE naming for Digimon World sheets (deterministic).

    Validated against the finished sets (InTraining/*, ToGenerate/*). These
    sheets are laid out as:
        row of BIG battle poses   (top)      -> attack1..N  (ALL of them)
        row(s) of SMALL map frames (below)   -> walk/walkback/happy + profile
        credit text + ripper avatar (bottom) -> dropped (_To_Remove_)

    Steps:
      1. Group blobs into rows; drop everything BELOW the top sprite band
         (text/credit) by POSITION -- robust even when text fragments into many
         tiny per-word blobs.
      2. Split the band into a LARGE tier and a SMALL tier by an area gap.
         Large tier -> attack1..N in reading order (every big pose; or capped at
         counts["attack"] if a positive value is passed).
      3. SMALL tier: profile = most-square frame; then walk/walkback as MIRROR
         PAIRS (one facing each), then happy, by the counts passed. Leftover
         small frames (extra map directions) -> _To_Remove_.

    `counts` keys: walk, walkback, happy (default 3/3/3), and optional attack
    (0 = take ALL large-tier poses; >0 = cap). Returns (blob, name, reason) in
    input order.
    """
    if not blobs:
        return []
    counts = dict(counts or {})
    counts.setdefault("walk", 3)
    counts.setdefault("walkback", 3)
    counts.setdefault("happy", 3)

    assignments = {}   # blob idx -> (name, reason)

    # ---- 1. rows -> sprite band vs text band (position-based text drop) ----
    rows = _group_rows(blobs)
    if drop_text:
        sprite_rows, text_rows = _detect_sprite_band(
            rows, blobs, text_below_y=text_below_y, sprite_rows_n=sprite_rows_n)
    else:
        sprite_rows, text_rows = rows, []

    for r in text_rows:
        for b in r:
            assignments[b["idx"]] = ("To_Remove", "below sprite band (text/credit)")

    band = [b for r in sprite_rows for b in r]

    # ---- 2. size tiers: large = attack poses, small = walk/walkback/happy --
    large, small = _split_size_tiers(band)

    attack_cap = int(counts.get("attack", 0))
    for i, b in enumerate(large, start=1):
        if attack_cap and i > attack_cap:
            assignments[b["idx"]] = ("To_Remove", "extra large pose beyond --attack cap")
        else:
            assignments[b["idx"]] = (f"attack{i}", "large top-row pose")

    # ---- 3a. profile: most-square frame in the SMALL tier ------------------
    small_unassigned = [b for b in small if b["idx"] not in assignments]
    if want_profile and small_unassigned:
        profile_blob = max(small_unassigned,
                           key=lambda b: (_squareness(b["w"], b["h"]), b["x0"]))
        assignments[profile_blob["idx"]] = ("profile", "most-square small frame")

    # ---- 3b. SMALL tier in READING ORDER (left-to-right, top-to-bottom) -----
    # The small map-sprite row is already laid out in a fixed order, so we just
    # consume it sequentially per `order` (default walkback,walk,happy) rather
    # than trying to mirror-match (which mis-slotted some frames). `small` is
    # already in reading order from _split_size_tiers.
    for action in order:
        if action == "attack":
            continue   # attack comes from the LARGE tier, never small frames
        want = int(counts.get(action, 0))
        if want <= 0:
            continue
        taken = 0
        for c in small:
            if taken >= want:
                break
            if c["idx"] in assignments:
                continue
            taken += 1
            assignments[c["idx"]] = (f"{action}{taken}", f"{action} (small, reading order)")

    # ---- leftover SMALL frames (extra map directions) -> flag --------------
    for b in band:
        if b["idx"] not in assignments:
            assignments[b["idx"]] = ("To_Remove", "extra small/map frame")

    # ---- build result + sequential To_Remove numbering ---------------------
    result = []
    remove_i = 0
    for b in blobs:
        name, reason = assignments.get(b["idx"], ("To_Remove", "unclassified"))
        if name == "To_Remove":
            remove_i += 1
            name = f"To_Remove_{remove_i:02d}"
        result.append((b, f"{prefix}_{name}", reason))
    return result


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
    ap.add_argument("sheet", nargs="?", default=None,
                     help="Path to the sprite sheet PNG. Optional: if omitted, it is derived from "
                          "--prefix as Assets/Digimons/<prefix>/<Prefix>.png (case-insensitive "
                          "match on the file inside that folder).")
    ap.add_argument("--out", default=None,
                     help="Output directory (default: Assets/Digimons/<prefix> when --prefix is "
                          "given and --sheet is derived, else the sheet's own folder).")
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
    ap.add_argument("--auto-name", action="store_true",
                     help="Classify each split sprite and name it by ACTION instead of a bare "
                          "index. ROW-AWARE + COUNT-DRIVEN: the top sprite band is kept and named "
                          "in reading order using the counts you pass (--walk/--walkback/--happy/"
                          "--attack/--profile); everything BELOW the band (credit text, avatar) is "
                          "renamed <prefix>_To_Remove_NN. Band blobs beyond the requested counts "
                          "are also flagged _To_Remove_ for a quick manual review.")
    ap.add_argument("--keep-text", action="store_true",
                     help="Do NOT drop the below-band text/credit region. By default (with "
                          "--auto-name) everything under the detected top sprite band is renamed "
                          "<prefix>_To_Remove_NN so baked-in credits/labels don't become sprites.")
    # ---- Count-driven naming (used with --auto-name) ----
    ap.add_argument("--walk", type=int, default=3,
                     help="[--auto-name] How many WALK frames to name, in reading order (default 3).")
    ap.add_argument("--walkback", type=int, default=3,
                     help="[--auto-name] How many WALKBACK frames (paired to walk by mirror when "
                          "possible, else reading order) (default 3).")
    ap.add_argument("--happy", type=int, default=3,
                     help="[--auto-name] How many HAPPY frames to name (default 3).")
    ap.add_argument("--attack", type=int, default=0,
                     help="[--auto-name] Cap on ATTACK frames (the big top-row poses). 0 (default) "
                          "= name ALL of them attack1..N; a positive value caps it and flags the "
                          "rest _To_Remove_.")
    ap.add_argument("--profile", type=int, default=1,
                     help="[--auto-name] 1 to name one profile sprite (most-square band blob), "
                          "0 to skip (default 1).")
    ap.add_argument("--order", default="walkback,walk,happy",
                     help="[--auto-name] Order the SMALL-tier frames are consumed left-to-right "
                          "(default walkback,walk,happy -- matches the DW map-sprite row). attack "
                          "always comes from the large top row and is ignored here.")
    ap.add_argument("--text-below", type=int, default=None, metavar="Y",
                     help="[--auto-name] Force every blob whose TOP is at/below pixel row Y to "
                          "<prefix>_To_Remove_NN. Deterministic override for when the automatic "
                          "sprite-band detection misjudges where the credit text begins.")
    ap.add_argument("--sprite-rows", type=int, default=None, metavar="N",
                     help="[--auto-name] Keep exactly the top N visual rows as sprites; everything "
                          "below goes to _To_Remove_. Use when the sheet has a known number of "
                          "sprite rows (e.g. --sprite-rows 2).")
    ap.add_argument("--batch", default=None, metavar="DIR",
                     help="Batch mode: treat DIR as a parent folder and run on EVERY subfolder. "
                          "Each subfolder's name (lowercased) becomes the --prefix, its "
                          "<Name>.png the sheet, and the subfolder itself the output dir. "
                          "All other flags (--auto-name, --walk, ...) apply to every subfolder. "
                          "Ignores the positional sheet / --prefix / --out when set.")
    ap.add_argument("--clean", action="store_true",
                     help="Before writing, delete existing <prefix>_*.png and _preview.png in the "
                          "output folder so a re-run starts fresh (removes stale output from a "
                          "previous run, e.g. old _To_Remove_ files). Never touches the source "
                          "sheet or other files.")
    args = ap.parse_args()

    exclude_boxes = []
    for box_str in args.exclude_box:
        x0, y0, x1, y1 = (int(v) for v in box_str.split(","))
        exclude_boxes.append((x0, y0, x1, y1))

    # ---- Batch mode: run on every subfolder of --batch ----------------------
    if args.batch:
        if not os.path.isdir(args.batch):
            sys.exit(f"error: --batch folder not found: {args.batch}")
        subs = sorted(d for d in os.listdir(args.batch)
                      if os.path.isdir(os.path.join(args.batch, d)))
        if not subs:
            sys.exit(f"error: no subfolders in {args.batch}")
        print(f"Batch: {len(subs)} subfolder(s) in {args.batch}\n")
        ok = fail = 0
        for name in subs:
            folder = os.path.join(args.batch, name)
            prefix = name.lower()           # folder name -> prefix (your convention)
            print("=" * 64)
            print(f"[{name}] prefix='{prefix}'")
            try:
                sheet = _find_sheet_in_folder(folder, prefix)
                process_sheet(sheet, folder, prefix, args, exclude_boxes)
                ok += 1
            except SystemExit as e:
                # _find_sheet_in_folder uses sys.exit on a missing sheet; in
                # batch we DON'T abort the whole run -- skip and continue.
                print(f"  SKIP {name}: {e}")
                fail += 1
            except Exception as e:
                print(f"  ERROR {name}: {e}")
                fail += 1
        print("=" * 64)
        print(f"Batch done: {ok} processed, {fail} skipped/failed.")
        return

    # ---- Single-sheet mode --------------------------------------------------
    # Apply the Assets/Digimons/<prefix> convention when sheet/out are omitted.
    sheet, out = resolve_sheet_and_out(args.sheet, args.out, args.prefix)
    print(f"Sheet: {sheet}\nOut:   {out}")
    process_sheet(sheet, out, args.prefix, args, exclude_boxes)


def process_sheet(sheet, out, prefix, args, exclude_boxes):
    """Split + (optionally) auto-name ONE sheet into `out`. Shared by single
    and batch modes."""
    os.makedirs(out, exist_ok=True)

    # Optional: wipe prior output (<prefix>_*.png + _preview.png) so a re-run
    # starts fresh. Never removes the source sheet or unrelated files.
    if args.clean:
        sheet_base = os.path.basename(sheet).lower()
        removed = 0
        for f in os.listdir(out):
            low = f.lower()
            if low == sheet_base:
                continue   # never delete the source sheet
            if low == "_preview.png" or low.startswith(prefix.lower() + "_"):
                try:
                    os.remove(os.path.join(out, f))
                    removed += 1
                except OSError:
                    pass
        if removed:
            print(f"  --clean: removed {removed} stale output file(s).")

    im = Image.open(sheet).convert("RGB")
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

    # First pass: crop every blob into memory (so --auto-name can compare their
    # silhouettes for mirror detection before deciding filenames).
    blobs = []
    for i, (y0, y1, x0, x1, size) in enumerate(boxes):
        y0p = max(0, y0 - args.pad)
        y1p = min(arr.shape[0] - 1, y1 + args.pad)
        x0p = max(0, x0 - args.pad)
        x1p = min(arr.shape[1] - 1, x1 + args.pad)
        crop = rgba[y0p:y1p + 1, x0p:x1p + 1]
        blobs.append({
            "idx": i, "crop": crop,
            "w": crop.shape[1], "h": crop.shape[0],
            "area": int(size), "y0": y0, "x0": x0,
        })

    # Decide a filename (sans extension) for each blob.
    drop_text = not args.keep_text
    if args.auto_name:
        counts = {"walk": args.walk, "walkback": args.walkback,
                  "happy": args.happy, "attack": args.attack}
        order = tuple(s.strip() for s in args.order.split(",") if s.strip())
        named = auto_name_sprites(blobs, prefix, drop_text=drop_text,
                                  counts=counts, order=order,
                                  want_profile=(args.profile > 0),
                                  text_below_y=args.text_below,
                                  sprite_rows_n=args.sprite_rows)
        print("\nAuto-name classification (review the _To_Remove_ ones):")
        print(f"  {'idx':>3}  {'y':>5}  {'area':>7}  {'w x h':>9}  {'name':<22} reason")
    else:
        # Plain indexed names (no classification). Use --auto-name for action
        # names + below-band text dropping.
        named = [(b, f"{prefix}_{b['idx']:02d}", "") for b in blobs]

    saved = []
    for (b, name, reason) in named:
        out_im = Image.fromarray(b["crop"], mode="RGBA")
        fname = f"{name}.png"
        out_path = os.path.join(out, fname)
        out_im.save(out_path)
        saved.append((fname, out_im.size, b["area"]))
        if args.auto_name:
            print(f"  {b['idx']:>3}  y={b['y0']:>3}  {b['area']:>7}  {b['w']:>3} x {b['h']:<3}  "
                  f"{name:<22} {reason}")
        else:
            print(f"  {fname}: {out_im.size[0]}x{out_im.size[1]} px  (area={b['area']})")

    if args.auto_name:
        removable = [f for f, _, _ in saved if "_To_Remove_" in f]
        print(f"\n{len(saved)} sprites named; {len(removable)} flagged _To_Remove_ "
              f"(delete after a visual check).")
        if removable and args.text_below is None and args.sprite_rows is None:
            print("  If REAL sprites were dropped as 'below sprite band', the auto band "
                  "cut was wrong -- rerun with --sprite-rows N (keep top N rows) or "
                  "--text-below Y (drop everything under pixel row Y). The 'w x h' and "
                  "'y' of each blob above help you pick Y.")

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
            sprite = Image.open(os.path.join(out, fname))
            cx = (i % cols) * cell + (cell - w) // 2
            cy = (i // cols) * cell + (cell - h) // 2
            preview.paste(sprite, (cx, cy), sprite)
        preview_path = os.path.join(out, "_preview.png")
        preview.save(preview_path)
        print(f"\nPreview saved: {preview_path}")


if __name__ == "__main__":
    main()