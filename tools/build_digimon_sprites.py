#!/usr/bin/env python3
"""
build_digimon_sprites.py - Automate PNG -> RGB565 sprite generation for a whole
Digimon folder, ready for digivolution (each Digimon can swap in its full set
of sprites at runtime).

SIZING MODEL: one uniform square size PER DIGIMON.
  A Tamagotchi character must keep a consistent on-screen size across all its
  actions (walking, attacking, ...), otherwise it appears to grow/shrink. So
  the tool derives ONE square size for the whole Digimon from its own art:
  the largest side across all ACTION frames (profile excluded), then clamps it
  into [--min-size, --max-size]. Every action sprite is fit+centered into that
  square; the profile image uses its own --profile-size. Pass --size to force
  an exact size instead of auto-deriving.

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
  # Auto-size from the art (recommended):
  python tools/build_digimon_sprites.py terriermon
  python tools/build_digimon_sprites.py gargomon --profile-size 28

  # Force an exact size:
  python tools/build_digimon_sprites.py terriermon --size 48

Notes:
  * Unrecognized files (empty base, source sheets like "gargomon.png" or
    "DS _ DSi ... Terriermon.png") are SKIPPED and listed at the end.
  * Symbols are prefixed with the sanitized Digimon name -> no collisions.
"""

import argparse
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


def derive_size(actions: dict, min_size: int, max_size: int) -> int:
    """One uniform square size for the whole Digimon: the largest side across
    all ACTION frames (profile excluded), clamped to [min_size, max_size]."""
    largest = 0
    for base, entries in actions.items():
        if base == "profile":
            continue
        for _, path in entries:
            w, h = Image.open(path).size
            largest = max(largest, w, h)
    if largest == 0:                     # only a profile? fall back to min.
        largest = min_size
    return max(min_size, min(max_size, largest))


def build(digimon: str, assets_dir: str, out_dir: str,
          size: int | None, profile_size: int,
          min_size: int, max_size: int, pad: float,
          integer_scale: int = 0, register: bool = True,
          stats=(0, 0, 0)) -> None:
    folder = os.path.join(assets_dir, digimon)
    if not os.path.isdir(folder):
        sys.exit(f"Folder not found: {folder}")

    prefix = sanitize(digimon)
    actions, skipped = collect(folder, digimon)
    if not actions:
        sys.exit(f"No recognized sprite files in {folder}. "
                 f"Recognized actions: {sorted(KNOWN_ACTIONS)}")

    # One uniform action-sprite size for this Digimon.
    if size is None:
        size = derive_size(actions, min_size, max_size)
        size_note = f"auto-derived (clamped to [{min_size},{max_size}])"
    else:
        size_note = "forced via --size"

    cpp_blocks: list[str] = []
    h_externs: list[str] = []
    h_tables: list[str] = []
    summary: list[str] = []
    has_profile = "profile" in actions
    count_by_base: dict[str, int] = {}   # base -> number of frames (for registry)

    for base in sorted(actions):
        entries = actions[base]
        entries.sort(key=lambda e: (e[0] is None, e[0] if e[0] is not None else 0))
        this_size = profile_size if base == "profile" else size

        symbols = []
        for new_idx, (_, path) in enumerate(entries):
            if len(entries) == 1 and entries[0][0] is None:
                sym = f"{prefix}_{base}"
            else:
                sym = f"{prefix}_{base}_{new_idx}"
            if integer_scale > 0:
                vals = convert_sprite_integer(path, this_size, integer_scale, pad)
            else:
                vals = convert_sprite_padded(path, this_size, pad)
            comment = (f"// {sym}: {this_size}x{this_size} RGB565 sprite of "
                       f"{os.path.basename(path)}")
            cpp_blocks.append(comment + "\n" + format_array(sym, vals, per_row=this_size))
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

    header_txt = (
        f"// AUTO-GENERATED by tools/build_digimon_sprites.py -- do not edit by hand.\n"
        f"// Digimon: {digimon}   action size: {size}px ({size_note})   "
        f"profile size: {profile_size}px   pad: {pad:.2f}\n"
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
          f"profile = {profile_size}px")
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
        register_in_registry(digimon, out_dir, anim_counts, has_profile,
                             stats[0], stats[1], stats[2])



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


