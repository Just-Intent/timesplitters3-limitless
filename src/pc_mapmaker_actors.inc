/* US 45410066 MapMaker capacity, actors area: how many characters can be
 * alive at once in a MapMaker map in play (scene 100). See
 * docs/mapmaker-actors.md for the inventory, the evidence and every site.
 *
 * Stock engine: 32 characters (players + AI). A character's id is its slot
 * in the actor pool (0xEA0 B each, [0x007356D8], sub_000D6960). This area
 * raises a LOCAL MapMaker STORY map (scene 100, story mode byte
 * [0x007DE455] == 10, no System Link session [0x0076B108]) to 64 characters:
 * "wide" below. Every other scene, mode and network session keeps 32 and the
 * stock limits; the capacity changes there are memory only.
 *
 * Capacity (config/guest-code-patches/actors.json, actors-list.json):
 * - live character list 0x007A3580 (32) -> 0x00900100 (64), every reference;
 * - AI table 0x007A2D00 (30) -> 0x00900200 (64), AI slot cap 30 -> 63
 *   (enforced by the slot hook below);
 * - pair table [0x0074C368] 32 -> 64 characters in story mode;
 * - per-character table 0x00739AA0 (3 per id) -> 0x00900300 (64 ids);
 * - per-player perception array: characters 0..63 at +0..+0x3FF, objects
 *   from +0x400 (the slice is 0xC00 in objects.json).
 *
 * Hooks (all cdecl, plain ret):
 * - pool / pool_clear (sub_000D6960 / sub_000D6920): a 64-slot actor pool in
 *   scene 100 (stock 32; 4 in scene 101), allocated from the Level heap as
 *   the original; the slot count is recorded for the slot guard.
 * - spawn_gate (sub_00163F10): the spawner's two "20 alive" throttles
 *   compare a gate dword (0x00900000) that holds the count lowered by
 *   (limit - 20) in a local MapMaker map: limit 64 when wide, else 32;
 *   TSFP_MAPMAKER_ALIVE=1..64 lowers it (20 = stock).
 * - slot (sub_0015DF20, every scene): keeps the stock 30-AI cap except when
 *   wide (64), and refuses (-1, the original's own refusal) any slot outside
 *   the actor pool, the pair table or 64; drops are counted and logged.
 * - placed (sub_0025F830): level-start AI creation takes its count from level
 *   data with no bound; it is clamped to the AI table and the pool.
 * - list_add / list_readd (sub_0015BCA0 / sub_0015C3D0): refuse to append to
 *   a full 64-entry live list.
 * - pick_* (sub_00163B40/BD0/C50/CF0/D90): these fill a 32-entry stack
 *   array with one entry per matching live character; past 32 live
 *   characters they are reimplemented with a 64-entry array on the guest
 *   stack and the same sub_001638F0 call.
 * - enemies (sub_0027BA40): same 32-entry stack array (characters of
 *   another team) feeding an argmin; past 33 live characters it runs twice
 *   over halves of the list (each <= 32 candidates, pass 2 starts with pass
 *   1's winner), which gives the stock choice over every candidate.
 * - create / kill / score / deaths (sub_000DAC30 / sub_000644D0 /
 *   sub_000640C0 / sub_00064650): each actor has 32 per-opponent kill words
 *   at +0xA10 (+0xA50.. are other score fields). Kills of victims 32..63 go
 *   to a side table (64 killers x 32 words at 0x00900600), cleared with the
 *   actor's own words, and both sums read it.
 * - blip (sub_001339E0): radar blips go to a 256-entry buffer
 *   (0x00902100, stock 56) and are bounded; the radar's character loop runs
 *   to 64 characters (stock stops at 52).
 * - effects (sub_002B24E0): the per-death effect lists (objects 0x0075F670,
 *   pads 0x0075F5F8) hold 64 at 0x00903400 / 0x00903200 with bound 64; the
 *   hook keeps the stock 30 outside wide play.
 * - The patrol table 0x00520C38 (id*10 + step) is read from a copy at
 *   0x00901600 (rows 0..31 stock, 32..64 "no route"), refreshed by the pool
 *   hook at each level load.
 *
 * Hot hooks read fixed .bss addresses and check pointers with
 * MAPMAKER_PTR_OK / MAPMAKER_STACK_OK only (no guest_writable).
 * Requires the includer's WORD_AT, guest_call, g_eax, g_esp and
 * g_xbox_mem_offset. */
#ifndef MAPMAKER_GUEST_RET
#define MAPMAKER_GUEST_RET() (g_esp+=4u) /* see pc_mapmaker_limits.inc */
#endif
#ifndef MAPMAKER_PTR_OK
#define MAPMAKER_PTR_OK(p,n) ((((uint32_t)(p))>=0x00010000u && ((uint32_t)(p))<0x04000000u-(uint32_t)(n)) || \
                              (((uint32_t)(p))>=0x80000000u && ((uint32_t)(p))<0x84000000u-(uint32_t)(n)))
