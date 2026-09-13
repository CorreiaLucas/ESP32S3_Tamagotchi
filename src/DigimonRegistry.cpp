#include "DigimonRegistry.h"
#include "terriermonSprites.h"
#include "gargomonSprites.h"
#include <string.h>

// --------------------------------------------------------------------------
// Single-frame actions are shipped as a lone symbol (e.g. terriermon_sleep).
// Wrap each in a 1-element frame table so it plugs into the DigimonSprites
// table pointers uniformly.
// --------------------------------------------------------------------------
static const uint16_t* const terriermon_sleep_frames[1] = { terriermon_sleep };

// ==========================================================================
//  TERRIERMON
//  Ships: walk, walkback, sleep, happy, attack, profile.
//  Missing -> fallback:  eat/play -> happy,  sad -> walk,  dead -> sleep.
// ==========================================================================
const DigimonSprites DIGIMON_terriermon = {
  "terriermon",
  TERRIERMON_SPRITE_SIZE,
  TERRIERMON_PROFILE_SIZE,
  45,   // realHeightCm (Terriermon ~40-50cm)
  terriermon_walk_frames,      3,   // walk
  terriermon_walkback_frames,  3,   // walkBack
  terriermon_sleep_frames,     1,   // sleep
  terriermon_happy_frames,     3,   // eat      (fallback: happy)
  terriermon_happy_frames,     3,   // play     (fallback: happy)
  terriermon_walk_frames,      3,   // sad      (fallback: walk)
  terriermon_sleep_frames,     1,   // dead     (fallback: sleep)
  terriermon_happy_frames,     3,   // happy
  terriermon_attack_frames,    2,   // attack
  terriermon_profile               // profile
};

// ==========================================================================
//  GARGOMON
//  Ships: walk, walkback, happy, profile.
//  Missing -> fallback:  sleep -> walk,  eat/play -> happy,  sad -> walk,
//                        dead -> walk,   attack -> happy.
// ==========================================================================
const DigimonSprites DIGIMON_gargomon = {
  "gargomon",
  GARGOMON_SPRITE_SIZE,
  GARGOMON_PROFILE_SIZE,
  150,  // realHeightCm (Gargomon ~1.5m)
  gargomon_walk_frames,      3,   // walk
  gargomon_walkback_frames,  3,   // walkBack
  gargomon_walk_frames,      3,   // sleep    (fallback: walk)
  gargomon_happy_frames,     3,   // eat      (fallback: happy)
  gargomon_happy_frames,     3,   // play     (fallback: happy)
  gargomon_walk_frames,      3,   // sad      (fallback: walk)
  gargomon_walk_frames,      3,   // dead     (fallback: walk)
  gargomon_happy_frames,     3,   // happy
  gargomon_happy_frames,     3,   // attack   (fallback: happy)
  gargomon_profile               // profile
};

// --------------------------------------------------------------------------
const DigimonSprites* const DIGIMON_ALL[] = {
  &DIGIMON_terriermon,
  &DIGIMON_gargomon,
};
const int DIGIMON_COUNT = sizeof(DIGIMON_ALL) / sizeof(DIGIMON_ALL[0]);

const DigimonSprites* digimonByName(const char* name) {
  if (!name) return nullptr;
  for (int i = 0; i < DIGIMON_COUNT; i++) {
    if (strcmp(DIGIMON_ALL[i]->name, name) == 0) return DIGIMON_ALL[i];
  }
  return nullptr;
}

