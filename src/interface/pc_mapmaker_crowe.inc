/* US 45410066 MapMaker real story boss: the split Crowe of story mission 10
 * (far future 2, scene 14) as a MapMaker Story AI. Area `crowe`; every hook
 * is inert unless TSFP_MAPMAKER_LIMITS=1 (TSFP_MAPMAKER_REAL_BOSS=0 keeps the
 * bot AI). Docs: docs/mapmaker/mapmaker-crowe.md (CONFIRMED/INFERRED labels,
 * the wrapper list for the integrator). Tests: tests/mapmaker/test_mapmaker_crowe.py.
 *
 * The boss (CONFIRMED, US XBE data and decode): character id 0x46 (character
 * table: voice set 0x00539948, model 0x5E8 CHR_FARFUTURE2_SPLITTERCROW
 * chrs/splittercrow in pak/chr.pak, no LOD; no Arcade row). Story AI class
 * 0x40000 (record +4 -> AI +0x14, +0x10 = 0): sub_0027D590 0x0027DDE5 ->
 * call sub_0028FE30(entity) at 0x0027DE39; messages sub_0027CEF0 0x0027D36B
 * -> sub_00292430 (0x0027D3E9), which handles the death (message 2: anim
 * 0x815, state 0x11, -1) in every scene, so no message hook. Its eight
 * anims splitcrowe* 0x80E..0x815 are named only by pak/stream/lv14/anim/lv14.pak
 * (string 0x0049DC78, registered by sub_001CA2F0 at 0x001CA316, table
 * 0x0047E938, 41 names, no id shared with lv1_3/lv1_6/lv81). Per-level state:
 * stock sub_0025F720 calls, for scene 14 only (0x0025F760), sub_0028D6D0
 * (sub_002A8300 effect blocks [0x0078B090]/[0x0078B098], [0x0075F5E8] =
 * sub_0003DFA0(0x2E04, 1), tail sub_0028D0C0 = every field and the arena
 * grid), sub_00292630 (the giant robot's, not needed) and sub_002A62C0 (the
 * particle block [0x0078B0D0], also used by the giant skeleton and the Crow
 * walker). [0x0075F5E8] is read only by the Crowe functions 0x0028D0C0..
 * 0x00292430; the giant robot (sub_00293730) never reads it.
 *
 * Why it needs more than a class: the fight is anchored to the mission 10
 * arena (CONFIRMED, decode):
 *  - Arena grid: sub_0028D0C0 builds, with sub_002A93D0(state+0x970, +0xEE8,
 *    +0xF10, +0xF60, +0x1190, +0x1198, 283.45, 61.2), 5 x 7 cells of 0x28
 *    bytes at state+0x970 (+0 x0, +4 z0, +8 x1, +0xC z1, +0x10/+0x14 centre,
 *    +0x18/+0x1C half size, +0x20 type), the outer rectangle at +0xEE8 and
 *    14 edge rectangles at +0xF60, in world x 283.45..326.71, z 61.2..100.7.
 *    State 0 puts the boss on the centre of its cell (0x00290E81/0x00290EAF);
 *    every move is to a cell centre. The start cell is the one holding
 *    (310.0, the boss's z) (0x00290DC2: x = [0x0049F3D8] = 310.0; lookup
 *    sub_002A9D00 -> sub_002A9330 into +0x1198/+0x119C, -1 from the build
 *    when no cell holds it). relocate: on its first frame the whole grid is
 *    moved in x/z so the start cell (the 310.0 column's platform nearest the
 *    arena's centre) is centred on the Story AI's spawn point, and the
 *    first frame ends with the boss on the spawn point and that cell.
 *  - Floor height 86 (mission 10): the death puts the boss at (310.49, 86,
 *    80.95) (0x002922FB..0x0029231D); the falling hazards (+0x11A0, eight
 *    0x168 objects: spawned at y 86 by sub_002AB1B0 from 0x00291E7F, moved by
 *    sub_002AA6A0, which explodes them (sub_000EFBD0 0x22/0x20/0x1B) below
 *    86.205 and compares 88.25/88.4), the debris floor of sub_002AD3D0 (86)
 *    and the spark floor sub_002A85C0(86.4) pushed at 0x00290BF8. fight
 *    shifts these image constants (each read only by these Crowe functions,
 *    and 0x0049F3C4 = 86.0 also by sub_0028DD50, as a distance threshold, and
 *    by the Crow walker sub_002998E0, not called from the fight) by the spawn
 *    height - 86 (x/z of the death point by the grid offset) for the
 *    duration of the fight call only, then restores the stock bits; near
 *    restores 86.0 inside sub_0028DD50; sparks shifts the pushed 86.4.
 *  - Intro: state 1 waits for player 0 in the scripted mode +0x114 == 0x100
 *    (set by sub_000D4B00, the mission's intro cutscene; 0x00290FE5), which
 *    a MapMaker map never enters: the boss would idle forever. fight turns
 *    state 1 into state 2, whose first test (player not scripted,
 *    0x002911B5) starts the fight (state 4, parts 30/20/20/20/20) as stock
 *    does after the cutscene.
 *  - Parts: a destroyed turret (sub_0028E770(part, angle) from 0x0028ECEC)
 *    spawns prop 0x4BE + part, whose model files are only in
 *    story/l_14_st.pak (not mounted in MapMaker). part: in scene 100 only
 *    the stock explosion sub_000EFBD0(0x20, 0x17, state+0x2BFC+part*0x5C, 0,
 *    0, 0, 0, 1) (0x0028E802); the created prop is only stored (+0x2C30, never
 *    read).
 *  - Level event: at the end of its death, sub_002275F0(0) (call 0x002923A3,
 *    return 0x002923A8) sets level marker 0 to state 5; in MapMaker that is
 *    the map's own logic marker 0: dropped. The death then leaves it in state
 *    0x12 (no case; the level script ends mission 10). Every fight call, before
 *    its state switch, still updates the sparks (sub_002A85C0), hazards and
 *    particles (sub_002A7D60 at 0x00290D3E, which ages them, recounts the live
 *    ones in [0x0078B0CC] and emits smoke at every destroyed part while the
 *    parts mask +0x2E00 is set, which the death leaves at 0x2F): stock smokes
 *    on until the mission ends. fight keeps calling the original in state
 *    0x12 with +0x2E00 = 0 for the call (no new smoke), and once no particle
 *    is live (or after 300 calls) removes the AI with sub_0015DDA0; fx then
 *    stops drawing, so nothing it drew stays frozen. No sub_002274C0 and no tag lookup
 *    (sub_0023FF20) is reachable from the fight (test).
 *  - Draw: the frame sub_001CEBC0 draws the boss's effects (sub_0028D710,
 *    no args, returns when [0x0075F5E8] or its +0 is 0) only in scene 14
 *    (0x001CEE53). fx: after sub_00126560 (call 0x001CEE87, return
 *    0x001CEE8C), with this level's state, in scene 100.
 *  - HUD: the fight sets the story focus (0x00290F61) and makes a +0x268
 *    record (0x00290F67); hud clears the focus for the stock bar
 *    (sub_0014CC80 case 0x5E8, label 0x163B outside scene 14) and meter hides
 *    the overhead meter unless the Story AI chose the Sub-boss bar, as the
 *    realbosses area does for the others.
 *
 * Several bosses: up to CRW_MAX split Crowes per map each have their own
 * state pointer and their own copy of the boss particle/effect statics
 * (pc_mapmaker_bossfx.inc, one live holder shared with the realbosses area),
 * swapped in around every entry into their code (fight, message handler,
 * think and hit callbacks, draw) and put back after; the boss whose copy is
 * live is crw_lv. Docs: "Several split Crowes".
 *
 * Eggs: the falling eggs need prop 0x4BD, whose model only l_14_st.pak has;
 * the drop is skipped (eggs, below; the v0.4.0-mapmaker.28 crash), but the
 * cells it would block are still blocked for a while (cells), or the walk
 * picks the same cell again and the boss never walks (.31). The map: cells
 * in no room at the boss's height are blocked (crw_rooms), a walk that stops
 * getting nearer its target ends where the boss stands and blocks the cells
 * ahead (crw_stop; up to .34 the boss was put on the target, into walls), a
 * player outside the arena counts as its nearest cell (crw_player). Each
 * boss logs its fight states (crw_trace). Docs: "Walls (.34)".
 *
 * Requires the includer's WORD_AT, guest_call, g_esp/g_eax, g_xbox_mem_offset,
 * pc_mapmaker_area_enabled and pc_mapmaker_limits.inc. Hot hooks (hud, meter,
 * fx: every frame) test a fixed value first. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif

#define CRW_SCENE 0x0079094Cu
#define CRW_MODE 0x007DE455u            /* byte: 10 = story */
#define CRW_ID 0x46u                    /* character id */
#define CRW_MODEL 0x5E8u                /* CHR_FARFUTURE2_SPLITTERCROW */
#define CRW_CLASS 0x40000u              /* record +4 -> AI +0x14: sub_0028FE30 */
#define CRW_PAK 0x0049DC78u             /* "pak/stream/lv14/anim/lv14.pak" */
#define CRW_MOUNTS 0x006D7880u          /* mounted paks, table of 30 */
#define CRW_MOUNT_MAX 64u               /* paktable.json: 64 slots at 0x00907000 */
#define CRW_MOUNT_KEEP 8u
#define CRW_STATE 0x0075F5E8u           /* 0x2E04 bytes, sub_0028D6D0 */
#define CRW_STATE_SIZE 0x2E04u
#define CRW_PARTS 0x0078B0D0u           /* 0x7D00 bytes, sub_002A62C0 */
#define CRW_PARTS_SIZE 0x7D00u
#define CRW_LIVE 0x0078B0CCu            /* live particles, recounted by sub_002A7D60 every fight call */
#define CRW_PARTS_ON 0x2E00u            /* state: parts present (0x2F); the smoke is emitted while non-zero */
#define CRW_FADE_MAX 300u               /* fight calls after the death before the boss goes anyway */
#define CRW_FX0 0x0078B090u             /* effect blocks, sub_002A8300 */
#define CRW_FX1 0x0078B098u
#define CRW_GRID 0x970u                 /* 35 cells of 0x28 */
#define CRW_CELLS 35u
#define CRW_CELL 0x28u
#define CRW_ROWS 5u                     /* rows by z, 7 columns by x */
#define CRW_COLS 7u
#define CRW_START_ROW 0x1198u           /* the boss's cell (state 0 start, moves) */
#define CRW_START_COL 0x119Cu
#define CRW_START_X 310.0f              /* 0x0049F3D8: stock state 0 x */
#define CRW_OUTER 0xEE8u
#define CRW_EDGE 0xF60u                 /* 14 rectangles, to +0x1190 */
#define CRW_EDGES 14u
#define CRW_SHOT 0x2BFCu                /* part 0 position; parts 0x5C apart */
#define CRW_PART_STRIDE 0x5Cu
#define CRW_PART_MAX 3u                 /* back, right, left turret (0x4BE..0x4C0) */
#define CRW_AI_STATE 0x20u              /* AI +0x20 state, +0x9D0 next, +0x9D4 previous, +0x9D8 time */
#define CRW_FLOOR 86.0f                 /* the mission 10 arena floor */
#define CRW_CONTAINER 0x00784014u
#define CRW_MAGIC 0x00C77667u
#define CRW_TAG_INFO 0x00C77668u
#define CRW_TAG_SAI 0x00C7766Cu
#define CRW_TAG_END 0x00C7766Au
#define CRW_TAG_SETS 0x00C776F5u
#define CRW_SCAN 0x00080000u
#define CRW_SET_MAP 0x11u
#define CRW_INFO_SLOTS 10u
#define CRW_SAI_INDEX 0x22u
#define CRW_SAI_BOSS 0x29u
#define CRW_SAI_MIN 0x44u
#define CRW_SAI_BASE 0x00783A50u
#define CRW_BOSS_BAR 1u
#define CRW_BOSS_SUB 2u
#define CRW_ACTOR 0xEA0u
#define CRW_ACTOR_CHAR 0x48u
#define CRW_ACTOR_RECORD 0xBF0u
#define CRW_ENT_AI 0x7Cu
#define CRW_ENT_MODEL 0x94u
#define CRW_ENT_X 0x64u                 /* +0x64 x, +0x68 y (up), +0x6C z */
#define CRW_PLAYER 0x007B0C7Cu
#define CRW_FOCUS 0xFF4u
#define CRW_RET_NEAR 0x00290BE7u        /* sub_0028FE30: call sub_0028DD50 at 0x00290BE2 */
#define CRW_RET_SPARKS 0x00290C02u      /* sub_0028FE30: push 86.4; call sub_002A85C0 at 0x00290BFD */
#define CRW_RET_PART 0x0028ECF1u        /* sub_0028E9E0: call sub_0028E770 at 0x0028ECEC */
#define CRW_RET_EVENT 0x002923A8u       /* sub_0028FE30: call sub_002275F0(0) at 0x002923A3 */
#define CRW_RET_EGG 0x00291E84u         /* sub_0028FE30: call sub_002AB1B0 (egg drop) at 0x00291E7F */
#define CRW_RET_CELLS 0x002901BFu       /* sub_0028FE30: call sub_002A9D00 (cell lookups) at 0x002901BA, every call */
#define CRW_RET_CELLS0 0x00290E67u      /* ... and at 0x00290E62 in state 0 */
#define CRW_BLOCKED 0x24u               /* cell: 1 = an egg on it or next to it (sub_002A9D00, every call) */
#define CRW_HAZARD 0x11A0u              /* eight eggs of 0x168 (+0xEC row, +0xF0 col, +0xF8 phase: 0 = none) */
#define CRW_HAZARD_SIZE 0x168u
#define CRW_EGGS 8u
#define CRW_EGG_CALLS 120u              /* fight calls a skipped egg keeps its cells blocked (INFERRED: its fall and landing) */
#define CRW_TARGET 0x1CECu              /* move target: +0x1CEC/+0x1CF0 x/z (cell centre), +0x1CF4/+0x1CF8 row/col */
#define CRW_PCELL 0x1190u               /* the player's cell (row, col) */
#define CRW_DIR 0x1D24u                 /* the move's direction 0..3 (-1: no walk) */
#define CRW_BEAM 0x2DDCu                /* 1 while a turret beam is on the player: state 4 waits */
#define CRW_JUMP 9u                     /* AI state: the walk (0x80E), done when the boss passes the target */
#define CRW_AIR_MAX 600u                /* fight calls in state 9 before an unfinished walk is ended where the boss stands */
#define CRW_STALL_CALLS 90u             /* ... or fight calls in a row without getting CRW_STALL_STEP nearer its target */
#define CRW_STALL_STEP 0.25f
#define CRW_NEXT 0x2DE8u                /* the state the walk goes to on arrival (0x00291C69) */
#define CRW_ARRIVED 0x2DF0u             /* zeroed on arrival (0x00291CA8) */
#define CRW_ANIM_STOP 0x810u            /* splitcrowewalktostand: sub_0005F460(entity, 0x810, 0, 1.2) on arrival (0x00291C4F..) */
#define CRW_ANIM_SPEED 0x3F99999Au
#define CRW_DECIDE 4u                   /* AI state 4: the next move is chosen */
#define CRW_WALL_CALLS 1800u            /* fight calls the cells ahead of a stopped walk stay blocked */
#define CRW_QUERY_OUT 8u                /* room query results kept (only the count is used) */
#define CRW_BODY_HALF 0.3f              /* the body box of the room query, as the roof area's fall check */
#define CRW_BODY_LOW 0.25f
#define CRW_BODY_HIGH 1.5f
#define CRW_PROBE_UP 0x3FA00000u        /* 1.25: the floor finder's search height, as the pad snap (0x0009AFCE) */
#define CRW_PROBE_R 0x3DCCCCCDu         /* 0.1: its radius, as the pad snap */
#define CRW_STEP 1.0f                   /* floor height difference a walk may step (more: a ledge or a drop) */
#define CRW_FLOOR_Y 0x004DCB00u         /* sub_000C3C50 result: the floor height found */
#define CRW_FLOOR_KEEP1 0x004DCAFCu     /* ... and the two globals it also writes (put back after the probes) */
#define CRW_FLOOR_KEEP2 0x00733088u
#define CRW_TRACE_MAX 40u               /* fight state changes logged per boss and level */
#define CRW_BEAT 600u                   /* fight calls between two status lines */
#define CRW_BEATS 12u
#define CRW_RET_FX 0x001CEE8Cu          /* sub_001CEBC0: call sub_00126560 at 0x001CEE87 */
#define CRW_RET_METER 0x0014D264u       /* sub_0014D1F0: call sub_0013A400 at 0x0014D25F */
#define CRW_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define CRW_H(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

