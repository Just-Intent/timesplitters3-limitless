/* Asset-free contract for src/interface/pc_mapmaker_openview.inc: after the
 * stock portal walk (sub_00089B90), in MapMaker play (scene 100,
 * TSFP_MAPMAKER_LIMITS=1) on a map with open-air rooms (marked by
 * pc_mapmaker_roof.inc's rooms hook for bounded and unbounded pieces), every
 * room of an open area joined by portals, and every room one portal away
 * from it, gets the union of the area's visible rectangles (the whole
 * viewport from inside the area) and a depth when the walk did not reach it;
 * rooms elsewhere, other scenes, the switch off and maps without open air
 * keep the stock result. The original runs exactly once and pops only its
 * return address. The Python side checks the stderr text. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00900000/4], g_eax, g_ecx, g_edx, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }
void sub_0003DFA0(void) { g_ecx=6; }
void sub_000BD090(void) { g_ecx=7; }
void sub_00087310(void) { g_ecx=8; }
static uint32_t guest_call(void (*f)(void),int n,...) { (void)f;(void)n;return 0; }
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) { (void)bytes;(void)alignment;return 0; }
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_roof.inc"
#include "pc_mapmaker_openview.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stdout,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

#define SCENE 0x0079094Cu
#define VIEW 0x00100000u
#define RECTS (VIEW+0x6000u)
#define RECORDS 0x00200000u
#define LISTS 0x00210000u
#define PORTALS 0x00220000u
#define PSTRUCT 0x00230000u
#define STACK 0x006F0000u
#define ROOMS 9u

/* Map: open area {1,2,3}: 1-2, 2-3; closed 4 next to 3 and 2, closed 5 behind 4;
 * open island 6 with closed 7 beside it; closed 8 alone; closed 9 next to 1
 * through a door (portal 7, also listed by 9). Portal 0 is never used. */
static const uint32_t portal_rooms[][2]={{0,0},{1,2},{2,3},{3,4},{4,5},{6,7},{8,8},{1,9},{3,77},{2,4}};
#define NPORTALS 9u     /* ids 0..9 (8: a portal to an out-of-range room) */
static uint32_t lists[ROOMS+1][5]={
    {0},{2,1,7},{3,1,2,9},{3,2,3,8},{3,3,4,9},{1,4},{1,5},{1,5},{0},{1,7}
};

static int16_t *h(uint32_t a) { return (int16_t *)((uintptr_t)a+g_xbox_mem_offset); }
static uint32_t entry(uint32_t r) { return RECTS+r*12u; }
static void set_room(uint32_t r,int32_t depth,int x0,int y0,int x1,int y1)
{
    WORD_AT(entry(r))=(uint32_t)depth;
    *h(entry(r)+4u)=(int16_t)x0;*h(entry(r)+6u)=(int16_t)y0;*h(entry(r)+8u)=(int16_t)x1;*h(entry(r)+10u)=(int16_t)y1;
}
static int room_is(uint32_t r,int32_t depth,int x0,int y0,int x1,int y1)
{
    return (int32_t)WORD_AT(entry(r))==depth && *h(entry(r)+4u)==x0 && *h(entry(r)+6u)==y0 && *h(entry(r)+8u)==x1 && *h(entry(r)+10u)==y1;
}
static int unseen(uint32_t r) { return room_is(r,-1,640,480,-1,-1); }

/* Stock sub_00089B90 reduced to its result: reset (depth -1, x0/y0 the
 * screen size, x1/y1 -1), then the walk's rectangles from walk[]. */
