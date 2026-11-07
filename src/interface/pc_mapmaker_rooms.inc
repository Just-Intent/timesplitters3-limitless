/* US 45410066 MapMaker rooms: 200 -> 1023 visibility rooms (one per piece;
 * 511 until 2026-11-07), room machinery for 1023.
 *
 * The guest-code table config/guest-code-patches/rooms.json renumbers the
 * room special ids above the real rooms (players 0xCA..0xCE -> 0x401..0x405,
 * views 0xCF..0xD4 -> 0x406..0x40B, global 0xD5 -> 0x40C, spare 0xC9 ->
 * 0x400, id space 0xD6 -> 0x40D; in every scene), raises every "real room"
 * test from 200 to 1023 (ids 1..0x3FF), moves the static per-room and per-id
 * tables into 0x008C0000..0x008E0000 and 0x00908000.. and the view struct's per-room arrays
 * into a shadow at view+0x1000 inside a view stride grown from 0xFFC to
 * 0x5000 (see docs/mapmaker/mapmaker-rooms.md). This
 * file is the C half:
 *
 *   WRAP sub_00087310 -> pc_mapmaker_rooms_query
 *   WRAP sub_0008A780 -> pc_mapmaker_rooms_compile
 *   WRAP sub_00083F80 -> pc_mapmaker_rooms_piece
 *   WRAP sub_00085030 -> pc_mapmaker_rooms_piece
 *   WRAP sub_00242780 -> pc_mapmaker_rooms_props
 *   WRAP sub_001121E0 -> pc_mapmaker_rooms_light_reset
 *   WRAP sub_001123C0 -> pc_mapmaker_rooms_light_unlink   (light in eax)
 *   WRAP sub_001124D0 -> pc_mapmaker_rooms_light_colour
 *   WRAP sub_00114030 -> pc_mapmaker_rooms_light_link     (room in eax)
 *   WRAP sub_00114290 -> pc_mapmaker_rooms_light_create   (source in eax)
 *   WRAP sub_00086C40 -> pc_mapmaker_rooms_buckets        (replaces the body)
 *   WRAP sub_000897D0 -> pc_mapmaker_rooms_view_lists     (replaces the body)
 *   WRAP sub_00086480 -> pc_mapmaker_rooms_bucket_draw    (depth in eax)
 *   WRAP sub_000865E0 -> pc_mapmaker_rooms_bucket_draw    (depth in eax)
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
 * is the last line. The limit is MAPMAKER_ROOMS_MAX only when every room model 0x908..0x907+MAX
 * has a prop slot, else the stock 200.
 *
 * Room props (sub_00242780): room r is drawn by a prop of model 0x907+r, whose
 * slot must come from the prop table; the MapMaker table defines 0x908..0x9CF
 * (200 rooms). In scene 100 the missing room models 0x9D0..0x907+MAX get slots
 * copied from room 200's (model 0x9CF), in a slot array grown by that many
 * entries (same Level heap; the old array is left to the scene). Story levels
 * are not touched.
 *
 * Light rooms (sub_001121E0, sub_001123C0, sub_001124D0, sub_00114030,
 * sub_00114290): a light record (pool [0x0073D950], stride 0xB8, count
 * [0x004E62DC]; plus the one 0xC0 record [0x0073D960]) keeps its room in the
 * byte +0x99 and has no spare field, so a room id above 0xFF (real rooms
 * 0x100..0x3FF and the special ids 0x400..0x40C, in every scene) is kept here per light, in a
 * side table. Creation (sub_00114290) gets the full room as its 4th argument
 * and picks the light's pool group with it, but its round-robin step
 * (0x00114829) and its link call (0x00114851) re-read the byte: the link hook
 * gets the full id back and the step is redone on the full id's group. Unlink
 * (sub_001123C0) and the colour lookup (sub_001124D0) read the byte; for a
 * light with a full id the head entry is unlinked here and the original sees
 * room 0, its "no room" path (for the colour the same result a special id
 * above the real rooms gives stock: the colour unchanged; in scene 100 a light
 * with a source prop reads the prop's dword room instead, so only sourceless
 * lights in rooms 0x100..0x3FF keep their colour untinted). Lights in rooms
 * 0..0xFF (every room of a story level, and every light of the 241 layout)
 * take the original paths unchanged.
 *
 * Requires the includer's WORD_AT, guest_writable, guest_call, g_esp, g_eax
 * and g_xbox_mem_offset. */
