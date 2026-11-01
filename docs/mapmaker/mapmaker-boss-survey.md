# MapMaker: survey of every story boss (2026-11-01)

User goal: every story boss working in MapMaker story play. This survey
lists, for each of the 13 story missions, what the player fights that looks
like a boss (a boss bar or a dedicated handler), what drives it, what it
needs and whether it can work in a MapMaker map (scene 100).

Sources: `private/pipeline/disasm/asm/text.asm`, the US XBE data, the pak
indexes (CRC32 of asset paths) and the prop table `prop_all_xbas.raw` read
from `pak/arcade/l_100.pak`. Research basis: `logs/limits/features/real-bosses.md`
and `story-enemies.md` (main checkout, ignored). Read only: nothing was built
or played for the survey. CONFIRMED = seen at the address or in the data;
INFERRED = reasoning.

Mission order: story table `0x004D1AC0` (13 x 0x44). Scene = `[0x0079094C]`.

## Summary

| mission (scene) | boss or boss-like encounter | status in MapMaker |
|---|---|---|
| 1 intro (2) | **Timesplitter mothership**, prop 0x539 | **ported here** (`pc_mapmaker_mothership.inc`, [mapmaker-mothership.md](mapmaker-mothership.md)) |
| 2 castle (4) | castle tank, prop 0x281 | ported (`pc_mapmaker_tank.inc`) |
| 3 depot (18) | none (Tipper escort meter) | nothing to port |
| 4 train (11) | Khallos jetpack chair, prop 0x376 | another session (in progress) |
| 5 mansion2 (1) | giant trophy monster id 0x15; giant skeleton id 0x28; mansion worm id 0x2C | trophy monster and skeleton ported (`pc_mapmaker_realbosses.inc`); worm: another session |
| 6 catacombs (17) | **giant skeleton, pit-fight variant** (id 0x28, `sub_0029E0A0`) | partial at best; not taken (shares id and hooks with the mansion skeleton port) |
| 7 nearfuture1 (6) | none (Amy Chen escort meter) | nothing to port |
| 8 nearfuture2 (22) | none (Amy Chen escort meter); rail bots are enemies | nothing to port as a boss |
| 9 farfuture1 (5) | none (the giant robot here is player-steered; Cortez meters) | nothing to port |
| 10 farfuture2 (14) | giant robot id 0x42; split Crowe id 0x46 | ported (`pc_mapmaker_robot.inc`, `pc_mapmaker_crowe.inc`) |
| 11 nemo (9) | none | nothing to port |
| 12 outro (26) | none (past-Cortez escort meter) | nothing to port |
| 13 nemoend (81) | Crow walker id 0x64 | ported (`pc_mapmaker_realbosses.inc`) |

So, apart from the sessions in progress, the only bosses left are the intro
mothership (ported in this change) and the catacombs variant of the giant
skeleton.

## Every writer of the story boss focus (CONFIRMED)

The bar `sub_0014CC80` draws for the focus `[player+0xFF4]` (player `[0x007B0C7C]`,
the player array `[0x007B0C48]` stride 0x1584, or farfuture1's pilot
`[0x0078D02C]`). Level code is attributed through the level descriptors
(init/update pointers stored just before each `data/*.raw` name pointer) and the
callback index tables the level loader resolves (`sub_00069F30`: actor
records +0x6C/+0x78/+0x7C -> tables 0x004D29C8/0x004D2A5C/0x004D2A88; logic
actions 0x004D2B10). Some writers are reached only through those tables,
so a plain cross-reference search misses them.

