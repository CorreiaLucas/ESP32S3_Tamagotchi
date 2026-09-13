#include "DisplayManager.h"
#include "Sprites.h"
#include <string.h>

DisplayManager::DisplayManager()
#ifdef SIMULATOR_BUILD
  : tft(&SPI, TFT_CS, TFT_DC, TFT_RST), frameBuffer(SCREEN_WIDTH, SCREEN_HEIGHT)
#else
  : tft(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, TFT_CS, TFT_DC, TFT_RST), frameBuffer(SCREEN_WIDTH, SCREEN_HEIGHT)
#endif
{
  profileSpriteWidth = 30;
  profileSpriteHeight = 30;
}

void DisplayManager::begin() {
  #ifdef SIMULATOR_BUILD
    // Wokwi ST7789 stand-in. Its native panel is 240x240, but the real
    // hardware is a 128x128 SSD1351. To make the simulator visually match
    // the real device, we frame a CENTERED 128x128 window inside the panel
    // via a draw-origin offset (originX/originY). Every DisplayManager draw
    // adds that offset; on real hardware the offset is 0.
    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    tft.init(240, 240);
    // The Adafruit ST7789 stand-in defaults to a slow (~4 MHz) SPI clock,
    // which makes a full-frame push visibly paint line-by-line. Bump it so
    // the 128x128 background/menu blits are effectively instant.
    tft.setSPISpeed(40000000);            // 40 MHz on the sim
    originX = (240 - SCREEN_WIDTH) / 2;   // 56
    originY = (240 - SCREEN_HEIGHT) / 2;  // 56
  #else
    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    // Real hardware: Waveshare SSD1351 128x128. Draw origin is (0,0).
    // The SSD1351 datasheet rates SPI up to 20 MHz; begin() otherwise falls
    // back to a conservative default. Set it explicitly so full-frame redraws
    // (background, menus) push in a few ms instead of appearing slowly.
    tft.begin(20000000);                  // 20 MHz (SSD1351 max)
    originX = 0;
    originY = 0;
  #endif
  // Rotation 2 = 180deg. The Wokwi ST7789 stand-in renders its origin at
  // the opposite corner from what our 128x128 UI layout assumes, so
  // rotation 0 shows everything upside-down. This flips the whole panel.
  tft.setRotation(2);
  #ifdef SIMULATOR_BUILD
    tft.fillScreen(TFT_BEZEL);   // bezel fills the whole 240x240 panel
  #else
    tft.fillScreen(TFT_SAGE_GREEN);
  #endif
}

// Push a RAM uint16_t buffer to the panel. On the Wokwi ST7789 stand-in the
// RAM overload of drawRGBBitmap interprets 16-bit pixels with the OPPOSITE
// byte order to the PROGMEM overload used for the (correct-looking) forest
// background -- so RAM-composited pixels (sprites, trail patches, the whole
// offscreen frame) came out purple/washed-out. We byte-swap the buffer in the
// simulator so every RAM push matches the PROGMEM reference. Real SSD1351
// hardware is left untouched (it renders both overloads identically).
void DisplayManager::pushRGBBuf(int x, int y, uint16_t* buf, int w, int h) {
  // Plain push. RAM buffers here are filled straight from PROGMEM
  // (pgm_read_word), so they already match the byte order of the correct
  // PROGMEM background -- NO swap. (An earlier swap turned the forest green
  // composited behind the pet into a magenta box.)
  tft.drawRGBBitmap(originX + x, originY + y, buf, w, h);
}

