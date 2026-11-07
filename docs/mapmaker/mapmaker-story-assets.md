# MapMaker: original story assets inventory (wave 0, 2026-11-07)

User track: "original game assets into MapMaker". Wave 0 is research only:
for each requested item, where its assets live, its ids, its behaviour code,
the per-level state it needs, what MapMaker supports today, whether a map can
hold several, and an effort estimate. Nothing was built, run or changed.

Sources: US Xbox XBE 45410066 (`private/game/default.xbe`), its disassembly
(`private/pipeline/disasm/asm/text.asm`), the prop table
`prop/table/prop_all_xbas.raw` in `pak/arcade/l_100.pak` (0x28-byte rows
{model id, type, .., path, .., flags, init index}), pak indexes (an entry is
the CRC32 of `assets/ts/models/<path>/xbox/output/<leaf>.xbr`), story level
files `data/<level>.raw` (read in place, not kept), and the earlier notes
`logs/limits/features/story-enemies.md` / `story-character.md` (ignored). This
file holds ids, addresses, pak names and sizes only, never asset bytes.

**CONFIRMED** = seen at the address or in the data. **INFERRED** = reasoning.
"hi:lo" class = AI +0x14 : AI +0x10 (spawn record +4 : +0).

Mission numbers follow the story table `0x004D1AC0`: 1 intro (scene 2),
2 castle (4, "Scotland the Brave"), 3 depot (18), 4 train (11), 5 mansion2 (1),
6 catacombs (17), 7 nearfuture1 (6), 8 nearfuture2 (22), 9 farfuture1 (5,
"The Machine Wars"), 10 farfuture2 (14), 11 nemo (9), 12 outro (26),
13 nemoend (81).

## Corrections to earlier notes (CONFIRMED)

- **Every character id 0x00..0xB0 has its main model in `pak/chr.pak`**, which
  MapMaker play mounts (character table `0x0053A3CC + id*0x30`, +4 model).
  Character *models* are never what blocks a story creature. The blockers are
  anims (level anim paks), the AI class and per-level state.
- Id 0x28 is `chrs/mansionboss` (the giant skeleton, ported) and 0x2C is
  `chrs/mansionworm` (ported). Neither is the wraith. The wraith is a level
  object (item 3).
- The character table has a row 0xB0: `chrs/ragbug`, model 0x6CB (30,024 B,
  chr.pak). Story set 5 (catacombs) holds it as entry 12 even though its count
  word is 12. The slot lookup `sub_0006BC50` clamps to <= 0x13 and never reads
  the count (0x0006BC55).
- Character name text id = int16 at `0x0053A3C8 + id*0x30` (4 bytes before the
  model row). The strings are in `pak/arcade/l_103.pak` entry 0x78D4E0DF, at
  index text - 0xD87.
- Anim registration rows are {name*, 0, flags, x, id, 0}, stride 0x18, with
  the id at +0x10. `sub_001CA2F0` registers a table only when its pak is mounted.
- The Story AI editor's 5-option row (`0x0052E060` = 14, 10, 6, 3, 0) is
  **Health**, not behaviour, so MapMaker has no "follow" option.
- Prop init index table `0x004FCA60`. `sub_00242780` (0x00242A21) turns each
  prop row's index into a function, and `sub_000A6EC0` calls it at
  0x000A6F3A. Because the rows come from prop_all, these level init functions
  also run in scene 100. Indexes used here: 2 `sub_00214FF0` (catacombs),
  20 `sub_0021A710` (intro), 22 `sub_0020C300` (nearfuture1), 25
  `sub_00370B20` (wall-panel screen), 26 `sub_002560C0` (castle tank),
  33 `sub_00206810`, 38 `sub_00340470` (MapMaker).
- Props created by the existing ports through `sub_000A6F90(model, &pos,
  &rot, 0, 0)` get level record obj+0x2D8 = 0. Any init or damage callback
  that reads the record needs a stand-in record (+4 logic tag, +8 flags,
  +0xC model, +0x10 position, +0x1C rotation, +0x28 item value; `sub_0006C740`).

## Summary table

