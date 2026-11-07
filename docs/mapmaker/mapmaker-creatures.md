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
