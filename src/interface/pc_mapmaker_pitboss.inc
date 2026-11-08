/* US 45410066 MapMaker: the giant skeleton's pit fight of story mission 6
 * (catacombs, scene 17) and its flying imps (ragbugs) as MapMaker Story AIs.
 * Area `pitboss`; every hook is inert unless TSFP_MAPMAKER_LIMITS=1 and acts
 * only in MapMaker story play (scene 100, story mode byte [0x007DE455] ==
 * 10). Docs: docs/mapmaker/mapmaker-pitboss.md. Tests:
 * tests/mapmaker/test_mapmaker_pitboss.py (fixture mapmaker_pitboss.c plus
 * decode, XBE data and story pak checks of every site below).
 *
 * Placement (no save change): two story creature rows
 * (pc_mapmaker_creatures.def) give the picker rows, the lv17_4 anim pak, the
 * look and the class: "Pit Skeleton" stores carrier 0x31 (a soldiermodel_talk
 * copy, no Arcade row, no story set) made with the giant skeleton 0x28's
 * model 0x5E9 and class 0x20000:0x10000000 (the catacombs 'PRIN' record);
 * "Ragbug" stores the imp's own id 0xB0 with class 0x200000:0x10000000 (the
 * 'RAG0'..'RAG5' records). This area runs the fight around them.
 *
 * The fight (CONFIRMED, decode): class 0x20000 -> sub_0029FE00, which in any
 * scene but 1 tail-jumps to the pit fight sub_0029E0A0 (17 states, table
 * 0x0029FDBC). It keeps everything in the skeleton block [0x0078B1EC]
 * (0x4C7C, sub_00287E00, the same block the mansion skeleton uses) and the
 * particle statics 0x0078B080.. (pc_mapmaker_bossfx.inc), and it reads the
 * catacombs arena from constants: the boss is pinned to (-11.8, -100.279,
 * 56.295) (0x0049F498/0x0049F494/0x0049F37C), the rack, the gas bottle, the
 * ragbugs' park and flight circle, the rack heights and four rim points are
 * more constants (PIT_REBASE). The boss cannot be hurt (message handler
 * sub_0028C560 answers -1 to 1/2); it carries a gas bottle 0x24A whose damage
 * callback sub_0028C2F0 takes its health +0x4274 (8.0) down; at 0 the boss
 * dies (state 14: explosion, princess_pit_death 0x241, every ragbug exploded
 * and removed, level event 2, then the props and the AI removed with
 * sub_0015DDA0). A timer +0x423C lowers the hostage rack (sub_0028B780 at
 * 20/50/80 s) and at the bottom the boss eats the hostage 'PRJB' (state 12,
 * 0x23F) and the mission fails (state 16, sub_00212160).
 *
 * In MapMaker, per instance (at most PIT_MAX per map, one per placed Story
 * AI, each with its own blocks, as the giant skeleton area does):
 *  - level (sub_0025F720 chain): blocks from the stock sub_00287E00 with the
 *    particle statics put back after; the arena constants' stock values.
 *  - mount (sub_00062790 chain): story/l_17_st.pak and storymem/l_17_stm.pak
 *    kept open (index only), for the props 0x246/0x249/0x24A/0x24B and their
 *    textures; request (sub_0015E7C0 chain) asks for the four models;
 *    textures (sub_00063900 chain) registers their textures the load
 *    pass's way (as the story props area).
 *  - princess (sub_0029FE00 chain): the pit fight with the instance's block
 *    and statics swapped in and the arena constants moved by (spawn point -
 *    the catacombs boss point) for the call; after it the rack timer is held
 *    at 0 (no hostage, no failure) and the 13 rim pads it looks up
 *    (0x4B5..0x4C1, sub_0009CC40, block +0x4BF0..) point at the instance's
 *    own pad records (the catacombs ring, moved). Its ragbugs fill the flock
 *    slots +0x4A30 (stock finds 'RAG0'..'RAG5' records). Not ready (no
 *    block, props not loaded): the AI is removed, logged.
 *  - logic (NEW WRAP sub_00234270, null-safe 'NMME' test): at 0x0029EDA5 0
 *    (the intro anim princess_cat_cs 0x23B plays) and at 0x0029EED1 1 once
 *    that animation has finished (the fight starts).
 *  - message (sub_0028D0A0 chain), the props' and boss's callbacks (NEW
 *    WRAPs sub_00289720, sub_00289E20, sub_0028C1C0, sub_0028C390,
 *    sub_0028C4A0, sub_0028C2F0) and the particle type callbacks (NEW
 *    WRAPs sub_00289080, sub_00288E20): the owning instance swapped in.
 *  - event (sub_002274C0 chain): event 2 at 0x0029FC38 dropped (the map's
 *    logic marker 2 untouched); failure (NEW WRAP sub_00212160): at
 *    0x0029FD15 dropped.
 *  - ragbug (NEW WRAP sub_0028B610, the class 0x200000 handler: it parks the
 *    bug at a catacombs point once): the bug belongs to the nearest placed
 *    pit skeleton with a free slot (6 each), parked with that one's offset;
 *    with no pit skeleton this level it is removed (it cannot fly alone:
 *    the flock is the boss's code).
 *  - restart (sub_001CDD60 chain): before the original the props are deleted
 *    and this map's lv17_4 anims noted (they carry 0x4000, which the restart
 *    unmaps); after it they are mapped again and every block is set up again
 *    in place (zeroed, sub_00287860), as the giant skeleton area does.
 *  - hud (sub_0014CC80 chain, inside pc_mapmaker_bosses_hud) and meter
 *    (sub_0013A400 chain): as for the real bosses: no stock bar or hostage
 *    meter; the Boss flag's bar, or the meter for the Sub-boss bar.
 *
 * Requires the includer's WORD_AT, guest_call, g_esp, g_eax,
 * g_xbox_mem_offset, xbox_HeapAlloc, pc_mapmaker_limits.inc and
 * pc_mapmaker_area_enabled. Statics are prefixed pit/PIT_. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif

#define PIT_SCENE 0x0079094Cu
#define PIT_MODE 0x007DE455u            /* byte: 10 = story */
#define PIT_ID 0x31u                    /* "Pit Skeleton" carrier (pc_mapmaker_creatures.def) */
#define PIT_BUG_ID 0xB0u                /* chrs/ragbug */
#define PIT_CLASS 0x20000u              /* record +4: sub_0029FE00 */
#define PIT_MAX 4u
#define PIT_BUGS 6u                     /* flock slots per boss */
#define PIT_BUG_MAX 24u
#define PIT_CONTAINER 0x00784014u
#define PIT_MAGIC 0x00C77667u
#define PIT_TAG_INFO 0x00C77668u
#define PIT_TAG_SAI 0x00C7766Cu
#define PIT_TAG_SETS 0x00C776F5u
#define PIT_TAG_END 0x00C7766Au
#define PIT_SCAN 0x00080000u
#define PIT_SET_MAP 0x11u
#define PIT_SLOTS 20u
#define PIT_INFO_SLOTS 10u
#define PIT_SAI_SLOT 0x28u              /* Story AI chunk: character slot 0..19 */
#define PIT_SAI_INDEX 0x22u
#define PIT_SAI_BOSS 0x29u              /* bosses area: 0xB0 | sub-boss 4 | level */
#define PIT_SAI_MIN 0x44u
#define PIT_SAI_BASE 0x00783A50u        /* Story AI ids are this + index + 1 */
#define PIT_BOSS_BAR 1u
#define PIT_BOSS_SUB 2u
#define PIT_ACTOR 0xEA0u
#define PIT_ACTOR_CHAR 0x48u
#define PIT_ACTOR_RECORD 0xBF0u
#define PIT_AI_STATE 0x20u              /* fight state (AI +0x20) */
#define PIT_ENT_AI 0x7Cu
#define PIT_ENT_ANIM 0x4u               /* animation controller (sub_00059DD0: finished) */
#define PIT_ENT_POS 0x64u
#define PIT_PLAYER 0x007B0C7Cu
#define PIT_FOCUS 0xFF4u
#define PIT_STATE 0x0078B1ECu           /* skeleton block, 0x4C7C (sub_00287E00) */
#define PIT_STATE_SIZE 0x4C7Cu
#define PIT_PARTS 0x0078B0D0u           /* its particle block, 0x7D00 (in the statics window) */
#define PIT_PARTS_SIZE 0x7D00u
#define PIT_ST_TIMER 0x423Cu            /* float: rack timer (sub_0028B780 stages at 20/50/80) */
#define PIT_ST_BOTTLE 0x4270u           /* gas bottle 0x24A */
#define PIT_ST_HEALTH 0x4274u           /* float: the bottle's health, 8.0 */
#define PIT_ST_COLL 0x4BCCu             /* collision 0x249 */
#define PIT_ST_BOSS 0x4BD0u             /* the boss entity */
#define PIT_ST_RACK 0x4BD8u             /* rack 0x246 */
#define PIT_ST_BANDAGE 0x4BDCu          /* bandage 0x24B */
#define PIT_ST_FLOCK 0x4A30u            /* 6 x 0x44: +0 ragbug entity */
#define PIT_FLOCK_STRIDE 0x44u
#define PIT_FLOCK_ACTIVE 0x24u           /* slot +0x24: launched (sub_0028A0A0 sets 1) */
#define PIT_FIGHT 6u                    /* fight states 6..13: the rack is up, the fight proper */
#define PIT_ROOMS 0x30u                 /* prop: int8 listed rooms, +0x38 up to 10 ids */
#define PIT_TRACE_EVERY 120u            /* frames between trace lines (60 a second) */
#define PIT_TRACES 24u
#define PIT_ENT_YAW 0x70u               /* entity yaw (radians); the fight sets 0 at its start (0x0029EA49) */
#define PIT_MOUTH_UP 11.0f              /* the mouth over the boss's feet (m66: eye bandage 12.35 up, 2.27 ahead) */
#define PIT_MOUTH_AHEAD 2.5f
#define PIT_HULL_CB 0x0028B5F0u         /* the hull's damage callback: the ragbug's (sets health 1.0): no damage, never destroyed */
#define PIT_BONE 0x29u                  /* the bottle's bone (0x0029E47E) */
#define PIT_BONE_FRESH 30u              /* fight frames a drawn bone point stays in use */
#define PIT_BONE_MIN 3.0f               /* a drawn mouth this far over the feet at least, else not trusted */
#define PIT_NEAR 8.0f                   /* ... and within this of the boss across (x/z): a world point */
#define PIT_EYE_MIN 6.0f                /* the eye bandage counts once this far over the feet */
#define PIT_EYE_TO_MOUTH 1.8f           /* the mouth under the eye bandage (.66/.77: eye 12.35 up, 2.27 ahead) */
#define PIT_SND_FIRE 0x56Cu             /* SFX_PRINCESS_PIT_FIREATTACK: sounddata flags 0x11000044, level 17 only */
#define PIT_SND_ROAR 0x56Du             /* SFX_PRINCESS_PIT_ROAR, 0x11000044 */
#define PIT_SND_LOAD 4u                 /* sub_00045670's "load it this level" */
#define PIT_ST_PADS 0x4BF0u             /* 13 pad indices (sub_0009CC40 of 0x4B5..0x4C1) */
#define PIT_PADS_N 13u
#define PIT_PAD_TABLE 0x00715F2Cu       /* -> pads, 0x20 each (+0 id, +4 1, +8 0x1A, +0xC x/y/z, +0x18 yaw) */
#define PIT_DEATH 14u                   /* fight states >= 14: dying (timer no longer counts) */
#define PIT_RET_NMME0 0x0029EDA5u       /* state 0: sub_00234270('NMME' item) */
#define PIT_RET_NMME1 0x0029EED1u       /* state 1 */
#define PIT_RET_EVENT 0x0029FC38u       /* state 14: sub_002274C0(2, 0) */
#define PIT_RET_FAIL 0x0029FD15u        /* state 16: sub_00212160(0) */
#define PIT_RET_METER 0x0014D264u       /* sub_0014D1F0: call sub_0013A400 */
#define PIT_RESMAP 0x008CA000u          /* int16 resource row per model (rooms area) */
#define PIT_RESMAP_MODELS 0xA00u
#define PIT_RES 0x00791D4Cu
#define PIT_MOUNTS 0x006D7880u
#define PIT_MOUNT_MAX 64u
#define PIT_MOUNT_KEEP 8u
#define PIT_MOUNT_SKIP 0x006D7C4Cu      /* sub_00062790 does nothing when set */
#define PIT_ANIM_PAK 0x0049D59Cu        /* "pak/stream/lv17/anim/lv17_4.pak" */
#define PIT_ANIM_FIRST 0x23Bu           /* princess_cat_cs .. ragbugrightdive 0x246 */
#define PIT_ANIM_N 12u
#define PIT_ANIM_MAP 0x007E03C0u
#define PIT_ANIM_PTRS 0x007E3954u
#define PIT_ANIM_WORDS 0x007DE660u
#define PIT_ANIM_NEXT 0x006D77F8u
#define PIT_ANIM_SLOTS 940u
#define PIT_ANIM_DROPPED 0x4000u
#define PIT_ANIM_FREE 0xFFFFu
#define PIT_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define PIT_H(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

