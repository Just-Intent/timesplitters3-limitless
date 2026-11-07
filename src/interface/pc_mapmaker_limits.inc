/* US 45410066 MapMaker capacity, stage A: AI navigation pools and editor
 * limits (research/capacity-constraints.md §1c).
 *
 * Pools (on by default when a MapMaker map is played, scene 100;
 * TSFP_MAPMAKER_POOLS=0 restores the original): sub_0009A140 allocates the
 * AI navigation pools from the Level heap with fixed sizes and nothing checks
 * them when pads are appended. Reimplemented here with 8000 regular pads
 * (x 32 B, plus 8000 x 44 B of per-pad AI data), 32,512 links (x 20 B) and
 * 10,496 cover pads (x 32 B); the 20 runtime pads are unchanged. Same order
 * and zeroing as the original; the memory comes from the nav block in the
 * runtime heap when pc_mapmaker_memory.inc provides it (2026-11-02), else
 * from the Level heap as the original. Current limits: the defines below,
 * which supersede the values in the next paragraphs. On by
 * default so that a map built with raised editor limits never overflows the
 * stock pools here.
 *
 * Cover pads cannot go past 1280 in play: in scene 100 sub_0009A420 decodes
 * handles 0x1F40..0x243F as cover pads and anything above as a regular pad,
 * and items and objects add cover pads at load time on top of the tiles'
 * count (60 on a 1278-cover test map), so the stock editor limit of 1024 is
 * kept. The larger pool keeps an overflow from writing over the runtime pads
 * (spawn points); handles above 1280 would still decode wrongly.
 *
 * Editor limits (opt-in, TSFP_MAPMAKER_LIMITS=1, only while the pools are on):
 * sub_002FA720 refuses placement (editor+0x25C bit 0x400000) when placed +
 * held pieces exceed pads 2400, cover pads 1024, links 4400, 200 pieces, the
 * memory estimate (editor+0x278 > 2,048,000), global pad groups 300 or 50
 * sub-entries, and writes the meter (editor+0x270, placed only) and the held
 * piece preview (editor+0x274) from the same limits. After the original runs,
 * the decision and both values are recomputed with pads 4800, links 12000,
 * pad groups 600, 500 items ("sub-entries"; cover pads stay 1024) and 1023
 * pieces (one visibility room each: the engine's 200-room limit is raised to
 * 1023 by config/guest-code-patches/rooms.json, see src/interface/pc_mapmaker_rooms.inc;
 * 255 until 2026-11-02, 511 until 2026-11-07);
 * memory (raised separately by TSFP_MAPMAKER_MEMORY_SCALE) keeps its limit.
 * The editor's item node pool is 5300 (config/guest-code-patches/items.json):
 * 500 items, their clipboard copies and up to 81 start points fit twice over.
 * In play items are Level-heap arrays by count (sub_00240590) and about one
 * engine object each (pool 2047, pc_mapmaker_objects.inc; the editor keeps
 * pieces + items + a 128-slot reserve within it, MAPMAKER_LIMIT_OBJECTS).
 *
 * Cover pads stay at the stock 1024: the map adds about 60 more when it loads
 * (measured: 60 on a 142-tile, 50-item map and on a 110-tile, 67-item map, so
 * not per item), and more than 1280 cannot work in play.
 *
 * Per-category maxima (writable .data table 0x00524BE0, 37 x 24 B, max at +4,
 * read by sub_00303460 place and sub_00303E10 paste) are raised while the
 * limits are on: pickups x4 (weapons, ammo, keycards, health, armour,
 * power-ups, special pickups) x4, keycards x8 (16 per colour) and movers,
 * props and markers x8 (160 each; round 3, all within the 500 item total and
 * the 1000-object pool; the slot-0/8 object lists drop and log past
 * 448/200, pc_mapmaker_objects.inc). Objects come from a
 * 550-slot engine pool taken without a bound check (203 in use with 30+ movers
 * on a 110-tile map). Turrets (categories 27-29, 8 each) stay at stock: each
 * live turret takes a gun and an owner struct from two 32-slot pools whose
 * allocator returns 0 when empty (sub_00245990, sub_000B3BE0), and joins a
 * 30-entry object list with no bound (sub_0009E650, 0x00716148). Teleporters stay at 2 per colour (sub_0023FD10 stores them in
 * two 10-slot stack arrays without a bound), and flags, start points and cover
 * props (category 33) are unchanged.
 *
 * Maps built past the stock limits must only be played in this build.
 * Requires the includer's WORD_AT, FLOAT_AT, guest_writable, guest_call,
 * save_registers/restore_registers, g_xbox_mem_offset and
 * mapmaker_budget_apply (pc_mapmaker_budget.inc). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Regular pads (2026-11-02, was 4800): ids are 0x3E9 + index and the 13
 * id-threshold compares now read ids up to 0x2328 as regular
 * (config/guest-code-patches/pads.json pads-idthreshold-*), and regular
 * handles are the index itself (< 0x1F40), so 8000 regular pads decode in
 * play. The pool holds all 8000; the editor keeps 320 spare and allows 7680,
 * 511 pieces of the heaviest tile (14 pads) need 7154. */
