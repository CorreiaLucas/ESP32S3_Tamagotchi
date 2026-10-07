#!/usr/bin/env python3
"""
build_digimon_sprites.py - Automate PNG -> RGB565 sprite generation for a whole
Digimon folder, ready for digivolution (each Digimon can swap in its full set
of sprites at runtime).

SIZING MODEL: one uniform square size PER DIGIMON, chosen automatically.
  A Tamagotchi character must keep a consistent on-screen size across all its
  actions (walking, attacking, ...), otherwise it appears to grow/shrink. So
  the tool derives ONE square size for the whole Digimon from its own art:
  the largest side of its NORMAL poses (walk, walkback, happy, sleep, ...;
  attack and profile excluded) plus the --pad margin, clamped to
  [--min-size, --max-size]. Frames are drawn at native 1:1 (--scale 1); bigger
  frames such as attack poses are shrunk to fit the same square. The profile
  uses its own native size with no padding. --size / --profile-size force
  exact values instead.

Given a Digimon folder under Assets/Digimons/<name>/, this:
  1. Scans every PNG, strips the Digimon-name prefix and splits each filename
     into an ACTION base (walk, walkback, happy, attack, sleep, profile, ...)
     plus an optional trailing frame NUMBER. Handles both project naming styles:
       terriermon_walk1.png -> base "walk", frame 1
       walk_1.png           -> base "walk", frame 1
       terriermon_sleep.png -> base "sleep", no frame
  2. Groups same-base frames into an animation, renumbered 0-based to match the
     firmware convention (walk1/2/3 -> walk_0/walk_1/walk_2).
  3. Converts each PNG to an RGB565 PROGMEM array (reusing png_to_rgb565.py) with
     PREFIXED symbols so Digimon coexist: terriermon_walk_0[], gargomon_walk_0[]
  4. Emits animation frame tables for multi-frame actions:
       const uint16_t* const terriermon_walk_frames[3] = {
         terriermon_walk_0, terriermon_walk_1, terriermon_walk_2 };
  5. Emits size #defines so the firmware/registry knows how to draw it:
       #define TERRIERMON_SPRITE_SIZE   48   // uniform action-sprite square
       #define TERRIERMON_PROFILE_SIZE  30
  6. Writes <name>Sprites.cpp and <name>Sprites.h into src/.

USAGE
-----
  # One Digimon folder (size picked automatically):
  python tools/build_digimon_sprites.py Assets/Digimons/ToGenerate/Agumon

  # Every Digimon folder below a folder, at any depth:
  python tools/build_digimon_sprites.py Assets/Digimons/ToGenerate
  python tools/build_digimon_sprites.py Assets/Digimons

  # Bare name, looked up under --assets (old style):
  python tools/build_digimon_sprites.py terriermon

  # Force exact sizes:
  python tools/build_digimon_sprites.py terriermon --size 48 --profile-size 30

Notes:
  * Unrecognized files (empty base, source sheets like "gargomon.png" or
    "DS _ DSi ... Terriermon.png") are SKIPPED and listed at the end.
  * Symbols are prefixed with the sanitized Digimon name -> no collisions.
"""

import argparse
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    from png_to_rgb565 import convert_sprite, format_array
except ImportError as e:  # pragma: no cover
    sys.exit(f"Could not import png_to_rgb565.py (expected next to this script): {e}")

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required:  pip install pillow")


