#include "DigimonRegistry.h"
#include "pandamonSprites.h"
#include "pagumonSprites.h"
#include "kuramonSprites.h"
#include "kapurimonSprites.h"
#include "koromonSprites.h"
#include "tsunomonSprites.h"
#include "terriermonSprites.h"
#include "gargomonSprites.h"
#include <string.h>

// --------------------------------------------------------------------------
// Single-frame actions are shipped as a lone symbol (e.g. terriermon_sleep).
// Wrap each in a 1-element frame table so it plugs into the DigimonSprites
// table pointers uniformly.
// --------------------------------------------------------------------------
static const uint16_t* const terriermon_sleep_frames[1] = { terriermon_sleep };

// --------------------------------------------------------------------------
// EVOLUTIONS
//   Terriermon (Rookie) -> Gargomon (Champion) once trained + cared enough.
//   Requirement fields are min-thresholds; 0 = ignore that field. Tune freely.
//   (&DIGIMON_gargomon is declared extern in the header, so referencing its
//   address here -- before its definition below -- is valid.)
// --------------------------------------------------------------------------
static const EvolutionReq terriermon_evolutions[] = {
  { &DIGIMON_gargomon, /*maxHp*/150, /*ap*/25, /*dp*/20, /*ageDays*/2, /*happy*/50, /*hunger*/0 },
};

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
  100, 10, 10,                        // baseMaxHp, baseAp, baseDp (Rookie)
  terriermon_evolutions, 1,           // evolutions, count
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
  250, 30, 25,                        // baseMaxHp, baseAp, baseDp (Champion)
  nullptr, 0,                         // evolutions, count (final form for now)
  gargomon_walk_frames,      4,   // walk
  gargomon_walkback_frames,  4,   // walkBack
  gargomon_walk_frames,      4,   // sleep    (fallback: walk)
  gargomon_happy_frames,     3,   // eat      (fallback: happy)
  gargomon_happy_frames,     3,   // play     (fallback: happy)
  gargomon_walk_frames,      4,   // sad      (fallback: walk)
  gargomon_walk_frames,      4,   // dead     (fallback: walk)
  gargomon_happy_frames,     3,   // happy
  gargomon_happy_frames,     3,   // attack   (fallback: happy)
  gargomon_profile               // profile
};

// >>> AUTO-REGISTER tsunomon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_tsunomon = {
  "tsunomon",
  TSUNOMON_SPRITE_SIZE,
  TSUNOMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  120, 10, 10,                        // baseMaxHp, baseAp, baseDp
  nullptr, 0,                         // evolutions, count (wire by hand)
  tsunomon_walk_frames,      3,   // walk
  tsunomon_walkback_frames,  3,   // walkBack
  tsunomon_walk_frames,     3,   // sleep
  tsunomon_happy_frames,     3,   // eat
  tsunomon_happy_frames,     3,   // play
  tsunomon_walk_frames,      3,   // sad
  tsunomon_walk_frames,     3,   // dead
  tsunomon_happy_frames,     3,   // happy
  tsunomon_attack_frames,    4,   // attack
  tsunomon_profile               // profile
};
// <<< AUTO-REGISTER tsunomon END

// >>> AUTO-REGISTER koromon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_koromon = {
  "koromon",
  KOROMON_SPRITE_SIZE,
  KOROMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  120, 10, 10,                        // baseMaxHp, baseAp, baseDp
  nullptr, 0,                         // evolutions, count (wire by hand)
  koromon_walk_frames,      3,   // walk
  koromon_walkback_frames,  3,   // walkBack
  koromon_walk_frames,     3,   // sleep
  koromon_happy_frames,     2,   // eat
  koromon_happy_frames,     2,   // play
  koromon_walk_frames,      3,   // sad
  koromon_walk_frames,     3,   // dead
  koromon_happy_frames,     2,   // happy
  koromon_attack_frames,    5,   // attack
  koromon_profile               // profile
};
// <<< AUTO-REGISTER koromon END

// >>> AUTO-REGISTER kapurimon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_kapurimon = {
  "kapurimon",
  KAPURIMON_SPRITE_SIZE,
  KAPURIMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  120, 10, 10,                        // baseMaxHp, baseAp, baseDp
  nullptr, 0,                         // evolutions, count (wire by hand)
  kapurimon_walk_frames,      3,   // walk
  kapurimon_walkback_frames,  3,   // walkBack
  kapurimon_walk_frames,     3,   // sleep
  kapurimon_happy_frames,     3,   // eat
  kapurimon_happy_frames,     3,   // play
  kapurimon_walk_frames,      3,   // sad
  kapurimon_walk_frames,     3,   // dead
  kapurimon_happy_frames,     3,   // happy
  kapurimon_attack_frames,    3,   // attack
  kapurimon_profile               // profile
};
// <<< AUTO-REGISTER kapurimon END

