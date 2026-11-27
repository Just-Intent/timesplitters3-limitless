/* US 45410066 MapMaker real story boss: the mansion worm of story mission 5
 * (scene 1, "data/mansion2.raw") as a MapMaker Story AI. Area `worm`; every
 * hook is inert unless TSFP_MAPMAKER_LIMITS=1 (TSFP_MAPMAKER_REAL_BOSS=0
 * keeps the bot AI). Docs: docs/mapmaker/mapmaker-worm.md (CONFIRMED/INFERRED
 * labels, the wrapper list for the integrator). Tests:
 * tests/mapmaker/test_mapmaker_worm.py.
 *
 * The worm (CONFIRMED, US XBE data and decode): character id 0x2C (character
 * table: voice set 0x005397C8, model 0x5EA CHR_MANSION_MANSIONWORM
 * chrs/mansionworm in pak/chr.pak, no LOD; no Arcade row), skeleton group
 * 0x47. Its eight anims wrm* 0x194..0x19B are named only by
 * pak/stream/lv1/anim/lv1_2.pak (string 0x0049D7BC, registered by
 * sub_001CA2F0 at 0x001CAD57, table 0x0048BEC8, 30 names).
 *
 * The worm is not an AI class. The mansion level runs it:
 *  - Blocks: [0x0075B4B4] = 5 worms of 0x450 bytes (sub_001D1E80, from
 *    sub_0036C740 in scene 1 only), set up by sub_001D2350 (from the level
 *    init sub_001DDB50 of the scene descriptor at 0x004FDF80): fixed starts
 *    in the mansion garden (y -2.4), worm 2 in its own state 8, a dirt
 *    emitter each (+0x43C), the enable flag [0x0075B4BC] = 0 (the level
 *    script's sub_001D5E10 sets it), and two mansion-only steps: the six
 *    route pad ids at 0x004FDC24 (0x4FF, 0x514, 0x515, 0x51D, 0x516, 0x500)
 *    converted to pad indices once ([0x004FDC58]), and level object 0x18
 *    ([[0x007B9784]+0x60], no null check) given flag 0x10 and a hit callback.
 *  - Update: the descriptor's level update sub_001DDC90 calls sub_001DAAB0
 *    (17 states at worm +0x24) every frame. A worm runs only once its head is
 *    bound: sub_001D9AF0 looks up the AI whose spawn record is tagged
 *    'WRM1'+i (sub_0015BA10, the AI pool), clears its slots 3 (the character
 *    update sub_000D8E30) and 4, sets its hit callback sub_001D1F30, lowers
 *    it by 5 and gives it 12.1 health (+0x1CC/+0x1D8).
 *  - Mansion coordinates read from image constants: the burrowing height
 *    -2.399, the target box (x -77.2..-55.7, z -6.0..39.5; -6.0 is the shared
 *    constant 0x0047D69C, whose other readers are not reachable from the
 *    update), the wake region (sub_001D5D70: a player in x -79..-55.7, z
 *    -7.9..40.3) and worm 2's point (-62.31, 16.9).
 *  - Route: the worm patrols the six pads (next = (i + 1) mod 6) and walks
 *    the pad graph (sub_001596F0); a missing pad (-1) reads position 0.
 *  - Draw: the frame sub_001CEBC0 calls sub_001DA990 only in scene 1
 *    (0x001CEE2C). It draws the bodies and positions the surfaced heads
 *    (sub_001D9B80), but also runs sub_001D9370 (mansion props, tag
 *    lookups) and sub_001D7200, and clips the bodies at a mansion wall
 *    (camera x > -53.9, 0x0049DE08).
 *  - Hit: sub_001D1F30 needs the head's +0x60 == 'WRM1'+i, counts kills in
 *    the mansion script block [[0x0075B0C4]+0x3C] (no null check) and at 5
 *    kills fires level marker 1 and event 4; its effect uses the mansion's
 *    effect type [0x0078F590] (sub_00378400(sub_001D6290, 5)).
 * This area runs the stock code with one worm in scene 100:
 *  - level: allocates the worm block, a scratch block (stand-ins for the
 *    script block and object 0x18) and the effect type, runs the stock init
 *    with a fake object table and the pad conversion off, parks worms 1-4
 *    (state 4: no head, never updated or drawn, emitters released).
 *  - tick: on the head's character update, the first time: worm 0 at the
 *    spawn point, the mansion box centred on it (dx, dz), the burrowing
 *    height at the spawn floor (dy), six pads around the spawn, enable. Every
 *    tick: the constants shifted and the pad table swapped in for the stock
 *    update only, slot 3 kept, no character think (as stock).
 *  - tag: 'WRM1' finds this map's head, 'WRM2'..'WRM5' nothing.
 *  - hit: the stand-ins swapped in for the stock callback; the kill counter
 *    reset, so the 5-kill marker never fires.
 *  - fx: the stock draw in scene 100, with its two mansion callees skipped
 *    and the wall clip off.
 *  - room: a bound head's room (stock 0x18, the garden) is its spawn room, or
 *    surfaced, a near player's: the damage rays start there (.28).
 *  - gone: the head's slot 12 (sub_001D5E20) sets its own worm gone (.28).
 *  - script: the update's trigger of the mansion script object 'SFCC'
 *    (sub_00234CD0 at 0x001DC45A, which reaches level markers and tag
 *    lookups through sub_0025F0C0) dropped. The only other way from the
 *    update to a marker is the generic explosion sub_000BB6C0 (68 callers),
 *    which only reads one.
 *  - restart (sub_001CDD60, "START restart": the teardown sub_001CDAF0 and
 *    this, never the level setup sub_001CDB80 with the level hook): before
 *    the original, each worm keeps its block (level memory, which a restart
 *    does not free), scratch, effect type and spawn record, and forgets the
 *    run before (bound head, route, ready/failed, live globals); its next AI
 *    start runs the stock block init again on the same block (zeroed, no new
 *    allocation), so the restarted worm is set up as on the level's start.
 *    Without it the restarted head matched its old worm by record and ran on
 *    the last fight's block (a killed worm stayed gone).
 *
 * Several worms: up to WRM_MAX map worms each get their own worm block (its
 * worm 0 theirs, 1-4 parked) and their own copy of the 9 globals the block's
 * code keeps (wrm_win), swapped in around every entry into their code (tick,
 * hit callback, draw) and put back after; the worm whose copy is live is
 * wrm_lv. Docs: "Several mansion worms".
 *
 * Requires the includer's WORD_AT, guest_call, g_esp/g_eax, g_xbox_mem_offset,
 * pc_mapmaker_area_enabled and pc_mapmaker_limits.inc. Hot hooks (tick: every
 * character every frame; fx: every frame) test a fixed value first. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_mapmaker_animslots.inc"   /* the anim slot budget run by pc_mapmaker_worm_register */

#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif

#define WRM_SCENE 0x0079094Cu
#define WRM_MODE 0x007DE455u            /* byte: 10 = story */
#define WRM_ID 0x2Cu                    /* character id */
#define WRM_MODEL 0x5EAu                /* CHR_MANSION_MANSIONWORM */
#define WRM_PAK 0x0049D7BCu             /* "pak/stream/lv1/anim/lv1_2.pak" */
#define WRM_PAK_TROPHY 0x0049D77Cu      /* "pak/stream/lv1/anim/lv1_3.pak": anim ids overlap lv1_2's */
#define WRM_PAK_SKELETON 0x0049D6FCu    /* "pak/stream/lv1/anim/lv1_6.pak": anim ids overlap lv1_2's */
#define WRM_MOUNTS 0x006D7880u          /* mounted paks */
#define WRM_MOUNT_MAX 64u               /* paktable.json: 64 slots at 0x00907000 */
#define WRM_MOUNT_KEEP 8u
#define WRM_STATE 0x0075B4B4u           /* 5 worms, sub_001D1E80 */
#define WRM_WORM 0x450u
#define WRM_WORMS 5u
#define WRM_STATE_SIZE 0x1590u
#define WRM_ENABLE 0x0075B4BCu          /* sub_001D5E10 (the level script) */
#define WRM_HEADS 0x0078FAE0u           /* bound head entity per worm */
#define WRM_SCRIPT 0x0075B0C4u          /* mansion script block, 0x50 (sub_001D7830) */
#define WRM_SCRIPT_SIZE 0x50u
#define WRM_KILLS 0x3Cu                 /* script block: worms killed */
#define WRM_OBJECTS 0x007B9784u         /* level object table (sub_000877B0) */
#define WRM_OBJECT 0x18u                /* the mansion object the init touches */
#define WRM_EFFECT 0x0078F590u          /* the mansion effect type (sub_001D6420) */
#define WRM_EFFECT_FN 0x001D6290u       /* its behaviour; 5 variants */
#define WRM_ROUTE 0x004FDC24u           /* six route pads (ids, indices after the init) */
#define WRM_ROUTE_N 6u
#define WRM_ROUTE_DONE 0x004FDC58u      /* 1: ids not converted yet */
#define WRM_PADS 0x00715F2Cu            /* regular pads, 0x20 each, position +0xC */
#define WRM_PAD_COUNT 0x007B6664u
#define WRM_PAD 0x20u
#define WRM_PAD_SCAN 0x2000u
#define WRM_TAG 0x57524D31u             /* 'WRM1' + worm */
#define WRM_CHAR_UPDATE 0x000D8E30u     /* slot 3 of every character (sub_0009E2D0(8, 3, ..) at 0x000D9210) */
#define WRM_SLOT3 0x160u                /* entity +0x154 + 3*4 */
#define WRM_ENT_TAG 0x60u               /* head: 'WRM1' + worm for the hit callback */
#define WRM_W_STATE 0x24u               /* worm +0x24 state, 4 = gone */
#define WRM_W_TRAIL 0x3Cu               /* 64 trail positions of 12 bytes */
#define WRM_W_TRAILS 64u
#define WRM_W_EMITTER 0x43Cu
#define WRM_W_PAD 0x440u                /* current pad, +0x444 previous */
#define WRM_GONE 4u
#define WRM_SURFACED 8u                 /* states >= 8: the head is out (placed by the draw) */
#define WRM_W_ROUTE_FROM 0x44Cu         /* the state the burst came from (2: the fixed burst point) */
#define WRM_GROUND (-2.4f)              /* mission 5: the worms' start height */
#define WRM_BOX_X (-66.45f)             /* centre of the target box (x -77.2..-55.7, z -6..39.5) */
#define WRM_BOX_Z 16.75f
#define WRM_BOX_HX 10.75f
#define WRM_BOX_HZ 22.75f
#define WRM_PAD_DY 4.0f                 /* a route pad is on the spawn's floor */
#define WRM_PAD_GAP 3.0f
#define WRM_FLT_MAX 0x7F7FFFFFu
#define WRM_WALL 0x0049DE08u            /* -53.9: the mansion wall clip (camera x) */
#define WRM_WALL_BITS 0xC257999Au
#define WRM_CONTAINER 0x00784014u
#define WRM_MAGIC 0x00C77667u
#define WRM_TAG_INFO 0x00C77668u
#define WRM_TAG_END 0x00C7766Au
#define WRM_TAG_SETS 0x00C776F5u
#define WRM_SCAN 0x00080000u
#define WRM_SET_MAP 0x11u
#define WRM_INFO_SLOTS 10u
#define WRM_ACTOR 0xEA0u
#define WRM_ACTOR_CHAR 0x48u
#define WRM_ACTOR_RECORD 0xBF0u
#define WRM_ACTOR_PAD 0x678u            /* the AI's current pad */
#define WRM_ENT_AI 0x7Cu
#define WRM_ENT_MODEL 0x94u
#define WRM_ENT_X 0x64u                 /* +0x64 x, +0x68 y (up), +0x6C z */
#define WRM_RET_TAG 0x001D9AFDu         /* sub_001D9AF0: call sub_0015BA10 at 0x001D9AF8 */
#define WRM_RET_PROPS 0x001DA998u       /* sub_001DA990: call sub_001D9370 at 0x001DA993 */
#define WRM_RET_EXTRA 0x001DA9A9u       /* sub_001DA990: call sub_001D7200 at 0x001DA9A4 */
#define WRM_RET_FX 0x001CEE8Cu          /* sub_001CEBC0: call sub_00126560 at 0x001CEE87 */
#define WRM_RET_SCRIPT 0x001DC45Fu      /* sub_001DAAB0: push 'SFCC'; call sub_00234CD0 at 0x001DC45A */
#define WRM_SCRATCH 0x300u              /* +0 script block, +0x50 object table, +0x100 object */
#define WRM_FAKE_TABLE 0x50u
#define WRM_FAKE_OBJECT 0x100u
#define WRM_ENT_ROOM 0x34u            /* +0x34 room (level object index, 1-based), +0x38 */
#define WRM_STOCK_ROOM 0x18u          /* the binding's room (0x001D9B48): the mansion garden */
#define WRM_ROOM_COUNT 0x007B975Cu    /* rooms (sub_000C3B10 at 0x000C3B46) */
#define WRM_PLAYERS 0x007B0C48u       /* player structs, 0x1584 each: +4 < 0 empty, +0x14 entity, +0xA8 position */
#define WRM_PLAYER 0x1584u
#define WRM_PLAYER_N 0x00790950u
#define WRM_PLAYER_MAX 4u
#define WRM_WAKE_X0 (-79.0f)          /* the stock wake region (sub_001D5D70), before the shift */
#define WRM_WAKE_X1 (-55.7f)
#define WRM_WAKE_Z0 (-7.9f)
#define WRM_WAKE_Z1 40.3f
#define WRM_REACH 12.0f               /* a player this near the surfaced worm lends it his room */
#define WRM_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define WRM_H(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

/* Image constants shifted during the stock update: address, stock bits, axis
 * (0 x, 1 y, 2 z). */