| # | item | assets | behaviour | MapMaker today | several | effort | main blockers | status |
|---|---|---|---|---|---|---|---|---|
| 1 | Hidden weapons: TNT 0x2F, time grenade 0x29, uplink 0x38, variant twins | all in gun.pak and `pak/anim/gun/*` | normal projectiles (TNT = mine update `sub_000F39A0`) | weapon list holds every id 1..0x45; these are hidden by unlock type 0 | yes | **cheap** | AI use of TNT; twin semantics | **wave 1: 0x29, 0x2F, 0x38 in the editor list** (fixture only); twins out |
| 1b | Story-only "weapons" 0x3C..0x3F (binoculars, whiskey, JB camera, zombie limb) | story paks only | script props | none | - | hard | name text, story ammo types | - |
| 2 | Mission 1 big exploding barrel = power cell 0x53D | 34,412 B; l_2_st / l_64_ch (mounted by the mothership port) | init 20 case 12, damage `sub_00218650`, explosion 0x2F | none (barrel is 0x0A) | yes | **cheap** | stand-in record at obj+0x2D8; resource 0xCFB | **wave 1: drum Type option, area storyprops** (played .57: textures OK; height back to the drum's, tunable) |
| 3 | Mission 5 wraith (black ghost) | prop 0x1DA/0x242 11,996 B in **gun.pak**; anim 0x13E in lv60.pak (816 B) | global wraith system `sub_0024ABA0`, tag 'WRTH' | none | **no** (one global) | medium-hard | scene gate, rooms, spirit table coordinates | - |
| 3b | Mission 5 worms | 0x2C | - | **ported** (worm area) | yes | done | - | - |
| 4 | Cut-out targets + scoring | 0xF1..0xFA, 3,652 B each; l_63/64/65/78_ch | challenge kinds 3..5, `sub_0022E7E0`, table `[0x0075CC8C]` | none | <= 75 | medium-hard | what else `[0x7DE470]` switches | - |
| 5 | Mission 6 electric zombies | anims 0x247/0x248 lv17_3.pak (66,768 B) | class 400:0 `sub_00282300` | none | INFERRED yes | hard (visual-only variant: cheap) | catacombs tags, pads, emitters | - |
| 6 | Mission 6 ghosts | anims 0x249..0x24C lv17.pak (334,016 B) | class 0:40000000 `sub_002BA300`, self-contained | none | **yes** | **cheap-medium** | ghost char id (probe); shared anim ids | **wave 2: row 161 "Catacomb Ghost"** (fixture only) |
| 7 | Mission 6 pit skeleton | lv17_4.pak (395,440 B) + l_17_st props | `sub_0029E0A0` | mansion variant ported | <= 4 (instances) | hard | coexisting with the id-0x28 routing; arena | - |
| 8 | Mission 6 flying imps = ragbugs | char 0xB0 (chr.pak); anims 0x243..0x246 lv17_4 | class 200000:0 parks them; flock driven by the pit fight | none | <= 6 per skeleton | medium with item 7, hard alone | flock driver lives in the boss | - |
| 9 | Mission 7 robots = securibot 0x4D | anims 0xA22..0xA2C lv6.pak (828,752 B) | class 0x4000 `sub_0026B7A0`, no level state | none (0x5C nfrobosec is already a roster row) | yes | **medium** | picker row, lv6 mount | **wave 2: row 162 "SecuriDroid XP"** (fixture only) |
| 10 | Mission 7 cleaning robot 0x4F | model 0x609 chr.pak; anims 0xA2C..0xA2F lv6 | INFERRED class 0x4000 family; loop sound 0x365 | none | yes | cheap-medium | which anims and class it really uses (probe) | deferred: scripted class 0x80000 (see creatures doc) |
| 11 | Mission 7 slow-motion grenade = weapon 0x29 | gun.pak | projectile type 0xF, explosion 0x2B | hidden (item 1) | yes | cheap to offer, medium to verify | slow-motion code **not found** | **wave 1: offered**; P2 offline: no slow-time code, no level state |
| 12 | Mission 7 city sky + other story skies | `skyTS3nf1_xbox` 0x790 | sky record 52 | **already offered** (sky-music area) | - | done | - | nothing to do (already offered) |
| 13 | Mission 7 red laser pairs + control panels | 0x4E0..0x4E3, about 2 KB each, l_6_st | init 22 `sub_0020C300`: lasers `sub_00209680`, panel damage `sub_0020B8C0`; alarm table `0x0078B2E0` | none | yes (5 lasers per panel group, 18 alarms) | **medium** | stand-in records; alarm rows in scene 100; damage type 0x2F | - |
| 14 | Mission 8 robot spiders 0x4A/0x50 | chr.pak; anims lv22.pak (610,832 B) | class 0x2000 `sub_0037B720`; pool `sub_003796B0` (15) | none | up to the pool size | medium-hard | rail paths in descriptor +0x54..+0x60; pool | - |
| 15 | Mission 8 mutants = 0x59 "The Freak" | chr.pak; anims 0x76B..0x770 lv22.pak | class 8000:0 `sub_00264A10`, no level state | roster char placeable, stock AI | yes | **medium** | lv22 mount (shared with item 14) | **wave 2: row 163 "Mutant (Story)"** (fixture only) |
| 16 | Mission 8 "rail cameras" | none of their own | the player pilots a rail bot from a wall terminal (`sub_0037ACB0`) | none | - | very hard (drop) | wall-panel pool exists only in scenes 6/17/22 | - |
| 17 | Followers (Ash 0x08, Tipper 0x00, Jo-Beth 0x19, Amy 0x44, R-110 0x2D; R-107 0x90) | chr.pak, roster rows | buddy = `sub_00277FE0` by scene tag, one buddy, single player only | placeable as normal AI | placing yes; buddy behaviour 1 | cheap to place, **hard** for story follow | follow movement not located | - |
| 18 | Machine Wars "Prometheus" robots = PROMETHEUS SK/8 rocket robot 0x62 (SD/7 guard robots 0x12/0x36/0x75) | chr.pak; anims `animrb.pak` (already mounted in scene 100) | robot list `sub_000D6DD0`, per id, no class | 0xA9/0xAA roster twins already placeable | yes | **cheap** | picker rows only | **wave 1: picker rows 159/160** (fixture only) |
| 19 | Machine Wars tank = driveable future tank, vehicle def 2 | 0xB/0xC/0xD + gibs, stream/lv5/prop1.pak (2.6 MB) | stock vehicle system, builder `sub_00181D40` | vehicles exist (cat. 35, max 4); not our boss tank | yes (4 vehicles) | **medium** | scene-5 gate in `sub_00174120`; templates +0x30/+0x34 | **wave 3: vehicle option "Future tank", area vehicles** (fixture only; not destroyed in scene 100) |
| 20 | Scotland the Brave truck = vehicle def 0 | model 7 + Vickers turret 0xE, stream/lv4/prop3.pak (1.36 MB) | builder `sub_00180BA0` | vehicles exist | yes (4 vehicles) | **medium** | seat anims 0x78..0x83 location | **wave 3: vehicle options "Castle truck" with/without Vickers, area vehicles** (fixture only; anims in animcar.pak) |
| 21 | Mission 1 big turrets = 0xE6 prisonfixedgun | 88,716 B, l_2_st / l_26_st | generic class-0x200 turret, gun type 7 | turret categories 27-29 (cat. 29 same system) | yes | **cheap** | gun type 7 effects, flag 0x10000 | **wave 1: fixed gun Model option, area storyprops** (played .55; textures fixed after) |

## Per-item notes

### 1 and 11. Weapons, TNT and the time grenade

Tables (CONFIRMED):
- Weapon table `0x004FB190 + id*0x54`, ids 0..0x45: +0 name text, +4 pickup
  model, +8 promo model, +0xC flags.
- Weapon record `0x004FA134 + id*0x3C`: +4 ammo type, +8 variant flag, +0xC
  anim pak path (loaded by `sub_001C8540` for each weapon in the play set; all
  exist under `pak/anim/gun/`), +0x10 unlock condition.

How the list is built (CONFIRMED):
- The weapon list builder `sub_002BFA70` walks the list table `0x004FC88C`:
  69 dwords, every id 1..0x45.
- It skips an id when `sub_001CD9D0(id) == 0` (return address 0x002BFB08).
  Unlock type 0 always gives 0 (jump table 0x001CD530), even with the
  unlock-all flag `[0x0075AEF4]`.
- `sub_00064E70` greys only 0xC, 0xD and 0x14 in modes 0xB/0xC.

Hidden by type 0, all assets in gun.pak:
- 0x2F TNT (models 0x141, 0x713..0x716)
- 0x29 time grenade (0x160, 0x748..0x74B)
- 0x38/0x39 uplink (story maps append 0x38)
- 0x01
- variant twins 0x03, 0x06, 0x09, 0x11, 0x18, 0x1A, 0x1C, 0x1F, 0x21, 0x25,
  0x2D, 0x31 (same name and model as their neighbour; variant flag 1, meaning
  unknown)
- 0x27 (monkey gun twin)

Story-only:
- 0x3C binoculars, 0x3D whiskey bottle (l_4_st)
- 0x3E JB camera (l_1_st/l_17_st)
- 0x3F zombie limb (l_17_st)

Empty: 0x40..0x43 (no model), 0x44/0x45 (model in no pak). The ghost gun
(0x22) is already offered. Amy Chen's sword 0x752 is a prop, not a weapon id.

TNT: projectile type 0x13. It shares the mine update and swaps to model 0x715
when placed (0x000F3BB7). Several are fine (object pool).

Time grenade:
- Chain: ammo type 0x17 -> projectile type 0xF (table `0x004DEB28`, create
  `sub_000E34D0`, update `sub_000F2B80`) -> explosion type 0x2B (radii
  7/6/5, table `0x004DB498`).
- **The slow-motion code was not found.** No compare against 0x2B shows up in
  the explosion, damage or projectile paths, and the game delta writers
  (`[0x004E7944]`/`[0x004E794C]`) are the clock, demo playback and zero-delta
  scopes.
- So whether the effect is global or per actor, and whether it needs level
  state, is unknown. The probe (P2) settles it.

Change: WRAP `sub_001CD9D0` and answer 1 for an allow-list only when the
return address is 0x002BFB08 (the list builder). Ids below 0x46 already fit
the 6- and 20-slot sets and the save format, and play does not re-check
unlocks. Start with {0x29, 0x2F}. Leave the twins out until their variant
flag is understood.

**Wave 1 (2026-11-07, fixture only).** Done in the weaponset area
(`pc_mapmaker_weaponset_unlock`, editor only): 0x29, 0x2F and 0x38. The
variant flag is now understood: it becomes the weapon state's mode +0x38
(0x001BE0A4), so a twin is its neighbour in its other mode; twins stay out.
P2 offline: the type-0xF projectile path (explosion 0x2B, decal) reads no
scene or level state and no slow-time code exists on it; the time grenade
works in MapMaker as an ordinary grenade. Details and test steps:
[weapon set](mapmaker-weaponset.md) "Hidden weapons".

### 2. Mission 1 big exploding barrel (intro power cell)

- Model 0x53D `farfuture2/powercell`, type 1, init 20, 34,412 B. In
  `story/l_2_st.pak`, l_26_st, l_14_st, l_5_st and `challenge/l_64_ch.pak`.
  The mothership port already mounts l_64_ch, and its shared files are
  identical to the other paks'. Twins 0x487/0x549 use init 33.
- intro.raw places 12: tags 'CEL1'..'CEL6' (objective cells), 'PCX1..3', and
  3 untagged.
- Init `sub_0021A710`, case 12 at 0x0021ABC5: health 2.0, damage
  `sub_00218650`, update `sub_00218740` (random force when hit by type 0x1A),
  kind-1 physics body (`sub_0016C170`).
- At health 0, an untagged cell explodes with type **0x2F**: table
  `0x004DB498`, fields (10, 12, 25) against the MapMaker barrel's type 0x0A
  (10, 6, 25), which `sub_000AE4F0` pushes at 0x000AE767. That is "big"
  (INFERRED: the 12 is the radius).
- CEL-tagged cells use type 0x28 plus the objective update
  `sub_002274C0(3, 0)`; MapMaker tags never match them.
- Physics preload `sub_00257150` asks for resource 0xCFB for this model.
- Needed: a cover/barrel category variant whose record carries model 0x53D.
  The damage path reads the level record at obj+0x2D8, so it must be a real
  record, not 0 (INFERRED).
- **Wave 1 (2026-11-07, fixture only):** area `storyprops`
  ([mapmaker-storyprops.md](mapmaker-storyprops.md)). A placed explosive drum
  (Props item 0x49) gets an options menu "Type: Explosive drum / Power cell";
  the choice is item record byte +0xC (stock-unused for it). In play the
  fill's prop record gets model 0x53D, so the record MapMaker already builds
  is the stand-in (CONFIRMED: `sub_0006C740` stores it at obj+0x2D8; its +0 is
  never a CEL tag, so the explosion is 0x2F). l_64_ch is mounted when the map
  has one; 0xCFB comes with the model request. Several per map: yes.

### 3. Mission 5 wraith (black ghost)

**What it is.** A single level object tagged 'WRTH' (`sub_0023FF20` at
0x0024A1FA/0x0024ABA9), played with `flight_cycle`, driven by one global
system (CONFIRMED).

**Model.** `mansion/wraith` 0x1DA (init 0) / 0x242 (init 2), type 0x4000000,
11,996 B, in **gun.pak** (also l_1_st and l_60/61/62/74_ch).

**Anim.** 0x13E `flight_cycle`: lv1_1 (38,240 B), lv1_2 and the
one-anim **`stream/lv60/anim/lv60.pak` (816 B)**.

**Code** (CONFIRMED):
- Scene gate `sub_0024A150`: scenes 1 and 0x3C only.
- Block `[0x0078B718]`: 0x640 B, 5 spirits x 0x140, allocated by
  `sub_0024A180` at level start (0x001CE12C).
- Reset `sub_002498C0` on restart (0x001CDE0E).
- Room list `sub_00249940` (mansion room 0x17; challenge 0 rooms 1 and 2).
- Update `sub_0024ABA0`: from the mansion update 0x001DDE27 and from
  challenge `sub_0022E090`. It looks up 'VDMN'/'VNMN'/'JBTH', raises scene-1
  level events and damages players.
- Draw `sub_0024A1A0`.
- Logic action 0x2D `sub_0024A100` adds 5 spirits from table `0x0051AF4C`.
- The add-spirit guard in `sub_00249C00` (`cmp edx,5 / jg`) lets a 6th
  spirit into the 5-slot block (INFERRED latent overflow).

**Only non-story use:** challenge 0 (scene 60).

**Port.** One per map:
- gate hook, allocation and reset in scene 100;
- a 'WRTH' stand-in prop at a placeholder;
- room registration;
- a per-frame update call;
- activation through `sub_0024A0C0(x, y, z)`;
- lv60.pak mount;
- neutralise the scene-1 events.

**Unknowns:** what 'VDMN'/'VNMN' are, whether the spirit table is absolute,
whether texture 0x292 resolves, and MapMaker rooms against player +0xF54.

### 4. Cut-out targets and challenge scoring

**Models.** 0xF1..0xFA, 3,652 B each, type 1, init 0:
- Venice 0xF6..0xFA: l_63_ch (5.2 MB)
- futurecanyon 0xF1/0xF2: l_64_ch (already mounted)
- robot 0xF3..0xF5: l_65_ch (11.4 MB) and l_78_ch (3.8 MB)

"Bad" is hard-coded by model: 0xF1, 0xF3, 0xF4, 0xF6, 0xF7, 0xF8.

**Data.** Level raw +0x108 -> descriptor +0xFC/+0x100: 0x40-byte target
records, loaded only when `[0x7DE458] & 0x40`. Fields:
- +0 movement 1..5 (slide, rise, swing)
- +4 model
- +8 end-test flags
- +0xC position, +0x18 heading, +0x1C travel, +0x20 speed
- +0x28/+0x2C up/down times
- +0x30 points
- +0x34/+0x38 trigger (time or player-1 zone)

**Code** (CONFIRMED):
- Loader `sub_002292B0`: no bound, about 75 rows in the 0x19E0-byte table
  `[0x0075CC8C]`, allocated by the challenge init `sub_0022CD30`.
- Update `sub_0022E7E0` from `sub_00231760` (switch on `[0x7DE470]` = 3..5).
- Good hit `sub_0022E5C0` scores -100.
- Bad hit `sub_0022E6E0`: kinds 4/5 score 100/75/50/25 by distance from the
  centre (`sub_00229000`).
- Score `[0x0075CC8C]+0`, mirrored to the HUD `[0x75C360 + p*4]`.
- Medal rows at 0x0050E4D8/0x0050E65C/0x0050E7E0.
- Finish `sub_00229250`: hard-coded zone 0x3FE or 0x429.

**MapMaker version.** Needs:
- an editor target object with these fields;
- the table allocated in scene 100;
- `[0x7DE470]` set, or the update called directly;
- a finish/timer rule;
- a score HUD.

**Risk.** `[0x7DE470]` has about 100 readers. Wave 5.

### 5. Mission 6 electric zombies

- Class 400:0 -> `sub_00282300` (call 0x0027DC8F), messages `sub_00282FF0`.
- Anims 0x247 `zomelectrified` and 0x248 `zomgrow` in
  `stream/lv17/anim/lv17_3.pak` (66,768 B).
- Hard level dependencies (CONFIRMED): pad tags 'ZPD4'/'ZPD5', actor tags
  'ZMK1'/'ZMK2', pad ids 0x49F/0x4A2, and emitters `[0x0078DA0C]`/
  `[0x0078DA10]` made only by catacombs init `sub_00214220`. It is a set piece.
- Which zombie ids use it: INFERRED ordinary set-5 zombies (probe P1).
- Cheap alternative: a roster zombie that plays 0x247 (lv17_3 mount), visual
  only.

### 6. Mission 6 ghosts

- Class 0:40000000 -> `sub_002BA300`: four states (table 0x002BACC0).
- Messages `sub_002BA100`: a hit kills it unless it is in state 2.
- Contact damage `sub_002BA1C0`/`sub_002BA160`.
- No scene or level-state reads (CONFIRMED).
- Anims 0x249..0x24C `ghostidle/trap/attack/death` in
  `stream/lv17/anim/lv17.pak` (334,016 B, 42 entries). No ghost model is in
  the catacombs paks, so the ghost is a human-skeleton set-5 character with
  this class (INFERRED; probe P1 gives the id).
- Port: class override on any roster character (e.g. 0x80 `chrs/ghost`),
  lv17.pak mount, 4 anim slots. Other lv17 names share ids, so use the
  worm-style shared-name handling.
- **Wave 2 (2026-11-07, fixture only): [story creatures](mapmaker-creatures.md), row 161.
  CONFIRMED: the GHT1..GHT7 records are zombies 0x17/0x18/0x23/0x2A with
  class 0:0x50000000 (0x10000000 = active at spawn); the ghost is a class.
  Carrier 0x2F made with the zombie 0x17's models; lv17 entries 30..33.

### 7. Mission 6 pit-fight skeleton

See [mapmaker-boss-survey.md](mapmaker-boss-survey.md) section 6. Additions:

| id | path | size | pak |
|---|---|---|---|
| 0x245 | torturerack | 132,040 B | stream/lv17/prop4.pak |
| 0x246 | torturerackNoRope | 79,572 B | l_17_st only |
| 0x249 | mansionboss_collision | 61,112 B | l_17_st only |
| 0x24A | gas_bottle (the only damage path) | 28,672 B | l_17_st only |
| 0x24B | eyebandage | 7,848 B | l_17_st only |
| 0x24C | gascananim | 29,520 B | l_17_st only (init 2 writes +0x34 = 0xD5, the old global room) |

Paks: l_17_st.pak is 15.5 MB; prop4.pak is 1.3 MB.

Selector design (INFERRED). A second picker row "Giant Skeleton (pit)" stores
char 0x28 with a variant marker, and `pc_mapmaker_realbosses` keeps it per
instance. For pit instances:
- `_princess` -> `sub_0029E0A0`
- `_message` -> `sub_0028C560`
- `_risen` answers 0x23B
- the HUD bar is forced
- drop level event 2 (0x0029FC33)
- stand in 'NMME'
- suppress the 'PRJB' hostage/timer failure `sub_00212160`
- rebase the coordinates (0x0049F478.., ragbug park 0x0049F388..)
- mount lv17_4

Reduced design: boss + gas bottle (3 hits) + optional ragbugs. Swap the
props the boss spawns to mounted models where l_17_st is too large.

### 8. Mission 6 flying imps (ragbugs)

- Char 0xB0, model 0x6CB.
- Class 200000:0 -> `sub_0028B610` only parks the bug, at a fixed catacombs
  position (constants 0x0049F388..0x0049F3A4).
- Flight and attacks are run by the pit fight: `sub_0029E0A0` ->
  `sub_00289F30` (finds 'RAG0'..'RAG5'), `sub_0028A0A0`, `sub_0028AAA0`
  (flock), `sub_0028B780`. Six slots in the Princess block `[0x0078B1EC]`
  +0x4A30.
- Anims 0x243..0x246 in lv17_4.
- Best done with item 7. Standalone needs a new flock driver.

### 9. Mission 7 robots (securibot)

- Set 3 = Cortez, Amy 0x44, securibot 0x4D, office guard 0x4C, cleaning bot
  0x4F, maintenance 0x53.
- 0x4D "SecuriDroid XP": class 0x4000 -> `sub_0026B7A0` (0x0027DD06),
  messages `sub_0026B5E0`.
- Anims 0xA22..0xA2C (11) in `stream/lv6/anim/lv6.pak` (828,752 B, 70
  entries).
- The scene-6 case of `sub_0025F720` only sets a sound. No statics, no scene
  reads.
- 0x5C nfrobosec (same name) is an Arcade row and already placeable.
- **Wave 2 (2026-11-07, fixture only): [story creatures](mapmaker-creatures.md), row 162 stores 0x4D.
  CONFIRMED: RS01/RS02/CR01 records are 0x4D with class 0x4000:0x10000000;
  the handler reads the scene only in `sub_00228B40` (scene 100: MapMaker's
  kill marker). lv6 entries 3..13.

### 10. Mission 7 cleaning robot

- 0x4F, model 0x609 (80,560 B, chr.pak), skeleton 0xF5.
- Anims 0xA2C..0xA2F in lv6.
- No code tests id 0x4F. Model 0x609 only gets loop sound 0x365
  (`sub_00158CB0`).
- Scene 6 has 8 spawn records of class 0x10000000:0x4000, including 'CR01'.
  So it is INFERRED to use the securibot handler family, with anims chosen by
  data.
- No push or suck code found. Probe P1.
- **Wave 2: deferred.** CONFIRMED: its one record 'TPCB' (CR01 is a
  securibot) has lo 0x10080000 -> `sub_002682A0`, a scripted handler that
  reads the scene, tagged objects and level events. Anims are 0xA2D..0xA30
  (`cleanbotfor/death/rotlft/rotrht`), played by no code by id.

### 12. Skies

Every story sky with a model in a pak is already offered by the sky-music area
(records 5, 35, 39, 41..52, 56, 57; [mapmaker-sky-music.md](mapmaker-sky-music.md)).
The mission 7 city sky is record 52 `skyTS3nf1_xbox` (model 0x790, l_6_st /
l_66_ch). The rest of the 0x755..0x79F names have no model in any pak.
Nothing left to add.

### 13. Mission 7 lasers, panels and alarms

**Models.** All in `story/l_6_st.pak` (10.4 MB):
- lasertrip 0x4E0 / lasertripout 0x4E1: 1,948 B each, type 0x8000000
- lasercontrol 0x4E2: 2,356 B / lasercontrolout 0x4E3: 2,616 B
- camera1 0x4F2: 13,908 B, a class-0x200 gun
- the beam texture is resource 0x33E (not located)

**Lasers** (init 22 `sub_0020C300`):
- update `sub_00209680`, draw `sub_00209AC0`, 0x18-byte state in obj+0x7C
- each frame a ray (`sub_00088C60`) runs along the beam; a player hit calls
  `sub_00209270` -> alarm `[record+0x28]`
- a pair is two facing props, and the second beam is suppressed
- an NPC near the beam switches it off for 1 s

**Panels:**
- damage `sub_0020B8C0`
- damage type 0x2F (INFERRED "use") sets up to 5 tag-linked lasers
  (`sub_00240070(tag, out, 5)`) to flicker and then off
- destroying the panel turns them off at once
- slot 0 `sub_00209640` raises the alarm and completes 'OBVS' (null-safe)

**Alarms:**
- table `0x0078B2E0`, 0x20-byte rows, at most 18 (`sub_0025F9C0`), cleared
  in `sub_0025F6A0`
- rows come only from the scene-6/scene-4 descriptors (+0x9C/+0xA0)
- raise `sub_0025FA50`: HUD flag, AI alert to a group mask (story mode),
  alarm spawns (descriptor +0xBC)
- the camera raises after `[0x4E8CEC]` ticks (0x00246942)

**Wall panels** (`wallpanel`, init 25 `sub_00370B20`) are terminal screens,
not the laser panels. Their pool `[0x0076C270]` exists only in scenes 6, 17
and 22, and is NULL in scene 100 (INFERRED crash if placed).

**Port.**
- mount l_6_st or the four models
- stand-in records with a logic tag and alarm id
- register alarm rows in scene 100
- choose the alarm consequence (HUD + AI alert; no fail)

### 14 and 16. Mission 8 robot spiders and "rail cameras"

**Ids.** 0x4A `railbot_rail` (0x620, 31,524 B) and 0x50 `railbot_ground`
(0x621, 80,500 B), both in chr.pak; skeletons 0xF7/0xF8.

**Behaviour:**
- Class 0x2000 -> `sub_0037B720` (0x0027DCDF), messages `sub_0037B1F0`
  (event 5 frees the record).
- Anims in `stream/lv22/anim/lv22.pak` (610,832 B, 73 entries): drop,
  walk, rotjump, jump, fire, death.

**Pool:**
- `sub_003796B0` builds 15 x 0xA0 records at `0x00772B50` (scene-22 case
  of `sub_0025F720`, 0x0025F787); reset `sub_003796E0`.
- Each bot pops one with **no NULL check** (0x0037BA0F..0x0037BA27), so in
  scene 100 the first bot writes to address 0 (INFERRED crash).
- Fix: allocate the pool (or a bigger one through `sub_00156DB0`) in a
  scene-100 `sub_0025F720` wrap, plus a restart reset.

**Rails:**
- `sub_00379730(id)` reads descriptor +0x54/+0x58 rows {FourCC, u16 count,
  u16 flags (1 = loop)} and points at +0x5C/+0x60 (0x18 B each).
- nearfuture2 has 15 paths / 233 points (PRB1..6 loops, PSB/SPG single
  points). The path id is spawn record +0x28.
- MapMaker version: convert a Story AI's patrol points into a path row.

**"Rail camera".** It has no object of its own. The player pilots a rail bot
from a wall-panel terminal (`sub_0037ACB0` -> `sub_0036CCD0`/`sub_00370D80`,
HUD `consoleiconnearfuturerailbot`); without the terminal pool it returns 0
safely (0x0037ACC7). Drop it, or treat it as part of wall terminals (very hard).

### 15. Mission 8 mutants

- The only mutant character is 0x59 mutantsplitter "The Freak" (Arcade row
  74, chr.pak). It is already placeable with stock AI.
- Story class 8000:0 -> `sub_00264A10` (0x0027DDB2): no level state.
- Anims 0x76B..0x770 (walk, electric attack, leap, swipe, hit react) in
  lv22.pak.
- `chrs/dam_mutant` 0x571 has no model in any pak.
- The lv22_3 slice anims belong to the auto-slicer set piece.
- **Wave 2 (2026-11-07, fixture only): [story creatures](mapmaker-creatures.md), row 163.
  CONFIRMED: MT06..MT19 are 0x59 with 0x8000:0x10000000 (SDM1..4 use the
  slicer class 0x10000). 0x59 is an Arcade row, so the row stores carrier
  0x30 made with 0x59's models. lv22 entries 41..46.

### 17. Followers

Story partners (story table +0xE):

| partner | id | missions |
|---|---|---|
| soldier | 0x4B | 1, 12 |
| Captain Ash | 0x08 | 2 |
| Tipper | 0x00 | 3, 4 |
| Jo-Beth | 0x19 | 5, 6 |
| Amy Chen | 0x44 | 7, 8 |
| R-110 | 0x2D | 9, 10, 11, 13 |

R-107 is 0x90 "R One-Oh-Seven", Arcade row 129, not a partner (INFERRED: the
user means R-110). All of them are Arcade rows, so they can be placed today.

The buddy is found by `sub_00277FE0` only in story mode with
`[0x00790950] == 1`. It returns the first actor whose spawn record +8
matches the scene's tag (`sub_002776F0`):

| tag | buddy | scenes |
|---|---|---|
| 'CPTA' | Ash | 4 |
| 'HTIP' | Tipper | 11, 18 |
| 'JBTH' | Jo-Beth | 1, 17 |
| 'ACHN' | Amy | 6, 22 |
| 'R110' | R-110 | 5, 9, 14, 81 |

Scene 100 gets no buddy. The buddy feeds friendly fire (`sub_0015A540`), the
enemy target list (`sub_0027E6C0`), state 0x40 `sub_00276660` and the zone
triggers.

**The follow movement itself was not located** (INFERRED: generic same-team
AI plus level-script orders). MapMaker's class 0x11000000 includes patrol bit
0x1000000, which heads for player 1's pad when the AI has no route: a partial
follow.

Effort: placing is cheap. A real buddy (wrap `sub_00277FE0`, 28 callers, one
buddy, single player only) is hard. Several followers is very hard.

### 18. Machine Wars "Prometheus" robots

Names (CONFIRMED):
- PROMETHEUS SD/7: guard robots 0x12, 0x36, 0x75, 0xA9
- PROMETHEUS SK/8: rocket robots **0x62** (set 10) and 0xAA

Behaviour is chosen by id through the robot list `sub_000D6DD0` (22 ids), not
by a creature class. Their anims are `pak/anim/animrb.pak` (104,288 B,
`bigbots*`/`fastbots*`), which is already mounted in scene 100 (switch at
0x001C884A). 0xA9/0xAA are already placeable (preset 7). 0x62's model has the
same size as 0xAA's (114,004 B; INFERRED same mesh). So only picker rows are
needed, in the realbosses/tank synthetic-row pattern.

**Wave 1 (2026-11-07, fixture only).** Picker rows in realbosses'
`rbs_extra`: 159 "Prometheus SK/8" stores 0x62 (preview Arcade 0xAA), 160
"Prometheus SD/7" stores 0x36 (preview Arcade 0xA9). 0x12 and 0x75 are not
used: they are the castle tank and mothership placeholders. Both ids are in
the robot list (byte table 0x000D6E00) and have no Arcade row (decode test
`test_prometheus_rows_data`). No play hook: the stock robot AI runs them.
Several per map: yes (ordinary Story AIs).

### 19 and 20. Vehicles: Machine Wars tank and Scotland the Brave truck

**Stock vehicle system** (CONFIRMED):
- Class 0x400000 (`sub_001829D0`); create `sub_000A6F90` + `sub_00182A20(obj, rec)`.
- Def table `0x004E84A8 + rec[+0x24]*0xD8`, builder at def+0xB0:
  - 0 castle truck `sub_00180BA0`
  - 2 future tank `sub_00181D40`
  - 3 depot jeep
  - 4/5 buggies
- 4 vehicle blocks of 0x970, popped by `sub_00182A20` **without a bound check**.
- Vehicle turrets come from templates `0x004E8300 + i*0x8C` through
  `sub_00180920`/`sub_000B3B80`, so they use the 80-slot turret pools:
  0 Vickers 0xE, 1 tank main gun 0xC, 2 tank AA gun 0xD.

**MapMaker** already places vehicles: catalogue item 0x2B, category 35, cap 4
(`0x00524F2C`). The fill handler (0x00241CC4..0x00241D54) writes def 3 or 5
and +0x30 = -1. The tank-track and seat adapters exist.

**Future tank** (stream/lv5/prop1.pak, 2,595,936 B):
- Models 0xB (72,852 B), 0xC, 0xD, gibs 0x54D..0x54F.
- farfuture1 records: 'TANK'/'TNK2'/'TNK3' with def 2, +0x30 = 1, +0x34 = 2.
- Driver seat type 0, gunner type 3. A player vehicle; no AI driver found.
- `sub_00174120` runs its body (gibs) only in scene 5 (0x00174152).
- The castle boss tank (prop 0x281, class 0x8000000, own AI) is unrelated.

**Truck** (stream/lv4/prop3.pak, 1,356,736 B):
- Model 7 (158,248 B), def 0. Rec +0x30 >= 0 adds the Vickers gunner seat.
- Seat anims 0x78..0x83 (the jeep shares 0x7F..0x82). Their pak is INFERRED
  stream/lv4/anim.
- The story ride ('TRCK') is scripted. `castle/npctruck` 0x287 (256,892 B)
  is a scripted animated prop; not worth porting.

**Work for both:** mount the prop pak, a hook on `sub_0033DDA0` or the fill
for def/model/templates, and the scene gate for the tank's gibs.

**Wave 3 (done, fixture only):** [story vehicles](mapmaker-vehicles.md).
The vehicle item's options choose tank / truck + gun / truck (record byte
+0xC). Hooks: `sub_0033DDA0` (def at preload and fill), `sub_00182A20`
(story record fields: tank +0x28 4, +0x2C 4, +0x30 1, +0x34 2; truck 0, 1,
0 or -1), the gun templates preloaded with `sub_000B3B20`, the stream paks
mounted index-only (models and textures are in the same pak). The
truck's seat anims are all in `pak/anim/animcar.pak`, which scene 100
mounts. The scene-5 wreck is left alone: in scene 100 a tank is not
destroyed, as the buggies. The limits' raise of category 35 to 8 was
removed: the pool has 4 blocks and no bound check.

