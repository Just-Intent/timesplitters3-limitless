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
    CHECK(checksum()==before && owner_calls==0);

    set_enabled("1");
    /* Other scenes (main menu 101, map play 100) never own the pointer. */
    WORD_AT(0x0079094Cu)=101;before=checksum();
    game_frame();move(120.f,60.f);game_frame();
    CHECK(checksum()==before && owner_last==0);
    WORD_AT(0x0079094Cu)=100;before=checksum();
    game_frame();move(121.f,60.f);game_frame();
    CHECK(checksum()==before && owner_last==0);
    WORD_AT(0x0079094Cu)=102;
    /* A menu with rows (category list) or an open child (pause) does not. */
    WORD_AT(MENU+0x178)=0x00301000u;before=checksum();
    game_frame();move(130.f,70.f);game_frame();
    CHECK(checksum()==before && owner_last==0);
    WORD_AT(MENU+0x178)=0;WORD_AT(MENU+0x170)=0x006FEE24u;before=checksum();
    game_frame();move(131.f,70.f);game_frame();
    CHECK(checksum()==before && owner_last==0);
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

    /* Editor menu rows: a real A press instead of the menu confirm, scene 102 only. */
    set_enabled(NULL);
    CHECK(!mapmaker_pointer_row_click() && row_presses==0);
    set_enabled("1");
    WORD_AT(0x0079094Cu)=101;
    CHECK(!mapmaker_pointer_row_click() && row_presses==0);
    WORD_AT(0x0079094Cu)=102;
    CHECK(mapmaker_pointer_row_click() && row_presses==1);

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