static const struct { uint32_t addr, bits; int axis; } wrm_consts[]={
    {0x0049DF08u,0xC0198937u,1},   /* -2.399: the burrowing height */
    {0x0049DF10u,0xC29A6666u,0},   /* -77.2: target box x */
    {0x0049DE40u,0xC25ECCCDu,0},   /* -55.7: target box and wake region x */
    {0x0049DF0Cu,0x421E0000u,2},   /* 39.5: target box z */
    {0x0047D69Cu,0xC0C00000u,2},   /* -6.0: target box z (shared; no other reader in the update) */
    {0x0049DE3Cu,0xC29E0000u,0},   /* -79.0: wake region x */
    {0x0049DE34u,0xC0FCCCCDu,2},   /* -7.9: wake region z */
    {0x0049DE38u,0x42213333u,2},   /* 40.3: wake region z */
    {0x0047D78Cu,0x41D80000u,2},   /* 27.0: a state-2 worm south of it bursts out (shared; no other reader in the update) */
    {0x0049DDD8u,0xC2794189u,0},   /* -62.31: worm 2's point x */
    {0x0049DDD4u,0x41873333u,2},   /* 16.9: worm 2's point z */
};
#define WRM_NCONST (sizeof wrm_consts/sizeof wrm_consts[0])
#define WRM_NDRAW 2u                    /* the last two are also read by the draw */

extern void sub_0005B980(void),sub_0005A130(void),sub_00062260(void),sub_001D1E80(void),sub_001D2350(void),sub_001DAAB0(void);
extern void sub_001DA990(void),sub_00378400(void),sub_0010E2A0(void),sub_0003DFA0(void),sub_0009A420(void);
int pc_mapmaker_area_enabled(const char *area,const char *hook);

static struct {
    int read, optout;
    unsigned logged, extras, ticks, hits, draws, scripts, surfacings, levels, gones;
    unsigned serial;                    /* worm levels this session: tells two levels' lines apart */
} wrm;
/* The globals one worm block's code keeps between calls (CONFIRMED, every
 * written [abs] operand of the code reachable from its entry points, test):
 * the block, its init flag, the enable flag, the "worms active" flag and the
 * five bound heads. The rest it writes is the body-geometry tables of
 * sub_001D1BD0 (the same values every init) and the draw's scratch. Each map
 * worm has its own block and copy, swapped in around every entry into its
 * code (its tick's update, its hit callback, its draw). */
static const uint32_t wrm_win[]={
    WRM_STATE,0x0075B4B8u,WRM_ENABLE,0x004FDC20u,
    0x0078FAE0u,0x0078FAE4u,0x0078FAE8u,0x0078FAECu,0x0078FAF0u,
};
#define WRM_NWIN (sizeof wrm_win/sizeof wrm_win[0])
#define WRM_MAX 4u                      /* mansion worms with the story behaviour per map */
/* One map worm: its block, spawn record, pool entry, placement. */
struct wrm_worm_s {
    uint32_t state, scratch, effect, record, actor;
    int ready, failed, shifted, drawing, swapped, surfaced;
    int again;                        /* restarted: its block is set up again at its AI start */
    uint32_t head, room, last_state;  /* the bound head, its spawn room (0: none), the state last logged */
    unsigned states;                  /* state changes logged */
    float dx, dy, dz;
    uint32_t pad[WRM_ROUTE_N], saved[WRM_NCONST], route[WRM_ROUTE_N], wall;
    uint32_t win[WRM_NWIN];             /* its globals while another's (or nobody's) are live */
};
static struct wrm_worm_s wrm_inst[WRM_MAX], wrm_none;
static unsigned wrm_n;                  /* worms with their own blocks this level */
static int wrm_cur=-1;                  /* whose globals are live; -1: the outside values */
static uint32_t wrm_out[WRM_NWIN];
static uint32_t wrm_scratch, wrm_effect; /* this level's stand-ins, shared by every worm */
static struct wrm_worm_s *wrm_i=&wrm_none;
#define wrm_lv (*wrm_i)                 /* the worm whose code runs now (wrm_none outside) */

static void wrm_store(int k)
{
    uint32_t *to=k<0?wrm_out:wrm_inst[k].win;
    unsigned j;
    for(j=0;j<WRM_NWIN;j++) to[j]=WORD_AT(wrm_win[j]);
}
static void wrm_load(int k)
{
    const uint32_t *from=k<0?wrm_out:wrm_inst[k].win;
    unsigned j;
    for(j=0;j<WRM_NWIN;j++) WORD_AT(wrm_win[j])=from[j];
    wrm_i=k<0?&wrm_none:&wrm_inst[k];
}
/* Make worm k's globals live; returns whose were live, for wrm_leave. */
static int wrm_enter(int k)
{
    int prev=wrm_cur;
    if(prev==k) return prev;
    wrm_store(prev);
    wrm_load(k);
    wrm_cur=k;
    return prev;
}
static void wrm_leave(int prev)
{
    if(wrm_cur==prev) return;
    wrm_store(wrm_cur);
    wrm_load(prev);
    wrm_cur=prev;
}

static void wrm_log(const char *what,uint32_t a,uint32_t b)
{
    if(wrm.logged>=24u || wrm.levels>=160u) return;
    wrm.levels++;
    wrm.logged++;
    fprintf(stderr,"[mapmaker] worm: %s (0x%X, 0x%X)\n",what,(unsigned)a,(unsigned)b);
    fflush(stderr);
}
static int wrm_on(void)
{
    mapmaker_limits_read();
    if(!wrm.read) {
        const char *v=getenv("TSFP_MAPMAKER_REAL_BOSS");
        wrm.read=1;
        wrm.optout=v && v[0]=='0' && !v[1];
    }
    return mapmaker_limits.limits;
}
static int wrm_play(void) { return wrm_on() && WORD_AT(WRM_SCENE)==100u && WRM_B(WRM_MODE)==10u; }
static float wrm_getf(uint32_t a) { float f;memcpy(&f,(const void *)((uintptr_t)a+g_xbox_mem_offset),4);return f; }
static void wrm_setf(uint32_t a,float f) { memcpy((void *)((uintptr_t)a+g_xbox_mem_offset),&f,4); }
static int wrm_sane(float f) { return f==f && f>-100000.0f && f<100000.0f; }
static void wrm_zero(uint32_t a,uint32_t n) { memset((void *)((uintptr_t)a+g_xbox_mem_offset),0,n); }

/* The map's custom set (index 0x11; slots 1-10 in the map-info chunk, 11-20
 * in the sets area's chunk), as pc_mapmaker_realbosses.inc reads it.
 * 1: holds the worm, 0: does not (or not the map's set), -1: unreadable. */
