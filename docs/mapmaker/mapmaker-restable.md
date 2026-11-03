# MapMaker resource table

[Back to the MapMaker guide](mapmaker-limitless.md)

With the raised limits on (`TSFP_MAPMAKER_LIMITS=1`, launcher **Raised
MapMaker limits**), the editor's single free-memory meter is replaced by a
table: one row per resource the editor checks, `Title: used/max`. A row turns
amber at 90% and red at its maximum, or when the piece in hand would pass that
cap (the red blinks like the original "full" meter label). With the limits off
the original meter is unchanged. `TSFP_MAPMAKER_TABLE=0` brings the original
meter back with the limits on.

Source: `src/interface/pc_mapmaker_restable.inc` (area `restable`), counts
from `src/interface/pc_mapmaker_limits.inc`. Fixture:
`tests/mapmaker/mapmaker_restable.c` / `test_mapmaker_restable.py`. Not built
or seen in game yet.

## Rows

"Placed" is what is on the map; the editor refuses a placement when placed +
held passes the cap, and the row is then red.

| Row | Used | Max | Source of both numbers |
| --- | --- | --- | --- |
| Tiles | placed pieces | 511 | `MAPMAKER_LIMIT_PIECES`, `mapmaker_limits_over` |
| Items | items (ids above 9) on placed pieces + free-standing objects (`editor+0x306E`) | 500 | `MAPMAKER_LIMIT_SUB` |
| Engine objects | pieces + items + 64 characters | 1023 | `mapmaker_objects_needed`, `MAPMAKER_LIMIT_OBJECTS` |
| Pickups | placed items in categories 8-13, 19-21, 31 (weapons, ammo, keycards, health, armour, power-ups, special pickups) | 768 | play-time pickup records (`MAPMAKER_OBJECTS_PICKUPS`); not an editor cap, the 500 items keep it below |
| Regular pads | tile pad cost | 7680 | `MAPMAKER_LIMIT_PADS` |
| Cover pads | tile cover cost | 9984 | `MAPMAKER_LIMIT_COVER` |
| Links | template links + 16 per piece | 32,256 | `mapmaker_link_cost`, `MAPMAKER_LIMIT_LINKS` |
| Pad groups | global groups `[0x00762CB8]` | 1024 | `MAPMAKER_LIMIT_GROUPS` |
| Memory | placed memory estimate (after the budget scale), KB rounded up | 2000 KB | `mapmaker_placed_memory`, `MAPMAKER_LIMIT_MEMORY` / 1024 |
| Story AI | `editor+0x280` | 511 | `MAPMAKER_EDITOR_SAI_LIMIT` (`sub_0031BE70` add) |
| Logic | logic ops `editor+0x41E0` | 120 | `MAPMAKER_EDITOR_OPS_CAP` (`sub_00321E60`) |
| Movers | category 25 | 160 | per-category check |
| Props | category 26 | 160 | per-category check |
| Turrets | categories 27-29 added up | 3 x 24 | per-category check; red when any one type is full |
| Teleporters | categories 14-18 added up | 5 x 8 | per-category check; red when any one colour is full |

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
text, colour, 0x80)` and an 80 x 16 bar (`sub_0001EFB0` backdrop,
`sub_000814B0` bar) of 1 - max(`editor+0x270`, ...), then a hover help test.

The table takes the existing manual wrapper of `sub_0030C210`
(`patches/*-mapmaker-meter-probe.patch`, hook `pc_display_mapmaker_meter`),
so it needs no new guest wrapper and no regeneration. Rows start at the
meter's x, y in font 0x12 at 75% (`\vs75`), 12 units apart, outlined like the
stock label (`sub_00041E30`, outline 0x80); 15 rows reach about y 216. The
text alignment word `[0x007E7260]` is zeroed for the draw and restored, the
font restored. The stock hover help of the meter is not shown while the table
is up.

Layout: guest units, so on a wide output it stays in the editor HUD's centred
4:3 box ([widescreen](mapmaker-widescreen.md)) at 4:3 proportions; the fixture
checks it inside the canvas and its left third (`pc_ui_side_anchor`) and left
of the toolbar (x 388-599) at 640 x 480, 1024 x 768, 16:9, 21:9 and 32:9.
A meter position that would push the table off the canvas is moved inside.
The glyph width (6.5 units at 75%) used for that is an estimate.

Always visible: it replaces a meter that was always visible, and every pad
button already has an editor action, so a toggle would need a new binding.

The original meter is drawn (with the probe's scope) when the limits are off,
the limit check has not completed this frame for this editor object,
`TSFP_MAPMAKER_TABLE=0`, meter probe flag 1 is set (so its measurements stay
those of the stock meter), or the 15 x 64-byte text buffer
(`xbox_HeapAlloc`) is missing.

## Playtest checks

1. `LIMITS` on, open the editor: the table sits top left where the meter was,
   readable over the grid at 640 x 480 and in a 16:9 window; rows do not
   overlap each other or the toolbar.
2. Place tiles and items: Tiles, Items, Engine objects, pads, links and memory
   rise; undo brings them back.
3. Fill one cap (for example 24 turrets of one type, or 160 movers): its row
   turns amber near the end and red at the cap, and placement is refused with
   the matching `[MAPMAKER] placement refused:` line where that cap is one of
   the logged ones.
4. Add Story AI and logic ops: their rows count.
5. `TSFP_MAPMAKER_TABLE=0` or `LIMITS` off: the original meter.

Mac: shared code in the same include chain; not built or run on a Mac.
