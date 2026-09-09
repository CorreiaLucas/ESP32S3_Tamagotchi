#include "DisplayManager.h"
#include "Sprites.h"
#include <string.h>

DisplayManager::DisplayManager()
#ifdef SIMULATOR_BUILD
  : tft(&SPI, TFT_CS, TFT_DC, TFT_RST), petBuffer(48, 48)
#else
  : tft(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, TFT_CS, TFT_DC, TFT_RST), petBuffer(48, 48)
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
    tft.drawRGBBitmap(originX + x0, originY + sy, rowBuf, rowW, 1);
  }
}

void DisplayManager::forceFullRedraw(int hunger, int happiness, int energy) {
  drawBackground();
  drawStatusPanelChrome();
  drawMainScreen(hunger, happiness, energy);
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
  // Seed the buffer with the forest pixels behind the pet so transparent
  // sprite pixels reveal the background instead of a flat colour.
  for (int row = 0; row < height; row++) {
    int sy = y + row;
    for (int col = 0; col < width; col++) {
      int sx = x + col;
      uint16_t bg = (sx >= 0 && sx < SCREEN_WIDTH && sy >= 0 && sy < SCREEN_HEIGHT)
                      ? pgm_read_word(&background_data_forest[sy * SCREEN_WIDTH + sx])
                      : TFT_BLACK;
      petBuffer.drawPixel(col, row, bg);
    }
  }
  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      uint16_t color = pgm_read_word(&frame[row * width + col]);
      if (color != TFT_BLACK) {
        petBuffer.drawPixel(col, row, color);
      }
    }
  }
  tft.drawRGBBitmap( originX +x, originY + y, petBuffer.getBuffer(), width, height);
}

void DisplayManager::drawSpriteFlipped(int x, int y, int width, int height, const uint16_t* frame) {
  // Seed with the forest background behind the pet (mirrored draw follows).
  for (int row = 0; row < height; row++) {
    int sy = y + row;
    for (int col = 0; col < width; col++) {
      int sx = x + col;
      uint16_t bg = (sx >= 0 && sx < SCREEN_WIDTH && sy >= 0 && sy < SCREEN_HEIGHT)
                      ? pgm_read_word(&background_data_forest[sy * SCREEN_WIDTH + sx])
                      : TFT_BLACK;
      petBuffer.drawPixel(col, row, bg);
    }
  }
  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      uint16_t color = pgm_read_word(&frame[row * width + col]);
      if (color != TFT_BLACK) {
        petBuffer.drawPixel(width - 1 - col, row, color);
      }
    }
  }
  tft.drawRGBBitmap( originX +x, originY + y, petBuffer.getBuffer(), width, height);
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
  tft.fillRect(x, y, w, h, STAT_PLATE_GREY);
  tft.drawRect(x, y, w, h, STAT_FRAME_COLOR);
  tft.drawFastHLine(x + 1, y + 1, w - 2, STAT_BEVEL_WHITE);
  tft.drawFastVLine(x + 1, y + 1, h - 2, STAT_BEVEL_WHITE);
  tft.drawFastHLine(x + 1, y + h - 2, w - 2, STAT_PLATE_DGREY);
  tft.drawFastVLine(x + w - 2, y + 1, h - 2, STAT_PLATE_DGREY);
}

// One menu row as a beveled plate. Selected rows are a lighter (raised)
// plate with dark text and a '>' arrow; others are plate grey with light
// text. Optional right-aligned suffix (e.g. "ON"/"OFF").
void DisplayManager::drawMenuRow(int x, int y, int w, int h, const char* label, bool selected, const char* suffix) {
  uint16_t body   = selected ? MENU_PLATE_LGREY : STAT_PLATE_GREY;
  uint16_t light  = selected ? STAT_BEVEL_WHITE : MENU_PLATE_LGREY;
  uint16_t textCol = selected ? MENU_TEXT_DARK : MENU_TEXT_LIGHT;

  tft.fillRect(x, y, w, h, body);
  tft.drawRect(x, y, w, h, STAT_FRAME_COLOR);
  tft.drawFastHLine(x + 1, y + 1, w - 2, light);
  tft.drawFastVLine(x + 1, y + 1, h - 2, light);
  tft.drawFastHLine(x + 1, y + h - 2, w - 2, STAT_PLATE_DGREY);
  tft.drawFastVLine(x + w - 2, y + 1, h - 2, STAT_PLATE_DGREY);

  tft.setTextSize(1);
  tft.setTextColor(textCol);
  tft.setCursor(x + 5, y + (h - 7) / 2);
  tft.print(selected ? "> " : "  ");
  tft.print(label);
  if (suffix) {
    // Right-align the suffix inside the row.
    int sw = (int)strlen(suffix) * 6;
    tft.setCursor(x + w - sw - 5, y + (h - 7) / 2);
    tft.print(suffix);
  }
}

