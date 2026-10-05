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


def auto_name_sprites(blobs, prefix, mirror_thresh=0.72, drop_text=True,
                      counts=None, order=("walk", "walkback", "happy", "attack"),
                      want_profile=True, pair_mirrors=True,
                      text_below_y=None, sprite_rows_n=None):
    """
    ROW-AWARE, COUNT-DRIVEN naming (deterministic -- no size guessing).

    These ripped Digimon World sheets vary too much for a size-based "this blob
    is attack vs walk" guess to be reliable (one sheet has a few big attack
    frames + tidy walk rows; another has a dozen mirrored poses + the credit
    text shattered into ~40 tiny word blobs). So instead:

      1. Group blobs into visual ROWS, detect the TOP sprite band, and send
         everything BELOW it (credit text + ripper avatar) to _To_Remove_ by
         POSITION -- robust even when text fragments into many small blobs.
      2. Within the sprite band, take blobs in reading order and assign names
         using the COUNTS the caller passes (e.g. walk=3, walkback=3, happy=3,
         attack=2), in `order`. You stay in control of how many of each.
      3. `profile` = the single most-square small blob in the band (if
         want_profile). Leftover band blobs beyond the requested counts are
         flagged _To_Remove_ (likely redundant mirror facings / map sprites).

    `counts` is a dict like {"walk":3,"walkback":3,"happy":3,"attack":2}. A
    missing/zero action is simply skipped. `blobs` dicts carry
    {idx, crop, w, h, area, y0, x0}. Returns (blob, name, reason) in input order.
    """
    if not blobs:
        return []
    counts = dict(counts or {})

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

    band = [b for r in sprite_rows for b in r]   # reading order (row-major)

    # ---- 2. profile: most-square blob in the band (prefer smaller/rightmost) ----
    if want_profile and band:
        heights = sorted(b["h"] for b in band)
        med_h = heights[len(heights) // 2] or 1
        small = [b for b in band if b["h"] <= med_h * 1.2] or band
        profile_blob = max(small, key=lambda b: (_squareness(b["w"], b["h"]), b["x0"]))
        assignments[profile_blob["idx"]] = ("profile", "most-square band blob")

    remaining = [b for b in band if b["idx"] not in assignments]

    # ---- 3. assign the requested counts ------------------------------------
    # KEY INSIGHT for these sheets: the band is usually made of MIRROR PAIRS
    # (the same pose facing left and right), often laid out adjacently. "walk"
    # and "walkback" are precisely those two facings. So when both are wanted
    # and pair_mirrors is on, we PAIR FIRST -- group the band into mirror pairs
    # and split each pair across walk{i}/walkback{i} -- instead of taking the
    # first N band blobs as walk (which would grab BOTH halves of a pair as
    # walk1/walk2 and strand their mirrors).
    want_walk = int(counts.get("walk", 0)) if pair_mirrors else 0
    want_back = int(counts.get("walkback", 0)) if pair_mirrors else 0
    npairs = min(want_walk, want_back)
    if npairs > 0:
        used = set()
        made = 0
        for b in remaining:
            if made >= npairs or b["idx"] in used or b["idx"] in assignments:
                continue
            best, best_sim = None, mirror_thresh
            for c in remaining:
                if c["idx"] == b["idx"] or c["idx"] in used or c["idx"] in assignments:
                    continue
                sim = _mirror_similarity(b["crop"], c["crop"])
                if sim >= best_sim:
                    best_sim, best = sim, c
            if best is not None:
                made += 1
                used.add(b["idx"]); used.add(best["idx"])
                assignments[b["idx"]]   = (f"walk{made}", "mirror pair (facing A)")
                assignments[best["idx"]] = (f"walkback{made}", "mirror pair (facing B)")
        # Decrement the pair-satisfied counts; any shortfall is filled in reading
        # order by the generic pass below (so single-facing art still works).
        counts = dict(counts)
        counts["walk"] = max(0, want_walk - made)
        counts["walkback"] = max(0, want_back - made)
        # Keep walk/walkback numbering continuing after the pairs we made.
        start_at = {"walk": made + 1, "walkback": made + 1}
    else:
        start_at = {}

    # Generic reading-order fill for every action (and any walk/walkback left
    # over after pairing). Numbering continues from start_at where set.
    remaining = [b for b in band if b["idx"] not in assignments]
    counters = dict(start_at)
    for action in order:
        want = int(counts.get(action, 0))
        if want <= 0:
            continue
        taken = 0
        nxt = counters.get(action, 1)
        for c in remaining:
            if taken >= want:
                break
            if c["idx"] in assignments:
                continue
            taken += 1
            assignments[c["idx"]] = (f"{action}{nxt}", f"{action} (reading order)")
            nxt += 1
        counters[action] = nxt

    # ---- leftover band blobs beyond requested counts -> flag ----------------
    for b in band:
        if b["idx"] not in assignments:
            assignments[b["idx"]] = ("To_Remove", "band blob beyond requested counts")

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
                     help="[--auto-name] How many ATTACK frames to name (default 0 -- set it per "
                          "sheet, e.g. --attack 2).")
    ap.add_argument("--profile", type=int, default=1,
                     help="[--auto-name] 1 to name one profile sprite (most-square band blob), "
                          "0 to skip (default 1).")
    ap.add_argument("--order", default="walk,walkback,happy,attack",
                     help="[--auto-name] Comma-separated order the counts are consumed from the "
                          "band in reading order (default walk,walkback,happy,attack).")
    ap.add_argument("--mirror-thresh", type=float, default=0.72,
                     help="[--auto-name] Min silhouette-IoU (0..1) to pair a WALK frame with its "
                          "left/right MIRROR as the matching WALKBACK (default 0.72). Lower = pair "
                          "more loosely; raise = require closer mirrors.")
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
                                  mirror_thresh=args.mirror_thresh,
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