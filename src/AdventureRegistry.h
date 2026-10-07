#ifndef ADVENTURE_REGISTRY_H
#define ADVENTURE_REGISTRY_H

#include <Arduino.h>
#include "EnemyRegistry.h"

// ==========================================================================
//  ADVENTURE REGISTRY  (linear stage progression)
//
//  Adventure mode is a data-driven campaign layered on top of the existing
//  self-contained Combat engine. It adds no new combat logic: an AdventureArea
//  is simply an ORDERED list of enemies (regular fights) followed by a single
//  BOSS. Clearing an area (defeating its boss) unlocks the next area in
//  ADVENTURE_ALL[] -- that linear unlock chain is the game's progression spine.
//
//  This mirrors the EnemyRegistry / DigimonRegistry pattern: a flat table of
//  const entries in flash, referenced by pointer. Enemies themselves live in
//  EnemyRegistry; an area just references those pointers, so the same enemy can
//  appear in multiple areas for free.
//
//  Adding an area = add its enemy list + a row to ADVENTURE_ALL[]. The optional
//  `background` is a full-screen 128x128 RGB565 image shown behind that area's
//  battles; nullptr falls back to the default combat background (forest), so an
//  area can be added before its art exists.
// ==========================================================================
struct AdventureArea {
  const char* name;              // display name shown in the area-select list
  const Enemy* const* enemies;   // ordered regular-fight enemies (before boss)
  int enemyCount;                // number of entries in `enemies`
  const Enemy* boss;             // final fight of the area (never nullptr)
  const uint16_t* background;    // per-area combat background (nullptr = forest)
};

// Registered areas, in unlock order (index 0 is always unlocked).
extern const AdventureArea* const ADVENTURE_ALL[];
extern const int ADVENTURE_COUNT;

// Total number of fights in an area (regular enemies + boss).
inline int adventureAreaBattleCount(const AdventureArea* a) {
  if (!a) return 0;
  return a->enemyCount + (a->boss ? 1 : 0);
}

// The enemy for battle index `i` within an area: regular enemies first, then
// the boss as the final battle. Returns nullptr if `i` is out of range.
const Enemy* adventureEnemyAt(const AdventureArea* a, int i);

#endif