#endif
#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAPMAKER_ACTORS_GATE 0x00900000u      /* actors.json actor-spawn-gate-* */
#define MAPMAKER_ACTORS_LIST 0x00900100u      /* actors-list.json actor-list-* */
#define MAPMAKER_ACTORS_AITAB 0x00900200u     /* actors-list.json actor-aitab-* */
#define MAPMAKER_ACTORS_ATTACH 0x00900300u    /* actors.json actor-attach-relocate */
#define MAPMAKER_ACTORS_SIDE 0x00900600u      /* kill words for victims 32..63: 64 killers x 32 x 2 B */
#define MAPMAKER_ACTORS_PATROL 0x00901600u    /* actors.json actor-patrol-*: 0x00520C38 rows, 65 x 40 B */
#define MAPMAKER_ACTORS_BLIPS 0x00902100u     /* actors.json actor-blips-*: radar blips, 16 B each */
#define MAPMAKER_ACTORS_BLIP_CAP 256u         /* the stock buffer 0x007A5DC0 holds 56 */
#define MAPMAKER_ACTORS_RADAR_END 0x0077BB44u /* actor-radar-bound: 0x00777244 + 64 x 0x124 */
#define MAPMAKER_ACTORS_EFFECT_PADS 0x00903200u /* actors.json actor-effects-pads-*: 0x0075F5F8, 64 dwords */
#define MAPMAKER_ACTORS_EFFECTS 0x00903400u   /* actor-effects-objects-relocate: 0x0075F670, 64 dwords */
#define MAPMAKER_ACTORS_EFFECTS_STOCK 30u     /* sub_002B24E0 cmp [0x0075F6E8],0x1E (stock) */
#define MAPMAKER_ACTORS_PATROL_STOCK 0x00520C38u
#define MAPMAKER_ACTORS_PATROL_ROW 40u        /* 10 dwords: index id*10 + step */
#define MAPMAKER_ACTORS_MAX 64u               /* characters when wide: list, AI table, pairs, pool */
#define MAPMAKER_ACTORS_STOCK 32u             /* characters everywhere else */
#define MAPMAKER_ACTORS_STOCK_ALIVE 20u       /* the spawner's imm8 0x14, kept */
#define MAPMAKER_ACTORS_POOL_101 4u           /* sub_000D6960 in scene 101 */
#define MAPMAKER_ACTORS_STOCK_AI 30u          /* sub_0015DF20 cmp ebx,0x1E (stock) */
#define MAPMAKER_ACTORS_AI_CAP 63u            /* actor-ai-slot-cap operand */
#define MAPMAKER_ACTORS_STRIDE 0xEA0u
#define MAPMAKER_ACTORS_PAIRS_EXTRA 10u       /* pair table outside story: humans + 10 */
#define MAPMAKER_ACTORS_ARRAY 32u             /* stack arrays of sub_00163B40.. / sub_0027BA40 */
#define MAPMAKER_ACTORS_SCORE_WORDS 32u       /* per-opponent kill words at actor+0xA10 */

#define MAPMAKER_ACTORS_SCENE 0x0079094Cu
#define MAPMAKER_ACTORS_COUNT 0x0074C37Cu
#define MAPMAKER_ACTORS_HUMANS 0x00790950u
#define MAPMAKER_ACTORS_USED 0x007A2958u
#define MAPMAKER_ACTORS_POOLPTR 0x007356D8u
#define MAPMAKER_ACTORS_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define MAPMAKER_ACTORS_S16(a) (*(int16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

extern void sub_0003DFA0(void),sub_001CD040(void),sub_001638F0(void),sub_003533F0(void);

enum { MAPMAKER_ACTORS_DROP_SLOT, MAPMAKER_ACTORS_DROP_PLACED, MAPMAKER_ACTORS_DROP_LIST,
       MAPMAKER_ACTORS_DROP_PICK, MAPMAKER_ACTORS_DROP_SCORE, MAPMAKER_ACTORS_DROP_BLIP,
       MAPMAKER_ACTORS_DROPS };
static const char *const mapmaker_actors_drop_name[MAPMAKER_ACTORS_DROPS]={
    "actor pool full: AI not spawned","level AI past the AI table: not created",
    "live character list full: character not listed","AI target list full: candidate skipped",
    "kill of character >= 32 in an arcade sub-mode: not recorded","radar blip buffer full: blip not drawn"};
static struct {
    int initialized;
    unsigned alive;
    unsigned drops[MAPMAKER_ACTORS_DROPS];
    uint32_t pool_ptr,pool_slots;               /* last allocation by sub_000D6960 */
} mapmaker_actors;

