#include "DigimonRegistry.h"
#include "wormmonSprites.h"
#include "veemonSprites.h"
#include "tentomonSprites.h"
#include "tapirmonSprites.h"
#include "snowagumonSprites.h"
#include "shadowtoyagumonSprites.h"
#include "salamonSprites.h"
#include "renamonSprites.h"
#include "penguinmonSprites.h"
#include "pawnchessmonblackSprites.h"
#include "pawnchessmon_whiteSprites.h"
#include "patamonSprites.h"
#include "palmonSprites.h"
#include "otamamonSprites.h"
#include "muchomonSprites.h"
#include "lopmonSprites.h"
#include "lalamonSprites.h"
#include "kumamonSprites.h"
#include "kudamonSprites.h"
#include "kotemonSprites.h"
#include "keramonSprites.h"
#include "impmonSprites.h"
#include "hawkmonSprites.h"
#include "guilmonSprites.h"
#include "gotsumonSprites.h"
#include "goburimonSprites.h"
#include "gaomonSprites.h"
#include "gabumonSprites.h"
#include "floramonSprites.h"
#include "dorumonSprites.h"
#include "demidevimonSprites.h"
#include "biyomonSprites.h"
#include "betamonSprites.h"
#include "aruraumonSprites.h"
#include "armadillomonSprites.h"
#include "agumonSprites.h"
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

// >>> AUTO-REGISTER agumon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_agumon = {
  "agumon",
  AGUMON_SPRITE_SIZE,
  AGUMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  agumon_walk_frames,      3,   // walk
  agumon_walkback_frames,  3,   // walkBack
  agumon_walk_frames,     3,   // sleep
  agumon_happy_frames,     3,   // eat
  agumon_happy_frames,     3,   // play
  agumon_walk_frames,      3,   // sad
  agumon_walk_frames,     3,   // dead
  agumon_happy_frames,     3,   // happy
  agumon_attack_frames,    5,   // attack
  agumon_profile,               // profile
  AGUMON_ATTACK_W, AGUMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER agumon END

// >>> AUTO-REGISTER armadillomon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_armadillomon = {
  "armadillomon",
  ARMADILLOMON_SPRITE_SIZE,
  ARMADILLOMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  armadillomon_walk_frames,      3,   // walk
  armadillomon_walkback_frames,  3,   // walkBack
  armadillomon_walk_frames,     3,   // sleep
  armadillomon_happy_frames,     3,   // eat
  armadillomon_happy_frames,     3,   // play
  armadillomon_walk_frames,      3,   // sad
  armadillomon_walk_frames,     3,   // dead
  armadillomon_happy_frames,     3,   // happy
  armadillomon_attack_frames,    3,   // attack
  armadillomon_profile,               // profile
  ARMADILLOMON_ATTACK_W, ARMADILLOMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER armadillomon END

// >>> AUTO-REGISTER aruraumon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_aruraumon = {
  "aruraumon",
  ARURAUMON_SPRITE_SIZE,
  ARURAUMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  aruraumon_walk_frames,      3,   // walk
  aruraumon_walkback_frames,  3,   // walkBack
  aruraumon_walk_frames,     3,   // sleep
  aruraumon_happy_frames,     3,   // eat
  aruraumon_happy_frames,     3,   // play
  aruraumon_walk_frames,      3,   // sad
  aruraumon_walk_frames,     3,   // dead
  aruraumon_happy_frames,     3,   // happy
  aruraumon_attack_frames,    3,   // attack
  aruraumon_profile,               // profile
  ARURAUMON_ATTACK_W, ARURAUMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER aruraumon END

// >>> AUTO-REGISTER betamon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_betamon = {
  "betamon",
  BETAMON_SPRITE_SIZE,
  BETAMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  betamon_walk_frames,      3,   // walk
  betamon_walkback_frames,  3,   // walkBack
  betamon_walk_frames,     3,   // sleep
  betamon_happy_frames,     3,   // eat
  betamon_happy_frames,     3,   // play
  betamon_walk_frames,      3,   // sad
  betamon_walk_frames,     3,   // dead
  betamon_happy_frames,     3,   // happy
  betamon_attack_frames,    4,   // attack
  betamon_profile,               // profile
  BETAMON_ATTACK_W, BETAMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER betamon END

