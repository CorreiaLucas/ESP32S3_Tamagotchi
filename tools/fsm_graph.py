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

    # Each row: optional `/* STATE_X */` then `{ enterFn, updateFn }`.
    row_re = re.compile(
        r"(?:/\*\s*(?P<state>STATE_\w+)\s*\*/\s*)?"
        r"\{\s*(?P<enter>[A-Za-z_]\w*)\s*,\s*(?P<update>[A-Za-z_]\w*)\s*\}",
    )

    handlers = {}  # state -> {"onEnter": fn, "onUpdate": fn}
    positional = []
    for m in row_re.finditer(body):
        entry = {"onEnter": m.group("enter"), "onUpdate": m.group("update")}
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

        nodes.append(
            {
                "id": state,
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


def main(argv=None):
    ap = argparse.ArgumentParser(description="Extract the game FSM into JSON.")
    ap.add_argument("--src", default=DEFAULT_SRC, help="Path to GameStateMachine.cpp")
    ap.add_argument("--header", default=DEFAULT_HEADER, help="Path to GameStateMachine.h")
    ap.add_argument("--out", default=DEFAULT_OUT, help="Output JSON path")
    ap.add_argument("--stdout", action="store_true", help="Print JSON to stdout instead of writing a file")
    args = ap.parse_args(argv)

    if not os.path.isfile(args.src):
        ap.error("source not found: %s" % args.src)
    if not os.path.isfile(args.header):
        ap.error("header not found: %s" % args.header)

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
        print(
            "Wrote %s : %d states, %d transitions"
            % (os.path.relpath(args.out, PROJECT_ROOT), len(graph["states"]), len(graph["transitions"]))
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