static void mapmaker_actors_drop(unsigned kind,uint32_t a,uint32_t b)
{
    if(mapmaker_actors.drops[kind]++<3u) {
        fprintf(stderr,"[mapmaker] %s (%u of %u)\n",mapmaker_actors_drop_name[kind],(unsigned)a,(unsigned)b);
        fflush(stderr);
    }
}

static void mapmaker_actors_read(void)
{
    const char *text;
    char *end;
    unsigned long v;
    if(mapmaker_actors.initialized) return;
    mapmaker_actors.initialized=1;
    mapmaker_actors.alive=MAPMAKER_ACTORS_MAX;
    text=getenv("TSFP_MAPMAKER_ALIVE");
    if(text && *text) {
        v=strtoul(text,&end,10);
        if(!*end && v>=1u && v<=MAPMAKER_ACTORS_MAX) mapmaker_actors.alive=(unsigned)v;
    }
}

static int mapmaker_actors_story(void) { return MAPMAKER_ACTORS_B(0x007DE455u)==10u; }
static int mapmaker_actors_local100(void)
{
    return WORD_AT(MAPMAKER_ACTORS_SCENE)==100u && !WORD_AT(0x0076B108u);  /* scene, network session */
}
/* Ids >= 32 may exist: a MapMaker story map played locally. */
static int mapmaker_actors_wide(void) { return mapmaker_actors_local100() && mapmaker_actors_story(); }

/* Slots in the pool [0x007356D8] now points at. */
static uint32_t mapmaker_actors_pool_slots(uint32_t pool)
{
    if(pool && pool==mapmaker_actors.pool_ptr) return mapmaker_actors.pool_slots;
    return WORD_AT(MAPMAKER_ACTORS_SCENE)==101u?MAPMAKER_ACTORS_POOL_101:MAPMAKER_ACTORS_STOCK;
}

/* ---- actor pool (sub_000D6960 allocate, sub_000D6920 clear) ---- */
static void mapmaker_actors_zero(uint32_t a,uint32_t bytes)
{
    memset((void *)((uintptr_t)a+g_xbox_mem_offset),0,bytes);
}
static uint32_t mapmaker_actors_pool_bytes(uint32_t slots) { return (slots*MAPMAKER_ACTORS_STRIDE+15u)&~15u; }

/* Kill words of killer slot k against victim v (32..63): an actor's own
 * array at +0xA10 holds victims 0..31 only. */
static uint32_t mapmaker_actors_side(uint32_t k,uint32_t v)
{
    return MAPMAKER_ACTORS_SIDE+(k*MAPMAKER_ACTORS_SCORE_WORDS+(v-MAPMAKER_ACTORS_SCORE_WORDS))*2u;
}
#define MAPMAKER_ACTORS_SIDE_BYTES (MAPMAKER_ACTORS_MAX*MAPMAKER_ACTORS_SCORE_WORDS*2u)
/* Kill words of victim j as seen by actor e (killer slot k). */
static int32_t mapmaker_actors_kills(uint32_t e,uint32_t k,uint32_t j)
{
    if(j<MAPMAKER_ACTORS_SCORE_WORDS) return MAPMAKER_ACTORS_S16(e+0xA10u+j*2u);
    if(k<MAPMAKER_ACTORS_MAX && j<MAPMAKER_ACTORS_MAX) return MAPMAKER_ACTORS_S16(mapmaker_actors_side(k,j));
    return 0;
}
/* The patrol table 0x00520C38 (index id*10 + step) holds real routes for
 * ids 0..4 only (sub_002B2750's own switch @0x002B3AB6); the rows after it
 * are other data. The moved copy keeps rows 0..31 exactly and gives rows
 * 32..64 -1, "no route": sub_0009CC40(-1) returns -1 and the code takes its
 * own fallback, player 1's current pad (@0x002B3B4A, @0x002B3CA6). */
static void mapmaker_actors_patrol_init(void)
{
    memcpy((void *)((uintptr_t)MAPMAKER_ACTORS_PATROL+g_xbox_mem_offset),
           (const void *)((uintptr_t)MAPMAKER_ACTORS_PATROL_STOCK+g_xbox_mem_offset),
           MAPMAKER_ACTORS_STOCK*MAPMAKER_ACTORS_PATROL_ROW);
    memset((void *)((uintptr_t)(MAPMAKER_ACTORS_PATROL+MAPMAKER_ACTORS_STOCK*MAPMAKER_ACTORS_PATROL_ROW)+g_xbox_mem_offset),
           0xFF,(MAPMAKER_ACTORS_MAX-MAPMAKER_ACTORS_STOCK+1u)*MAPMAKER_ACTORS_PATROL_ROW);
}

