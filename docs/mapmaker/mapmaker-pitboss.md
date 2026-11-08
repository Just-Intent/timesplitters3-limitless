# MapMaker pit skeleton: the mission 6 pit fight and its ragbugs

Status 2026-11-07, playtest fix after .62 below (story assets wave 3, items 7 and 8 of the
[inventory](mapmaker-story-assets.md)): implemented in
`src/interface/pc_mapmaker_pitboss.inc` (area `pitboss`) plus two rows in
`src/interface/pc_mapmaker_creatures.def`, fixture-tested
(`tests/mapmaker/mapmaker_pitboss.c` + `tests/mapmaker/test_mapmaker_pitboss.py`:
40 mutations, decode, US XBE data and story pak checks). **Not wired, not
built, not played**: the wrappers are the integrator's (list below). Switch:
`TSFP_MAPMAKER_LIMITS=1`; `TSFP_MAPMAKER_CREATURES=0` keeps both rows on the
MapMaker bot AI. No guest-code table, no fork chunk, no save change.

CONFIRMED = decode, XBE data or story pak (the test names the site);
INFERRED = reasoning, needs a playtest.

## What the player gets

| row | label | stores | made with | class (hi:lo) | anims (lv17_4.pak) |
|---|---|---|---|---|---|
| 164 | Pit Skeleton | 0x31 | the giant skeleton 0x28's model 0x5E9 | 0x20000:0x10000000 -> `sub_0029FE00` -> `sub_0029E0A0` | entries 16..27 (0x23B..0x246) |
| 165 | Ragbug | 0xB0 (its own) | chrs/ragbug 0x6CB | 0x200000:0x10000000 -> `sub_0028B610` | entries 24..27 (0x243..0x246) |

Both classes are the catacombs level's own records ('PRIN', 'RAG0'..'RAG5',
CONFIRMED). The mansion **Giant Skeleton** row (151, id 0x28) is unchanged:
`pc_mapmaker_realbosses.inc` no longer claims a model-0x5E9 actor whose
character is 0x31 (`RBS_PIT_ID`). Preview: the Arcade skeleton cowboy (0xA6)
and the Arcade Timesplitter (0x4E). A stock build plays the map with bots
on a soldier (0x31) and the ragbug model, and an empty name in the editor.

Play: the skeleton rises with `princess_cat_cs` (the catacombs intro), then
fights as in the pit: roars, fire attacks at the rim, throws, launches its
ragbugs. It cannot be hurt; the **gas bottle** it carries (later set down
beside it) is the only target: health 8.0, a type-0x21 hit takes 2.667,
another 0.8 x its damage (`sub_0028C2F0`). At 0 it dies (`princess_pit_death`
0x241), its ragbugs explode and are removed, and once a fade ends (about 17 s,
or sooner when a player stands 25 above its floor) the props and the AI are
removed the stock way (`sub_0015DDA0`). Several per map: up to 4, each with
its own block, statics, arena and up to 6 ragbugs.

## The story fight (CONFIRMED)

`sub_0029E0A0`, 17 states (jump table 0x0029FDBC), state in the shared
skeleton block `[0x0078B1EC]` (0x4C7C, `sub_00287E00`) and the particle
statics 0x0078B080.. (`pc_mapmaker_bossfx.inc`):

| state | what |
|---|---|
| 0 | boss pinned to (-11.8, -100.279, 56.295); 13 rim pads 0x4B5..0x4C1 looked up (`sub_0009CC40` -> +0x4BF0..+0x4C20); focus = boss; collision prop 0x249 and eye bandage 0x24B created; 'NMME' (`sub_00234190`) not triggered -> `princess_cat_cs` |
| 1 | bottle 0x24A at a bone once the intro is 0.315 in; 'NMME' triggered -> rack 0x246 at (-11.28, -77.8, 55.6), callbacks, rack stage 1, `feeble_throw` 0x242, state 6 |
| 2-13 | idle, fire (0x23E), raise (0x23D), roar (0x240, shake), ragbug launch (`sub_0028A0A0`), the bottle again at (-3.68, -96.88, 58.8) when gone, hostage eaten (12, 0x23F) |
| 14 | bottle health <= 0: explosion, 0x241, then every ragbug exploded and removed, focus cleared, level event 2 (0x0029FC33), stage 5 |
| 15 | stage 6 (`sub_0028B780`: fade or a player above y -75): props deleted, AI removed |
| 16 | hostage lost: props deleted, `sub_00212160` (mission failed), AI removed |

