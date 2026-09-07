#include "HardwareConfig.h"
#include "PetState.h"
#include "DisplayManager.h"
#include "InputManager.h"
#include "SoundManager.h"
#include "CharacterManager.h"
enum GameState { STATE_MAIN,
                 STATE_MENU,
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

const int NUM_MENU_ITEMS = 6;

uint32_t mgStartTime = 0;

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
      display.drawMenu(menuSelection);
    }
  } else if (currentState == STATE_MENU) {
    if (input.isLeftPressed()) {
      sound.playClick();
      menuSelection--;
      if (menuSelection < 0) menuSelection = NUM_MENU_ITEMS - 1;
      display.drawMenu(menuSelection);
    }

    if (input.isRightPressed()) {
      sound.playClick();
      menuSelection++;
      if (menuSelection >= NUM_MENU_ITEMS) menuSelection = 0;
      display.drawMenu(menuSelection);
    }

    if (input.isOkPressed()) {
      sound.playClick();
      if (menuSelection == 0) {
        if (pet.isSleeping()) pet.wakeUp();
        pet.feed();
        cat.setAction(EATING);
        currentState = STATE_MAIN;
      } else if (menuSelection == 1) {
        if (pet.isSleeping()) pet.wakeUp();
        currentState = STATE_MINIGAME;
        cat.setAction(MINIGAME);
        mgScore = 0;
        mgStartTime = millis();
        mgTreatY = 14;
        mgTreatX = random(2, SCREEN_WIDTH - 18);
        display.clearScreen();
      } else if (menuSelection == 2) {
        pet.forceSleep();
        cat.setAction(SLEEPING);
        currentState = STATE_MAIN;
      } else if (menuSelection == 3) {
        pet.clean();
        sound.playHappyTone();
        currentState = STATE_MAIN;
      } else if (menuSelection == 4) {
        currentState = STATE_SETTINGS;
        menuSelection = 0;
        display.drawSettings(menuSelection, sound.getMuted());
      } else if (menuSelection == 5) {
        currentState = STATE_MAIN;
      }
      if (currentState == STATE_MAIN) {
        display.forceFullRedraw(pet.getHunger(), pet.getHappiness(), pet.getEnergy());
        lastHunger = -1;
        lastPoopCount = -1;
        lastCatX = cat.getX();
      }
    }
  } else if (currentState == STATE_SETTINGS) {
    if (input.isLeftPressed()) {
      sound.playClick();
      menuSelection--;
      if (menuSelection < 0) menuSelection = 1;
      display.drawSettings(menuSelection, sound.getMuted());
    }

    if (input.isRightPressed()) {
      sound.playClick();
      menuSelection++;
      if (menuSelection > 1) menuSelection = 0;
      display.drawSettings(menuSelection, sound.getMuted());
    }

    if (input.isOkPressed()) {
      sound.playClick();

      if (menuSelection == 0) {
        sound.toggleMute();
        display.drawSettings(menuSelection, sound.getMuted());
      } else if (menuSelection == 1) {
        currentState = STATE_MENU;
        menuSelection = 4;
        display.drawMenu(menuSelection);
      }
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
      currentState = STATE_MAIN;
      display.forceFullRedraw(pet.getHunger(), pet.getHappiness(), pet.getEnergy());

      lastHunger = -1;
      lastCatX = cat.getX();
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
        currentState = STATE_MAIN;
        display.forceFullRedraw(pet.getHunger(), pet.getHappiness(), pet.getEnergy());
        lastHunger = -1;
        lastPoopCount = -1;
        lastCatX = cat.getX();
      } else if (menuSelection == 1) {
        display.drawGameOver(menuSelection);
      }
    }
  }
  delay(10);
}