void pc_mapmaker_actors_pool(void (*original)(void))
{
    uint32_t scene=WORD_AT(MAPMAKER_ACTORS_SCENE),bytes,p,f;
    mapmaker_actors_patrol_init();             /* every level load, before any character */
    mapmaker_actors_zero(MAPMAKER_ACTORS_SIDE,MAPMAKER_ACTORS_SIDE_BYTES);
    if(scene!=100u) {
        original();
        mapmaker_actors.pool_ptr=WORD_AT(MAPMAKER_ACTORS_POOLPTR);
        mapmaker_actors.pool_slots=scene==101u?MAPMAKER_ACTORS_POOL_101:MAPMAKER_ACTORS_STOCK;
        return;
    }
    g_eax=0;
    if(WORD_AT(0x007DE304u)) {                 /* the original returns at once without it */
        bytes=mapmaker_actors_pool_bytes(MAPMAKER_ACTORS_MAX);
        mapmaker_actors_zero(0x00735630u,0x28u*4u);
        mapmaker_actors_zero(0x007331A8u,0x870u*4u);
        p=guest_call(sub_0003DFA0,2,bytes,1u);  /* Level heap, as the original */
        WORD_AT(MAPMAKER_ACTORS_POOLPTR)=p;
        mapmaker_actors_zero(0x00735368u,0xB1u*4u);
        f=guest_call(sub_001CD040,0)&WORD_AT(0x00790958u)&WORD_AT(0x00790954u);
        WORD_AT(0x007356D0u)=(f&0x1000u)?1u:0u;   /* scene 100: the scene-101 test does not apply */
        WORD_AT(0x007356D4u)=0;
        if(p && MAPMAKER_PTR_OK(p,bytes)) mapmaker_actors_zero(p,bytes);
        WORD_AT(0x007B0C44u)=0;
        mapmaker_actors.pool_ptr=p;
        mapmaker_actors.pool_slots=p?MAPMAKER_ACTORS_MAX:0u;
    }
    MAPMAKER_GUEST_RET();
}

void pc_mapmaker_actors_pool_clear(void (*original)(void))
{
    uint32_t p=WORD_AT(MAPMAKER_ACTORS_POOLPTR),bytes;
    mapmaker_actors_zero(MAPMAKER_ACTORS_SIDE,MAPMAKER_ACTORS_SIDE_BYTES);   /* as the pool's +0xA10 words */
    if(WORD_AT(MAPMAKER_ACTORS_SCENE)!=100u || !p || p!=mapmaker_actors.pool_ptr ||
       mapmaker_actors.pool_slots<=MAPMAKER_ACTORS_STOCK) { original(); return; }
    bytes=mapmaker_actors_pool_bytes(mapmaker_actors.pool_slots);
    if(MAPMAKER_PTR_OK(p,bytes)) mapmaker_actors_zero(p,bytes);
    WORD_AT(0x007B0C44u)=0;
    g_eax=0;
    MAPMAKER_GUEST_RET();
}

/* ---- spawner throttle (sub_00163F10) ---- */
/* Raised limit minus the stock 20, only for a MapMaker map played locally. */
static int32_t mapmaker_actors_delta(void)
{
    unsigned limit;
    if(!mapmaker_actors_local100()) return 0;
    mapmaker_actors_read();
    limit=mapmaker_actors.alive;
    if(!mapmaker_actors_story() && limit>MAPMAKER_ACTORS_STOCK) limit=MAPMAKER_ACTORS_STOCK;
    return (int32_t)limit-(int32_t)MAPMAKER_ACTORS_STOCK_ALIVE;
}

void pc_mapmaker_actors_spawn_gate(void (*original)(void))
{
    original();                                /* edx = record; result in eax, kept */
    WORD_AT(MAPMAKER_ACTORS_GATE)=(uint32_t)((int32_t)WORD_AT(MAPMAKER_ACTORS_COUNT)-mapmaker_actors_delta());
}

/* ---- AI slot allocation (sub_0015DF20) ---- */
/* The slot sub_0015DF20 would take: the first free one in [humans, humans+used),
 * else humans+used while fewer than ai_cap are used; -1: refused. */
static int32_t mapmaker_actors_next_slot(uint32_t pool,uint32_t humans,uint32_t used,int story,uint32_t ai_cap)
{
    uint32_t i,a;
    for(i=humans;i<humans+used;i++) {
        a=pool+i*MAPMAKER_ACTORS_STRIDE+(story?0x10u:0x18u);
        if(!MAPMAKER_PTR_OK(a,8)) return (int32_t)i;      /* refused below: outside guest memory */
        if(!(WORD_AT(a)|WORD_AT(a+4))) return (int32_t)i;
    }
    return used<ai_cap?(int32_t)(humans+used):-1;
}

