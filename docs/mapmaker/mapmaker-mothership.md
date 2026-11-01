# MapMaker Timesplitter mothership (story mission 1 boss)

Status: implemented in `src/interface/pc_mapmaker_mothership.inc` (area
`mothership`), fixture-tested (`tests/mapmaker/mapmaker_mothership.c` +
`tests/mapmaker/test_mapmaker_mothership.py`: mutations, plus decode, US XBE
data and game-pak checks of every site below). **Not wired, not built, not
played**: the wrappers, the picker row and the stubs are the integrator's
([Wrappers for the integrator](#wrappers-for-the-integrator)). Switch:
`TSFP_MAPMAKER_LIMITS=1`; `TSFP_MAPMAKER_MOTHERSHIP=0` keeps the placeholder
a normal bot. No guest-code table, no new save bytes.

Why this boss: the survey ([mapmaker-boss-survey.md](mapmaker-boss-survey.md))
found it and the catacombs pit-fight skeleton as the only story bosses not yet
ported or in progress. The mothership has no animations, no per-level state
block and no level event, and it doesn't share hooks with another area.
CONFIRMED = decode, XBE data or pak index (the test names the site);
INFERRED = reasoning, needs a playtest.

## What the mothership is (CONFIRMED)

| | value |
|---|---|
| object | prop 0x539 `INTRO_TIMESPLITTERS_MOTHERSHIP` (`intro/intro_mothership`, type 1, no animation group), init index 20 = `sub_0021A710`, case 0x0021AADA |
| init | update `sub_00217050` in slot 3 (0x0021AB03), damage callback `sub_002193A0` (0x0021AB74), slot 10 `sub_00215B50`, health 250 into +0x1CC/+0x1D8 (0x0047D80C), state +0x1A0 = 0 (0x0021AB19); no scene test |
| wreck | prop 0x53A `intro/intro_mscrash`, made in state 5 (`sub_000A6F90` 0x002185B2), then the ship is removed (`sub_000A9070` 0x002185B8) |
| models | not in `chr.pak`; only in `story/l_2_st.pak`, `story/l_26_st.pak` and `challenge/l_64_ch.pak` (184,108 + 128,884 bytes) |
| bar | `sub_0014CC80` case 0x539 pushes text 0x164B (0x0014CDDD) and jumps to the shared `sub_0003D7B0` call at 0x0014D09A (return 0x0014D09F); the focus is written by the ship every frame in state 2 (0x00217947) |

States (`sub_00217050`, +0x1A0, table 0x00218630):

| state | what it does |
|---|---|
| 0 | flies to the intro's fixed point (-500, 50, -220) (0x0049EA9C, 0x0049EA98) |
| 1 | hovers |
| 2 | attack: focus of every player while health > 0; on a timer a homing shot `sub_000EA9C0(0x32, ..)` at a random player inside the box x -67..-56 (0x0049EA90/0x0049EA94), z -88.04..-47.8 (0x0049EA88/0x0049EA8C), player struct +0xA8/+0xB0 |
| 3 | set by the damage handler at 0 health (0x00219692, focus cleared at 0x00219683): falls without lift until its height is below 6.6 (0x0049E324, 0x00217DCE), then 4 |
| 4 | ten explosions, dust at height -45.5 (0x0049EA84, 0x00218075), then 5 |
| 5 | camera shake, focus to the actor 'ZCTP' only when it exists (0x0021856E), wreck, remove |

In states other than 0 the ship's lift balances gravity and its velocity is
damped (0x00217478..0x002176FE), so it hovers where it is. The intro's level
script starts the attack with logic action 0x5B `sub_00217030`, which only
sets +0x1A0 = 2 on the object tagged 'MTHS'. Death calls
`sub_00234CD0('MTHS')` (a logic item, null-safe). No level event
(`sub_002274C0`) is reachable from the init, update, damage or slot-10
callbacks (static call graph, test).

Each rebased constant (0x0049EA84..0x0049EA94) is read by exactly one ship
instruction. The crash floor 0x0049E324 (6.6) is also read by other code, so
it is left alone.

## The pak

`challenge/l_64_ch.pak` (7.4 MB) is mounted kept open (`sub_000622D0(path, 0,
1)`, as chr.pak and the tank's prop6.pak): only its index stays in memory and
the two models are read during the level load. A whole-level pak could
shadow MapMaker's files, because lookups first try the two paks that answered
last (0x000623C7/0x000623CD) and then the mount table in order. It doesn't:
every one of its files that also exists in another pak (239 of them in
`arcade/l_100.pak`) is byte-identical there (checked against every pak,
`test_models_and_pak`). Its other 59 files are the challenge level's own data,
which a MapMaker map never asks for. The path is built by the game from
`pak/%s%s/l_%d%s.pak` (0x0047874C), so the area puts its own string on the
heap.

## Placement and save format

A Story AI whose character is the placeholder id **0x75**
(`CHR_FARFUTURE_GUARDROBOT`, model 0x65A `chrs/guardrobot` in chr.pak; no
Arcade row; in no story set). The map stores it as a stock custom-set member,
and a stock or older build plays it as a guard robot. The ship appears
**30 m above** the placeholder (INFERRED height; the intro's ship flies at
y 50), facing its yaw, already attacking.

## What each hook does (scene 100, story mode, switch on)

| hook | behaviour |
|---|---|
| mount (`sub_00062790`) | after the original: forget the last level's ship and put the rebased constants back to stock (saved once); when the map's own custom set (slots 1-20) holds 0x75, or the container can't be read: mount l_64_ch.pak, keeping 8 table slots free |
| request (`sub_0015E7C0`) | after the original: request 0x539 and 0x53A (`sub_000A5E40`) so the load pass reads them |
| ai_start (`sub_0015B830`) | after the original, first placeholder only, when both models are ready (resource rows with init 0x0021A710, read requests): create the ship (`sub_000A6F90(0x539, &pos, &rot, 0, 0)`), set state 2 (as `sub_00217030`), firing box = spawn ± 60 m (x/z), dust height = the placeholder's floor. A second placeholder stays a bot (logged) |
| update (`sub_00217050`) | our ship only: first frame removes the placeholder AI (`sub_0015DDA0`, focus cleared if it was on it); in states 3 and 4 the ship's height is moved down by the floor for the call and back, so the fall ends 6.6 m above the map floor |
| text (`sub_0003D7B0`) | id 0x164B at return 0x0014D09F, with a ship this level: "Timesplitter Mothership" |

Log lines (`[mapmaker] mothership: ...`): `mounted l_64_ch.pak for the
mothership`, `requested the mothership and wreck models`, `mothership created
above the placeholder`, `spawn (x, y, z), floor f, firing box 60 m around it`,
`mothership placeholder AI removed`, `mothership shot down: falling to the
map floor`.

## Wrappers for the integrator

| wrapper | function | chaining |
|---|---|---|
| `WRAP sub_00062790 -> pc_mapmaker_mothership_mount` | level pak mounts (no args) | **shared** with `pc_mapmaker_tank_mount`; order free (each mounts its own pak after the original) |
| `WRAP sub_0015E7C0 -> pc_mapmaker_mothership_request` | story level init (no args) | **shared** with `pc_mapmaker_tank_request`; order free (both work after the original) |
| `WRAP sub_0015B830 -> pc_mapmaker_mothership_ai_start` | AI start (cdecl pool entry) | **shared** with realbosses, robot, crowe, tank; order free (acts only on id 0x75, after the original) |
| `WRAP sub_00217050 -> pc_mapmaker_mothership_update` | ship frame (cdecl object; called through the slot-3 pointer) | new. The native-high-refresh numeric policy (`src/gameplay/pc_native_numeric.h`) patches instructions inside this function, not its entry, so the two don't conflict |
| `WRAP sub_0003D7B0 -> pc_mapmaker_mothership_text` | text lookup (cdecl), hot | **shared** with realbosses and tank text hooks; order free (id 0x164B at return 0x0014D09F only) |

Guest functions called: `sub_00062260`, `sub_000622D0`, `sub_000A5E40`,
`sub_000631C0`, `sub_000A6F90`, `sub_0015DDA0` (all already used by the tank
area, so the area stubs exist). Host: `xbox_HeapAlloc` (pak path, label,
32-byte argument buffer; once per session).

**Picker row** (in `pc_mapmaker_realbosses.inc`, `rbs_extra`):
`{158u, 0x75u, 0x52420075u, 0x4Eu, "Mothership"}`. Row 158, id 0x75, text
0x52420075, preview stand-in the Arcade Timesplitter (id 0x4E, Arcade row 93),
label "Mothership" (the bar says "Timesplitter Mothership"; `rbs_extra`
labels hold 15 characters). Check that no other session took id 0x75 or
row 158; the Khallos chair and worm ports were not visible from here.

Also for the integrator: the `docs/agent-context` guide entry for this area.

## Risks (INFERRED, need a playtest)

- Size and height: the intro ship is cutscene scale. 30 m may be too low
  (clipping into tall maps) or too high for draw distance and fog.
- Firing: shots target players within 60 m (x/z) of the spawn; the homing
  shot's damage and speed are the intro's.
- Hover drift: random wobble moves it slowly; there is no leash back to the
  spawn.
- Crash: the fall ends 6.6 m above the placeholder's floor. A map whose floor
  under the ship is lower or higher than the placeholder's will look off. The
  sound and explosions during the fall are placed about one floor height low
  (the shift).
- Health 250 with no boss-row scaling (the ship isn't a Story AI, so the
  bosses area's multipliers don't apply).
- Story restart (`sub_0025F6A0`) was not examined for the ship.
- Textures: assumed embedded in the .xbr (not checked).
