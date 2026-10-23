/* Asset-free contract for src/pc_mapmaker_rooms.inc (MapMaker rooms 200 ->
 * 241): the bounded room query keeps the original's result and flag handling
 * past the old ~209-room stack frame; the compile guard drops only pieces that
 * would pass the room or portal limit, with the limit tied to the room-model
 * slots; prop slots for room models 0x9D0..0x9F8 are synthesized in scene 100
 * only, from room 200's slot, without disturbing existing slots. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00900000/4], g_eax, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
static uint16_t u16(uint32_t a) { return *(uint16_t *)((uintptr_t)a+g_xbox_mem_offset); }
static void set16(uint32_t a,uint16_t v) { *(uint16_t *)((uintptr_t)a+g_xbox_mem_offset)=v; }
static uint8_t *u8(uint32_t a) { return (uint8_t *)((uintptr_t)a+g_xbox_mem_offset); }

void sub_00157440(void) {}
void sub_0003DFA0(void) {}
/* Fake tree query: offers fake_hits[] (with repeats) to a model of the
 * callback sub_00087260, which lists a room once (record flag 0x10) as
 * room*2 | in-box bit. */
static uint32_t fake_hits[600],fake_inside[600],fake_hit_count,query_calls,query_args[5];
static uint32_t heap_next,allocs,alloc_size;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t r=0;
    int i;
    va_start(a,n);
    if(f==sub_00157440 && n==5) {
        uint32_t list,records,count;
        for(i=0;i<5;i++) query_args[i]=va_arg(a,uint32_t);
        query_calls++;
        list=query_args[4];records=WORD_AT(0x00708788u);count=u16(list);
        for(i=0;i<(int)fake_hit_count;i++) {
            uint8_t *flag=u8(records+fake_hits[i]*0xB4u+0xB2u);
            if(*flag&0x10u) continue;
            *flag|=0x10u;
            set16(list+2u+2u*count,(uint16_t)(fake_hits[i]*2u|(fake_inside[i]&1u)));
            count++;
        }
        set16(list,(uint16_t)count);
    } else if(f==sub_0003DFA0 && n==2) {
        alloc_size=va_arg(a,uint32_t);(void)va_arg(a,uint32_t);
        r=heap_next;heap_next+=(alloc_size+15u)&~15u;allocs++;
        memset((char *)memory+r,0xCD,alloc_size);
    }
    va_end(a);
    return r;
}
#include "pc_mapmaker_rooms.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

static int originals;
static void fake_original(void) { originals++; g_esp+=4u; } /* pops like a real ret */

/* sub_00087310 is cdecl: [esp] return, [esp+4] box, [esp+8] out, [esp+12] max. */
#define STACK 0x00100000u
#define RECORDS 0x00200000u
#define OUT 0x00180000u
static uint32_t query(uint32_t box,uint32_t max)
{
    g_esp=STACK;WORD_AT(STACK)=0x12345678u;WORD_AT(STACK+4)=box;WORD_AT(STACK+8)=OUT;WORD_AT(STACK+12)=max;
    memset((char *)memory+OUT,0xEE,0x1000);
    g_eax=0xDEADu;
    pc_mapmaker_rooms_query(fake_original);
    if(g_esp!=STACK+4u) return 0xBADu;            /* the replaced original's ret */
    return g_eax;
}

/* Compile: the fake compiler builds n entries of 0x68 B and calls the piece wrapper for each. */
#define ENTRIES 0x00300000u
static uint32_t piece_rooms[400],piece_portals[400],piece_count;
static void fake_piece_builder(void)
{
    uint32_t entry=WORD_AT(g_esp+4u);
    WORD_AT(entry+0x14u)=piece_rooms[(entry-ENTRIES)/0x68u];
    WORD_AT(entry+0x18u)=piece_portals[(entry-ENTRIES)/0x68u];
    WORD_AT(entry+0x24u)=3;                                 /* 0x2C records */
}
static void fake_compile(void)
{
    uint32_t i,rooms=0,portals=0;
    originals++;
    for(i=0;i<piece_count;i++) {
        uint32_t entry=ENTRIES+i*0x68u;
        g_esp=STACK;WORD_AT(STACK+4)=entry;
        pc_mapmaker_rooms_piece(fake_piece_builder);
        rooms+=WORD_AT(entry+0x14u);portals+=WORD_AT(entry+0x18u);
    }
    WORD_AT(0x007B975Cu)=rooms;WORD_AT(0x007B9754u)=portals;
}
static void room_models(uint32_t last_model,uint32_t slots)
{
    uint32_t m;
    for(m=0;m<0xA00u;m++) set16(MAPMAKER_ROOMS_MODEL_TABLE+m*2u,0xFFFFu);
    for(m=0x908u;m<=last_model;m++) set16(MAPMAKER_ROOMS_MODEL_TABLE+m*2u,(uint16_t)(10u+m-0x908u));
    WORD_AT(0x0075EAA8u)=slots;
}