extern void sub_000942B0(void);
extern void sub_0029E0A0(void),sub_0028C560(void),sub_00287E00(void),sub_00287860(void),sub_0015DDA0(void),
    sub_00059DD0(void),sub_000A9070(void),sub_000622D0(void),sub_00062260(void),sub_000A5E40(void),
    sub_000631C0(void),sub_00096220(void);
extern uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment);
int pc_mapmaker_area_enabled(const char *area,const char *hook);
#include "pc_mapmaker_bossfx.inc"

/* The arena constants the pit code reads (0x0049F34C..0x0049F498, and the
 * rim point's 0x0049E854) and the axis each one is: during an instance's
 * call each holds its stock value + sign * the instance's offset on that
 * axis (sign -1: the code subtracts it, e.g. x = r cos a - 11.8). The other
 * 0x0049F3xx/4xx words are times, speeds and angles (left alone). */
static const struct { uint32_t a; uint8_t axis; int8_t sign; } pit_rebase[]={
    {0x0049F498u,0,1},{0x0049F494u,1,1},{0x0049F37Cu,2,1},   /* the boss point (states 0, 7, 11) */
    {0x0049F380u,0,-1},{0x0049F378u,1,-1},                     /* flight circle centre (sub_0028A0A0, sub_0028AAA0) */
    {0x0049F390u,0,1},{0x0049F38Cu,1,1},{0x0049F388u,2,1},     /* ragbug rest point (sub_0028AAA0) */
    {0x0049F3A4u,1,1},                                         /* ragbug park y (sub_0028B610; x/z as above) */
    {0x0049F398u,1,1},{0x0049F394u,1,1},                       /* ragbug heights */
    {0x0049F3A8u,1,1},                                         /* "a player is out of the pit" (sub_0028B730) */
    {0x0049F3ACu,1,1},{0x0049F3B0u,1,1},{0x0049F3B4u,1,1},     /* rack stage heights (sub_0028B780) */
    {0x0049F45Cu,0,1},{0x0049F48Cu,1,1},{0x0049F458u,2,1},     /* rack spawn (state 1) */
    {0x0049F478u,1,1},                                         /* hostage drop y (state 12) */
    {0x0049F488u,0,1},{0x0049F484u,1,1},{0x0049F480u,2,1},     /* gas bottle again (state 6) */
    {0x0049F374u,0,1},{0x0049F370u,1,1},{0x0049F36Cu,2,1},     /* four rim points (sub_00288540) */
    {0x0049F368u,0,1},{0x0049F364u,1,1},{0x0049F360u,2,1},
    {0x0049F35Cu,0,1},{0x0049E854u,1,1},{0x0049F358u,2,1},
    {0x0049F354u,0,1},{0x0049F350u,1,1},{0x0049F34Cu,2,1},
};
#define PIT_REBASE_N (sizeof pit_rebase/sizeof pit_rebase[0])
/* The catacombs rim pads 0x4B5..0x4C1 (story/l_17_st.pak pad file): x, y, z, yaw. */
static const float pit_pad_xyz[PIT_PADS_N][4]={
    {-3.6750f,-94.4642f,64.4975f,212.0f},{-8.8316f,-95.3627f,66.9232f,87.0f},{-14.8249f,-96.2695f,66.9244f,87.0f},
    {-19.4575f,-96.0591f,64.4425f,87.0f},{-23.0405f,-95.5479f,59.4133f,87.0f},{-23.2569f,-95.1186f,53.6641f,87.0f},
    {-20.5313f,-95.5573f,50.0054f,34.0f},{-16.6589f,-95.8564f,45.5666f,87.0f},{-11.3781f,-96.5683f,43.5713f,87.0f},
    {-6.8119f,-96.7539f,46.2826f,87.0f},{-1.4412f,-96.0500f,49.1113f,87.0f},{0.3999f,-95.0545f,54.8525f,87.0f},
    {-0.4593f,-94.4096f,59.7972f,252.0f},
};
#define PIT_PAD_FIRST 0x4B5u
/* The rim pads go in the pad pool's last entries (pc_mapmaker_limits.inc:
 * MAPMAKER_POOL_PADS handles; the map's own use the first [0x007B6664]). The
 * ragbugs' dives read them through sub_0009A420(index), which sends 0x1F40..
 * 0x243F (scene 100) to the cover pool and returns 0 for a pad its check
 * sub_0009A330 refuses: the .66/.68 heap block's index was past 0x1F40, so the
 * dives had no target and the ragbugs hung in phase 4 (m68.err "4 4 4").
 * Since Layout C (pc_mapmaker_pads.inc, 2026-11-07) the pool is 16,384 and
 * the cover base 0x4000, so the tail (16,384 - 13 x (n + 1)) still decodes
 * as regular pads; the map's own pads keep the pool's front. */
#ifdef MAPMAKER_POOL_PADS
#define PIT_PAD_POOL MAPMAKER_POOL_PADS
#else
#define PIT_PAD_POOL 8000u
#endif
#define PIT_PAD_COUNT 0x007B6664u        /* the map's regular pads */
#define PIT_MODELS 4u
static const uint32_t pit_models[PIT_MODELS]={0x249u,0x24Bu,0x24Au,0x246u};   /* collision, bandage, bottle, rack */
static const char *const pit_paks[2]={"pak/story/l_17_st.pak","pak/storymem/l_17_stm.pak"};

typedef struct {
    uint32_t state, parts, record, ent, pads;
    float dx, dy, dz;
    int placed, started, ready, released;
    uint32_t props[4];                  /* the props last seen (bottle, collision, rack, bandage): logged when made */
    unsigned no_bottle, traces, hits, frames, mouthed, pads_full, hidden, bone_at, bone_src;
    float bone[3];
    unsigned drawn_bottle, drawn_bandage, drawn_boss, raw_logged, eye_logged;
    float eye[3];
    int eye_ok;
    uint8_t bug_phase[6];
    uint8_t boss;
    unsigned bugs, moves, last;
    MapmakerFx fx;
} PitInst;
typedef struct { uint32_t record, ent; int owner, slot, filled; } PitBug;

static struct {
    int read;
    unsigned logged;
    uint32_t paks[2];
} pit;
static struct {
    unsigned n, nbugs, removed, events, fails, waits, meters, textured;
    int want, pak_mounted[2];
    PitInst *cur;
    float base[PIT_REBASE_N];
    uint8_t fx_base[MAPMAKER_FX_SIZE];
    PitInst k[PIT_MAX];
    PitBug bug[PIT_BUG_MAX];
} pit_lv;
typedef struct { uint32_t ptr; PitInst *prev; MapmakerFx *fx; } PitSave;

static void pit_log(const char *what,uint32_t a,uint32_t b)
{
    if(pit.logged>=40u) return;
    pit.logged++;
    fprintf(stderr,"[mapmaker] pitboss: %s (0x%X, 0x%X)\n",what,(unsigned)a,(unsigned)b);
    fflush(stderr);
}
static int pit_on(void) { mapmaker_limits_read(); return mapmaker_limits.limits; }
static int pit_play(void) { return pit_on() && WORD_AT(PIT_SCENE)==100u && PIT_B(PIT_MODE)==10u; }
static float pit_getf(uint32_t a) { float f;memcpy(&f,(const void *)((uintptr_t)a+g_xbox_mem_offset),4);return f; }
static void pit_setf(uint32_t a,float f) { memcpy((void *)((uintptr_t)a+g_xbox_mem_offset),&f,4); }
static int pit_sane(float f) { return f==f && f>-100000.0f && f<100000.0f; }

