# MapMaker grid: sixteen floors, 80x80, overlapping tiles, stacked items and Story AI

Editor-only feature area (scene 102). Research and evidence:
`logs/limits/features/floors-grid.md` and `overlap-items.md` (ignored, main
checkout). Code: `src/interface/pc_mapmaker_grid.inc`, `src/interface/pc_mapmaker_grid.h`,
`config/guest-code-patches/grid.json`; fixture `tests/mapmaker/mapmaker_grid.c` +
`tests/mapmaker/test_mapmaker_grid.py` (mutations, table/settings-block checks,
decoder inventory of the redirected constants, save round trips). Seven floors
were checked in game (2026-10-31); the sixteen-floor raise (2026-11-01, why it
stops at 16: `docs/agent-context/limitless-floors.md`) was not built or run.

## Switches (environment only)

| switch | effect |
|---|---|
| `TSFP_MAPMAKER_LIMITS=1` (pools on) | floors -2..13 (16 bars; the 3D view reaches 13 too), grid 80 x 80 x 17 levels (z -2..14), placement refused above level 14, view/pan/shading/placement bounds 80, widest zoom 100 cells |
| `TSFP_MAPMAKER_OVERLAP=1` | tile placement ignores occupancy (bounds still apply; layers above the top grid level refused) |
| `TSFP_MAPMAKER_STACK_ITEMS=1` | several items per cell for weapons, ammo, keycards, teleporters, health, armour, power-ups, special pickups and props (categories 8-13, 14-18, 19-21, 26, 31, 33, 35, 36); Story AI on cells with those items and with other Story AI, and those items on Story AI cells; starts, flags, turrets, movers, markers and unknown categories keep one per cell (nothing joins them) |
| `LIMITS` or `OVERLAP` | removing a piece writes back the cells of other pieces inside its box (last placed wins) |

The overlap and stack switches are not in `src/diagnostics/pc_debug_options.def`: those
entries are display/debug booleans persisted in JSON, and no MapMaker switch
(including `TSFP_MAPMAKER_LIMITS`) is wired through them yet. Adding them there
needs a host-side bridge from the setting to the environment on both
launchers.

## How it works

- Grid `[0x00784004]`: index `x + W*(y + W*(z+2))`. Stock W 40, 6 levels,
  0x9600 bytes. With the limits the hooks allocate W 80, 17 levels (0x6A400
  bytes = 425 KB of Level heap, editor only; 8 levels took 200 KB) and bound
  every axis (stock bounds only the flat index, which would wrap x into the
  next row).
- Floor up: stock `cmp ecx,2` at 0x00306E14 refuses with text 0x1576 through
  `sub_002F79E0` (return 0x00306E99). The hook turns that refusal at floors
  2..12 into the stock success (floor +1, below = old floor, text 0x157D).
  Floor down is unchanged (-2).
- 3D view (`[0x00778D34]`, floor `+0x98`, camera height floor x 5.0 at
  0x0032D649): its own floor change `sub_0032EDA0` refuses up at 2
  (0x0032EEBC) and was not raised for seven floors. `pc_mapmaker_grid_preview_floor`
  (return 0x0032F20F, limits, scene 102, no glide `+0x80 & 0x800`) runs it
  with the floor shifted to 1 for floors 2..12 and adds the shift back.
- Floor menu `sub_0030BE60`: loop count 5 -> 16 (`grid.json`); panel extent
  read from the settings block (5.0 / 16.0). After the first bar's
  `sub_0001ED50` (floor -2, return 0x0030BFB5) the hook sets the loop count
  (`[frame+0x18]` = esp+0x20 after the call) to 5 without the limits, so the
  stock menu draws its own 5 bars. If that slot does not hold 16 the old
  fallback applies: bars for floors 3+ get y = -100000 in their stack slot;
  nothing reads that slot after the loop. The panel grows downward from the
  stock top by 2.2% of the screen height per bar (16 bars: about 26%..60%).
