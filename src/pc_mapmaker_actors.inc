/* US 45410066 MapMaker capacity, actors area: how many characters can be
 * alive at once in a MapMaker map in play (scene 100). See
 * docs/mapmaker-actors.md for the inventory and the evidence.
 *
 * The engine is built for 32 characters: a 32-slot actor pool (0xEA0 B each,
 * sub_000D6960), a 32-entry live character list (0x007A3580, count
 * [0x0074C37C]), a 32-character pair table in story mode (sub_0015E7C0),
 * 32 character entries in every player's perception array, 32-bit
 * per-character masks and 30-entry stack arrays. AI take slots
 * humans..humans+29 (the 30-slot cap in sub_0015DF20, table 0x007A2D00[30]).
 * Going past 32 is not safe in this pass (see the doc); what this area does
 * is let a MapMaker map use all of it.
 *
 * In play, the per-frame spawner sub_00164010 holds every placed AI back
 * while about 20 characters are alive: at 0x001640EC (every scene) only
 * records with flag 0x20000 spawn once the count reaches 20, and at
 * 0x001641A2 (scene 100 only) nothing spawns once it passes 20. Both
 * comparisons read the count through a gate dword instead
 * (config/guest-code-patches/actors.json):
 *
 * - pc_mapmaker_actors_spawn_gate (sub_00163F10, the spawner's per-record
 *   check, called right before 0x001640EC): after the original runs, the
 *   gate is set to the live count, lowered in a MapMaker map in play by the
 *   raised limit minus 20, so both stock comparisons with 20 now compare the
 *   count with the raised limit. Every other scene, and a System Link session
 *   in scene 100, sees the count itself: stock behaviour. Default limit 32
 *   characters (humans included), which the 30 AI slots and the pool reach
 *   first; TSFP_MAPMAKER_ALIVE=1..32 lowers it (20 = stock) for slow PCs.
 * - pc_mapmaker_actors_slot (sub_0015DF20, AI slot allocation, scene 100):
 *   refuses (returns -1, as the original does when its 30 slots are used) a
 *   slot that would lie outside the actor pool or the pair table. Stock only
 *   stayed inside them because of the 20-alive throttle: with three or four
 *   humans, 30 AI slots end past the 32-slot pool, and outside story mode the
 *   pair table holds only humans + 10 characters. Drops are counted and the
 *   first few logged.
 *
 * Both hooks run often (per spawn record per frame) and read fixed .bss
 * addresses only; the actor scan is bounds-checked with MAPMAKER_PTR_OK.
 * Requires the includer's WORD_AT, g_eax, g_esp and g_xbox_mem_offset. */
#ifndef MAPMAKER_GUEST_RET
#define MAPMAKER_GUEST_RET() (g_esp+=4u) /* see pc_mapmaker_limits.inc */
#endif
#ifndef MAPMAKER_PTR_OK
#define MAPMAKER_PTR_OK(p,n) ((((uint32_t)(p))>=0x00010000u && ((uint32_t)(p))<0x04000000u-(uint32_t)(n)) || \
                              (((uint32_t)(p))>=0x80000000u && ((uint32_t)(p))<0x84000000u-(uint32_t)(n)))
#endif
#include <stdio.h>
#include <stdlib.h>

#define MAPMAKER_ACTORS_GATE 0x00900000u      /* actors.json actor-spawn-gate-* */
#define MAPMAKER_ACTORS_STOCK_ALIVE 20u       /* the spawner's imm8 0x14, kept */
#define MAPMAKER_ACTORS_ALIVE 32u             /* default and ceiling: the pool */
#define MAPMAKER_ACTORS_POOL 32u              /* sub_000D6960: 32 slots (scene 101: 4) */
#define MAPMAKER_ACTORS_POOL_101 4u
#define MAPMAKER_ACTORS_AI_SLOTS 30u          /* sub_0015DF20 cmp ebx,0x1E */
#define MAPMAKER_ACTORS_STRIDE 0xEA0u
#define MAPMAKER_ACTORS_PAIRS_STORY 32u       /* sub_0015E7C0: mode 10 */
#define MAPMAKER_ACTORS_PAIRS_EXTRA 10u       /* otherwise humans + 10 */