/* The map's own set, slots 1-20: 1 read, 0 a preset/profile set, -1 unreadable. */
static int pit_set(uint16_t ids[PIT_SLOTS])
{
    uint32_t container=WORD_AT(PIT_CONTAINER),c,end,steps,info=0,isz=0,extra=0,n,k;
    for(k=0;k<PIT_SLOTS;k++) ids[k]=0xFFFFu;
    if(!container || !MAPMAKER_PTR_OK(container,PIT_SCAN) || WORD_AT(container)!=PIT_MAGIC) return -1;
    c=container+8u;end=container+PIT_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u),d=c+8u;
        if(tag==PIT_TAG_END || sz>end-c-8u) break;
        if(tag==PIT_TAG_INFO && sz>=0x18u && !info) { info=d;isz=sz; }
        else if(tag==PIT_TAG_SETS && sz>=0x30u && !extra && PIT_H(d)==1u && PIT_B(d+2u)==PIT_INFO_SLOTS &&
                PIT_B(d+3u)>=1u && PIT_B(d+3u)<=PIT_SLOTS-PIT_INFO_SLOTS) extra=d;
        c+=8u+sz;
    }
    if(!info) return -1;
    if(PIT_H(info+0x14u)!=PIT_SET_MAP) return 0;
    n=PIT_B(info+0x16u);
    if(n>PIT_INFO_SLOTS) n=PIT_INFO_SLOTS;
    if(0x18u+n*4u>isz) n=(isz-0x18u)/4u;
    for(k=0;k<n;k++) ids[k]=PIT_H(info+0x18u+k*4u);
    for(k=0;extra && k<PIT_B(extra+3u);k++) ids[PIT_INFO_SLOTS+k]=PIT_H(extra+4u+k*4u);
    return 1;
}
static int pit_has(const uint16_t ids[PIT_SLOTS],uint32_t id)
{
    unsigned k;
    for(k=0;k<PIT_SLOTS;k++) if(ids[k]==id) return 1;
    return 0;
}
/* Story AIs placed with the pit skeleton: at least 1 (set member, no Story AI yet), at most PIT_MAX. */
static unsigned pit_count(const uint16_t ids[PIT_SLOTS])
{
    uint32_t container=WORD_AT(PIT_CONTAINER),c,end,steps,slot;
    unsigned n=0;
    c=container+8u;end=container+PIT_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u);
        if(tag==PIT_TAG_END || sz>end-c-8u) break;
        if(tag==PIT_TAG_SAI && sz>PIT_SAI_SLOT && (slot=PIT_B(c+8u+PIT_SAI_SLOT))<PIT_SLOTS && ids[slot]==PIT_ID) n++;
        c+=8u+sz;
    }
    return n<1u?1u:n>PIT_MAX?PIT_MAX:n;
}
/* The Story AI's Boss flag (bosses area, chunk +0x29): bar, sub-boss meter or none. */
static uint8_t pit_boss_flag(uint32_t index)
{
    uint32_t container=WORD_AT(PIT_CONTAINER),c,end,steps;
    if(!container || !MAPMAKER_PTR_OK(container,PIT_SCAN) || WORD_AT(container)!=PIT_MAGIC) return 0;
    c=container+8u;end=container+PIT_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u);
        if(tag==PIT_TAG_END || sz>end-c-8u) break;
        if(tag==PIT_TAG_SAI && sz>=PIT_SAI_MIN && PIT_H(c+8u+PIT_SAI_INDEX)==index) {
            uint8_t f=PIT_B(c+8u+PIT_SAI_BOSS);
            if((f&0xF8u)!=0xB0u) return 0;
            return (uint8_t)((f&4u)?PIT_BOSS_SUB:PIT_BOSS_BAR);
        }
        c+=8u+sz;
    }
    return 0;
}

/* ---- instances ---------------------------------------------------------- */
static void pit_in(PitInst *k,PitSave *s)
{
    s->prev=pit_lv.cur;
    s->fx=mapmaker_fx_enter(&k->fx);
    s->ptr=WORD_AT(PIT_STATE);
    WORD_AT(PIT_STATE)=k->state;
    pit_lv.cur=k;
}
static void pit_out(const PitSave *s)
{
    WORD_AT(PIT_STATE)=s->ptr;
    mapmaker_fx_leave(s->fx);
    pit_lv.cur=s->prev;
}
/* The arena constants for instance k (saved first), or back. */
static void pit_move(const PitInst *k,float *saved)
{
    unsigned i;
    for(i=0;i<PIT_REBASE_N;i++) {
        float d=pit_rebase[i].axis==0?k->dx:pit_rebase[i].axis==1?k->dy:k->dz;
        saved[i]=pit_getf(pit_rebase[i].a);
        pit_setf(pit_rebase[i].a,pit_lv.base[i]+(float)pit_rebase[i].sign*d);
    }
}
static void pit_back(const float *saved)
{
    unsigned i;
    for(i=PIT_REBASE_N;i-->0;) pit_setf(pit_rebase[i].a,saved[i]);
}
/* A new block for one instance: the stock init with the level's statics, the
 * instance keeps its statics copy, the live statics and pointer go back. */
static int pit_alloc(PitInst *k)
{
    uint8_t cur[MAPMAKER_FX_SIZE];
    uint32_t st,pt,home=WORD_AT(PIT_STATE);
    int ok;
    MapmakerFx *prev=mapmaker_fx_enter(0);
    memcpy(cur,(void *)((uintptr_t)MAPMAKER_FX+g_xbox_mem_offset),MAPMAKER_FX_SIZE);
    memcpy((void *)((uintptr_t)MAPMAKER_FX+g_xbox_mem_offset),pit_lv.fx_base,MAPMAKER_FX_SIZE);
    WORD_AT(PIT_STATE)=0u;WORD_AT(PIT_PARTS)=0u;
    guest_call(sub_00287E00,0);
    st=WORD_AT(PIT_STATE);pt=WORD_AT(PIT_PARTS);
    ok=st && pt && MAPMAKER_PTR_OK(st,PIT_STATE_SIZE) && MAPMAKER_PTR_OK(pt,PIT_PARTS_SIZE);
    k->state=ok?st:0u;k->parts=ok?pt:0u;
    if(ok) memcpy(k->fx.b,(void *)((uintptr_t)MAPMAKER_FX+g_xbox_mem_offset),MAPMAKER_FX_SIZE);
    memcpy((void *)((uintptr_t)MAPMAKER_FX+g_xbox_mem_offset),cur,MAPMAKER_FX_SIZE);
    WORD_AT(PIT_STATE)=home;
    mapmaker_fx_leave(prev);
    return ok;
}
/* The instance's blocks set up again in place (a restart keeps the Level heap). */
static int pit_again(PitInst *k)
{
    uint8_t cur[MAPMAKER_FX_SIZE];
    uint32_t home=WORD_AT(PIT_STATE);
    MapmakerFx *prev;
    if(!k->state || !k->parts || !MAPMAKER_PTR_OK(k->state,PIT_STATE_SIZE) || !MAPMAKER_PTR_OK(k->parts,PIT_PARTS_SIZE)) return 0;
    prev=mapmaker_fx_enter(0);
    memcpy(cur,(void *)((uintptr_t)MAPMAKER_FX+g_xbox_mem_offset),MAPMAKER_FX_SIZE);
    memcpy((void *)((uintptr_t)MAPMAKER_FX+g_xbox_mem_offset),pit_lv.fx_base,MAPMAKER_FX_SIZE);
    memset((void *)((uintptr_t)k->state+g_xbox_mem_offset),0,PIT_STATE_SIZE);
    memset((void *)((uintptr_t)k->parts+g_xbox_mem_offset),0,PIT_PARTS_SIZE);
    WORD_AT(PIT_STATE)=k->state;WORD_AT(PIT_PARTS)=k->parts;
    guest_call(sub_00287860,0);
    memcpy(k->fx.b,(void *)((uintptr_t)MAPMAKER_FX+g_xbox_mem_offset),MAPMAKER_FX_SIZE);
    memcpy((void *)((uintptr_t)MAPMAKER_FX+g_xbox_mem_offset),cur,MAPMAKER_FX_SIZE);
    WORD_AT(PIT_STATE)=home;
    mapmaker_fx_leave(prev);
    return 1;
}
/* The pit skeleton an entity is (its spawn record), 0 for any other. */
static PitInst *pit_of(uint32_t ent)
{
    uint32_t a,rec;
    unsigned k;
    if(!pit_lv.n || !MAPMAKER_PTR_OK(ent,0x100u) || !MAPMAKER_PTR_OK(a=WORD_AT(ent+PIT_ENT_AI),PIT_ACTOR)) return 0;
    rec=WORD_AT(a+PIT_ACTOR_RECORD);
    for(k=0;k<pit_lv.n;k++) if(pit_lv.k[k].record && pit_lv.k[k].record==rec) return &pit_lv.k[k];
    return 0;
}
static int pit_char(uint32_t ent,uint32_t id)
{
    uint32_t a;
    return MAPMAKER_PTR_OK(ent,0x100u) && MAPMAKER_PTR_OK(a=WORD_AT(ent+PIT_ENT_AI),PIT_ACTOR) && PIT_H(a+PIT_ACTOR_CHAR)==id;
}
/* The instance an object belongs to: its boss or one of its props. */
static PitInst *pit_owner(uint32_t obj)
{
    static const uint32_t f[5]={PIT_ST_BOSS,PIT_ST_BOTTLE,PIT_ST_COLL,PIT_ST_RACK,PIT_ST_BANDAGE};
    unsigned k,i;
    if(!obj) return 0;
    for(k=0;k<pit_lv.n;k++) {
        PitInst *p=&pit_lv.k[k];
        if(p->ent==obj) return p;
        for(i=0;p->state && i<5u;i++) if(WORD_AT(p->state+f[i])==obj) return p;
    }
    return 0;
}

/* The instance's pad records (13, rebased) and the indices that reach them in
 * the pad table: sub_00288230/sub_00288540 read [0x00715F2C] + index * 0x20. */
static void pit_pads(PitInst *k)
{
    uint32_t base=WORD_AT(PIT_PAD_TABLE),idx,p,i;
    unsigned n=(unsigned)(k-pit_lv.k);
    if(n>=PIT_MAX || !base) return;
    idx=PIT_PAD_POOL-PIT_PADS_N*(n+1u);                 /* the pool's tail: below the cover base, so sub_0009A420 reads the pad table */
    if(WORD_AT(PIT_PAD_COUNT)>idx || !MAPMAKER_PTR_OK(base+idx*0x20u,PIT_PADS_N*0x20u)) {
        if(!k->pads_full++) pit_log("pad pool too full for the rim pads: the fire and dives keep stock targets (pads, first free)",WORD_AT(PIT_PAD_COUNT),idx);
        return;
    }
    p=base+idx*0x20u;
    for(i=0;i<PIT_PADS_N;i++) {
        uint32_t r=p+i*0x20u;
        memset((void *)((uintptr_t)r+g_xbox_mem_offset),0,0x20u);
        WORD_AT(r)=PIT_PAD_FIRST+i;WORD_AT(r+4u)=1u;WORD_AT(r+8u)=0x1Au;
        pit_setf(r+0xCu,pit_pad_xyz[i][0]+k->dx);pit_setf(r+0x10u,pit_pad_xyz[i][1]+k->dy);
        pit_setf(r+0x14u,pit_pad_xyz[i][2]+k->dz);pit_setf(r+0x18u,pit_pad_xyz[i][3]);
        WORD_AT(k->state+PIT_ST_PADS+i*4u)=idx+i;
    }
    k->pads=p;
}

