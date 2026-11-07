/* Asset-free contract for src/interface/pc_mapmaker_skymask.inc: the dev switch
 * TSFP_MAPMAKER_ROOM_MASK_CLEAR clears node-mask bits in the relocated
 * per-room table after the stock store (sub_00085710), only in scene 100,
 * never in the build record, within the 1024-entry table, and logs one summary
 * per map build. The Python side checks the stderr text. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x900000/4],g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define TSFP_MAPMAKER_SKYMASK_TEST
#include "pc_mapmaker_skymask.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stdout,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

#define SCENE 0x0079094Cu
#define TABLE MAPMAKER_SKYMASK_TABLE
#define STOCK_TABLE 0x00703460u
#define BUILDS 0x00400000u      /* 0x68-byte build records */
#define STACK 0x006F0000u

static void set_switch(const char *value)
{
#ifdef _WIN32
    _putenv_s("TSFP_MAPMAKER_ROOM_MASK_CLEAR",value?value:"");
#else
    if(value)setenv("TSFP_MAPMAKER_ROOM_MASK_CLEAR",value,1); else unsetenv("TSFP_MAPMAKER_ROOM_MASK_CLEAR");
#endif
    mapmaker_skymask_reset();
}

/* Stock sub_00085710, reduced to the store at 0x000857A3: for each of the
 * piece's build+0x14 rooms, table[build+0x28 + j] = build+0x44; plain ret. */
static unsigned originals;
static void original(void)
{
    uint32_t build=WORD_AT(g_esp+4u),j;
    originals++;
    for(j=0;j<WORD_AT(build+0x14u);j++) {
        uint32_t room=WORD_AT(build+0x28u)+j;
        if(room<MAPMAKER_SKYMASK_ROOMS) WORD_AT(TABLE+room*4u)=WORD_AT(build+0x44u);
    }
    g_esp+=4u;
}
static uint32_t piece(unsigned index,uint32_t first,uint32_t rooms,uint32_t mask)
{
    uint32_t b=BUILDS+index*0x68u;
    WORD_AT(b+0x14u)=rooms;WORD_AT(b+0x28u)=first;WORD_AT(b+0x44u)=mask;
    return b;
}
static void call(uint32_t build)
{
    uint32_t sp=g_esp;unsigned before=originals;
    g_esp-=4u;WORD_AT(g_esp)=build;
    g_esp-=4u;WORD_AT(g_esp)=0x0008AC76u;
    pc_mapmaker_sky_room_mask(original);
    CHECK(originals==before+1u);   /* the original runs exactly once */
    CHECK(g_esp==sp-4u);           /* it popped its return address; the caller pops the argument */
    g_esp=sp;
}
static uint64_t checksum_outside_table(void)
{
    uint64_t h=1469598103934665603ull;size_t i;
    for(i=0;i<sizeof memory/4;i++) {
        if(i>=TABLE/4u && i<TABLE/4u+MAPMAKER_SKYMASK_ROOMS)continue;
        if(i>=(STACK-0x1000u)/4u && i<STACK/4u)continue;
        h^=memory[i];h*=1099511628211ull;
    }
    return h;
}
static void clear_table(void) { memset((char *)memory+TABLE,0,MAPMAKER_SKYMASK_ROOMS*4u); }

int main(void)
{
    uint64_t before;uint32_t b0,b1,b2;
    g_xbox_mem_offset=(uintptr_t)memory;g_esp=STACK;
    WORD_AT(STOCK_TABLE)=0x5A5A5A5Au;

    /* Off by default: stock values. */
    set_switch(NULL);WORD_AT(SCENE)=100u;
    b0=piece(0,0,2,0x020001C7u);b1=piece(1,2,1,0x01FFFBFFu);
    call(b0);call(b1);pc_mapmaker_sky_room_mask_flush();
    CHECK(WORD_AT(TABLE)==0x020001C7u && WORD_AT(TABLE+4u)==0x020001C7u && WORD_AT(TABLE+8u)==0x01FFFBFFu);
    puts("phase off");fflush(stdout);fflush(stderr);

    /* Bit 25 in play: rooms 1 and 2 change, room 3 already open. */
    set_switch("2000000");clear_table();
    before=checksum_outside_table();
    call(b0);call(b1);
    CHECK(WORD_AT(TABLE)==0x000001C7u && WORD_AT(TABLE+4u)==0x000001C7u && WORD_AT(TABLE+8u)==0x01FFFBFFu);
    CHECK(WORD_AT(b0+0x44u)==0x020001C7u);       /* the build record (saved map data) is untouched */
    CHECK(WORD_AT(STOCK_TABLE)==0x5A5A5A5Au);    /* the stock table is not the live one */
    CHECK(checksum_outside_table()==before);
    fflush(stderr);puts("phase built");fflush(stdout);
    pc_mapmaker_sky_room_mask_flush();
    pc_mapmaker_sky_room_mask_flush();             /* idle: nothing */
    fflush(stderr);puts("phase flushed");fflush(stdout);

    /* A second map build before a frame flush: the repeated room starts a new summary. */
    clear_table();
    call(b0);
    clear_table();
    b2=piece(2,0,1,0x0A000000u);
    call(b2);
    CHECK(WORD_AT(TABLE)==0x08000000u);
    pc_mapmaker_sky_room_mask_flush();
    fflush(stderr);puts("phase rebuild");fflush(stdout);

    /* Several bits, 0x prefix, rooms past the table. */
    set_switch("0x09000000");clear_table();
    b0=piece(0,1022,4,0xFFFFFFFFu);
    call(b0);
    CHECK(WORD_AT(TABLE+1022u*4u)==0xF6FFFFFFu && WORD_AT(TABLE+1023u*4u)==0xF6FFFFFFu);
    CHECK(WORD_AT(TABLE+1024u*4u)==0u);          /* the next relocated table is not written */
    pc_mapmaker_sky_room_mask_flush();
    fflush(stderr);puts("phase bound");fflush(stdout);

    /* Editor (or any other scene): stock values, one note. */
    set_switch("2000000");clear_table();WORD_AT(SCENE)=102u;
    b0=piece(0,0,2,0x020001C7u);
    call(b0);call(b0);pc_mapmaker_sky_room_mask_flush();
    CHECK(WORD_AT(TABLE)==0x020001C7u && WORD_AT(TABLE+4u)==0x020001C7u);
    WORD_AT(SCENE)=1u;call(b0);                  /* a story scene */
    CHECK(WORD_AT(TABLE)==0x020001C7u);
    fflush(stderr);puts("phase editor");fflush(stdout);

    /* A bad value is refused, not guessed. */
    set_switch("zz");clear_table();WORD_AT(SCENE)=100u;
    call(b0);pc_mapmaker_sky_room_mask_flush();
    CHECK(WORD_AT(TABLE)==0x020001C7u);
    set_switch("0");clear_table();
    call(b0);pc_mapmaker_sky_room_mask_flush();
    CHECK(WORD_AT(TABLE)==0x020001C7u);
    /* A bad build pointer or no rooms: no write. */
    set_switch("2000000");clear_table();
    WORD_AT(BUILDS+0x14u)=0u;call(BUILDS);pc_mapmaker_sky_room_mask_flush();
    fflush(stderr);puts("phase bad");fflush(stdout);

    if(failures)return 1;
    puts("mapmaker skymask ok");
    return 0;
}
