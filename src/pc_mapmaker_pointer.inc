/* US 45410066 MapMaker editor pointer (prototype; off unless
 * TSFP_MAPMAKER_POINTER=1).
 *
 * The editor screen (scene 102) draws its toolbar, floor buttons and grid
 * itself; they are not menu rows. They hit-test the game's own per-player
 * pointer (sub_000711F0): floats at 0x007BA260 + player*0x1E0 + 0x20/+0x24,
 * player index 0x007BA9E0. The game moves that pointer from the stick in
 * sub_00073870 and places it with sub_00071140(x, y).
 *
 * After the original sub_00073870 for any menu dispatched in scene 102, when
 * the mouse has moved or a button was pressed, place the pointer at the mouse
 * exactly as sub_00071140 + sub_00071050 do: normalised offsets at
 * +0x18/+0x1C, position at +0x20/+0x24, clamped to +0x48..+0x54 with bit 2 of
 * +0x00 set when clamped. A still mouse leaves controller movement alone.
 * Editor menus (mode dialog, pause menu, category lists; menu flag 0x8000)
 * also select the row under this pointer, so it follows the mouse there too.
 * Only the editor screen itself (no menu rows, no open child) owns the left
 * button as held A.
 * The left button becomes the A button on the host side
 * (pc_controls_mapmaker_pointer, pc_mapmaker_pointer.h). A click on an editor
 * menu row becomes a real A press too (mapmaker_pointer_row_click).
 *
 * Navigation (editor screen only): right-drag grabs and pans the map, and the
 * mouse at a window edge pans like the right stick. Both apply the game's pan
 * (sub_00309160: view left/top +=, right/bottom recomputed) and its centre
 * clamp to the 40x40 grid (sub_002F9F70), and are skipped in the states where
 * the editor itself does not pan (editor+0x25C & 0x0C044088).
 *
 * Menu scrollbars (tile, item and Story AI lists): A over the bar did not
 * scroll the Story AI list in game, so the scrollbar is driven directly. A
 * left press on an arrow steps the scroll target (menu+0x88) and repeats while
 * held, a press on the track jumps there (as sub_000736F0), and a press on the
 * thumb drags it. The target is clamped like sub_00073560; the engine then
 * animates the scroll itself (sub_000731B0).
 *
 * Requires the includer's WORD_AT, FLOAT_AT, guest_writable,
 * d3d8_display_output_shape, pc_menu_mouse_position,
 * pc_controls_menu_hover_pointer, pc_controls_mapmaker_pointer and
 * pc_controls_mapmaker_row_press, pc_controls_mapmaker_editor,
 * pc_controls_mapmaker_zoom and pc_controls_mapmaker_pulse. */
#include <math.h>
#include <stdlib.h>
#include "pc_mapmaker_pointer.h"

#define MAPMAKER_POINTER_SCENE 102
#define MAPMAKER_POINTER_PLAYER 0x007BA9E0u
#define MAPMAKER_POINTER_BLOCK 0x007BA260u
#define MAPMAKER_POINTER_STRIDE 0x1E0u
#define MAPMAKER_EDITOR 0x007844A8u
#define MAPMAKER_EDGE 24.f           /* UI pixels from the canvas edge (of 640x480) */
#define MAPMAKER_EDGE_SPEED 0.25f    /* cells per frame at the very edge */
#define MAPMAKER_GRID 40.f
#define MAPMAKER_SCROLL_DELAY 300u   /* ms before an arrow repeats */
#define MAPMAKER_SCROLL_REPEAT 80u   /* ms between repeats */
/* Toolbar strip (category icons and arrows), UI canvas; from the recorded clicks. */
#define MAPMAKER_TOOLBAR_X0 370.f
#define MAPMAKER_TOOLBAR_X1 620.f
#define MAPMAKER_TOOLBAR_Y0 35.f
#define MAPMAKER_TOOLBAR_Y1 70.f
#define MAPMAKER_SWITCH_BACKS 3      /* menus closed at most */
#define MAPMAKER_SWITCH_GAP 250u     /* ms between Back presses */
#define MAPMAKER_SWITCH_TIMEOUT 1500u
#define MAPMAKER_SWITCH_HOVER 120u   /* ms for the toolbar to take the hover before A */

