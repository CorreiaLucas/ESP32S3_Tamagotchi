#ifndef ADVENTURE_MANAGER_H
#define ADVENTURE_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "AdventureRegistry.h"

// ==========================================================================
//  ADVENTURE MANAGER  (run state + saved progress)
//
//  Two kinds of state:
//
//   - PROGRESS (saved in flash, namespace "adv_data"): how many areas have been
//     cleared. Area i is unlocked when i <= clearedCount, so area 0 is always
//     open and clearing area i opens area i+1.
//
//   - CURRENT RUN (RAM only): which area is being played and which fight of it
//     is next. A run is lost on reboot on purpose -- you restart the area.
//
//  It does not run combat itself. The FSM asks currentEnemy(), feeds it to the
//  existing Combat engine, then calls advance() on a win or endRun() on a loss.
// ==========================================================================
class AdventureManager {
public:
  AdventureManager();

  // Load saved progress. Call once in setup().
  void begin();

  // ---- Progress -----------------------------------------------------------
  int  clearedCount() const { return cleared; }          // 0..ADVENTURE_COUNT
  int  unlockedCount() const;                             // 1..ADVENTURE_COUNT
  bool isUnlocked(int areaIndex) const;
  bool isCleared(int areaIndex) const;
  // Forget all progress: only the first area is unlocked again.
  void reset();

  // ---- Current run --------------------------------------------------------
  // Start a run of area `areaIndex` at its first fight. Returns false (and
  // starts nothing) if the index is invalid or the area is still locked.
  bool startArea(int areaIndex);
  bool isActive() const { return area != nullptr; }
  const AdventureArea* currentArea() const { return area; }
  int  currentAreaIndex() const { return areaIdx; }
  int  battleIndex() const { return battle; }             // 0-based
  int  battleCount() const { return adventureAreaBattleCount(area); }
  // The enemy for the next fight (nullptr when no run is active).
  const Enemy* currentEnemy() const { return adventureEnemyAt(area, battle); }
  bool isBossBattle() const;

  // After a WON fight: move to the next one. Returns true if the area has
  // another fight left, false if the boss was just beaten (call
  // markAreaCleared() then).
  bool advance();

  // Record the current area as cleared (unlocks the next one) and save.
  // Returns true if this unlocked a NEW area (false on a replay of an area
  // that was already cleared). Also ends the run.
  bool markAreaCleared();

  // Leave the current run without clearing it (defeat or quit).
  void endRun();

private:
  Preferences prefs;
  int cleared;                    // persisted: number of areas cleared

  const AdventureArea* area;      // run state (RAM only)
  int areaIdx;
  int battle;

  void save();
};

#endif