### 21. Mission 1 big turrets

- Model 0xE6 (twin 0x7B1) `level29/props/prisonfixedgun`, type 0x200,
  88,716 B, l_2_st / l_26_st only.
- intro.raw: 3 turret records ('OHDR' on the first), flags 0x10040 (0x40 =
  manned), gun type 7, pitch -30..60, yaw -83..83.
- Generic turret code: `sub_0006CE80` -> `sub_000B3B80` -> `sub_00245990`,
  manned path `sub_000B74C0`, update `sub_00244580`. This is the same path
  as MapMaker category 29, which uses gun types 8..22.
- Story link: the outro ship damage handler `sub_002193A0` takes hits only
  from 0xE6, and only in scene 26.
- Port: a category-29 variant with model 0xE6, gun type 7, flags 0x10040 and
  the intro limits, plus a model mount.
- **Wave 1 (2026-11-07, fixture only):** area `storyprops`
  ([mapmaker-storyprops.md](mapmaker-storyprops.md)). A placed fixed gun
  (0x32..0x34) gets "Model: MapMaker gun / Prison gun (mission 1)" after its
  weapon choice (record byte +0xC). In play the turret record gets model 0xE6
  and gun type 7 (cat. 29 already writes 0x10040); l_26_st.pak (325 files,
  every shared file identical elsewhere) is mounted index-only when used. The
  intro's aim limits are not copied: the stock range option stays.