The rack timer +0x423C (seconds) lowers the hostage rack at 20/50/80 s
(`sub_0028B780`); at stage 4 it requests state 12. The hostage is the 'PRJB'
record's AI (`sub_0015BA10`; null-safe everywhere). Message handler
`sub_0028C560` (stock for every scene but 1): -1 to messages 1/2 (no hurt, no
death), 5 deletes the props. Ragbug class handler `sub_0028B610` only parks
the bug at (-12.73, -102.5, 62.42) and plays `ragbugflight`; its damage
callback resets its health (it cannot be shot down); the flock (slots +0x4A30
+ k*0x44, found by record tag 'RAGk' when empty) is flown by the fight
(`sub_00289F30`, `sub_0028A0A0`, `sub_0028AAA0`).

## What this area does

| hook | WRAP | what (scene 100 story, switch on) |
|---|---|---|
| mount | chain `sub_00062790` | set holds 0x31: `story/l_17_st.pak` and `storymem/l_17_stm.pak` kept open (`sub_000622D0(path, 0, 1)`, index only), 8 table slots kept free. Every entry they share with another pak is byte-identical there (test) |
| request | chain `sub_0015E7C0` | `sub_000A5E40` for 0x249, 0x24B, 0x24A, 0x246 |
| textures | chain `sub_00063900` | each read prop model the load pass left marked (request +2 & 0x10): `sub_00096220(path)`, mark cleared (as the story props area) |
| level | chain `sub_0025F720` | one block per pit Story AI (1..4) from the stock `sub_00287E00`, statics put back; the arena constants' stock values; `[0x0078B1EC]` = the first block only when it was 0 |
| ai_start | chain `sub_0015B830` | after the original: pit skeleton (char 0x31, class 0x20000) -> its block by spawn record (late block when more were placed), spawn point, Boss flag; ragbug (0xB0) noted |
| princess | chain `sub_0029FE00` | char 0x31: its block and statics in, the 34 arena constants moved by (spawn - boss point), `sub_0029E0A0()` directly (it pops the caller's return, as the tail jump), all back; after: timer held at 0 (state < 14), the 13 pad indices pointed at the instance's own pad records. No block or props not loaded: AI removed, logged |
| message | chain `sub_0028D0A0` | pit skeleton: `sub_0028C560()` with its block |
| logic | **NEW** `sub_00234270` | inside a pit call: at 0x0029EDA5 answers 0 (intro plays), at 0x0029EED1 1 once `sub_00059DD0` says the boss's animation finished |
| event | chain `sub_002274C0` | return 0x0029FC38 (event 2): dropped, logged |
| failure | **NEW** `sub_00212160` | return 0x0029FD15: dropped (not reachable with the timer held) |
| ragbug | **NEW** `sub_0028B610` | no pit skeleton this level: removed. Else the nearest placed skeleton with a free slot owns it; the park runs with that one's arena; its entity fills that flock slot before each fight call (bit 0 of +0x28 cleared as the tag lookup does), never again after the fight emptied it |
| callbacks | **NEW** `sub_00289720`, `sub_00289E20`, `sub_0028C1C0`, `sub_0028C390`, `sub_0028C4A0`, `sub_0028C2F0` | the owner's block (boss entity or prop; the damage record's +0 for the bottle), else the current or first one |
| particles | **NEW** `sub_00289080`, `sub_00288E20` | the current or first block (no entity) |
| restart | chain `sub_001CDD60` | before: props deleted (`sub_000A9070`), this map's lv17_4 anims noted (all carry 0x4000); after: mapped again if free, every block zeroed + `sub_00287860` in place, bugs unassigned |
| hud | chain `sub_0014CC80` (inside `pc_mapmaker_bosses_hud`) | focus = a pit skeleton: cleared for the original (no stock bar, no hostage meter) |
| meter | chain `sub_0013A400` | return 0x0014D264, a pit skeleton without the Sub-boss bar: none |

