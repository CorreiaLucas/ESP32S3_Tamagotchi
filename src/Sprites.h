#ifndef SPRITES_H
#define SPRITES_H

#include <Arduino.h>

// --------------------------------------------------------------------------
// Shared, digimon-INDEPENDENT assets only.
// Per-digimon pet sprites (walk/sleep/eat/play/sad/dead/happy/attack/profile)
// now live in generated <name>Sprites.cpp/.h and are wired through
// DigimonRegistry. Regenerate them with tools/build_digimon_sprites.py.
// --------------------------------------------------------------------------

extern const uint16_t treat_frame[] PROGMEM;   // 16x16 minigame treat
extern const uint16_t poop_frame[] PROGMEM;    // 20x20 poop

// Full-screen 128x128 background (RGB565)
extern const uint16_t background_data_forest[] PROGMEM;

#endif
