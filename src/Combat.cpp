#include "Combat.h"

Combat::Combat()
  : enemy(nullptr), playerArt(nullptr), phase(CP_DONE), menuSel(0),
    won(false), xpGranted(false),
    playerHp(0), playerMaxHp(0), playerAp(0), playerDp(0),
    enemyHp(0), enemyMaxHp(0),
    charge(0), playerGuarding(false), enemyGuarding(false), enemyIntent(EI_ATTACK),
    parryDir(PARRY_OVER), parryResolved(false), parrySuccess(false),
    displayPlayerHp(0), displayEnemyHp(0), flashTarget(0), flashUntil(0),
    popTarget(0), popStart(0), popUntil(0), shakeUntil(0),
    timingPos(0), timingDir(1), pendingAction(CA_ATTACK),
    phaseStart(0) {
  message[0] = '\0';
  popText[0] = '\0';
}

void Combat::begin(const Enemy* e, PetState& pet, CharacterManager& cat) {
  enemy = e;
  playerArt = cat.getDigimon();

  // The player's battle HP pool is the pet's maxHp (training raises it). The
  // persistent pet HP in the care sim is intentionally left untouched.
  playerMaxHp = pet.getMaxHp();
  playerHp    = playerMaxHp;
  playerAp    = pet.getAp();
  playerDp    = pet.getDp();
  playerSpeed = pet.getSpeed();
  playerIntel = pet.getInt();

  enemyMaxHp = enemy ? enemy->maxHp : 1;
  enemyHp    = enemyMaxHp;

  // Juice: eased HP starts at full; no flash/pop/shake pending.
  displayPlayerHp = playerHp;
  displayEnemyHp  = enemyHp;
  flashTarget = 0; flashUntil = 0;
  popText[0] = '\0'; popTarget = 0; popStart = 0; popUntil = 0;
  shakeUntil = 0;

  menuSel = 0;
  won = false;
  xpGranted = false;
  charge = 0;
  playerGuarding = false;
  enemyGuarding = false;
  enemyIntent = EI_ATTACK;
  parryDir = PARRY_OVER;
  parryResolved = false;
  parrySuccess = false;
  timingPos = 0;
  timingDir = 1;
  pendingAction = CA_ATTACK;

  snprintf(message, sizeof(message), "%s appeared!", enemy ? enemy->name : "Enemy");
  phase = CP_INTRO;
  phaseStart = millis();
}

void Combat::setMessage(const char* m) {
  snprintf(message, sizeof(message), "%s", m);
}

void Combat::toPhase(CombatPhase p) {
  phase = p;
  phaseStart = millis();
}

// Choose the enemy's next move and telegraph it. Weighted so most turns are a
// normal attack, with occasional heavy hits (Guard pays off) and guards. The
// enemy guards more when it is low on HP (defensive when threatened).
void Combat::rollEnemyIntent() {
  int r = (int)random(0, 100);
  bool lowHp = (enemyMaxHp > 0 && enemyHp * 100 / enemyMaxHp <= 35);
  if (lowHp && r < 35)      enemyIntent = EI_GUARD;   // turtle up when hurt
  else if (r < 20)          enemyIntent = EI_HEAVY;   // ~20% big telegraphed hit
  else if (r < 32)          enemyIntent = EI_GUARD;   // ~12% guard
  else                      enemyIntent = EI_ATTACK;  // otherwise normal
}

int Combat::rollDamage(int atk, int def) const {
  // Simple, readable formula: attack minus half defense, floored at 1, with a
  // small +/-15% variance so repeated turns aren't identical.
  int base = atk - def / 2;
  if (base < 1) base = 1;
  int variance = base * (int)random(-15, 16) / 100;
  int dmg = base + variance;
  if (dmg < 1) dmg = 1;
  return dmg;
}

float Combat::timingMultiplier(int pos) const {
  if (pos >= TIMING_PERFECT_LO && pos <= TIMING_PERFECT_HI) return 1.5f;
  if (pos >= TIMING_GOOD_LO    && pos <= TIMING_GOOD_HI)    return 1.0f;
  return 0.5f;
}

