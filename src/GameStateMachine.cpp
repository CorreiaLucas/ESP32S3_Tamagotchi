#include "GameStateMachine.h"

#include "HardwareConfig.h"
#include "PetState.h"
#include "DisplayManager.h"
#include "InputManager.h"
#include "SoundManager.h"
#include "CharacterManager.h"
#include "DigimonRegistry.h"

// ==========================================================================
//  Managers (single instances shared by every state handler).
// ==========================================================================
static PetState        pet;
static DisplayManager  display;
static InputManager    input;
static SoundManager    sound;
static CharacterManager cat;

// Current active state. Never assign this directly outside changeState().
static GameState currentState = STATE_MAIN;

// ==========================================================================
//  Cached "last drawn" values for the main scene (dirty-checking so we only
//  repaint stat bars / poops when something actually changed). -1 forces a
//  redraw on the next comparison.
// ==========================================================================
static int lastHunger    = -1;
static int lastHappiness = -1;
static int lastEnergy    = -1;
static int lastCatX      = 20;
static int lastPoopCount = -1;

// ==========================================================================
//  Minigame state.
// ==========================================================================
static int mgTreatX    = 60;
static int mgTreatY    = 0;
static int mgOldTreatX = 60;
static int mgOldTreatY = 0;
static int mgScore     = 0;
static int mgLastScore = -1;
static int mgLastTime  = -1;
static uint32_t mgStartTime = 0;

// ==========================================================================
//  Screensaver (global, runs regardless of the active state).
// ==========================================================================
static unsigned long lastInputTime = 0;
static bool screensaverActive = false;
static const unsigned long SCREENSAVER_TIMEOUT = 60000;

// ==========================================================================
//  Menu definitions. The items live in flash; each menu state binds them into
//  the shared `menu` controller in its onEnter().
// ==========================================================================
static const char* topMenuItems[]      = { "Action", "Digimon", "Settings", "Exit" };
static const char* actionMenuItems[]   = { "Feed", "Play", "Sleep", "Clean", "Back" };
static const char* digimonMenuItems[]  = { "Stats", "Training", "Digivolution", "Back" };
static const char* trainingMenuItems[] = { "Train HP", "Train AP", "Train DP", "Back" };

static const int NUM_MENU_ITEMS     = 4;
static const int NUM_ACTION_ITEMS   = 5;
static const int NUM_DIGIMON_ITEMS  = 4;
static const int NUM_SETTINGS_ITEMS = 3;
static const int NUM_TRAINING_ITEMS = 4;

// Single shared menu controller: only one menu is on screen at a time, so one
// instance is enough. Its `selection` also doubles as the selection index for
// the non-list pages (digivolution list, game-over choice).
static MenuController menu;

// ==========================================================================
//  MenuController implementation (the reusable navigation behavior).
// ==========================================================================
void MenuController::reset(const char* menuTitle, const char* const* menuItems, int itemCount) {
  title = menuTitle;
  items = menuItems;
  count = itemCount;
  selection = 0;
}

void MenuController::redraw() const {
  display.drawMenu(title, items, count, selection);
}

// Apply LEFT/RIGHT with wraparound. Returns true if the selection moved (so the
// caller can decide to repaint). Menus repaint on move; special pages that use
// selection for other layouts repaint themselves.
bool MenuController::moveOnNavigation() {
  if (input.isLeftPressed()) {
    sound.playClick();
    selection--;
    if (selection < 0) selection = count - 1;
    return true;
  }
  if (input.isRightPressed()) {
    sound.playClick();
    selection++;
    if (selection >= count) selection = 0;
    return true;
  }
  return false;
}

// ==========================================================================
//  Shared helpers (unchanged behavior from the original loop()).
// ==========================================================================

// Draw the Digivolution page for the active Digimon using the pet's live stats.
// menu.selection selects among the current Digimon's possible evolutions.
static void drawDigivolvePage() {
  const DigimonSprites* cur = cat.getDigimon();
  display.drawDigivolutionPage(cur, menu.selection,
                               pet.getMaxHp(), pet.getAp(), pet.getDp(),
                               pet.getAge(), pet.getHappiness(), pet.getHunger());
}