// >>> AUTO-REGISTER kuramon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_kuramon = {
  "kuramon",
  KURAMON_SPRITE_SIZE,
  KURAMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  120, 10, 10,                        // baseMaxHp, baseAp, baseDp
  nullptr, 0,                         // evolutions, count (wire by hand)
  kuramon_walk_frames,      3,   // walk
  kuramon_walkback_frames,  3,   // walkBack
  kuramon_walk_frames,     3,   // sleep
  kuramon_happy_frames,     3,   // eat
  kuramon_happy_frames,     3,   // play
  kuramon_walk_frames,      3,   // sad
  kuramon_walk_frames,     3,   // dead
  kuramon_happy_frames,     3,   // happy
  kuramon_attack_frames,    3,   // attack
  kuramon_profile               // profile
};
// <<< AUTO-REGISTER kuramon END

// >>> AUTO-REGISTER pagumon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_pagumon = {
  "pagumon",
  PAGUMON_SPRITE_SIZE,
  PAGUMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  120, 10, 10,                        // baseMaxHp, baseAp, baseDp
  nullptr, 0,                         // evolutions, count (wire by hand)
  pagumon_walk_frames,      3,   // walk
  pagumon_walkback_frames,  3,   // walkBack
  pagumon_walk_frames,     3,   // sleep
  pagumon_happy_frames,     3,   // eat
  pagumon_happy_frames,     3,   // play
  pagumon_walk_frames,      3,   // sad
  pagumon_walk_frames,     3,   // dead
  pagumon_happy_frames,     3,   // happy
  pagumon_attack_frames,    5,   // attack
  pagumon_profile               // profile
};
// <<< AUTO-REGISTER pagumon END

// >>> AUTO-REGISTER pandamon BEGIN (build_digimon_sprites.py)
static const uint16_t* const pandamon_idle_frames[1] = { pandamon_idle };
const DigimonSprites DIGIMON_pandamon = {
  "pandamon",
  PANDAMON_SPRITE_SIZE,
  PANDAMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  0, 0, 0,                        // baseMaxHp, baseAp, baseDp
  nullptr, 0,                         // evolutions, count (wire by hand)
  nullptr,      0,   // walk
  nullptr,  0,   // walkBack
  nullptr,     0,   // sleep
  pandamon_happy_frames,     3,   // eat
  pandamon_happy_frames,     3,   // play
  nullptr,      0,   // sad
  nullptr,     0,   // dead
  pandamon_happy_frames,     3,   // happy
  pandamon_happy_frames,    3,   // attack
  pandamon_profile               // profile
};
// <<< AUTO-REGISTER pandamon END

// --------------------------------------------------------------------------
const DigimonSprites* const DIGIMON_ALL[] = {
  &DIGIMON_pandamon,
  &DIGIMON_pagumon,
  &DIGIMON_kuramon,
  &DIGIMON_kapurimon,
  &DIGIMON_koromon,
  &DIGIMON_tsunomon,
  &DIGIMON_terriermon,
  &DIGIMON_gargomon,
  &DIGIMON_chibomon,
  &DIGIMON_dorimon,
};
const int DIGIMON_COUNT = sizeof(DIGIMON_ALL) / sizeof(DIGIMON_ALL[0]);

const DigimonSprites* digimonByName(const char* name) {
  if (!name) return nullptr;
  for (int i = 0; i < DIGIMON_COUNT; i++) {
    if (strcmp(DIGIMON_ALL[i]->name, name) == 0) return DIGIMON_ALL[i];
  }
  return nullptr;
}

bool evolutionRequirementsMet(const EvolutionReq& req,
                              int maxHp, int ap, int dp,
                              int ageDays, int happiness, int hunger) {
  if (req.minMaxHp    > 0 && maxHp     < req.minMaxHp)    return false;
  if (req.minAp       > 0 && ap        < req.minAp)       return false;
  if (req.minDp       > 0 && dp        < req.minDp)       return false;
  if (req.minAgeDays  > 0 && ageDays   < req.minAgeDays)  return false;
  if (req.minHappiness> 0 && happiness < req.minHappiness)return false;
  if (req.minHunger   > 0 && hunger    < req.minHunger)   return false;
  return true;
}
