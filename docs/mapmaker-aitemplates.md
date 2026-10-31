# MapMaker Story AI templates (duplicate and linked copies)

Status: implemented in `src/pc_mapmaker_aitemplates.inc`, fixture-tested
(`tests/test_mapmaker_aitemplates.py`, 46 mutations; decode checks in
`tests/test_mapmaker_aitemplates_classified.py`). Not built or played yet.
Opt-in: `TSFP_MAPMAKER_LIMITS=1`; with it off every hook only runs its
original. No guest-code table entry (`config/guest-code-patches/` unchanged).
Research basis: `logs/limits/features/ai-templates.md` (main checkout, ignored).

## What the player gets

In the MapMaker editor, open a Story AI from the Story AI list (its edit
screen). Two rows appear just above the last row ("Done"):

| Story AI is | Rows |
|---|---|
| not linked (or a template) | **Duplicate Story AI**, **Add linked copy** |
| a linked copy | **Add linked copy**, **Unlink copy** |
| type 5 (game mode 7 singleton) | none |

- **Duplicate Story AI**: a new, independent Story AI with every setting and
  every point (path) of this one, a fresh default name and its own colour.
  It starts on the same pad(s); move its point if needed.
- **Add linked copy**: the first time, on an unlinked AI "Guard", it creates
  a template **"All Guard"** (same settings, no points, so it never spawns) and
  a copy of Guard at Guard's points, and links Guard and the copy to the
  template. Further "Add linked copy" (on any member or on the template) adds
  one more copy (points from the AI used, or from the first copy with points
  when used on the template).
- **Unlink copy**: the AI keeps its settings but leaves the group.
- Linked AIs share character, variant (+0x2C), the on/off toggle (+0x30), type,
  spawn count, spawn delay, team (+0x44), weapon and behaviour. Changing any
  of these on one member (or the template) changes the whole group when its
  edit screen closes (list) or at the next save/compile, edit screen or delete
  (map path). Name, colour, points, facing (+0x54) and patrol mode (+0x5C)
  stay per AI.
- At the 255 limit or without container room the stock messages appear on the
  edit screen, which stays open. A first linked copy needs two free slots.
- Deleting the template unlinks its copies; deleting anything renumbers links.

Logic: every AI keeps its own entry in the logic picker, so logic on a copy is
"this copy" exactly as before. Logic on the template ("All Guard") means "any
linked copy" in play (scene 100):