def register_in_registry(digimon, out_dir, count_by_base, has_profile,
                         base_max_hp, base_ap, base_dp):
    """Insert/replace this Digimon's block in DigimonRegistry.cpp/.h."""
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

    # --- Build the .cpp block (guarded by AUTO markers for idempotent replace) ---
    begin = f"// >>> AUTO-REGISTER {prefix} BEGIN (build_digimon_sprites.py)"
    end   = f"// <<< AUTO-REGISTER {prefix} END"
    cpp_block = (
        f"{begin}\n"
        + ("\n".join(wrapper_lines) + "\n" if wrapper_lines else "")
        + f"const DigimonSprites DIGIMON_{prefix} = {{\n"
        f'  "{prefix}",\n'
        f"  {UP}_SPRITE_SIZE,\n"
        f"  {UP}_PROFILE_SIZE,\n"
        f"  0,                                  // realHeightCm (set by hand if used)\n"
        f"  {base_max_hp}, {base_ap}, {base_dp},                        // baseMaxHp, baseAp, baseDp\n"
        f"  nullptr, 0,                         // evolutions, count (wire by hand)\n"
        f"  {w[0]},      {w[1]},   // walk\n"
        f"  {wb[0]},  {wb[1]},   // walkBack\n"
        f"  {sl[0]},     {sl[1]},   // sleep\n"
        f"  {ea[0]},     {ea[1]},   // eat\n"
        f"  {pl[0]},     {pl[1]},   // play\n"
        f"  {sa[0]},      {sa[1]},   // sad\n"
        f"  {de[0]},     {de[1]},   // dead\n"
        f"  {ha[0]},     {ha[1]},   // happy\n"
        f"  {at[0]},    {at[1]},   // attack\n"
        f"  {profile_expr}               // profile\n"
        f"}};\n"
        f"{end}"
    )

    # ---- Patch the .cpp ----
    cpp = open(reg_cpp, encoding="utf-8").read()

    # 1. ensure the sprite header is included
    inc = f'#include "{prefix}Sprites.h"'
    if inc not in cpp:
        cpp = cpp.replace('#include "DigimonRegistry.h"\n',
                          f'#include "DigimonRegistry.h"\n{inc}\n', 1)

    # 2. replace existing AUTO block, or insert before DIGIMON_ALL
    import re as _re
    pat = _re.compile(_re.escape(begin) + r".*?" + _re.escape(end), _re.S)
    if pat.search(cpp):
        cpp = pat.sub(cpp_block, cpp)
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

    print(f"     [registry] registered DIGIMON_{prefix} "
          f"(stats {base_max_hp}/{base_ap}/{base_dp}); added include + extern + DIGIMON_ALL entry.")



def main() -> None:
    ap = argparse.ArgumentParser(
        description="Generate <digimon>Sprites.cpp/.h from Assets/Digimons/<digimon>/")
    ap.add_argument("digimon", help="folder name under the assets dir (e.g. terriermon)")
    ap.add_argument("--size", type=int, default=None,
                    help="force an exact square action-sprite size; "
                         "omit to auto-derive one uniform size from the art")
    ap.add_argument("--profile-size", type=int, default=30,
                    help="square size for the profile sprite (default 30)")
    ap.add_argument("--min-size", type=int, default=32,
                    help="lower clamp for the auto-derived size (default 32)")
    ap.add_argument("--max-size", type=int, default=56,
                    help="upper clamp for the auto-derived size (default 56)")
    ap.add_argument("--pad", type=float, default=0.12,
                    help="transparent safety margin per side, as a fraction of "
                         "the square (default 0.12 = ~12%% border; 0 = fill edge)")
    ap.add_argument("--scale", type=int, default=0,
                    help="integer upscale factor cap for EVEN pixels (e.g. 2 = "
                         "each source pixel -> 2x2 block). 0 = fit-scale (default)")
    ap.add_argument("--stats", type=int, nargs=3, metavar=("MAXHP", "AP", "DP"),
                    default=[0, 0, 0],
                    help="base stats baseMaxHp baseAp baseDp for the registry "
                         "entry (default: 0 0 0)")
    ap.add_argument("--no-register", action="store_true",
                    help="only generate <name>Sprites.cpp/.h; do NOT add the "
                         "Digimon to DigimonRegistry.cpp/.h")
    ap.add_argument("--assets", default=os.path.join("Assets", "Digimons"),
                    help="assets root holding per-digimon folders")
    ap.add_argument("--out-dir", default="src",
                    help="where to write <digimon>Sprites.cpp/.h (default src)")
    args = ap.parse_args()
    build(args.digimon, args.assets, args.out_dir,
          args.size, args.profile_size, args.min_size, args.max_size, args.pad,
          args.scale, not args.no_register, tuple(args.stats))


if __name__ == "__main__":
    main()