void DisplayManager::drawMenu(const char* title, const char* const* items, int itemCount, int selectedIndex) {
  // Grey Digimon-World-style window (same bevel language as drawSettings /
  // the stat bars): a raised grey panel with a title bar, then one beveled
  // grey plate per item. The selected row is a lighter raised plate with a
  // '>' arrow and dark text.
  const int pnlX = originX + 8,  pnlY = originY + 6;
  const int pnlW = 112,          pnlH = 116;
  drawBevelPanel(pnlX, pnlY, pnlW, pnlH);

  // Title
  tft.setTextSize(1);
  tft.setTextColor(MENU_TEXT_DARK);
  tft.setCursor(pnlX + 6, pnlY + 5);
  tft.print(title);
  tft.drawFastHLine(pnlX + 4, pnlY + 15, pnlW - 8, STAT_FRAME_COLOR);

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
}

void DisplayManager::drawStatsPage(const char* name, int hp, int maxHp, int ap, int dp) {
  const int pnlX = originX + 8,  pnlY = originY + 6;
  const int pnlW = 112,          pnlH = 116;
  drawBevelPanel(pnlX, pnlY, pnlW, pnlH);
  tft.setTextSize(1);

  tft.setTextColor(MENU_TEXT_DARK);
  tft.setCursor(pnlX + 6, pnlY + 5);
  tft.print("Stats");
  tft.drawFastHLine(pnlX + 4, pnlY + 15, pnlW - 8, STAT_FRAME_COLOR);

  drawProfileSprite(pnlX + 8, pnlY + 20, profileSpriteWidth, profileSpriteHeight, profileSprites[0], TFT_BLACK);
  
  tft.setTextColor(MENU_TEXT_DARK);
  tft.setCursor(pnlX + profileSpriteWidth + 10, pnlY + 20 + (profileSpriteHeight / 2) - 4);
  tft.printf("%s", name);
  
  tft.setCursor(pnlX + 8, pnlY + 60);
  tft.printf("HP: %d/%d", hp, maxHp);

  tft.setCursor(pnlX + 8, pnlY + 60);
  tft.printf("HP: %d/%d", hp, maxHp);
  tft.setCursor(pnlX + 8, pnlY + 76);
  tft.printf("AP: %d", ap);
  tft.setCursor(pnlX + 8, pnlY + 92);
  tft.printf("DP: %d", dp);

  tft.setCursor(pnlX + 8, pnlY + pnlH - 14);
  tft.print("OK: Back");
}

void DisplayManager::drawDigivolutionPage() {
  const int pnlX = originX + 8,  pnlY = originY + 6;
  const int pnlW = 112,          pnlH = 116;
  drawBevelPanel(pnlX, pnlY, pnlW, pnlH);
  tft.setTextSize(1);

  tft.setTextColor(MENU_TEXT_DARK);
  tft.setCursor(pnlX + 6, pnlY + 5);
  tft.print("Digivolution");
  tft.drawFastHLine(pnlX + 4, pnlY + 15, pnlW - 8, STAT_FRAME_COLOR);

  tft.setTextColor(MENU_TEXT_DARK);
  tft.setCursor(pnlX + 8, pnlY + 50);
  tft.print("Coming soon");

  tft.setCursor(pnlX + 8, pnlY + pnlH - 14);
  tft.print("OK: Back");
}

void DisplayManager::drawSettings(int selectedIndex, bool isMuted) {
  const int pnlX = originX + 8,  pnlY = originY + 30;
  const int pnlW = 112,          pnlH = 68;
  drawBevelPanel(pnlX, pnlY, pnlW, pnlH);

  const int itemX = pnlX + 4;
  const int itemW = pnlW - 8;
  const int itemH = 15;
  const int itemY0 = pnlY + 8;
  const int itemPitch = 20;

  // Row 0: Sound with ON/OFF suffix; Row 1: Back.
  drawMenuRow(itemX, itemY0, itemW, itemH, "Sound",
              selectedIndex == 0, isMuted ? "OFF" : "ON");
  drawMenuRow(itemX, itemY0 + itemPitch, itemW, itemH, "Back",
              selectedIndex == 1, nullptr);
}

void DisplayManager::drawGameOver(int selectedIndex) {
  const int pnlX = originX + 8,  pnlY = originY + 34;
  const int pnlW = 112,          pnlH = 60;
  drawBevelPanel(pnlX, pnlY, pnlW, pnlH);

  // "R.I.P." title, centered near the top of the panel.
  tft.setTextSize(1);
  tft.setTextColor(TFT_RED);
  tft.setCursor(pnlX + (pnlW - 6 * 6) / 2, pnlY + 6);
  tft.print("R.I.P.");

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