## Shared building blocks

1. **Extra pak mount + anim keep-list.** The pak mount table has 64 entries
   (paktable.json), and `pc_mapmaker_animslots.inc` registers only the listed
   ids and never passes slot 940. One table row per asset: {pak, anim ids,
   model ids}. Mount only when the map uses the asset. Measured Level heap
   free is about 11-14 MB. Small paks: lv60 (816 B), lv17_3 (67 KB), lv6
   (0.83 MB), lv22 (0.61 MB), lv17 (0.33 MB), lv17_4 (0.40 MB),
   lv5/prop1 (2.6 MB), lv4/prop3 (1.36 MB). Large: l_2_st (9.2 MB), l_6_st
   (10.4 MB), l_17_st (15.5 MB). For large paks prefer a small pak that holds
   the same file (l_64_ch), or skip the prop.
2. **Generic story creature deployer.** Each row holds:
   - character id;
   - class lo/hi, written into spawn record +0/+4 at AI start the way the
     robot port does (`sub_0015B830`); `sets_spawn_masks` already covers
     slots 11-20;
   - anim pak + keep-list (block 1);
   - optional scene-100 init/reset (`sub_0025F720`/`sub_0025F6A0` wraps,
     e.g. the rail-bot pool);
   - a synthetic picker row (`sub_002BE600`/`sub_002BE300`, rows 150+, the
     realbosses pattern) with a name.

   It covers items 6 (ghost), 9 (securibot), 10 (cleaning bot), 14 (spiders,
   with pool and paths), 15 (mutant), 18 (picker rows only), and a visual-only
   item 5. Ragbugs (8) need the pit fight.
