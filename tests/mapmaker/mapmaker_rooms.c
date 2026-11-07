/* Asset-free contract for src/interface/pc_mapmaker_rooms.inc (MapMaker rooms 200 ->
 * 1023): the bounded room query keeps the original's result and flag handling
 * past the old ~209-room stack frame; the compile guard drops only pieces that
 * would pass the room or portal limit, with the limit tied to the room-model
 * slots; prop slots for room models 0x9D0..0x907+MAX are synthesized in scene 100
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
/* word draw buckets: tests/mapmaker/mapmaker_rooms_buckets.c */
void sub_0001A010(void) {} void sub_000A79B0(void) {} void sub_003442B0(void) {}
void sub_00086AE0(void) {} void sub_00344350(void) {} void sub_00087720(void) {}
/* Fake tree query: offers fake_hits[] (with repeats) to a model of the
 * callback sub_00087260, which lists a room once (record flag 0x10) as
 * room*2 | in-box bit. */
static uint32_t fake_hits[1200],fake_inside[1200],fake_hit_count,query_calls,query_args[5];
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
    memset((char *)memory+OUT,0xEE,0x2000);
    g_eax=0xDEADu;
    pc_mapmaker_rooms_query(fake_original);
    if(g_esp!=STACK+4u) return 0xBADu;            /* the replaced original's ret */
    return g_eax;
}

/* Compile: the fake compiler builds n entries of 0x68 B and calls the piece wrapper for each. */
#define ENTRIES 0x00300000u
static uint32_t piece_rooms[1100],piece_portals[1100],piece_count;
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
    for(m=0;m<MAPMAKER_ROOMS_MODEL_ENTRIES;m++) set16(MAPMAKER_ROOMS_MODEL_TABLE+m*2u,0xFFFFu);
    for(m=0x908u;m<=last_model;m++) set16(MAPMAKER_ROOMS_MODEL_TABLE+m*2u,(uint16_t)(10u+m-0x908u));
    WORD_AT(0x0075EAA8u)=slots;
}

/* ---- lights: models of the guest bodies (byte room +0x99), called through the hooks ----
 * Each fake original does what its function's code does with the room (text.asm
 * sub_001123C0, sub_001124D0, sub_00114030, sub_00114290) and pops the return
 * address like its ret. Inner calls go through the hooks, as the lifted calls go
 * through the generated wrappers. */
