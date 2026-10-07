#include "DisplayManager.h"
#include "Sprites.h"
#include "EggSprites.h"
#include "Combat.h"
#include "EnemyRegistry.h"
// NPC sprite source. NOTE: there is no Pandamon art in Assets/ yet, so this
// uses an existing 30x30 profile as a STAND-IN so the NPC is visible/testable.
// To swap in real art: generate it (see README "Digimon sprite generator" or
// png_to_rgb565.py at --size 30), then change NPC_SPRITE below to the new
// symbol. Nothing else needs to change.
#include "pandamonSprites.h"
#define NPC_SPRITE pandamon_idle
#include <string.h>
#include <stdio.h>

// --------------------------------------------------------------------------
//  Static scene layer: outline > NPC sprite > forest background.
//
//  Keeping the NPC here (rather than painting it as a one-off overlay) is what
//  makes it survive the pet walking over it. drawSprite() fills the pet's
//  transparent pixels from this, and drawBackgroundRegion() repaints from this,
//  so both the NPC and its selection outline are restored automatically.
// --------------------------------------------------------------------------
// True if scene pixel (sx,sy) lands on an OPAQUE pixel of the NPC sprite.
// (Black == transparent, so those don't count.) Used both to draw the NPC and
// to trace a silhouette-hugging selection outline.
static inline bool npcOpaqueAt(int sx, int sy) {
  if (sx < NPC_X || sx >= NPC_X + NPC_SIZE ||
      sy < NPC_Y || sy >= NPC_Y + NPC_SIZE) return false;
  uint16_t c = pgm_read_word(&NPC_SPRITE[(sy - NPC_Y) * NPC_SIZE + (sx - NPC_X)]);
  return c != TFT_BLACK;
}

uint16_t DisplayManager::scenePixel(int sx, int sy) const {
  // The NPC itself (black == transparent, so the forest shows through).
  if (npcOpaqueAt(sx, sy)) {
    return pgm_read_word(&NPC_SPRITE[(sy - NPC_Y) * NPC_SIZE + (sx - NPC_X)]);
  }

  // Silhouette-hugging selection outline: a 1px halo that follows the SPRITE'S
  // SHAPE, not the image box. A (transparent) pixel is part of the outline when
  // any of its 8 neighbours is an opaque sprite pixel -- i.e. it sits just
  // outside the character's real edge. This wraps ears/limbs, not a square.
  if (npcHighlight) {
    // 2px-thick silhouette halo: a transparent pixel is part of the outline if
    // any opaque sprite pixel lies within 2px (Chebyshev distance). Bounded to
    // a +2px halo around the sprite box for a cheap early-out.
    const int R = 2;
    if (sx >= NPC_X - R && sx <= NPC_X + NPC_SIZE - 1 + R &&
        sy >= NPC_Y - R && sy <= NPC_Y + NPC_SIZE - 1 + R) {
      for (int dy = -R; dy <= R; dy++) {
        for (int dx = -R; dx <= R; dx++) {
          if (dx == 0 && dy == 0) continue;
          if (npcOpaqueAt(sx + dx, sy + dy)) return NPC_OUTLINE_COLOR;
        }
      }
    }
  }
  // Poops lie on the ground behind the pet, wherever they were dropped.
  for (int i = 0; i < poopPlaced; i++) {
    if (sx >= poops[i].x && sx < poops[i].x + POOP_SIZE &&
        sy >= poops[i].y && sy < poops[i].y + POOP_SIZE) {
      uint16_t c = pgm_read_word(&poop_frame[(sy - poops[i].y) * POOP_SIZE
                                            + (sx - poops[i].x)]);
      if (c != TFT_BLACK) return c;
    }
  }
  return pgm_read_word(&background_data_forest[sy * SCREEN_WIDTH + sx]);
}

void DisplayManager::addPoopBehind(int petX, int petY, int petW, int petH,
                                   bool facingRight) {
  if (poopPlaced >= MAX_POOPS) return;

  // The pet only strolls 14-36px between pauses, so the naive "always right
  // behind me" spot would often land on an existing poop. Try progressively
  // further offsets (and a little vertical variation) and take the first that
  // clears everything already on the ground.
  static const int along[] = { 0, 24, 48 };            // distance out from the pet
  static const int vert[]  = { 0, -12, 12, -24, 24 };  // feet level, then up/down
  const int feetY = petY + petH - POOP_SIZE;

  int bestX = 0, bestY = feetY, bestGap = -1;

  // side 0 = behind the pet (preferred), side 1 = in front. The fallback side
  // matters when "behind" is off-screen: a pet cornered on the left while facing
  // right has every behind-offset clamp to x=0, which would stack every poop on
  // the same pixel.
  for (int side = 0; side < 2; side++) {
    const bool toLeft = (side == 0) ? facingRight : !facingRight;
   for (unsigned vi = 0; vi < sizeof(vert) / sizeof(vert[0]); vi++) {
    for (unsigned ai = 0; ai < sizeof(along) / sizeof(along[0]); ai++) {
      int px = toLeft ? (petX - POOP_SIZE + 6 - along[ai])
                      : (petX + petW - 6 + along[ai]);
      int py = feetY + vert[vi];
      // Keep it fully on screen and out of the status-bar band.
      if (px < 0) px = 0;
      if (px > SCREEN_WIDTH - POOP_SIZE)  px = SCREEN_WIDTH - POOP_SIZE;
      if (py < UI_BAR_HEIGHT)             py = UI_BAR_HEIGHT;
      if (py > SCREEN_HEIGHT - POOP_SIZE) py = SCREEN_HEIGHT - POOP_SIZE;

      // Separation from the nearest existing poop. Two equal-size boxes miss
      // each other exactly when max(|dx|,|dy|) >= POOP_SIZE, so that maximum is
      // the useful measure of "how clear is this spot".
      int gap = SCREEN_WIDTH;          // nothing placed yet -> wide open
      for (int i = 0; i < poopPlaced; i++) {
        int gx = px - poops[i].x; if (gx < 0) gx = -gx;
        int gy = py - poops[i].y; if (gy < 0) gy = -gy;
        int sep = (gx > gy) ? gx : gy;
        if (sep < gap) gap = sep;
      }

      if (gap >= POOP_SIZE + POOP_MIN_GAP) {   // clear -> take it immediately
        poops[poopPlaced].x = px;
        poops[poopPlaced].y = py;
        poopPlaced++;
        return;
      }
      if (gap > bestGap) { bestGap = gap; bestX = px; bestY = py; }
    }
   }
  }

  // Every candidate was crowded (e.g. the pet is cornered): use whichever was
  // least crowded rather than dropping the poop on top of another.
  poops[poopPlaced].x = bestX;
  poops[poopPlaced].y = bestY;
  poopPlaced++;
}