#define MAPMAKER_LIMIT_PADS 7680u
#define MAPMAKER_POOL_PADS 8000u      /* regular handles 0..0x1F3F, ids 0x3E9..0x2328 */
/* Cover pads: the runtime pad handles are moved above a 10,240-entry cover
 * handle range (config/guest-code-patches/pads.json, docs/mapmaker/mapmaker-pads.md);
 * the editor keeps 256 spare as stock did (1280 - 1024) for the start-point
 * pads a map adds at load (measured 60). */
#define MAPMAKER_LIMIT_COVER 9984u    /* 10240 cover handles - 256 */
#define MAPMAKER_POOL_COVER 10496u    /* 10240 cover handles + 256 guard */
/* Links (2026-11-02; 12000 template links before, pool 16,336): the A*
 * nodes and routes hold link indices as int16, so the scene-100 pool is fixed
 * at 32,512 entries (indices < 32,767). Play adds 16 stitch links per piece
 * (sub_0009DC70, table of 16 per piece from sub_0009A0F0) and teleporter and
 * door links (sub_0009CD00, the 256 slack), so the editor limits template
 * links + 16 per piece to 32,256: 511 pieces of the heaviest tile (46 links)
 * need 23,506 + 8,176 = 31,682, and 1024 pieces fit with up to 15 links each
 * (docs/mapmaker/mapmaker-pads.md). */
#define MAPMAKER_POOL_LINKS 32512u
#define MAPMAKER_LINK_STITCH 16u
#define MAPMAKER_LINK_SLACK 256u
#define MAPMAKER_LIMIT_LINKS (MAPMAKER_POOL_LINKS-MAPMAKER_LINK_SLACK)  /* template + 16 per piece */
/* Pad groups (2026-11-02, was 600): measured 1.5 per piece, so 511 pieces
 * need about 770. Group ids are int16 (pad+8), the group A* node pool is
 * bounded (pc_mapmaker_pads.inc, 8192 nodes) and the group table is sized by
 * count; the N x N distance matrix sub_00275F70 builds is (N+1) x N x 4 B,
 * 4.2 MB at 1024, and takes N x N route searches at load (unmeasured). */
#define MAPMAKER_LIMIT_GROUPS 1024
/* Pieces (2026-11-07, was 511; 255 before 2026-11-02): one visibility room
 * each, so = MAPMAKER_ROOMS_MAX (rooms.json real rooms 1..0x3FF). Each piece is
 * also one engine object; since 2026-11-07 handles are slot << 21
 * (config/guest-code-patches/handles.json) and the pool holds 2047, so
 * 1023 pieces + 500 items + 128 = 1651 fit and the objects cap no longer
 * binds before pieces or items (it did at 895 - items with the 1023-slot
 * pool of the slot << 22 encoding). The editor piece pool is 2048 nodes
 * (items.json), so a copy of every placed piece still fits. Heavy tiles stop
 * on links (521), pads (549) or the memory estimate (386 at scale 8). */
