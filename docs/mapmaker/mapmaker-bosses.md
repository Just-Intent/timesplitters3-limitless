# MapMaker Story AI bosses and the story creature probe

Status: implemented in `src/interface/pc_mapmaker_bosses.inc` (area `bosses`),
fixture-tested (`tests/mapmaker/test_mapmaker_bosses.py`: 99 mutations plus
decode checks of every guest site). Not built or played yet.

Later requests (2026-10-31): "If in range of multiple bosses, multiple boss
health bars should be visible on screen" and "Add multiple additional story
AI health settings in mapmaker that are stronger than the "Tough Guy"
setting" - see [Several boss bars](#several-boss-bars) and
[Stronger health](#stronger-health).
Request (2026-10-31): "an option to appear on the story AI customization
screen whenever you set an enemy as a boss to decide whether to use "boss
health" (big bar on the bottom of the screen) or "sub-boss health" (bar
visible on the back of the enemy)" - see [Health display](#health-display).
Research basis: `logs/limits/features/story-enemies.md` (main checkout, ignored).
No guest-code table entry.

User requests (2026-10-31): "Add boss enemies from the game as deployable
story AI, with their boss health appearing once in range" and "Add the black
ghost and worm type enemies from the fifth story mission as deployable story
AI". This batch adds the Boss option and a read-only probe that tells which
story creatures could load in MapMaker; the creatures themselves are not
deployable yet (see Limits).

## What the player gets (`TSFP_MAPMAKER_LIMITS=1`)

