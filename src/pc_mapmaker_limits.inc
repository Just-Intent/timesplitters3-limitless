/* US 45410066 MapMaker capacity, stage A: AI navigation pools and editor
 * limits (research/capacity-constraints.md §1c).
 *
 * Pools (on by default when a MapMaker map is played, scene 100;
 * TSFP_MAPMAKER_POOLS=0 restores the original): sub_0009A140 allocates the
 * AI navigation pools from the Level heap with fixed sizes and nothing checks
 * them when pads are appended. Reimplemented here with twice the regular
 * pads (4800 x 32 B, plus 4800 x 44 B of per-pad AI data) and links
 * (8800 x 20 B); the cover pool grows from 1280 to 1536 x 32 B as a guard
 * only, and the 20 runtime pads are unchanged. Same heap, order and zeroing
 * as the original; about 300 KB more of the ~12 MB measured free. On by
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
 * the decision and both values are recomputed with pads 4800, links 8800,
 * pad groups 600 and 200 items ("sub-entries"; cover pads stay 1024); pieces
 * and memory (raised separately by TSFP_MAPMAKER_MEMORY_SCALE) keep their
 * limits. The editor's item node pool is 2120 (config/guest-code-patches.json).
 *
 * Items add cover pads when the map loads (60 for 50 items on a test map), and
 * the stock 1024 limit only leaves 256 spare below the 1280 that can work in
 * play, so every item past the stock 50 is charged MAPMAKER_ITEM_COVER cover
 * pads in the decision and the meter. Validate with probe flag 64 (cover in
 * play) before lowering it.
 *
 * Per-category maxima (writable .data table 0x00524BE0, 37 x 24 B, max at +4,
 * read by sub_00303460 place and sub_00303E10 paste) are raised while the
 * limits are on: pickups x4 (weapons, ammo, keycards, health, armour,
 * power-ups, special pickups) and object-making categories x2 (movers, props,
 * turrets), because objects come from a 550-slot engine pool taken without a
 * bound check. Teleporters stay at 2 per colour (sub_0023FD10 stores them in
 * two 10-slot stack arrays without a bound), and flags, start points and cover
 * props (category 33) are unchanged.
 *
 * Maps built past the stock limits must only be played in this build.
 * Requires the includer's WORD_AT, FLOAT_AT, guest_writable, guest_call,
 * save_registers/restore_registers, g_xbox_mem_offset and
 * mapmaker_budget_apply (pc_mapmaker_budget.inc). */
#include <stdlib.h>
#include <string.h>

#define MAPMAKER_LIMIT_PADS 4800u
#define MAPMAKER_LIMIT_COVER 1024u    /* stock: see above */
#define MAPMAKER_POOL_COVER 1536u     /* stock 1280, plus a guard */
#define MAPMAKER_LIMIT_LINKS 8800u
#define MAPMAKER_LIMIT_GROUPS 600
#define MAPMAKER_LIMIT_PIECES 200u
#define MAPMAKER_LIMIT_SUB 200u
#define MAPMAKER_STOCK_SUB 50u
#define MAPMAKER_ITEM_COVER 2u      /* cover pads charged per item past the stock 50 */
#define MAPMAKER_LIMIT_MEMORY 2048000u

static struct { int initialized, pools, limits; } mapmaker_limits;

