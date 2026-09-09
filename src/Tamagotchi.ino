#include "HardwareConfig.h"
#include "PetState.h"
#include "DisplayManager.h"
#include "InputManager.h"
#include "SoundManager.h"
#include "CharacterManager.h"
enum GameState { STATE_MAIN,
                 STATE_MENU,
                 STATE_ACTION_MENU,
                 STATE_DIGIMON_MENU,
                 STATE_STATS_PAGE,
                 STATE_DIGIVOLUTION_PAGE,
                 STATE_SETTINGS,
                 STATE_DEAD,
                 STATE_MINIGAME };

GameState currentState = STATE_MAIN;
PetState pet;
DisplayManager display;
InputManager input;
SoundManager sound;
CharacterManager cat;

int lastHunger = -1;
int lastHappiness = -1;
int lastEnergy = -1;
int lastCatX = 20;
int lastPoopCount = -1;
int mgTreatX = 60;
int mgTreatY = 0;
int mgOldTreatX = 60;
int mgOldTreatY = 0;
int mgScore = 0;
int menuSelection = 0;
int mgLastScore = -1;
int mgLastTime = -1;

const char* topMenuItems[]     = { "Action", "Digimon", "Settings", "Exit" };
const char* actionMenuItems[]  = { "Feed", "Play", "Sleep", "Clean", "Back" };
const char* digimonMenuItems[] = { "Stats", "Digivolution", "Back" };

const int NUM_MENU_ITEMS = 4;
const int NUM_ACTION_ITEMS = 5;
const int NUM_DIGIMON_ITEMS = 3;
const int NUM_SETTINGS_ITEMS = 2;   // Sound, Back

uint32_t mgStartTime = 0;

// Return to the main screen AND immediately repaint the whole scene in one
// shot: background + status chrome + stat bars + the digimon itself. Without
// the explicit cat draw here, the pet only reappears on a later animation
// tick (the "background only, then UI, then digimon seconds later" bug).
void returnToMain() {
  currentState = STATE_MAIN;
  display.forceFullRedraw(pet.getHunger(), pet.getHappiness(), pet.getEnergy());
  display.drawPoops(pet.getPoopCount());
  cat.update(display);   // draw the pet now instead of waiting for a tick
  lastHunger = -1;
  lastHappiness = -1;
  lastEnergy = -1;
  lastPoopCount = pet.getPoopCount();
  lastCatX = cat.getX();
}

void setup() {
  Serial.begin(115200);

  input.begin();
  sound.begin();
  pet.begin();
  display.begin();
  cat.begin();

  display.forceFullRedraw(pet.getHunger(), pet.getHappiness(), pet.getEnergy());

  sound.playHappyTone();
}