void DisplayManager::drawBackground() {
  // Full-screen 128x128 RGB565 background image.
  tft.drawRGBBitmap( originX +0, originY + 0, background_data_forest, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void DisplayManager::drawBackgroundRegion(int x, int y, int w, int h) {
  // Repaint just a slice of the forest background (used to "erase" the pet's
  // trail so the forest shows through instead of a flat colour). Clamps to
  // the panel bounds so partial off-screen regions are safe.
  //
  // Blit ONE ROW AT A TIME with a bulk drawRGBBitmap instead of per-pixel
  // drawPixel: a single row is one SPI address-window + streamed pixels,
  // versus one full transaction per pixel. On the SSD1351 this is ~10-30x
  // faster and is what makes the menu-exit repaint feel instant.

  // Horizontal clip to the panel.
  int x0 = x < 0 ? 0 : x;
  int x1 = (x + w > SCREEN_WIDTH) ? SCREEN_WIDTH : x + w;
  if (x1 <= x0) return;
  int rowW = x1 - x0;

  // Scratch row buffer (max 128 px). Copied out of PROGMEM so drawRGBBitmap
  // can stream it. 128 * 2 bytes = 256 B on the stack.
  uint16_t rowBuf[SCREEN_WIDTH];

  for (int row = 0; row < h; row++) {
    int sy = y + row;
    if (sy < 0 || sy >= SCREEN_HEIGHT) continue;
    const uint16_t* srcRow = &background_data_forest[sy * SCREEN_WIDTH + x0];
    for (int i = 0; i < rowW; i++) {
      rowBuf[i] = pgm_read_word(&srcRow[i]);
    }
    pushRGBBuf(x0, sy, rowBuf, rowW, 1);
  }
}

void DisplayManager::forceFullRedraw(int hunger, int happiness, int energy) {
  drawBackground();
  drawStatusPanelChrome();
  drawMainScreen(hunger, happiness, energy);
}

// ==========================================================================
//  BUFFERED MAIN-SCENE RENDERING (single-blit, no flicker)
//  Everything is composited into frameBuffer (a plain 128x128 canvas with NO
//  originX/originY offset). pushFrame() applies the offset once at blit time.
// ==========================================================================

void DisplayManager::pushFrame() {
  pushRGBBuf(0, 0, frameBuffer.getBuffer(), SCREEN_WIDTH, SCREEN_HEIGHT);
}

void DisplayManager::seedBufferBackground() {
  for (int row = 0; row < SCREEN_HEIGHT; row++) {
    for (int col = 0; col < SCREEN_WIDTH; col++) {
      frameBuffer.drawPixel(col, row, pgm_read_word(&background_data_forest[row * SCREEN_WIDTH + col]));
    }
  }
}

void DisplayManager::composeStatusPanel(int hunger, int happiness, int energy) {
  const uint16_t iconColors[3] = { STAT_HUNGER_COLOR, STAT_HAPPY_COLOR, STAT_ENERGY_COLOR };
  const int values[3] = { hunger, happiness, energy };
  const bool isHeart[3] = { true, false, false };

  for (int row = 0; row < 3; row++) {
    int y = STAT_ROW_Y0 + row * STAT_ROW_H;   // canvas coords, no origin offset
    // Icon
    if (isHeart[row]) {
      int cx = STAT_ICON_X + 3;
      frameBuffer.fillCircle(cx - 1, y + 2, 1, iconColors[row]);
      frameBuffer.fillCircle(cx + 1, y + 2, 1, iconColors[row]);
      frameBuffer.fillTriangle(cx - 2, y + 3, cx + 2, y + 3, cx, y + 5, iconColors[row]);
    } else {
      frameBuffer.fillCircle(STAT_ICON_X + 3, y + 3, 3, iconColors[row]);
    }

    int px = STAT_BAR_X;
    int py = y;
    // Grey plate body + frame + bevel + dividers (same as drawStatusPanelChrome).
    frameBuffer.fillRect(px + 1, py + 1, STAT_BAR_W - 2, STAT_PLATE_H - 2, STAT_PLATE_GREY);
    frameBuffer.drawFastHLine(px + 1, py, STAT_BAR_W - 2, STAT_FRAME_COLOR);
    frameBuffer.drawFastHLine(px + 1, py + STAT_PLATE_H - 1, STAT_BAR_W - 2, STAT_FRAME_COLOR);
    frameBuffer.drawFastVLine(px, py + 1, STAT_PLATE_H - 2, STAT_FRAME_COLOR);
    frameBuffer.drawFastVLine(px + STAT_BAR_W - 1, py + 1, STAT_PLATE_H - 2, STAT_FRAME_COLOR);
    frameBuffer.drawFastHLine(px + 1, py + 1, 2, STAT_BEVEL_WHITE);
    frameBuffer.drawFastHLine(px + 1, py + STAT_PLATE_H - 2, 2, STAT_BEVEL_WHITE);
    frameBuffer.drawPixel(px + 1, py + 2, STAT_BEVEL_WHITE);
    frameBuffer.drawPixel(px + 1, py + 3, STAT_BEVEL_WHITE);
    frameBuffer.drawFastVLine(px + STAT_LEFT_DIV,  py + 1, STAT_PLATE_H - 2, STAT_PLATE_DGREY);
    frameBuffer.drawFastVLine(px + STAT_RIGHT_DIV, py + 1, STAT_PLATE_H - 2, STAT_PLATE_DGREY);
    frameBuffer.drawFastVLine(px + STAT_FILL_BL, py + 2, 2, STAT_FRAME_COLOR);
    frameBuffer.drawFastVLine(px + STAT_FILL_BR, py + 2, 2, STAT_FRAME_COLOR);

    // Number box.
    int boxY = y + (STAT_PLATE_H - STAT_NUM_H) / 2;
    frameBuffer.fillRect(STAT_NUM_X, boxY, STAT_NUM_W, STAT_NUM_H, STAT_NUM_BG);
    frameBuffer.drawRect(STAT_NUM_X, boxY, STAT_NUM_W, STAT_NUM_H, STAT_NUM_FRAME);

    // ---- Dynamic fill lane ----
    int fx0 = px + STAT_FILL_X0;
    int fy  = y + 2;
    int laneW = STAT_FILL_W;
    frameBuffer.fillRect(fx0, fy, laneW, 2, STAT_PLATE_GREY);
    int fillWidth = (laneW * values[row]) / 100;
    if (fillWidth > 0) {
      uint16_t hi = iconColors[row] | STAT_HILITE_OR;
      frameBuffer.drawFastHLine(fx0, fy,     fillWidth, hi);
      frameBuffer.drawFastHLine(fx0, fy + 1, fillWidth, iconColors[row]);
    }

    // ---- Number text ----
    frameBuffer.fillRect(STAT_NUM_X + 1, boxY + 1, STAT_NUM_W - 2, STAT_NUM_H - 2, STAT_NUM_BG);
    frameBuffer.setTextSize(1);
    frameBuffer.setTextColor(TFT_WHITE);
    int digits = (values[row] >= 100) ? 3 : (values[row] >= 10 ? 2 : 1);
    int textW = digits * 6 - 1;
    int boxRight = STAT_NUM_X + STAT_NUM_W - 2;
    frameBuffer.setCursor(boxRight - textW, boxY + 1);
    frameBuffer.print(values[row]);
  }
}

void DisplayManager::composeMainScene(int hunger, int happiness, int energy,
                                      int petX, int petY, int petW, int petH,
                                      const uint16_t* petFrame, bool petFlip,
                                      int poopCount) {
  // 1. Background straight from PROGMEM into the canvas.
  for (int row = 0; row < SCREEN_HEIGHT; row++) {
    for (int col = 0; col < SCREEN_WIDTH; col++) {
      frameBuffer.drawPixel(col, row, pgm_read_word(&background_data_forest[row * SCREEN_WIDTH + col]));
    }
  }

  // 2. Poops (transparent-on-background) BEFORE the pet so the pet can overlap.
  {
    int poopXs[] = { 6, 104, 54 };
    int poopY = 104;
    for (int i = 0; i < poopCount && i < 3; i++) {
      for (int row = 0; row < 20; row++) {
        for (int col = 0; col < 20; col++) {
          uint16_t c = pgm_read_word(&poop_frame[row * 20 + col]);
          if (c != TFT_BLACK) frameBuffer.drawPixel(poopXs[i] + col, poopY + row, c);
        }
      }
    }
  }

  // 3. Pet sprite (transparent black), optionally horizontally mirrored.
  if (petFrame) {
    for (int row = 0; row < petH; row++) {
      for (int col = 0; col < petW; col++) {
        uint16_t c = pgm_read_word(&petFrame[row * petW + col]);
        if (c != TFT_BLACK) {
          int dstCol = petFlip ? (petW - 1 - col) : col;
          frameBuffer.drawPixel(petX + dstCol, petY + row, c);
        }
      }
    }
  }

  // 4. Status chrome + stat bars on top.
  composeStatusPanel(hunger, happiness, energy);
}

void DisplayManager::renderMainScene(int hunger, int happiness, int energy,
                                     int petX, int petY, int petW, int petH,
                                     const uint16_t* petFrame, bool petFlip, int poopCount) {
  composeMainScene(hunger, happiness, energy, petX, petY, petW, petH, petFrame, petFlip, poopCount);
  pushFrame();   // single blit -> no scanline wipe, no bar flicker
}

void DisplayManager::clearScreen() {
  // Clear to the forest background rather than a flat colour.
  drawBackground();
}

void DisplayManager::drawStatusPanelChrome() {
  // Static elements: draw once from forceFullRedraw, never on a stat tick.
  // NOTE: forceFullRedraw() calls drawBackground() (a single bulk 128x128
  // blit) immediately before this, so the top strip is ALREADY painted with
  // the forest. We intentionally do NOT re-erase it here — that redundant
  // per-region repaint was the bulk of the visible menu-exit lag.

  const uint16_t iconColors[3] = { STAT_HUNGER_COLOR, STAT_HAPPY_COLOR, STAT_ENERGY_COLOR };
  const bool isHeart[3] = { true, false, false };

  for (int row = 0; row < 3; row++) {
    int y = originY + STAT_ROW_Y0 + row * STAT_ROW_H;
    // Icon
    if (isHeart[row]) {
      int cx = originX + STAT_ICON_X + 3;
      tft.fillCircle(cx - 1, y + 2, 1, iconColors[row]);
      tft.fillCircle(cx + 1, y + 2, 1, iconColors[row]);
      tft.fillTriangle(cx - 2, y + 3, cx + 2, y + 3, cx, y + 5, iconColors[row]);
    } else {
      tft.fillCircle(originX + STAT_ICON_X + 3, y + 3, 3, iconColors[row]);
    }
    // ---- Grey plate (static parts of the DW-style bar) ----
    int px = originX + STAT_BAR_X;   // plate left
    int py = y;                      // plate top (6px tall)

    // Grey body between the borders.
    tft.fillRect(px + 1, py + 1, STAT_BAR_W - 2, STAT_PLATE_H - 2, STAT_PLATE_GREY);
    // Black top & bottom frame (inset 1px -> transparent corners).
    tft.drawFastHLine(px + 1, py, STAT_BAR_W - 2, STAT_FRAME_COLOR);
    tft.drawFastHLine(px + 1, py + STAT_PLATE_H - 1, STAT_BAR_W - 2, STAT_FRAME_COLOR);
    // Black left & right vertical borders (rows 1..4).
    tft.drawFastVLine(px, py + 1, STAT_PLATE_H - 2, STAT_FRAME_COLOR);
    tft.drawFastVLine(px + STAT_BAR_W - 1, py + 1, STAT_PLATE_H - 2, STAT_FRAME_COLOR);
    // White bevel on the left: 2px on the top & bottom body rows, 1px between.
    tft.drawFastHLine(px + 1, py + 1, 2, STAT_BEVEL_WHITE);
    tft.drawFastHLine(px + 1, py + STAT_PLATE_H - 2, 2, STAT_BEVEL_WHITE);
    tft.drawPixel(px + 1, py + 2, STAT_BEVEL_WHITE);
    tft.drawPixel(px + 1, py + 3, STAT_BEVEL_WHITE);
    // Dark-grey dividers near each end (full inner height).
    tft.drawFastVLine(px + STAT_LEFT_DIV,  py + 1, STAT_PLATE_H - 2, STAT_PLATE_DGREY);
    tft.drawFastVLine(px + STAT_RIGHT_DIV, py + 1, STAT_PLATE_H - 2, STAT_PLATE_DGREY);
    // Black borders of the 2px fill lane (rows 2..3).
    tft.drawFastVLine(px + STAT_FILL_BL, py + 2, 2, STAT_FRAME_COLOR);
    tft.drawFastVLine(px + STAT_FILL_BR, py + 2, 2, STAT_FRAME_COLOR);

    // Number box hugging the right end of the plate.
    int boxY = y + (STAT_PLATE_H - STAT_NUM_H) / 2;   // vertically centered (== y-1)
    tft.fillRect(originX + STAT_NUM_X, boxY, STAT_NUM_W, STAT_NUM_H, STAT_NUM_BG);
    tft.drawRect(originX + STAT_NUM_X, boxY, STAT_NUM_W, STAT_NUM_H, STAT_NUM_FRAME);
  }
}

void DisplayManager::drawStatBar(int row, uint16_t iconColor, uint16_t barColor, int value, bool isHeartUnused) {
  int y = originY + STAT_ROW_Y0 + row * STAT_ROW_H;
  int px = originX + STAT_BAR_X;

  // ---- Fill lane (dynamic) : 2px tall, inside the black lane borders ----
  int fx0 = px + STAT_FILL_X0;
  int fy  = y + 2;                       // top row of the 2px lane
  int laneW = STAT_FILL_W;               // total fillable width

  // Clear the lane to grey (empty portion of the bar reads as plate grey).
  tft.fillRect(fx0, fy, laneW, 2, STAT_PLATE_GREY);

  int fillWidth = (laneW * value) / 100;
  if (fillWidth > 0) {
    uint16_t hi = barColor | STAT_HILITE_OR;   // light shade for the top row
    tft.drawFastHLine(fx0, fy,     fillWidth, hi);        // l : light top
    tft.drawFastHLine(fx0, fy + 1, fillWidth, barColor);  // r : base bottom
  }

  // ---- Right-aligned number inside the box that hugs the plate ----
  int boxY = y + (STAT_PLATE_H - STAT_NUM_H) / 2;
  // Clear the box interior (keep its static grey frame).
  tft.fillRect(originX + STAT_NUM_X + 1, boxY + 1, STAT_NUM_W - 2, STAT_NUM_H - 2, STAT_NUM_BG);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  // Right-justify inside the box: ~6px per glyph (5 glyph + 1 spacing).
  int digits = (value >= 100) ? 3 : (value >= 10 ? 2 : 1);
  int textW = digits * 6 - 1;
  int boxRight = originX + STAT_NUM_X + STAT_NUM_W - 2;
  tft.setCursor(boxRight - textW, boxY + 1);
  tft.print(value);
}

void DisplayManager::drawMainScreen(int hunger, int happiness, int energy) {
  drawStatBar(0, STAT_HUNGER_COLOR, STAT_HUNGER_COLOR, hunger, true);
  drawStatBar(1, STAT_HAPPY_COLOR, STAT_HAPPY_COLOR, happiness, false);
  drawStatBar(2, STAT_ENERGY_COLOR, STAT_ENERGY_COLOR, energy, false);
}

void DisplayManager::clearTrail(int oldX, int newX, int y, int width, int height) {
  // Repaint the vacated strip with the forest background instead of a flat fill.
  if (newX > oldX) {
    drawBackgroundRegion(oldX, y, newX - oldX, height);
  } else if (newX < oldX) {
    drawBackgroundRegion(newX + width, y, oldX - newX, height);
  }
}

void DisplayManager::clearSpriteAt(int x, int y, int width, int height) {
  drawBackgroundRegion(x, y, width, height);
}

void DisplayManager::clearSpriteMargin(int oldX, int oldY, int newX, int newY, int w, int h) {
  // Repaint only the vacated horizontal band (from the x shift) and vertical
  // band (from the y shift). The rectangle where old and new boxes overlap is
  // NOT cleared -- the redraw covers it -- so there is no erase/redraw flicker.
  int dx = newX - oldX;
  int dy = newY - oldY;

  // Horizontal vacated strip (full old height so the corner is covered).
  if (dx > 0) {
    drawBackgroundRegion(oldX, oldY, dx, h);              // left edge exposed
  } else if (dx < 0) {
    drawBackgroundRegion(newX + w, oldY, -dx, h);         // right edge exposed
  }
  // Vertical vacated strip (full old width).
  if (dy > 0) {
    drawBackgroundRegion(oldX, oldY, w, dy);              // top edge exposed
  } else if (dy < 0) {
    drawBackgroundRegion(oldX, newY + h, w, -dy);         // bottom edge exposed
  }
}

void DisplayManager::drawTransparentImage(int x, int y, int width, int height, const uint16_t* frame, uint16_t transparentColor) {
  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      uint16_t color = pgm_read_word(&frame[row * width + col]);
      if (color != transparentColor) {
        tft.drawPixel( originX +x + col, originY + y + row, color);
      }
    }
  }
}

