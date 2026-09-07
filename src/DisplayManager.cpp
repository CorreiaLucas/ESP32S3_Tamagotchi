#include "DisplayManager.h"
#include "Sprites.h"

DisplayManager::DisplayManager()
#ifdef SIMULATOR_BUILD
  : tft(&SPI, TFT_CS, TFT_DC, TFT_RST), petBuffer(90, 90)
#else
  : tft(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, TFT_CS, TFT_DC, TFT_RST), petBuffer(90, 90)
#endif
{}

void DisplayManager::begin() {
  #ifdef SIMULATOR_BUILD
    // Wokwi ST7789 stand-in. Its native panel is 240x240; we draw our
    // 128x128 UI into the top-left corner. This lets us validate logic in
    // simulation before the real SSD1351 hardware arrives.
    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    tft.init(240, 240);
  #else
    // Real hardware: Waveshare SSD1351 128x128.
    tft.begin();
  #endif
  // Rotation 2 = 180deg. The Wokwi ST7789 stand-in renders its origin at
  // the opposite corner from what our 128x128 UI layout assumes, so
  // rotation 0 shows everything upside-down. This flips the whole panel.
  tft.setRotation(2);
  tft.fillScreen(TFT_SAGE_GREEN);
}

void DisplayManager::forceFullRedraw(int hunger, int happiness, int energy) {
  tft.fillScreen(TFT_SAGE_GREEN);
  drawMainScreen(hunger, happiness, energy);
}

void DisplayManager::clearScreen() {
  tft.fillScreen(TFT_SAGE_GREEN);
}

void DisplayManager::drawMainScreen(int hunger, int happiness, int energy) {
  // Top status area, full 128 width.
  tft.fillRect(0, 0, SCREEN_WIDTH, 34, TFT_SAGE_GREEN);
  tft.setTextSize(1);

  if (hunger <= 20 || happiness <= 20) {
    tft.setTextColor(TFT_RED);
  } else {
    tft.setTextColor(TFT_WHITE);
  }

  tft.setCursor(2, 2);
  tft.printf("H:%d Hap:%d", hunger, happiness);

  tft.setTextColor(TFT_WHITE);
  tft.setCursor(2, 14);
  tft.print("E:");

  // Energy bar: label ~12px, bar fills the rest.
  tft.drawRect(18, 13, 108, 10, TFT_WHITE);
  int fillWidth = (104 * energy) / 100;
  if (fillWidth > 0) {
    tft.fillRect(20, 15, fillWidth, 6, TFT_BLUE);
  }
}

void DisplayManager::clearTrail(int oldX, int newX, int y, int width, int height) {
  if (newX > oldX) {
    tft.fillRect(oldX, y, newX - oldX, height, TFT_SAGE_GREEN);
  } else if (newX < oldX) {
    tft.fillRect(newX + width, y, oldX - newX, height, TFT_SAGE_GREEN);
  }
}

void DisplayManager::drawTransparentImage(int x, int y, int width, int height, const uint16_t* frame, uint16_t transparentColor) {
  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      uint16_t color = pgm_read_word(&frame[row * width + col]);
      if (color != transparentColor) {
        tft.drawPixel(x + col, y + row, color);
      }
    }
  }
}

void DisplayManager::drawSprite(int x, int y, int width, int height, const uint16_t* frame) {
  petBuffer.fillScreen(TFT_SAGE_GREEN);
  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      uint16_t color = pgm_read_word(&frame[row * width + col]);
      if (color != TFT_BLACK) {
        petBuffer.drawPixel(col, row, color);
      }
    }
  }
  tft.drawRGBBitmap(x, y, petBuffer.getBuffer(), width, height);
}

void DisplayManager::drawSpriteFlipped(int x, int y, int width, int height, const uint16_t* frame) {
  petBuffer.fillScreen(TFT_SAGE_GREEN);

  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      uint16_t color = pgm_read_word(&frame[row * width + col]);
      if (color != TFT_BLACK) {
        petBuffer.drawPixel(width - 1 - col, row, color);
      }
    }
  }
  tft.drawRGBBitmap(x, y, petBuffer.getBuffer(), width, height);
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

  tft.fillRoundRect(8, 6, 112, 116, 6, TFT_BLACK);
  tft.drawRoundRect(8, 6, 112, 116, 6, TFT_WHITE);
  tft.setTextSize(1);

  for (int i = 0; i < 6; i++) {
    if (i == selectedIndex) {
      tft.setTextColor(TFT_SAGE_GREEN);
      tft.setCursor(18, 16 + (i * 17));
      tft.print("> ");
    } else {
      tft.setTextColor(TFT_WHITE);
      tft.setCursor(18, 16 + (i * 17));
      tft.print("  ");
    }
    tft.println(menuItems[i]);
  }
}

void DisplayManager::drawSettings(int selectedIndex, bool isMuted) {
  tft.fillRoundRect(8, 30, 112, 68, 6, TFT_BLACK);
  tft.drawRoundRect(8, 30, 112, 68, 6, TFT_WHITE);
  tft.setTextSize(1);

  const char* options[] = { "Sound: ", "Back" };

  for (int i = 0; i < 2; i++) {
    if (i == selectedIndex) {
      tft.setTextColor(TFT_SAGE_GREEN);
      tft.setCursor(18, 44 + (i * 20));
      tft.print("> ");
    } else {
      tft.setTextColor(TFT_WHITE);
      tft.setCursor(18, 44 + (i * 20));
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
  tft.fillRoundRect(8, 34, 112, 60, 6, TFT_BLACK);
  tft.drawRoundRect(8, 34, 112, 60, 6, TFT_WHITE);
  tft.setTextSize(1);

  tft.setTextColor(TFT_RED);
  tft.setCursor(46, 42);
  tft.print("R.I.P.");

  const char* options[] = { "Restart", "Leave" };

  for (int i = 0; i < 2; i++) {
    if (i == selectedIndex) {
      tft.setTextColor(TFT_SAGE_GREEN);
      tft.setCursor(24, 60 + (i * 16));
      tft.print("> ");
    } else {
      tft.setTextColor(TFT_WHITE);
      tft.setCursor(24, 60 + (i * 16));
      tft.print("  ");
    }
    tft.println(options[i]);
  }
}

void DisplayManager::drawMinigameUI(int score, int timeLeft, int treatX, int treatY, int oldTreatX, int oldTreatY) {
  if (oldTreatY > 0) {
    tft.fillRect(oldTreatX, oldTreatY, 16, 16, TFT_SAGE_GREEN);
  }

  tft.fillRect(0, 0, SCREEN_WIDTH, 12, TFT_SAGE_GREEN);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(2, 2);
  tft.printf("Sc:%d T:%ds", score, timeLeft);

  if (treatY > 0) {
    drawTransparentImage(treatX, treatY, 16, 16, treat_frame, TFT_BLACK);
  }
}

void DisplayManager::drawMinigameTopBar(int score, int timeLeft) {
  tft.fillRect(0, 0, SCREEN_WIDTH, 12, TFT_SAGE_GREEN);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(2, 2);
  tft.printf("Sc:%d T:%ds", score, timeLeft);
}

void DisplayManager::updateMinigameTreat(int treatX, int treatY, int oldTreatX, int oldTreatY) {
  if (oldTreatY > 0) {
    tft.fillRect(oldTreatX, oldTreatY, 16, 16, TFT_SAGE_GREEN);
  }
  if (treatY > 0) {
    drawTransparentImage(treatX, treatY, 16, 16, treat_frame, TFT_BLACK);
  }
}