Arena constants (all read only by pit code reachable from the two entries,
test): the boss point 0x0049F498/94/0x0049F37C; the flight centre 0x0049F380
(+11.8, subtracted) and 0x0049F378 (+100.279, subtracted); the ragbug rest
point 0x0049F390/8C/88, park y 0x0049F3A4, heights 0x0049F398/94, "out of
the pit" y 0x0049F3A8; rack heights 0x0049F3AC/B0/B4 and spawn
0x0049F45C/8C/58; hostage drop y 0x0049F478; the bottle 0x0049F488/84/80;
four rim points 0x0049F34C..0x0049F374 and 0x0049E854 (`sub_00288540`,
x/y/z triples). Times, speeds and yaw limits are not moved.

Rim pads: the 13 catacombs pads (pad file entry 0x671173A1 of l_17_st.pak,
ids 0x4B5..0x4C1, test) are copied per instance (+0 id, +4 1, +8 0x1A,
+0xC position + offset, +0x18 yaw) into a block placed so that (record -
`[0x00715F2C]`) is a multiple of 0x20; the fight reads `table + index*0x20`.
MapMaker pad ids start at 0x3E9, so a map with 205+ pads would hand the
stock lookup a MapMaker pad: the area always overwrites the 13 indices.

Log lines (`[mapmaker] pitboss: ...`, 40 a level): `mounted pak/story/...
for the pit skeleton's props`, `requested the pit props`, `pit prop textures
registered ...`, `pit skeleton blocks allocated (first, count)`, `pit
skeleton N placed at (x, y, z): the catacombs arena moved by (dx, dy, dz)`,
`pit skeleton fight starts`, `pit skeleton fight state (skeleton, state)`
(24), `ragbug joins pit skeleton N's flock (slot k)`, `pit skeleton killed:
its level event dropped`, `level restart: pit skeleton blocks set up again
...`, and every removal (`... removed`) or refusal.

## Playtest .62: no bottle, ragbugs one at a time (fixed after .62)

User report (.62): "Bomb didn't appear in pit skeleton mouth like it's
supposed to, which is supposed to be its weak spot. Ragbugs didn't appear
immediately when they should have spawned, but seemed to appear randomly
around the pit skeleton one at a time." `m62.err`: mounts, requests, the
placement (arena moved by (164.80, 100.79, 354.71)), three ragbugs joining
and the fight cycling states 1, 6, 4, 10, 5, 7, 11; no line about the props;
`textures registered (models) (0x0, ...)`; skelvis: the skeleton itself
"hidden (listed rooms visible but not reached by their prop lists)" until
`bossroom` linked it through the camera room.

Cause (INFERRED from the log and the stock code; the new lines decide):
- **Drawing.** A MapMaker tile is one room one floor high. The bottle hangs
  at the skeleton's mouth (bone 0x29, about 10 up), the rack 22.5 up, the
  bandage on its eye, the ragbugs fly the circle above the pit: their boxes
  list no room or one above the reached rooms, so no drawn room's prop list
  holds them. The skeleton had the same gap (fixed after .45/.46 by
  `pc_mapmaker_bossroom.inc` and `pc_mapmaker_bigdraw.inc`), which covered only
  the giant boss models. The fight states show the props were made (state 6
  needs the rack; the fight never stalled), so the bottle existed but was
  never drawn; a ragbug showed only while its dive crossed a reached room
  ("one at a time"). Whether the stock hit test also needs a room for the
  bottle was not traced.
- **Release.** The stock launch `sub_0028A0A0` runs only in state 10 (after
  the first roar), up to 3 parked bugs per call; the bugs wait 2.2 under the
  pit floor (the park point) until then.
- `textures registered (0x0)`: the four models' requests had no deferred
  mark (the load pass registers them, or they were not read): the new
  per-model lines say which.

Fix:
- `pc_mapmaker_bossroom.inc`: a second pass adds the pit satellites (models
  0x24A bottle, 0x246 rack, 0x24B bandage, 0x6CB ragbug, any type) to the
  camera room's list when one of their listed rooms is reached, or when they
  list none and a giant boss was reached this view. `pc_mapmaker_bigdraw.inc`:
  the same models get the whole viewport as their scissor (the collision prop
  0x249 stays stock). Fixture cases and mutations in both tests.
- Release: once the fight proper runs (state 6..13) and every placed ragbug
  has joined (or after 300 frames), `sub_0028A0A0(boss)` is called twice
  with the instance's block and arena: all its ragbugs (up to 6) leave the
  park together, once per fight (the stock state-10 launches follow).
