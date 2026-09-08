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

// --------------------------------------------------------------------------
// Digimon-World-style GREY PLATE bars.
//   Each bar is a 6px-tall grey plate: black top/bottom frame, white bevel on
//   the left edge, dark-grey dividers near each end, and a thin 2px colored
//   fill lane (light shade over base shade) framed inside the plate.
//   A dark number box hugs the right end of each plate.
// --------------------------------------------------------------------------
#define STAT_PLATE_H   6      // total plate height (matches the ASCII schema)
#define STAT_ROW_H     8      // row pitch (6px plate + 2px gap)  -> shorter/tighter
#define STAT_ROW_Y0    1
#define STAT_ICON_X    2
#define STAT_BAR_X     9      // plate left edge
#define STAT_BAR_W     62     // plate width

// Internal plate structure (x offsets from STAT_BAR_X)
#define STAT_LEFT_DIV   4              // dark divider just past the white bevel
#define STAT_RIGHT_DIV  (STAT_BAR_W-5) // dark divider near the right end
#define STAT_FILL_BL    (STAT_LEFT_DIV+1)   // fill-lane left border (black)
#define STAT_FILL_BR    (STAT_RIGHT_DIV-1)  // fill-lane right border (black)
#define STAT_FILL_X0    (STAT_FILL_BL+1)    // first fillable pixel
#define STAT_FILL_W     (STAT_FILL_BR - STAT_FILL_X0)  // fillable width

// Number box: hugs the right end of the plate.
#define STAT_NUM_X   (STAT_BAR_X + STAT_BAR_W + 1)
#define STAT_NUM_W   18
#define STAT_NUM_H   8

// Plate palette
#define STAT_FRAME_COLOR   0x0000  // b : black frame / borders
#define STAT_PLATE_GREY    0x8410  // g : plate body (mid grey)
#define STAT_PLATE_DGREY   0x4208  // d : dark-grey divider
#define STAT_BEVEL_WHITE   0xFFFF  // w : white bevel highlight (left edge)
#define STAT_NUM_BG        0x0000  // number box background
#define STAT_NUM_FRAME     0x8410  // grey border around the number box

// Lighten helper: OR this into a fill colour to get its "top" highlight shade.
#define STAT_HILITE_OR     0x8410

// --------------------------------------------------------------------------
// Menu styling (same grey-plate DW look as the stat bars).
//   Grey beveled window; each row is a grey plate; the selected row is a
//   lighter raised plate with a '>' arrow and dark text.
// --------------------------------------------------------------------------
#define MENU_PLATE_LGREY   0xBDF7  // lighter grey for the selected (raised) row
#define MENU_TEXT_LIGHT    0xFFFF  // text on unselected rows
#define MENU_TEXT_DARK     0x0000  // text on the light selected row


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
  void drawMenu(const char* title, const char* const* items, int itemCount, int selectedIndex);  void drawSettings(int selectedIndex, bool isMuted);
  void drawStatsPage(int hp, int maxHp, int ap, int dp);
  void drawDigivolutionPage();
  void drawGameOver(int selectedIndex);
  void drawMinigameUI(int score, int timeLeft, int treatX, int treatY, int oldTreatX, int oldTreatY);

  void drawMinigameTopBar(int score, int timeLeft);
  void updateMinigameTreat(int treatX, int treatY, int oldTreatX, int oldTreatY);
  void drawStatBar(int row, uint16_t iconColor, uint16_t barColor, int value, bool isHeart);

  // Shared DW grey-plate UI helpers (used by menu / settings / game-over).
  void drawBevelPanel(int x, int y, int w, int h);
  void drawMenuRow(int x, int y, int w, int h, const char* label,
                   bool selected, const char* suffix = nullptr);
};

#endif
