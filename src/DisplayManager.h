#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include "Sprites.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include <SPI.h>
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 128

// pick GPIOs that exist on your Heemol N16R8 board and aren't used elsewhere
#define TFT_CS   10
#define TFT_DC   9
#define TFT_RST  8
#define TFT_MOSI 11
#define TFT_SCLK 12
extern Adafruit_SSD1351 tft;

class DisplayManager {
private:
  Adafruit_SSD1351 tft;
  GFXcanvas16 buffer = GFXcanvas16(SCREEN_WIDTH, SCREEN_HEIGHT); // 128*128*2 = 32KB
  // ST7789_Sprite catBuffer;
public:
  DisplayManager();
  void begin();

  void forceFullRedraw(int hunger, int happiness, int energy);
  void clearScreen();
  void drawMainScreen(int hunger, int happiness, int energy);
  void clearTrail(int oldX, int newX, int y, int width, int height);

  void drawTransparentImage(int x, int y, int width, int height, const uint16_t* frame, uint16_t transparentColor);
  void drawSprite(int x, int y, int width, int height, const uint16_t* frame);
  void drawSpriteFlipped(int x, int y, int width, int height, const uint16_t* frame);

  void drawPoops(int count);
  void drawMenu(int selectedIndex);
  void drawSettings(int selectedIndex, bool isMuted);
  void drawGameOver(int selectedIndex);
  void drawMinigameUI(int score, int timeLeft, int treatX, int treatY, int oldTreatX, int oldTreatY);

  void drawMinigameTopBar(int score, int timeLeft);
  void updateMinigameTreat(int treatX, int treatY, int oldTreatX, int oldTreatY);
};

#endif