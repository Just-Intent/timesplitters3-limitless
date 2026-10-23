/* Asset-free contract for src/pc_mapmaker_pointer.inc and pc_mapmaker_pointer.h:
 * inert by default, editor screen only, places the game pointer exactly as
 * sub_00071140/sub_00071050 do, leaves a still mouse to the controller, and
 * never carries a held button into or out of the editor as A. */
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_menu_mouse.h"
#include "pc_mapmaker_pointer.h"

static uint32_t memory[0x800000/4];
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
static unsigned long long tick=1000;
static unsigned long long GetTickCount64(void) { return tick; }

static PcMenuPointer cursor;
static int owner_calls, owner_last=-1;
static PcMenuPointer pc_controls_menu_hover_pointer(void) { return cursor; }
static void pc_controls_mapmaker_pointer(int active) { owner_calls++; owner_last=active; }
static int row_presses;
static void pc_controls_mapmaker_row_press(void) { row_presses++; }
static int editor_last=-1;
static void pc_controls_mapmaker_editor(int active) { editor_last=active; }
static int zoom_notches;
static void pc_controls_mapmaker_zoom(int notches) { zoom_notches+=notches; }
static int pulses_a, pulses_back;
static int pulses_y;
static void pc_controls_mapmaker_pulse(int button) { if(button==0) pulses_a++; else if(button==1) pulses_back++; else if(button==2) pulses_y++; }
static void d3d8_display_output_shape(unsigned w,unsigned h,unsigned *sw,unsigned *sh) { *sw=w;*sh=h; }
/* sub_00076EF0(menu,item) highlights a row; record the call instead. */
static void sub_00076EF0(void) {}
static uint32_t highlight_menu, highlight_item;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    va_start(a,n);
    if(f==sub_00076EF0 && n==2) { highlight_menu=va_arg(a,uint32_t);highlight_item=va_arg(a,uint32_t);
        WORD_AT(highlight_menu+0x198)=highlight_item; }
    va_end(a);
    return 0;
}
#include "pc_mapmaker_pointer.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
#define NEAR(a,b) (fabs((double)(a)-(double)(b))<1e-4)

#define MENU 0x006FEBE8u
#define PLAYER 1u
#define B (0x007BA260u+PLAYER*0x1E0u)

static void set_enabled(const char *value)
{
#ifdef _WIN32
    _putenv_s("TSFP_MAPMAKER_POINTER",value?value:"");
#else
    if(value)setenv("TSFP_MAPMAKER_POINTER",value,1); else unsetenv("TSFP_MAPMAKER_POINTER");
#endif
    memset(&mapmaker_pointer,0,sizeof mapmaker_pointer);
}
static uint32_t checksum(void)
{
    uint32_t h=2166136261u;size_t i;
    for(i=0;i<sizeof memory/4;i++) { h^=memory[i];h*=16777619u; }
    return h;
}
/* Window 1280x960 shows the 640x480 game UI at exactly 2x. */
static void move(float x,float y) { cursor.x=x*2.f;cursor.y=y*2.f; }
static void game_frame(void) { mapmaker_pointer_input(MENU); }

