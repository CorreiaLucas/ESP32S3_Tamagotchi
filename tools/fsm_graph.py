#!/usr/bin/env python3
"""
fsm_graph.py — Extract the game state machine from GameStateMachine.cpp/.h and
emit a graph description (states + transitions) as JSON for the visualizer
(tools/fsm_viewer.html).

This is the "Tier 1" (read-only visualizer) backend. It relies on the regular
structure the FSM refactor introduced:

  * States are the `GameState` enum entries in GameStateMachine.h
    (STATE_COUNT is treated as a sentinel and ignored).
  * The `static const StateHandler kStates[STATE_COUNT] = { ... }` table maps
    each state to its onEnter / onUpdate handler function names.
  * Transitions are discovered inside each onEnter/onUpdate function body by
    scanning for `return STATE_X;` and `changeState(STATE_X)`. The nearest
    preceding `case N:` / `if (...)` / trailing `// comment` is captured as a
    human-readable trigger label when available.

No third-party dependencies (standard library only). Handler BODIES are never
modified — this tool only reads.

--------------------------------------------------------------------------
USAGE
--------------------------------------------------------------------------
  # Emit tools/fsm.json next to the viewer (default):
  python tools/fsm_graph.py

  # Custom source / output locations:
  python tools/fsm_graph.py --src src/GameStateMachine.cpp \
                            --header src/GameStateMachine.h \
                            --out tools/fsm.json

  # Print to stdout instead of writing a file:
  python tools/fsm_graph.py --stdout
"""

import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(HERE)
DEFAULT_SRC = os.path.join(PROJECT_ROOT, "src", "GameStateMachine.cpp")
DEFAULT_HEADER = os.path.join(PROJECT_ROOT, "src", "GameStateMachine.h")
DEFAULT_OUT = os.path.join(HERE, "fsm.json")


# --------------------------------------------------------------------------
# Enum parsing (header): the ordered list of states.
# --------------------------------------------------------------------------
def parse_states(header_text):
    """Return the ordered list of state names from `enum GameState { ... }`,
    dropping the STATE_COUNT sentinel."""
    m = re.search(r"enum\s+GameState\s*\{(.*?)\}", header_text, re.DOTALL)
    if not m:
        raise ValueError("Could not find `enum GameState { ... }` in the header.")
    body = m.group(1)
    states = []
    for raw in body.split(","):
        # Strip comments and any `= value` initializer.
        line = re.sub(r"//.*", "", raw)
        line = re.sub(r"/\*.*?\*/", "", line, flags=re.DOTALL)
        line = line.split("=")[0].strip()
        if not line:
            continue
        name = line.split()[0].strip()
        if name and name != "STATE_COUNT":
            states.append(name)
    if not states:
        raise ValueError("enum GameState parsed but no state names found.")
    return states


# --------------------------------------------------------------------------
# kStates[] table parsing (source): state -> {onEnter, onUpdate} handlers.
# --------------------------------------------------------------------------
def parse_state_table(src_text, states):
    """Map each state to its handler function names from the kStates[] table.

    The table rows are `{ enterFn, updateFn },` in the SAME ORDER as the enum,
    each usually prefixed by a `/* STATE_X */` comment. We prefer the comment
    to bind rows to states (robust to reordering); if comments are absent we
    fall back to positional order.
    """
    tbl = re.search(
        r"StateHandler\s+kStates\s*\[[^\]]*\]\s*=\s*\{(.*?)\};",
        src_text,
        re.DOTALL,
    )
    if not tbl:
        raise ValueError("Could not find the `kStates[...] = { ... };` table.")
    body = tbl.group(1)

    # Each row: optional `/* STATE_X @cat:kind */` then `{ enterFn, updateFn }`.
    # The @cat tag is optional; when absent we infer a category from the name.
    row_re = re.compile(
        r"(?:/\*\s*(?P<state>STATE_\w+)\s*"
        r"(?:@cat:\s*(?P<cat>\w+)\s*)?"
        r"\*/\s*)?"
        r"\{\s*(?P<enter>[A-Za-z_]\w*)\s*,\s*(?P<update>[A-Za-z_]\w*)\s*\}",
    )

    handlers = {}  # state -> {"onEnter": fn, "onUpdate": fn, "category": kind}
    positional = []
    for m in row_re.finditer(body):
        entry = {
            "onEnter": m.group("enter"),
            "onUpdate": m.group("update"),
            "category": m.group("cat"),   # may be None -> inferred later
        }
        if m.group("state"):
            handlers[m.group("state")] = entry
        positional.append(entry)

    # Fill any states not matched by comment using positional order.
    for i, state in enumerate(states):
        if state not in handlers and i < len(positional):
            handlers[state] = positional[i]

    return handlers


