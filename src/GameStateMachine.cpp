#include "GameStateMachine.h"

#include "HardwareConfig.h"
#include "PetState.h"
#include "DisplayManager.h"
#include "InputManager.h"
#include "SoundManager.h"
#include "CharacterManager.h"
#include "DigimonRegistry.h"
#include "DialogManager.h"

// ==========================================================================
//  Managers (single instances shared by every state handler).
// ==========================================================================
static PetState        pet;
static DisplayManager  display;
static InputManager    input;
static SoundManager    sound;
static CharacterManager cat;
static DialogManager   dlg;

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
static const char* topMenuItems[]      = { "Action", "Digimon", "Talk", "Settings", "Exit" };
static const char* actionMenuItems[]   = { "Feed", "Play", "Sleep", "Clean", "Back" };
static const char* digimonMenuItems[]  = { "Stats", "Training", "Digivolution", "Back" };

static const int NUM_MENU_ITEMS     = 5;
static const int NUM_ACTION_ITEMS   = 5;
static const int NUM_DIGIMON_ITEMS  = 4;
static const int NUM_SETTINGS_ITEMS = 3;

// Single shared menu controller: only one menu is on screen at a time, so one
// instance is enough. Its `selection` also doubles as the selection index for
// the non-list pages (digivolution list, game-over choice).
static MenuController menu;

// Which conversation STATE_NPC_DIALOG will run next. Menu handlers set this
// just before transitioning, so one dialog state serves every NPC.
static const DialogScript* pendingDialog = &DIALOG_intro_jijimon;

// ---- NPC selection on the main screen ------------------------------------
// LEFT/RIGHT highlights the Pandamon NPC in the top-right corner; OK then
// talks to it instead of opening the top menu. With only one NPC this is a
// simple on/off toggle; with more it would become an index.
static bool npcSelected = false;