// ==========================================================================
//  STATE_MAIN
// ==========================================================================
static void mainOnEnter() {
  // Composite background + poops + pet + chrome + bars in one buffered blit.
  display.renderMainScene(pet.getHunger(), pet.getHappiness(), pet.getEnergy(),
                          cat.getX(), cat.getY(), cat.getWidth(), cat.getHeight(),
                          cat.getCurrentFrame(), cat.getCurrentFlip(),
                          pet.getPoopCount());
  lastHunger    = -1;
  lastHappiness = -1;
  lastEnergy    = -1;
  lastPoopCount = pet.getPoopCount();
  lastCatX      = cat.getX();
}

static GameState mainOnUpdate() {
  // NOTE: pet.update() is intentionally NOT called here. It runs in gsmLoop()
  // for STATE_MAIN *before* the screensaver gate, so the pet keeps ageing even
  // while the screensaver is showing -- matching the original behavior exactly.

  // Death check: switch to the dead state (its onEnter paints game over).
  if (pet.isDead()) {
    return STATE_DEAD;
  }

  bool isCritical = (pet.getHunger() <= 20 || pet.getHappiness() <= 20);

  // Reconcile the pet's animation with its live status.
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
    // Trail erase happens inside cat.update() (full bounding box). Just keep
    // poops repainted and track the last X.
    lastCatX = cat.getX();
    display.drawPoops(pet.getPoopCount());
  }

  if (pet.getPoopCount() != lastPoopCount) {
    lastPoopCount = pet.getPoopCount();
    display.drawPoops(pet.getPoopCount());
  }

  if (pet.getHunger() != lastHunger || pet.getHappiness() != lastHappiness ||
      pet.getEnergy() != lastEnergy) {
    lastHunger    = pet.getHunger();
    lastHappiness = pet.getHappiness();
    lastEnergy    = pet.getEnergy();
    display.drawMainScreen(pet.getHunger(), pet.getHappiness(), pet.getEnergy());
  }

  if (input.isOkPressed()) {
    sound.playClick();
    return STATE_MENU;
  }
  return STATE_MAIN;
}

// ==========================================================================
//  STATE_MENU  (top menu)
// ==========================================================================
static void topMenuOnEnter() {
  menu.reset("Menu", topMenuItems, NUM_MENU_ITEMS);
  menu.redraw();
}

static GameState topMenuOnUpdate() {
  if (menu.moveOnNavigation()) menu.redraw();

  if (input.isOkPressed()) {
    sound.playClick();
    switch (menu.selection) {
      case 0: return STATE_ACTION_MENU;
      case 1: return STATE_DIGIMON_MENU;
      case 2: return STATE_SETTINGS;
      case 3: return STATE_MAIN;   // Exit -> main screen
    }
  }
  return STATE_MENU;
}

// ==========================================================================
//  STATE_SETTINGS
// ==========================================================================
static void settingsOnEnter() {
  // Settings uses its own drawer, but reuse the controller for selection state.
  menu.reset("Settings", nullptr, NUM_SETTINGS_ITEMS);
  display.drawSettings(menu.selection, sound.getMuted());
}

static GameState settingsOnUpdate() {
  if (menu.moveOnNavigation()) display.drawSettings(menu.selection, sound.getMuted());

  if (input.isOkPressed()) {
    sound.playClick();
    if (menu.selection == 0) {            // Sound: toggle mute, stay on the page
      sound.toggleMute();
      display.drawSettings(menu.selection, sound.getMuted());
    } else if (menu.selection == 1) {     // Reset
      pet.reset();
      cat.setAction(WALKING);
      return STATE_MAIN;
    } else if (menu.selection == 2) {     // Back -> top menu
      return STATE_MENU;
    }
  }
  return STATE_SETTINGS;
}

// ==========================================================================
//  STATE_ACTION_MENU
// ==========================================================================
static void actionMenuOnEnter() {
  menu.reset("Action", actionMenuItems, NUM_ACTION_ITEMS);
  menu.redraw();
}