- Logs: `prop model 0x..: loaded/not loaded, file read/not read, textures
  ...` per model at the texture queue; `pit skeleton N: gas bottle 0x24A made
  (object, model, listed rooms, first room) at (x, y, z)` (and the
  collision, rack, bandage) when each appears; `pit skeleton fights without
  its gas bottle` once; `pit skeleton N: flock complete, M ragbugs released
  together (parked, still joining)`; bossroom lines for the satellites.


```
WRAP sub_00062790 -> pc_mapmaker_pitboss_mount        (chain with storyprops/mothership/tank/khallos mounts; after the original)
WRAP sub_0015E7C0 -> pc_mapmaker_pitboss_request      (chain with creatures/khallos/tank/mothership request)
WRAP sub_00063900 -> pc_mapmaker_pitboss_textures     (chain with storyprops/skymusic textures; before the original)
WRAP sub_0025F720 -> pc_mapmaker_pitboss_level        (chain with realbosses/robot/crowe/worm level; order free)
WRAP sub_0015B830 -> pc_mapmaker_pitboss_ai_start     (chain; acts after the original on 0x31/0xB0 only; order vs creatures free)
WRAP sub_0029FE00 -> pc_mapmaker_pitboss_princess     (chain with realbosses_princess; order free)
WRAP sub_0028D0A0 -> pc_mapmaker_pitboss_message      (chain with realbosses_message; order free)
WRAP sub_002274C0 -> pc_mapmaker_pitboss_event        (chain; return 0x0029FC38 only)
WRAP sub_001CDD60 -> pc_mapmaker_pitboss_restart      (chain with the other restarts; own state only)
WRAP sub_0014CC80 -> pc_mapmaker_pitboss_hud          (chain INSIDE pc_mapmaker_bosses_hud, like realbosses_hud)
WRAP sub_0013A400 -> pc_mapmaker_pitboss_meter        (chain; return 0x0014D264 only)
WRAP sub_00234270 -> pc_mapmaker_pitboss_logic        (NEW; logic item test, hot: tests the current pit call first)
WRAP sub_00212160 -> pc_mapmaker_pitboss_failure      (NEW; return 0x0029FD15 only)
WRAP sub_0028B610 -> pc_mapmaker_pitboss_ragbug       (NEW; class 0x200000 handler)
WRAP sub_00289720 -> pc_mapmaker_pitboss_boss_slot    (NEW; boss slot 2)
WRAP sub_00289E20 -> pc_mapmaker_pitboss_coll_slot    (NEW; collision prop slot 2)
WRAP sub_0028C1C0 -> pc_mapmaker_pitboss_rack_slot    (NEW; rack slot 2)
WRAP sub_0028C390 -> pc_mapmaker_pitboss_bottle_slot  (NEW; bottle slot 3)
WRAP sub_0028C4A0 -> pc_mapmaker_pitboss_bandage_slot (NEW; bandage slot 0xC)
WRAP sub_0028C2F0 -> pc_mapmaker_pitboss_bottle_damage (NEW; bottle damage callback, record +0 = object)
WRAP sub_00289080 -> pc_mapmaker_pitboss_fire_type    (NEW; particle type callback)
WRAP sub_00288E20 -> pc_mapmaker_pitboss_smoke_type   (NEW; particle type callback)
```

All cdecl, plain `ret`. Include `pc_mapmaker_pitboss.inc` after
`pc_mapmaker_creatures.inc` (done in `pc_menu_mouse_guest.inc`; declarations
in `pc_display_menu.h`, stubs in its `#else` branch).

## Playtest .66: bottle at the feet, not hurt by shots (fixed after .66)

User report (.66): "Gas bottle appears at base of pit skeleton without
texture, shooting it doesn't result in skeleton taking damage, and the
bottle is easily destroyed. Ragbugs spawning/logic doesn't appear improved."

What `m66.err` shows (CONFIRMED by the log): the bottle **was** made every
run (`gas bottle 0x24A made ...`): in the first run at (123.02, 8.00,
420.21) with the boss at (117.00, 0.51, 411.00); after the level reloads and
the restart at (117.77, 1.64, 411.59), the boss's feet, once at (NaN, NaN,
NaN); the eye bandage likewise at 12.86 the first time and at the feet (0.97,
1.53) after. No death and no level event in any run; the bosses bar was "out
of range" in the later runs (the player far from the boss). All four models
"not loaded, file read, textures registered by the load pass".

