/* US 45410066 MapMaker editor (scene 102) on a non-4:3 display.
 *
 * The editor view is not a 3D camera: sub_003111F0 (drawn from sub_001CEBC0)
 * reaches neither sub_000C77C0 (player camera, widened by pc_display_camera)
 * nor sub_00151CF0. Everything is 2D in the guest's 640x480 pixels:
 *   cell -> pixel  x = (cell - [0x783DD8]) * [0x783F2C] + [0x783EE0]
 *                  y = (cell - [0x783CC0]) * [0x783F28] + [0x783DC4]
 *     (sub_00308B10, sub_00308C00 tiles, sub_0030A550 grid lines)
 *   pixel -> cell  x = (pointer - [0x783EE0]) * [0x783DD4] + [0x783DD8]
 *     (sub_002F8140 at 0x002F81E2; pointer 0x007BA280 + player * 0x1E0)
 * so picking, tiles and grid agree in guest pixels at every aspect.
 *
 * The widescreen renderer fits screen-space draws into a centred 4:3 box
 * (AUTO placement), except a draw spanning >= 98% of the guest width that is
 * flat colour or a full canvas, which it stretches over the whole window.
 * sub_0030A550 emits all grid lines into one batched, untextured line list
 * (sub_0001B090, counters 0x006B7AB0) across the whole view, so that batch
 * is stretched while the textured tiles and the pointer stay in the 4:3 box:
 * grid cells come out 1/hud_scale times wider than the tiles (1.33 at 16:9,
 * 1.75 at 21:9, 2.67 at 32:9), aligned only at the centre column.
 *
 * Fix: in scene 102 on a non-4:3 display, draw the grid view (sub_00310A20,
 * and sub_0030A550 where sub_003111F0 calls it directly) entirely at CENTRE,
 * so all of it shares the 4:3 box with the pointer mapping
 * (pc_menu_mouse_position) and the editor HUD. The full-canvas backdrop at
 * 0x0030A57B still covers the window (pc_display_transition_overlay). The
 * view rectangle, zoom and pan clamps are untouched: they are in cells.
 * TSFP_MAPMAKER_WIDESCREEN=0 restores the old placement for comparison.
 *
 * Hooks: runtime patch 3030-mapmaker-editor-widescreen.patch wraps
 * sub_00310A20 and sub_0030A550 in recomp_manual.c. Requires the includer's
 * WORD_AT, pc_display_wide_now and pc_display_ui_scope (pc_display_ui.inc).
 * Per frame: no guest_writable (fixed image address only). */
#include <stdlib.h>

#define MAPMAKER_WIDESCREEN_SCENE 102
#define MAPMAKER_WIDESCREEN_CENTRE 2 /* renderer placement ABI (pc_display_ui.inc) */

static struct { int initialized, enabled; } mapmaker_widescreen;

static int mapmaker_widescreen_active(void)
{
    if(!mapmaker_widescreen.initialized) {
        const char *v=getenv("TSFP_MAPMAKER_WIDESCREEN");
        mapmaker_widescreen.initialized=1;
        mapmaker_widescreen.enabled=!(v && v[0]=='0' && !v[1]);
    }
    return mapmaker_widescreen.enabled &&
        (int32_t)WORD_AT(0x0079094Cu)==MAPMAKER_WIDESCREEN_SCENE && pc_display_wide_now();
}

static void mapmaker_widescreen_scope(void (*original)(void),uint32_t tag)
{
    if(!mapmaker_widescreen_active()) { original();return; }
    pc_display_ui_scope(original,MAPMAKER_WIDESCREEN_CENTRE,tag);
}

/* sub_00310A20: editor grid view (grid, floors, tiles, items, cursor). */
void pc_mapmaker_grid_view(void (*original)(void)) { mapmaker_widescreen_scope(original,0x00310A20u); }
/* sub_0030A550: backdrop and grid lines. */
void pc_mapmaker_grid_lines(void (*original)(void)) { mapmaker_widescreen_scope(original,0x0030A550u); }