#ifndef MAPMAKER_GUEST_RET
#define MAPMAKER_GUEST_RET() (g_esp+=4u) /* see pc_mapmaker_limits.inc */
#endif
#include <stdio.h>
#include <string.h>

#define MAPMAKER_ROOMS_MAX 1023u
#define MAPMAKER_ROOMS_IDS_REAL 0x3FFu          /* rooms.json: real rooms 1..0x3FF, special ids 0x400.. */
#define MAPMAKER_ROOMS_STOCK 200u
#define MAPMAKER_ROOMS_PORTALS 0x3FFFu          /* flag bytes at esp+0x1280, 0x4000 B */
#define MAPMAKER_ROOMS_MODEL_TABLE 0x008CA000u   /* relocated 0x0078B860, MAPMAKER_ROOMS_MODEL_ENTRIES words */
#define MAPMAKER_ROOMS_MODEL_ENTRIES 0xD08u      /* rooms.json: models 0..0xD07, room models 0x908..0xD06 (1023 rooms) */
#define MAPMAKER_ROOMS_MODEL_FIRST 0x908u        /* model of room 1 */
#define MAPMAKER_ROOMS_QUERY_BUFFER 0x008C9000u   /* 2 x 1023 words; the tree holds real rooms only */
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
#ifdef MAPMAKER_SKELVIS
    /* pc_mapmaker_skelvis.inc (log only): sub_0009FBE0's room list query */
    uint32_t touched=0,list_call=MAPMAKER_PTR_OK(g_esp,20u) && WORD_AT(g_esp)==SKV_RET_QUERY && WORD_AT(g_esp+16u)==SKV_RET_LIST;
#endif
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
#ifdef MAPMAKER_SKELVIS
        touched+=word&1u;
#endif
    }
#ifdef MAPMAKER_SKELVIS
    if(list_call) mapmaker_skelvis_list(out,max,touched,found,buffer,count);
#endif
    mapmaker_rooms.query_depth--;
    g_eax=found;
    MAPMAKER_GUEST_RET();                    /* sub_00087310 ends in a plain ret */
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

/* ---- prop slots for room models 0x9D0..0x907+MAPMAKER_ROOMS_MAX ---------- */
typedef char mapmaker_rooms_models_fit[MAPMAKER_ROOMS_MODEL_FIRST-1u+MAPMAKER_ROOMS_IDS_REAL<MAPMAKER_ROOMS_MODEL_ENTRIES &&
                                       MAPMAKER_ROOMS_MAX<=MAPMAKER_ROOMS_IDS_REAL?1:-1]; /* every real room has a model entry */
