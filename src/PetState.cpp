#include "PetState.h"
#include <Arduino.h>

PetState::PetState()
  : name("Terriermon"), hunger(50), happiness(50), energy(100), sleeping(false), lastStatUpdateTime(0) {}

void PetState::begin() {
  preferences.begin("pet_data", false);
  hunger = preferences.getInt("hunger", 50);
  happiness = preferences.getInt("happiness", 50);
  energy = preferences.getInt("energy", 100);
  dead = preferences.getBool("dead", false);
  poopCount = preferences.getInt("poops", 0);
  starveTicks = preferences.getInt("starving", 0);
  age = preferences.getInt("age", 0);

  uptimeMinutes = preferences.getInt("uptime", 0);

  lastStatUpdateTime = millis();
  lastPoopTime = millis();
  lastMinuteTime = millis();
}

void PetState::update() {
  if (dead) return;

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