3. **Prop variant with a stand-in record.** A MapMaker object category entry
   that creates a prop-table model with a fake level record (tag, flags,
   item value at +0x28). This lets level init functions (init 20, 22) run as
   they do in story levels. It covers items 2, 13 and 21, and the wraith
   object (3).
4. **Vehicle def override.** One hook on the vehicle fill
   (`sub_0033DDA0`/0x00241CC4) chooses def, model and turret templates. It
   covers items 19 and 20.
5. **Allow-list on the unlock check** (`sub_001CD9D0` at 0x002BFB08) for
   item 1/11.
6. Existing: boss-bar mechanism, realbosses instance swapping (for item 7),
   restart audit rules (limitless-bosses.md): every new area needs a
   restart section.

## Recommended order within the agreed waves

- **W1 (skies, weapons, barrels, turrets):**
  1. Weapon allow-list for TNT + time grenade (block 5). Cheap.
  2. Intro power-cell barrel (block 3 + l_64_ch, already mounted). Cheap.
  3. Intro turret 0xE6 as a category-29 variant (block 3 + a model mount).
     Cheap.
  4. Skies: nothing to do (already complete); record that.
- **W2 (creatures):**
  1. Build block 2 first with the ghost class (self-contained, lv17.pak).
  2. Then the mutant 0x59 + spiders 0x4A/0x50 together (one lv22 mount, pool
     init, paths from patrol points). Wave 2: block 2 built
     ([story creatures](mapmaker-creatures.md)); ghost, securibot and mutant
     rows done; spiders and the cleaning bot open.
  3. Then securibot 0x4D + cleaning bot 0x4F (lv6).
  4. Then Prometheus picker rows (no pak).
  5. Wraith last in this wave (one per map, global system).
