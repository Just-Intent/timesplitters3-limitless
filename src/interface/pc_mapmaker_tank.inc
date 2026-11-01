/* US 45410066 MapMaker: the castle tank boss of story mission 2 in MapMaker
 * story play (scene 100, story mode). Area `tank`; every hook is inert unless
 * TSFP_MAPMAKER_LIMITS=1 (opt out of the tank alone with TSFP_MAPMAKER_TANK=0).
 * Docs: docs/mapmaker/mapmaker-tank.md. Research basis:
 * logs/limits/features/real-bosses.md section 4 (main checkout, ignored).
 * CONFIRMED facts are checked by tests/mapmaker/test_mapmaker_tank.py.
 *
 * The tank is a prop, not a character: model 0x281 CASTLETANKBOSS (resource
 * type 0x8000000, castle/props/tankboss, init index 26 -> sub_002560C0), with
 * the turret 0x297 and the wreck 0x298 it creates when it dies. All three
 * are rows of the scene-100 resource table (prop_all_xbas) but their model
 * files are only in pak/stream/lv4/prop6.pak (string 0x0049DF7C). Its whole
 * fight state is static, so this area keeps one copy per tank (up to
 * TNK_MAX per map) and swaps it in around every entry into the tank's code:
 *  - the block 0x0078B560..0x0078B5C3 (sub_00251AA0 zeroes these 0x19 words;
 *    0x0078B5E0..F4 belong to the castle script sub_002532D0 only) and the
 *    four sound wobble records 0x0075F468..0x0075F4D7 (sub_000E1100/E1200,
 *    plain numbers) are copied in on entry and out on return;
 *  - the three tables the block points at, parts 0x0051B920 (0x78), drive
 *    0x0051B998 (0x30) and guns 0x0051B9C8 (0x17C), contiguous to 0x0051BB44,
 *    get a guest copy per tank (allocated once per session) that the tank's
 *    block points at, so their addresses never change for a tank; the
 *    read-only tags 0x0051BB44 and courtyard 0x0051BB58 stay shared;
 *  - entries: the update (slot 3), slot 7 sub_00253840 and slot 2
 *    sub_00253D60 (object at [esp+4]), the damage sub_00255FD0 (record at
 *    [esp+4], object at +0), the boss bar's timer sub_00251800 (the tank in
 *    player 1's focus) and the spawn itself (the init resets the statics and
 *    the XBE tables: both are restored afterwards). The castle's own callers
 *    (sub_001E4030 -> sub_00256040 for its 'KNAT' object, the scripts
 *    sub_001E77E0 and sub_002532D0) only run in the castle level.
 * After every entry the statics hold exactly what they held before, so no
 * other code (another boss's swap included) sees a tank's copy.
 *
 * Placement (no new save bytes): a Story AI whose character is the
 * placeholder id TNK_ID 0x12 (CHR_PRISON_GUARDROBOT, a story-only id with no
 * Arcade row and in no story set). The map stores it as a stock custom-set
 * member; a stock or older build plays it as an ordinary guard robot. The
 * editor picker row that writes this id is the integrator's (see the doc).
 *
 * Play, per level:
 *  - mount (sub_00062790, the level's pak mounts, once per level load from
 *    sub_001CDB80): after the original, forget the previous level's tank;
 *    when the map's own custom set holds TNK_ID (or the map container cannot
 *    be read), prop6.pak is not mounted and the 30-slot mount table keeps two
 *    free, mount it with sub_000622D0(path, 0, 1): kept open, entries read on
 *    demand, as chr.pak (0x0006298A). The stock bulk unmount (sub_00062720)
 *    removes it with the rest.
 *  - request (sub_0015E7C0, story level init, inside the level setup after
 *    the resource table is built): request the tank, turret and wreck models
 *    (sub_000A5E40) so the scene loop's load pass (sub_00063390) reads them
 *    from prop6.pak; a MapMaker map requests only its own objects' models and
 *    sub_00096410 finds a model only through its request.
 *  - ai_start (sub_0015B830): after the original, for each pool entry
 *    whose character (a+0x48) is TNK_ID, up to TNK_MAX: when the pak is mounted and the
 *    resource map (0x008CA000, moved by the rooms area) has 0x281 (init 0x002560C0), 0x297 and 0x298,
 *    and each of the three is loaded or its request holds the read file
 *    (sub_000631C0 -> +0x10; sub_00096410 takes it without a null test),
 *    create the tank at the placeholder's position, facing its yaw:
 *    sub_000A6F90(0x281, &pos, &rot, 0, 0) (pos -> +0x64, rot -> +0xC0/+0x70/
 *    +0xC8, sub_000A6880; the init runs inside, sub_000A6EC0 0x000A6F3A).
 *    The castle frame offset d = spawn - TNK_C (the castle arena centre) is
 *    kept per tank; the init's first drive target (-9, y, -3.2) is moved by d.
 *    A placeholder past TNK_MAX keeps its bot behaviour (logged).
 *  - update (sub_00255550, the tank's per-frame callback, slot 3): on the
 *    first frame, the placeholder AI is removed (sub_0015DDA0, after checking
 *    it is still that AI) so only the tank stands there.
 *  - wake (sub_00253140, the fight's start test in state 0): stock looks up
 *    the castle object 'RDPC' (sub_000B2350 -> 0 in a MapMaker map, so the
 *    fight never starts) and then fires level marker 5 (sub_00227650(5, 3.0)).
 *    For our tank: 1 when a living player stands within TNK_WAKE metres
 *    (x/z) of it, else 0; no marker.
 *  - area (sub_00251CD0, ecx = point -> inside the courtyard: the union of
 *    three castle rectangles 0x0051BB58.., x -21.8..-3, z -10..13): the
 *    stock test with the point moved by -d, so the courtyard sits around the
 *    spawn (inside the castle-frame calls below the point is already moved).
 *    Player inside -> state 6, tank outside -> state 8; the drive target
 *    search in sub_00254690 (0x002547E0..0x002548A2) only counts points
 *    inside and loops until it has 10 of them or one 10 m from the tank:
 *    v0.4.0-mapmaker.22 answered 0 here and that loop never ended (freeze
 *    right after the fight started). As a guard, after TNK_SEARCH_MAX
 *    points outside in one search every point counts (logged).
 *  - target (sub_00254690, edi = tank: picks the drive target [0x0078B580..88]
 *    from castle constants: arena box x -19.1..-5, z -10..13 and fixed points)
 *    and state (sub_00255390, eax = tank, on entering state 7 "player below
 *    the courtyard": writes castle aim points into the five gun records
 *    [0x0078B560] + k*0x4C + 0x10): run in the castle frame. The tank and its
 *    target entity ([0x0078B5B0]) are moved by -d for the call and restored
 *    exactly; the drive target is handed in and out in castle coordinates;
 *    state 7's aim points are moved by +d after. Both callees only compute
 *    (sub_00153B40 random, sub_00251CD0), so nothing else sees the shift.
 *  - wreck (sub_00255C20, esi = tank, the death): stock creates the wreck and
 *    the flying turret, then parks the dead tank at the castle point
 *    (0, 20, 0) (0x00255DED..0x00255DF7). For our tank: parked TNK_PARK
 *    metres under its spawn point instead.
 *  - event (sub_002274C0): state 9 fires level event 0x16 every frame (call
 *    0x00255A8F, return 0x00255A94); in MapMaker that is the map's own logic
 *    marker 0x16: dropped in scene 100. No other level event is reachable from
 *    the tank's code (test; the castle script callback sub_002532D0 is only
 *    installed by the castle level setup sub_001E9CC0).
 *  - gun (sub_002522B0, ebx = gun 0..4, edi = tank: turns a gun toward its
 *    aim point, record +0x10, at a limited rate): in state 4 the cannon
 *    (gun 4) gets its aim point only from sub_00251DD0 (its +0x38), at the
 *    state entry and after each shot: a castle cover object near the target
 *    (none in a map) or the target's actor +0xBB4. The machine guns
 *    (sub_00251EF0) use the player record +0xB4 for a player instead; the
 *    v0.4.0-mapmaker.23 playtest saw the cannon aim off the player. For our
 *    tank in state 4 with a player target, the cannon's aim point is set to
 *    that player's +0xB4 point before every turn, so it tracks the player
 *    while it prepares to fire; each new stock aim (once per shot) is
 *    logged against the player's position.
 *  - text (sub_0003D7B0): the boss bar for model 0x281 (sub_0014CC80, focus
 *    set by the update at 0x002555AA) names its two meters with text 0x165B
 *    (return 0x0014CD42) and 0x1635 (0x0014CD83, the state-9 timer from
 *    sub_00251800); in scene 100 those ids are the MapMaker level's own text,
 *    so our labels are returned there.
 * The wake, area, target, state, gun and wreck hooks run inside a tank's
 * entry and act on the tank whose state is in the statics.
 * Not rebased (documented): the update's height tests (player y < -1.0 enters
 * state 7 at 0x002556BE, y <= -2.4 keeps it at 0x0025590D) use absolute
 * heights; the castle courtyard floor is INFERRED at y 0.
 *
 * Requires the includer's WORD_AT, guest_call, g_esp/g_eax/g_ecx/g_esi/g_edi,
 * g_xbox_mem_offset, xbox_HeapAlloc and pc_mapmaker_limits.inc
 * (mapmaker_limits, MAPMAKER_GUEST_RET, MAPMAKER_PTR_OK). Hot hook: text
 * (every text lookup) tests fixed values first. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif

#define TNK_SCENE 0x0079094Cu
#define TNK_MODE 0x007DE455u             /* byte: 10 = story */
#define TNK_ID 0x12u                     /* placeholder character: CHR_PRISON_GUARDROBOT */
#define TNK_MODEL 0x281u                 /* CASTLETANKBOSS */
#define TNK_TURRET 0x297u                /* CASTLETANKBOSSTURRET, made at its death */
#define TNK_WRECK 0x298u                 /* CASTLETANKBOSSWRECK */
#define TNK_INIT 0x002560C0u             /* resource init index 26 */
/* int16 per model: resource row or -1. Stock 0x0078B860 (0x9D0 words); the
 * rooms area's guest-code patches move every access, the builder's included
 * (sub_00242780 0x002427C8/0x00242A34) and the spawn path's (0x000A6F94,
 * 0x000A6EC4, 0x000A688F), to 0x008CA000 (0xA00 words): the stock address is
 * never written (v0.4.0-mapmaker.20 playtest read a stale row there). */