/* ---- ragbugs ------------------------------------------------------------ */
static PitBug *pit_bug_of(uint32_t ent)
{
    uint32_t a,rec;
    unsigned i;
    if(!MAPMAKER_PTR_OK(ent,0x100u) || !MAPMAKER_PTR_OK(a=WORD_AT(ent+PIT_ENT_AI),PIT_ACTOR)) return 0;
    rec=WORD_AT(a+PIT_ACTOR_RECORD);
    for(i=0;i<pit_lv.nbugs;i++) if(pit_lv.bug[i].record==rec) return &pit_lv.bug[i];
    return 0;
}
/* Nearest placed pit skeleton with a free flock slot, -1 none. */
static int pit_bug_owner(uint32_t ent)
{
    float x=pit_getf(ent+PIT_ENT_POS),y=pit_getf(ent+PIT_ENT_POS+4u),z=pit_getf(ent+PIT_ENT_POS+8u),best=0.f;
    int b=-1;
    unsigned k;
    for(k=0;k<pit_lv.n;k++) {
        PitInst *p=&pit_lv.k[k];
        float ex,ey,ez,d;
        if(!p->placed || p->bugs>=PIT_BUGS) continue;
        ex=x-(pit_lv.base[0]+p->dx);ey=y-(pit_lv.base[1]+p->dy);ez=z-(pit_lv.base[2]+p->dz);
        d=ex*ex+ey*ey+ez*ez;
        if(b<0 || d<best) { b=(int)k;best=d; }
    }
    return b;
}
/* Its bugs into the flock's empty slots (stock: found by 'RAGn' record tag,
 * flag bit 0 cleared); a slot the fight emptied after a fill stays empty. */
static void pit_flock(PitInst *k)
{
    unsigned i;
    for(i=0;i<pit_lv.nbugs;i++) {
        PitBug *b=&pit_lv.bug[i];
        uint32_t s;
        if(b->owner!=(int)(k-pit_lv.k) || b->slot<0 || !b->ent) continue;
        s=k->state+PIT_ST_FLOCK+(uint32_t)b->slot*PIT_FLOCK_STRIDE;
        if(!WORD_AT(s) && !b->filled) {
            WORD_AT(s)=b->ent;
            WORD_AT(b->ent+0x28u)&=0xFFFFFFFEu;
            b->filled=1;
        } else if(!WORD_AT(s) && b->filled) b->ent=0;     /* exploded and removed by the death */
    }
}

/* ---- the props' models -------------------------------------------------- */
static uint32_t pit_resource(uint32_t model)
{
    int16_t i;
    uint32_t t=WORD_AT(PIT_RES),e;
    if(model>=PIT_RESMAP_MODELS) return 0;
    i=(int16_t)PIT_H(PIT_RESMAP+model*2u);
    if(i<0 || !t) return 0;
    e=t+(uint32_t)i*0x30u;
    return MAPMAKER_PTR_OK(e,0x30u)?e:0;
}
static uint32_t pit_request(uint32_t path)
{
    uint32_t r;
    if(!path || !MAPMAKER_PTR_OK(path,4u)) return 0;
    r=guest_call(sub_000631C0,1,path);
    return r && MAPMAKER_PTR_OK(r,0x14u)?r:0u;
}
/* Every prop model the fight creates is loaded or read this level. */
static int pit_models_ready(uint32_t *missing)
{
    unsigned i;
    for(i=0;i<PIT_MODELS;i++) {
        uint32_t e=pit_resource(pit_models[i]),r;
        if(e && WORD_AT(e+4u)) continue;
        if(e && (r=pit_request(WORD_AT(e+0x10u)))!=0 && WORD_AT(r+0x10u)) continue;
        *missing=pit_models[i];
        return 0;
    }
    return 1;
}
static uint32_t pit_pak(unsigned j)
{
    if(!pit.paks[j]) {
        size_t n=strlen(pit_paks[j])+1u;
        uint32_t p=xbox_HeapAlloc((uint32_t)n,4u);
        if(!p || !MAPMAKER_PTR_OK(p,n)) { pit_log("no memory for a pak path",j,0);return 0; }
        memcpy((void *)((uintptr_t)p+g_xbox_mem_offset),pit_paks[j],n);
        pit.paks[j]=p;
    }
    return pit.paks[j];
}
static int pit_mounted(uint32_t path) { return path && (int32_t)guest_call(sub_00062260,1,path)!=-1; }

/* ---- level -------------------------------------------------------------- */
/* WRAP sub_00062790 (the level's pak mounts, no args): after the original,
 * forget the last level; when the map's set holds the pit skeleton, mount
 * l_17_st and l_17_stm kept open (index only), 8 table slots kept free. */
void pc_mapmaker_pitboss_mount(void (*original)(void))
{
    uint16_t ids[PIT_SLOTS];
    unsigned j;
    if(WORD_AT(PIT_MOUNT_SKIP)) { original();return; }
    original();
    if(!pit_play()) return;
    pit_lv.want=pit_set(ids)==1 && pit_has(ids,PIT_ID);
    pit_lv.pak_mounted[0]=pit_lv.pak_mounted[1]=0;
    pit_lv.textured=0;
    if(!pit_lv.want) return;
    for(j=0;j<2u;j++) {
        uint32_t used,p=pit_pak(j);
        if(!p) return;
        if(pit_mounted(p)) { pit_lv.pak_mounted[j]=1;continue; }
        if((used=WORD_AT(PIT_MOUNTS))+PIT_MOUNT_KEEP>PIT_MOUNT_MAX) { pit_log("pak mount table full: no pit props (used, pak)",used,j);return; }
        if((int32_t)guest_call(sub_000622D0,3,p,0u,1u)<0) { pit_log("story pak did not mount: no pit props (pak)",j,0);return; }
        pit_lv.pak_mounted[j]=1;
        fprintf(stderr,"[mapmaker] pitboss: mounted %s for the pit skeleton's props (table %u)\n",pit_paks[j],(unsigned)used+1u);
        fflush(stderr);
    }
}
/* WRAP sub_0015E7C0 (story level init, no args): after the original, ask for
 * the four prop models so the level's load pass reads them. */
void pc_mapmaker_pitboss_request(void (*original)(void))
{
    unsigned i;
    original();
    if(!pit_play() || !pit_lv.want || !pit_lv.pak_mounted[0]) return;
    for(i=0;i<PIT_MODELS;i++) guest_call(sub_000A5E40,1,pit_models[i]);
    pit_log("requested the pit props (collision, bandage, gas bottle, rack)",pit_models[0],pit_models[3]);
}
/* WRAP sub_00063900 (texture queue load, no args; only 0x0003D5E5): before
 * it, register each read prop model's textures the load pass's way when the
 * pass left them to the model's creation (request +2 & 0x10), which scene
 * 100 skips. */
void pc_mapmaker_pitboss_textures(void (*original)(void))
{
    unsigned i,n=0;
    if(pit_play() && pit_lv.want && pit_lv.pak_mounted[0] && !pit_lv.textured) {
        pit_lv.textured=1;
        for(i=0;i<PIT_MODELS;i++) {
            uint32_t e=pit_resource(pit_models[i]),path=0,r=0;
            if(e) r=pit_request(path=WORD_AT(e+0x10u));
            fprintf(stderr,"[mapmaker] pitboss: prop model 0x%X: %s, %s, %s\n",(unsigned)pit_models[i],
                    !e?"no resource row":WORD_AT(e+4u)?"loaded":"not loaded",!r?"no request":WORD_AT(r+0x10u)?"file read":"file not read",
                    !r?"-":(PIT_B(r+2u)&0x10u)?"textures left to its creation":"textures registered by the load pass");
            fflush(stderr);
            if(!e || !r || !WORD_AT(r+0x10u) || !(PIT_B(r+2u)&0x10u)) continue;
            guest_call(sub_00096220,1,path);
            PIT_B(r+2u)&=(uint8_t)~0x10u;
            n++;
        }
        pit_log("pit prop textures registered before the texture queue loads (models)",n,pit_lv.pak_mounted[1]);
    }
    original();
}
/* WRAP sub_0025F720 (story level init, no args): after the original, one
 * block per pit skeleton Story AI (stock sub_00287E00, the particle statics
 * as they were for each and put back after), the arena constants' stock
 * values; ragbugs forgotten. */
void pc_mapmaker_pitboss_level(void (*original)(void))
{
    uint16_t ids[PIT_SLOTS];
    unsigned k,n,i;
    original();
    mapmaker_fx_forget(pit_lv.k,sizeof pit_lv.k);
    pit_lv.n=pit_lv.nbugs=pit_lv.removed=pit_lv.events=pit_lv.fails=pit_lv.waits=pit_lv.meters=0;
    pit_lv.cur=0;
    memset(pit_lv.k,0,sizeof pit_lv.k);memset(pit_lv.bug,0,sizeof pit_lv.bug);
    pit.logged=0;
    if(!pit_play() || pit_set(ids)!=1 || !pit_has(ids,PIT_ID)) return;
    for(i=0;i<PIT_REBASE_N;i++) pit_lv.base[i]=pit_getf(pit_rebase[i].a);
    memcpy(pit_lv.fx_base,(void *)((uintptr_t)MAPMAKER_FX+g_xbox_mem_offset),MAPMAKER_FX_SIZE);
    n=pit_count(ids);
    for(k=0;k<n;k++) {
        if(!pit_alloc(&pit_lv.k[k])) { pit_log("no level memory for a pit skeleton (made, wanted)",k,n);break; }
        pit_lv.n=k+1u;
    }
    if(pit_lv.n && !WORD_AT(PIT_STATE)) WORD_AT(PIT_STATE)=pit_lv.k[0].state;   /* home, when no mansion skeleton holds it */
    if(pit_lv.n) pit_log("pit skeleton blocks allocated (first, count)",pit_lv.k[0].state,pit_lv.n);
}