extern void sub_0003DFA0(void);
void pc_mapmaker_rooms_props(void (*original)(void))
{
    const uint32_t first=MAPMAKER_ROOMS_MODEL_FIRST,last=MAPMAKER_ROOMS_MODEL_FIRST-1u+MAPMAKER_ROOMS_MAX;
    uint32_t count,slots,fresh,model,extra=0,n,template_slot;
    original();
    if(WORD_AT(0x0079094Cu)!=100u) return;
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

/* ---- light rooms past 0xFF -------------------------------------------- */
#define MAPMAKER_ROOMS_LIGHT_POOL 0x0073D950u   /* -> light records, stride 0xB8 */
#define MAPMAKER_ROOMS_LIGHT_COUNT 0x004E62DCu  /* records in the pool (500/200/265/400) */
#define MAPMAKER_ROOMS_LIGHT_ONE 0x0073D960u    /* -> the one 0xC0 record (type-1 source) */
#define MAPMAKER_ROOMS_LIGHT_STRIDE 0xB8u
#define MAPMAKER_ROOMS_LIGHT_SLOTS 512u
#define MAPMAKER_ROOMS_LIGHT_HEADS 0x008DC000u  /* light heads [3][row] (relocated 0x0073C350) */
#define MAPMAKER_ROOMS_LIGHT_ROW 0x40Cu         /* head row width = the global room id */
#define MAPMAKER_ROOMS_LIGHT_GROUPS 0x008CBC00u /* round robin per room/16 (relocated 0x0073D910) */
#define MAPMAKER_ROOMS_LIGHT_GROUP_COUNT 68u    /* its entries (ids up to 0x43F) */
#define MAPMAKER_ROOMS_LIGHT_PER 0x0073D954u    /* records per room group */
#define MAPMAKER_ROOMS_LIGHT_OTHERS 256u        /* records outside the pool (see below) */

/* A group past the level's groups (room/16 >= [0x0073D958]; stock: every
 * special id on a level of under ~200 rooms) indexes records past the pool,
 * as stock does; those records are kept by address in others[]. */
static struct {
    uint16_t room[MAPMAKER_ROOMS_LIGHT_SLOTS+1u];   /* full room id when above 0xFF, else 0 */
    struct { uint32_t light; uint16_t room; } others[MAPMAKER_ROOMS_LIGHT_OTHERS];
    uint32_t pending;                              /* sub_00114290's full room while it runs */
    unsigned long long wide, unlinked, full;
} mapmaker_rooms_light;

static uint8_t *mapmaker_rooms_light_byte(uint32_t light) { return (uint8_t *)mapmaker_rooms_host(light+0x99u); }

/* Side-table index of a pool record (or of the 0xC0 record), else -1. */
static int mapmaker_rooms_light_slot(uint32_t light)
{
    uint32_t pool=WORD_AT(MAPMAKER_ROOMS_LIGHT_POOL),n=WORD_AT(MAPMAKER_ROOMS_LIGHT_COUNT),off;
    if(!light) return -1;
    if(light==WORD_AT(MAPMAKER_ROOMS_LIGHT_ONE)) return (int)MAPMAKER_ROOMS_LIGHT_SLOTS;
    if(!pool || light<pool) return -1;
    off=light-pool;
    if(off%MAPMAKER_ROOMS_LIGHT_STRIDE || off/MAPMAKER_ROOMS_LIGHT_STRIDE>=n ||
       off/MAPMAKER_ROOMS_LIGHT_STRIDE>=MAPMAKER_ROOMS_LIGHT_SLOTS) return -1;
    return (int)(off/MAPMAKER_ROOMS_LIGHT_STRIDE);
}
static uint16_t *mapmaker_rooms_light_entry(uint32_t light,int add)
{
    int s=mapmaker_rooms_light_slot(light);
    unsigned i,spare=MAPMAKER_ROOMS_LIGHT_OTHERS;
    if(s>=0) return &mapmaker_rooms_light.room[s];
    if(!light) return NULL;
    for(i=0;i<MAPMAKER_ROOMS_LIGHT_OTHERS;i++) {
        if(mapmaker_rooms_light.others[i].light==light) return &mapmaker_rooms_light.others[i].room;
        if(!mapmaker_rooms_light.others[i].light && spare==MAPMAKER_ROOMS_LIGHT_OTHERS) spare=i;
    }
    if(!add) return NULL;
    if(spare==MAPMAKER_ROOMS_LIGHT_OTHERS) { mapmaker_rooms_light.full++;return NULL; }
    mapmaker_rooms_light.others[spare].light=light;
    return &mapmaker_rooms_light.others[spare].room;
}
static uint32_t mapmaker_rooms_light_wide(uint32_t light)
{
    uint16_t *e=mapmaker_rooms_light_entry(light,0);
    return e?*e:0u;
}
static void mapmaker_rooms_light_set(uint32_t light,uint32_t room)
{
    int wide=room>0xFFu && room<0x10000u;
    uint16_t *e=mapmaker_rooms_light_entry(light,wide);
    unsigned i;
    if(!e) return;
    *e=(uint16_t)(wide?room:0u);
    if(!wide) for(i=0;i<MAPMAKER_ROOMS_LIGHT_OTHERS;i++)
        if(e==&mapmaker_rooms_light.others[i].room) mapmaker_rooms_light.others[i].light=0;
}

/* sub_001121E0 (level light reset, plain ret; also reached by a tail jmp
 * from the pool allocation): clears every record's room byte and the heads,
 * so the side table goes with them. */
void pc_mapmaker_rooms_light_reset(void (*original)(void))
{
    original();
    memset(mapmaker_rooms_light.room,0,sizeof mapmaker_rooms_light.room);
    memset(mapmaker_rooms_light.others,0,sizeof mapmaker_rooms_light.others);
    mapmaker_rooms_light.pending=0;
}

/* sub_001123C0 (eax = light, plain ret; tail-jumped from sub_00112990):
 * unlinks the light from its room's head list (+0x80 next, +0x84 prev, head
 * [row +0x92][room], only touched when prev is 0) and from the global list. */
void pc_mapmaker_rooms_light_unlink(void (*original)(void))
{
    uint32_t light=g_eax,room=mapmaker_rooms_light_wide(light);
    if(room && guest_writable(light,0xB8u)) {
        uint32_t next=WORD_AT(light+0x80u),prev=WORD_AT(light+0x84u);
        if(!prev) {                      /* the original's two head cases, with the full id */
            uint32_t head=MAPMAKER_ROOMS_LIGHT_HEADS+(mapmaker_rooms_u16(light+0x92u)*MAPMAKER_ROOMS_LIGHT_ROW+room)*4u;
            if(!next) { if(WORD_AT(head)==light) WORD_AT(head)=0; }
            else if(guest_writable(next,0xB8u)) { WORD_AT(head)=next;WORD_AT(next+0x84u)=0;WORD_AT(light+0x80u)=0; }
            *mapmaker_rooms_light_byte(light)=0;  /* the original then takes its "no room" path */
        }
        mapmaker_rooms_light_set(light,0);
        mapmaker_rooms_light.unlinked++;
    }
    original();
}

/* sub_001124D0(light, colour) -> colour (cdecl): a light in a real room gets
 * that room's colour; a room above the real ones returns the colour as is. */
void pc_mapmaker_rooms_light_colour(void (*original)(void))
{
    uint32_t light=WORD_AT(g_esp+4u);
    uint8_t *byte,keep;
    if(!mapmaker_rooms_light_wide(light) || !guest_writable(light,0xB8u)) {
        original();
        return;
    }
    /* scene 100 with a source prop reads the prop's dword room, not the byte */
    byte=mapmaker_rooms_light_byte(light);keep=*byte;*byte=0;
    original();
    *byte=keep;
}

/* sub_00114030 (eax = room, [esp+4] = light, plain ret): links the light
 * into the room's head list and steps its group; called only by
 * sub_00114290 (0x00114859) with the room byte that function just stored. */
void pc_mapmaker_rooms_light_link(void (*original)(void))
{
    uint32_t light=WORD_AT(g_esp+4u);
    if(mapmaker_rooms_light.pending && (g_eax&0xFFu)==(mapmaker_rooms_light.pending&0xFFu))
        g_eax=mapmaker_rooms_light.pending;
    mapmaker_rooms_light_set(light,g_eax);
    original();
}

/* sub_00114290 (eax = source record, ebx, stack args, [esp+0x10] = room;
 * returns the light or 0): takes record rr[g] + g*per of the pool, g =
 * room/16 (a type-1 source takes the 0xC0 record), fills and links it. */
void pc_mapmaker_rooms_light_create(void (*original)(void))
{
    uint32_t room=WORD_AT(g_esp+0x10u),source=g_eax,outer=mapmaker_rooms_light.pending;
    uint32_t byte_group,saved,picked=0,light,g=room>>4;
    if(room<=0xFFu || room>=0x10000u) {          /* a byte holds it: the stock paths */
        mapmaker_rooms_light.pending=0;
        original();
        mapmaker_rooms_light.pending=outer;
        return;
    }
    mapmaker_rooms_light.wide++;
    if(guest_writable(source,2u) && mapmaker_rooms_u16(source)!=1u && g<MAPMAKER_ROOMS_LIGHT_GROUP_COUNT)
        picked=WORD_AT(MAPMAKER_ROOMS_LIGHT_POOL)+(WORD_AT(MAPMAKER_ROOMS_LIGHT_GROUPS+g*4u)+
               g*WORD_AT(MAPMAKER_ROOMS_LIGHT_PER))*MAPMAKER_ROOMS_LIGHT_STRIDE;
    byte_group=MAPMAKER_ROOMS_LIGHT_GROUPS+((room&0xFFu)>>4)*4u;
    saved=WORD_AT(byte_group);
    mapmaker_rooms_light.pending=room;
    original();
    mapmaker_rooms_light.pending=outer;
    light=g_eax;
    if(light && guest_writable(light,0xB8u) && mapmaker_rooms_u16(light+0x90u)!=1u) {
        /* the step at 0x00114829 used the byte's group: undo it, step the room's */
        int32_t per=(int32_t)WORD_AT(MAPMAKER_ROOMS_LIGHT_PER);
        WORD_AT(byte_group)=saved;
        if(g<MAPMAKER_ROOMS_LIGHT_GROUP_COUNT && per>0)
            WORD_AT(MAPMAKER_ROOMS_LIGHT_GROUPS+g*4u)=(uint32_t)(((int32_t)WORD_AT(MAPMAKER_ROOMS_LIGHT_GROUPS+g*4u)+1)%per);
    }
    if(!light && picked) mapmaker_rooms_light_set(picked,room);   /* filled, not linked: keep its id */
}

/* ---- word draw buckets (stage 6 of rooms past 255) ---------------------
 * The view's rooms are drawn by portal-walk depth: room r of the current
 * view has its depth dword at shadow rect +0 (view +0x1000 + 12*r; -1 = not
 * reached). Stock sub_00086C40 (every scene, once per view drawn) put each
 * room id as a BYTE in row [depth] of 0x7042B8 (stock, rows of 200)
 * with a byte count per row at 0x702DB0; rooms whose record +0xB2 has bit 8
 * went into at most 10 per depth at 0x702A40, appended as new depths after
 * [view+0xD28]. It then drew through sub_00086AE0 -> sub_000867F0, whose two
 * readers sub_00086480/sub_000865E0 (eax = depth) walk one row. A byte holds
 * rooms 1..255 and a row 255 rooms.
 *
 * Here sub_00086C40 is reimplemented (same order, same calls) with the rows
 * kept as word lists in C, one context per nesting level. rooms.json makes
 * both readers read words (movzx word [edx+eax*2]) from one staging row
 * (row stride 0: every depth reads MAPMAKER_ROOMS_BUCKET_ROW); their hook
 * copies the depth's list there, at most 255 per call, sets that depth's
 * count byte and runs the reader once per 255 rooms (only a depth of more
 * than 255 rooms takes more than one call). sub_000897D0 (per-view sorted
 * room list, view shadow +0x44B0, count word +0xE08) builds the same list
 * from the same depths without byte tables. With at most 255 rooms per depth
 * (every level of up to 255 rooms) the rooms, order and calls are the stock
 * ones. Deviations: a depth >= 80 (stock wrote past its 80 rows and counts)
 * is skipped, and the list never grows past its shadow capacity. */
#define MAPMAKER_ROOMS_VIEW_CURRENT 0x007B0CC0u  /* -> the view being drawn (0xFFC struct + shadow) */
#define MAPMAKER_ROOMS_VIEW_ARRAY 0x007B0CE4u    /* -> view 0 */
#define MAPMAKER_ROOMS_VIEW_COUNT 0x007B0CE0u
#define MAPMAKER_ROOMS_VIEW_STRIDE 0x5000u       /* rooms.json: stock 0xFFC (struct) -> 0x5000 (struct, shadow) */
#define MAPMAKER_ROOMS_VIEW_RECTS 0x1000u        /* shadow: room r at +0x1000+12*r (int32 depth, int16 x0,y0,x1,y1) */
#define MAPMAKER_ROOMS_VIEW_FLAGS 0x40A0u        /* shadow: visible flag byte per id (0x410) */
#define MAPMAKER_ROOMS_VIEW_LIST 0x44B0u         /* shadow: sorted room list (words), count word +0xE08 */
#define MAPMAKER_ROOMS_VIEW_LIST_WORDS 0x40Du
#define MAPMAKER_ROOMS_VIEW_LIST_CAP 0x40Cu      /* rooms.json 0x000899CA: cmp ax,cap before a neighbour */
#define MAPMAKER_ROOMS_ID_GLOBAL 0x40Cu          /* rooms.json: global room id */
#define MAPMAKER_ROOMS_DEPTHS 0x50u
#define MAPMAKER_ROOMS_SORTED 10u                /* stock 0x702A40 rows: 10 per depth */
#define MAPMAKER_ROOMS_BUCKET_ROW 0x008C8000u    /* staging row (rooms.json: reader stride 0) */
#define MAPMAKER_ROOMS_BUCKET_COUNTS 0x00702DB0u /* reader count bytes per depth (stock) */
#define MAPMAKER_ROOMS_BUCKET_SEGMENT 255u
#define MAPMAKER_ROOMS_BUCKET_ROOMS 2048u        /* room ids the C lists hold (1..2047) */
#define MAPMAKER_ROOMS_BUCKET_LEVELS 4u
#define MAPMAKER_ROOMS_RECORDS 0x00708788u       /* -> room records, 0xB4 each, flags +0xB2 */

typedef struct {
    uint32_t view;
    uint16_t start[MAPMAKER_ROOMS_DEPTHS],count[MAPMAKER_ROOMS_DEPTHS];
    uint16_t room[MAPMAKER_ROOMS_BUCKET_ROOMS];
} mapmaker_rooms_depths;
static struct {
    mapmaker_rooms_depths level[MAPMAKER_ROOMS_BUCKET_LEVELS];
    unsigned depth;
    int32_t depth_of[MAPMAKER_ROOMS_BUCKET_ROOMS];
    unsigned long long segmented,clipped,nested;
} mapmaker_rooms_bk;

static uint32_t mapmaker_rooms_bucket_rooms(void)
{
    uint32_t rooms=WORD_AT(0x007B975Cu);
    if((int32_t)rooms<0) return 0;
    if(rooms>MAPMAKER_ROOMS_BUCKET_ROOMS-1u) { mapmaker_rooms_bk.clipped++;rooms=MAPMAKER_ROOMS_BUCKET_ROOMS-1u; }
    return rooms;
}
static int32_t mapmaker_rooms_rect_depth(uint32_t view,uint32_t r)
{
    return (int32_t)WORD_AT(view+MAPMAKER_ROOMS_VIEW_RECTS+12u*r);
}
static int16_t mapmaker_rooms_rect_h(uint32_t view,uint32_t r,uint32_t field)
{
    return (int16_t)mapmaker_rooms_u16(view+MAPMAKER_ROOMS_VIEW_RECTS+12u*r+field);
}

/* sub_00086C40 (no arguments, plain ret): bucket the current view's rooms
 * by depth, then draw them (the calls of 0x00086DDA..0x00086E04). */
extern void sub_0001A010(void),sub_000A79B0(void),sub_003442B0(void),sub_00086AE0(void),sub_00344350(void);
void pc_mapmaker_rooms_buckets(void (*original)(void))
{
    uint32_t view=WORD_AT(MAPMAKER_ROOMS_VIEW_CURRENT),rooms=mapmaker_rooms_bucket_rooms();
    uint32_t records=WORD_AT(MAPMAKER_ROOMS_RECORDS),r,b,n,vp;
    int32_t deepest=(int32_t)WORD_AT(view+0xD28u),k;
    uint16_t normal[MAPMAKER_ROOMS_DEPTHS],sorted[MAPMAKER_ROOMS_DEPTHS][MAPMAKER_ROOMS_SORTED];
    uint8_t sorted_n[MAPMAKER_ROOMS_DEPTHS];
    uint8_t replaced[MAPMAKER_ROOMS_DEPTHS];       /* depth taken by sorted row k: k+1, else 0 */
    int any_sorted=0;
    mapmaker_rooms_depths *c;
    (void)original;
#ifdef MAPMAKER_BOSSROOM
    mapmaker_bossroom_view(view);   /* pc_mapmaker_bossroom.inc: giant bosses also listed in the camera's room */
#endif
    if(mapmaker_rooms_bk.depth>=MAPMAKER_ROOMS_BUCKET_LEVELS) mapmaker_rooms_bk.nested++;
    c=&mapmaker_rooms_bk.level[mapmaker_rooms_bk.depth<MAPMAKER_ROOMS_BUCKET_LEVELS?mapmaker_rooms_bk.depth:MAPMAKER_ROOMS_BUCKET_LEVELS-1u];
    memset(normal,0,sizeof normal);memset(sorted_n,0,sizeof sorted_n);memset(replaced,0,sizeof replaced);
    WORD_AT(0x0070AAD0u)=0;
    for(r=1;r<=rooms;r++) {
        int32_t d=mapmaker_rooms_rect_depth(view,r);
        mapmaker_rooms_bk.depth_of[r]=-1;
        if(d<0) continue;
        if(*(uint8_t *)mapmaker_rooms_host(records+r*0xB4u+0xB2u)&8u) {
            any_sorted=1;                           /* set even when the row is full */
            if(d<(int32_t)MAPMAKER_ROOMS_DEPTHS && sorted_n[d]<MAPMAKER_ROOMS_SORTED) sorted[d][sorted_n[d]++]=(uint16_t)r;
            continue;
        }
        if(d>=(int32_t)MAPMAKER_ROOMS_DEPTHS) continue;
        mapmaker_rooms_bk.depth_of[r]=d;normal[d]++;
    }
    if(any_sorted)                                   /* each non-empty sorted row becomes depth ++[view+0xD28] */
        for(k=0;k<=deepest && k<(int32_t)MAPMAKER_ROOMS_DEPTHS;k++) {
            int32_t e;
            if(!sorted_n[k]) continue;
            e=(int32_t)WORD_AT(view+0xD28u)+1;WORD_AT(view+0xD28u)=(uint32_t)e;
            if(e>=0 && e<(int32_t)MAPMAKER_ROOMS_DEPTHS) replaced[e]=(uint8_t)(k+1);
        }
    for(n=0,b=0;b<MAPMAKER_ROOMS_DEPTHS;b++) {
        c->start[b]=(uint16_t)n;
        c->count[b]=replaced[b]?sorted_n[replaced[b]-1u]:normal[b];
        if(replaced[b]) memcpy(&c->room[n],sorted[replaced[b]-1u],c->count[b]*sizeof(uint16_t));
        n+=c->count[b];
        normal[b]=0;                                 /* fill position below */
    }
    for(r=1;r<=rooms;r++) {
        int32_t d=mapmaker_rooms_bk.depth_of[r];
        if(d<0 || replaced[d]) continue;
        c->room[c->start[d]+normal[d]++]=(uint16_t)r;
    }
    c->view=view;
    if((int32_t)WORD_AT(view+0xD28u)>=(int32_t)MAPMAKER_ROOMS_DEPTHS) WORD_AT(view+0xD28u)=MAPMAKER_ROOMS_DEPTHS-1u;
    if(!WORD_AT(view+0x214u)) WORD_AT(0x006B7ACCu)=0;
    vp=WORD_AT(0x007E71C8u);
    mapmaker_rooms_bk.depth++;
    guest_call(sub_0001A010,4,WORD_AT(vp),WORD_AT(vp+0x14u),WORD_AT(vp+4u),WORD_AT(vp+0x18u));
    guest_call(sub_000A79B0,1,MAPMAKER_ROOMS_ID_GLOBAL);
    guest_call(sub_003442B0,0);
    guest_call(sub_00086AE0,1,(uint32_t)deepest);   /* -> sub_000867F0 -> the readers */
    guest_call(sub_00344350,0);
    mapmaker_rooms_bk.depth--;
    WORD_AT(WORD_AT(MAPMAKER_ROOMS_VIEW_CURRENT)+0xD28u)=(uint32_t)deepest;
    MAPMAKER_GUEST_RET();
}

/* sub_00086480 / sub_000865E0 (eax = depth, plain ret): draw one depth's
 * rooms from the staging row, 255 per call. */
void pc_mapmaker_rooms_bucket_draw(void (*original)(void))
{
    uint32_t b=g_eax,ret=WORD_AT(g_esp),done=0,n,i;
    const mapmaker_rooms_depths *c;
    if(!mapmaker_rooms_bk.depth || b>=MAPMAKER_ROOMS_DEPTHS) {   /* not reached from sub_00086C40 */
        if(b<MAPMAKER_ROOMS_DEPTHS) *(uint8_t *)mapmaker_rooms_host(MAPMAKER_ROOMS_BUCKET_COUNTS+b)=0;
        original();
        return;
    }
    c=&mapmaker_rooms_bk.level[(mapmaker_rooms_bk.depth<=MAPMAKER_ROOMS_BUCKET_LEVELS?mapmaker_rooms_bk.depth:MAPMAKER_ROOMS_BUCKET_LEVELS)-1u];
    n=c->count[b];
    if(n>MAPMAKER_ROOMS_BUCKET_SEGMENT) mapmaker_rooms_bk.segmented++;
    do {
        uint32_t part=n-done>MAPMAKER_ROOMS_BUCKET_SEGMENT?MAPMAKER_ROOMS_BUCKET_SEGMENT:n-done;
        for(i=0;i<part;i++) mapmaker_rooms_set_u16(MAPMAKER_ROOMS_BUCKET_ROW+2u*i,c->room[c->start[b]+done+i]);
        *(uint8_t *)mapmaker_rooms_host(MAPMAKER_ROOMS_BUCKET_COUNTS+b)=(uint8_t)part;
        if(done) { g_esp-=4u;WORD_AT(g_esp)=ret; }  /* the same call again */
        g_eax=b;
        original();
        done+=part;
    } while(done<n);
}

/* sub_000897D0 (no arguments, plain ret): for every view with flag 0x20,
 * the rooms of depths [view+0xD28]..0 whose rectangle is on screen (x1 >= 0,
 * x0 < [0x004E8CF0], x1 > x0), then the rooms sub_00087720 lists for the
 * view's room (unless global), each once, below the capacity. The first
 * call only sets [0x0070877C]. */
extern void sub_00087720(void);
void pc_mapmaker_rooms_view_lists(void (*original)(void))
{
    static uint16_t first[MAPMAKER_ROOMS_DEPTHS+1u],order[MAPMAKER_ROOMS_BUCKET_ROOMS];
    uint32_t v;
    (void)original;
    if(!WORD_AT(0x0070877Cu)) { WORD_AT(0x0070877Cu)=1;MAPMAKER_GUEST_RET();return; }
    for(v=0;(int32_t)v<(int32_t)WORD_AT(MAPMAKER_ROOMS_VIEW_COUNT);v++) {
        uint32_t view=WORD_AT(MAPMAKER_ROOMS_VIEW_ARRAY)+v*MAPMAKER_ROOMS_VIEW_STRIDE,rooms,r,room,n,i,j;
        uint32_t fill[MAPMAKER_ROOMS_DEPTHS],list=view+MAPMAKER_ROOMS_VIEW_LIST;
        int32_t deepest,b,right=(int32_t)WORD_AT(0x004E8CF0u);
        if(!(*(uint8_t *)mapmaker_rooms_host(view+0x21Cu)&0x20u)) continue;
        mapmaker_rooms_set_u16(view+0xE08u,0);mapmaker_rooms_set_u16(view+0xE0Au,0);
        if((int32_t)WORD_AT(view+0xD28u)>=(int32_t)MAPMAKER_ROOMS_DEPTHS) WORD_AT(view+0xD28u)=MAPMAKER_ROOMS_DEPTHS-1u;
        deepest=(int32_t)WORD_AT(view+0xD28u);
        rooms=mapmaker_rooms_bucket_rooms();
        memset(first,0,sizeof first);
        for(r=1;r<=rooms;r++) {                      /* counting sort by depth, rooms ascending */
            int32_t d=mapmaker_rooms_rect_depth(view,r);
            if(d>=0 && d<(int32_t)MAPMAKER_ROOMS_DEPTHS) first[d+1]++;
        }
        for(b=0;b<(int32_t)MAPMAKER_ROOMS_DEPTHS;b++) { first[b+1]=(uint16_t)(first[b+1]+first[b]);fill[b]=first[b]; }
        for(r=1;r<=rooms;r++) {
            int32_t d=mapmaker_rooms_rect_depth(view,r);
            if(d>=0 && d<(int32_t)MAPMAKER_ROOMS_DEPTHS) order[fill[d]++]=(uint16_t)r;
        }
        for(b=deepest;b>=0;b--)
            for(i=first[b];i<first[b+1];i++) {
                int16_t x0,x1;
                uint16_t at;
                r=order[i];
                x1=mapmaker_rooms_rect_h(view,r,8u);
                if(x1<0) continue;
                x0=mapmaker_rooms_rect_h(view,r,4u);
                if((int32_t)x0>=right || x1<=x0) continue;
                at=mapmaker_rooms_u16(view+0xE08u);
                if(at>=MAPMAKER_ROOMS_VIEW_LIST_WORDS) continue;
                mapmaker_rooms_set_u16(list+2u*at,(uint16_t)r);
                mapmaker_rooms_set_u16(view+0xE08u,(uint16_t)(at+1u));
            }
        mapmaker_rooms_set_u16(view+0xE0Au,mapmaker_rooms_u16(view+0xE08u));
        room=WORD_AT(view+0x2A0u);
        if(room==MAPMAKER_ROOMS_ID_GLOBAL) continue;
        g_esp-=0x80u;                                 /* the original's 30-dword frame buffer */
        n=guest_call(sub_00087720,3,room,g_esp,0x1Eu);
        for(i=0;(int32_t)i<(int32_t)n && i<0x1Eu;i++) {
            uint32_t near_room=WORD_AT(g_esp+4u*i);
            int16_t count=(int16_t)mapmaker_rooms_u16(view+0xE08u);
            int found=0;
            for(j=0;(int32_t)j<count;j++)
                if((uint32_t)(int32_t)(int16_t)mapmaker_rooms_u16(list+2u*j)==near_room) { found=1;break; }
            if(found || count>=(int16_t)MAPMAKER_ROOMS_VIEW_LIST_CAP || count<0) continue;
            mapmaker_rooms_set_u16(list+2u*(uint32_t)count,(uint16_t)near_room);
            mapmaker_rooms_set_u16(view+0xE08u,(uint16_t)(count+1));
        }
        g_esp+=0x80u;
    }
    MAPMAKER_GUEST_RET();
}