// >>> AUTO-REGISTER biyomon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_biyomon = {
  "biyomon",
  BIYOMON_SPRITE_SIZE,
  BIYOMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  biyomon_walk_frames,      3,   // walk
  biyomon_walkback_frames,  3,   // walkBack
  biyomon_walk_frames,     3,   // sleep
  biyomon_happy_frames,     3,   // eat
  biyomon_happy_frames,     3,   // play
  biyomon_walk_frames,      3,   // sad
  biyomon_walk_frames,     3,   // dead
  biyomon_happy_frames,     3,   // happy
  biyomon_attack_frames,    3,   // attack
  biyomon_profile,               // profile
  BIYOMON_ATTACK_W, BIYOMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER biyomon END

// >>> AUTO-REGISTER demidevimon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_demidevimon = {
  "demidevimon",
  DEMIDEVIMON_SPRITE_SIZE,
  DEMIDEVIMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  demidevimon_walk_frames,      3,   // walk
  demidevimon_walkback_frames,  3,   // walkBack
  demidevimon_walk_frames,     3,   // sleep
  demidevimon_happy_frames,     3,   // eat
  demidevimon_happy_frames,     3,   // play
  demidevimon_walk_frames,      3,   // sad
  demidevimon_walk_frames,     3,   // dead
  demidevimon_happy_frames,     3,   // happy
  demidevimon_attack_frames,    4,   // attack
  demidevimon_profile,               // profile
  DEMIDEVIMON_ATTACK_W, DEMIDEVIMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER demidevimon END

// >>> AUTO-REGISTER dorumon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_dorumon = {
  "dorumon",
  DORUMON_SPRITE_SIZE,
  DORUMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  dorumon_walk_frames,      3,   // walk
  dorumon_walkback_frames,  3,   // walkBack
  dorumon_walk_frames,     3,   // sleep
  dorumon_happy_frames,     3,   // eat
  dorumon_happy_frames,     3,   // play
  dorumon_walk_frames,      3,   // sad
  dorumon_walk_frames,     3,   // dead
  dorumon_happy_frames,     3,   // happy
  dorumon_attack_frames,    3,   // attack
  dorumon_profile,               // profile
  DORUMON_ATTACK_W, DORUMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER dorumon END

// >>> AUTO-REGISTER floramon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_floramon = {
  "floramon",
  FLORAMON_SPRITE_SIZE,
  FLORAMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  floramon_walk_frames,      3,   // walk
  floramon_walkback_frames,  3,   // walkBack
  floramon_walk_frames,     3,   // sleep
  floramon_happy_frames,     3,   // eat
  floramon_happy_frames,     3,   // play
  floramon_walk_frames,      3,   // sad
  floramon_walk_frames,     3,   // dead
  floramon_happy_frames,     3,   // happy
  floramon_attack_frames,    3,   // attack
  floramon_profile,               // profile
  FLORAMON_ATTACK_W, FLORAMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER floramon END

// >>> AUTO-REGISTER gabumon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_gabumon = {
  "gabumon",
  GABUMON_SPRITE_SIZE,
  GABUMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  gabumon_walk_frames,      3,   // walk
  gabumon_walkback_frames,  3,   // walkBack
  gabumon_walk_frames,     3,   // sleep
  gabumon_happy_frames,     3,   // eat
  gabumon_happy_frames,     3,   // play
  gabumon_walk_frames,      3,   // sad
  gabumon_walk_frames,     3,   // dead
  gabumon_happy_frames,     3,   // happy
  gabumon_attack_frames,    3,   // attack
  gabumon_profile,               // profile
  GABUMON_ATTACK_W, GABUMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER gabumon END

// >>> AUTO-REGISTER gaomon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_gaomon = {
  "gaomon",
  GAOMON_SPRITE_SIZE,
  GAOMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  gaomon_walk_frames,      3,   // walk
  gaomon_walkback_frames,  3,   // walkBack
  gaomon_walk_frames,     3,   // sleep
  gaomon_happy_frames,     2,   // eat
  gaomon_happy_frames,     2,   // play
  gaomon_walk_frames,      3,   // sad
  gaomon_walk_frames,     3,   // dead
  gaomon_happy_frames,     2,   // happy
  gaomon_attack_frames,    3,   // attack
  gaomon_profile,               // profile
  GAOMON_ATTACK_W, GAOMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER gaomon END