How the story makes and uses the bottle (CONFIRMED, decode): state 1 creates
0x24A (`sub_000A6F90`) at the boss's bone point (model bone 0x29's segment
through its matrix, `sub_00151E80`, minus a lean offset) once the intro is
0.315 in; every frame (0x0029E5EE..) it is put back at that bone point with
the bone's rotation and the boss's health +0x1CC is the bottle's +0x4274. Its
damage callback `sub_0028C2F0` is the object's +0x19C (`sub_0009E720`), which
the damage code calls instead of the generic prop damage (0x000BB62D..
0x000BB638). The boss's bone matrices ([entity+4]+0x10) are computed by
`sub_000A1B80(prop)` once per view slot, only from the prop draw
(`sub_0008F6B0`, 0x0008F700) or a stock bone reader (`sub_0018F8E0`).

Causes (INFERRED from the above; the new lines decide):
- **At the feet / NaN**: the catacombs boss is always on screen, so its bones
  are posed every frame; a MapMaker one out of view is not drawn, so the
  fight read stale or never-computed matrices and put the bottle (and the
  bandage, attached to a bone) at the feet or at NaN.
- **Shots do nothing**: hanging at the mouth (8-12 over a one-floor tile) the
  bottle lists no room (`0 listed rooms`), so no room's prop list holds it
  for the hit tests (the drawing gap of .62, which bossroom closed for drawing
  only). No `damage` call reached `sub_0028C2F0` (health never fell, no
  death). What the user saw "easily destroyed" was not traced; the new hit
  lines show whether the bottle is hit at all.
- **Untextured**: not settled. The load pass registered the four models'
  textures (no deferred mark), and l_17_stm was mounted before it; the
  untextured object at the feet may be the stale-posed bottle or the
  collision hull 0x249 (made at the boss's feet, listed in room 12, drawn by
  the stock room pass; in the story slot 4 `sub_0028C540` flags it).