void DisplayManager::drawSprite(int x, int y, int width, int height, const uint16_t* frame) {
  // Composite into a plain RAM buffer read straight from PROGMEM (same pixel
  // path as the background). We deliberately DO NOT round-trip through
  // GFXcanvas16: its drawPixel/getBuffer byte-order does not match the RAM
  // drawRGBBitmap overload, which turned the pet purple/washed-out while the
  // (PROGMEM) background stayed correct.
  if (width > 96 || height > 96) return;
  static uint16_t packed[96 * 96];
  for (int row = 0; row < height; row++) {
    int sy = y + row;
    for (int col = 0; col < width; col++) {
      int sx = x + col;
      uint16_t bg = (sx >= 0 && sx < SCREEN_WIDTH && sy >= 0 && sy < SCREEN_HEIGHT)
                      ? pgm_read_word(&background_data_forest[sy * SCREEN_WIDTH + sx])
                      : TFT_BLACK;
      uint16_t color = pgm_read_word(&frame[row * width + col]);
      packed[row * width + col] = (color != TFT_BLACK) ? color : bg;
    }
  }
  pushRGBBuf(x, y, packed, width, height);
}

void DisplayManager::drawSpriteFlipped(int x, int y, int width, int height, const uint16_t* frame) {
  // Same canvas-free composite as drawSprite(), horizontally mirrored.
  if (width > 96 || height > 96) return;
  static uint16_t packed[96 * 96];
  for (int row = 0; row < height; row++) {
    int sy = y + row;
    for (int col = 0; col < width; col++) {
      int sx = x + col;
      uint16_t bg = (sx >= 0 && sx < SCREEN_WIDTH && sy >= 0 && sy < SCREEN_HEIGHT)
                      ? pgm_read_word(&background_data_forest[sy * SCREEN_WIDTH + sx])
                      : TFT_BLACK;
      uint16_t color = pgm_read_word(&frame[row * width + (width - 1 - col)]);
      packed[row * width + col] = (color != TFT_BLACK) ? color : bg;
    }
  }
  pushRGBBuf(x, y, packed, width, height);
}