# --------------------------------------------------------------------------
# Function-body extraction (brace matching).
# --------------------------------------------------------------------------
def extract_function_body(src_text, fn_name):
    """Return (body_text, start_line) for `... fn_name(...) { ... }`, using
    brace matching so nested blocks are handled. Returns (None, None) if the
    function is not found."""
    # Find the signature: return-type fn_name( ... ) {
    sig = re.search(
        r"[A-Za-z_][\w:<>\*&\s]*\b" + re.escape(fn_name) + r"\s*\([^;{]*\)\s*\{",
        src_text,
    )
    if not sig:
        return None, None
    brace_start = src_text.index("{", sig.start())
    depth = 0
    i = brace_start
    n = len(src_text)
    while i < n:
        c = src_text[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                body = src_text[brace_start + 1 : i]
                start_line = src_text.count("\n", 0, sig.start()) + 1
                return body, start_line
        i += 1
    return None, None


# --------------------------------------------------------------------------
# Transition discovery within a handler body.
# --------------------------------------------------------------------------
TRANSITION_RE = re.compile(
    r"(?:return\s+(?P<ret>STATE_\w+)\s*;)"
    r"|(?:changeState\s*\(\s*(?P<call>STATE_\w+)\s*\))"
)


def _trigger_label_for(body, match_start):
    """Heuristically derive a short trigger label for a transition by looking
    at the code just before it. Priority:
      1) a trailing `// comment` on the transition's own line (most intentional);
      2) the nearest preceding `case N:` label (menu selection index);
      3) the nearest preceding `if (...)` condition, but ONLY when it is closer
         than any `case` (so an incidental guard doesn't mask the case label).
    """
    prefix = body[:match_start]

    # 1) Trailing line comment right after the transition (same line).
    line_end = body.find("\n", match_start)
    line = body[match_start : line_end if line_end != -1 else len(body)]
    cm = re.search(r"//\s*(.+)$", line)
    if cm:
        return cm.group(1).strip()

    # 2) Nearest preceding `case N:` (menu selection index). Capture any
    #    trailing comment on the case line (e.g. `case 0:  // Feed`) as a nicer
    #    label than the raw index.
    case_m = None
    for case_m in re.finditer(r"case\s+(\d+)\s*:[ \t]*(?://\s*(.+))?", prefix):
        pass
    # 3) Nearest preceding `if (...)` / `else if (...)` condition.
    if_m = None
    for if_m in re.finditer(r"\b(?:else\s+)?if\s*\(([^)]*)\)", prefix):
        pass

    case_pos = case_m.start() if case_m else -1
    if_pos = if_m.start() if if_m else -1

    def case_label(m):
        comment = m.group(2)
        if comment:
            return comment.strip()
        return "case " + m.group(1)

    # Prefer the case label; only fall back to the if-condition when there is
    # no case, or the if is strictly nearer to the transition than the case.
    if case_m and (not if_m or case_pos >= if_pos):
        return case_label(case_m)
    if if_m:
        return re.sub(r"\s+", " ", if_m.group(1)).strip()
    return ""


def find_transitions(body):
    """Return a list of {"to": STATE, "via": handler-kind-agnostic, "trigger": label}
    for a single handler body. `self` transitions (return current state) are
    filtered out by the caller since they mean 'stay'."""
    out = []
    for m in TRANSITION_RE.finditer(body):
        target = m.group("ret") or m.group("call")
        label = _trigger_label_for(body, m.start())
        out.append({"to": target, "trigger": label})
    return out


# --------------------------------------------------------------------------
# Category inference (fallback when a row has no @cat: tag).
#   menu   - list menus (navigable item lists)
#   dialog - NPC / conversation states
#   action - interactive gameplay (minigames, combat)
#   page   - static info pages (stats, digivolution)
#   system - engine states (main screen, death, boot)
# --------------------------------------------------------------------------
VALID_CATEGORIES = ("menu", "dialog", "action", "page", "system")


def infer_category(state_name):
    n = state_name.upper()
    if "DIALOG" in n or "NPC" in n or "TALK" in n:
        return "dialog"
    if "MINIGAME" in n or "COMBAT" in n or "BATTLE" in n or "FIGHT" in n:
        return "action"
    if n.endswith("_MENU") or n in ("STATE_MENU", "STATE_SETTINGS"):
        return "menu"
    if n.endswith("_PAGE"):
        return "page"
    if n in ("STATE_MAIN", "STATE_DEAD"):
        return "system"
    return "page"


# --------------------------------------------------------------------------
# Top-level graph build.
# --------------------------------------------------------------------------
def build_graph(src_text, header_text):
    states = parse_states(header_text)
    handlers = parse_state_table(src_text, states)

    nodes = []
    edges = []
    seen_edge = set()

    for state in states:
        h = handlers.get(state, {})
        enter_fn = h.get("onEnter")
        update_fn = h.get("onUpdate")

        enter_body, enter_line = (
            extract_function_body(src_text, enter_fn) if enter_fn else (None, None)
        )
        update_body, update_line = (
            extract_function_body(src_text, update_fn) if update_fn else (None, None)
        )

        category = h.get("category") or infer_category(state)
        if category not in VALID_CATEGORIES:
            category = infer_category(state)

        nodes.append(
            {
                "id": state,
                "category": category,
                "onEnter": enter_fn,
                "onUpdate": update_fn,
                "enterLine": enter_line,
                "updateLine": update_line,
            }
        )

        # Transitions come from both hooks (onEnter can changeState too).
        for kind, body in (("onEnter", enter_body), ("onUpdate", update_body)):
            if not body:
                continue
            for tr in find_transitions(body):
                target = tr["to"]
                # A handler returning its own state = "stay"; not a real edge.
                if target == state and kind == "onUpdate":
                    continue
                key = (state, target, tr["trigger"], kind)
                if key in seen_edge:
                    continue
                seen_edge.add(key)
                edges.append(
                    {
                        "from": state,
                        "to": target,
                        "trigger": tr["trigger"],
                        "via": kind,
                    }
                )

    return {"states": nodes, "transitions": edges}


# ==========================================================================
#  TIER 2 — SAFE WRITE-BACK
#
#  The tool only ever edits three machine-managed regions:
#    1) the `enum GameState { ... }` list (insert a new state before
#       STATE_COUNT),
#    2) the `kStates[STATE_COUNT] = { ... }` table (row + @cat tag),
#    3) FSM-GEN marker blocks holding generated stub handlers.
#
#  It NEVER rewrites a hand-written handler body. New states get stub handlers
#  inside guarded markers:
#
#    // >>> FSM-GEN:STATE_FOO  (generated stub — edit the body, keep the markers)
#    static void fooOnEnter() { /* TODO: paint STATE_FOO */ }
#    static GameState fooOnUpdate() { /* TODO */ return STATE_FOO; }
#    // <<< FSM-GEN:STATE_FOO
#
#  Every apply writes a `<src>.bak` backup and prints a unified diff.
# ==========================================================================
import difflib

GEN_BEGIN = "// >>> FSM-GEN:%s"
GEN_END = "// <<< FSM-GEN:%s"


def _handler_names(state_name):
    """Derive camelCase stub handler names from a STATE_FOO_BAR name."""
    parts = state_name.replace("STATE_", "").lower().split("_")
    camel = parts[0] + "".join(p.capitalize() for p in parts[1:])
    return camel + "OnEnter", camel + "OnUpdate"


def add_state(src_text, header_text, name, category):
    """Return (new_src, new_header) adding `name` with `category`. Idempotent-
    ish: raises if the state already exists."""
    if not re.match(r"^STATE_[A-Z0-9_]+$", name):
        raise ValueError("State name must match STATE_[A-Z0-9_]+ (got %r)" % name)
    if category not in VALID_CATEGORIES:
        raise ValueError("category must be one of %s" % (VALID_CATEGORIES,))

    existing = parse_states(header_text)
    if name in existing:
        raise ValueError("State %s already exists." % name)

    enter_fn, update_fn = _handler_names(name)

    # --- 1) Insert into the enum, right before STATE_COUNT ---
    def enum_repl(m):
        return "  %s,\n  STATE_COUNT" % name
    new_header, n = re.subn(r"\n\s*STATE_COUNT", "\n" + enum_repl(None), header_text, count=1)
    if n != 1:
        raise ValueError("Could not find STATE_COUNT in the enum to insert before.")

    # --- 2) Insert a row into kStates[], before the closing `};` ---
    tbl = re.search(r"(StateHandler\s+kStates\s*\[[^\]]*\]\s*=\s*\{)(.*?)(\n\};)",
                    src_text, re.DOTALL)
    if not tbl:
        raise ValueError("Could not find kStates table for row insertion.")
    row = "\n  /* %-24s @cat:%s */ { %s, %s }," % (name, category, enter_fn, update_fn)
    # Ensure the previous last row ends with a comma (it does in our style).
    new_table = tbl.group(1) + tbl.group(2).rstrip() + \
        ("" if tbl.group(2).rstrip().endswith(",") else ",") + row + tbl.group(3)
    new_src = src_text[: tbl.start()] + new_table + src_text[tbl.end():]

    # --- 3) Insert FSM-GEN stub handlers just before the kStates table ---
    stub = (
        "%s  (generated stub — edit the body, keep the markers)\n"
        "static void %s() { /* TODO: paint %s */ }\n"
        "static GameState %s() { /* TODO */ return %s; }\n"
        "%s\n\n"
    ) % (GEN_BEGIN % name, enter_fn, name, update_fn, name, GEN_END % name)

    anchor = new_src.find("static const StateHandler kStates")
    if anchor == -1:
        raise ValueError("Could not locate kStates definition for stub insertion.")
    # Back up to the start of the comment banner preceding the table, if any.
    new_src = new_src[:anchor] + stub + new_src[anchor:]

    return new_src, new_header


def set_category(src_text, name, category):
    """Rewrite (or add) the @cat tag on an existing state's kStates row."""
    if category not in VALID_CATEGORIES:
        raise ValueError("category must be one of %s" % (VALID_CATEGORIES,))

    # Match the row comment for this state (with or without an existing @cat).
    row_comment = re.compile(
        r"/\*\s*(" + re.escape(name) + r")\s*(?:@cat:\s*\w+\s*)?\*/"
    )
    if not row_comment.search(src_text):
        raise ValueError("Could not find a kStates row comment for %s." % name)
    new_src = row_comment.sub(
        lambda m: "/* %-24s @cat:%s */" % (m.group(1), category), src_text, count=1
    )
    return new_src


def _diff(old, new, path):
    return "".join(
        difflib.unified_diff(
            old.splitlines(keepends=True),
            new.splitlines(keepends=True),
            fromfile=path + " (before)",
            tofile=path + " (after)",
        )
    )


def _write_with_backup(path, new_text, dry_run):
    with open(path, "r", encoding="utf-8") as f:
        old = f.read()
    if old == new_text:
        print("  (no change) %s" % path)
        return
    print(_diff(old, new_text, os.path.relpath(path, PROJECT_ROOT)))
    if dry_run:
        return
    with open(path + ".bak", "w", encoding="utf-8") as f:
        f.write(old)
    with open(path, "w", encoding="utf-8") as f:
        f.write(new_text)
    print("  wrote %s (backup: %s.bak)" % (path, path))


# ==========================================================================
#  CLI
# ==========================================================================
def cmd_extract(args):
    if not os.path.isfile(args.src):
        raise SystemExit("source not found: %s" % args.src)
    if not os.path.isfile(args.header):
        raise SystemExit("header not found: %s" % args.header)

    with open(args.src, "r", encoding="utf-8") as f:
        src_text = f.read()
    with open(args.header, "r", encoding="utf-8") as f:
        header_text = f.read()

    graph = build_graph(src_text, header_text)
    graph["source"] = os.path.relpath(args.src, PROJECT_ROOT).replace("\\", "/")

    payload = json.dumps(graph, indent=2)
    if args.stdout:
        print(payload)
    else:
        with open(args.out, "w", encoding="utf-8") as f:
            f.write(payload)
        print("Wrote %s : %d states, %d transitions"
              % (os.path.relpath(args.out, PROJECT_ROOT), len(graph["states"]), len(graph["transitions"])))
    return 0


def cmd_add_state(args):
    with open(args.src, "r", encoding="utf-8") as f:
        src_text = f.read()
    with open(args.header, "r", encoding="utf-8") as f:
        header_text = f.read()
    new_src, new_header = add_state(src_text, header_text, args.name, args.category)
    print("== %s ==" % args.name)
    _write_with_backup(args.header, new_header, args.dry_run)
    _write_with_backup(args.src, new_src, args.dry_run)
    if args.dry_run:
        print("\n(dry-run: no files written. Re-run with --apply to write.)")
    return 0


def cmd_set_category(args):
    with open(args.src, "r", encoding="utf-8") as f:
        src_text = f.read()
    new_src = set_category(src_text, args.name, args.category)
    print("== set-category %s -> %s ==" % (args.name, args.category))
    _write_with_backup(args.src, new_src, args.dry_run)
    if args.dry_run:
        print("\n(dry-run: no files written. Re-run with --apply to write.)")
    return 0


# ==========================================================================
#  DIALOG SCRIPT EXTRACTION
#
#  Parses DialogManager.cpp for blocks delimited by:
#      // @dialog-script-begin: <name>
#      static const DialogNode kXxx[] = { ...rows... };
#      // @dialog-script-end
#  Each node is `{ "speaker", "text", { {label,next}, ... } }` and may be
#  annotated with `// @node <id>` on the line above. Node indices are array
#  positions; option `next` values reference those indices (DIALOG_END = end).
#  Emits a graph: nodes + option edges, for the viewer's "Dialogs" tab.
# ==========================================================================
DEFAULT_DIALOG_SRC = os.path.join(PROJECT_ROOT, "src", "DialogManager.cpp")
DEFAULT_DIALOG_OUT = os.path.join(HERE, "dialogs.json")


def _split_top_level(text, open_ch="{", close_ch="}"):
    """Yield the top-level {...} groups within `text` (one per DialogNode)."""
    depth = 0
    start = None
    for i, c in enumerate(text):
        if c == open_ch:
            if depth == 0:
                start = i + 1
            depth += 1
        elif c == close_ch:
            depth -= 1
            if depth == 0 and start is not None:
                yield text[start:i]
                start = None


_STR_RE = re.compile(r'"((?:[^"\\]|\\.)*)"|(nullptr)')


def _split_commas(text):
    """Split on top-level commas, ignoring commas inside double-quoted strings
    or nested braces. Needed because a dialog label may contain a comma."""
    parts, buf, depth, in_str, esc = [], [], 0, False, False
    for c in text:
        if in_str:
            buf.append(c)
            if esc:
                esc = False
            elif c == "\\":
                esc = True
            elif c == '"':
                in_str = False
            continue
        if c == '"':
            in_str = True
            buf.append(c)
        elif c in "{[(":
            depth += 1
            buf.append(c)
        elif c in "}])":
            depth -= 1
            buf.append(c)
        elif c == "," and depth == 0:
            parts.append("".join(buf))
            buf = []
        else:
            buf.append(c)
    if buf:
        parts.append("".join(buf))
    return [p.strip() for p in parts]


def _parse_next(tok):
    """Parse an option's `next` field: DIALOG_END or an integer index."""
    tok = tok.strip()
    if tok == "DIALOG_END":
        return -1
    try:
        return int(tok)
    except ValueError:
        return None


def _parse_dialog_node(node_text):
    """Parse one DialogNode initializer body into {speaker, text, options[]}.

    A node is `speaker, text, { opt, opt, opt }` and each option is
    `{ "label", next }` or `{ "label", next, DLG_ACTION }` (3rd field optional).
    """
    brace = node_text.find("{")
    head = node_text[:brace] if brace != -1 else node_text
    opts_blob = node_text[brace:] if brace != -1 else ""

    # First two string/nullptr tokens in the head are speaker + text.
    head_tokens = _STR_RE.findall(head)

    def tok(i):
        if i < len(head_tokens):
            s, null = head_tokens[i]
            return None if null else s
        return None

    speaker = tok(0)
    text = tok(1)

    options = []
    # opts_blob starts at the OUTER options array brace: `{ {..}, {..}, {..} }`.
    # Take the first top-level group (the array contents), then split THAT into
    # the individual `{ ... }` option groups.
    outer = next(_split_top_level(opts_blob), "")
    for opt in _split_top_level(outer):
        fields = _split_commas(opt)
        if not fields:
            continue
        # Field 0: label (quoted string, or nullptr for an unused slot).
        lm = _STR_RE.match(fields[0])
        label = None
        if lm and not lm.group(2):
            label = lm.group(1)
        if label is None:
            continue                      # unused slot -> not a real option
        nxt = _parse_next(fields[1]) if len(fields) > 1 else None
        action = fields[2].strip() if len(fields) > 2 else "DLG_NONE"
        options.append({"label": label, "next": nxt, "action": action})
    return {"speaker": speaker, "text": text, "options": options}


def parse_dialog_scripts(dlg_text):
    scripts = []
    block_re = re.compile(
        r"//\s*@dialog-script-begin:\s*(?P<name>\w+)(?P<body>.*?)//\s*@dialog-script-end",
        re.DOTALL,
    )
    for bm in block_re.finditer(dlg_text):
        name = bm.group("name")
        body = bm.group("body")
        # The array initializer: kXxx[] = { ...nodes... };
        arr = re.search(r"\{(.*)\}\s*;", body, re.DOTALL)
        if not arr:
            continue
        nodes_blob = arr.group(1)

        # @node ids (in order) so we can name nodes.
        node_ids = re.findall(r"//\s*@node\s+(\w+)", body)

        nodes = []
        for idx, node_text in enumerate(_split_top_level(nodes_blob)):
            parsed = _parse_dialog_node(node_text)
            parsed["index"] = idx
            parsed["id"] = node_ids[idx] if idx < len(node_ids) else ("node%d" % idx)
            nodes.append(parsed)

        edges = []
        for n in nodes:
            for oi, opt in enumerate(n["options"]):
                nxt = opt["next"]
                action = opt.get("action", "DLG_NONE")
                edge = {
                    "from": n["index"],
                    "label": opt["label"],
                    "action": action,
                    "end": nxt is None or nxt == -1,
                    "to": None if (nxt is None or nxt == -1) else nxt,
                }
                edges.append(edge)

        # The DialogScript initializer carries startNode + refusedNode as its
        # last two fields. Strip comments first, then split on top-level commas
        # (the sizeof(...) expression and `[0]` must not be mistaken for them).
        start_node, refused_node = 0, -1
        init = re.search(
            r"DialogScript\s+DIALOG_" + re.escape(name) + r"\s*=\s*\{(.*?)\};",
            dlg_text, re.DOTALL)
        if init:
            body = re.sub(r"//[^\n]*", "", init.group(1))   # drop line comments
            fields = _split_commas(body)
            fields = [f for f in fields if f]
            if len(fields) >= 2:
                try:
                    start_node = int(fields[-2])
                    refused_node = int(fields[-1])
                except ValueError:
                    pass

        scripts.append({
            "name": name,
            "nodes": nodes,
            "edges": edges,
            "startNode": start_node,
            "refusedNode": refused_node,
        })
    return scripts


def cmd_dialogs(args):
    if not os.path.isfile(args.dialog_src):
        raise SystemExit("dialog source not found: %s" % args.dialog_src)
    with open(args.dialog_src, "r", encoding="utf-8") as f:
        dlg_text = f.read()
    scripts = parse_dialog_scripts(dlg_text)
    out = {
        "source": os.path.relpath(args.dialog_src, PROJECT_ROOT).replace("\\", "/"),
        "scripts": scripts,
    }
    payload = json.dumps(out, indent=2)
    if args.stdout:
        print(payload)
    else:
        with open(args.dialog_out, "w", encoding="utf-8") as f:
            f.write(payload)
        total_nodes = sum(len(s["nodes"]) for s in scripts)
        print("Wrote %s : %d scripts, %d nodes"
              % (os.path.relpath(args.dialog_out, PROJECT_ROOT), len(scripts), total_nodes))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="Extract / edit the game FSM.")
    ap.add_argument("--src", default=DEFAULT_SRC, help="Path to GameStateMachine.cpp")
    ap.add_argument("--header", default=DEFAULT_HEADER, help="Path to GameStateMachine.h")

    sub = ap.add_subparsers(dest="cmd")

    p_ex = sub.add_parser("extract", help="Parse the FSM into JSON (default).")
    p_ex.add_argument("--out", default=DEFAULT_OUT)
    p_ex.add_argument("--stdout", action="store_true")
    p_ex.set_defaults(func=cmd_extract)

    p_add = sub.add_parser("add-state", help="Add a new state (enum + table row + stub handlers).")
    p_add.add_argument("name", help="STATE_NAME (e.g. STATE_NPC_DIALOG)")
    p_add.add_argument("--category", default="page", choices=VALID_CATEGORIES)
    p_add.add_argument("--apply", dest="dry_run", action="store_false",
                       help="Actually write files (default is a dry-run diff).")
    p_add.set_defaults(func=cmd_add_state, dry_run=True)

    p_cat = sub.add_parser("set-category", help="Change an existing state's @cat tag.")
    p_cat.add_argument("name")
    p_cat.add_argument("category", choices=VALID_CATEGORIES)
    p_cat.add_argument("--apply", dest="dry_run", action="store_false",
                       help="Actually write files (default is a dry-run diff).")
    p_cat.set_defaults(func=cmd_set_category, dry_run=True)

    p_dlg = sub.add_parser("dialogs", help="Extract dialog scripts into dialogs.json.")
    p_dlg.add_argument("--dialog-src", default=DEFAULT_DIALOG_SRC)
    p_dlg.add_argument("--dialog-out", default=DEFAULT_DIALOG_OUT)
    p_dlg.add_argument("--stdout", action="store_true")
    p_dlg.set_defaults(func=cmd_dialogs)

    args = ap.parse_args(argv)

    # Default subcommand = extract (so `python fsm_graph.py` still works).
    if args.cmd is None:
        args.out = DEFAULT_OUT
        args.stdout = False
        return cmd_extract(args)
    try:
        return args.func(args)
    except (ValueError, FileNotFoundError) as e:
        raise SystemExit("error: %s" % e)


if __name__ == "__main__":
    sys.exit(main())