// ---- Training result screen ----------------------------------------------
// Filled in by runDialogAction() when a drill succeeds, consumed by
// STATE_TRAIN_RESULT to show "<stat> <before> ==> <after>".
static const char* trainStatName = "";
static int  trainBefore   = 0;
static int  trainAfter    = 0;
static bool trainShowResult = false;
// When returning from the result screen we must RESUME the conversation rather
// than restart it (otherwise Pandamon greets you again every drill).
static bool dialogResume = false;

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
// Composite + push the whole main scene, then reset the incremental-draw
// caches. Used on state entry AND whenever the static scene changes (e.g. the
// NPC selection outline turns on/off).
static void repaintMainScene() {
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

static void mainOnEnter() {
  // Composite background + NPC + poops + pet + chrome + bars in one blit.
  repaintMainScene();
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
    // Trail erase happens inside cat.update() (full bounding box). Poops and
    // the NPC live in the display's static scene layer now, so they are
    // restored automatically -- nothing to repaint here.
    lastCatX = cat.getX();
  }

  if (pet.getPoopCount() != lastPoopCount) {
    if (pet.getPoopCount() > lastPoopCount) {
      // New poop: drop it where the pet is standing, just behind it.
      display.addPoopBehind(cat.getX(), cat.getY(),
                            cat.getWidth(), cat.getHeight(),
                            cat.getFacingRight());
    }
    // The static scene changed (poop added, or cleaned away), so recomposite.
    // repaintMainScene() also re-syncs the poop count and resets the caches.
    repaintMainScene();
  }

  if (pet.getHunger() != lastHunger || pet.getHappiness() != lastHappiness ||
      pet.getEnergy() != lastEnergy) {
    lastHunger    = pet.getHunger();
    lastHappiness = pet.getHappiness();
    lastEnergy    = pet.getEnergy();
    display.drawMainScreen(pet.getHunger(), pet.getHappiness(), pet.getEnergy());
  }

  // ---- NPC targeting -----------------------------------------------------
  // LEFT or RIGHT toggles the highlight on the Pandamon NPC. (With a single
  // NPC either direction does the same thing; add an index here when there are
  // more.) The outline lives in the static scene layer, so the scene has to be
  // recomposited for it to appear/disappear.
  if (input.isLeftPressed() || input.isRightPressed()) {
    npcSelected = !npcSelected;
    sound.playClick();
    display.setNpcHighlight(npcSelected);
    repaintMainScene();
    return STATE_MAIN;
  }

  if (input.isOkPressed()) {
    sound.playClick();
    if (npcSelected) {
      // Talk to Pandamon instead of opening the menu.
      npcSelected = false;
      display.setNpcHighlight(false);
      pendingDialog = &DIALOG_trainer_pandamon;
      dialogResume = false;              // fresh conversation
      return STATE_NPC_DIALOG;
    }
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
      case 2:                            // Talk -> village elder
        pendingDialog = &DIALOG_intro_jijimon;
        return STATE_NPC_DIALOG;
      case 3: return STATE_SETTINGS;
      case 4: return STATE_MAIN;   // Exit -> main screen
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
      case 1:                                    // Training -> Pandamon NPC
        pendingDialog = &DIALOG_trainer_pandamon;
        return STATE_NPC_DIALOG;
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
//  STATE_NPC_DIALOG  (data-driven conversation via DialogManager)
//
//  A single FSM state runs an entire branching conversation. onEnter starts
//  whichever script `pendingDialog` points at; onUpdate navigates options
//  (LEFT/RIGHT), confirms (OK), and executes any side effect the chosen option
//  carries. When the conversation ends we return to STATE_MAIN.
//
//  Effects are dispatched HERE (not inside DialogManager) because this is where
//  `pet` and `sound` live. Gated effects -- like training, which costs energy --
//  are vetoed here and redirect the conversation to the script's refusal node.
// ==========================================================================

static void drawCurrentDialog() {
  const DialogNode* n = dlg.node();
  if (!n) return;
  const char* labels[MAX_DIALOG_OPTIONS];
  int c = 0;
  for (int i = 0; i < MAX_DIALOG_OPTIONS; i++) {
    if (n->options[i].label) labels[c++] = n->options[i].label;
  }
  // n->speaker is the NPC's name (e.g. "Pandamon"); drawDialog renders it as
  // the highlighted header line above the body text.
  display.drawDialog(n->speaker, n->text, labels, c, dlg.selectedOption());
}

// Run the effect queued by the last confirm(). Returns true if the effect was
// REFUSED (so the caller can redirect to the script's refusal node). On a
// successful drill it records the before/after values and raises
// trainShowResult so the FSM can show the summary screen.
static bool runDialogAction(DialogAction action) {
  switch (action) {
    case DLG_TRAIN_HP:
    case DLG_TRAIN_AP:
    case DLG_TRAIN_DP:
      // Training costs energy; refuse when the pet is asleep or too drained.
      if (!pet.canTrain()) {
        sound.playSadTone();
        return true;                      // refused
      }
      if (action == DLG_TRAIN_HP) {
        trainStatName = "HP";
        trainBefore = pet.getMaxHp();
        pet.trainHp();
        trainAfter = pet.getMaxHp();
      } else if (action == DLG_TRAIN_AP) {
        trainStatName = "AP";
        trainBefore = pet.getAp();
        pet.trainAp();
        trainAfter = pet.getAp();
      } else {
        trainStatName = "DP";
        trainBefore = pet.getDp();
        pet.trainDp();
        trainAfter = pet.getDp();
      }
      sound.playHappyTone();
      trainShowResult = true;
      return false;
    case DLG_NONE:
    default:
      return false;
  }
}

static void npcDialogOnEnter() {
  // Resume rather than restart when coming back from the result screen, so the
  // conversation continues at the "Another round?" node.
  if (dialogResume) {
    dialogResume = false;
  } else {
    dlg.start(pendingDialog);
  }
  drawCurrentDialog();
}

static GameState npcDialogOnUpdate() {
  if (!dlg.isActive()) return STATE_MAIN;

  if (input.isLeftPressed()) {
    sound.playClick();
    dlg.moveSelection(-1);
    drawCurrentDialog();
  } else if (input.isRightPressed()) {
    sound.playClick();
    dlg.moveSelection(+1);
    drawCurrentDialog();
  } else if (input.isOkPressed()) {
    sound.playClick();
    bool stillActive = dlg.confirm();
    bool refused = runDialogAction(dlg.consumeAction());

    if (refused && dlg.jumpToRefused()) {
      // Show the script's "too tired" line instead of the success line.
      drawCurrentDialog();
      return STATE_NPC_DIALOG;
    }
    if (trainShowResult) {
      // Drill succeeded: show the before/after summary, then come back here.
      trainShowResult = false;
      dialogResume = true;
      return STATE_TRAIN_RESULT;
    }
    if (!stillActive) {
      return STATE_MAIN;                 // conversation finished
    }
    drawCurrentDialog();
  }
  return STATE_NPC_DIALOG;
}

// ==========================================================================
//  STATE_TRAIN_RESULT  (post-drill summary: stat before ==> after)
// ==========================================================================
static void trainResultOnEnter() {
  display.drawTrainResult(trainStatName, trainBefore, trainAfter, pet.getEnergy());
}

static GameState trainResultOnUpdate() {
  if (input.isOkPressed()) {
    sound.playClick();
    return STATE_NPC_DIALOG;            // back to Pandamon ("Another round?")
  }
  return STATE_TRAIN_RESULT;
}

// ==========================================================================
//  THE STATE TABLE. Indexed by GameState; order MUST match the enum.
// ==========================================================================
static const StateHandler kStates[STATE_COUNT] = {
  /* STATE_MAIN              @cat:system */ { mainOnEnter,        mainOnUpdate },
  /* STATE_MENU              @cat:menu   */ { topMenuOnEnter,     topMenuOnUpdate },
  /* STATE_ACTION_MENU       @cat:menu   */ { actionMenuOnEnter,  actionMenuOnUpdate },
  /* STATE_DIGIMON_MENU      @cat:menu   */ { digimonMenuOnEnter, digimonMenuOnUpdate },
  /* STATE_STATS_PAGE        @cat:page   */ { statsPageOnEnter,   statsPageOnUpdate },
  /* STATE_DIGIVOLUTION_PAGE @cat:page   */ { digivolutionOnEnter, digivolutionOnUpdate },
  /* STATE_SETTINGS          @cat:menu   */ { settingsOnEnter,    settingsOnUpdate },
  /* STATE_DEAD              @cat:system */ { deadOnEnter,        deadOnUpdate },
  /* STATE_MINIGAME          @cat:action */ { minigameOnEnter,    minigameOnUpdate },
  /* STATE_NPC_DIALOG        @cat:dialog */ { npcDialogOnEnter,   npcDialogOnUpdate },
  /* STATE_TRAIN_RESULT      @cat:page   */ { trainResultOnEnter, trainResultOnUpdate },
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

  sound.playHappyTone();

  // Enter STATE_MAIN through changeState() so its onEnter() composites the FULL
  // scene (forest + NPC + pet + chrome). forceFullRedraw() alone would miss the
  // NPC, which lives in the static scene layer.
  changeState(STATE_MAIN);
}

void gsmLoop() {
  // ---- Global: screensaver handling (runs in every state) ----
  if (input.isAnyPressed()) {
    lastInputTime = millis();
    if (screensaverActive) {
      screensaverActive = false;
      // Re-enter the active state so IT repaints itself. (The old
      // exitScreensaver() always painted main-screen chrome, which was wrong
      // when the screensaver kicked in over a menu, and it also skipped the
      // NPC scene layer.)
      changeState(currentState);
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
