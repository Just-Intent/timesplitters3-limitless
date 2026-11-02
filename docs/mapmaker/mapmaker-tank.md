# MapMaker castle tank (story mission 2 boss)

Status: implemented in `src/interface/pc_mapmaker_tank.inc` (area `tank`),
fixture-tested (`tests/mapmaker/test_mapmaker_tank.py`: 191 mutations, plus
decode, US XBE data and game-pak checks of every site below). Wired in
v0.4.0-mapmaker.20 with the wrappers [below](#wrappers-for-the-integrator).
No guest-code table entry of its own, no new save bytes.

## v0.4.0-mapmaker.28: the third tank stayed a bot; bars read "Castle Tank" (fixed after .28)

Report: three tanks placed, "only two tanks spawned, the third was a
prometheus robot", and "the bar header read Castle Tank instead of the Story
AI name". Log `logs/m28.err` (lines 19457-19489): tanks 1 and 2 log `created
at the placeholder`, `first draw`, `first update`, `placeholder AI removed`,
`fight started`. Tank 1's placeholder was pool slot 1, entity `0x8181D218`,
removed at its first update. Then `castle tank placeholder character created
(pool slot, object) (0x1, 0x8181D218)` (the third placeholder in the same
slot and entity), `realbosses: AI start: character 0x12, model 0x5AC, record
id 0x3`, and no tank line.

Cause (**proven** from the log and the code): the AI start hook's "started
again" test matched any tank whose placeholder entity was this entity, and
returned without a line. The removed placeholder's pool entry and entity
are reused by the next character created (realbosses shows the same reuse),
so the third placeholder looked like tank 1 restarting. Rejected: the 4-tank
cap (n was 2), the per-tank tables (allocated once for 4), the diagnostics
budget (`seen`, 8 entries; the session line budget had 19 of 24 lines used;
neither gates the create). A second, smaller fault: the general line budget
was per session (24), and was used up by the fifth level of that log, so a
later level's refusals would have been silent.

Fix: a tank is "started again" only when its placeholder is still this
entity (not removed yet) or it was made from this spawn record (same record
pointer and record id +8). Every refusal (started again, cap, pak, models,
position, memory, a tank running) logs with its own budget of 16 per level
(`tnk_refuse`); the general lines' 24 are per level now. The diagnostics
record of a placeholder (`seen`) starts afresh when its pool entry comes
back with another record, so the third placeholder's `waiting to spawn`
and `reached its AI start` lines show. The spawn line ends with the record
id.

Cause of the label (**proven** by decode): the bar is stock `sub_0014CC80`
for the focus entity (the tank that woke last), label text 0x165B, which this
area answered with "Castle Tank". The bosses area draws Story AI bars from
live AIs; the tank removes its placeholder AI, so its name was gone.

Fix, as Khallos' bars (`bos_bar_proxy`) but for a removed AI:
`pc_mapmaker_bosses.inc` gains `bos_bar_extra_add` (an area lists its props
with the removed AI's record id) and `bos_story_name` (every Story AI's
saved name, boss or not). This area registers at each mount:

- the stock bar (focus tank, with its "Tank exposed" timer) is labelled with
  that tank's Story AI name (a per-tank 32-byte guest copy, so two tanks
  never share one); "Castle Tank" when the AI is unnamed;
- the bosses area draws one bar per other tank whose Story AI is
  Boss-flagged, in range, alive, with its name, the tank's health and the
  Boss/health multipliers (the tank's damage subtracts fixed amounts and
  dies at 0, no threshold on its maximum: CONFIRMED at 0x00255EF0 and
  0x0025606C). The focus tank is not drawn twice while the stock bar shows
  it. No bar while the placeholder AI still stands there, or once the object
  is no longer model 0x281.

The mothership had both faults (same "started again" test; label
"Timesplitter Mothership"): fixed the same way, see
[mapmaker-mothership.md](mapmaker-mothership.md).

## v0.4.0-mapmaker.27: created, but never seen (fixed after .27)

Report: one tank placed, "did not spawn". The .27 log (`logs/m27.err`, line
36382 on) shows the placeholder created, its AI start, `castle tank created`
at (105, 0.51, 81), its first update (`placeholder AI removed`, logged inside
`pc_mapmaker_tank_update`), then nothing: the update kept running in state 0,
where stock asks the wake test every frame and leaves the update when it says
0 (`je 0x00255BD5` at 0x00255582). The player stood at (118.1, 108.8): 30.7 m
x/z, just outside `TNK_WAKE` 30 m, and never came nearer. In .22/.23 the
player started 19 m away and the fight began within a second.

Cause (CONFIRMED by decode, `test_hidden_until_the_fight`): the init
`sub_002560C0` clears prop `+0x28` bit 0 (0x00256120); only the fight start
sets it (`or [edi+0x28], esi`, esi = 1, 0x00255588); the death clears it
again (0x00255E44) as the wreck replaces the tank. That the bit means
"shown" is INFERRED from those sites (and the same pairing for held weapon
models at 0x001B9C19/0x001B9C7D): a sleeping tank was invisible, so a tank
placed more than 30 m from the player's start could not be found. The swap of
statics, the update and the placeholder removal worked as designed.

Fix: after the create, the AI start hook sets bit 0 (other flags kept), so the
tank stands at its placeholder from the start; the fight still starts at
30 m. Diagnostics, once per tank: `first update (object, flags, state)`,
`first draw (object, flags, at x, y, z)` from a new log-only
`WRAP sub_000A2740` (the prop draw, called by the room prop pass at
0x000A2B37), and, while asleep, `waiting for a player within 30 m (x/z);
nearest living player at (dx, dz)`. The spawn line now ends with the init's
flags.

## v0.4.0-mapmaker.25: no tank at all (diagnostics)

Report: the tank no longer appears, also alone. The .25 log
(`logs/mapmaker25-bossmap.err`, one set with nine bosses, a level per test)
shows, every level, `mounted prop6.pak` and `requested the tank, turret and
wreck models`, then nothing from the tank: its AI start hook never saw a
Story AI with character 0x12 (any 0x12 actor there logs, created or not; the
session log budget was not used up, 18 of 24 lines). Each level started
exactly one Story AI (walker, Crowe, robot, skeleton, worm, mothership); the
tank's level and two others started none. The tank's own hooks are those
of v0.4.0-mapmaker.23 (plus the cannon's gun hook, which runs only inside a
tank), and no other area claims id 0x12 or rewrites an actor's character
(checked: robot 0x42, Crowe 0x46, realbosses 0x15/0x28/0x64, Khallos 0x33
and its rider 2, worm 0x2C, mothership 0x75; the bosses area only reads
0x12 in its probe). Stock (CONFIRMED, test `test_story_ai_spawn_gate`): a
Story AI's spawn `sub_0015F8C0` runs its AI start `sub_0015B830` only when
`sub_0006C890(record +0xC, record +0x2C)` is true, in scene 100 the map's
logic `sub_0006B100` - a placeholder whose spawn logic is false waits
without a line. Not settled from this log, so the tank now says where its
placeholder stops (per level, its own budget):

- `castle tank placeholder character created (pool slot, object)` or
  `... NOT created (pool slot, 0)`: from `sub_000DAC30` (the character create
  realbosses already logs for its ids; id at `[esp+4]`, pool slot at
  `[esp+12]`).
- `castle tank placeholder waiting to spawn: its spawn logic is not true yet
  (actor, logic)`: once per placeholder, from `sub_0015F8C0`, with record
  +0xC; then `castle tank placeholder spawn reached its AI start (actor,
  spawn tries)` when it does.
- `castle tank placeholder AI started without an entity: stays a bot` (that
  case was silent before).

Next playtest: one tank placeholder, nothing else in the set, and the same
with the nine-boss set; send the `tank:` lines.

## Several tanks per map (after v0.4.0-mapmaker.23)

Up to **4** castle tanks per map (`TNK_MAX`); a fifth placeholder stays a
guard robot bot (logged `at most 4 castle tanks per map`). A placeholder that
reuses a removed one's pool entry and entity is a new tank (fixed after .28). The tank's code
keeps its whole fight in statics (CONFIRMED, test
`test_per_tank_statics_and_entries`), so each tank gets its own copy and the
area swaps it in around every way into that code:

| what | where | per tank |
|---|---|---|
| fight block | `0x0078B560..0x0078B5C3`, the 0x19 words `sub_00251AA0` zeroes (state, previous, timers, drive target, target entity, table pointers ...) | host copy, loaded on entry, saved on return |
| sound wobble records | `0x0075F468..0x0075F4D7`, 4 x 0x1C (`sub_000E1100`/`sub_000E1200`, plain numbers) | host copy, loaded and saved with the block |
| parts / drive / gun tables | `0x0051B920` (0x78), `0x0051B998` (0x30), `0x0051B9C8` (0x17C, the five gun records), named only by `sub_00251AA0`; all other code reaches them through block +8/+4/+0 | a guest copy per tank (4 x 0x224 bytes, allocated once per session); the tank's block points at its copy, so the addresses never change for a tank |
| shared, unchanged | tags `0x0051BB44`, courtyard `0x0051BB58`, turret model id `0x0051BB88` (read-only); effect templates `0x0075F288..0x0075F3E0` (every init builds the same ones); castle script words `0x0078B5E0..F4` (`sub_002532D0` only) | - |

Ways into the tank's code, each run with that tank's state (the statics
hold exactly what they held before once it returns, also when one tank's
code runs inside another's, e.g. a shell from tank 2 damaging tank 1):

- update `sub_00255550` (slot 3, object at `[esp+4]`), slot 7
  `sub_00253840` and slot 2 `sub_00253D60` (object at `[esp+4]`), damage
  `sub_00255FD0` (damage record at `[esp+4]`, the hit object at +0, as
  `sub_00255EF0` uses it);
- the boss bar's state-9 timer `sub_00251800` (only `sub_0014CC80` calls it,
  for the object in player 1's focus `[0x007B0C7C]+0xFF4`);
- the spawn: the init (`sub_002560C0` -> `sub_00251AA0`) resets the statics
  and the XBE tables; the result is copied to the new tank and both are put
  back as they were.
- Not needed: `sub_00256040` (called only by the castle's `sub_001E4030` for
  its tagged 'KNAT' object) and `sub_002533E0` (castle script
  `sub_001E77E0`).

The other hooks (wake, area, target, state, gun, wreck) run inside an entry
and act on the active tank: its own castle frame offset d, spawn, wake,
search count, cannon aim and log lines (`[mapmaker] tank N: ...`). Each tank
removes its own placeholder on its first frame and is parked under its own
spawn at its death. The stock boss bar shows the tank that last took player
1's focus (stock: the update sets it when a fight starts), with its Story
AI's name; the other Boss-flagged tanks get the bosses area's bars (after
.28).

## Fixed after v0.4.0-mapmaker.23: the cannon did not aim at the player

Playtest (v0.4.0-mapmaker.23): the tank works; the fight cycles states
1 -> 3 -> 4 -> 3 -> 6 ..., but the user saw the tank gun not aiming at them
when it prepared to fire. How the cannon aims (CONFIRMED, test
`test_cannon_aim`):

- Five gun records `[0x0078B560]` (from 0x0051B9C8, 0x4C each). Guns 0-3
  are machine guns (callbacks +0x38 `sub_00251EF0`, ...); gun 4 is the
  cannon (`sub_00251DD0`, `sub_00253F10` fire effect, `sub_00251F80` turn,
  `sub_00251D40`).
- `sub_002522B0` (ebx = gun, edi = tank, no arguments) turns a gun toward
  its aim point (record +0x10..+0x18, world coordinates) at a limited rate
  (`sub_00251F80` steps by frame time). For guns 0-3 it first calls their
  +0x38 to refresh the aim; for the cannon it does not.
- State 4 (0x00255803): every frame the cannon turns (0x00255894, while
  shots are left in `[0x0078B5AC]`); when the timer `[0x0078B5A0]` runs out
  it fires (`sub_002540B0(4)`, which reads the same aim point) and only then
  re-aims with `sub_00251DD0` (0x00255880; also at the state 4 entry,
  0x002554E3). So stock aims each shot where the target was at the last
  shot.
- `sub_00251DD0` aims at a castle cover object near the target (tags at
  `[0x0078B56C]`, `'TCV5'..'TCV7'`; none in a map), else at the target's
  actor +0xBB4..+0xBBC. The machine guns' `sub_00251EF0` uses that actor
  point only for AIs: for a player (actor +8 < `[0x00790950]`) it uses the
  player record +0xB4..+0xBC (`[0x007B0C48]` + index * 0x1584). INFERRED:
  +0xBB4 is a body point from the character's animation update and is not
  the player's aim point in first person.

Fix: **gun** (new `WRAP sub_002522B0`): for our tank in state 4, when the
cannon (ebx = 4) turns and the target is a player, its aim point is set to
that player's +0xB4 point first. The cannon follows the player at its stock
turn rate while it prepares, and each shot (which reads the aim point) goes
at where the player is. Other guns, other states (state 7 drives at the
cannon's aim point), AI targets and the castle are left to stock code. Log,
each time stock picks a new cannon aim (state 4 entry and after every shot;
32 lines per level): `cannon aim: stock (x, y, z), set to player (x, y, z),
player at (x, y, z)`, the last being the target's position +0x64.

## Fixed after v0.4.0-mapmaker.22: the game froze when the fight started

Playtest log (v0.4.0-mapmaker.22, twice): `castle tank created`, `spawn
(...)`, `placeholder AI removed`, `fight started: a player came near`, then
no more guest frames. Cause (CONFIRMED by decode, test
`test_arena_constants_and_courtyard`): an endless loop in the drive target
search, caused by this area's own **area** hook.

- `sub_00251CD0` is not a "gate" test: its three rectangles 0x0051BB58..
  (x -15..-3 / z -10..-6.8, x -17.5..-3 / z -6.8..6.7, x -21.8..-3 /
  z 6.7..13) together are the castle **courtyard**, where the tank drives.
  It answers 1 when the point is strictly inside one.
- `sub_00254690` (default branch, every state but 6/7/8) picks the next
  drive target: a random point 15 m around the player (clamped to the
  arena box), counted only when `sub_00251CD0` says inside (`je 0x00254895`
  skips `inc esi` at 0x00254894), until 10 points are counted (0x002547E0)
  or the best is 10 m from the tank (0x0025489D, `ja 0x002547E0`). The hook
  answered "never inside", so nothing was ever counted and the loop never
  ended. It runs as soon as the fight starts (state 1).
- Every other loop the tank's frame reaches was checked (backward jumps in
  all 215 functions reachable from `sub_00255550`): the tank code's loops
  run over fixed counts (4-5 guns, 3 sounds, 3 rectangles, 10 model parts,
  the player count, a model's part count); `sub_002548F0`'s backward jump is
  a shared exit, not a loop; the tagged-object lookup `sub_0023FF20`
  ('TKDR', state 1) walks a null-terminated sorted list. None depends on
  castle positions.

Fix: **area** now answers the stock courtyard test with the point moved by
-d (the courtyard sits around the spawn just as the arena box does); inside
the castle-frame calls (target, state 7) the point is already moved and is
tested as is. The same answer drives states 6 (player inside: drive at the
player) and 8 (tank outside: drive back to the castle point
(-9.7, -0.17, 4.3) + d) as in the castle. Guard: when one search has met 64
points outside the courtyard (call site 0x00254848 only), every further
point counts, which ends the loop within 10 more tries, and the log says
`drive target search found no courtyard point: taking any (state, misses)`.
The miss count restarts with every call of `sub_00254690`.

New log lines (their own budget of 48 per level, then `fight log budget used
up for this level`), each `(new state, previous)` when the fight state
changed: `fight state at frame start` (start of the tank's frame), `fight
state at drive target` (entry of `sub_00254690`), `fight state entry`
(entry of `sub_00255390`, end of the frame). The last line before a hang
names the state reached.

## Fixed after v0.4.0-mapmaker.21: the tank model never loaded

Playtest log (v0.4.0-mapmaker.21): `mounted prop6.pak`, then `the tank was
not created (model not loaded?) (0x81B7DDE0, 0x0)`: the resource rows were
found, but `sub_000A6F90` returned 0. Cause (CONFIRMED, test
`test_model_load_path`): nothing had **requested** the models.

- `sub_000A6880` loads a row's model with `sub_00096410(row +0x10 path,
  model)` (0x000A6927). That function finds the file only through the
  level's load request for that path (`sub_000631C0`, 0x00096422). Without a
  request it falls back to looking up the bare row path
  `castle/props/tankboss` in the mounted paks (`sub_00062C30`,
  `sub_00062F00`), which never matches a pak entry (those are the CRC32 of
  `assets/ts/models/<path>/xbox/output/<name>.xbr`). So it returns no model,
  and `sub_000A6880` returns 0 (0x000A695C).
- Requests are made by `sub_000A5E40(model)` -> `sub_00063A40` (builds the
  `.xbr` path from format 0x00478354, queues it sorted by file offset at
  `[0x006D7C84]`). The scene loop `sub_0003D440` reads every queued request
  right after the level setup (`sub_001CDB80` at 0x0003D590, then
  `sub_00063390` at 0x0003D5AD). The castle level requests its tank; a
  MapMaker map requests only the models of its own objects and set.
- In scene 100 the story-only flag and the streaming branch are skipped
  (`sub_00096410` 0x00096451, `sub_000A5E40` 0x000A5E83), so the models
  come from whichever mounted pak holds them: prop6.pak, mounted with
  load=0 (file kept open, so the load pass can read from it).

Fix: **request** (`sub_0015E7C0`, story level init, called once by the level
setup at 0x001CDC20, after `sub_000A5C00` at 0x001CDBF3 has built the
resource table through the callback `sub_00242BA0` -> `sub_00242780`, and
before the load pass): when the map's set holds 0x12, prop6.pak is mounted
and the three rows exist, `sub_000A5E40(0x281)`, `(0x297)`, `(0x298)`.

Guard: `sub_00096410` takes a request's data (+0x10) without a null test
(0x00096437 -> 0x0009651F). So before creating the tank, ai_start checks
that each of the three models is already loaded (row +4) or that its
request (row path +0x10, and second path +0x14 when set) holds the read
file. Otherwise the placeholder stays a bot and the log names the model,
its row and the request data (`tank models not requested this level` /
`a tank model was requested but not read`). The turret and wreck are
checked too because the death creates them through the same path.

## Fixed after v0.4.0-mapmaker.20: the tank never appeared

Playtest log: `mounted prop6.pak for the castle tank`, then `tank models
missing from the resource table: the placeholder stays a bot (0x818B8C80,
0x0)`. The hook looked the models up in the stock model -> row map
`0x0078B860`. The rooms area's guest-code patches
(`config/guest-code-patches/rooms.json`) move every access of that map to
`0x008CA000` (0xA00 words), including the builder `sub_00242780` (0x002427C8,
0x00242A34) and the spawn path (`sub_000A6F94`, `sub_000A6EC4`,
`sub_000A688F`), so the stock address is never written and held a stale row
(init 0). The tank now reads `0x008CA000` (CONFIRMED, test
`test_resource_map_relocated`).

No rows need adding: story scene 100 loads `prop/table/prop_all_xbas.raw`
(`sub_00242780`: `sub_00065250` answers 1 in story mode -> 0x0049F01C,
CONFIRMED), and every shipped copy of that table (all `arcade/l_*`,
`challenge/l_*_ch`, `story/l_56_st` paks, 2512 rows) has 0x281 (type
0x8000000, init index 26), 0x297 and 0x298 (CONFIRMED, game paks). The
builder sizes the row table exactly (`sub_0003DFA0(count*0x30, 1)`,
0x0024298C..0x0024299B; count `[0x0075EAA8]`, no spare rows), so appending
would mean a reallocation as the rooms area does for its room models; it is
not needed. If the tank still does not appear, the failure log now also
prints the three rows found (`row+1`, 0 = none). Switch: `TSFP_MAPMAKER_LIMITS=1`; `TSFP_MAPMAKER_TANK=0` turns only
the tank off (its placeholder then plays as an ordinary bot).

User report (2026-10-31): the tank boss of story mission 2 was not available
in MapMaker. Research: `logs/limits/features/real-bosses.md` section 4 (main
checkout, ignored). CONFIRMED = seen in the decode, the US XBE data or the
game paks and checked by the test; INFERRED = reasoning.

## What the tank is

| | value |
|---|---|
| actor | a **prop**, not a character: model 0x281 `CASTLETANKBOSS`, resource type 0x8000000, path `castle/props/tankboss`, init index 26 -> `sub_002560C0` (CONFIRMED: `prop_all_xbas` in `arcade/l_100.pak`, table 0x004FCA60) |
| made at its death | turret 0x297 `CASTLETANKBOSSTURRET` (via `sub_00254D10`), wreck 0x298 `CASTLETANKBOSSWRECK` (`sub_000A6EC0(1, 0x298, ...)` at 0x00255D4E) (CONFIRMED) |
| model files | all three only in `pak/stream/lv4/prop6.pak` (string 0x0049DF7C); not in `chr.pak`, `gun.pak` or `l_100.pak` (CONFIRMED, CRC32 index). Textures: not checked separately (INFERRED: inside the .xbr or the same pak) |
| scene-100 resource rows | 0x281, 0x297, 0x298 are rows of `prop_all_xbas`, the table MapMaker play loads (CONFIRMED) |
| callbacks (init) | update `sub_00255550` (slot 3), damage `sub_00255FD0` (only damage types 8 and 0xC hurt it), static reset `sub_00251AA0`; health +0x1CC from the difficulty byte (CONFIRMED) |
| state | static `0x0078B560..0x0078B5F4`: state `[0x0078B594]` 0..10, previous `[0x0078B598]`, drive target `[0x0078B580..88]`, target entity `[0x0078B5B0]`, five gun records `[0x0078B560]` = 0x0051B9C8 (0x4C each, aim point +0x10) (CONFIRMED) -> **a copy per tank** (above) |
| scene tests | none in the tank's code (CONFIRMED) |

The fight (CONFIRMED from `sub_00255550` and its jump tables 0x00255BDC/
0x00255BE8/0x00255BF4; meanings INFERRED): 0 wait for the start test, 1
drive in, 3 chase and fire, 4 gun attack, 5 no target, 6 player in the
courtyard (drive at the player), 7 player below the courtyard, 8 tank out of
the courtyard (drive back in), 9 stunned by a
heavy hit (12 s; a second bar shows the time), 10 dead.

## Castle assumptions and what this area does about them

| castle assumption (CONFIRMED site) | in a MapMaker map | this area |
|---|---|---|
| start test `sub_00253140`: the castle object `'RDPC'` (`sub_000B2350`) must exist and every living player be in state 0x15, then level marker 5 (`sub_00227650(5, 3.0)`) | `'RDPC'` is not found: the fight would never start; marker 5 is the map's own logic marker | **wake**: starts when a living player is within 30 m (x/z) of the tank; no marker |
| courtyard: three rectangles 0x0051BB58.. (`sub_00251CD0`), x -21.8..-3, z -10..13: player inside -> state 6, tank outside -> state 8 (drive to the castle point (-9.7, -0.17, 4.3)), the drive target search counts only points inside and loops until it has some | the courtyard is elsewhere | **area**: the stock test moved by -d; the search takes any point after 64 outside (v0.4.0-mapmaker.22 answered "never" and froze) |
| drive target `sub_00254690`: player clamped to the arena x -19.1..-5, z -10..13; state 7 target from gun 4's aim point, y 0.17 | the map is elsewhere | **target**: run in the castle frame (below) |
| state 7 entry `sub_00255390`: castle aim points (-5.3, -1.4, 19.9) or (-13, -2.2, 25) into all five guns | same | **state**: castle frame, aim points moved by +d after |
| first drive target (-9, own y, -3.2) set by `sub_00251AA0` | same | moved by +d at spawn |
| death `sub_00255C20` parks the dead tank at (0, 20, 0) | a point in the open map | **wreck**: parked 50 m under its spawn point |
| state 9 fires level event 0x16 every frame (`sub_002274C0` at 0x00255A8F) | the map's own logic marker 0x16 | **event**: dropped |
| boss bar labels 0x165B / 0x1635 (`sub_0014CC80` case 0x281) | the MapMaker level's own text ids | **text**: the focus tank's Story AI name (else "Castle Tank") / "Tank exposed" at those two call sites |
| cannon aim `sub_00251DD0`: a castle cover object near the target, else the target's actor +0xBB4, refreshed only after each shot | no cover; for a player that point is not the aim point the machine guns use | **gun**: in state 4 the cannon aims at the player record +0xB4 before every turn |
| tagged objects `'TKDR'` (door, 0x00255767) and `'TCV5'..'TCV7'` (`sub_00251DD0`) | absent | none needed: both null-checked by stock code (CONFIRMED) |

**Castle frame.** d = spawn - (-12.05, 0, 1.5), the arena's centre (x
-19.1..-5, z -10..13, CONFIRMED) with the courtyard floor INFERRED at y 0
(the castle's fixed targets sit at y -0.17 and 0.17; "below the courtyard"
is y < -1). For the call, the tank and its target entity are moved by -d
and restored exactly afterwards; the drive target goes in and out in castle
coordinates (left bit for bit when the call picks none). `sub_00254690`
calls only `sub_00153B40` (random) and `sub_00251CD0` (CONFIRMED), so no
other code sees the moved positions. The tank therefore roams a 14 x 23 m
box centred on its spawn point (x +-7.05, z +-11.5), inside the courtyard
moved by d (x -9.75..+9.05, z -11.5..+11.5 from the spawn, narrower at the
-z end): a MapMaker floor should be open there.

**Not rebased (risk).** The update's height tests use absolute heights:
player y < -1.0 enters state 7 (0x002556BE), y <= -2.4 keeps it
(0x0025590D). They cannot be changed by a wrapper (inline compares). On a
MapMaker floor lower than y -1 a player near the tank puts it in state 7: it
aims at the two rebased castle points and drives near gun 4's instead of
chasing. Place the tank on floor height 0 or above. The spawn height is
logged (`[mapmaker] tank: spawn (...)`).

## Placement and save format

A map places the tank with a **Story AI whose character is id 0x12**
(`CHR_PRISON_GUARDROBOT`, model 0x5AC in `chr.pak`, human rig; CONFIRMED).
0x12 is story-only (no Arcade row, CONFIRMED) and in no story set (research,
story-enemies.md), and no other boss port uses it. Change `TNK_ID` (one
define) if the integrator prefers another id.

- Save format: **unchanged**. The id is a stock custom-set member (map-info
  chunk +0x18 or the sets chunk's slots 11-20); the Story AI chunk is stock.
- Stock or older builds: the Story AI plays as a guard robot bot.
- With `TSFP_MAPMAKER_TANK=0`, or when the pak/models cannot be loaded, or
  for a fifth placeholder in the same map: a guard robot bot (logged).
- The Story AI's name labels the tank's bar; its Boss flag and Boss/health
  multipliers apply to the tank's health and bar (bosses area, after .28).
  Its own health setting and logic do not: the tank is a prop with its own
  damage rules, and its defeat is not seen by MapMaker logic (no level
  event, no AI death).

Editor: this area adds **no picker row** (the picker hooks belong to
`pc_mapmaker_realbosses.inc`). Until the integrator adds one, a map gets id
0x12 only from a file edit. Suggested row: synthetic row 153 "Castle Tank",
id 0x12, variant -1, preview stand-in any Arcade robot (as the real-boss rows
do; 0x12 has no Arcade row).

## Play, step by step

1. **mount** (`sub_00062790`, the level's pak mounts, once per level load
   from `sub_001CDB80` 0x001CDB85; skipped by stock when `[0x006D7C4C]` is
   set, which no code writes): after the original the previous level's tank
   is forgotten; when the map's own custom set (index 0x11) holds 0x12, or
   the container cannot be read, prop6.pak is not mounted and the 30-slot
   table `[0x006D7880]` keeps two free, mount it with
   `sub_000622D0(0x0049DF7C, 0, 1)`: kept open, entries read on demand, like
   `chr.pak` (0x0006298E). A missing file answers -1 (`sub_000389B0` is a
   plain ret). The stock bulk unmount removes it with the level.
1b. **request** (`sub_0015E7C0`): the three models are requested for the
   load pass (see the v0.4.0-mapmaker.21 fix above).
2. **ai_start** (`sub_0015B830`): after the original, each pool entry
   with character (a+0x48) 0x12 (up to 4 tanks), when the pak is mounted and the resource map
   (`0x008CA000`, moved there by the rooms area) has 0x281 (row +0x2C == `sub_002560C0`), 0x297 and 0x298,
   and all three are loaded or their requests hold the read file:
   `sub_000A6F90(0x281, &pos, &rot, 0, 0)` with pos = the placeholder's
   +0x64..+0x6C and rot = (0, its +0x70, 0). Layout (CONFIRMED):
   `sub_000A6F90` -> `sub_000A6EC0(type, model, pos, rot, a, b)` ->
   `sub_000A6880` copies pos to +0x64/+0x68/+0x6C and rot to +0xC0/+0x70/
   +0xC8, returns 0 when the model does not load (0x000A695C); the init
   runs at 0x000A6F3A. Labels and the 4 x 32-byte bar names are allocated
   here. Started again (no new tank, logged): a tank whose placeholder is
   still this entity, or made from this spawn record and id.
3. **update** (`sub_00255550`, the tank's frame): on its first frame the
   placeholder AI is removed with `sub_0015DDA0` (the Crow walker's removal
   call) after checking the pool entry still holds that AI; a story focus
   on it is cleared.
4. **wake**, **area**, **target**, **gun**, **state**, **wreck**, **event**,
   **text**: as in the table above.

## Wrappers for the integrator

All in `src/interface/pc_mapmaker_tank.inc`; every hook runs the original
unchanged when the switch is off or outside scene 100.

| wrapper | function | chaining |
|---|---|---|
| `WRAP sub_00062790 -> pc_mapmaker_tank_mount` | level pak mounts | new |
| `WRAP sub_0015E7C0 -> pc_mapmaker_tank_request` | story level init (no args) | **new after v0.4.0-mapmaker.21**; works after the original, so any order with other wrappers |
| `WRAP sub_0015B830 -> pc_mapmaker_tank_ai_start` | AI start (cdecl pool entry) | **shared** with `pc_mapmaker_realbosses_ai_start`; order free (each acts only on its own ids; the tank's runs its work after the original) |
| `WRAP sub_00255550 -> pc_mapmaker_tank_update` | tank frame (cdecl object) | new |
| `WRAP sub_00253140 -> pc_mapmaker_tank_wake` | fight start test (no args, eax) | new |
| `WRAP sub_00251CD0 -> pc_mapmaker_tank_area` | courtyard test (ecx = point, eax) | new; behaviour changed after v0.4.0-mapmaker.22 (no wiring change) |
| `WRAP sub_00254690 -> pc_mapmaker_tank_target` | drive target (edi = tank) | new; must see the guest `g_edi` |
| `WRAP sub_002522B0 -> pc_mapmaker_tank_gun` | gun turn (no args; ebx = gun, edi = tank) | **new after v0.4.0-mapmaker.23**; must see the guest `g_ebx` and `g_edi` at entry; works before the original |
| `WRAP sub_000DAC30 -> pc_mapmaker_tank_create` | character create (cdecl id, ..., pool slot) | **new (.25 diagnostics)**; **shared** with `pc_mapmaker_realbosses_create`; order free (log only, after the original) |
| `WRAP sub_0015F8C0 -> pc_mapmaker_tank_spawn` | Story AI spawn (cdecl entity) | **new (.25 diagnostics)**; log only, around the original |
| `WRAP sub_00253840 -> pc_mapmaker_tank_slot7` | tank callback slot 7 (cdecl object, arg) | **new for several tanks** |
| `WRAP sub_00253D60 -> pc_mapmaker_tank_slot2` | tank callback slot 2 (cdecl object) | **new for several tanks** |
| `WRAP sub_00255FD0 -> pc_mapmaker_tank_damage` | tank damage callback (cdecl damage record) | **new for several tanks** |
| `WRAP sub_00251800 -> pc_mapmaker_tank_bar` | boss bar state-9 timer (no args, returns on the x87 stack) | **new for several tanks**; the hook only swaps memory around the original |
| `WRAP sub_000A2740 -> pc_mapmaker_tank_draw` | prop draw (cdecl prop, view), hot | **new after v0.4.0-mapmaker.27**; log only, before the original; no tank this level: one compare |
| `WRAP sub_00255390 -> pc_mapmaker_tank_state` | state entry (eax = tank) | new; must see the guest `g_eax` at entry |
| `WRAP sub_00255C20 -> pc_mapmaker_tank_wreck` | death (esi = tank) | new |
| `WRAP sub_002274C0 -> pc_mapmaker_tank_event` | level event (cdecl) | **shared** with `pc_mapmaker_realbosses_event` (and other boss ports); order free: each matches only its own return addresses |
| `WRAP sub_0003D7B0 -> pc_mapmaker_tank_text` | text lookup (cdecl), hot | **shared** with `pc_mapmaker_realbosses_text` (and others); order free: ids 0x165B/0x1635 at their return addresses only |

After .28 no wrapper changed: the bar names use `pc_mapmaker_bosses.inc`
(`bos_bar_extra_add`, `bos_story_name`, under `BOS_BAR_EXTRA`), included
before this area as for Khallos' `bos_bar_proxy`.

Guest functions called: `sub_00062260` (is a pak mounted), `sub_000622D0`
(mount), `sub_000A5E40` (request a model), `sub_000631C0` (find a model's
request), `sub_000A6F90` (create the tank), `sub_0015DDA0` (remove the
placeholder AI). Host: `xbox_HeapAlloc` (labels, 32-byte argument buffer, the 4 x 0x224-byte tank tables,
the 4 x 32-byte bar names, once per session). The area stubs (`tests/mapmaker/mapmaker_area_stubs.h`)
need `sub_000622D0`, `sub_000A6F90`, `sub_000A5E40` and `sub_000631C0` when
the include chain links this area (`sub_00062260` and `sub_0015DDA0` are
already there).

Also for the integrator: the editor picker row (above), and the
`docs/agent-context` guide entry for this area.

## Risks (not verified in game)

- Played four times: v0.4.0-mapmaker.20 failed on the stale map address,
  .21 on the missing model requests, .22 froze in the drive target search,
  .23 worked with the cannon aiming off the player (all fixed above). The
  cannon tracking has not been played yet; it makes the cannon harder to
  dodge than the castle's (which aims where the player was at the last shot).
- MapMaker walls or pits inside the moved courtyard: the tank's targets
  ignore them (the castle courtyard was open ground).
- The models are read during the level load (about 380 KB from prop6.pak,
  research estimate), whether or not the tank's placeholder spawns.
- Collision and driving of a 0x8000000 prop on MapMaker tile floors
  (`sub_002548F0` drives by its own position; INFERRED it follows the floor
  through stock object physics).
- Textures of the tank not checked separately from the .xbr.
- Shown before its fight: shots can damage a sleeping tank (the damage
  callback has no state-0 test); unplayed.
- Creating a prop during AI start (`sub_0015B830` from `sub_0015F8C0`)
  instead of during the level's object load (INFERRED safe: the castle and
  MapMaker both create props at level start).
- Removing the placeholder AI might count as an AI death for MapMaker logic
  ("kill all" objectives) (unknown).
- The absolute height tests above; the castle floor height (INFERRED 0).
- Model loads need memory in the level heap when the tank spawns (~1.9 MB
  pak, about 380 KB of models loaded on demand, research estimate).

## User test steps

1. A map whose custom set holds id 0x12 with one Story AI of it on a
   ground-floor spot more than 30 m from the start: in story play the tank
   stands there, visible from afar (log: `mounted prop6.pak`, `requested the
   tank, turret and wreck models`, `castle tank created`, `spawn (...) flags
   0x.. shown`, `placeholder AI removed`, `first update`, `waiting for a
   player`, `first draw` once it is on screen).
2. Walk within 30 m: it starts (log `fight started`, then `fight state ...
   (1, 0)`), drives and fires; the bottom bar reads the Story AI's name
   ("Castle Tank" when unnamed). The game keeps running; if it stops, the
   last `fight state` line names the state.
3. Explosives hurt it; a heavy hit shows "Tank exposed" for 12 s
   (log `level event dropped`); map logic markers are unaffected.
4. Destroy it: wreck and flying turret appear, the tank vanishes
   (log `destroyed`).
5. Three and four named tanks (after .28): every one is a tank (log `tank 3:
   spawn ... record id 0x..`, `tank 4: ...`), none a guard robot; a fifth
   stays one (log `at most 4 castle tanks`). Each bar shows its own Story AI
   name; with Boss on, each tank in range gets a bar. `TSFP_MAPMAKER_TANK=0`
   gives guard robots; story mission 2 still plays stock.

## Not appearing in .25/.26: set slot 14 (fixed in .27)

The tank sat on set slot 14, whose spawn mode mask never allowed story mode;
see [Spawn mode mask](mapmaker-sets.md). Nothing in the tank area was at fault.
