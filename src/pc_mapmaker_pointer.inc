/* US 45410066 MapMaker editor pointer (prototype; off unless
 * TSFP_MAPMAKER_POINTER=1).
 *
 * The editor screen (scene 102) draws its toolbar, floor buttons and grid
 * itself; they are not menu rows. They hit-test the game's own per-player
 * pointer (sub_000711F0): floats at 0x007BA260 + player*0x1E0 + 0x20/+0x24,
 * player index 0x007BA9E0. The game moves that pointer from the stick in
 * sub_00073870 and places it with sub_00071140(x, y).
 *
 * After the original sub_00073870, when the editor screen itself is in front
 * (no menu rows, no open child such as the pause menu or a category list) and
 * the mouse has moved or a button was pressed, place the pointer at the mouse
 * exactly as sub_00071140 + sub_00071050 do: normalised offsets at
 * +0x18/+0x1C, position at +0x20/+0x24, clamped to +0x48..+0x54 with bit 2 of
 * +0x00 set when clamped. A still mouse leaves controller movement alone.
 * The left button becomes the A button on the host side
 * (pc_controls_mapmaker_pointer, pc_mapmaker_pointer.h).
 *
 * Requires the includer's WORD_AT, FLOAT_AT, guest_writable,
 * d3d8_display_output_shape, pc_menu_mouse_position,
 * pc_controls_menu_hover_pointer and pc_controls_mapmaker_pointer. */
#include <math.h>
#include <stdlib.h>

#define MAPMAKER_POINTER_SCENE 102
#define MAPMAKER_POINTER_PLAYER 0x007BA9E0u
#define MAPMAKER_POINTER_BLOCK 0x007BA260u
#define MAPMAKER_POINTER_STRIDE 0x1E0u

static struct {
    int initialized, enabled;
    int have_last, last_left, last_right;
    float last_x, last_y;
} mapmaker_pointer;

static int mapmaker_pointer_enabled(void)
{
    if(!mapmaker_pointer.initialized) {
        const char *v=getenv("TSFP_MAPMAKER_POINTER");
        mapmaker_pointer.initialized=1;
        mapmaker_pointer.enabled=v && v[0]=='1' && !v[1];
    }
    return mapmaker_pointer.enabled;
}

/* The editor screen is the dispatched menu with no rows and no child. */
static int mapmaker_pointer_context(uint32_t menu)
{
    return guest_writable(0x0079094Cu,4) &&
        (int32_t)WORD_AT(0x0079094Cu)==MAPMAKER_POINTER_SCENE &&
        guest_writable(menu,0x23c) && !WORD_AT(menu+0x178) && !WORD_AT(menu+0x170);
}

/* sub_00071140(x, y) followed by sub_00071050, for the current player. */
static int mapmaker_pointer_place(float x,float y)
{
    uint32_t player,b;
    float cx,cy,sx,sy,ix,iy,px,py;
    if(!guest_writable(MAPMAKER_POINTER_PLAYER,4) || !guest_writable(0x007BA030u,4) ||
       !guest_writable(0x007BA038u,4) || !guest_writable(0x007BA180u,0x10)) return 0;
    player=WORD_AT(MAPMAKER_POINTER_PLAYER);
    if(player>=4) return 0;
    b=MAPMAKER_POINTER_BLOCK+player*MAPMAKER_POINTER_STRIDE;
    if(!guest_writable(b,0x58)) return 0;
    cx=FLOAT_AT(0x007BA180u);cy=FLOAT_AT(0x007BA184u);
    sx=FLOAT_AT(0x007BA038u);sy=FLOAT_AT(0x007BA188u);
    ix=FLOAT_AT(0x007BA18Cu);iy=FLOAT_AT(0x007BA030u);
    if(!isfinite(x) || !isfinite(y) || !isfinite(cx) || !isfinite(cy) ||
       !isfinite(sx) || !isfinite(sy) || !isfinite(ix) || !isfinite(iy) ||
       !sx || !sy || !ix || !iy) return 0;
    FLOAT_AT(b+0x18)=(x-cx)*ix;
    FLOAT_AT(b+0x1c)=(y-cy)*iy;
    px=sx*FLOAT_AT(b+0x18)+cx;
    py=sy*FLOAT_AT(b+0x1c)+cy;
    FLOAT_AT(b+0x20)=px;FLOAT_AT(b+0x24)=py;
    if(FLOAT_AT(b+0x48)>px) {
        WORD_AT(b)|=2u;px=FLOAT_AT(b+0x48);FLOAT_AT(b+0x20)=px;FLOAT_AT(b+0x18)=(px-cx)*ix;
    }
    if(px>FLOAT_AT(b+0x50)) {
        WORD_AT(b)|=2u;px=FLOAT_AT(b+0x50);FLOAT_AT(b+0x20)=px;FLOAT_AT(b+0x18)=(px-cx)*ix;
    }
    if(FLOAT_AT(b+0x4c)>py) {
        WORD_AT(b)|=2u;py=FLOAT_AT(b+0x4c);FLOAT_AT(b+0x24)=py;FLOAT_AT(b+0x1c)=(py-cy)*iy;
    }
    if(py>FLOAT_AT(b+0x54)) {
        WORD_AT(b)|=2u;py=FLOAT_AT(b+0x54);FLOAT_AT(b+0x24)=py;FLOAT_AT(b+0x1c)=(py-cy)*iy;
    }
    return 1;
}

/* Called after the original sub_00073870 for every dispatched menu. */
static void mapmaker_pointer_input(uint32_t menu)
{
    PcMenuPointer p;
    float gw,gh,x,y;
    unsigned sw,sh;
    int moved,pressed;
    if(!mapmaker_pointer_enabled()) return;
    if(!mapmaker_pointer_context(menu)) {
        pc_controls_mapmaker_pointer(0);
        mapmaker_pointer.have_last=0;
        return;
    }
    pc_controls_mapmaker_pointer(1);
    p=pc_controls_menu_hover_pointer();
    gw=(float)WORD_AT(0x004E8CF0u);gh=(float)WORD_AT(0x004E8CF4u);
    d3d8_display_output_shape((unsigned)gw,(unsigned)gh,&sw,&sh);
    if(!sh || !pc_menu_mouse_position(p,(float)sw/sh,gw,gh,&x,&y)) {
        mapmaker_pointer.have_last=0;
        return;
    }
    moved=mapmaker_pointer.have_last && (p.x!=mapmaker_pointer.last_x || p.y!=mapmaker_pointer.last_y);
    pressed=(p.left && !mapmaker_pointer.last_left) || (p.right && !mapmaker_pointer.last_right);
    if(moved || pressed) mapmaker_pointer_place(x,y);
    mapmaker_pointer.have_last=1;
    mapmaker_pointer.last_x=p.x;mapmaker_pointer.last_y=p.y;
    mapmaker_pointer.last_left=p.left;mapmaker_pointer.last_right=p.right;
}