- **W3 (skeleton variant, robot tank, truck):** vehicle def override (block
  4): tank first (the track adapter exists, gib gate), then the truck. Then
  the pit skeleton with ragbugs (realbosses selector, lv17_4).
- **W4 (lasers, panels, rail cameras, followers):** lasers/panels/alarms
  (block 3 + alarm rows). Followers: placeable today; buddy behaviour after
  probe P5. Rail cameras: drop, or defer to wall terminals.
- **W5:** cut-out targets with a MapMaker score mode.

First implementation tasks: (1) weapon allow-list (TNT, time grenade) plus
the P2 time-grenade probe; (2) power-cell barrel; (3) intro turret variant;
(4) creature deployer with the ghost class; (5) lv22 creatures (mutant +
spiders).

## One-run probe logs that would settle the main unknowns

| probe | run | log | settles |
|---|---|---|---|
| P1 | story scenes 6, 17, 22 with the existing `TSFP_MAPMAKER_CREATURE_PROBE=1` (`pc_mapmaker_bosses.inc`) | per new actor: char id +0x48, class +0x10/+0x14, model, tag, position; every `sub_0005F460` anim id for 0x609 | ghost and electric-zombie ids, cleaning-bot class/anims, RAG0..5, rail-bot flags |
| P2 | scene 100 with 0x29/0x2F forced into the set, and scene 6 as reference | `sub_000EFBD0` type; `sub_0005A130` for grenade/tnt paks; for 10 s after explosion 0x2B: `[0x4E7944]`, `[0x4E794C]`, `[0x7497B4]`, and the speed of the player and bots inside/outside 7 units | where the slow motion lives and whether it works in MapMaker |
| P3 | front end / editor | `sub_001CD9D0(id)` at return 0x002BFB08 | the offered weapon set for the real profile |
| P4 | scene 100 load with lv60/lv17/lv17_3/lv17_4/lv6/lv22 mounted | `sub_000A5E40` for 0x1DA, 0x242, 0x6CB, 0x53D, 0xE6; texture 0x292; anim slots registered and headroom | asset resolution and slot budget |
| P5 | mission 3 or 7 | once a second: `sub_00277FE0` result, its +0x10/+0x14, team +0x84, goal pad +0x684 vs player pad +0x678, state handler | the follow logic |
| P6 | scenes 1 and 60 | `sub_0023FF20('WRTH')`, `[0x0078B6D4]` changes, spirit positions, room list 0x0078B6A0, player +0xF54, actions 0x2C/0x2D, 'VDMN'/'VNMN' | wraith port inputs |
| P7 | scene 6 | `sub_0020B8C0` damage type, `sub_00209270`/`sub_0025FA50` arguments, alarm rows after load; in scene 100 `[0x7DE455]` | laser/panel/alarm port |
| P8 | scene 100 with a forced def-2/def-0 vehicle | `sub_00182A20` def, `sub_00180920` templates, seat types, missing anim ids, `sub_00174120` early return | vehicle ports |
| P9 | a stock cut-out challenge | `[0x7DE470]`, `[0x7DE458]`, table count and score each second, `sub_0022E5C0`/`sub_0022E6E0` calls | score-mode design |
| P10 | scene 22 | `sub_00379730` arguments/results, `[ai+0xAA0]+4`, rec +0x58 & 0x8000000 | spider rails |

## Open points

- The time-grenade slow-motion effect (P2).
- The meaning of the weapon variant flag (twins).
- The ghost and electric-zombie character ids (P1).
- 'VDMN'/'VNMN'.
- Damage type 0x2F as "use".
- The follow movement.
- What `[0x7DE470]` = 3..5 switches besides the cut-out update.
