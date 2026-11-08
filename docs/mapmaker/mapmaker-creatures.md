# MapMaker story creatures: one deployer, one row per creature

Status 2026-11-07: implemented in `src/interface/pc_mapmaker_creatures.inc`
(area `creatures`) with its table `src/interface/pc_mapmaker_creatures.def`,
fixture-tested (`tests/mapmaker/mapmaker_creatures.c` +
`tests/mapmaker/test_mapmaker_creatures.py`: 26 mutations, plus decode, US XBE
data and story-pak checks of every row). **Not wired, not built, not
played**: the wrappers are the integrator's (list below). Switch:
`TSFP_MAPMAKER_LIMITS=1`; `TSFP_MAPMAKER_CREATURES=0` keeps every creature on
the MapMaker bot AI. No guest-code table, no fork chunk, no save change.
Story assets wave 2, items 6, 9, 10 and 15
([inventory](mapmaker-story-assets.md)).

CONFIRMED = decode, XBE data or story pak (the test names the site);
INFERRED = reasoning, needs a playtest.

## What the player gets

Three more rows at the end of the custom character set's picker (after the
ported bosses' 150..160):

| row | label | stores | made with | AI class (hi:lo) | anims |
|---|---|---|---|---|---|
| 159 | Prometheus SK/8 | 0x62 | its own (rocket robot 0x633) | 0:0x10000004 -> `sub_0027F8E0` case 4 -> `sub_00283730` | animrb.pak (every level's), whole |
| 160 | Prometheus SD/7 | 0x36 | its own (guard robot 0x5F8) | none: the MapMaker bot AI (after .73) | animrb.pak, whole |
| 161 | Catacomb Ghost | 0x2F | the zombie 0x17's models | 0:0x50000000 -> `sub_002BA300` | lv17.pak 0x249..0x24C |
| 162 | SecuriDroid XP | 0x4D | its own (chrs/securibot 0x607) | 0x4000:0x10000000 -> `sub_0026B7A0` | lv6.pak 0xA22..0xA2C |
| 163 | Mutant (Story) | 0x30 | the mutant 0x59's models | 0x8000:0x10000000 -> `sub_00264A10` | lv22.pak 0x76B..0x770 |
| 164 | Pit Skeleton | 0x31 | the giant skeleton 0x28's model | 0x20000:0x10000000 -> `sub_0029FE00` | lv17_4.pak 0x23B..0x246 |
| 165 | Ragbug | 0xB0 | its own (chrs/ragbug 0x6CB) | 0x200000:0x10000000 -> `sub_0028B610` | lv17_4.pak 0x243..0x246 |

Rows 164/165 (wave 3) have per-level state, arena and props, run by
`pc_mapmaker_pitboss.inc` ([pit skeleton](mapmaker-pitboss.md)); their lv17_4
entries carry 0x4000, which that area maps again after a restart. The
stateless checks below cover rows 161..163.

Put the slot in any number of Story AIs: no row has per-level or per-AI
state, so several per map work as one. The preview shows the zombie, the
Arcade SecuriDroid (0x5C) and the Arcade Freak (0x59). A stock build plays
the map with the stored ids on the MapMaker bot AI and their own models
(soldier, securibot, soldier), and shows an empty name in the editor.

## The table (`pc_mapmaker_creatures.def`)

One X-macro row per creature, read by three files:

```
MAPMAKER_CREATURE(row, id, look, preview, label, class_lo, class_hi,
                  pak, pak_name, table, count, first, n, level)
```

- `row`: synthetic picker row. `pc_mapmaker_realbosses.inc` builds its
  `rbs_extra` rows from it (text id 0x52420000 | id, label at most 15
  characters, preview `preview`), so pick, slot reload, Story AI name and
  preview work as for the Prometheus rows.
- `id`: the character id the set member stores. It must have no Arcade row
  and be used by no other port, so stock rows and characters never change.
  0x2F and 0x30 are copies of the cutscene soldier `soldiermodel_talk` (no
  set, no Arcade row; Khallos uses 0x33). 0x4D is the securibot's own story
  id (only scene 6's set).
- `look` (0xFF: own): whose character-table models (+4/+8) the creature is
  made with in play.
- `class_lo`/`class_hi`: as the creature's story level spawns it.
- `pak`, `pak_name`, `table`, `count`, `first`, `n`: the anim pak (guest
  string), its `sub_001CA2F0` registration table and the kept entries.
  `pc_mapmaker_animslots.inc` adds one `ans_paks` row per creature; rows that
  share a pak keep the union of their runs.
- `level`: optional per-level state hook `void fn(unsigned kind)`, called at
  the anim mount for a row in the set (all 0 now; a rail-bot row would
  allocate its pool here, or through a `sub_0025F720` wrap).

Adding a creature is one row, its picker test lines and its decode facts.

## Hooks (play, scene 100, story mode byte `[0x007DE455]` == 10)

| WRAP | after/before | what it does |
|---|---|---|
| `sub_001C8540` -> `pc_mapmaker_creatures_anims` | after | for every row whose id the map's own set holds (slots 1-10 map-info chunk, 11-20 sets chunk; preset sets and an unreadable container: none), mount its pak whole with `sub_0005A130` unless mounted, while 8 of the 64 mount slots stay free; log `[mapmaker] creatures: <label>: mounted <pak>.pak (anim entries a..b kept, mount slots used n)`; then the row's level hook |
| `sub_0015E7C0` -> `pc_mapmaker_creatures_request` | after | a row with a look whose look id is not a set member: `sub_000A5E40` for the look's +4/+8 models (the set preload `sub_000D7780` asks for members) |
| `sub_000DAC30` -> `pc_mapmaker_creatures_create` | around | a row id with a look whose models are loaded or read (resource row via the map 0x008CA000, loaded flag or request data, as the khallos area): the id's +4/+8 hold the look's models for this call only, then are put back; else its own models (logged once) |
| `sub_0015B830` -> `pc_mapmaker_creatures_ai_start` | before | pool entry with character a+0x48 a row id, its pak mounted and every kept anim id mapped (id map 0x007E03C0 != 0xFFFF): spawn record +0 = class_lo, +4 = class_hi (MapMaker wrote 0x11000000/0 at 0x002421A7); log `[mapmaker] creatures: <label> (character 0x..) gets its story AI, class hi:lo, n of n anims from <pak>.pak` once per kind and level; else the bot class stays and the reason is logged |

Whole mount, not index-only: `sub_00057F30` stores a pointer into the anim
file and fixes it up in place (flag 4 at header +8), so the file must stay in
memory, as for every boss anim pak (INFERRED from the registration code; the
model and prop paks of other areas are index-only). Memory: lv17 0.33 MB,
lv6 0.83 MB, lv22 0.61 MB of the level heap, only when the set holds the row.

Restart (`sub_001CDD60`): nothing to do. Every kept entry's slot word +0xC is
0x1800 (no 0x4000), so the restart's `sub_0005C2F0` keeps it mapped (unlike
lv1_3/lv1_6, see real bosses); the record keeps its class for the restarted
AI start (CONFIRMED, `test_registration_tables_and_paks`).

## Why each row is safe (CONFIRMED, `test_mapmaker_creatures.py`)

- **Class reaches the handler.** `sub_0027D590` compares the whole 64-bit
  class (+0x14 hi in ecx, +0x10 lo in eax): lo 0x40000000 -> 0x0027D947 ->
  `sub_002BA300` (0x0027D958); hi 0x4000 -> 0x0027DCEF -> `sub_0026B7A0`
  (0x0027DD06); hi 0x8000 -> `sub_00264A10` (0x0027DDB2). `sub_0015F8C0`
  strips the 0x10000000 "active" bit (0x0015FA3A..0x0015FA49) as for
  MapMaker's own 0x11000000; without it the class is parked at 0x4000000.
- **The story levels spawn exactly these.** Catacombs (scene 17, set 5)
  GHT1..GHT7 are zombies 0x17/0x18/0x23/0x2A with 0:0x50000000 (the ghost is
  a class, not a character); mission 7 (scene 6, set 3) RS01/RS02/CR01 are
  0x4D with 0x4000:0x10000000; mission 8 (scene 22, set 4) MT06..MT19 are
  0x59 with 0x8000:0x10000000 (spawn records 0x98 B: +0 lo, +4 hi, +8 tag,
  +0x16 set slot).
- **No scene-only state.** From each handler and its message handler
  (`sub_002BA100`, `sub_0026B5E0`) through callees with at most four
  callers, the only scene test is `sub_00228B40` (scene 100 only: MapMaker's
  own kill marker `sub_0033E560`, called by the securibot); no boss block
  (0x0078B080..0x0078B1FF), alarm table, rail-bot or wall-panel pool is
  read. Patrol paths come from the record (+0x68, -2 none) as for bots.
- **Anims.** Every pak-table id the handlers use is a kept entry (ghost
  0x249..0x24B, securibot 0xA22..0xA2B, mutant 0x76B..0x76F; the deaths and
  hit reacts 0x24C/0xA2C/0x770 are kept too). No kept id or name is
  registered by a `pak/anim/` table, a boss pak or another row's pak.
- **Carriers.** 0x2F/0x30/0x4D have no Arcade row; no game code compares
  +0x48 with 0x2F/0x30; 0x4D only for its collision radius (0x000A3BB9,
  0x000A4434). `sub_000DAC30` reads the character table +4/+8.

## Not done

- **Cleaning robot 0x4F** (item 10). Scene 6 spawns it once ('TPCB') with
  lo 0x10080000 -> `sub_002682A0`, a scripted handler that reads the scene,
  finds tagged objects (`sub_0023FF20`) and fires level events
  (`sub_002273C0`). No code plays its anims 0xA2D..0xA30 by id, and the
  securibot handler plays the securibot's skeleton anims. A row needs its
  own driver; deferred.
- Spiders/rail bots (item 14): need the scene-22 pool (`level` hook) and
  paths. Wraith (item 3): global system.

## Unverified (INFERRED, playtest)

- The look swap: that a carrier made with the zombie's or mutant's models
  animates and collides like them (the khallos area does the same for the
  seated Khallos). The zombie models carry the story flag 0x2000000, which
  `sub_000A5E40` ignores in scene 100.
- Whether the ghost looks ghostly: its state 0 sets AI +0x28 |= 0x1000000
  (meaning not traced). Whether a ghost death fires the map's "AI killed"
  logic (the securibot calls MapMaker's marker itself; the ghost and mutant
  rely on the common death path).
- The picker list now has 14 synthetic rows after the 150 Arcade rows; no
  menu item cap was found, none was checked.
- The level heap with all three paks (about 1.8 MB) next to boss paks.

## Integrator notes

```
WRAP sub_001C8540 -> pc_mapmaker_creatures_anims     (layer with realbosses/robot/crowe/khallos/worm anims, any order)
WRAP sub_0015E7C0 -> pc_mapmaker_creatures_request   (layer with khallos/tank request, any order)
WRAP sub_000DAC30 -> pc_mapmaker_creatures_create    (layer with realbosses/khallos create, any order)
WRAP sub_0015B830 -> pc_mapmaker_creatures_ai_start  (layer with realbosses/robot/crowe/khallos/... ai_start; its id set is disjoint from theirs)
WRAP sub_002B8F90 -> pc_mapmaker_creatures_robot_weapon (after .66, log only; built-in weapon update, every AI every frame)
WRAP sub_001BB5C0 -> pc_mapmaker_creatures_robot_fire   (after .66, log only; an AI fires its weapon)
WRAP sub_00088C60 -> pc_mapmaker_creatures_robot_muzzle (after .66, log only; collision ray, hot: return-address test 0x001BC420 first)
```

Area name `creatures`. Prototypes are in `pc_display_menu.h`, off-build stubs
in `pc_menu_mouse_guest.inc`, which includes the `.inc` after storyprops. No
new guest calls (link stubs for `sub_0005A130`, `sub_00062260`,
`sub_000A5E40` and `sub_000631C0` exist). The picker rows need area
`realbosses` (its editor hooks).

## User test steps

1. `TSFP_MAPMAKER_LIMITS=1`. Editor -> Map Settings -> characters: custom
   set -> a slot: the picker ends with **Catacomb Ghost**, **SecuriDroid
   XP**, **Mutant (Story)** (previews: zombie, SecuriDroid, the Freak). Pick
   all three (one on the slot page 11-20); leave and reopen: kept.
2. Place two Story AIs of each, save, reload, play in Story. stderr:
   `creatures: Catacomb Ghost: mounted lv17.pak`, `... SecuriDroid XP:
   mounted lv6.pak`, `... Mutant (Story): mounted lv22.pak`, `Mutant (Story):
   requested the look's models`, `made with the look's models` for the ghost
   and the mutant, and one `gets its story AI` line per kind.
3. Ghosts: zombies that idle, trap and attack with the mission 6 ghost
   moves; one hit kills (unless trapping). SecuriDroids patrol and fire. The
   mutants walk, leap, swipe and fire electricity. Note anything frozen,
   T-posed or floating, and whether killing them fires a logic trigger.
4. Restart from the pause menu: they act the same afterwards.
5. `TSFP_MAPMAKER_CREATURES=0`: the same AIs are bots (stderr `keeps the bot
   AI`). Without `TSFP_MAPMAKER_LIMITS` the rows are absent; the map plays
   with bots. Story missions 6, 7 and 8 play stock.

## The Prometheus robots (rows 159/160, after the v0.4.0-mapmaker.58 report)

User (.58+): "New Prometheus SK/8 story AI addition, if unarmed, continues to
repeat its rocket firing animation without actually firing a projectile. If
given a weapon, it does this as well but does not fire its given weapon unless
very close to the player."

**Cause (CONFIRMED, `test_robot_class_4_path`, `test_bot_class_never_reaches_the_robot_code`,
`test_rocket_robot_built_in_weapon`, `test_story_robots_spawn_class_4`).** Wave 1
added the rows as editor rows only, so the Story AIs kept MapMaker's bot class
0x01000000 (`sub_0026EFC0`, 0x0027D8B6). The story levels spawn them with class
0:0x10000004 and no weapon (mission 9: 21 SK/8 records such as SB06/PR01/GB04;
mission 10: 11 SD/7 records such as DW03/TC03). Class 4 runs `sub_0027F8E0` (story
mode, lo & 0x1FFF, from `sub_0027FE30`): case 4 (0x0027F9C4) sends a robot (robot
list `sub_000D6DD0`, 0x0027FA37) with a target to the robot handler `sub_00283730`
(robot moves and fire poses from animrb's table 0x00520840, by type: 0x34 / 0x62 /
other); messages go to `sub_00283F80` (0x0027D05C). In scene 100 the stock code
already has a robot branch: no built-in weapon (+0x94), no +0x2BC and no usable
weapon -> the unarmed attack `sub_0027E4E0` (0x0027FA43..0x0027FA86). The AI start
gives 0x62 its built-in rocket weapon 0x43 (+0x94 at 0x0015B927, ammo from weapon
0x43's record) when the record has no weapon list (+0x18 -1, which MapMaker writes
at 0x00242155). Nothing the bot class reaches calls the robot list, the robot
handler or `sub_0027F8E0`, so the rocket weapon was never fired; the robot's
model still turned the bot's fire requests into its rocket pose, which looped.
INFERRED: the bot only fired a given weapon at close range because its aim/fire
timing ran against the robot's poses (not traced further; the story class
replaces the whole path).

