# MapMaker Arcade matches: 20 bots and 24 characters

Status 2026-10-31: implemented in `src/interface/pc_mapmaker_matchbots.inc`
(area `matchbots`) and `config/guest-code-patches/matchbots.json`,
fixture-tested (`tests/mapmaker/test_mapmaker_matchbots.py`: 45 mutations
plus XBE facts; `tests/mapmaker/test_mapmaker_matchbots_classified.py`).
Not built or played.

User request (2026-10-31): more bots per Arcade-style match than the stock 10
bot rows / 16 characters, so that the 20-slot custom set is useful there too.

US Xbox build 45410066. **CONFIRMED** = seen at the address (lifter's decoder
or the disassembly); **INFERRED** = reasoned.

## Result

| | Stock | Now (`TSFP_MAPMAKER_LIMITS=1`, local MapMaker match) |
| --- | --- | --- |
| Bot rows on the Bots screen | 10 | 20 (two pages) |
| Bots per match | 10 | 20 |
| Characters (players + bots) | 16 by the arrays, 14-16 by the map table | 24 (4 players + 20 bots) |

Modes: MapMaker Arcade from the front end (game mode 9) and its in-game
pause setup, the editor's Test map (scene 102, custom set; mode 5) and a
Test map's pause setup (mode 5 on a MapMaker map). Ordinary Arcade maps,
MapMaker Story, story levels, attract demos and System Link stay stock
(see "Why not ordinary Arcade").

## Every fixed limit on characters per match

| Limit | Where (CONFIRMED) | Size | Now |
| --- | --- | --- | --- |
| Bot rows | work copy `0x0075FD90` (10 x 0x1C: +0 character, +4 team, +8 difficulty, +0xC on, +0x10 override, +0x14 portrait, +0x18 variant); fill `sub_002C0310` (end `0x0075FEB8` @`0x002C0371`), store `sub_002BFFD0`/`sub_002C0000` (profile arrays `+0x6B0/+0x6D8/+0x700/+0x728 + index*0xA0`, 10 per index) | 10 | page 2 kept by the hook (session state, not in the profile) |
| Rows offered | `sub_002C0310` @`0x002C0441`: `[0x004D239C + (map*15 + col)*4] - [0x005236C4]`, capped at 10 (`0x002C044A`); every map row: 14/14/16/16 (columns: default, mode 5 sub-mode 0x10, mode 0xB, mode 0xD) | 10 | page 2 always 10 rows |
| Match roster | `sub_002D2C70` loop `0x002D36EA..0x002D3822` (10 rows; network modes 0xB-0xD skip it at `0x002D36C7..0x002D36DC`); bot count `[0x0078AD5A] = ebx - players` @`0x002D382F` | 10 | the hook adds rows 11-20 after it |
| Per-actor arrays | setup block `0x0078AD58` (+0x14/+0x24/+0x34/+0x44, 16 bytes each), copied whole (0x28 dwords) to backup `0x0075F800` (`sub_002CA880`/`8B0`), pending `0x007DE340` (`sub_000658D0` @`0x000658E2`), live `0x007DE400` (`sub_000670C0` @`0x00067167`, `sub_00067280` @`0x000672D8`/`0x000672F3`), back to setup (`sub_002EF9F0` @`0x002EFA0B`), the replay save (`0x00067310`) and System Link (`sub_00352BC0`, `sub_00354240`, `sub_003550A0`, `sub_003563C0`) | 16 | live arrays read through a 64-entry mirror |
| Rank table | `0x007DE4C0`, one dword per place, filled by `sub_000646B0` @`0x000647B9` / `sub_00064810` @`0x00064ABB`/`0x00064BF5` for humans + bots with no bound; team scores `0x007DE500` follow | 16 | relocated to `0x00904100` (64) |
| Pair table | `[0x0074C368]` outside story mode sized `humans + 10` (`add ...,0xA` @`0x0015E7DF` alloc, @`0x0015E76C` clear, @`0x0015E0F5` column clear) | humans + 10 | humans + 20 |
| Arcade stats | `sub_00238DD0`: `[0x0075CCC0]` = 26 x 0x9CC outside story (`add eax,0x1A` @`0x00238DE4`); awards `sub_0023A040` frame 0x19A4 = 26 rows of 0xEC (@`0x0023A834`) | 26 | unchanged: sets the 24-character limit |
| Actor pool | `[0x007356D8]` 32 x 0xEA0, 64 in scene 100 (actors area) | 32 / 64 | unchanged |
| Per-opponent kill words / bit masks | actor `+0xA10` 32 words; `[0x007A2BC8]` 1 << slot | 32 | unchanged (24 < 32) |
| Radar / network slot table | `0x00777244` 52 x 0x124 (`sub_00133AB0`) | 52 | unchanged |
| AI table | `0x007A2D00` (moved to 64 by the actors area); arcade bots `sub_002B51E0` @`0x002B522C` | 30 / 64 | unchanged |
| System Link | `sub_0023A040` uses 16 slots when networked (@`0x0023B69A` ...); network messages carry 16 ranks (`sub_00064C60`, `sub_00357B00` @`0x00357B42`) | 16 | stock |