#define LPOOL 0x00600000u
#define LONE 0x00680000u
#define LSOURCE 0x00690000u
#define LGLOBAL 0x0073C34Cu            /* global light list head (stock address, not moved) */
#define LCOUNT 400u
static int light_ok=1;                 /* sub_001134D0's answer in the create model */
static uint32_t lhead(uint32_t row,uint32_t room) { return MAPMAKER_ROOMS_LIGHT_HEADS+(row*MAPMAKER_ROOMS_LIGHT_ROW+room)*4u; }
static uint32_t lgroup(uint32_t g) { return MAPMAKER_ROOMS_LIGHT_GROUPS+g*4u; }
static void model_unlink(void)
{
    uint32_t light=g_eax,next=WORD_AT(light+0x80u),prev=WORD_AT(light+0x84u),room;
    if(!next && !prev) {
        room=*u8(light+0x99u);
        if(room && WORD_AT(lhead(u16(light+0x92u),room))==light) WORD_AT(lhead(u16(light+0x92u),room))=0;
    } else if(!prev) {
        room=*u8(light+0x99u);
        WORD_AT(lhead(u16(light+0x92u),room))=next;WORD_AT(next+0x84u)=0;WORD_AT(light+0x80u)=0;
    } else if(!next) { WORD_AT(prev+0x80u)=0;WORD_AT(light+0x84u)=0; }
    else { WORD_AT(next+0x84u)=prev;WORD_AT(prev+0x80u)=next;WORD_AT(light+0x84u)=0;WORD_AT(light+0x80u)=0; }
    if(WORD_AT(light+0x8Cu)) WORD_AT(WORD_AT(light+0x8Cu)+0x88u)=WORD_AT(light+0x88u);
    else if(WORD_AT(LGLOBAL)==light) WORD_AT(LGLOBAL)=0;
    if(WORD_AT(light+0x88u)) WORD_AT(WORD_AT(light+0x88u)+0x8Cu)=WORD_AT(light+0x8Cu);
    WORD_AT(light+0x8Cu)=0;WORD_AT(light+0x88u)=0;*u8(light+0x99u)=0;
    g_esp+=4u;
}
static void model_colour(void)
{
    uint32_t light=WORD_AT(g_esp+4u),room;
    room=(WORD_AT(0x0079094Cu)==100u && WORD_AT(light+0x9Cu))?WORD_AT(WORD_AT(light+0x9Cu)+0x34u):*u8(light+0x99u);
    g_eax=((int32_t)room<=0 || room>=WORD_AT(0x007B975Cu)+1u)?WORD_AT(g_esp+8u):0xC0000000u+room;
    g_esp+=4u;
}
static void model_link(void)
{
    uint32_t light=WORD_AT(g_esp+4u),room=g_eax,head=lhead(u16(light+0x92u),room),tail;
    if(!WORD_AT(head)) WORD_AT(head)=light;
    else {
        unsigned steps=0;
        for(tail=WORD_AT(head);WORD_AT(tail+0x80u) && steps<1000u;tail=WORD_AT(tail+0x80u)) steps++;
        CHECK(steps<1000u);                                  /* a cycle: a list was corrupted */
        WORD_AT(tail+0x80u)=light;WORD_AT(light+0x84u)=tail;
    }
    *u8(light+0x99u)=(uint8_t)room;
    (void)WORD_AT(lgroup((uint32_t)((int32_t)room/16)));   /* the brightness loop reads rr[room/16] */
    WORD_AT(light+0x88u)=WORD_AT(LGLOBAL);
    if(WORD_AT(LGLOBAL)) WORD_AT(WORD_AT(LGLOBAL)+0x8Cu)=light;
    WORD_AT(LGLOBAL)=light;
    g_esp+=4u;
}
static void model_create(void)
{
    uint32_t source=g_eax,room=WORD_AT(g_esp+0x10u),light,g,esp=g_esp;
    if(u16(source)==1u) light=WORD_AT(MAPMAKER_ROOMS_LIGHT_ONE);
    else {
        g=(uint32_t)((int32_t)room/16);
        light=WORD_AT(MAPMAKER_ROOMS_LIGHT_POOL)+(WORD_AT(lgroup(g))+g*WORD_AT(MAPMAKER_ROOMS_LIGHT_PER))*0xB8u;
    }
    g_eax=light;g_esp-=4u;pc_mapmaker_rooms_light_unlink(model_unlink);
    set16(light+0x90u,u16(source));set16(light+0x92u,u16(source+2u));
    *u8(light+0x99u)=(uint8_t)room;                       /* 0x00114390: mov [esi+0x99], dl */
    if(!light_ok) { set16(light+0x96u,0xFFFFu);g_eax=0;g_esp=esp+4u;return; }
    if(u16(light+0x90u)!=1u) {                            /* 0x00114829: step the byte's group */
        g=*u8(light+0x99u)>>4;
        WORD_AT(lgroup(g))=(uint32_t)(((int32_t)WORD_AT(lgroup(g))+1)%(int32_t)WORD_AT(MAPMAKER_ROOMS_LIGHT_PER));
    }
    g_eax=*u8(light+0x99u);                               /* 0x00114851: movzx eax, byte [esi+0x99] */
    g_esp-=8u;WORD_AT(g_esp+4u)=light;pc_mapmaker_rooms_light_link(model_link);g_esp+=4u;
    g_eax=light;g_esp=esp+4u;
}
static uint32_t light_create(uint32_t type,uint32_t row,uint32_t room)
{
    set16(LSOURCE,(uint16_t)type);set16(LSOURCE+2u,(uint16_t)row);
    g_esp=STACK;WORD_AT(STACK+0x10u)=room;g_eax=LSOURCE;
    pc_mapmaker_rooms_light_create(model_create);
    CHECK(g_esp==STACK+4u);
    return g_eax;
}
static void light_unlink(uint32_t light)
{
    g_esp=STACK;g_eax=light;
    pc_mapmaker_rooms_light_unlink(model_unlink);
    CHECK(g_esp==STACK+4u);
}
static uint32_t light_colour(uint32_t light)
{
    g_esp=STACK;WORD_AT(STACK+4u)=light;WORD_AT(STACK+8u)=0x00ABCDEFu;
    pc_mapmaker_rooms_light_colour(model_colour);
    CHECK(g_esp==STACK+4u);
    return g_eax;
}
static void light_reset(void)
{
    uint32_t i;
    memset((char *)memory+LPOOL,0,LCOUNT*0xB8u);memset((char *)memory+LONE,0,0xC0u);
    memset((char *)memory+MAPMAKER_ROOMS_LIGHT_HEADS,0,3u*MAPMAKER_ROOMS_LIGHT_ROW*4u);
    for(i=0;i<MAPMAKER_ROOMS_LIGHT_GROUP_COUNT;i++) WORD_AT(lgroup(i))=0;
    WORD_AT(LGLOBAL)=0;
    g_esp=STACK;pc_mapmaker_rooms_light_reset(fake_original);
}
static void lights(void)
{
    uint32_t a,b,c,d,e,f,g;
    uint8_t before[0x2000];
    WORD_AT(MAPMAKER_ROOMS_LIGHT_POOL)=LPOOL;WORD_AT(MAPMAKER_ROOMS_LIGHT_COUNT)=LCOUNT;
    WORD_AT(MAPMAKER_ROOMS_LIGHT_ONE)=LONE;WORD_AT(MAPMAKER_ROOMS_LIGHT_PER)=LCOUNT/MAPMAKER_ROOMS_LIGHT_GROUP_COUNT;
    WORD_AT(0x007B975Cu)=200;WORD_AT(0x0079094Cu)=53;          /* a story level, 200 rooms */
    light_reset();
    /* real rooms: the stock byte paths, nothing in the side table */
    a=light_create(0,1,5);b=light_create(0,1,5);
    CHECK(a==LPOOL && b==LPOOL+0xB8u && WORD_AT(lhead(1,5))==a && WORD_AT(a+0x80u)==b && WORD_AT(lgroup(0))==2);
    CHECK(light_colour(a)==0xC0000005u && mapmaker_rooms_light_wide(a)==0 && mapmaker_rooms_light.wide==0);
    light_unlink(a);
    CHECK(WORD_AT(lhead(1,5))==b && WORD_AT(b+0x84u)==0 && *u8(a+0x99u)==0);
    /* a room id above 0xFF: its own head entry, never room (id & 0xFF)'s */
    CHECK(MAPMAKER_ROOMS_LIGHT_GROUP_COUNT>MAPMAKER_ROOMS_ID_GLOBAL/16u);
    {
        c=light_create(0,2,0x101);                         /* group 16: record 16 * per */
        CHECK(c==LPOOL+16u*WORD_AT(MAPMAKER_ROOMS_LIGHT_PER)*0xB8u);
        CHECK(WORD_AT(lhead(2,0x101))==c && WORD_AT(lhead(2,1))==0 && *u8(c+0x99u)==1);
        CHECK(mapmaker_rooms_light_wide(c)==0x101 && mapmaker_rooms_light.wide==1);
        CHECK(WORD_AT(lgroup(16))==1 && WORD_AT(lgroup(0))==2);  /* stepped on the room's group, not the byte's */
        d=light_create(0,2,0x101);
        CHECK(d==c+0xB8u && WORD_AT(c+0x80u)==d && WORD_AT(d+0x84u)==c && WORD_AT(lgroup(16))==2);
        e=light_create(0,2,1);                             /* real room 1 beside it */
        CHECK(WORD_AT(lhead(2,1))==e && WORD_AT(lhead(2,0x101))==c);
        /* colour: rooms above the real ones keep the colour, also with the byte 1 */
        CHECK(light_colour(c)==0x00ABCDEFu && *u8(c+0x99u)==1 && light_colour(e)==0xC0000001u);
        WORD_AT(0x0079094Cu)=100;WORD_AT(c+0x9Cu)=0x00650000u;WORD_AT(0x00650000u+0x34u)=7;
        CHECK(light_colour(c)==0xC0000007u);                /* MapMaker: the source prop's room */
        WORD_AT(0x0079094Cu)=53;WORD_AT(c+0x9Cu)=0;
        /* unlink the head with a next: the next becomes the head, room 1 untouched */
        light_unlink(c);
        CHECK(WORD_AT(lhead(2,0x101))==d && WORD_AT(d+0x84u)==0 && WORD_AT(c+0x80u)==0 && WORD_AT(lhead(2,1))==e);
        CHECK(mapmaker_rooms_light_wide(c)==0 && *u8(c+0x99u)==0 && mapmaker_rooms_light.unlinked==1);
        light_unlink(d);                                   /* alone */
        CHECK(WORD_AT(lhead(2,0x101))==0 && WORD_AT(lhead(2,1))==e && mapmaker_rooms_light_wide(d)==0);
        /* the type-1 record */
        f=light_create(1,0,0x105);
        CHECK(f==LONE && WORD_AT(lhead(0,0x105))==f && mapmaker_rooms_light_wide(f)==0x105 && WORD_AT(lgroup(16))==2);
        light_unlink(f);
        CHECK(WORD_AT(lhead(0,0x105))==0 && WORD_AT(lhead(0,5))==0);
        /* a failed creation (sub_001134D0 = 0) keeps the id for the record's next unlink */
        light_ok=0;
        g=light_create(0,1,0x102);
        CHECK(g==0 && mapmaker_rooms_light_wide(LPOOL+(16u*WORD_AT(MAPMAKER_ROOMS_LIGHT_PER)+2u)*0xB8u)==0x102);
        light_ok=1;
        /* slot reuse: creating over a record unlinks its old full-id room */
        WORD_AT(lgroup(16))=0;
        c=light_create(0,2,0x101);d=light_create(0,2,0x101);
        WORD_AT(lgroup(16))=0;
        e=light_create(0,2,0x101);                         /* reuses c: d becomes the head */
        CHECK(e==c && WORD_AT(lhead(2,0x101))==d && WORD_AT(d+0x80u)==e && WORD_AT(e+0x84u)==d);
        /* a group past the pool (stock does the same for its special ids): kept by address */
        WORD_AT(MAPMAKER_ROOMS_LIGHT_PER)=30;WORD_AT(lgroup(16))=0;
        f=light_create(0,1,0x103);
        CHECK(f==LPOOL+480u*0xB8u && mapmaker_rooms_light_slot(f)<0 && mapmaker_rooms_light_wide(f)==0x103);
        CHECK(WORD_AT(lhead(1,0x103))==f && WORD_AT(lhead(1,3))==0 && light_colour(f)==0x00ABCDEFu);
        light_unlink(f);
        CHECK(WORD_AT(lhead(1,0x103))==0 && mapmaker_rooms_light_wide(f)==0 && mapmaker_rooms_light.others[0].light==0);
        f=light_create(0,1,0x103);
        CHECK(f==LPOOL+481u*0xB8u && mapmaker_rooms_light.others[0].light==f);
        WORD_AT(MAPMAKER_ROOMS_LIGHT_PER)=LCOUNT/MAPMAKER_ROOMS_LIGHT_GROUP_COUNT;
        /* reset clears the side table */
        light_reset();
        CHECK(mapmaker_rooms_light_wide(c)==0 && mapmaker_rooms_light_wide(LONE)==0 &&
              mapmaker_rooms_light_wide(f)==0 && mapmaker_rooms_light.others[0].light==0);
    }
    /* not a record: no side entry */
    CHECK(mapmaker_rooms_light_slot(LPOOL+5u)<0 && mapmaker_rooms_light_slot(LPOOL+LCOUNT*0xB8u)<0 &&
          mapmaker_rooms_light_slot(0)<0 && mapmaker_rooms_light_slot(LPOOL-0xB8u)<0);
    /* the stock paths leave exactly what the bodies alone leave */
    light_reset();
    a=light_create(0,0,3);b=light_create(0,0,3);light_unlink(a);
    memcpy(before,(char *)memory+MAPMAKER_ROOMS_LIGHT_HEADS,sizeof before);
    light_reset();
    set16(LSOURCE,0);set16(LSOURCE+2u,0);
    g_esp=STACK;WORD_AT(STACK+0x10u)=3;g_eax=LSOURCE;model_create();
    g_esp=STACK;WORD_AT(STACK+0x10u)=3;g_eax=LSOURCE;model_create();
    g_esp=STACK;g_eax=a;model_unlink();
    CHECK(!memcmp(before,(char *)memory+MAPMAKER_ROOMS_LIGHT_HEADS,sizeof before));
}