void DisplayManager::drawPoops(int count) {
  int poopX[] = { 6, 104, 54 };
  int poopY = 104;
  for (int i = 0; i < count; i++) {
    drawTransparentImage(poopX[i], poopY, 20, 20, poop_frame, TFT_BLACK);
  }
}

void DisplayManager::drawProfileSprite(int x, int y, int width, int height, const uint16_t* frame, uint16_t transparentColor) {
  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      uint16_t color = pgm_read_word(&frame[row * width + col]);
      if (color != transparentColor) {
        tft.drawPixel(x + col, y + row, color);
      }
    }
  }
}

// ---- Shared DW grey-plate helpers -----------------------------------------
// A raised grey window: grey body, black frame, white top/left bevel and
// dark-grey bottom/right shadow (same language as the stat bars).
void DisplayManager::drawBevelPanel(int x, int y, int w, int h) {
  Adafruit_GFX* g = gfx ? gfx : &tft;   // buffered menu -> canvas, else panel
  g->fillRect(x, y, w, h, STAT_PLATE_GREY);
  g->drawRect(x, y, w, h, STAT_FRAME_COLOR);
  g->drawFastHLine(x + 1, y + 1, w - 2, STAT_BEVEL_WHITE);
  g->drawFastVLine(x + 1, y + 1, h - 2, STAT_BEVEL_WHITE);
  g->drawFastHLine(x + 1, y + h - 2, w - 2, STAT_PLATE_DGREY);
  g->drawFastVLine(x + w - 2, y + 1, h - 2, STAT_PLATE_DGREY);
}

