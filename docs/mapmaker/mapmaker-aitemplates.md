# MapMaker Story AI groups (duplicate, leader and numbered linked copies)

Status: implemented in `src/interface/pc_mapmaker_aitemplates.inc`, fixture-tested
(`tests/mapmaker/test_mapmaker_aitemplates.py`, mutations; decode checks in
`tests/mapmaker/test_mapmaker_aitemplates_classified.py`). Not built or played yet.
Opt-in: `TSFP_MAPMAKER_LIMITS=1`; with it off every hook only runs its
original (except the menu-pool diagnostic below). Guest-code table:
`config/guest-code-patches/aitemplates-pool.json` (editor menu item pool,
unconditional). Research basis: `logs/limits/features/ai-templates.md` and
`logs/limits/features/linked-ai-bugs.md` (main checkout, ignored).
The area keeps its name `aitemplates` (hooks, files) although the earlier
"All X" template entry is gone (user feedback 2026-10-31: it added two list
entries; the user chose "Leader + numbered copies").

## What the player gets

In the MapMaker editor, open a Story AI from the Story AI list (its edit
screen). Rows appear just above the last row ("Done"):

| Story AI is | Rows |
|---|---|
| not linked | **Duplicate Story AI**, **Add linked copy** |
| a group leader | **Add linked copy**, **Logic means: this AI** / **Logic means: whole group**, **Unlink (next copy leads)** |
| a linked copy | **Add linked copy**, **Unlink copy** |
| type 5 (game mode 7 singleton) | none |

