# MapMaker resource table

[Back to the MapMaker guide](mapmaker-limitless.md)

With the raised limits on (`TSFP_MAPMAKER_LIMITS=1`, launcher **Raised
MapMaker limits**), the editor's single free-memory meter is replaced by a
resource panel: one row per resource the editor checks. A row turns amber at
90% and red at its maximum, or when the piece in hand would pass that cap
(the red blinks like the original "full" meter label). With the limits off
the original meter is unchanged. `TSFP_MAPMAKER_TABLE=0` brings the original
meter back with the limits on.

- **Collapsed** (the default): one line where the meter was,
  `+ Fullest: Objects 445/1023`, naming the row nearest its cap (red rows
  first, then amber, then the highest used/max; the first listed on a tie),
  in that row's colour.
- **Expanded**: a `- Resources` header (in the worst row's colour) and the
  15 rows below it, short titles on the left and `used/max` right-aligned in
  one column, so the numbers line up.
- **Toggle**: click the panel (left mouse button, or move the pointer onto it
  with the stick and press A), or press the **I key**. The state is kept for
  the rest of the session; every editor visit starts as it was left. The
  click's A press goes to the panel only, so it does not place or select
  anything underneath. If I is bound to an action in the controls, that
  action keeps it and only the click toggles.

Source: `src/interface/pc_mapmaker_restable.inc` (area `restable`), counts
from `src/interface/pc_mapmaker_limits.inc`, the I key in
`pc_controls_mapmaker_table_key` (`src/input/pc_controls.c`, Mac
`src/native/input_sdl.c`, shared policy `pc_mapmaker_table_key` in
`src/interface/pc_mapmaker_pointer.h`). Fixture:
`tests/mapmaker/mapmaker_restable.c` / `test_mapmaker_restable.py`. Not built
or seen in game yet.

## Rows

"Placed" is what is on the map; the editor refuses a placement when placed +
held passes the cap, and the row is then red.

| Row (full name) | Used | Max | Source of both numbers |
| --- | --- | --- | --- |
| Tiles | placed pieces | 1023 (engine objects bind first: 895 without items) | `MAPMAKER_LIMIT_PIECES`, `mapmaker_limits_over` |
| Items | items (ids above 9) on placed pieces + free-standing objects (`editor+0x306E`) | 500 | `MAPMAKER_LIMIT_SUB` |
| Objects (Engine objects) | pieces + items + 128 reserved | 1023 | `mapmaker_objects_needed`, `MAPMAKER_LIMIT_OBJECTS` |
| Pickups | placed items in categories 8-13, 19-21, 31 (weapons, ammo, keycards, health, armour, power-ups, special pickups) | 768 | play-time pickup records (`MAPMAKER_OBJECTS_PICKUPS`); not an editor cap, the 500 items keep it below |
| Pads (Regular pads) | tile pad cost | 7680 | `MAPMAKER_LIMIT_PADS` |
| Cover (Cover pads) | tile cover cost | 9984 | `MAPMAKER_LIMIT_COVER` |
| Links | template links + 16 per piece | 32,256 | `mapmaker_link_cost`, `MAPMAKER_LIMIT_LINKS` |
| Groups (Pad groups) | global groups `[0x00762CB8]` | 1024 | `MAPMAKER_LIMIT_GROUPS` |
| Memory | placed memory estimate (after the budget scale), KB rounded up | 2000 KB | `mapmaker_placed_memory`, `MAPMAKER_LIMIT_MEMORY` / 1024 |
| Story AI | `editor+0x280` | 511 | `MAPMAKER_EDITOR_SAI_LIMIT` (`sub_0031BE70` add) |
| Logic | logic ops `editor+0x41E0` | 120 | `MAPMAKER_EDITOR_OPS_CAP` (`sub_00321E60`) |
| Movers | category 25 | 160 | per-category check |
| Props | category 26 | 160 | per-category check |
| Turrets | categories 27-29 added up | 3 x 24 | per-category check; red when any one type is full |
| Teleports (Teleporters) | categories 14-18 added up | 5 x 8 | per-category check; red when any one colour is full |

The first nine rows read `mapmaker_limits_view`, written by the same
`sub_002FA720` wrapper (`pc_display_mapmaker_limits`) that makes the placement
decision and logs `[MAPMAKER] placement refused:`; its `over` bits colour the
rows. The per-category rows repeat `sub_00303460`'s test: `sub_00308420`
counts each placed piece's items by the int16 category at catalogue
`0x00524F58 + id * 0xC4 + 2` (ids 0..79), against the live maxima at
`0x00524BE0 + category * 24 + 4` (raised by `mapmaker_categories_apply`). The
memory row shows the placed estimate; the cap check adds the held piece
(`editor+0x278`). Flags, start points, cover props and lights have no row.

## Where it draws

The editor HUD `sub_003111F0` calls `sub_0030C3C0` (0x0031150C and
0x00311743), which calls the meter painter `sub_0030C210(x, y)` (cdecl,
floats, 0x0030C42E) at the top left of the safe area: x = 22 or 32, y = 36 or
24 on the 640 x 480 canvas (`sub_00022B40`/`sub_00022B90`, safe factors 0.93,
0.9, 0.85) plus the HUD slide `[0x007BA038]` x 5 and `[0x007BA188]` x 8. The
stock painter sets font 0x12, draws label text 0xA7F with `sub_00041E30(x, y,
text, colour, 0x80)` and an 80 x 16 bar (`sub_0001EFB0(x0, y0, x1, y1,
0x0F0F0F64)` backdrop, `sub_000814B0` bar) of 1 - max(`editor+0x270`, ...),
then a hover help test (`sub_000711F0` on the meter rectangle, so the game
pointer and the meter share these units).

The panel takes the existing manual wrapper of `sub_0030C210`
(`patches/*-mapmaker-meter-probe.patch`, hook `pc_display_mapmaker_meter`),
so it needs no new guest wrapper and no regeneration. It sets font 0x12 and
the text alignment word `[0x007E7260]` to 0 (both restored after), then:

1. Measures each string with the game's own `sub_0003FCF0(&w, &h, text, 0)`
   in that font (scratch 32 bytes below the guest stack). It reads the
   `\vs60` scale code, so widths are at 60%. A result that is not a finite
   width inside the canvas falls back to 5.2 units per character.
2. Lays out the panel at the meter's x, y: 3 units of padding, the widest
   title, an 8-unit gap, then the value column, whose right edge is fixed
   (each value is drawn at that edge minus its own width). The line pitch is
   11.5 units, or the tallest measured glyph + 1.5 when that is more (font
   0x12 is about 16 units tall at 100%, 9.6 at 60%; the old 75% rows 12 apart
   touched). A position that would push the panel off the canvas is moved
   inside (8-unit margin).
3. Draws the backdrop with `sub_0001EFB0` in the stock meter's backdrop
   colour `0x0F0F0F64`, then the outlined text with `sub_00041E30` (outline
   0x80).

Every value starts with a space: `\vs` reads every digit that follows it
(atoi over the digits, 0x00040082 in the measure and the same parser in the
draw), so `\vs60227/500` would ask for scale 60227. Right-aligned, the space
only widens the measured text.

Size: collapsed about 140 x 18 units; expanded about 130 x 190 (16 lines),
from y 36 to about 226, in the upper half and left of the toolbar.

Layout: guest units, so on a wide output it stays in the editor HUD's centred
4:3 box ([widescreen](mapmaker-widescreen.md)) at 4:3 proportions; the fixture
checks both states inside the canvas and its left third (`pc_ui_side_anchor`)
and left of the toolbar (x 388-599) at 640 x 480, 1024 x 768, 16:9, 21:9 and
32:9, with the estimated widths of the longest strings the caps allow.

The original meter is drawn (with the probe's scope) when the limits are off,
the limit check has not completed this frame for this editor object,
`TSFP_MAPMAKER_TABLE=0`, meter probe flag 1 is set (so its measurements stay
those of the stock meter), or the 31 x 64-byte text buffer
(`xbox_HeapAlloc`) is missing. The stock hover help of the meter is not shown
while the panel is up.

## Toggle input

`mapmaker_table_input(menu)` runs in `pc_display_menu_mouse_input` after the
original `sub_00073870` input read (and after the mouse pointer code) for
every dispatched menu, and acts only on the editor screen itself: scene 102
(`[0x0079094C]`), a menu with no rows (`+0x178`) and no child (`+0x170`).
It does nothing with `TSFP_MAPMAKER_TABLE=0`, or when the panel has not been
drawn within the last 4 editor ticks (a mode without the meter), so clicks
there stay the editor's.

- `sub_00073870` records button i of the menu as bit i (press) and bit i+16
  (press or auto-repeat) of `+0xC4`, bit i (held) and bit i+16 (release) of
  `+0xC8`. The editor reads A as `+0xC4 & 0x10` of the active menu
  (`sub_00077470`, e.g. 0x00303E35, 0x00305785).
- A press of A (`+0xC4 & 0x10`) with the current player's game pointer
  (`0x007BA260 + [0x007BA9E0] * 0x1E0 + 0x20/+0x24`, placed at the mouse by
  `pc_mapmaker_pointer.inc` or moved by the stick) inside the last drawn
  panel toggles it and clears `0x00100010` from `+0xC4` and `+0xC8`; until
  A is released those bits stay cleared each tick, and the release edge too.
  Other buttons pass through.
- The I key: `pc_controls_mapmaker_table_key()` reports I down (window
  focused, no text entry, not while the settings dialog edits bindings),
  unless any player-1 binding uses I (`pc_settings_bound_keys`). One toggle
  per press. Not a pad button: every pad button already has an editor action.

## Playtest checks

1. `LIMITS` on, open the editor: one line top left where the meter was,
   `+ Fullest: Objects 128/1023` on an empty map, over a dark backdrop,
   readable at 640 x 480 and in a 16:9 window.
2. Click it: the panel expands with `- Resources`; titles in one column,
   numbers right-aligned in another; rows do not touch or overlap; the panel
   stays left of the toolbar and below the title bar. Click again: back to
   one line. No tile is placed or selected by either click.
3. Move the pointer onto it with the stick and press A: same toggle. Press I
   on the editor screen: same toggle; I inside a menu or name entry does not.
4. Leave and re-enter the editor: the panel is as it was left.
5. Check the glyphs: `+`, `-` and the right-aligned numbers render, and the
   value column starts with no stray digit-sized gap (the leading space).
6. Place tiles and items: Tiles, Items, Objects, pads, links and memory rise;
   undo brings them back.
7. Fill one cap (for example 24 turrets of one type, or 160 movers): its row
   turns amber near the end and red at the cap, the collapsed line names it,
   and placement is refused with the matching `[MAPMAKER] placement refused:`
   line where that cap is one of the logged ones.
8. Add Story AI and logic ops: their rows count.
9. `TSFP_MAPMAKER_TABLE=0` or `LIMITS` off: the original meter; a click there
   behaves as before.

Mac: shared code in the same include chain, the I key through
`tsfp_native_key_down`; not built or run on a Mac.
