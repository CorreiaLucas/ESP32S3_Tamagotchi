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