static int wrm_set_has(void)
{
    uint32_t container=WORD_AT(WRM_CONTAINER),c,end,steps,info=0,isz=0,extra=0,n,k;
    if(!container || !MAPMAKER_PTR_OK(container,WRM_SCAN) || WORD_AT(container)!=WRM_MAGIC) return -1;
    c=container+8u;end=container+WRM_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u),d=c+8u;
        if(tag==WRM_TAG_END || sz>end-c-8u) break;
        if(tag==WRM_TAG_INFO && sz>=0x18u && !info) { info=d;isz=sz; }
        else if(tag==WRM_TAG_SETS && sz>=0x30u && !extra && WRM_H(d)==1u && WRM_B(d+2u)==WRM_INFO_SLOTS &&
                WRM_B(d+3u)>=1u && WRM_B(d+3u)<=WRM_INFO_SLOTS) extra=d;
        c+=8u+sz;
    }
    if(!info) return -1;
    if(WRM_H(info+0x14u)!=WRM_SET_MAP) return 0;
    n=WRM_B(info+0x16u);
    if(n>WRM_INFO_SLOTS) n=WRM_INFO_SLOTS;
    if(0x18u+n*4u>isz) n=(isz-0x18u)/4u;
    for(k=0;k<n;k++) if(WRM_H(info+0x18u+k*4u)==WRM_ID) return 1;
    for(k=0;extra && k<WRM_B(extra+3u);k++) if(WRM_H(extra+4u+k*4u)==WRM_ID) return 1;
    return 0;
}
static int wrm_pak_mounted(void) { return (int32_t)guest_call(sub_00062260,1,WRM_PAK)!=-1; }
static int wrm_state_ok(void) { return wrm_lv.state && WORD_AT(WRM_STATE)==wrm_lv.state; }

/* WRAP sub_001C8540 (the level's anim paks; sub_001CA2F0 registers names next). */
void pc_mapmaker_worm_anims(void (*original)(void))
{
    int has;
    uint32_t mounted;
    original();
    if(!wrm_play() || wrm.optout || (has=wrm_set_has())==0 || wrm_pak_mounted()) return;
    /* Without register (the anim slot budget) lv1_2's whole 30-name table
     * registers. With the trophy monster's or skeleton's pak too the 940 anim
     * slots ran out and the level load crashed (v0.4.0-mapmaker.24; .34 with
     * lv11.pak: docs/mapmaker/mapmaker-animslots.md), so the worm stays a bot
     * next to those bosses. */
    if(!pc_mapmaker_area_enabled("worm","pc_mapmaker_worm_register") &&
       ((int32_t)guest_call(sub_00062260,1,WRM_PAK_TROPHY)!=-1 || (int32_t)guest_call(sub_00062260,1,WRM_PAK_SKELETON)!=-1)) {
        wrm_log("lv1_2.pak shares anim ids with the trophy monster's or skeleton's pak: the mansion worm keeps the bot behaviour",
                WORD_AT(WRM_MOUNTS),0);
        return;
    }
    if((mounted=WORD_AT(WRM_MOUNTS))+WRM_MOUNT_KEEP>WRM_MOUNT_MAX)
        wrm_log("pak mount table full; the mansion worm keeps the bot behaviour",mounted,WRM_MOUNT_MAX);
    else if(!guest_call(sub_0005A130,1,WRM_PAK)) wrm_log("lv1_2.pak did not mount; bot behaviour",mounted,(uint32_t)has);
    else wrm_log(has<0?"mounted lv1_2.pak (map container unreadable)":"mounted lv1_2.pak for the mansion worm",mounted+1u,0);
}

/* WRAP sub_0005B980 (cdecl prefix, name table, count: registers a mounted
 * anim pak's names, from sub_001CA2F0 and sub_001C95F0): the anim slot
 * budget of pc_mapmaker_animslots.inc (scene 100 only). lv1_2's table gives
 * only the worm's names, barney_worm and wrmburst..wrmdeath; every boss pak
 * likewise only its boss's; an id already registered is skipped; nothing
 * past the 940 slots. Its nested per-entry calls come back here and run the
 * original. */
void pc_mapmaker_worm_register(void (*original)(void))
{
    if(ans_register()) return;
    original();
}

/* WRAP sub_0025F720 (story level init): after the original, the worm block
 * and the stock worm init with this area's stand-ins; worms 1-4 parked. */
/* Worm k's block: the stock allocation and init with this area's stand-ins
 * (the fake object table, no pad conversion), its worms 1-4 parked (state 4:
 * no head, never updated or drawn, emitters released), captured into its copy;
 * the globals as they were are put back. 1: allocated. */
static int wrm_alloc(unsigned k)
{
    uint32_t live[WRM_NWIN],st,objects,route_done,i,w;
    uint32_t reuse=k<wrm_n && wrm_inst[k].again?wrm_inst[k].state:0u;   /* restarted: its own block again */
    unsigned j;
    for(j=0;j<WRM_NWIN;j++) live[j]=WORD_AT(wrm_win[j]);
    WORD_AT(WRM_STATE)=0;
    if(reuse && MAPMAKER_PTR_OK(reuse,WRM_STATE_SIZE)) { wrm_zero(reuse,WRM_STATE_SIZE);WORD_AT(WRM_STATE)=reuse; }
    else guest_call(sub_001D1E80,0);
    st=WORD_AT(WRM_STATE);
    if(!st || !MAPMAKER_PTR_OK(st,WRM_STATE_SIZE)) {
        wrm_log("no level memory for a mansion worm; bot behaviour",st,k);
        for(j=0;j<WRM_NWIN;j++) WORD_AT(wrm_win[j])=live[j];
        return 0;
    }
    objects=WORD_AT(WRM_OBJECTS);route_done=WORD_AT(WRM_ROUTE_DONE);
    WORD_AT(WRM_OBJECTS)=wrm_scratch+WRM_FAKE_TABLE;  /* object 0x18: a zeroed stand-in */
    WORD_AT(WRM_ROUTE_DONE)=0;                         /* the mansion pad ids: not converted */
    guest_call(sub_001D2350,0);
    WORD_AT(WRM_OBJECTS)=objects;WORD_AT(WRM_ROUTE_DONE)=route_done;
    for(i=1;i<WRM_WORMS;i++) {                         /* one map worm per block: the others gone */
        w=st+i*WRM_WORM;
        WORD_AT(w+WRM_W_STATE)=WRM_GONE;
        if(WORD_AT(w+WRM_W_EMITTER)) guest_call(sub_0010E2A0,1,WORD_AT(w+WRM_W_EMITTER));
    }
    WORD_AT(WRM_ENABLE)=0;                             /* until the head's first tick */
    memset(&wrm_inst[k],0,sizeof wrm_inst[k]);
    for(j=0;j<WRM_NWIN;j++) wrm_inst[k].win[j]=WORD_AT(wrm_win[j]);
    for(j=0;j<WRM_NWIN;j++) WORD_AT(wrm_win[j])=live[j];
    wrm_inst[k].state=st;wrm_inst[k].scratch=wrm_scratch;wrm_inst[k].effect=wrm_effect;
    wrm_log(reuse?"mansion worm state set up again after the restart (worm, block)":"mansion worm state allocated (worm, block)",k,st);
    return 1;
}
/* The map worm whose spawn record ent's AI has, or -1. */
static int wrm_find(uint32_t ent)
{
    uint32_t a,rec;
    unsigned k;
    if(!wrm_n || !MAPMAKER_PTR_OK(ent,0x200u) || !MAPMAKER_PTR_OK(a=WORD_AT(ent+WRM_ENT_AI),WRM_ACTOR)) return -1;
    rec=WORD_AT(a+WRM_ACTOR_RECORD);
    for(k=0;k<wrm_n;k++)                               /* not a restarted one before its AI start */
        if(wrm_inst[k].record && wrm_inst[k].record==rec && wrm_inst[k].state && !wrm_inst[k].again) return (int)k;
    return -1;
}

