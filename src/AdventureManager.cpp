#include "AdventureManager.h"

AdventureManager::AdventureManager()
  : cleared(0), area(nullptr), areaIdx(-1), battle(0) {}

void AdventureManager::begin() {
  prefs.begin("adv_data", false);
  cleared = prefs.getInt("cleared", 0);
  // Clamp in case areas were removed from ADVENTURE_ALL since the last save.
  if (cleared < 0) cleared = 0;
  if (cleared > ADVENTURE_COUNT) cleared = ADVENTURE_COUNT;
  endRun();
}

void AdventureManager::save() {
  prefs.putInt("cleared", cleared);
}

int AdventureManager::unlockedCount() const {
  int n = cleared + 1;                       // the next uncleared area is open
  if (n > ADVENTURE_COUNT) n = ADVENTURE_COUNT;
  return n;
}

bool AdventureManager::isUnlocked(int areaIndex) const {
  return areaIndex >= 0 && areaIndex < unlockedCount();
}

bool AdventureManager::isCleared(int areaIndex) const {
  return areaIndex >= 0 && areaIndex < cleared;
}

void AdventureManager::reset() {
  cleared = 0;
  save();
  endRun();
}

bool AdventureManager::startArea(int areaIndex) {
  if (!isUnlocked(areaIndex)) return false;
  const AdventureArea* a = ADVENTURE_ALL[areaIndex];
  if (!a || adventureAreaBattleCount(a) == 0) return false;
  area = a;
  areaIdx = areaIndex;
  battle = 0;
  return true;
}

bool AdventureManager::isBossBattle() const {
  return area && area->boss && battle == area->enemyCount;
}

bool AdventureManager::advance() {
  if (!area) return false;
  battle++;
  return battle < battleCount();
}

bool AdventureManager::markAreaCleared() {
  bool newlyUnlocked = false;
  // Only the frontier area moves progress forward; replaying an older area
  // changes nothing.
  if (area && areaIdx == cleared) {
    cleared++;
    save();
    newlyUnlocked = (cleared < ADVENTURE_COUNT);
  }
  endRun();
  return newlyUnlocked;
}

void AdventureManager::endRun() {
  area = nullptr;
  areaIdx = -1;
  battle = 0;
}