void pc_mapmaker_actors_slot(void (*original)(void))
{
    uint32_t pool,humans,used,cap,pairs,ai_cap;
    int32_t slot;
    int story,wide;
    pool=WORD_AT(MAPMAKER_ACTORS_POOLPTR);humans=WORD_AT(MAPMAKER_ACTORS_HUMANS);used=WORD_AT(MAPMAKER_ACTORS_USED);
    if(!pool || humans>MAPMAKER_ACTORS_MAX || used>MAPMAKER_ACTORS_AI_CAP) { original(); return; }
    story=mapmaker_actors_story();
    wide=story && mapmaker_actors_local100();
    ai_cap=wide?MAPMAKER_ACTORS_AI_CAP:MAPMAKER_ACTORS_STOCK_AI;
    cap=mapmaker_actors_pool_slots(pool);
    if(cap>(wide?MAPMAKER_ACTORS_MAX:MAPMAKER_ACTORS_STOCK)) cap=wide?MAPMAKER_ACTORS_MAX:MAPMAKER_ACTORS_STOCK;
    pairs=story?MAPMAKER_ACTORS_MAX:humans+MAPMAKER_ACTORS_PAIRS_EXTRA;
    if(pairs<cap) cap=pairs;
    slot=mapmaker_actors_next_slot(pool,humans,used,story,ai_cap);
    if(slot<0 || (uint32_t)slot>=cap ||
       !MAPMAKER_PTR_OK(pool+(uint32_t)slot*MAPMAKER_ACTORS_STRIDE,MAPMAKER_ACTORS_STRIDE)) {
        if(slot>=0) mapmaker_actors_drop(MAPMAKER_ACTORS_DROP_SLOT,(uint32_t)slot,cap);
        g_eax=0xFFFFFFFFu;                     /* the original's own refusal */
        MAPMAKER_GUEST_RET();
        return;
    }
    original();
}

/* ---- level-start AI (sub_0025F830): count [[0x0074C36C]+0x30], no bound ---- */
void pc_mapmaker_actors_placed(void (*original)(void))
{
    uint32_t level=WORD_AT(0x0074C36Cu),n,humans,slots,cap;
    if(!MAPMAKER_PTR_OK(level+0x30u,4)) { original(); return; }
    n=WORD_AT(level+0x30u);humans=WORD_AT(MAPMAKER_ACTORS_HUMANS);
    slots=mapmaker_actors_pool_slots(WORD_AT(MAPMAKER_ACTORS_POOLPTR));
    cap=slots>humans?slots-humans:0u;
    if(cap>MAPMAKER_ACTORS_MAX) cap=MAPMAKER_ACTORS_MAX;
    if((int32_t)n>(int32_t)cap) {
        mapmaker_actors_drop(MAPMAKER_ACTORS_DROP_PLACED,n,cap);
        WORD_AT(level+0x30u)=cap;              /* kept: the level data then matches the AI table */
    }
    original();
}

/* ---- live list appends (sub_0015BCA0, sub_0015C3D0) ---- */
static int mapmaker_actors_list_full(void)
{
    uint32_t count=WORD_AT(MAPMAKER_ACTORS_COUNT);
    if(count<MAPMAKER_ACTORS_MAX) return 0;
    mapmaker_actors_drop(MAPMAKER_ACTORS_DROP_LIST,count,MAPMAKER_ACTORS_MAX);
    return 1;
}
void pc_mapmaker_actors_list_add(void (*original)(void))
{
    if(mapmaker_actors_list_full()) { g_eax=WORD_AT(MAPMAKER_ACTORS_COUNT);MAPMAKER_GUEST_RET();return; }
    original();
}
void pc_mapmaker_actors_list_readd(void (*original)(void))
{
    if(mapmaker_actors_list_full()) { g_eax=0;MAPMAKER_GUEST_RET();return; }   /* "not added" */
    original();
}

/* ---- AI target candidates (sub_00163B40/BD0/C50/CF0/D90 -> sub_001638F0) ---- */
enum { MAPMAKER_ACTORS_PICK_TYPE, MAPMAKER_ACTORS_PICK_TEAM_AI, MAPMAKER_ACTORS_PICK_GROUP,
       MAPMAKER_ACTORS_PICK_TEAM, MAPMAKER_ACTORS_PICK_GROUP_AI };