| Logic on the template | Effect |
|---|---|
| trigger 0xB (event-style, flag 2 from `sub_0033E560`) | fires when the event happens to any copy |
| trigger 0x14 (poll `sub_00339600`, actor flag 4, then a delay) | fires when any copy qualifies |
| action 2 (`sub_00163780`, `sub_000BD090` on each live actor) | applies to every copy |
| action 0x11 (spawn gate `sub_0033E620`, types 2/3/4) | holds every copy until it fires (a copy's own 0x11 action still applies too) |

The trigger/action names (kill, remove, spawn, ...) are inferred from code
paths; text ids for them were not read. Whether the logic AI picker lists a
point-less AI is inferred (it lists editor entries) and must be checked.

## Save format and compatibility

- Editor entry +0x50 (dword, never addressed by the game: no access in the
  editor tracker's site list, logs/limits/ai-logic.md section A) = template
  index + 1, 0 = none.
- Story AI chunk (tag 0x00C7766C) data +0x32 (word) = the same value. The
  compiler zeroes the container first (`rep stosd` at 0x0030070D) and the
  writer (0x003013E1..0x003015DF) never stores +0x32/+0x33; the loader
  (0x00302428..0x0030267A) never reads them (both checked in the decode). The
  writer stores the loop index at +0x22 (0x0030143F) and writes every entry,
  so chunk ordinal = editor index = save index.
- Stock game / `TSFP_MAPMAKER_LIMITS` off: the copies are ordinary independent
  AIs; the template is an AI without points (the play builder drops it,
  0x00241DDD); logic on the template never fires; re-saving drops the links.
- Old maps: +0x32 is 0, nothing linked. Loader robustness: a link is kept only
  if it names another chunk of the same load that is itself unlinked, and the
  loaded entry holds its chunk's saved index (+0x24 = +0x22); otherwise it is
  dropped and logged (`[mapmaker] story AI templates: link dropped at load`).
  Stale +0x50 values (the loader does not write +0x50) are always overwritten.

## Hooks (WRAP lines for the integrator)

```
WRAP sub_000784A0 -> pc_mapmaker_aitemplates_rows        (generic menu spacer; acts only for return 0x0031D083)
WRAP sub_0031CAC0 -> pc_mapmaker_aitemplates_close
WRAP sub_0031CB90 -> pc_mapmaker_aitemplates_open
WRAP sub_0031C8B0 -> pc_mapmaker_aitemplates_delete
WRAP sub_003006C0 -> pc_mapmaker_aitemplates_compile     (already wrapped: chain OUTSIDE pc_mapmaker_container_compile)
WRAP sub_00301A90 -> pc_mapmaker_aitemplates_load        (already wrapped: chain OUTSIDE pc_mapmaker_editor_load)
WRAP sub_00240C50 -> pc_mapmaker_aitemplates_level       (already wrapped: chain OUTSIDE pc_mapmaker_editor_level_load)
WRAP sub_0033E560 -> pc_mapmaker_aitemplates_event
WRAP sub_00163780 -> pc_mapmaker_aitemplates_remove
WRAP sub_0033E620 -> pc_mapmaker_aitemplates_spawn_gate
WRAP sub_00339600 -> pc_mapmaker_aitemplates_alert       (called through trigger +0x10)
```

"Chain outside X" means `pc_mapmaker_aitemplates_<what>(chain)` where `chain`
is a static function calling `X(sub_..._gen)` (or `sub_..._gen` when X is
disabled by `pc_mapmaker_area_enabled`). Inner order matters for compile (the
link is written after the container area's compile) and load (links are read
after the editor area restored hidden chunk tags; chunks past 255 are not
loaded and are skipped by count). Include the `.inc` after
`pc_mapmaker_limits.inc` (it uses `mapmaker_limits`, `MAPMAKER_GUEST_RET`,
`MAPMAKER_PTR_OK`); it does not depend on `pc_mapmaker_editor.inc`.
Declarations go between the area hook markers in `src/pc_display_menu.h`,
no-op stubs between the stub markers in `pc_menu_mouse_guest.inc`.
`xbox_HeapAlloc` (96 bytes once, the row labels) is the same allocator the
quit menu uses; whole-chain fixtures need it (menu_mouse.c/fov_settings.c get
it from `tests/mapmaker_area_stubs.h` if missing there).

## Sites (CONFIRMED in text.asm and, where marked, the decode)

- List row select `sub_0031BF00`: stores the index byte at [0x00762F94], copies
  the entry to scratch editor+0x11758 and opens menu table 0x0052E088
  (decode); table dword 0 = build `sub_0031D140` (XBE, decode test), which
  calls `sub_0031CB90(menu, 0x0031CAC0, 4)` (decode). The map-path table
  0x0052E098 (from 0x00305809) builds with `sub_0031D160` and close
  `sub_002FB3C0`; no rows are added there.
- Edit screen `sub_0031CB90`: `call sub_000784A0` at 0x0031D07E, then the
  "Done" row `sub_00079D60(text 0xD73, -1, 0, 0, [esp+0x20] = close, 0)` at
  0x0031D083..0x0031D0A1 (decode). Row arguments match the quit menu's
  `sub_00079D60(text, id, 0, 0, callback, 0)`.
- Menu engine `sub_0007AE..`: highlight stores the row id at menu+0x18C
  (0x0007AF66); A (menu+0xC4 bit 0x10 / 0x100000) calls the row callback
  (0x0007B0F6); B (bit 0x20) stores menu+0x1E8 (initialised to -1 at
  0x0007763A) into menu+0x18C and calls the back callback (0x0007B1D7..0x0007B1E4,
  decode). The rows use ids 0x41540001..3.
- Close `sub_0031CAC0(menu)`: clamps points for types other than 1/3, compares
  the entry with the scratch copy (change flag, `sub_003370A0(0x13)`), clears
  the scratch, `sub_0007A7A0(menu)`; plain `ret`.
- Add `sub_0031BE70(menu)` -> `sub_0031BB60(type 0, menu)`: zeroes the new
  entry, default name, colour; menu 0 skips `sub_000746D0`. Messages as
  stock: 0xBB0 with the limit (`sub_003C85D4` into the editor object) and
  0x9B6 via `sub_0007A270(menu, text, 0)`.
- Delete `sub_0031C8B0(i)`: `sub_00323330(i)` logic fix-up, memmove, count-1.
- Ids: `sub_003396E0`/`sub_00339780` build trigger/action ids as
  `[0x00783A50] + node+0xE + 1`; kind-2 handlers: triggers 0xB -> 0x003395E0,
  0x14 -> 0x00339600; actions 2 -> 0x00339750 (`sub_00163780`), 0x11 ->
  0x00339770 (gate flag 4 read by `sub_0033E620`). These four plus event
  delivery `sub_0033E560` are every kind-2-specific handler in those two
  constructors.

## Risks and open items

- A duplicate starts on the same pad as its source; two AIs on one pad in play
  is untested (move the copy's point).
- The edit screen gains two rows; if a type 2/3/4 AI's screen is already full,
  the bottom rows might not fit (check the layout).
- Shared-field propagation from the map path (`sub_002FB3C0` close) happens at
  the next save/compile, edit screen open or delete, not immediately.
- A remembered edit is matched by editor object, index and colour id; a
  different AI with the same colour at the same index after a reset could
  receive a stale propagation (only if it is itself in a group).
- System Link: each machine applies the template logic only if it runs this
  build with the switch on.
- Trigger 0x14 on a template polls each copy in turn; its delay state is the
  template trigger's own.

## Test steps (user)

Run with `TSFP_MAPMAKER_LIMITS=1`.

1. Duplicate: place a Story AI "Guard" (type 1 patrol with 3 points; then a
   respawner type 2). Story AI list -> Guard -> **Duplicate Story AI**. A new
   AI appears (default name, other colour) with the same settings and path.
   Show in Map on the copy. Try on a mode 7 type 5 AI: no rows.
2. Limits: at 255 AIs (or with the map memory full) Duplicate shows the stock
   "limit"/"no room" message and stays on the edit screen.
3. Linked copies: Guard -> **Add linked copy**: "All Guard" (no points) and a
   copy appear; add two more copies from a copy and from "All Guard". Move the
   copies' points apart.
4. Edit the template (or any copy): change weapon/character, close: all
   members change; names, colours and points do not. Unlink one copy and
   change the template again: the unlinked one keeps its old settings.
5. Logic, one copy: a logic trigger on one copy (e.g. killed -> objective):
   killing another copy does not fire it.
6. Logic, any copy: the same trigger on "All Guard": killing any copy fires it.
   An action on "All Guard" (remove/kill style) affects every copy; a spawn
   action on "All Guard" releases every held type 2/3/4 copy. Trigger 0x14
   style (alert) on the template fires for any copy.
7. Save, reload (editor): groups kept (edit the template again to check).
   Delete a copy, then the template: links fixed, no crash; logic on the
   deleted template removed as stock.
8. Undo after "Add linked copy" and after a template edit.
9. Without the switch (or a stock build if available): load the saved map; the
   copies are independent AIs, "All Guard" is a point-less AI; play it: the
   template never spawns, per-copy logic works, template logic does nothing.
10. A story level and an arcade/MapMaker map without links behave as before.
