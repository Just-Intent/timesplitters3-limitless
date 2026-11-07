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
| `sub_0015B830` -> `pc_mapmaker_crowe_ai_start` | character 0x46 or model 0x5E8: class 0/0x40000 when lv14.pak is mounted and this level's state exists; up to 4 per map, each with its own blocks ([Several split Crowes](#several-split-crowes); the fifth and later keep the bot AI); notes the Story AI's Boss flag (chunk +0x29) for the HUD |
| `sub_0028FE30` -> `pc_mapmaker_crowe_fight` | for this boss in scene 100: **relocate** on its first frame (state 0): the start cell (the 310.0 column's cell, a type-0 platform first, nearest the outer centre in z, whose centre the stock lookup maps back to it; with no such cell, the type-0 cell nearest the outer centre) is centred on the spawn x/z, every cell, the outer and the used edge rectangles (+0/+4/+8/+0xC/+0x10/+0x14) move by the same (dx, dz), sizes and types unchanged; dy = spawn height - 86; then each cell's centre is asked the room query (cells in no room at the spawn height are *off*, [Walls](#walls-the-v040-mapmaker34-playtest)). **Intro**: state 1 becomes 2 (+0x9D4 1, +0x20 2, +0x9D0 2, +0x9D8 0). **Floor**: during the original only, the image constants 86.0 (0x0049F3C4), 86.205, 88.25, 88.4 get + dy, 310.49 and 310.0 + dx, 80.95 + dz, and the stock bits are written back after (a constant that is not stock is left alone). **Start**: after that first state-0 frame, if the boss is not on the spawn x/z or +0x1198/+0x119C are not the start cell (only possible when no cell holds 310.0), both are set; the boss starts exactly at its Story AI spawn point. **End**: in state 0x12 the original still runs (shifted constants), with +0x2E00 = 0 for the call so no new smoke is emitted; the sparks, hazards and particles play out, and once no particle is live (`[0x0078B0CC]` after the call) or after 300 calls the boss is removed with `sub_0015DDA0` (focus cleared), once. **Eggs**: each fight call counts its kept eggs down. **Walk**: a walk in state 9 that gets no 0.25 nearer its target (+0x1CEC/+0x1CF0) in 90 calls, or lasts 600, ends where the boss stands as stock ends it on arrival, and the cells ahead in its direction stay blocked for 1800 calls (up to .34: put on the target). **Trace**: each AI state change (40 per boss and level) and every 600 calls (12) log one line |
| `sub_0028DD50` -> `pc_mapmaker_crowe_near` | return 0x00290BE7 while shifted: 86.0 restored inside (a distance there), shifted again after |
| `sub_002A85C0` -> `pc_mapmaker_crowe_sparks` | return 0x00290C02 while shifted: the pushed floor 86.4 + dy, the caller's slot restored |
| `sub_0028E770` -> `pc_mapmaker_crowe_part` | return 0x0028ECF1, scene 100: only the stock explosion `sub_000EFBD0(0x20, 0x17, state+0x2BFC+part*0x5C, 0, 0, 0, 0, 1)` for parts 0..2, no prop; the original is not run |
| `sub_002275F0` -> `pc_mapmaker_crowe_event` | return 0x002923A8: level marker 0 is the map's own logic marker: dropped |
| `sub_002AB1B0` -> `pc_mapmaker_crowe_eggs` | return 0x00291E84 (its only call, 0x00291E7F), inside this boss's fight in scene 100: the egg drop is skipped (cdecl `ret`; the fight pops its six arguments) - see [Eggs](#eggs-the-v040-mapmaker28-crash); the egg's hazard slot (argument 1) keeps its cell (arguments 5, 6) for 120 fight calls, unless that slot's egg is still out - see [No walk](#no-walk-the-v040-mapmaker31-playtest) |
| `sub_001CDD60` -> `pc_mapmaker_crowe_restart` | the pause menu's level restart (no level init): before the original, the outside statics back and `[0x0075F5E8]` 0; after it, in scene 100, every Crowe keeps its Level heap blocks and record and sets them up again in place (no allocation) - see [Restart](#restart-from-the-pause-menu) |
| `sub_002A9D00` -> `pc_mapmaker_crowe_cells` | return 0x002901BF or 0x00290E67 (the fight's cell lookups), argument 1 = this boss's state + 4: after the original (which clears every cell's +0x24 and blocks the eggs in flight), each kept egg blocks its cell and the cells above, below, left and right of it (+0x24 = 1), as a live egg does; the off cells, the cells beyond an off cell in line with the boss and the cells ahead of a stopped walk are blocked too; a player in no cell (arguments 2, 3) gets the nearest cell in a room as +0x1190/+0x1194 |
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
`overhead health meter hidden`, `split Crowe egg drop skipped`; per boss
`split Crowe N fight state A -> B: cell r,c, player cell r,c, target r,c at
(x, z), direction d, beam b, at (x, z), eggs skipped n (m blocking)`, the
same with `still: state` every 600 calls, `walk blocked; ended where it
stands, the cells ahead blocked` / `walk too long; ...`, and `split Crowe N
arena: n of 35 cells in no room at its height, d over a drop or ledge, g gaps
between cells (blocked)`; once per boss
`player outside a split Crowe's arena: its nearest cell stands in` and
(should not appear) `start cell is in no room: arena cells not checked` or
`start cell has no floor at its height: floor probe not used`. The
budget of the other lines (24) is per level.

## Risks (INFERRED, need a playtest)

- **Space**: the arena is about 43 x 40 world units around the spawn point.
  The boss starts on its spawn point, but the start platform is in column 4
  of 7, as in mission 10: the lattice reaches about 27 units towards -x and
  17 towards +x of the spawn, and about 20 each way in z (INFERRED from the
  grid constants). Cells over no room at its height are never targets, and
  a walk a wall stops ends there (since .35, [Walls](#walls-the-v040-mapmaker34-playtest));
  an open floor around it still gives the best fight. MapMaker's world
  units per tile were not measured.
- **Uneven floors**: the fight keeps its spawn height; its sparks and smoke
  use the spawn floor. A map whose floor differs inside the arena shows them
  in the air or under the floor.
- **Player off the grid**: the nearest cell in a room stands in for the
  player (stock kept the last cell the player was in, or -1 when they never
  were). State 16 sets `[0x00739A90]` = 1 for the rest of the level (a
  collision flag; cleared at its death).
- **Boss multiplier**: health is the sum of the parts, rewritten every frame,
  so the Boss row's x2..x8 does not make it tougher and the bar may read low
  (the bosses area scales +0x1D8). Use x1.
- **Shared blocks**: each split Crowe, Crow walker and giant skeleton has
  its own effect and particle blocks, swapped in for its calls through one
  shared holder (`pc_mapmaker_bossfx.inc`), so no boss uses another's.
- **Several Crowes close together**: each arena (about 43 x 40) is laid out
  under its own spawn; arenas that overlap are not a problem for the code
  (each boss walks its own grid), but the bosses walk through each other.
- **Turret debris**: a shot-off turret explodes but no turret prop flies off
  (its model is only in `l_14_st.pak`).
- **Constants**: the shift writes the game image's read-only data in guest
  memory for the duration of one call (the lifted code reads it from memory,
  `_simd_b_address = 0x49F3C4` in the generated C). Nothing else runs inside
  that call that reads them except `sub_0028DD50`, handled.
- **Respawn**: a second spawn of the same Story AI in one level keeps the
  first relocation (and stock state 0 then snaps it to that grid).
- **Fixed after the v0.4.0-mapmaker.34 playtest**: the split Crowes went
  into walls or vanished after the player walked around. The .32 safety net
  put a stuck walk on its target, 14-28 units away, in walls or off the
  map. See [Walls](#walls-the-v040-mapmaker34-playtest).
- **Fixed after the v0.4.0-mapmaker.31 playtest** (four of each): the
  Crowes fired but never walked; the skipped egg no longer blocked the cell
  it was dropped on, so the walk picked that cell again and again. See
  [No walk](#no-walk-the-v040-mapmaker31-playtest).
- **Fixed after the v0.4.0-mapmaker.28 playtest** (four split Crowes and
  four Crow walkers): the game crashed once the fights started. The Crowe's
  falling eggs could not be made in MapMaker and their landing corrupted the
  stack. The eggs are now skipped; see [Eggs](#eggs-the-v040-mapmaker28-crash).
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

## Several split Crowes

User request: every placed split Crowe should fight with its story
behaviour, not just the first.

**What one Crowe's code keeps globally** (CONFIRMED; tests
`test_multi_instance_globals`, the fixture's `multi()`):
- `[0x0075F5E8]`: its 0x2E04-byte state, with the arena grid, the parts and
  the hazards. Every reader (17 functions) is below the fight
  `sub_0028FE30`, the message handler `sub_00292430`, the think callback
  `sub_0028DA10` (slot 2, set at 0x00290EF7), the hit callback `sub_0028F730`
  (+0x19C, set at 0x00290EEC), the draw `sub_0028D710` or the allocation
  `sub_0028D6D0` (direct-call graph, test).
- The effect system `0x0078B080..0x0078B098` (`sub_002A8300/8380/85C0/8FC0`):
  two allocated blocks and their counters.
- The particle system `0x0078B0CC..0x0078B188` (`sub_002A62C0/62F0/7D60/7F30`):
  the block, the live count and the emitter templates.
- These 29 words are all (the 28 statics lie inside the boss statics
  `0x0078B080..0x0078B1DB` that the realbosses area swaps for the walker and
  the skeleton); the test checks them against every absolute
  operand the code reaches. `[0x0078B1E4]` is only a temporary of one fight
  call (written at 0x00291AF6, read at 0x00291B61). The other boss
  singletons the graph reaches (`[0x0078B1DC]` robot, `[0x0078B1E8]` walker,
  `[0x0078B1EC]` skeleton) go through generic code and are not the Crowe's.
- Not per boss: the player's HUD focus (+0xFF4, the last boss to start
  wins) and the image constants the area shifts during each call (shifted
  per call by that boss's own offsets).

**What the area does**:
- Up to 4 bosses (`CRW_MAX`), each with its own state pointer
  (`crw_inst[k].state`) and its own copy of the whole boss statics window
  (`crw_inst[k].fx`, a `MapmakerFx` of `pc_mapmaker_bossfx.inc`).
- Allocation: boss 0 at level init as before, each further one at its AI
  start (the stock `sub_0028D6D0` and `sub_002A62C0`, with the globals as
  they were put back afterwards).
- `crw_enter(k)` / `crw_leave(prev)` swap the copies around every entry into
  the boss's code: the fight, the message handler, the two callbacks, and
  each boss's draw (the fx hook draws them one by one).
  - The statics go through the one holder shared with the realbosses area
    (`mapmaker_fx_enter`): it parks the live window in its current holder's
    copy (a Crowe, a walker, a skeleton, or the outside copy) and loads the
    new one; leaving enters the previous holder again.
  - So any nesting is safe: another boss inside (one Crowe's hazard hits
    another), or this boss again inside another area's boss inside its own
    call (its hit callback inside a Crow walker's damage inside its fight)
    gets its own latest values, and each caller gets its own back.
    Up to v0.4.0-mapmaker.28 each area swapped its part of the window on its
    own and "the same boss again" changed nothing, so that re-entry ran on
    the walker's particle and effect blocks (`test_mapmaker_bossnest.py`).
  - The values that are live outside every boss call (the stock singleton)
    are put back after each call; the state pointer `[0x0075F5E8]` stays the
    Crowe area's own (only `sub_0028D6D0` writes it).
- Everything per boss: relocation, the floor offsets, the death fade and
  removal, the Boss flag (meter), the mission 10 constants' shift.
- The fifth and later split Crowes keep the bot AI (logged:
  `more than 4 split Crowes in the map`).
- Mission 10 is untouched (every hook acts in scene 100 only).
- Memory: about 45 KB of level heap per extra boss (state 0x2E04,
  particles 0x7D00, effect blocks).

**Would it carry over to the other one-per-map bosses?**
Not verified beyond what is listed. The same method applies where a boss's
state is a small set of globals reached only through hookable entry points:
- Giant skeleton (`[0x0078B1EC]`, `pc_mapmaker_realbosses.inc`): its fight
  uses the shared particle system too. Likely the same approach: its
  state pointer plus the same particle window, swapped around its fight,
  message and callbacks.
- Crow walker (`[0x0078B1E8]`): uses the same effect and particle systems as
  the Crowe. Likely the same.
- Giant robot (`[0x0078B1DC]`, plus `[0x0075F5F0]`/`[0x0075F5F4]` floats):
  the extra globals need the same inventory first.
- Castle tank and Khallos: their state is in props or static data of their
  own; each needs its own inventory before anyone can say.
- Mansion worm: it is already a five-worm system in one block. More than one
  map worm can use the stock worms 1-4, each with its own head, instead of
  more blocks (handled in the worm area).



## Eggs (the v0.4.0-mapmaker.28 crash)

User report: four split Crowes and four Crow walkers in one map; the game
crashed after the fights started (crash-20261102-024139391, exe SHA
7dd66f38...).

**What happened** (CONFIRMED from the dump, the log and the decode; tests
`test_egg_drop_site`, `test_models_in_paks`):
- The crash: `sub_0009A570` got a garbage pad index from the loop at
  0x0015F0A3 in the per-character update `sub_0015EAE0`. Its esp was 0x20
  too high: the stack still held the return 0x0015F083 of
  `sub_0027FE30` at the balanced depth, so that call (the story AI
  dispatcher, reaching the Crowe fight at 0x0027DE39) returned 0x20 high.
- The log's last line before the crash: `[ICALL] target 0x8147C368 is not
  code -- skipped`, callers 0x000A907F <- 0x002AAEAA <- 0x0028F58C (the
  egg landing loop in `sub_002AA6A0`, called by the Crowe's `sub_0028F440`
  hazard update).
- Each egg (8 per boss, state +0x11A0, 0x168 each) is four props 0x4BD
  (`farfuture2/splittercrowegg`), made by `sub_002AB1B0` (only call
  0x00291E7F) with `sub_000A6F90`. Its model is only in
  `story/l_14_st.pak` (14 MB, never mounted in MapMaker), so each
  `sub_000A6F90` returns 0 (`sub_000A6880` frees the slot when the model
  instance fails) and the egg keeps four 0 handles at +0x150..+0x15C.
- On landing (0x002AAEA2..0x002AAEB7) all four are released with
  `sub_000A9070` and no null test (the death's `sub_002AA2E0` has one).
  `sub_0009E330(0, 1, 0)` reads class 0 at guest 0x20, indexes 5 entries
  before its callback table 0x0072F2E0 and calls that word (a heap
  pointer).
- The runtime skips a call to a non-code target, but `RECOMP_ICALL_SAFE`
  restores the esp from before the call's two arguments, and the caller's
  `add esp, 8` then pops them again: 8 bytes high per skip, 0x20 for one
  egg. The esp skew went up through the fight and the dispatcher into the
  per-character update. (A runtime defect for any skipped call with
  arguments; not changed here.)
- Not the cause: every Crowe and realbosses hook was audited for esp on
  each path (each runs its original once or does one `ret`); guest_call
  saves and restores esp.

**Fix**: `pc_mapmaker_crowe_eggs` skips the egg drop for a boss in scene 100
(no egg, no null prop, nothing lands). The fight's other attacks (turrets,
sparks) and its egg counter are unchanged. Mission 10 is untouched.

Unchanged: a stock custom-set member (word id 0x46, sbyte variant -1). No
Story AI chunk byte of its own (+0x29 is the bosses area's Boss flag). A
stock game or an older fork build shows an empty name for the slot and plays
id 0x46 with the MapMaker bot class (`sub_000D6D00` covers 0x16..0xA3 only,
like the others) on its own rig. Mission 10 holds the same id.

## No walk (the v0.4.0-mapmaker.31 playtest)

User report: four Crow walkers and four split Crowes; "the split crows
attacked but didn't move. After moving them around, one of the split crows
moved, but the others still didn't." No crash. A single Crowe walked in
earlier builds, before the egg skip. Log `m31.err`: each Crowe's arena moved
under its spawn (start cell 2,4), the intro skipped, then one `egg drop
skipped` per Crowe right after the fight began, and nothing else (the 24-line
log budget was spent in the first level, so no later line).

**Cause** (CONFIRMED from the decode, test `test_egg_steers_the_walk`; the
playtest is consistent, no state was logged):
- State 4 decides; state 5 (0x00291560) calls `sub_002A9E60(grid, player
  cell, boss cell, target x/z, target cell, &[0x0078B1E4], 0)`. With the
  last argument 0 it takes the first cell, in a fixed order around the
  player's cell, that is in line with the boss, not next to it, not a gap
  (+0x20 = 1) and not blocked (+0x24 = 1). No walk there (-1) goes to state
  10 (0x00291D91), which drops an egg on that target cell (0x00291E7F) and
  goes back to state 4 (0x00291EB1) either way.
- `sub_002A9D00`, the fight's cell lookups at the start of every call
  (0x002901BA), clears every +0x24 and then blocks each egg in flight
  (hazard +0xF8 set): its cell (+0xEC row, +0xF0 col) and the cells above,
  below, left and right (0x002A9D88..0x002A9DE5). The egg itself never
  touches the fight's state (no `[0x0075F5E8]` in `sub_002AB1B0`/
  `sub_002AA6A0`/`sub_002AA2E0`).
- So in stock the egg's cells are what makes the next state 5 pick another
  cell. With the egg skipped (.30) nothing was blocked: the same cell, -1,
  state 10, state 4, again for as long as the player stood in the same cell.
  Moving changed the order's first cell; that is the one that walked.
- Rejected: the shared statics (the walk reads only the boss's state block;
  `[0x0078B1E4]` is written before it is read in one call, 0x00291AF6/
  0x00291B1E then 0x00291B61); the per-boss arena offsets (the grid and the
  targets are the relocated cells; `sub_002A9330`..`sub_002AA1F5` read no
  arena constant); the walkers (nothing of theirs is read); nav/pad data
  (the walk uses only the grid); the room ids (not read).

**Fix**: the skipped egg keeps its hazard slot and its cell for 120 fight
calls (INFERRED: about the egg's fall and landing; not measured);
`pc_mapmaker_crowe_cells` blocks those cells after each lookup as a live egg
would, so state 5 picks the next cell. A slot whose egg is still out drops
nothing, as stock (0x002AB1DA). Each boss keeps its own eggs.

**Also**: state 9 (0x00291B8B) waits, after the walk animation, until the
boss's centre has passed its target (0x00291C1A..0x00291D8C); a walk stopped
short would wait for ever. In .32 to .34, after 240 calls in state 9 the
boss was put on its target cell's centre; that put it into walls (replaced
in .35, see [Walls](#walls-the-v040-mapmaker34-playtest)). Per-boss trace
lines show each state change, the cells, the target, the direction, the
beam (+0x2DDC: state 4 waits while a turret beam is on the player) and the
eggs.

## Walls (the v0.4.0-mapmaker.34 playtest)

User report: "After walking around my map, the splitter crows disappeared
and/or glitched into walls." Log `m34.err`: four Crowes, each arena moved
under its spawn (floor -9.49); 16 lines `walk stopped short; put on its
target`, targets 14-28 units from the boss (e.g. `target 4,0 at (63.00,
89.22)` from (65.46, 77.54)); `player cell -1,-1` on every line; the map has
no open-air tile (`0 bounded, 0 unbounded`); no Crowe died.

**Causes**:
- CONFIRMED (log, code): the .32 safety net wrote the target cell's centre
  into the boss's position. The mission 10 lattice (43 x 40 units) knows
  nothing of the map, so those centres can be inside walls, in another tile
  behind one, or past the map's edge. After the move the walk animation went
  on from there (positions past the target in the next lines).
- CONFIRMED (log, decode): the player was never in any Crowe's grid.
  `sub_002A9330` writes no cell for a point no cell holds (0x002A93AB), so
  +0x1190 kept the build's -1, and state 5's picker took its 5 x 5
  candidates from rows 0-4, columns 0-4 (0x002A9E99..0x002A9EBF) in a fixed
  order: the boss walked to the same corners whatever the player did, and
  state 4 never found the player in line (`sub_002A9E10`).
- INFERRED (not traced in the playtest): why the walks stopped short. The
  walk is the animation's movement through the game's collision, and state
  9 ends when a body point passes the target (+0x1D08/+0x1D10); a wall in
  the way stops the body short (the lines show the boss barely moving, e.g.
  87.03 -> 86.58 during a state 9 of 240 calls).
- "Disappeared", NOT the fall check (CONFIRMED): the roof area's kill runs
  only on a map with an unbounded open-air tile, this map has none, and no
  `roof: character ... fell off the map` line exists. NOT the eggs
  (CONFIRMED): the egg logic only sets cells' +0x24 and never moves or
  removes the boss. The teleport is the likely cause (INFERRED): a boss put
  outside the map stands in the void or falls, and one put inside another
  tile keeps its old room (+0x34: the floor probe and the room draw use it),
  so it can be culled from view or float.

**Fix** (v0.4.0-mapmaker.35):
- **No teleport**: a walk that gets no 0.25 nearer its target in 90 fight
  calls (a wall), or lasts 600 calls, ends where the boss stands, as stock
  ends it on arrival (0x00291C4F..0x00291CA8: `sub_0005F460(entity, 0x810,
  0, 1.2)`, the state the walk was for (+0x2DE8; 4 if that is not a state),
  +0x9D0/+0x9D4/+0x9D8, +0x2DF0 = 0). The position stays the one the
  collision gave it. The cells ahead of the boss in the walk's direction
  (+0x1D24: 0 row+, 1 column-, 2 row-, 3 column+) stay blocked for 1800 fight
  calls (about 30 s at 60 Hz, INFERRED), so the next choice goes another way.
- **The grid follows the map**: after the relocation each cell's centre is
  passed to the room query `sub_00087310(box, out, 8)` with the body box of
  the roof area's fall check (x/z +-0.3, y spawn +0.25..+1.5; on the guest
  stack below esp). A cell in no room at that height (off the map, a void,
  a floor of another height) is *off*: blocked after every lookup, and so is
  every cell beyond an off cell in line with the boss (the walk would cross
  it). The picker (`sub_002A9E60`, +0x24 = 1 skipped) then never picks them;
  with no free cell it returns -2 and state 4 decides again. When the start
  cell itself is in no room the query is not trusted and nothing is off
  (logged). Walls inside a room are not seen by the query (INFERRED: the
  rooms are the tiles' boxes); the walk-stop rule covers those.
- **The player outside the grid**: after each lookup, when no cell holds the
  player's x/z, the nearest cell in a room (any cell when none is) is written
  as the player's cell. The boss turns and walks towards the player's side
  as stock does for a player on the grid. Re-centring the arena on the
  player was rejected: it would carry the boss across the map onto cells
  nobody checked, away from where the mapper placed it, and the off cells
  would have to be found again on every move.

Tests: `walls()` and the 4-Crowe part of `multi()` in
`tests/mapmaker/mapmaker_crowe.c` (cells over no room, a hole with an open
cell beyond, another floor's room, an untrusted start, the player inside an
off cell and outside on each side, each boss its own cells and walls), the
`stop-*`, `wall-*`, `rooms-*`, `shadow-*` and `player-*` mutations, and the
decode test `test_walk_end_and_cells_for_the_map`. Wrappers: none new (the
room query and the walk-to-stand anim are guest calls with existing stubs).

## Ledges (the v0.4.0-mapmaker.40 playtest)

User report: "splitter crow movement logic resulting in them wandering off
of ledges". Logs (`m40a.err`, `m40b.err`): `arena: 5 of 35` / `15 of 35 cells
in no room at its height`, so the room check ran, yet the bosses still
walked off edges.

Cause (CONFIRMED in code): the room query tests a body box at the boss's
height, and a MapMaker room is a tile's box. A cell over a lower floor of
the same tile (a raised platform's edge), or over a tile one floor down
whose box reaches the boss's height, is "in a room" and stays walkable. A
narrow hole between two good cell centres is not seen either.

Fix (`crw_rooms`, no new wrapper):
- **Floor probe**: every cell in a room also asks the game's floor finder
  `sub_000C3C50(point, 1.25, 0.1)` at its centre and the boss's spawn
  height, as the pad snap does (`sub_0009AF90` 0x0009AFC5..0x0009AFD4,
  CONFIRMED: room index in eax, > 0 when a floor is under the point, the
  floor height in `[0x004DCB00]`). No floor, or a floor more than a step
  (1.0) above or below the boss's height, makes the cell *off* (a ledge, a
  drop, a raised block). The finder also writes `[0x004DCAFC]` and
  `[0x00733088]` (its own absolute stores, test); all three are put back
  after the probes.
- **Gaps**: the midpoints between neighbouring good cells (row and column
  neighbours) are probed the same way; a gap there blocks the cells beyond
  it in line with the boss, as an off cell does (`crw_shadow`).
- **Untrusted**: when the start cell itself gets no floor at the boss's
  height, the probe is not used (logged); the room results still count.
- INFERRED: the step of 1.0 is below a MapMaker floor (5.0) and above a
  stair's single step; ramps steeper than 1.0 per cell (about 6 units) block.
  The probe point is the cell centre: a ledge between centre and edge of a
  cell (3.2 units wide) is not seen.

Tests: `ledges()` in `tests/mapmaker/mapmaker_crowe.c` (one room with a
lower floor beyond x 13, a raised block, a small step, a gap between two
cells; the boss on either side of the gap; an untrusted start; a spawn
height a step above its floor; the finder's globals restored), the
`ledge-*` and `gap-*` mutations, and the decode test
`test_floor_probe_like_the_pad_snap`.

### Reference floor (the v0.4.0-mapmaker.42 playtest)

User report: "The splitter crows aren't moving at all now" (they walked in
.34-.40; .41 added the floor probe above). Log `m42.err` (.41 code): every
arena at `floor 0.51` (right), yet `arena: 15 of 35 cells in no room at its
height, 5 over a drop or ledge` (5/15, 7/7), `0 gaps`, the player outside
the arena, and the fight looping 4 -> 5 -> 10 -> 4 (no walk, eggs only).

- SUSPECTED (geometry, not traced in game): tile seams. Every logged spawn
  is at a tile centre (x and z = 3 mod 6, MapMaker tiles 6 wide), so after
  the move every cell's x centre is a tile centre and every x midpoint
  between neighbouring cells lies exactly on a tile seam. A finder miss on
  a seam (its last step `sub_000888A0` places the found point in a room)
  marks a gap on both sides of the start cell, and `crw_shadow` then blocks
  the whole start row.
- SUSPECTED (m42.err): false drops at cell centres. The probe point was
  the spawn height itself, a pad's 0.01 over the floor plane; a point on the
  plane may fall outside its tile's room and get the floor below, or no
  answer. Not traced in game.
- The brief's "floor 10 below the real floor" was checked: in `m34.err`
  -9.49 is the pad snap's height (finder floor + 0.01, 0x0009B006) on
  floor -2 (`roof: ... floors -2..11`, floor y = 5n + 0.5); the Khallos
  chairs of the same map stand at -9.49. So NOT confirmed; still handled.

Fix (`crw_rooms`): the **reference floor** is the finder's floor under the
start cell, `crw_ground`: from the spawn height (within a step), else
climbing one search height (1.25) at a time up to 11.25 for a floor above
the spawn (a spawn under its floor); then the room query also uses it. Each
cell is probed from and compared with that floor. A **gap** is sampled 0.25
either side of the midpoint and counts only when both samples miss. Every
probe point is `CRW_LIFT` (0.5) over the height asked. A cell is a ledge
only when the finder answers with a floor more than a step away; no answer
is not a ledge (counted: `N with no floor answer (not blocked)`), and the
first 6 blocked cells per Crowe log `cell r,c over a floor at F (reference
R): blocked`. No
reference floor: no height check (logged). New line per Crowe:
`split Crowe N reference floor F (spawn height Y, under the spawn | found
above the spawn)` or `reference floor none (...)`. Tests:
`reference_floor()` (spawn -9.49 under a 0.5 floor with a lower one at -20
and a drop: the drop blocks, nothing else; seams missed by the finder: no
gap, nothing blocked; a spawn 0.01 over its floor whose plane gives the
floor below; no answer under a cell: not blocked), mutations `ref-*`,
`gap-at-seam`, `gap-one-sample`, `ledge-no-lift`, `ledge-unseen-blocks`.

## Restart from the pause menu

Found by the Khallos work: the pause menu's restart (`sub_0003D440`, flag
`[0x006B7AA8]`) calls `sub_001CDAF0` and then `sub_001CDD60`, never
`sub_001CDB80`, so `sub_0025F720` (only called from `sub_0015E7C0` at
0x0015E818, which only `sub_001CDB80` calls) and the level hook do not run
(CONFIRMED, test `test_restart_runs_no_level_init`). Stock resets the scene
14 Crowe in place (`sub_0025F6A0` -> `sub_0028D0C0`). Before .31: the restarted
Crowes came back with the same records and kept their old blocks, already
relocated, with the last fight's cells, target and statics (`m31.err`: AI
starts with no new level line and no `state allocated`).

Corrected 2026-11-06 (CONFIRMED, `test_mapmaker_restart_heap.py`): the
restart's `sub_0003E740(3)` frees only heap 0 (`sub_0003E240`: `[0x006B8388]`);
the Crowe blocks (`sub_0028D6D0`: effect blocks and state, `sub_002A62C0`:
particles) are Level heap (heap 1, `[0x006B837C]`), which the restart never
resets. The .31 fix forgot them and gave each Crowe new ones at its AI start:
about 60 KB (100 KB with one player) of Level heap used up per Crowe per
restart. Now `pc_mapmaker_crowe_restart` puts the outside values back before
the original; after it, in scene 100, each Crowe keeps its blocks and record,
forgets the run (relocation, eggs, walls, death) and sets its blocks up again
in place (`crw_again`: effect count/blocks from its copy, every block zeroed,
`sub_0028D0C0`, then its particle block). One whose blocks are not usable,
and those after it, are dropped (bot behaviour). The AI start finds them by
record; they relocate again. Log `level restart: split Crowe blocks set up
again in place`. Fixture: four Crowes, their blocks poisoned, set up again
with no allocation, restarted twice, an unusable effect count and state,
scene 14 and none; mutations `restart-*`, `again-*`. Unplayed.

## Wrappers for the integrator

New area `crowe` (add it where the areas are listed, e.g. the
`pc_mapmaker_area_enabled` table). Include `pc_mapmaker_crowe.inc` after
`pc_mapmaker_realbosses.inc` (it needs only `pc_mapmaker_limits.inc` and the
includer's guest helpers; its statics are prefixed `crw`/`CRW_`). Both
include `pc_mapmaker_bossfx.inc` (guarded; one holder for the boss statics,
prefixed `mapmaker_fx`).

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
WRAP sub_00292430 -> pc_mapmaker_crowe_message     (new; the boss's message handler, from sub_0027CEF0)
WRAP sub_0028DA10 -> pc_mapmaker_crowe_think       (new; entity slot 2 callback)
WRAP sub_0028F730 -> pc_mapmaker_crowe_hit         (new; entity hit callback +0x19C)
WRAP sub_002AB1B0 -> pc_mapmaker_crowe_eggs        (new after v0.4.0-mapmaker.28; return-address test first)
WRAP sub_002A9D00 -> pc_mapmaker_crowe_cells       (new after v0.4.0-mapmaker.31; return-address test first, 2 calls per Crowe frame)
WRAP sub_001CDD60 -> pc_mapmaker_crowe_restart     (new after v0.4.0-mapmaker.31; layer with khallos_restart, either order; around the original)
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
`sub_0015DDA0(entity)`, and since .35 `sub_0005F460(entity, anim, 0, speed)`
and `sub_00087310(box, out, max)` (both already stubbed in
`tests/mapmaker/mapmaker_area_stubs.h` by realbosses and roof). Link stubs to add in
`tests/mapmaker/mapmaker_area_stubs.h` (new): `sub_0028D6D0`,
`sub_002A62C0`, `sub_0028D710`, `sub_000EFBD0`; the others are already there.
Off-build no-op hooks (`{ original(); }`) in `pc_menu_mouse_guest.inc` and
prototypes in `pc_display_menu.h` for the functions above (with
`pc_mapmaker_crowe_eggs`, `_cells` and `_restart`; they make no guest call,
so no new stub). Owning
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
   fires. No eggs fall (`egg drop skipped`: their model is not in
   MapMaker), and it still walks on after one, also while the player
   stands still and with Crow walkers in the map: `split Crowe N fight
   state 5 -> 9` (walk) lines follow `5 -> 10` (egg) lines within a few
   seconds; `still: state` lines every 10 s show where each one waits.
   Shoot its turrets: each explodes (`part destroyed`), no debris.
4. Kill it: death animation, then after about 3 s the smoke stops and the
   remaining smoke, sparks and explosions finish within about a second, then
   it disappears with them (`level marker dropped`, `effects play out`,
   `defeated: removed`). Nothing should stay frozen on screen; map logic
   marker 0 does not change.
5. Boss Off: no health shown; Boss bar (x1): one bar with the AI's name;
   Sub-boss bar: the meter. Up to four Split Crowe AIs each fight with their own arena; a fifth fights as a bot (`more than 4
   split Crowes in the map`). Mission 10 still plays stock.
6. Report: where it walks relative to tile walls, whether it falls through
   or floats, and the stderr lines.
7. v0.4.0-mapmaker.28 crash: four Split Crowes and four Crow Walkers in one
   map, fight all of them for a few minutes (let them shoot each other):
   no crash, no `[ICALL] ... not code` line, each boss keeps its own
   effects.
8. v0.4.0-mapmaker.34 walls: four Split Crowes in a map with walls and
   floors of several heights near them; walk around and away from them for a
   few minutes. They stay visible, never inside a wall or outside the map;
   `arena: n of 35 cells in no room` once per Crowe, `player outside a split
   Crowe's arena` when you are away, `walk blocked; ended where it stands`
   instead of `put on its target`; after one, the next walk goes another way
   (`5 -> 6` with another direction).
9. v0.4.0-mapmaker.40 ledges: a Split Crowe on a raised platform or a
   balcony over a lower floor (same tile and a tile below), with a narrow
   gap in the floor near it; it never walks off the edge or across the gap.
   `arena: n of 35 cells in no room at its height, d over a drop or ledge, g
   gaps between cells` once per Crowe, d > 0 near the edge.
