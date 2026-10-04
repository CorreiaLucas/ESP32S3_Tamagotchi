#ifndef PET_STATE_H
#define PET_STATE_H

#include <Preferences.h>

class PetState {
private:
  Preferences preferences;
  bool sleeping;
  bool dead;

  char name[16];
  int hunger;
  int happiness;
  int energy;
  int poopCount;
  int starveTicks;
  int age;
  int uptimeMinutes;
  int hp;
  int maxHp;
  int ap;
  int dp;

  uint32_t lastMinuteTime;
  uint32_t lastStatUpdateTime;
  uint32_t lastPoopTime;

  // Deduct one drill's worth of energy (clamped at 0).
  void spendTrainingEnergy();

public:
  PetState();
  void begin();
  void update();
  void feed();
  void play();
  void wakeUp();
  void forceSleep();
  void clean();
  void saveState();
  void reset();
  void addDay();
  void trainHp();
  void trainAp();
  void trainDp();
  // Energy each training drill costs. Training is refused below this (see
  // canTrain()), so the pet can't be drilled into the ground.
  static const int TRAIN_ENERGY_COST = 15;
  // Whether the pet is currently able to train: awake, alive, and with enough
  // energy to pay TRAIN_ENERGY_COST.
  bool canTrain() const {
    return !dead && !sleeping && energy >= TRAIN_ENERGY_COST;
  }
  // On digivolution: raise stats to at least the new form's base
  // (max of current vs base -- never lose trained progress).
  void applyEvolutionStats(int baseMaxHp, int baseAp, int baseDp);
  char* getName() {
    return name;
  }
  int getHunger() const {
    return hunger;
  }
  int getHappiness() const {
    return happiness;
  }
  int getEnergy() const {
    return energy;
  }
  int getPoopCount() const {
    return poopCount;
  }
  bool isSleeping() const {
    return sleeping;
  }
  bool isDead() const {
    return dead;
  }
  int getAge() const {
    return age;
  }
  int getHp() const {
    return hp;
  }
  int getMaxHp() const {
    return maxHp;
  }
  int getAp() const {
    return ap;
  }
  int getDp() const {
    return dp;
  }
};

#endif