- Settings block 0x00903600 (16 bytes, extension range 0x009036xx; 0x00903500 is the story gate; free per the
  feature brief): floors, extent, widest zoom, 1/widest zoom. Stock values are
  copied from the image constants, so stock mode is bit-exact. Written on the
  first hook call (the generic `sub_0001ED50` runs from boot) and again at
  editor grid allocation.
- `grid.json` redirects (decoder-checked inventory): 0x0030BEDD (5.0),
  0x002F8ED2, 0x002F9FBC, 0x002F9FE3, 0x0030A974, 0x0030AB90 (40.0),
  0x003075E2 (1/60), 0x0030764D (60.0). Other 40.0 reads: 0x00334F82 UI layout
  scale, 0x002E03A4 / 0x002E06DC front-end layout (INFERRED from context).
  The default view (16 cells at 20,20) is kept so old maps open where they
  were.
- `src/interface/pc_mapmaker_pointer.inc` reads the extent for its mouse-pan clamp
  (`MAPMAKER_GRID`), falling back to 40 for anything outside 40..127.
- Overlap: `sub_002F8140` answers 0 only when called from the placement probe
  (return 0x002F8F93) in scene 102; its side effects (+0x2F80, +0x25C) are
  kept. Connection building and pads still see the single grid owner, so
  overlap artefacts (sky windows, one-way walls, AI islands) are expected and
  depend on placement order (overlap-items.md section 4).
- Removal (`sub_00307DA0`, called by `sub_00308480` after the piece is
  unlinked): after the clear, every other placed piece's cells inside the
  removed box are written back in list order; cells outside the box are left
  alone, so a newer neighbour keeps the cells it owns.