void pc_mapmaker_worm_level(void (*original)(void))
{
    int has;
    uint32_t scratch;
    original();
    if(wrm_cur>=0) wrm_leave(-1);
    memset(wrm_inst,0,sizeof wrm_inst);               /* a new level: nothing of the last one */
    memset(&wrm_none,0,sizeof wrm_none);
    wrm_n=0;wrm_cur=-1;wrm_i=&wrm_none;wrm_scratch=0;wrm_effect=0;
    /* a new level logs its own first lines again (the .27 second worm level
     * logged neither its update nor its draw) */
    wrm.logged=wrm.extras=wrm.ticks=wrm.hits=wrm.draws=wrm.scripts=wrm.surfacings=wrm.gones=0;
    if(!wrm_play() || wrm.optout || (has=wrm_set_has())==0) return;
    WORD_AT(WRM_STATE)=0;                              /* a stale pointer of an earlier level */
    wrm.serial++;
    scratch=guest_call(sub_0003DFA0,2,WRM_SCRATCH,1u);
    if(!scratch || !MAPMAKER_PTR_OK(scratch,WRM_SCRATCH)) {
        wrm_log("no level memory for the mansion worm; bot behaviour",0,scratch);
        return;
    }
    wrm_zero(scratch,WRM_SCRATCH);
    WORD_AT(scratch+WRM_FAKE_TABLE+WRM_OBJECT*4u)=scratch+WRM_FAKE_OBJECT;
    wrm_scratch=scratch;
    wrm_effect=guest_call(sub_00378400,2,WRM_EFFECT_FN,5u);
    /* No block yet: the stock block init (sub_001D2350) takes particle
     * emitters, and this hook runs in sub_001CDB80, before sub_001CDE30 builds
     * this level's emitter pool (sub_000FF1B0 -> sub_001121B0). The free list
     * then still points into the last level's freed memory, which lv1_2's
     * data overwrote: v0.4.0-mapmaker.26 crashed in sub_0010DF90 from
     * 0x001D2604 on every level whose set holds the worm. Each worm's block is
     * made at its AI start, in play. */
    if(has<0) wrm_log("mansion worm level set up with the map container unreadable",0,0);
}

/* WRAP sub_001CDD60 (the level restart, no args; only at 0x0003D4E1, after
 * the teardown, never the level hook above): before the original, the
 * outside globals come back and each worm keeps only its block, scratch,
 * effect type and spawn record (the level's memory and effect list survive a
 * restart: sub_001CDD60 allocates nothing, sub_003796A0 keeps the list); its
 * AI start sets the block up again. The log budgets start afresh. */
static void wrm_restart_reset(void)
{
    unsigned k,again=0;
    if(wrm_cur>=0) wrm_leave(-1);
    for(k=0;k<wrm_n && k<WRM_MAX;k++) {
        uint32_t st=wrm_inst[k].state,scratch=wrm_inst[k].scratch,effect=wrm_inst[k].effect,rec=wrm_inst[k].record;
        memset(&wrm_inst[k],0,sizeof wrm_inst[k]);        /* the run before: head, route, ready, live globals */
        wrm_inst[k].state=st;wrm_inst[k].scratch=scratch;wrm_inst[k].effect=effect;wrm_inst[k].record=rec;
        wrm_inst[k].again=1;
        again++;
    }
    memset(&wrm_none,0,sizeof wrm_none);
    wrm_cur=-1;wrm_i=&wrm_none;
    wrm.logged=wrm.extras=wrm.ticks=wrm.hits=wrm.draws=wrm.scripts=wrm.surfacings=wrm.gones=0;
    if(again) wrm_log("level restart: each mansion worm is set up again at its AI start (worms, scratch)",again,wrm_scratch);
}
void pc_mapmaker_worm_restart(void (*original)(void))
{
    if(wrm_on()) wrm_restart_reset();
    original();
}

/* WRAP sub_0015B830 (cdecl AI pool entry): one mansion worm per map is noted;
 * its spawn record keeps the MapMaker class (the head's think stops when the
 * worm binds it, as in stock). */
void pc_mapmaker_worm_ai_start(void (*original)(void))
{
    uint32_t a,ent,rec;
    unsigned k;
    if(!MAPMAKER_STACK_OK(8) || !wrm_play()) { original();return; }
    a=WORD_AT(g_esp+4u);
    if(!MAPMAKER_PTR_OK(a,WRM_ACTOR) || !MAPMAKER_PTR_OK(rec=WORD_AT(a+WRM_ACTOR_RECORD),12u)) { original();return; }
    ent=WORD_AT(a);
    if(WRM_H(a+WRM_ACTOR_CHAR)!=WRM_ID && !(MAPMAKER_PTR_OK(ent,0x100u) && WRM_H(ent+WRM_ENT_MODEL)==WRM_MODEL)) { original();return; }
    for(k=0;k<wrm_n && wrm_inst[k].record!=rec;k++) {}  /* the same record again */
    if(wrm.optout) wrm_log("TSFP_MAPMAKER_REAL_BOSS=0: mansion worm with bot behaviour",a,0);
    else if(!wrm_pak_mounted()) wrm_log("lv1_2.pak not mounted: mansion worm with bot behaviour",a,0);
    else if(!wrm_scratch) wrm_log("no mansion worm level set-up: bot behaviour",a,WORD_AT(WRM_STATE));
    else if(k==wrm_n && wrm_n>=WRM_MAX) {
        if(wrm.extras++<4u) wrm_log("more than 4 mansion worms in the map: this one keeps the bot behaviour",a,WORD_AT(rec+8u));
    } else if(k==wrm_n && !wrm_alloc(k)) {
        if(wrm.extras++<4u) wrm_log("no memory for another mansion worm: this one keeps the bot behaviour",a,WORD_AT(rec+8u));
    } else if(k<wrm_n && wrm_inst[k].again && !wrm_alloc(k)) {
        wrm_inst[k].state=0;                           /* never runs on the last fight's block */
        if(wrm.extras++<4u) wrm_log("restarted mansion worm's block not set up again: bot behaviour",a,WORD_AT(rec+8u));
    } else {
        if(k==wrm_n) wrm_n++;
        wrm_inst[k].record=rec;wrm_inst[k].actor=a;
        wrm_log("mansion worm noted; it binds on its first frame (actor, worm)",a,k);
    }
    original();
}

/* ent is one of this level's map worms (in scene 100): its index, or -1. */
static int wrm_worm(uint32_t ent)
{
    int k=wrm_find(ent);
    return k>=0 && WORD_AT(WRM_SCENE)==100u && wrm_on()?k:-1;
}
/* ent is the head of the worm whose code runs now. */
static int wrm_head(uint32_t ent)
{
    int k=wrm_worm(ent);
    return k>=0 && wrm_i==&wrm_inst[k] && wrm_state_ok();
}

/* Six route pads around the spawn point (the box the worm targets), nearest
 * first with a gap between them, then in angle order (the stock patrol goes
 * to (i + 1) mod 6). Fewer repeat; none: the head's own pad; returns the
 * number found. */
