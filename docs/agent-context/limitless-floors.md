# Limitless: sixteen MapMaker floors

Linked from [limitless.md](limitless.md) (area `grid`). User docs:
`docs/mapmaker/mapmaker-grid.md`, `docs/mapmaker/mapmaker-limitless.md`.
Raised 2026-11-01 from seven floors (-2..4) to sixteen (-2..13) with
`TSFP_MAPMAKER_LIMITS=1`. Static analysis and fixtures only; not built or run.

## Where the floor count lives (all checked)

| place | stock | now | how |
|---|---|---|---|
| editor floor `editor+0x264` (int), below `+0x268` | -2..2 | -2..13 | floor-up refusal at `0x00306E14` (`cmp ecx,2`) turned into success by `pc_mapmaker_grid_floor_sound` (WRAP `sub_002F79E0`, return 0x00306E99) for floors 2..12; down still stops at -2 |
| 3D view floor `[0x00778D34]+0x98` (camera y = floor x 5.0 at 0x0032D649) | up refused at 2 (`0x0032EEBC`) | 13 | `pc_mapmaker_grid_preview_floor` (WRAP `sub_0032EDA0`, sole caller 0x0032F20A, return 0x0032F20F): floors 2..12, no glide (`+0x80 & 0x800`), run with the floor set to 1 and the shift added back; the glide end copies `+0x98` to `editor+0x264` (0x0032EE7F). This gap already existed with seven floors |
| floor menu `sub_0030BE60` | 5 bars | 16 | `grid.json` loop count 0x10 at 0x0030BF3F; panel extent from settings block `0x00903600` (5.0 / 16.0); `pc_mapmaker_grid_floor_bar` sets the remaining count (`esp+0x20` after `sub_0001ED50` returns to 0x0030BFB5, floor -2 only, only when it holds 16) to 5 without the limits; old off-screen fallback for floors 3+ |
| occupancy grid `[0x00784004]` | 40x40x6 | 80x80x17 (z -2..14) | the five grid functions reimplemented (`MAPMAKER_GRID_WIDE_L`); 435,200 bytes of Level heap, editor scene only |
| placement guard `sub_002F8DE0` | bottom only | every layer below level 15 | `pc_mapmaker_grid_placement` |
| load start floor `sub_002FE160` | 3, then the lowest piece | unchanged | lowest-piece rule works at any floor |
| piece record floor byte `+6` | signed byte | unchanged | `cvttss2si` at 0x00300BDD, `movsx` at 0x00302194 (editor) / 0x0008A87E (play) |
| play y | z x 5.0 (`[0x004760CC]`) | unchanged | `sub_0008A780`; floor 13 is y 65 (story levels reach y 86, Crowe's arena) |
| roof fall kills | lowest floor | unchanged | `mapmaker_roof_play.low/high` are computed per build from the records; `kill_y` = 5 x low - 20 |

Not floor-bound (searched): open air view (portal groups), sky mask (per
room), pads/nav (per piece and room), rooms (count), container tiers and the
validator (no floor checks), Story AI points (word z), free objects (signed
byte y), item cells (local to the tile). The floor count is not stored in a
map.

## What bounds it

No hard cap below 127 (signed byte) was found. Sixteen is the chosen safe
maximum: each level costs 25.6 KB of editor Level heap (17 levels = 425 KB,
+225 KB over seven floors; the editor's heap headroom is unmeasured), and
each menu bar 2.2% of the screen height (16 bars reach about 60%). More floors
need only `MAPMAKER_GRID_TOP_FLOOR`, `WIDE_L` (= top + 4), `WIDE_BARS`
(= top + 3) and the `grid.json` count, plus a menu layout check.

## Save format

Unchanged. Old maps load with the same floors; maps using floors 5..13 load
in the stock game and in this build without the limits as pieces outside the
editor grid (like the stock 7th-floor glitch) and still play.

## Tests

`tests/mapmaker/mapmaker_grid.c` / `test_mapmaker_grid.py`: floors to 13,
the menu count write (stock 5, wide 16, first bar only, slot check), the 3D
view (every floor x up/down/both/none, glide, caller, scene, stock), the
guard at level 14, a 7-floor map rebuilt in the 16-floor grid, a -2..13 map
round-tripped through the record bytes, constants agreement and a decoder
check of `sub_0032EDA0`. `mapmaker_roof.c`: a piece on floor 13 raises only
`high`; kill heights follow the lowest floor.

## Playtest

Floor menu with 16 bars (layout, click rectangle), floor up to 13 and down,
2-tall tiles on 12/13, connections on high floors after save, the 3D view
moving through every floor, Show in Map to a Story AI on floor 13, an old
map unchanged, play: falls from floor 13 (fall kill only below the lowest
floor), AI/bots and pickups on high floors, sky/clouds at y 65,
`TSFP_MAPMAKER_PROBE=32` heap line in the editor.