void DisplayManager::syncPoopCount(int count) {
  if (count < 0) count = 0;
  if (count > MAX_POOPS) count = MAX_POOPS;
  if (count < poopPlaced) { poopPlaced = count; return; }   // cleaned
  // Poops whose position we never saw (restored from flash): fall back to the
  // old fixed spots along the bottom.
  static const int fallbackX[MAX_POOPS] = { 6, 104, 54 };
  while (poopPlaced < count) {
    poops[poopPlaced].x = fallbackX[poopPlaced];
    poops[poopPlaced].y = SCREEN_HEIGHT - POOP_SIZE - 2;
    poopPlaced++;
  }
}


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
    // Wokwi's board-st7789 shows red and blue swapped with the library's
    // default RGB order (slate-blue menus came out orange, sand paths blue).
    // Re-send MADCTL for rotation 2 with the BGR bit (0x08) set. This only runs
    // on the simulator; the real SSD1351 is unaffected.
    uint8_t madctl = ST77XX_MADCTL_RGB | 0x08;   // rotation 2 + BGR
    tft.sendCommand(ST77XX_MADCTL, &madctl, 1);
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
    for (int i = 0; i < rowW; i++) {
      rowBuf[i] = scenePixel(x0 + i, sy);
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
  // Poops are part of the static scene layer now, so reconcile them with the
  // game's count before seeding the canvas.
  syncPoopCount(poopCount);

  // 1. Static scene (forest + NPC + outline + poops) into the canvas.
  for (int row = 0; row < SCREEN_HEIGHT; row++) {
    for (int col = 0; col < SCREEN_WIDTH; col++) {
      frameBuffer.drawPixel(col, row, scenePixel(col, row));
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
  const IconId rowIcon[3] = { ICON_FOOD, ICON_HEART, ICON_BOLT };

  for (int row = 0; row < 3; row++) {
    int y = originY + STAT_ROW_Y0 + row * STAT_ROW_H;
    // Icon: food / heart / bolt. Small (STAT_PLATE_H ~6px) to fit the bar row.
    // gfx is nullptr here (chrome draws straight to the panel), so drawIcon
    // targets tft.
    drawIcon(rowIcon[row], originX + STAT_ICON_X, y, STAT_PLATE_H + 1, iconColors[row]);
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
                      ? scenePixel(sx, sy)
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
                      ? scenePixel(sx, sy)
                      : TFT_BLACK;
      uint16_t color = pgm_read_word(&frame[row * width + (width - 1 - col)]);
      packed[row * width + col] = (color != TFT_BLACK) ? color : bg;
    }
  }
  pushRGBBuf(x, y, packed, width, height);
}

void DisplayManager::drawPoops(int count) {
  // Draw from the recorded drop positions. (The old version indexed a 3-entry
  // array by `count` with no bound, which would read past it if the cap ever
  // rose above 3.)
  syncPoopCount(count);
  for (int i = 0; i < poopPlaced; i++) {
    drawTransparentImage(poops[i].x, poops[i].y, POOP_SIZE, POOP_SIZE,
                         poop_frame, TFT_BLACK);
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
// ==========================================================================
//  ICON SYSTEM. Small vector icons drawn with primitives on a 10px design grid,
//  scaled to `s` and offset to (x,y). Draws into gfx (frameBuffer) or tft.
//  Primitive-drawn (not blitted bitmaps) -> no PROGMEM cost, recolorable, and
//  immune to the GFXcanvas16 RAM-push byte-order issue.
// ==========================================================================
void DisplayManager::drawIcon(IconId id, int x, int y, int s, uint16_t color) {
  Adafruit_GFX* g = gfx ? gfx : &tft;
  // Map a 0..10 design coord to the target box.
  #define IX(v) (x + (v) * s / 10)
  #define IY(v) (y + (v) * s / 10)
  // Secondary shades derived from `color` (highlight = lighten, dark = darken).
  uint16_t hi   = color | 0x1082;                 // slight lighten
  uint16_t dark = (uint16_t)((color >> 1) & 0x7BEF);  // ~half brightness

  switch (id) {
    case ICON_HEART: {
      g->fillCircle(IX(3), IY(3), s/5, color);
      g->fillCircle(IX(7), IY(3), s/5, color);
      g->fillTriangle(IX(1), IY(4), IX(9), IY(4), IX(5), IY(9), color);
      break;
    }
    case ICON_FOOD: {   // drumstick: round meat + bone
      g->fillCircle(IX(3), IY(3), s*3/10, color);
      g->drawLine(IX(5), IY(5), IX(9), IY(9), 0xFFFF);
      g->drawLine(IX(5), IY(6), IX(9), IY(9), 0xFFFF);
      g->fillCircle(IX(9), IY(9), 1, 0xFFFF);
      break;
    }
    case ICON_BOLT: {
      g->fillTriangle(IX(6), IY(0), IX(2), IY(6), IX(5), IY(6), color);
      g->fillTriangle(IX(5), IY(6), IX(9), IY(3), IX(3), IY(9), color);
      break;
    }
    case ICON_SWORD: {  // vertical blade + guard + hilt
      g->fillRect(IX(4), IY(0), (s*2/10 < 1 ? 1 : s*2/10), s*7/10, color);
      g->drawLine(IX(2), IY(6), IX(8), IY(6), 0xC410);   // guard (brass-ish)
      g->fillRect(IX(4), IY(6), (s*2/10 < 1 ? 1 : s*2/10), s*4/10, 0xC410); // hilt
      break;
    }
    case ICON_SHIELD: {
      g->fillTriangle(IX(1), IY(1), IX(9), IY(1), IX(5), IY(5), color);
      g->fillTriangle(IX(1), IY(1), IX(5), IY(5), IX(5), IY(9), color);
      g->fillTriangle(IX(9), IY(1), IX(5), IY(5), IX(5), IY(9), color);
      break;
    }
    case ICON_T_VACCINE: {  // 3D droplet, dark round bottom-left
      g->fillCircle(IX(6), IY(7), s*4/10, color);
      g->fillTriangle(IX(6), IY(0), IX(3), IY(5), IX(9), IY(5), color);
      g->fillCircle(IX(5), IY(5), s/10 < 1 ? 1 : s/10, hi);         // highlight
      g->fillCircle(IX(4), IY(8), s*2/10 < 1 ? 1 : s*2/10, STAT_PLATE_DGREY); // dark round
      break;
    }
    case ICON_T_DATA: {     // isometric cube
      g->fillTriangle(IX(6), IY(0), IX(10), IY(2), IX(6), IY(4), hi);
      g->fillTriangle(IX(6), IY(0), IX(2),  IY(2), IX(6), IY(4), hi);
      g->fillTriangle(IX(2), IY(2), IX(6), IY(4), IX(6), IY(10), color);  // left face
      g->fillTriangle(IX(2), IY(2), IX(6), IY(10), IX(2), IY(8), color);
      g->fillTriangle(IX(10), IY(2), IX(6), IY(4), IX(6), IY(10), dark);  // right face
      g->fillTriangle(IX(10), IY(2), IX(6), IY(10), IX(10), IY(8), dark);
      break;
    }
    case ICON_T_VIRUS: {    // centered spiky ball, even spikes
      int cx = IX(6), cy = IY(6);
      // 8 spikes
      const int dirs[8][2] = {{0,-6},{4,-4},{6,0},{4,4},{0,6},{-4,4},{-6,0},{-4,-4}};
      for (int k = 0; k < 8; k++) {
        int tx = x + (6 + dirs[k][0]) * s/10;
        int ty = y + (6 + dirs[k][1]) * s/10;
        // perpendicular base offset ~2 design units
        int px = -dirs[k][1], py = dirs[k][0];   // perpendicular
        int b1x = cx + px * s/40, b1y = cy + py * s/40;
        int b2x = cx - px * s/40, b2y = cy - py * s/40;
        g->fillTriangle(b1x, b1y, tx, ty, b2x, b2y, dark);
      }
      g->fillCircle(cx, cy, s*3/10, color);
      g->fillCircle(cx - s/10, cy - s/10, s/10 < 1 ? 1 : s/10, hi);
      break;
    }
  }
  #undef IX
  #undef IY
}

// Linear interpolate between two RGB565 colours (t in 0..1), per channel.
static uint16_t lerp565(uint16_t a, uint16_t b, float t) {
  int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  int r = ar + (int)((br - ar) * t);
  int g = ag + (int)((bg - ag) * t);
  int bl = ab + (int)((bb - ab) * t);
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

// Modern panel: soft drop shadow, rounded corners, vertical 2-tone gradient
// body, soft border, and a 1px top highlight line. Replaces the old flat grey
// bevel plate -- every caller (combat, stats, menus, dialog) upgrades at once.
void DisplayManager::drawBevelPanel(int x, int y, int w, int h) {
  Adafruit_GFX* g = gfx ? gfx : &tft;
  const int r = UI_PANEL_RADIUS;

  // Soft drop shadow, offset down-right (drawn first, behind the panel).
  g->fillRoundRect(x + 2, y + 3, w, h, r, UI_SHADOW);

  // Gradient body: fill the rounded rect base colour, then paint lighter->darker
  // horizontal lines inset by 1px so the rounded border stays clean.
  g->fillRoundRect(x, y, w, h, r, UI_PANEL_TOP);
  for (int row = 1; row < h - 1; row++) {
    uint16_t col = lerp565(UI_PANEL_TOP, UI_PANEL_BOT, (float)row / (h - 1));
    // Inset the gradient lines so corner rounding (drawn by the border) shows.
    int inset = (row < r || row > h - 1 - r) ? 2 : 1;
    g->drawFastHLine(x + inset, y + row, w - 2 * inset, col);
  }

  // Soft rounded border + 1px top highlight.
  g->drawRoundRect(x, y, w, h, r, UI_PANEL_BORDER);
  g->drawFastHLine(x + r, y + 1, w - 2 * r, UI_PANEL_HI);
}

// One menu row as a beveled plate. Selected rows are a lighter (raised)
// plate with dark text and a '>' arrow; others are plate grey with light
// text. Optional right-aligned suffix (e.g. "ON"/"OFF").
void DisplayManager::drawMenuRow(int x, int y, int w, int h, const char* label, bool selected, const char* suffix) {
  Adafruit_GFX* g = gfx ? gfx : &tft;   // buffered menu -> canvas, else panel
  const int rr = 3;                      // small rounded corners for buttons

  // Gradient endpoints: selected rows glow with the accent, others use the
  // panel slate (one shade lighter than the panel body so rows read as raised).
  uint16_t top = selected ? UI_ACCENT     : UI_PANEL_TOP;
  uint16_t bot = selected ? UI_PANEL_TOP   : UI_PANEL_BOT;
  uint16_t textCol = selected ? UI_TEXT : UI_TEXT_MUTED;

  // Soft drop shadow, then the rounded gradient button body.
  g->fillRoundRect(x + 1, y + 2, w, h, rr, UI_SHADOW);
  g->fillRoundRect(x, y, w, h, rr, top);
  for (int row = 1; row < h - 1; row++) {
    uint16_t col = lerp565(top, bot, (float)row / (h - 1));
    int inset = (row < rr || row > h - 1 - rr) ? 2 : 1;
    g->drawFastHLine(x + inset, y + row, w - 2 * inset, col);
  }
  g->drawRoundRect(x, y, w, h, rr, UI_PANEL_BORDER);
  // 1px top highlight on the selected button for extra pop.
  if (selected) g->drawFastHLine(x + rr, y + 1, w - 2 * rr, UI_PANEL_HI);

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

void DisplayManager::drawListMenu(const char* title, const char* const* items,
                                  const char* const* suffixes, int itemCount,
                                  int selectedIndex) {
  // Same window and row style as drawMenu, composited offscreen in one blit.
  seedBufferBackground();
  gfx = &frameBuffer;

  const int pnlX = 8,   pnlY = 6;
  const int pnlW = 112, pnlH = 116;
  drawBevelPanel(pnlX, pnlY, pnlW, pnlH);

  frameBuffer.setTextSize(1);
  frameBuffer.setTextColor(MENU_TEXT_DARK);
  frameBuffer.setCursor(pnlX + 6, pnlY + 5);
  frameBuffer.print(title);
  frameBuffer.drawFastHLine(pnlX + 4, pnlY + 15, pnlW - 8, STAT_FRAME_COLOR);

  // Fixed 18px pitch. Up to 5 rows are visible; with more, the window scrolls
  // to keep the selection on screen.
  const int MAX_VISIBLE = 5;
  const int itemX  = pnlX + 4;
  const int itemW  = pnlW - 8;
  const int listY0 = pnlY + 20;
  const int pitch  = 18;
  const int itemH  = pitch - 3;

  int visible = itemCount < MAX_VISIBLE ? itemCount : MAX_VISIBLE;
  int first = 0;
  if (selectedIndex >= visible) first = selectedIndex - visible + 1;
  if (first > itemCount - visible) first = itemCount - visible;
  if (first < 0) first = 0;

  for (int r = 0; r < visible; r++) {
    int i = first + r;
    const char* suffix = suffixes ? suffixes[i] : nullptr;
    drawMenuRow(itemX, listY0 + r * pitch, itemW, itemH,
                items[i], i == selectedIndex, suffix);
  }

  // Scroll hints: small arrows on the right edge when rows are hidden.
  uint16_t hint = UI_TEXT_MUTED;
  int ax = pnlX + pnlW - 8;
  if (first > 0) {                       // "more above": in the title bar
    int ty = pnlY + 10;
    frameBuffer.fillTriangle(ax, ty, ax + 4, ty, ax + 2, ty - 3, hint);
  }
  if (first + visible < itemCount) {
    int by = listY0 + visible * pitch;
    frameBuffer.fillTriangle(ax, by - 1, ax + 4, by - 1, ax + 2, by + 2, hint);
  }

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

void DisplayManager::drawStatsPage(const char* name, int hp, int maxHp, int ap, int dp, int intel, int speed,
                                   int level, int xp, int xpForNext,
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
  frameBuffer.setCursor(pnlX + (pw > 0 ? pw : 0) + 10, pnlY + 20 + ((pw > 0 ? pw : 20) / 2) - 8);
  // Display the name with a capitalized first letter (registry names are
  // lowercase, e.g. "koromon" -> "Koromon").
  if (name && name[0]) {
    char disp[16];
    strncpy(disp, name, sizeof(disp) - 1);
    disp[sizeof(disp) - 1] = '\0';
    if (disp[0] >= 'a' && disp[0] <= 'z') disp[0] = (char)(disp[0] - 'a' + 'A');
    frameBuffer.printf("%s", disp);
  }
  frameBuffer.setCursor(pnlX + (pw > 0 ? pw : 0) + 10, pnlY + 20 + ((pw > 0 ? pw : 20) / 2) + 2);
  frameBuffer.printf("Lv %d", level);

  // Stat rows with icons (heart=HP, sword=AP, shield=DP). Icon at the left,
  // value text to its right.
  // Stat block: two columns x three rows -> HP, AP, DP (left) / --, INT, SPD (right).
  // Icons: heart=HP, sword=AP, shield=DP, book=INT (uses sword tint), bolt=SPD.
  const int c1 = pnlX + 8,  t1 = pnlX + 21;    // left column icon / text
  const int c2 = pnlX + 60, t2 = pnlX + 73;    // right column icon / text
  frameBuffer.setTextColor(MENU_TEXT_LIGHT);
  // Row 1: HP (full width value)
  drawIcon(ICON_HEART,  c1, pnlY + 52, 10, UI_DANGER);
  frameBuffer.setCursor(t1, pnlY + 54); frameBuffer.printf("%d/%d", hp, maxHp);
  // Row 2: AP (left) + INT (right)
  drawIcon(ICON_SWORD,  c1, pnlY + 64, 10, MENU_TEXT_LIGHT);
  frameBuffer.setCursor(t1, pnlY + 66); frameBuffer.printf("%d", ap);
  drawIcon(ICON_BOLT,   c2, pnlY + 64, 10, UI_SPECIAL);   // INT (violet)
  frameBuffer.setCursor(t2, pnlY + 66); frameBuffer.printf("%d", intel);
  // Row 3: DP (left) + SPD (right)
  drawIcon(ICON_SHIELD, c1, pnlY + 76, 10, STAT_ENERGY_COLOR);
  frameBuffer.setCursor(t1, pnlY + 78); frameBuffer.printf("%d", dp);
  drawIcon(ICON_BOLT,   c2, pnlY + 76, 10, STAT_HAPPY_COLOR); // SPD (cyan)
  frameBuffer.setCursor(t2, pnlY + 78); frameBuffer.printf("%d", speed);

  // XP progress bar toward the next level.
  frameBuffer.setCursor(pnlX + 8, pnlY + 92);
  frameBuffer.printf("XP: %d/%d", xp, xpForNext);
  const int xbX = pnlX + 8;
  const int xbY = pnlY + 102;
  const int xbW = pnlW - 20;
  const int xbH = 5;
  frameBuffer.drawRect(xbX, xbY, xbW, xbH, STAT_FRAME_COLOR);
  int fillW = (xpForNext > 0) ? ((xbW - 2) * xp) / xpForNext : 0;
  if (fillW < 0) fillW = 0;
  if (fillW > xbW - 2) fillW = xbW - 2;
  if (fillW > 0) frameBuffer.fillRect(xbX + 1, xbY + 1, fillW, xbH - 2, STAT_HAPPY_COLOR);

  frameBuffer.setCursor(pnlX + 8, pnlY + pnlH - 12);
  frameBuffer.print("OK: Back");

  gfx = nullptr;
  pushFrame();
}

void DisplayManager::drawDigivolutionList(const DigimonSprites* current, int selectedIndex) {
  seedBufferBackground();
  gfx = &frameBuffer;

  const int pnlX = 8,   pnlY = 6;
  const int pnlW = 112, pnlH = 116;
  drawBevelPanel(pnlX, pnlY, pnlW, pnlH);
  frameBuffer.setTextSize(1);

  frameBuffer.setTextColor(MENU_TEXT_DARK);
  frameBuffer.setCursor(pnlX + 6, pnlY + 5);
  frameBuffer.print("Digivolve");
  frameBuffer.drawFastHLine(pnlX + 4, pnlY + 15, pnlW - 8, STAT_FRAME_COLOR);

  int n = (current && current->evolutions) ? current->evolutionCount : 0;
  int rowCount = n + 1;   // + "Back"

  const int itemX  = pnlX + 4;
  const int itemW  = pnlW - 8;
  const int listY0 = pnlY + 20;
  const int listH  = pnlH - (listY0 - pnlY) - 4;
  int pitch = listH / rowCount;
  if (pitch > 34) pitch = 34;
  if (pitch < 14) pitch = 14;
  const int rowH = pitch - 2;

  // One row per possible evolution: portrait + name only, no requirement line
  // (that detail now lives on drawDigivolutionDetail()).
  for (int i = 0; i < n; i++) {
    const EvolutionReq& req = current->evolutions[i];
    int ry = listY0 + i * pitch;
    bool sel = (i == selectedIndex);

    uint16_t body = sel ? MENU_PLATE_LGREY : STAT_PLATE_GREY;
    frameBuffer.fillRect(itemX, ry, itemW, rowH, body);
    frameBuffer.drawRect(itemX, ry, itemW, rowH, STAT_FRAME_COLOR);

    const uint16_t* prof = req.target ? req.target->profile : nullptr;
    int ps = req.target ? req.target->profileSize : 0;
    int drawSize = ps;
    if (drawSize > rowH - 4) drawSize = rowH - 4;   // shrink to fit a tight row
    int px = itemX + 3;
    int py = ry + (rowH - drawSize) / 2;
    if (prof && drawSize > 0) {
      for (int row = 0; row < drawSize; row++) {
        for (int col = 0; col < drawSize; col++) {
          int srow = (ps == drawSize) ? row : (row * ps) / drawSize;
          int scol = (ps == drawSize) ? col : (col * ps) / drawSize;
          uint16_t c = pgm_read_word(&prof[srow * ps + scol]);
          if (c != TFT_BLACK) frameBuffer.drawPixel(px + col, py + row, c);
        }
      }
    }

    frameBuffer.setTextColor(MENU_TEXT_DARK);
    int nameX = px + (ps > 0 ? drawSize : 0) + 6;
    frameBuffer.setCursor(nameX, ry + (rowH - 7) / 2);
    frameBuffer.printf("%s%s", sel ? "> " : "  ", req.target ? req.target->name : "?");
  }

  // "Back" row, always last.
  {
    int ry = listY0 + n * pitch;
    bool sel = (selectedIndex == n) || (n == 0);
    if (n == 0) {
      frameBuffer.setTextColor(MENU_TEXT_DARK);
      frameBuffer.setCursor(itemX + 4, ry - 4);
      frameBuffer.print("Final form!");
    }
    drawMenuRow(itemX, ry, itemW, rowH, "Back", sel);
  }

  gfx = nullptr;
  pushFrame();
}

void DisplayManager::drawDigivolutionDetail(const DigimonSprites* current, int targetIndex,
                                            int detailSelection, int maxHp, int ap, int dp,
                                            int ageDays, int happiness, int hunger,
                                            int level, int intelligence, int speed) {
  seedBufferBackground();
  gfx = &frameBuffer;

  const int pnlX = 8,   pnlY = 6;
  const int pnlW = 112, pnlH = 116;
  drawBevelPanel(pnlX, pnlY, pnlW, pnlH);
  frameBuffer.setTextSize(1);

  const EvolutionReq* req = (current && current->evolutions &&
                             targetIndex >= 0 && targetIndex < current->evolutionCount)
                             ? &current->evolutions[targetIndex] : nullptr;

  frameBuffer.setTextColor(MENU_TEXT_DARK);
  frameBuffer.setCursor(pnlX + 6, pnlY + 5);
  frameBuffer.print(req && req->target ? req->target->name : "?");
  frameBuffer.drawFastHLine(pnlX + 4, pnlY + 15, pnlW - 8, STAT_FRAME_COLOR);

  int ps = 0;
  if (!req) {
    frameBuffer.setTextColor(MENU_TEXT_DARK);
    frameBuffer.setCursor(pnlX + 8, pnlY + 40);
    frameBuffer.print("No data.");
  } else {
    const uint16_t* prof = req->target ? req->target->profile : nullptr;
    ps = req->target ? req->target->profileSize : 0;
    int sx = pnlX + 8, sy = pnlY + 20;
    if (prof && ps > 0) {
      for (int row = 0; row < ps; row++) {
        for (int col = 0; col < ps; col++) {
          uint16_t c = pgm_read_word(&prof[row * ps + col]);
          if (c != TFT_BLACK) frameBuffer.drawPixel(sx + col, sy + row, c);
        }
      }
    }

    // All requirement lines (not just the first unmet one), colored red when
    // that specific requirement isn't met yet.
    int tx = sx + (ps > 0 ? ps + 6 : 0);
    int ty = pnlY + 20;
    const int lh = 9;
    char buf[24];
    bool any = false;

    if (req->minMaxHp > 0) {
      snprintf(buf, sizeof(buf), "HP %d/%d", maxHp, req->minMaxHp);
      frameBuffer.setTextColor(maxHp >= req->minMaxHp ? MENU_TEXT_DARK : TFT_RED);
      frameBuffer.setCursor(tx, ty); frameBuffer.print(buf); ty += lh; any = true;
    }
    if (req->minAp > 0) {
      snprintf(buf, sizeof(buf), "AP %d/%d", ap, req->minAp);
      frameBuffer.setTextColor(ap >= req->minAp ? MENU_TEXT_DARK : TFT_RED);
      frameBuffer.setCursor(tx, ty); frameBuffer.print(buf); ty += lh; any = true;
    }
    if (req->minDp > 0) {
      snprintf(buf, sizeof(buf), "DP %d/%d", dp, req->minDp);
      frameBuffer.setTextColor(dp >= req->minDp ? MENU_TEXT_DARK : TFT_RED);
      frameBuffer.setCursor(tx, ty); frameBuffer.print(buf); ty += lh; any = true;
    }
    if (req->minAgeDays > 0) {
      snprintf(buf, sizeof(buf), "Age %d/%d", ageDays, req->minAgeDays);
      frameBuffer.setTextColor(ageDays >= req->minAgeDays ? MENU_TEXT_DARK : TFT_RED);
      frameBuffer.setCursor(tx, ty); frameBuffer.print(buf); ty += lh; any = true;
    }
    if (req->minHappiness > 0) {
      snprintf(buf, sizeof(buf), "Joy %d/%d", happiness, req->minHappiness);
      frameBuffer.setTextColor(happiness >= req->minHappiness ? MENU_TEXT_DARK : TFT_RED);
      frameBuffer.setCursor(tx, ty); frameBuffer.print(buf); ty += lh; any = true;
    }
    if (req->minHunger > 0) {
      snprintf(buf, sizeof(buf), "Fed %d/%d", hunger, req->minHunger);
      frameBuffer.setTextColor(hunger >= req->minHunger ? MENU_TEXT_DARK : TFT_RED);
      frameBuffer.setCursor(tx, ty); frameBuffer.print(buf); ty += lh; any = true;
    }
    if (req->minLevel > 0) {
      snprintf(buf, sizeof(buf), "Lv %d/%d", level, req->minLevel);
      frameBuffer.setTextColor(level >= req->minLevel ? MENU_TEXT_DARK : TFT_RED);
      frameBuffer.setCursor(tx, ty); frameBuffer.print(buf); ty += lh; any = true;
    }
    if (req->minIntelligence > 0) {
      snprintf(buf, sizeof(buf), "INT %d/%d", intelligence, req->minIntelligence);
      frameBuffer.setTextColor(intelligence >= req->minIntelligence ? MENU_TEXT_DARK : TFT_RED);
      frameBuffer.setCursor(tx, ty); frameBuffer.print(buf); ty += lh; any = true;
    }
    if (!any) {
      frameBuffer.setTextColor(MENU_TEXT_DARK);
      frameBuffer.setCursor(tx, ty); frameBuffer.print("No requirements");
    }
  }

  // Two selectable rows: "Digivolve!" then "Back".
  const int itemX = pnlX + 4;
  const int itemW = pnlW - 8;
  const int itemH = 14;
  const int rowY0 = pnlY + pnlH - itemH * 2 - 6;
  drawMenuRow(itemX, rowY0,            itemW, itemH, "Digivolve!", detailSelection == 0);
  drawMenuRow(itemX, rowY0 + itemH + 2, itemW, itemH, "Back",       detailSelection == 1);

  gfx = nullptr;
  pushFrame();
}

void DisplayManager::drawTrainResult(const char* statName, int before, int after,
                                     int energyLeft) {
  seedBufferBackground();
  gfx = &frameBuffer;

  const int pnlX = 10,  pnlY = 28;
  const int pnlW = 108, pnlH = 74;
  drawBevelPanel(pnlX, pnlY, pnlW, pnlH);

  frameBuffer.setTextSize(1);
  char line[32];

  // Title
  frameBuffer.setTextColor(MENU_TEXT_DARK);
  frameBuffer.setCursor(pnlX + 6, pnlY + 6);
  frameBuffer.print("Training done!");
  frameBuffer.drawFastHLine(pnlX + 4, pnlY + 17, pnlW - 8, STAT_FRAME_COLOR);

  // Which stat (own line, so long values below can't push it off the panel).
  frameBuffer.setTextColor(MENU_TEXT_DARK);
  frameBuffer.setCursor(pnlX + 8, pnlY + 23);
  frameBuffer.print(statName);

  // before ==> after. Kept on its own line: maxHp reaches 4 digits, and
  // "1250 ==> 1330" is 13 chars (78px) inside a 96px inner width.
  snprintf(line, sizeof(line), "%d ==> %d", before, after);
  frameBuffer.setTextColor(MENU_TEXT_DARK);
  frameBuffer.setCursor(pnlX + 8, pnlY + 35);
  frameBuffer.print(line);

  // Gain (highlighted) and the energy it cost.
  int gain = after - before;
  snprintf(line, sizeof(line), "+%d", gain > 0 ? gain : 0);
  frameBuffer.setTextColor(STAT_ENERGY_COLOR);
  frameBuffer.setCursor(pnlX + 8, pnlY + 47);
  frameBuffer.print(line);

  snprintf(line, sizeof(line), "Energy %d", energyLeft);
  frameBuffer.setTextColor(MENU_TEXT_DARK);
  frameBuffer.setCursor(pnlX + 42, pnlY + 47);
  frameBuffer.print(line);

  drawMenuRow(pnlX + 4, pnlY + pnlH - 18, pnlW - 8, 14, "OK", true);

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

void DisplayManager::drawDialog(const char* speaker, const char* text,
                                const char* const* options, int optionCount,
                                int selectedIndex) {
  seedBufferBackground();
  gfx = &frameBuffer;

  const int pnlX = 6,   pnlY = 20;
  const int pnlW = 116, pnlH = 96;
  drawBevelPanel(pnlX, pnlY, pnlW, pnlH);

  // Speaker name (accent) at the top of the panel.
  frameBuffer.setTextSize(1);
  if (speaker) {
    frameBuffer.setTextColor(STAT_HAPPY_COLOR);
    frameBuffer.setCursor(pnlX + 4, pnlY + 4);
    frameBuffer.print(speaker);
  }

  // Body text: naive word-wrap at ~18 chars/line into the panel body.
  frameBuffer.setTextColor(MENU_TEXT_LIGHT);
  const int bodyX = pnlX + 4;
  int bodyY = pnlY + 16;
  const int lineH = 9;
  const int maxChars = 18;
  if (text) {
    int lineLen = 0;
    int wordStart = 0;
    char line[maxChars + 2];
    int li = 0;
    for (int i = 0; ; i++) {
      char ch = text[i];
      bool boundary = (ch == ' ' || ch == '\0');
      if (boundary) {
        int wordLen = i - wordStart;
        if (lineLen + (lineLen ? 1 : 0) + wordLen > maxChars && lineLen > 0) {
          line[li] = '\0';
          frameBuffer.setCursor(bodyX, bodyY);
          frameBuffer.print(line);
          bodyY += lineH;
          li = 0; lineLen = 0;
        }
        if (lineLen && li < maxChars) { line[li++] = ' '; lineLen++; }
        for (int k = wordStart; k < i && li < maxChars; k++) { line[li++] = text[k]; lineLen++; }
        wordStart = i + 1;
      }
      if (ch == '\0') break;
    }
    if (li > 0) {
      line[li] = '\0';
      frameBuffer.setCursor(bodyX, bodyY);
      frameBuffer.print(line);
      bodyY += lineH;
    }
  }

  // Options as selectable rows near the bottom of the panel.
  const int itemX = pnlX + 4;
  const int itemW = pnlW - 8;
  const int itemH = 11;
  int optY = pnlY + pnlH - optionCount * (itemH + 2) - 2;
  for (int i = 0; i < optionCount; i++) {
    drawMenuRow(itemX, optY + i * (itemH + 2), itemW, itemH,
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


// ==========================================================================
//  EGG SELECT (first run). Placeholder art: 7 distinctly-coloured egg shapes
//  in a row; the selected one is enlarged with a yellow outline. Swap in real
//  egg sprites later by drawing them here instead of the coloured ellipses.
// ==========================================================================
void DisplayManager::drawEggSelect(int selected, int count, const char* label, int frame) {
  seedBufferBackground();
  gfx = &frameBuffer;

  const int pnlX = 4, pnlY = 4, pnlW = 120, pnlH = 120;
  drawBevelPanel(pnlX, pnlY, pnlW, pnlH);

  // Title.
  frameBuffer.setTextSize(1);
  frameBuffer.setTextColor(MENU_TEXT_DARK);
  frameBuffer.setCursor(pnlX + 22, pnlY + 6);
  frameBuffer.print("Choose an egg");
  frameBuffer.drawFastHLine(pnlX + 6, pnlY + 16, pnlW - 12, STAT_FRAME_COLOR);

  if (count < 1) { gfx = nullptr; pushFrame(); return; }

  // ---- CAROUSEL of real egg sprites -------------------------------------
  // One big egg centred (animated through its 3 wobble frames); the previous
  // and next eggs peek in on each side (static frame 0). Eggs are composited
  // straight from PROGMEM into the canvas (transparent black = show-through),
  // the SAME path the NPC/pet use -- so colours match and there is no
  // GFXcanvas16 byte-order issue.
  const int S      = EGG_SPRITE_SIZE;        // 32
  const int cxMid  = pnlX + pnlW / 2;
  const int cyEgg  = pnlY + 58;              // egg vertical centre
  const int sideDX = 40;                     // how far the side eggs sit out

  int prev = (selected - 1 + count) % count;
  int next = (selected + 1) % count;

  // Draw one egg frame centred at (cx,cy). Pixels are clipped to the panel
  // interior so a side egg that overhangs the bevel doesn't spill outside.
  auto drawEgg = [&](const uint16_t* fr, int cx, int cy) {
    if (!fr) return;
    int ox = cx - S / 2, oy = cy - S / 2;
    for (int row = 0; row < S; row++) {
      int py = oy + row;
      if (py <= pnlY + 17 || py >= pnlY + pnlH - 1) continue;   // keep off title/border
      for (int col = 0; col < S; col++) {
        int px = ox + col;
        if (px <= pnlX || px >= pnlX + pnlW - 1) continue;
        uint16_t cpx = pgm_read_word(&fr[row * S + col]);
        if (cpx != TFT_BLACK) frameBuffer.drawPixel(px, py, cpx);
      }
    }
  };

  // Side eggs first (static), so the centre egg overlaps them.
  drawEgg(EGG_FRAMES[prev][0], cxMid - sideDX, cyEgg);
  drawEgg(EGG_FRAMES[next][0], cxMid + sideDX, cyEgg);

  // Centre (selected) egg, animated: cycle its 3 frames.
  int f = frame % EGG_FRAME_COUNT;
  if (f < 0) f = 0;
  drawEgg(EGG_FRAMES[selected][f], cxMid, cyEgg);

  // ---- Navigation arrows (only when there's more than one egg) -----------
  if (count > 1) {
    int ay = cyEgg;
    int lx = pnlX + 6;
    frameBuffer.fillTriangle(lx + 6, ay - 6, lx + 6, ay + 6, lx, ay, MENU_TEXT_DARK);
    int rx = pnlX + pnlW - 6;
    frameBuffer.fillTriangle(rx - 6, ay - 6, rx - 6, ay + 6, rx, ay, MENU_TEXT_DARK);
  }

  // Selected egg's starter label, centred below the carousel.
  if (label && label[0]) {
    int len = 0; while (label[len]) len++;
    int tw = len * 6;
    frameBuffer.setTextColor(MENU_TEXT_DARK);
    frameBuffer.setCursor(pnlX + (pnlW - tw) / 2, pnlY + 92);
    frameBuffer.print(label);
  }

  // Footer hint.
  frameBuffer.setTextColor(MENU_TEXT_DARK);
  frameBuffer.setCursor(pnlX + 10, pnlY + pnlH - 12);
  frameBuffer.print("L/R choose  OK hatch");

  gfx = nullptr;
  pushFrame();
}

// Blit a square sprite frame into an offscreen canvas (RAM), black treated as
// transparent (same convention as the profile sprite). Static file-local
// helper ported from the original project for drawCombatScene().
static void blitSpriteToBuffer(GFXcanvas16& fb, int dx, int dy, int w, int h,
                               const uint16_t* frame, bool flip, uint16_t tint = 0) {
  if (!frame || w <= 0 || h <= 0) return;
  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      int srcCol = flip ? (w - 1 - col) : col;
      uint16_t c = pgm_read_word(&frame[row * w + srcCol]);
      if (c != TFT_BLACK) {
        // tint != 0 -> draw the OPAQUE pixels as the tint colour (hit-flash
        // silhouette that hugs the sprite shape, not a box filter).
        fb.drawPixel(dx + col, dy + row, tint ? tint : c);
      }
    }
  }
}

// Digimon-World-style grey-plate value bar: a bevelled grey plate with a thin
// 2px colored fill lane (light top row + base bottom row) -- the SAME visual
// language as the main-screen stat bars (see drawStatBar/drawStatusPanelChrome).
// Scales `value` out of `maxValue` across the fill lane. Draws into whichever
// target the grey-plate helpers use (frameBuffer when gfx is set, else the panel
// via the g=... pattern used elsewhere). Here we draw straight into frameBuffer
// since the combat scene composites into it.
void DisplayManager::drawPlateBar(int x, int y, int w, int value, int maxValue, uint16_t barColor) {
  const int h = STAT_PLATE_H;                 // 6px plate, matches main screen
  // Plate body + black frame.
  frameBuffer.fillRect(x + 1, y + 1, w - 2, h - 2, STAT_PLATE_GREY);
  frameBuffer.drawFastHLine(x + 1, y,         w - 2, STAT_FRAME_COLOR);
  frameBuffer.drawFastHLine(x + 1, y + h - 1, w - 2, STAT_FRAME_COLOR);
  frameBuffer.drawFastVLine(x,         y + 1, h - 2, STAT_FRAME_COLOR);
  frameBuffer.drawFastVLine(x + w - 1, y + 1, h - 2, STAT_FRAME_COLOR);
  // White bevel on the top/left, dark-grey shadow bottom/right.
  frameBuffer.drawFastHLine(x + 1, y + 1,     2, STAT_BEVEL_WHITE);
  frameBuffer.drawPixel(x + 1, y + 2, STAT_BEVEL_WHITE);
  frameBuffer.drawFastHLine(x + 1, y + h - 2, w - 2, STAT_PLATE_DGREY);

  // 2px fill lane inset inside the plate (same construction as drawStatBar).
  int laneX = x + 2;
  int laneW = w - 4;
  int laneY = y + 2;
  frameBuffer.fillRect(laneX, laneY, laneW, 2, STAT_PLATE_GREY);   // empty = grey
  if (maxValue < 1) maxValue = 1;
  int fillW = (laneW * value) / maxValue;
  if (fillW < 0) fillW = 0;
  if (fillW > laneW) fillW = laneW;
  if (fillW > 0) {
    uint16_t hi = barColor | STAT_HILITE_OR;                  // light top shade
    frameBuffer.drawFastHLine(laneX, laneY,     fillW, hi);
    frameBuffer.drawFastHLine(laneX, laneY + 1, fillW, barColor);
  }
}

// Short tag + accent colour for a Digimon attribute type (combat badges).
static const char* typeTag(DigimonType t) {
  switch (t) { case TYPE_VACCINE: return "Vc"; case TYPE_VIRUS: return "Vi";
               default: return "Da"; }
}
static uint16_t typeColor(DigimonType t) {
  switch (t) { case TYPE_VACCINE: return STAT_ENERGY_COLOR;   // blue-ish
               case TYPE_VIRUS:   return TFT_RED;
               default:           return STAT_HAPPY_COLOR; }   // yellow for Data
}
static IconId typeIcon(DigimonType t) {
  switch (t) { case TYPE_VACCINE: return ICON_T_VACCINE;
               case TYPE_VIRUS:   return ICON_T_VIRUS;
               default:           return ICON_T_DATA; }
}

void DisplayManager::drawCombatScene(const Combat& combat) {
  seedBufferBackground();
  gfx = &frameBuffer;
  frameBuffer.setTextSize(1);

  const Enemy* enemy = combat.getEnemy();
  CombatPhase phase = combat.getPhase();

  // ---- Enemy header: name + HP bar ----------------------------------------
  frameBuffer.setTextColor(TFT_WHITE);
  frameBuffer.setCursor(4, 3);
  frameBuffer.printf("%s", enemy ? enemy->name : "Enemy");
  // Enemy type badge (small coloured tag) at the right end of the HP-bar row.
  if (enemy && enemy->art) {
    DigimonType et = enemy->art->type;
    int bx = 4 + 80 + 3;                 // just past the 80px enemy HP bar
    drawIcon(typeIcon(et), bx, 11, 10, typeColor(et));
  }

  const int eBarX = 4, eBarY = 13, eBarW = 80;
  drawPlateBar(eBarX, eBarY, eBarW, combat.getDisplayEnemyHp(), combat.getEnemyMaxHp(), TFT_RED);

  // ---- Enemy sprite (upper-right) -----------------------------------------
  if (enemy && enemy->art && enemy->art->walk && enemy->art->walkCount > 0) {
    int esize = enemy->art->spriteSize;
    int ex = SCREEN_WIDTH - esize - 2;
    int ey = 20;
    // Enemy faces LEFT (toward the player). walk[0] drawn flipped to face left.
    // Hit flash: draw the sprite as a solid white silhouette on impact (hugs
    // its shape); otherwise draw it normally.
    uint16_t etint = (combat.getFlashTarget() == 2) ? 0xFFFF : 0;
    blitSpriteToBuffer(frameBuffer, ex, ey, esize, esize, enemy->art->walk[0], false, etint);
  }

  // ---- Enemy intent telegraph ---------------------------------------------
  // During the player's decision phases, show a small icon above the enemy
  // hinting its NEXT move so Guard/timing choices are informed:
  //   ATTACK = sword  |  HEAVY = double chevron (big hit)  |  GUARD = shield.
  if (phase == CP_PLAYER_MENU || phase == CP_PLAYER_TIMING) {
    // Labeled intent chip, placed BELOW the enemy HP bar on the LEFT so it
    // never overlaps the enemy sprite (which occupies the upper-right).
    EnemyIntent intent = combat.getEnemyIntent();
    const int cx = 4, cy = 22, cw = 58, ch = 13;
    frameBuffer.fillRoundRect(cx, cy, cw, ch, 3, STAT_PLATE_GREY);
    frameBuffer.drawRoundRect(cx, cy, cw, ch, 3, STAT_FRAME_COLOR);
    frameBuffer.drawFastHLine(cx + 3, cy + 1, cw - 6, STAT_BEVEL_WHITE);  // top highlight

    int ix = cx + 8, iy = cy + 6;     // icon anchor (centre)
    const char* label;
    uint16_t icol;
    if (intent == EI_GUARD) {
      icol = STAT_ENERGY_COLOR; label = "Guard";
      frameBuffer.fillTriangle(ix - 3, iy - 3, ix + 3, iy - 3, ix, iy + 3, icol);  // shield
    } else if (intent == EI_HEAVY) {
      icol = TFT_RED; label = "Heavy!";
      for (int k = 0; k < 2; k++) {   // double chevron
        int bx = ix - 3 + k * 3;
        frameBuffer.drawLine(bx, iy - 3, bx + 3, iy, icol);
        frameBuffer.drawLine(bx + 3, iy, bx, iy + 3, icol);
      }
    } else {
      icol = MENU_TEXT_LIGHT; label = "Attack";
      frameBuffer.drawLine(ix - 3, iy + 3, ix + 3, iy - 3, icol);  // sword blade
      frameBuffer.drawLine(ix - 3, iy + 1, ix - 1, iy + 3, icol);  // guard
    }
    frameBuffer.setTextColor(MENU_TEXT_LIGHT);
    frameBuffer.setCursor(cx + 16, cy + 3);
    frameBuffer.print(label);
  }

  // ---- Player sprite (idle: lower-left / attacking: sweeps across) --------
  const DigimonSprites* pArt = combat.getPlayerArt();
  if (pArt) {
    // Attack frames live in their OWN (larger) box than the idle pose, so the
    // size is chosen per frame-set rather than from one uniform spriteSize.
    bool attacking = (phase == CP_PLAYER_RESOLVE) && combat.getShowAttackArt() &&
                     pArt->attack && pArt->attackCount > 0;

    // The attack pose is too big to read as a pose standing in the corner, so
    // it CHARGES across the screen left -> right (the direction the player
    // faces) and exits right, then the idle pose reappears in its slot for the
    // rest of the resolve beat. The sweep is centred in the PLAYFIELD -- the
    // band between the enemy header and the command panel -- because the panel
    // is drawn after the sprites and would otherwise cover its lower part.
    const uint32_t SWEEP_MS = 650;         // resolve lasts ~1000ms
    const int PLAY_TOP = 20;               // enemy sprite's top edge
    const int PLAY_BOT = 76;               // command panel's top edge
    uint32_t el = attacking ? combat.getPhaseElapsed() : 0;
    bool sweeping = attacking && el < SWEEP_MS;

    const uint16_t* pframe = nullptr;
    int pw, ph, px, py;
    if (sweeping) {
      pw = digimonAttackW(pArt);
      ph = digimonAttackH(pArt);
      // Step the attack animation off the phase clock (~120ms/frame), holding
      // the last frame once they run out.
      int f = (int)(el / 120);
      if (f >= pArt->attackCount) f = pArt->attackCount - 1;
      pframe = pArt->attack[f];
      // Fully off-screen left -> fully off-screen right. Out-of-range pixels
      // are clipped by GFXcanvas16::drawPixel, so the entry/exit is partial.
      px = -pw + (int)(((uint32_t)(SCREEN_WIDTH + pw) * el) / SWEEP_MS);
      py = PLAY_TOP + (PLAY_BOT - PLAY_TOP - ph) / 2;
      if (py < PLAY_TOP) py = PLAY_TOP;    // taller than the band: top-align
    } else {
      pw = ph = pArt->spriteSize;
      if (pArt->walk && pArt->walkCount > 0) pframe = pArt->walk[0];
      // Anchor by the FEET, just above the command panel.
      px = 4;
      py = 74 - ph;
      if (py < 22) py = 22;                // never overlap the enemy HP header
    }
    // Player faces RIGHT (toward the enemy): flip the sprite.
    uint16_t ptint = (combat.getFlashTarget() == 1) ? 0xFFFF : 0;
    blitSpriteToBuffer(frameBuffer, px, py, pw, ph, pframe, true, ptint);
  }

  // ---- Lower command panel: message, HP, charge, and the action row -------
  // Taller panel using the full bottom of the screen (76..128) with each
  // element on its own vertical band so nothing overlaps (the old 40px panel
  // crammed the menu/timing on top of the footer).
  const int pnlX = 2, pnlY = 76, pnlW = 124, pnlH = SCREEN_HEIGHT - pnlY - 1;
  drawBevelPanel(pnlX, pnlY, pnlW, pnlH);

  // Message line (top band).
  frameBuffer.setTextColor(MENU_TEXT_DARK);
  frameBuffer.setCursor(pnlX + 5, pnlY + 4);
  frameBuffer.printf("%s", combat.getMessage());

  // Player HP bar + numeric (second band).
  const int pBarX = pnlX + 5, pBarY = pnlY + 14, pBarW = 70;
  drawPlateBar(pBarX, pBarY, pBarW, combat.getDisplayPlayerHp(), combat.getPlayerMaxHp(), STAT_HAPPY_COLOR);
  frameBuffer.setTextColor(MENU_TEXT_DARK);
  frameBuffer.setCursor(pBarX + pBarW + 4, pBarY - 1);
  frameBuffer.printf("%d", combat.getPlayerHp());
  // Player type badge below the HP number.
  const DigimonSprites* pa = combat.getPlayerArt();
  if (pa) {
    drawIcon(typeIcon(pa->type), pBarX + pBarW + 4, pBarY + 7, 10, typeColor(pa->type));
  }

  // Charge pips (third band), to the RIGHT of the HP bar so they never touch
  // the action row below.
  int chg = combat.getCharge();
  int chgMax = combat.getChargeMax();
  for (int i = 0; i < chgMax; i++) {
    int cx = pBarX + i * 8;
    int cy = pBarY + 8;
    frameBuffer.drawRect(cx, cy, 6, 5, STAT_FRAME_COLOR);
    if (i < chg) frameBuffer.fillRect(cx + 1, cy + 1, 4, 3, STAT_ENERGY_COLOR);
  }

  // ---- Action row (bottom band): menu OR timing bar. No footer -- the action
  // labels are self-explanatory, and a footer here collided with this row. ----
  const int rowY = pnlY + 34;
  if (phase == CP_PLAYER_MENU) {
    const char* acts[3] = { "Atk", "Grd", "Spc" };
    int sel = combat.getMenuSelection();
    for (int i = 0; i < 3; i++) {
      int ax = pnlX + 4 + i * 40;
      int aw = 38, ah = 12;
      bool on = (i == sel);
      bool locked = (i == 2 && chg < chgMax);
      // Same rounded + shadow + gradient look as the menu buttons.
      drawMenuRow(ax, rowY, aw, ah, acts[i], on);
      // Dim a locked Special by overprinting its label in the muted colour.
      if (locked) {
        frameBuffer.setTextColor(UI_TEXT_MUTED);
        frameBuffer.setCursor(ax + 5, rowY + (ah - 7) / 2);
        frameBuffer.printf("%s%s", on ? "> " : "  ", acts[i]);
      }
    }
  } else if (phase == CP_PLAYER_TIMING) {
    const int tX = pnlX + 5, tY = rowY + 1, tW = pnlW - 10, tH = 9;
    frameBuffer.drawRect(tX, tY, tW, tH, STAT_FRAME_COLOR);
    int goodLo = tX + 1 + ((tW - 2) * 30) / 100;
    int goodHi = tX + 1 + ((tW - 2) * 70) / 100;
    int perfLo = tX + 1 + ((tW - 2) * 44) / 100;
    int perfHi = tX + 1 + ((tW - 2) * 56) / 100;
    frameBuffer.fillRect(goodLo, tY + 1, goodHi - goodLo, tH - 2, MENU_PLATE_LGREY);
    frameBuffer.fillRect(perfLo, tY + 1, perfHi - perfLo, tH - 2, STAT_HAPPY_COLOR);
    int mTotal = combat.getTimingMax() > 0 ? combat.getTimingMax() : 1;
    int mx = tX + 1 + ((tW - 2) * combat.getTimingPos()) / mTotal;
    frameBuffer.fillRect(mx - 1, tY - 1, 3, tH + 2, TFT_RED);
  } else if (phase == CP_ENEMY_PARRY) {
    // Directional parry prompt: a big arrow showing WHICH button to press to
    // block the incoming hit (LEFT / RIGHT / OK-overhead).
    ParryDir dir = combat.getParryDir();
    int cxp = pnlX + pnlW / 2;          // centre of the action row
    int cyp = rowY + 6;
    frameBuffer.setTextColor(TFT_RED);
    frameBuffer.setCursor(pnlX + 4, rowY + 2);
    frameBuffer.print("BLOCK!");
    // Arrow glyph drawn as a filled triangle pointing the parry direction.
    int ax = cxp + 18;                  // arrow sits right of the BLOCK! text
    if (dir == PARRY_LEFT) {
      frameBuffer.fillTriangle(ax + 8, cyp - 6, ax + 8, cyp + 6, ax, cyp, TFT_RED);
    } else if (dir == PARRY_RIGHT) {
      frameBuffer.fillTriangle(ax, cyp - 6, ax, cyp + 6, ax + 8, cyp, TFT_RED);
    } else { // PARRY_OVER -> press OK; draw an up chevron + "OK"
      frameBuffer.fillTriangle(ax, cyp + 5, ax + 8, cyp + 5, ax + 4, cyp - 5, TFT_RED);
      frameBuffer.setTextColor(MENU_TEXT_DARK);
      frameBuffer.setCursor(ax + 12, rowY + 2);
      frameBuffer.print("OK");
    }
  }

  // ---- Damage number pop: a floating "-N" that rises & fades near the victim.
  int popAge = combat.getPopAge();
  if (popAge >= 0) {
    const char* pt = combat.getPopText();
    int rise = popAge / 60;                 // rises ~1px per 60ms
    bool blink = (popAge / 90) % 2 == 0;    // simple fade via blink near the end
    if (popAge < 500 || blink) {
      int tx, ty;
      if (combat.getPopTarget() == 2) { tx = SCREEN_WIDTH - 34; ty = 34 - rise; }  // over enemy
      else                            { tx = 10;                ty = 60 - rise; }  // over player
      frameBuffer.setTextSize(1);
      // outline for readability, then the number in a punchy colour.
      frameBuffer.setTextColor(TFT_BLACK);
      frameBuffer.setCursor(tx + 1, ty + 1);  frameBuffer.print(pt);
      frameBuffer.setTextColor(combat.getPopTarget() == 2 ? 0xFFE0 : UI_DANGER);
      frameBuffer.setCursor(tx, ty);          frameBuffer.print(pt);
    }
  }

  gfx = nullptr;
  // Cheap heavy-hit shake: nudge the single frame-buffer blit by a few px.
  // Reuses the existing origin offset -> just ONE push at an offset, no
  // recomposite loop (true multi-frame shake was rejected as too costly).
  if (combat.getShake()) {
    int ox = originX, oy = originY;
    int jitter = (millis() / 40) % 2 ? 2 : -2;
    originX += jitter; originY += (jitter > 0 ? -1 : 1);
    pushFrame();
    originX = ox; originY = oy;
  } else {
    pushFrame();
  }
}