#define TNK_RESMAP 0x008CA000u
#define TNK_RESMAP_MODELS 0xA00u
#define TNK_RES 0x00791D4Cu              /* resource table, 0x30 per entry */
#define TNK_RES_INIT 0x2Cu
#define TNK_PAK 0x0049DF7Cu              /* "pak/stream/lv4/prop6.pak" */
#define TNK_MOUNTS 0x006D7880u           /* mounted paks, table of 30 (0x000620F9) */
#define TNK_MOUNT_MAX 64u               /* paktable.json: 64 slots at 0x00907000 */
#define TNK_MOUNT_KEEP 8u
#define TNK_MOUNT_SKIP 0x006D7C4Cu       /* sub_00062790 does nothing when set */
#define TNK_CONTAINER 0x00784014u
#define TNK_MAGIC 0x00C77667u
#define TNK_TAG_INFO 0x00C77668u
#define TNK_TAG_END 0x00C7766Au
#define TNK_TAG_SETS 0x00C776F5u         /* pc_mapmaker_sets.inc: slots 11-20 */
#define TNK_SCAN 0x00080000u
#define TNK_INFO_SET 0x14u
#define TNK_INFO_COUNT 0x16u
#define TNK_INFO_MEMBERS 0x18u
#define TNK_INFO_SLOTS 10u
#define TNK_SET_MAX 20u
#define TNK_SET_MAP 0x11u
#define TNK_SETS_SIZE 0x30u
#define TNK_ACTOR 0xEA0u
#define TNK_ACTOR_CHAR 0x48u
#define TNK_ACTOR_RECORD 0xBF0u
#define TNK_ENT_AI 0x7Cu
#define TNK_ENT_POS 0x64u                /* x, y (up), z */
#define TNK_ENT_YAW 0x70u
#define TNK_ENT_HEALTH 0x1CCu
#define TNK_PLAYER 0x007B0C7Cu
#define TNK_FOCUS 0xFF4u
#define TNK_PLAYERS 0x00790950u          /* count */
#define TNK_PLAYER_BASE 0x007B0C48u      /* players, 0x1584 each, entity +0x14 */
#define TNK_PLAYER_STRIDE 0x1584u
#define TNK_PLAYER_ENT 0x14u
#define TNK_WAKE 30.0f                   /* metres (x/z) */
#define TNK_STATE 0x0078B594u            /* fight state 0..10 */
#define TNK_PREV 0x0078B598u             /* state handled by sub_00255390 */
#define TNK_TARGET 0x0078B5B0u           /* target entity (a player) */
#define TNK_GOAL 0x0078B580u             /* drive target x, y, z */
#define TNK_GUNS 0x0078B560u             /* 5 records of 0x4C, aim point +0x10 */
#define TNK_GUN_STRIDE 0x4Cu
#define TNK_GUN_AIM 0x10u
#define TNK_GUN_COUNT 5u
#define TNK_STATE_LOW 7u                 /* "player below the courtyard" */
#define TNK_STATE_GUNS 4u                /* "gun attack": the cannon fires */
#define TNK_CANNON 4u                    /* gun record 4: the cannon */
#define TNK_ACTOR_PLAYER 0x8u            /* actor: player index (sub_00251EF0 0x00251F04) */
#define TNK_PLAYER_AIM 0xB4u             /* player record: aim point x, y, z */
#define TNK_AIM_LOGS 32u                 /* cannon aim lines per level */
#define TNK_YARD 0x0051BB58u             /* courtyard rectangles x1, z1, x2, z2 (castle frame) */
#define TNK_YARD_COUNT 3u
#define TNK_RET_SEARCH 0x00254848u       /* sub_00254690: the search's call to sub_00251CD0 */
#define TNK_SEARCH_MAX 64u               /* points outside before every point counts */
#define TNK_LV_LOGS 48u                  /* fight-state and search lines per level */
#define TNK_MAX 4u                       /* castle tanks per map */
#define TNK_BLOCK 0x0078B560u            /* the fight statics sub_00251AA0 zeroes */
#define TNK_BLOCK_WORDS 0x19u
#define TNK_BLOCK_TABLES 3u              /* block +0 guns, +4 drive, +8 parts: point into TNK_TABS */
#define TNK_SOUNDS 0x0075F468u           /* 4 sound wobble records of 0x1C (to 0x0075F4D8) */
#define TNK_SOUNDS_WORDS 0x1Cu
#define TNK_TABS 0x0051B920u             /* parts 0x78, drive 0x30, guns 0x17C: copied per tank */
#define TNK_TABS_SIZE 0x224u
#define TNK_CX (-12.05f)                 /* castle arena centre: x -19.1..-5 */
#define TNK_CY 0.0f                      /* courtyard floor (INFERRED) */
#define TNK_CZ 1.5f                      /* z -10..13 */
#define TNK_PARK (-50.0f)                /* dead tank: this far under its spawn */
#define TNK_RET_EVENT 0x00255A94u        /* sub_00255550: call sub_002274C0(0x16, 0) at 0x00255A8F */
#define TNK_BAR_TEXT 0x165Bu
#define TNK_RET_BAR_TEXT 0x0014CD42u
#define TNK_TIMER_TEXT 0x1635u
#define TNK_RET_TIMER_TEXT 0x0014CD83u
#define TNK_LABEL "Castle Tank"
#define TNK_TIMER_LABEL "Tank exposed"
#define TNK_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define TNK_H(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

