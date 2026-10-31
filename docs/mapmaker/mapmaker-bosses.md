# MapMaker Story AI bosses and the story creature probe

Status: implemented in `src/interface/pc_mapmaker_bosses.inc` (area `bosses`),
fixture-tested (`tests/mapmaker/test_mapmaker_bosses.py`: 39 mutations plus
decode checks of every guest site). Not built or played yet.
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
AI to see the value). Type 5 AIs (the game-mode-7 singleton) get no row.
The setting is per AI: duplicates and linked copies start with Off and a
group does not share it.

In play (MapMaker Story, scene 100, story mode): every Boss AI's health is
multiplied by its factor when it spawns (and again after a respawn). The
nearest living Boss AI within 30 game units (`TSFP_MAPMAKER_BOSS_RANGE=<1..100000>`,
3D distance, no line-of-sight test) shows the story boss health bar with the
AI's name as its label ("Boss" when unnamed). It disappears when the boss is
out of range or dead and switches to whichever boss is nearest. In split
screen each viewport shows its own nearest boss, except that the stock bar
code hides it for player 2 in story split screen (sub_00139740 0x0013974C).

Good boss candidates already in the Arcade roster (via the map's character
set): giant robot (preset 7), trophy monster, Khallos, Jacob Crow, mutant
splitter, big monster. They keep their normal MapMaker AI behaviour; true
story boss behaviour (Princess, giant robot phases, Crow walker) needs
per-level state MapMaker never creates (`sub_0025F720`) and is out of scope.

## Save format and compatibility

- Story AI chunk (tag 0x00C7766C) data byte **+0x29**: 0 = no boss,
  0xB0 | (level - 1) = boss with x1 / x2 / x4 / x8 health.
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
  it (0x0014D0DD). The hook runs the original first; if no stock bar was
  drawn it calls `sub_00139740` itself for the nearest Boss AI, with the
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
```

`pc_mapmaker_bosses_close` returns without calling its original for its own
row; inside aitemplates' close, aitemplates would flush its pending group
snapshot after the click, so bosses goes outside. Include the `.inc` after
`pc_mapmaker_aitemplates.inc`; link stub `sub_00139740` added to
`tests/mapmaker/mapmaker_area_stubs.h`.

## Limits and not done

- Story-only creatures (wraith/black ghost, worm, ids 0x15/0x28/0x2C in
  mission 5) are not deployable: their models are INFERRED to exist only in
  the story level paks, and MapMaker hard-codes the AI class. The probe run
  decides the next step.
- Part C (every Arcade-roster character in MapMaker character sets
  regardless of unlocks) was skipped: the custom-set picker was not found
  quickly (`sub_001CD980`, the unlocked-roster builder, has no editor
  caller).
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
3. Two Boss AIs at different distances: the nearer one's bar shows.
4. Without the switch, load and play the same map: ordinary AIs, no bar.
5. Mission 6 Princess / mission 10 giant robot still show their own bars.
6. Probe: set `TSFP_MAPMAKER_CREATURE_PROBE=1` and
   `TSFP_MAPMAKER_CREATURE_PROBE_LOG=<path>\creature-probe.log`, play
   mission 5 until the ghosts and worms have appeared, then a MapMaker
   Story map; send `creature-probe.log`.