#define MAPMAKER_LIMIT_PIECES 1023u
#define MAPMAKER_LIMIT_SUB 500u      /* items incl. free objects; node pool 5300 (items.json) */
#define MAPMAKER_LIMIT_MEMORY 2048000u
/* Engine objects (2026-11-02): in play each piece (room) and about each item
 * takes one slot of the engine object pool (pc_mapmaker_objects.inc, 2047
 * slots since 2026-11-07, 1023 before; taken without a bound check; 462 used
 * at 236 pieces + 200 items), characters up to 64 more. The editor refuses
 * placement past the pool; with 2047 slots the piece (1023) and item (500)
 * caps are reached first (1651 objects), so tiles no longer take items away.
 * The reserve (2026-11-03, v0.4.0-mapmaker.37: 459 pieces + about 500 items
 * filled all 1023 slots in play and shot barrels did nothing) is 64
 * characters plus 64 for what explodes and breaks: sub_000AB590 refuses a
 * flag-8 object (debris, fragments) unless 5 slots stay free (sub_000A4C70),
 * flag-8 props live in a 30-block ring (sub_000AC940) and broken props keep
 * up to 20 debris pieces (0x00732688). */
#define MAPMAKER_LIMIT_OBJECTS 2047u  /* = MAPMAKER_OBJECTS_POOL */
#define MAPMAKER_OBJECTS_CHARACTERS 64u /* characters in MapMaker story play */
#define MAPMAKER_OBJECTS_EFFECTS 64u    /* >= 30 ring + 20 debris + 5 refusal margin */
#define MAPMAKER_OBJECTS_RESERVE (MAPMAKER_OBJECTS_CHARACTERS+MAPMAKER_OBJECTS_EFFECTS)
typedef char mapmaker_limits_effects_reserve[(MAPMAKER_OBJECTS_EFFECTS>=30u+20u+5u)?1:-1];
/* Regular pads: the pool fits the handle range and every pool id stays at or
 * below the raised id threshold 0x2328. */
typedef char mapmaker_limits_pad_ids[(MAPMAKER_POOL_PADS<=0x1F40u && 0x3E9u+MAPMAKER_POOL_PADS-1u<=0x2328u &&
                                      MAPMAKER_LIMIT_PADS<=MAPMAKER_POOL_PADS)?1:-1];
/* Links: every pool index fits the int16 A* indices. */
typedef char mapmaker_limits_link_pool[(MAPMAKER_POOL_LINKS<=0x7FFFu &&
                                        MAPMAKER_LIMIT_LINKS+MAPMAKER_LINK_SLACK<=MAPMAKER_POOL_LINKS)?1:-1];

static struct { int initialized, pools, limits; } mapmaker_limits;

/* A hook that replaces its original (does not call it) must also do the
 * original's `ret`: the lifted caller pushes the return address and expects
 * the callee to pop it (RECOMP_ABI_CALL_POP). Every replaced MapMaker
 * function ends in a plain `ret` (checked in text.asm), so pop 4. */
#define MAPMAKER_GUEST_RET() (g_esp+=4u)

/* The editor's limit check runs every frame over every piece and item, so
 * its pointer checks must be cheap: guest RAM (0x00010000..0x03FFFFFF) or the
 * contiguous window (0x80000000..0x83FFFFFF), the only places the game keeps
 * these structures. guest_writable asks Windows (VirtualQuery) every time,
 * which made a 186-piece map's editor visibly slow. */
#define MAPMAKER_PTR_OK(p,n) ((((uint32_t)(p))>=0x00010000u && ((uint32_t)(p))<0x04000000u-(uint32_t)(n)) || \
                              (((uint32_t)(p))>=0x80000000u && ((uint32_t)(p))<0x84000000u-(uint32_t)(n)))