Spawn points: MapMaker arcade spawning was not traced; more characters than
start points share them (INFERRED from stock respawn behaviour; test).

## Design

### Live arrays (every scene)

`matchbots.json` moves all 26 indexed references to the live arrays (the
only references; no absolute form, no pointer into them, CONFIRMED by the
classified test) to a mirror at `0x00904000`: characters `+0x00`, teams
`+0x40`, difficulty `+0x80`, variants `+0xC0`, 64 entries each. Readers:
`sub_000668F0`, `sub_000CF230`, `sub_000D2FD0`, `sub_000D50E0`,
`sub_00148310`, `sub_00158BC0`, `sub_0015C560`, `sub_001B5BB0`,
`sub_002B51E0` (arcade bot creation), `sub_003586A0`, `sub_000D7570`,
`sub_0033ACB0`, `sub_00350DC0` (AI skill); writers (teams): `sub_00068830`,
`sub_002EC340`, `sub_002EDDC0`, `sub_0033ACB0`, `sub_00359AA0`,
`sub_0035AD90`, `sub_0035D150`.

- `pc_mapmaker_matchbots_live` (after `sub_000670C0`, the pending -> live
  copy; `sub_00067280` ends by calling it too): mirror 0..15 = the block's
  arrays; 16+ = the side arrays of the match start that added them, only
  when the live block is a MapMaker map (`+0x54 == 0x64`), local, and its
  humans/bots match; else zero. A live count past 16 without its extras
  (never in stock) is clamped to `16 - humans` and logged.
- `pc_mapmaker_matchbots_restart` (before `sub_002EF9F0`, the only live ->
  setup copy): mirror 0..15 back into the block, so team changes made in
  play travel as in stock.
- Between the two copies in `sub_00067280` and its final `sub_000670C0`
  the mirror still holds the previous match (INFERRED harmless: the callees
  there, `sub_00069A60`, `sub_000431F0`, `sub_001CEB00`, do not index the
  arrays directly).

### Rank table and pair table

`matchbots-rank` relocates `0x007DE4C0` (0x40) to `0x00904100` (0x100); the
six references to `0x007DE500` (team scores) are `keep`, and the System Link
copy loop end `0x00064CAB` gets its own operand (`0x00904140`) so it still
copies 16 ranks. `matchbots-pairs-*` size the pair table `humans + 20`
outside story mode (3 x 12 B more per extra character pair; about 2.2 KB
with 4 players).

### Bots screen page 2 (scenes 100/101)

Screen `0x0052375C` = {builder `sub_002C2C20`, update `sub_002C0490`}; the
update reloads the work copy when the set index (`[0x0078A068]`, profile
`+0x6A8`) differs from `[0x0078AEAC]` and tail-jumps to the store
`sub_002BFFD0` every frame (@`0x002C05C8`).

- Eligible: scene 100 or 101, MapMaker Arcade (mode 9) or mode 5 with the
  setup's map scene `0x64` (Test map pause), no System Link session,
  `TSFP_MAPMAKER_LIMITS=1`, no reload pending.
