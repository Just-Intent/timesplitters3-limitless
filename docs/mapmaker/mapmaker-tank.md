# MapMaker castle tank (story mission 2 boss)

Status: implemented in `src/interface/pc_mapmaker_tank.inc` (area `tank`),
fixture-tested (`tests/mapmaker/test_mapmaker_tank.py`: 78 mutations, plus
decode, US XBE data and game-pak checks of every site below). **Not played
yet**, not wired: the wrappers are listed for the integrator
[below](#wrappers-for-the-integrator). No guest-code table entry, no new save
bytes. Switch: `TSFP_MAPMAKER_LIMITS=1`; `TSFP_MAPMAKER_TANK=0` turns only
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
| state | static `0x0078B560..0x0078B5F4`: state `[0x0078B594]` 0..10, previous `[0x0078B598]`, drive target `[0x0078B580..88]`, target entity `[0x0078B5B0]`, five gun records `[0x0078B560]` = 0x0051B9C8 (0x4C each, aim point +0x10) (CONFIRMED) -> **one tank per map** |
| scene tests | none in the tank's code (CONFIRMED) |

The fight (CONFIRMED from `sub_00255550` and its jump tables 0x00255BDC/
0x00255BE8/0x00255BF4; meanings INFERRED): 0 wait for the start test, 1
drive in, 3 chase and fire, 4 gun attack, 5 no target, 6 player in a gate
area, 7 player below the courtyard, 8 tank in a gate area, 9 stunned by a
heavy hit (12 s; a second bar shows the time), 10 dead.

## Castle assumptions and what this area does about them

| castle assumption (CONFIRMED site) | in a MapMaker map | this area |
|---|---|---|
| start test `sub_00253140`: the castle object `'RDPC'` (`sub_000B2350`) must exist and every living player be in state 0x15, then level marker 5 (`sub_00227650(5, 3.0)`) | `'RDPC'` is not found: the fight would never start; marker 5 is the map's own logic marker | **wake**: starts when a living player is within 30 m (x/z) of the tank; no marker |
| three gate rectangles 0x0051BB58.. (`sub_00251CD0`): player inside -> state 6, tank inside -> state 8 (drive to the castle point (-9.7, -0.17, 4.3)), random targets inside rejected | no castle gate | **area**: never inside |
| drive target `sub_00254690`: player clamped to the arena x -19.1..-5, z -10..13; state 7 target from gun 4's aim point, y 0.17 | the map is elsewhere | **target**: run in the castle frame (below) |
| state 7 entry `sub_00255390`: castle aim points (-5.3, -1.4, 19.9) or (-13, -2.2, 25) into all five guns | same | **state**: castle frame, aim points moved by +d after |
| first drive target (-9, own y, -3.2) set by `sub_00251AA0` | same | moved by +d at spawn |
| death `sub_00255C20` parks the dead tank at (0, 20, 0) | a point in the open map | **wreck**: parked 50 m under its spawn point |
| state 9 fires level event 0x16 every frame (`sub_002274C0` at 0x00255A8F) | the map's own logic marker 0x16 | **event**: dropped |
| boss bar labels 0x165B / 0x1635 (`sub_0014CC80` case 0x281) | the MapMaker level's own text ids | **text**: "Castle Tank" / "Tank exposed" at those two call sites |
| tagged objects `'TKDR'` (door, 0x00255767) and `'TCV5'..'TCV7'` (`sub_00251DD0`) | absent | none needed: both null-checked by stock code (CONFIRMED) |

**Castle frame.** d = spawn - (-12.05, 0, 1.5), the arena's centre (x
-19.1..-5, z -10..13, CONFIRMED) with the courtyard floor INFERRED at y 0
(the castle's fixed targets sit at y -0.17 and 0.17; "below the courtyard"
is y < -1). For the call, the tank and its target entity are moved by -d
and restored exactly afterwards; the drive target goes in and out in castle
coordinates (left bit for bit when the call picks none). `sub_00254690`
calls only `sub_00153B40` (random) and `sub_00251CD0` (CONFIRMED), so no
other code sees the moved positions. The tank therefore roams a 14 x 23 m
box centred on its spawn point (x +-7.05, z +-11.5).

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
  for a second placeholder in the same map: a guard robot bot (logged).