static GameState actionMenuOnUpdate() {
  if (menu.moveOnNavigation()) menu.redraw();

  if (input.isOkPressed()) {
    sound.playClick();
    switch (menu.selection) {
      case 0:                              // Feed
        if (pet.isSleeping()) pet.wakeUp();
        pet.feed();
        cat.setAction(HAPPY);              // eating a treat -> happy animation
        return STATE_MAIN;
      case 1:                              // Play -> minigame
        if (pet.isSleeping()) pet.wakeUp();
        return STATE_MINIGAME;
      case 2:                              // Sleep
        pet.forceSleep();
        cat.setAction(SLEEPING);
        return STATE_MAIN;
      case 3:                              // Clean
        pet.clean();
        sound.playHappyTone();
        return STATE_MAIN;
      case 4:                              // Back
        return STATE_MENU;
    }
  }
  return STATE_ACTION_MENU;
}

// ==========================================================================
//  STATE_DIGIMON_MENU
// ==========================================================================
static void digimonMenuOnEnter() {
  menu.reset("Digimon", digimonMenuItems, NUM_DIGIMON_ITEMS);
  menu.redraw();
}

static GameState digimonMenuOnUpdate() {
  if (menu.moveOnNavigation()) menu.redraw();

  if (input.isOkPressed()) {
    sound.playClick();
    switch (menu.selection) {
      case 0: return STATE_STATS_PAGE;
      case 1: return STATE_TRAINING_MENU;
      case 2: return STATE_DIGIVOLUTION_PAGE;
      case 3: return STATE_MENU;           // Back
    }
  }
  return STATE_DIGIMON_MENU;
}

// ==========================================================================
//  STATE_STATS_PAGE
// ==========================================================================
static void statsPageOnEnter() {
  display.drawStatsPage(pet.getName(), pet.getHp(), pet.getMaxHp(), pet.getAp(), pet.getDp(),
                        cat.getDigimon() ? cat.getDigimon()->profile : nullptr,
                        cat.getDigimon() ? cat.getDigimon()->profileSize : 0);
}

static GameState statsPageOnUpdate() {
  if (input.isOkPressed()) {
    sound.playClick();
    return STATE_DIGIMON_MENU;
  }
  return STATE_STATS_PAGE;
}

// ==========================================================================
//  STATE_TRAINING_MENU
// ==========================================================================
static void trainingMenuOnEnter() {
  menu.reset("Training", trainingMenuItems, NUM_TRAINING_ITEMS);
  menu.redraw();
}

static GameState trainingMenuOnUpdate() {
  if (menu.moveOnNavigation()) menu.redraw();

  if (input.isOkPressed()) {
    sound.playClick();
    switch (menu.selection) {
      case 0:                              // Train HP
        pet.trainHp();
        sound.playHappyTone();
        menu.redraw();
        break;
      case 1:                              // Train AP
        pet.trainAp();
        sound.playHappyTone();
        menu.redraw();
        break;
      case 2:                              // Train DP
        pet.trainDp();
        sound.playHappyTone();
        menu.redraw();
        break;
      case 3:                              // Back
        return STATE_DIGIMON_MENU;
    }
  }
  return STATE_TRAINING_MENU;
}

// ==========================================================================
//  STATE_DIGIVOLUTION_PAGE
// ==========================================================================
static void digivolutionOnEnter() {
  menu.selection = 0;   // select first possible evolution
  drawDigivolvePage();
}

static GameState digivolutionOnUpdate() {
  const DigimonSprites* cur = cat.getDigimon();
  int n = (cur && cur->evolutions) ? cur->evolutionCount : 0;

  // No evolutions (final form): OK just returns to the Digimon menu.
  if (n == 0) {
    if (input.isOkPressed()) {
      sound.playClick();
      return STATE_DIGIMON_MENU;
    }
    return STATE_DIGIVOLUTION_PAGE;
  }

  // Navigate the list of possible evolutions (wraparound over `n`).
  if (input.isLeftPressed()) {
    sound.playClick();
    menu.selection--;
    if (menu.selection < 0) menu.selection = n - 1;
    drawDigivolvePage();
  }
  if (input.isRightPressed()) {
    sound.playClick();
    menu.selection++;
    if (menu.selection >= n) menu.selection = 0;
    drawDigivolvePage();
  }

  if (input.isOkPressed()) {
    const EvolutionReq& req = cur->evolutions[menu.selection];
    bool ok = evolutionRequirementsMet(req,
                pet.getMaxHp(), pet.getAp(), pet.getDp(),
                pet.getAge(), pet.getHappiness(), pet.getHunger());
    if (ok && req.target) {
      sound.playHappyTone();
      // Carry stats: raise to at least the new form's base.
      pet.applyEvolutionStats(req.target->baseMaxHp, req.target->baseAp, req.target->baseDp);
      cat.setDigimon(req.target);
      cat.setAction(WALKING);
      return STATE_MAIN;                 // show the new Digimon immediately
    } else {
      // Requirements not met: reject with a click, stay on the page.
      sound.playClick();
      drawDigivolvePage();
    }
  }
  return STATE_DIGIVOLUTION_PAGE;
}