// >>> AUTO-REGISTER goburimon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_goburimon = {
  "goburimon",
  GOBURIMON_SPRITE_SIZE,
  GOBURIMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  goburimon_walk_frames,      3,   // walk
  goburimon_walkback_frames,  3,   // walkBack
  goburimon_walk_frames,     3,   // sleep
  goburimon_happy_frames,     3,   // eat
  goburimon_happy_frames,     3,   // play
  goburimon_walk_frames,      3,   // sad
  goburimon_walk_frames,     3,   // dead
  goburimon_happy_frames,     3,   // happy
  goburimon_attack_frames,    3,   // attack
  goburimon_profile,               // profile
  GOBURIMON_ATTACK_W, GOBURIMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER goburimon END

// >>> AUTO-REGISTER gotsumon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_gotsumon = {
  "gotsumon",
  GOTSUMON_SPRITE_SIZE,
  GOTSUMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  gotsumon_walk_frames,      3,   // walk
  gotsumon_walkback_frames,  3,   // walkBack
  gotsumon_walk_frames,     3,   // sleep
  gotsumon_happy_frames,     3,   // eat
  gotsumon_happy_frames,     3,   // play
  gotsumon_walk_frames,      3,   // sad
  gotsumon_walk_frames,     3,   // dead
  gotsumon_happy_frames,     3,   // happy
  gotsumon_attack_frames,    3,   // attack
  gotsumon_profile,               // profile
  GOTSUMON_ATTACK_W, GOTSUMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER gotsumon END

// >>> AUTO-REGISTER guilmon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_guilmon = {
  "guilmon",
  GUILMON_SPRITE_SIZE,
  GUILMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  guilmon_walk_frames,      4,   // walk
  guilmon_walkback_frames,  4,   // walkBack
  guilmon_walk_frames,     4,   // sleep
  guilmon_walk_frames,     4,   // eat
  guilmon_walk_frames,     4,   // play
  guilmon_walk_frames,      4,   // sad
  guilmon_walk_frames,     4,   // dead
  guilmon_walk_frames,     4,   // happy
  guilmon_attack_frames,    3,   // attack
  nullptr,               // profile
  GUILMON_ATTACK_W, GUILMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER guilmon END

// >>> AUTO-REGISTER hawkmon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_hawkmon = {
  "hawkmon",
  HAWKMON_SPRITE_SIZE,
  HAWKMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  hawkmon_walk_frames,      3,   // walk
  hawkmon_walkback_frames,  3,   // walkBack
  hawkmon_walk_frames,     3,   // sleep
  hawkmon_happy_frames,     3,   // eat
  hawkmon_happy_frames,     3,   // play
  hawkmon_walk_frames,      3,   // sad
  hawkmon_walk_frames,     3,   // dead
  hawkmon_happy_frames,     3,   // happy
  hawkmon_attack_frames,    3,   // attack
  hawkmon_profile,               // profile
  HAWKMON_ATTACK_W, HAWKMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER hawkmon END

// >>> AUTO-REGISTER impmon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_impmon = {
  "impmon",
  IMPMON_SPRITE_SIZE,
  IMPMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  impmon_walk_frames,      3,   // walk
  impmon_walkback_frames,  3,   // walkBack
  impmon_walk_frames,     3,   // sleep
  impmon_happy_frames,     3,   // eat
  impmon_happy_frames,     3,   // play
  impmon_walk_frames,      3,   // sad
  impmon_walk_frames,     3,   // dead
  impmon_happy_frames,     3,   // happy
  impmon_attack_frames,    3,   // attack
  impmon_profile,               // profile
  IMPMON_ATTACK_W, IMPMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER impmon END

// >>> AUTO-REGISTER keramon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_keramon = {
  "keramon",
  KERAMON_SPRITE_SIZE,
  KERAMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  keramon_walk_frames,      3,   // walk
  keramon_walkback_frames,  3,   // walkBack
  keramon_walk_frames,     3,   // sleep
  keramon_happy_frames,     3,   // eat
  keramon_happy_frames,     3,   // play
  keramon_walk_frames,      3,   // sad
  keramon_walk_frames,     3,   // dead
  keramon_happy_frames,     3,   // happy
  keramon_attack_frames,    3,   // attack
  keramon_profile,               // profile
  KERAMON_ATTACK_W, KERAMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER keramon END