int main(void)
{
    uint32_t before;
    PcMapMakerClick click;
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x0079094Cu)=102;
    WORD_AT(0x004E8CF0u)=640;WORD_AT(0x004E8CF4u)=480;
    FLOAT_AT(0x007BA180u)=320.f;FLOAT_AT(0x007BA184u)=240.f;   /* centre */
    FLOAT_AT(0x007BA038u)=320.f;FLOAT_AT(0x007BA188u)=240.f;   /* offset -> pixels */
    FLOAT_AT(0x007BA18Cu)=1.f/320.f;FLOAT_AT(0x007BA030u)=1.f/240.f;
    WORD_AT(0x007BA9E0u)=PLAYER;
    FLOAT_AT(B+0x48)=0.f;FLOAT_AT(B+0x4c)=0.f;FLOAT_AT(B+0x50)=640.f;FLOAT_AT(B+0x54)=480.f;
    FLOAT_AT(B+0x20)=320.f;FLOAT_AT(B+0x24)=240.f;
    cursor.width=1280.f;cursor.height=960.f;cursor.valid=1;
    move(320.f,240.f);

    /* Off by default: no guest write, no host ownership change. */
    set_enabled(NULL);
    before=checksum();
    game_frame();move(100.f,50.f);game_frame();
    CHECK(checksum()==before && owner_calls==0);
    set_enabled("2");
    game_frame();move(101.f,50.f);game_frame();
    CHECK(checksum()==before && owner_calls==0 && editor_last==-1);

    set_enabled("1");
    /* Other scenes (main menu 101, map play 100) never own the pointer. */
    WORD_AT(0x0079094Cu)=101;before=checksum();
    game_frame();move(120.f,60.f);game_frame();
    CHECK(checksum()==before && owner_last==0 && editor_last==0);
    WORD_AT(0x0079094Cu)=100;before=checksum();
    game_frame();move(121.f,60.f);game_frame();
    CHECK(checksum()==before && owner_last==0);
    WORD_AT(0x0079094Cu)=102;
    /* Editor menus with rows (mode dialog, category list) or an open child
     * (pause) select under the pointer: it follows the mouse there, but they
     * never own the left button as held A. */
    WORD_AT(MENU+0x178)=0x00301000u;
    game_frame();move(130.f,70.f);game_frame();
    CHECK(owner_last==0 && editor_last==1 && NEAR(FLOAT_AT(B+0x20),130.f) && NEAR(FLOAT_AT(B+0x24),70.f));
    WORD_AT(MENU+0x178)=0;WORD_AT(MENU+0x170)=0x006FEE24u;
    move(131.f,70.f);game_frame();
    CHECK(owner_last==0 && NEAR(FLOAT_AT(B+0x20),131.f));
    WORD_AT(MENU+0x170)=0;before=checksum();

    /* Editor screen: the first sample only records; a still mouse writes nothing. */
    game_frame();
    CHECK(owner_last==1 && checksum()==before);
    game_frame();
    CHECK(checksum()==before);

    /* Moving the mouse places the pointer: sub_00071140 then sub_00071050. */
    move(100.f,50.f);game_frame();
    CHECK(NEAR(FLOAT_AT(B+0x20),100.f) && NEAR(FLOAT_AT(B+0x24),50.f));
    CHECK(NEAR(FLOAT_AT(B+0x18),(100.f-320.f)/320.f) && NEAR(FLOAT_AT(B+0x1c),(50.f-240.f)/240.f));
    CHECK(WORD_AT(B)==0); /* inside bounds: no edge flag */
    CHECK(FLOAT_AT(0x007BA260u+0x20)==0.f); /* player 0 untouched */

    /* Controller moves the pointer; a still mouse must not pull it back. */
    FLOAT_AT(B+0x20)=400.f;FLOAT_AT(B+0x18)=0.25f;
    game_frame();game_frame();
    CHECK(FLOAT_AT(B+0x20)==400.f && FLOAT_AT(B+0x18)==0.25f);
    /* A fresh click without motion acts at the mouse. */
    cursor.left=1;game_frame();
    CHECK(NEAR(FLOAT_AT(B+0x20),100.f));
    FLOAT_AT(B+0x20)=400.f;game_frame(); /* still held: no new placement */
    CHECK(FLOAT_AT(B+0x20)==400.f);
    cursor.left=0;game_frame();

    /* Clamped like the original: bit 2 set, offset recomputed from the bound. */
    FLOAT_AT(B+0x48)=20.f;
    move(10.f,50.f);game_frame();
    CHECK(NEAR(FLOAT_AT(B+0x20),20.f) && NEAR(FLOAT_AT(B+0x18),(20.f-320.f)/320.f) && (WORD_AT(B)&2u));
    WORD_AT(B)=0;FLOAT_AT(B+0x50)=600.f;FLOAT_AT(B+0x54)=400.f;
    move(630.f,470.f);game_frame();
    CHECK(NEAR(FLOAT_AT(B+0x20),600.f) && NEAR(FLOAT_AT(B+0x18),(600.f-320.f)/320.f) && (WORD_AT(B)&2u));
    CHECK(NEAR(FLOAT_AT(B+0x24),400.f) && NEAR(FLOAT_AT(B+0x1c),(400.f-240.f)/240.f));
    WORD_AT(B)=0;

    /* Outside the displayed image or invalid player: nothing is written. */
    before=checksum();
    cursor.valid=0;move(200.f,200.f);game_frame();
    CHECK(checksum()==before);
    cursor.valid=1;game_frame();move(210.f,200.f);
    WORD_AT(0x007BA9E0u)=4;before=checksum();game_frame();
    CHECK(checksum()==before);
    WORD_AT(0x007BA9E0u)=PLAYER;

    /* Menu scrollbar (UI scale 1 for readable numbers): bar x 280..296, y 80..400,
     * track 96..384, content 640 over a 320 view, thumb 96..240 at scroll 0. */