static void mapmaker_actors_pick(void (*original)(void),unsigned kind)
{
    uint32_t count=WORD_AT(MAPMAKER_ACTORS_COUNT),humans,arg,self,rec,t,def,id,i,e,r,buf,n=0;
    int humans_too=kind==MAPMAKER_ACTORS_PICK_GROUP || kind==MAPMAKER_ACTORS_PICK_TEAM;
    if(count<=MAPMAKER_ACTORS_ARRAY || !MAPMAKER_STACK_OK(0x200) || g_esp<0x00010200u) { original(); return; }
    arg=WORD_AT(g_esp+4);
    if(!MAPMAKER_PTR_OK(arg,0x80)) { original(); return; }
    self=WORD_AT(arg+0x7Cu);
    if(!MAPMAKER_PTR_OK(self,MAPMAKER_ACTORS_STRIDE)) { original(); return; }
    rec=WORD_AT(self+0xBF0u);
    if(!MAPMAKER_PTR_OK(rec,0x78)) { original(); return; }
    t=WORD_AT(rec+0x74u);
    def=(kind==MAPMAKER_ACTORS_PICK_TEAM_AI || kind==MAPMAKER_ACTORS_PICK_GROUP_AI)?0u:
        WORD_AT(WORD_AT(0x007B0C48u)+0x14u);
    if((int32_t)t<=-1) { g_eax=def;MAPMAKER_GUEST_RET();return; }
    humans=WORD_AT(MAPMAKER_ACTORS_HUMANS);
    if(count>MAPMAKER_ACTORS_MAX) count=MAPMAKER_ACTORS_MAX;
    buf=g_esp-0x100u;
    id=WORD_AT(self+8u);
    for(i=kind==MAPMAKER_ACTORS_PICK_GROUP || kind==MAPMAKER_ACTORS_PICK_TEAM?0u:humans;i<count;i++) {
        e=WORD_AT(MAPMAKER_ACTORS_LIST+i*4u);
        if(!MAPMAKER_PTR_OK(e,MAPMAKER_ACTORS_STRIDE) || WORD_AT(e+8u)==id) continue;
        if(kind==MAPMAKER_ACTORS_PICK_TYPE) {
            if((int32_t)MAPMAKER_ACTORS_S16(e+0x48u)!=(int32_t)t) continue;
        } else if(!(humans_too && i<humans)) {
            r=WORD_AT(e+0xBF0u);
            if(!MAPMAKER_PTR_OK(r,0x2C)) continue;
            if(WORD_AT(r+((kind==MAPMAKER_ACTORS_PICK_GROUP || kind==MAPMAKER_ACTORS_PICK_GROUP_AI)?8u:0x28u))!=t) continue;
        }
        if(n>=0x40u) { mapmaker_actors_drop(MAPMAKER_ACTORS_DROP_PICK,i,count);break; }
        WORD_AT(buf+n*4u)=e;n++;
    }
    g_esp=buf;
    r=guest_call(sub_001638F0,4,self,n,buf,def);
    g_esp=buf+0x100u;
    g_eax=r;
    MAPMAKER_GUEST_RET();
}
void pc_mapmaker_actors_pick_type(void (*original)(void)) { mapmaker_actors_pick(original,MAPMAKER_ACTORS_PICK_TYPE); }
void pc_mapmaker_actors_pick_team_ai(void (*original)(void)) { mapmaker_actors_pick(original,MAPMAKER_ACTORS_PICK_TEAM_AI); }
void pc_mapmaker_actors_pick_group(void (*original)(void)) { mapmaker_actors_pick(original,MAPMAKER_ACTORS_PICK_GROUP); }
void pc_mapmaker_actors_pick_team(void (*original)(void)) { mapmaker_actors_pick(original,MAPMAKER_ACTORS_PICK_TEAM); }
void pc_mapmaker_actors_pick_group_ai(void (*original)(void)) { mapmaker_actors_pick(original,MAPMAKER_ACTORS_PICK_GROUP_AI); }

/* ---- AI enemy choice (sub_0027BA40): 32-entry stack array of other-team characters ----
 * It fills the array (team [e+0x84] differs, not [e+0x24]&8, more filters)
 * and returns the object with the lowest score; each score depends only on
 * the AI and that candidate, and ties keep the first in list order
 * (comiss/jbe @0x0027BCA4, @0x0027BE96). Past 33 live characters the hook
 * runs it twice over the same-length list with the other entries replaced by
 * the AI itself (rejected at once by the team test): pass A = the longest
 * prefix holding at most 32 candidates, pass B = pass A's winner first, then
 * the rest (at most 31 candidates). Pass B's result is the stock argmin over
 * all candidates. The count stays the same, so the original's
 * (count - humans) divisor is unchanged. Its direct-call closure (16
 * functions) never writes the list or the count and has no indirect call. */
