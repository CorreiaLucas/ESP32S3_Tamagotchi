#include "DisplayManager.h"
#include "Sprites.h"

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
  drawMainScreen(hunger, happiness, energy);
}

void DisplayManager::clearScreen() {
  // Clear to the forest background rather than a flat colour.
  drawBackground();
}

void DisplayManager::drawMainScreen(int hunger, int happiness, int energy) {
  // Repaint the forest behind the status area first, then lay a translucent-
  // looking dark panel over it so the white text/bars stay readable on the
  // busy background (matches the menu/settings panel style).
  drawBackgroundRegion(0, 0, SCREEN_WIDTH, 34);
  tft.fillRoundRect( originX +2, originY + 1, SCREEN_WIDTH - 4, 32, 4, TFT_BLACK);
  tft.drawRoundRect( originX +2, originY + 1, SCREEN_WIDTH - 4, 32, 4, TFT_WHITE);
  tft.setTextSize(1);

  if (hunger <= 20 || happiness <= 20) {
    tft.setTextColor(TFT_RED);
  } else {
    tft.setTextColor(TFT_WHITE);
  }

  tft.setCursor( originX +6, originY + 4);
  tft.printf("H:%d Hap:%d", hunger, happiness);

  tft.setTextColor(TFT_WHITE);
  tft.setCursor( originX +6, originY + 16);
  tft.print("E:");

  // Energy bar: label ~12px, bar fills the rest.
  tft.drawRect( originX +22, originY + 15, 100, 10, TFT_WHITE);
  int fillWidth = (96 * energy) / 100;
  if (fillWidth > 0) {
    tft.fillRect( originX +24, originY + 17, fillWidth, 6, TFT_BLUE);
  }
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

void DisplayManager::drawMenu(int selectedIndex) {
  const char* menuItems[] = { "Feed", "Play", "Sleep", "Clean", "Settings", "Exit" };

  tft.fillRoundRect( originX +8, originY + 6, 112, 116, 6, TFT_BLACK);
  tft.drawRoundRect( originX +8, originY + 6, 112, 116, 6, TFT_WHITE);
  tft.setTextSize(1);

  for (int i = 0; i < 6; i++) {
    if (i == selectedIndex) {
      tft.setTextColor(TFT_SAGE_GREEN);
      tft.setCursor( originX +18, originY + 16 + (i * 17));
      tft.print("> ");
    } else {
      tft.setTextColor(TFT_WHITE);
      tft.setCursor( originX +18, originY + 16 + (i * 17));
      tft.print("  ");
    }
    tft.println(menuItems[i]);
  }
}

void DisplayManager::drawSettings(int selectedIndex, bool isMuted) {
  tft.fillRoundRect( originX +8, originY + 30, 112, 68, 6, TFT_BLACK);
  tft.drawRoundRect( originX +8, originY + 30, 112, 68, 6, TFT_WHITE);
  tft.setTextSize(1);

  const char* options[] = { "Sound: ", "Back" };

  for (int i = 0; i < 2; i++) {
    if (i == selectedIndex) {
      tft.setTextColor(TFT_SAGE_GREEN);
      tft.setCursor( originX +18, originY + 44 + (i * 20));
      tft.print("> ");
    } else {
      tft.setTextColor(TFT_WHITE);
      tft.setCursor( originX +18, originY + 44 + (i * 20));
      tft.print("  ");
    }
    tft.print(options[i]);

    if (i == 0) {
      tft.println(isMuted ? "OFF" : "ON");
    } else {
      tft.println();
    }
  }
}

void DisplayManager::drawGameOver(int selectedIndex) {
  tft.fillRoundRect( originX +8, originY + 34, 112, 60, 6, TFT_BLACK);
  tft.drawRoundRect( originX +8, originY + 34, 112, 60, 6, TFT_WHITE);
  tft.setTextSize(1);

  tft.setTextColor(TFT_RED);
  tft.setCursor( originX +46, originY + 42);
  tft.print("R.I.P.");

  const char* options[] = { "Restart", "Leave" };

  for (int i = 0; i < 2; i++) {
    if (i == selectedIndex) {
      tft.setTextColor(TFT_SAGE_GREEN);
      tft.setCursor( originX +24, originY + 60 + (i * 16));
      tft.print("> ");
    } else {
      tft.setTextColor(TFT_WHITE);
      tft.setCursor( originX +24, originY + 60 + (i * 16));
      tft.print("  ");
    }
    tft.println(options[i]);
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