#define SM 0x00310000u
#define STYLE 0x00311000u
    FLOAT_AT(0x007BA038u)=1.f;FLOAT_AT(0x007BA188u)=1.f;
    WORD_AT(SM+0x1c)=STYLE;WORD_AT(STYLE)=0x40u;WORD_AT(STYLE+0x70)=0;
    FLOAT_AT(STYLE+0x74)=280.f;FLOAT_AT(STYLE+0x78)=296.f;FLOAT_AT(SM+0xe4)=0.f;
    FLOAT_AT(SM+0x6c)=80.f;FLOAT_AT(SM+0x70)=400.f;FLOAT_AT(SM+0xac)=0.f;
    FLOAT_AT(SM+0x5c)=640.f;FLOAT_AT(SM+0x84)=0.f;
    WORD_AT(SM+0x178)=0x00301000u; /* a list has rows: not the editor screen */
    {
        MapMakerScroll s;
        CHECK(mapmaker_scroll_geometry(SM,&s) && NEAR(s.x0,280.f) && NEAR(s.x1,296.f) &&
              NEAR(s.track0,96.f) && NEAR(s.track1,384.f) && NEAR(s.thumb0,96.f) && NEAR(s.thumb1,240.f));
    }
    /* Driven directly through the scroll target (+0x88), never through A. */
    row_presses=0;FLOAT_AT(SM+0x88)=0.f;FLOAT_AT(SM+0x98)=32.f;
    mapmaker_pointer_scroll_press(SM,288.f,390.f);           /* down arrow: one step */
    CHECK(NEAR(FLOAT_AT(SM+0x88),32.f) && mapmaker_pointer.grab_menu==SM && mapmaker_pointer.grab_mode==2);
    tick+=100;mapmaker_pointer_scroll_drag(SM,390.f,1);      /* held, before the repeat delay */
    CHECK(NEAR(FLOAT_AT(SM+0x88),32.f));
    tick+=200;mapmaker_pointer_scroll_drag(SM,390.f,1);      /* repeats */
    CHECK(NEAR(FLOAT_AT(SM+0x88),64.f));
    tick+=40;mapmaker_pointer_scroll_drag(SM,390.f,1);
    CHECK(NEAR(FLOAT_AT(SM+0x88),64.f));
    tick+=40;mapmaker_pointer_scroll_drag(SM,390.f,1);
    CHECK(NEAR(FLOAT_AT(SM+0x88),96.f));
    mapmaker_pointer_scroll_drag(SM,390.f,0);                /* released */
    tick+=500;mapmaker_pointer_scroll_drag(SM,390.f,1);
    CHECK(NEAR(FLOAT_AT(SM+0x88),96.f) && mapmaker_pointer.grab_menu==0);
    mapmaker_pointer_scroll_press(SM,288.f,90.f);            /* up arrow */
    CHECK(NEAR(FLOAT_AT(SM+0x88),64.f) && mapmaker_pointer.grab_mode==1);
    mapmaker_pointer_scroll_drag(SM,90.f,0);
    FLOAT_AT(SM+0x88)=310.f;
    mapmaker_pointer_scroll_press(SM,288.f,390.f);           /* clamped to content - view */
    CHECK(NEAR(FLOAT_AT(SM+0x88),320.f));
    mapmaker_pointer_scroll_drag(SM,390.f,0);
    FLOAT_AT(SM+0x88)=10.f;
    mapmaker_pointer_scroll_press(SM,288.f,90.f);            /* clamped to 0 */
    CHECK(FLOAT_AT(SM+0x88)==0.f);
    mapmaker_pointer_scroll_drag(SM,90.f,0);
    FLOAT_AT(SM+0x98)=1000.f;                                /* no sensible step: a tenth of the view */
    mapmaker_pointer_scroll_press(SM,288.f,390.f);
    CHECK(NEAR(FLOAT_AT(SM+0x88),32.f));
    mapmaker_pointer_scroll_drag(SM,390.f,0);
    /* Track: centre the view there, then drag from the thumb's middle. */
    FLOAT_AT(SM+0x88)=0.f;
    mapmaker_pointer_scroll_press(SM,288.f,300.f);
    CHECK(NEAR(FLOAT_AT(SM+0x88),(300.f-96.f)/288.f*640.f-160.f) && mapmaker_pointer.grab_mode==3 &&
          NEAR(mapmaker_pointer.grab,72.f));
    mapmaker_pointer_scroll_drag(SM,300.f,1);
    CHECK(NEAR(FLOAT_AT(SM+0x88),(300.f-72.f-96.f)/288.f*640.f));
    mapmaker_pointer_scroll_drag(SM,300.f,0);
    /* Thumb: grabbed where pressed; follows the mouse. */
    FLOAT_AT(SM+0x88)=0.f;
    mapmaker_pointer_scroll_press(SM,288.f,150.f);
    CHECK(FLOAT_AT(SM+0x88)==0.f && NEAR(mapmaker_pointer.grab,54.f));
    mapmaker_pointer_scroll_drag(SM,222.f,1);                /* thumb top 168 -> scroll 160 */
    CHECK(NEAR(FLOAT_AT(SM+0x88),160.f));
    mapmaker_pointer_scroll_drag(SM,222.f,0);
    /* Beside the bar, other scenes, or a style without a bar: nothing. */
    FLOAT_AT(SM+0x88)=0.f;
    mapmaker_pointer_scroll_press(SM,100.f,150.f);
    CHECK(FLOAT_AT(SM+0x88)==0.f && mapmaker_pointer.grab_menu==0);
    WORD_AT(0x0079094Cu)=101;
    mapmaker_pointer_scroll_press(SM,288.f,390.f);
    CHECK(FLOAT_AT(SM+0x88)==0.f);
    WORD_AT(0x0079094Cu)=102;
    WORD_AT(STYLE)=0;
    mapmaker_pointer_scroll_press(SM,288.f,390.f);
    CHECK(FLOAT_AT(SM+0x88)==0.f);
    WORD_AT(STYLE)=0x40u;
    CHECK(row_presses==0);
    /* Wheel in a menu with a scrollbar: three steps per notch, up is negative. */
    FLOAT_AT(SM+0x98)=32.f;FLOAT_AT(SM+0x88)=200.f;
    CHECK(mapmaker_pointer_wheel(SM,-1) && NEAR(FLOAT_AT(SM+0x88),296.f));
    CHECK(mapmaker_pointer_wheel(SM,2) && NEAR(FLOAT_AT(SM+0x88),104.f));
    CHECK(mapmaker_pointer_wheel(SM,9) && FLOAT_AT(SM+0x88)==0.f);
    CHECK(zoom_notches==0);
    WORD_AT(STYLE)=0;
    CHECK(!mapmaker_pointer_wheel(SM,1));                  /* no bar: generic wheel keeps it */
    WORD_AT(STYLE)=0x40u;
    WORD_AT(0x0079094Cu)=101;
    CHECK(!mapmaker_pointer_wheel(SM,1));                  /* other scenes: generic wheel */
    WORD_AT(0x0079094Cu)=102;
    /* Toolbar click while a list is open: Back (up to 3, 250 ms apart), then A. */
    pulses_a=pulses_back=0;WORD_AT(SM+0x1b0)=3;
    /* Toolbar icons as sub_00335890 sees them: five 30x24 icons from x 410. */
    WORD_AT(0x007844A8u)=0x00320000u;WORD_AT(0x00765C0Cu)=1;WORD_AT(0x00320000u+0x200)=1;WORD_AT(0x00320000u+0x254)=2;
    for(int i=0;i<5;i++) {
        FLOAT_AT(0x00765BA8u+i*16)=410.f+i*36.f;FLOAT_AT(0x00765BACu+i*16)=40.f;
        FLOAT_AT(0x00765BB0u+i*16)=440.f+i*36.f;FLOAT_AT(0x00765BB4u+i*16)=64.f;
    }
    CHECK(mapmaker_toolbar_icon(420.f,50.f)==0 && mapmaker_toolbar_icon(495.f,50.f)==2);
    CHECK(mapmaker_toolbar_icon(560.f,50.f)==4 && mapmaker_toolbar_icon(400.f,50.f)==-1);
    WORD_AT(0x00320000u+0x200)=0;                            /* beginner mode: three icons */
    CHECK(mapmaker_toolbar_icon(560.f,50.f)==-1 && mapmaker_toolbar_icon(495.f,50.f)==2);
    WORD_AT(0x00320000u+0x200)=1;
    mapmaker_pointer_switch_press(SM,495.f,50.f);           /* the open list's own icon: keep it */
    CHECK(!mapmaker_pointer.switching);
    mapmaker_pointer_switch_press(SM,385.f,50.f);           /* an arrow (not an icon): switch */
    CHECK(mapmaker_pointer.switching);
    mapmaker_pointer.switching=0;
    FLOAT_AT(0x007BA038u)=320.f;FLOAT_AT(0x007BA188u)=240.f; /* pointer scale for the hover checks */
    CHECK(!mapmaker_pointer_scroll_press(SM,450.f,50.f));
    mapmaker_pointer_switch_press(SM,450.f,50.f);
    CHECK(mapmaker_pointer.switching);
    mapmaker_pointer_switch_step(SM,0);
    CHECK(pulses_back==1 && pulses_a==0);
    tick+=100;mapmaker_pointer_switch_step(SM,0);           /* waits between presses */
    CHECK(pulses_back==1);
    tick+=200;WORD_AT(SM+0x1b0)=4;mapmaker_pointer_switch_step(SM,0); /* closing: no Back */
    CHECK(pulses_back==1);
    WORD_AT(SM+0x1b0)=3;WORD_AT(SM+0x170)=0x006FF060u;mapmaker_pointer_switch_step(SM,0); /* has a child */
    CHECK(pulses_back==1);
    WORD_AT(SM+0x170)=0;mapmaker_pointer_switch_step(SM,0); /* a nested menu, open: one more Back */
    CHECK(pulses_back==2);
    tick+=100;mapmaker_pointer_switch_step(MENU,1);         /* editor screen, too soon */
    CHECK(pulses_a==0);
    tick+=200;mapmaker_pointer_switch_step(MENU,1);         /* editor screen: hover first */
    CHECK(pulses_a==0 && mapmaker_pointer.switching && NEAR(FLOAT_AT(B+0x20),451.f) && NEAR(FLOAT_AT(B+0x24),50.f));
    tick+=200;mapmaker_pointer_switch_step(MENU,1);         /* settle time not over */
    CHECK(pulses_a==0);
    tick+=120;mapmaker_pointer_switch_step(MENU,1);         /* back on the icon, then A */
    CHECK(pulses_a==1 && !mapmaker_pointer.switching && NEAR(FLOAT_AT(B+0x20),450.f));
    mapmaker_pointer_switch_step(MENU,1);
    CHECK(pulses_a==1);
    mapmaker_pointer_switch_press(SM,450.f,200.f);          /* not the toolbar */
    CHECK(!mapmaker_pointer.switching);
    mapmaker_pointer_switch_press(MENU,450.f,50.f);         /* already on the editor screen */
    CHECK(!mapmaker_pointer.switching);
    mapmaker_pointer_switch_press(SM,450.f,50.f);           /* gives up after 3 Backs ... */
    for(int i=0;i<5;i++) { mapmaker_pointer_switch_step(SM,0);tick+=260; }
    CHECK(pulses_back==5 && !mapmaker_pointer.switching);
    mapmaker_pointer_switch_press(SM,450.f,50.f);           /* ... or after 1.5 s */
    mapmaker_pointer_switch_step(SM,0);tick+=1600;mapmaker_pointer_switch_step(MENU,1);
    CHECK(pulses_a==1 && !mapmaker_pointer.switching);
    WORD_AT(0x0079094Cu)=101;
    mapmaker_pointer_switch_press(SM,450.f,50.f);           /* other scenes: never */
    CHECK(!mapmaker_pointer.switching);
    WORD_AT(0x0079094Cu)=102;
    row_presses=0;
    FLOAT_AT(0x007BA038u)=320.f;FLOAT_AT(0x007BA188u)=240.f;

    /* Pan: the game's left/top add, right/bottom recompute, centre clamp, flag. */