#define TNK_RES_LOADED 0x4u              /* row: model already loaded */
#define TNK_RES_PATH 0x10u               /* row: model path ("castle/props/tankboss") */
#define TNK_RES_LOD 0x14u                /* row: second model path or 0 */
#define TNK_REQ_DATA 0x10u               /* request (sub_000631C0): loaded file or 0 */
static const uint32_t tnk_models[3]={TNK_MODEL,TNK_TURRET,TNK_WRECK};

extern void sub_00062260(void),sub_000622D0(void),sub_000A6F90(void),sub_0015DDA0(void);
extern void sub_000A5E40(void),sub_000631C0(void);
extern uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment);

/* Kept for the session: settings, labels and the spawn argument buffer. */
static struct { int read, optout; uint32_t label, timer, args, tabs; unsigned logged; } tnk;
/* One tank: its object and placeholder, castle frame, and its copy of the statics. */
struct tnk_one {
    uint32_t obj, place, place_ent, place_rec;
    float d[3], spawn[3];
    unsigned wakes, misses, aims;        /* misses: points outside in this search */
    uint32_t seen_state;                 /* last fight state logged */
    uint32_t cannon[3];                  /* the cannon aim point last set here */
    uint32_t block[TNK_BLOCK_WORDS], sounds[TNK_SOUNDS_WORDS];
};
/* This level's tanks (cleared by every level's pak mount). */
static struct {
    struct tnk_one t[TNK_MAX];
    unsigned n;                          /* tanks created */
    unsigned act;                        /* 1 + the tank whose state is in the statics; 0: none */
    uint32_t rest[TNK_BLOCK_WORDS], rest_sounds[TNK_SOUNDS_WORDS];   /* the statics outside any tank */
    int shifted, requested;
    unsigned extras, events, logs;
} tnk_lv;
#define TK (tnk_lv.t[tnk_lv.act-1u])     /* the active tank (only with tnk_lv.act) */
#define TNK_P(a) ((void *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

static void tnk_log(const char *what,uint32_t a,uint32_t b)
{
    if(tnk.logged>=24u) return;
    tnk.logged++;
    fprintf(stderr,"[mapmaker] tank: %s (0x%X, 0x%X)\n",what,(unsigned)a,(unsigned)b);
    fflush(stderr);
}
/* Fight lines: their own budget per level, so the last state reached shows. */
static void tnk_lvlog(const char *what,uint32_t a,uint32_t b)
{
    if(tnk_lv.logs>TNK_LV_LOGS) return;
    if(tnk_lv.logs++==TNK_LV_LOGS) what="fight log budget used up for this level";
    fprintf(stderr,"[mapmaker] tank %u: %s (%u, %u)\n",tnk_lv.act,what,(unsigned)a,(unsigned)b);
    fflush(stderr);
}
/* Logs the fight state when it changed: (new state, previous). */
static void tnk_note_state(const char *where)
{
    uint32_t s=WORD_AT(TNK_STATE),was=TK.seen_state;
    if(s==was) return;
    TK.seen_state=s;
    tnk_lvlog(where,s,was);
}
static int tnk_on(void)
{
    mapmaker_limits_read();
    if(!tnk.read) {
        const char *v=getenv("TSFP_MAPMAKER_TANK");
        tnk.read=1;
        tnk.optout=v && v[0]=='0' && !v[1];
    }
    return mapmaker_limits.limits;
}
static int tnk_play(void) { return tnk_on() && WORD_AT(TNK_SCENE)==100u && TNK_B(TNK_MODE)==10u; }
/* 1 + the index of this level's tank obj, in scene 100; 0 for any other object. */
static unsigned tnk_find(uint32_t obj)
{
    unsigned k;
    if(!obj || WORD_AT(TNK_SCENE)!=100u || !tnk_on()) return 0;
    for(k=0;k<tnk_lv.n && k<TNK_MAX;k++) if(tnk_lv.t[k].obj==obj) return k+1u;
    return 0;
}
/* obj is the tank whose state is in the statics. */
static int tnk_mine(uint32_t obj) { return tnk_lv.act && obj && TK.obj==obj && WORD_AT(TNK_SCENE)==100u && tnk_on(); }