/* Image constants shifted during the fight call: address, stock bits, axis
 * (0 x, 1 y, 2 z). */
static const struct { uint32_t addr, bits; int axis; } crw_consts[]={
    {0x0049F3C4u,0x42AC0000u,1},   /* 86.0: hazard spawn y, debris floor, death y */
    {0x0049F4CCu,0x42AC68F6u,1},   /* 86.205: hazard explodes below */
    {0x0049F4D0u,0x42B08000u,1},   /* 88.25 */
    {0x0049F4D4u,0x42B0CCCDu,1},   /* 88.4 */
    {0x0049F3CCu,0x439B3EB8u,0},   /* 310.49: death x */
    {0x0049F3C8u,0x42A1E666u,2},   /* 80.95: death z */
    {0x0049F3D8u,0x439B0000u,0},   /* 310.0: state 0 x before the cell centre */
};
#define CRW_NCONST (sizeof crw_consts/sizeof crw_consts[0])

extern void sub_0005A130(void),sub_00062260(void),sub_0028D6D0(void),sub_002A62C0(void),sub_0028D710(void);
extern void sub_000EFBD0(void),sub_0015DDA0(void),sub_0005F460(void),sub_00087310(void),sub_000C3C50(void);
int pc_mapmaker_area_enabled(const char *area,const char *hook);

