#ifndef PET_STATE_H
#define PET_STATE_H

#include <Preferences.h>

// Minutes an egg incubates before hatching into the chosen Digimon.
static const int EGG_HATCH_MINUTES = 1;
static const bool DISABLE_EGG_PHASE = true;  // TESTING: skip incubation

class PetState {
private:
  Preferences preferences;
  bool sleeping;
  bool dead;

  char name[16];
  char species[16];   // chosen Digimon (egg pick). Empty = not chosen yet.
  bool isEggPhase;    // true while incubating (shows egg, no decay).
  int  eggMinutes;    // minutes elapsed in the egg phase (persisted).
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
  int intel;   // Intelligence (Digimon-Story-style stat; drives digivolution)
  int speed;   // Speed (drives combat turn order + digivolution)
  int level;   // shared across digivolutions; starts at 1
  int xp;      // XP accumulated toward the NEXT level

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
  void trainInt();
  void trainSpeed();
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
  void applyEvolutionStats(int baseMaxHp, int baseAp, int baseDp, int baseIntel, int baseSpeed);
  // Grant combat XP. Levels up (possibly multiple times) when the threshold is
  // crossed. Does NOT change ap/dp/maxHp -- stats come only from training.
  // Returns the number of levels gained (0 = none) so the caller can play a
  // "LEVEL UP!" flourish.
  int gainXp(int amount);
  // XP required to go FROM the given level to the next one.
  static int xpForNext(int level) { return 20 + level * 10; }
  int getLevel() const { return level; }
  int getXp() const { return xp; }
  int getXpForNext() const { return xpForNext(level); }
  char* getName() {
    return name;
  }
  // Store the pet's display name (persisted). Capitalizes the first letter so
  // a lowercase species id like "koromon" shows as "Koromon". Ready for a
  // future rename feature. Truncates to the 16-char buffer.
  void setName(const char* n);
  const char* getSpecies() const { return species; }
  bool hasSpecies() const { return species[0] != '\0'; }
  void setSpecies(const char* s);
  // Begin the egg (incubation) phase for a freshly chosen species.
  void startEgg();
  bool isEgg() const { return isEggPhase; }
  int  getEggMinutes() const { return eggMinutes; }
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
  int getInt() const {
    return intel;
  }
  int getSpeed() const { return speed; }
};

#endif