/* WRAP sub_0015B830 (cdecl AI pool entry): after the original, note pit
 * skeletons (an instance per spawn record, its spawn point, its Boss flag)
 * and ragbugs. */
void pc_mapmaker_pitboss_ai_start(void (*original)(void))
{
    uint32_t a,rec,ent,id;
    unsigned k,i;
    if(!MAPMAKER_STACK_OK(8) || !pit_play()) { original();return; }
    a=WORD_AT(g_esp+4u);
    original();
    if(!MAPMAKER_PTR_OK(a,PIT_ACTOR) || !MAPMAKER_PTR_OK(rec=WORD_AT(a+PIT_ACTOR_RECORD),12u)) return;
    id=PIT_H(a+PIT_ACTOR_CHAR);ent=WORD_AT(a);
    if(id==PIT_BUG_ID) {
        for(i=0;i<pit_lv.nbugs && pit_lv.bug[i].record!=rec;i++) {}
        if(i==pit_lv.nbugs) {
            if(i>=PIT_BUG_MAX) { pit_log("ragbug table full (at most 24 a map)",rec,i);return; }
            pit_lv.nbugs++;
        }
        memset(&pit_lv.bug[i],0,sizeof pit_lv.bug[i]);
        pit_lv.bug[i].record=rec;pit_lv.bug[i].owner=-1;pit_lv.bug[i].slot=-1;
        return;
    }
    if(id!=PIT_ID || WORD_AT(rec+4u)!=PIT_CLASS) return;
    for(k=0;k<pit_lv.n && pit_lv.k[k].record!=rec;k++) {}
    if(k==pit_lv.n) for(k=0;k<pit_lv.n && pit_lv.k[k].record;k++) {}
    if(k==pit_lv.n && k<PIT_MAX && pit_lv.n) {          /* more placed than counted: a late block */
        memset(&pit_lv.k[k],0,sizeof pit_lv.k[k]);
        if(pit_alloc(&pit_lv.k[k])) pit_lv.n=k+1u;
        pit_log(pit_lv.n>k?"pit skeleton not counted at level init: its block allocated now":"no level memory for another pit skeleton",
                pit_lv.k[k].state,k);
    }
    if(k>=pit_lv.n) { pit_log("no pit skeleton block left (at most 4 a map): it will be removed",a,WORD_AT(rec+8u));return; }
    pit_lv.k[k].record=rec;pit_lv.k[k].ent=ent;
    pit_lv.k[k].boss=pc_mapmaker_area_enabled("bosses","pc_mapmaker_bosses_hud")?pit_boss_flag(WORD_AT(rec+8u)-WORD_AT(PIT_SAI_BASE)-1u):0u;
    if(MAPMAKER_PTR_OK(ent,0x100u) && pit_sane(pit_getf(ent+PIT_ENT_POS)) && pit_sane(pit_getf(ent+PIT_ENT_POS+4u)) &&
       pit_sane(pit_getf(ent+PIT_ENT_POS+8u))) {
        PitInst *p=&pit_lv.k[k];
        p->dx=pit_getf(ent+PIT_ENT_POS)-pit_lv.base[0];p->dy=pit_getf(ent+PIT_ENT_POS+4u)-pit_lv.base[1];
        p->dz=pit_getf(ent+PIT_ENT_POS+8u)-pit_lv.base[2];p->placed=1;
        fprintf(stderr,"[mapmaker] pitboss: pit skeleton %u placed at (%.2f, %.2f, %.2f): the catacombs arena moved by (%.2f, %.2f, %.2f)\n",
                k+1u,(double)pit_getf(ent+PIT_ENT_POS),(double)pit_getf(ent+PIT_ENT_POS+4u),(double)pit_getf(ent+PIT_ENT_POS+8u),
                (double)p->dx,(double)p->dy,(double)p->dz);
        fflush(stderr);
    }
}

/* Removes an AI this area cannot run (logged once per reason a level). */
static void pit_remove(uint32_t ent,const char *why)
{
    if(pit_lv.removed++<8u) pit_log(why,ent,pit_lv.removed);
    guest_call(sub_0015DDA0,1,ent);
}

static void pit_props(PitInst *k);
static void pit_props_rooms(PitInst *k,uint32_t ent);
static void pit_trace(PitInst *k,uint32_t ent,uint32_t st);
static void pit_mouth(PitInst *k,uint32_t ent,uint32_t st);
/* Playtest .68 (after .66's pose call): "even more glitched out and messes up
 * my camera view". sub_000A1B80 computes the pose for the view slot being set
 * up ([0x007B0CC0]+0x214, 0 = the player's camera) and is meant for the draw;
 * called from the AI frame it disturbed that view, and the bone point was
 * still at the feet or NaN (m68.err). Removed: the mouth is placed by
 * pit_mouth below instead. */
static void pit_flock_note(PitInst *k);
/* The collision hull 0x249 is never drawn in the story: its slot-4 callback
 * sub_0028C540 calls sub_000942B0(model instance, 8), which sets instance
 * +0x1F6 bit 8, and the prop draw sub_000A2740 skips a prop whose instance
 * has it (0x000A2829, 0x000A28C7). That slot is not run in MapMaker play
 * (playtest .73: "appears both at the base of the skeleton very large and
 * untextured, as well as the correct size and textured near the head": the
 * hull drawn at the boss's feet, the bottle at the mouth). After each fight
 * frame the same call hides it; it still takes hits (bottle callback). */
/* Playtest .75 ("an object floating above him that doesn't need to be there"):
 * the torture rack 0x246 hangs 22.5 over the spawn (the catacombs hostage
 * rack, m75.err "rack 0x246 made ... at (117.52, 22.99, 410.31)"); a MapMaker
 * map has no hostage, so it is hidden the same way. The fight still keeps it
 * (its stages and slot 2 read it). */
static void pit_hide(PitInst *k,uint32_t field,const char *what)
{
    uint32_t h=WORD_AT(k->state+field),inst;
    if(!h || !MAPMAKER_PTR_OK(h,0x200u) || !MAPMAKER_PTR_OK(inst=WORD_AT(h+4u),0x200u) || (PIT_B(inst+0x1F6u)&8u)) return;
    guest_call(sub_000942B0,2,inst,8u);
    if(k->hidden++<2u) pit_log(what,(uint32_t)(k-pit_lv.k)+1u,h);
}
static void pit_hull(PitInst *k)
{
    pit_hide(k,PIT_ST_COLL,"collision hull 0x249 hidden as the story's slot 4 does (skeleton, hull)");
    pit_hide(k,PIT_ST_RACK,"hostage rack 0x246 hidden (no hostage in MapMaker) (skeleton, rack)");
}
/* WRAP sub_0029FE00 (cdecl entity; class 0x20000): the pit fight for a pit
 * skeleton, with its block, statics and arena swapped in. The lifted fight
 * pops the caller's return address, as the stock tail jump. */
void pc_mapmaker_pitboss_princess(void (*original)(void))
{
    uint32_t ent,missing=0,a;
    PitInst *k;
    PitSave s;
    float saved[PIT_REBASE_N];
    if(!MAPMAKER_STACK_OK(8) || !pit_char(ent=WORD_AT(g_esp+4u),PIT_ID) || WORD_AT(PIT_SCENE)!=100u || !pit_on()) { original();return; }
    if(!(k=pit_of(ent))) { pit_remove(ent,"pit skeleton without a block this level: removed");MAPMAKER_GUEST_RET();return; }
    if(!k->placed) {
        k->dx=pit_getf(ent+PIT_ENT_POS)-pit_lv.base[0];k->dy=pit_getf(ent+PIT_ENT_POS+4u)-pit_lv.base[1];
        k->dz=pit_getf(ent+PIT_ENT_POS+8u)-pit_lv.base[2];k->placed=1;
    }
    if(!k->ready && !(k->ready=pit_models_ready(&missing))) { pit_remove(ent,"pit skeleton props not loaded this level: removed (model)");MAPMAKER_GUEST_RET();return; }
    k->ent=ent;
    if(!k->started) { k->started=1;pit_log("pit skeleton fight starts (skeleton, block)",(uint32_t)(k-pit_lv.k)+1u,k->state); }
    pit_flock(k);
    pit_in(k,&s);
    pit_move(k,saved);
    sub_0029E0A0();
    pit_back(saved);
    pit_out(&s);
    if(MAPMAKER_PTR_OK(a=WORD_AT(ent+PIT_ENT_AI),PIT_ACTOR)) {
        uint32_t st=WORD_AT(a+PIT_AI_STATE);
        if(st<PIT_DEATH) pit_setf(k->state+PIT_ST_TIMER,0.0f);   /* no hostage: the rack never comes down */
        if(st && st!=k->last && k->moves++<24u) pit_log("pit skeleton fight state (skeleton, state)",(uint32_t)(k-pit_lv.k)+1u,st);
        k->last=st;
        if(st) pit_pads(k);                               /* after state 0 looked the pads up */
        pit_props(k);
        pit_props_rooms(k,ent);
        pit_mouth(k,ent,st);
        pit_flock_note(k);
        pit_hull(k);
        pit_trace(k,ent,st);
    }
}

/* Each prop the fight makes, logged once when it appears (playtest .62: the
 * gas bottle was not seen): object, its listed rooms (+0x30, +0x38: none = in
 * no room, drawn only with the boss through pc_mapmaker_bossroom.inc) and
 * position; once if the fight runs on without a bottle. */
static void pit_props(PitInst *k)
{
    static const uint32_t f[4]={PIT_ST_BOTTLE,PIT_ST_COLL,PIT_ST_RACK,PIT_ST_BANDAGE};
    static const char *const what[4]={"gas bottle 0x24A","collision 0x249","rack 0x246","eye bandage 0x24B"};
    unsigned i,n=(unsigned)(k-pit_lv.k)+1u;
    for(i=0;i<4u;i++) {
        uint32_t o=WORD_AT(k->state+f[i]);
        if(o==k->props[i]) continue;
        k->props[i]=o;
        if(!o || !MAPMAKER_PTR_OK(o,0x100u)) continue;
        fprintf(stderr,"[mapmaker] pitboss: pit skeleton %u: %s made (object 0x%X, model 0x%X, %d listed rooms, first %u) at (%.2f, %.2f, %.2f)\n",
                n,what[i],(unsigned)o,(unsigned)PIT_H(o+0x94u),(int)*(int8_t *)((uintptr_t)(o+0x30u)+g_xbox_mem_offset),
                (unsigned)WORD_AT(o+0x38u),(double)pit_getf(o+0x64u),(double)pit_getf(o+0x68u),(double)pit_getf(o+0x6Cu));
        fflush(stderr);
    }
    if(!k->props[0] && k->last>=PIT_FIGHT && k->last<PIT_DEATH && k->no_bottle++==120u)
        pit_log("pit skeleton fights without its gas bottle (skeleton, state)",n,k->last);
}
/* Props hanging over the skeleton's tile list no room (a MapMaker tile is one
 * floor high), so no room's prop list holds them: not drawn and, INFERRED, not
 * hit (playtest .66: shooting the bottle did nothing, the fight never died).
 * After each fight frame a bottle, rack or bandage that lists no room gets the
 * boss's listed rooms (+0x30/+0x38) and its first one as room +0x34 (the
 * fight gives the bottle and rack the player's room, the catacombs pit), so
 * the next bucket pass links it where the boss stands. */