static void tnk_save(uint32_t *block,uint32_t *sounds)
{
    memcpy(block,TNK_P(TNK_BLOCK),TNK_BLOCK_WORDS*4u);
    memcpy(sounds,TNK_P(TNK_SOUNDS),TNK_SOUNDS_WORDS*4u);
}
static void tnk_load(const uint32_t *block,const uint32_t *sounds)
{
    memcpy(TNK_P(TNK_BLOCK),block,TNK_BLOCK_WORDS*4u);
    memcpy(TNK_P(TNK_SOUNDS),sounds,TNK_SOUNDS_WORDS*4u);
}
/* Puts tank k (1-based) in the statics; returns what was active before. */
static unsigned tnk_enter(unsigned k)
{
    unsigned p=tnk_lv.act;
    if(p==k) return p;
    if(p) tnk_save(tnk_lv.t[p-1u].block,tnk_lv.t[p-1u].sounds);
    else tnk_save(tnk_lv.rest,tnk_lv.rest_sounds);
    tnk_load(tnk_lv.t[k-1u].block,tnk_lv.t[k-1u].sounds);
    tnk_lv.act=k;
    return p;
}
/* Takes tank k out of the statics and puts back what tnk_enter found. */
static void tnk_leave(unsigned k,unsigned p)
{
    if(p==k) return;
    tnk_save(tnk_lv.t[k-1u].block,tnk_lv.t[k-1u].sounds);
    if(p) tnk_load(tnk_lv.t[p-1u].block,tnk_lv.t[p-1u].sounds);
    else tnk_load(tnk_lv.rest,tnk_lv.rest_sounds);
    tnk_lv.act=p;
}
/* The original with obj's state in the statics, when obj is one of ours. */
static void tnk_run(void (*original)(void),uint32_t obj)
{
    unsigned k=tnk_find(obj),p;
    if(!k) { original();return; }
    p=tnk_enter(k);
    original();
    tnk_leave(k,p);
}

static float tnk_getf(uint32_t a) { float f;memcpy(&f,(const void *)((uintptr_t)a+g_xbox_mem_offset),4);return f; }
static void tnk_setf(uint32_t a,float f) { memcpy((void *)((uintptr_t)a+g_xbox_mem_offset),&f,4); }
static int tnk_sane(float f) { return f==f && f>-100000.0f && f<100000.0f; }

static uint32_t tnk_string(uint32_t *slot,const char *text,size_t n)
{
    if(!*slot) {
        uint32_t p=xbox_HeapAlloc((uint32_t)n,4u);
        if(p && MAPMAKER_PTR_OK(p,n)) {
            memcpy((void *)((uintptr_t)p+g_xbox_mem_offset),text,n);
            *slot=p;
        } else tnk_log("no memory for a label",(uint32_t)n,0);
    }
    return *slot;
}

/* The map's own custom set (slots 1-10 in the map-info chunk, 11-20 in the
 * sets area's chunk), as pc_mapmaker_realbosses.inc reads it.
 * 1: holds TNK_ID; 0: does not (or a preset/profile set); -1: unreadable. */
static int tnk_map_has(void)
{
    uint32_t container=WORD_AT(TNK_CONTAINER),c,end,steps,info=0,isz=0,extra=0,n,k;
    if(!container || !MAPMAKER_PTR_OK(container,TNK_SCAN) || WORD_AT(container)!=TNK_MAGIC) return -1;
    c=container+8u;end=container+TNK_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u),d=c+8u;
        if(tag==TNK_TAG_END || sz>end-c-8u) break;
        if(tag==TNK_TAG_INFO && sz>=TNK_INFO_MEMBERS && !info) { info=d;isz=sz; }
        else if(tag==TNK_TAG_SETS && sz>=TNK_SETS_SIZE && !extra && TNK_H(d)==1u && TNK_B(d+2u)==TNK_INFO_SLOTS &&
                TNK_B(d+3u)>=1u && TNK_B(d+3u)<=TNK_SET_MAX-TNK_INFO_SLOTS) extra=d;
        c+=8u+sz;
    }
    if(!info) return -1;
    if(TNK_H(info+TNK_INFO_SET)!=TNK_SET_MAP) return 0;
    n=TNK_B(info+TNK_INFO_COUNT);
    if(n>TNK_INFO_SLOTS) n=TNK_INFO_SLOTS;
    if(TNK_INFO_MEMBERS+n*4u>isz) n=(isz-TNK_INFO_MEMBERS)/4u;
    for(k=0;k<n;k++) if(TNK_H(info+TNK_INFO_MEMBERS+k*4u)==TNK_ID) return 1;
    for(k=0;extra && k<TNK_B(extra+3u);k++) if(TNK_H(extra+4u+k*4u)==TNK_ID) return 1;
    return 0;
}
static int tnk_pak_mounted(void) { return (int32_t)guest_call(sub_00062260,1,TNK_PAK)!=-1; }
/* The resource row of a model, or 0 when scene 100's table lacks it. */
static uint32_t tnk_resource(uint32_t model)
{
    int16_t i;
    uint32_t t=WORD_AT(TNK_RES),e;
    if(model>=TNK_RESMAP_MODELS) return 0;
    i=(int16_t)TNK_H(TNK_RESMAP+model*2u);
    if(i<0 || !t) return 0;
    e=t+(uint32_t)i*0x30u;
    return MAPMAKER_PTR_OK(e,0x30u)?e:0;
}
/* A model path the level's load pass has read: its request (sub_000631C0,
 * keyed by the row's path) holds the file. sub_00096410 takes the request's
 * data without a null test (0x00096437 -> 0x0009651F), so a model whose
 * request is missing or empty must not be created. 1: ready. */