#define ED 0x00320000u
    WORD_AT(0x007844A8u)=ED;WORD_AT(ED+0x25c)=0;
    FLOAT_AT(0x00783DD8u)=10.f;FLOAT_AT(0x00783CC0u)=10.f;   /* left, top */
    FLOAT_AT(0x00783DE0u)=20.f;FLOAT_AT(0x00783DDCu)=15.f;   /* width, height */
    FLOAT_AT(0x00783DD4u)=.05f;FLOAT_AT(0x00783F20u)=.05f;   /* cells per pixel */
    CHECK(mapmaker_pointer_pan(1.f,2.f));
    CHECK(NEAR(FLOAT_AT(0x00783DD8u),11.f) && NEAR(FLOAT_AT(0x00783CC0u),12.f) &&
          NEAR(FLOAT_AT(0x00783DC0u),31.f) && NEAR(FLOAT_AT(0x00783DCCu),27.f) && (WORD_AT(ED+0x25c)&2u));
    CHECK(mapmaker_pointer_pan(100.f,-100.f));               /* centre stays on the grid */
    CHECK(NEAR(FLOAT_AT(0x00783DD8u),30.f) && NEAR(FLOAT_AT(0x00783CC0u),-7.5f));
    FLOAT_AT(0x00783DD8u)=10.f;FLOAT_AT(0x00783CC0u)=10.f;
    WORD_AT(ED+0x25c)=0x00004000u;                           /* a state the editor does not pan in */
    CHECK(!mapmaker_pointer_pan(1.f,1.f) && FLOAT_AT(0x00783DD8u)==10.f);
    WORD_AT(ED+0x25c)=0;
    /* Wheel on the editor screen zooms (a short White/Black hold on the host). */
    CHECK(mapmaker_pointer_wheel(MENU,2) && zoom_notches==2);
    CHECK(mapmaker_pointer_wheel(MENU,-1) && zoom_notches==1);
    zoom_notches=0;
    /* Right click without a drag: the menu Back action (rotate), on release. */
    WORD_AT(MENU+0xc4)=0;WORD_AT(ED+0x2f7c)=0x00340000u;     /* a piece is held */
    move(300.f,200.f);game_frame();
    cursor.right=1;game_frame();
    CHECK(WORD_AT(MENU+0xc4)==0);                            /* not on press */
    move(301.f,201.f);game_frame();                          /* within the click slop */
    cursor.right=0;game_frame();
    CHECK(WORD_AT(MENU+0xc4)==0x20u && NEAR(FLOAT_AT(0x00783DD8u),10.f));
    WORD_AT(MENU+0xc4)=0;
    /* ... over an empty cell with nothing held: Y (clear selection) instead. */
    WORD_AT(ED+0x2f7c)=0;WORD_AT(ED+0x2f18)=0;pulses_y=0;
    cursor.right=1;game_frame();cursor.right=0;game_frame();
    CHECK(pulses_y==1 && WORD_AT(MENU+0xc4)==0);
    WORD_AT(ED+0x2f7c)=0x00340000u;                          /* holding a piece: rotate */
    cursor.right=1;game_frame();cursor.right=0;game_frame();
    CHECK(pulses_y==1 && WORD_AT(MENU+0xc4)==0x20u);
    WORD_AT(MENU+0xc4)=0;WORD_AT(ED+0x2f7c)=0;WORD_AT(ED+0x2f18)=0x00350000u; /* occupied cell */
    cursor.right=1;game_frame();cursor.right=0;game_frame();
    CHECK(pulses_y==1 && WORD_AT(MENU+0xc4)==0x20u);
    WORD_AT(MENU+0xc4)=0;WORD_AT(ED+0x2f18)=0;WORD_AT(ED+0x2f7c)=0x00340000u;
    /* Right-drag on the editor screen grabs the map, and does not rotate. */
    move(300.f,200.f);cursor.right=1;game_frame();
    move(320.f,190.f);game_frame();                          /* +20 px right, -10 px up */
    CHECK(NEAR(FLOAT_AT(0x00783DD8u),9.f) && NEAR(FLOAT_AT(0x00783CC0u),10.5f));
    cursor.right=0;game_frame();
    CHECK(NEAR(FLOAT_AT(0x00783DD8u),9.f) && WORD_AT(MENU+0xc4)==0); /* released: no pan, no rotate */
    /* Mouse at the left edge pans left; in a menu it never pans. */
    move(2.f,200.f);game_frame();
    CHECK(NEAR(FLOAT_AT(0x00783DD8u),9.f-(MAPMAKER_EDGE-2.f)/MAPMAKER_EDGE*MAPMAKER_EDGE_SPEED));
    FLOAT_AT(0x00783DD8u)=9.f;
    move(MAPMAKER_EDGE+1.f,200.f);game_frame();             /* just inside the zone edge: no pan */
    CHECK(FLOAT_AT(0x00783DD8u)==9.f);
    move(630.f,200.f);game_frame();                         /* right edge pans right */
    CHECK(FLOAT_AT(0x00783DD8u)>9.f);
    FLOAT_AT(0x00783DD8u)=9.f;move(2.f,200.f);
    WORD_AT(MENU+0x178)=0x00301000u;FLOAT_AT(0x00783DD8u)=9.f;
    game_frame();
    CHECK(FLOAT_AT(0x00783DD8u)==9.f);
    WORD_AT(MENU+0x178)=0;
    move(320.f,240.f);game_frame();

    /* Logic list: the active menu is its passive description panel; a click
     * that missed the panel uses the parent's rows. */
