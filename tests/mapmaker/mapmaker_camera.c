/* Asset-free contract for src/interface/pc_mapmaker_camera.inc: in MapMaker
 * play (scene 100) a camera/mounted-gun actor held by a player (reciprocal
 * +0xD8 / +0xC14 links, player mode 0x80) runs its update with the stock
 * "skip the state machine" flag (data +8 bit 0) set, so the sweep stops while
 * the player aims; the flag returns to its previous value afterwards and the
 * sweep resumes when control ends. Other scenes, broken links and a flag the
 * stock switch path already set are untouched. The view diagnostic reads the
 * visible rooms' floors. The Python side checks the stderr text. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00800000/4], g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#include "pc_mapmaker_camera.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stdout,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

#define SCENE 0x0079094Cu
#define GUN 0x00100000u        /* camera entity */
#define DATA 0x00101000u       /* its data (+0x7C) */
#define CHAR 0x00102000u       /* the character */
#define EXTRA 0x00103000u      /* character extra (+0x7C) */
#define PLAYER 0x00104000u     /* player record (character +0x150) */
#define STACK 0x006F0000u

/* sub_00246000 reduced to the part that matters: unless data +8 bit 0 is
 * set, state 2 sweeps the yaw by +0x5C per tick and bounces at the limits. */
static int calls;
static uint32_t seen_flags;
static void gun_update(void)
{
    uint32_t data=WORD_AT(WORD_AT(g_esp+4u)+0x7Cu);
    calls++;seen_flags=WORD_AT(data+8u);
    if(!(WORD_AT(data+8u)&1u) && WORD_AT(data+0x10u)==2u) {
        float yaw=FLOAT_AT(data+0x24u)+FLOAT_AT(data+0x5Cu);
        if(yaw>FLOAT_AT(data+0x40u) || yaw<FLOAT_AT(data+0x3Cu)) FLOAT_AT(data+0x5Cu)=-FLOAT_AT(data+0x5Cu);
        FLOAT_AT(data+0x24u)=yaw;
    }
}
static void run(void)
{
    g_esp=STACK;WORD_AT(STACK)=0x0024ABCDu;WORD_AT(STACK+4u)=GUN;calls=0;
    pc_mapmaker_camera_gun_update(gun_update);
    CHECK(calls==1);CHECK(g_esp==STACK);
}
static void setup(void)
{
    memset(memory,0,sizeof memory);
    WORD_AT(SCENE)=100u;WORD_AT(0x007B975Cu)=4u;
    WORD_AT(GUN+0x7Cu)=DATA;WORD_AT(DATA)=GUN;WORD_AT(DATA+0xCu)=7u;
    WORD_AT(DATA+8u)=0x00400004u;WORD_AT(DATA+0x10u)=2u;
    FLOAT_AT(DATA+0x24u)=0;FLOAT_AT(DATA+0x3Cu)=-45;FLOAT_AT(DATA+0x40u)=45;FLOAT_AT(DATA+0x5Cu)=1.5f;
    FLOAT_AT(DATA+0x34u)=-10;FLOAT_AT(DATA+0x38u)=10;
    WORD_AT(CHAR+0x7Cu)=EXTRA;WORD_AT(CHAR+0x150u)=PLAYER;WORD_AT(PLAYER+0x14u)=CHAR;
}
static void take(void) { WORD_AT(DATA+0xD8u)=CHAR;WORD_AT(EXTRA+0xC14u)=GUN;WORD_AT(PLAYER+0x114u)=0x80u; }
static void release(void) { WORD_AT(DATA+0xD8u)=0;WORD_AT(EXTRA+0xC14u)=0;WORD_AT(PLAYER+0x114u)=2u; }