static int tnk_path_ready(uint32_t path,uint32_t *req)
{
    uint32_t r;
    *req=0;
    if(!path || !MAPMAKER_PTR_OK(path,4u)) return 0;
    r=guest_call(sub_000631C0,1,path);
    *req=r;
    return r && MAPMAKER_PTR_OK(r,0x14u) && WORD_AT(r+TNK_REQ_DATA)!=0;
}
/* 0 when the tank, turret and wreck can all be created, else the model that cannot. */
static uint32_t tnk_models_unready(uint32_t *row,uint32_t *req)
{
    uint32_t k,e,path;
    for(k=0;k<3u;k++) {
        *row=e=tnk_resource(tnk_models[k]);
        *req=0;
        if(!e) return tnk_models[k];
        if(WORD_AT(e+TNK_RES_LOADED)) continue;            /* loaded before: no file read */
        if(!tnk_path_ready(WORD_AT(e+TNK_RES_PATH),req)) return tnk_models[k];
        if((path=WORD_AT(e+TNK_RES_LOD))!=0 && !tnk_path_ready(path,req)) return tnk_models[k];
    }
    return 0;
}

/* WRAP sub_0015E7C0 (story level init, no args; called once by the level
 * setup sub_001CDB80 at 0x001CDC20). The level's resource rows exist by then
 * (sub_000A5C00 at 0x001CDBF3 runs the table builder sub_00242BA0 ->
 * sub_00242780), and the scene loop sub_0003D440 reads every requested file
 * right after the setup (sub_00063390 at 0x0003D5AD). Models are found only
 * through those requests (sub_00096410: sub_000631C0, else a lookup by the
 * bare row path that never matches), and a MapMaker map requests only its
 * own objects' models: request the tank, turret and wreck here
 * (sub_000A5E40(model): sub_00063A40 queues the .xbr of the row's path). */
void pc_mapmaker_tank_request(void (*original)(void))
{
    uint32_t k;
    original();
    if(!tnk_play() || tnk.optout || tnk_map_has()==0) return;
    if(!tnk_pak_mounted()) { tnk_log("prop6.pak not mounted at level init: tank models not requested",0,0);return; }
    for(k=0;k<3u;k++) {
        if(!tnk_resource(tnk_models[k])) { tnk_log("no resource row: tank models not requested",tnk_models[k],0);return; }
    }
    for(k=0;k<3u;k++) guest_call(sub_000A5E40,1,tnk_models[k]);
    tnk_lv.requested=1;
    tnk_log("requested the tank, turret and wreck models",TNK_MODEL,TNK_WRECK);
}

/* WRAP sub_00062790 (the level's pak mounts, no args). */
void pc_mapmaker_tank_mount(void (*original)(void))
{
    int has;
    uint32_t mounted;
    if(WORD_AT(TNK_MOUNT_SKIP)) { original();return; }    /* the original does nothing */
    original();
    memset(&tnk_lv,0,sizeof tnk_lv);                       /* a new level: forget the last tank */
    if(!tnk_play() || tnk.optout || (has=tnk_map_has())==0) return;
    if(tnk_pak_mounted()) return;
    if((mounted=WORD_AT(TNK_MOUNTS))+TNK_MOUNT_KEEP>TNK_MOUNT_MAX) {
        tnk_log("pak mount table full; the tank placeholder stays a bot",mounted,TNK_MOUNT_MAX);
        return;
    }
    if((int32_t)guest_call(sub_000622D0,3,TNK_PAK,0u,1u)<0) tnk_log("prop6.pak did not mount; the tank placeholder stays a bot",mounted,(uint32_t)has);
    else tnk_log(has<0?"mounted prop6.pak (map container unreadable)":"mounted prop6.pak for the castle tank",mounted+1u,0);
}