// One menu row as a beveled plate. Selected rows are a lighter (raised)
// plate with dark text and a '>' arrow; others are plate grey with light
// text. Optional right-aligned suffix (e.g. "ON"/"OFF").
void DisplayManager::drawMenuRow(int x, int y, int w, int h, const char* label, bool selected, const char* suffix) {
  Adafruit_GFX* g = gfx ? gfx : &tft;   // buffered menu -> canvas, else panel
  uint16_t body   = selected ? MENU_PLATE_LGREY : STAT_PLATE_GREY;
  uint16_t light  = selected ? STAT_BEVEL_WHITE : MENU_PLATE_LGREY;
  uint16_t textCol = selected ? MENU_TEXT_DARK : MENU_TEXT_LIGHT;

  g->fillRect(x, y, w, h, body);
  g->drawRect(x, y, w, h, STAT_FRAME_COLOR);
  g->drawFastHLine(x + 1, y + 1, w - 2, light);
  g->drawFastVLine(x + 1, y + 1, h - 2, light);
  g->drawFastHLine(x + 1, y + h - 2, w - 2, STAT_PLATE_DGREY);
  g->drawFastVLine(x + w - 2, y + 1, h - 2, STAT_PLATE_DGREY);

  g->setTextSize(1);
  g->setTextColor(textCol);
  g->setCursor(x + 5, y + (h - 7) / 2);
  g->print(selected ? "> " : "  ");
  g->print(label);
  if (suffix) {
    // Right-align the suffix inside the row.
    int sw = (int)strlen(suffix) * 6;
    g->setCursor(x + w - sw - 5, y + (h - 7) / 2);
    g->print(suffix);
  }
}