// ==========================================================================
//  STATE_MINIGAME
// ==========================================================================
static void minigameOnEnter() {
  cat.setAction(MINIGAME);
  mgScore = 0;
  mgStartTime = millis();
  mgTreatY = 20;
  mgTreatX = random(20, 100);            // clamped to the 128px screen width
  mgLastScore = -1;
  mgLastTime = -1;
  display.clearScreen();
}

static GameState minigameOnUpdate() {
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
    return STATE_MAIN;
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
  return STATE_MINIGAME;
}

// ==========================================================================
//  STATE_DEAD
// ==========================================================================
static void deadOnEnter() {
  cat.setAction(DEAD);
  menu.selection = 0;
  sound.playSadTone();
  display.forceFullRedraw(pet.getHunger(), pet.getHappiness(), pet.getEnergy());
  cat.update(display);          // draw the dead pose ONCE here
  display.drawGameOver(menu.selection);
}

static GameState deadOnUpdate() {
  if (input.isLeftPressed() || input.isRightPressed()) {
    sound.playClick();
    menu.selection = (menu.selection == 0) ? 1 : 0;
    display.drawGameOver(menu.selection);
  }
  if (input.isOkPressed()) {
    sound.playClick();
    if (menu.selection == 0) {
      pet.reset();
      cat.setAction(WALKING);
      return STATE_MAIN;
    } else if (menu.selection == 1) {
      display.drawGameOver(menu.selection);
    }
  }
  return STATE_DEAD;
}

// ==========================================================================
//  THE STATE TABLE. Indexed by GameState; order MUST match the enum.
// ==========================================================================
static const StateHandler kStates[STATE_COUNT] = {
  /* STATE_MAIN               */ { mainOnEnter,        mainOnUpdate },
  /* STATE_MENU               */ { topMenuOnEnter,     topMenuOnUpdate },
  /* STATE_ACTION_MENU        */ { actionMenuOnEnter,  actionMenuOnUpdate },
  /* STATE_DIGIMON_MENU       */ { digimonMenuOnEnter, digimonMenuOnUpdate },
  /* STATE_STATS_PAGE         */ { statsPageOnEnter,   statsPageOnUpdate },
  /* STATE_DIGIVOLUTION_PAGE  */ { digivolutionOnEnter, digivolutionOnUpdate },
  /* STATE_TRAINING_MENU      */ { trainingMenuOnEnter, trainingMenuOnUpdate },
  /* STATE_SETTINGS           */ { settingsOnEnter,    settingsOnUpdate },
  /* STATE_DEAD               */ { deadOnEnter,        deadOnUpdate },
  /* STATE_MINIGAME           */ { minigameOnEnter,    minigameOnUpdate },
};

// ==========================================================================
//  Transition helper: set the state and run its one-shot entry paint.
// ==========================================================================
void changeState(GameState next) {
  currentState = next;
  if (kStates[next].onEnter) kStates[next].onEnter();
}

// ==========================================================================
//  Setup / loop entry points (called from Tamagotchi.ino).
// ==========================================================================
void gsmSetup() {
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

  currentState = STATE_MAIN;   // no onEnter here to preserve original startup
                               // paint (forceFullRedraw above), matching the
                               // legacy behavior exactly.
}

void gsmLoop() {
  // ---- Global: screensaver handling (runs in every state) ----
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

  // Global: the pet ages/decays on the main screen even during the screensaver
  // (preserves the original loop ordering, where pet.update() ran before the
  // screensaver gate).
  if (currentState == STATE_MAIN) {
    pet.update();
  }

  // While the screensaver is up, suspend all state logic.
  if (screensaverActive) {
    delay(10);
    return;
  }

  // ---- Run the active state, transition if it asked to ----
  GameState next = kStates[currentState].onUpdate();
  if (next != currentState) {
    changeState(next);
  }

  delay(10);
}