# RGB565 packing (kept local so the padded converter is self-contained).
def _to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def convert_sprite_padded(path, size, pad):
    """Fit+center the art into a `size` square, but only fill (1-2*pad) of it so
    there is a transparent SAFETY MARGIN on every side. Prevents ears/feet from
    touching (and getting cropped at) the canvas / screen edge. pad is a
    fraction per side, e.g. 0.12 -> ~12% transparent border all around.
    Alpha<128 -> transparent (0x0000); opaque black nudged to avoid transparency."""
    from PIL import Image
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    inner = max(1, int(round(size * (1.0 - 2.0 * pad))))
    scale = min(inner / w, inner / h)
    nw, nh = max(1, round(w * scale)), max(1, round(h * scale))
    im = im.resize((nw, nh), Image.NEAREST)
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.paste(im, ((size - nw) // 2, (size - nh) // 2))
    px = canvas.load()
    vals = []
    for y in range(size):
        for x in range(size):
            r, g, b, a = px[x, y]
            if a < 128:
                vals.append(0x0000)
            else:
                v = _to_rgb565(r, g, b)
                vals.append(0x0821 if v == 0x0000 else v)
    return vals


def convert_sprite_integer(path, size, max_scale, pad):
    """Upscale the source art by the largest INTEGER factor (2, 3, ...) that
    still fits inside the padded canvas, so every source pixel becomes an exact
    NxN block -> perfectly EVEN pixels (no fractional-scale artifacts like
    mismatched eyes). The result is centered in a `size` square with a
    transparent margin. If the art is already larger than the padded inner box
    even at 1x (e.g. a wide 'attack' pose), it is fit-scaled down to fit that
    single frame (can't integer-upscale something too big) -- normal frames
    stay crisp integer multiples.
    max_scale caps the integer factor (e.g. 2 for a 2x look).
    Alpha<128 -> transparent (0x0000); opaque black nudged away from 0x0000."""
    from PIL import Image
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    inner = max(1, int(round(size * (1.0 - 2.0 * pad))))

    if w <= inner and h <= inner:
        # Largest integer factor that keeps both dimensions within `inner`.
        factor = max(1, min(max_scale, inner // w, inner // h))
        nw, nh = w * factor, h * factor
        im = im.resize((nw, nh), Image.NEAREST)   # exact NxN blocks -> even
    else:
        # Oversized frame: fit-scale down (integer upscale impossible here).
        scale = min(inner / w, inner / h)
        nw, nh = max(1, round(w * scale)), max(1, round(h * scale))
        im = im.resize((nw, nh), Image.NEAREST)

    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.paste(im, ((size - nw) // 2, (size - nh) // 2))
    px = canvas.load()
    vals = []
    for y in range(size):
        for x in range(size):
            r, g, b, a = px[x, y]
            if a < 128:
                vals.append(0x0000)
            else:
                v = _to_rgb565(r, g, b)
                vals.append(0x0821 if v == 0x0000 else v)
    return vals


def convert_sprite_rect(path, box_w, box_h, max_scale, pad):
    """Like convert_sprite_integer, but into a RECTANGULAR box_w x box_h canvas.
    Used for attack frames, whose art is much larger (and usually wider) than
    the normal poses, so squeezing it into one square either wasted memory or
    shrank it hard.

    Scaling rules, in order:
      * fits at 1:1  -> largest INTEGER upscale (<= max_scale) that still fits,
                        so pixels stay exact NxN blocks (NEAREST).
      * too big      -> downscale with BOX (area-average) rather than NEAREST.
                        At a fractional factor NEAREST drops whole rows/columns
                        of pixels (eyes/limbs vanish); BOX keeps the shape.
    """
    from PIL import Image
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    inner_w = max(1, int(round(box_w * (1.0 - 2.0 * pad))))
    inner_h = max(1, int(round(box_h * (1.0 - 2.0 * pad))))

    if w <= inner_w and h <= inner_h:
        factor = max(1, min(max_scale, inner_w // w, inner_h // h))
        nw, nh = w * factor, h * factor
        im = im.resize((nw, nh), Image.NEAREST)
    else:
        scale = min(inner_w / w, inner_h / h)
        nw, nh = max(1, round(w * scale)), max(1, round(h * scale))
        im = im.resize((nw, nh), Image.BOX)

    canvas = Image.new("RGBA", (box_w, box_h), (0, 0, 0, 0))
    canvas.paste(im, ((box_w - nw) // 2, (box_h - nh) // 2))
    px = canvas.load()
    vals = []
    for y in range(box_h):
        for x in range(box_w):
            r, g, b, a = px[x, y]
            if a < 128:
                vals.append(0x0000)
            else:
                v = _to_rgb565(r, g, b)
                vals.append(0x0821 if v == 0x0000 else v)
    return vals


KNOWN_ACTIONS = {
    "walk", "walkback", "happy", "attack", "sleep", "profile",
    "eat", "play", "sad", "dead", "idle", "sick", "sleepback",
}


def sanitize(name: str) -> str:
    return re.sub(r"_+", "_", re.sub(r"[^a-z0-9]+", "_", name.lower())).strip("_")


def parse_name(fname: str, digimon: str):
    stem = os.path.splitext(fname)[0]
    low = stem.lower()
    if low.startswith(digimon.lower()):
        low = low[len(digimon):]
    low = low.strip("_- ")
    m = re.match(r"^(.*?)[ _-]*(\d+)$", low)
    if m:
        base, idx = m.group(1), int(m.group(2))
    else:
        base, idx = low, None
    return sanitize(base), idx


def collect(folder: str, digimon: str):
    """Return (actions dict, skipped list). actions[base] = [(idx, path), ...]."""
    actions: dict[str, list] = {}
    skipped: list[str] = []
    for fname in sorted(os.listdir(folder)):
        if not fname.lower().endswith(".png"):
            continue
        base, idx = parse_name(fname, digimon)
        if base not in KNOWN_ACTIONS:
            skipped.append(fname)
            continue
        actions.setdefault(base, []).append((idx, os.path.join(folder, fname)))
    return actions, skipped


def derive_size(actions: dict, min_size: int, max_size: int, pad: float) -> int:
    """One uniform square size for the whole Digimon, chosen so its NORMAL poses
    (walk, walkback, happy, sleep, ...) draw at native 1:1 size with a `pad`
    margin on each side. Attack poses (much bigger battle art on these sheets)
    and the profile are excluded, otherwise they would blow up the size of
    every other frame. Falls back to all non-profile frames when a Digimon only
    ships attack art. Result is clamped to [min_size, max_size]."""
    def largest_side(skip):
        largest = 0
        for base, entries in actions.items():
            if base in skip:
                continue
            for _, path in entries:
                w, h = Image.open(path).size
                largest = max(largest, w, h)
        return largest

    largest = largest_side({"profile", "attack"}) or largest_side({"profile"})
    if largest == 0:                     # only a profile? fall back to min.
        return min_size
    fill = max(0.05, 1.0 - 2.0 * pad)   # fraction of the square the art fills
    size = math.ceil(largest / fill)
    return max(min_size, min(max_size, size))


def derive_profile_size(actions: dict, fallback: int) -> int:
    """Profile square = the profile image's own largest side (native 1:1)."""
    entries = actions.get("profile")
    if not entries:
        return fallback
    w, h = Image.open(entries[0][1]).size
    return max(w, h)


def derive_attack_box(actions: dict, attack_max: int) -> tuple:
    """(w, h) box for the ATTACK frames, at the art's own aspect ratio.

    Attack art on these sheets is ~3x the normal poses (e.g. 62x73 vs 22x26).
    It still has to fit the combat layout (the player slot is roughly 50px
    tall, between the enemy header and the command panel), so the box is the
    attack art's bounding box scaled down to `attack_max` on its longer side --
    never up. That is far better than squeezing it into the walk-sized square:
    for Agumon it goes from ~37% scale to ~66%.
    Returns (0, 0) when the Digimon ships no attack art."""
    entries = actions.get("attack")
    if not entries or attack_max <= 0:
        return (0, 0)
    mw = mh = 0
    for _, path in entries:
        w, h = Image.open(path).size
        mw, mh = max(mw, w), max(mh, h)
    if mw == 0 or mh == 0:
        return (0, 0)
    longest = max(mw, mh)
    scale = min(1.0, attack_max / longest)
    return (max(1, int(math.ceil(mw * scale))),
            max(1, int(math.ceil(mh * scale))))


def is_digimon_folder(folder: str) -> bool:
    """True if `folder` directly contains at least one recognized sprite PNG."""
    actions, _ = collect(folder, os.path.basename(os.path.normpath(folder)))
    return bool(actions)


def find_digimon_folders(root: str) -> list:
    """`root` and every folder below it that holds recognized sprite PNGs.
    Each one is treated as one Digimon named after the folder."""
    found = []
    for dirpath, dirnames, _ in os.walk(root):
        dirnames.sort()
        if is_digimon_folder(dirpath):
            found.append(dirpath)
    return found


def build(folder: str, out_dir: str,
          size: int | None, profile_size: int | None,
          min_size: int, max_size: int, pad: float,
          integer_scale: int = 1, register: bool = True,
          stats=None, attack_max: int = 48) -> None:
    if not os.path.isdir(folder):
        sys.exit(f"Folder not found: {folder}")
    digimon = os.path.basename(os.path.normpath(folder))

    prefix = sanitize(digimon)
    actions, skipped = collect(folder, digimon)
    if not actions:
        sys.exit(f"No recognized sprite files in {folder}. "
                 f"Recognized actions: {sorted(KNOWN_ACTIONS)}")

    # One uniform action-sprite size for this Digimon.
    if size is None:
        size = derive_size(actions, min_size, max_size, pad)
        size_note = f"auto (normal poses + pad, clamped to [{min_size},{max_size}])"
    else:
        size_note = "forced via --size"

    # Profile: auto = native size, no padding (it's a portrait, not a pose).
    if profile_size is None:
        profile_size = derive_profile_size(actions, 30)
        profile_pad, profile_note = 0.0, "auto (native)"
    else:
        profile_pad, profile_note = pad, "forced via --profile-size"

    # Attack frames get their own box (see derive_attack_box).
    attack_w, attack_h = derive_attack_box(actions, attack_max)

    cpp_blocks: list[str] = []
    h_externs: list[str] = []
    h_tables: list[str] = []
    summary: list[str] = []
    has_profile = "profile" in actions
    count_by_base: dict[str, int] = {}   # base -> number of frames (for registry)

    for base in sorted(actions):
        entries = actions[base]
        entries.sort(key=lambda e: (e[0] is None, e[0] if e[0] is not None else 0))
        is_attack = (base == "attack" and attack_w > 0)
        this_size = profile_size if base == "profile" else size
        this_pad = profile_pad if base == "profile" else pad

        symbols = []
        for new_idx, (_, path) in enumerate(entries):
            if len(entries) == 1:
                sym = f"{prefix}_{base}"     # lone frame (sleep.png or sleep1.png)
            else:
                sym = f"{prefix}_{base}_{new_idx}"
            if is_attack:
                vals = convert_sprite_rect(path, attack_w, attack_h,
                                           max(1, integer_scale), pad)
                row_w, dims = attack_w, f"{attack_w}x{attack_h}"
            elif integer_scale > 0:
                vals = convert_sprite_integer(path, this_size, integer_scale, this_pad)
                row_w, dims = this_size, f"{this_size}x{this_size}"
            else:
                vals = convert_sprite_padded(path, this_size, this_pad)
                row_w, dims = this_size, f"{this_size}x{this_size}"
            comment = (f"// {sym}: {dims} RGB565 sprite of "
                       f"{os.path.basename(path)}")
            cpp_blocks.append(comment + "\n" + format_array(sym, vals, per_row=row_w))
            h_externs.append(f"extern const uint16_t {sym}[] PROGMEM;")
            symbols.append(sym)

        count_by_base[base] = len(symbols)

        if len(symbols) > 1:
            table = f"{prefix}_{base}_frames"
            cpp_blocks.append(
                f"const uint16_t* const {table}[{len(symbols)}] = {{ {', '.join(symbols)} }};")
            h_tables.append(f"extern const uint16_t* const {table}[{len(symbols)}];")
            summary.append(f"{base}: {len(symbols)} frames -> {table}[{len(symbols)}]")
        else:
            summary.append(f"{base}: 1 frame -> {symbols[0]}")

    guard = f"{prefix.upper()}_SPRITES_H"
    cpp_name = f"{prefix}Sprites.cpp"
    h_name = f"{prefix}Sprites.h"

    size_defines = (
        f"// Uniform on-screen square size for this Digimon's action sprites.\n"
        f"#define {prefix.upper()}_SPRITE_SIZE   {size}\n"
    )
    if has_profile:
        size_defines += f"#define {prefix.upper()}_PROFILE_SIZE  {profile_size}\n"
    if attack_w > 0:
        size_defines += (
            f"// Attack frames use their OWN box (bigger than the pose square).\n"
            f"#define {prefix.upper()}_ATTACK_W      {attack_w}\n"
            f"#define {prefix.upper()}_ATTACK_H      {attack_h}\n")

    header_txt = (
        f"// AUTO-GENERATED by tools/build_digimon_sprites.py -- do not edit by hand.\n"
        f"// Digimon: {digimon}   action size: {size}px ({size_note})   "
        f"profile size: {profile_size}px ({profile_note})   pad: {pad:.2f}\n"
        f"#ifndef {guard}\n#define {guard}\n\n#include <Arduino.h>\n\n"
        + size_defines + "\n"
        + "\n".join(h_externs) + "\n\n"
        + "\n".join(h_tables) + "\n\n#endif\n"
    )
    cpp_txt = (
        f"// AUTO-GENERATED by tools/build_digimon_sprites.py -- do not edit by hand.\n"
        f"// Digimon: {digimon}\n"
        f'#include "{h_name}"\n#include <pgmspace.h>\n\n'
        + "\n\n".join(cpp_blocks) + "\n"
    )

    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, h_name), "w", encoding="utf-8") as f:
        f.write(header_txt)
    with open(os.path.join(out_dir, cpp_name), "w", encoding="utf-8") as f:
        f.write(cpp_txt)

    print(f"[ok] {digimon}: action size = {size}px ({size_note}); "
          f"profile = {profile_size}px ({profile_note})"
          + (f"; attack box = {attack_w}x{attack_h}" if attack_w else ""))
    print(f"     wrote {out_dir}/{cpp_name} and {out_dir}/{h_name}")
    for line in summary:
        print(f"       - {line}")
    if skipped:
        print(f"     skipped {len(skipped)} non-sprite file(s):")
        for sk in skipped:
            print(f"       ~ {sk}")

    if register:
        # Exclude 'profile' from the animation count map (it's not an action table).
        anim_counts = {b: c for b, c in count_by_base.items() if b != "profile"}
        register_in_registry(digimon, out_dir, anim_counts, has_profile, stats,
                             (attack_w, attack_h))



# ==========================================================================
#  REGISTRY INTEGRATION
#  Also register the generated Digimon in src/DigimonRegistry.cpp/.h so it is
#  usable at runtime without hand-editing. Idempotent: re-running for the same
#  Digimon REPLACES its block (matched by AUTO markers) rather than duplicating.
#  Animation fallbacks mirror Terriermon exactly:
#    walkBack->walk, sleep->walk, eat/play->happy->walk, sad->walk,
#    dead->sleep->walk, happy->walk, attack->happy->walk, profile->nullptr.
#  Base stats come from --stats (default 0 0 0). Evolutions default to none
#  (nullptr, 0) -- wire evolution chains by hand afterwards.
# ==========================================================================

def _action_expr(prefix, actions_present, base, count_by_base):
    """Return (table_symbol, count) for an action, applying Terriermon-style
    fallbacks based on which actions this Digimon actually ships.
    `actions_present` is the set of base names that produced a frame table."""
    def tbl(b):
        return f"{prefix}_{b}_frames", count_by_base[b]

    # Preference chains per action (first present wins).
    chains = {
        "walk":     ["walk"],
        "walkBack": ["walkback", "walk"],
        "sleep":    ["sleep", "walk"],
        "eat":      ["eat", "happy", "walk"],
        "play":     ["play", "happy", "walk"],
        "sad":      ["sad", "walk"],
        "dead":     ["dead", "sleep", "walk"],
        "happy":    ["happy", "walk"],
        "attack":   ["attack", "happy", "walk"],
    }
    for cand in chains[base]:
        if cand in actions_present:
            return tbl(cand)
    # Nothing available at all -> nullptr, 0 (shouldn't happen if walk exists).
    return "nullptr", 0


def register_in_registry(digimon, out_dir, count_by_base, has_profile, stats=None,
                         attack_box=(0, 0)):
    """Insert/replace this Digimon's block in DigimonRegistry.cpp/.h.
    `stats` = (maxHp, ap, dp) to set, or None to keep the existing values.
    `attack_box` = (w, h) for the attack frames, (0,0) if none."""
    prefix = sanitize(digimon)
    UP = prefix.upper()
    reg_cpp = os.path.join(out_dir, "DigimonRegistry.cpp")
    reg_h = os.path.join(out_dir, "DigimonRegistry.h")
    if not (os.path.isfile(reg_cpp) and os.path.isfile(reg_h)):
        print(f"     [registry] {reg_cpp} / .h not found -- skipping registration.")
        return

    actions_present = set(count_by_base.keys())

    # --- Frame-table symbols, wrapping single-frame actions in a 1-elem table ---
    # Multi-frame actions already have `<prefix>_<base>_frames`. Single-frame
    # actions (count 1, generated as a lone symbol `<prefix>_<base>`) need a
    # wrapper table so the struct can point at it uniformly.
    wrapper_lines = []
    frame_table = {}   # base -> (symbol, count)
    for base, cnt in count_by_base.items():
        if cnt > 1:
            frame_table[base] = (f"{prefix}_{base}_frames", cnt)
        else:
            wrap = f"{prefix}_{base}_frames"
            wrapper_lines.append(
                f"static const uint16_t* const {wrap}[1] = {{ {prefix}_{base} }};")
            frame_table[base] = (wrap, 1)

    def expr(action_field, base):
        sym, cnt = _action_expr(prefix, actions_present, base, {b: frame_table[b][1] for b in frame_table})
        if sym == "nullptr":
            return "nullptr", 0
        # map chosen base -> its (possibly wrapped) table symbol
        chosen_base = None
        chains = {
            "walk": ["walk"], "walkBack": ["walkback", "walk"],
            "sleep": ["sleep", "walk"], "eat": ["eat", "happy", "walk"],
            "play": ["play", "happy", "walk"], "sad": ["sad", "walk"],
            "dead": ["dead", "sleep", "walk"], "happy": ["happy", "walk"],
            "attack": ["attack", "happy", "walk"],
        }
        for cand in chains[base]:
            if cand in actions_present:
                chosen_base = cand
                break
        tsym, tcnt = frame_table[chosen_base]
        return tsym, tcnt

    w   = expr("walk", "walk")
    wb  = expr("walkBack", "walkBack")
    sl  = expr("sleep", "sleep")
    ea  = expr("eat", "eat")
    pl  = expr("play", "play")
    sa  = expr("sad", "sad")
    de  = expr("dead", "dead")
    ha  = expr("happy", "happy")
    at  = expr("attack", "attack")
    profile_expr = f"{prefix}_profile" if has_profile else "nullptr"

    # --- Hand-edited fields are KEPT on re-runs ---------------------------------
    # realHeightCm, type, base stats and evolutions are usually tuned by hand
    # after the first generation. When this Digimon already has an AUTO block,
    # reuse those lines from it (matched by their trailing comment) so a sprite
    # regeneration never resets them. --stats overrides the stat line.
    begin = f"// >>> AUTO-REGISTER {prefix} BEGIN (build_digimon_sprites.py)"
    end   = f"// <<< AUTO-REGISTER {prefix} END"
    import re as _re
    pat = _re.compile(_re.escape(begin) + r".*?" + _re.escape(end), _re.S)
    cpp = open(reg_cpp, encoding="utf-8").read()
    old = pat.search(cpp)
    old_lines = old.group(0).splitlines() if old else []

    # A hand-written (non-AUTO) definition already exists: don't create a
    # duplicate symbol.
    if not old and f"const DigimonSprites DIGIMON_{prefix} =" in cpp:
        print(f"     [registry] DIGIMON_{prefix} is defined by hand in "
              f"DigimonRegistry.cpp -- left untouched.")
        return

    def keep(tag, default):
        for line in old_lines:
            if "//" in line and tag in line.split("//", 1)[1]:
                return line
        return default

    height_line = keep("realHeightCm",
        "  0,                                  // realHeightCm (set by hand if used)")
    type_line = keep("combat type",
        "  TYPE_DATA,                          // combat type")
    if stats is None:
        stats_line = keep("baseMaxHp",
            "  0, 0, 0,                            // baseMaxHp, baseAp, baseDp")
    else:
        stats_line = (f"  {stats[0]}, {stats[1]}, {stats[2]},"
                      f"                        // baseMaxHp, baseAp, baseDp")
    intel_line = keep("baseIntel",
        "  10, 10,                             // baseIntel, baseSpeed")
    evo_line = keep("evolutions",
        "  nullptr, 0,                         // evolutions, count (wire by hand)")

    # --- Build the .cpp block (guarded by AUTO markers for idempotent replace) ---
    cpp_block = (
        f"{begin}\n"
        + ("\n".join(wrapper_lines) + "\n" if wrapper_lines else "")
        + f"const DigimonSprites DIGIMON_{prefix} = {{\n"
        f'  "{prefix}",\n'
        f"  {UP}_SPRITE_SIZE,\n"
        f"  {UP}_PROFILE_SIZE,\n"
        f"{height_line}\n"
        f"{type_line}\n"
        f"{stats_line}\n"
        f"{intel_line}\n"
        f"{evo_line}\n"
        f"  {w[0]},      {w[1]},   // walk\n"
        f"  {wb[0]},  {wb[1]},   // walkBack\n"
        f"  {sl[0]},     {sl[1]},   // sleep\n"
        f"  {ea[0]},     {ea[1]},   // eat\n"
        f"  {pl[0]},     {pl[1]},   // play\n"
        f"  {sa[0]},      {sa[1]},   // sad\n"
        f"  {de[0]},     {de[1]},   // dead\n"
        f"  {ha[0]},     {ha[1]},   // happy\n"
        f"  {at[0]},    {at[1]},   // attack\n"
        f"  {profile_expr},               // profile\n"
        + (f"  {UP}_ATTACK_W, {UP}_ATTACK_H     // attack box (w, h)\n"
           if attack_box[0] > 0 else
           "  0, 0                                // attack box (none)\n")
        + f"}};\n"
        f"{end}"
    )

    # ---- Patch the .cpp ----
    # 1. ensure the sprite header is included
    inc = f'#include "{prefix}Sprites.h"'
    if inc not in cpp:
        cpp = cpp.replace('#include "DigimonRegistry.h"\n',
                          f'#include "DigimonRegistry.h"\n{inc}\n', 1)

    # 2. replace existing AUTO block, or insert before DIGIMON_ALL
    if old:
        cpp = pat.sub(lambda _m: cpp_block, cpp)
    else:
        marker = "// --------------------------------------------------------------------------\nconst DigimonSprites* const DIGIMON_ALL[] = {"
        assert marker in cpp, "DIGIMON_ALL marker not found in registry cpp"
        cpp = cpp.replace(marker, cpp_block + "\n\n" + marker, 1)

    # 3. add &DIGIMON_<prefix> to DIGIMON_ALL if missing
    entry = f"  &DIGIMON_{prefix},"
    if entry not in cpp:
        cpp = cpp.replace("const DigimonSprites* const DIGIMON_ALL[] = {\n",
                          f"const DigimonSprites* const DIGIMON_ALL[] = {{\n{entry}\n", 1)

    open(reg_cpp, "w", encoding="utf-8", newline="").write(cpp)

    # ---- Patch the .h: extern declaration ----
    h = open(reg_h, encoding="utf-8").read()
    ext = f"extern const DigimonSprites DIGIMON_{prefix};"
    if ext not in h:
        # add after the last existing "extern const DigimonSprites DIGIMON_..." line
        lines = h.splitlines(keepends=True)
        last = max(i for i, l in enumerate(lines)
                   if l.startswith("extern const DigimonSprites DIGIMON_"))
        lines.insert(last + 1, ext + "\n")
        h = "".join(lines)
        open(reg_h, "w", encoding="utf-8", newline="").write(h)

    stats_note = (f"stats {stats[0]}/{stats[1]}/{stats[2]}" if stats is not None
                  else "type/stats/evolutions kept" if old else "default stats")
    print(f"     [registry] registered DIGIMON_{prefix} "
          f"({stats_note}); include + extern + DIGIMON_ALL entry ensured.")



def main() -> None:
    ap = argparse.ArgumentParser(
        description="Generate <digimon>Sprites.cpp/.h from a Digimon sprite folder. "
                    "Give a folder of folders to build every Digimon below it.")
    ap.add_argument("target",
                    help="a Digimon folder (e.g. Assets/Digimons/ToGenerate/Agumon), "
                         "a folder containing Digimon folders at any depth "
                         "(e.g. Assets/Digimons/ToGenerate), or a bare name "
                         "looked up under --assets (e.g. terriermon)")
    ap.add_argument("--size", type=int, default=None,
                    help="force an exact square action-sprite size; omit to "
                         "auto-size from the normal poses (walk/happy/...) + --pad")
    ap.add_argument("--profile-size", type=int, default=None,
                    help="force the profile square size; omit to use the profile "
                         "image's own size (native, no padding)")
    ap.add_argument("--min-size", type=int, default=16,
                    help="lower clamp for the auto-derived size (default 16)")
    ap.add_argument("--max-size", type=int, default=64,
                    help="upper clamp for the auto-derived size (default 64)")
    ap.add_argument("--pad", type=float, default=0.12,
                    help="transparent safety margin per side, as a fraction of "
                         "the square (default 0.12 = ~12%% border; 0 = fill edge)")
    ap.add_argument("--scale", type=int, default=1,
                    help="integer upscale cap for EVEN pixels. 1 (default) = native "
                         "1:1, 2 = each source pixel -> 2x2 block, 0 = fractional "
                         "fit-scale (uneven pixels; avoid)")
    ap.add_argument("--attack-max", type=int, default=48,
                    help="longest side (px) allowed for the ATTACK frame box. "
                         "Attack art keeps its own aspect ratio and is only ever "
                         "scaled DOWN to this (default 48; 0 = reuse the pose size)")
    ap.add_argument("--stats", type=int, nargs=3, metavar=("MAXHP", "AP", "DP"),
                    default=None,
                    help="set base stats baseMaxHp baseAp baseDp in the registry "
                         "entry (default: keep existing values, 0 0 0 for a new one)")
    ap.add_argument("--no-register", action="store_true",
                    help="only generate <name>Sprites.cpp/.h; do NOT add the "
                         "Digimon to DigimonRegistry.cpp/.h")
    ap.add_argument("--assets", default=os.path.join("Assets", "Digimons"),
                    help="assets root used when `target` is a bare name")
    ap.add_argument("--out-dir", default="src",
                    help="where to write <digimon>Sprites.cpp/.h (default src)")
    args = ap.parse_args()

    target = args.target
    if not os.path.isdir(target):
        target = os.path.join(args.assets, args.target)
    if not os.path.isdir(target):
        sys.exit(f"Folder not found: {args.target} (also tried {target})")

    folders = find_digimon_folders(target)
    if not folders:
        sys.exit(f"No folder with recognized sprite files under {target}. "
                 f"Recognized actions: {sorted(KNOWN_ACTIONS)}")
    if len(folders) > 1 and args.size is not None:
        print(f"note: --size {args.size} applies to all {len(folders)} Digimon.")

    ok, failed = 0, []
    for folder in folders:
        try:
            build(folder, args.out_dir,
                  args.size, args.profile_size, args.min_size, args.max_size,
                  args.pad, args.scale, not args.no_register, args.stats,
                  args.attack_max)
            ok += 1
        except SystemExit as e:          # keep going on a bad folder in a batch
            print(f"[skip] {folder}: {e}")
            failed.append(folder)
    if len(folders) > 1:
        print(f"\nDone: {ok} built, {len(failed)} skipped.")


if __name__ == "__main__":
    main()