/* Category maxima: stock values read from the US XBE; see research/capacity-raise-plan.md. */
static const struct { uint32_t address, stock, raised; } mapmaker_categories[]={
    {0x00524CA4u,32u,128u},  /* 8  weapon pickups */
    {0x00524CBCu,32u,128u},  /* 9  ammo */
    {0x00524CD4u,2u,16u},    /* 10 keycard red: pickup records by count, no pool of their own */
    {0x00524CECu,2u,16u},    /* 11 keycard blue */
    {0x00524D04u,2u,16u},    /* 12 keycard yellow */
    {0x00524D1Cu,2u,16u},    /* 13 keycard green */
    {0x00524D34u,2u,8u},     /* 14-18 teleporters: sub_0009CD00 wrapper (pc_mapmaker_objects.inc) */
    {0x00524D4Cu,2u,8u},
    {0x00524D64u,2u,8u},
    {0x00524D7Cu,2u,8u},
    {0x00524D94u,2u,8u},
    {0x00524E6Cu,8u,24u},    /* 27-29 turrets: 72 <= the 80 gun/owner slots (objects.json) */
    {0x00524E84u,8u,24u},
    {0x00524E9Cu,8u,24u},
    {0x00524EFCu,66u,132u},  /* 33 cover props: no cover pads added at load (only starts, sub_0023F9D0) */
    {0x00524DACu,16u,64u},   /* 19 health */
    {0x00524DC4u,16u,64u},   /* 20 armour */
    {0x00524DDCu,12u,48u},   /* 21 power-ups */
    {0x00524ECCu,32u,128u},  /* 31 special pickups */
    {0x00524E3Cu,20u,160u},  /* 25 mover (object) */
    {0x00524E54u,20u,160u},  /* 26 props (objects) */
    {0x00524EE4u,20u,160u},  /* 32 marker */
    {0x00524F2Cu,4u,8u},     /* 35 */
    {0x00524F44u,4u,8u},     /* 36 */
};
static void mapmaker_categories_apply(void)
{
    unsigned i;
    for(i=0;i<sizeof mapmaker_categories/sizeof mapmaker_categories[0];i++) {
        uint32_t a=mapmaker_categories[i].address;
        if(MAPMAKER_PTR_OK(a,4) && WORD_AT(a)==mapmaker_categories[i].stock)
            WORD_AT(a)=mapmaker_categories[i].raised;
    }
}

static void mapmaker_limits_read(void)
{
    const char *p,*l;
    if(mapmaker_limits.initialized) return;
    p=getenv("TSFP_MAPMAKER_POOLS");l=getenv("TSFP_MAPMAKER_LIMITS");
    mapmaker_limits.initialized=1;
    mapmaker_limits.pools=!(p && p[0]=='0' && !p[1]);
    /* Raised editor limits are only safe with the raised pools. */
    mapmaker_limits.limits=mapmaker_limits.pools && l && l[0]=='1' && !l[1];
}

/* ---- pools ------------------------------------------------------------ */
extern void sub_0003DFA0(void);
/* Where the pools come from first: the MapMaker nav block in the runtime heap
 * (pc_mapmaker_memory.inc registers it once its level-reset hook has run;
 * 0 = no room or not registered), else the Level heap as the original. */
#define MAPMAKER_POOL_SOURCE 1
static uint32_t (*mapmaker_pool_source)(uint32_t bytes);
static uint32_t mapmaker_pool(uint32_t bytes)
{
    uint32_t p=mapmaker_pool_source?mapmaker_pool_source(bytes):0u;
    if(!p) p=guest_call(sub_0003DFA0,2,bytes,1u); /* Level heap, as the original */
    if(guest_writable(p,bytes)) memset((void *)((uintptr_t)p+g_xbox_mem_offset),0,bytes);
    return p;
}
void pc_display_mapmaker_pools(void (*original)(void))
{
    GuestRegisters r;
    mapmaker_limits_read();
    if(!mapmaker_limits.pools || !guest_writable(0x0079094Cu,4) || WORD_AT(0x0079094Cu)!=100u) {
        original();
        return;
    }
    save_registers(&r);
    WORD_AT(0x00715F2Cu)=mapmaker_pool(MAPMAKER_POOL_PADS*32u);
    /* Links: template links, one per stitch entry (table of 16 per piece) and
     * a few for objects, all within the int16 index range. */
    WORD_AT(0x00715F30u)=mapmaker_pool(MAPMAKER_POOL_LINKS*20u);
    if((WORD_AT(0x007DE454u)>>8&0xFFu)!=10u)                 /* byte [0x007DE455] */
        WORD_AT(0x00715F3Cu)=mapmaker_pool(MAPMAKER_POOL_PADS*44u);
    WORD_AT(0x00715F34u)=mapmaker_pool(MAPMAKER_POOL_COVER*32u);
    WORD_AT(0x00715F38u)=mapmaker_pool(0x280u);
    WORD_AT(0x007B6664u)=0;WORD_AT(0x007B6660u)=0;WORD_AT(0x007B65C0u)=0;WORD_AT(0x007B6674u)=0;
    WORD_AT(0x007B6778u)=0;WORD_AT(0x007B65BCu)=0;WORD_AT(0x007B6780u)=0;WORD_AT(0x007B666Cu)=0;
    restore_registers(&r);
    MAPMAKER_GUEST_RET();
}