static unsigned wrm_route(uint32_t a,float sx,float sy,float sz)
{
    uint32_t base=WORD_AT(WRM_PADS),n=WORD_AT(WRM_PAD_COUNT),i,k,j,p,t;
    float px[WRM_ROUTE_N],pz[WRM_ROUTE_N],ang[WRM_ROUTE_N],d,dx,dz;
    uint32_t reject[16];
    unsigned found=0,rejects=0,r;
    if(n>WRM_PAD_SCAN) n=WRM_PAD_SCAN;
    while(found<WRM_ROUTE_N) {
        float bd=-1.0f;
        uint32_t best=0xFFFFFFFFu;
        for(i=0;base && i<n;i++) {
            p=base+i*WRM_PAD;
            if(!MAPMAKER_PTR_OK(p,WRM_PAD) || !(WORD_AT(p+4u)&1u)) continue;
            dx=wrm_getf(p+0xCu)-sx;dz=wrm_getf(p+0x14u)-sz;
            if(!wrm_sane(dx) || !wrm_sane(dz) || fabsf(dx)>WRM_BOX_HX || fabsf(dz)>WRM_BOX_HZ ||
               fabsf(wrm_getf(p+0x10u)-sy)>WRM_PAD_DY) continue;
            for(r=0;r<rejects && reject[r]!=i;r++) {}
            if(r<rejects) continue;
            for(k=0;k<found;k++) {
                float ex=wrm_getf(p+0xCu)-px[k],ez=wrm_getf(p+0x14u)-pz[k];
                if(wrm_lv.pad[k]==i || ex*ex+ez*ez<WRM_PAD_GAP*WRM_PAD_GAP) break;
            }
            if(k<found) continue;
            d=dx*dx+dz*dz;
            if(bd<0.0f || d<bd) { bd=d;best=i; }
        }
        if(best==0xFFFFFFFFu) break;
        if(guest_call(sub_0009A420,1,best)!=base+best*WRM_PAD) {     /* the stock lookup refuses it */
            if(rejects>=16u) break;
            reject[rejects++]=best;
            continue;
        }
        p=base+best*WRM_PAD;
        wrm_lv.pad[found]=best;px[found]=wrm_getf(p+0xCu);pz[found]=wrm_getf(p+0x14u);
        ang[found]=atan2f(pz[found]-sz,px[found]-sx);
        found++;
    }
    for(k=1;k<found;k++)                                  /* angle order: a loop around the spawn */
        for(j=k;j>0 && ang[j-1]>ang[j];j--) {
            float f=ang[j];ang[j]=ang[j-1];ang[j-1]=f;
            t=wrm_lv.pad[j];wrm_lv.pad[j]=wrm_lv.pad[j-1];wrm_lv.pad[j-1]=t;
        }
    if(!found) {
        int32_t own=(int32_t)WORD_AT(a+WRM_ACTOR_PAD);
        if(own<0 || (uint32_t)own>=n || guest_call(sub_0009A420,1,(uint32_t)own)!=base+(uint32_t)own*WRM_PAD) return 0;
        wrm_lv.pad[0]=(uint32_t)own;
        found=1;
    }
    for(k=found;k<WRM_ROUTE_N;k++) wrm_lv.pad[k]=wrm_lv.pad[k%found];
    return found;
}

/* The head's first tick: worm 0 at the spawn point, the mansion box around
 * it, the route, enable. */
static void wrm_setup(uint32_t ent)
{
    uint32_t w=wrm_lv.state,a=WORD_AT(ent+WRM_ENT_AI),k;
    float sx=wrm_getf(ent+WRM_ENT_X),sy=wrm_getf(ent+WRM_ENT_X+4u),sz=wrm_getf(ent+WRM_ENT_X+8u);
    unsigned pads;
    if(!wrm_sane(sx) || !wrm_sane(sy) || !wrm_sane(sz)) {
        wrm_lv.failed=1;
        wrm_log("mansion worm spawn not a usable point; bot behaviour",WORD_AT(ent+WRM_ENT_X),WORD_AT(ent+WRM_ENT_X+8u));
        return;
    }
    if(!(pads=wrm_route(a,sx,sy,sz))) {
        wrm_lv.failed=1;
        wrm_log("no pads near the mansion worm's spawn; bot behaviour",WORD_AT(WRM_PAD_COUNT),WORD_AT(a+WRM_ACTOR_PAD));
        return;
    }
    wrm_lv.dx=sx-WRM_BOX_X;wrm_lv.dz=sz-WRM_BOX_Z;wrm_lv.dy=sy-WRM_GROUND;
    for(k=0;k<2u+WRM_W_TRAILS;k++) {                   /* position, previous, trail */
        uint32_t p=k==0?w:k==1?w+0xCu:w+WRM_W_TRAIL+(k-2u)*12u;
        wrm_setf(p,sx);wrm_setf(p+4u,sy);wrm_setf(p+8u,sz);
    }
    WORD_AT(w+WRM_W_PAD)=0xFFFFFFFFu;WORD_AT(w+WRM_W_PAD+4u)=0xFFFFFFFFu;
    WORD_AT(WRM_ENABLE)=1u;
    wrm_lv.ready=1;
    /* A bound worm stays buried (state 1: only its burrow is drawn) until a
     * player is in its wake region; the .28 playtest stood outside all three. */
    fprintf(stderr,"[mapmaker] worm: worm %d (level %u): mansion garden moved by (%.2f, %.2f), floor %.2f (mission 5: -2.4), "
            "%u route pads; buried until a player is in x %.1f..%.1f, z %.1f..%.1f\n",(int)(wrm_i-wrm_inst),wrm.serial,
            (double)wrm_lv.dx,(double)wrm_lv.dz,(double)sy,pads,(double)(WRM_WAKE_X0+wrm_lv.dx),(double)(WRM_WAKE_X1+wrm_lv.dx),
            (double)(WRM_WAKE_Z0+wrm_lv.dz),(double)(WRM_WAKE_Z1+wrm_lv.dz));
    fflush(stderr);
}
/* Shift (on) or restore (off) the constants (all for the update, the last
 * WRM_NDRAW plus the wall for the draw) and swap in the route pads. A
 * constant that does not hold its stock bits is left alone. */
static void wrm_shift(int on,int draw)
{
    unsigned k,first=draw?WRM_NCONST-WRM_NDRAW:0u;
    float off;
    if(on) {
        for(k=first;k<WRM_NCONST;k++) {
            wrm_lv.saved[k]=WORD_AT(wrm_consts[k].addr);
            if(wrm_lv.saved[k]!=wrm_consts[k].bits) continue;
            off=wrm_consts[k].axis==0?wrm_lv.dx:wrm_consts[k].axis==1?wrm_lv.dy:wrm_lv.dz;
            wrm_setf(wrm_consts[k].addr,wrm_getf(wrm_consts[k].addr)+off);
        }
        if(draw) {
            wrm_lv.wall=WORD_AT(WRM_WALL);
            if(wrm_lv.wall==WRM_WALL_BITS) WORD_AT(WRM_WALL)=WRM_FLT_MAX;   /* never past the mansion wall */
        } else {
            for(k=0;k<WRM_ROUTE_N;k++) { wrm_lv.route[k]=WORD_AT(WRM_ROUTE+k*4u);WORD_AT(WRM_ROUTE+k*4u)=wrm_lv.pad[k]; }
            wrm_lv.swapped=1;
        }
        wrm_lv.shifted=1;
    } else if(wrm_lv.shifted) {
        for(k=first;k<WRM_NCONST;k++) WORD_AT(wrm_consts[k].addr)=wrm_lv.saved[k];
        if(draw) WORD_AT(WRM_WALL)=wrm_lv.wall;
        else if(wrm_lv.swapped) {
            for(k=0;k<WRM_ROUTE_N;k++) WORD_AT(WRM_ROUTE+k*4u)=wrm_lv.route[k];
            wrm_lv.swapped=0;
        }
        wrm_lv.shifted=0;
    }
}