Open a Story AI from the Story AI list. A row **Boss: Off** appears above
**Done** (after any group rows). Each click cycles
**Off -> On -> health x2 -> x4 -> x8 -> Off**; the edit screen stays open
and the label changes in place (if the label does not refresh, reopen the
AI to see the value). Type 5 AIs (the game-mode-7 singleton) get no row. While Boss is not Off a
second row **Health display: Boss bar** follows; a click toggles it to
**Health display: Sub-boss bar** and back, in place. The row is built with
the screen, so after switching Boss on from Off reopen the AI to see it;
cycling Boss to Off clears the choice (its label reads "(Boss off)" and
clicks do nothing until reopened).
**Duplicate Story AI** and **Add linked copy** (see
[AI templates](mapmaker-aitemplates.md)) give the new AI the source's Boss
level and Health display choice; its Health (including Brute..Colossus) is
copied with the other settings. Changed 2026-10-31 (user report "Duplicating
story ai set as bosses defaults the boss setting on the new AI back to No"):
before, the copy started with Off. After that the Boss setting is per AI:
changing it on one member of a linked group changes only that AI (Health,
a shared group field, still follows the group).

In play (MapMaker Story, scene 100, story mode): every Boss AI's health is
multiplied by its factor when it spawns (and again after a respawn). Each
living Boss AI within 30 game units (`TSFP_MAPMAKER_BOSS_RANGE=<1..100000>`,
3D distance, no line-of-sight test) shows the story boss health bar with the
AI's name as its label ("Boss" when unnamed), up to four stacked bars,
nearest at the bottom. A bar disappears when its boss is out of range or
dead. In split screen each viewport shows its own nearest two bosses, except that the stock bar
code hides it for player 2 in story split screen (sub_00139740 0x0013974C).

Good boss candidates already in the Arcade roster (via the map's character
set): giant robot (preset 7), trophy monster, Khallos, Jacob Crow, mutant
splitter, big monster. They keep their normal MapMaker AI behaviour; true
story boss behaviour (Princess, giant robot phases, Crow walker) needs
per-level state MapMaker never creates (`sub_0025F720`) and is out of scope.

## Save format and compatibility

- Story AI chunk (tag 0x00C7766C) data byte **+0x29**: 0 = no boss,
  0xB0 | (level - 1) = boss with x1 / x2 / x4 / x8 health and the Boss bar,
  0xB4 | (level - 1) = the same with the Sub-boss bar (bit 2; mask 0xF8).
  Builds before this change (mask 0xFC) log 0xB4..0xB7 as unknown and play
  that AI as an ordinary AI; a stock game ignores the byte.
- Proof that it is free (decode test `test_sai_chunk_byte_0x29_unused`): the
  compiler zeroes the container (`rep stosd` 0x0030070D); the writer
  (0x003013E1..0x003015E5) stores byte +0x28 (0x00301459, size 1) and word
  +0x2A (0x0030146A) but nothing at +0x29; the editor loader
  (0x00302428..0x0030267F) never reads +0x29 nor a wider +0x28. The only
  other Story AI chunk walkers (every `cmp ..., 0xC7766C` in game code:
  0x0023F511, 0x00240757, 0x00241DB9, 0x00337E2D, plus `sub_0033EC30` that
  gets the data pointer) never touch it either.
- A stock game, or this build without the switch, loads the map with
  ordinary AIs; re-saving there drops the byte. Unknown values (not
  0xB0..0xB3) are ignored and logged (`[mapmaker] story AI bosses: unknown
  chunk +0x29 value ignored`).
- Editor side: there is no free field in the 0xE4-byte editor entry (+0x50
  is the aitemplates link word), so the setting lives in a C table indexed
  like the entries, with each entry's colour (+0x60) as a check. It follows
  delete (`sub_0031C8B0` memmove), map reset (`sub_002FEBF0`), load
  (`sub_00301A90`, also undo) and compile (`sub_003006C0`, save and the undo
  shadow). A loaded chunk whose entry does not hold its saved index
  (entry +0x24 = chunk +0x22) or a slot whose colour changed drops the
  setting with a log line (`boss setting dropped ...`).

## How it works in play

- `sub_00240C50` (MapMaker level start, chained): index -> boss level and the
  chunk's saved name (+0x00) from the map container `[0x00784014]`;
  `TSFP_MAPMAKER_BOSS_RANGE` is read here.
- Health: a character entity's health is the float pair +0x1CC current /
  +0x1D8 maximum (set at creation `sub_000DAC30` 0x000DADEE and at AI start
  `sub_0015F8C0` 0x0015FAB4 from `sub_000B9A90`). Each HUD frame the hook
  walks the live list (`0x00900100`, actors relocation of `0x007A3580`,
  count `[0x0074C37C]`, players first `[0x00790950]`); for pool entry a:
  entity = a+0, entity+0x7C must be a, spawn record a+0xBF0, record+8 =
  R + index + 1 (`[0x00783A50]` = R). A boss with factor > 1 gets maximum
  and current multiplied once; when the maximum no longer equals the value
  the hook wrote (respawn into the same pool slot, or the AI-start reset)
  it is scaled again (current is multiplied if it was within the stock
  maximum, then capped). A 64-entry table keyed by pool entry and record.
- Bar: the story boss bar is `sub_0014CC80`, called by the HUD frame
  `sub_0014F0B0` at 0x0014F256 with ebx = HUD block 0x007A6160 +
  player*0x177C (0x0014F0CC). In story mode (0x0014CCB3) it switches on the
  focus entity `[[0x007B0C7C]+0xFF4]` model word +0x94 and draws known story
  bosses with `sub_00139740` (esi = HUD block, edi = entity; stack text,
  fraction, int *drawn), then sets HUD block +8 bit 0 (0x0014D0D5) or clears
  it (0x0014D0DD). The hook runs the original first, then calls `sub_00139740` itself
  for each Boss AI in range (see Several boss bars), with the
  label in a 32-byte guest buffer and the out slot on the guest stack, and
  sets bit 0 like the stock path. `sub_00139740` reads the fraction from
  +0x1CC/+0x1D8 (0x001398B8/0x001398C0) and smooths it in HUD block
  +0x171C..+0x172C.
- Nothing writes the focus field `+0xFF4` or the actor's AI state. The
  research's suggested wrap of `sub_0014F0B0` with the gate at
  0x0014FAF6..0x0014FB46 was rejected: that block is a different meter
  (player 1's AI +0x9E0 against the focus's a+0x28 threshold, fed by
  `sub_000B8C60`, which kills the player at the threshold via
  `sub_000A0D30`), and forcing a+0xC = 4 would change that logic.

## Several boss bars

Up to `BOS_BARS_MAX` = 4 bars per viewport (2 when more than one human
plays, `[0x00790950]` > 1; INFERRED to fit a half-height viewport), stock
bar included, for the living Boss AIs within range, nearest first; the
farther ones wait for a free place.

- Position (CONFIRMED, `sub_00139740`): x is centred between the viewport
  `[[0x007E71C8]+0]`/`+4` and the side margins `[0x007A6140]`/`[0x007A6148]`;
  the box bottom is `[[0x007E71C8]+0x18]` minus the int HUD margin
  `[0x007A614C]` (0x0013986E..0x001398AB); height 45 (0x00478900) x
  `[0x004E68F8]` x 1 / 0.933 / 0.8; `sub_00130400` draws the frame 6 x
  `[0x004E68F8]` above the box and the label inside it (INFERRED from its
  `(top + bottom - text height) / 2`). The HUD frame rewrites the margin
  (0x0014B084, 0x0014BB3F, 0x0014D4CC).
- The hook raises `[0x007A614C]` by slot x round(57 x `[0x004E68F8]`)
  (45 + 6 frame + 6 gap; a scale outside 0.25..4 counts as 1) around each
  `sub_00139740` call and puts the old value back. Slot 0 is the bottom (the
  stock position); a stock story bar drawn by the original (HUD block +8 bit
  0) keeps slot 0 and ours start at slot 1, so nothing overlaps it. A bar the
  game did not draw (`*drawn` = 0, e.g. player 2 in story split screen) uses
  no slot.
- `sub_00139740` smooths the drain in HUD block +0x171C/+0x1724/+0x172C (the
  only esi fields it uses, decode check). Each boss gets its own copy
  (16-entry table keyed by HUD block and entity, oldest reused) swapped in
  before and out after its call; the stock values are restored at the end,
  so a stock bar keeps its own animation.
- Drain (CONFIRMED, 0x0013994B..0x00139AAF and `sub_0014B090`): with damage
  d = 1 - cur/max, +0x171C = last d, +0x1724 = recent drain, +0x172C = fade,
  the solid fill is 1 - (d - drain x fade); a hit raises the fade to 1. Only
  `sub_0014B090` (game update, per player, from `sub_001CE850` 0x001CE8D1)
  lowers it: +0x172C -= dt x [0x00475D10] (2.0, floor 0) while the HUD block
  clock +0x2C += dt. The first build swapped each boss copy out during that
  tick, so its fade stayed 1 and the bar stayed full ("not showing health
  decreasing"). Now each copy is faded by the clock time since its own last
  draw (same rule; a clock that went back counts as no time), so a hit drains
  to the new length in 0.5 s like the stock bar. A real story boss (giant
  trophy monster, giant skeleton) without the Boss flag shows no health at
  all (user, 2026-10-31): its own stock bar is suppressed.
- Widescreen: the coordinates are the guest's 640x480 HUD space like every
  stock HUD draw, so the renderer keeps all bars inside the centred 4:3 HUD
  box ([widescreen](mapmaker-widescreen.md)).

## Health display

| Story AI | bottom bar | overhead meter |
|---|---|---|
| Boss, Boss bar | yes (stacked, in range) | no |
| Boss, Sub-boss bar | no | yes, while alive (any distance, like stock) |
| real story boss (trophy monster, skeleton), Boss Off | none | hidden ([real bosses](mapmaker-realbosses.md)) |
| real story boss, Boss bar | the Boss bar | hidden |
| real story boss, Sub-boss bar | none | its stock meter |

- The meter is the stock MapMaker story overhead meter `sub_0013A400`
  (CONFIRMED by decode, `test_overhead_meter_draw_contract`): cdecl
  `(actor, 0, 0x7F, 0x6E)`, plain `ret`, only draws (billboard quads via
  `sub_0001AC50`); reads actor +0xC68 (fade, `sub_0011F780`), entity +0x10
  (visibility bit of the camera), +0x64..+0x6C and +0x1CC/+0x1D8, and never
  +0x268. Stock `sub_0014D1F0` calls it (scene 100, story) only for actors
  whose entity has a damage-stage record +0x268.
- Not chosen: giving ordinary AIs a record. `sub_0012C800` is a bump
  allocator of 17 slots x 0x144 (0x00745938, count [0x007490A0], reset only
  at 0x0014853D, no free), shared with players and story scripts, so
  respawning Sub-boss AIs would exhaust it; and +0x268 has about 100
  readers (damage flash `sub_0014D3A0`, `sub_0014CC80`, hit handling), so
  it would change behaviour, not just the HUD.
- Chosen: WRAP `sub_0014D1F0` -> `pc_mapmaker_bosses_meters`. Before the
  original (which calls nothing before its meter loop, so the render state
  is the same) it calls `sub_0013A400(actor, 0, 0x7F, 0x6E)` for each live
  Sub-boss AI (list 0x00900100 from `[0x00790950]`) that is alive, not the
  player's entity, has a model instance ([entity+4], +0x8C, +0x10) and has
  no +0x268 record (with one, the stock loop draws it: no double meter).
  Gated on scene 100, story mode, the switch and a Sub-boss AI in the map;
  registers are restored after every call.
- Height (CONFIRMED): the meter spans 0.4..0.6 units ([0x475CC8] = 0.6,
  [0x4785E4] = 0.2) above node [[[entity+4]+0x8C]+0x18] of the model's own
  pose (matrix +0x34, plus entity +0x68), so it already follows each
  model's skeleton rather than a human height; on the trophy monster that
  node is what puts it on its back. The node index is the first of the
  skeleton header's body-part nodes (`sub_000C30E4` switch: +0x18, +0x34,
  +0x38, +0x54; INFERRED head). No cheap model-height field was found, so
  no extra lift is applied.
- Health x2..x8 and the stronger health levels apply to Sub-boss AIs too.

## Stronger health

Stock (CONFIRMED): the Story AI edit screen's **Health** row is text 0xC0D
with options 0xC0E..0xC12 (Weakling, Low, Normal, High, Tough Guy; English
text read from the MapMaker level pak's table, ids 3085..3090), values 0..4,
bound to editor entry +0x4C (`lea ecx,[esi+0x4C]; call sub_00078590` at
0x0031CFCE/0x0031CFD2). The writer copies the byte to chunk +0x30
(0x0030149F/0x003014A2), the loader back unclamped (0x0030260A/0x00302614).
At map start `sub_00240C50` maps it through `sub_0031BA70` = table
0x0052E060 {14, 10, 6, 3, 0} (5 entries, then unrelated data) into spawn
record +0x4E (0x00242356..0x00242360); in scene 100 story mode health is
`0x004DACF4[record+0x4E]` x the character base (`sub_000B9A30` via
`sub_000B9A90`): Weakling 1.5, Low 2.75, Normal 4.5, High 11, Tough Guy 20.

New (`TSFP_MAPMAKER_LIMITS=1`):

| Setting | Entry +0x4C | Health | Saved chunk +0x30 / +0x21 |
|---|---|---|---|
| Tough Guy (stock) | 4 | 20 | 4 / 0 |
| Brute | 5 | 40 (x2) | 4 / 0xA1 |
| Juggernaut | 6 | 80 (x4) | 4 / 0xA2 |
| Titan | 7 | 160 (x8) | 4 / 0xA3 |
| Colossus | 8 | 320 (x16) | 4 / 0xA4 |

- Editor: WRAP `sub_00078590` acts only for the return 0x0031CFD7 with the
  selected entry's +0x4C as argument and appends the four options with
  `sub_00071800(text, value)` before the binding, so the list shows and
  stores 5..8 natively (groups share +0x4C through aitemplates as before).
  Labels are guest strings from `xbox_HeapAlloc`.
- Compile: a value 5..8 is saved as Tough Guy (+0x30 = 4) plus +0x21 =
  0xA0 | (value - 4); a value above 8 is saved as Tough Guy and logged. A
  stock game therefore reads Tough Guy and never indexes past 0x0052E060.
  Chunk byte +0x21 is decode-proven free: the writer stores byte +0x20 and
  word +0x22 only, the loader and every chunk reader never cover +0x21
  (`test_sai_chunk_byte_0x21_unused`).
- Load: +0x21 = 0xA1..0xA4 with +0x30 = 4 and a matching index sets entry
  +0x4C to 4 + extra; other values are ignored and logged
  (`unknown chunk +0x21 health value ignored`, `health setting dropped at
  load`).
- Play: the level hook records the extra level per Story AI index; the HUD
  hook multiplies the entity health of every such AI (boss or not) by
  2^extra x the Boss factor, once per (re)initialisation, with the same
  table as the boss scaling.

## Story creature probe (`TSFP_MAPMAKER_CREATURE_PROBE=1`)

Read-only log (no guest writes; fixture-checked by memory snapshot), any
scene whose HUD runs (story levels and MapMaker play). Output:
`TSFP_MAPMAKER_CREATURE_PROBE_LOG=<file>` (appended) or stderr. At most
4096 lines per run, 160 new characters per level.

- Once per level (scene or actor pool `[0x007356D8]` change): a `level:`
  line, then for each story-only character id of the research list (0x04,
  0x12, 0x15, 0x28, 0x2C, 0x2E..0x3B, 0x42..0x46, 0x4A, 0x4B, 0x4D, 0x4F,
  0x50, 0x52, 0x62, 0x64, 0x65, 0x75) its two model ids from the character
  table 0x0053A3CC + id*0x30 (+4, +8) and the resource slot
  `0x0078B860[model]` (int16, ids < 0x9D0) with that entry's type (+0),
  resources (+0x10/+0x14) and flags (+0x28) in `[0x00791D4C]` + slot*0x30
  (layout from `sub_000A5E40`; flag 0x2000000 = skipped in story/MapMaker
  scenes).
- Each new live character (pool entry, record, id): `char #i id .. var ..
  ai+10 .. ai+14 .. rec +0 .. +4 .. id .. model .. skel .. health a/b`,
  then its own model line. `skel` is `[[entity+4]+0x88]+4` (the skeleton
  type `sub_000DAC30` switches on; INFERRED pointer path).

How to read it: in mission 5 (scene 1) the ids 0x15/0x28/0x2C show which
creature is which (class `ai+10/ai+14` 0x40000000 = ghost handler, skeleton
0xF0 = worm). In a MapMaker map, the same ids' `slot` lines say whether
their models have a resource entry there (`no entry` / a negative slot /
flag 0x2000000 means they cannot load without extra pak data).

## Hooks (WRAP lines for the integrator, area `bosses`)

```
WRAP sub_000784A0 -> pc_mapmaker_bosses_rows       (chained: aitemplates_rows; order free; acts only for return 0x0031D083)
WRAP sub_0031CAC0 -> pc_mapmaker_bosses_close      (chained: aitemplates_close; put bosses OUTSIDE it)
WRAP sub_0031C8B0 -> pc_mapmaker_bosses_delete     (chained: aitemplates_delete; order free)
WRAP sub_002FEBF0 -> pc_mapmaker_bosses_reset      (chained: sky/roof reset hooks; order free)
WRAP sub_003006C0 -> pc_mapmaker_bosses_compile    (chained: OUTSIDE pc_mapmaker_container_compile; order vs aitemplates/sky/roof free)
WRAP sub_00301A90 -> pc_mapmaker_bosses_load       (chained: OUTSIDE pc_mapmaker_editor_load; order vs aitemplates free)
WRAP sub_00240C50 -> pc_mapmaker_bosses_level      (chained: OUTSIDE pc_mapmaker_editor_level_load; order vs aitemplates free)
WRAP sub_0014CC80 -> pc_mapmaker_bosses_hud        (new; every HUD frame, ebx = HUD block)
WRAP sub_00078590 -> pc_mapmaker_bosses_health_rows (new; list binder, acts only for return 0x0031CFD7)
WRAP sub_0014D1F0 -> pc_mapmaker_bosses_meters     (new; HUD per viewport, scene 100 story; draws before the original)
```

`pc_mapmaker_bosses_close` returns without calling its original for its own
row; inside aitemplates' close, aitemplates would flush its pending group
snapshot after the click, so bosses goes outside. For every other row it
remembers the edited AI's Boss level/display and the entry count, calls the
original (aitemplates' Duplicate / Add linked copy run there) and, when
exactly one entry was added, the selection is unchanged and the new last
entry's words +0x28..+0x4C equal the source's (`ait_copy_all`), sets the
same level/display for it (`bos_set`, new colour recorded). No cross-area
call or new WRAP line. Include the `.inc` after
`pc_mapmaker_aitemplates.inc`; link stubs `sub_00139740` and `sub_00071800`
and `sub_0013A400` are in `tests/mapmaker/mapmaker_area_stubs.h`.

## Limits and not done

- Story-only creatures (wraith/black ghost, worm, ids 0x15/0x28/0x2C in
  mission 5) are not deployable: their models are INFERRED to exist only in
  the story level paks, and MapMaker hard-codes the AI class. The probe run
  decides the next step.
- Part C (every Arcade-roster character in MapMaker character sets
  regardless of unlocks) is done separately in
  [characters](mapmaker-characters.md): the picker is `sub_002C1EA0`, whose
  only filter is `sub_001CD980` (call 0x002C1F30).
- The range unit is the game's world unit (INFERRED close to a metre).
- The label text and bar position are the stock story bar's; no split-screen
  player 2 bar in story split screen (stock behaviour).

## User test steps

1. `TSFP_MAPMAKER_LIMITS=1`. In the editor pick a character set containing
   the giant robot (preset 7) or Khallos; place a Story AI with that
   character, name it, open it and click **Boss** until **health x4**.
   Reopen: it still reads **Boss: health x4**. Save, reload: kept.
2. Play: walk toward it; within about 30 units the boss bar appears with
   its name; it drains as you shoot (four times the hits of a normal AI);
   walk away: it hides; kill it: it hides.
3. Two or more Boss AIs in range: one bar each, nearest at the bottom,
   stacked upward without overlap (at most four; two in split screen),
   each draining on its own: after a hit the bar shrinks to the boss's new
   health within about half a second (shoot one boss and only its bar moves). Also on a 16:9 display: inside the 4:3 HUD area.
   Check the label sits inside each box and bars do not cover other HUD.
4. Health: open a Story AI, Health now lists Brute, Juggernaut, Titan,
   Colossus after Tough Guy. Pick Titan; save, reload: still Titan. In play
   it takes about 8x the hits of a Tough Guy (16x with Colossus; times the
   Boss factor when both are set).
5. Health display: on a Boss AI (reopen after setting Boss) click
   **Health display** to **Sub-boss bar**; save, reload: kept. In play no
   bottom bar appears for it; a small meter floats above it and shrinks as
   it is hurt; it vanishes when it dies. Another Boss AI on **Boss bar**
   still gets its bottom bar and no meter.
5a. Copies: on a Boss AI at **health x4**, **Sub-boss bar**, Health Titan
   click **Duplicate Story AI**: the new AI (place it) reads **Boss: health
   x4**, **Health display: Sub-boss bar** and Health Titan. Same with **Add
   linked copy**. Change Boss on the copy: the source keeps x4. Save,
   reload: kept; delete the source: the copies keep theirs.
6. Without the switch, load and play the same map: ordinary AIs, no bar,
   Titan/Colossus AIs read Tough Guy.
7. Mission 6 Princess / mission 10 giant robot still show their own bars.
8. Probe: set `TSFP_MAPMAKER_CREATURE_PROBE=1` and
   `TSFP_MAPMAKER_CREATURE_PROBE_LOG=<path>\creature-probe.log`, play
   mission 5 until the ghosts and worms have appeared, then a MapMaker
   Story map; send `creature-probe.log`.