/* WRAP sub_0015B830 (cdecl AI pool entry): the placeholder becomes the tank. */
void pc_mapmaker_tank_ai_start(void (*original)(void))
{
    uint32_t a,ent,e,obj,args,i,tabs,w;
    float pos[3],yaw,f;
    struct tnk_one *t;
    if(!MAPMAKER_STACK_OK(8) || !tnk_play() || tnk.optout) { original();return; }
    a=WORD_AT(g_esp+4u);
    original();
    if(!MAPMAKER_PTR_OK(a,TNK_ACTOR) || TNK_H(a+TNK_ACTOR_CHAR)!=TNK_ID || !MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u)) return;
    for(i=0;i<tnk_lv.n;i++) if(tnk_lv.t[i].place_ent==ent && tnk_lv.t[i].obj) return;   /* started again */
    if(tnk_lv.n>=TNK_MAX) {
        if(tnk_lv.extras++<4u) tnk_log("at most 4 castle tanks per map: this placeholder stays a bot",a,tnk_lv.n);
        return;
    }
    if(!tnk_pak_mounted()) { tnk_log("prop6.pak not mounted: the tank placeholder stays a bot",a,0);return; }
    if(!(e=tnk_resource(TNK_MODEL)) || WORD_AT(e+TNK_RES_INIT)!=TNK_INIT || !tnk_resource(TNK_TURRET) || !tnk_resource(TNK_WRECK)) {
        tnk_log("tank models missing from the resource table: the placeholder stays a bot",e,e?WORD_AT(e+TNK_RES_INIT):0);
        tnk_log("resource rows of the tank / turret / wreck (row+1, 0 = none)",
                (uint32_t)(int16_t)TNK_H(TNK_RESMAP+TNK_MODEL*2u)+1u,
                ((uint32_t)(int16_t)TNK_H(TNK_RESMAP+TNK_TURRET*2u)+1u)<<16 | (((uint32_t)(int16_t)TNK_H(TNK_RESMAP+TNK_WRECK*2u)+1u)&0xFFFFu));
        return;
    }
    {
        uint32_t row,req,m=tnk_models_unready(&row,&req);
        if(m) {
            tnk_log(tnk_lv.requested?"a tank model was requested but not read: the placeholder stays a bot":
                                     "tank models not requested this level: the placeholder stays a bot",m,req);
            tnk_log("its resource row and request data (0 = none)",row,req && MAPMAKER_PTR_OK(req,0x14u)?WORD_AT(req+TNK_REQ_DATA):0);
            return;
        }
    }
    for(i=0;i<3u;i++) if(!tnk_sane(pos[i]=tnk_getf(ent+TNK_ENT_POS+i*4u))) { tnk_log("placeholder position unusable",a,i);return; }
    if(!tnk_sane(yaw=tnk_getf(ent+TNK_ENT_YAW))) yaw=0.0f;
    if(!tnk.args) {
        uint32_t p=xbox_HeapAlloc(32u,16u);
        if(!p || !MAPMAKER_PTR_OK(p,32u)) { tnk_log("no memory for the spawn arguments",32u,0);return; }
        tnk.args=p;
    }
    if(!tnk.tabs) {
        uint32_t p=xbox_HeapAlloc(TNK_MAX*TNK_TABS_SIZE,16u);
        if(!p || !MAPMAKER_PTR_OK(p,TNK_MAX*TNK_TABS_SIZE)) { tnk_log("no memory for the tank tables",TNK_MAX*TNK_TABS_SIZE,0);return; }
        tnk.tabs=p;
    }
    if(tnk_lv.act) { tnk_log("a tank is running: this placeholder stays a bot",a,tnk_lv.act);return; }
    args=tnk.args;
    t=&tnk_lv.t[tnk_lv.n];
    tabs=tnk.tabs+tnk_lv.n*TNK_TABS_SIZE;
    for(i=0;i<3u;i++) tnk_setf(args+i*4u,pos[i]);
    tnk_setf(args+12u,0.0f);tnk_setf(args+16u,yaw);tnk_setf(args+20u,0.0f);
    {   /* the init resets the statics and the XBE tables: keep both for after */
        static uint8_t keep_tabs[TNK_TABS_SIZE];
        uint32_t keep[TNK_BLOCK_WORDS],keep_sounds[TNK_SOUNDS_WORDS];
        tnk_save(keep,keep_sounds);
        memcpy(keep_tabs,TNK_P(TNK_TABS),TNK_TABS_SIZE);
        obj=guest_call(sub_000A6F90,5,TNK_MODEL,args,args+12u,0u,0u);
        if(obj && MAPMAKER_PTR_OK(obj,0x100u)) {
            memset(t,0,sizeof *t);
            tnk_save(t->block,t->sounds);
            memcpy(TNK_P(tabs),TNK_P(TNK_TABS),TNK_TABS_SIZE);
            for(i=0;i<TNK_BLOCK_TABLES;i++)                      /* the tank's own tables */
                if((w=t->block[i])>=TNK_TABS && w<TNK_TABS+TNK_TABS_SIZE) t->block[i]=tabs+(w-TNK_TABS);
        }
        tnk_load(keep,keep_sounds);
        memcpy(TNK_P(TNK_TABS),keep_tabs,TNK_TABS_SIZE);
    }
    if(!obj || !MAPMAKER_PTR_OK(obj,0x100u)) { tnk_log("the tank was not created (model not loaded?): placeholder stays a bot",a,obj);return; }
    t->obj=obj;t->place=a;t->place_ent=ent;t->place_rec=WORD_AT(a+TNK_ACTOR_RECORD);
    for(i=0;i<3u;i++) t->spawn[i]=pos[i];
    t->d[0]=pos[0]-TNK_CX;t->d[1]=pos[1]-TNK_CY;t->d[2]=pos[2]-TNK_CZ;
    w=(TNK_GOAL-TNK_BLOCK)/4u;                                     /* the init's first drive target */
    memcpy(&f,&t->block[w],4);f+=t->d[0];memcpy(&t->block[w],&f,4);
    memcpy(&f,&t->block[w+2u],4);f+=t->d[2];memcpy(&t->block[w+2u],&f,4);
    tnk_lv.n++;
    tnk_string(&tnk.label,TNK_LABEL,sizeof TNK_LABEL);              /* off the text hook's path */
    tnk_string(&tnk.timer,TNK_TIMER_LABEL,sizeof TNK_TIMER_LABEL);
    tnk_log("castle tank created at the placeholder",a,obj);
    fprintf(stderr,"[mapmaker] tank %u: spawn (%.2f, %.2f, %.2f), castle frame offset (%.2f, %.2f, %.2f)\n",tnk_lv.n,
            (double)pos[0],(double)pos[1],(double)pos[2],(double)t->d[0],(double)t->d[1],(double)t->d[2]);
    fflush(stderr);
}

/* WRAP sub_00255550 (cdecl tank; its per-frame callback). */
void pc_mapmaker_tank_update(void (*original)(void))
{
    uint32_t a,ent,player;
    unsigned k,p;
    if(!MAPMAKER_STACK_OK(8) || !(k=tnk_find(WORD_AT(g_esp+4u)))) { original();return; }
    p=tnk_enter(k);
    tnk_note_state("fight state at frame start (state, previous)");
    if(!TK.place) { original();tnk_leave(k,p);return; }
    a=TK.place;ent=TK.place_ent;TK.place=0;
    if(MAPMAKER_PTR_OK(a,TNK_ACTOR) && WORD_AT(a+TNK_ACTOR_RECORD)==TK.place_rec && WORD_AT(a)==ent &&
       MAPMAKER_PTR_OK(ent,0x100u) && WORD_AT(ent+TNK_ENT_AI)==a) {
        player=WORD_AT(TNK_PLAYER);
        if(MAPMAKER_PTR_OK(player,TNK_FOCUS+4u) && WORD_AT(player+TNK_FOCUS)==ent) WORD_AT(player+TNK_FOCUS)=0;
        guest_call(sub_0015DDA0,1,ent);
        tnk_log("tank placeholder AI removed",a,ent);
    } else tnk_log("tank placeholder AI already gone",a,ent);
    original();
    tnk_leave(k,p);
}

/* WRAP sub_00253840 (cdecl tank, arg; slot 7) and WRAP sub_00253D60 (cdecl
 * tank; slot 2): the tank's other callbacks, with its state. */