#define PARENT 0x00330000u
#define PANEL 0x00331000u
#define ROW 0x00332000u
    WORD_AT(PANEL+0x2c)=PARENT;WORD_AT(PARENT+0x170)=PANEL;WORD_AT(PARENT+0x178)=ROW;
    WORD_AT(ROW)=7;WORD_AT(ROW+4)=0;WORD_AT(ROW+0x28)=0x040E0600u;
    FLOAT_AT(ROW+0x78)=25.6f;FLOAT_AT(ROW+0x7c)=107.2f;FLOAT_AT(ROW+0x80)=176.f;FLOAT_AT(ROW+0x84)=130.f;
    row_presses=0;
    CHECK(mapmaker_pointer_parent_press(PANEL,100.f,120.f)==ROW && row_presses==1 &&
          highlight_menu==PARENT && highlight_item==ROW);
    CHECK(!mapmaker_pointer_parent_press(PANEL,100.f,150.f) && row_presses==1);   /* between rows */
    WORD_AT(ROW+0x28)|=1u;                                                          /* disabled row */
    CHECK(!mapmaker_pointer_parent_press(PANEL,100.f,120.f) && row_presses==1);
    WORD_AT(ROW+0x28)&=~1u;
    WORD_AT(PARENT+0x170)=0;                                                        /* not its child */
    CHECK(!mapmaker_pointer_parent_press(PANEL,100.f,120.f) && row_presses==1);
    WORD_AT(PARENT+0x170)=PANEL;
    WORD_AT(0x0079094Cu)=101;
    CHECK(!mapmaker_pointer_parent_press(PANEL,100.f,120.f) && row_presses==1);
    WORD_AT(0x0079094Cu)=102;
    /* The panel's own entry list (one non-selectable type-8 item): A only. */
