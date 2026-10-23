/* US 45410066 MapMaker rooms: 200 -> 241 visibility rooms (one per piece).
 *
 * The guest-code table config/guest-code-patches/rooms.json renumbers the
 * room special ids (players 0xCA..0xCE -> 0xF3..0xF7, views 0xCF..0xD4 ->
 * 0xF8..0xFD, global 0xD5 -> 0xFE, id space 0xD6 -> 0xFF), raises every
 * "real room" test from 200 to 241, moves the static per-room tables into
 * 0x008C0000..0x008CFFFF and the view struct's per-room arrays into a shadow
 * at view+0x6000 (see docs/mapmaker-rooms.md). This file is the C half:
 *
 *   WRAP sub_00087310 -> pc_mapmaker_rooms_query
 *   WRAP sub_0008A780 -> pc_mapmaker_rooms_compile
 *   WRAP sub_00083F80 -> pc_mapmaker_rooms_piece
 *   WRAP sub_00085030 -> pc_mapmaker_rooms_piece
 *   WRAP sub_00242780 -> pc_mapmaker_rooms_props
 *
 * Room query (sub_00087310): the original collects the rooms overlapping a box
 * into a 0x1B0-byte stack frame (about 209 words) with no bound; reimplemented
 * here with the same tree walk, callback and result, collecting into a static
 * guest buffer of 1023 words (the callback de-duplicates, so it holds at most
 * one word per room).
 *
 * Compile guard (sub_0008A780, MapMaker map play only): sub_00083F80 and
 * sub_00085030 build one entry per piece (+0x14 rooms, +0x18 portals). Totals
 * are counted as they are built; a piece that would take the map past the room
 * limit or past the 0x3FFF portal flags (the table grows the original's stack
 * buffer from 2048 to 16384) is dropped: its rooms, portals and 0x2C records
 * are set to 0 before the compiler sums them, and the drop is logged. The
 * editor's piece limit (MAPMAKER_LIMIT_PIECES, opt-in) is the real bound; this
 * is the last line. The limit is 241 only when every room model 0x908..0x9F8
 * has a prop slot, else the stock 200.
 *
 * Room props (sub_00242780): room r is drawn by a prop of model 0x907+r, whose
 * slot must come from the prop table; the MapMaker table defines 0x908..0x9CF
 * (200 rooms). In scene 100 the missing room models 0x9D0..0x9F8 get slots
 * copied from room 200's (model 0x9CF), in a slot array grown by that many
 * entries (same Level heap; the old array is left to the scene). Story levels
 * are not touched.
 *
 * Requires the includer's WORD_AT, guest_writable, guest_call, g_esp, g_eax
 * and g_xbox_mem_offset. */
#include <stdio.h>
#include <string.h>

#define MAPMAKER_ROOMS_MAX 241u
#define MAPMAKER_ROOMS_STOCK 200u
#define MAPMAKER_ROOMS_PORTALS 0x3FFFu          /* flag bytes at esp+0x1280, 0x4000 B */
#define MAPMAKER_ROOMS_MODEL_TABLE 0x008CA000u   /* relocated 0x0078B860, 0xA00 words */
#define MAPMAKER_ROOMS_MODEL_FIRST 0x908u        /* model of room 1 */
#define MAPMAKER_ROOMS_QUERY_BUFFER 0x008CB800u
#define MAPMAKER_ROOMS_QUERY_BYTES 0x800u
#define MAPMAKER_ROOMS_QUERY_LEVELS 2u
#define MAPMAKER_ROOMS_SLOT 0x30u

static struct {
    uint32_t cap,rooms,portals,pieces,dropped;
    unsigned query_depth;
    int compiling;
} mapmaker_rooms;

static uint16_t mapmaker_rooms_u16(uint32_t a) { return *(uint16_t *)((uintptr_t)a+g_xbox_mem_offset); }
static void mapmaker_rooms_set_u16(uint32_t a,uint16_t v) { *(uint16_t *)((uintptr_t)a+g_xbox_mem_offset)=v; }
static void *mapmaker_rooms_host(uint32_t a) { return (void *)((uintptr_t)a+g_xbox_mem_offset); }

static int mapmaker_rooms_slot_ok(uint32_t model)
{
    int16_t slot=(int16_t)mapmaker_rooms_u16(MAPMAKER_ROOMS_MODEL_TABLE+model*2u);
    return slot>=0 && (uint32_t)slot<WORD_AT(0x0075EAA8u);
}
static uint32_t mapmaker_rooms_cap(void)
{
    uint32_t r;
    for(r=1;r<=MAPMAKER_ROOMS_MAX;r++)
        if(!mapmaker_rooms_slot_ok(MAPMAKER_ROOMS_MODEL_FIRST-1u+r)) return MAPMAKER_ROOMS_STOCK;
    return MAPMAKER_ROOMS_MAX;
}