- The Story AI's own settings (health, Boss flag, logic) do not apply to the
  tank: it is a prop with its own health and damage rules, and its defeat is
  not seen by MapMaker logic (no level event, no AI death).

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
2. **ai_start** (`sub_0015B830`): after the original, the first pool entry
   with character (a+0x48) 0x12, when the pak is mounted and the resource map
   `0x0078B860` has 0x281 (row +0x2C == `sub_002560C0`), 0x297 and 0x298:
   `sub_000A6F90(0x281, &pos, &rot, 0, 0)` with pos = the placeholder's
   +0x64..+0x6C and rot = (0, its +0x70, 0). Layout (CONFIRMED):
   `sub_000A6F90` -> `sub_000A6EC0(type, model, pos, rot, a, b)` ->
   `sub_000A6880` copies pos to +0x64/+0x68/+0x6C and rot to +0xC0/+0x70/
   +0xC8, returns 0 when the model does not load (0x000A695C); the init
   runs at 0x000A6F3A. Labels are allocated here.
3. **update** (`sub_00255550`, the tank's frame): on its first frame the
   placeholder AI is removed with `sub_0015DDA0` (the Crow walker's removal
   call) after checking the pool entry still holds that AI; a story focus
   on it is cleared.
4. **wake**, **area**, **target**, **state**, **wreck**, **event**,
   **text**: as in the table above.

## Wrappers for the integrator

All in `src/interface/pc_mapmaker_tank.inc`; every hook runs the original
unchanged when the switch is off or outside scene 100.

| wrapper | function | chaining |
|---|---|---|
| `WRAP sub_00062790 -> pc_mapmaker_tank_mount` | level pak mounts | new |
| `WRAP sub_0015B830 -> pc_mapmaker_tank_ai_start` | AI start (cdecl pool entry) | **shared** with `pc_mapmaker_realbosses_ai_start`; order free (each acts only on its own ids; the tank's runs its work after the original) |
| `WRAP sub_00255550 -> pc_mapmaker_tank_update` | tank frame (cdecl object) | new |
| `WRAP sub_00253140 -> pc_mapmaker_tank_wake` | fight start test (no args, eax) | new |
| `WRAP sub_00251CD0 -> pc_mapmaker_tank_area` | gate test (ecx = point, eax) | new |
| `WRAP sub_00254690 -> pc_mapmaker_tank_target` | drive target (edi = tank) | new; must see the guest `g_edi` |
| `WRAP sub_00255390 -> pc_mapmaker_tank_state` | state entry (eax = tank) | new; must see the guest `g_eax` at entry |
| `WRAP sub_00255C20 -> pc_mapmaker_tank_wreck` | death (esi = tank) | new |
| `WRAP sub_002274C0 -> pc_mapmaker_tank_event` | level event (cdecl) | **shared** with `pc_mapmaker_realbosses_event` (and other boss ports); order free: each matches only its own return addresses |
| `WRAP sub_0003D7B0 -> pc_mapmaker_tank_text` | text lookup (cdecl), hot | **shared** with `pc_mapmaker_realbosses_text` (and others); order free: ids 0x165B/0x1635 at their return addresses only |

Guest functions called: `sub_00062260` (is a pak mounted), `sub_000622D0`
(mount), `sub_000A6F90` (create the tank), `sub_0015DDA0` (remove the
placeholder AI). Host: `xbox_HeapAlloc` (labels, 32-byte argument buffer,
once per session). The area stubs (`tests/mapmaker/mapmaker_area_stubs.h`)
need `sub_000622D0` and `sub_000A6F90` when the include chain links this
area (`sub_00062260` and `sub_0015DDA0` are already there).

Also for the integrator: the editor picker row (above), and the
`docs/agent-context` guide entry for this area.

## Risks (not verified in game)

- Not played: the whole area is fixture- and decode-verified only.
- Collision and driving of a 0x8000000 prop on MapMaker tile floors
  (`sub_002548F0` drives by its own position; INFERRED it follows the floor
  through stock object physics).
- Textures of the tank not checked separately from the .xbr.
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
   ground-floor spot: in story play the tank appears there (log: `mounted
   prop6.pak`, `castle tank created`, `spawn (...)`, `placeholder AI
   removed`).
2. Walk within 30 m: it starts (log `fight started`), drives and fires;
   the bottom bar reads "Castle Tank".
3. Explosives hurt it; a heavy hit shows "Tank exposed" for 12 s
   (log `level event dropped`); map logic markers are unaffected.
4. Destroy it: wreck and flying turret appear, the tank vanishes
   (log `destroyed`).
5. Second placeholder in the same map stays a guard robot; `TSFP_MAPMAKER_TANK=0`
   gives a guard robot; story mission 2 still plays stock.
