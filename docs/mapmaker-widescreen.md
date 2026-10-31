# MapMaker editor on widescreen displays

Symptom (user, 1536x869 window, Auto output): tiles did not lock to the grid
cells they were placed in and the grid looked a different size from the tiles.
Every earlier MapMaker test ran at 640x480 (factor 1).

## How the editor draws and picks (US 45410066)

- The editor view is not a 3D camera. Its draw, `sub_003111F0` (from
  `sub_001CEBC0`), does not reach `sub_000C77C0` (player camera, widened by
  `pc_display_camera`) or `sub_00151CF0` (perspective builder) in the static
  call graph. CONFIRMED (direct calls). The `[PC-DISPLAY] Camera horizontal
  factor` log line only shows that widescreen was active.
- All of it is 2D in guest pixels (640x480): cell -> pixel
  `x = (cell - [0x783DD8]) * [0x783F2C] + [0x783EE0]`,
  `y = (cell - [0x783CC0]) * [0x783F28] + [0x783DC4]` (`sub_00308B10`,
  `sub_00308C00` tiles, `sub_0030A550` grid lines at 0x0030A6E8/0x0030A7EA).
  Pointer -> cell: `x = (pointer - [0x783EE0]) * [0x783DD4] + [0x783DD8]`
  (`sub_002F8140` at 0x002F81E2..0x002F8239, pointer 0x007BA280 + player *
  0x1E0). The view (`sub_00308F40`, `sub_00309050`) derives the cells-per-pixel
  values from the guest viewport [0x007E71C8]. CONFIRMED. So picking, tiles
  and lines agree in guest pixels at every aspect.
- The renderer fits screen-space draws into a centred 4:3 box in widescreen,
  but a draw spanning >= 98% of the guest width that is untextured (or a full
  canvas) escapes and is stretched over the window (`d3d8_draw_escapes_squeeze`,
  external `src/d3d/d3d8_device.c`). CONFIRMED in source.
- `sub_0030A550` draws a full-canvas backdrop (`sub_0001EFB0` at 0x0030A57B)
  and then all grid lines with `sub_0001B090`, which appends to one batched
  line list (counters 0x006B7AB0..B8) spanning the whole view. CONFIRMED. That
  untextured batch therefore escaped the squeeze (INFERRED; not measured in
  game) while textured tiles and the pointer stayed in the 4:3 box: grid
  cells 1/hud_scale wider than tiles (1.33 at 16:9, 1.75 at 21:9, 2.67 at
  32:9), aligned only at the centre column.

## Fix

`src/pc_mapmaker_widescreen.inc` scopes `sub_00310A20` (grid view: grid,
floors, tiles, items, cursor) and `sub_0030A550` (also called directly by
`sub_003111F0`) to CENTRE placement via `pc_display_ui_scope`
(`src/pc_display_ui.inc`), in scene 102 on a non-4:3 display only. The
backdrop caller 0x0030A580 is added to `pc_display_transition_overlay`, so it
still fills the window. The whole editor field then shares the 4:3 box with
the pointer mapping (`pc_menu_mouse_position`) and HUD; the window's sides show
the backdrop. 4:3 output, scene 100 (MapMaker play), story levels and the
widened gameplay camera are unchanged. The view rectangle, zoom and pan
clamps are untouched (they are in cells).

Using the full widescreen width for the field instead would need stretched
field draws with a view width scaled by the camera factor while the HUD and
toolbar (which hit-test the same guest pointer) stay in the 4:3 box: one guest
pointer cannot serve both mappings, so this was not attempted.

Also fixed: mouse edge panning clamped only the right/bottom speed; in
widescreen the side bars map left of 0, so left panning ran up to ~5x faster
(`src/pc_mapmaker_pointer.inc`).

`TSFP_MAPMAKER_WIDESCREEN=0` restores the old placement for comparison.
`RECOMP_D3D8_2D_TAGS=1` logs tags 00310A20/0030A550 as `centre`.

Wrappers: `WRAP sub_00310A20 -> pc_mapmaker_grid_view`,
`WRAP sub_0030A550 -> pc_mapmaker_grid_lines`.

## Tests

`tests/test_mapmaker_widescreen.py` (production placement scopes with the
`ui_layout.c` batch harness and a host geometry model: 4:3, 16:9, 1536x869,
21:9, 32:9, portrait; mouse at the centre and 4:3 box edges; zoom 20/60/100
cells and pan clamp extremes), `tests/test_mapmaker_pointer.py` (edge pan at
16:9/21:9), `tests/transition_overlay.c` (backdrop caller).

User test: 16:9 window; place tiles at the left/right/top/bottom edges of the
grid view and check each sits in its cell under the cursor; zoom out fully;
pan to all four grid edges (mouse at window edges and right-drag) and check
the speed is the same left and right; repeat at 4:3 (no change expected).
Mac uses the same shared code; native validation is pending.