void DisplayManager::drawMenu(const char* title, const char* const* items, int itemCount, int selectedIndex) {
  // Grey Digimon-World-style window composited OFFSCREEN and pushed in one
  // blit, so navigating (left/right/ok) no longer makes the panel blink
  // (disappear + rebuild) on every keypress. All coords are canvas-space
  // (NO origin offset); pushFrame() applies the offset once.
  seedBufferBackground();
  gfx = &frameBuffer;                    // route the shared helpers to the canvas

  const int pnlX = 8,   pnlY = 6;
  const int pnlW = 112, pnlH = 116;
  drawBevelPanel(pnlX, pnlY, pnlW, pnlH);

  // Title
  frameBuffer.setTextSize(1);
  frameBuffer.setTextColor(MENU_TEXT_DARK);
  frameBuffer.setCursor(pnlX + 6, pnlY + 5);
  frameBuffer.print(title);
  frameBuffer.drawFastHLine(pnlX + 4, pnlY + 15, pnlW - 8, STAT_FRAME_COLOR);

  // Item rows as beveled plates. Height/pitch adapt so up to 5 items fit.
  const int itemX  = pnlX + 4;
  const int itemW  = pnlW - 8;
  const int listY0 = pnlY + 20;
  const int listH  = pnlH - (listY0 - pnlY) - 6;
  int pitch = (itemCount > 0) ? (listH / itemCount) : listH;
  if (pitch > 20) pitch = 20;
  const int itemH = pitch - 3;

  for (int i = 0; i < itemCount; i++) {
    drawMenuRow(itemX, listY0 + i * pitch, itemW, itemH,
                items[i], i == selectedIndex);
  }

  gfx = nullptr;                         // restore default (panel) target
  pushFrame();                           // single blit -> no blink
}

