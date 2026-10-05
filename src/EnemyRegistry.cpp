#include "EnemyRegistry.h"

// --------------------------------------------------------------------------
// Enemies reuse registered Digimon sprite sets for their art. Combat stats are
// tuned independently of the Digimon's own base stats so the same sprite can
// serve as a weak grunt or a tough boss.
// --------------------------------------------------------------------------

// Weak random-encounter grunt.
const Enemy ENEMY_pagumon_grunt = {
  "Pagumon", &DIGIMON_pagumon,
  /*maxHp*/ 60, /*ap*/ 8, /*dp*/ 6, /*spd*/ 6, /*int*/ 6, /*xpReward*/ 12
};

// Slightly tougher scout.
const Enemy ENEMY_kuramon_scout = {
  "Kuramon", &DIGIMON_kuramon,
  /*maxHp*/ 90, /*ap*/ 12, /*dp*/ 8, /*spd*/ 22, /*int*/ 10, /*xpReward*/ 20
};

// First boss: uses Gargomon's art and hits hard.
const Enemy ENEMY_gargomon_boss = {
  "Gargomon", &DIGIMON_gargomon,
  /*maxHp*/ 220, /*ap*/ 24, /*dp*/ 20, /*spd*/ 18, /*int*/ 20, /*xpReward*/ 60
};

const Enemy* const ENEMY_ALL[] = {
  &ENEMY_pagumon_grunt,
  &ENEMY_kuramon_scout,
  &ENEMY_gargomon_boss,
};
const int ENEMY_COUNT = sizeof(ENEMY_ALL) / sizeof(ENEMY_ALL[0]);
