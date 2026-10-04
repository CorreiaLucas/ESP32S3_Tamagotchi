#include "Combat.h"

Combat::Combat()
  : enemy(nullptr), playerArt(nullptr), phase(CP_DONE), menuSel(0),
    won(false), xpGranted(false),
    playerHp(0), playerMaxHp(0), playerAp(0), playerDp(0),
    enemyHp(0), enemyMaxHp(0),
    charge(0), playerGuarding(false),
    timingPos(0), timingDir(1), pendingAction(CA_ATTACK),
    phaseStart(0) {
  message[0] = '\0';
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

  enemyMaxHp = enemy ? enemy->maxHp : 1;
  enemyHp    = enemyMaxHp;

  menuSel = 0;
  won = false;
  xpGranted = false;
  charge = 0;
  playerGuarding = false;
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

  switch (phase) {

    case CP_INTRO: {
      display.drawCombatScene(*this);
      // Auto-advance after a short banner, or let OK skip it.
      if (now - phaseStart > 1200 || input.isOkPressed()) {
        setMessage("Your move");
        toPhase(CP_PLAYER_MENU);
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
          atk = (int)(playerAp * 1.8f);  // special hits harder
          def = def / 2;                 // and partially ignores defense
          charge = 0;                    // consume full charge
        }
        int dmg = (int)(rollDamage(atk, def) * mult);
        if (dmg < 1) dmg = 1;
        enemyHp -= dmg;
        if (enemyHp < 0) enemyHp = 0;
        if (pendingAction == CA_ATTACK && charge < CHARGE_MAX) charge++;

        if (mult >= 1.5f)      snprintf(message, sizeof(message), "PERFECT! -%d", dmg);
        else if (mult >= 1.0f) snprintf(message, sizeof(message), "Hit! -%d", dmg);
        else                   snprintf(message, sizeof(message), "Weak.. -%d", dmg);

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
        int atk = enemy ? enemy->ap : 1;
        int dmg = rollDamage(atk, playerDp);
        if (playerGuarding) {
          dmg = dmg / 2;               // guard halves incoming damage
          playerGuarding = false;
        }
        if (dmg < 1) dmg = 1;
        playerHp -= dmg;
        if (playerHp < 0) playerHp = 0;
        snprintf(message, sizeof(message), "Took -%d", dmg);
        sound.playClick();
        toPhase(CP_ENEMY_RESOLVE);
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