/* ---- sub_00087310(box, out, max): rooms overlapping box ---------------- */
extern void sub_00157440(void);
void pc_mapmaker_rooms_query(void (*original)(void))
{
    uint32_t box=WORD_AT(g_esp+4u),out=WORD_AT(g_esp+8u),max=WORD_AT(g_esp+12u);
    uint32_t buffer,count,records,i,found=0,take=1;
    if(mapmaker_rooms.query_depth>=MAPMAKER_ROOMS_QUERY_LEVELS) { original(); return; }
    buffer=MAPMAKER_ROOMS_QUERY_BUFFER+mapmaker_rooms.query_depth*MAPMAKER_ROOMS_QUERY_BYTES;
    mapmaker_rooms.query_depth++;
    mapmaker_rooms_set_u16(buffer,0);
    /* the original: sub_00157440(tree 0x702E00, box min, box max, sub_00087260, &list) */
    guest_call(sub_00157440,5,0x00702E00u,box,box+0xCu,0x00087260u,buffer);
    count=mapmaker_rooms_u16(buffer);
    if(count>MAPMAKER_ROOMS_QUERY_BYTES/2u-1u) count=MAPMAKER_ROOMS_QUERY_BYTES/2u-1u;
    records=WORD_AT(0x00708788u);
    for(i=0;i<count;i++) {
        uint32_t word=mapmaker_rooms_u16(buffer+2u+2u*i),room=word>>1;
        *(uint8_t *)mapmaker_rooms_host(records+room*0xB4u+0xB2u)&=0xEFu; /* clear "listed" */
        if(word&take) {
            WORD_AT(out+found*4u)=room;
            if(++found==max) take=0;
        }
    }
    mapmaker_rooms.query_depth--;
    g_eax=found;
}

/* ---- MapMaker compile: count rooms and portals per piece --------------- */
static void mapmaker_rooms_account(uint32_t entry)
{
    uint32_t rooms,portals;
    if(!mapmaker_rooms.compiling || !guest_writable(entry,0x2Cu)) return;
    rooms=WORD_AT(entry+0x14u);portals=WORD_AT(entry+0x18u);
    mapmaker_rooms.pieces++;
    if(rooms>mapmaker_rooms.cap-mapmaker_rooms.rooms ||
       portals>MAPMAKER_ROOMS_PORTALS-mapmaker_rooms.portals) {
        WORD_AT(entry+0x14u)=0;WORD_AT(entry+0x18u)=0;WORD_AT(entry+0x24u)=0;
        mapmaker_rooms.dropped++;
        return;
    }
    mapmaker_rooms.rooms+=rooms;mapmaker_rooms.portals+=portals;
}
void pc_mapmaker_rooms_piece(void (*original)(void))
{
    uint32_t entry=WORD_AT(g_esp+4u);
    original();
    mapmaker_rooms_account(entry);
}
void pc_mapmaker_rooms_compile(void (*original)(void))
{
    mapmaker_rooms.cap=mapmaker_rooms_cap();
    mapmaker_rooms.rooms=mapmaker_rooms.portals=mapmaker_rooms.pieces=mapmaker_rooms.dropped=0;
    mapmaker_rooms.compiling=1;
    original();
    mapmaker_rooms.compiling=0;
    fprintf(stderr,"[MAPMAKER] rooms %u of %u, portals %u, pieces %u",
            (unsigned)WORD_AT(0x007B975Cu),(unsigned)mapmaker_rooms.cap,
            (unsigned)WORD_AT(0x007B9754u),(unsigned)mapmaker_rooms.pieces);
    if(mapmaker_rooms.dropped)
        fprintf(stderr,"; %u pieces DROPPED (over the room or portal limit)",(unsigned)mapmaker_rooms.dropped);
    fputc('\n',stderr);
}

/* ---- prop slots for room models 0x9D0..0x9F8 ---------------------------- */
extern void sub_0003DFA0(void);
void pc_mapmaker_rooms_props(void (*original)(void))
{
    const uint32_t first=MAPMAKER_ROOMS_MODEL_FIRST,last=MAPMAKER_ROOMS_MODEL_FIRST-1u+MAPMAKER_ROOMS_MAX;
    uint32_t count,slots,fresh,model,extra=0,n,template_slot;
    original();
    if(!guest_writable(0x0079094Cu,4) || WORD_AT(0x0079094Cu)!=100u) return;
    for(model=first;model<first+MAPMAKER_ROOMS_STOCK;model++)
        if(!mapmaker_rooms_slot_ok(model)) return;          /* not the MapMaker table */
    for(model=first+MAPMAKER_ROOMS_STOCK;model<=last;model++)
        if((int16_t)mapmaker_rooms_u16(MAPMAKER_ROOMS_MODEL_TABLE+model*2u)<0) extra++;
    if(!extra) return;
    count=WORD_AT(0x0075EAA8u);slots=WORD_AT(0x00791D4Cu);
    template_slot=slots+mapmaker_rooms_u16(MAPMAKER_ROOMS_MODEL_TABLE+(first+MAPMAKER_ROOMS_STOCK-1u)*2u)*MAPMAKER_ROOMS_SLOT;
    if(!guest_writable(slots,count*MAPMAKER_ROOMS_SLOT)) return;
    fresh=guest_call(sub_0003DFA0,2,((count+extra)*MAPMAKER_ROOMS_SLOT+0xFu)&~0xFu,1u);
    if(!guest_writable(fresh,(count+extra)*MAPMAKER_ROOMS_SLOT)) return;
    memcpy(mapmaker_rooms_host(fresh),mapmaker_rooms_host(slots),count*MAPMAKER_ROOMS_SLOT);
    n=count;
    for(model=first+MAPMAKER_ROOMS_STOCK;model<=last;model++) {
        uint32_t slot;
        if((int16_t)mapmaker_rooms_u16(MAPMAKER_ROOMS_MODEL_TABLE+model*2u)>=0) continue;
        slot=fresh+n*MAPMAKER_ROOMS_SLOT;
        memcpy(mapmaker_rooms_host(slot),mapmaker_rooms_host(template_slot),MAPMAKER_ROOMS_SLOT);
        WORD_AT(slot+4u)=0;WORD_AT(slot+8u)=0;            /* geometry set by the compiler */
        mapmaker_rooms_set_u16(MAPMAKER_ROOMS_MODEL_TABLE+model*2u,(uint16_t)n);
        n++;
    }
    WORD_AT(0x00791D4Cu)=fresh;WORD_AT(0x0075EAA8u)=n;
}