void pc_mapmaker_tank_slot7(void (*original)(void))
{
    if(!MAPMAKER_STACK_OK(8)) { original();return; }
    tnk_run(original,WORD_AT(g_esp+4u));
}
void pc_mapmaker_tank_slot2(void (*original)(void))
{
    if(!MAPMAKER_STACK_OK(8)) { original();return; }
    tnk_run(original,WORD_AT(g_esp+4u));
}

/* WRAP sub_00255FD0 (cdecl damage record; the hit object at +0). */
void pc_mapmaker_tank_damage(void (*original)(void))
{
    uint32_t r;
    if(!MAPMAKER_STACK_OK(8) || !MAPMAKER_PTR_OK(r=WORD_AT(g_esp+4u),4u)) { original();return; }
    tnk_run(original,WORD_AT(r));
}

/* WRAP sub_00251800 (no args -> the state-9 timer bar, x87): the boss bar
 * (sub_0014CC80 at 0x0014CD28) reads the tank in player 1's focus. */
void pc_mapmaker_tank_bar(void (*original)(void))
{
    uint32_t pl=WORD_AT(TNK_PLAYER);
    if(!MAPMAKER_PTR_OK(pl,TNK_FOCUS+4u)) { original();return; }
    tnk_run(original,WORD_AT(pl+TNK_FOCUS));
}

/* WRAP sub_00253140 (no args -> 1 start the fight): a living player is near. */
void pc_mapmaker_tank_wake(void (*original)(void))
{
    uint32_t n,base,k,ent;
    float x,z,dx,dz,h;
    if(!tnk_lv.act || !MAPMAKER_STACK_OK(4) || WORD_AT(TNK_SCENE)!=100u || !tnk_on()) { original();return; }
    g_eax=0;
    x=tnk_getf(TK.obj+TNK_ENT_POS);z=tnk_getf(TK.obj+TNK_ENT_POS+8u);
    n=WORD_AT(TNK_PLAYERS);base=WORD_AT(TNK_PLAYER_BASE);
    if(n>4u) n=4u;
    for(k=0;k<n && MAPMAKER_PTR_OK(base,TNK_PLAYER_STRIDE*4u);k++) {
        ent=WORD_AT(base+k*TNK_PLAYER_STRIDE+TNK_PLAYER_ENT);
        if(!MAPMAKER_PTR_OK(ent,0x200u) || !((h=tnk_getf(ent+TNK_ENT_HEALTH))>0.0f)) continue;
        dx=tnk_getf(ent+TNK_ENT_POS)-x;dz=tnk_getf(ent+TNK_ENT_POS+8u)-z;
        if(dx*dx+dz*dz<=TNK_WAKE*TNK_WAKE) { g_eax=1;break; }
    }
    if(g_eax && !TK.wakes++) tnk_log("castle tank fight started: a player came near",tnk_lv.act,TK.obj);
    MAPMAKER_GUEST_RET();
}

/* sub_00251CD0's test in the castle frame: strictly inside a rectangle. */
static int tnk_yard(float x,float z)
{
    uint32_t k,r;
    float x1,z1,x2,z2,t;
    for(k=0;k<TNK_YARD_COUNT;k++) {
        r=TNK_YARD+k*16u;
        x1=tnk_getf(r);z1=tnk_getf(r+4u);x2=tnk_getf(r+8u);z2=tnk_getf(r+12u);
        if(x1>x2) { t=x1;x1=x2;x2=t; }
        if(z1>z2) { t=z1;z1=z2;z2=t; }
        if(x2>x && x>x1 && z2>z && z>z1) return 1;
    }
    return 0;
}

/* WRAP sub_00251CD0 (ecx = point x, y, z -> inside the courtyard). */
void pc_mapmaker_tank_area(void (*original)(void))
{
    float x,z;
    if(!tnk_lv.act || !MAPMAKER_STACK_OK(4) || WORD_AT(TNK_SCENE)!=100u || !tnk_on() || !MAPMAKER_PTR_OK(g_ecx,12u)) { original();return; }
    x=tnk_getf(g_ecx);z=tnk_getf(g_ecx+8u);
    if(!tnk_lv.shifted) { x-=TK.d[0];z-=TK.d[2]; }
    g_eax=(uint32_t)tnk_yard(x,z);
    if(!g_eax && WORD_AT(g_esp)==TNK_RET_SEARCH && ++TK.misses>=TNK_SEARCH_MAX) {
        if(TK.misses==TNK_SEARCH_MAX) tnk_lvlog("drive target search found no courtyard point: taking any (state, misses)",WORD_AT(TNK_STATE),TK.misses);
        g_eax=1;
    }
    MAPMAKER_GUEST_RET();
}

/* The original in the castle frame: tank and target moved by -d and
 * restored; the drive target handed in and out in castle coordinates; with
 * guns, state 7's five aim points moved by +d afterwards. */
static void tnk_shifted(void (*original)(void),int guns)
{
    uint32_t tank=TK.obj,t=WORD_AT(TNK_TARGET),i,k,g,cas[3],old[3];
    float tk[3],tp[3];
    int tok=t && t!=tank && MAPMAKER_PTR_OK(t,0x100u);
    for(i=0;i<3u;i++) {
        tk[i]=tnk_getf(tank+TNK_ENT_POS+i*4u);tnk_setf(tank+TNK_ENT_POS+i*4u,tk[i]-TK.d[i]);
        if(tok) { tp[i]=tnk_getf(t+TNK_ENT_POS+i*4u);tnk_setf(t+TNK_ENT_POS+i*4u,tp[i]-TK.d[i]); }
        old[i]=WORD_AT(TNK_GOAL+i*4u);
        tnk_setf(TNK_GOAL+i*4u,tnk_getf(TNK_GOAL+i*4u)-TK.d[i]);
        cas[i]=WORD_AT(TNK_GOAL+i*4u);
    }
    tnk_lv.shifted=1;
    original();
    tnk_lv.shifted=0;
    for(i=0;i<3u;i++) {
        tnk_setf(tank+TNK_ENT_POS+i*4u,tk[i]);
        if(tok) tnk_setf(t+TNK_ENT_POS+i*4u,tp[i]);
        if(WORD_AT(TNK_GOAL+i*4u)==cas[i]) WORD_AT(TNK_GOAL+i*4u)=old[i];   /* not chosen: exactly as before */
        else tnk_setf(TNK_GOAL+i*4u,tnk_getf(TNK_GOAL+i*4u)+TK.d[i]);
    }
    if(guns && MAPMAKER_PTR_OK(g=WORD_AT(TNK_GUNS),TNK_GUN_STRIDE*TNK_GUN_COUNT))
        for(k=0;k<TNK_GUN_COUNT;k++)
            for(i=0;i<3u;i++) {
                uint32_t p=g+k*TNK_GUN_STRIDE+TNK_GUN_AIM+i*4u;
                tnk_setf(p,tnk_getf(p)+TK.d[i]);
            }
}

