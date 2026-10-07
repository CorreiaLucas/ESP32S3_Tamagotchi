#include "AdventureRegistry.h"

// --------------------------------------------------------------------------
// Adventure areas, in unlock order. Each area is an ordered list of regular
// fights followed by a boss. Enemies come from EnemyRegistry, so an enemy can
// appear in several areas.
//
// PLACEHOLDER CONTENT: only three enemies exist so far, so the two areas below
// reuse them. Swap in dedicated enemies and set `background` once the art is
// ready (nullptr = default forest background).
// --------------------------------------------------------------------------

// ---- Area 1: Data Forest -------------------------------------------------
static const Enemy* const kDataForestEnemies[] = {
  &ENEMY_pagumon_grunt,
  &ENEMY_kuramon_scout,
};
static const AdventureArea AREA_data_forest = {
  "Data Forest",
  kDataForestEnemies,
  sizeof(kDataForestEnemies) / sizeof(kDataForestEnemies[0]),
  &ENEMY_gargomon_boss,
  nullptr,                    // background (nullptr = forest)
};

// ---- Area 2: Gear Savanna ------------------------------------------------
static const Enemy* const kGearSavannaEnemies[] = {
  &ENEMY_kuramon_scout,
  &ENEMY_pagumon_grunt,
  &ENEMY_kuramon_scout,
};
static const AdventureArea AREA_gear_savanna = {
  "Gear Savanna",
  kGearSavannaEnemies,
  sizeof(kGearSavannaEnemies) / sizeof(kGearSavannaEnemies[0]),
  &ENEMY_gargomon_boss,
  nullptr,                    // background (nullptr = forest)
};

// --------------------------------------------------------------------------
const AdventureArea* const ADVENTURE_ALL[] = {
  &AREA_data_forest,
  &AREA_gear_savanna,
};
const int ADVENTURE_COUNT = sizeof(ADVENTURE_ALL) / sizeof(ADVENTURE_ALL[0]);

const Enemy* adventureEnemyAt(const AdventureArea* a, int i) {
  if (!a || i < 0) return nullptr;
  if (i < a->enemyCount) return a->enemies[i];
  if (i == a->enemyCount) return a->boss;   // boss is always the last fight
  return nullptr;
}
