#include "DisplayManager.h"
#include "Sprites.h"
#include <string.h>

DisplayManager::DisplayManager()
#ifdef SIMULATOR_BUILD
  : tft(&SPI, TFT_CS, TFT_DC, TFT_RST), petBuffer(48, 48)
#else
  : tft(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, TFT_CS, TFT_DC, TFT_RST), petBuffer(48, 48)
#endif
{}

void DisplayManager::begin() {
  #ifdef SIMULATOR_BUILD
    // Wokwi ST7789 stand-in. Its native panel is 240x240, but the real
    // hardware is a 128x128 SSD1351. To make the simulator visually match
    // the real device, we frame a CENTERED 128x128 window inside the panel
    // via a draw-origin offset (originX/originY). Every DisplayManager draw
    // adds that offset; on real hardware the offset is 0.
    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    tft.init(240, 240);
    originX = (240 - SCREEN_WIDTH) / 2;   // 56
    originY = (240 - SCREEN_HEIGHT) / 2;  // 56
  #else
    // Real hardware: Waveshare SSD1351 128x128. Draw origin is (0,0).
    tft.begin();
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
  for (int row = 0; row < h; row++) {
    int sy = y + row;
    if (sy < 0 || sy >= SCREEN_HEIGHT) continue;
    for (int col = 0; col < w; col++) {
      int sx = x + col;
      if (sx < 0 || sx >= SCREEN_WIDTH) continue;
      uint16_t color = pgm_read_word(&background_data_forest[sy * SCREEN_WIDTH + sx]);
      tft.drawPixel( originX +sx, originY + sy, color);
    }
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
  // Erase a generous strip across the FULL width so no stale pixels from a
  // previous (taller/wider) bar layout survive. 32px comfortably covers the
  // old 3-row layout; the forest background is repainted underneath.
  drawBackgroundRegion(0, 0, SCREEN_WIDTH, 34);

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
void DisplayManager::drawMenuRow(int x, int y, int w, int h, const char* label,
                                 bool selected, const char* suffix) {
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
  tft.fillRoundRect(originX + 8, originY + 6, 112, 116, 6, TFT_BLACK);
  tft.drawRoundRect(originX + 8, originY + 6, 112, 116, 6, TFT_WHITE);
  tft.setTextSize(1);

  for (int i = 0; i < itemCount; i++) {
    if (i == selectedIndex) {
      tft.setTextColor(TFT_SAGE_GREEN);
      tft.setCursor(originX + 18, originY + 16 + (i * 17));
      tft.print("> ");
    } else {
      tft.setTextColor(TFT_WHITE);
      tft.setCursor(originX + 18, originY + 16 + (i * 17));
      tft.print("  ");
    }

    tft.println(items[i]);
  }
}

void DisplayManager::drawStatsPage(int hp, int maxHp, int ap, int dp) {
  tft.fillRoundRect(originX + 8, originY + 6, 112, 116, 6, TFT_BLACK);
  tft.drawRoundRect(originX + 8, originY + 6, 112, 116, 6, TFT_WHITE);
  tft.setTextSize(1);

  tft.setTextColor(TFT_SAGE_GREEN);
  tft.setCursor(originX + 14, originY + 10);
  tft.print("Stats");
  tft.drawFastHLine(originX + 12, originY + 20, 104, TFT_WHITE);

  tft.setTextColor(TFT_WHITE);

  tft.setCursor(originX + 16, originY + 30);
  tft.printf("HP: %d/%d", hp, maxHp);

  tft.setCursor(originX + 16, originY + 46);
  tft.printf("AP: %d", ap);

  tft.setCursor(originX + 16, originY + 62);
  tft.printf("DP: %d", dp);

  tft.setTextColor(TFT_SAGE_GREEN);
  tft.setCursor(originX + 16, originY + 106);
  tft.print("OK: Back");
}

void DisplayManager::drawDigivolutionPage() {
  tft.fillRoundRect(originX + 8, originY + 6, 112, 116, 6, TFT_BLACK);
  tft.drawRoundRect(originX + 8, originY + 6, 112, 116, 6, TFT_WHITE);
  tft.setTextSize(1);

  tft.setTextColor(TFT_SAGE_GREEN);
  tft.setCursor(originX + 14, originY + 10);
  tft.print("Digivolution");
  tft.drawFastHLine(originX + 12, originY + 20, 104, TFT_WHITE);

  tft.setTextColor(TFT_WHITE);
  tft.setCursor(originX + 16, originY + 55);
  tft.print("Coming soon");

  tft.setTextColor(TFT_SAGE_GREEN);
  tft.setCursor(originX + 16, originY + 106);
  tft.print("OK: Back");
}

void DisplayManager::drawSettings(int selectedIndex, bool isMuted) {
  const int PX = originX + 8,  PY = originY + 30;
  const int PW = 112,          PH = 68;
  drawBevelPanel(PX, PY, PW, PH);

  const int ITEM_X = PX + 4;
  const int ITEM_W = PW - 8;
  const int ITEM_H = 15;
  const int ITEM_Y0 = PY + 8;
  const int ITEM_PITCH = 20;

  // Row 0: Sound with ON/OFF suffix; Row 1: Back.
  drawMenuRow(ITEM_X, ITEM_Y0, ITEM_W, ITEM_H, "Sound",
              selectedIndex == 0, isMuted ? "OFF" : "ON");
  drawMenuRow(ITEM_X, ITEM_Y0 + ITEM_PITCH, ITEM_W, ITEM_H, "Back",
              selectedIndex == 1, nullptr);
}

void DisplayManager::drawGameOver(int selectedIndex) {
  const int PX = originX + 8,  PY = originY + 34;
  const int PW = 112,          PH = 60;
  drawBevelPanel(PX, PY, PW, PH);

  // "R.I.P." title, centered near the top of the panel.
  tft.setTextSize(1);
  tft.setTextColor(TFT_RED);
  tft.setCursor(PX + (PW - 6 * 6) / 2, PY + 6);
  tft.print("R.I.P.");

  const char* options[] = { "Restart", "Leave" };
  const int ITEM_X = PX + 4;
  const int ITEM_W = PW - 8;
  const int ITEM_H = 14;
  const int ITEM_Y0 = PY + 20;
  const int ITEM_PITCH = 17;
  for (int i = 0; i < 2; i++) {
    drawMenuRow(ITEM_X, ITEM_Y0 + i * ITEM_PITCH, ITEM_W, ITEM_H,
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
