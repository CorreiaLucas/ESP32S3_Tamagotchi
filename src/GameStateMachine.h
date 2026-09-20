#ifndef GAME_STATE_MACHINE_H
#define GAME_STATE_MACHINE_H

// ==========================================================================
//  GAME STATE MACHINE  (table-driven finite state machine)
//
//  Instead of one giant if/else-if chain in loop(), the game is modelled as a
//  set of STATES, each described by a small record in a STATIC TABLE (kStates)
//  that lives in flash. Every state provides up to two hooks:
//
//     onEnter()  - called ONCE, the moment we switch into the state. This is
//                  where the screen for that state is painted. Making "paint
//                  once on entry" a structural guarantee kills the whole class
//                  of "blank screen / bars vanish" bugs that came from
//                  forgetting a draw call at a transition.
//
//     onUpdate() - called every frame while the state is active. It reads
//                  input, runs that state's logic, and RETURNS THE NEXT STATE.
//                  Returning the same state = stay put; returning a different
//                  state triggers a transition (and that state's onEnter()).
//
//  loop() therefore collapses to: run global stuff (screensaver, per-frame pet
//  update), call the current state's onUpdate(), and transition if it asked to.
//
//  Menus all behave identically (LEFT/RIGHT to move with wraparound, OK to
//  choose), so that behavior is factored into a single reusable MenuController
//  rather than copy-pasted per menu.
//
//  Everything is plain structs + function pointers: no heap, no virtuals, no
//  STL -- ESP32/Arduino friendly, and the tables sit in PROGMEM/flash.
// ==========================================================================

// The complete set of game states. Order is irrelevant except that it must
// match the initializer order of kStates[] in GameStateMachine.cpp (the state
// enum value is used to index that table).
enum GameState {
  STATE_MAIN = 0,
  STATE_MENU,
  STATE_ACTION_MENU,
  STATE_DIGIMON_MENU,
  STATE_STATS_PAGE,
  STATE_DIGIVOLUTION_PAGE,
  STATE_SETTINGS,
  STATE_DEAD,
  STATE_MINIGAME,
  STATE_NPC_DIALOG,
  STATE_TRAIN_RESULT,
  STATE_COUNT   // sentinel: number of states (keep last)
};

// One state's behavior. onEnter may be nullptr (some states need no one-shot
// paint); onUpdate must be provided and returns the state to run next frame.
struct StateHandler {
  void (*onEnter)();          // paint the screen for this state (once)
  GameState (*onUpdate)();    // per-frame logic; returns the next state
};

// --------------------------------------------------------------------------
//  Reusable list-menu behavior. A menu is just a titled array of labels plus a
//  current selection. handleNavigation() implements the shared LEFT/RIGHT/OK
//  interaction and repaints on movement; on OK it invokes onSelect(index) and
//  returns whatever next state that callback decides (via changeState / a
//  return value). This removes the ~5x duplicated navigation code.
// --------------------------------------------------------------------------
struct MenuController {
  const char* title;
  const char* const* items;
  int count;
  int selection;

  void reset(const char* menuTitle, const char* const* menuItems, int itemCount);
  void redraw() const;                 // draw the menu at its current selection
  bool moveOnNavigation();             // apply LEFT/RIGHT; returns true if moved
};

// Public entry points, called from the .ino's setup()/loop().
void gsmSetup();
void gsmLoop();

// Force a transition to `next`: sets the current state and runs its onEnter().
// Use this from state handlers whenever a transition also needs an immediate
// repaint (which is almost always). Returning a state from onUpdate() has the
// same effect; changeState() is the explicit form used inside menu callbacks.
void changeState(GameState next);

#endif
