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
 *   another team); past 33 live characters the live count is lowered for the
 *   call to the longest prefix of the list holding at most 32 such
 *   characters (counted, logged). Its call closure never changes the count.
 * - kill / score / deaths (sub_000644D0 / sub_000640C0 / sub_00064650): each
 *   actor has 32 per-opponent kill words at +0xA10 (+0xA50.. are other
 *   score fields). A kill of a victim id >= 32 is not recorded there (the
 *   stray add is undone), and the sums only read the 32 words.
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
       MAPMAKER_ACTORS_DROP_PICK, MAPMAKER_ACTORS_DROP_ENEMIES, MAPMAKER_ACTORS_DROP_SCORE,
       MAPMAKER_ACTORS_DROPS };
static const char *const mapmaker_actors_drop_name[MAPMAKER_ACTORS_DROPS]={
    "actor pool full: AI not spawned","level AI past the AI table: not created",
    "live character list full: character not listed","AI target list full: candidate skipped",
    "AI enemy list full: characters past it not considered","kill of character >= 32: per-opponent score not recorded"};
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

void pc_mapmaker_actors_pool(void (*original)(void))
{
    uint32_t scene=WORD_AT(MAPMAKER_ACTORS_SCENE),bytes,p,f;
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

/* ---- AI enemy list (sub_0027BA40): 32-entry stack array of other-team characters ---- */
void pc_mapmaker_actors_enemies(void (*original)(void))
{
    uint32_t count=WORD_AT(MAPMAKER_ACTORS_COUNT),arg,self,team,lim,i,e,n=0,after;
    if(count<=MAPMAKER_ACTORS_ARRAY+1u || !MAPMAKER_STACK_OK(8)) { original(); return; }
    arg=WORD_AT(g_esp+4);
    if(!MAPMAKER_PTR_OK(arg,0x80)) { original(); return; }
    self=WORD_AT(arg+0x7Cu);
    if(!MAPMAKER_PTR_OK(self,MAPMAKER_ACTORS_STRIDE)) { original(); return; }
    team=WORD_AT(self+0x84u);
    lim=count>MAPMAKER_ACTORS_MAX?MAPMAKER_ACTORS_MAX:count;
    for(i=0;i<lim;i++) {                       /* superset of the original's filter (0x0027BAC7..) */
        e=WORD_AT(MAPMAKER_ACTORS_LIST+i*4u);
        if(!MAPMAKER_PTR_OK(e,MAPMAKER_ACTORS_STRIDE) || (MAPMAKER_ACTORS_B(e+0x24u)&8u) || WORD_AT(e+0x84u)==team) continue;
        if(n==MAPMAKER_ACTORS_ARRAY) break;
        n++;
    }
    if(i>=count) { original(); return; }
    mapmaker_actors_drop(MAPMAKER_ACTORS_DROP_ENEMIES,i,count);
    WORD_AT(MAPMAKER_ACTORS_COUNT)=i;
    original();
    after=WORD_AT(MAPMAKER_ACTORS_COUNT);
    WORD_AT(MAPMAKER_ACTORS_COUNT)=count+(after-i);   /* = count: the call closure never writes it */
}

/* ---- per-opponent kill words at actor+0xA10 (32 words) ---- */
void pc_mapmaker_actors_kill(void (*original)(void))
{
    uint32_t killer,victim,points,state,a;
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
    added=state==6u || state==7u;              /* then 0x000645CC adds points at killer+0xA10+victim*2 */
    a=killer+0xA10u+victim*2u;
    original();
    if(added && MAPMAKER_PTR_OK(a,2)) {
        MAPMAKER_ACTORS_S16(a)=(int16_t)(uint16_t)((uint16_t)MAPMAKER_ACTORS_S16(a)-(uint16_t)points);
        mapmaker_actors_drop(MAPMAKER_ACTORS_DROP_SCORE,victim,MAPMAKER_ACTORS_SCORE_WORDS);
    }
}

/* Kills on characters of other teams (sum of the per-opponent words). */
void pc_mapmaker_actors_score(void (*original)(void))
{
    uint32_t n=WORD_AT(MAPMAKER_ACTORS_HUMANS)+WORD_AT(MAPMAKER_ACTORS_USED),pool,e,team,j,id;
    int32_t sum=0,result;
    uint8_t flags;
    if(n<=MAPMAKER_ACTORS_SCORE_WORDS || !MAPMAKER_STACK_OK(8)) { original(); return; }
    e=WORD_AT(g_esp+4);pool=WORD_AT(MAPMAKER_ACTORS_POOLPTR);
    if(!MAPMAKER_PTR_OK(e,MAPMAKER_ACTORS_STRIDE) ||
       !MAPMAKER_PTR_OK(pool,MAPMAKER_ACTORS_SCORE_WORDS*MAPMAKER_ACTORS_STRIDE)) { original(); return; }
    team=WORD_AT(e+0x84u);
    for(j=0;j<MAPMAKER_ACTORS_SCORE_WORDS;j++)
        if(WORD_AT(pool+j*MAPMAKER_ACTORS_STRIDE+0x84u)!=team) sum+=MAPMAKER_ACTORS_S16(e+0xA10u+j*2u);
    if(guest_call(sub_003533F0,0)) sum+=MAPMAKER_ACTORS_S16(e+0xA50u);
    if(WORD_AT(0x007DE474u)) result=MAPMAKER_ACTORS_S16(e+0xA60u);
    else {
        flags=MAPMAKER_ACTORS_B(0x007DE458u);
        if(flags&0x20u) {
            id=WORD_AT(e+8u);
            if(id<MAPMAKER_ACTORS_SCORE_WORDS) sum-=MAPMAKER_ACTORS_S16(e+0xA10u+id*2u);
        } else if(flags&0x10u) sum-=MAPMAKER_ACTORS_S16(e+0xA52u);
        result=sum;
    }
    g_eax=(uint32_t)result;
    MAPMAKER_GUEST_RET();
}

/* Times a character was killed (column sum): ids >= 32 are never recorded. */
void pc_mapmaker_actors_deaths(void (*original)(void))
{
    uint32_t e,id;
    if(!MAPMAKER_STACK_OK(8) || (MAPMAKER_ACTORS_B(0x007DE458u)&0x40u)) { original(); return; }
    e=WORD_AT(g_esp+4);
    if(!MAPMAKER_PTR_OK(e,0x10)) { original(); return; }
    id=WORD_AT(e+8u);
    if(id<MAPMAKER_ACTORS_SCORE_WORDS || id>=MAPMAKER_ACTORS_MAX) { original(); return; }
    g_eax=0;
    MAPMAKER_GUEST_RET();
}