static void pit_props_rooms(PitInst *k,uint32_t ent)
{
    static const uint32_t f[3]={PIT_ST_BOTTLE,PIT_ST_RACK,PIT_ST_BANDAGE};
    int8_t n=*(int8_t *)((uintptr_t)(ent+PIT_ROOMS)+g_xbox_mem_offset);
    unsigned i,j;
    if(n<=0 || n>10) return;
    for(i=0;i<3u;i++) {
        uint32_t o=WORD_AT(k->state+f[i]);
        if(!o || !MAPMAKER_PTR_OK(o,0x100u) || (i && *(int8_t *)((uintptr_t)(o+PIT_ROOMS)+g_xbox_mem_offset)>0)) continue;   /* the bottle: always */
        *(int8_t *)((uintptr_t)(o+PIT_ROOMS)+g_xbox_mem_offset)=n;
        for(j=0;j<(unsigned)n;j++) WORD_AT(o+0x38u+j*4u)=WORD_AT(ent+0x38u+j*4u);
        WORD_AT(o+0x34u)=WORD_AT(ent+0x38u);
    }
}
/* The gas bottle at the mouth (playtests .62-.68: made and kept at the feet,
 * at NaN in states 4-7, m68.err). Stock puts it at model bone 0x29's point
 * (0x0029E46E..0x0029E517 then 0x0029E5EE..0x0029E6FC every frame) from the
 * boss's world bone matrices, which a MapMaker boss does not have posed when
 * the fight runs (they come from the draw, sub_000A1B80 per view). After each
 * fight frame (states 1..13) the bottle is put at bone 0x29's point as the
 * boss's last model draw left it (pc_mapmaker_pitboss_draw, after .75: the
 * fixed estimate was not at the mouth), or, when it was not drawn in the last
 * PIT_BONE_FRESH frames, at PIT_MOUTH_UP over its feet and PIT_MOUTH_AHEAD in
 * front (yaw +0x70: forward (sin, 0, cos)); facing as the boss; a NaN yaw or
 * tilt is cleared. Nothing the fight wrote stays NaN. The collision hull 0x249
 * gets the ragbug's health-reset callback 0x0028B5F0 (stock: none, so hits
 * destroyed it as a plain prop, .66): shots on the body do nothing (.75: the
 * user wants only the bottle to hurt it; the .68 quarter share is gone). */
static int pit_finite(float f) { return f==f && f>-1.0e6f && f<1.0e6f; }
static float pit_sin(float a)
{
    float x=a,t,r;
    int i;
    while(x>3.14159265f) x-=6.28318531f;
    while(x<-3.14159265f) x+=6.28318531f;
    for(t=x,r=x,i=1;i<8;i++) { t*=-x*x/(float)((2*i)*(2*i+1));r+=t; }
    return r;
}
/* The bottle's bone point as the story computes it (0x0029E46E..0x0029E500):
 * the model skeleton [anim+4] entry (0x29 - its first word) * 0xC8, the midpoint
 * of its segment [entry+0xAC] (+0..+0xB), through bone matrix [anim+0x10] +
 * int8 [entry+0x49] * 0x40 (sub_00151E80: x*m0 + y*m4 + z*m8 + m12). Read
 * right after the boss's model draw (chained on sub_0008F950, the model draw:
 * sub_000A1B80 posed it there for the view drawn), never in the fight frame,
 * whose matrices were stale or NaN (playtests .62-.75). */
/* A point near the boss's head: finite, within PIT_NEAR across and at least
 * 'up' over its feet (rejects the model-local/camera points of .77). */
static int pit_near_head(uint32_t ent,const float *b,float up)
{
    float dx=b[0]-pit_getf(ent+PIT_ENT_POS),dz=b[2]-pit_getf(ent+PIT_ENT_POS+8u),dy=b[1]-pit_getf(ent+PIT_ENT_POS+4u);
    return pit_finite(b[0]) && pit_finite(b[1]) && pit_finite(b[2]) && dx*dx+dz*dz<=PIT_NEAR*PIT_NEAR && dy>=up && dy<30.0f;
}
static int pit_bone_point(uint32_t ac,float out[3])
{
    uint32_t def,entry,seg,m;
    int32_t first;
    float p[3],q[3];
    unsigned i;
    if(!MAPMAKER_PTR_OK(ac,0x20u) || !MAPMAKER_PTR_OK(def=WORD_AT(ac+4u),4u) || !(m=WORD_AT(ac+0x10u))) return 0;
    first=(int32_t)WORD_AT(def);
    entry=def+(uint32_t)(((int32_t)PIT_BONE-first)*0xC8);
    if(!MAPMAKER_PTR_OK(entry,0xC8u) || !MAPMAKER_PTR_OK(seg=WORD_AT(entry+0xACu),0x18u)) return 0;
    m+=(uint32_t)((int32_t)*(int8_t *)((uintptr_t)(entry+0x49u)+g_xbox_mem_offset)*0x40);
    if(!MAPMAKER_PTR_OK(m,0x40u)) return 0;
    for(i=0;i<3u;i++) p[i]=pit_getf(seg+i*4u)+(pit_getf(seg+0xCu+i*4u)-pit_getf(seg+i*4u))*0.5f;
    for(i=0;i<3u;i++) q[i]=p[0]*pit_getf(m+i*4u)+p[1]*pit_getf(m+0x10u+i*4u)+p[2]*pit_getf(m+0x20u+i*4u)+pit_getf(m+0x30u+i*4u);
    for(i=0;i<3u;i++) if(!pit_finite(q[i])) return 0;
    for(i=0;i<3u;i++) out[i]=q[i];
    return 1;
}
/* WRAP sub_0008F950 (cdecl model instance, pass, layer; the model draw), chained
 * with pc_mapmaker_probe_prop_draw: after the original, for a pit skeleton's
 * instance, cache bone 0x29's point (trusted when PIT_BONE_MIN over its feet). */
void pc_mapmaker_pitboss_draw(void (*original)(void))
{
    uint32_t inst=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u;
    unsigned k;
    float b[3];
    original();
    if(!pit_lv.n || !inst) return;
    for(k=0;k<pit_lv.n;k++) {                          /* the props' draws, for the trace (.77: no bottle seen) */
        uint32_t st=pit_lv.k[k].state;
        if(!st) continue;
        if(WORD_AT(st+PIT_ST_BOTTLE) && WORD_AT(WORD_AT(st+PIT_ST_BOTTLE)+4u)==inst) pit_lv.k[k].drawn_bottle++;
        if(WORD_AT(st+PIT_ST_BANDAGE) && WORD_AT(WORD_AT(st+PIT_ST_BANDAGE)+4u)==inst) pit_lv.k[k].drawn_bandage++;
    }
    for(k=0;k<pit_lv.n && !(pit_lv.k[k].ent && WORD_AT(pit_lv.k[k].ent+PIT_ENT_ANIM)==inst);k++) {}
    if(k==pit_lv.n || WORD_AT(PIT_SCENE)!=100u || !pit_bone_point(inst,b)) return;
    pit_lv.k[k].drawn_boss++;
    if(pit_lv.k[k].raw_logged<4u) {                     /* what the post-draw matrices give (.77: (8.32, 6.44, 3.33), not world) */
        pit_lv.k[k].raw_logged++;
        fprintf(stderr,"[mapmaker] pitboss: pit skeleton %u drawn: bone 0x29 through its post-draw matrix (%.2f, %.2f, %.2f), boss at (%.2f, %.2f, %.2f), view slot %u\n",
                k+1u,(double)b[0],(double)b[1],(double)b[2],(double)pit_getf(pit_lv.k[k].ent+PIT_ENT_POS),
                (double)pit_getf(pit_lv.k[k].ent+PIT_ENT_POS+4u),(double)pit_getf(pit_lv.k[k].ent+PIT_ENT_POS+8u),
                MAPMAKER_PTR_OK(WORD_AT(0x007B0CC0u),0x218u)?(unsigned)WORD_AT(WORD_AT(0x007B0CC0u)+0x214u):0xFFu);
        fflush(stderr);
    }
    if(!pit_near_head(pit_lv.k[k].ent,b,PIT_BONE_MIN)) return;   /* world points only: near the boss, over its feet */
    memcpy(pit_lv.k[k].bone,b,sizeof b);
    pit_lv.k[k].bone_at=pit_lv.k[k].frames+1u;        /* 0: never */
}
static void pit_mouth(PitInst *k,uint32_t ent,uint32_t st)
{
    uint32_t b=WORD_AT(k->state+PIT_ST_BOTTLE),h=WORD_AT(k->state+PIT_ST_COLL);
    float yaw=pit_getf(ent+PIT_ENT_YAW),x,y,z;
    int was_nan;
    if(h && MAPMAKER_PTR_OK(h,0x200u) && WORD_AT(h+0x19Cu)!=PIT_HULL_CB) WORD_AT(h+0x19Cu)=PIT_HULL_CB;
    if(!b || !MAPMAKER_PTR_OK(b,0x200u) || st<1u || st>=PIT_DEATH) return;
    if(!pit_finite(yaw)) yaw=0.0f;
    was_nan=!pit_finite(pit_getf(b+0x64u)) || !pit_finite(pit_getf(b+0x68u)) || !pit_finite(pit_getf(b+0x6Cu));
    {   /* the eye bandage: attached to the head by the engine (sub_000A2C00), so it follows the head's pose */
        uint32_t e=WORD_AT(k->state+PIT_ST_BANDAGE);
        float eye[3];
        if(e && MAPMAKER_PTR_OK(e,0x100u)) {
            eye[0]=pit_getf(e+0x64u);eye[1]=pit_getf(e+0x68u);eye[2]=pit_getf(e+0x6Cu);
            if(pit_near_head(ent,eye,PIT_EYE_MIN)) { k->eye[0]=eye[0];k->eye[1]=eye[1]-PIT_EYE_TO_MOUTH;k->eye[2]=eye[2];k->eye_ok=1; }
        }
    }
    if(k->eye_ok) { x=k->eye[0];y=k->eye[1];z=k->eye[2]; }
    else if(k->bone_at && k->frames+1u-k->bone_at<=PIT_BONE_FRESH) { x=k->bone[0];y=k->bone[1];z=k->bone[2]; }
    else {                                               /* not drawn lately: the estimate */
        x=pit_getf(ent+PIT_ENT_POS)+PIT_MOUTH_AHEAD*pit_sin(yaw);
        y=pit_getf(ent+PIT_ENT_POS+4u)+PIT_MOUTH_UP;
        z=pit_getf(ent+PIT_ENT_POS+8u)+PIT_MOUTH_AHEAD*pit_sin(yaw+1.57079633f);
    }
    if(!pit_finite(x) || !pit_finite(y) || !pit_finite(z)) return;
    {
        unsigned src=k->eye_ok?3u:k->bone_at && k->frames+1u-k->bone_at<=PIT_BONE_FRESH?1u:2u;
        if(src!=k->bone_src && k->hits<1000u) {
            k->bone_src=src;
            fprintf(stderr,"[mapmaker] pitboss: pit skeleton %u: gas bottle at %s (%.2f, %.2f, %.2f), %.2f over its feet\n",
                    (unsigned)(k-pit_lv.k)+1u,src==3u?"the eye bandage, 1.8 lower":src==1u?"bone 0x29 of the last draw":"the head estimate (not drawn lately)",
                    (double)x,(double)y,(double)z,(double)(y-pit_getf(ent+PIT_ENT_POS+4u)));
            fflush(stderr);
        }
    }
    pit_setf(b+0x64u,x);pit_setf(b+0x68u,y);pit_setf(b+0x6Cu,z);
    pit_setf(b+PIT_ENT_YAW,yaw);pit_setf(b+0xC4u,yaw);
    if(!pit_finite(pit_getf(b+0xC0u))) pit_setf(b+0xC0u,0.0f);
    if(!k->mouthed++ || (was_nan && k->mouthed<1000u && (k->mouthed+=1000u)))
        fprintf(stderr,"[mapmaker] pitboss: pit skeleton %u: gas bottle held at the mouth (%.2f, %.2f, %.2f)%s\n",(unsigned)(k-pit_lv.k)+1u,
                (double)x,(double)y,(double)z,was_nan?"; the fight's bone point was NaN":"");
}
/* Every PIT_TRACE_EVERY frames (PIT_TRACES lines a fight): the boss and bottle
 * positions, the bottle's rooms, room and damage callback, its health and each
 * ragbug's flock phase; and each ragbug phase change (slot +0x24: 0 parked,
 * 1 rising, 3 circling, others diving/returning, sub_0028AAA0). */