// >>> AUTO-REGISTER kotemon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_kotemon = {
  "kotemon",
  KOTEMON_SPRITE_SIZE,
  KOTEMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  kotemon_walk_frames,      3,   // walk
  kotemon_walkback_frames,  3,   // walkBack
  kotemon_walk_frames,     3,   // sleep
  kotemon_happy_frames,     3,   // eat
  kotemon_happy_frames,     3,   // play
  kotemon_walk_frames,      3,   // sad
  kotemon_walk_frames,     3,   // dead
  kotemon_happy_frames,     3,   // happy
  kotemon_attack_frames,    3,   // attack
  kotemon_profile,               // profile
  KOTEMON_ATTACK_W, KOTEMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER kotemon END

// >>> AUTO-REGISTER kudamon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_kudamon = {
  "kudamon",
  KUDAMON_SPRITE_SIZE,
  KUDAMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  kudamon_walk_frames,      3,   // walk
  kudamon_walkback_frames,  3,   // walkBack
  kudamon_walk_frames,     3,   // sleep
  kudamon_happy_frames,     3,   // eat
  kudamon_happy_frames,     3,   // play
  kudamon_walk_frames,      3,   // sad
  kudamon_walk_frames,     3,   // dead
  kudamon_happy_frames,     3,   // happy
  kudamon_attack_frames,    3,   // attack
  kudamon_profile,               // profile
  KUDAMON_ATTACK_W, KUDAMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER kudamon END

// >>> AUTO-REGISTER kumamon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_kumamon = {
  "kumamon",
  KUMAMON_SPRITE_SIZE,
  KUMAMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  kumamon_walk_frames,      3,   // walk
  kumamon_walkback_frames,  3,   // walkBack
  kumamon_walk_frames,     3,   // sleep
  kumamon_happy_frames,     3,   // eat
  kumamon_happy_frames,     3,   // play
  kumamon_walk_frames,      3,   // sad
  kumamon_walk_frames,     3,   // dead
  kumamon_happy_frames,     3,   // happy
  kumamon_attack_frames,    3,   // attack
  kumamon_profile,               // profile
  KUMAMON_ATTACK_W, KUMAMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER kumamon END

// >>> AUTO-REGISTER lalamon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_lalamon = {
  "lalamon",
  LALAMON_SPRITE_SIZE,
  LALAMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  lalamon_walk_frames,      3,   // walk
  lalamon_walkback_frames,  3,   // walkBack
  lalamon_walk_frames,     3,   // sleep
  lalamon_happy_frames,     3,   // eat
  lalamon_happy_frames,     3,   // play
  lalamon_walk_frames,      3,   // sad
  lalamon_walk_frames,     3,   // dead
  lalamon_happy_frames,     3,   // happy
  lalamon_attack_frames,    3,   // attack
  lalamon_profile,               // profile
  LALAMON_ATTACK_W, LALAMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER lalamon END

// >>> AUTO-REGISTER lopmon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_lopmon = {
  "lopmon",
  LOPMON_SPRITE_SIZE,
  LOPMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  lopmon_walk_frames,      3,   // walk
  lopmon_walkback_frames,  3,   // walkBack
  lopmon_walk_frames,     3,   // sleep
  lopmon_happy_frames,     3,   // eat
  lopmon_happy_frames,     3,   // play
  lopmon_walk_frames,      3,   // sad
  lopmon_walk_frames,     3,   // dead
  lopmon_happy_frames,     3,   // happy
  lopmon_attack_frames,    3,   // attack
  lopmon_profile,               // profile
  LOPMON_ATTACK_W, LOPMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER lopmon END

// >>> AUTO-REGISTER muchomon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_muchomon = {
  "muchomon",
  MUCHOMON_SPRITE_SIZE,
  MUCHOMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  muchomon_walk_frames,      3,   // walk
  muchomon_walkback_frames,  3,   // walkBack
  muchomon_walk_frames,     3,   // sleep
  muchomon_happy_frames,     3,   // eat
  muchomon_happy_frames,     3,   // play
  muchomon_walk_frames,      3,   // sad
  muchomon_walk_frames,     3,   // dead
  muchomon_happy_frames,     3,   // happy
  muchomon_attack_frames,    3,   // attack
  muchomon_profile,               // profile
  MUCHOMON_ATTACK_W, MUCHOMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER muchomon END