**Change.** The two rows move from realbosses' `rbs_extra` into
`pc_mapmaker_creatures.def` (first in the table, so the picker order, row numbers
159/160, text ids 0x52420062/0x52420036 and previews 0xAA/0xA9 are unchanged; saved
maps store the character ids, never the rows, so old maps load as before). The
creatures area's AI start now writes 0:0x10000004 for 0x62 and 0x36. Their pak is
`pak/anim/animrb.pak` (0x0047E65C), which every MapMaker level mounts and
registers whole (0x001CB133..0x001CB151): the row keeps the whole table (first 0,
n = count), so the area never mounts it (logged "anim pak already mounted") and
the anim slot budget adds no row for it (a whole-table row becomes table 0 in
`ans_paks`, which matches nothing). The AI start still checks that all 21 animrb
ids are mapped. The Arcade twins 0xA9/0xAA keep the bot AI.

**State.** `sub_00283730` and `sub_00283F80` read no scene-only state (the same
reach test as the other rows); the class-4 path tests the scene only for its own
scene-100 branches. No per-level state, no level hook: several per map.

**Unverified (playtest):** the SK/8's rockets in a MapMaker map; what an unarmed
SD/7 does (`sub_0027E4E0`) and how SK/8/SD/7 use a weapon given in the editor
(the robot handler fires the held weapon, slot +0x204); MapMaker logic triggers
on their deaths.

