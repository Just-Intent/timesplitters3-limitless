# MapMaker real story boss: the far-future giant robot (story mission 10)

Status: implemented in `src/interface/pc_mapmaker_robot.inc` (area `robot`),
fixture-tested (`tests/mapmaker/test_mapmaker_robot.py`: 98 mutations, plus
decode and US XBE data checks of every site below). Wired in mapmaker-recon;
**up to four robots per map**, each with its own state (see
[Several robots](#several-robots)): the four new wrappers for that are not
wired yet (see [Wrappers for the integrator](#wrappers-for-the-integrator)). Switch: `TSFP_MAPMAKER_LIMITS=1`;
`TSFP_MAPMAKER_REAL_BOSS=0` keeps the robot on the normal bot AI.

Research basis: `logs/limits/features/real-bosses.md` section 6 (main
checkout, ignored). CONFIRMED = seen in the decode or XBE data (and checked by
the test); INFERRED = reasoning, needs a playtest.

## What the robot is

| | value (CONFIRMED) |
|---|---|
| character id | 0x42 (no Arcade-table row; `sub_002BE270(0x42)` == 0, so the stock slot reload drops its variant) |
| character table 0x0053A3CC + 0x42*0x30 | voice set 0x0053A0C8 (the Arcade giant robot's, id 0xA8), model 0x5F2 `chrs/FF2GiantRobot`, no LOD, story-only flag 0x2000000 (skipped in scene 100 by `sub_000A5E40`) |
| model file | `pak/chr.pak`, mounted in MapMaker play (research). INFERRED: the same mesh as the Arcade giant robot (`giantrobot_arcade`, same file size) |
| story AI class | AI +0x14 = 0x80000, +0x10 = 0: `sub_0027D590` 0x0027DDF5 -> `sub_00293730(entity)` (call 0x0027DE18, its only caller); message handler `sub_00294520` (0x0027D402 / call 0x0027D447) |
| animations | `giantrob_startthroughdoor` 0x807, `giantrob_walkcycle` 0x808, `giantrob_death` 0x809, named only by `pak/stream/lv14/anim/lv14.pak` (string 0x0049DC78, 258 KB, table 0x0047E938 x 0x29 registered at 0x001CA316). No id shared with lv1_3, lv1_6 or lv81. (`lv5_4.pak` names other giantrob ids 0x792..0x794 for a scene-5 cameo the boss does not use.) |
| per-level state | `sub_00292630`: `[0x0078B1DC]` = `sub_0003DFA0(0x4C, 1)`, then `sub_00292550` sets every field. The handler dereferences it unchecked (0x002937DC). Particles: the shared 800 x 0x28 block `[0x0078B0D0]` (`sub_002A62C0`) |
| where stock makes them | `sub_0025F720` (story level init) for **scene 14 only** (jump table case 0x0025F760: `sub_0028D6D0`, `sub_00292630`, `sub_002A62C0`). Scene 5 gets none. `sub_0028D6D0` is the split Crowe's block `[0x0075F5E8]`; no robot function reads it, nor the walker/skeleton states |
| boss bar | `sub_0014CC80` case 0x0014D06F for model 0x5F2, no scene test, label text 0x163D (level-local id) |

### The fight (CONFIRMED unless marked)

States (AI +0x20, table 0x002944FC): 0 intro (0x00293847: damage callback
`sub_000BC470`, particle setup, health +0x1CC/+0x1D8 = 55.0, story focus
0x002938A9); 1 door entry (0x002938B4): plays `giantrob_startthroughdoor`
until `sub_00059DD0` says the animation ended. This "scripted door entry" is
only an animation: no level door or tagged object is involved, so it
completes anywhere (INFERRED: its root motion walks the robot a few units
forward from its spawn point). 2-5 walk and attacks (projectiles through
`sub_00292BF0`, `sub_00292FA0`, `sub_00293100`, the stock projectile
`sub_000EA9C0`); 6 death.

- No scene read in any robot function; the only direct callee of the
  handler that reads the scene is the level event `sub_002274C0`. The message
  handler and the callbacks `sub_000BC470`, `sub_002926B0`, `sub_002936C0`
  call nothing that reads it.
- No forced height: no robot function stores to entity +0x68 (unlike the
  skeleton's 0x0029D06B).
- No tagged-object lookup (`sub_0023FF20`) is reachable from the fight.
- Distances are from the robot's and the first player's positions
  (`[0x007B0C48]+0x14`, copied into the state each frame).
- **Hangar pads**: the walk compares the first player's pad position
  (pad table `[0x00715F2C]`, stride 0x20) against hangar coordinates (230,
  265, 2, -2) to pick one of the pad ids 0x46E, 0x6A4, 0x465, 0x466, 0x473,
  0x468, 0x690; `sub_0009CC40(id)` (binary search, -1 when missing) is
  called once as an existence test, and only when the id was found a second
  time to fetch the index (0x00293981, 0x00293CE0). A missing id falls back
  to the player's own pad (actor +0x678). MapMaker pad ids are 0x3E9 + i
  (mapmaker-pads.md), so a map with more than 124 pads would hold such ids at
  arbitrary places.
- Damage callback `sub_002936C0`: health -= damage (floor 0); only the first
  player's hits set the state's aggro flag +0x38.
- Death: message 2 (unless already in state 6) gives the kill credit, shakes
  nearby players' views, sets state +0x48 = 1 (0x00294697), plays a sound,
  `giantrob_death` 0x809 and AI +0x9D0 = 6, returns -1 (no generic death).
  No scene test. State 6 spawns explosions (`sub_002A7B90`) every frame,
  raises character-table field +0x20 of id 0x42 by 0.075 per frame up to
  3.5 (global data; generic code `sub_000D2FD0`/`sub_000DAC30` also writes
  that field; INFERRED: a shade/glow, not traced further), and once the death
  animation has ended **every frame** calls `sub_000462B0(0)` (music track 0,
  `[0x004D1850]`), level event 2 (`sub_002274C0(2, 0.5)`, 0x002944E0) and
  clears the focus. It is never removed; mission 10 moves on through its
  level script instead.
- Explosions are drawn by `sub_00292650`, called by the frame
  `sub_001CEBC0` only in scene 14 (0x001CEE49 .. 0x001CEE58), only when a
  level object tagged 'MECH' exists (null-checked) and state +0x48 == 1, and
  not in story split screen: `sub_00019D70(1)`, `sub_002A7F30([[0x007B0CC0]+0x208], 2.5)`.
  `sub_002A7F30` only draws (its stores are to its frame). Particles age only
  inside the spawn calls.
- Soft restart: `sub_0015E4F0` (from the scene loop's restart path
  `sub_001CDD60`, which runs after the level was initialised) tail-jumps to
  `sub_0025F6A0`, which resets the robot state with `sub_00292550` only in
  scene 14. Restart audit after .31 (the bosses' restart bug,
  [Khallos](mapmaker-khallos.md)): this area is **not affected**. Its level
  hook `sub_0025F720` does not run on "START restart", but `sub_0025F6A0`
  does (`sub_001CDD60` 0x001CDD74 -> `sub_0015E4F0` -> 0x0015E7B1), so
  `pc_mapmaker_robot_reset` resets every block, its statics and `removed`;
  the restarted robot keeps its instance by record and gets its class again
  at its AI start (fixture: "the same one again (restart)" and the soft
  restart case). Its other per-level counters only bound log lines.
- No overhead meter: the robot never calls `sub_0012C800`, so it gets no
  +0x268 record and `sub_0014D1F0` draws no meter on it.

## What the area does (scene 100, story mode, switch on)

| WRAP | function | what it does |
|---|---|---|
| `sub_001C8540` | `pc_mapmaker_robot_anims` | after the original: when the map's custom set (index 0x11; slots 1-10 in map-info chunk 0x00C77668, 11-20 in the sets chunk 0x00C776F5) holds 0x42, or the container cannot be read, and lv14.pak is not mounted (`sub_00062260`; the split Crowe area may already have mounted it) and two of the 30 mount slots stay free: `sub_0005A130(lv14.pak)`, the stock per-level anim pak call (whole file into the level heap, unmounted with the level) |
| `sub_0025F720` | `pc_mapmaker_robot_level` | before the original: `[0x0078B0D0]` = 0 (in scene 100 it can only be an earlier level's dangling block). After: forget the previous level's robots; for such a map, when no particle block exists yet (the realbosses skeleton/walker make one), `sub_002A62C0` (32 KB); then four state blocks: each time `[0x0078B1DC]` = 0 and `sub_00292630` (a fresh 76-byte block, set by `sub_00292550`), kept while the pointer is usable. Outside any robot `[0x0078B1DC]` holds the first block |
| `sub_0015B830` | `pc_mapmaker_robot_ai_start` | before the original: pool entry with character 0x42 (or entity model 0x5F2), lv14.pak mounted and this level's state in place: record +0/+4 = 0/0x80000, and the robot gets the next free state block (its statics start at 0). Beyond the blocks made (four) a robot keeps the MapMaker class (logged). The same record again (restart) keeps its block |
| `sub_00293730` | `pc_mapmaker_robot_handler` | the robot's AI (dispatcher 0x0027DE18, entity arg 1): runs with that robot's globals (see Several robots) |
| `sub_00294520` | `pc_mapmaker_robot_message` | its messages (0x0027D447, entity arg 1): same |
| `sub_002926B0` | `pc_mapmaker_robot_callback` | the slot-2 callback its door entry installs (0x002938EE, entity arg 1): same |
| `sub_002936C0` | `pc_mapmaker_robot_damage` | its damage callback (0x00293923, hit record arg 1, entity at +0): same |
| `sub_0009CC40` | `pc_mapmaker_robot_nav` | for the seven existence tests in the handler (returns 0x00293949, 0x00293BC9, 0x00293C10, 0x00293C2B, 0x00293C64, 0x00293C9E, 0x00293CD0) with this level's robot: -1, so the robot always walks to the player's pad. Every other caller (and the two index fetches, unreachable then) runs the original |
| `sub_000462B0` | `pc_mapmaker_robot_music` | the death's music change (return 0x002944D9): dropped, the map keeps its track |
| `sub_002274C0` | `pc_mapmaker_robot_event` | the death's level event 2 (return 0x002944E5) is the map's own logic marker 2 in MapMaker: dropped; the robot whose code is running (the swap knows it) is removed with `sub_0015DDA0` (the skeleton's stock death call, as the walker port) and a story focus still on it is cleared. Once per robot |
| `sub_00126560` | `pc_mapmaker_robot_fx` | after the original (only the call returning to 0x001CEE8C): while any of this level's robots is dying (its own state +0x48 == 1) and not yet removed (once a frame: one shared particle block), and not in story split screen: `sub_00019D70(1)`, `sub_002A7F30(view, 2.5)` as `sub_00292650` |
| `sub_0025F6A0` | `pc_mapmaker_robot_reset` | after the original, with this level's state blocks: for each, `sub_00292550` on it as scene 14 does (pointer put back after), its statics cleared, and the robot counts as not removed again |
| `sub_0014CC80` | `pc_mapmaker_robot_hud` | chain INSIDE `pc_mapmaker_bosses_hud`: when the focus is one of this level's robots, the focus is cleared for the original and restored after, so the stock bar (with level text 0x163D as label) never shows; the bosses area then draws its Boss bar for a Boss-flagged robot (its name, multipliers), none otherwise |

## Openings lower than the robot (v0.4.0-mapmaker.40)

User report: "giant robots being able to follow the player through walls
which exceed their height as long as there is some opening/door on the
ground".

Cause (CONFIRMED, decode): in every walk frame the handler sets actor
+0x684 to the player's pad (the hangar ids are -1, above) and calls the
stock walk goal `sub_0015D500(entity, [actor+0x684])` (0x00293D22, return
0x00293D27), which routes over the human-sized pad graph. MapMaker pads and
links come from the tile templates and the stitches through each
connection, so every doorway has a link; the robot's collision does not
stop it there.

Options considered: filtering the stock A* by clearance needs the robot's
height inside the group and pad searches and the precomputed group distance
matrix (`sub_00275F70`), which all AIs share; stopping at the first low
opening and fighting from range alone leaves a robot stuck when a tall way
round exists. Chosen: **its own reachable pad set, with steering**
(`pc_mapmaker_robot_goal`, WRAP `sub_0015D500`, only that call):

- Once per level the area reads the pads (`[0x00715F2C]` x `[0x007B6664]`,
  0x20 each: +4 bit 0 used, +0xA room from the pad snap, +0xC position) and
  the used links (`[0x00715F30]` x `[0x007B6660]`, 0x14 each: +0 flags
  0x10000, +4/+8 pad indices; `sub_0009A2C0`, `sub_0009A5B0`). A link between
  pads of two rooms crosses the portal joining them nearest to its midpoint
  (room record `[0x00708788]` + room x 0xB4, +4 -> {count, ids}; portal
  `[[0x0070878C]+id*4]`: +0/+4 rooms, +0x14 int16 vertex count, +0x18
  vertices of 0xC, `sub_0008C620`/`sub_0008BDD0`); its opening is the
  vertices' y extent. Links inside one room, with an unknown room or with no
  portal between the rooms (teleporters) stay open (stock).
- The robot's height is its entity box top (+0x1AC centre y + +0x1C4 half
  height, read as at 0x0006B61D..0x0006B68F); 6.0 when that is not between 1 and 40 (INFERRED).
  A link is passable when its opening is at least the height - 0.25.
- From the robot's pad (actor +0x678) a breadth-first search gives the pads
  it reaches (again whenever it stands on a pad outside the set). A player
  pad outside the set is replaced by the reachable used pad nearest to it:
  the robot walks to the opening and stays there, attacking by its stock
  distance rules.
- When the set has blocked links, the goal is steered: hop distances to the
  goal over passable links, and the robot gets the last pad of its own room
  on that path, then the first pad across the opening, one room at a time.
  Within a room the stock route is used (INFERRED: it stays in the room).
- The replaced goal is written to the call's argument and to actor +0x684
  (so the stock goal compare in `sub_0015D500` sees one goal, and the
  handler's arrival test re-picks the player's pad on arrival); the argument
  slot is put back after.

Logs (`[mapmaker] robot:`): `giant robot k (height h) reaches n of m pads
from pad p; b links through openings lower than it blocked (c room
crossings)` at most 4 times per robot and level, `cannot fit to the
player's pad: it waits at the nearest pad it reaches` and `steered to the
opening it fits through` (budgeted).

INFERRED (playtest): that MapMaker portals are the doorway's shape (if a
tile side is one portal, a robot taller than a floor passes no side of a
one-floor tile, and one shorter passes every side); the robot's box height;
that pad steps look smooth. Tests: `low_door()` in
`tests/mapmaker/mapmaker_robot.c` (a robot facing a low door with and
without a tall way round, a door as tall as it, the box fallback and a small
robot, an unknown room, other callers and entities), the `goal-*`
mutations and the decode test `test_walk_goal_and_pad_graph`.

Health display, as the other real bosses: Boss Off = no health shown; Boss
bar = the bosses area's bottom bar; Sub-boss bar = nothing (the robot has no
overhead meter). The handler sets health 55 in state 0; the bosses area
rescales a maximum the game set again (bosses.inc), so its multiplier still
applies (INFERRED from that rule).

Save format: unchanged. The robot is a stock custom-set member (word id 0x42,
variant -1). A stock game or an older build spawns id 0x42 with the stock
MapMaker class on the robot model (INFERRED: the human-rig animations, as the
other real bosses' stage 1).

## Several robots

User request (v0.4.0-mapmaker.25): "Only one giant robot acting under
appropriate logic when creating multiple." Stock keeps the robot's fight
state behind globals, so the first port allowed one. Decode (CONFIRMED,
`test_robot_per_instance_globals`):

| global | users | per robot? |
|---|---|---|
| `[0x0078B1DC]` state pointer (0x4C block) | only the robot's functions: `sub_00292550/2630/2650/26B0/2BF0`, `sub_00293100/3660/36C0`, the handler, the message handler | **yes**: one block each, the pointer swapped |
| `[0x0075F5EC]` shot counter | `sub_00293100` only | **yes**: swapped |
| `[0x0075F5F0]`, `[0x0075F5F4]` walk / attack timers | the handler only (`sub_002B26F0` reads `[0x0075F5F4 + i*4]` as an unrelated table) | **yes**: swapped |
| `[0x0078B0D0]` particle block, `[0x0078B0CC..0x0078B188]` its cursor and spawn parameters | the particle module `sub_002A62F0/65A0/7B90/7D60/7F30` (also the realbosses skeleton/walker) | **no**: the same parameters for every robot; one pool. A robot's intro re-arms the pool (`sub_002A65A0`), so a second robot's intro during the first one's death restarts those explosions (cosmetic) |
| `[0x0078B080..0x0078B098]` effects blocks | `sub_002A8300..` (the split Crowe's effects); no robot code reaches them | untouched |
| `[0x0075F5E8]` split Crowe block | never read or written by robot code | untouched: the robot's swap is independent of any Crowe swap |
| story focus `+0xFF4`, level event 2, music | the handler | focus: whichever robot set it last (the bar hook hides the stock bar for any robot); event and music dropped, removal by the running robot |

Every state reader is reached only from four entries (CONFIRMED): the
handler (dispatcher 0x0027DE18), the message handler (0x0027D447), and the
two callbacks the handler installs (slot-2 callback `sub_002926B0` at
0x002938EE, damage `sub_002936C0` at 0x00293923); plus the scene-14 draw and
the restart reset, which this area replaces in scene 100. Each of the four
is wrapped: the entity (or the hit record's +0) names the robot (its AI's
pool entry and spawn record); the wrapper saves the current pointer and
statics, loads that robot's, runs the original, keeps the robot's statics
and puts the previous values back. Nested entries (one robot's code reaching
another's messages) stack. An entity that is not one of this level's robots
(mission 10, a fifth robot on the bot class, a reused pool entry) runs the
original untouched. Removal: event 2 removes the robot whose code is running.

Cap: four (four 76-byte blocks per level; the statics live in the area).
Change `RBT_MAX` to raise it.

## Risks (INFERRED, need a playtest)

- **Size**: a giant model on MapMaker tiles: it may not fit through
  doorways; the walk targets the player's pad, so it may push into walls.
  Add it to the bigdraw list or it is cut at portal edges.
- **Door-entry root motion**: `giantrob_startthroughdoor` may move it
  forward from the spawn point into a wall; spawn it with room in front.
- **Memory**: lv14.pak is 258 KB of level heap (more than lv1_3/lv1_6/lv81);
  the state and particles add 32 KB. With the skeleton and walker also in a
  map the heap use grows accordingly; no measurement yet.
- **Particles shared**: the skeleton, the walker and the robot share
  `[0x0078B0D0]`. With the walker in the same map its beam draw also draws the
  block, so the robot's explosions may be drawn twice (brighter). After the
  robot is removed its leftover particles are not drawn (they would freeze).
- **Several robots**: each walks to the player's pad, so they bunch up; the
  door-entry animations all play at the start. The shared particle pool (see
  above) is the only shared fight state.
- **Pads**: walking to the player's pad is a simplification of the hangar
  waypoints; the attacks themselves are unchanged.
- **Split Crowe**: the split Crowe area (separate branch) also needs lv14.pak
  and scene-14 globals; both areas check the mount first. If it allocates
  the particle block too, keep one allocation (see integrator notes).

## Wrappers for the integrator

Include `pc_mapmaker_robot.inc` in `src/interface/pc_menu_mouse_guest.inc`
after `pc_mapmaker_realbosses.inc` (it is self-contained; the order only
matters for the shared `#include`s). Declare every function in
`src/interface/pc_display_menu.h`. Area name `robot`. New WRAP entries (layer
order relative to existing wrappers in parentheses):

| WRAP | function | chain |
|---|---|---|
| `sub_001C8540` | `pc_mapmaker_robot_anims` | any order with `pc_mapmaker_realbosses_anims` |
| `sub_0025F720` | `pc_mapmaker_robot_level` | **outer** of `pc_mapmaker_realbosses_level` (so its clear of `[0x0078B0D0]` runs first and a block the skeleton/walker made is reused; inner works but leaks one 32 KB block) |
| `sub_0015B830` | `pc_mapmaker_robot_ai_start` | any order with `pc_mapmaker_realbosses_ai_start` (different ids) |
| `sub_00293730` | `pc_mapmaker_robot_handler` | **new (several robots)**: robot AI handler (cdecl entity) |
| `sub_00294520` | `pc_mapmaker_robot_message` | **new (several robots)**: robot message handler (cdecl entity, ...) |
| `sub_002926B0` | `pc_mapmaker_robot_callback` | **new (several robots)**: robot slot-2 callback (cdecl entity); reached through a stored pointer, so the lifted function itself is wrapped |
| `sub_002936C0` | `pc_mapmaker_robot_damage` | **new (several robots)**: robot damage callback (cdecl hit record); same |
| `sub_0009CC40` | `pc_mapmaker_robot_nav` | new wrapper |
| `sub_000462B0` | `pc_mapmaker_robot_music` | new wrapper |
| `sub_0015D500` | `pc_mapmaker_robot_goal` | **new (v0.4.0-mapmaker.40)**: the AI walk goal (cdecl entity, pad index; 63 callers, the hook acts only for return 0x00293D27) |
| `sub_002274C0` | `pc_mapmaker_robot_event` | any order with `pc_mapmaker_realbosses_event` (different return addresses) |
| `sub_00126560` | `pc_mapmaker_robot_fx` | any order with `pc_mapmaker_realbosses_fx` |
| `sub_0025F6A0` | `pc_mapmaker_robot_reset` | new wrapper (the call is a lifted tail jump from `sub_0015E4F0`, which reaches the wrapper as a call, as `sub_0029CEF0` does) |
| `sub_0014CC80` | `pc_mapmaker_robot_hud` | **inside** `pc_mapmaker_bosses_hud` (any order with `pc_mapmaker_realbosses_hud`) |

Guest calls made (need `extern` symbols only, no table entries):
`sub_00062260`, `sub_0005A130`, `sub_00292630`, `sub_002A62C0`,
`sub_00292550`, `sub_0015DDA0`, `sub_00019D70`, `sub_002A7F30`.

Additions in `src/interface/pc_mapmaker_realbosses.inc` (not edited here),
mirroring the Crow walker's row 152:

- `#define RBS_ROBOT_ID 0x42u`, `RBS_ROBOT_MODEL 0x5F2u`, `RBS_ROBOT_ROW 153u`,
  `RBS_ROBOT_TEXT 0x52420042u`, `RBS_ROBOT_LABEL "Giant Robot"`,
  `RBS_ROBOT_STANDIN 0xA8u` (Arcade giant robot, row 90: preview stand-in).
- a label allocator `rbs_robot_label()` (as `rbs_walk_label`).
- `picker_end`: a fourth row 153 (same flags 0x20900 / callback 0x002C1C90, enabled).
- `pick`: row 153 -> id 0x42, variant -1.
- `rbs_preview_row`: 153 -> `rbs_arcade_row(RBS_ROBOT_STANDIN)`.
- `row_of`: id 0x42 -> row 153 (reload/name) or the stand-in (preview).
- `row_text`: row 153 -> `RBS_ROBOT_TEXT`; `text`: `RBS_ROBOT_TEXT` -> the label.
- `model_request` (optional, log only): model 0x5F2.
- `hud`/`meter` model lists: **no change needed** (the robot area clears the
  focus itself, and the robot has no meter record). Adding 0x5F2 there is
  harmless (it is never noted in `rbs.real`, so those hooks run the original).
- The anim/level/ai_start set scan needs no change (the robot area scans the
  set itself).

Also: `src/interface/pc_mapmaker_bigdraw.inc` `BIGDRAW_ROBOT 0x5F2u` in its
model test (whole height on screen), and the sets-names test if it lists the
synthetic rows. After merging, refresh `docs/agent-context` (the owning
guide for MapMaker areas) and regenerate the wrapper patch.

## User test steps

1. Editor: custom character set shows "Giant Robot"; picking it shows the
   Arcade giant robot in the preview; save and reload keep it.
2. Play a story map with a Giant Robot Story AI: the log shows "mounted
   lv14.pak", "giant robot state allocated", "gets its story AI (class
   0x80000)". It walks through its door-entry animation, then walks toward
   the player and fires.
3. Boss flag on: one bottom Boss bar with the AI's name; Boss Off: no health.
4. Kill it: it plays its death with explosions, then disappears ("defeated:
   removed"); the map's music keeps playing; logic objectives on marker 2 are
   not triggered.
5. Two to four robots in one map: each walks in, attacks and dies on its own
   (log: "gets its story AI (class 0x80000) and state", slot in the high
   half); killing one removes only that one; a fifth is a normal bot
   (logged). Restart the level after a death: all fight again.
6. Story mission 10 still plays stock (all hooks test scene 100).
7. v0.4.0-mapmaker.40: a Giant Robot in a tall hall (two-floor tiles or open
   air) with only a normal door to the player's room: it walks to the door
   and attacks from there, never through it (log: `reaches n of m pads`,
   `cannot fit to the player's pad`). Add a tall opening elsewhere: it walks
   round through that (`steered to the opening it fits through`).