int main(void)
{
    int i;
    setvbuf(stdout,NULL,_IONBF,0);
    g_xbox_mem_offset=(uintptr_t)memory;
    setup();
    /* nobody holds it: it sweeps */
    run();CHECK(!(seen_flags&1u));CHECK(FLOAT_AT(DATA+0x24u)==1.5f);
    /* held: the update runs with bit 0 set, which is cleared again; no sweep */
    take();
    for(i=0;i<5;i++) { run();CHECK(seen_flags&1u);CHECK(!(WORD_AT(DATA+8u)&1u)); }
    CHECK(FLOAT_AT(DATA+0x24u)==1.5f);
    CHECK(WORD_AT(DATA+8u)==0x00400004u);
    /* the player aims (mouse writes +0x24), the camera stays there */
    FLOAT_AT(DATA+0x24u)=30;run();CHECK(FLOAT_AT(DATA+0x24u)==30);
    /* released: the sweep resumes from where the player left it */
    release();run();CHECK(!(seen_flags&1u));CHECK(FLOAT_AT(DATA+0x24u)==31.5f);
    puts("phase sweep");
    /* the stock switch path already set bit 0 (flag 0x20 maps): left set */
    take();WORD_AT(DATA+8u)|=1u;run();CHECK(seen_flags&1u);CHECK(WORD_AT(DATA+8u)&1u);
    WORD_AT(DATA+8u)&=~1u;
    puts("phase stock");
    /* broken or foreign links: stock (sweeps) */
    WORD_AT(EXTRA+0xC14u)=GUN+0x10u;run();CHECK(!(seen_flags&1u));WORD_AT(EXTRA+0xC14u)=GUN;
    WORD_AT(PLAYER+0x114u)=2u;run();CHECK(!(seen_flags&1u));WORD_AT(PLAYER+0x114u)=0x80u;
    WORD_AT(PLAYER+0x14u)=CHAR+4u;run();CHECK(!(seen_flags&1u));WORD_AT(PLAYER+0x14u)=CHAR;
    WORD_AT(DATA)=GUN+4u;run();CHECK(!(seen_flags&1u));WORD_AT(DATA)=GUN;
    WORD_AT(DATA+0xD8u)=0x05000000u;run();CHECK(!(seen_flags&1u));WORD_AT(DATA+0xD8u)=CHAR;
    /* other scenes (story CCTV and turret scripts): stock */
    WORD_AT(SCENE)=1u;run();CHECK(!(seen_flags&1u));WORD_AT(SCENE)=100u;
    run();CHECK(seen_flags&1u);
    puts("phase links");

    /* view diagnostic: rooms 1..4 on floors -2, 0, 9, 13; 1, 2, 3 visible */
    {
        uint32_t view=0x00200000u,records=0x00300000u,r;
        static const float y[5]={0,-10,0,45,65};
        WORD_AT(0x007B0CC0u)=view;WORD_AT(0x00708788u)=records;WORD_AT(0x007B0C7Cu)=PLAYER;
        FLOAT_AT(0x004DCB0Cu)=1800;FLOAT_AT(view+0x384u)=55;
        WORD_AT(view+0x214u)=0;WORD_AT(view+0x2A0u)=1;WORD_AT(view+0xD28u)=2;
        for(r=1;r<=4;r++) {
            uint32_t e=view+0x6000u+r*12u;
            FLOAT_AT(records+r*0xB4u+0x14u)=y[r];FLOAT_AT(records+r*0xB4u+0x20u)=y[r]+5;
            WORD_AT(e)=r<4?r-1:(uint32_t)-1;
            *(int16_t *)((uintptr_t)(e+4u)+g_xbox_mem_offset)=0;
            *(int16_t *)((uintptr_t)(e+8u)+g_xbox_mem_offset)=r<4?640:0;
        }
        mapmaker_camera_view_diag();
        mapmaker_camera_view_diag();          /* unchanged: no second line */
        WORD_AT(view+0x6000u+4u*12u)=3;*(int16_t *)((uintptr_t)(view+0x6000u+4u*12u+8u)+g_xbox_mem_offset)=640;
        mapmaker_camera_view_diag();
        WORD_AT(SCENE)=1u;WORD_AT(view+0x6000u+3u*12u)=(uint32_t)-1;
        mapmaker_camera_view_diag();          /* other scene: silent */
    }
    puts("phase view");
    if(failures) { printf("%d failures\n",failures);return 1; }
    puts("mapmaker camera ok");
    return 0;
}