int main(void)
{
    uint32_t i,r,n,fresh,old;
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x00708788u)=RECORDS;

    /* ---- room query ---- */
    for(i=0;i<241;i++) { fake_hits[i]=i+1;fake_inside[i]=1; }
    for(i=0;i<100;i++) { fake_hits[241+i]=(i*7)%241+1;fake_inside[241+i]=1; } /* repeats */
    fake_hit_count=341;
    n=query(0x00170000u,0x100u);
    CHECK(n==241 && originals==0 && query_calls==1);
    CHECK(query_args[0]==0x00702E00u && query_args[1]==0x00170000u && query_args[2]==0x0017000Cu &&
          query_args[3]==0x00087260u && query_args[4]==MAPMAKER_ROOMS_QUERY_BUFFER);
    CHECK(WORD_AT(OUT)==1 && WORD_AT(OUT+240*4)==241 && WORD_AT(OUT+241*4)==0xEEEEEEEEu);
    for(r=1;r<=241;r++) CHECK(!(*u8(RECORDS+r*0xB4u+0xB2u)&0x10u));       /* flags cleared */
    CHECK(WORD_AT(STACK)==0x12345678u && g_esp==STACK+4u); /* the original's ret */
    CHECK(mapmaker_rooms.query_depth==0);
    /* the in-box bit decides, max stops listing but every flag is still cleared */
    for(i=0;i<241;i++) fake_inside[i]=i&1;
    fake_hit_count=241;
    n=query(0x00170000u,30u);
    CHECK(n==30 && WORD_AT(OUT)==2 && WORD_AT(OUT+29*4)==60 && WORD_AT(OUT+30*4)==0xEEEEEEEEu);
    for(r=1;r<=241;r++) CHECK(!(*u8(RECORDS+r*0xB4u+0xB2u)&0x10u));
    /* nested deeper than the static buffers: the original runs */
    mapmaker_rooms.query_depth=MAPMAKER_ROOMS_QUERY_LEVELS;
    query_calls=0;
    query(0x00170000u,30u);
    CHECK(originals==1 && query_calls==0);
    mapmaker_rooms.query_depth=1;originals=0;
    query(0x00170000u,30u);
    CHECK(originals==0 && query_args[4]==MAPMAKER_ROOMS_QUERY_BUFFER+MAPMAKER_ROOMS_QUERY_BYTES);
    CHECK(mapmaker_rooms.query_depth==1);
    mapmaker_rooms.query_depth=0;

    /* ---- compile guard ---- */
    room_models(0x907u+241u,300u);                       /* every room model has a slot */
    piece_count=242;
    for(i=0;i<piece_count;i++) { piece_rooms[i]=1;piece_portals[i]=4; }
    originals=0;
    pc_mapmaker_rooms_compile(fake_compile);
    CHECK(originals==1 && mapmaker_rooms.cap==241 && !mapmaker_rooms.compiling);
    CHECK(WORD_AT(0x007B975Cu)==241 && mapmaker_rooms.dropped==1 && mapmaker_rooms.pieces==242);
    CHECK(WORD_AT(ENTRIES+240*0x68u+0x14u)==1 && WORD_AT(ENTRIES+240*0x68u+0x24u)==3);
    CHECK(WORD_AT(ENTRIES+241*0x68u+0x14u)==0 && WORD_AT(ENTRIES+241*0x68u+0x18u)==0 &&
          WORD_AT(ENTRIES+241*0x68u+0x24u)==0);              /* the 242nd piece is dropped whole */
    /* a second compile starts again from zero */
    piece_count=241;
    pc_mapmaker_rooms_compile(fake_compile);
    CHECK(WORD_AT(0x007B975Cu)==241 && mapmaker_rooms.dropped==0);
    /* without slots for the extra room models the limit stays at 200 */
    room_models(0x907u+200u,300u);
    pc_mapmaker_rooms_compile(fake_compile);
    CHECK(mapmaker_rooms.cap==200 && WORD_AT(0x007B975Cu)==200 && mapmaker_rooms.dropped==41);
    /* a two-room piece that does not fit is dropped, later one-room pieces still fit */
    room_models(0x907u+241u,300u);
    piece_count=4;piece_rooms[0]=239;piece_rooms[1]=3;piece_rooms[2]=1;piece_rooms[3]=1;
    pc_mapmaker_rooms_compile(fake_compile);
    CHECK(WORD_AT(0x007B975Cu)==241 && mapmaker_rooms.dropped==1 && WORD_AT(ENTRIES+0x68u+0x14u)==0);
    /* portals: 0x3FFF flag bytes */
    piece_count=3;piece_rooms[0]=piece_rooms[1]=piece_rooms[2]=1;
    piece_portals[0]=0x2000;piece_portals[1]=0x2000;piece_portals[2]=0x1FFF;
    pc_mapmaker_rooms_compile(fake_compile);
    CHECK(WORD_AT(0x007B9754u)==0x3FFF && mapmaker_rooms.dropped==1 && WORD_AT(ENTRIES+0x68u+0x18u)==0);
    /* outside a compile the piece wrapper only runs the original */
    WORD_AT(STACK+4)=ENTRIES;g_esp=STACK;piece_rooms[0]=500;
    pc_mapmaker_rooms_piece(fake_piece_builder);
    CHECK(WORD_AT(ENTRIES+0x14u)==500);

    /* ---- room-model prop slots ---- */
    heap_next=0x00500000u;
    room_models(0x9CFu,300u);                            /* MapMaker table: rooms 1..200 */
    old=0x00400000u;WORD_AT(0x00791D4Cu)=old;
    for(i=0;i<300u*0x30u/4u;i++) WORD_AT(old+i*4u)=0xA0000000u+i;
    WORD_AT(0x0079094Cu)=100;allocs=0;originals=0;
    pc_mapmaker_rooms_props(fake_original);
    fresh=WORD_AT(0x00791D4Cu);
    CHECK(originals==1 && allocs==1 && alloc_size==((341u*0x30u+15u)&~15u) && fresh==0x00500000u);
    CHECK(WORD_AT(0x0075EAA8u)==341);
    CHECK(!memcmp((char *)memory+fresh,(char *)memory+old,300u*0x30u));   /* existing slots kept */
    for(r=201;r<=241;r++) CHECK(u16(MAPMAKER_ROOMS_MODEL_TABLE+(0x907u+r)*2u)==300u+r-201u);
    CHECK(u16(MAPMAKER_ROOMS_MODEL_TABLE+(0x907u+242u)*2u)==0xFFFFu);
    /* new slots copy room 200's slot (index 209), geometry fields cleared */
    CHECK(WORD_AT(fresh+300u*0x30u)==WORD_AT(old+209u*0x30u) &&
          WORD_AT(fresh+340u*0x30u+0x2Cu)==WORD_AT(old+209u*0x30u+0x2Cu));
    CHECK(WORD_AT(fresh+300u*0x30u+4u)==0 && WORD_AT(fresh+300u*0x30u+8u)==0);
    CHECK(mapmaker_rooms_cap()==241);
    /* already complete: nothing allocated */
    allocs=0;
    pc_mapmaker_rooms_props(fake_original);
    CHECK(allocs==0 && WORD_AT(0x0075EAA8u)==341);
    /* story levels: untouched */
    room_models(0x9CFu,300u);WORD_AT(0x00791D4Cu)=old;WORD_AT(0x0079094Cu)=53;
    pc_mapmaker_rooms_props(fake_original);
    CHECK(allocs==0 && WORD_AT(0x00791D4Cu)==old && WORD_AT(0x0075EAA8u)==300);
    /* a table without all 200 room models is not the MapMaker one: untouched */
    room_models(0x9CEu,300u);WORD_AT(0x0079094Cu)=100;
    pc_mapmaker_rooms_props(fake_original);
    CHECK(allocs==0 && WORD_AT(0x0075EAA8u)==300);
    /* models already defined by the data are not replaced */
    room_models(0x9CFu,300u);set16(MAPMAKER_ROOMS_MODEL_TABLE+0x9D0u*2u,7);
    pc_mapmaker_rooms_props(fake_original);
    CHECK(allocs==1 && WORD_AT(0x0075EAA8u)==340 && u16(MAPMAKER_ROOMS_MODEL_TABLE+0x9D0u*2u)==7 &&
          u16(MAPMAKER_ROOMS_MODEL_TABLE+0x9D1u*2u)==300);

    if(failures)return 1;
    puts("mapmaker rooms ok");
    return 0;
}
