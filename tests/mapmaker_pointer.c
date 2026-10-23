/* Asset-free contract for src/pc_mapmaker_pointer.inc and pc_mapmaker_pointer.h:
 * inert by default, editor screen only, places the game pointer exactly as
 * sub_00071140/sub_00071050 do, leaves a still mouse to the controller, and
 * never carries a held button into or out of the editor as A. */
#include <math.h>
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

static PcMenuPointer cursor;
static int owner_calls, owner_last=-1;
static PcMenuPointer pc_controls_menu_hover_pointer(void) { return cursor; }
static void pc_controls_mapmaker_pointer(int active) { owner_calls++; owner_last=active; }
static int row_presses;
static void pc_controls_mapmaker_row_press(void) { row_presses++; }
static int editor_last=-1;
static void pc_controls_mapmaker_editor(int active) { editor_last=active; }
static void d3d8_display_output_shape(unsigned w,unsigned h,unsigned *sw,unsigned *sh) { *sw=w;*sh=h; }
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
    {
        MapMakerScroll s;
        CHECK(mapmaker_scroll_geometry(SM,&s) && NEAR(s.x0,280.f) && NEAR(s.x1,296.f) &&
              NEAR(s.track0,96.f) && NEAR(s.track1,384.f) && NEAR(s.thumb0,96.f) && NEAR(s.thumb1,240.f));
    }
    row_presses=0;
    mapmaker_pointer_scroll_press(SM,288.f,150.f);           /* on the thumb */
    CHECK(row_presses==1 && mapmaker_pointer.grab_menu==SM && NEAR(mapmaker_pointer.grab,54.f));
    mapmaker_pointer_scroll_press(SM,100.f,150.f);           /* beside the bar */
    CHECK(row_presses==1 && mapmaker_pointer.grab_menu==0);
    mapmaker_pointer_scroll_press(SM,288.f,390.f);           /* down arrow */
    CHECK(row_presses==2 && mapmaker_pointer.grab_menu==0);
    WORD_AT(0x0079094Cu)=101;
    mapmaker_pointer_scroll_press(SM,288.f,150.f);           /* other scenes: never */
    CHECK(row_presses==2);
    WORD_AT(0x0079094Cu)=102;
    WORD_AT(STYLE)=0;
    mapmaker_pointer_scroll_press(SM,288.f,150.f);           /* style without a bar */
    CHECK(row_presses==2);
    WORD_AT(STYLE)=0x40u;
    /* Thumb drag: the engine's speed field makes the thumb follow the mouse. */
    mapmaker_pointer_scroll_press(SM,288.f,150.f);
    FLOAT_AT(B+0x34)=0.f;
    mapmaker_pointer_scroll_drag(SM,222.f,1);                /* engine not dragging yet */
    CHECK(FLOAT_AT(B+0x34)==0.f);
    WORD_AT(SM+0x194)=0xFFFFFFFEu;
    mapmaker_pointer_scroll_drag(SM,222.f,1);                /* thumb top 168 -> scroll 160 */
    CHECK(NEAR(FLOAT_AT(B+0x34),160.f/3.f));
    FLOAT_AT(B+0x34)=0.f;
    mapmaker_pointer_scroll_drag(SM,222.f,0);                /* released */
    mapmaker_pointer_scroll_drag(SM,222.f,1);
    CHECK(FLOAT_AT(B+0x34)==0.f && mapmaker_pointer.grab_menu==0);
    WORD_AT(SM+0x194)=0;
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
    /* Right-drag on the editor screen grabs the map. */
    move(300.f,200.f);cursor.right=1;game_frame();
    move(320.f,190.f);game_frame();                          /* +20 px right, -10 px up */
    CHECK(NEAR(FLOAT_AT(0x00783DD8u),9.f) && NEAR(FLOAT_AT(0x00783CC0u),10.5f));
    cursor.right=0;game_frame();
    CHECK(NEAR(FLOAT_AT(0x00783DD8u),9.f));                  /* released, mid-screen: no pan */
    /* Mouse at the left edge pans left; in a menu it never pans. */
    move(2.f,200.f);game_frame();
    CHECK(NEAR(FLOAT_AT(0x00783DD8u),9.f-.75f*.15f));
    WORD_AT(MENU+0x178)=0x00301000u;FLOAT_AT(0x00783DD8u)=9.f;
    game_frame();
    CHECK(FLOAT_AT(0x00783DD8u)==9.f);
    WORD_AT(MENU+0x178)=0;
    move(320.f,240.f);game_frame();

    /* Editor menu rows: a real A press instead of the menu confirm, scene 102 only. */
    set_enabled(NULL);
    CHECK(!mapmaker_pointer_row_click() && row_presses==0);
    set_enabled("1");
    WORD_AT(0x0079094Cu)=101;
    CHECK(!mapmaker_pointer_row_click() && row_presses==0);
    WORD_AT(0x0079094Cu)=102;
    CHECK(mapmaker_pointer_row_click() && row_presses==1);

    /* Escape acts as the first Back key in the editor only. */
    CHECK(pc_mapmaker_key(0,0x1B,0x09,1,1)==1);  /* outside the editor: Start as bound */
    CHECK(pc_mapmaker_key(1,0x1B,0x09,1,1)==0);  /* editor: Escape no longer Start */
    CHECK(pc_mapmaker_key(1,0x09,0x09,0,1)==1);  /* ... but Back */
    CHECK(pc_mapmaker_key(1,0x09,0x09,1,0)==1);  /* Tab still Back */
    CHECK(pc_mapmaker_key(1,0x09,0x09,0,0)==0);
    CHECK(pc_mapmaker_key(1,'W',0x09,1,1)==1);   /* other keys untouched */
    CHECK(pc_mapmaker_key(1,0x1B,0,1,1)==1);     /* no Back key: leave Escape alone */
    CHECK(pc_mapmaker_key(1,0x1B,0x1B,1,1)==1);  /* Escape is Back already */

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