/* A room of this map: 1..[0x007B975C] (the level object table's rooms). */
static int wrm_room_ok(uint32_t r) { return r>=1u && r<=WORD_AT(WRM_ROOM_COUNT); }
/* The head's room after the update. The binding (sub_001D9AF0 0x001D9B48)
 * puts the head in room 0x18, the mission 5 garden; on a MapMaker map that is
 * whatever tile has that index. Both damage rays (the burst at 0x001DC3E8 and
 * the attack at 0x001DCCC1, sub_00088C60 -> sub_00088950) start from the
 * head's +0x34, so they missed the player (v0.4.0-mapmaker.27: no damage).
 * The head gets its spawn room (the MapMaker AI's own, kept before the bind)
 * or, surfaced with a player within WRM_REACH, the nearest player's room. */
static void wrm_room(uint32_t ent)
{
    uint32_t w=wrm_lv.state,room=wrm_lv.room,players=WORD_AT(WRM_PLAYERS),n=WORD_AT(WRM_PLAYER_N),i,r;
    float best=WRM_REACH*WRM_REACH;
    if(!room) return;                                  /* no spawn room: stock */
    if(n>WRM_PLAYER_MAX) n=WRM_PLAYER_MAX;
    for(i=0;WORD_AT(w+WRM_W_STATE)>=WRM_SURFACED && i<n && MAPMAKER_PTR_OK(players,(i+1u)*WRM_PLAYER);i++) {
        uint32_t pl=players+i*WRM_PLAYER,pe=WORD_AT(pl+0x14u);
        float dx=wrm_getf(pl+0xA8u)-wrm_getf(w),dy=wrm_getf(pl+0xACu)-wrm_getf(w+4u),dz=wrm_getf(pl+0xB0u)-wrm_getf(w+8u),d;
        if((int32_t)WORD_AT(pl+4u)<0 || !MAPMAKER_PTR_OK(pe,0x40u) || !wrm_room_ok(r=WORD_AT(pe+WRM_ENT_ROOM))) continue;
        d=dx*dx+dy*dy+dz*dz;
        if(wrm_sane(d) && d<best) { best=d;room=r; }
    }
    WORD_AT(ent+WRM_ENT_ROOM)=room;WORD_AT(ent+WRM_ENT_ROOM+4u)=room;
}
/* Each state change (up to 48 a level per worm): state, head anim (anim +0xD4)
 * and its done flag (+0x124, sub_00059DD0), room. */
static void wrm_states(int k,uint32_t ent)
{
    uint32_t st=WORD_AT(wrm_lv.state+WRM_W_STATE),ctrl=WORD_AT(ent+4u);
    int ok=MAPMAKER_PTR_OK(ctrl,0x130u);
    if(st==wrm_lv.last_state) return;
    if(wrm_lv.states++<48u && wrm.levels<160u) {
        wrm.levels++;
        fprintf(stderr,"[mapmaker] worm: worm %d state %u -> %u (level %u, head anim 0x%X done %u, room %u, height %.2f)\n",k,
                (unsigned)wrm_lv.last_state,(unsigned)st,wrm.serial,ok?(unsigned)WORD_AT(ctrl+0xD4u):0u,ok?(unsigned)WORD_AT(ctrl+0x124u):0u,
                (unsigned)WORD_AT(ent+WRM_ENT_ROOM),(double)wrm_getf(ent+WRM_ENT_X+4u));
        fflush(stderr);
    }
    wrm_lv.last_state=st;
}

/* WRAP sub_000D8E30 (cdecl entity; every character's update, slot 3): for this
 * map's worm head, the stock worm update instead of the character think. */
