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
- Emits `#define <NAME>_SPRITE_SIZE` / `_PROFILE_SIZE` so the firmware knows the
  stored size.
- Skips non-sprite files (source sheets like `gargomon.png`) and lists them.

## Current sizing convention (native 1:1 — no upscaling)
Sprites are stored at each Digimon's NATIVE art size and drawn 1:1 (no runtime
scaling), which keeps pixels crisp/even. Apparent size differences between
Digimon are intended to be conveyed later via **background zoom** (using
`realHeightCm` in `DigimonRegistry`), NOT by scaling the sprites.

Current sizes: terriermon 41px, gargomon 50px (profile 30px), pad 0.12.

## Usage
```
# Native 1:1 (current convention). --size = the box that fits the largest
# normal-pose frame + ~25% padding headroom.
python tools\build_digimon_sprites.py terriermon --size 41 --profile-size 30 --scale 1
python tools\build_digimon_sprites.py gargomon   --size 50 --profile-size 30 --scale 1
```

Flags:
- `--size N`       : force the square canvas size (omit to auto-derive from art).
- `--profile-size N`: profile sprite square size (default 30).
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

## TODO
[ ]  Training
  [ ] Minigame or just waiting  
  [ ]
  [ ]
[ ]  Combats
  [ ] Combat sprites
  [ ] Enemies
[ ]  Eggs / more digimons
[ ]  Real digivolution system 
  [ ]  Attaching base stat for each digimon
[ ] Correct time gestion