- **Duplicate Story AI**: a new, independent Story AI with every setting of
  this one (facing and patrol mode included), a fresh default name and its own
  colour, but **no points**: it starts unplaced, shows the "!" next to its
  icon in the Story AI list and must be placed (select its icon row, put it
  on a cell). Changed 2026-10-31 (user report "linked story AI ... acting as
  if they have already been placed"): before, the duplicate copied the path
  and sat invisibly on the source's pad, and spawned there in play.
- **Add linked copy**: on an unlinked AI "Guard" it makes Guard the group
  leader (no extra entry) and adds **"Guard 2"**, unplaced like a duplicate
  ("!" in the list, place it yourself; it spawns where it is placed); on any
  member it adds the next copy ("Guard 3", ...). The number is the smallest
  >= 2 that no Story AI name uses as "Guard N". When the leader itself is
  numbered (a promoted "Guard 2" next to "Guard 3") the base is "Guard"; a
  leader whose own name merely ends in a number with no numbered member
  ("Story AI 3") keeps it ("Story AI 3 2"). Names longer than 27 characters are
  cut before the number. A copy never takes points from any member (before
  2026-10-31 it copied the clicked member's points, or the first placed
  member's, so every copy made after one member was placed was born on that
  member's pad: no "!", its icon row jumped the camera instead of picking it
  up, and in play it spawned on the leader's spot). Patrol copies (types 1/3)
  start with no path: the first placement sets their spawn, then add
  waypoints from the list's insert rows.
- Maps saved before the fix keep their stacked copies (nothing is
  un-placed automatically). With `TSFP_MAPMAKER_STACK_ITEMS=1`, picking up
  from a shared cell with empty hands lifts the **highest-index** (newest)
  Story AI first, so the copy comes off before its leader
  (`pc_mapmaker_stack_sai_point`, docs/mapmaker/mapmaker-grid.md).
- Story AI list length: the list is one menu built from the global menu item
  pool. Each AI costs 2 rows (unplaced or a non-patrol type) up to 18 (a
  type 1/3 patrol with 8 points); the stock editor pool of 920 items fits
  ~50 AIs at worst. `aitemplates-pool.json` raises it to 3072 items (0x0C00,
  +0.63 MB of Level heap in the editor): every AI unplaced or non-patrol
  (2 rows x 255), or ~170 full 8-point patrols. Rows past a full pool
  silently vanish from the list; stderr then shows `[mapmaker] menu item
  pool full (3072 items)` once.
- Story AI list, as previewed: `Guard`, `Guard 2`, `Guard 3`, `Sniper`.
  No "(leader)" / "(linked)" markers: the list label is the AI's saved 32-byte
  name (entry +0x00), so a marker would have to be written into the saved
  name; the list's row builder was not hooked (INFERRED: it shows the name
  directly). The edit screen's rows show the role instead.
- **Logic means** (leader only): toggles group logic; the label shows the
  current state. Off (default): logic on the leader means the leader only, as
  stock. On: logic on the leader means any member (see Logic). Logic on a copy
  always means that copy.
- **Unlink copy**: the copy keeps its settings and leaves the group.
  **Unlink (next copy leads)** on the leader: the leader leaves, its first copy
  (lowest list position) becomes the leader of the others, with group logic
  off (its own logic keeps meaning "this AI"). A group left with one member
  dissolves (its group logic is cleared).
- Deleting the leader promotes the first copy the same way (stock logic
  fix-up removes logic on the deleted AI); deleting anything renumbers links.
- Linked AIs share character, variant (+0x2C), the on/off toggle (+0x30), type,
  spawn count, spawn delay, team (+0x44), weapon and behaviour. Changing any
  of these on one member changes the whole group when its edit screen closes
  (list) or at the next save/compile, edit screen or delete (map path). Name,
  colour, points, facing (+0x54) and patrol mode (+0x5C) stay per AI.
- At the 255 limit or without container room the stock messages appear on the
  edit screen, which stays open. Every copy needs one free slot. The other
  rows close the edit screen (back to the list), like "Done".

## Logic: "whole group" targeting

Chosen: the per-leader toggle row, stored in a byte the stock game leaves
zero. Rejected: an extra logic-picker entry "Guard [group]". Story AI logic ids
are by index, `[0x00783A50]` + index + 1 (R = `sub_0031DA60`), and the next id
class (objects) starts at `[0x00783A18]` = R + Story AI count (`sub_0031DA90`,
0x0031DA9B, CONFIRMED). A picker entry without a real AI record would need an
index >= the count, which is an object id in play; a stock game (or the switch
off) would run such logic on an object, and the editor's delete fix-up
(`sub_00323330`) would renumber it as an AI. The toggle keeps every saved
logic node a plain AI reference.

With group logic on the leader, in play (scene 100):

| Logic on the leader | Effect |
|---|---|
| trigger 0xB (event-style, flag 2 from `sub_0033E560`) | fires when the event happens to any member |
| trigger 0x14 (poll `sub_00339600`, actor flag 4, then a delay) | fires when any member qualifies |
| action 2 (`sub_00163780`, `sub_000BD090` on each live actor) | applies to every member |
| action 0x11 (spawn gate `sub_0033E620`, types 2/3/4) | holds every member until it fires (a copy's own 0x11 action still applies too) |

The trigger/action names (kill, remove, spawn, ...) are inferred from code
paths; text ids for them were not read.

## Save format and compatibility

- Editor entry +0x50 (dword, never addressed by the game: no access in the
  editor tracker's site list, logs/limits/ai-logic.md section A): low word =
  leader index + 1 on a copy, 0 otherwise; bit 16 = group logic on a leader.
- Story AI chunk (tag 0x00C7766C) data +0x32 (word): copy = 0x4000 | leader
  ordinal + 1; leader = 0x4000 | 0x8000 when group logic is on; 0 otherwise.
  0x4000 marks this format. The compiler zeroes the container first
  (`rep stosd` at 0x0030070D) and the writer (0x003013E1..0x003015DF) never
  stores +0x32/+0x33; the loader (0x00302428..0x0030267A) never reads them
  (both checked in the decode, `test_sai_chunk_bytes_0x32_unused`; the flag
  bits live in +0x33, inside the same verified word). The writer stores the
  loop index at +0x22 (0x0030143F) and writes every entry, so chunk ordinal =
  editor index = save index.
- Stock game / `TSFP_MAPMAKER_LIMITS` off: every member is an ordinary
  independent AI (the leader is a normal AI with its points); group logic does
  nothing, logic on the leader means the leader; re-saving drops the links.
- Old maps: +0x32 is 0, nothing linked. Loader robustness: a link is kept only
  if it names another chunk of the same load whose word has no link, the word
  has no group bit, and the loaded entry holds its chunk's saved index
  (+0x24 = +0x22); otherwise it is dropped and logged
  (`[mapmaker] story AI groups: link dropped at load`). A group bit on an AI
  without copies is cleared. Stale +0x50 values are always overwritten.
- Migration of the earlier template format (plain link word without 0x4000 to
  a point-less "All Guard" entry): at editor load the template's first linked
  member (lowest index, normally the original Guard) becomes the leader, the
  other members link to it, group logic off; the template is left in place as
  an unlinked point-less AI (logged `old template group converted`). It is not
  deleted: deleting at load would run the stock logic fix-up (removing its
  logic nodes) and shift every later index. A point-less AI is harmless (the
  play builder drops it, 0x00241DDD) and the user can delete it. Logic on the
  old template does nothing once the map is re-saved; move it to the leader
  and turn on "Logic means: whole group". An old-format map played without
  re-saving keeps the old meaning (logic on the template = any copy).

## Hooks (WRAP lines for the integrator)

Unchanged by the leader rework (same functions, same hook names):

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
WRAP sub_000715E0 -> pc_mapmaker_aitemplates_menu_item   (menu item allocator; diagnostic, logs once when the pool is full in scene 102)
```

## Menu item pool (CONFIRMED, decode)

`sub_00070DB0` (called by `sub_002D6E10`, `sub_002F4300`, `sub_00302A60`)
sets the pool count `[0x006FE920]`: `mov eax, 0x398` at 0x00070DC3 in the
scene 102 branch (`cmp eax, 0x66` 0x00070DBE), 650 / 200 computed at
0x00070DCA..0x00070DDF for scene 101 / elsewhere. Items `[0x007BB1DC]` =
count x 0x10C and the selection ring `[0x007BB1D0]` = count x 0x18 come from
the Level heap (`sub_0003DFA0(size, 1)`, 0x00070DF5 / 0x00070E14). A decode
scan of every reference to the three variables finds only readers of the
count (0x00070DFF, the ring modulo 0x00070EDF, allocator `sub_000715E0`
0x000715E0, clear `sub_00074F70` 0x00074F81, `sub_00078980` 0x00078C24); the
immediate 0x398 appears elsewhere only in unrelated functions (0x0019893D,
0x001EC43B, 0x002C7B7E). The row builder `sub_00078FF0` takes its item from
`sub_000715E0` (0x0007906C, its only call site) and does not test for 0.
The operand patch therefore changes the editor's pool only. Value 0x0C00
rather than the report's 0x1400 (5120, +1.17 MB): the editor's Level-heap
headroom has never been measured and the editor already holds the 2.5 MB
undo tables and the 0x58390-byte editor object; 0x0C00 covers every
realistic list and the diagnostic says when it does not (raise it after a
heap reading from the meter probe, flag 32).

"Chain outside X" means `pc_mapmaker_aitemplates_<what>(chain)` where `chain`
is a static function calling `X(sub_..._gen)` (or `sub_..._gen` when X is
disabled by `pc_mapmaker_area_enabled`). Inner order matters for compile (the
link is written after the container area's compile) and load (links are read
after the editor area restored hidden chunk tags; chunks past 255 are not
loaded and are skipped by count). Include the `.inc` after
`pc_mapmaker_limits.inc` (it uses `mapmaker_limits`, `MAPMAKER_GUEST_RET`,
`MAPMAKER_PTR_OK`); it does not depend on `pc_mapmaker_editor.inc`.
Declarations go between the area hook markers in `src/interface/pc_display_menu.h`,
no-op stubs between the stub markers in `pc_menu_mouse_guest.inc`.
`xbox_HeapAlloc` (192 bytes once, the six row labels) is the same allocator the
quit menu uses; whole-chain fixtures need it (menu_mouse.c/fov_settings.c get
it from `tests/mapmaker/mapmaker_area_stubs.h` if missing there).

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
  decode). The rows use ids 0x41540001..4.
- Close `sub_0031CAC0(menu)`: clamps points for types other than 1/3, compares
  the entry with the scratch copy (change flag, `sub_003370A0(0x13)`), clears
  the scratch, `sub_0007A7A0(menu)`; plain `ret`.
- Add `sub_0031BE70(menu)` -> `sub_0031BB60(type 0, menu)`: zeroes the new
  entry, default name, colour; menu 0 skips `sub_000746D0`. Messages as
  stock: 0xBB0 with the limit (`sub_003C85D4` into the editor object) and
  0x9B6 via `sub_0007A270(menu, text, 0)`.
- Delete `sub_0031C8B0(i)`: `sub_00323330(i)` logic fix-up, memmove, count-1.
- Id classes: `sub_0031DA60` sets R = `[0x00783A50]` = editor+0xDD10;
  `sub_0031DA90` sets the object base `[0x00783A18]` = R + editor+0x280
  (Story AI count, 0x0031DA9B).
- Ids: `sub_003396E0`/`sub_00339780` build trigger/action ids as
  `[0x00783A50] + node+0xE + 1`; kind-2 handlers: triggers 0xB -> 0x003395E0,
  0x14 -> 0x00339600; actions 2 -> 0x00339750 (`sub_00163780`), 0x11 ->
  0x00339770 (gate flag 4 read by `sub_0033E620`). These four plus event
  delivery `sub_0033E560` are every kind-2-specific handler in those two
  constructors.

## Risks and open items

- Copies and duplicates now start unplaced (no spawn until placed; the "!"
  shows it). Two AIs deliberately put on one pad in play is still untested.
- Stock quirk (not fixed, INFERRED UI effect, CONFIRMED code): AI 0's point
  and insert row ids are 0..8 (`(0 << 16) + k`), equal to the name-row ids of
  AIs 0..8. `sub_0031C9E0` treats any highlighted id below the AI count with
  occurrence 0 (`menu+0x190`, `sub_000719A0`) as a name row (highlight text
  0xA9F, Y = delete AI [id], 0x0031CA2F..0x0031CAAA), so Y on AI 0's insert
  row after point k may offer to delete AI k+1. A fix needs a different row
  id scheme for AI 0's point rows (every list callback decodes `id >> 16`),
  which is not clearly safe; left stock.
- A leader's edit screen gains three rows; if a type 2/3/4 AI's screen is
  already full, the bottom rows might not fit (check the layout).
- The toggle closes the edit screen; reopen it to see the new label (whether
  the menu copies row text was not checked, so the label is not rewritten in
  place).
- Shared-field propagation from the map path (`sub_002FB3C0` close) happens at
  the next save/compile, edit screen open or delete, not immediately.
- A remembered edit is matched by editor object, index and colour id; a
  different AI with the same colour at the same index after a reset could
  receive a stale propagation (only if it is itself in a group).
- Turning group logic on changes the meaning of logic already placed on the
  leader (it then means any member); promotion leaves it off for that reason.
- System Link: each machine applies group logic only if it runs this build
  with the switch on.
- Trigger 0x14 on a group leader polls each copy in turn; its delay state is
  the leader trigger's own.

## Test steps (user)

Run with `TSFP_MAPMAKER_LIMITS=1`.

1. Duplicate: place a Story AI "Guard" (type 1 patrol with 3 points; then a
   respawner type 2). Story AI list -> Guard -> **Duplicate Story AI**. A new
   AI appears (default name, other colour) with the same settings, the "!"
   next to its icon and no path: select its icon, place it.
   Try on a mode 7 type 5 AI: no rows.
2. Linked copies: Guard -> **Add linked copy**: only **Guard 2** is added (no
   "All Guard"), with the "!". From Guard 2 add another (**Guard 3**), and one
   from Guard (**Guard 4**). Each shows "!" and is picked up from its icon
   row; place each on its own cell. Rename an unrelated AI to "Guard 5" and
   add a copy: it is named "Guard 6".
2a. Placed leader, many copies: a placed type 0/2 "Guard" -> Add linked copy
   30 times: every copy shows "!" and none sits on Guard's cell unless put
   there. Play: every copy spawns where it was placed (watch copies past 24),
   none on the leader's spot; an unplaced copy does not spawn.
2b. Patrol leader type 1 with 3 points: its copies show the icon row with
   "!"; the first placement sets their spawn, add waypoints after.
2c. Old map saved with pre-placed copies, `TSFP_MAPMAKER_STACK_ITEMS=1`:
   copies still sit on the leader's cell; picking up there lifts the newest
   copy first (stderr `pick-up takes the newest story AI on a shared cell`).
2d. Pool: 60+ patrol AIs with 8 points each, scroll the Story AI list to the
   bottom: all rows present; stderr has no `menu item pool full` (with ~170+
   such AIs the line appears and the last rows are missing).
3. Rows: Guard shows Add linked copy / Logic means: this AI / Unlink (next
   copy leads); Guard 2 shows Add linked copy / Unlink copy; an unlinked AI
   shows Duplicate / Add linked copy.
4. Edit any member (weapon, character), close: all members change; names,
   colours and points do not. Unlink Guard 4 and edit Guard again: Guard 4
   keeps its old settings.
5. Logic, one AI: a trigger on Guard 2 (e.g. killed -> objective): killing
   Guard 3 does not fire it. The same trigger on Guard with "Logic means: this
   AI": only killing Guard fires it.
6. Logic, whole group: Guard -> **Logic means: this AI** (screen closes);
   reopen: it reads **Logic means: whole group**. Now a trigger on Guard fires
   when any member is killed; an action on Guard (remove/kill style) affects
   every member; a spawn action on Guard releases every held type 2/3/4 member;
   an alert-style (0x14) trigger on Guard fires for any member.
7. Promotion: Guard -> **Unlink (next copy leads)**: Guard 2 now shows the
   leader rows (logic "this AI"), Guard 3 still follows it. Delete Guard 2:
   Guard 3 leads the rest. Unlink the last copy: no leader rows remain.
8. Save, reload (editor): groups and the group-logic setting kept. Undo after
   "Add linked copy", after a toggle and after a group edit.
9. An old map saved with the previous build (it has "All Guard"): load it in
   the editor: Guard leads Guard's copies, "All Guard" is an ordinary
   point-less AI (stderr: `old template group converted`); delete it if
   wanted. Before re-saving, playing it still treats logic on "All Guard" as
   any copy.
10. Without the switch (or a stock build if available): load the saved map;
    every member is an independent AI; group logic does nothing.
11. A story level and an arcade/MapMaker map without links behave as before.