- Stacked items: `sub_00308260` answers "none" at returns 0x003034AD and
  0x00303696 (held item from `sub_00303460`'s `[ebp+0x14]`), so nothing is
  replaced or uncounted; the pick-up caller 0x002F831B is unchanged (repeated
  pick-up peels the oldest first). `sub_00334770` sees the piece's items in the
  cell whose category may share with x = 0x7FFF for the call (restored after,
  also on refusal); held id editor+0x2F34. Since 2026-10-31 an item of a
  non-sharing category (start, flag, turret, mover, marker) stays visible, so
  a pickup can no longer be dropped onto a start or flag cell (it could
  before); in `sub_00303460` such an item is replaced as stock. `sub_00308260` itself always runs (a pure list walk), so the
  log line `stack: item added to an occupied cell` now appears only when a
  real item was in the cell (it used to print on every placement).
- Red X / refusal (fixed 2026-10-31): the per-frame "placement valid" bit
  (editor `+0x25C` value 4, cleared at 0x00306EA1) has a second occupancy test
  in `sub_00306460`. With an item held, `sub_003348C0` (-> `sub_00334770`) is
  called at 0x003070A2; when it passes, 0x003070B4 still requires no item
  under the cursor (`[editor+0x2F1C]`, from `sub_002F82E0` -> `sub_00308260`
  at 0x00306F9D) before setting the bit at 0x00307168; otherwise it goes to
  0x003070C2 -> 0x0030713A, which only sets it for category 0x22, so the bit
  stays clear: red X, and the button (`sub_00304C30`) refuses with text
  0x1576 at 0x00304E0D..0x00304E3E. `pc_mapmaker_stack_valid` (on
  `sub_003348C0`, return 0x003070A7 only) sets the bit after a pass when an
  item is under the cursor and the held item may stack; the button then takes
  the stock drop path (0x00304CD5 -> `sub_00334770` at 0x00304D0B ->
  `sub_00303460`). `+0x2F1C` is left as it is (HUD text and highlight).
- Story AI (2026-10-31). No catalogue category: a Story AI is an entry of the
  editor array (relocated by `editor.json` to editor+0x11950, 0xE4 bytes, count
  editor+0x280, point count +0x58, up to 8 points `{piece, x, y, z}` x 0x10 from
  +0x64). Its points are placed in Story AI mode (`+0x254` = 3; button handler
  `sub_003051F0`); held = signed word editor+0x2F10 >= 0 (+0x2F12 = point
  index; picking a point up removes it from the array, 0x003056EF..0x00305735).
  Two stock checks refuse a shared cell: (1) `sub_00334770`, also used for
  items, refuses an item of the piece in the cell (0x0033482A..0x00334846)
  and any Story AI point in the cell (0x0033484F..0x00334893); (2) in Story AI
  mode `sub_00306460` stores `sub_002F8B40` (point under the cursor, (sai << 16)
  | point or -1, call 0x0030723A) in `[editor+0x2F20]`; the valid bit is set at
  0x003072BE only when `sub_003348C0` passed (0x003072A2) and +0x2F20 < 0
  (0x003072B4), and the drop needs +0x2F20 < 0 as well (0x00305238).
  `pc_mapmaker_stack_cell` now also hides the Story AI points in the cell
  (x word = 0x7FFF) except the held AI's own points, whether an allowed item
  or a Story AI is held; `pc_mapmaker_stack_sai_point` (on `sub_002F8B40`,
  return 0x0030723F only) answers -1 while a Story AI is held. With empty
  hands (no item and no Story AI held) it answers the HIGHEST-index AI with a
  point in the cell (its arguments piece/x/y/z are read at entry; the
  function is cdecl, caller `add esp,0xC` at 0x0030724B), so pick-up takes a
  shared cell apart newest first: on maps saved while linked copies were
  born on their leader's points (fixed 2026-10-31, mapmaker-aitemplates.md)
  the copy comes off before the leader. Logged the first 3 times
  (`[mapmaker] stack: pick-up takes the newest story AI on a shared cell`).
  With an item in hand the stock lowest-index answer stays. No other Story AI limit applies per cell (the add check
  `sub_0031BE70` counts AIs, not cells). A Story AI's own points still may not
  share a cell. Points beyond 8 in an entry (only from damaged files) are left
  to the original, which then refuses.
- Teleporters (categories 14-18, one per colour, ids 0x0A..0x0E) now share
  like pickups. Each colour's cap (2 stock, 8 with the limits) still applies,
  so two of one colour stack only while the pair is incomplete.
- Button meaning: holding an item the button always drops; it picks up the
  item under the cursor only with empty hands (0x00304D7D, the first item of
  the cell, so repeated pick-ups peel the stack oldest first).
- Play time: each stacked item is its own record and spawns (the fill pass
  walks every record, items.md section 2.1). They share the cell's pad
  (`sub_00331E50` picks it from piece and cell), and the pickup record stores
  only the pad (`0x002413E8 mov [edi+0x14],ebp`), so they spawn on one spot;
  no offset is applied (that needs a different pad per item at save time or a
  play-time position hook; not done). Pickups are separate objects, so one
  does not block collecting the other (INFERRED, not tested). The fill
  handler's pad writes (`pad+4 |= 0x4000`, `pad+0x18`) repeat per item; the
  last item wins `+0x18` (INFERRED harmless for pickups).
- Play time, Story AI: AIs on one cell spawn on the same pad (the same as
  Duplicate Story AI, user-tested), and next to any pickup there.
- Play time, teleporters (CONFIRMED code, behaviour not tested in game).
  Destination: `sub_000C0540` sends the toucher to the first other teleporter
  object of the same colour in pool order (0x000C05A1..0x000C05E2, the touched
  object itself excluded at 0x000C05B6), at that object's pad. A per-character
  cooldown `[char+0xB40]` must be <= 0 (0x000C0572) and is set to 2.0
  (`[0x475D10]`) on every teleport (0x000C0673), counting down per frame
  (0x000D8EC9; the rate is a .data value, INFERRED frame time). So: a pair stacked on one cell teleports you to where you
  stand (a no-op with the effect); two colours on one cell, or a teleporter
  on the arrival cell of another colour, send you on only after the cooldown
  and only if you are still on the pad, exactly as standing on the stock
  arrival teleporter does. Nothing loops faster than stock, and a second
  teleporter touched in the same frame is skipped by the cooldown. AI only
  teleport when their route's next pad is the destination (0x000C0615). The
  arrival facing is your yaw minus the source pad's `+0x18` plus the
  destination's (0x000C0731, 0x000C078C); teleporters sharing a pad share
  that angle (the last loaded wins, fill 0x0024140D), so a rotated teleporter
  on a shared pad can turn you differently than its arrow shows (cosmetic).
  Nav pairing `sub_0009CD00` links each teleporter to the next of its colour;
  a stacked pair would get a pad -> pad self link from `sub_0009A5B0`. The
  play-time hook `pc_mapmaker_objects_teleporters` (`src/interface/pc_mapmaker_objects.inc`,
  scene 100, always on) now skips that link (logged `teleporter pair on one
  pad`); the chain continues with the next teleporter.

## Save and story compatibility

No format change: absolute signed-byte cells, rebuilt into the grid at load.
Old maps load identically (they sit in 0..39, levels 0..5; the fixture loads
a 7-floor map into the 16-floor grid and round-trips a map on floors -2..13
through the record bytes). The floor byte `+6` is signed (`movsx`), so the
format itself would allow floors up to 127; 13 is this build's editor limit
(see `docs/agent-context/limitless-floors.md`). A map that fits
the stock limits (x/y < 40, floors <= 2 plus 2-tall pieces, one item per
cell) is a stock map. Pieces at x/y >= 40 or z >= 4 work in play but are
outside the stock editor's grid (cannot be picked there); in this build with
the limits off the same is true above level 3. Stacked items load
in the stock game too (the loader does not dedupe); the stock editor replaces
the oldest when a new item is placed there. Re-saving an old map that used the
7th-floor glitch recomputes connections for its high pieces (they now connect).
All hooks are editor functions or gated on their call site / scene 102; play
and story levels are untouched.

## WRAP lines

```
WRAP sub_00307CC0 -> pc_mapmaker_grid_alloc
WRAP sub_00307C20 -> pc_mapmaker_grid_reset
WRAP sub_00307CF0 -> pc_mapmaker_grid_insert
WRAP sub_00307DA0 -> pc_mapmaker_grid_remove
WRAP sub_00307E60 -> pc_mapmaker_grid_lookup
WRAP sub_002F8DE0 -> pc_mapmaker_grid_placement
WRAP sub_002F79E0 -> pc_mapmaker_grid_floor_sound
WRAP sub_0001ED50 -> pc_mapmaker_grid_floor_bar
WRAP sub_0032EDA0 -> pc_mapmaker_grid_preview_floor
WRAP sub_002F8140 -> pc_mapmaker_overlap_cell
WRAP sub_00308260 -> pc_mapmaker_stack_find
WRAP sub_00334770 -> pc_mapmaker_stack_cell
WRAP sub_003348C0 -> pc_mapmaker_stack_valid
WRAP sub_002F8B40 -> pc_mapmaker_stack_sai_point
```

`sub_0001ED50` is a 5-byte `jmp` thunk to `sub_0001C720` called from 353
sites; the hook costs one stack compare when not in the floor menu.
`#include "pc_mapmaker_grid.inc"` after `pc_mapmaker_limits.inc`.

## Risks

- Floor menu: the 16-bar panel grows downward from the stock top (bars are
  bottom-up from base + 16 steps, to about 60% of the screen height); check
  it covers nothing on the right-hand side (7 bars were checked in game).
- Stock mode draws its 5 bars through the loop-count write; the off-screen
  fallback only runs if that slot is unexpected.
- 80x80 play: longer sightlines, AI pathing over distance, fog; rooms stay
  capped at 241 pieces.
- Overlap maps look broken by design; starts/flags/turrets/movers never stack.
- Stacked teleporters and Story AI are not tested in game (see play time above).