static struct {
    int read, optout, restarted;   /* restarted: this level's Crowes get new blocks at their AI start */
    unsigned logged, meters, extras, events, parts, fx, eggs;
} crw;
/* The globals one split Crowe's code uses (CONFIRMED, every [abs] operand of
 * the functions reachable from its entry points, test): the state pointer
 * [0x0075F5E8] (written only by the allocation sub_0028D6D0) and 28 words of
 * the boss particle/effect statics 0x0078B080..0x0078B1DB (the effect blocks
 * of sub_002A8300/8380/85C0/8FC0, the particle block with its count and
 * emitter templates of sub_002A62C0/62F0/7D60/7F30), which the giant skeleton
 * and the Crow walker use too. Each boss has its own state and its own copy
 * of those statics (pc_mapmaker_bossfx.inc, shared with the realbosses area),
 * swapped in around every entry into Crowe code. */
#include "pc_mapmaker_bossfx.inc"
#define CRW_MAX 4u                      /* split Crowes with the story behaviour per map */
/* One boss of this level: its blocks, spawn record, pool entry, relocation. */
struct crw_boss_s {
    uint32_t state, record, actor;
    uint8_t boss;
    int relocated, removed, shifted, place;
    unsigned fading;                    /* fight calls since the death */
    uint32_t row, col;                  /* the start cell */
    float dx, dy, dz, sx, sz;           /* grid offset, spawn point */
    uint32_t saved[CRW_NCONST];
    uint32_t egg_row[CRW_EGGS], egg_col[CRW_EGGS];
    unsigned egg_left[CRW_EGGS];        /* fight calls a skipped egg still blocks its cells */
    unsigned eggs, air, calls, trace, beats;   /* skipped eggs, calls in state 9, fight calls, log lines */
    unsigned stall, clamps;             /* state 9 calls without progress; player cells stood in */
    float best;                         /* the walk's nearest distance to its target so far */
    uint8_t off[CRW_CELLS];             /* cell centre in no MapMaker room at the boss's height, or over a drop */
    uint8_t gap_h[CRW_ROWS][CRW_COLS-1u];   /* no floor at its height between cell (r, c) and (r, c+1) */
    uint8_t gap_v[CRW_ROWS-1u][CRW_COLS];   /* ... between (r, c) and (r+1, c) */
    unsigned wall_left[CRW_CELLS];      /* fight calls a cell stays blocked behind a stopped walk */
    MapmakerFx fx;                      /* its statics while another's (or nobody's) are live */
};
static struct crw_boss_s crw_inst[CRW_MAX], crw_none;
static unsigned crw_n;                  /* bosses with their own blocks this level */
static int crw_cur=-1;                  /* whose state pointer is live; -1: the outside value */
static uint32_t crw_out;                /* [0x0075F5E8] outside the Crowes' code */
static struct crw_boss_s *crw_i=&crw_none;
#define crw_lv (*crw_i)                 /* the boss whose code runs now (crw_none outside) */
typedef struct { int k; MapmakerFx *fx; } CrwPrev;   /* what crw_enter replaced */

static uint8_t *crw_window(void) { return (uint8_t *)((uintptr_t)MAPMAKER_FX+g_xbox_mem_offset); }
static void crw_set(int k)
{
    WORD_AT(CRW_STATE)=k<0?crw_out:crw_inst[k].state;
    crw_cur=k;
    crw_i=k<0?&crw_none:&crw_inst[k];
}
/* Make boss k's state pointer and statics live; returns what was, for
 * crw_leave. Any nesting: another Crowe inside (one Crowe's hazard hits
 * another), or this one again inside another boss's code (its hit callback
 * inside a walker's damage inside its own fight) gets its own latest values. */
static CrwPrev crw_enter(int k)
{
    CrwPrev p;
    p.k=crw_cur;
    p.fx=mapmaker_fx_enter(k<0?0:&crw_inst[k].fx);
    if(p.k!=k) {
        if(p.k<0) crw_out=WORD_AT(CRW_STATE);
        crw_set(k);
    }
    return p;
}
static void crw_leave(CrwPrev p)
{
    mapmaker_fx_leave(p.fx);
    if(crw_cur!=p.k) crw_set(p.k);
}

static void crw_log(const char *what,uint32_t a,uint32_t b)
{
    if(crw.logged>=24u) return;
    crw.logged++;
    fprintf(stderr,"[mapmaker] crowe: %s (0x%X, 0x%X)\n",what,(unsigned)a,(unsigned)b);
    fflush(stderr);
}
static int crw_on(void)
{
    mapmaker_limits_read();
    if(!crw.read) {
        const char *v=getenv("TSFP_MAPMAKER_REAL_BOSS");
        crw.read=1;
        crw.optout=v && v[0]=='0' && !v[1];
    }
    return mapmaker_limits.limits;
}
static int crw_play(void) { return crw_on() && WORD_AT(CRW_SCENE)==100u && CRW_B(CRW_MODE)==10u; }
static float crw_getf(uint32_t a) { float f;memcpy(&f,(const void *)((uintptr_t)a+g_xbox_mem_offset),4);return f; }
static void crw_setf(uint32_t a,float f) { memcpy((void *)((uintptr_t)a+g_xbox_mem_offset),&f,4); }
static int crw_sane(float f) { return f==f && f>-100000.0f && f<100000.0f; }

/* The map's custom set (index 0x11; slots 1-10 in the map-info chunk, 11-20
 * in the sets area's chunk), as pc_mapmaker_realbosses.inc reads it.
 * 1: holds the Crowe, 0: does not (or not the map's set), -1: unreadable. */
static int crw_set_has(void)
{
    uint32_t container=WORD_AT(CRW_CONTAINER),c,end,steps,info=0,isz=0,extra=0,n,k;
    if(!container || !MAPMAKER_PTR_OK(container,CRW_SCAN) || WORD_AT(container)!=CRW_MAGIC) return -1;
    c=container+8u;end=container+CRW_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u),d=c+8u;
        if(tag==CRW_TAG_END || sz>end-c-8u) break;
        if(tag==CRW_TAG_INFO && sz>=0x18u && !info) { info=d;isz=sz; }
        else if(tag==CRW_TAG_SETS && sz>=0x30u && !extra && CRW_H(d)==1u && CRW_B(d+2u)==CRW_INFO_SLOTS &&
                CRW_B(d+3u)>=1u && CRW_B(d+3u)<=CRW_INFO_SLOTS) extra=d;
        c+=8u+sz;
    }
    if(!info) return -1;
    if(CRW_H(info+0x14u)!=CRW_SET_MAP) return 0;
    n=CRW_B(info+0x16u);
    if(n>CRW_INFO_SLOTS) n=CRW_INFO_SLOTS;
    if(0x18u+n*4u>isz) n=(isz-0x18u)/4u;
    for(k=0;k<n;k++) if(CRW_H(info+0x18u+k*4u)==CRW_ID) return 1;
    for(k=0;extra && k<CRW_B(extra+3u);k++) if(CRW_H(extra+4u+k*4u)==CRW_ID) return 1;
    return 0;
}
static int crw_pak_mounted(void) { return (int32_t)guest_call(sub_00062260,1,CRW_PAK)!=-1; }
static int crw_state_ok(void) { return crw_lv.state!=0; }
/* A new level: no boss, the live globals are the outside values. */
static void crw_reset(void)
{
    if(crw_cur>=0) crw_set(-1);
    mapmaker_fx_forget(crw_inst,sizeof crw_inst);
    memset(crw_inst,0,sizeof crw_inst);
    memset(&crw_none,0,sizeof crw_none);
    crw_n=0;crw_cur=-1;crw_i=&crw_none;
    crw.logged=0;                                      /* a log budget per level (v0.4.0-mapmaker.31: spent in the first) */
    crw.restarted=0;
}
/* Boss k's blocks: the stock scene 14 allocation (sub_0028D6D0: effect
 * blocks, state, its init; sub_002A62C0: particles), captured into its copy;
 * the globals as they were (another boss's blocks, the stock values) are put
 * back. 1: allocated. */