bool Combat::update(PetState& pet, DisplayManager& display,
                    InputManager& input, SoundManager& sound) {
  uint32_t now = millis();
  tickJuice();   // ease HP bars toward their real values each tick

  switch (phase) {

    case CP_INTRO: {
      display.drawCombatScene(*this);
      // Auto-advance after a short banner, or let OK skip it.
      if (now - phaseStart > 1200 || input.isOkPressed()) {
        rollEnemyIntent();
        // SPD turn order: a clearly faster enemy AMBUSHES -- it strikes first
        // (the player still gets the parry window). Otherwise the player opens.
        int espd = enemy ? enemy->speed : 0;
        if (espd > playerSpeed + 5) {
          enemyIntent = EI_ATTACK;        // ambush is a normal strike
          setMessage("Ambushed!");
          toPhase(CP_ENEMY_TURN);
        } else {
          setMessage("Your move");
          toPhase(CP_PLAYER_MENU);
        }
        display.drawCombatScene(*this);
      }
      break;
    }

    case CP_PLAYER_MENU: {
      bool dirty = false;
      if (input.isLeftPressed()) {
        sound.playClick();
        menuSel = (menuSel + 2) % 3;   // -1 mod 3
        dirty = true;
      }
      if (input.isRightPressed()) {
        sound.playClick();
        menuSel = (menuSel + 1) % 3;
        dirty = true;
      }
      if (input.isOkPressed()) {
        sound.playClick();
        if (menuSel == CA_GUARD) {
          // Guard resolves immediately: raise shield + build charge, enemy turn.
          playerGuarding = true;
          if (charge < CHARGE_MAX) charge++;
          pendingAction = CA_GUARD;
          setMessage("Guarding!");
          toPhase(CP_PLAYER_RESOLVE);
          display.drawCombatScene(*this);
          break;
        }
        if (menuSel == CA_SPECIAL && charge < CHARGE_MAX) {
          // Not enough charge: reject, stay in menu.
          sound.playClick();
          setMessage("No charge!");
          dirty = true;
        } else {
          // Attack or ready Special -> start the timing action command.
          pendingAction = (menuSel == CA_SPECIAL) ? CA_SPECIAL : CA_ATTACK;
          timingPos = 0;
          timingDir = 1;
          setMessage("Time it! OK");
          toPhase(CP_PLAYER_TIMING);
          display.drawCombatScene(*this);
          break;
        }
      }
      if (dirty) display.drawCombatScene(*this);
      break;
    }

    case CP_PLAYER_TIMING: {
      // Sweep the marker back and forth; OK locks it in.
      const int speed = 4;
      timingPos += timingDir * speed;
      if (timingPos >= TIMING_MAX) { timingPos = TIMING_MAX; timingDir = -1; }
      if (timingPos <= 0)          { timingPos = 0;          timingDir =  1; }

      if (input.isOkPressed()) {
        float mult = timingMultiplier(timingPos);
        int atk = playerAp;
        int def = enemy ? enemy->dp : 0;
        if (pendingAction == CA_SPECIAL) {
          // Special scales with ATK *and* Intelligence: +1% damage per INT
          // point (so a smart Digimon's special hits notably harder).
          atk = (int)(playerAp * 1.8f * (1.0f + playerIntel * 0.01f));
          def = def / 2;                 // and partially ignores defense
          charge = 0;                    // consume full charge
        }
        // Type matchup: player's attribute vs the enemy's attribute.
        float typeMult = 1.0f;
        if (playerArt && enemy && enemy->art)
          typeMult = typeMultiplier(playerArt->type, enemy->art->type);
        int dmg = (int)(rollDamage(atk, def) * mult * typeMult);
        if (enemyGuarding) { dmg = dmg / 2; }   // enemy was defending this turn
        if (dmg < 1) dmg = 1;
        enemyHp -= dmg;
        if (enemyHp < 0) enemyHp = 0;
        triggerHit(2, dmg, pendingAction == CA_SPECIAL);  // flash/pop enemy; shake on Special
        if (pendingAction == CA_ATTACK && charge < CHARGE_MAX) charge++;

        // Message: lead with the type verdict when it mattered, else the timing.
        if (typeMult > 1.0f)       snprintf(message, sizeof(message), "Super! -%d", dmg);
        else if (typeMult < 1.0f)  snprintf(message, sizeof(message), "Resisted -%d", dmg);
        else if (mult >= 1.5f)     snprintf(message, sizeof(message), "PERFECT! -%d", dmg);
        else if (mult >= 1.0f)     snprintf(message, sizeof(message), "Hit! -%d", dmg);
        else                       snprintf(message, sizeof(message), "Weak.. -%d", dmg);

        sound.playClick();
        toPhase(CP_PLAYER_RESOLVE);
        display.drawCombatScene(*this);
        break;
      }
      // Keep animating the marker.
      display.drawCombatScene(*this);
      break;
    }

    case CP_PLAYER_RESOLVE: {
      display.drawCombatScene(*this);
      if (now - phaseStart > 1000) {
        if (enemyHp <= 0) {
          won = true;
          snprintf(message, sizeof(message), "%s down!", enemy ? enemy->name : "Enemy");
          toPhase(CP_WIN);
          display.drawCombatScene(*this);
        } else {
          setMessage("Enemy turn..");
          toPhase(CP_ENEMY_TURN);
          display.drawCombatScene(*this);
        }
      }
      break;
    }

    case CP_ENEMY_TURN: {
      display.drawCombatScene(*this);
      // Brief telegraph, then the enemy strikes.
      if (now - phaseStart > 800) {
        enemyGuarding = false;                 // clear last turn's guard
        if (enemyIntent == EI_GUARD) {
          // Enemy defends instead of striking: it will reduce the player's next
          // hit, and does no damage this turn.
          enemyGuarding = true;
          setMessage("Enemy guards!");
          sound.playClick();
          toPhase(CP_ENEMY_RESOLVE);
          display.drawCombatScene(*this);
          break;
        }
        // Real attack -> open a DIRECTIONAL PARRY window. Pick the incoming
        // direction; the player must press the matching button to block.
        parryDir = (ParryDir)random(0, 3);
        parryResolved = false;
        parrySuccess = false;
        setMessage("Block it!");
        toPhase(CP_ENEMY_PARRY);
        display.drawCombatScene(*this);
      }
      break;
    }

    case CP_ENEMY_PARRY: {
      // Reaction window: press the button matching parryDir to block.
      // LEFT->PARRY_LEFT, RIGHT->PARRY_RIGHT, OK->PARRY_OVER.
      // Reaction window scales with enemy DIFFICULTY (xpReward proxy): tougher
      // foes give less time to react. 1000ms base - 9ms per xp, floored at
      // 450ms so it's never impossible. HEAVY hits are telegraphed, so they get
      // a small +150ms grace (reading the wind-up should pay off).
      int xpDiff = (enemy ? enemy->xpReward : 0);
      long win = 1000L - (long)xpDiff * 9L;
      if (win < 450) win = 450;
      if (enemyIntent == EI_HEAVY) win += 150;
      const uint32_t PARRY_WINDOW_MS = (uint32_t)win;
      if (!parryResolved) {
        int pressed = -1;
        if (input.isLeftPressed())  pressed = PARRY_LEFT;
        else if (input.isRightPressed()) pressed = PARRY_RIGHT;
        else if (input.isOkPressed())    pressed = PARRY_OVER;

        if (pressed >= 0) {
          parryResolved = true;
          parrySuccess = (pressed == (int)parryDir);
          sound.playClick();
        } else if (now - phaseStart > PARRY_WINDOW_MS) {
          parryResolved = true;        // window expired -> no block
          parrySuccess = false;
        }
      }

      if (parryResolved) {
        // Resolve the hit, applying the parry outcome.
        int atk = enemy ? enemy->ap : 1;
        if (enemyIntent == EI_HEAVY) atk = (int)(atk * 1.7f);
        float typeMult = 1.0f;
        if (enemy && enemy->art && playerArt)
          typeMult = typeMultiplier(enemy->art->type, playerArt->type);
        int dmg = (int)(rollDamage(atk, playerDp) * typeMult);

        if (parrySuccess) {
          dmg = (dmg + 3) / 4;         // successful parry: ~75% reduction
        } else if (playerGuarding) {
          dmg = dmg / 2;               // fell back on a held Guard: half
        }
        playerGuarding = false;
        if (dmg < 1) dmg = 1;
        playerHp -= dmg;
        if (playerHp < 0) playerHp = 0;
        triggerHit(1, dmg, enemyIntent == EI_HEAVY);     // flash/pop player; shake on HEAVY

        if (parrySuccess)                 snprintf(message, sizeof(message), "Parried! -%d", dmg);
        else if (typeMult > 1.0f)         snprintf(message, sizeof(message), "Super! -%d", dmg);
        else if (enemyIntent == EI_HEAVY) snprintf(message, sizeof(message), "HEAVY! -%d", dmg);
        else                              snprintf(message, sizeof(message), "Hit! -%d", dmg);

        toPhase(CP_ENEMY_RESOLVE);
        display.drawCombatScene(*this);
      } else {
        // Keep rendering so the UI can animate the parry prompt / countdown.
        display.drawCombatScene(*this);
      }
      break;
    }

    case CP_ENEMY_RESOLVE: {
      display.drawCombatScene(*this);
      if (now - phaseStart > 1000) {
        if (playerHp <= 0) {
          won = false;
          setMessage("You lost..");
          toPhase(CP_LOSE);
          display.drawCombatScene(*this);
        } else {
          rollEnemyIntent();
          setMessage("Your move");
          menuSel = 0;
          toPhase(CP_PLAYER_MENU);
          display.drawCombatScene(*this);
        }
      }
      break;
    }

    case CP_WIN: {
      if (!xpGranted) {
        xpGranted = true;
        int reward = enemy ? enemy->xpReward : 0;
        int levels = pet.gainXp(reward);
        sound.playHappyTone();
        if (levels > 0) snprintf(message, sizeof(message), "Win! +%d XP  LVL UP!", reward);
        else            snprintf(message, sizeof(message), "Win! +%d XP", reward);
        display.drawCombatScene(*this);
      } else {
        display.drawCombatScene(*this);
      }
      if (now - phaseStart > 1600 || input.isOkPressed()) {
        toPhase(CP_DONE);
      }
      break;
    }

    case CP_LOSE: {
      display.drawCombatScene(*this);
      if (now - phaseStart > 1600 || input.isOkPressed()) {
        sound.playSadTone();
        toPhase(CP_DONE);
      }
      break;
    }

    case CP_DONE:
    default:
      return false;
  }

  return phase != CP_DONE;
}