void DisplayManager::drawStatsPage(const char* name, int hp, int maxHp, int ap, int dp,
                                   const uint16_t* profileFrame, int profileSize) {
  // Buffered composite (single blit) -> no blink.
  seedBufferBackground();
  gfx = &frameBuffer;

  const int pnlX = 8,   pnlY = 6;
  const int pnlW = 112, pnlH = 116;
  drawBevelPanel(pnlX, pnlY, pnlW, pnlH);
  frameBuffer.setTextSize(1);

  frameBuffer.setTextColor(MENU_TEXT_DARK);
  frameBuffer.setCursor(pnlX + 6, pnlY + 5);
  frameBuffer.print("Stats");
  frameBuffer.drawFastHLine(pnlX + 4, pnlY + 15, pnlW - 8, STAT_FRAME_COLOR);

  // Active Digimon's profile sprite composited into the canvas
  // (transparent-on-bg). Falls back to no image if none supplied.
  int pw = (profileFrame && profileSize > 0) ? profileSize : 0;
  if (pw > 0) {
    int sx = pnlX + 8, sy = pnlY + 20;
    for (int row = 0; row < pw; row++) {
      for (int col = 0; col < pw; col++) {
        uint16_t c = pgm_read_word(&profileFrame[row * pw + col]);
        if (c != TFT_BLACK) frameBuffer.drawPixel(sx + col, sy + row, c);
      }
    }
  }

  frameBuffer.setTextColor(MENU_TEXT_DARK);
  frameBuffer.setCursor(pnlX + (pw > 0 ? pw : 0) + 10, pnlY + 20 + ((pw > 0 ? pw : 20) / 2) - 4);
  frameBuffer.printf("%s", name);

  frameBuffer.setCursor(pnlX + 8, pnlY + 60);
  frameBuffer.printf("HP: %d/%d", hp, maxHp);
  frameBuffer.setCursor(pnlX + 8, pnlY + 76);
  frameBuffer.printf("AP: %d", ap);
  frameBuffer.setCursor(pnlX + 8, pnlY + 92);
  frameBuffer.printf("DP: %d", dp);

  frameBuffer.setCursor(pnlX + 8, pnlY + pnlH - 14);
  frameBuffer.print("OK: Back");

  gfx = nullptr;
  pushFrame();
}

void DisplayManager::drawDigivolutionPage(const char* currentName, const char* nextName) {
  seedBufferBackground();
  gfx = &frameBuffer;

  const int pnlX = 8,   pnlY = 6;
  const int pnlW = 112, pnlH = 116;
  drawBevelPanel(pnlX, pnlY, pnlW, pnlH);
  frameBuffer.setTextSize(1);

  frameBuffer.setTextColor(MENU_TEXT_DARK);
  frameBuffer.setCursor(pnlX + 6, pnlY + 5);
  frameBuffer.print("Digivolution");
  frameBuffer.drawFastHLine(pnlX + 4, pnlY + 15, pnlW - 8, STAT_FRAME_COLOR);

  frameBuffer.setTextColor(MENU_TEXT_DARK);
  frameBuffer.setCursor(pnlX + 8, pnlY + 30);
  frameBuffer.printf("Now: %s", currentName ? currentName : "?");
  frameBuffer.setCursor(pnlX + 8, pnlY + 50);
  frameBuffer.printf("Next: %s", nextName ? nextName : "?");

  frameBuffer.setCursor(pnlX + 8, pnlY + 76);
  frameBuffer.print("OK: Digivolve!");

  frameBuffer.setCursor(pnlX + 8, pnlY + pnlH - 14);
  frameBuffer.print("(OK also exits)");

  gfx = nullptr;
  pushFrame();
}

