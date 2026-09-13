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
                 STATE_TRAINING_MENU,
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
const char* digimonMenuItems[] = { "Stats", "Training", "Digivolution", "Back" };
const char* trainingMenuItems[] = { "Train HP", "Train AP", "Train DP", "Back" };


const int NUM_MENU_ITEMS = 4;
const int NUM_ACTION_ITEMS = 5;
const int NUM_DIGIMON_ITEMS = 4;
const int NUM_SETTINGS_ITEMS = 3;  
const int NUM_TRAINING_ITEMS = 4;

uint32_t mgStartTime = 0;

// Return to the main screen AND immediately repaint the whole scene in one
// shot: background + status chrome + stat bars + the digimon itself. Without
// the explicit cat draw here, the pet only reappears on a later animation
// tick (the "background only, then UI, then digimon seconds later" bug).
void returnToMain() {
  currentState = STATE_MAIN;
  // Composite the ENTIRE scene (background + poops + pet + chrome + bars) in
  // an offscreen buffer and push it in ONE blit. This eliminates both the
  // visible top-to-bottom scanline wipe and the "bars vanish then reappear"
  // flicker that a multi-call forceFullRedraw produced on menu exit.
  display.renderMainScene(pet.getHunger(), pet.getHappiness(), pet.getEnergy(),
                          cat.getX(), cat.getY(), cat.getWidth(), cat.getHeight(),
                          cat.getCurrentFrame(), cat.getCurrentFlip(),
                          pet.getPoopCount());
  lastHunger = -1;
  lastHappiness = -1;
  lastEnergy = -1;
  lastPoopCount = pet.getPoopCount();
  lastCatX = cat.getX();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting display init...");

  input.begin();
  sound.begin();
  pet.begin();
  display.begin();
  Serial.println("Display init done");

  cat.begin();

  display.forceFullRedraw(pet.getHunger(), pet.getHappiness(), pet.getEnergy());

  sound.playHappyTone();
}

unsigned long lastInputTime = 0;
bool screensaverActive = false;
const unsigned long SCREENSAVER_TIMEOUT = 60000;

void loop() {
  if (input.isAnyPressed()) {
    lastInputTime = millis();
    if (screensaverActive) {
      screensaverActive = false;
      display.exitScreensaver(pet.getHunger(), pet.getHappiness(), pet.getEnergy());
    }
  }

  if (!screensaverActive && millis() - lastInputTime > SCREENSAVER_TIMEOUT) {
    display.enterScreensaver();
    screensaverActive = true;
  }

  if (currentState == STATE_MAIN) {
    pet.update();
  }

  if (!screensaverActive) {
    if(currentState == STATE_MAIN){
      if (pet.isDead()) {
        cat.setAction(DEAD);
        currentState = STATE_DEAD;
        menuSelection = 0;
        sound.playSadTone();
        display.forceFullRedraw(pet.getHunger(), pet.getHappiness(), pet.getEnergy());
        cat.update(display);          // <-- draw the dead pose ONCE here
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
      // Trail erase now happens inside cat.update() (full bounding box, correct
      // sprite size). Just keep poops repainted and track the last X.
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
  }
  else if (currentState == STATE_MENU) {
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
    } else if (menuSelection == 1) {   // Reset
      pet.reset();
      cat.setAction(WALKING);
      menuSelection = 0;
      returnToMain();
    }
    else if (menuSelection == 2) {   // Back -> top menu
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
      cat.setAction(HAPPY);              // eating a treat -> happy animation
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
      display.drawStatsPage(pet.getName(), pet.getHp(), pet.getMaxHp(), pet.getAp(), pet.getDp(),
                            cat.getDigimon() ? cat.getDigimon()->profile : nullptr,
                            cat.getDigimon() ? cat.getDigimon()->profileSize : 0);
    }else if (menuSelection == 1) {   // Training (new)
      currentState = STATE_TRAINING_MENU;
      menuSelection = 0;
      display.drawMenu("Training", trainingMenuItems, NUM_TRAINING_ITEMS, menuSelection);
    } else if (menuSelection == 2) {
      currentState = STATE_DIGIVOLUTION_PAGE;
      {
        const DigimonSprites* cur = cat.getDigimon();
        int idx = 0;
        for (int i = 0; i < DIGIMON_COUNT; i++) {
          if (DIGIMON_ALL[i] == cur) { idx = i; break; }
        }
        const DigimonSprites* nxt = DIGIMON_ALL[(idx + 1) % DIGIMON_COUNT];
        display.drawDigivolutionPage(cur ? cur->name : "?", nxt ? nxt->name : "?");
      }
    } else if (menuSelection == 3) {   // Back
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

else if (currentState == STATE_TRAINING_MENU) {
  if (input.isLeftPressed()) {
    sound.playClick();
    menuSelection--;
    if (menuSelection < 0) menuSelection = NUM_TRAINING_ITEMS - 1;
    display.drawMenu("Training", trainingMenuItems, NUM_TRAINING_ITEMS, menuSelection);
  }
  if (input.isRightPressed()) {
    sound.playClick();
    menuSelection++;
    if (menuSelection >= NUM_TRAINING_ITEMS) menuSelection = 0;
    display.drawMenu("Training", trainingMenuItems, NUM_TRAINING_ITEMS, menuSelection);
  }
  if (input.isOkPressed()) {
    sound.playClick();
    if (menuSelection == 0) {          // Train HP
      pet.trainHp();
      sound.playHappyTone();
      display.drawMenu("Training", trainingMenuItems, NUM_TRAINING_ITEMS, menuSelection);
    } else if (menuSelection == 1) {   // Train AP
      pet.trainAp();
      sound.playHappyTone();
      display.drawMenu("Training", trainingMenuItems, NUM_TRAINING_ITEMS, menuSelection);
    } else if (menuSelection == 2) {   // Train DP
      pet.trainDp();
      sound.playHappyTone();
      display.drawMenu("Training", trainingMenuItems, NUM_TRAINING_ITEMS, menuSelection);
    } else if (menuSelection == 3) {   // Back
      currentState = STATE_DIGIMON_MENU;
      menuSelection = 0;
      display.drawMenu("Digimon", digimonMenuItems, NUM_DIGIMON_ITEMS, menuSelection);
    }
  }
}

else if (currentState == STATE_DIGIVOLUTION_PAGE) {
  if (input.isOkPressed()) {
    sound.playClick();
    // Digivolve: advance to the next registered Digimon (wraps around).
    const DigimonSprites* cur = cat.getDigimon();
    int idx = 0;
    for (int i = 0; i < DIGIMON_COUNT; i++) {
      if (DIGIMON_ALL[i] == cur) { idx = i; break; }
    }
    int next = (idx + 1) % DIGIMON_COUNT;
    cat.setDigimon(DIGIMON_ALL[next]);
    cat.setAction(WALKING);
    sound.playHappyTone();
    returnToMain();   // show the new Digimon immediately (single-blit redraw)
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
      display.clearTrail(oldCatX, cat.getX(), cat.getY(), cat.getWidth(), cat.getHeight());
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
    // cat.update(display);

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
}
  delay(10);
}