static unsigned originals;
static int32_t walk[ROOMS+1][5];
static void original(void)
{
    uint32_t r;
    originals++;
    for(r=1;r<=ROOMS;r++) {
        set_room(r,-1,640,480,-1,-1);
        if(walk[r][0]>=0) set_room(r,walk[r][0],walk[r][1],walk[r][2],walk[r][3],walk[r][4]);
    }
    WORD_AT(VIEW+0xD28u)=0;
    for(r=1;r<=ROOMS;r++) if((int32_t)WORD_AT(entry(r))>(int32_t)WORD_AT(VIEW+0xD28u)) WORD_AT(VIEW+0xD28u)=WORD_AT(entry(r));
    g_esp+=4u;
}
static void no_walk(void) { uint32_t r;for(r=0;r<=ROOMS;r++) walk[r][0]=-1; }
static void sees(uint32_t r,int32_t d,int x0,int y0,int x1,int y1) { walk[r][0]=d;walk[r][1]=x0;walk[r][2]=y0;walk[r][3]=x1;walk[r][4]=y1; }
static void call(void)
{
    uint32_t sp=g_esp;unsigned before=originals;
    g_esp-=4u;WORD_AT(g_esp)=0x001CE68Du;
    pc_mapmaker_openview_walk(original);
    CHECK(originals==before+1u);     /* the original runs exactly once */
    CHECK(g_esp==sp);                /* it popped its return address, nothing else */
    g_esp=sp;
}
static void set_limits(const char *value)
{
#ifdef _WIN32
    _putenv_s("TSFP_MAPMAKER_LIMITS",value?value:"");
#else
    if(value)setenv("TSFP_MAPMAKER_LIMITS",value,1); else unsetenv("TSFP_MAPMAKER_LIMITS");
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}
static uint64_t checksum_outside(void)
{
    uint64_t hsh=1469598103934665603ull;size_t i;
    for(i=0;i<sizeof memory/4;i++) {
        if(i>=RECTS/4u && i<(RECTS+(ROOMS+1u)*12u)/4u) continue;
        if(i==(VIEW+0xD28u)/4u) continue;
        if(i>=(STACK-0x100u)/4u && i<STACK/4u) continue;
        hsh^=memory[i];hsh*=1099511628211ull;
    }
    return hsh;
}
/* The roof's rooms hook during a play build: piece k has rooms base..base+count-1. */
static void rooms_original(void) { g_esp+=4u; }
static void build_piece(unsigned k,uint32_t base,uint32_t count)
{
    uint32_t b=0x00300000u+k*0x68u;
    memset((char *)memory+b,0,0x68);
    WORD_AT(b+0x14u)=count;WORD_AT(b+0x28u)=base;WORD_AT(b+0x44u)=mapmaker_roof_play.mask[k];
    g_esp=STACK-8u;WORD_AT(g_esp)=0x0008AC76u;WORD_AT(g_esp+4u)=b;
    pc_mapmaker_roof_rooms(rooms_original);
    g_esp=STACK;
}
/* A play build: the roof reads the piece records (word +0x26 mark, +8 mask),
 * then its rooms hook sees the build entries: rooms 1,2 (piece 0, 0x5202
 * unbounded), 3 (piece 1, 0x5203 bounded), 4,5 (piece 2, closed), 6 (piece
 * 3, 0x5202), 7,8,9 (piece 4, 0x5201 no roof). */
static void play_build(void)
{
    static const uint32_t marks[5]={0x5202u,0x5203u,0u,0x5202u,0x5201u};
    unsigned k;
    memset(mapmaker_roof_play.open,0,sizeof mapmaker_roof_play.open);
    memset(mapmaker_roof_play.hide,0,sizeof mapmaker_roof_play.hide);
    mapmaker_roof_play.open_rooms=mapmaker_roof_play.rooms=0;mapmaker_roof_play.generation++;
    mapmaker_roof_play.pieces=mapmaker_roof_play.next=0;
    for(k=0;k<5u;k++) {
        uint32_t rec=0x00310000u+k*0x28u;
        memset((char *)memory+rec,0,0x28);
        WORD_AT(rec+0x24u)=marks[k]<<16;WORD_AT(rec+8u)=0x01000000u;
        CHECK(mapmaker_roof_play_record(rec,NULL)==1);
    }
    mapmaker_roof_play.building=1;
    build_piece(0,0,2);
    build_piece(1,2,1);
    build_piece(2,3,2);
    build_piece(3,5,1);
    build_piece(4,6,3);
    mapmaker_roof_play.building=0;
}

