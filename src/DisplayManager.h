#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Adafruit_GFX.h>
#include <SPI.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 128

#define TFT_CS   10
#define TFT_DC   13
#define TFT_RST  14
#define TFT_MOSI 11
#define TFT_SCLK 12

#define TFT_BLACK      0x0000
#define TFT_WHITE      0xFFFF
#define TFT_RED        0xF800
#define TFT_BLUE       0x001F
#define TFT_SAGE_GREEN 0x8E4D  // placeholder — replace with the real value if you know it

#ifdef SIMULATOR_BUILD
  #include <Adafruit_ST7789.h>
  using DisplayDriver = Adafruit_ST7789;
#else
  #include <Adafruit_SSD1351.h>
  using DisplayDriver = Adafruit_SSD1351;
#endif

class DisplayManager {
private:
  DisplayDriver tft;
  GFXcanvas16 petBuffer;
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