// >>> AUTO-REGISTER otamamon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_otamamon = {
  "otamamon",
  OTAMAMON_SPRITE_SIZE,
  OTAMAMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  otamamon_walk_frames,      3,   // walk
  otamamon_walkback_frames,  3,   // walkBack
  otamamon_walk_frames,     3,   // sleep
  otamamon_happy_frames,     3,   // eat
  otamamon_happy_frames,     3,   // play
  otamamon_walk_frames,      3,   // sad
  otamamon_walk_frames,     3,   // dead
  otamamon_happy_frames,     3,   // happy
  otamamon_attack_frames,    4,   // attack
  otamamon_profile,               // profile
  OTAMAMON_ATTACK_W, OTAMAMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER otamamon END

// >>> AUTO-REGISTER palmon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_palmon = {
  "palmon",
  PALMON_SPRITE_SIZE,
  PALMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  palmon_walk_frames,      3,   // walk
  palmon_walkback_frames,  3,   // walkBack
  palmon_walk_frames,     3,   // sleep
  palmon_happy_frames,     3,   // eat
  palmon_happy_frames,     3,   // play
  palmon_walk_frames,      3,   // sad
  palmon_walk_frames,     3,   // dead
  palmon_happy_frames,     3,   // happy
  palmon_attack_frames,    3,   // attack
  palmon_profile,               // profile
  PALMON_ATTACK_W, PALMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER palmon END

// >>> AUTO-REGISTER patamon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_patamon = {
  "patamon",
  PATAMON_SPRITE_SIZE,
  PATAMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  patamon_walk_frames,      3,   // walk
  patamon_walkback_frames,  3,   // walkBack
  patamon_walk_frames,     3,   // sleep
  patamon_happy_frames,     3,   // eat
  patamon_happy_frames,     3,   // play
  patamon_walk_frames,      3,   // sad
  patamon_walk_frames,     3,   // dead
  patamon_happy_frames,     3,   // happy
  patamon_attack_frames,    4,   // attack
  patamon_profile,               // profile
  PATAMON_ATTACK_W, PATAMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER patamon END

// >>> AUTO-REGISTER pawnchessmon_white BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_pawnchessmon_white = {
  "pawnchessmon_white",
  PAWNCHESSMON_WHITE_SPRITE_SIZE,
  PAWNCHESSMON_WHITE_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  pawnchessmon_white_walk_frames,      3,   // walk
  pawnchessmon_white_walkback_frames,  3,   // walkBack
  pawnchessmon_white_walk_frames,     3,   // sleep
  pawnchessmon_white_happy_frames,     3,   // eat
  pawnchessmon_white_happy_frames,     3,   // play
  pawnchessmon_white_walk_frames,      3,   // sad
  pawnchessmon_white_walk_frames,     3,   // dead
  pawnchessmon_white_happy_frames,     3,   // happy
  pawnchessmon_white_attack_frames,    3,   // attack
  pawnchessmon_white_profile,               // profile
  PAWNCHESSMON_WHITE_ATTACK_W, PAWNCHESSMON_WHITE_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER pawnchessmon_white END

// >>> AUTO-REGISTER pawnchessmonblack BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_pawnchessmonblack = {
  "pawnchessmonblack",
  PAWNCHESSMONBLACK_SPRITE_SIZE,
  PAWNCHESSMONBLACK_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  pawnchessmonblack_walk_frames,      3,   // walk
  pawnchessmonblack_walkback_frames,  3,   // walkBack
  pawnchessmonblack_walk_frames,     3,   // sleep
  pawnchessmonblack_happy_frames,     3,   // eat
  pawnchessmonblack_happy_frames,     3,   // play
  pawnchessmonblack_walk_frames,      3,   // sad
  pawnchessmonblack_walk_frames,     3,   // dead
  pawnchessmonblack_happy_frames,     3,   // happy
  pawnchessmonblack_attack_frames,    3,   // attack
  pawnchessmonblack_profile,               // profile
  PAWNCHESSMONBLACK_ATTACK_W, PAWNCHESSMONBLACK_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER pawnchessmonblack END

