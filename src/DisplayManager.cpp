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
    tft.init(SCREEN_WIDTH, SCREEN_HEIGHT, SPI_MODE3);
  #else
    tft.begin();
  #endif
  tft.setRotation(3);
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
  tft.fillRect(0, 0, 280, 45, TFT_SAGE_GREEN);
  tft.setTextSize(2);

  if (hunger <= 20 || happiness <= 20) {
    tft.setTextColor(TFT_RED);
  } else {
    tft.setTextColor(TFT_WHITE);
  }

  tft.setCursor(5, 5);
  tft.printf("Hng:%d  Hap:%d", hunger, happiness);

  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 25);
  tft.print("Eny:");

  tft.drawRect(55, 25, 215, 16, TFT_WHITE);

  int fillWidth = (211 * energy) / 100;
  if (fillWidth > 0) {
    tft.fillRect(57, 27, fillWidth, 12, TFT_BLUE);
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
  int poopX[] = { 20, 230, 120 };
  int poopY = 195;
  for (int i = 0; i < count; i++) {
    drawTransparentImage(poopX[i], poopY, 20, 20, poop_frame, TFT_BLACK);
  }
}

void DisplayManager::drawMenu(int selectedIndex) {
  const char* menuItems[] = { "Feed", "Play", "Sleep", "Clean", "Settings", "Exit" };

  tft.fillRoundRect(40, 40, 200, 165, 10, TFT_BLACK);
  tft.drawRoundRect(40, 40, 200, 165, 10, TFT_WHITE);
  tft.setTextSize(2);

  for (int i = 0; i < 6; i++) {
    if (i == selectedIndex) {
      tft.setTextColor(TFT_SAGE_GREEN);
      tft.setCursor(60, 55 + (i * 25));
      tft.print("> ");
    } else {
      tft.setTextColor(TFT_WHITE);
      tft.setCursor(60, 55 + (i * 25));
      tft.print("  ");
    }
    tft.println(menuItems[i]);
  }
}

void DisplayManager::drawSettings(int selectedIndex, bool isMuted) {
  tft.fillRoundRect(40, 60, 200, 120, 10, TFT_BLACK);
  tft.drawRoundRect(40, 60, 200, 120, 10, TFT_WHITE);
  tft.setTextSize(2);

  const char* options[] = { "Sound: ", "Back" };

  for (int i = 0; i < 2; i++) {
    if (i == selectedIndex) {
      tft.setTextColor(TFT_SAGE_GREEN);
      tft.setCursor(60, 80 + (i * 30));
      tft.print("> ");
    } else {
      tft.setTextColor(TFT_WHITE);
      tft.setCursor(60, 80 + (i * 30));
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
  tft.fillRoundRect(30, 70, 220, 100, 10, TFT_BLACK);
  tft.drawRoundRect(30, 70, 220, 100, 10, TFT_WHITE);
  tft.setTextSize(2);

  tft.setTextColor(TFT_RED);
  tft.setCursor(80, 85);
  tft.print("R.I.P.");

  const char* options[] = { "Restart", "Leave" };

  for (int i = 0; i < 2; i++) {
    if (i == selectedIndex) {
      tft.setTextColor(TFT_SAGE_GREEN);
      tft.setCursor(60, 115 + (i * 25));
      tft.print("> ");
    } else {
      tft.setTextColor(TFT_WHITE);
      tft.setCursor(60, 115 + (i * 25));
      tft.print("  ");
    }
    tft.println(options[i]);
  }
}

void DisplayManager::drawMinigameUI(int score, int timeLeft, int treatX, int treatY, int oldTreatX, int oldTreatY) {
  if (oldTreatY > 0) {
    tft.fillRect(oldTreatX, oldTreatY, 16, 16, TFT_SAGE_GREEN);
  }

  tft.fillRect(0, 0, 280, 30, TFT_SAGE_GREEN);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 5);
  tft.printf("Score:%d  Time:%ds", score, timeLeft);

  if (treatY > 0) {
    drawTransparentImage(treatX, treatY, 16, 16, treat_frame, TFT_BLACK);
  }
}

void DisplayManager::drawMinigameTopBar(int score, int timeLeft) {
  tft.fillRect(0, 0, 280, 30, TFT_SAGE_GREEN);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 5);
  tft.printf("Score:%d  Time:%ds", score, timeLeft);
}

void DisplayManager::updateMinigameTreat(int treatX, int treatY, int oldTreatX, int oldTreatY) {
  if (oldTreatY > 0) {
    tft.fillRect(oldTreatX, oldTreatY, 16, 16, TFT_SAGE_GREEN);
  }
  if (treatY > 0) {
    drawTransparentImage(treatX, treatY, 16, 16, treat_frame, TFT_BLACK);
  }
}