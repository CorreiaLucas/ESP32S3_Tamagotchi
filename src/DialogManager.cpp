#include "DialogManager.h"

// ==========================================================================
//  DIALOG SCRIPTS
//
//  The markers and `// @node` annotations are read by tools/fsm_graph.py to
//  build the Dialogs graph view; keep them if you want the visualizer in sync.
//
//  Node indices are array positions (0-based). An option's `next` uses those
//  indices, or DIALOG_END to close the conversation. The third field is the
//  side effect (see DialogAction); omit it for pure navigation.
// ==========================================================================

// --------------------------------------------------------------------------
//  "intro_jijimon" — a plain branching conversation with no side effects.
// --------------------------------------------------------------------------
// @dialog-script-begin: intro_jijimon
static const DialogNode kIntroJijimon[] = {
  // @node greet
  { "Jijimon", "Ah, a new tamer! Are you ready to train?",
    { { "Yes!", 1, DLG_NONE }, { "Not yet", 2, DLG_NONE }, { nullptr, DIALOG_END, DLG_NONE } } },

  // @node encourage
  { "Jijimon", "Splendid! Feed and train your Digimon well.",
    { { "I will", 3, DLG_NONE }, { nullptr, DIALOG_END, DLG_NONE }, { nullptr, DIALOG_END, DLG_NONE } } },

  // @node reassure
  { "Jijimon", "Take your time. Come back when you feel ready.",
    { { "Okay", 3, DLG_NONE }, { nullptr, DIALOG_END, DLG_NONE }, { nullptr, DIALOG_END, DLG_NONE } } },

  // @node farewell
  { "Jijimon", "May your bond grow strong. Farewell!",
    { { "Bye", DIALOG_END, DLG_NONE }, { nullptr, DIALOG_END, DLG_NONE }, { nullptr, DIALOG_END, DLG_NONE } } },
};
// @dialog-script-end

const DialogScript DIALOG_intro_jijimon = {
  "intro_jijimon", kIntroJijimon,
  (int)(sizeof(kIntroJijimon) / sizeof(kIntroJijimon[0])),
  0,        // startNode
  -1        // refusedNode: none (this script has no gated actions)
};

// --------------------------------------------------------------------------
//  "trainer_pandamon" — the TRAINING NPC that replaces the old Training menu.
//
//  Node 0 is the drill menu; each choice carries a DLG_TRAIN_* effect and
//  leads to node 1, which loops back to 0 so the player can keep drilling.
//  Node 2 is the refusal line, shown by the FSM when the pet lacks the energy
//  to train (PetState::canTrain() == false).
// --------------------------------------------------------------------------
// @dialog-script-begin: trainer_pandamon
static const DialogNode kTrainerPandamon[] = {
  // @node menu
  { "Pandamon", "Training time! Which drill today?",
    { { "Strength", 1, DLG_TRAIN_AP },
      { "Defense",  1, DLG_TRAIN_DP },
      { "Stamina",  1, DLG_TRAIN_HP } } },

  // @node done
  { "Pandamon", "Good work! Another round?",
    { { "Again", 0, DLG_NONE },
      { "Enough", DIALOG_END, DLG_NONE },
      { nullptr, DIALOG_END, DLG_NONE } } },

  // @node tired
  { "Pandamon", "You're too worn out. Rest first, then come back.",
    { { "Okay", DIALOG_END, DLG_NONE },
      { nullptr, DIALOG_END, DLG_NONE },
      { nullptr, DIALOG_END, DLG_NONE } } },
};
// @dialog-script-end

const DialogScript DIALOG_trainer_pandamon = {
  "trainer_pandamon", kTrainerPandamon,
  (int)(sizeof(kTrainerPandamon) / sizeof(kTrainerPandamon[0])),
  0,        // startNode  -> "menu"
  2         // refusedNode -> "tired"
};

// ==========================================================================
//  Interpreter
// ==========================================================================
bool DialogManager::start(const DialogScript* s) {
  if (!s || s->nodeCount <= 0) return false;
  script = s;
  current = s->startNode;
  selection = 0;
  pending = DLG_NONE;
  return true;
}

const DialogNode* DialogManager::node() const {
  if (!isActive()) return nullptr;
  return &script->nodes[current];
}

int DialogManager::optionCount() const {
  const DialogNode* n = node();
  if (!n) return 0;
  int c = 0;
  for (int i = 0; i < MAX_DIALOG_OPTIONS; i++) {
    if (n->options[i].label != nullptr) c++;
  }
  return c;
}

void DialogManager::moveSelection(int delta) {
  int c = optionCount();
  if (c <= 0) return;
  selection = (selection + delta) % c;
  if (selection < 0) selection += c;
}

bool DialogManager::confirm() {
  const DialogNode* n = node();
  if (!n) { end(); return false; }

  int c = optionCount();
  // A node with no options just ends the conversation.
  if (c == 0) { end(); return false; }

  const DialogOption& opt = n->options[selection];
  // Queue the side effect for the FSM to run (and possibly veto).
  pending = opt.action;

  int target = opt.next;
  if (target == DIALOG_END || target < 0 || target >= script->nodeCount) {
    // End the conversation but KEEP `pending` set, so the caller still gets to
    // run the effect of the option that closed the dialog.
    current = DIALOG_END;
    return false;
  }
  current = target;
  selection = 0;
  return true;
}

DialogAction DialogManager::consumeAction() {
  DialogAction a = pending;
  pending = DLG_NONE;
  return a;
}

bool DialogManager::jumpTo(int nodeIndex) {
  if (!script) return false;
  if (nodeIndex < 0 || nodeIndex >= script->nodeCount) return false;
  current = nodeIndex;
  selection = 0;
  return true;
}

bool DialogManager::jumpToRefused() {
  if (!script) return false;
  return jumpTo(script->refusedNode);
}