void pc_mapmaker_actors_enemies(void (*original)(void))
{
    uint32_t count=WORD_AT(MAPMAKER_ACTORS_COUNT),arg,arg2,self,team,i,e,n=0,k,ret,winner;
    uint32_t saved[MAPMAKER_ACTORS_MAX];
    if(count<=MAPMAKER_ACTORS_ARRAY+1u || count>MAPMAKER_ACTORS_MAX || !MAPMAKER_STACK_OK(12)) { original(); return; }
    arg=WORD_AT(g_esp+4);arg2=WORD_AT(g_esp+8);
    if(!MAPMAKER_PTR_OK(arg,0x80)) { original(); return; }
    self=WORD_AT(arg+0x7Cu);
    if(!MAPMAKER_PTR_OK(self,MAPMAKER_ACTORS_STRIDE)) { original(); return; }
    /* the original's early returns build no array (@0x0027BA7A, @0x0027BA98) */
    if((arg2 && WORD_AT(0x0074C388u)!=WORD_AT(self+8u)) || (MAPMAKER_ACTORS_B(0x007DE458u)&0x40u)) { original(); return; }
    team=WORD_AT(self+0x84u);
    for(i=0;i<count;i++) {                     /* superset of the original's filter (0x0027BAC7..) */
        e=WORD_AT(MAPMAKER_ACTORS_LIST+i*4u);
        saved[i]=e;
        if(!MAPMAKER_PTR_OK(e,MAPMAKER_ACTORS_STRIDE) || (MAPMAKER_ACTORS_B(e+0x24u)&8u) || WORD_AT(e+0x84u)==team) continue;
        if(n==MAPMAKER_ACTORS_ARRAY) break;
        n++;
    }
    if(i>=count) { original(); return; }
    k=i;
    for(;i<count;i++) saved[i]=WORD_AT(MAPMAKER_ACTORS_LIST+i*4u);
    ret=WORD_AT(g_esp);
    for(i=k;i<count;i++) WORD_AT(MAPMAKER_ACTORS_LIST+i*4u)=self;           /* pass A */
    original();
    winner=g_eax;
    for(i=0;i<count;i++) WORD_AT(MAPMAKER_ACTORS_LIST+i*4u)=i<k?self:saved[i];  /* pass B */
    if(winner)
        for(i=0;i<k;i++)
            if(MAPMAKER_PTR_OK(saved[i],4) && WORD_AT(saved[i])==winner) { WORD_AT(MAPMAKER_ACTORS_LIST)=saved[i];break; }
    g_esp-=4u;WORD_AT(g_esp)=ret;                 /* same arguments, same return address */
    original();
    for(i=0;i<count;i++) WORD_AT(MAPMAKER_ACTORS_LIST+i*4u)=saved[i];
}

/* ---- per-opponent kill words: actor+0xA10 for victims 0..31, side table for 32..63 ---- */
/* Character creation (sub_000DAC30, slot = 3rd argument) clears the new
 * actor's 0xA10 words (@0x000DAEC1); clear its side row with them. */
void pc_mapmaker_actors_create(void (*original)(void))
{
    uint32_t slot,pool,a;
    if(!MAPMAKER_STACK_OK(16)) { original(); return; }
    slot=WORD_AT(g_esp+12);
    original();
    if(slot>=MAPMAKER_ACTORS_MAX || !g_eax) return;
    pool=WORD_AT(MAPMAKER_ACTORS_POOLPTR);a=pool+slot*MAPMAKER_ACTORS_STRIDE;
    if(pool && MAPMAKER_PTR_OK(a,4) && WORD_AT(a)==g_eax)    /* the new actor's object: reached the clear */
        mapmaker_actors_zero(mapmaker_actors_side(slot,MAPMAKER_ACTORS_SCORE_WORDS),MAPMAKER_ACTORS_SCORE_WORDS*2u);
}

/* sub_000644D0(killer entry, victim id, points): in round states 6/7 it adds
 * the points at killer+0xA10+victim*2 (@0x000645CC); for victims 32..63
 * that word is another score field, so the add is moved to the side table. */
void pc_mapmaker_actors_kill(void (*original)(void))
{
    uint32_t killer,victim,points,state,a,k;
    uint8_t sub;
    int added;
    if(!MAPMAKER_STACK_OK(16)) { original(); return; }
    killer=WORD_AT(g_esp+4);victim=WORD_AT(g_esp+8);points=WORD_AT(g_esp+12)&0xFFFFu;
    if(!killer || victim<MAPMAKER_ACTORS_SCORE_WORDS || victim>=MAPMAKER_ACTORS_MAX) { original(); return; }
    sub=MAPMAKER_ACTORS_B(0x007DE456u);
    if(sub==0x12u || sub==0x13u) {             /* arcade sub-modes: never with ids >= 32 */
        mapmaker_actors_drop(MAPMAKER_ACTORS_DROP_SCORE,victim,MAPMAKER_ACTORS_SCORE_WORDS);
        g_eax=0;MAPMAKER_GUEST_RET();return;
    }
    state=WORD_AT(0x007DE30Cu);
    added=state==6u || state==7u;
    a=killer+0xA10u+victim*2u;
    original();
    if(added && MAPMAKER_PTR_OK(a,2) && MAPMAKER_PTR_OK(killer+8u,4)) {
        MAPMAKER_ACTORS_S16(a)=(int16_t)(uint16_t)((uint16_t)MAPMAKER_ACTORS_S16(a)-(uint16_t)points);
        k=WORD_AT(killer+8u);
        if(k<MAPMAKER_ACTORS_MAX) {
            a=mapmaker_actors_side(k,victim);
            MAPMAKER_ACTORS_S16(a)=(int16_t)(uint16_t)((uint16_t)MAPMAKER_ACTORS_S16(a)+(uint16_t)points);
        }
    }
}

