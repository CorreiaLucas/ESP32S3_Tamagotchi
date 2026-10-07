#include "PetState.h"
#include <Arduino.h>
#include <string.h>

PetState::PetState()
  :  species(""), isEggPhase(false), eggMinutes(0), hunger(50), happiness(50), energy(100), sleeping(false), lastStatUpdateTime(0), level(1), xp(0), intel(10), speed(10) {}

void PetState::begin() {
  preferences.begin("pet_data", false);
  hunger = preferences.getInt("hunger", 50);
  happiness = preferences.getInt("happiness", 50);
  energy = preferences.getInt("energy", 100);
  dead = preferences.getBool("dead", false);
  poopCount = preferences.getInt("poops", 0);
  starveTicks = preferences.getInt("starving", 0);
  age = preferences.getInt("age", 0);
  hp = preferences.getInt("hp", 100);
  maxHp = preferences.getInt("maxHp", 100);
  ap = preferences.getInt("ap", 10);
  dp = preferences.getInt("dp", 10);
  intel = preferences.getInt("intel", 10);
  speed = preferences.getInt("speed", 10);
  level = preferences.getInt("level", 1);
  xp = preferences.getInt("xp", 0);

  uptimeMinutes = preferences.getInt("uptime", 0);
  {
    String nm = preferences.getString("name", "Terriermon");
    strncpy(name, nm.c_str(), sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
  }
  {
    String sp = preferences.getString("species", "");
    strncpy(species, sp.c_str(), sizeof(species) - 1);
    species[sizeof(species) - 1] = '\0';
  }
  isEggPhase = preferences.getBool("isEgg", false);
  eggMinutes = preferences.getInt("eggMin", 0);

  lastStatUpdateTime = millis();
  lastPoopTime = millis();
  lastMinuteTime = millis();
}

void PetState::update() {
  if (dead) return;

  // ---- Egg (incubation) phase: frozen -- no poop, no stat decay, no death.
  // Only the minute counter advances; at EGG_HATCH_MINUTES the egg hatches
  // into the chosen Digimon. Uses the same 60s tick as uptime so it survives
  // reboots via the persisted eggMinutes counter.
  if (isEggPhase) {
    if (millis() - lastMinuteTime >= 60000) {
      lastMinuteTime = millis();
      eggMinutes++;
      if (eggMinutes >= EGG_HATCH_MINUTES) {
        isEggPhase = false;
        eggMinutes = EGG_HATCH_MINUTES;
      }
      saveState();
    }
    return;
  }

  if (!sleeping) {
    if (millis() - lastPoopTime > 15000) {
      lastPoopTime = millis();
      if (poopCount < 3) {
        poopCount++;
        saveState();
      }
    }
  } else {
    lastPoopTime = millis();
  }

  if (millis() - lastMinuteTime >= 60000) {
    lastMinuteTime = millis();
    uptimeMinutes++;

    if (uptimeMinutes >= 60) {
      uptimeMinutes = 0;
      addDay();
    } else {
      saveState();
    }
  }

  if (millis() - lastStatUpdateTime > 2000) {
    lastStatUpdateTime = millis();

    if (sleeping) {
      energy += 5;
      if (energy >= 100) {
        energy = 100;
        sleeping = false;
      }
    } else {
      if (hunger > 0) {
        hunger--;
        starveTicks = 0; 
      } else {
        starveTicks++; 
      }

      happiness -= (1 + poopCount);
      if (happiness < 0) happiness = 0;

      energy -= 2;
      if (energy <= 0) {
        energy = 0;

        if (starveTicks >= 15) {
          dead = true; 
        } else {
          sleeping = true; 
        }
      }
    }

    saveState();
  }
}

void PetState::feed() {
  if (sleeping) wakeUp();
  hunger += 20;
  if (hunger > 100) hunger = 100;
  saveState();
}

void PetState::play() {
  if (sleeping) wakeUp();
  happiness += 20;
  if (happiness > 100) happiness = 100;
  energy -= 10;
  if (energy < 0) energy = 0;
  saveState();
}

void PetState::wakeUp() {
  sleeping = false;
  happiness -= 5;
  if (happiness < 0) happiness = 0;
}

void PetState::clean() {
  poopCount = 0;
  saveState();
}

void PetState::saveState() {
  preferences.putInt("hunger", hunger);
  preferences.putInt("happiness", happiness);
  preferences.putInt("energy", energy);
  preferences.putBool("dead", dead);
  preferences.putInt("poops", poopCount);
  preferences.putInt("starving", starveTicks);
  preferences.putInt("age", age);
  preferences.putInt("uptime", uptimeMinutes);
  preferences.putInt("hp", hp);
  preferences.putInt("maxHp", maxHp);
  preferences.putInt("ap", ap);
  preferences.putInt("dp", dp);
  preferences.putInt("intel", intel);
  preferences.putInt("speed", speed);
  preferences.putInt("level", level);
  preferences.putInt("xp", xp);
  preferences.putString("name", name);
  preferences.putString("species", species);
  preferences.putBool("isEgg", isEggPhase);
  preferences.putInt("eggMin", eggMinutes);
}

void PetState::reset() {
  hunger = 50;
  happiness = 50;
  energy = 100;
  sleeping = false;
  dead = false;
  poopCount = 0;
  starveTicks = 0;
  age = 0;
  uptimeMinutes = 0;
  hp = 100;
  maxHp = 100;
  ap = 10;
  dp = 10;
  intel = 10;
  speed = 10;
  level = 1;
  xp = 0;
  species[0] = '\0';        // clear -> egg selection on next start
  isEggPhase = false;
  eggMinutes = 0;
  saveState();
}

void PetState::forceSleep() {
  if (!sleeping) {
    sleeping = true;
    energy = 0;
    saveState();
  }
}

void PetState::addDay() {
  age++;
  saveState();
}

// Pay the energy cost of one training drill. Callers should check canTrain()
// first; this clamps at 0 so a drill can never push energy negative.
void PetState::spendTrainingEnergy() {
  energy -= TRAIN_ENERGY_COST;
  if (energy < 0) energy = 0;
}

void PetState::trainHp() {
  int gain = random(50, 101);   
  maxHp += gain;
  hp = maxHp;                   
  spendTrainingEnergy();
  saveState();
}

void PetState::trainAp() {
  int gain = random(5, 11);     
  ap += gain;
  spendTrainingEnergy();
  saveState();
}

void PetState::trainDp() {
  int gain = random(5, 11);     
  dp += gain;
  spendTrainingEnergy();
  saveState();
}

void PetState::trainInt() {
  int gain = random(5, 11);
  intel += gain;
  spendTrainingEnergy();
  saveState();
}

void PetState::trainSpeed() {
  int gain = random(5, 11);
  speed += gain;
  spendTrainingEnergy();
  saveState();
}


void PetState::applyEvolutionStats(int baseMaxHp, int baseAp, int baseDp, int baseIntel, int baseSpeed) {
  // Keep the MAX of current vs the new form's base for each stat (never lose
  // trained progress, but a stronger base lifts weak stats).
  if (baseMaxHp > maxHp)  maxHp = baseMaxHp;
  if (baseAp    > ap)     ap    = baseAp;
  if (baseDp    > dp)     dp    = baseDp;
  if (baseIntel > intel)  intel = baseIntel;
  if (baseSpeed > speed)  speed = baseSpeed;
  hp = maxHp;                    // heal to full on evolve
  saveState();
}


void PetState::setSpecies(const char* s) {
  if (!s) s = "";
  strncpy(species, s, sizeof(species) - 1);
  species[sizeof(species) - 1] = '\0';
  saveState();
}


void PetState::startEgg() {
  if (DISABLE_EGG_PHASE) {
    // TESTING: skip incubation -- hatch immediately so the Digimon shows now.
    isEggPhase = false;
    eggMinutes = EGG_HATCH_MINUTES;
    saveState();
    return;
  }
  isEggPhase = true;
  eggMinutes = 0;
  lastMinuteTime = millis();   // start the incubation minute clock now
  saveState();
}

int PetState::gainXp(int amount) {
  if (amount <= 0) return 0;
  int levelsGained = 0;
  xp += amount;
  // Level is shared across digivolutions and never resets. Stats are left
  // untouched here on purpose -- ap/dp/maxHp come only from the training menu.
  while (xp >= xpForNext(level)) {
    xp -= xpForNext(level);
    level++;
    levelsGained++;
  }
  saveState();
  return levelsGained;
}


void PetState::setName(const char* n) {
  if (!n || !n[0]) return;
  strncpy(name, n, sizeof(name) - 1);
  name[sizeof(name) - 1] = '\0';
  if (name[0] >= 'a' && name[0] <= 'z') name[0] = (char)(name[0] - 'a' + 'A');
  saveState();
}


void PetState::setSpecies(const char* s) {
  if (!s) s = "";
  strncpy(species, s, sizeof(species) - 1);
  species[sizeof(species) - 1] = '\0';
  saveState();
}


void PetState::startEgg() {
  isEggPhase = true;
  eggMinutes = 0;
  lastMinuteTime = millis();   // start the incubation minute clock now
  saveState();
}

int PetState::gainXp(int amount) {
  if (amount <= 0) return 0;
  int levelsGained = 0;
  xp += amount;
  // Level is shared across digivolutions and never resets. Stats are left
  // untouched here on purpose -- ap/dp/maxHp come only from the training menu.
  while (xp >= xpForNext(level)) {
    xp -= xpForNext(level);
    level++;
    levelsGained++;
  }
  saveState();
  return levelsGained;
}