static int crw_alloc(unsigned k)
{
    uint32_t st,parts=0,live_state=WORD_AT(CRW_STATE);
    MapmakerFx live;
    int ok=1;
    memcpy(live.b,crw_window(),MAPMAKER_FX_SIZE);
    WORD_AT(CRW_STATE)=0;
    guest_call(sub_0028D6D0,0);
    st=WORD_AT(CRW_STATE);
    if(!st || !MAPMAKER_PTR_OK(st,CRW_STATE_SIZE) || !WORD_AT(CRW_FX0) || !WORD_AT(CRW_FX1)) {
        crw_log("no level memory for a split Crowe; bot behaviour",st,WORD_AT(CRW_FX0));
        ok=0;
    } else {
        WORD_AT(CRW_PARTS)=0;
        guest_call(sub_002A62C0,0);
        parts=WORD_AT(CRW_PARTS);
        if(!parts || !MAPMAKER_PTR_OK(parts,CRW_PARTS_SIZE)) {
            crw_log("no level memory for a split Crowe's particles; bot behaviour",parts,st);
            ok=0;
        }
    }
    if(ok) {
        memcpy(crw_inst[k].fx.b,crw_window(),MAPMAKER_FX_SIZE);
        crw_inst[k].state=st;
    }
    memcpy(crw_window(),live.b,MAPMAKER_FX_SIZE);
    WORD_AT(CRW_STATE)=live_state;
    if(ok) crw_log("split Crowe state allocated (boss, particles)",k,parts);
    return ok;
}
/* The boss whose spawn record ent's AI has, or -1. */
static int crw_find(uint32_t ent)
{
    uint32_t a,rec;
    unsigned k;
    if(!crw_n || !MAPMAKER_PTR_OK(ent,0x100u) || !MAPMAKER_PTR_OK(a=WORD_AT(ent+CRW_ENT_AI),CRW_ACTOR)) return -1;
    rec=WORD_AT(a+CRW_ACTOR_RECORD);
    for(k=0;k<crw_n;k++) if(crw_inst[k].record && crw_inst[k].record==rec && crw_inst[k].state) return (int)k;
    return -1;
}

/* WRAP sub_001C8540 (the level's anim paks; sub_001CA2F0 registers names next). */
void pc_mapmaker_crowe_anims(void (*original)(void))
{
    int has;
    uint32_t mounted;
    original();
    if(!crw_play() || crw.optout || (has=crw_set_has())==0 || crw_pak_mounted()) return;
    if((mounted=WORD_AT(CRW_MOUNTS))+CRW_MOUNT_KEEP>CRW_MOUNT_MAX)
        crw_log("pak mount table full; the split Crowe keeps the bot behaviour",mounted,CRW_MOUNT_MAX);
    else if(!guest_call(sub_0005A130,1,CRW_PAK)) crw_log("lv14.pak did not mount; bot behaviour",mounted,(uint32_t)has);
    else crw_log(has<0?"mounted lv14.pak (map container unreadable)":"mounted lv14.pak for the split Crowe",mounted+1u,0);
}

/* WRAP sub_0025F720 (story level init): after the original, the scene 14
 * Crowe blocks (sub_0028D6D0, then the particle block sub_002A62C0). */
void pc_mapmaker_crowe_level(void (*original)(void))
{
    int has;
    original();
    crw_reset();                                       /* a new level: nothing of the last one */
    if(!crw_play() || crw.optout || (has=crw_set_has())==0) return;
    WORD_AT(CRW_STATE)=0;                              /* a stale pointer of an earlier level */
    if(crw_alloc(0)) {
        crw_n=1;
        if(has<0) crw_log("split Crowe state allocated with the map container unreadable",crw_inst[0].state,0);
    }
}

static uint8_t crw_boss_flag(uint32_t index)
{
    uint32_t container=WORD_AT(CRW_CONTAINER),c,end,steps;
    if(!container || !MAPMAKER_PTR_OK(container,CRW_SCAN) || WORD_AT(container)!=CRW_MAGIC) return 0;
    c=container+8u;end=container+CRW_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u);
        if(tag==CRW_TAG_END || sz>end-c-8u) break;
        if(tag==CRW_TAG_SAI && sz>=CRW_SAI_MIN && CRW_H(c+8u+CRW_SAI_INDEX)==index) {
            uint8_t f=CRW_B(c+8u+CRW_SAI_BOSS);
            if((f&0xF8u)!=0xB0u) return 0;
            return (uint8_t)((f&4u)?CRW_BOSS_SUB:CRW_BOSS_BAR);
        }
        c+=8u+sz;
    }
    return 0;
}

/* WRAP sub_001CDD60 (the level restart: the pause menu's restart,
 * sub_0003D440 with [0x006B7AA8], calls sub_001CDAF0 and then this, never
 * sub_001CDB80, so neither sub_0025F720 nor the level hook runs; level
 * memory is set up again in here, INFERRED from sub_0003E740 at 0x001CDD62). Stock resets the
 * scene 14 Crowe's state in place (sub_0025F6A0 -> sub_0028D0C0); the
 * MapMaker Crowes' blocks are level heap too. Before the original: forget
 * them (as a new level does, outside values back, [0x0075F5E8] 0); the
 * restarted Crowes come back through the AI start with the same records and
 * get new blocks there, the first one included. */
void pc_mapmaker_crowe_restart(void (*original)(void))
{
    unsigned had=crw_n;
    if(had || crw.restarted) {
        crw_reset();
        WORD_AT(CRW_STATE)=0;
        crw.restarted=crw_play() && !crw.optout;
        crw_log("level restart: split Crowe blocks forgotten; new ones at the AI start (bosses, again)",had,(uint32_t)crw.restarted);
    }
    original();
}

/* WRAP sub_0015B830 (cdecl AI pool entry; story mode copies record +0/+4 to
 * AI +0x10/+0x14 at 0x0015B9A3): up to CRW_MAX split Crowes per map get class
 * 0/0x40000, each with its own blocks (the first allocated at level init, the
 * others here). */
void pc_mapmaker_crowe_ai_start(void (*original)(void))
{
    uint32_t a,ent,rec,id;
    unsigned k;
    if(!MAPMAKER_STACK_OK(8) || !crw_play()) { original();return; }
    a=WORD_AT(g_esp+4u);
    if(!MAPMAKER_PTR_OK(a,CRW_ACTOR) || !MAPMAKER_PTR_OK(rec=WORD_AT(a+CRW_ACTOR_RECORD),12u)) { original();return; }
    ent=WORD_AT(a);
    if(CRW_H(a+CRW_ACTOR_CHAR)!=CRW_ID && !(MAPMAKER_PTR_OK(ent,0x100u) && CRW_H(ent+CRW_ENT_MODEL)==CRW_MODEL)) { original();return; }
    for(k=0;k<crw_n && crw_inst[k].record!=rec;k++) {}  /* the same record again */
    if(k==crw_n) for(k=0;k<crw_n && crw_inst[k].record;k++) {}   /* a slot allocated but unused */
    if(crw.optout) crw_log("TSFP_MAPMAKER_REAL_BOSS=0: split Crowe with bot behaviour",a,0);
    else if(!crw_pak_mounted()) crw_log("lv14.pak not mounted: split Crowe with bot behaviour",a,0);
    else if(!crw_n && !crw.restarted) crw_log("no split Crowe state this level: bot behaviour",a,WORD_AT(CRW_STATE));
    else if(k==crw_n && crw_n>=CRW_MAX) {
        if(crw.extras++<4u) crw_log("more than 4 split Crowes in the map: this one keeps the bot behaviour",a,WORD_AT(rec+8u));
    } else if(k==crw_n && !crw_alloc(k)) {
        if(crw.extras++<4u) crw_log("no memory for another split Crowe: this one keeps the bot behaviour",a,WORD_AT(rec+8u));
    } else {
        if(k==crw_n) crw_n++;
        id=WORD_AT(rec+8u);
        WORD_AT(rec)=0;WORD_AT(rec+4u)=CRW_CLASS;
        crw_inst[k].record=rec;crw_inst[k].actor=a;
        crw_inst[k].boss=pc_mapmaker_area_enabled("bosses","pc_mapmaker_bosses_hud")?crw_boss_flag(id-WORD_AT(CRW_SAI_BASE)-1u):0u;
        crw_log("split Crowe gets its story AI (class 0x40000; actor, boss)",a,k);
    }
    original();
}

/* ent is one of this level's bosses (in scene 100): its index, or -1. */
static int crw_boss(uint32_t ent)
{
    int k=crw_find(ent);
    return k>=0 && WORD_AT(CRW_SCENE)==100u && crw_on()?k:-1;
}

/* sub_002A9330 (grid, x, z, out): the row whose first cell holds z strictly
 * in (+4, +0xC), then that row's cell holding x strictly in (+0, +8); no
 * write when either is missing (sub_002A93D0 leaves the -1 of the build). */
static int crw_cell_at(uint32_t g,float x,float z,uint32_t *row,uint32_t *col)
{
    uint32_t r,c,cell;
    for(r=0;r<CRW_ROWS;r++) {
        cell=g+r*CRW_COLS*CRW_CELL;
        if(!(z>crw_getf(cell+4u) && crw_getf(cell+0xCu)>z)) continue;
        for(c=0;c<CRW_COLS;c++) {
            cell=g+(r*CRW_COLS+c)*CRW_CELL;
            if(x>crw_getf(cell) && crw_getf(cell+8u)>x) { *row=r;*col=c;return 1; }
        }
        return 0;
    }
    return 0;
}
/* The start cell: stock state 0 (0x00290DC2..0x00290EAF) sets the boss's x
 * to 310.0 (0x0049F3D8, ebp = entity+0x64 at 0x00290022), looks up the cell of
 * (that x, its own z) into state+0x1198/+0x119C (sub_002A9D00 -> sub_002A9330)
 * and puts the boss on that cell's centre. So the start is in the 310.0
 * column: take that column's cell (a type-0 platform first) nearest the
 * arena's centre in z whose centre the lookup maps back to it. */