void pc_mapmaker_worm_tick(void (*original)(void))
{
    uint32_t ent,r;
    int k,prev;
    if(!wrm_n || !MAPMAKER_STACK_OK(8) || (k=wrm_worm(ent=WORD_AT(g_esp+4u)))<0) { original();return; }
    prev=wrm_enter(k);                                 /* this worm's block and heads live */
    if(!wrm_lv.ready && !wrm_lv.failed) wrm_setup(ent);
    if(wrm_lv.failed) { wrm_leave(prev);original();return; }
    if(WORD_AT(WRM_HEADS)!=ent && wrm_room_ok(r=WORD_AT(ent+WRM_ENT_ROOM))) wrm_lv.room=r;   /* not bound yet: its own room */
    wrm_shift(1,0);
    guest_call(sub_001DAAB0,0);
    wrm_shift(0,0);
    if(WORD_AT(WRM_HEADS)==ent) {
        if(!wrm_lv.head) wrm_log("mansion worm head room (spawn room, binding's)",wrm_lv.room,WORD_AT(ent+WRM_ENT_ROOM));
        wrm_lv.head=ent;
        wrm_room(ent);
        if(!WORD_AT(ent+WRM_SLOT3)) WORD_AT(ent+WRM_SLOT3)=WRM_CHAR_UPDATE;   /* bound: keep ticking */
    }
    wrm_states(k,ent);
    if(!wrm.ticks++) wrm_log("mansion worm runs its story update (worm, state)",(uint32_t)k,WORD_AT(wrm_lv.state+WRM_W_STATE));
    wrm_leave(prev);
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_001D5E20 (cdecl entity; the head's slot 12, set by the binding at
 * 0x001D9B1A, run by sub_000A7090 when the entity goes): stock writes 4 (gone)
 * to [0x0075B4B4] + (head +0x60 - 'WRM1') * 0x450 + 0x24. A MapMaker head's
 * +0x60 is not 'WRM1' (the hit hook sets it only during the hit callback) and
 * [0x0075B4B4] is the live block or none: a write far outside any block. For a
 * map worm's head: its own worm 0 gone and its head slot cleared. */
void pc_mapmaker_worm_gone(void (*original)(void))
{
    uint32_t ent,j;
    unsigned k;
    int prev;
    if(!wrm_n || !MAPMAKER_STACK_OK(8)) { original();return; }
    ent=WORD_AT(g_esp+4u);
    for(k=0;k<wrm_n && !(ent && wrm_inst[k].head==ent && wrm_inst[k].state);k++) {}
    if(k==wrm_n) { original();return; }
    prev=wrm_enter((int)k);
    WORD_AT(wrm_lv.state+WRM_W_STATE)=WRM_GONE;
    for(j=0;j<WRM_WORMS;j++) if(WORD_AT(WRM_HEADS+j*4u)==ent) WORD_AT(WRM_HEADS+j*4u)=0;
    wrm_lv.head=0;
    if(wrm.gones++<4u) wrm_log("mansion worm head removed: worm gone (worm, head)",k,ent);
    wrm_leave(prev);
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_0015BA10 (cdecl record tag -> entity of the active AI, or 0): the
 * worm's head lookup finds this map's worm for 'WRM1' and nothing else. */
void pc_mapmaker_worm_tag(void (*original)(void))
{
    uint32_t tag,ent;
    if(!wrm_lv.ready || !MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=WRM_RET_TAG || !wrm_state_ok()) { original();return; }
    tag=WORD_AT(g_esp+4u);
    ent=MAPMAKER_PTR_OK(wrm_lv.actor,WRM_ACTOR)?WORD_AT(wrm_lv.actor):0u;
    g_eax=tag==WRM_TAG && wrm_head(ent)?ent:0u;
    if(g_eax) wrm_log("mansion worm head bound (head, worm)",ent,(uint32_t)(wrm_i-wrm_inst));
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_001D1F30 (cdecl hit record; the head's hit callback): the stock
 * callback with this level's stand-ins; the kill counter never reaches 5. */
static void wrm_hit(uint32_t ent,void (*original)(void));
void pc_mapmaker_worm_hit(void (*original)(void))
{
    uint32_t info,ent;
    int k,prev;
    if(!wrm_n || !MAPMAKER_STACK_OK(8) || !MAPMAKER_PTR_OK(info=WORD_AT(g_esp+4u),0x30u) ||
       (k=wrm_worm(ent=WORD_AT(info)))<0 || !wrm_inst[k].ready) { original();return; }
    prev=wrm_enter(k);
    wrm_hit(ent,original);
    wrm_leave(prev);
}
/* The hit callback for the worm whose globals are live. */
static void wrm_hit(uint32_t ent,void (*original)(void))
{
    uint32_t script,objects,effect,tag,st;
    script=WORD_AT(WRM_SCRIPT);objects=WORD_AT(WRM_OBJECTS);effect=WORD_AT(WRM_EFFECT);tag=WORD_AT(ent+WRM_ENT_TAG);
    st=WORD_AT(wrm_lv.state+WRM_W_STATE);
    WORD_AT(wrm_lv.scratch+WRM_KILLS)=0;
    WORD_AT(WRM_SCRIPT)=wrm_lv.scratch;WORD_AT(WRM_OBJECTS)=wrm_lv.scratch+WRM_FAKE_TABLE;
    WORD_AT(WRM_EFFECT)=wrm_lv.effect;WORD_AT(ent+WRM_ENT_TAG)=WRM_TAG;
    original();
    WORD_AT(WRM_SCRIPT)=script;WORD_AT(WRM_OBJECTS)=objects;WORD_AT(WRM_EFFECT)=effect;WORD_AT(ent+WRM_ENT_TAG)=tag;
    WORD_AT(wrm_lv.scratch+WRM_KILLS)=0;
    if(WORD_AT(wrm_lv.state+WRM_W_STATE)!=st && wrm.hits++<8u)
        wrm_log("mansion worm hit (state before, after)",st,WORD_AT(wrm_lv.state+WRM_W_STATE));
}

/* After the draw: the surfaced head (states >= 8, placed by sub_001D9B80 at
 * the worm's height, the garden floor -2.4 shifted to the spawn floor). After
 * a state-2 burst (+0x44C 2) stock puts it at the fixed point (-62.31, 0,
 * 16.9) (0x001DA94C..0x001DA969): x/z follow the shifted constants, y 0 gets
 * the floor offset here. Each surfacing is logged against the spawn floor. */
static void wrm_height(void)
{
    uint32_t w=wrm_lv.state,head=WORD_AT(WRM_HEADS);
    if(!head || !MAPMAKER_PTR_OK(head,0x200u) || WORD_AT(w+WRM_W_STATE)<WRM_SURFACED) { wrm_lv.surfaced=0;return; }
    if(WORD_AT(w+WRM_W_ROUTE_FROM)==2u) wrm_setf(head+WRM_ENT_X+4u,wrm_getf(head+WRM_ENT_X+4u)+wrm_lv.dy);
    if(!wrm_lv.surfaced && wrm.surfacings++<8u) {
        fprintf(stderr,"[mapmaker] worm: worm %d surfaced, state %u, head height %.2f, spawn floor %.2f, worm height %.2f, room %u%s\n",
                (int)(wrm_i-wrm_inst),(unsigned)WORD_AT(w+WRM_W_STATE),(double)wrm_getf(head+WRM_ENT_X+4u),(double)(WRM_GROUND+wrm_lv.dy),
                (double)wrm_getf(w+4u),(unsigned)WORD_AT(head+WRM_ENT_ROOM),WORD_AT(w+WRM_W_ROUTE_FROM)==2u?" (state-2 burst point)":"");
        fflush(stderr);
    }
    wrm_lv.surfaced=1;
}

/* WRAP sub_00126560 (frame, return 0x001CEE8C): the stock worm draw (bodies,
 * surfaced heads) in scene 100. */
void pc_mapmaker_worm_fx(void (*original)(void))
{
    int here=wrm_n && MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==WRM_RET_FX,prev;
    unsigned k;
    original();
    if(!here || WORD_AT(WRM_SCENE)!=100u || !wrm_on()) return;
    for(k=0;k<wrm_n;k++) {                             /* each map worm with its own block */
        if(!wrm_inst[k].ready) continue;
        prev=wrm_enter((int)k);
        if(wrm_state_ok()) {
            wrm_shift(1,1);
            wrm_lv.drawing=1;
            guest_call(sub_001DA990,0);
            wrm_lv.drawing=0;
            wrm_shift(0,1);
            wrm_height();
            if(!wrm.draws++) wrm_log("mansion worm drawn (worm, enable)",k,WORD_AT(WRM_ENABLE));
        }
        wrm_leave(prev);
    }
}

/* WRAP sub_00234CD0 (cdecl tag; runs the level-script object with that tag):
 * the update's 'SFCC' (0x001DC45A) is the mansion's script, whose run reaches
 * level markers and tag lookups (sub_0025F0C0): dropped in scene 100. */
void pc_mapmaker_worm_script(void (*original)(void))
{
    if(wrm_lv.ready && MAPMAKER_STACK_OK(8) && WORD_AT(g_esp)==WRM_RET_SCRIPT && WORD_AT(WRM_SCENE)==100u) {
        if(!wrm.scripts++) wrm_log("mansion worm: the mansion script trigger dropped",WORD_AT(g_esp+4u),0);
        MAPMAKER_GUEST_RET();
        return;
    }
    original();
}

/* WRAP sub_001D9370 and sub_001D7200 (no arguments): the mansion's own parts
 * of the worm draw, not in this area's draw. */
void pc_mapmaker_worm_props(void (*original)(void))
{
    if(wrm_lv.drawing && MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==WRM_RET_PROPS) { MAPMAKER_GUEST_RET();return; }
    original();
}
void pc_mapmaker_worm_extra(void (*original)(void))
{
    if(wrm_lv.drawing && MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==WRM_RET_EXTRA) { MAPMAKER_GUEST_RET();return; }
    original();
}