// >>> AUTO-REGISTER penguinmon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_penguinmon = {
  "penguinmon",
  PENGUINMON_SPRITE_SIZE,
  PENGUINMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  penguinmon_walk_frames,      3,   // walk
  penguinmon_walkback_frames,  3,   // walkBack
  penguinmon_walk_frames,     3,   // sleep
  penguinmon_happy_frames,     3,   // eat
  penguinmon_happy_frames,     3,   // play
  penguinmon_walk_frames,      3,   // sad
  penguinmon_walk_frames,     3,   // dead
  penguinmon_happy_frames,     3,   // happy
  penguinmon_attack_frames,    3,   // attack
  penguinmon_profile,               // profile
  PENGUINMON_ATTACK_W, PENGUINMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER penguinmon END

// >>> AUTO-REGISTER renamon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_renamon = {
  "renamon",
  RENAMON_SPRITE_SIZE,
  RENAMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  renamon_walk_frames,      3,   // walk
  renamon_walkback_frames,  3,   // walkBack
  renamon_walk_frames,     3,   // sleep
  renamon_happy_frames,     3,   // eat
  renamon_happy_frames,     3,   // play
  renamon_walk_frames,      3,   // sad
  renamon_walk_frames,     3,   // dead
  renamon_happy_frames,     3,   // happy
  renamon_attack_frames,    3,   // attack
  renamon_profile,               // profile
  RENAMON_ATTACK_W, RENAMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER renamon END

// >>> AUTO-REGISTER salamon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_salamon = {
  "salamon",
  SALAMON_SPRITE_SIZE,
  SALAMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  salamon_walk_frames,      3,   // walk
  salamon_walkback_frames,  3,   // walkBack
  salamon_walk_frames,     3,   // sleep
  salamon_happy_frames,     3,   // eat
  salamon_happy_frames,     3,   // play
  salamon_walk_frames,      3,   // sad
  salamon_walk_frames,     3,   // dead
  salamon_happy_frames,     3,   // happy
  salamon_attack_frames,    3,   // attack
  salamon_profile,               // profile
  SALAMON_ATTACK_W, SALAMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER salamon END

// >>> AUTO-REGISTER shadowtoyagumon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_shadowtoyagumon = {
  "shadowtoyagumon",
  SHADOWTOYAGUMON_SPRITE_SIZE,
  SHADOWTOYAGUMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  shadowtoyagumon_walk_frames,      3,   // walk
  shadowtoyagumon_walkback_frames,  3,   // walkBack
  shadowtoyagumon_walk_frames,     3,   // sleep
  shadowtoyagumon_happy_frames,     3,   // eat
  shadowtoyagumon_happy_frames,     3,   // play
  shadowtoyagumon_walk_frames,      3,   // sad
  shadowtoyagumon_walk_frames,     3,   // dead
  shadowtoyagumon_happy_frames,     3,   // happy
  shadowtoyagumon_attack_frames,    3,   // attack
  shadowtoyagumon_profile,               // profile
  SHADOWTOYAGUMON_ATTACK_W, SHADOWTOYAGUMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER shadowtoyagumon END

// >>> AUTO-REGISTER snowagumon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_snowagumon = {
  "snowagumon",
  SNOWAGUMON_SPRITE_SIZE,
  SNOWAGUMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  snowagumon_walk_frames,      3,   // walk
  snowagumon_walkback_frames,  3,   // walkBack
  snowagumon_walk_frames,     3,   // sleep
  snowagumon_happy_frames,     3,   // eat
  snowagumon_happy_frames,     3,   // play
  snowagumon_walk_frames,      3,   // sad
  snowagumon_walk_frames,     3,   // dead
  snowagumon_happy_frames,     3,   // happy
  snowagumon_attack_frames,    3,   // attack
  snowagumon_profile,               // profile
  SNOWAGUMON_ATTACK_W, SNOWAGUMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER snowagumon END

// >>> AUTO-REGISTER tapirmon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_tapirmon = {
  "tapirmon",
  TAPIRMON_SPRITE_SIZE,
  TAPIRMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  tapirmon_walk_frames,      3,   // walk
  tapirmon_walkback_frames,  3,   // walkBack
  tapirmon_walk_frames,     3,   // sleep
  tapirmon_happy_frames,     3,   // eat
  tapirmon_happy_frames,     3,   // play
  tapirmon_walk_frames,      3,   // sad
  tapirmon_walk_frames,     3,   // dead
  tapirmon_happy_frames,     3,   // happy
  tapirmon_attack_frames,    3,   // attack
  tapirmon_profile,               // profile
  TAPIRMON_ATTACK_W, TAPIRMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER tapirmon END