static uint32_t crw_start_cell(uint32_t g,float oz,uint32_t *row,uint32_t *col)
{
    uint32_t r,c,best=0,br=0,bc=0,cell,rr,cc;
    float bd=-1.0f,d;
    int bt=0,t;
    for(r=0;r<CRW_ROWS;r++)
        for(c=0;c<CRW_COLS;c++) {
            cell=g+(r*CRW_COLS+c)*CRW_CELL;
            if(!(CRW_START_X>crw_getf(cell) && crw_getf(cell+8u)>CRW_START_X)) continue;
            if(!crw_cell_at(g,CRW_START_X,crw_getf(cell+0x14u),&rr,&cc) || rr!=r || cc!=c) continue;
            t=WORD_AT(cell+0x20u)==0;d=crw_getf(cell+0x14u)-oz;d*=d;
            if(!best || t>bt || (t==bt && d<bd)) { best=cell;br=r;bc=c;bt=t;bd=d; }
        }
    *row=br;*col=bc;
    return best;
}
/* The mission 10 lattice does not know the map (v0.4.0-mapmaker.34: walks
 * to cells 14-28 units away in walls or off the map). After the move, each
 * cell whose centre is in no MapMaker room at the boss's height (the room
 * query sub_00087310(box, out, max), tree 0x00702E00, as the roof area's fall
 * check asks it for a body box: x/z +-0.3, y +0.25..+1.5) is off: off the
 * map, a void, or a floor of another height. A start cell in no room means
 * the query cannot be trusted here: nothing is off. The box sits on the guest
 * stack below esp; the call's own pushes go below the box.
 *
 * v0.4.0-mapmaker.40 ("wandering off of ledges"): a cell over a lower floor
 * of the same room, or of a room below, is still in a room at the boss's
 * height. So every cell in a room also gets a floor probe at its centre, the
 * game's floor finder sub_000C3C50(point, 1.25, 0.1) (as the pad snap
 * 0x0009AFD4 asks it: the room it returns, > 0 when a floor is under the
 * point, the floor height in [0x004DCB00]): no floor, or one more than a step
 * (1.0) above or below the boss's floor, is off. The midpoints between
 * neighbouring cells are probed the same way (a gap between two good cells).
 * cells blocks those and what lies beyond them in line with the boss. A start
 * cell with no floor at the boss's height means the probe cannot be trusted
 * here: only the room results count. */
static int crw_floor_at(uint32_t pt,float x,float y,float z)
{
    float f;
    crw_setf(pt,x);crw_setf(pt+4u,y);crw_setf(pt+8u,z);
    if((int32_t)guest_call(sub_000C3C50,3,pt,CRW_PROBE_UP,CRW_PROBE_R)<=0) return 0;
    f=crw_getf(CRW_FLOOR_Y);
    return crw_sane(f) && fabsf(f-y)<=CRW_STEP;
}
static void crw_rooms(uint32_t st,float y)
{
    uint32_t sp=g_esp,q,pt,k,c,start=crw_lv.row*CRW_COLS+crw_lv.col,keep[3];
    unsigned n=0,drops=0,gaps=0,r,j;
    uint8_t drop[CRW_CELLS];
    memset(crw_lv.off,0,sizeof crw_lv.off);
    memset(crw_lv.gap_h,0,sizeof crw_lv.gap_h);memset(crw_lv.gap_v,0,sizeof crw_lv.gap_v);
    memset(drop,0,sizeof drop);
    if(!MAPMAKER_STACK_OK(0x100) || g_esp<0x00010200u || start>=CRW_CELLS) return;
    q=(g_esp-0x60u)&~0xFu;pt=q+0x40u;               /* box, 8 result words, the probe point */
    g_esp=q;
    for(k=0;k<CRW_CELLS;k++) {
        float x,z;
        c=st+CRW_GRID+k*CRW_CELL;
        x=crw_getf(c+0x10u);z=crw_getf(c+0x14u);
        crw_setf(q,x-CRW_BODY_HALF);crw_setf(q+4u,y+CRW_BODY_LOW);crw_setf(q+8u,z-CRW_BODY_HALF);
        crw_setf(q+0xCu,x+CRW_BODY_HALF);crw_setf(q+0x10u,y+CRW_BODY_HIGH);crw_setf(q+0x14u,z+CRW_BODY_HALF);
        crw_lv.off[k]=guest_call(sub_00087310,3,q,q+0x18u,CRW_QUERY_OUT)==0u;
    }
    if(crw_lv.off[start]) {
        g_esp=sp;
        memset(crw_lv.off,0,sizeof crw_lv.off);
        crw_log("split Crowe start cell is in no room: arena cells not checked (cell, state)",start,st);
        return;
    }
    keep[0]=WORD_AT(CRW_FLOOR_Y);keep[1]=WORD_AT(CRW_FLOOR_KEEP1);keep[2]=WORD_AT(CRW_FLOOR_KEEP2);
    c=st+CRW_GRID+start*CRW_CELL;
    if(crw_floor_at(pt,crw_getf(c+0x10u),y,crw_getf(c+0x14u))) {
        for(k=0;k<CRW_CELLS;k++) {
            c=st+CRW_GRID+k*CRW_CELL;
            if(!crw_lv.off[k] && !crw_floor_at(pt,crw_getf(c+0x10u),y,crw_getf(c+0x14u))) drop[k]=1u;
        }
        for(r=0;r<CRW_ROWS;r++)
            for(j=0;j<CRW_COLS;j++) {
                uint32_t a=st+CRW_GRID+(r*CRW_COLS+j)*CRW_CELL,b;
                k=r*CRW_COLS+j;
                if(crw_lv.off[k] || drop[k]) continue;
                if(j+1u<CRW_COLS && !crw_lv.off[k+1u] && !drop[k+1u]) {
                    b=a+CRW_CELL;
                    crw_lv.gap_h[r][j]=(uint8_t)!crw_floor_at(pt,(crw_getf(a+0x10u)+crw_getf(b+0x10u))*0.5f,y,
                                                              (crw_getf(a+0x14u)+crw_getf(b+0x14u))*0.5f);
                    gaps+=crw_lv.gap_h[r][j];
                }
                if(r+1u<CRW_ROWS && !crw_lv.off[k+CRW_COLS] && !drop[k+CRW_COLS]) {
                    b=a+CRW_COLS*CRW_CELL;
                    crw_lv.gap_v[r][j]=(uint8_t)!crw_floor_at(pt,(crw_getf(a+0x10u)+crw_getf(b+0x10u))*0.5f,y,
                                                              (crw_getf(a+0x14u)+crw_getf(b+0x14u))*0.5f);
                    gaps+=crw_lv.gap_v[r][j];
                }
            }
    } else crw_log("split Crowe start cell has no floor at its height: floor probe not used (cell, state)",start,st);
    WORD_AT(CRW_FLOOR_Y)=keep[0];WORD_AT(CRW_FLOOR_KEEP1)=keep[1];WORD_AT(CRW_FLOOR_KEEP2)=keep[2];
    g_esp=sp;
    for(k=0;k<CRW_CELLS;k++) { n+=crw_lv.off[k];drops+=drop[k];crw_lv.off[k]|=drop[k]; }
    fprintf(stderr,"[mapmaker] crowe: split Crowe %d arena: %u of %u cells in no room at its height, %u over a drop or ledge, %u gaps between cells (blocked)\n",
            (int)(crw_i-crw_inst),n,CRW_CELLS,drops,gaps);
    fflush(stderr);
}
/* Move the arena grid so the boss's start cell is centred on the spawn point
 * (x/z), note the floor offset, then find the cells off the map. */