static void pit_trace(PitInst *k,uint32_t ent,uint32_t st)
{
    uint32_t b=WORD_AT(k->state+PIT_ST_BOTTLE);
    unsigned i,n=(unsigned)(k-pit_lv.k)+1u;
    for(i=0;i<PIT_BUGS;i++) {
        uint32_t sl=k->state+PIT_ST_FLOCK+i*PIT_FLOCK_STRIDE,e=WORD_AT(sl);
        uint8_t ph=(uint8_t)(e?WORD_AT(sl+PIT_FLOCK_ACTIVE):0xFFu);
        if((uint8_t)(ph+1u)==k->bug_phase[i]) continue;   /* stored + 1: 0 = not seen yet */
        k->bug_phase[i]=(uint8_t)(ph+1u);
        if(pit.logged>=40u || !e || !MAPMAKER_PTR_OK(e,0x100u)) continue;
        pit.logged++;
        fprintf(stderr,"[mapmaker] pitboss: pit skeleton %u: ragbug %u phase %u at (%.2f, %.2f, %.2f), boss state %u\n",n,i,(unsigned)ph,
                (double)pit_getf(e+0x64u),(double)pit_getf(e+0x68u),(double)pit_getf(e+0x6Cu),(unsigned)st);
        fflush(stderr);
    }
    if(k->frames++%PIT_TRACE_EVERY || k->traces>=PIT_TRACES) return;
    k->traces++;
    fprintf(stderr,"[mapmaker] pitboss: pit skeleton %u trace: state %u, boss (%.2f, %.2f, %.2f) rooms %d, bottle 0x%X",n,(unsigned)st,
            (double)pit_getf(ent+0x64u),(double)pit_getf(ent+0x68u),(double)pit_getf(ent+0x6Cu),
            (int)*(int8_t *)((uintptr_t)(ent+PIT_ROOMS)+g_xbox_mem_offset),(unsigned)b);
    if(b && MAPMAKER_PTR_OK(b,0x200u))
        fprintf(stderr," at (%.2f, %.2f, %.2f) model 0x%X rooms %d room %u damage cb 0x%X",(double)pit_getf(b+0x64u),(double)pit_getf(b+0x68u),
                (double)pit_getf(b+0x6Cu),(unsigned)PIT_H(b+0x94u),(int)*(int8_t *)((uintptr_t)(b+PIT_ROOMS)+g_xbox_mem_offset),
                (unsigned)WORD_AT(b+0x34u),(unsigned)WORD_AT(b+0x19Cu));
    fprintf(stderr,", health %.2f, draws boss %u bottle %u bandage %u, ragbug phases %d %d %d %d %d %d\n",(double)pit_getf(k->state+PIT_ST_HEALTH),
            k->drawn_boss,k->drawn_bottle,k->drawn_bandage,
            (int)k->bug_phase[0]-1,(int)k->bug_phase[1]-1,(int)k->bug_phase[2]-1,(int)k->bug_phase[3]-1,
            (int)k->bug_phase[4]-1,(int)k->bug_phase[5]-1);
    fflush(stderr);
}

/* Playtest .73 (the .62 release reverted): the ragbugs rose at state 6, before
 * the story would launch them. Stock launches up to 3 parked bugs each time
 * the fight passes state 10 (sub_0028A0A0 at 0x0029F647), after a fire attack
 * and raise; that is the only launch now. The flock is logged once complete. */
static void pit_flock_note(PitInst *k)
{
    unsigned i,waiting=0;
    if(k->released || !k->bugs) return;
    for(i=0;i<pit_lv.nbugs;i++) if(pit_lv.bug[i].owner<0 && pit_lv.bug[i].record) waiting++;
    if(waiting) return;
    k->released=1;
    fprintf(stderr,"[mapmaker] pitboss: pit skeleton %u: flock complete, %u ragbugs parked; they rise at the fight's launches (state 10)\n",
            (unsigned)(k-pit_lv.k)+1u,k->bugs);
    fflush(stderr);
}

/* WRAP sub_00045670 (cdecl sound id -> nonzero when this level loads it; called
 * only by the level's sound load sub_00027700, after the pak mounts), chained
 * with pc_mapmaker_khallos_sound_load. The pit fight's roar and fire attack
 * are level-17 sounds (sounddata flag 4, level byte 0x11), never loaded in
 * scene 100 (playtest .77: "the skeleton had no audio"). Loaded when the
 * map's set holds the pit skeleton. Its death 0x568 and the mansion princess
 * sounds have no level flag (loaded everywhere). */
void pc_mapmaker_pitboss_sound_load(void (*original)(void))
{
    uint32_t id=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0xFFFFFFFFu;
    original();
    if(g_eax || !pit_lv.want || (id!=PIT_SND_FIRE && id!=PIT_SND_ROAR) || WORD_AT(PIT_SCENE)!=100u || !pit_on()) return;
    g_eax=PIT_SND_LOAD;
    pit_log("the pit skeleton's level-17 sound is loaded in this map (sound, load)",id,PIT_SND_LOAD);
}

/* WRAP sub_0028D0A0 (cdecl entity, message, a, b): stock sends every scene
 * but 1 to the pit handler sub_0028C560 (-1 to 1/2: it cannot be hurt; 5
 * deletes its props): for a pit skeleton, with its block (direct call: the
 * callee pops the caller's return address, as the tail jump). */
void pc_mapmaker_pitboss_message(void (*original)(void))
{
    PitInst *k;
    PitSave s;
    if(!pit_lv.n || !MAPMAKER_STACK_OK(20) || !(k=pit_of(WORD_AT(g_esp+4u))) || WORD_AT(PIT_SCENE)!=100u || !pit_on()) { original();return; }
    pit_in(k,&s);
    sub_0028C560();
    pit_out(&s);
}

/* The callbacks the fight installs on its boss and props, and the particle
 * type callbacks: the owner's block in (the current one inside a fight call;
 * else the first instance, never a stale or missing pointer). */
static void pit_run(void (*original)(void),uint32_t obj)
{
    PitInst *k;
    PitSave s;
    if(!pit_lv.n || WORD_AT(PIT_SCENE)!=100u || !pit_on()) { original();return; }
    k=pit_owner(obj);
    if(!k) k=pit_lv.cur?pit_lv.cur:&pit_lv.k[0];
    pit_in(k,&s);
    original();
    pit_out(&s);
}
static uint32_t pit_arg(void) { return MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u; }
void pc_mapmaker_pitboss_boss_slot(void (*original)(void)) { pit_run(original,pit_arg()); }       /* sub_00289720 (entity) */
void pc_mapmaker_pitboss_coll_slot(void (*original)(void)) { pit_run(original,pit_arg()); }       /* sub_00289E20 (object) */
void pc_mapmaker_pitboss_rack_slot(void (*original)(void)) { pit_run(original,pit_arg()); }       /* sub_0028C1C0 (object) */
void pc_mapmaker_pitboss_bottle_slot(void (*original)(void)) { pit_run(original,pit_arg()); }     /* sub_0028C390 (object) */
void pc_mapmaker_pitboss_bandage_slot(void (*original)(void)) { pit_run(original,pit_arg()); }    /* sub_0028C4A0 (object) */
void pc_mapmaker_pitboss_bottle_damage(void (*original)(void))                                    /* sub_0028C2F0 (record, object at +0) */
{
    uint32_t r=pit_arg(),o=r && MAPMAKER_PTR_OK(r,0x2Cu)?WORD_AT(r):0u;
    PitInst *k=pit_lv.n?pit_owner(o):0;
    float before=k?pit_getf(k->state+PIT_ST_HEALTH):0.f;
    pit_run(original,o);
    if(k && k->hits++<16u) {                            /* each hit on the bottle: type, kind, amount, health (8 at the start) */
        fprintf(stderr,"[mapmaker] pitboss: pit skeleton %u: gas bottle hit (type 0x%X, kind 0x%X, amount %.2f): health %.2f -> %.2f\n",
                (unsigned)(k-pit_lv.k)+1u,(unsigned)WORD_AT(r+0x14u),(unsigned)WORD_AT(r+0x24u),(double)pit_getf(r+0x20u),
                (double)before,(double)pit_getf(k->state+PIT_ST_HEALTH));
        fflush(stderr);
    }
}
void pc_mapmaker_pitboss_fire_type(void (*original)(void)) { pit_run(original,0u); }               /* sub_00289080 */
void pc_mapmaker_pitboss_smoke_type(void (*original)(void)) { pit_run(original,0u); }              /* sub_00288E20 */