- Down on row 10: the store runs once (rows 1-10's last change), rows 1-10
  are saved, rows 11-20 are copied into the work copy with their portraits
  (`sub_002BE600(character, variant)`, as the reload @`0x002C0547`), the
  cursor goes to the first row (`sub_00071970`/`sub_00076EF0`) and the
  direction bits are cleared. Up on the first row of page 2 swaps back.
  The title gets "(1-10)" / "(11-20)" (sets area's `sets_title`).
- While page 2 is shown `pc_mapmaker_matchbots_store` keeps the work copy
  in rows 11-20 and returns without the profile store. If the set index
  changed (the update reloaded rows 1-10), the page is dropped and the
  store runs.
- Rows 11-20 start as copies of rows 1-10 (team, difficulty, on flags) with
  no character; for MapMaker Arcade's map set (index 0x11, mode 9) they are
  the map's slots 11-20 from the sets chunk (variant -1 when
  `sub_002BE270(id)` says unavailable, as the fill), reloaded on every
  opening as stock reloads rows 1-10.
- The builder hook and the match start swap rows 1-10 back if the screen
  was left on page 2 (stock does not reload the work copy for set index 0).

### Match start

`pc_mapmaker_matchbots_match_start` (around `sub_002D2C70`, chained with
the sets and story hooks): rows 1-10 back first, then the stock roster, then
rows 11-20 with the stock rules (row used when character >= 0 and (override
`+0x10` != -1 ? override : `+0xC`) != 0; team = `+4 % (sub-mode 4 ? 4 : 2) +
1` in a team game, else 0; difficulty = `(15 - [0x0053A3DC + id*0x30]) / 3`
when profile `+0x10` is set and the type byte is not 5, else `+8`), until
24 characters / 20 bots (logged when rows are left over). Index < 16 goes
into the setup block's arrays, every index into the side arrays; the bot
count byte becomes the total. Refused: System Link or network modes,
story type (`+0x55 == 10`), monkey sub-mode 0x13, other maps, switch off.
In scene 102 (editor Test map) the source is the custom set's members
11-20 (`editor +0x11860`, index >= 0xC) with row k's team and difficulty.

## Why not ordinary Arcade

No hard limit is specific to MapMaker: all 17 rows of the map table
`0x004D2390` (stride 0x3C, scene byte +0) allow 14/14/16/16 characters
(CONFIRMED from the XBE data), and the mirror, rank and pair changes are
scene-independent. What is not known for the stock Arcade levels is their
Level heap headroom with up to 24 different character models (MapMaker play
measured 5.5-14 MB free; stock levels were never measured) and whether
their start points and AI navigation (stock pools outside scene 100) cope
with 24 characters. Without a build to measure, the extra bots stay off
there; enabling them later needs only the eligibility checks
(`mb_screen_ok`, `mb_extras`) and a play test per level.

## System Link

A session (`[0x0076B108]`) or network mode refuses page 2 and the extra
bots; the mirror then equals the block (16), ranks copy 16 as before. Maps
are unaffected (the extras are setup state, never saved).

## Extension memory

`0x00904000..0x009040FF` live mirror, `0x00904100..0x009041FF` rank table,
`0x00904200..0x009042FF` reserved for this area. Rows 11-20 and the side
arrays are host memory.

## Hooks (WRAP lines for the integrator, area `matchbots`)

```
WRAP sub_000670C0 -> pc_mapmaker_matchbots_live        (new; must be wrapped wherever matchbots.json is applied)
WRAP sub_002EF9F0 -> pc_mapmaker_matchbots_restart     (new; menu callback, cdecl menu)
WRAP sub_002D2C70 -> pc_mapmaker_matchbots_match_start (chained: story's pc_mapmaker_story_char_config, sets' pc_mapmaker_sets_match_start; order free)
WRAP sub_002C0490 -> pc_mapmaker_matchbots_screen      (chained: sets' pc_mapmaker_sets_screen; order free; per frame)
WRAP sub_002C2C20 -> pc_mapmaker_matchbots_open        (new; screen builder)
WRAP sub_002BFFD0 -> pc_mapmaker_matchbots_store       (new; per frame, also reached by a tail jump; replaces the original on page 2)
```

All cdecl with a plain `ret` (CONFIRMED). Include the `.inc` after
`pc_mapmaker_sets.inc` (uses `sets_select`, `sets_title`, `sets_chunk`,
`sets_editor`). New `guest_call` targets: `sub_00069640`, `sub_002BE600`
(wrapped by realbosses; the call goes through it as stock's does),
`sub_002BE270` (stubs in `tests/mapmaker/mapmaker_area_stubs.h`).

## User test steps

1. `TSFP_MAPMAKER_LIMITS=1`. MapMaker -> load a map -> Arcade -> setup ->
   Bots. The title reads "(1-10)". Press Down on the last row: "(11-20)",
   rows show the map's slots 11-20 (map set) or empty rows. Pick a few
   characters; Up on the first row returns, rows 1-10 unchanged.
2. Start the match: count the bots (scoreboard, radar): up to 20 with one
   player; stderr has `[mapmaker] matchbots: extra bots added`. Play a few
   minutes; check kills/score/ranks of bots 17+ and the end-of-match awards.
3. Pause -> setup -> Bots: same paging; restart the match.
4. Two to four players (splitscreen): 24 characters at most; with 4 players
   and 20 rows used, nothing is lost.
5. Editor -> custom set with slots 11-20 -> Test map (Arcade type): bots for
   slots 1-20.
6. System Link / ordinary Arcade map / without the switch: 10 rows, stock.
7. Watch `[mapmaker] heap full` with many distinct characters.