static void crw_relocate(uint32_t ent)
{
    uint32_t st=crw_lv.state,g=st+CRW_GRID,o=st+CRW_OUTER,c,best=0,k,row=0,col=0;
    float sx=crw_getf(ent+CRW_ENT_X),sy=crw_getf(ent+CRW_ENT_X+4u),sz=crw_getf(ent+CRW_ENT_X+8u);
    float ox=crw_getf(o+0x10u),oz=crw_getf(o+0x14u),bd=-1.0f,d,dx,dz;
    static const uint32_t fx[3]={0u,8u,0x10u},fz[3]={4u,0xCu,0x14u};
    crw_lv.relocated=1;
    if(!crw_sane(sx) || !crw_sane(sy) || !crw_sane(sz) || !crw_sane(ox) || !crw_sane(oz)) {
        crw_log("split Crowe spawn or arena not a usable point; mission 10 coordinates kept",WORD_AT(ent+CRW_ENT_X),WORD_AT(o+0x10u));
        return;
    }
    if(!(best=crw_start_cell(g,oz,&row,&col))) {
        for(k=0;k<CRW_CELLS;k++) {                     /* no cell holds 310.0: the start is set after the call */
            c=g+k*CRW_CELL;
            if(WORD_AT(c+0x20u)!=0) continue;              /* type 0: a full platform */
            dx=crw_getf(c+0x10u)-ox;dz=crw_getf(c+0x14u)-oz;d=dx*dx+dz*dz;
            if(bd<0.0f || d<bd) { bd=d;best=c;row=k/CRW_COLS;col=k%CRW_COLS; }
        }
        crw_log("split Crowe: no arena cell holds the stock start x; start cell set after the first frame",st,row*CRW_COLS+col);
    }
    if(!best) { crw_log("split Crowe arena has no platform cell; mission 10 coordinates kept",st,0);return; }
    crw_lv.row=row;crw_lv.col=col;crw_lv.sx=sx;crw_lv.sz=sz;crw_lv.place=1;
    crw_lv.dx=sx-crw_getf(best+0x10u);crw_lv.dz=sz-crw_getf(best+0x14u);crw_lv.dy=sy-CRW_FLOOR;
    for(k=0;k<CRW_CELLS+1u+CRW_EDGES;k++) {
        unsigned j;
        c=k<CRW_CELLS?g+k*CRW_CELL:k==CRW_CELLS?o:st+CRW_EDGE+(k-CRW_CELLS-1u)*CRW_CELL;
        if(!WORD_AT(c) && !WORD_AT(c+8u)) continue;        /* an unused edge entry */
        for(j=0;j<3u;j++) {
            crw_setf(c+fx[j],crw_getf(c+fx[j])+crw_lv.dx);
            crw_setf(c+fz[j],crw_getf(c+fz[j])+crw_lv.dz);
        }
    }
    fprintf(stderr,"[mapmaker] crowe: arena moved by (%.2f, %.2f), floor %.2f (mission 10: 86), start cell %u,%u at the spawn (%.2f, %.2f)\n",
            (double)crw_lv.dx,(double)crw_lv.dz,(double)sy,(unsigned)row,(unsigned)col,(double)sx,(double)sz);
    fflush(stderr);
    crw_rooms(st,sy);
}
/* After the first state-0 frame: the boss stands at the spawn point on its
 * start cell. Stock already does so when the start cell holds 310.0; anything
 * else (no such cell: index -1 reads cell -8) is put right here. */
static void crw_place(uint32_t ent)
{
    uint32_t st=crw_lv.state,xb=WORD_AT(ent+CRW_ENT_X);
    float x=crw_getf(ent+CRW_ENT_X),z=crw_getf(ent+CRW_ENT_X+8u);
    crw_lv.place=0;
    if(WORD_AT(st+CRW_START_ROW)==crw_lv.row && WORD_AT(st+CRW_START_COL)==crw_lv.col &&
       fabsf(x-crw_lv.sx)<0.05f && fabsf(z-crw_lv.sz)<0.05f) return;
    WORD_AT(st+CRW_START_ROW)=crw_lv.row;WORD_AT(st+CRW_START_COL)=crw_lv.col;
    crw_setf(ent+CRW_ENT_X,crw_lv.sx);crw_setf(ent+CRW_ENT_X+8u,crw_lv.sz);
    crw_log("split Crowe start put back on the spawn point (start cell, stock x bits)",crw_lv.row*CRW_COLS+crw_lv.col,xb);
}
/* Shift (on) or restore (off) the arena constants. A constant that does not
 * hold its stock bits is left alone. */
static void crw_shift(int on)
{
    unsigned k;
    float off;
    if(on) {
        for(k=0;k<CRW_NCONST;k++) {
            crw_lv.saved[k]=WORD_AT(crw_consts[k].addr);
            if(crw_lv.saved[k]!=crw_consts[k].bits) continue;
            off=crw_consts[k].axis==0?crw_lv.dx:crw_consts[k].axis==1?crw_lv.dy:crw_lv.dz;
            crw_setf(crw_consts[k].addr,crw_getf(crw_consts[k].addr)+off);
        }
        crw_lv.shifted=1;
    } else if(crw_lv.shifted) {
        for(k=0;k<CRW_NCONST;k++) WORD_AT(crw_consts[k].addr)=crw_lv.saved[k];
        crw_lv.shifted=0;
    }
}

/* One line of the live boss's fight: AI state (from -> to), its cell, the
 * player's, the move target (cell, centre), direction, beam, eggs. */
static void crw_trace(const char *what,uint32_t ent,uint32_t from,uint32_t to)
{
    uint32_t st=crw_lv.state;
    unsigned k,held=0;
    for(k=0;k<CRW_EGGS;k++) held+=crw_lv.egg_left[k]!=0;
    fprintf(stderr,"[mapmaker] crowe: split Crowe %d %s %u -> %u: cell %d,%d, player cell %d,%d, target %d,%d at (%.2f, %.2f), "
            "direction %d, beam %u, at (%.2f, %.2f), eggs skipped %u (%u blocking)\n",
            (int)(crw_i-crw_inst),what,(unsigned)from,(unsigned)to,(int)WORD_AT(st+CRW_START_ROW),(int)WORD_AT(st+CRW_START_COL),
            (int)WORD_AT(st+CRW_PCELL),(int)WORD_AT(st+CRW_PCELL+4u),(int)WORD_AT(st+CRW_TARGET+8u),(int)WORD_AT(st+CRW_TARGET+0xCu),
            (double)crw_getf(st+CRW_TARGET),(double)crw_getf(st+CRW_TARGET+4u),(int)WORD_AT(st+CRW_DIR),(unsigned)WORD_AT(st+CRW_BEAM),
            (double)crw_getf(ent+CRW_ENT_X),(double)crw_getf(ent+CRW_ENT_X+8u),crw_lv.eggs,held);
    fflush(stderr);
}
/* As stock sub_002A9D00 does for an egg in flight (0x002A9D88..0x002A9DE5):
 * its cell, the cells above and below it and left and right of it. */
static void crw_block(uint32_t st,uint32_t r,uint32_t c)
{
    uint32_t i;
    for(i=r?r-1u:0u;i<=r+1u && i<CRW_ROWS;i++) WORD_AT(st+CRW_GRID+(i*CRW_COLS+c)*CRW_CELL+CRW_BLOCKED)=1u;
    for(i=c?c-1u:0u;i<=c+1u && i<CRW_COLS;i++) WORD_AT(st+CRW_GRID+(r*CRW_COLS+i)*CRW_CELL+CRW_BLOCKED)=1u;
}
/* State 9 waits, after the walk animation, until the boss's centre passes
 * its target (0x00291C1A..0x00291D8C); a walk stopped short (a MapMaker wall,
 * another boss) would wait for ever. Up to .34 the boss was then put on its
 * target cell's centre: inside walls or off the map (v0.4.0-mapmaker.34).
 * Now the walk ends where the boss stands, as stock ends it on arrival
 * (0x00291C4F..0x00291CA8: the walk-to-stand anim, the state the walk was
 * for, +0x2DF0 = 0); the position is the one the game's collision gave it.
 * The cells ahead of it in the walk's direction (+0x1D24: 0 row+, 1 col-,
 * 2 row-, 3 col+; sub_002A9E60) stay blocked for CRW_WALL_CALLS, so the next
 * choice is another way. */