/* Category maxima: stock values read from the US XBE; see research/capacity-raise-plan.md. */
static const struct { uint32_t address, stock, raised; } mapmaker_categories[]={
    {0x00524CA4u,32u,128u},  /* 8  weapon pickups */
    {0x00524CBCu,32u,128u},  /* 9  ammo */
    {0x00524CD4u,2u,8u},     /* 10 keycard red */
    {0x00524CECu,2u,8u},     /* 11 keycard blue */
    {0x00524D04u,2u,8u},     /* 12 keycard yellow */
    {0x00524D1Cu,2u,8u},     /* 13 keycard green */
    {0x00524DACu,16u,64u},   /* 19 health */
    {0x00524DC4u,16u,64u},   /* 20 armour */
    {0x00524DDCu,12u,48u},   /* 21 power-ups */
    {0x00524ECCu,32u,128u},  /* 31 special pickups */
    {0x00524E3Cu,20u,40u},   /* 25 mover (object) */
    {0x00524E54u,20u,40u},   /* 26 props (objects) */
    {0x00524EE4u,20u,40u},   /* 32 marker */
    {0x00524E6Cu,8u,16u},    /* 27 turrets (objects) */
    {0x00524E84u,8u,16u},    /* 28 */
    {0x00524E9Cu,8u,16u},    /* 29 */
    {0x00524F2Cu,4u,8u},     /* 35 */
    {0x00524F44u,4u,8u},     /* 36 */
};
static void mapmaker_categories_apply(void)
{
    unsigned i;
    for(i=0;i<sizeof mapmaker_categories/sizeof mapmaker_categories[0];i++) {
        uint32_t a=mapmaker_categories[i].address;
        if(guest_writable(a,4) && WORD_AT(a)==mapmaker_categories[i].stock)
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
static uint32_t mapmaker_pool(uint32_t bytes)
{
    uint32_t p=guest_call(sub_0003DFA0,2,bytes,1u); /* Level heap, as the original */
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
    WORD_AT(0x00715F2Cu)=mapmaker_pool(MAPMAKER_LIMIT_PADS*32u);
    WORD_AT(0x00715F30u)=mapmaker_pool(MAPMAKER_LIMIT_LINKS*20u);
    if((WORD_AT(0x007DE454u)>>8&0xFFu)!=10u)                 /* byte [0x007DE455] */
        WORD_AT(0x00715F3Cu)=mapmaker_pool(MAPMAKER_LIMIT_PADS*44u);
    WORD_AT(0x00715F34u)=mapmaker_pool(MAPMAKER_POOL_COVER*32u);
    WORD_AT(0x00715F38u)=mapmaker_pool(0x280u);
    WORD_AT(0x007B6664u)=0;WORD_AT(0x007B6660u)=0;WORD_AT(0x007B65C0u)=0;WORD_AT(0x007B6674u)=0;
    WORD_AT(0x007B6778u)=0;WORD_AT(0x007B65BCu)=0;WORD_AT(0x007B6780u)=0;WORD_AT(0x007B666Cu)=0;
    restore_registers(&r);
}

/* ---- editor limits ---------------------------------------------------- */
typedef struct MapMakerTotals { uint32_t cost[3], pieces, sub; } MapMakerTotals;
static void mapmaker_list_totals(uint32_t piece,MapMakerTotals *t)
{
    unsigned i,j;
    memset(t,0,sizeof *t);
    for(i=0;piece && i<4096 && guest_writable(piece,0x44);i++,piece=WORD_AT(piece+0x40)) {
        uint32_t tp=WORD_AT(piece+0x2c),sub;
        t->pieces++;
        if(guest_writable(tp,4) && WORD_AT(tp)<1024u) {
            uint32_t row=0x00529400u+WORD_AT(tp)*0x140u;
            if(guest_writable(row,12)) { t->cost[0]+=WORD_AT(row);t->cost[1]+=WORD_AT(row+4);t->cost[2]+=WORD_AT(row+8); }
        }
        sub=WORD_AT(piece+0x3c);
        for(j=0;sub && j<4096 && guest_writable(sub,0x18);j++,sub=WORD_AT(sub+0x14))
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
    for(i=0;piece && i<4096 && guest_writable(piece,0x44);i++,piece=WORD_AT(piece+0x40)) {
        uint32_t tp=WORD_AT(piece+0x2c),sub;
        if(guest_writable(tp,4) && WORD_AT(tp)<1024u && guest_writable(0x00529410u+WORD_AT(tp)*0x140u,4))
            total+=WORD_AT(0x00529410u+WORD_AT(tp)*0x140u);
        sub=WORD_AT(piece+0x3c);
        for(j=0;sub && j<4096 && guest_writable(sub,0x18);j++,sub=WORD_AT(sub+0x14)) {
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
static float mapmaker_fraction_max(const float *f,unsigned n)
{
    float m=0;
    unsigned i;
    for(i=0;i<n;i++) if(f[i]>m) m=f[i];
    return m;
}
void pc_display_mapmaker_limits(void (*original)(void))
{
    uint32_t editor,placed_head,held_head,flags,placed_mem,total_mem,extra,items,cover;
    int32_t groups,held_id;
    MapMakerTotals p,h;
    float f[7];
    int refuse;
    original();
    mapmaker_limits_read();
    if(!mapmaker_limits.limits) return;
    mapmaker_categories_apply();
    if(!guest_writable(0x007844A8u,4) || !guest_writable(0x0078400Cu,4) ||
       !guest_writable(0x00762CB8u,4)) return;
    editor=WORD_AT(0x007844A8u);
    if(!guest_writable(editor,0x3074)) return;
    placed_head=WORD_AT(0x0078400Cu);held_head=WORD_AT(editor+0x2f7c);
    mapmaker_list_totals(placed_head,&p);mapmaker_list_totals(held_head,&h);
    groups=(int32_t)WORD_AT(0x00762CB8u);
    extra=WORD_AT(editor+0x306e)&0xFFFFu;
    held_id=(int32_t)WORD_AT(editor+0x2f34);
    placed_mem=mapmaker_placed_memory(placed_head);
    total_mem=WORD_AT(editor+0x278);              /* written by the original */
    /* Meter (placed only), as the first part of the original. */
    items=p.sub+extra;
    cover=p.cost[1]+MAPMAKER_ITEM_COVER*(items>MAPMAKER_STOCK_SUB?items-MAPMAKER_STOCK_SUB:0u);
    f[0]=(float)p.cost[0]/MAPMAKER_LIMIT_PADS;f[1]=(float)cover/MAPMAKER_LIMIT_COVER;
    f[2]=(float)p.cost[2]/MAPMAKER_LIMIT_LINKS;f[3]=(float)groups/MAPMAKER_LIMIT_GROUPS;
    f[4]=(float)p.pieces/MAPMAKER_LIMIT_PIECES;f[5]=(float)(p.sub+extra)/MAPMAKER_LIMIT_SUB;
    f[6]=(float)placed_mem/MAPMAKER_LIMIT_MEMORY;
    FLOAT_AT(editor+0x270)=mapmaker_fraction_max(f,7);
    /* Held piece preview. */
    f[0]=(float)h.cost[0]/MAPMAKER_LIMIT_PADS;f[1]=(float)h.cost[1]/MAPMAKER_LIMIT_COVER;
    f[2]=(float)h.cost[2]/MAPMAKER_LIMIT_LINKS;f[3]=(float)h.pieces/MAPMAKER_LIMIT_PIECES;
    f[4]=(float)(h.sub+(held_id>=0?1u:0u))/MAPMAKER_LIMIT_SUB;
    f[5]=(float)(total_mem>placed_mem?total_mem-placed_mem:0u)/MAPMAKER_LIMIT_MEMORY;
    FLOAT_AT(editor+0x274)=mapmaker_fraction_max(f,6);
    /* Placement decision over placed + held. */
    items=p.sub+h.sub+(held_id>9?1u:0u)+extra;
    cover=p.cost[1]+h.cost[1]+MAPMAKER_ITEM_COVER*(items>MAPMAKER_STOCK_SUB?items-MAPMAKER_STOCK_SUB:0u);
    refuse=p.cost[0]+h.cost[0]>MAPMAKER_LIMIT_PADS || cover>MAPMAKER_LIMIT_COVER ||
        p.cost[2]+h.cost[2]>MAPMAKER_LIMIT_LINKS || p.pieces+h.pieces>MAPMAKER_LIMIT_PIECES ||
        total_mem>MAPMAKER_LIMIT_MEMORY || groups>MAPMAKER_LIMIT_GROUPS ||
        items>MAPMAKER_LIMIT_SUB;
    flags=WORD_AT(editor+0x25c);
    WORD_AT(editor+0x25c)=refuse?(flags|0x400000u):(flags&~0x400000u);
}