static struct {
    int initialized, enabled;
    int have_last, last_left, last_right;
    float last_x, last_y, last_ux, last_uy;
    uint32_t grab_menu;  /* menu whose scrollbar the left press is driving */
    int grab_mode;       /* 1 up arrow, 2 down arrow, 3 thumb */
    float grab;          /* pointer y minus thumb top at the press */
    unsigned long long grab_next; /* next arrow repeat */
    int switching;       /* toolbar click while a menu was open: close it, then A */
    int switch_backs, switch_hover;
    float switch_x, switch_y;
    unsigned long long switch_at, switch_next;
    float right_travel;  /* UI pixels moved while the right button is held */
} mapmaker_pointer;
#define MAPMAKER_RIGHT_CLICK 4.f /* less travel than this is a click, not a drag */

static int mapmaker_pointer_enabled(void)
{
    if(!mapmaker_pointer.initialized) {
        const char *v=getenv("TSFP_MAPMAKER_POINTER");
        mapmaker_pointer.initialized=1;
        mapmaker_pointer.enabled=v && v[0]=='1' && !v[1];
    }
    return mapmaker_pointer.enabled;
}

static int mapmaker_pointer_scene(uint32_t menu)
{
    return guest_writable(0x0079094Cu,4) &&
        (int32_t)WORD_AT(0x0079094Cu)==MAPMAKER_POINTER_SCENE && guest_writable(menu,0x23c);
}
/* The editor screen is the dispatched menu with no rows and no child. */
static int mapmaker_pointer_context(uint32_t menu)
{
    return mapmaker_pointer_scene(menu) && !WORD_AT(menu+0x178) && !WORD_AT(menu+0x170);
}

static uint32_t mapmaker_pointer_block(void)
{
    uint32_t player,b;
    if(!guest_writable(MAPMAKER_POINTER_PLAYER,4)) return 0;
    player=WORD_AT(MAPMAKER_POINTER_PLAYER);
    if(player>=4) return 0;
    b=MAPMAKER_POINTER_BLOCK+player*MAPMAKER_POINTER_STRIDE;
    return guest_writable(b,0x58)?b:0;
}