static void crw_stop(uint32_t ent,uint32_t a,const char *why)
{
    uint32_t st=crw_lv.state,next=WORD_AT(st+CRW_NEXT),r=WORD_AT(st+CRW_START_ROW),c=WORD_AT(st+CRW_START_COL),dir=WORD_AT(st+CRW_DIR);
    int dr=dir==0u?1:dir==2u?-1:0,dc=dir==3u?1:dir==1u?-1:0,i,j;
    crw_lv.air=crw_lv.stall=0;
    if(r<CRW_ROWS && c<CRW_COLS && (dr || dc))
        for(i=(int)r+dr,j=(int)c+dc;i>=0 && i<(int)CRW_ROWS && j>=0 && j<(int)CRW_COLS;i+=dr,j+=dc)
            crw_lv.wall_left[(unsigned)i*CRW_COLS+(unsigned)j]=CRW_WALL_CALLS;
    if(next>=0x12u || next==CRW_JUMP) next=CRW_DECIDE;
    guest_call(sub_0005F460,4,ent,CRW_ANIM_STOP,0u,CRW_ANIM_SPEED);
    WORD_AT(a+0x9D4u)=CRW_JUMP;WORD_AT(a+CRW_AI_STATE)=next;WORD_AT(a+0x9D0u)=next;crw_setf(a+0x9D8u,0.0f);
    crw_setf(st+CRW_ARRIVED,0.0f);
    if(crw_lv.trace<CRW_TRACE_MAX) { crw_lv.trace++;crw_trace(why,ent,CRW_JUMP,next); }
}
/* One fight call in state 9: progress is the x+z distance to the target. */
static void crw_walking(uint32_t ent,uint32_t a)
{
    uint32_t st=crw_lv.state;
    float d=fabsf(crw_getf(ent+CRW_ENT_X)-crw_getf(st+CRW_TARGET))+fabsf(crw_getf(ent+CRW_ENT_X+8u)-crw_getf(st+CRW_TARGET+4u));
    if(!crw_lv.air++ || !(d>=crw_lv.best-CRW_STALL_STEP)) { crw_lv.best=d;crw_lv.stall=0; }
    else crw_lv.stall++;
    if(crw_lv.stall>=CRW_STALL_CALLS) crw_stop(ent,a,"walk blocked; ended where it stands, the cells ahead blocked: state");
    else if(crw_lv.air>=CRW_AIR_MAX) crw_stop(ent,a,"walk too long; ended where it stands, the cells ahead blocked: state");
}
/* The fight call for the boss whose globals are live. */
static void crw_fight(uint32_t ent,void (*original)(void))
{
    uint32_t a,player,from,to;
    unsigned k;
    int placing;
    a=WORD_AT(ent+CRW_ENT_AI);
    if(WORD_AT(a+CRW_AI_STATE)==0x12u) {                  /* defeated: stock waits for the level script */
        uint32_t parts;
        if(crw_lv.removed) { original();return; }
        /* the fight keeps updating its sparks, hazards and particles; no new
         * smoke (the parts mask off for this call): they play out, then the
         * boss goes */
        parts=WORD_AT(crw_lv.state+CRW_PARTS_ON);
        WORD_AT(crw_lv.state+CRW_PARTS_ON)=0;
        crw_shift(1);
        original();
        crw_shift(0);
        WORD_AT(crw_lv.state+CRW_PARTS_ON)=parts;
        if(!crw_lv.fading++) crw_log("split Crowe defeated: its effects play out",a,WORD_AT(CRW_LIVE));
        if(WORD_AT(CRW_LIVE) && crw_lv.fading<CRW_FADE_MAX) return;
        crw_lv.removed=1;
        player=WORD_AT(CRW_PLAYER);
        if(MAPMAKER_PTR_OK(player,CRW_FOCUS+4u) && WORD_AT(player+CRW_FOCUS)==ent) WORD_AT(player+CRW_FOCUS)=0;
        guest_call(sub_0015DDA0,1,ent);
        crw_log("split Crowe defeated: removed (frames, live particles)",crw_lv.fading,WORD_AT(CRW_LIVE));
        return;
    }
    if(WORD_AT(a+CRW_AI_STATE)==0u && !crw_lv.relocated) crw_relocate(ent);
    placing=crw_lv.place && WORD_AT(a+CRW_AI_STATE)==0u;
    if(WORD_AT(a+CRW_AI_STATE)==1u) {                     /* no intro cutscene in MapMaker */
        WORD_AT(a+0x9D4u)=1u;WORD_AT(a+CRW_AI_STATE)=2u;WORD_AT(a+0x9D0u)=2u;crw_setf(a+0x9D8u,0.0f);
        crw_log("split Crowe: no intro cutscene; the fight starts",a,0);
    }
    from=WORD_AT(a+CRW_AI_STATE);
    crw_shift(1);
    original();
    crw_shift(0);
    if(placing) crw_place(ent);
    for(k=0;k<CRW_EGGS;k++) if(crw_lv.egg_left[k]) crw_lv.egg_left[k]--;
    for(k=0;k<CRW_CELLS;k++) if(crw_lv.wall_left[k]) crw_lv.wall_left[k]--;
    to=WORD_AT(a+CRW_AI_STATE);
    if(to!=from && crw_lv.trace<CRW_TRACE_MAX) { crw_lv.trace++;crw_trace("fight state",ent,from,to); }
    if(to==CRW_JUMP) crw_walking(ent,a);
    else crw_lv.air=crw_lv.stall=0;
    if(!(++crw_lv.calls%CRW_BEAT) && crw_lv.beats<CRW_BEATS) { crw_lv.beats++;crw_trace("still: state",ent,to,to); }
}
/* WRAP sub_0028FE30 (cdecl entity; class 0x40000, per AI frame): with this
 * boss's globals live. */
void pc_mapmaker_crowe_fight(void (*original)(void))
{
    uint32_t ent;
    int k;
    CrwPrev prev;
    if(!crw_n || !MAPMAKER_STACK_OK(8) || (k=crw_boss(ent=WORD_AT(g_esp+4u)))<0) { original();return; }
    prev=crw_enter(k);
    crw_fight(ent,original);
    crw_leave(prev);
}
/* An entry into a boss's code with entity ent: its globals live during the
 * original. */
static void crw_entry(uint32_t ent,void (*original)(void))
{
    int k;
    CrwPrev prev;
    if((k=crw_boss(ent))<0) { original();return; }
    prev=crw_enter(k);
    original();
    crw_leave(prev);
}
/* WRAP sub_00292430 (cdecl entity, message, a, b; the boss's message handler,
 * from sub_0027CEF0 at 0x0027D3E9). */
void pc_mapmaker_crowe_message(void (*original)(void))
{
    if(!crw_n || !MAPMAKER_STACK_OK(8)) { original();return; }
    crw_entry(WORD_AT(g_esp+4u),original);
}
/* WRAP sub_0028DA10 (cdecl entity; slot 2, set by the fight's state 0 at
 * 0x00290EF7). */
void pc_mapmaker_crowe_think(void (*original)(void))
{
    if(!crw_n || !MAPMAKER_STACK_OK(8)) { original();return; }
    crw_entry(WORD_AT(g_esp+4u),original);
}
/* WRAP sub_0028F730 (cdecl hit record; +0 the entity hit, +4 the attacker;
 * the boss's hit callback, set at 0x00290EEC). */
void pc_mapmaker_crowe_hit(void (*original)(void))
{
    uint32_t info;
    if(!crw_n || !MAPMAKER_STACK_OK(8) || !MAPMAKER_PTR_OK(info=WORD_AT(g_esp+4u),0x30u)) { original();return; }
    crw_entry(WORD_AT(info),original);
}

/* WRAP sub_0028DD50 (cdecl entity): 86.0 is a distance threshold here. */
void pc_mapmaker_crowe_near(void (*original)(void))
{
    uint32_t v;
    if(!crw_lv.shifted || !MAPMAKER_STACK_OK(4) || WORD_AT(g_esp)!=CRW_RET_NEAR) { original();return; }
    v=WORD_AT(crw_consts[0].addr);
    WORD_AT(crw_consts[0].addr)=crw_lv.saved[0];
    original();
    WORD_AT(crw_consts[0].addr)=v;
}

/* WRAP sub_002A85C0 (cdecl floor height): the sparks bounce at 86.4. */
void pc_mapmaker_crowe_sparks(void (*original)(void))
{
    uint32_t v,slot;
    if(!crw_lv.shifted || !MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=CRW_RET_SPARKS) { original();return; }
    slot=g_esp+4u;v=WORD_AT(slot);
    crw_setf(slot,crw_getf(slot)+crw_lv.dy);
    original();
    WORD_AT(slot)=v;                                   /* the caller pops it; keep its stack as pushed */
}