void loop() {
  if (currentState == STATE_MAIN) {
    pet.update();

    if (pet.isDead()) {
      cat.setAction(DEAD);
      currentState = STATE_DEAD;
      menuSelection = 0;
      sound.playSadTone();
      display.forceFullRedraw(pet.getHunger(), pet.getHappiness(), pet.getEnergy());
      display.drawGameOver(menuSelection);
      return;
    }

    bool isCritical = (pet.getHunger() <= 20 || pet.getHappiness() <= 20);

    if (pet.isSleeping() && cat.getCurrentAction() != SLEEPING) {
      cat.setAction(SLEEPING);
    } else if (!pet.isSleeping()) {
      if (cat.getCurrentAction() == SLEEPING) {
        cat.setAction(isCritical ? SAD : WALKING);
      } else if (cat.getCurrentAction() == WALKING && isCritical) {
        cat.setAction(SAD);
      } else if (cat.getCurrentAction() == SAD && !isCritical) {
        cat.setAction(WALKING);
      }
    }

    cat.update(display);

    if (cat.getX() != lastCatX) {
      display.clearTrail(lastCatX, cat.getX(), cat.getY(), 48, 48);
      lastCatX = cat.getX();
      display.drawPoops(pet.getPoopCount());
    }

    if (pet.getPoopCount() != lastPoopCount) {
      lastPoopCount = pet.getPoopCount();
      display.drawPoops(pet.getPoopCount());
    }

    if (pet.getHunger() != lastHunger || pet.getHappiness() != lastHappiness || pet.getEnergy() != lastEnergy) {
      lastHunger = pet.getHunger();
      lastHappiness = pet.getHappiness();
      lastEnergy = pet.getEnergy();
      display.drawMainScreen(pet.getHunger(), pet.getHappiness(), pet.getEnergy());
    }

    if (input.isOkPressed()) {
      sound.playClick();
      currentState = STATE_MENU;
      menuSelection = 0;
    display.drawMenu("Menu", topMenuItems, NUM_MENU_ITEMS, menuSelection);
    }
} else if (currentState == STATE_MENU) {
  if (input.isLeftPressed()) {
    sound.playClick();
    menuSelection--;
    if (menuSelection < 0) menuSelection = NUM_MENU_ITEMS - 1;
    display.drawMenu("Menu", topMenuItems, NUM_MENU_ITEMS, menuSelection);
  }
  if (input.isRightPressed()) {
    sound.playClick();
    menuSelection++;
    if (menuSelection >= NUM_MENU_ITEMS) menuSelection = 0;
    display.drawMenu("Menu", topMenuItems, NUM_MENU_ITEMS, menuSelection);
  }
  if (input.isOkPressed()) {
    sound.playClick();
    if (menuSelection == 0) {
      currentState = STATE_ACTION_MENU;
      menuSelection = 0;
      display.drawMenu("Action", actionMenuItems, NUM_ACTION_ITEMS, menuSelection);
    } else if (menuSelection == 1) {
      currentState = STATE_DIGIMON_MENU;
      menuSelection = 0;
      display.drawMenu("Digimon", digimonMenuItems, NUM_DIGIMON_ITEMS, menuSelection);
    } else if (menuSelection == 2) {
      currentState = STATE_SETTINGS;
      menuSelection = 0;
      display.drawSettings(menuSelection, sound.getMuted());
    } else if (menuSelection == 3) {   // Exit -> back to main screen
      returnToMain();
    }
  }
}

else if (currentState == STATE_SETTINGS) {
  if (input.isLeftPressed()) {
    sound.playClick();
    menuSelection--;
    if (menuSelection < 0) menuSelection = NUM_SETTINGS_ITEMS - 1;
    display.drawSettings(menuSelection, sound.getMuted());
  }
  if (input.isRightPressed()) {
    sound.playClick();
    menuSelection++;
    if (menuSelection >= NUM_SETTINGS_ITEMS) menuSelection = 0;
    display.drawSettings(menuSelection, sound.getMuted());
  }
  if (input.isOkPressed()) {
    sound.playClick();
    if (menuSelection == 0) {          // Sound: toggle mute, stay on the page
      sound.toggleMute();
      display.drawSettings(menuSelection, sound.getMuted());
    } else if (menuSelection == 1) {   // Back -> top menu
      currentState = STATE_MENU;
      menuSelection = 0;
      display.drawMenu("Menu", topMenuItems, NUM_MENU_ITEMS, menuSelection);
    }
  }
}

else if (currentState == STATE_ACTION_MENU) {
  if (input.isLeftPressed()) {
    sound.playClick();
    menuSelection--;
    if (menuSelection < 0) menuSelection = NUM_ACTION_ITEMS - 1;
    display.drawMenu("Action", actionMenuItems, NUM_ACTION_ITEMS, menuSelection);
  }
  if (input.isRightPressed()) {
    sound.playClick();
    menuSelection++;
    if (menuSelection >= NUM_ACTION_ITEMS) menuSelection = 0;
    display.drawMenu("Action", actionMenuItems, NUM_ACTION_ITEMS, menuSelection);
  }
  if (input.isOkPressed()) {
    sound.playClick();
    bool backToMain = true;
    if (menuSelection == 0) {          // Feed
      if (pet.isSleeping()) pet.wakeUp();
      pet.feed();
      cat.setAction(EATING);
    } else if (menuSelection == 1) {   // Play
      if (pet.isSleeping()) pet.wakeUp();
      currentState = STATE_MINIGAME;
      cat.setAction(MINIGAME);
      mgScore = 0;
      mgStartTime = millis();
      mgTreatY = 20;
      mgTreatX = random(20, 100);      // clamped to your 128px screen width
      display.clearScreen();
      backToMain = false;
    } else if (menuSelection == 2) {   // Sleep
      pet.forceSleep();
      cat.setAction(SLEEPING);
    } else if (menuSelection == 3) {   // Clean
      pet.clean();
      sound.playHappyTone();
    } else if (menuSelection == 4) {   // Back
      currentState = STATE_MENU;
      menuSelection = 0;
      display.drawMenu("Menu", topMenuItems, NUM_MENU_ITEMS, menuSelection);
      backToMain = false;
    }
    if (backToMain) {
      returnToMain();
    }
  }
}

else if (currentState == STATE_DIGIMON_MENU) {
  if (input.isLeftPressed()) {
    sound.playClick();
    menuSelection--;
    if (menuSelection < 0) menuSelection = NUM_DIGIMON_ITEMS - 1;
    display.drawMenu("Digimon", digimonMenuItems, NUM_DIGIMON_ITEMS, menuSelection);
  }
  if (input.isRightPressed()) {
    sound.playClick();
    menuSelection++;
    if (menuSelection >= NUM_DIGIMON_ITEMS) menuSelection = 0;
    display.drawMenu("Digimon", digimonMenuItems, NUM_DIGIMON_ITEMS, menuSelection);
  }
  if (input.isOkPressed()) {
    sound.playClick();
    if (menuSelection == 0) {
      currentState = STATE_STATS_PAGE;
      display.drawStatsPage(pet.getName(),pet.getHp(), pet.getMaxHp(), pet.getAp(), pet.getDp());
    } else if (menuSelection == 1) {
      currentState = STATE_DIGIVOLUTION_PAGE;
      display.drawDigivolutionPage();
    } else if (menuSelection == 2) {   // Back
      currentState = STATE_MENU;
      menuSelection = 0;
      display.drawMenu("Menu", topMenuItems, NUM_MENU_ITEMS, menuSelection);
    }
  }
}

else if (currentState == STATE_STATS_PAGE) {
  if (input.isOkPressed()) {
    sound.playClick();
    currentState = STATE_DIGIMON_MENU;
    menuSelection = 0;
    display.drawMenu("Digimon", digimonMenuItems, NUM_DIGIMON_ITEMS, menuSelection);
  }
}

else if (currentState == STATE_DIGIVOLUTION_PAGE) {
  if (input.isOkPressed()) {
    sound.playClick();
    currentState = STATE_DIGIMON_MENU;
    menuSelection = 0;
    display.drawMenu("Digimon", digimonMenuItems, NUM_DIGIMON_ITEMS, menuSelection);
  }
}

  else if (currentState == STATE_MINIGAME) {
    int timeLeft = 15 - ((millis() - mgStartTime) / 1000);

    if (mgScore != mgLastScore || timeLeft != mgLastTime) {
      mgLastScore = mgScore;
      mgLastTime = timeLeft;
      display.drawMinigameTopBar(mgScore, timeLeft);
    }
    if (timeLeft <= 0) {
      sound.playHappyTone();
      pet.play();
      if (mgScore >= 5) pet.play();

      cat.setAction(PLAYING);
      returnToMain();
      mgLastScore = -1;
      mgLastTime = -1;
      return;
    }

    int oldCatX = cat.getX();
    int mgStepSpeed = 8;

    if (input.isLeftPressed()) {
      cat.setX(cat.getX() - mgStepSpeed);
      cat.setFacingRight(false);
    } else if (input.isRightPressed()) {
      cat.setX(cat.getX() + mgStepSpeed);
      cat.setFacingRight(true);
    }

    if (oldCatX != cat.getX()) {
      display.clearTrail(oldCatX, cat.getX(), cat.getY(), 48, 48);
    }

    mgOldTreatX = mgTreatX;
    mgOldTreatY = mgTreatY;

    int dropSpeed = 3;
    mgTreatY += dropSpeed;

    // Pet occupies y=36..126; catch when treat is over the pet's body.
    if (mgTreatY > 70 && mgTreatY < 120) {
      // treat center vs pet center (pet is 90 wide)
      if (abs((mgTreatX + 8) - (cat.getX() + 45)) < 30) {
        mgScore++;
        sound.playClick();
        mgTreatY = 14;
        mgTreatX = random(2, SCREEN_WIDTH - 18);
      }
    } else if (mgTreatY > SCREEN_HEIGHT) {
      mgTreatY = 14;
      mgTreatX = random(2, SCREEN_WIDTH - 18);
    }

    display.updateMinigameTreat(mgTreatX, mgTreatY, mgOldTreatX, mgOldTreatY);
    cat.update(display);
  }

  else if (currentState == STATE_DEAD) {
    cat.update(display);

    if (input.isLeftPressed() || input.isRightPressed()) {
      sound.playClick();
      menuSelection = (menuSelection == 0) ? 1 : 0;
      display.drawGameOver(menuSelection);
    }
    if (input.isOkPressed()) {
      sound.playClick();
      if (menuSelection == 0) {
        pet.reset();
        cat.setAction(WALKING);
        returnToMain();
      } else if (menuSelection == 1) {
        display.drawGameOver(menuSelection);
      }
    }
  }
  delay(10);
}