int main(void)
{
    uint32_t r,i;uint64_t before;
    g_xbox_mem_offset=(uintptr_t)memory;g_esp=STACK;
    WORD_AT(0x007B0CC0u)=VIEW;WORD_AT(VIEW+0x2A0u)=1u;
    WORD_AT(0x007B975Cu)=ROOMS;WORD_AT(0x007B9754u)=NPORTALS;
    WORD_AT(0x00708788u)=RECORDS;WORD_AT(0x0070878Cu)=PORTALS;
    for(r=1;r<=ROOMS;r++) {
        WORD_AT(RECORDS+r*0xB4u+4u)=lists[r][0]?LISTS+r*0x20u:0u;
        for(i=0;i<5u;i++) WORD_AT(LISTS+r*0x20u+i*4u)=lists[r][i];
    }
    for(i=0;i<=NPORTALS;i++) {
        WORD_AT(PORTALS+i*4u)=PSTRUCT+i*0x40u;
        WORD_AT(PSTRUCT+i*0x40u)=portal_rooms[i][0];WORD_AT(PSTRUCT+i*0x40u+4u)=portal_rooms[i][1];
    }

    /* The roof marks the rooms of bounded and unbounded pieces open, and
     * hides the side nodes of the bounded ones only. */
    WORD_AT(SCENE)=100u;set_limits("1");
    play_build();
    CHECK(mapmaker_roof_play.open_rooms==4u && mapmaker_roof_play.rooms==1u);
    CHECK(mapmaker_roof_play.open[0] && mapmaker_roof_play.open[1] && mapmaker_roof_play.open[2] && mapmaker_roof_play.open[5]);
    CHECK(!mapmaker_roof_play.open[3] && !mapmaker_roof_play.open[4] && !mapmaker_roof_play.open[6] && !mapmaker_roof_play.open[8]);
    CHECK(!mapmaker_roof_play.hide[0] && mapmaker_roof_play.hide[2]==0x00FFFFFFu && !mapmaker_roof_play.hide[5]);
    puts("phase roof");fflush(stdout);fflush(stderr);

    /* Switch off, other scenes: the stock walk only. */
    no_walk();sees(1,0,0,0,640,480);sees(2,1,300,200,340,230);
    set_limits(NULL);call();
    CHECK(room_is(2,1,300,200,340,230) && unseen(3) && unseen(4));
    set_limits("1");
    WORD_AT(SCENE)=102u;call();CHECK(unseen(3));
    WORD_AT(SCENE)=1u;call();CHECK(unseen(3));
    WORD_AT(SCENE)=100u;
    puts("phase stock");fflush(stdout);fflush(stderr);

    /* Camera in open room 1: the area {1,2,3} and the closed 4 and 9 next to
     * it get the whole viewport; 3, 4 and 9 (not reached) depth 1; 5 (behind
     * the closed 4), the island 6, 7, 8 stay stock. */
    before=checksum_outside();
    call();
    CHECK(room_is(1,0,0,0,640,480));
    CHECK(room_is(2,1,0,0,640,480));
    CHECK(room_is(3,1,0,0,640,480) && room_is(4,1,0,0,640,480) && room_is(9,1,0,0,640,480));
    CHECK(unseen(5) && unseen(6) && unseen(7) && unseen(8));
    CHECK(WORD_AT(VIEW+0xD28u)==1u);
    CHECK(checksum_outside()==before);
    call();CHECK(room_is(3,1,0,0,640,480));          /* same every frame */
    puts("phase inside");fflush(stdout);fflush(stderr);

    /* Camera in the closed 5, looking through 4's doorway at 3: the area gets
     * the rectangle it is seen through, at depth 3 (3 was at 2). */
    no_walk();sees(5,0,0,0,640,480);sees(4,1,200,100,400,300);sees(3,2,250,150,350,250);
    WORD_AT(VIEW+0x2A0u)=5u;
    call();
    CHECK(room_is(5,0,0,0,640,480));
    CHECK(room_is(4,1,200,100,400,300));             /* already wider */
    CHECK(room_is(3,2,250,150,350,250));
    CHECK(room_is(1,3,250,150,350,250) && room_is(2,3,250,150,350,250) && room_is(9,3,250,150,350,250));
    CHECK(unseen(6) && unseen(7) && WORD_AT(VIEW+0xD28u)==3u);
    /* two visible rooms: their union, the smallest depth */
    sees(1,4,300,120,380,140);
    call();
    CHECK(room_is(2,3,250,120,380,250) && room_is(1,4,250,120,380,250) && room_is(3,2,250,120,380,250));
    CHECK(room_is(4,1,200,100,400,300));
    /* a visible depth with an empty rectangle does not count */
    no_walk();sees(5,0,0,0,640,480);sees(3,2,300,150,300,250);
    call();
    CHECK(room_is(3,2,300,150,300,250) && unseen(1) && unseen(2) && unseen(4));
    /* deep walks: the new depth is capped */
    no_walk();sees(3,0x46,10,10,20,20);
    call();
    CHECK(room_is(1,0x40,10,10,20,20) && room_is(3,0x46,10,10,20,20) && WORD_AT(VIEW+0xD28u)==0x46u);
    puts("phase outside");fflush(stdout);fflush(stderr);

    /* The island: its closed neighbour only. */
    no_walk();sees(6,0,0,0,640,480);WORD_AT(VIEW+0x2A0u)=6u;
    call();
    CHECK(room_is(7,1,0,0,640,480) && unseen(1) && unseen(3) && unseen(8) && unseen(9));
    puts("phase island");fflush(stdout);fflush(stderr);

    /* A new build without open rooms: stock; groups are rebuilt per build. */
    memset(mapmaker_roof_play.open,0,sizeof mapmaker_roof_play.open);
    mapmaker_roof_play.open_rooms=0;mapmaker_roof_play.generation++;
    no_walk();sees(1,0,0,0,640,480);WORD_AT(VIEW+0x2A0u)=1u;
    call();CHECK(unseen(2) && unseen(3));
    play_build();
    mapmaker_roof_play.open[1]=0;mapmaker_roof_play.open_rooms=3;  /* room 2 closed now: 1 and 3 apart */
    call();
    CHECK(room_is(2,1,0,0,640,480) && room_is(9,1,0,0,640,480) && unseen(3) && unseen(4));
    puts("phase rebuild");fflush(stdout);fflush(stderr);

    /* Bad tables: nothing written, the original still runs. */
    play_build();
    WORD_AT(0x007B975Cu)=0x100u;call();CHECK(unseen(3));     /* more rooms than ids */
    WORD_AT(0x007B975Cu)=ROOMS;
    WORD_AT(0x007B0CC0u)=0x00000100u;call();                  /* view below guest RAM */
    WORD_AT(0x007B0CC0u)=VIEW;
    WORD_AT(0x00708788u)=0x00000100u;mapmaker_roof_play.generation++;
    call();CHECK(unseen(3) && unseen(2));                     /* no room records: no area */
    WORD_AT(0x00708788u)=RECORDS;mapmaker_roof_play.generation++;
    WORD_AT(LISTS+2u*0x20u)=0x1000u;                          /* room 2: absurd portal count */
    call();CHECK(unseen(3) && room_is(2,1,0,0,640,480));
    WORD_AT(LISTS+2u*0x20u)=3u;mapmaker_roof_play.generation++;
    WORD_AT(PORTALS+2u*4u)=0x00000100u;                       /* portal 2 (2-3) below guest RAM */
    WORD_AT(0x100u)=2u;WORD_AT(0x104u)=3u;
    call();CHECK(unseen(3) && room_is(2,1,0,0,640,480));
    WORD_AT(PORTALS+2u*4u)=PSTRUCT+2u*0x40u;mapmaker_roof_play.generation++;
    call();CHECK(room_is(3,1,0,0,640,480));
    puts("phase bad");fflush(stdout);fflush(stderr);

    if(failures) return 1;
    puts("mapmaker openview ok");
    return 0;
}