## The .66 playtest: fire pose out of range, misses up close (log only so far)

User (build .66, both robots "gets its story AI, class 0x0:0x10000004, 21 of
21 anims"): "Prometheus rocket shooting animation begins when they are well
out of range and not actually firing rockets. When close, they fire normally
but can easily miss you if standing in place."

**How a rocket is (not) launched (CONFIRMED, `test_robot_fire_pose_choice`,
`test_robot_fire_gates`).**
- The robot handler `sub_00283730` only chooses poses. In state 1 it takes a
  fire pose (SK/8, type 2: `bigbotrocketadvfire` 0x9A7 or `bigbotrocket` 0x9A5)
  when the squared target distance AI +0xAF4 is below 100 (10 units,
  0x004785C8), or one time in three below 289 (17 units, 0x0049F33C). It
  checks neither sight, ammo nor the muzzle.
- The rocket itself comes from the built-in weapon update `sub_002B8F90` (every
  AI each frame, from `sub_001B8C40`): only for +0x94 0x42/0x43 or +0x2BC, while
  the current anim is a fire pose (slot word 0x100000), with the ammo state of
  `sub_001B9340`, sight `sub_0015E120(ai, 0)` (the shared visibility table) and
  the pose's fire event 0x200 (`sub_00061570`); then `sub_001BB5C0` fires. Its
  projectile path first casts a 4-unit ray from the muzzle along the aim
  (`sub_00088C60`, return 0x001BC420; flags 0x2000058): any hit and no rocket
  leaves (0x001BC425).
- The SK/8's aim point is the target's feet - 0.2 when the target is more
  than 5 units away and not above the robot (0x000D87A0..0x000D87E6: rockets
  are aimed at the floor for their blast); closer it uses the ordinary aim.
- The SD/7 has no built-in weapon (+0x94 0, `sub_002B8F90` returns at
  0x002B9153): it fires only a weapon given in the editor.

So the pose and the launch are decided by different code: a pose taken
without sight or with a blocked muzzle plays without a rocket. Which gate
stops the rockets in MapMaker maps is **not known yet**; candidates, all
INFERRED: the 4-unit muzzle ray hitting MapMaker geometry (tile walls, floor
lips, the robot's own surroundings) when aimed down at the floor point, the
visibility table (MapMaker rooms and portals), or the fire event. The misses
up close are likewise unexplained (aim at the body with the stock aim error,
or rockets aimed at the floor exploding out of reach).

**Change (log only).** Three hooks log, once per robot and outcome per level
(at most two of each, eight per robot), every fire pose of a class-4 robot row
when it ends:

```
[mapmaker] creatures: Prometheus SK/8 #1: fire pose ended without a rocket (squared distance 250.0 at its start,
  5 frames, 5 without sight, 0 fire calls, 0 muzzle hits, built-in weapon 0x43)
[mapmaker] creatures: Prometheus SK/8 #1: fire pose launched a rocket (squared distance 30.0 ...)
```

"without sight" counts frames where the weapon code's own sight test failed,
"fire calls" calls of `sub_001BB5C0`, "muzzle hits" muzzle-ray hits (a pose is
"launched" when it had more fire calls than muzzle hits). Nothing in the game
changes. The next playtest's log decides the fix: gate the fire pose on the
failing test (sight, muzzle) in the robot handler, and the aim point if the
floor shots miss.

Integrator note (2026-11-07): `WRAP sub_001BB5C0 -> pc_mapmaker_creatures_robot_fire` is not generated. Upstream already defines `sub_001BB5C0` (network bot shot / kill feed) and the generated story-chain rename for it does not apply in the staged patch run, so the fire-call count stays 0 in game; the muzzle-hit and sight counts still log.

## The .73 log: far fire poses, no sight, editor weapon 0x16

Log (build .73): "Prometheus SK/8 #2: fire pose ended without a rocket
(squared distance 15926.9 at its start, 1000 frames, 870 without sight, 0 fire
calls, 0 muzzle hits, built-in weapon 0x43)", and two more at squared
distances 15336.8 and 2338.9 (126 and 48 units) with built-in weapon 0x16.
"fire calls" was always 0: `sub_001BB5C0` is defined by the runtime
(`recomp_manual.c`) and that hook was never wired.

**Causes (CONFIRMED, `test_far_fire_pose_is_the_advance`,
`test_sight_table_and_update`, `test_editor_weapon_replaces_the_rocket`).**
- *Far fire poses.* The 100 / 289 limits are only state 1's. State 0 sends a
  target at 17 units or more to state 3, which walks with the advance-fire
  pose 0x9A7 at any distance (0x002838C5..0x002838F5, 0x00283AF6). That is the
  stock Machine Wars advance: firing while walking, rockets only when the
  weapon gates pass.
- *Weapon 0x16.* MapMaker writes the Story AI's editor weapon (chunk +0x26)
  to spawn record +0x18 (0x0024205A..0x00242155). The AI start takes the
  built-in weapon +0x94 from that list when it is not -1
  (0x0015B878..0x0015B889), so an armed SK/8 lost its rocket launcher 0x43.
  The class-4 robot path fires only 0x42/0x43 (0x002B9145..0x002B9153), so
  it then fired nothing at all.
- *SD/7.* It has no built-in weapon in any case, so class 4 never fires for it.
- *Sight.* `sub_0015E120(ai, 0)` reads the pair table `[0x0074C368]`, which
  `sub_0025FB70` updates for one AI per frame (index `[0x007A3500]`) with a
  ray `sub_00088C60` (return 0x00260134) from the AI's collision object
  toward the target's eye; the pair is visible when the ray returns the
  target. Why it fails about 90% of the time for the SK/8 is **not known**
  (INFERRED candidates: the ray's start/eye point on the tall robot model,
  MapMaker geometry in between, or the per-frame turn order).

**Fixes.**
- SK/8 (`crt_builtin`): at the AI start the record's weapon list +0x18 is
  set to -1 when an editor weapon was chosen, so it always gets its rocket
  launcher 0x43 (logged: "editor weapon ignored: the story robot fires only
  its built-in weapon"). A weapon given in the editor cannot be fired by the
  robot code.
- SD/7: its row has no story class any more (class 0:0 = keep MapMaker's bot
  class), so it fights as a bot and fires the weapon given in the editor (as
  before .58; logged "keeps the MapMaker bot AI"). Its picker row and anims
  are unchanged.
- Log: the fire log now counts fire events 0x200 (`sub_00061570`, as the
  weapon code asks) and muzzle tests (the muzzle ray's calls, which mean
  `sub_001BB5C0` reached its projectile path) instead of the unwired fire
  calls. A new log-only WRAP on `sub_0025FB70` marks the robot's sight turn;
  its sight rays (return 0x00260134) are counted, and after 64 the robot
  logs once how many reached a player, hit something else (the last
  object and its type +0x20) or hit nothing, with its y, eye height +0xBB8
  and height +0x1AC.

**Not done:** the SK/8's sight and therefore its rocket launches. The next
log names what the sight ray hits; the fix then goes there (the eye point, or
a robot-specific sight). Gating the advance-fire pose on sight was not done:
swapping the pose under the robot handler restarts its state 3 every frame.

WRAPs after .73 (log only):

```
WRAP sub_0025FB70 -> pc_mapmaker_creatures_robot_sight  (sight update of one AI per frame, no args)
WRAP sub_00088C60 -> pc_mapmaker_creatures_robot_muzzle (as before; now also its return 0x00260134)
WRAP sub_002B8F90 -> pc_mapmaker_creatures_robot_weapon (as before)
(no WRAP on sub_001BB5C0: the runtime defines it)
```

## The .75 log: the sight ray hits a character at the robot

User (.75): "Prometheus robots repeating firing animation continuously, even
when not firing." Log: "SK/8 #2: sight rays: 1 reached a player, 60 hit
something else (last 0x142A5A0, type 8), 3 hit nothing; robot y 0.50, eye
+0xBB8 2.60, height +0x1AC 1.25"; #3: 0 / 64 (0x142B428, type 8); #1 one
rocket in 1540 frames (1490 without sight).

**Cause (CONFIRMED part, `test_sight_ray_excludes_only_the_entity`).** The
sight update hides only the updating AI's entity from its own ray: bits 0x60
of the entity's +0x28 are cleared around the ray (0x0025FF14..0x0025FF21,
restored at 0x00260175 / 0x002601E9), and the ray skips objects without 0x20
(0x000A35F4, 0x000A7B67) or with a type in its mask 0x2410270. The SK/8's
ray starts at its eye 2.6 units up and almost always hits a type-8
(character) object first, a different one for each robot. INFERRED: the
robot's own body or a part (a second character object of the tall model),
which that exclusion misses. The log says which next time ("own entity").
With no sight the weapon never fires, while state 3 keeps walking in its
advance-fire pose and states 1/2 hold the fire-still pose.

**Fix.**
- Sight: when the robot's sight ray (return 0x00260134) stops at a collidable
  character within 2.5 units (x/z) of the robot that is not a player, the
  ray is repeated with that object's 0x20 bit cleared (at most three such
  objects, all put back after the call). Logged once per robot: "sight ray
  passes through a character at its own body (0x..., own entity yes/no)". A
  character farther away still blocks the view. A player next to the robot
  is seen as before.
- Poses: a new WRAP on the robot handler `sub_00283730` checks the weapon
  code's sight test `sub_0015E120(ai, 0)` first. Without sight, the type-2
  pose row (0x00520838 + 2*0x20) holds the non-firing poses for that call
  only: advance-fire 0x9A7 becomes advance 0x9A6 (+8/+0xC) and fire still
  0x9A5 becomes attack idle 0x99E (+0x1C). The handler's own "is it playing"
  tests read the same row, so its states stay consistent; the row is put
  back after. Logged once per robot: "no sight of its target: poses without
  firing". With sight it walks and fires as in Machine Wars.

WRAPs after .75:

```
WRAP sub_00283730 -> pc_mapmaker_creatures_robot_pose   (new; the class-4 robot handler, per robot frame)
WRAP sub_00088C60 -> pc_mapmaker_creatures_robot_muzzle (as before; its sight-ray branch now repeats the ray past the robot's body)
WRAP sub_0025FB70 -> pc_mapmaker_creatures_robot_sight  (as before)
WRAP sub_002B8F90 -> pc_mapmaker_creatures_robot_weapon (as before)
```

**Unverified (playtest):** that the blocker is the robot's own body (log
line "own entity"), that the SK/8 then sees and fires at a visible player,
and that switching poses when sight comes and goes looks acceptable (a
"stand to advance" step may play when sight returns).