Fixes:
- Pose: every fight frame, before the fight, the boss's view-slot bit
  (prop word +0x14) is cleared and `sub_000A1B80(boss)` called, as
  `sub_0018F8E0` does, so the bone points (bottle, bandage, fire, throws)
  are this frame's. Log `pit skeleton pose computed before its fight each
  frame (skeleton, view slot)`.
- Rooms: after each fight frame, a bottle, rack or bandage that lists no room
  gets the boss's listed rooms (+0x30/+0x38) and room +0x34, so the bucket pass
  links it where the boss stands (drawn and hit there).
- Logs: each bottle hit (`gas bottle hit (type, kind, amount): health a ->
  b`, 16 a fight); a trace every 2 s (24 a fight: boss and bottle positions,
  the bottle's model, rooms, room, damage callback, health, ragbug phases);
  each ragbug's phase change.

Ragbugs in the story (CONFIRMED, decode of `sub_0028AAA0`/`sub_0028A0A0`):
they wait parked 2.2 under the pit floor (phase 0). Each time the fight passes
state 10 (after a fire attack and raise), up to 3 parked ones rise out of the
floor on a circle of radius 5 round the boss (phase 1, about 1 s), circle it
(3), pick a player and dive (4), strike (5) and explode (6, `sub_000EFBD0`),
then return to the park (0) until the next launch. So in the story they show
in waves of up to three, not all at once and not for good; one at a time
round the skeleton matches a flock whose bugs dive and explode at different
times. The .62 release only adds one launch at state 6. The phase lines show
what each bug does.

## Playtest .68: camera broken, bottle still at the feet (fixed after .68)

User report (.68): "Pit skeleton is now even more glitched out and messes up
my camera view. Didn't fix bottle at all." `m68.err`: `pose computed ...
(0x1, 0x0)` (view slot 0, the player's camera); traces with the bottle at
the feet (y 0.6-2.7) in state 1 and at NaN in states 4-7; no bottle hit line
in the whole run; ragbugs 0, 1, 3, then 4 4 4 for good.

Causes:
- **Camera** (CONFIRMED by the decode, INFERRED as the symptom): the .66
  pose call `sub_000A1B80(boss)` works on the view slot being set up
  (`[0x007B0CC0]+0x214`); from the AI frame that is slot 0, the player's
  camera, so the draw's per-view state was disturbed. And it did not help:
  the bone point stayed at the feet or NaN.
- **Ragbugs stuck in phase 4** (CONFIRMED by the decode): the dive takes its
  target from the rim pad indices (+0x4BF0, 0x0028B08F) through
  `sub_0009A420`, which sends indices 0x1F40..0x243F (scene 100) to the
  cover pool and returns 0 when `sub_0009A330` refuses the pad. The .62-.66
  pads lived in a heap block whose index from the pad table was past 0x1F40:
  no target, so the bugs hovered in their dive.
- **No hits**: the bottle sat at the feet inside the collision hull 0x249,
  which has no damage callback, so shots hit the hull as a plain prop (the
  .66 "easily destroyed" object at the base).

Fixes:
- The pose call is gone.
- The bottle is put at the mouth after every fight frame (states 1..13):
  11 over the boss's feet and 2.5 ahead along its yaw (+0x70; forward
  (sin, 0, cos), +z at the fight's yaw 0, from the .66 eye bandage at 12.35
  up and 2.27 ahead), facing as the boss. A NaN tilt is cleared, and no NaN
  position is kept. Log: `gas bottle held at the mouth (x, y, z)`, plus a
  note when the fight's bone point was NaN.
- The hull gets the bottle's damage callback `0x0028C2F0` (+0x19C). Its hits
  count a quarter of a bottle hit (`gas bottle hit through the body`), so
  body shots wear the bottle down slowly and the bottle itself is the weak
  spot (8 health: type-0x21 hits 2.667, others 0.8 x damage).
- The rim pads go in the pad pool's last entries (index 8000 - 13 x
  (skeleton)), below 0x1F40, written each frame. When the map's own pads
  reach them, they are skipped and logged.

Not verified: whether a mouth 11 up and 2.5 ahead matches the model on screen
(INFERRED from the .66 bandage), the yaw sign for a turned boss, and whether
the bottle at the mouth is hit outside the hull.

## Playtest .73: a second, giant untextured bottle; ragbugs (fixed after .73)

User report (.73): the bottle at the mouth works (hits logged, the skeleton
dies with the explosion in its mouth), but "it actually appears both at the
base of the skeleton very large and untextured, as well as the correct size
and textured but floating near the head", and "Ragbugs still not spawning
correctly". `m73.err`: hits and body hits; ragbug phases 0 -> 1 (state 6) ->
3 -> 4 (state 4), one bug reached 5, none 6.

Causes (CONFIRMED by the decode):
- **The giant object at the base is the collision hull 0x249**
  (`mansionboss_collision`, made at the boss's feet, posed by its slot 2). The
  story never draws it: its slot-4 callback `sub_0028C540` calls
  `sub_000942B0(instance, 8)` (instance +0x1F6 |= 8), and the prop draw
  `sub_000A2740` skips such a prop (0x000A2829, 0x000A28C7). That slot is not
  run in MapMaker play. The bossroom and bigdraw satellite passes never
  included 0x249.
- **Ragbugs rose at state 6**: the .62 release, which the story does not do.
  Stock launches up to 3 parked bugs each time the fight passes state 10.
- **Dives**: the rim pads now resolve. Layout C regular handles are 0..0x3FFF
  (`MAPMAKER_POOL_PADS` 16384, cover base 0x4000; `pads.json` patches
  `sub_0009A420`/`sub_0009A330`, ids up to 0x7BE8). The pit's indices
  16384 - 13 x (n+1) are regular handles and the ids 0x4B5..0x4C1 are valid.
  Phase 4 is the story's patrol: each bug flies from rim pad to rim pad
  (about 5 over the floor, a ring of radius 10 to 12 round the boss). It
  turns to 5 (dive) only when a player is within 5 squared (2.24 m, x/z) of
  the bug (0x0028B1D1, players [0x007B0C48] + 0x1584 each), then strikes
  (`sub_000BA700`). It explodes (6) only when its health is 1.0, which its
  damage callback `sub_0028B5F0` sets: **a ragbug blows up when shot**, then
  re-parks (6 -> 0, health 2) and returns at a later launch. In the catacombs
  the player walks the rim the bugs patrol, so dives are frequent; on a
  MapMaker floor a player must pass under the ring.

Fixes:
- The hull is hidden after every fight frame with the same
  `sub_000942B0(instance, 8)`, once. It still takes the body hits
  (`gas bottle hit through the body`). Log: `collision hull 0x249 hidden as
  the story's slot 4 does`.