static struct { int initialized; unsigned alive, drops; } mapmaker_actors;

static void mapmaker_actors_read(void)
{
    const char *text;
    char *end;
    unsigned long v;
    if(mapmaker_actors.initialized) return;
    mapmaker_actors.initialized=1;
    mapmaker_actors.alive=MAPMAKER_ACTORS_ALIVE;
    text=getenv("TSFP_MAPMAKER_ALIVE");
    if(text && *text) {
        v=strtoul(text,&end,10);
        if(!*end && v>=1u && v<=MAPMAKER_ACTORS_ALIVE) mapmaker_actors.alive=(unsigned)v;
    }
}

/* Raised limit minus the stock 20, only for a MapMaker map played locally. */
static int32_t mapmaker_actors_delta(void)
{
    if(WORD_AT(0x0079094Cu)!=100u || WORD_AT(0x0076B108u)) return 0;  /* scene, network session */
    mapmaker_actors_read();
    return (int32_t)mapmaker_actors.alive-(int32_t)MAPMAKER_ACTORS_STOCK_ALIVE;
}

void pc_mapmaker_actors_spawn_gate(void (*original)(void))
{
    original();                                /* edx = record; result in eax, kept */
    WORD_AT(MAPMAKER_ACTORS_GATE)=(uint32_t)((int32_t)WORD_AT(0x0074C37Cu)-mapmaker_actors_delta());
}

/* The slot sub_0015DF20 would take: the first free one in
 * [humans, humans+used), else humans+used while fewer than 30 are used;
 * -1 when the original refuses by itself. */
static int32_t mapmaker_actors_next_slot(uint32_t pool,uint32_t humans,uint32_t used,int story)
{
    uint32_t i,a;
    for(i=humans;i<humans+used;i++) {
        a=pool+i*MAPMAKER_ACTORS_STRIDE+(story?0x10u:0x18u);
        if(!MAPMAKER_PTR_OK(a,8)) return (int32_t)i;      /* refused below: outside guest memory */
        if(!(WORD_AT(a)|WORD_AT(a+4))) return (int32_t)i;
    }
    return used<MAPMAKER_ACTORS_AI_SLOTS?(int32_t)(humans+used):-1;
}

void pc_mapmaker_actors_slot(void (*original)(void))
{
    uint32_t pool,humans,used,cap,pairs;
    int32_t slot;
    int story;
    if(WORD_AT(0x0079094Cu)!=100u) { original(); return; }
    pool=WORD_AT(0x007356D8u);humans=WORD_AT(0x00790950u);used=WORD_AT(0x007A2958u);
    if(!pool || humans>MAPMAKER_ACTORS_POOL || used>MAPMAKER_ACTORS_AI_SLOTS) { original(); return; }
    story=(WORD_AT(0x007DE454u)>>8&0xFFu)==10u;              /* byte [0x007DE455] */
    cap=MAPMAKER_ACTORS_POOL;                                  /* scene 100: (scene==101)?4:32 */
    pairs=story?MAPMAKER_ACTORS_PAIRS_STORY:humans+MAPMAKER_ACTORS_PAIRS_EXTRA;
    if(pairs<cap) cap=pairs;
    slot=mapmaker_actors_next_slot(pool,humans,used,story);
    if(slot>=0 && ((uint32_t)slot>=cap || !MAPMAKER_PTR_OK(pool+(uint32_t)slot*MAPMAKER_ACTORS_STRIDE,MAPMAKER_ACTORS_STRIDE))) {
        if(mapmaker_actors.drops++<3u) {
            fprintf(stderr,"[mapmaker] actor pool full: AI not spawned (slot %d of %u)\n",(int)slot,cap);
            fflush(stderr);
        }
        g_eax=0xFFFFFFFFu;                     /* the original's own refusal */
        MAPMAKER_GUEST_RET();
        return;
    }
    original();
}
