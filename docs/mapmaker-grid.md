# MapMaker grid: seven floors, 80x80, overlapping tiles, stacked items

Editor-only feature area (scene 102). Research and evidence:
`logs/limits/features/floors-grid.md` and `overlap-items.md` (ignored, main
checkout). Code: `src/pc_mapmaker_grid.inc`, `src/pc_mapmaker_grid.h`,
`config/guest-code-patches/grid.json`; fixture `tests/mapmaker_grid.c` +
`tests/test_mapmaker_grid.py` (44 mutations, table/settings-block checks,
decoder inventory of the redirected constants). Nothing here was built or run
in the game.

## Switches (environment only)

| switch | effect |
|---|---|
| `TSFP_MAPMAKER_LIMITS=1` (pools on) | floors -2..4 (7 bars), grid 80 x 80 x 8 levels (z -2..5), placement refused above level 5, view/pan/shading/placement bounds 80, widest zoom 100 cells |
| `TSFP_MAPMAKER_OVERLAP=1` | tile placement ignores occupancy (bounds still apply; layers above the top grid level refused) |
| `TSFP_MAPMAKER_STACK_ITEMS=1` | several items per cell for weapons, ammo, keycards, health, armour, power-ups, special pickups and props (categories 8-13, 19-21, 26, 31, 33, 35, 36); every other category keeps one per cell |
| `LIMITS` or `OVERLAP` | removing a piece writes back the cells of other pieces inside its box (last placed wins) |

The overlap and stack switches are not in `src/pc_debug_options.def`: those
entries are display/debug booleans persisted in JSON, and no MapMaker switch
(including `TSFP_MAPMAKER_LIMITS`) is wired through them yet. Adding them there
needs a host-side bridge from the setting to the environment on both
launchers.

## How it works

- Grid `[0x00784004]`: index `x + W*(y + W*(z+2))`. Stock W 40, 6 levels,
  0x9600 bytes. With the limits the hooks allocate W 80, 8 levels (0x32000
  bytes of Level heap) and bound every axis (stock bounds only the flat index,
  which would wrap x into the next row).
- Floor up: stock `cmp ecx,2` at 0x00306E14 refuses with text 0x1576 through
  `sub_002F79E0` (return 0x00306E99). The hook turns that refusal at floors 2
  and 3 into the stock success (floor +1, below = old floor, text 0x157D).
  Floor down is unchanged (-2).
- Floor menu `sub_0030BE60`: loop count 5 -> 7 (`grid.json`, unconditional);
  panel extent read from the settings block (5.0 / 7.0). Without the limits
  the bars for floors 3 and 4 (drawn last) get y = -100000 in their stack slot
  after `sub_0001ED50` (return 0x0030BFB5); nothing reads that slot after the
  loop, so the menu looks and clicks as stock.
- Settings block 0x00903600 (16 bytes, extension range 0x009036xx; 0x00903500 is the story gate; free per the
  feature brief): floors, extent, widest zoom, 1/widest zoom. Stock values are
  copied from the image constants, so stock mode is bit-exact. Written on the
  first hook call (the generic `sub_0001ED50` runs from boot) and again at
  editor grid allocation.
- `grid.json` redirects (decoder-checked inventory): 0x0030BEDD (5.0),
  0x002F8ED2, 0x002F9FBC, 0x002F9FE3, 0x0030A974, 0x0030AB90 (40.0),
  0x003075E2 (1/60), 0x0030764D (60.0). Other 40.0 reads: 0x00334F82 UI layout
  scale, 0x002E03A4 / 0x002E06DC front-end layout (INFERRED from context).
  The default view (16 cells at 20,20) is kept so old maps open where they
  were.
- `src/pc_mapmaker_pointer.inc` reads the extent for its mouse-pan clamp
  (`MAPMAKER_GRID`), falling back to 40 for anything outside 40..127.
- Overlap: `sub_002F8140` answers 0 only when called from the placement probe
  (return 0x002F8F93) in scene 102; its side effects (+0x2F80, +0x25C) are
  kept. Connection building and pads still see the single grid owner, so
  overlap artefacts (sky windows, one-way walls, AI islands) are expected and
  depend on placement order (overlap-items.md section 4).
- Removal (`sub_00307DA0`, called by `sub_00308480` after the piece is
  unlinked): after the clear, every other placed piece's cells inside the
  removed box are written back in list order; cells outside the box are left
  alone, so a newer neighbour keeps the cells it owns.
- Stacked items: `sub_00308260` answers "none" at returns 0x003034AD and
  0x00303696 (held item from `sub_00303460`'s `[ebp+0x14]`), so nothing is
  replaced or uncounted; the pick-up caller 0x002F831B is unchanged (repeated
  pick-up peels the oldest first). `sub_00334770` sees the piece's items in the
  cell with x = 0x7FFF for the call (restored after, also on refusal); held id
  editor+0x2F34.

## Save and story compatibility

No format change: absolute signed-byte cells, rebuilt into the grid at load.
Old maps load identically (they sit in 0..39, levels 0..5). A map that fits
the stock limits (x/y < 40, floors <= 2 plus 2-tall pieces, one item per
cell) is a stock map. Pieces at x/y >= 40 or z >= 4 work in play but are
outside the stock editor's grid (cannot be picked there). Stacked items load
in the stock game too (the loader does not dedupe); the stock editor replaces
the oldest when a new item is placed there. Re-saving an old map that used the
7th-floor glitch recomputes connections for its high pieces (they now connect).
All hooks are editor functions or gated on their call site / scene 102; play
and story levels are untouched.

## WRAP lines

```
WRAP sub_00307CC0 -> pc_mapmaker_grid_alloc
WRAP sub_00307C20 -> pc_mapmaker_grid_reset
WRAP sub_00307CF0 -> pc_mapmaker_grid_insert
WRAP sub_00307DA0 -> pc_mapmaker_grid_remove
WRAP sub_00307E60 -> pc_mapmaker_grid_lookup
WRAP sub_002F8DE0 -> pc_mapmaker_grid_placement
WRAP sub_002F79E0 -> pc_mapmaker_grid_floor_sound
WRAP sub_0001ED50 -> pc_mapmaker_grid_floor_bar
WRAP sub_002F8140 -> pc_mapmaker_overlap_cell
WRAP sub_00308260 -> pc_mapmaker_stack_find
WRAP sub_00334770 -> pc_mapmaker_stack_cell
```

`sub_0001ED50` is a 5-byte `jmp` thunk to `sub_0001C720` called from 353
sites; the hook costs one stack compare when not in the floor menu.
`#include "pc_mapmaker_grid.inc"` after `pc_mapmaker_limits.inc`.

## Risks

- Floor menu: the 7-bar panel grows downward from the stock top (bars are
  bottom-up from base + 7 steps); check it does not cover the toolbar.
- Hidden stock-mode bars are drawn off screen, not skipped (two extra sprite
  calls per frame).
- 80x80 play: longer sightlines, AI pathing over distance, fog; rooms stay
  capped at 241 pieces.
- Overlap maps look broken by design; teleporters/starts/flags never stack.