void DisplayManager::drawSettings(int selectedIndex, bool isMuted) {
  // Buffered composite (single blit) so toggling Sound / moving the selection
  // doesn't blink the whole panel.
  seedBufferBackground();
  gfx = &frameBuffer;

  const int pnlX = 8,   pnlY = 30;
  const int pnlW = 112, pnlH = 68;
  drawBevelPanel(pnlX, pnlY, pnlW, pnlH);

  const int itemX = pnlX + 4;
  const int itemW = pnlW - 8;
  const int itemH = 15;
  const int itemY0 = pnlY + 8;
  const int itemPitch = 20;

  // Row 0: Sound with ON/OFF suffix; Row 1: Back.
  drawMenuRow(itemX, itemY0, itemW, itemH, "Sound",
              selectedIndex == 0, isMuted ? "OFF" : "ON");
  drawMenuRow(itemX, itemY0 + itemPitch, itemW, itemH, "Reset",
              selectedIndex == 1, nullptr);
  drawMenuRow(itemX, itemY0 + itemPitch * 2, itemW, itemH, "Back",
              selectedIndex == 2, nullptr);
  gfx = nullptr;
  pushFrame();
}

void DisplayManager::drawGameOver(int selectedIndex) {
  seedBufferBackground();
  gfx = &frameBuffer;

  const int pnlX = 8,   pnlY = 34;
  const int pnlW = 112, pnlH = 60;
  drawBevelPanel(pnlX, pnlY, pnlW, pnlH);

  // "R.I.P." title, centered near the top of the panel.
  frameBuffer.setTextSize(1);
  frameBuffer.setTextColor(TFT_RED);
  frameBuffer.setCursor(pnlX + (pnlW - 6 * 6) / 2, pnlY + 6);
  frameBuffer.print("R.I.P.");

  const char* options[] = { "Restart", "Leave" };
  const int itemX = pnlX + 4;
  const int itemW = pnlW - 8;
  const int itemH = 14;
  const int itemY0 = pnlY + 20;
  const int itemPitch = 17;
  for (int i = 0; i < 2; i++) {
    drawMenuRow(itemX, itemY0 + i * itemPitch, itemW, itemH,
                options[i], i == selectedIndex);
  }

  gfx = nullptr;
  pushFrame();
}

void DisplayManager::drawMinigameUI(int score, int timeLeft, int treatX, int treatY, int oldTreatX, int oldTreatY) {
  if (oldTreatY > 0) {
    drawBackgroundRegion(oldTreatX, oldTreatY, 16, 16);
  }

  drawBackgroundRegion(0, 0, SCREEN_WIDTH, 14);
  tft.fillRoundRect( originX +2, originY + 0, SCREEN_WIDTH - 4, 13, 3, TFT_BLACK);
  tft.drawRoundRect( originX +2, originY + 0, SCREEN_WIDTH - 4, 13, 3, TFT_WHITE);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor( originX +6, originY + 3);
  tft.printf("Sc:%d T:%ds", score, timeLeft);

  if (treatY > 0) {
    drawTransparentImage(treatX, treatY, 16, 16, treat_frame, TFT_BLACK);
  }
}

void DisplayManager::drawMinigameTopBar(int score, int timeLeft) {
  drawBackgroundRegion(0, 0, SCREEN_WIDTH, 14);
  tft.fillRoundRect( originX +2, originY + 0, SCREEN_WIDTH - 4, 13, 3, TFT_BLACK);
  tft.drawRoundRect( originX +2, originY + 0, SCREEN_WIDTH - 4, 13, 3, TFT_WHITE);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor( originX +6, originY + 3);
  tft.printf("Sc:%d T:%ds", score, timeLeft);
}

void DisplayManager::updateMinigameTreat(int treatX, int treatY, int oldTreatX, int oldTreatY) {
  if (oldTreatY > 0) {
    drawBackgroundRegion(oldTreatX, oldTreatY, 16, 16);
  }
  if (treatY > 0) {
    drawTransparentImage(treatX, treatY, 16, 16, treat_frame, TFT_BLACK);
  }
}

void DisplayManager::enterScreensaver() {
  tft.fillScreen(TFT_BLACK);
}

void DisplayManager::exitScreensaver(int hunger, int happiness, int energy) {
  forceFullRedraw(hunger, happiness, energy); // repaint everything
}
