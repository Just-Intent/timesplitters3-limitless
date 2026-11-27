# MapMaker logic: action Delay (extends [limitless.md](limitless.md))

Player view: [mapmaker-limitless.md](../mapmaker/mapmaker-limitless.md)
"Logic: Delay on actions". Design, stock logic model and later phases:
`logs/limits/features/logic-design.md` (ignored, main checkout).

Stock logic model (CONFIRMED, text.asm and the 8 sample maps in
`l_102.pak`): record 0x6F (0x8C, a thing: type +0x80), node 0x6D (0xB4: +0
kind 0 op / 1 trigger / 2 action, +4 subject 0..15, +8 verb, +0xC record,
+0xE Story AI, +0x10 parameter, +0x20 name, +0xA4 op of an op's own node),
op 0x6E (0x220: +4 kind, +0x50 triggers at +0x54 (4 B, 10), +0x7C modes
(bit 0 NOT; the first trigger's bit 1 = any of), +0x88 actions at +0x8C (4
B, 50), +0x154 + 4k spare, +0x21C objective). Builders: table 0x0052ED28
(16 {trigger, action} pairs, no bound check: never store a subject >= 16).
Per frame `sub_0033D1B0` (level desc +0x110): timers, evaluate
`sub_0033CD00`, fire `sub_0033CF80`, re-arm `sub_0033D090`.

## Entry points

`src/interface/pc_mapmaker_logic.inc` (area `logic`, included after
`pc_mapmaker_restable.inc`; prototypes in `pc_display_menu.h`, no-op stubs in
`pc_menu_mouse_guest.inc`). Generated wrappers (area `logic`):

    WRAP sub_0033AA80 -> pc_mapmaker_logic_setup
    WRAP sub_0033CF80 -> pc_mapmaker_logic_fire
    WRAP sub_000784A0 -> pc_mapmaker_logic_rows
    WRAP sub_00328BF0 -> pc_mapmaker_logic_done
    WRAP sub_00324B60 -> pc_mapmaker_logic_actions

`sub_000784A0` is also wrapped by aitemplates (inner) and bosses (outer);
logic goes outermost. Each hook acts on its own return address, so the order
is not load-bearing. The other four are new wrappers.

## Contract

- Storage: the Delay is op +0x154 + 4k (low word, tenths of a second, 0 =
  none). Stock writes 0 when it adds an action (0x00320944). The delete
  fix-up moves the slot (0x00322F65/0x00322F6B). Loaders and the writer copy
  ops whole. Play copies the word to runtime op +0x194 + 2k (0x0033AC14);
  nothing reads it there (`test_spare_op_word`). No save-format change. A
  stock game runs the action at once. Steps: 0, 0.5, 1, 2, 3, 5, 10, 15, 30,
  60 s; a value off the list steps to the next larger one.
- Editor (`TSFP_MAPMAKER_LIMITS=1`, scene 102): the edit screen `sub_00328D20`
  (menu 0x0052EA48, node editor +0x11858) calls `sub_000784A0` at 0x00329B5C
  (return 0x00329B61) before its Done row (callback `sub_00328BF0`). For an
  action node inside the node array (+0x2FB70, count +0x81A4) the hook adds
  the row (`sub_00079D60(label, 0x4C474401, 0, 0, 0x00328BF0, 0)`). A click
  reaches `sub_00328BF0` with menu +0x18C = the row id. The hook steps the
  Delay in every op slot that holds the node, rewrites the label in place,
  and pops the return (the screen stays open). An action in no op shows
  "Delay: add to an operation first" and stores nothing. New nodes on the
  scratch copy (+0x11618) get no row. The action picker's rebuild
  `sub_00324B60` (from `sub_003282A0`) zeroes the op's slots: the hook
  snapshots node -> delay first, then restores it. A node new to the op takes
  its delay from another op.
- Play (scene 100, Story mode `[0x007DE455]` == 10, only when a runtime op
  slot has a delay): `setup` empties the queue at each level start or restart
  (`sub_00340A60`) and logs one summary. `fire`:
  1. Ages the queue by whole ticks `[0x004E7944]` (0 on native subticks).
  2. Releases due entries through the original (`guest_call(original, 0)`):
     their op is flagged alone, its list is cut to the due nodes (at most 50
     per pass), and the flags and lists are restored.
  3. For this frame's flagged ops, queues the delayed slots (ticks = tenths x
     `[0x004E8CEC]` / 10) and cuts them out of the list.
  4. Runs the original, then restores the lists.

  The queue holds 256 entries; on overflow the action fires at once and is
  logged once. Other modes, and maps without delays, run the original alone.
- Logs: `[mapmaker] logic: N operations, M actions, K with a delay (a..b)`
  per level (64 at most). Editor notes are capped. `TSFP_MAPMAKER_LOGIC_TRACE=1`
  logs every `sub_000784A0` return in the editor with the current node
  (kind/subject/verb/index), to pin the screen in a playtest.
- Offline: `tools/testing/map_report.py` prints a `logic:` summary line when a
  map has operations; `--logic` prints each operation in words (triggers with
  NOT/and/or, actions with "(after 5 s)").

## Tests

`tests/mapmaker/test_mapmaker_logic.py` covers:

- the fixture `tests/mapmaker/mapmaker_logic.c` (play queue, release, restore,
  overflow, restart, non-Story, editor row and cycle, picker rebuild) with 22
  mutations;
- decoder checks of the spare word and hook sites (private XBE);
- the sample maps (140 action slots, all 0) and their printout;
- wiring.

Registered in `.github/workflows/runtime-patches.yml`. Not built or played.
Mac: shared code, no adapter (pending native validation).