// >>> AUTO-REGISTER tentomon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_tentomon = {
  "tentomon",
  TENTOMON_SPRITE_SIZE,
  TENTOMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  tentomon_walk_frames,      3,   // walk
  tentomon_walkback_frames,  3,   // walkBack
  tentomon_walk_frames,     3,   // sleep
  tentomon_happy_frames,     3,   // eat
  tentomon_happy_frames,     3,   // play
  tentomon_walk_frames,      3,   // sad
  tentomon_walk_frames,     3,   // dead
  tentomon_happy_frames,     3,   // happy
  tentomon_attack_frames,    3,   // attack
  tentomon_profile,               // profile
  TENTOMON_ATTACK_W, TENTOMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER tentomon END

// >>> AUTO-REGISTER veemon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_veemon = {
  "veemon",
  VEEMON_SPRITE_SIZE,
  VEEMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  veemon_walk_frames,      3,   // walk
  veemon_walkback_frames,  3,   // walkBack
  veemon_walk_frames,     3,   // sleep
  veemon_happy_frames,     3,   // eat
  veemon_happy_frames,     3,   // play
  veemon_walk_frames,      3,   // sad
  veemon_walk_frames,     3,   // dead
  veemon_happy_frames,     3,   // happy
  veemon_attack_frames,    3,   // attack
  veemon_profile,               // profile
  VEEMON_ATTACK_W, VEEMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER veemon END

// >>> AUTO-REGISTER wormmon BEGIN (build_digimon_sprites.py)
const DigimonSprites DIGIMON_wormmon = {
  "wormmon",
  WORMMON_SPRITE_SIZE,
  WORMMON_PROFILE_SIZE,
  0,                                  // realHeightCm (set by hand if used)
  TYPE_DATA,                          // combat type
  0, 0, 0,                            // baseMaxHp, baseAp, baseDp
  10, 10,                             // baseIntel, baseSpeed
  nullptr, 0,                         // evolutions, count (wire by hand)
  wormmon_walk_frames,      3,   // walk
  wormmon_walkback_frames,  3,   // walkBack
  wormmon_walk_frames,     3,   // sleep
  wormmon_happy_frames,     3,   // eat
  wormmon_happy_frames,     3,   // play
  wormmon_walk_frames,      3,   // sad
  wormmon_walk_frames,     3,   // dead
  wormmon_happy_frames,     3,   // happy
  wormmon_attack_frames,    3,   // attack
  wormmon_profile,               // profile
  WORMMON_ATTACK_W, WORMMON_ATTACK_H     // attack box (w, h)
};
// <<< AUTO-REGISTER wormmon END

// --------------------------------------------------------------------------
const DigimonSprites* const DIGIMON_ALL[] = {
  &DIGIMON_wormmon,
  &DIGIMON_veemon,
  &DIGIMON_tentomon,
  &DIGIMON_tapirmon,
  &DIGIMON_snowagumon,
  &DIGIMON_shadowtoyagumon,
  &DIGIMON_salamon,
  &DIGIMON_renamon,
  &DIGIMON_penguinmon,
  &DIGIMON_pawnchessmonblack,
  &DIGIMON_pawnchessmon_white,
  &DIGIMON_patamon,
  &DIGIMON_palmon,
  &DIGIMON_otamamon,
  &DIGIMON_muchomon,
  &DIGIMON_lopmon,
  &DIGIMON_lalamon,
  &DIGIMON_kumamon,
  &DIGIMON_kudamon,
  &DIGIMON_kotemon,
  &DIGIMON_keramon,
  &DIGIMON_impmon,
  &DIGIMON_hawkmon,
  &DIGIMON_guilmon,
  &DIGIMON_gotsumon,
  &DIGIMON_goburimon,
  &DIGIMON_gaomon,
  &DIGIMON_gabumon,
  &DIGIMON_floramon,
  &DIGIMON_dorumon,
  &DIGIMON_demidevimon,
  &DIGIMON_biyomon,
  &DIGIMON_betamon,
  &DIGIMON_aruraumon,
  &DIGIMON_armadillomon,
  &DIGIMON_agumon,
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
