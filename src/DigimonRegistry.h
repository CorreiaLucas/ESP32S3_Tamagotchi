#ifndef DIGIMON_REGISTRY_H
#define DIGIMON_REGISTRY_H

#include <Arduino.h>

// ==========================================================================
//  DIGIMON REGISTRY  (digivolution core)
//  Each Digimon's sprite set is described by one DigimonSprites entry: a
//  uniform on-screen size plus a frame table (+ frame count) for every action
//  the firmware can play. Digivolution is a single pointer swap:
//      cat.setDigimon(&DIGIMON_gargomon);
//  Actions a Digimon doesn't ship art for fall back to a sensible substitute
//  (handled where the table is populated in DigimonRegistry.cpp), so the
//  firmware can always call any action without a null check.
// ==========================================================================

struct DigimonSprites {
  const char* name;
  int spriteSize;      // size (px) the ACTION art is STORED at (canvas resolution)
  int profileSize;     // square size for the profile sprite (px)
  int realHeightCm;    // lore height (cm). Kept for future use (e.g. background
                       // zoom for very tall Digimon). Does NOT affect sprite
                       // size -- sprites draw 1:1 at their stored spriteSize.

  // Each action: pointer to a frame table + how many frames it has.
  const uint16_t* const* walk;      int walkCount;
  const uint16_t* const* walkBack;  int walkBackCount;
  const uint16_t* const* sleep;     int sleepCount;
  const uint16_t* const* eat;       int eatCount;
  const uint16_t* const* play;      int playCount;
  const uint16_t* const* sad;       int sadCount;
  const uint16_t* const* dead;      int deadCount;
  const uint16_t* const* happy;     int happyCount;
  const uint16_t* const* attack;    int attackCount;

  const uint16_t* profile;          // single frame (may be nullptr)
};

// Registered Digimon (add one extern per generated <name>Sprites set).
extern const DigimonSprites DIGIMON_terriermon;
extern const DigimonSprites DIGIMON_gargomon;

// Ordered list for iteration / digivolution chains, plus a name lookup.
extern const DigimonSprites* const DIGIMON_ALL[];
extern const int DIGIMON_COUNT;

// Return the registered Digimon with this name, or nullptr if unknown.
const DigimonSprites* digimonByName(const char* name);

#endif