// Ease the displayed HP toward the real HP (smooth drain), ~a few units/tick.
void Combat::tickJuice() {
  const int STEP = 3;
  if (displayPlayerHp > playerHp) { displayPlayerHp -= STEP; if (displayPlayerHp < playerHp) displayPlayerHp = playerHp; }
  else if (displayPlayerHp < playerHp) displayPlayerHp = playerHp;   // healing snaps up
  if (displayEnemyHp > enemyHp)   { displayEnemyHp -= STEP;  if (displayEnemyHp < enemyHp)  displayEnemyHp = enemyHp; }
  else if (displayEnemyHp < enemyHp) displayEnemyHp = enemyHp;
}

// Fire the flash + floating damage number on `target` (1 player, 2 enemy),
// and a brief shake on heavy hits.
void Combat::triggerHit(int target, int dmg, bool heavy) {
  uint32_t now = millis();
  flashTarget = target;
  flashUntil  = now + 140;
  snprintf(popText, sizeof(popText), "-%d", dmg);
  popTarget = target;
  popStart  = now;
  popUntil  = now + 650;
  if (heavy) shakeUntil = now + 180;
}

int Combat::getPopAge() const {
  if (!popText[0] || millis() > popUntil) return -1;   // no active pop
  return (int)(millis() - popStart);
}

bool Combat::getShake() const { return millis() < shakeUntil; }

int Combat::getFlashTarget() const {
  // Quick blink: flash ON for the first ~70ms, OFF for the rest of the window,
  // then nothing. Reads as a snappy impact rather than a held white veil.
  uint32_t now = millis();
  if (now >= flashUntil) return 0;
  uint32_t elapsed = now - (flashUntil - 140);
  return (elapsed < 70) ? flashTarget : 0;
}