/* sub_000640C0(entry): kills on characters of other teams. Past 32 slots
 * the stock loop would read +0xA50.. as kill words; the hook reads victims
 * 32..63 from the side table instead. */
void pc_mapmaker_actors_score(void (*original)(void))
{
    uint32_t n=WORD_AT(MAPMAKER_ACTORS_HUMANS)+WORD_AT(MAPMAKER_ACTORS_USED),pool,e,team,j,k;
    int32_t sum=0,result;
    uint8_t flags;
    if(n<=MAPMAKER_ACTORS_SCORE_WORDS || !MAPMAKER_STACK_OK(8)) { original(); return; }
    if(n>MAPMAKER_ACTORS_MAX) n=MAPMAKER_ACTORS_MAX;
    e=WORD_AT(g_esp+4);pool=WORD_AT(MAPMAKER_ACTORS_POOLPTR);
    if(!MAPMAKER_PTR_OK(e,MAPMAKER_ACTORS_STRIDE) ||
       !MAPMAKER_PTR_OK(pool,n*MAPMAKER_ACTORS_STRIDE)) { original(); return; }
    team=WORD_AT(e+0x84u);k=WORD_AT(e+8u);
    for(j=0;j<n;j++)
        if(WORD_AT(pool+j*MAPMAKER_ACTORS_STRIDE+0x84u)!=team) sum+=mapmaker_actors_kills(e,k,j);
    if(guest_call(sub_003533F0,0)) sum+=MAPMAKER_ACTORS_S16(e+0xA50u);
    if(WORD_AT(0x007DE474u)) result=MAPMAKER_ACTORS_S16(e+0xA60u);
    else {
        flags=MAPMAKER_ACTORS_B(0x007DE458u);
        if(flags&0x20u) sum-=mapmaker_actors_kills(e,k,k);
        else if(flags&0x10u) sum-=MAPMAKER_ACTORS_S16(e+0xA52u);
        result=sum;
    }
    g_eax=(uint32_t)result;
    MAPMAKER_GUEST_RET();
}

/* sub_00064650(entry): times this character was killed (column sum over
 * every slot). For ids 32..63 the column is in the side table. */
void pc_mapmaker_actors_deaths(void (*original)(void))
{
    uint32_t e,id,n,j;
    int32_t sum=0;
    if(!MAPMAKER_STACK_OK(8) || (MAPMAKER_ACTORS_B(0x007DE458u)&0x40u)) { original(); return; }
    e=WORD_AT(g_esp+4);
    if(!MAPMAKER_PTR_OK(e,0x10)) { original(); return; }
    id=WORD_AT(e+8u);
    if(id<MAPMAKER_ACTORS_SCORE_WORDS || id>=MAPMAKER_ACTORS_MAX) { original(); return; }
    n=WORD_AT(MAPMAKER_ACTORS_HUMANS)+WORD_AT(MAPMAKER_ACTORS_USED);
    if((int32_t)n>(int32_t)MAPMAKER_ACTORS_MAX) n=MAPMAKER_ACTORS_MAX;
    for(j=0;(int32_t)j<(int32_t)n;j++) sum+=MAPMAKER_ACTORS_S16(mapmaker_actors_side(j,id));
    g_eax=(uint32_t)sum;
    MAPMAKER_GUEST_RET();
}

/* ---- radar blips (sub_001339E0, edx = position): moved buffer, bounded ----
 * The counter [0x007490F8] (reset each radar frame by sub_00133AB0) had no
 * bound; the buffer (stock 0x007A5DC0, 56 entries) is now 256 entries. */
void pc_mapmaker_actors_blip(void (*original)(void))
{
    uint32_t count=WORD_AT(0x007490F8u);
    if(count>=MAPMAKER_ACTORS_BLIP_CAP) {
        mapmaker_actors_drop(MAPMAKER_ACTORS_DROP_BLIP,count,MAPMAKER_ACTORS_BLIP_CAP);
        g_eax=count;MAPMAKER_GUEST_RET();return;
    }
    original();
}

/* ---- per-death effect list (sub_002B24E0): objects 0x0075F670 / pads
 * 0x0075F5F8, count [0x0075F6E8] ----
 * Both lists hold 64 entries now and the adder's bound is 64; outside wide
 * play the hook keeps the stock 30 (the original's refusal returns its
 * argument in eax, @0x002B24E7/0x002B25DC). */
void pc_mapmaker_actors_effects(void (*original)(void))
{
    if(!mapmaker_actors_wide() && WORD_AT(0x0075F6E8u)>=MAPMAKER_ACTORS_EFFECTS_STOCK && MAPMAKER_STACK_OK(8)) {
        g_eax=WORD_AT(g_esp+4);MAPMAKER_GUEST_RET();return;
    }
    original();
}
