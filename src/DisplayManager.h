#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

// ==========================================================================
// BUILD TARGET TOGGLE
//   SIMULATOR_BUILD is controlled by platformio.ini, NOT hardcoded here:
//     - env:wokwi-sim  passes -DSIMULATOR_BUILD  -> ST7789 stand-in
//       (Wokwi has no SSD1351 part).
//     - env:esp32-s3-devkitc-1 omits it          -> real SSD1351 128x128.
//   Do not #define it here, or the hardware build will pull in the ST7789
//   library it doesn't depend on and fail to compile.
// ==========================================================================

#include <Adafruit_GFX.h>
#include <SPI.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 128
#define UI_BAR_HEIGHT 24

#define TFT_CS   10
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_DC   13
#define TFT_RST  14

#define TFT_BLACK      0x0000
#define TFT_WHITE      0xFFFF
#define TFT_RED        0xF800
#define TFT_BLUE       0x001F
#define TFT_SAGE_GREEN 0x8E4D  // placeholder — replace with the real value if you know it
#define TFT_BEZEL      0x2124  // dark grey "bezel" drawn around the framed 128x128 sim window

// ==========================================================================
//                            UI COLORS
// ==========================================================================
#define STAT_HUNGER_COLOR  0xFD20  // orange
#define STAT_HAPPY_COLOR   0xFFE0  // yellow
#define STAT_ENERGY_COLOR  0x001F  // blue
#define STAT_BAR_BG        0x2104  // dark track

#define STAT_ROW_H   9      // was 12
#define STAT_ROW_Y0  1      // was 2
#define STAT_ICON_X  2      // was 4
#define STAT_BAR_X   10     // was 15
#define STAT_BAR_W   72     // was 92
#define STAT_BAR_H   5      // was 7
#define STAT_NUM_X   84     // was 109


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
  // Draw-origin offset. 0 on real hardware; on the Wokwi ST7789 stand-in it
  // centers the 128x128 UI inside the 240x240 panel (see begin()).
  int originX = 0;
  int originY = 0;
public:
  DisplayManager();
  void begin();

  void forceFullRedraw(int hunger, int happiness, int energy);
  void drawBackground();
  void drawBackgroundRegion(int x, int y, int w, int h);
  void clearScreen();
  void drawStatusPanelChrome();
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
  void drawStatBar(int row, uint16_t iconColor, uint16_t barColor, int value, bool isHeart);
};

#endif