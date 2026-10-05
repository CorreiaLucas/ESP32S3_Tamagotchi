#include "DigimonRegistry.h"
#include "chibomonSprites.h"
#include "tokomonSprites.h"
#include "dorimonSprites.h"
#include "tanemonSprites.h"
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
  { &DIGIMON_gargomon, /*maxHp*/150, /*ap*/25, /*dp*/20, /*ageDays*/2, /*happy*/50, /*hunger*/0, /*level*/5, /*INT*/30, /*SPD*/25 },
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
  TYPE_VACCINE,                               // combat type
  100, 10, 10,                        // baseMaxHp, baseAp, baseDp (Rookie)
  10, 10,                               // baseIntel, baseSpeed
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
  TYPE_VACCINE,                               // combat type
  250, 30, 25,                        // baseMaxHp, baseAp, baseDp (Champion)
  10, 10,                               // baseIntel, baseSpeed
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
  TYPE_DATA,                               // combat type
  120, 10, 10,                        // baseMaxHp, baseAp, baseDp
  10, 10,                               // baseIntel, baseSpeed
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
  TYPE_DATA,                               // combat type
  120, 10, 10,                        // baseMaxHp, baseAp, baseDp
  10, 10,                               // baseIntel, baseSpeed
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
  TYPE_VIRUS,                               // combat type
  120, 10, 10,                        // baseMaxHp, baseAp, baseDp
  10, 10,                               // baseIntel, baseSpeed
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
  TYPE_VIRUS,                               // combat type
  120, 10, 10,                        // baseMaxHp, baseAp, baseDp
  10, 10,                               // baseIntel, baseSpeed
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
  TYPE_VIRUS,                               // combat type
  120, 10, 10,                        // baseMaxHp, baseAp, baseDp
  10, 10,                               // baseIntel, baseSpeed
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
  TYPE_DATA,                               // combat type
  0, 0, 0,                        // baseMaxHp, baseAp, baseDp
  10, 10,                               // baseIntel, baseSpeed
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

// >>> AUTO-REGISTER tanemon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_tanemon = {
  "tanemon",
  TANEMON_SPRITE_SIZE,
  TANEMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                               // combat type
  0, 0, 0,                        // baseMaxHp, baseAp, baseDp
  10, 10,                               // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  tanemon_walk_frames,      3,   // walk
  tanemon_walkback_frames,  3,   // walkBack
  tanemon_walk_frames,     3,   // sleep
  tanemon_happy_frames,     3,   // eat
  tanemon_happy_frames,     3,   // play
  tanemon_walk_frames,      3,   // sad
  tanemon_walk_frames,     3,   // dead
  tanemon_happy_frames,     3,   // happy
  tanemon_attack_frames,    5,   // attack
  tanemon_profile               // profile
};
// <<< AUTO-REGISTER tanemon END

// >>> AUTO-REGISTER dorimon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_dorimon = {
  "dorimon",
  DORIMON_SPRITE_SIZE,
  DORIMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                               // combat type
  0, 0, 0,                        // baseMaxHp, baseAp, baseDp
  10, 10,                               // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  dorimon_walk_frames,      3,   // walk
  dorimon_walkback_frames,  3,   // walkBack
  dorimon_walk_frames,     3,   // sleep
  dorimon_happy_frames,     3,   // eat
  dorimon_happy_frames,     3,   // play
  dorimon_walk_frames,      3,   // sad
  dorimon_walk_frames,     3,   // dead
  dorimon_happy_frames,     3,   // happy
  dorimon_attack_frames,    4,   // attack
  dorimon_profile               // profile
};
// <<< AUTO-REGISTER dorimon END

// >>> AUTO-REGISTER tokomon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_tokomon = {
  "tokomon",
  TOKOMON_SPRITE_SIZE,
  TOKOMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                               // combat type
  0, 0, 0,                        // baseMaxHp, baseAp, baseDp
  10, 10,                               // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  tokomon_walk_frames,      3,   // walk
  tokomon_walkback_frames,  3,   // walkBack
  tokomon_walk_frames,     3,   // sleep
  tokomon_happy_frames,     3,   // eat
  tokomon_happy_frames,     3,   // play
  tokomon_walk_frames,      3,   // sad
  tokomon_walk_frames,     3,   // dead
  tokomon_happy_frames,     3,   // happy
  tokomon_attack_frames,    3,   // attack
  tokomon_profile               // profile
};
// <<< AUTO-REGISTER tokomon END

// >>> AUTO-REGISTER chibomon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_chibomon = {
  "chibomon",
  CHIBOMON_SPRITE_SIZE,
  CHIBOMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                               // combat type
  0, 0, 0,                        // baseMaxHp, baseAp, baseDp
  10, 10,                               // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  chibomon_walk_frames,      3,   // walk
  chibomon_walkback_frames,  3,   // walkBack
  chibomon_walk_frames,     3,   // sleep
  chibomon_happy_frames,     3,   // eat
  chibomon_happy_frames,     3,   // play
  chibomon_walk_frames,      3,   // sad
  chibomon_walk_frames,     3,   // dead
  chibomon_happy_frames,     3,   // happy
  chibomon_happy_frames,    3,   // attack
  chibomon_profile               // profile
};
// <<< AUTO-REGISTER chibomon END

// --------------------------------------------------------------------------
const DigimonSprites* const DIGIMON_ALL[] = {
  &DIGIMON_tokomon,
  &DIGIMON_tanemon,
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
                              int ageDays, int happiness, int hunger,
                              int level, int intelligence, int speed) {
  if (req.minMaxHp       > 0 && maxHp        < req.minMaxHp)       return false;
  if (req.minAp          > 0 && ap           < req.minAp)          return false;
  if (req.minDp          > 0 && dp           < req.minDp)          return false;
  if (req.minAgeDays     > 0 && ageDays      < req.minAgeDays)     return false;
  if (req.minHappiness   > 0 && happiness    < req.minHappiness)   return false;
  if (req.minHunger      > 0 && hunger       < req.minHunger)      return false;
  if (req.minLevel       > 0 && level        < req.minLevel)       return false;
  if (req.minIntelligence> 0 && intelligence < req.minIntelligence)return false;
  if (req.minSpeed       > 0 && speed        < req.minSpeed)       return false;
  return true;
}


float typeMultiplier(DigimonType attacker, DigimonType defender) {
  // Official triangle: Vaccine > Virus > Data > Vaccine.
  if (attacker == defender) return 1.0f;
  bool advantage =
      (attacker == TYPE_VACCINE && defender == TYPE_VIRUS) ||
      (attacker == TYPE_VIRUS   && defender == TYPE_DATA)  ||
      (attacker == TYPE_DATA    && defender == TYPE_VACCINE);
  return advantage ? 1.5f : 0.75f;
}