/* WRAP sub_0028E770 (cdecl part, angle): a turret shot off. */
void pc_mapmaker_crowe_part(void (*original)(void))
{
    uint32_t part;
    if(!crw_lv.record || !MAPMAKER_STACK_OK(12) || WORD_AT(g_esp)!=CRW_RET_PART || !crw_state_ok() ||
       WORD_AT(CRW_SCENE)!=100u || !crw_on()) { original();return; }
    part=WORD_AT(g_esp+4u);
    if(part<CRW_PART_MAX)
        guest_call(sub_000EFBD0,8,0x20u,0x17u,crw_lv.state+CRW_SHOT+part*CRW_PART_STRIDE,0u,0u,0u,0u,1u);
    if(crw.parts++<4u) crw_log("split Crowe part destroyed: explosion only (its prop is in l_14_st.pak)",part,0);
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_002275F0 (cdecl marker): the death sets marker 0; MapMaker's own. */
void pc_mapmaker_crowe_event(void (*original)(void))
{
    if(!crw_lv.record || !MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=CRW_RET_EVENT || WORD_AT(CRW_SCENE)!=100u || !crw_on()) { original();return; }
    if(crw.events++<4u) crw_log("split Crowe level marker dropped (MapMaker logic marker untouched)",WORD_AT(g_esp+4u),0);
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_002AB1B0 (cdecl hazard, cell, 4 more; its only call 0x00291E7F in
 * the fight drops one of the eight falling eggs). Each egg is four props
 * 0x4BD (farfuture2/splittercrowegg, model only in story/l_14_st.pak, not
 * mounted in MapMaker): sub_000A6F90 returns 0 for each, the spawn stores the
 * 0 handles at hazard +0x150..+0x15C and calls on with them, and the landing
 * (sub_002AA6A0, 0x002AAEA2) releases all four with no null test:
 * sub_000A9070(0) -> sub_0009E330 reads class 0 at guest 0x20 and calls a
 * wild pointer, which the runtime skips with the stack 8 bytes too high
 * (RECOMP_ICALL_SAFE restores the esp from before its two arguments). Four
 * skips left the per-character update 0x20 off and crashed it
 * (v0.4.0-mapmaker.28, crash-20261102-024139391). For a boss in scene 100
 * the drop is skipped: no egg, no null prop, nothing to land.
 *
 * But the egg also steers the walk (v0.4.0-mapmaker.31: the Crowes fired and
 * never walked). State 5 asks sub_002A9E60 for a cell near the player in
 * line with the boss, taking the first one in a fixed order that is not a
 * gap (+0x20 = 1) or blocked (+0x24 = 1); -1 (no walk there) goes to state
 * 10, which drops the egg on that cell and returns to state 4. While the egg
 * is in flight, sub_002A9D00 blocks its cell and the four next to it on
 * every call, so the next state 5 picks another cell. With no egg the same
 * cell came back every time: 4, 5, 10, 4, ... and no walk until the player
 * moved. The skipped egg keeps its hazard slot (arguments 1, row 5, col 6)
 * for CRW_EGG_CALLS fight calls; cells blocks its cells like a live egg. */
void pc_mapmaker_crowe_eggs(void (*original)(void))
{
    uint32_t off,row,col,slot;
    if(!crw_lv.record || !MAPMAKER_STACK_OK(0x1C) || WORD_AT(g_esp)!=CRW_RET_EGG || WORD_AT(CRW_SCENE)!=100u || !crw_on()) { original();return; }
    off=WORD_AT(g_esp+4u)-(crw_lv.state+CRW_HAZARD);row=WORD_AT(g_esp+0x14u);col=WORD_AT(g_esp+0x18u);
    slot=off/CRW_HAZARD_SIZE;
    /* stock: a slot whose egg is still out drops nothing (0x002AB1DA) */
    if(!(off%CRW_HAZARD_SIZE) && slot<CRW_EGGS && !crw_lv.egg_left[slot] && row<CRW_ROWS && col<CRW_COLS) {
        crw_lv.egg_row[slot]=row;crw_lv.egg_col[slot]=col;crw_lv.egg_left[slot]=CRW_EGG_CALLS;
    }
    crw_lv.eggs++;
    if(crw.eggs++<4u) crw_log("split Crowe egg drop skipped (prop 0x4BD is only in l_14_st.pak; null props would be released on landing)",
                               WORD_AT(g_esp+4u),crw.eggs);
    MAPMAKER_GUEST_RET();
}
/* A gap in the floor between neighbouring cells (i0, j0) and (i, j). */
static int crw_gap(int i0,int j0,int i,int j)
{
    if(i0==i) return crw_lv.gap_h[i][j<j0?j:j0];
    return crw_lv.gap_v[i<i0?i:i0][j];
}
/* Cells in line with the boss (r, c) beyond a cell in no room (or over a
 * drop) or a gap between two cells: the walk would cross the void (or
 * another floor) to reach them. */
static void crw_shadow(uint32_t st,uint32_t r,uint32_t c)
{
    static const int step[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    unsigned d;
    for(d=0;d<4u;d++) {
        int i=(int)r+step[d][0],j=(int)c+step[d][1],hit=0;
        for(;i>=0 && i<(int)CRW_ROWS && j>=0 && j<(int)CRW_COLS;i+=step[d][0],j+=step[d][1]) {
            unsigned k=(unsigned)i*CRW_COLS+(unsigned)j;
            hit|=crw_lv.off[k] || crw_gap(i-step[d][0],j-step[d][1],i,j);
            if(hit) WORD_AT(st+CRW_GRID+k*CRW_CELL+CRW_BLOCKED)=1u;
        }
    }
}
/* The player outside the arena (v0.4.0-mapmaker.34: "player cell -1,-1" on
 * every line): sub_002A9330 writes no cell, so +0x1190 keeps the build's -1
 * (or the last cell the player was in), and state 5 picks among the 5 x 5
 * cells of rows 0-4, columns 0-4 (0x002A9E99..0x002A9EBF) whatever the
 * player does. The nearest cell in a room (any cell when none is) stands in
 * for the player, so the boss still turns and walks towards them. The arena
 * stays under the spawn: re-centring it on the player would carry the boss
 * across the map onto cells nobody checked. */
static void crw_player(uint32_t st,float px,float pz)
{
    uint32_t g=st+CRW_GRID,r,c,k,best=CRW_CELLS;
    float bd=0.0f,d,dx,dz;
    int open,bopen=0;
    if(!crw_sane(px) || !crw_sane(pz) || crw_cell_at(g,px,pz,&r,&c)) return;
    for(k=0;k<CRW_CELLS;k++) {
        dx=crw_getf(g+k*CRW_CELL+0x10u)-px;dz=crw_getf(g+k*CRW_CELL+0x14u)-pz;d=dx*dx+dz*dz;
        open=!crw_lv.off[k];
        if(best==CRW_CELLS || open>bopen || (open==bopen && d<bd)) { best=k;bd=d;bopen=open; }
    }
    if(best==CRW_CELLS) return;
    WORD_AT(st+CRW_PCELL)=best/CRW_COLS;WORD_AT(st+CRW_PCELL+4u)=best%CRW_COLS;
    if(!crw_lv.clamps++) crw_log("player outside a split Crowe's arena: its nearest cell stands in (boss, cell)",(uint32_t)(crw_i-crw_inst),best);
}
/* WRAP sub_002A9D00 (cdecl state+4, player x/z, boss x/z; the fight's cell
 * lookups, which also clear every cell's +0x24 and block the cells of the
 * eggs in flight): the live boss's skipped eggs block theirs too, and so do
 * its cells in no room, those beyond them in line with it and those ahead of
 * a stopped walk; a player outside the arena gets its nearest cell. */
void pc_mapmaker_crowe_cells(void (*original)(void))
{
    uint32_t st=crw_lv.state,r,c;
    unsigned k;
    float px,pz;
    if(!crw_lv.record || !st || !MAPMAKER_STACK_OK(0x14) || (WORD_AT(g_esp)!=CRW_RET_CELLS && WORD_AT(g_esp)!=CRW_RET_CELLS0) ||
       WORD_AT(g_esp+4u)!=st+4u) { original();return; }
    px=crw_getf(g_esp+8u);pz=crw_getf(g_esp+0xCu);
    original();
    for(k=0;k<CRW_EGGS;k++) if(crw_lv.egg_left[k]) crw_block(st,crw_lv.egg_row[k],crw_lv.egg_col[k]);
    for(k=0;k<CRW_CELLS;k++) if(crw_lv.off[k] || crw_lv.wall_left[k]) WORD_AT(st+CRW_GRID+k*CRW_CELL+CRW_BLOCKED)=1u;
    r=WORD_AT(st+CRW_START_ROW);c=WORD_AT(st+CRW_START_COL);
    if(r<CRW_ROWS && c<CRW_COLS) crw_shadow(st,r,c);
    crw_player(st,px,pz);
}

/* WRAP sub_00126560 (frame step before the scene-81 draw; the Crowe's own
 * draw sub_0028D710 runs only in scene 14). */
void pc_mapmaker_crowe_fx(void (*original)(void))
{
    int here=crw_n && MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==CRW_RET_FX;
    unsigned k;
    CrwPrev prev;
    original();
    if(!here || WORD_AT(CRW_SCENE)!=100u || !crw_on()) return;
    for(k=0;k<crw_n;k++) {                             /* each boss with its own blocks, until it is removed */
        if(!crw_inst[k].state || !crw_inst[k].record || crw_inst[k].removed) continue;
        prev=crw_enter((int)k);
        guest_call(sub_0028D710,0);
        crw_leave(prev);
        if(!crw.fx++) crw_log("split Crowe effects drawn (boss)",k,0);
    }
}

/* WRAP sub_0014CC80 (story boss bar; chain inside pc_mapmaker_bosses_hud):
 * never the stock bar (Boss Off = none, Boss bar = the bosses area's). */
void pc_mapmaker_crowe_hud(void (*original)(void))
{
    uint32_t player,focus;
    if(!crw_n) { original();return; }
    player=WORD_AT(CRW_PLAYER);
    if(!MAPMAKER_PTR_OK(player,CRW_FOCUS+4u) || !MAPMAKER_PTR_OK(focus=WORD_AT(player+CRW_FOCUS),0x100u) ||
       CRW_H(focus+CRW_ENT_MODEL)!=CRW_MODEL || crw_find(focus)<0 || !crw_play()) { original();return; }
    WORD_AT(player+CRW_FOCUS)=0;
    original();
    WORD_AT(player+CRW_FOCUS)=focus;
}

/* WRAP sub_0013A400 (cdecl actor, 0, 0x7F, 0x6E; overhead meter). */
void pc_mapmaker_crowe_meter(void (*original)(void))
{
    uint32_t a;
    int k;
    if(!crw_n || !MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=CRW_RET_METER) { original();return; }
    a=WORD_AT(g_esp+4u);
    if(!MAPMAKER_PTR_OK(a,CRW_ACTOR)) { original();return; }
    for(k=0;k<(int)crw_n && (!crw_inst[k].record || crw_inst[k].record!=WORD_AT(a+CRW_ACTOR_RECORD));k++) {}
    if(k==(int)crw_n || crw_inst[k].boss==CRW_BOSS_SUB || !crw_play()) { original();return; }
    if(!crw.meters++) crw_log("overhead health meter hidden on the split Crowe",a,0);
    MAPMAKER_GUEST_RET();
}