/* WRAP sub_00254690 (edi = tank; picks the drive target). */
void pc_mapmaker_tank_target(void (*original)(void))
{
    if(!tnk_mine(g_edi)) { original();return; }
    tnk_note_state("fight state at drive target (state, previous)");
    TK.misses=0;
    if(tnk_lv.shifted) { original();return; }
    tnk_shifted(original,0);
}

/* WRAP sub_00255390 (eax = tank; runs a state's entry once per change). */
void pc_mapmaker_tank_state(void (*original)(void))
{
    if(tnk_mine(g_eax)) tnk_note_state("fight state entry (state, previous)");
    if(!tnk_mine(g_eax) || WORD_AT(TNK_STATE)!=TNK_STATE_LOW || WORD_AT(TNK_PREV)==TNK_STATE_LOW) { original();return; }
    tnk_shifted(original,1);
}

/* WRAP sub_00255C20 (esi = tank; wreck and turret, then the tank is parked). */
void pc_mapmaker_tank_wreck(void (*original)(void))
{
    uint32_t tank=g_esi;
    original();
    if(!tnk_mine(tank)) return;
    tnk_setf(tank+TNK_ENT_POS,TK.spawn[0]);
    tnk_setf(tank+TNK_ENT_POS+4u,TK.spawn[1]+TNK_PARK);
    tnk_setf(tank+TNK_ENT_POS+8u,TK.spawn[2]);
    tnk_log("castle tank destroyed: wreck made, tank parked under its spawn",tnk_lv.act,tank);
}

/* The player record of the tank's target, or 0 when it is not a player. */
static uint32_t tnk_target_player(void)
{
    uint32_t t=WORD_AT(TNK_TARGET),act,n,base;
    int32_t idx;
    if(!t || !MAPMAKER_PTR_OK(t,0x100u)) return 0;
    act=WORD_AT(t+TNK_ENT_AI);
    if(!act || !MAPMAKER_PTR_OK(act,0x10u)) return 0;
    idx=(int32_t)WORD_AT(act+TNK_ACTOR_PLAYER);n=WORD_AT(TNK_PLAYERS);base=WORD_AT(TNK_PLAYER_BASE);
    if(idx<0 || (uint32_t)idx>=n || n>4u || !MAPMAKER_PTR_OK(base,TNK_PLAYER_STRIDE*n)) return 0;
    return base+(uint32_t)idx*TNK_PLAYER_STRIDE;
}

/* WRAP sub_002522B0 (no args; ebx = gun, edi = tank): the cannon tracks a
 * player target in state 4. */
void pc_mapmaker_tank_gun(void (*original)(void))
{
    uint32_t g,p,aim,i,t;
    if(g_ebx!=TNK_CANNON || !tnk_mine(g_edi) || WORD_AT(TNK_STATE)!=TNK_STATE_GUNS ||
       !MAPMAKER_PTR_OK(g=WORD_AT(TNK_GUNS),TNK_GUN_STRIDE*TNK_GUN_COUNT) || !(p=tnk_target_player())) { original();return; }
    aim=g+TNK_CANNON*TNK_GUN_STRIDE+TNK_GUN_AIM;
    if(WORD_AT(aim)!=TK.cannon[0] || WORD_AT(aim+4u)!=TK.cannon[1] || WORD_AT(aim+8u)!=TK.cannon[2]) {
        if(TK.aims<TNK_AIM_LOGS) {   /* stock chose a new aim point: once per shot and at the state entry */
            t=WORD_AT(TNK_TARGET);
            TK.aims++;
            fprintf(stderr,"[mapmaker] tank %u: cannon aim: stock (%.2f, %.2f, %.2f), set to player (%.2f, %.2f, %.2f), player at (%.2f, %.2f, %.2f)\n",tnk_lv.act,
                    (double)tnk_getf(aim),(double)tnk_getf(aim+4u),(double)tnk_getf(aim+8u),
                    (double)tnk_getf(p+TNK_PLAYER_AIM),(double)tnk_getf(p+TNK_PLAYER_AIM+4u),(double)tnk_getf(p+TNK_PLAYER_AIM+8u),
                    (double)tnk_getf(t+TNK_ENT_POS),(double)tnk_getf(t+TNK_ENT_POS+4u),(double)tnk_getf(t+TNK_ENT_POS+8u));
            fflush(stderr);
        }
    }
    for(i=0;i<3u;i++) WORD_AT(aim+i*4u)=TK.cannon[i]=WORD_AT(p+TNK_PLAYER_AIM+i*4u);
    original();
}

/* WRAP sub_002274C0 (cdecl event, arg): the tank's event 0x16. */
void pc_mapmaker_tank_event(void (*original)(void))
{
    if(!MAPMAKER_STACK_OK(12) || WORD_AT(g_esp)!=TNK_RET_EVENT || WORD_AT(TNK_SCENE)!=100u || !tnk_on()) { original();return; }
    if(tnk_lv.events++<4u) tnk_log("tank level event dropped (MapMaker logic marker untouched)",WORD_AT(g_esp+4u),WORD_AT(g_esp+8u));
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_0003D7B0 (cdecl text id -> string). Every text lookup: fixed
 * compares first, the original for everything else. */
void pc_mapmaker_tank_text(void (*original)(void))
{
    uint32_t id,ret;
    if(!MAPMAKER_STACK_OK(8)) { original();return; }
    id=WORD_AT(g_esp+4u);
    if(id!=TNK_BAR_TEXT && id!=TNK_TIMER_TEXT) { original();return; }
    ret=WORD_AT(g_esp);
    if(id==TNK_BAR_TEXT && ret==TNK_RET_BAR_TEXT && tnk.label && tnk_play()) g_eax=tnk.label;
    else if(id==TNK_TIMER_TEXT && ret==TNK_RET_TIMER_TEXT && tnk.timer && tnk_play()) g_eax=tnk.timer;
    else { original();return; }
    MAPMAKER_GUEST_RET();
}