/* WRAP sub_00234270 (cdecl logic item -> triggered; null-safe): the
 * catacombs 'NMME' item, which a MapMaker map has not. State 0 (0x0029EDA5):
 * 0, so the intro anim princess_cat_cs 0x23B plays; state 1 (0x0029EED1): 1
 * once the boss's animation has finished, and the fight begins. */
void pc_mapmaker_pitboss_logic(void (*original)(void))
{
    uint32_t ret;
    PitInst *k=pit_lv.cur;
    if(!k || !MAPMAKER_STACK_OK(8) || ((ret=WORD_AT(g_esp))!=PIT_RET_NMME0 && ret!=PIT_RET_NMME1) || WORD_AT(PIT_SCENE)!=100u) { original();return; }
    g_eax=ret==PIT_RET_NMME0?0u:(MAPMAKER_PTR_OK(k->ent,0x100u) && guest_call(sub_00059DD0,1,WORD_AT(k->ent+PIT_ENT_ANIM))!=0u);
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_002274C0 (cdecl event, a): the death's level event 2 would fire
 * the map's logic marker 2: dropped. */
void pc_mapmaker_pitboss_event(void (*original)(void))
{
    if(!MAPMAKER_STACK_OK(12) || WORD_AT(g_esp)!=PIT_RET_EVENT || WORD_AT(PIT_SCENE)!=100u || !pit_on()) { original();return; }
    if(pit_lv.events++<4u) pit_log("pit skeleton killed: its level event dropped (MapMaker logic marker untouched) (event, skeleton)",
                                   WORD_AT(g_esp+4u),pit_lv.cur?(uint32_t)(pit_lv.cur-pit_lv.k)+1u:0u);
    MAPMAKER_GUEST_RET();
}
/* WRAP sub_00212160 (cdecl; mission failed "hostage lost"): dropped for the
 * pit fight in scene 100 (the rack timer is held at 0, so it is not reached). */
void pc_mapmaker_pitboss_failure(void (*original)(void))
{
    if(!MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=PIT_RET_FAIL || WORD_AT(PIT_SCENE)!=100u || !pit_on()) { original();return; }
    if(pit_lv.fails++<4u) pit_log("pit skeleton hostage failure dropped",WORD_AT(g_esp+4u),0);
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_0028B610 (cdecl entity; class 0x200000): the ragbug's own handler
 * parks it once at the catacombs rest point; its flight is the boss's flock. */
void pc_mapmaker_pitboss_ragbug(void (*original)(void))
{
    uint32_t ent;
    PitBug *b;
    float saved[PIT_REBASE_N];
    int o;
    if(!MAPMAKER_STACK_OK(8) || !pit_char(ent=WORD_AT(g_esp+4u),PIT_BUG_ID) || WORD_AT(PIT_SCENE)!=100u || !pit_on()) { original();return; }
    if(!pit_lv.n) { pit_remove(ent,"ragbug without a pit skeleton this level (it only flies in its flock): removed");MAPMAKER_GUEST_RET();return; }
    if(!(b=pit_bug_of(ent))) { pit_remove(ent,"ragbug not noted at its AI start (table full): removed");MAPMAKER_GUEST_RET();return; }
    if(b->owner<0) {
        if((o=pit_bug_owner(ent))<0) {
            unsigned k,free_k=0;
            for(k=0;k<pit_lv.n;k++) if(!pit_lv.k[k].placed || pit_lv.k[k].bugs<PIT_BUGS) free_k++;
            if(!free_k) { pit_remove(ent,"ragbug past six for every pit skeleton: removed");MAPMAKER_GUEST_RET();return; }
            if(pit_lv.waits++<2u) pit_log("ragbug waits for its pit skeleton to be placed",ent,0);
            MAPMAKER_GUEST_RET();
            return;
        }
        b->owner=o;b->slot=(int)pit_lv.k[o].bugs++;b->ent=ent;
        fprintf(stderr,"[mapmaker] pitboss: ragbug joins pit skeleton %d's flock (slot %d)\n",o+1,b->slot);
        fflush(stderr);
    }
    pit_move(&pit_lv.k[b->owner],saved);
    original();
    pit_back(saved);
}

/* ---- restart ------------------------------------------------------------ */
static struct { uint16_t id, slot; uint32_t ptr; } pit_anims[PIT_ANIM_N];
static unsigned pit_anim_n;
/* WRAP sub_001CDD60 (level restart, no args): see the head comment. */
void pc_mapmaker_pitboss_restart(void (*original)(void))
{
    uint32_t ptrs,slot,ptr,id,next;
    unsigned k,i,again=0,mapped=0,missed=0;
    static const uint32_t f[4]={PIT_ST_BOTTLE,PIT_ST_COLL,PIT_ST_RACK,PIT_ST_BANDAGE};
    pit_anim_n=0;
    if(pit_play() && pit_lv.n) {
        for(k=0;k<pit_lv.n;k++) for(i=0;pit_lv.k[k].state && i<4u;i++) {
            uint32_t o=WORD_AT(pit_lv.k[k].state+f[i]);
            if(o) { guest_call(sub_000A9070,1,o);WORD_AT(pit_lv.k[k].state+f[i])=0u; }
        }
        if(pit_mounted(PIT_ANIM_PAK) && MAPMAKER_PTR_OK(ptrs=WORD_AT(PIT_ANIM_PTRS),PIT_ANIM_SLOTS*4u))
            for(i=0;i<PIT_ANIM_N;i++) {
                id=PIT_ANIM_FIRST+i;
                if((slot=PIT_H(PIT_ANIM_MAP+id*2u))>=PIT_ANIM_SLOTS || !(ptr=WORD_AT(ptrs+slot*4u)) ||
                   !(WORD_AT(PIT_ANIM_WORDS+slot*8u+4u)&PIT_ANIM_DROPPED)) continue;
                pit_anims[pit_anim_n].id=(uint16_t)id;pit_anims[pit_anim_n].slot=(uint16_t)slot;pit_anims[pit_anim_n].ptr=ptr;
                pit_anim_n++;
            }
    }
    original();
    if(!pit_on() || WORD_AT(PIT_SCENE)!=100u || !pit_lv.n) return;
    pit.logged=0;
    ptrs=WORD_AT(PIT_ANIM_PTRS);next=WORD_AT(PIT_ANIM_NEXT);
    for(i=0;i<pit_anim_n;i++) {
        id=pit_anims[i].id;slot=pit_anims[i].slot;
        if(!MAPMAKER_PTR_OK(ptrs,PIT_ANIM_SLOTS*4u) || PIT_H(PIT_ANIM_MAP+id*2u)!=PIT_ANIM_FREE || WORD_AT(ptrs+slot*4u) || slot>=next) { missed++;continue; }
        PIT_H(PIT_ANIM_MAP+id*2u)=(uint16_t)slot;
        WORD_AT(ptrs+slot*4u)=pit_anims[i].ptr;
        mapped++;
    }
    pit_anim_n=0;
    mapmaker_fx_forget(pit_lv.k,sizeof pit_lv.k);
    pit_lv.cur=0;
    for(k=0;k<pit_lv.n;k++) {
        PitInst *p=&pit_lv.k[k];
        uint32_t rec=p->record,st=p->state,pt=p->parts;
        memset(p,0,sizeof *p);
        p->record=rec;p->state=st;p->parts=pt;
        if(!pit_again(p)) { memset(p,0,sizeof *p);break; }
        again++;
    }
    pit_lv.n=again;
    for(i=0;i<pit_lv.nbugs;i++) { pit_lv.bug[i].owner=-1;pit_lv.bug[i].slot=-1;pit_lv.bug[i].ent=0;pit_lv.bug[i].filled=0; }
    pit_lv.removed=pit_lv.events=pit_lv.fails=pit_lv.waits=0;
    pit_log("level restart: pit skeleton blocks set up again, props deleted, lv17_4 anims mapped again (blocks, anims)",again,mapped);
    if(missed) pit_log("level restart: lv17_4 anims not mapped again (slot taken)",missed,0);
}

/* ---- bar and meter -------------------------------------------------------- */
/* WRAP sub_0014CC80 (story boss bar; chain INSIDE pc_mapmaker_bosses_hud):
 * never the stock bar or the hostage meter (entity +0x1A0 == 1) on a pit
 * skeleton: the focus is cleared for the original. */
void pc_mapmaker_pitboss_hud(void (*original)(void))
{
    uint32_t player,focus;
    if(!pit_lv.n) { original();return; }
    player=WORD_AT(PIT_PLAYER);
    if(!MAPMAKER_PTR_OK(player,PIT_FOCUS+4u) || !(focus=WORD_AT(player+PIT_FOCUS)) || !pit_of(focus) || !pit_play()) { original();return; }
    WORD_AT(player+PIT_FOCUS)=0;
    original();
    WORD_AT(player+PIT_FOCUS)=focus;
}
/* WRAP sub_0013A400 (cdecl actor, 0, 0x7F, 0x6E; overhead meter): none on a
 * pit skeleton unless its Story AI has the Sub-boss bar. */
void pc_mapmaker_pitboss_meter(void (*original)(void))
{
    uint32_t a;
    PitInst *k;
    if(!pit_lv.n || !MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=PIT_RET_METER) { original();return; }
    a=WORD_AT(g_esp+4u);
    if(!MAPMAKER_PTR_OK(a,PIT_ACTOR) || !(k=pit_of(WORD_AT(a))) || k->boss==PIT_BOSS_SUB || !pit_play()) { original();return; }
    if(!pit_lv.meters++) pit_log("overhead health meter hidden on a pit skeleton",a,0);
    MAPMAKER_GUEST_RET();
}
