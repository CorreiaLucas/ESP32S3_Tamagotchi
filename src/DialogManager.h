#ifndef DIALOG_MANAGER_H
#define DIALOG_MANAGER_H

#include <Arduino.h>

// ==========================================================================
//  DIALOG MANAGER  (data-driven NPC conversations)
//
//  A conversation is NOT one FSM state per line -- that would explode the
//  GameState enum. Instead the whole system is a SINGLE FSM state
//  (STATE_NPC_DIALOG) that runs a small interpreter over a static table of
//  DialogNode records (living in flash, same style as DigimonRegistry).
//
//  Each node shows one line of text from a `speaker`, plus 0..MAX_DIALOG_OPTIONS
//  choices. Choosing an option jumps to another node index (its `next`). A node
//  with zero options auto-advances (via `next` of option[0]) or ends the
//  conversation when `next == DIALOG_END`.
//
//  The dialog GRAPH (nodes + option edges) is extracted by tools/fsm_graph.py
//  into dialogs.json and shown as the "Dialogs" tab of the FSM viewer. To keep
//  that extraction simple and robust, the script table is wrapped in
//  machine-readable markers:
//
//      // @dialog-script-begin: <script name>
//      ...DialogNode rows...
//      // @dialog-script-end
//
//  and each node is annotated with `// @node <id>` so the tool can name them.
// ==========================================================================

#define MAX_DIALOG_OPTIONS 3
#define DIALOG_END (-1)

// --------------------------------------------------------------------------
//  Side effects a dialog option can trigger.
//
//  The script table stays PURE DATA: an option only names an action, it does
//  not hold a function pointer. The FSM state (STATE_NPC_DIALOG) owns the
//  dispatch, because that is where `pet` / `sound` live. This also keeps the
//  effect visible to tools/fsm_graph.py, which shows it on the dialog edge.
//
//  Add a new effect by extending this enum and adding a case to the dispatch
//  switch in GameStateMachine.cpp.
// --------------------------------------------------------------------------
enum DialogAction {
  DLG_NONE = 0,     // pure navigation (default for un-tagged options)
  DLG_TRAIN_HP,     // stamina drill  -> pet.trainHp()
  DLG_TRAIN_AP,     // strength drill -> pet.trainAp()
  DLG_TRAIN_DP,     // defense drill  -> pet.trainDp()
};

// One selectable choice in a dialog node.
struct DialogOption {
  const char* label;    // text shown for this choice (nullptr = unused slot)
  int next;             // index of the node to go to, or DIALOG_END
  DialogAction action;  // side effect to run on confirm (DLG_NONE = none)
};

// One line/screen of a conversation.
struct DialogNode {
  const char* speaker;                        // e.g. "Pandamon" (nullptr = pet)
  const char* text;                           // the line shown
  DialogOption options[MAX_DIALOG_OPTIONS];   // unused slots have label == nullptr
};

// A named, self-contained conversation (an array of nodes + its length).
struct DialogScript {
  const char* name;
  const DialogNode* nodes;
  int nodeCount;
  int startNode;       // usually 0
  // Node to jump to when an action is REFUSED because the pet doesn't meet its
  // requirements (e.g. too tired to train). -1 = no refusal node; the action is
  // simply skipped. Keeps the "you can't do that" wording in the script instead
  // of hard-coding a node index in the FSM.
  int refusedNode;
};

// Registered scripts (extern-declared here, defined in DialogManager.cpp).
extern const DialogScript DIALOG_intro_jijimon;
extern const DialogScript DIALOG_trainer_pandamon;

// The runtime interpreter. Holds the active script + current node and advances
// through it based on input. Rendering is delegated to a callback so the
// manager stays display-agnostic (the FSM state wires it to DisplayManager).
class DialogManager {
private:
  const DialogScript* script = nullptr;
  int current = DIALOG_END;
  int selection = 0;                 // highlighted option index
  DialogAction pending = DLG_NONE;   // effect chosen but not yet executed

public:
  // Begin a conversation. Returns false if the script is empty.
  bool start(const DialogScript* s);

  bool isActive() const { return script != nullptr && current != DIALOG_END; }

  // Current node accessors (valid only while isActive()).
  const DialogNode* node() const;
  int optionCount() const;
  int selectedOption() const { return selection; }

  // Input handling (call from the FSM state's onUpdate):
  void moveSelection(int delta);     // navigate options (wraps)
  // Confirm the current selection. Advances to the chosen node's target (or
  // ends the conversation). Returns true if the conversation is still active.
  // Any side effect on the chosen option is queued for consumeAction().
  bool confirm();

  // Take the effect queued by the last confirm() and clear it, so the FSM runs
  // each effect exactly once.
  DialogAction consumeAction();

  // Redirect to a node mid-conversation (used to show the script's "refused"
  // line when an action's requirements aren't met).
  bool jumpTo(int nodeIndex);
  // Convenience: jump to this script's refusedNode, if it defines one.
  bool jumpToRefused();

  void end() { script = nullptr; current = DIALOG_END; pending = DLG_NONE; }
};

#endif
