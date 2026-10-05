#ifndef ENEMY_REGISTRY_H
#define ENEMY_REGISTRY_H

#include <Arduino.h>
#include "DigimonRegistry.h"

// ==========================================================================
//  ENEMY REGISTRY  (combat opponents)
//  An Enemy reuses an existing DigimonSprites set for its art (free sprites,
//  same drawing path as the player's pet) and layers combat-only data on top:
//  its own HP/AP/DP and the XP it awards on defeat. This mirrors the
//  DigimonRegistry pattern -- a flat table of const entries referenced by the
//  combat engine. Dedicated enemy creatures can later swap `art` for their own
//  DigimonSprites without any engine change.
// ==========================================================================
struct Enemy {
  const char* name;             // display name (may differ from the art's name)
  const DigimonSprites* art;    // sprite set used for the enemy's on-screen body
  int maxHp;                    // enemy starting / max HP
  int ap;                       // enemy attack power
  int dp;                       // enemy defense power
  int speed;                    // enemy speed (turn order / ambush)
  int intel;                    // enemy intelligence (reserved)
  int xpReward;                 // XP granted to the pet on victory
};

// Registered enemies. Start small; add entries as the adventure grows.
extern const Enemy ENEMY_pagumon_grunt;
extern const Enemy ENEMY_kuramon_scout;
extern const Enemy ENEMY_gargomon_boss;

// Ordered list for iteration / level tables.
extern const Enemy* const ENEMY_ALL[];
extern const int ENEMY_COUNT;

#endif
