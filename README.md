# Wokwi Simulation — ESP32 Digimon Pet

This folder contains a [Wokwi](https://wokwi.com) diagram to prototype the wiring
and game logic **before the real hardware arrives**.

## How to use

1. Go to https://wokwi.com and create a new project (choose **ESP32-S3** / or start blank).
2. Click the `diagram.json` tab in Wokwi and paste the contents of this file's `diagram.json`.
3. Add your sketch code in the code tab (Wokwi uses the Arduino framework).
4. Press the green **Play** button to simulate.

## IMPORTANT: display is a stand-in

Wokwi does **not** stock the Waveshare **SSD1351** 128×128 OLED.
This diagram uses an **ST7789** as a color-SPI stand-in so you can develop and
test:

- the game logic (hunger/happiness/energy, sleep, death, save)
- button handling (LEFT / OK / RIGHT)
- the buzzer
- general drawing / sprite blitting flow

When you move to real hardware, only the **display driver + resolution** change
(ST7789 stand-in → SSD1351 128×128). The button/buzzer wiring below is **exact**
and matches the real breadboard build.

## Pin mapping (matches HardwareConfig + display config)

| Function        | ESP32-S3 GPIO | Wokwi net        |
|-----------------|---------------|------------------|
| Display DIN/MOSI| GPIO11        | lcd SDA          |
| Display CLK/SCK | GPIO12        | lcd SCL          |
| Display CS      | GPIO10        | lcd CS           |
| Display DC      | GPIO13        | lcd DC           |
| Display RST     | GPIO14        | lcd RST          |
| Display VCC     | 3V3           | lcd VCC          |
| Display GND     | GND           | lcd GND          |
| Button LEFT     | GPIO4         | btnLeft → GND    |
| Button OK       | GPIO5         | btnOk → GND      |
| Button RIGHT    | GPIO6         | btnRight → GND   |
| Buzzer +        | GPIO17        | bz1 → GND        |

All buttons use `INPUT_PULLUP` (internal), pressed = LOW. No external resistors.

## Notes / caveats

- The exact Wokwi ST7789 part name/pin labels may need a small tweak depending on
  the current Wokwi library (e.g. `board-st7789` pins may be `SDA/SCL` or
  `MOSI/SCK`). If a wire shows an error in Wokwi, re-map to the pin label Wokwi
  shows on the part — the GPIO side stays the same.
- On real hardware, the Waveshare VCC must be **3.3V only**.

# PNG to RGB565 converter
 - Images are not natively supported by the microcontrollers and need to be converted to RGB565.
 - How to use the script : 
Run it from the project root. Two different modes:
 - Sprites (fit+center, transparent, crisp nearest-neighbor — for pet frames):
 exemple : python tools\png_to_rgb565.py sprite Assets\Digimons\terriermon\terriermon_walk1.png --name walk_0 --size 48
 - Backgrounds (fill+crop, fully opaque, smooth Lanczos — for full-screen art):
 python tools\png_to_rgb565.py background Assets\Backgrounds\Data_forest.png --name background_data_forest --size 128
 - Write straight into Sprites.cpp with --out + --append:
 python tools\png_to_rgb565.py sprite Assets\...\eat1.png --name eat_0 --size 48 --out src\Sprites.cpp --append

# Sprite-sheet splitter (`tools/split_sprite_sheet.py`)

Auto-splits a ripped sprite sheet into individual transparent PNGs (keys out the
background, finds connected blobs, tight-crops each). Add `--auto-name` to also
NAME each sprite by its action instead of a bare index:

```
python tools/split_sprite_sheet.py Assets/Digimons/poyomon/Poyomon.png \
    --out Assets/Digimons/poyomon --prefix poyomon --auto-name
```

Thanks to the project convention, you can usually pass just `--prefix` and omit
the sheet path and `--out`: the sheet is derived as
`Assets/Digimons/<prefix>/<Prefix>.png` (the folder is `<prefix>`; the PNG inside
is matched case-insensitively, so `Kapurimon.png` is found for prefix
`kapurimon`), and `--out` defaults to that same folder:

```
# Equivalent to the explicit paths above:
python tools/split_sprite_sheet.py --prefix poyomon --auto-name
```

### Batch mode — a whole folder of Digimon at once

Point `--batch` at a parent folder and the script runs on **every subfolder**,
using each subfolder's name (lowercased) as the `--prefix`, its `<Name>.png` as
the sheet, and the subfolder itself as the output dir. All other flags apply to
every subfolder. A subfolder with no usable sheet is skipped (the batch keeps
going), and a summary is printed at the end.

```
# To_split/ contains Aruraumon/ and Betamon/ (each with <Name>.png):
python tools/split_sprite_sheet.py --batch Assets/Digimons/To_split --auto-name
#   -> Aruraumon/Aruraumon.png -> prefix "aruraumon", split into Aruraumon/
#   -> Betamon/Betamon.png     -> prefix "betamon",   split into Betamon/
```

`--auto-name` is now ROW-AWARE and COUNT-DRIVEN (deterministic — no size
guessing, which was unreliable across these varied rips):

`--auto-name` is TWO-TIER and ROW-AWARE (deterministic), validated to reproduce
the finished sets in `InTraining/*` and `ToGenerate/*`. These Digimon World
sheets are laid out as:

```
row of BIG battle poses   (top)       -> attack1..N   (ALL of them)
row(s) of SMALL map frames (below)    -> walk/walkback/happy + profile
credit text + ripper avatar (bottom)  -> dropped (_To_Remove_)
```

- Rows below the top sprite band (credit text, avatar) are dropped by
  **position**, robust to text fragmenting into dozens of tiny word blobs.
- The band is split into a **large tier** (the big poses → `attack1..N`, all of
  them by default; cap with `--attack N`) and a **small tier** (the map sprites).
- In the small tier: `profile` = most-square frame; the rest are consumed
  **strictly left-to-right** per `--order` (default `walkback,walk,happy`) —
  `walkback1..3`, then `walk1..3`, then `happy1..3`. No mirror detection; the
  map-sprite row is already in order, so position decides the name. Extra small
  frames (unused map directions) → `_To_Remove_`.
- If the automatic band/tier split misjudges, override with `--sprite-rows N` or
  `--text-below Y`; the printed table shows each blob's `y` and `w x h`.
- `--clean` wipes prior `<prefix>_*.png` + `_preview.png` before writing (handy
  for re-runs); it never touches the source sheet.

```
# Agumon: big top row -> attack1..N, small row -> walkback/walk/happy + profile:
python tools/split_sprite_sheet.py Assets/Digimons/Agumon/Agumon.png \
    --out Assets/Digimons/Agumon --prefix agumon --auto-name --clean
```

Count flags (used with `--auto-name`):

| Flag | Default | Meaning |
|------|---------|---------|
| `--walk N` | 3 | WALK frames (small tier, reading order). |
| `--walkback N` | 3 | WALKBACK frames (small tier, reading order). |
| `--happy N` | 3 | HAPPY frames (small tier, reading order). |
| `--attack N` | 0 | Cap on ATTACK frames (big top row). 0 = all of them. |
| `--profile 0/1` | 1 | Name one profile sprite (most-square small frame), or skip. |
| `--order` | `walkback,walk,happy` | Left-to-right order the small tier is consumed. |
| `--sprite-rows N` | auto | Keep exactly the top N visual rows as sprites; drop the rest. |
| `--text-below Y` | auto | Drop every blob whose top is at/below pixel row Y. |
| `--clean` | off | Delete prior `<prefix>_*.png` + `_preview.png` before writing. |
| `--keep-text` | off | Keep the below-band region instead of dropping it to `_To_Remove_`. |

Because these ripped sheets vary, treat the result as a first pass: skim the
printed per-sprite table and `_preview.png`, delete the `_To_Remove_` files you
agree with, and rename any frame the order mis-slotted.

### Multiple background colors

`detect_background_colors` keys out EVERY color whose largest single blob covers
≥3% of the sheet (tune with `--min-fill-fraction`), so sheets that group sprites
on colored PANELS over an outer margin (e.g. Koromon's pink boxes on purple) are
handled — both the panel and the margin are removed. A sheet with a single
background still works, since the outer fill always qualifies.

```
# Example with a manual exclude region (e.g. a stubborn watermark):
python tools/split_sprite_sheet.py Assets/Digimons/koromon/Koromon.png \
    --out Assets/Digimons/koromon --prefix koromon --auto-name \
    --walk 3 --walkback 3 --happy 2 --attack 5 --exclude-box 230,95,400,170
```

The script prints a per-sprite table (idx, size, assigned name, reason). Because
these ripped sheets vary a lot, treat the result as a first pass: skim the
summary / `_preview.png` and hand-correct any mis-slotted frame before running
the Digimon sprite generator below.

# Digimon sprite generator (whole folder → <name>Sprites.cpp/.h)

`tools\build_digimon_sprites.py` automates converting an ENTIRE Digimon folder
to RGB565 in one command (wraps `png_to_rgb565.py`). It groups animation frames,
prefixes every symbol with the Digimon name (so multiple Digimon coexist for
**digivolution**), and emits `src\<name>Sprites.cpp` + `.h`.

## What it does
- Scans `Assets\Digimons\<name>\` for PNGs.
- Strips the Digimon-name prefix and splits each file into an ACTION base
  (walk, walkback, happy, attack, sleep, profile, ...) + optional frame number.
  Handles both naming styles: `terriermon_walk1.png` and `walk_1.png`.
- Groups same-base frames into a 0-based animation table, e.g.:
  `const uint16_t* const terriermon_walk_frames[3] = { terriermon_walk_0, terriermon_walk_1, terriermon_walk_2 };`
- Single unnumbered images (sleep, profile) become a lone symbol.
- Emits `#define <NAME>_SPRITE_SIZE` / `_PROFILE_SIZE` / `_ATTACK_W` / `_ATTACK_H`
  so the firmware knows the stored size of each group.
- Skips non-sprite files (source sheets like `gargomon.png`) and lists them.

## Sizing convention (per-action, native 1:1 where possible)

Sprites are stored at native art size and drawn 1:1 (no runtime scaling), which
keeps pixels crisp. Sizes are **per action group**, not one value per Digimon:

| group | box | why |
|---|---|---|
| normal poses (walk, walkback, happy, sleep, ...) | one shared square = largest pose + `--pad` | the wandering pet must not change size between actions, or it appears to grow/shrink |
| **attack** | its own `w x h` at the art's aspect ratio, longest side capped by `--attack-max` (48) | attack art is ~3x the poses (e.g. Agumon 62x73 vs 22x26); sharing the pose square crushed it to ~37% |
| profile | the profile image's own size, no padding | it's a portrait, not a pose |

Attack frames are combat-only (`drawCombatScene` draws them, the main screen
never does), so a different size there cannot disturb the wandering pet. The
generator emits `#define <NAME>_ATTACK_W` / `_ATTACK_H` and fills `attackW` /
`attackH` in the registry entry; both are `0` for older sprite sets, which makes
the firmware fall back to a `spriteSize` square.

Downscaling now uses **BOX** (area-average) instead of NEAREST. At a fractional
factor NEAREST drops whole rows and columns, so eyes and limbs vanished.

Apparent size differences between Digimon are still meant to be conveyed by
**background zoom** (`realHeightCm`), not by scaling sprites.

## Usage
```
# Sizes are picked automatically (native 1:1 is now the default):
#   action size  = largest NORMAL pose (walk/walkback/happy/sleep...; attack
#                  excluded) + the --pad margin
#   attack box   = attack art's aspect, longest side <= --attack-max
#   profile size = the profile image's own size, no padding
python tools\build_digimon_sprites.py Assets\Digimons\ToGenerate\Agumon

# A folder of folders builds every Digimon folder below it, at any depth:
python tools\build_digimon_sprites.py Assets\Digimons\ToGenerate

# A bare name is still looked up under --assets (Assets\Digimons):
python tools\build_digimon_sprites.py terriermon --size 41 --profile-size 30
```

Re-running on a Digimon that is already registered keeps its hand-edited
`realHeightCm`, type, base stats and evolutions (pass `--stats` to overwrite the
stats). Digimon defined by hand in `DigimonRegistry.cpp` (terriermon, gargomon)
are never touched by the registration step.

Flags:
- `--size N`       : force the square canvas size (omit to auto-derive from art).
- `--profile-size N`: force the profile square size (omit = profile's own size).
- `--attack-max N` : longest side allowed for the attack box (default 48). Attack
                     art keeps its aspect ratio and is only ever scaled DOWN to
                     this. `0` = reuse the pose square (old behaviour).
- `--pad F`        : transparent safety margin per side (default 0.12 ≈ 12%),
                     prevents ears/feet from clipping the canvas edge.
- `--scale N`      : integer scale cap. `1` = TRUE native 1:1 — frames are
                     centered at their own pixel size with transparent padding,
                     NEVER fractionally upscaled (this is what keeps pixels even).
                     `2` = crisp 2x. `0` = fit-scale to fill (fractional — causes
                     uneven pixels for frames smaller than the box; avoid).
                     ⚠️ Use `--scale 1` for native art, NOT `--scale 0`.
- `--min-size / --max-size`: clamps for the auto-derived size.

## Adding a new Digimon
1. Drop its PNGs in `Assets\Digimons\<name>\` (name frames like `walk1.png`).
2. Run the generator with a `--size` that fits its largest normal pose (+~25%).
3. Add it to `DigimonRegistry.cpp/.h`: `#include "<name>Sprites.h"`, a
   `DIGIMON_<name>` entry (map its action tables, set `realHeightCm`), and add it
   to `DIGIMON_ALL[]`.

## Note — Wokwi ST7789 color/byte-order
Sprites are composited by reading RGB565 straight from PROGMEM into a plain RAM
buffer, then pushed with `drawRGBBitmap`. Do NOT round-trip sprite pixels through
`GFXcanvas16` (its `getBuffer()` byte order differs on the Wokwi ST7789 stand-in
and renders colors purple/pink). The direct-PROGMEM background is the color
reference.

# Game state machine + visualizer

The game logic is a **table-driven finite state machine** in
`src/GameStateMachine.{h,cpp}` (the `.ino` just calls `gsmSetup()`/`gsmLoop()`).
Each state provides an `onEnter()` (paint the screen once) and an `onUpdate()`
(per-frame logic that returns the next state). All states live in the static
`kStates[]` table; menus share one reusable `MenuController`. To add a state:
add it to the `GameState` enum, write its two handlers, and add a row to
`kStates[]`.

## Visualize the states as a graph

`tools/fsm_graph.py` parses the FSM source and emits `tools/fsm.json`
(states + transitions, with source line numbers). `tools/fsm_viewer.html` renders
it as an interactive, Blueprint-style graph (pan/zoom, drag nodes, click a node
or edge to highlight its connections and see the handler + trigger).

```
# 1. Extract the graph (re-run after editing GameStateMachine.cpp):
python tools/fsm_graph.py

# 2. Open the viewer (it auto-loads fsm.json sitting next to it). Because
#    browsers block fetch() on file://, serve the tools folder:
python -m http.server 8000 --directory tools
#    then open http://localhost:8000/fsm_viewer.html
#    (Or just open the HTML directly and use the “Load fsm.json” button.)
```

Notes / caveats:
- Transition edges/topology are exact. Trigger *labels* are heuristic (derived
  from the nearest `case`/comment/`if`), so a couple may show an incidental
  guard (e.g. the Feed/Play actions show `pet.isSleeping()` instead of the case
  comment) — the arrows themselves are correct.

## Node coloring by category

Each `kStates[]` row carries an `@cat:` tag that colors its node in the viewer:

| category | meaning                              | color  |
|----------|--------------------------------------|--------|
| `menu`   | navigable item lists                 | blue   |
| `dialog` | NPC / conversation states            | purple |
| `action` | interactive gameplay (minigame/combat)| orange |
| `page`   | static info pages (stats, digivolve) | slate  |
| `system` | engine states (main screen, death)   | green  |

```cpp
/* STATE_MINIGAME  @cat:action */ { minigameOnEnter, minigameOnUpdate },
```
If a row omits `@cat:`, the extractor infers one from the name. The viewer shows
a legend and lets you re-tag a state (see Tier 2 below).

## Tier 2 — safe visual editing (write-back)

`fsm_graph.py` can also *edit* the FSM, but only in machine-managed regions —
it never rewrites your hand-written handler bodies:

```
# Add a new state: inserts the enum entry, a kStates[] row (@cat tag), and
# STUB onEnter/onUpdate handlers wrapped in `// >>> FSM-GEN:` markers.
python tools/fsm_graph.py add-state STATE_SHOP --category action          # dry-run diff
python tools/fsm_graph.py add-state STATE_SHOP --category action --apply   # write (+ .bak)

# Re-tag an existing state's category:
python tools/fsm_graph.py set-category STATE_MINIGAME action --apply
```

Safety model:
- Default is a **dry-run** that prints a unified diff; `--apply` writes and first
  saves a `<file>.bak` backup.
- New handlers are **stubs inside `FSM-GEN` markers** — safe to fill in; keep the
  markers. Existing hand-written handlers are never touched.
- Duplicate / malformed state names are refused with a clear error.
- The **viewer's Edit panel never writes files** — it generates the exact
  `fsm_graph.py` command for you to run in a terminal (so writes stay reviewable).

## NPC dialogs

Conversations are **data-driven** (`src/DialogManager.{h,cpp}`), not one state
per line: a single `STATE_NPC_DIALOG` FSM state runs an interpreter over a table
of `DialogNode { speaker, text, options[] }`. The node's `speaker` is drawn as
the NPC's name header, so each conversation shows who is talking.

One dialog state serves **every** NPC: a caller sets `pendingDialog` to the
script it wants, then transitions. Current entry points:

| Reached from            | Script             | NPC      |
|-------------------------|--------------------|----------|
| Digimon menu → **Training** | `trainer_pandamon` | Pandamon |

### Options can have side effects

An option is `{ label, next, action }`. `next` is the node index to jump to (or
`DIALOG_END`); `action` is a `DialogAction` enum value. The script stays **pure
data** — it names an effect rather than holding a function pointer — and the
dispatch lives in `runDialogAction()` in `GameStateMachine.cpp` (where `pet` and
`sound` are). Omit the third field for pure navigation.

```cpp
// @dialog-script-begin: trainer_pandamon
static const DialogNode kTrainerPandamon[] = {
  // @node menu
  { "Pandamon", "Training time! Which drill today?",
    { { "Strength", 1, DLG_TRAIN_AP },
      { "Defense",  1, DLG_TRAIN_DP },
      { "Stamina",  1, DLG_TRAIN_HP } } },
  ...
};
// @dialog-script-end
```

Add a new effect by extending `enum DialogAction` and adding a `case` to
`runDialogAction()`.

### Gated actions (training costs energy)

`DialogScript` carries a `refusedNode`. When an effect's requirements aren't met,
the FSM redirects the conversation there instead of running it, so the "you
can't do that" wording stays in the script rather than being hard-coded.

Training costs `PetState::TRAIN_ENERGY_COST` (15) energy per drill and is
refused by `PetState::canTrain()` when the pet is asleep, dead, or below that
cost. From a full 100 energy that's six drills before Pandamon sends you to rest.

### Visualizing dialogs

```
python tools/fsm_graph.py dialogs        # -> tools/dialogs.json
```
The viewer's **Dialogs** tab shows the conversation graph. With more than one
script a picker appears in the toolbar. Nodes are marked ▶ start and ⛔ refused,
and edges show ⚡ACTION when a choice has a side effect.

## Adventure mode

Action → **Battle** opens the adventure. It is a linear campaign: a list of
**areas**, each a fixed sequence of fights ending in a **boss**. Beating an
area's boss unlocks the next area.

Files:
- `src/AdventureRegistry.{h,cpp}` — the areas (data only). Each
  `AdventureArea` is `{ name, enemies[], enemyCount, boss, background }`;
  enemies come from `EnemyRegistry`. Order in `ADVENTURE_ALL[]` = unlock order.
- `src/AdventureManager.{h,cpp}` — saves progress (number of areas cleared,
  Preferences namespace `"adv_data"`) and tracks the current run (area + fight
  index, RAM only).
- `GameStateMachine.cpp` — three states:
  `STATE_ADVENTURE_SELECT` (area list), `STATE_ADVENTURE_INTERLUDE` (screen
  before each fight + cleared/defeat result), `STATE_ADVENTURE_BATTLE` (one fight
  through the existing `Combat` engine).

Rules:
- Locked areas show as `???` and can't be entered; cleared areas show `*`.
- A run starts at full HP. **HP carries over** between fights, plus a heal of
  `ADV_HEAL_PERCENT` (30%) of max HP after each win.
- Between fights you can **Continue** or **Retreat**. Retreat keeps the XP already
  earned (XP is granted at the end of each fight) but doesn't clear the area.
- Losing a fight ends the run. Replaying a cleared area unlocks nothing new.
- Settings → Reset and starting a new egg after death both reset adventure
  progress to the first area.

Adding an area: write its enemy list and an `AdventureArea` in
`AdventureRegistry.cpp`, then add it to `ADVENTURE_ALL[]`. Leave `background`
as `nullptr` until the art exists (battles then use the forest background).

## TODO
[ ]  Training
  [ ] Minigame or just waiting  
  [ ]
  [ ]
[ ]  Combats
  [X] Combat sprites (attack art sweeps across the screen; always on Special,
      ~45% on a normal attack)
  [ ] Enemies
[X]  Adventure mode (linear areas + boss, unlocks, HP carry-over, retreat)
  [ ] Per-area combat backgrounds
  [ ] Real enemies / areas (currently placeholders)
[ ]  Eggs / more digimons
[X]  Real digivolution system 
  [X]  Attaching base stat for each digimon
[ ] Correct time gestion