int main(void)
{
    uint32_t i,r,n,fresh,old;
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x00708788u)=RECORDS;

    /* ---- room query ---- */
    for(i=0;i<1023;i++) { fake_hits[i]=i+1;fake_inside[i]=1; }
    for(i=0;i<177;i++) { fake_hits[1023+i]=(i*7)%1023+1;fake_inside[1023+i]=1; } /* repeats */
    fake_hit_count=1200;
    n=query(0x00170000u,0x400u);
    CHECK(n==1023 && originals==0 && query_calls==1);
    CHECK(query_args[0]==0x00702E00u && query_args[1]==0x00170000u && query_args[2]==0x0017000Cu &&
          query_args[3]==0x00087260u && query_args[4]==MAPMAKER_ROOMS_QUERY_BUFFER);
    CHECK(WORD_AT(OUT)==1 && WORD_AT(OUT+1022*4)==1023 && WORD_AT(OUT+1023*4)==0xEEEEEEEEu);
    for(r=1;r<=1023;r++) CHECK(!(*u8(RECORDS+r*0xB4u+0xB2u)&0x10u));       /* flags cleared */
    CHECK(WORD_AT(STACK)==0x12345678u && g_esp==STACK+4u); /* the original's ret */
    CHECK(mapmaker_rooms.query_depth==0);
    /* the in-box bit decides, max stops listing but every flag is still cleared */
    for(i=0;i<1023;i++) fake_inside[i]=i&1;
    fake_hit_count=1023;
    n=query(0x00170000u,30u);
    CHECK(n==30 && WORD_AT(OUT)==2 && WORD_AT(OUT+29*4)==60 && WORD_AT(OUT+30*4)==0xEEEEEEEEu);
    for(r=1;r<=1023;r++) CHECK(!(*u8(RECORDS+r*0xB4u+0xB2u)&0x10u));
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
    room_models(0x907u+1023u,1100u);                     /* every room model has a slot */
    piece_count=1024;
    for(i=0;i<piece_count;i++) { piece_rooms[i]=1;piece_portals[i]=4; }
    originals=0;
    pc_mapmaker_rooms_compile(fake_compile);
    CHECK(originals==1 && mapmaker_rooms.cap==1023 && !mapmaker_rooms.compiling);
    CHECK(WORD_AT(0x007B975Cu)==1023 && mapmaker_rooms.dropped==1 && mapmaker_rooms.pieces==1024);
    CHECK(WORD_AT(ENTRIES+1022*0x68u+0x14u)==1 && WORD_AT(ENTRIES+1022*0x68u+0x24u)==3);
    CHECK(WORD_AT(ENTRIES+1023*0x68u+0x14u)==0 && WORD_AT(ENTRIES+1023*0x68u+0x18u)==0 &&
          WORD_AT(ENTRIES+1023*0x68u+0x24u)==0);             /* the 1024th piece is dropped whole */
    /* a second compile starts again from zero */
    piece_count=1023;
    pc_mapmaker_rooms_compile(fake_compile);
    CHECK(WORD_AT(0x007B975Cu)==1023 && mapmaker_rooms.dropped==0);
    /* the old caps: 256 and 512 pieces are no longer dropped */
    piece_count=256;
    pc_mapmaker_rooms_compile(fake_compile);
    CHECK(WORD_AT(0x007B975Cu)==256 && mapmaker_rooms.dropped==0);
    piece_count=512;
    pc_mapmaker_rooms_compile(fake_compile);
    CHECK(WORD_AT(0x007B975Cu)==512 && mapmaker_rooms.dropped==0);
    piece_count=1023;
    /* without slots for every extra room model the limit stays at 200 */
    room_models(0x907u+200u,1100u);
    pc_mapmaker_rooms_compile(fake_compile);
    CHECK(mapmaker_rooms.cap==200 && WORD_AT(0x007B975Cu)==200 && mapmaker_rooms.dropped==823);
    room_models(0x907u+1022u,1100u);                     /* room 1023's model missing */
    pc_mapmaker_rooms_compile(fake_compile);
    CHECK(mapmaker_rooms.cap==200 && mapmaker_rooms.dropped==823);
    room_models(0x907u+1023u,1030u);                     /* room 1023's slot past the slot count */
    pc_mapmaker_rooms_compile(fake_compile);
    CHECK(mapmaker_rooms.cap==200);
    /* a two-room piece that does not fit is dropped, later one-room pieces still fit */
    room_models(0x907u+1023u,1100u);
    piece_count=4;piece_rooms[0]=1021;piece_rooms[1]=3;piece_rooms[2]=1;piece_rooms[3]=1;
    pc_mapmaker_rooms_compile(fake_compile);
    CHECK(WORD_AT(0x007B975Cu)==1023 && mapmaker_rooms.dropped==1 && WORD_AT(ENTRIES+0x68u+0x14u)==0);
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
    CHECK(originals==1 && allocs==1 && alloc_size==((1123u*0x30u+15u)&~15u) && fresh==0x00500000u);
    CHECK(WORD_AT(0x0075EAA8u)==1123);                   /* 300 + rooms 201..1023 */
    CHECK(!memcmp((char *)memory+fresh,(char *)memory+old,300u*0x30u));   /* existing slots kept */
    for(r=201;r<=1023;r++) CHECK(u16(MAPMAKER_ROOMS_MODEL_TABLE+(0x907u+r)*2u)==300u+r-201u);
    CHECK(u16(MAPMAKER_ROOMS_MODEL_TABLE+(0x907u+1024u)*2u)==0xFFFFu); /* model 0xD07: past the rooms */
    /* new slots copy room 200's slot (index 209), geometry fields cleared */
    CHECK(WORD_AT(fresh+300u*0x30u)==WORD_AT(old+209u*0x30u) &&
          WORD_AT(fresh+1122u*0x30u+0x2Cu)==WORD_AT(old+209u*0x30u+0x2Cu));
    CHECK(WORD_AT(fresh+300u*0x30u+4u)==0 && WORD_AT(fresh+300u*0x30u+8u)==0);
    CHECK(WORD_AT(fresh+1122u*0x30u+4u)==0 && WORD_AT(fresh+1122u*0x30u+8u)==0);
    CHECK(mapmaker_rooms_cap()==1023);
    /* already complete: nothing allocated */
    allocs=0;
    pc_mapmaker_rooms_props(fake_original);
    CHECK(allocs==0 && WORD_AT(0x0075EAA8u)==1123);
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
    CHECK(allocs==1 && WORD_AT(0x0075EAA8u)==1122 && u16(MAPMAKER_ROOMS_MODEL_TABLE+0x9D0u*2u)==7 &&
          u16(MAPMAKER_ROOMS_MODEL_TABLE+0x9D1u*2u)==300);

    /* ---- light rooms past 0xFF ---- */
    lights();

    if(failures)return 1;
    puts("mapmaker rooms ok");
    return 0;
}