| writer | level | focus |
|---|---|---|
| `sub_000B8C60`, `sub_000D2D50`, `sub_000D2FD0`/`sub_000D4F50`, `sub_0014F0B0`, `sub_0023EFE0` | generic | timed interaction target, player init/swap, clears |
| `sub_001EA780` (object event #3) | train | an actor while alive (INFERRED Tipper) |
| `sub_001EC300` (from the train update `sub_001F5A10`) | train | gas-carriage hazard meter: 'GTLT' light 0x371 / 'SHUT' shutters 0x330 |
| `sub_001EEBD0`, `sub_001EF840` | train | Khallos jetpack 0x376 |
| `sub_001F9CE0`, `sub_001F9D50` (logic actions 0x52/0x53) | depot | the buddy from `sub_00277FE0` (Tipper's escort meter) |
| `sub_0020B630` (update) | nearfuture1 | Amy Chen (timed "save Amy" drain, `sub_00208AF0`) |
| `sub_00210270`, `sub_00210680` | nearfuture2 | Amy Chen |
| `sub_00213730` (actor handler), `sub_00213900` (from update `sub_00213F50`) | catacombs | Jo-Beth; the 'JBTH' defend-wave meter |
| **`sub_00217050`** (0x00217947) / `sub_002193A0` (clear 0x00219683) | **intro** | **the mothership 0x539** while its health > 0; then 'ZCTP' (0x00218576) |
| `sub_0021BBB0` (actor handler) | farfuture1 | Cortez future/past selves 'ZCTF'/'ZCTP' |
| `sub_00220600` (update) | farfuture1 | the player-steered giant robot 'RGIB' |
| `sub_00221B60` (actor handler) | outro | past Cortez 'ZCTP' |
| `sub_002532D0`, `sub_00255550`, `sub_00255E70` | castle | tank 0x281 |
| `sub_00280FA0` | mansion | trophy monster 0x5BE |
| `sub_0029002C` | farfuture2 | split Crowe 0x5E8 |
| `sub_00293730` | farfuture2 | giant robot 0x5F2 |
| `sub_002998E0` | nemoend | Crow walker 0x637 |
| `sub_0029CEF0`, `sub_0029E0A0` | mansion, catacombs | giant skeleton 0x5E9 |

HUD cases (`sub_0014CC80` jump tables 0x0014D0E8/0x0014D0F8/0x0014D158/0x0014D174):
bosses 0x281, 0x376, 0x539, 0x5BE, 0x5E8, 0x5E9, 0x5F2, 0x637; ally or objective
meters 0x330/0x371 (train gas), 0x572 Tipper (label 0xDA3), 0x595/0x596 Cortez
(0x1638 with record 'PRAP', else 0x1639), 0x5CB Jo-Beth (0x1293), 0x5FF Amy Chen
(0xDC6). Corrections to the earlier research: 0x572 is Tipper, not Cortez;
`sub_00217050`/`sub_002193A0` are intro code; `sub_0021BBB0`/`sub_00220600`
are farfuture1; only `sub_00221B60` is outro.

Story level init `sub_0025F720` (switch 0x0025F7D8) has cases only for scenes
1 and 17 (skeleton `sub_00287E00`), 6 (sound), 14 (robot and Crowe), 22
(`sub_003796B0`, the rail-bot pool) and 81 (walker). CONFIRMED.

## Per mission

### 1 intro (scene 2): the Timesplitter mothership. Ported here.

| | |
|---|---|
| actor | prop 0x539 `INTRO_TIMESPLITTERS_MOTHERSHIP` (`intro/intro_mothership`, type 1, no animation group), init index 20 = `sub_0021A710` (case 0x0021AADA); wreck 0x53A `intro_mscrash`. Level tag 'MTHS' (CONFIRMED) |
| behaviour | update `sub_00217050` (slot 3), damage `sub_002193A0`, slot 10 `sub_00215B50`; health 250 (+0x1CC/+0x1D8); states +0x1A0 (table 0x00218630): 0 fly to the fixed point (-500, 50, -220), 1 hover, 2 attack (focus, homing shots `sub_000EA9C0(0x32, ..)` at a random player inside the box x -67..-56, z -88.04..-47.8), 3 fall, 4 crash explosions, 5 wreck 0x53A and remove (CONFIRMED) |
| start | logic action 0x5B `sub_00217030` sets state 2 on the 'MTHS' object (CONFIRMED); the level script calls it |
| assets | model files only in `story/l_2_st.pak`, `story/l_26_st.pak` and `challenge/l_64_ch.pak` (not in chr.pak). Every file `l_64_ch.pak` shares with any other pak is byte-identical (checked against all paks), so mounting it kept open adds only its models |
| per-level state | none in `sub_0025F720`; statics 0x0075C0DC..E4, 0x0078D500.. (scorches) |
| level dependencies | 'MTHS' logic item at death (`sub_00234CD0`, null-safe), 'ZCTP' focus at the wreck (null-safe). No level event. Absolute numbers: the approach point, the firing box (constants 0x0049EA88..0x0049EA94, each read only here), the crash floor 6.6 (0x0049E324, shared) and the crash dust height -45.5 (0x0049EA84, read only here) |
| verdict | **feasible** as a hovering boss: spawn at a placeholder Story AI, skip the approach, rebase the box and the dust height, shift the fall to the map floor. See [mapmaker-mothership.md](mapmaker-mothership.md) |

### 2 castle (scene 4): castle tank. Ported (`pc_mapmaker_tank.inc`).

### 3 depot (scene 18): no boss
The only focus writers are Tipper's escort meter (logic actions 0x52/0x53).
Id 0x65 (newmonkey, models 0x638/0x623, chr.pak) is an ordinary monkey-class
creature. No `sub_0025F720` case. Nothing to port as a boss.

### 4 train (scene 11): Khallos jetpack chair (another session)
Prop 0x376, init 30 -> `sub_001F4E00` case 0x001F5281, `sub_001EEA30`/`sub_001EEBD0`,
anchored to absolute train coordinates. The gas-carriage meter (`sub_001EC300`)
is a hazard, not a boss. Id 0x04 is `CHR_TRAIN_HENCHMAN_SCALED` (0x58C, chr.pak,
human anims): a normal enemy.

### 5 mansion2 (scene 1)
Trophy monster (0x15) and giant skeleton (0x28): ported. Mansion worm (0x2C):
another session.

### 6 catacombs (scene 17): giant skeleton pit fight (not taken)

| | |
|---|---|
| actor | id 0x28, model 0x5E9, class 0x20000; `sub_0029FE00` takes `sub_0029E0A0` outside scene 1 (17 states, table 0x0029FDBC); message handler `sub_0028C560` ignores hits |
| anims | `princess_cat_cs` 0x23B .. `feeble_throw` 0x242 only in `pak/stream/lv17/anim/lv17_4.pak` (395,440 B, table 0x0048E298) |
| state | the same `[0x0078B1EC]` block (`sub_00287E00`); health state +0x4274 = 8.0 |
| fight | the boss cannot be hurt; it spawns a gas bottle 0x24A at a bone (damage callback `sub_0028C2F0`: 2.667 per hit of type 0x21, so 3 hits), plus collision 0x249, bandage 0x24B and rack 0x246 props (`sub_000A6F90`). A hostage 'PRJB' and a timer (+0x423C, second meter labels 0x1293/0x1294) lead to the failure `sub_00212160` |
| level dependencies | logic item 'NMME' (`sub_00234190` 0x0029ED82, gated by `sub_00234270`: missing = the fight stalls in state 1, like the walker); hard-coded catacombs coordinates (rack spawn (55.6, -11.28, -77.8), bottle (58.8, .., -96.88), more at 0x0049F478..); level event 2 at death (0x0029FC33) |
| verdict | **partial, 1-2 weeks**. It shares the character id and the per-level state with the mansion skeleton, and `pc_mapmaker_realbosses.inc` already sends id 0x28 to the mansion variant through its `sub_0029FE00`/`sub_0028D0A0` hooks, so it needs a separate selector and a change in that area. Without the pit, the rack and the hostage it reduces to "shoot the bottle three times". The catacomb props' models were not located |

### 7 nearfuture1 (scene 6): no boss
`sub_0020B630` focuses Amy Chen (timed drain). Ids 0x4D securibot (0x607,
class 0x4000 `sub_0026B7A0`, anims in lv6.pak) and 0x4F cleaning bot (0x609, an
object behaviour) are enemies or props.

### 8 nearfuture2 (scene 22): no boss
Amy Chen meter (`sub_00210270`/`sub_00210680`). `sub_003796B0` is a pool of 15
rail-bot records; ids 0x4A/0x50 (rail/ground rail bot, class 0x2000 ->
`sub_0037B720`, anims in lv22.pak) are enemies. The ground rail bot is a
possible future story enemy (not a boss): `sub_003796B0` in scene 100, lv22.pak,
spawned active.

### 9 farfuture1 (scene 5): no boss
The giant robot here ('RGIB', id 0x42) has the scripted-actor class 0:80000
(`sub_002682A0`) and is steered by the player through the control panel 0x55D;
its bar is its own health while piloted. Cortez meters ('ZCTF'/'ZCTP'). Id 0x62
rocket robot is an enemy. Big props 0x556 (bigflyer) and 0x553 (big sentinel, no
model in any pak) have no bar.

### 10 farfuture2 (scene 14): giant robot and split Crowe. Ported.

### 11 nemo (scene 9): no boss
No focus writer in its code, no boss class, no HUD model.

### 12 outro (scene 26): no boss
The bar is the past-Cortez escort meter (`sub_00221B60`). The mothership model
is also in its pak and `sub_002193A0` has a scene-26 branch (0x002195E4: double
damage from an attacker with model 0xE6), but no outro code drives a fight
with it (INFERRED: a set piece).

### 13 nemoend (scene 81): Crow walker. Ported.