/* ---- editor limits ---------------------------------------------------- */
typedef struct MapMakerTotals { uint32_t cost[3], pieces, sub; } MapMakerTotals;
static void mapmaker_list_totals(uint32_t piece,MapMakerTotals *t)
{
    unsigned i,j;
    memset(t,0,sizeof *t);
    for(i=0;piece && i<4096 && MAPMAKER_PTR_OK(piece,0x44);i++,piece=WORD_AT(piece+0x40)) {
        uint32_t tp=WORD_AT(piece+0x2c),sub;
        t->pieces++;
        if(MAPMAKER_PTR_OK(tp,4) && WORD_AT(tp)<1024u) {
            uint32_t row=0x00529400u+WORD_AT(tp)*0x140u;
            if(MAPMAKER_PTR_OK(row,12)) { t->cost[0]+=WORD_AT(row);t->cost[1]+=WORD_AT(row+4);t->cost[2]+=WORD_AT(row+8); }
        }
        sub=WORD_AT(piece+0x3c);
        for(j=0;sub && j<4096 && MAPMAKER_PTR_OK(sub,0x18);j++,sub=WORD_AT(sub+0x14))
            if((int32_t)WORD_AT(sub)>9) t->sub++;
    }
}
/* sub_002FA060 for the placed list, with a fresh seen table as the original
 * has at that point (it clears 0x00762AE0 first), then the budget scale. */
static uint32_t mapmaker_placed_memory(uint32_t piece)
{
    unsigned char seen[80];
    uint32_t total=0;
    unsigned i,j;
    memset(seen,0,sizeof seen);
    for(i=0;piece && i<4096 && MAPMAKER_PTR_OK(piece,0x44);i++,piece=WORD_AT(piece+0x40)) {
        uint32_t tp=WORD_AT(piece+0x2c),sub;
        if(MAPMAKER_PTR_OK(tp,4) && WORD_AT(tp)<1024u && MAPMAKER_PTR_OK(0x00529410u+WORD_AT(tp)*0x140u,4))
            total+=WORD_AT(0x00529410u+WORD_AT(tp)*0x140u);
        sub=WORD_AT(piece+0x3c);
        for(j=0;sub && j<4096 && MAPMAKER_PTR_OK(sub,0x18);j++,sub=WORD_AT(sub+0x14)) {
            uint32_t id=WORD_AT(sub);
            if(id<80u && seen[id] && id!=0x45u) total+=0x400u;
            else {
                if(id<80u) seen[id]=1;
                if(id!=0x45u && (int32_t)id>9) total+=0xA000u;
            }
        }
    }
    return mapmaker_budget_apply(total);
}
/* Links a list costs in play: its template links and the stitch links. */
static uint32_t mapmaker_link_cost(const MapMakerTotals *t)
{
    return t->cost[2]+MAPMAKER_LINK_STITCH*t->pieces;
}
/* Which limits placed + held content passes (0: placement allowed). */
enum { MAPMAKER_CAP_PADS=1u, MAPMAKER_CAP_COVER=2u, MAPMAKER_CAP_LINKS=4u, MAPMAKER_CAP_PIECES=8u,
       MAPMAKER_CAP_MEMORY=16u, MAPMAKER_CAP_GROUPS=32u, MAPMAKER_CAP_SUB=64u, MAPMAKER_CAP_OBJECTS=128u };
/* Engine objects the content takes in play: one per piece and per item, plus
 * the character reserve. */
