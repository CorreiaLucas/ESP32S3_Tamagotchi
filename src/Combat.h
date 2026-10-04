#ifndef COMBAT_H
#define COMBAT_H

#include <Arduino.h>
#include "EnemyRegistry.h"
#include "PetState.h"
#include "CharacterManager.h"
#include "DisplayManager.h"
#include "InputManager.h"
#include "SoundManager.h"

// ==========================================================================
//  COMBAT  (Tier-2 turn-based boss engine)
//  A self-contained, non-blocking turn-based fight. One Combat instance owns
//  the whole battle: local HP pools, whose turn it is, the timing action
//  command, and the win/lose outcome. It is updated once per loop() tick
//  (like the existing minigame) and never blocks.
//
//  Decoupling:
//   - Reads the pet's stats (maxHp as the battle HP pool, ap, dp) and the
//     active Digimon's `attack` frames for the hit animation.
//   - Grants rewards ONLY through pet.gainXp() on victory.
//   - Does NOT mutate persistent pet HP -- the fight is a self-contained pool,
//     so losing a battle never corrupts the care-sim state. (A death/penalty
//     model can be layered on later via the Outcome.)
//
//  Turn model:
//    PLAYER chooses Attack / Guard / Special. Attack & Special resolve through
//    a timing "action command" (a sweeping marker; OK to strike). Timing
//    quality scales damage (miss 0.5x / good 1.0x / perfect 1.5x). Guard sets
//    a damage-reduction flag for the enemy's next hit and builds charge.
//    Special costs a full charge meter and ignores part of enemy DP.
// ==========================================================================

enum CombatPhase {
  CP_INTRO,          // "A wild X appeared" banner
  CP_PLAYER_MENU,    // choosing Attack / Guard / Special
  CP_PLAYER_TIMING,  // timing action command running
  CP_PLAYER_RESOLVE, // show player's hit + damage, play attack frames
  CP_ENEMY_TURN,     // enemy telegraphs + strikes
  CP_ENEMY_RESOLVE,  // show enemy's damage
  CP_WIN,            // victory banner (+XP)
  CP_LOSE,           // defeat banner
  CP_DONE            // battle finished; caller should leave STATE_COMBAT
};

enum CombatAction { CA_ATTACK = 0, CA_GUARD = 1, CA_SPECIAL = 2 };

class Combat {
public:
  Combat();

  // Start a fresh battle against `enemy`, using the pet's current stats and
  // the active Digimon's art (from CharacterManager).
  void begin(const Enemy* enemy, PetState& pet, CharacterManager& cat);

  // Advance the battle one tick. Returns true while the battle is ongoing;
  // returns false once the phase reaches CP_DONE (caller leaves STATE_COMBAT).
  // Grants XP on the transition into CP_WIN exactly once.
  bool update(PetState& pet, DisplayManager& display,
              InputManager& input, SoundManager& sound);

  bool isFinished() const { return phase == CP_DONE; }
  bool didWin() const { return won; }

  // --- read-only accessors used by the renderer -------------------------
  CombatPhase getPhase() const { return phase; }
  const Enemy* getEnemy() const { return enemy; }
  int getPlayerHp() const { return playerHp; }
  int getPlayerMaxHp() const { return playerMaxHp; }
  int getEnemyHp() const { return enemyHp; }
  int getEnemyMaxHp() const { return enemyMaxHp; }
  int getMenuSelection() const { return menuSel; }
  int getCharge() const { return charge; }
  int getChargeMax() const { return CHARGE_MAX; }
  int getTimingPos() const { return timingPos; }    // 0..TIMING_MAX marker
  int getTimingMax() const { return TIMING_MAX; }
  const char* getMessage() const { return message; }
  const DigimonSprites* getPlayerArt() const { return playerArt; }

private:
  static const int CHARGE_MAX = 3;   // Guard/attack builds charge; Special costs full
  static const int TIMING_MAX = 100; // timing marker range
  // Timing zones (marker position): perfect center, good band around it.
  static const int TIMING_PERFECT_LO = 44;
  static const int TIMING_PERFECT_HI = 56;
  static const int TIMING_GOOD_LO    = 30;
  static const int TIMING_GOOD_HI    = 70;

  const Enemy* enemy;
  const DigimonSprites* playerArt;

  CombatPhase phase;
  int menuSel;          // 0 Attack / 1 Guard / 2 Special
  bool won;
  bool xpGranted;

  int playerHp, playerMaxHp;
  int playerAp, playerDp;
  int enemyHp, enemyMaxHp;

  int charge;           // special-move charge meter (0..CHARGE_MAX)
  bool playerGuarding;  // guard flag consumed by the enemy's next hit

  // Timing action command state.
  int timingPos;        // current marker position 0..TIMING_MAX
  int timingDir;        // +1 / -1 sweep direction
  CombatAction pendingAction;

  // Phase timing + transient message.
  uint32_t phaseStart;
  char message[28];

  // Helpers.
  int rollDamage(int atk, int def) const;       // base damage, min 1
  float timingMultiplier(int pos) const;        // 0.5 / 1.0 / 1.5
  void setMessage(const char* m);
  void toPhase(CombatPhase p);
};

#endif
