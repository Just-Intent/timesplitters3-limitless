# MapMaker real story boss: the split Crowe (story mission 10)

Status: implemented in `src/interface/pc_mapmaker_crowe.inc` (area `crowe`),
fixture-tested (`tests/mapmaker/mapmaker_crowe.c` +
`tests/mapmaker/test_mapmaker_crowe.py`: 80 mutations, plus decode, US XBE data
and pak-index checks of every site below). **Not wired, not built, not
played**: the wrappers, the picker row and the stubs are the integrator's
([Wrappers for the integrator](#wrappers-for-the-integrator)). Switch:
`TSFP_MAPMAKER_LIMITS=1`; `TSFP_MAPMAKER_REAL_BOSS=0` keeps the bot AI.

Research basis: `logs/limits/features/real-bosses.md` section 6 (main
checkout, ignored). CONFIRMED = seen in the decode, the US XBE data or the pak
indexes (the test names the site); INFERRED = reasoning, needs a playtest.

## The boss

| | value (CONFIRMED) |
|---|---|
| character id | 0x46: character table `0x0053A3CC + 0x46*0x30` = voice set 0x00539948, model 0x5E8, no LOD; no Arcade row |
| model | 0x5E8 `CHR_FARFUTURE2_SPLITTERCROW` `chrs/splittercrow` in `pak/chr.pak` (mounted in scene 100), type 8, skeleton group 0x4D, story-only flag 0x2000000 (skipped in scene 100, `sub_000A5E40`) |
| behaviour | class 0x40000 (record +4, +0x10 = 0): `sub_0027D590` 0x0027DDE5 -> `sub_0028FE30(entity)` (call 0x0027DE39, its only caller). 0x80000 is the giant robot `sub_00293730` |
| messages | `sub_0027CEF0` 0x0027D36B -> `sub_00292430` (0x0027D3E9): message 2 (death) in states > 2 plays `splitcrowedeath` 0x815, sets state 0x11 and returns -1, in every scene. No message hook |
| animations | `pak/stream/lv14/anim/lv14.pak` (string 0x0049DC78, 258 KB, registered at 0x001CA316 from table 0x0047E938, 41 names): `splitcrowewalk` 0x80E .. `splitcrowedeath` 0x815; no id shared with lv1_3, lv1_6 or lv81 |
| per-level state | `sub_0025F720` scene 14 only (case 0x0025F760): `sub_0028D6D0` = `sub_002A8300` (effect blocks `[0x0078B090]`/`[0x0078B098]`), `[0x0075F5E8]` = `sub_0003DFA0(0x2E04, 1)`, tail `sub_0028D0C0` (every field and the arena grid); then the robot's `sub_00292630` and the particle block `sub_002A62C0` (`[0x0078B0D0]`, 0x7D00) |
| bar | `sub_0014CC80` case 0x5E8: label 0x163C in scene 14, else 0x163B; focus set by the fight (0x00290F61) |
| overhead meter record | `sub_0012C800` at 0x00290F67, stored at entity +0x268 (0x00290F6E) |
| health | entity +0x1CC = the sum of its parts' health (+0x2C2C, +0x2C88, +0x2CE4, +0x1D84, +0x1D88, +0x1DC4), rewritten every frame while parts live (0x00290C1D..); max +0x1D8 = 110 (0x00290F53) |

Shared state (CONFIRMED, decode): `[0x0075F5E8]` is read only by the Crowe
functions 0x0028D0C0..0x00292430; the giant robot `sub_00293730` reads
`[0x0078B1DC]` (`sub_00292630`) and `[0x0075F5F0]`/`[0x0075F5F4]`, never
`[0x0075F5E8]` (the research note's "the robot's `sub_0028D6D0`" is wrong; a
note was left for the robot session). Truly shared with other bosses:
`[0x0078B0D0]` (particles: giant skeleton, Crow walker), the effect system
`[0x0078B080..0x0078B098]` (`sub_002A8300`/`sub_002A8380`; the Crow walker's
`sub_002976F0` also calls `sub_002A8300`), and `lv14.pak` (the giant robot's
`giantrob_*` anims are in it too).

## Why it needs more than a class

Checked as the brief asked (CONFIRMED unless marked):

- **Scene tests**: none in the Crowe's own functions (0x0028D0C0..0x00292630,
  test). The frame draw is gated (below).
- **Tag lookups**: `sub_0023FF20` is not reachable from the fight, its
  messages, its draw or its callbacks (`sub_0028DA10`, `sub_0028F730`,
  `sub_0028E6F0`, `sub_0028E700`), and neither is `sub_002274C0`.
- **Arena coordinates**: the fight walks on a grid built at fixed world
  coordinates. `sub_0028D0C0` calls `sub_002A93D0(state+0x970, +0xEE8,
  +0xF10, +0xF60, +0x1190, +0x1198, 283.45, 61.2)`: 5 x 7 cells of 0x28 bytes
  at +0x970 (+0 x0, +4 z0, +8 x1, +0xC z1, +0x10/+0x14 centre, +0x18/+0x1C
  half size, +0x20 type: 0 platform, 1/2/3 bridges), the outer rectangle
  +0xEE8 and 14 edge rectangles +0xF60, spanning x 283.45..326.71, z
  61.2..100.7 (about 43 x 40 world units; INFERRED from the constants: 9.2/7.6
  wide platforms and 3.22 wide bridges per row). Every frame
  `sub_002A9D00` -> `sub_002A9330` finds the player's (+0x1190) and the
  boss's (+0x1198) cell; state 0 puts the boss on its cell's centre
  (0x00290E81/0x00290EAF) and every move walks to a cell centre
  (`sub_0005BA40` with the x/z distance, 0x00291B78).
- **Start cell** (CONFIRMED, decode; test `test_start_cell`): state 0 first
  writes 310.0 (`[0x0049F3D8]`, 0x00290DC2) to the boss's x (`ebp` =
  entity+0x64, 0x00290022), then `sub_002A9D00(state+4, player x/z, 310.0,
  own z)` (0x00290E62) looks up the cell of (310.0, own z) into
  +0x1198/+0x119C, and the boss goes to that cell's centre. So the stock
  start is always in the 310.0 column, in the row of the boss's own z.
  `sub_002A9330` picks the row by z (each row's first cell, +4 < z < +0xC)
  and then the column by x (+0 < x < +8), writing nothing when either is
  missing; the build `sub_002A93D0` leaves -1 there (0x002A9400), so a miss
  reads cell -8 (state+0x830) as the position.
- **Floor height**: mission 10's floor is y = 86. Hard-coded: the death
  point (310.49, 86, 80.95) written to +0x64/+0x68/+0x6C (0x002922FB..
  0x0029231D); the eight falling hazards at state +0x11A0 (0x168 each,
  spawned by `sub_002AB1B0` at y 86 (0x002AB39A) from state 10, 0x00291E7F;
  moved by `sub_002AA6A0`, which compares 88.4/88.25/86 and below 86.205
  explodes them with `sub_000EFBD0` 0x22/0x20/0x1B at their position); the
  debris floor of `sub_002AD3D0` (86); the spark floor `sub_002A85C0(86.4)`
  (push at 0x00290BF8). So the hazards would fall and explode at y 86
  whatever the MapMaker floor is: gameplay, not just looks. 0x0049F3C4
  (86.0) is also read by `sub_0028DD50` as a squared-distance threshold
  (0x0028DD83) and twice by the Crow walker `sub_002998E0` (outside the
  fight); the other shifted constants have one reader each (test).
  The fight never writes the height otherwise (no forced floor like the
  skeleton's 0x0029D06B).
- **Level-only objects**: a shot-off turret (`sub_0028E770(part, angle)`,
  from 0x0028ECEC) creates prop 0x4BE + part (`FARFUTURE2_SPLITTERCROWBACK/
  RIGHT/LEFTTURRET` 0x4BE..0x4C0), whose model files are only in
  `story/l_14_st.pak` (pak index, test; not in chr.pak, gun.pak or l_100).
  The created object is only stored (+0x2C30 + part*0x5C, never read). The
  texture 0x303 (`sub_00080F20`) is a global one (used by generic code).
- **Script dependence**: state 1 waits for player 0 in the scripted mode
  `+0x114 == 0x100` (0x00290FE5), which `sub_000D4B00` sets for the mission's
  intro cutscene; a MapMaker map never plays it, so stock would idle forever.
  State 2 then starts the fight when the player leaves that mode
  (0x002911B5: parts 30/20/20/20/20, state 4) or after 14 s of it (tantrum
  0x811, state 3). After the death (state 0x11: the animation, then 3 s) it
  calls `sub_002275F0(0)` (0x002923A3: level marker 0 to state 5), clears
  the focus and stays in state 0x12, which no case handles (jump table
  0x002923DC has 18 states): the level script ends mission 10. Every fight
  call still updates the sparks, hazards and particles before its state
  switch (`sub_002A7D60` at 0x00290D3E ages the particles, recounts the live
  ones in `[0x0078B0CC]` and emits smoke at every destroyed part while the
  parts mask +0x2E00 is set; the death leaves it at 0x2F), so in mission 10
  the wreck smokes on until the mission ends (test `test_death_effects`). It also sets
  `[0x00739A90]` (`sub_000DCB60`: 1 when it finds no path, state 16; 0 at
  the death), a global flag that adds 0x20 to some collision tests
  (`sub_000DCB80`, `sub_000E97A0`).
- **Frame draw**: `sub_001CEBC0` calls the Crowe's draw `sub_0028D710` (no
  args; returns at once when `[0x0075F5E8]` or its +0 is 0) only in scene 14
  (0x001CEE49 .. 0x001CEE53).

Not impractical: everything the fight reads is in its own state block or in
image constants that only it reads, so it can be moved.

## What the area does (scene 100 story, `TSFP_MAPMAKER_LIMITS=1`)

| WRAP | what it does |
|---|---|
| `sub_001C8540` -> `pc_mapmaker_crowe_anims` | after the original: when the map's custom set (index 0x11, slots 1-20, as realbosses reads it) holds 0x46 (or the container is unreadable) and lv14.pak is not mounted, mount it with `sub_0005A130` when >= 2 of the 30 mount slots are free |
| `sub_0025F720` -> `pc_mapmaker_crowe_level` | after the original: forget the last level; when the set holds 0x46, clear `[0x0075F5E8]`, call the stock `sub_0028D6D0` (0x2E04 bytes of level heap plus the effect blocks), then `sub_002A62C0` for the particle block (a block another boss allocated this level stays if this one fails; otherwise the new one replaces it, which leaks 32 KB of level heap and keeps every boss working: they read the global). A failed allocation logs and keeps the bot AI |
| `sub_0015B830` -> `pc_mapmaker_crowe_ai_start` | character 0x46 or model 0x5E8: class 0/0x40000 when lv14.pak is mounted and this level's state exists; one per map (extras keep the bot AI); notes the Story AI's Boss flag (chunk +0x29) for the HUD |
| `sub_0028FE30` -> `pc_mapmaker_crowe_fight` | for this boss in scene 100: **relocate** on its first frame (state 0): the start cell (the 310.0 column's cell, a type-0 platform first, nearest the outer centre in z, whose centre the stock lookup maps back to it; with no such cell, the type-0 cell nearest the outer centre) is centred on the spawn x/z, every cell, the outer and the used edge rectangles (+0/+4/+8/+0xC/+0x10/+0x14) move by the same (dx, dz), sizes and types unchanged; dy = spawn height - 86. **Intro**: state 1 becomes 2 (+0x9D4 1, +0x20 2, +0x9D0 2, +0x9D8 0). **Floor**: during the original only, the image constants 86.0 (0x0049F3C4), 86.205, 88.25, 88.4 get + dy, 310.49 and 310.0 + dx, 80.95 + dz, and the stock bits are written back after (a constant that is not stock is left alone). **Start**: after that first state-0 frame, if the boss is not on the spawn x/z or +0x1198/+0x119C are not the start cell (only possible when no cell holds 310.0), both are set; the boss starts exactly at its Story AI spawn point. **End**: in state 0x12 the original still runs (shifted constants), with +0x2E00 = 0 for the call so no new smoke is emitted; the sparks, hazards and particles play out, and once no particle is live (`[0x0078B0CC]` after the call) or after 300 calls the boss is removed with `sub_0015DDA0` (focus cleared), once |
| `sub_0028DD50` -> `pc_mapmaker_crowe_near` | return 0x00290BE7 while shifted: 86.0 restored inside (a distance there), shifted again after |
| `sub_002A85C0` -> `pc_mapmaker_crowe_sparks` | return 0x00290C02 while shifted: the pushed floor 86.4 + dy, the caller's slot restored |
| `sub_0028E770` -> `pc_mapmaker_crowe_part` | return 0x0028ECF1, scene 100: only the stock explosion `sub_000EFBD0(0x20, 0x17, state+0x2BFC+part*0x5C, 0, 0, 0, 0, 1)` for parts 0..2, no prop; the original is not run |
| `sub_002275F0` -> `pc_mapmaker_crowe_event` | return 0x002923A8: level marker 0 is the map's own logic marker: dropped |
| `sub_00126560` -> `pc_mapmaker_crowe_fx` | return 0x001CEE8C: after the original, `sub_0028D710` with this level's state, until the boss is removed (then nothing it draws can stay frozen) |
| `sub_0014CC80` -> `pc_mapmaker_crowe_hud` | focus on this boss: cleared for the stock bar, restored after (Boss Off = no health; Boss bar = the bosses area's; Sub-boss = the meter) |
| `sub_0013A400` -> `pc_mapmaker_crowe_meter` | return 0x0014D264, this boss's record: the overhead meter hidden unless its Story AI chose the Sub-boss bar |

Logs (stderr, `[mapmaker] crowe:`): `mounted lv14.pak for the split
Crowe`, `split Crowe state allocated`, `split Crowe gets its story AI (class
0x40000)`, `arena moved by (dx, dz), floor y (mission 10: 86), start cell row,col at
the spawn (x, z)`, `split Crowe start put back on the spawn point` (should
not appear), `no intro
cutscene; the fight starts`, `split Crowe effects drawn`, `part destroyed:
explosion only`, `level marker dropped`, `split Crowe defeated: its effects
play out`, `split Crowe defeated: removed (frames, live particles)`,
`overhead health meter hidden`.

## Risks (INFERRED, need a playtest)

- **Space**: the arena is about 43 x 40 world units around the spawn point.
  The boss starts on its spawn point, but the start platform is in column 4
  of 7, as in mission 10: the lattice reaches about 27 units towards -x and
  17 towards +x of the spawn, and about 20 each way in z (INFERRED from the
  grid constants). The boss walks its lattice of
  platforms and bridges straight through whatever the map has there; it has
  no wall test of its own. Build an open floor (e.g. Open air tiles) around
  it. MapMaker's world units per tile were not measured.
- **Uneven floors**: the fight keeps its spawn height; hazards explode at the
  spawn floor. A map whose floor differs inside the arena shows them in the
  air or under the floor.
- **Player off the grid**: as in stock, the boss then targets the nearest
  cell or stops (state 16 sets `[0x00739A90]` = 1 for the rest of the level,
  a collision flag; cleared at its death).
- **Boss multiplier**: health is the sum of the parts, rewritten every frame,
  so the Boss row's x2..x8 does not make it tougher and the bar may read low
  (the bosses area scales +0x1D8). Use x1.
- **Shared blocks**: a Crowe and a Crow walker in one map share the effect
  system (`sub_002A8380` resets it at each boss's start): effects may look
  wrong; a Crowe with the giant skeleton or the walker shares the particle
  emitter. Both work (no crash expected), looks unverified.
- **Turret debris**: a shot-off turret explodes but no turret prop flies off
  (its model is only in `l_14_st.pak`).
- **Constants**: the shift writes the game image's read-only data in guest
  memory for the duration of one call (the lifted code reads it from memory,
  `_simd_b_address = 0x49F3C4` in the generated C). Nothing else runs inside
  that call that reads them except `sub_0028DD50`, handled.
- **Respawn**: a second spawn of the same Story AI in one level keeps the
  first relocation (and stock state 0 then snaps it to that grid).
- **Fixed after a later playtest**: when the boss died and was removed, its
  smoke and explosions stayed on screen frozen. The removal stopped the fight
  call that updates them while the draw went on. Now the fight call keeps
  running after the death without new smoke until the particles are gone,
  then the boss is removed and its draw stops.
- **Fixed after the v0.4.0-mapmaker.21 playtest**: the boss started about
  one platform (about 6 units in x) away from its spawn point, sometimes
  behind a wall. The relocation centred the platform nearest the arena's
  centre (column 3) on the spawn point, but stock state 0 always moves the
  boss to the 310.0 column (column 4). The relocation now centres that start
  cell on the spawn point.
- Untested: collision size on tiles, the walk animation's root motion on a
  flat MapMaker floor, the giant robot and the Crowe together (both use
  lv14.pak; the robot's state is separate).

## Save format and stock games

Unchanged: a stock custom-set member (word id 0x46, sbyte variant -1). No
Story AI chunk byte of its own (+0x29 is the bosses area's Boss flag). A
stock game or an older fork build shows an empty name for the slot and plays
id 0x46 with the MapMaker bot class (`sub_000D6D00` covers 0x16..0xA3 only,
like the others) on its own rig. Mission 10 holds the same id.

## Wrappers for the integrator

New area `crowe` (add it where the areas are listed, e.g. the
`pc_mapmaker_area_enabled` table). Include `pc_mapmaker_crowe.inc` after
`pc_mapmaker_realbosses.inc` (it needs only `pc_mapmaker_limits.inc` and the
includer's guest helpers; its statics are prefixed `crw`/`CRW_`).

```
WRAP sub_001C8540 -> pc_mapmaker_crowe_anims       (layer with realbosses_anims; either order)
WRAP sub_0025F720 -> pc_mapmaker_crowe_level       (layer with realbosses_level; either order)
WRAP sub_0015B830 -> pc_mapmaker_crowe_ai_start    (layer with realbosses_ai_start; either order)
WRAP sub_0028FE30 -> pc_mapmaker_crowe_fight       (new; class 0x40000, per AI frame)
WRAP sub_0028DD50 -> pc_mapmaker_crowe_near        (new; return-address test first)
WRAP sub_002A85C0 -> pc_mapmaker_crowe_sparks      (new; return-address test first)
WRAP sub_0028E770 -> pc_mapmaker_crowe_part        (new; return-address test first)
WRAP sub_002275F0 -> pc_mapmaker_crowe_event       (new; also called by sub_00236070, untouched)
WRAP sub_00126560 -> pc_mapmaker_crowe_fx          (layer with realbosses_fx; per frame)
WRAP sub_0014CC80 -> pc_mapmaker_crowe_hud         (layer INSIDE pc_mapmaker_bosses_hud, with realbosses_hud)
WRAP sub_0013A400 -> pc_mapmaker_crowe_meter       (layer with realbosses_meter; per frame per recorded actor)
```

Each layer: `if(pc_mapmaker_area_enabled("crowe","pc_mapmaker_crowe_<x>"))
pc_mapmaker_crowe_<x>(inner); else inner();`, as the generated
`sub_0014CC80_layer0` does. The crowe hooks act only on their own record, so
layer order does not matter; keep `bosses_hud` outermost on `sub_0014CC80`.
The robot session may wrap `sub_001C8540`/`sub_0025F720`/`sub_0015B830`
too: each hook checks "already mounted" and its own record.

Guest calls (`extern void ...(void)`, called with `guest_call`):
`sub_0005A130(path)`, `sub_00062260(path)`, `sub_0028D6D0()`,
`sub_002A62C0()`, `sub_0028D710()`, `sub_000EFBD0(8 args)`,
`sub_0015DDA0(entity)`. Link stubs to add in
`tests/mapmaker/mapmaker_area_stubs.h` (new): `sub_0028D6D0`,
`sub_002A62C0`, `sub_0028D710`, `sub_000EFBD0`; the others are already there.
Off-build no-op hooks (`{ original(); }`) in `pc_menu_mouse_guest.inc` and
prototypes in `pc_display_menu.h` for the eleven functions above. Owning
agent-context guide: add the area like realbosses (not edited here, per the
brief).

Additions to `pc_mapmaker_realbosses.inc` (editor side; the play side above
needs none of it):

| what | value |
|---|---|
| picker row | `RBS_CROWE_ROW 154`, label `"Split Crowe"` (Latin-1, `xbox_HeapAlloc` like the others), added in `pc_mapmaker_realbosses_picker_end` after the other rows with `sub_00079D60(text, 154, 0x20900, 0, 0x002C1C90, 0)` + `sub_00071C70(sub_00071630(), 0)` |
| pick | row 154 -> slot id 0x46, variant -1 (`sub_002BE270(0x46)` is 0: byte table 0x002BE29C entry 1, test) |
| text | `RBS_CROWE_TEXT 0x52420046` in `row_text` (row 154) and `text` |
| row_of | id 0x46: row 154 for the slot reload / Story AI edit name, the stand-in row for the previews (as for 0x64) |
| preview stand-in | Arcade Old Crow, id 0x1E, row 14 (`CHR_CROW_OLD`, test) in `rbs_preview_row` (highlight and previews) |
| model_request log | add 0x5E8 to the editor log list (optional; the stock call skips the flagged model in 0x66) |
| bigdraw | add `0x5E8` (chrs/splittercrow) to the `pc_mapmaker_bigdraw.inc` model list (466,996 B model, INFERRED tall) |
| hud / meter lists | **do not** add 0x5E8: the crowe area has its own hud and meter hooks keyed on its record (adding it is harmless: realbosses acts only on records it noted) |

## User test steps

1. `TSFP_MAPMAKER_LIMITS=1`. Editor -> custom set: pick **Split Crowe** (row
   after the other bosses; the preview shows Old Crow). Place one Story AI
   with that slot in the middle of a large open, flat area (about 6 x 6
   tiles of open floor or more). Save, reload: kept.
2. Play in Story. stderr: `mounted lv14.pak for the split Crowe`, `split
   Crowe state allocated`, `gets its story AI (class 0x40000)`, `arena moved
   by (...) ... start cell 2,4 at the spawn (x, z)` (row/column may differ), `no intro cutscene; the fight starts`, `effects drawn`.
3. Watch: it appears exactly where its Story AI was placed (not a few
   tiles off), then walks between platforms around its spawn, turns 90 degrees,
   fires; falling hazards drop near you and explode on the floor (not in
   the air). Shoot its turrets: each explodes (`part destroyed`), no debris.
4. Kill it: death animation, then after about 3 s the smoke stops and the
   remaining smoke, sparks and explosions finish within about a second, then
   it disappears with them (`level marker dropped`, `effects play out`,
   `defeated: removed`). Nothing should stay frozen on screen; map logic
   marker 0 does not change.
5. Boss Off: no health shown; Boss bar (x1): one bar with the AI's name;
   Sub-boss bar: the meter. A second Split Crowe AI fights as a bot (`only
   one split Crowe per map`). Mission 10 still plays stock.
6. Report: where it walks relative to tile walls, whether it falls through
   or floats, hazard height, and the stderr lines.