static uint32_t mapmaker_objects_needed(uint32_t pieces,uint32_t sub)
{
    return pieces+sub+MAPMAKER_OBJECTS_RESERVE;
}
static unsigned mapmaker_limits_over(const MapMakerTotals *p,const MapMakerTotals *h,uint32_t memory,
                                     int32_t groups,uint32_t sub)
{
    unsigned over=0;
    if(p->cost[0]+h->cost[0]>MAPMAKER_LIMIT_PADS) over|=MAPMAKER_CAP_PADS;
    if(p->cost[1]+h->cost[1]>MAPMAKER_LIMIT_COVER) over|=MAPMAKER_CAP_COVER;
    if(mapmaker_link_cost(p)+mapmaker_link_cost(h)>MAPMAKER_LIMIT_LINKS) over|=MAPMAKER_CAP_LINKS;
    if(p->pieces+h->pieces>MAPMAKER_LIMIT_PIECES) over|=MAPMAKER_CAP_PIECES;
    if(memory>MAPMAKER_LIMIT_MEMORY) over|=MAPMAKER_CAP_MEMORY;
    if(groups>MAPMAKER_LIMIT_GROUPS) over|=MAPMAKER_CAP_GROUPS;
    if(sub>MAPMAKER_LIMIT_SUB) over|=MAPMAKER_CAP_SUB;
    if(mapmaker_objects_needed(p->pieces+h->pieces,sub)>MAPMAKER_LIMIT_OBJECTS) over|=MAPMAKER_CAP_OBJECTS;
    return over;
}
/* The refusal reason, logged once each time the set of limits passed changes
 * (the check runs every frame): every limit placed + held content passes, the
 * first-named one first in the order the meter lists them. */
static void mapmaker_limits_log(unsigned over,const MapMakerTotals *p,const MapMakerTotals *h,
                                uint32_t memory,int32_t groups,uint32_t sub)
{
    static unsigned last;
    if(over==last) return;
    last=over;
    if(!over) return;
    fprintf(stderr,"[MAPMAKER] placement refused:");
    if(over&MAPMAKER_CAP_PADS) fprintf(stderr," pads %u of %u;",(unsigned)(p->cost[0]+h->cost[0]),MAPMAKER_LIMIT_PADS);
    if(over&MAPMAKER_CAP_COVER) fprintf(stderr," cover pads %u of %u;",(unsigned)(p->cost[1]+h->cost[1]),MAPMAKER_LIMIT_COVER);
    if(over&MAPMAKER_CAP_LINKS) fprintf(stderr," links %u of %u;",(unsigned)(mapmaker_link_cost(p)+mapmaker_link_cost(h)),
                                        (unsigned)MAPMAKER_LIMIT_LINKS);
    if(over&MAPMAKER_CAP_GROUPS) fprintf(stderr," pad groups %d of %d;",(int)groups,MAPMAKER_LIMIT_GROUPS);
    if(over&MAPMAKER_CAP_PIECES) fprintf(stderr," pieces %u of %u;",(unsigned)(p->pieces+h->pieces),MAPMAKER_LIMIT_PIECES);
    if(over&MAPMAKER_CAP_SUB) fprintf(stderr," items %u of %u;",(unsigned)sub,MAPMAKER_LIMIT_SUB);
    if(over&MAPMAKER_CAP_MEMORY) fprintf(stderr," memory estimate %u of %u at scale %u (TSFP_MAPMAKER_ARENA_MB raises it);",
                                         (unsigned)memory,MAPMAKER_LIMIT_MEMORY,mapmaker_budget_scale());
    if(over&MAPMAKER_CAP_OBJECTS) fprintf(stderr," engine objects %u of %u (pieces + items + %u reserved);",
                                          (unsigned)mapmaker_objects_needed(p->pieces+h->pieces,sub),MAPMAKER_LIMIT_OBJECTS,
                                          MAPMAKER_OBJECTS_RESERVE);
    fputc('\n',stderr);
}
/* What the last limit check saw, for the editor's resource table
 * (pc_mapmaker_restable.inc): the placed counts the meter divides and the
 * caps placed + held content passes. valid = 0 until the check has run with
 * the raised limits on; the table then shows the stock meter instead. */
