# MapMaker real story boss: the mansion worm (story mission 5)

Status: implemented in `src/interface/pc_mapmaker_worm.inc` (area `worm`),
fixture-tested (`tests/mapmaker/mapmaker_worm.c` +
`tests/mapmaker/test_mapmaker_worm.py`: 82 mutations, plus decode and US XBE data
checks of every site below). **Not wired, not built, not played**: the
wrappers, the picker row and the stubs are the integrator's
([Wrappers for the integrator](#wrappers-for-the-integrator)). Switch:
`TSFP_MAPMAKER_LIMITS=1`; `TSFP_MAPMAKER_REAL_BOSS=0` keeps the bot AI.

Research basis: `logs/limits/features/real-bosses.md` sections 0 and 6 (main
checkout, ignored). CONFIRMED = seen in the decode or the US XBE data (the test
names the site); INFERRED = reasoning, needs a playtest.

The research's lead was wrong: `sub_001D9730` (prop init index 24) is the
mansion's generic prop init (models 0x183..0x1CD), not the worm. The worm is
not an AI class at all; the mansion level runs it.

## The worm (CONFIRMED)

- Character id 0x2C (character table 0x0053A3CC: voice set 0x005397C8, model
  0x5EA `CHR_MANSION_MANSIONWORM`, `chrs/mansionworm` in `pak/chr.pak`, no LOD),
  skeleton group 0x47. No Arcade row. `sub_002BE270(0x2C)` = 0 (byte table
  0x002BE29C entry 1, as for the split Crowe): variant -1.
- Anims `wrmburst` 0x194, `wrmidle` 0x195, `wrmflaming` 0x196, `wrmattack`
  0x197, `wrmburrow` 0x198, `wrmhitreact` 0x199, `wrmtantrum` 0x19A,
  `wrmdeath` 0x19B, named only by `pak/stream/lv1/anim/lv1_2.pak` (string
  0x0049D7BC, registered by `sub_001CA2F0` at 0x001CAD57 from table
  0x0048BEC8, 30 names; mounted before registration: `sub_0005C420` calls
  `sub_001C8540` at 0x0005C501, `sub_001CA2F0` at 0x0005C558; also
  `sub_001C95F0`). Two of its names are also in lv1_3/lv1_6 (`jblookout`
  0x193, `jbgrossend` 0x1AE): see [Shared anim names](#shared-anim-names-crash-after-v040-mapmaker24).
- Health 12.1 (`+0x1CC`/`+0x1D8`, written at binding, 0x001D9B6E).

## How mission 5 runs it (CONFIRMED)

- **Scene descriptor** `data/mansion2.raw` (record around 0x004FDF80):
  0x004FDFC4 `sub_001D7830` (allocates the mansion script block
  `[0x0075B0C4]`, 0x50 bytes), 0x004FDFC8 `sub_001DDB50` (level init),
  0x004FDFD0 `sub_001DDC90` (level update).
- **Worm block** `[0x0075B4B4]`: 5 worms x 0x450 bytes, `sub_001D1E80`, whose
  only caller is `sub_0036C740` (scene 1 only, a tail jump).
- **Init** `sub_001D2350` (from `sub_001DDB50` at 0x001DDBBA):
  - fixed starts (-71.67, -2.4, 12.97), (-71.44, -2.4, 38.99),
    (-58.68, -2.4, 28.4), (-71.0, -2.4, 3.3), (-72.44, -2.4, 33.48);
  - worm 2 in state 8 with its own point (-62.31, ?, 16.9), the others in
    state 1;
  - per worm: position, previous position, a 64-entry trail from +0x3C,
    heading +0x1C, pad indices +0x440/+0x444 = -1 and a dirt emitter +0x43C
    (`sub_00111AE0` with the templates of `sub_001D1970`);
  - the enable flag `[0x0075B4BC]` = 0. Only the level script sets it:
    `sub_001D5E10` has no caller, only data references;
  - **mansion-only**: the six route pad ids at 0x004FDC24 (0x4FF, 0x514,
    0x515, 0x51D, 0x516, 0x500) are converted to pad indices once
    (`[0x004FDC58]`, image data);
  - **mansion-only**: level object 0x18 of the object table `[0x007B9784]`
    (built by the level geometry loader) gets flag 0x10 and hit callback
    `sub_001D1EA0` (`sub_000877B0(0x18)`, no null check).
- **Update** `sub_001DAAB0` (from `sub_001DDC90` at 0x001DDDC0, every frame):
  - 17 states at worm +0x24 (jump table 0x001DD400);
  - a worm runs only when its head is bound;
  - the head lookup `sub_001D9AF0` (0x001DABFC) calls `sub_0015BA10('WRM1' + i)`.
    That scans the AI pool for the active AI whose spawn record +8 is the tag;
  - binding clears slots 3 (the character update `sub_000D8E30`, installed for
    every character by `sub_0009E2D0(8, 3, ..)` at 0x000D9210) and 4, sets
    the hit callback `sub_001D1F30` (+0x19C), lowers the head by 5 and sets
    its health;
  - the head is only moved by the draw (`sub_001D9B80`, states >= 8).
- **States**:
  - 1: buried until a player is in the wake region (`sub_001D5D70`);
  - 4: gone. Once its anim is done the head AI is removed (`sub_0015DDA0`) and
    `[0x0078FAE0 + i]` cleared; a worm with no head is skipped;
  - 7: waits for a player near object id 0x2349 (worm 2's path);
  - 9/10/11: surfaced, can be hit.
- **Mansion coordinates** in image constants:

  | constant | value | readers |
  |---|---|---|
  | `0x0049DF08` | -2.399, the burrowing height | the update only |
  | `0x0049DF10`, `0x0049DE40` | target box x -77.2..-55.7 (the target is the nearest player, clamped) | the update; `0x0049DE40` also `sub_001D5D70` |
  | `0x0049DF0C`, `0x0047D69C` | target box z -6.0..39.5 | `0x0047D69C` is shared with 7 other functions, none reachable from the update |
  | `0x0049DE3C`/`0x0049DE40`, `0x0049DE34`/`0x0049DE38` | wake region x -79..-55.7, z -7.9..40.3 | `sub_001D5D70` only |
  | `0x0047D78C` | 27.0: a worm in state 2 south of it bursts out with `barney_worm` (+0x44C 2) | shared with 5 other functions, none reachable from the update |
  | `0x0049DDD8`/`0x0049DDD4` | worm 2's point (-62.31, 16.9) | init, update, draw `sub_001D9B80` |
  | `0x0049DE08` | -53.9, the mansion wall | draw: bodies are clipped at a screen line (`sub_001D2690`, fixed points `0x0049DE0C..0x0049DE18`) when the camera x is past it |

- **Route**: the worm patrols the six pads (next = (i + 1) mod 6, 0x001DB370..)
  and walks the pad graph (`sub_001596F0`). Only the update reads 0x004FDC24.
  A missing pad (-1) gives `sub_0009A420(-1)` = 0, and the update reads +0xC
  of 0. MapMaker pad ids are 0x3E9 + i (mapmaker-pads.md), so a map with more
  than 278 pads would turn the mansion ids into arbitrary pads.
- **Hit** `sub_001D1F30`:
  - only for the worm whose head +0x60 == `'WRM1' + i`, in states 9/10/11;
  - an explosion (type 0x15) gives state 0xC, other damage takes health, then
    0xF (dead) or 0xD (hurt);
  - kills are counted in `[[0x0075B0C4] + 0x3C]` (no null check). At 5 kills
    it clears object 0x18 and fires level marker 1 and event 4 (0x001D204F,
    0x001D2116);
  - its effect uses the mansion's effect type `[0x0078F590]`
    (`sub_00378400(sub_001D6290, 5)` in `sub_001D6420`) plus the worm index.
- **Draw** `sub_001DA990`, from the frame `sub_001CEBC0` in scene 1 only
  (0x001CEE1D/0x001CEE2C):
  - first `sub_001D9370` (mansion props, reaches the tag lookup
    `sub_0023FF20`) and `sub_001D7200`;
  - then, when enabled, per worm: `sub_001D2800`/`sub_001D5380` (buried) or
    `sub_001D9B80` (surfaced: places the head).
- **Reachability**: from the update, the level markers and tag lookups are
  reached only:
  - through the `'SFCC'` script trigger `sub_00234CD0` at 0x001DC45A
    (null-checked, runs the level-script object with that tag via
    `sub_0025F0C0`);
  - through the generic explosion `sub_000BB6C0` (68 callers), which only
    reads a marker (`sub_002273C0`).

  No `sub_002274C0`, `sub_002275F0` or `sub_0023FF20` remains without the
  trigger (test, direct calls). Indirect calls are not covered.

## What the area does (scene 100 story, `TSFP_MAPMAKER_LIMITS=1`)

Up to 4 map worms, each with its own worm block: in each block worm 0 is
that map worm, worms 1-4 are parked ([Several mansion worms](#several-mansion-worms)).

| hook | what |
|---|---|
| `sub_001C8540` -> `pc_mapmaker_worm_anims` | mounts lv1_2.pak when the map's set holds id 0x2C (not into a full mount table, not twice) |
| `sub_0025F720` -> `pc_mapmaker_worm_level` | after the original, see the list below |
| `sub_0015B830` -> `pc_mapmaker_worm_ai_start` | character 0x2C or model 0x5EA with the pak and this level's block: noted (one per map; extras keep the bot AI). The record keeps the MapMaker class |
| `sub_000D8E30` -> `pc_mapmaker_worm_tick` | the noted worm's character update: see the list below |
| `sub_0015BA10` -> `pc_mapmaker_worm_tag` | return 0x001D9AFD only: `'WRM1'` -> this map's worm, any other tag -> 0 |
| `sub_001D1F30` -> `pc_mapmaker_worm_hit` | for the worm's head: during the original only, `[0x0075B0C4]` = scratch, `[0x007B9784]` = fake table, `[0x0078F590]` = this level's effect type, head +0x60 = `'WRM1'`; the kill counter is reset before and after, so the 5-kill marker never fires |
| `sub_00126560` -> `pc_mapmaker_worm_fx` | return 0x001CEE8C (the frame), once placed: the stock `sub_001DA990` with worm 2's point shifted and the wall at +FLT_MAX (no clip) |
| `sub_001D9370` -> `pc_mapmaker_worm_props` | return 0x001DA998 during this area's draw: skipped |
| `sub_001D7200` -> `pc_mapmaker_worm_extra` | return 0x001DA9A9 during this area's draw: skipped |
| `sub_00234CD0` -> `pc_mapmaker_worm_script` | return 0x001DC45F in scene 100: the mansion script trigger dropped |

**Level init** (`pc_mapmaker_worm_level`), in order:
1. Allocates the worm block (`sub_001D1E80`).
2. Allocates a 0x300 scratch block (`sub_0003DFA0(0x300, 1)`, zeroed): +0 the
   script block stand-in, +0x50 the fake object table, +0x100 a zeroed object
   0x18.
3. Registers the effect type (`sub_00378400(0x001D6290, 5)`, deduplicated by
   the callee).
4. Runs the stock init with `[0x007B9784]` = the fake table and
   `[0x004FDC58]` = 0 (no pad conversion), then restores both.
5. Parks worms 1-4 (state 4, emitters released with `sub_0010E2A0`) and keeps
   the enable flag at 0.

**First tick** (`pc_mapmaker_worm_tick`, before the stock update):
- worm 0 (position, previous, trail) at the head's spawn point;
- the offsets: dx/dz put the target box centre (-66.45, 16.75) on the spawn,
  dy puts -2.4 at the spawn floor;
- six route pads (below);
- enable.

**Every tick**: the update constants are shifted (a constant without its stock
bits is left alone) and the six pads swapped into 0x004FDC24 for the stock
update only, then restored. After a bind it puts slot 3 back to
`sub_000D8E30`, so the tick keeps coming; it doesn't once the worm is gone.
The character think never runs, as in stock. With no usable spawn point or no
pads, the head keeps the bot behaviour.

**Route pads**:
- Only valid regular pads count (`[0x00715F2C] + i*0x20`, flag bit 0, and
  `sub_0009A420(i)` agrees).
- They must be inside the target box around the spawn (+-10.75 x, +-22.75 z)
  and within 4 units of the spawn height.
- Nearest first, at least 3 units apart, up to six, in angle order around the
  spawn.
- Fewer than six repeat. With none, the head's own pad (actor +0x678) is used.

Logs (stderr, `[mapmaker] worm:`):
- `mounted lv1_2.pak for the mansion worm`
- `mansion worm state allocated`
- `mansion worm noted; it binds on its first frame`
- `mansion garden moved by (dx, dz), floor y (mission 5: -2.4), n route pads`
- `mansion worm runs its story update`
- `mansion worm head bound`
- `mansion worm drawn`
- `surfaced, state s, head height h, spawn floor f, worm height w` (each
  surfacing, up to 8)
- `lv1_2.pak: only the mansion worm's anims registered`
- `mansion worm: the mansion script trigger dropped`
- `mansion worm hit (state before, after)`
- failures: `no pads near the mansion worm's spawn; bot behaviour`,
  `more than 4 mansion worms in the map`, and so on

## Risks (INFERRED, need a playtest)

- **Ground height** (CONFIRMED, test `test_heights`):
  - -2.4 is the mission 5 garden floor: it is by far the most common height
    of the level geometry (`data/mansion2.raw`'s geometry file in
    `story/l_1_st.pak`) inside the worms' region.
  - The worm's height (the burrowing constant and the starts) is that floor,
    and the surfaced head is placed at it (`sub_001D9B80`); nothing probes
    collision. The area puts it at the spawn floor (dy = spawn y + 2.4).
  - The one fixed height: after a state-2 burst (+0x44C 2) stock puts the
    head at (-62.31, **0**, 16.9) (0x001DA94C..0x001DA969). x and z follow
    the shifted constants; after each draw the area adds dy to that y 0, so
    it stays 2.4 above the floor, as in mission 5.
  - The mapmaker.25 playtest ("floats above the ground, no proper animations
    or logic") had the trophy monster and the skeleton in the set, so the
    stopgap skipped lv1_2.pak and the worm ran as a MapMaker bot on its rig.
    The registration fix gives it the story behaviour in that set too.
  - Logs: `mansion garden moved by (dx, dz), floor y (mission 5: -2.4, the
    garden floor)` and, at each surfacing (up to 8), `surfaced, state s,
    head height h, spawn floor f, worm height w`. h should be about f (or
    f + 2.4 after a state-2 burst).
- **Space**: the worm only targets the nearest player clamped to a box of 21.5
  x 45.5 world units centred on the spawn (long axis along z), and wakes when
  a player enters a region of the same size. It burrows through anything; it
  has no wall test of its own. Build an open floor around the spawn, longer in
  z than in x.
- **Route**: it patrols pads the area picked (logged count). On a map with
  few or sparse pads it may circle one pad.
- **Head and AI**: the head is a MapMaker Story AI whose think stops once
  bound, as in stock (slot 3). Between spawn and binding (the first frame)
  nothing runs. MapMaker map logic that waits for this AI's normal death may
  not fire: the worm dies through its own states and removes the AI itself
  (`sub_0015DDA0`).
- **Hit**: only while surfaced (states 9-11). An explosion kills it outright
  (state 0xC). Bullets take its 12.1 health.
- **Boss bar**: no stock bar; the bosses area's Boss flag bar should show it
  (health +0x1CC/+0x1D8). Not checked.
- **Draw**: the stock wall clip is off; other draw state (`sub_001D2690`'s
  screen line) is unused then. Body rendering on MapMaker tiles is unverified.
- **Shared constant**: `0x0047D69C` (-6.0) is shifted during the update call;
  no other reader is reachable from it (direct calls; indirect calls not
  covered).
- **Mansion afterwards**: the area restores every image word it changes
  (constants, pad table, conversion flag, object table pointer). Mission 5
  later in the same session should be stock (untested).
- Untested: worm 2's scripted state 8 path (parked); the `0x2349` object
  (state 7, parked worms only).

## Several mansion worms

User request: every placed mansion worm should have its story behaviour.

**Why not use the stock worms 1-4 of one block**: the stock update runs
all five worms in one call, with one set of mission 5 coordinates. Map
worms at different spawn points need their own offsets, so each map worm
gets its own block.

**What one block's code keeps globally** (CONFIRMED, test
`test_multi_instance_globals`; written operands of the code reachable from
the update, draw, hit callback, init and allocation):
- the block `[0x0075B4B4]`, its init flag `[0x0075B4B8]`, the enable flag
  `[0x0075B4BC]`, the "worms active" flag `[0x004FDC20]` and the five bound
  heads `[0x0078FAE0..0x0078FAF0]`: 9 words, swapped per worm;
- `[0x004FDC58]` (the pad conversion flag): kept 0 by the area during each
  init and restored, as before;
- the rest it writes is the body-geometry tables of `sub_001D1BD0` (the same
  values every init) and the draw's scratch (`sub_001D2800`, rewritten every
  draw): shared, not swapped.

**What the area does**:
- Up to 4 map worms (`WRM_MAX`), each with its own block (`wrm_alloc`: the
  stock allocation and init with the stand-ins, worms 1-4 parked) and its own
  copy of the 9 words.
- Allocation: the first block at level init, each further one at that worm's
  AI start. The globals as they were are put back after each allocation.
- `wrm_enter(k)` / `wrm_leave(prev)` swap the copies around every entry into
  that worm's code: its tick (the stock update, with its own constants
  offsets and route pads), its hit callback, and its draw (the fx hook draws
  each worm in turn).
- The head lookup inside an update finds that worm's own head.
- The scratch block (script block and object stand-ins) and the effect type
  are shared by every worm. The kill counter is reset around every hit, so
  it never reaches 5.
- The fifth and later mansion worms keep the bot AI (logged).



Unchanged: a stock custom-set member (word id 0x2C, sbyte variant -1). No
Story AI chunk byte of its own (+0x29 is the bosses area's Boss flag). A stock
game or an older fork build plays id 0x2C with the MapMaker bot class on the
worm rig (no anims). Mission 5 holds the same id.

## Shared anim names (crash after v0.4.0-mapmaker.24)

**User report**: mapmaker.24 crashed at every level load when a set held the
giant trophy monster, the giant skeleton and the worm.

**Crash site** (crash dump; CONFIRMED): guest return 0x0005C562 is
`sub_00350EC0`, called right after the anim name registration `sub_001CA2F0`
at 0x0005C558.

**Cause** (CONFIRMED, decode and registration tables; test
`test_lv1_paks_share_anim_ids`, `test_registration_sites_and_fixup`):
- `sub_0005B980(prefix, table, count)` registers a mounted pak's names one by
  one through `sub_00057F30(path, id, 0)`. That finds the anim file in the
  mounted paks, maps the id (`0x007E03C0`) and **fixes the file up in place**:
  its offsets become pointers (+0x1C, +0x28, +0x2C at 0x0005805F..).
- lv1_2.pak's table (0x0048BEC8, 30 entries) names two Jo-Beth anims that
  other boss paks also name: `jblookout` 0x193 (lv1_3.pak, 0x0048C1B0, the
  trophy monster) and `jbgrossend` 0x1AE (lv1_6.pak, 0x0048C738, the
  skeleton). With both paks mounted, the same anim is fixed up twice and its
  pointers become junk (eax 0xFFC00000 in the dump).
- No other id or name is shared. lv1_3 and lv1_6 share nothing, and the
  worm's names are unique among lv1_2, lv1_3, lv1_6, lv81 and lv14 (test).
- Mission 5 never mounts these paks together.

**Fix**: in scene 100, `pc_mapmaker_worm_register` (on `sub_0005B980`)
replaces lv1_2's whole table with the worm's own entries:
- `barney_worm` 0x149 (entry 5), registered by a nested call;
- `wrmburst` 0x194 .. `wrmdeath` 0x19B (entries 21..28), registered by the
  original with the table and count arguments changed for the call and then
  restored.

The rest of the mansion cast (and the two shared Jo-Beth names) is not
registered; MapMaker never uses them. The other bosses' paks keep their stock
tables and ids. No anim id is remapped: the worm code requests 0x149 and
0x194..0x19B, and none of these is registered by any other pak.

`barney_worm` is needed: the update plays it on the head in state 9 when
+0x44C == 2 (0x001DBF46). +0x44C becomes 2 when a worm in state 2 is south
of z 27.0 (0x001DB887..0x001DB8A4; `0x0047D78C`, shifted by dz with the
other constants).

**Fallback**: the stopgap of mapmaker.25 is kept for when
`pc_mapmaker_worm_register` is off (`pc_mapmaker_area_enabled`). In that
case the mount hook skips lv1_2.pak if lv1_3.pak or lv1_6.pak is mounted,
and the worm keeps the bot behaviour (logged).

## Wrappers for the integrator

New area `worm` (add it where the areas are listed, e.g. the
`pc_mapmaker_area_enabled` table). Include `pc_mapmaker_worm.inc` after
`pc_mapmaker_realbosses.inc` (it needs only `pc_mapmaker_limits.inc` and the
includer's guest helpers; its statics are prefixed `wrm`/`WRM_`).

```
WRAP sub_001C8540 -> pc_mapmaker_worm_anims      (layer with realbosses/crowe/... anims; either order)
WRAP sub_0025F720 -> pc_mapmaker_worm_level      (layer with realbosses/crowe/... level; either order)
WRAP sub_0015B830 -> pc_mapmaker_worm_ai_start   (layer with realbosses/crowe/... ai_start; either order)
WRAP sub_000D8E30 -> pc_mapmaker_worm_tick       (new; HOT: every character every frame, tests wrm_lv.record first)
WRAP sub_0015BA10 -> pc_mapmaker_worm_tag        (new; return-address test first)
WRAP sub_001D1F30 -> pc_mapmaker_worm_hit        (new; only installed on worm heads)
WRAP sub_00126560 -> pc_mapmaker_worm_fx         (layer with realbosses_fx/crowe_fx; per frame)
WRAP sub_001D9370 -> pc_mapmaker_worm_props      (new; return-address test)
WRAP sub_001D7200 -> pc_mapmaker_worm_extra      (new; return-address test)
WRAP sub_00234CD0 -> pc_mapmaker_worm_script     (new; return-address test; 5 callers)
WRAP sub_0005B980 -> pc_mapmaker_worm_register   (new; level load only; table 0x0048BEC8 count 0x1E test first)
(several worms: no new wrapper; the same hooks swap each worm's globals)
```

Each layer: `if(pc_mapmaker_area_enabled("worm","pc_mapmaker_worm_<x>"))
pc_mapmaker_worm_<x>(inner); else inner();`. The worm hooks act only on their
own record and return addresses, so layer order does not matter.
`sub_000D8E30` is reached both by direct calls and through entity slot 3. The
tick relies on slot 3 (the guest address 0x000D8E30) dispatching to the
wrapper, as for any wrapped guest function.

Guest calls (`extern void ...(void)`, called with `guest_call`):
`sub_0005A130(path)`, `sub_00062260(path)`, `sub_001D1E80()`,
`sub_0003DFA0(size, 1)`, `sub_00378400(fn, n)`, `sub_001D2350()`,
`sub_0010E2A0(emitter)`, `sub_001DAAB0()`, `sub_001DA990()`,
`sub_0009A420(pad)`, `sub_0005B980(prefix, table, count)` (new, by
`pc_mapmaker_worm_register`; a guest function the build already has).

Link stubs to add in `tests/mapmaker/mapmaker_area_stubs.h` (new):
`sub_001D1E80`, `sub_001D2350`, `sub_001DAAB0`, `sub_001DA990`,
`sub_00378400`, `sub_0010E2A0`, `sub_0009A420`, plus `sub_0003DFA0` if not
already defined there. Off-build no-op hooks (`{ original(); }`) in
`pc_menu_mouse_guest.inc` and prototypes in `pc_display_menu.h` for the ten
functions above. Owning agent-context guide: add the area like crowe (not
edited here, per the brief).

Addition to `pc_mapmaker_realbosses.inc` (`rbs_extra`, editor side; the play
side needs none of it), with `RBS_EXTRA_N` 4:

```c
{157u,0x2Cu,0x5242002Cu,0x6Bu,"Mansion Worm"},  /* preview: Arcade mansion zombie nurse */
```

- **Row**: 157.
- **Pick**: id 0x2C, variant -1 (`sub_002BE270(0x2C)` = 0).
- **Text**: 0x5242002C.
- **Stand-in**: the Arcade mansion zombie nurse, id 0x6B, Arcade row 61
  (`chrs/mansionzombienurse`). Any Arcade character works; this one is from
  the same level.

Optional: add 0x5EA (`chrs/mansionworm`) to the bigdraw model list if it turns
out to be culled early (INFERRED long body). The worm needs no hud or meter
entries.

## User test steps

1. `TSFP_MAPMAKER_LIMITS=1`. Editor -> custom set: pick **Mansion Worm** (the
   preview shows the zombie nurse). Place one Story AI with that slot in a
   large open flat area, about 22 x 46 units (longer in one direction) with
   AI pads around it. Save, reload: kept.
2. Play in Story. stderr should show:
   - `mounted lv1_2.pak for the mansion worm`
   - `mansion worm state allocated`
   - `noted; it binds on its first frame`
   - `mansion garden moved by (...) ... n route pads` (n > 0)
   - `runs its story update`
   - `head bound`
   - `drawn`
3. Watch: nothing visible until you walk near; then a dirt trail burrows
   towards you, the worm bursts out (`wrmburst`), attacks and burrows again.
   Its height relative to the floor is the main check.
4. Shoot it while it is out (`hit` lines with states 9-11 -> 0xD), or use an
   explosive (-> 0xC). When it dies (`wrmdeath`) it sinks and the Story AI is
   removed. No map logic marker changes.
5. Boss flag Boss bar: a bar while it is alive. Up to four Mansion Worm AIs
   each burrow and attack around their own spawn; a fifth fights as a bot
   (`more than 4 mansion worms in the map`). Mission 5 still plays
   stock.
6. Report:
   - where it surfaces relative to the floor and the spawn;
   - whether it goes through walls;
   - the logged pad count;
   - the stderr lines.

## Block made at the AI start (v0.4.0-mapmaker.27)

The worm's block (`sub_001D1E80`, then the stock init `sub_001D2350`) is no
longer made in the level hook. That hook runs inside `sub_001CDB80`, before
`sub_001CDE30` builds the level's particle emitter pool (`sub_000FF1B0` ->
`sub_001121B0`, free list rebuilt by `sub_000FEEA0`). The stock init takes
emitters (`sub_00111AE0` -> `sub_0010DF90`) and found the last level's free
list in freed memory, which lv1_2's data now overwrites: every level whose set
holds the worm crashed in .26 (three dumps, `sub_0010DF90+0xB5` from
`0x001D2604`). The level hook now only makes the shared scratch and effect;
each worm's block is made at its AI start (`wrm_alloc(k)`), in play, and a
failed scratch leaves the worm a bot. Confirmed by the user: three split
Crowes loaded once the worm was out of the set.