- The early release is gone: story timing (state 10 launches only). Log:
  `flock complete, N ragbugs parked; they rise at the fight's launches
  (state 10)`.
- The ragbug patrol, dive and explosion are left as in the story.

What the player should see: the ragbugs stay under the floor through the
intro. After the skeleton's first fire attack and raise, up to 3 rise
around it, circle and patrol a ring about 10 out and 5 up. They dive at a
player who comes under the ring, and they explode when shot. More rise at
each later launch.

## Restart

The restart keeps the Level heap (limitless-bosses.md), so the blocks are
set up again in place. lv17_4's entries all carry 0x4000, so the restart's
`sub_0005C2F0` unmaps them; this map's ids 0x23B..0x246 are noted before and
mapped again after (same slot and data, as `rbs_anims_again`). The props the
fight made are deleted first (whether the stock restart deletes runtime
props was not traced). The records keep their class; the restarted AI start
places each skeleton again.

## Not done / unverified (INFERRED)

- Unplayed. The arena keeps the catacombs' orientation (no rotation); the
  rack hangs 22.5 above the spawn and the rim points sit 9-13 above it, so in
  a low MapMaker room they may be above the ceiling (cosmetic; the fire
  attack aims at the rim pads).
- No hostage, no timer, no failure (the rack stays at stage 1; the stock
  hostage meter is hidden). A MapMaker hostage/timer mode is not offered.
- The Health and Boss multipliers: the fight writes the bottle's health into
  entity +0x1CC every frame, so the bosses area's bar shows bottle health
  (maximum 8, +0x1D8) and a health multiplier has no effect.
- The death removal counts for MapMaker kill logic like the mansion
  skeleton's (stock `sub_0015DDA0`); the generic kill statistics are skipped.
- Particle type callbacks see the first instance's block outside a fight
  call (visual only with several skeletons). A mansion skeleton's message
  arriving during a pit call would run on the pit block (needs a mansion
  skeleton in the pit's fire; not handled).
- lv1_6 (mansion) names three pit anims too (`princess_low_idle1`,
  `princess_raise_up`, `princess_attack_feeble_throw`) under its own ids
  (CONFIRMED, test). With both skeletons in one map the registration's name
  lookup may take the other pak's file for those three ids (same rig;
  timing may differ).
- `TSFP_MAPMAKER_AREA_OFF=pitboss` alone would let the creatures area give
  the class without a block: turn `creatures` off with it.
- Memory: the two story pak indexes, four models (177 KB) and their
  textures, lv17_4 (0.40 MB), 52 KB per skeleton.

## User test steps

1. `TSFP_MAPMAKER_LIMITS=1`. Editor -> custom character set: add **Pit
   Skeleton** and **Ragbug**. Place one Pit Skeleton Story AI on an open
   floor (a tall room helps) and three Ragbug Story AIs near it. Play in
   Story.
2. stderr: `creatures: Pit Skeleton: mounted lv17_4.pak`, both `pitboss:
   mounted pak/story...` lines, `pit skeleton blocks allocated`, `pit
   skeleton 1 placed at ...`, `ragbug joins pit skeleton 1's flock` (x3),
   `pit skeleton fight starts`, then `fight state` lines.
3. Watch: the intro rises, the skeleton holds a bottle; ragbugs circle and
   dive; fire hits the floor around it. Shoot the bottle: the skeleton dies,
   the ragbugs explode; about 17 s later it is gone; stderr `pit skeleton
   killed: its level event dropped`.
4. Pause -> restart: it rises and fights again (`level restart: pit
   skeleton blocks set up again ... (0x1, 0xC)`).
5. Two Pit Skeletons far apart, three ragbugs each: each flies its own.
6. A map with only Ragbugs: they vanish at start (`ragbug without a pit
   skeleton this level`). Report the room size, what you saw and the
   `pitboss:` lines.