typedef struct MapMakerLimitsView {
    int valid;
    uint32_t editor, pieces, items, objects, pads, cover, links, memory;
    int32_t groups;
    unsigned over;
} MapMakerLimitsView;
static MapMakerLimitsView mapmaker_limits_view;
static float mapmaker_fraction_max(const float *f,unsigned n)
{
    float m=0;
    unsigned i;
    for(i=0;i<n;i++) if(f[i]>m) m=f[i];
    return m;
}
void pc_display_mapmaker_limits(void (*original)(void))
{
    uint32_t editor,placed_head,held_head,flags,placed_mem,total_mem,extra;
    int32_t groups,held_id;
    MapMakerTotals p,h;
    float f[8];
    int refuse;
    original();
    mapmaker_limits_read();
    if(!mapmaker_limits.limits) return;
    mapmaker_categories_apply();
    mapmaker_limits_view.valid=0;                 /* until this check completes */
    if(!MAPMAKER_PTR_OK(0x007844A8u,4) || !MAPMAKER_PTR_OK(0x0078400Cu,4) ||
       !MAPMAKER_PTR_OK(0x00762CB8u,4)) return;
    editor=WORD_AT(0x007844A8u);
    if(!MAPMAKER_PTR_OK(editor,0x3074)) return;
    placed_head=WORD_AT(0x0078400Cu);held_head=WORD_AT(editor+0x2f7c);
    mapmaker_list_totals(placed_head,&p);mapmaker_list_totals(held_head,&h);
    groups=(int32_t)WORD_AT(0x00762CB8u);
    extra=WORD_AT(editor+0x306e)&0xFFFFu;
    held_id=(int32_t)WORD_AT(editor+0x2f34);
    placed_mem=mapmaker_placed_memory(placed_head);
    total_mem=WORD_AT(editor+0x278);              /* written by the original */
    /* Meter (placed only), as the first part of the original. */
    f[0]=(float)p.cost[0]/MAPMAKER_LIMIT_PADS;f[1]=(float)p.cost[1]/MAPMAKER_LIMIT_COVER;
    f[2]=(float)mapmaker_link_cost(&p)/MAPMAKER_LIMIT_LINKS;f[3]=(float)groups/MAPMAKER_LIMIT_GROUPS;
    f[4]=(float)p.pieces/MAPMAKER_LIMIT_PIECES;f[5]=(float)(p.sub+extra)/MAPMAKER_LIMIT_SUB;
    f[6]=(float)placed_mem/MAPMAKER_LIMIT_MEMORY;
    f[7]=(float)mapmaker_objects_needed(p.pieces,p.sub+extra)/MAPMAKER_LIMIT_OBJECTS;
    FLOAT_AT(editor+0x270)=mapmaker_fraction_max(f,8);
    /* Held piece preview. */
    f[0]=(float)h.cost[0]/MAPMAKER_LIMIT_PADS;f[1]=(float)h.cost[1]/MAPMAKER_LIMIT_COVER;
    f[2]=(float)mapmaker_link_cost(&h)/MAPMAKER_LIMIT_LINKS;f[3]=(float)h.pieces/MAPMAKER_LIMIT_PIECES;
    f[4]=(float)(h.sub+(held_id>=0?1u:0u))/MAPMAKER_LIMIT_SUB;
    f[5]=(float)(total_mem>placed_mem?total_mem-placed_mem:0u)/MAPMAKER_LIMIT_MEMORY;
    FLOAT_AT(editor+0x274)=mapmaker_fraction_max(f,6);
    /* Placement decision over placed + held. */
    {
        uint32_t sub=p.sub+h.sub+(held_id>9?1u:0u)+extra;
        unsigned over=mapmaker_limits_over(&p,&h,total_mem,groups,sub);
        mapmaker_limits_log(over,&p,&h,total_mem,groups,sub);
        refuse=over!=0;
        mapmaker_limits_view.pieces=p.pieces;mapmaker_limits_view.items=p.sub+extra;
        mapmaker_limits_view.objects=mapmaker_objects_needed(p.pieces,p.sub+extra);
        mapmaker_limits_view.pads=p.cost[0];mapmaker_limits_view.cover=p.cost[1];
        mapmaker_limits_view.links=mapmaker_link_cost(&p);mapmaker_limits_view.memory=placed_mem;
        mapmaker_limits_view.groups=groups;mapmaker_limits_view.over=over;
        mapmaker_limits_view.editor=editor;mapmaker_limits_view.valid=1;
    }
    flags=WORD_AT(editor+0x25c);
    WORD_AT(editor+0x25c)=refuse?(flags|0x400000u):(flags&~0x400000u);
}