/* sub_00071140(x, y) followed by sub_00071050, for the current player. */
static int mapmaker_pointer_place(float x,float y)
{
    uint32_t b;
    float cx,cy,sx,sy,ix,iy,px,py;
    if(!guest_writable(0x007BA030u,4) || !guest_writable(0x007BA038u,4) ||
       !guest_writable(0x007BA180u,0x10) || !(b=mapmaker_pointer_block())) return 0;
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

/* Scrollbar geometry as sub_00071400 computes it, in UI pixels: bar x/y, the
 * track (bar minus one bar width at each end) and the thumb. */
typedef struct MapMakerScroll { float x0,y0,x1,y1,track0,track1,thumb0,thumb1,content; } MapMakerScroll;
static int mapmaker_scroll_geometry(uint32_t menu,MapMakerScroll *s)
{
    uint32_t style;
    float kx,ky,base,w,view,cur;
    if(!guest_writable(menu,0x23c) || !guest_writable(0x007BA038u,4) || !guest_writable(0x007BA188u,4)) return 0;
    style=WORD_AT(menu+0x1c);
    if(!guest_writable(style,0x80) || !(WORD_AT(style)&0x40u)) return 0;
    s->content=FLOAT_AT(menu+0x5c);view=FLOAT_AT(menu+0x70)-FLOAT_AT(menu+0x6c);
    if(!isfinite(s->content) || !isfinite(view) || !(s->content>view) || !(s->content>0)) return 0;
    kx=FLOAT_AT(0x007BA038u);ky=FLOAT_AT(0x007BA188u);
    base=FLOAT_AT(menu+(WORD_AT(style+0x70)?0xecu:0xe4u));
    s->x0=FLOAT_AT(style+0x74)*kx+base;s->x1=FLOAT_AT(style+0x78)*kx+base;
    s->y0=FLOAT_AT(menu+0x6c)*ky+FLOAT_AT(menu+0xac);s->y1=FLOAT_AT(menu+0x70)*ky+FLOAT_AT(menu+0xac);
    w=s->x1-s->x0;
    s->track0=s->y0+w;s->track1=s->y1-w;
    cur=FLOAT_AT(menu+0x84);
    s->thumb0=s->track0+(s->track1-s->track0)*(cur/s->content);
    s->thumb1=s->track0+(s->track1-s->track0)*((view+cur)/s->content);
    return isfinite(s->x0) && isfinite(s->x1) && isfinite(s->y0) && isfinite(s->y1) &&
        isfinite(s->thumb0) && isfinite(s->thumb1) && s->x1>s->x0 && s->track1>s->track0;
}

/* A fresh left press that hit no row: on an editor menu's scrollbar, hold A
 * for the press (arrows, track and thumb are the engine's own A actions). */
/* Read-only: what the scrollbar test saw, when the probe logs menus (flag 8). */
static void mapmaker_pointer_scroll_log(uint32_t menu,float x,float y,int ok,const MapMakerScroll *s)
{
#ifdef MAPMAKER_PROBE_MENUS
    uint32_t style=guest_writable(menu,0x23c)?WORD_AT(menu+0x1c):0u;
    int known=guest_writable(menu,0x23c) && guest_writable(style,0x80) &&
        guest_writable(0x007BA038u,4) && guest_writable(0x007BA188u,4);
    if(!(mapmaker_probe_flags()&MAPMAKER_PROBE_MENUS)) return;
    mapmaker_probe_write("[mapmaker-probe] t=%llu scroll-press menu=0x%08X at=%.1f,%.1f geometry=%d"
        " bar=%.1f,%.1f,%.1f,%.1f track=%.1f..%.1f thumb=%.1f..%.1f"
        " style+70=%08X +74=%.3f +78=%.3f menu+E4=%.1f +EC=%.1f +AC=%.1f k=%.3f,%.3f\n",
        GetTickCount64(),menu,(double)x,(double)y,ok,
        ok?(double)s->x0:0.,ok?(double)s->y0:0.,ok?(double)s->x1:0.,ok?(double)s->y1:0.,
        ok?(double)s->track0:0.,ok?(double)s->track1:0.,ok?(double)s->thumb0:0.,ok?(double)s->thumb1:0.,
        known?WORD_AT(style+0x70):0u,known?(double)FLOAT_AT(style+0x74):0.,known?(double)FLOAT_AT(style+0x78):0.,
        known?(double)FLOAT_AT(menu+0xe4):0.,known?(double)FLOAT_AT(menu+0xec):0.,known?(double)FLOAT_AT(menu+0xac):0.,
        known?(double)FLOAT_AT(0x007BA038u):0.,known?(double)FLOAT_AT(0x007BA188u):0.);
#else
    (void)menu;(void)x;(void)y;(void)ok;(void)s;
#endif
}

/* sub_00073560: set the scroll target, clamped to the scrollable range. */
static void mapmaker_scroll_to(uint32_t menu,const MapMakerScroll *s,float target)
{
    float view=FLOAT_AT(menu+0x70)-FLOAT_AT(menu+0x6c),last=s->content-view;
    if(!isfinite(target)) return;
    if(target>last) target=last;
    if(target<0) target=0;
    FLOAT_AT(menu+0x88)=target;
}
/* One arrow step: the menu's own step (+0x98) when sensible, else a tenth of the view. */
static float mapmaker_scroll_step(uint32_t menu)
{
    float view=FLOAT_AT(menu+0x70)-FLOAT_AT(menu+0x6c),step=FLOAT_AT(menu+0x98);
    return isfinite(step) && step>0 && step<=view ? step : view*.1f;
}

static int mapmaker_pointer_scroll_press(uint32_t menu,float x,float y)
{
    MapMakerScroll s;
    int ok;
    mapmaker_pointer.grab_menu=0;
    if(!mapmaker_pointer_enabled() || !mapmaker_pointer_scene(menu)) return 0;
    ok=mapmaker_scroll_geometry(menu,&s);
    mapmaker_pointer_scroll_log(menu,x,y,ok,&s);
    if(!ok || x<s.x0 || x>=s.x1 || y<s.y0 || y>=s.y1) return 0;
    mapmaker_pointer.grab_menu=menu;
    if(y<s.track0 || y>=s.track1) {
        mapmaker_pointer.grab_mode=y<s.track0?1:2;
        mapmaker_scroll_to(menu,&s,FLOAT_AT(menu+0x88)+
            (mapmaker_pointer.grab_mode==1?-1.f:1.f)*mapmaker_scroll_step(menu));
        mapmaker_pointer.grab_next=GetTickCount64()+MAPMAKER_SCROLL_DELAY;
        return 1;
    }
    if(y<s.thumb0 || y>=s.thumb1) {
        /* Track: centre the view there (sub_000736F0), then drag from it. */
        float view=FLOAT_AT(menu+0x70)-FLOAT_AT(menu+0x6c);
        mapmaker_scroll_to(menu,&s,(y-s.track0)/(s.track1-s.track0)*s.content-view*.5f);
        mapmaker_pointer.grab=(s.thumb1-s.thumb0)*.5f;
    } else mapmaker_pointer.grab=y-s.thumb0;
    mapmaker_pointer.grab_mode=3;
    return 1;
}

/* A left press on the toolbar while an editor menu is open (it hit nothing in
 * that menu): the editor only acts on the toolbar from its own screen, and on
 * real buttons. Close the menu with short B presses (Back is the 3D view),
 * then press A once the editor screen is back; the game pointer is still over
 * the clicked icon. */
static void mapmaker_pointer_switch_press(uint32_t menu,float x,float y)
{
    if(!mapmaker_pointer_enabled() || !mapmaker_pointer_scene(menu) || mapmaker_pointer_context(menu) ||
       x<MAPMAKER_TOOLBAR_X0 || x>=MAPMAKER_TOOLBAR_X1 || y<MAPMAKER_TOOLBAR_Y0 || y>=MAPMAKER_TOOLBAR_Y1) return;
    mapmaker_pointer.switching=1;mapmaker_pointer.switch_backs=0;mapmaker_pointer.switch_hover=0;
    mapmaker_pointer.switch_x=x;mapmaker_pointer.switch_y=y;
    mapmaker_pointer.switch_at=GetTickCount64();mapmaker_pointer.switch_next=0;
}
static void mapmaker_pointer_switch_step(uint32_t menu,int root)
{
    unsigned long long now=GetTickCount64();
    if(!mapmaker_pointer.switching) return;
    if(now-mapmaker_pointer.switch_at>MAPMAKER_SWITCH_TIMEOUT) { mapmaker_pointer.switching=0;return; }
    if(now<mapmaker_pointer.switch_next) return;
    if(root) {
        /* Back on the editor screen. A right away would act on the category
         * that was highlighted before (the list reopened): move the pointer
         * off and back onto the clicked icon, give the toolbar time to take
         * the hover, then press A. */
        if(!mapmaker_pointer.switch_hover) {
            mapmaker_pointer_place(mapmaker_pointer.switch_x+1.f,mapmaker_pointer.switch_y);
            mapmaker_pointer.switch_hover=1;
            mapmaker_pointer.switch_next=now+MAPMAKER_SWITCH_HOVER;
            return;
        }
        mapmaker_pointer_place(mapmaker_pointer.switch_x,mapmaker_pointer.switch_y);
        pc_controls_mapmaker_pulse(PC_MAPMAKER_PULSE_A);
        mapmaker_pointer.switching=0;
        return;
    }
    /* B only to a menu that is fully open and in front: a closing menu
     * (state 4/5) or one under an open child would pass it on. */
    if(!guest_writable(menu,0x23c) || WORD_AT(menu+0x1b0)!=3 || WORD_AT(menu+0x170)) return;
    if(mapmaker_pointer.switch_backs>=MAPMAKER_SWITCH_BACKS) { mapmaker_pointer.switching=0;return; }
    pc_controls_mapmaker_pulse(PC_MAPMAKER_PULSE_CANCEL);
    mapmaker_pointer.switch_backs++;
    mapmaker_pointer.switch_next=now+MAPMAKER_SWITCH_GAP;
}

/* Wheel notches (positive up) in the editor: zoom on the editor screen, or
 * scroll a menu with a scrollbar by three steps per notch (the generic wheel
 * moves the row selection, which the Story AI lists ignore). Returns 1 when
 * the notches were used, so the generic wheel must not also apply them. */
static int mapmaker_pointer_wheel(uint32_t menu,int notches)
{
    MapMakerScroll s;
    if(!notches || !mapmaker_pointer_enabled() || !mapmaker_pointer_scene(menu)) return 0;
    if(mapmaker_pointer_context(menu)) { pc_controls_mapmaker_zoom(notches);return 1; }
    if(!mapmaker_scroll_geometry(menu,&s)) return 0;
    mapmaker_scroll_to(menu,&s,FLOAT_AT(menu+0x88)-(float)notches*3.f*mapmaker_scroll_step(menu));
    return 1;
}

/* Every frame of the press: repeat arrows, or keep the thumb under the mouse. */
static void mapmaker_pointer_scroll_drag(uint32_t menu,float y,int left)
{
    MapMakerScroll s;
    unsigned long long now;
    if(!left) { mapmaker_pointer.grab_menu=0;return; }
    if(menu!=mapmaker_pointer.grab_menu || !mapmaker_scroll_geometry(menu,&s)) return;
    if(mapmaker_pointer.grab_mode==3) {
        mapmaker_scroll_to(menu,&s,(y-mapmaker_pointer.grab-s.track0)/(s.track1-s.track0)*s.content);
        return;
    }
    now=GetTickCount64();
    if(now<mapmaker_pointer.grab_next) return;
    mapmaker_pointer.grab_next=now+MAPMAKER_SCROLL_REPEAT;
    mapmaker_scroll_to(menu,&s,FLOAT_AT(menu+0x88)+
        (mapmaker_pointer.grab_mode==1?-1.f:1.f)*mapmaker_scroll_step(menu));
}

/* sub_00309160 then the sub_002F9F70 centre clamp; dx/dy in grid cells. */
static int mapmaker_pointer_pan(float dx,float dy)
{
    uint32_t editor;
    float left,top,w,h,c;
    if(!guest_writable(MAPMAKER_EDITOR,4)) return 0;
    editor=WORD_AT(MAPMAKER_EDITOR);
    if(!guest_writable(editor,0x260) || (WORD_AT(editor+0x25c)&0x0C044088u) ||
       !guest_writable(0x00783CC0u,4) || !guest_writable(0x00783DC0u,4) ||
       !guest_writable(0x00783DCCu,4) || !guest_writable(0x00783DD8u,0xc)) return 0;
    left=FLOAT_AT(0x00783DD8u)+dx;top=FLOAT_AT(0x00783CC0u)+dy;
    w=FLOAT_AT(0x00783DE0u);h=FLOAT_AT(0x00783DDCu);
    if(!isfinite(left) || !isfinite(top) || !isfinite(w) || !isfinite(h) || !(w>0) || !(h>0)) return 0;
    c=left+w*.5f; if(c<0) left-=c; else if(c>MAPMAKER_GRID) left-=c-MAPMAKER_GRID;
    c=top+h*.5f;  if(c<0) top-=c;  else if(c>MAPMAKER_GRID) top-=c-MAPMAKER_GRID;
    FLOAT_AT(0x00783DD8u)=left;FLOAT_AT(0x00783CC0u)=top;
    FLOAT_AT(0x00783DC0u)=left+w;FLOAT_AT(0x00783DCCu)=top+h;
    WORD_AT(editor+0x25c)|=2u; /* as the editor's own pan */
    return 1;
}

/* Editor screen: right-drag grabs the map; the window edge pans. A right
 * click without a drag keeps its original meaning (the menu Back action, which
 * the editor uses to rotate the held piece), sent on release, except over an
 * empty cell with no piece held, where it presses Y (clears the selection).
 * Fields from static analysis: editor+0x2F7C held piece, +0x2F18 cell under
 * the pointer. */
static int mapmaker_pointer_right_clears(void)
{
    uint32_t editor=guest_writable(MAPMAKER_EDITOR,4)?WORD_AT(MAPMAKER_EDITOR):0u;
    return guest_writable(editor,0x2f80) && !WORD_AT(editor+0x2f7c) && !WORD_AT(editor+0x2f18);
}
static void mapmaker_pointer_navigate(uint32_t menu,float x,float y,float gw,float gh,int right)
{
    float sx,sy,ex=0,ey=0;
    if(right && !mapmaker_pointer.last_right) mapmaker_pointer.right_travel=0;
    if(!right && mapmaker_pointer.last_right && mapmaker_pointer.have_last) {
        if(mapmaker_pointer.right_travel<MAPMAKER_RIGHT_CLICK && guest_writable(menu,0x23c)) {
            if(mapmaker_pointer_right_clears()) pc_controls_mapmaker_pulse(PC_MAPMAKER_PULSE_Y);
            else WORD_AT(menu+0xc4)|=0x20u; /* as the shared hook's right click */
        }
        mapmaker_pointer.right_travel=MAPMAKER_RIGHT_CLICK;
    }
    if(right) {
        if(mapmaker_pointer.have_last && mapmaker_pointer.last_right)
            mapmaker_pointer.right_travel+=fabsf(x-mapmaker_pointer.last_ux)+fabsf(y-mapmaker_pointer.last_uy);
        if(mapmaker_pointer.have_last && mapmaker_pointer.last_right &&
           mapmaker_pointer.right_travel>=MAPMAKER_RIGHT_CLICK &&
           guest_writable(0x00783DD4u,4) && guest_writable(0x00783F20u,4)) {
            sx=FLOAT_AT(0x00783DD4u);sy=FLOAT_AT(0x00783F20u); /* cells per pixel */
            if((x!=mapmaker_pointer.last_ux || y!=mapmaker_pointer.last_uy) && isfinite(sx) && isfinite(sy))
                mapmaker_pointer_pan(-(x-mapmaker_pointer.last_ux)*sx,-(y-mapmaker_pointer.last_uy)*sy);
        }
        return;
    }
    if(x<MAPMAKER_EDGE) ex=-(MAPMAKER_EDGE-x)/MAPMAKER_EDGE;
    else if(x>gw-MAPMAKER_EDGE) ex=(x-(gw-MAPMAKER_EDGE))/MAPMAKER_EDGE;
    if(y<MAPMAKER_EDGE) ey=-(MAPMAKER_EDGE-y)/MAPMAKER_EDGE;
    else if(y>gh-MAPMAKER_EDGE) ey=(y-(gh-MAPMAKER_EDGE))/MAPMAKER_EDGE;
    if(ex || ey) mapmaker_pointer_pan(fminf(1.f,ex)*MAPMAKER_EDGE_SPEED,fminf(1.f,ey)*MAPMAKER_EDGE_SPEED);
}

/* The logic list keeps a passive description panel open as its child, so
 * the engine (sub_00074760) treats the panel as the active menu and the PC
 * hook only hit-tests the panel. A left press that hit nothing there presses A
 * over the panel's own entry list, or else tries the parent's rows: highlight
 * the row (sub_00076EF0) and press A for it. */
static uint32_t mapmaker_pointer_parent_press(uint32_t menu,float x,float y)
{
    uint32_t parent,item;
    unsigned n;
    if(!mapmaker_pointer_enabled() || !mapmaker_pointer_scene(menu)) return 0;
    /* The panel itself lists the existing entries inside one non-selectable
     * type-8 item drawn by sub_00327520. Its draw code (sub_00323C30) tests
     * each entry against the game pointer and acts on A (+0xC4 & 0x10), so
     * a click there only needs A. */
    if(WORD_AT(menu+4)==0x0052E9D8u || WORD_AT(menu+4)==0x0052E9E8u) {
        item=WORD_AT(menu+0x178);
        for(n=0;item && n<64 && guest_writable(item,0x10c);n++,item=WORD_AT(item+4))
            if(WORD_AT(item)==8 &&
               x>=FLOAT_AT(item+0x78) && x<FLOAT_AT(item+0x80) &&
               y>=FLOAT_AT(item+0x7c) && y<FLOAT_AT(item+0x84)) {
                pc_controls_mapmaker_row_press();
                return item;
            }
    }
    parent=WORD_AT(menu+0x2c);
    if(!guest_writable(parent,0x23c) || WORD_AT(parent+0x170)!=menu) return 0;
    item=WORD_AT(parent+0x178);
    for(n=0;item && n<256 && guest_writable(item,0x10c);n++,item=WORD_AT(item+4)) {
        uint32_t type=WORD_AT(item),flags=WORD_AT(item+0x28);
        if((type==4 || type==7 || type==8) && !(flags&5) &&
           x>=FLOAT_AT(item+0x78) && x<FLOAT_AT(item+0x80) &&
           y>=FLOAT_AT(item+0x7c) && y<FLOAT_AT(item+0x84)) {
            if(WORD_AT(parent+0x198)!=item) guest_call(sub_00076EF0,2,parent,item);
            pc_controls_mapmaker_row_press();
            return item;
        }
    }
    return 0;
}

/* Editor menus (pause menu, category lists) are normal menu rows that the PC
 * layer already hit-tests and highlights, but the editor acts on the A button
 * itself, not on the menu engine's confirm. Called for a fresh left click that
 * hit a row: returns 1 when it requested a real A press instead. */
static int mapmaker_pointer_row_click(void)
{
    if(!mapmaker_pointer_enabled() || !guest_writable(0x0079094Cu,4) ||
       (int32_t)WORD_AT(0x0079094Cu)!=MAPMAKER_POINTER_SCENE) return 0;
    pc_controls_mapmaker_row_press();
    return 1;
}

/* Called after the original sub_00073870 for every dispatched menu. */
static void mapmaker_pointer_input(uint32_t menu)
{
    PcMenuPointer p;
    float gw,gh,x,y;
    unsigned sw,sh;
    int moved,pressed,root;
    if(!mapmaker_pointer_enabled()) return;
    root=mapmaker_pointer_context(menu);
    pc_controls_mapmaker_pointer(root);
    pc_controls_mapmaker_editor(mapmaker_pointer_scene(menu)); /* Escape acts as Back */
    if(!mapmaker_pointer_scene(menu)) {
        mapmaker_pointer.have_last=0;mapmaker_pointer.grab_menu=0;
        return;
    }
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
    mapmaker_pointer_scroll_drag(menu,y,p.left);
    mapmaker_pointer_switch_step(menu,root);
    if(root) mapmaker_pointer_navigate(menu,x,y,gw,gh,p.right);
    mapmaker_pointer.have_last=1;
    mapmaker_pointer.last_x=p.x;mapmaker_pointer.last_y=p.y;
    mapmaker_pointer.last_ux=x;mapmaker_pointer.last_uy=y;
    mapmaker_pointer.last_left=p.left;mapmaker_pointer.last_right=p.right;
}