#define LIST 0x00333000u
    WORD_AT(PANEL+4)=0x0052E9D8u;WORD_AT(PANEL+0x178)=LIST;
    WORD_AT(LIST)=8;WORD_AT(LIST+4)=0;WORD_AT(LIST+0x28)=0x000C0044u;
    FLOAT_AT(LIST+0x78)=236.8f;FLOAT_AT(LIST+0x7c)=84.5f;FLOAT_AT(LIST+0x80)=614.4f;FLOAT_AT(LIST+0x84)=200.9f;
    highlight_menu=0;
    CHECK(mapmaker_pointer_parent_press(PANEL,330.f,110.f)==LIST && row_presses==2 && highlight_menu==0);
    CHECK(mapmaker_pointer_parent_press(PANEL,100.f,120.f)==ROW && row_presses==3);  /* parent rows still */
    CHECK(!mapmaker_pointer_parent_press(PANEL,620.f,110.f) && row_presses==3);
    WORD_AT(PANEL+4)=0x0052E078u;                                                   /* other menus: no */
    CHECK(mapmaker_pointer_parent_press(PANEL,330.f,110.f)!=LIST);
    WORD_AT(PANEL+4)=0;WORD_AT(PANEL+0x178)=0;
    row_presses=0;

    /* Editor menu rows: a real A press instead of the menu confirm, scene 102 only. */
    set_enabled(NULL);
    CHECK(!mapmaker_pointer_row_click() && row_presses==0);
    set_enabled("1");
    WORD_AT(0x0079094Cu)=101;
    CHECK(!mapmaker_pointer_row_click() && row_presses==0);
    WORD_AT(0x0079094Cu)=102;
    CHECK(mapmaker_pointer_row_click() && row_presses==1);

    /* Escape acts as Back while an editor menu is open, latched per press. */
    {
        PcMapMakerEscape esc={0};
        CHECK(!pc_mapmaker_escape_back(&esc,1,0));   /* editor screen: stays Start */
        CHECK(!pc_mapmaker_escape_back(&esc,1,1));   /* pause opened under the same press */
        CHECK(!pc_mapmaker_escape_back(&esc,0,1));
        CHECK(pc_mapmaker_escape_back(&esc,1,1));    /* in a menu: Back */
        CHECK(pc_mapmaker_escape_back(&esc,1,0));    /* menu closed under the press: still Back */
        CHECK(!pc_mapmaker_escape_back(&esc,0,0));
        CHECK(!pc_mapmaker_escape_back(&esc,1,0));
    }
    CHECK(pc_mapmaker_key(0,0x1B,0x09,1,1)==1);  /* outside the editor: Start as bound */
    CHECK(pc_mapmaker_key(1,0x1B,0x09,1,0)==1);  /* editor screen: Escape still Start */
    CHECK(pc_mapmaker_key(1,0x09,0x09,0,0)==0);
    CHECK(pc_mapmaker_key(1,0x1B,0x09,1,1)==0);  /* menu open: Escape no longer Start */
    CHECK(pc_mapmaker_key(1,0x09,0x09,0,1)==1);  /* ... but Back */
    CHECK(pc_mapmaker_key(1,0x09,0x09,1,0)==1);  /* Tab still Back */
    CHECK(pc_mapmaker_key(1,0x09,0x09,0,0)==0);
    CHECK(pc_mapmaker_key(1,'W',0x09,1,1)==1);   /* other keys untouched */
    CHECK(pc_mapmaker_key(1,0x1B,0,1,1)==1);     /* no Back key: leave Escape alone */
    CHECK(pc_mapmaker_key(1,0x1B,0x1B,1,1)==1);  /* Escape is Back already */
    /* Movement keys: up in the editor; strafe left/right alias the D-pad. */
    {
        const int move[4]={'W','S','A','D'};
        CHECK(pc_mapmaker_move_key(1,'A',move,1)==0 && pc_mapmaker_move_key(1,'W',move,1)==0);
        CHECK(pc_mapmaker_move_key(0,'A',move,1)==1);            /* outside the editor */
        CHECK(pc_mapmaker_move_key(1,'E',move,1)==1);            /* other keys */
        CHECK(pc_mapmaker_dpad_alias(1,0x25,move)=='A' && pc_mapmaker_dpad_alias(1,0x27,move)=='D');
        CHECK(!pc_mapmaker_dpad_alias(1,0x26,move) && !pc_mapmaker_dpad_alias(0,0x25,move));
    }
    /* Wheel zoom: each notch holds White (up) or Black (down) for 100 ms. */
    {
        PcMapMakerZoom z={0};
        CHECK(!pc_mapmaker_zoom_key(&z,1,'1','1','2',0,1000));
        pc_mapmaker_zoom_add(&z,1,1000);
        CHECK(pc_mapmaker_zoom_key(&z,1,'1','1','2',0,1050));     /* White held */
        CHECK(!pc_mapmaker_zoom_key(&z,1,'2','1','2',0,1050));
        CHECK(!pc_mapmaker_zoom_key(&z,0,'1','1','2',0,1050));    /* outside the editor */
        CHECK(!pc_mapmaker_zoom_key(&z,1,'1','1','2',0,1100));    /* expired */
        pc_mapmaker_zoom_add(&z,2,1100);pc_mapmaker_zoom_add(&z,1,1150);
        CHECK(pc_mapmaker_zoom_key(&z,1,'1','1','2',0,1390));     /* notches add up */
        CHECK(!pc_mapmaker_zoom_key(&z,1,'1','1','2',0,1400));
        pc_mapmaker_zoom_add(&z,3,2000);pc_mapmaker_zoom_add(&z,-1,2050); /* reversal starts over */
        CHECK(!pc_mapmaker_zoom_key(&z,1,'1','1','2',0,2060) && pc_mapmaker_zoom_key(&z,1,'2','1','2',0,2060));
        CHECK(!pc_mapmaker_zoom_key(&z,1,'2','1','2',0,2150));
        pc_mapmaker_zoom_add(&z,40,3000);
        CHECK(pc_mapmaker_zoom_key(&z,1,'1','1','2',0,3999) && !pc_mapmaker_zoom_key(&z,1,'1','1','2',0,4000));
        CHECK(pc_mapmaker_zoom_key(&z,1,'W','1','2',1,3500));     /* other keys untouched */
    }
    /* T raises the floor (right trigger) in the editor only. */
    CHECK(pc_mapmaker_trigger_key(1,0x01,0x01,0,1)==1);   /* fire key (left mouse) reports T */
    CHECK(pc_mapmaker_trigger_key(1,0x01,0x01,0,0)==0);
    CHECK(pc_mapmaker_trigger_key(0,0x01,0x01,0,1)==0);   /* outside the editor: Fire as bound */
    CHECK(pc_mapmaker_trigger_key(1,'G',0x01,1,1)==1);    /* other keys untouched */
    CHECK(pc_mapmaker_trigger_key(1,'G',0x01,0,1)==0);

    /* Host click policy: held left = held A only while the editor owns it. */
    memset(&click,0,sizeof click);
    CHECK(!pc_mapmaker_click(&click,0,1));      /* outside the editor */
    CHECK(!pc_mapmaker_click(&click,1,1));      /* held into the editor: blocked */
    CHECK(!pc_mapmaker_click(&click,1,0));
    CHECK(pc_mapmaker_click(&click,1,1));       /* fresh press */
    CHECK(pc_mapmaker_click(&click,1,1));       /* held: drag */
    CHECK(!pc_mapmaker_click(&click,0,1));      /* list opened under the click */
    CHECK(!pc_mapmaker_click(&click,1,1));      /* list closed, still held: blocked */
    CHECK(!pc_mapmaker_click(&click,1,0));
    CHECK(pc_mapmaker_click(&click,1,1));
    /* Row press in a list: A held for that press only. */
    memset(&click,0,sizeof click);
    CHECK(!pc_mapmaker_click(&click,0,1));      /* press lands on a row ... */
    pc_mapmaker_row_press(&click);
    CHECK(pc_mapmaker_click(&click,0,1));       /* ... and holds A */
    CHECK(pc_mapmaker_click(&click,0,1));
    CHECK(!pc_mapmaker_click(&click,0,0));      /* release ends it */
    CHECK(!pc_mapmaker_click(&click,0,1));      /* next press elsewhere: no A */
    CHECK(!pc_mapmaker_click(&click,0,0));
    pc_mapmaker_row_press(&click);
    CHECK(!pc_mapmaker_click(&click,0,0));      /* press already released: nothing */
    CHECK(!pc_mapmaker_click(&click,0,1));
    pc_mapmaker_row_press(&click);
    CHECK(pc_mapmaker_click(&click,0,1));
    CHECK(!pc_mapmaker_click(&click,1,1));      /* list closed under the held click */
    CHECK(!pc_mapmaker_click(&click,1,1));

    if(failures)return 1;
    puts("mapmaker pointer ok");
    return 0;
}
