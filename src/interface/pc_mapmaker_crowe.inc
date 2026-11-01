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
 *    every move is to a cell centre. relocate: on its first frame the whole
 *    grid is moved in x/z so the type-0 cell nearest the arena's centre is
 *    under the Story AI's spawn point.
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
 *    0x12 (nothing, the level script ends mission 10): fight removes the AI
 *    with sub_0015DDA0 on its next frame. No sub_002274C0 and no tag lookup
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
 * Requires the includer's WORD_AT, guest_call, g_esp/g_eax, g_xbox_mem_offset,
 * pc_mapmaker_area_enabled and pc_mapmaker_limits.inc. Hot hooks (hud, meter,
 * fx: every frame) test a fixed value first. */
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
#define CRW_MOUNT_MAX 30u
#define CRW_MOUNT_KEEP 2u
#define CRW_STATE 0x0075F5E8u           /* 0x2E04 bytes, sub_0028D6D0 */
#define CRW_STATE_SIZE 0x2E04u
#define CRW_PARTS 0x0078B0D0u           /* 0x7D00 bytes, sub_002A62C0 */
#define CRW_PARTS_SIZE 0x7D00u
#define CRW_FX0 0x0078B090u             /* effect blocks, sub_002A8300 */
#define CRW_FX1 0x0078B098u
#define CRW_GRID 0x970u                 /* 35 cells of 0x28 */
#define CRW_CELLS 35u
#define CRW_CELL 0x28u
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
extern void sub_000EFBD0(void),sub_0015DDA0(void);
int pc_mapmaker_area_enabled(const char *area,const char *hook);

static struct {
    int read, optout;
    unsigned logged, meters, extras, events, parts, fx;
} crw;
/* This level's boss: state block, spawn record, pool entry, relocation. */
static struct {
    uint32_t state, record, actor;
    uint8_t boss;
    int relocated, removed, shifted;
    float dx, dy, dz;
    uint32_t saved[CRW_NCONST];
} crw_lv;

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
static int crw_state_ok(void) { return crw_lv.state && WORD_AT(CRW_STATE)==crw_lv.state; }

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
    uint32_t st,parts,old;
    original();
    memset(&crw_lv,0,sizeof crw_lv);                  /* a new level: nothing of the last one */
    if(!crw_play() || crw.optout || (has=crw_set_has())==0) return;
    WORD_AT(CRW_STATE)=0;                              /* a stale pointer of an earlier level */
    guest_call(sub_0028D6D0,0);
    st=WORD_AT(CRW_STATE);
    if(!st || !MAPMAKER_PTR_OK(st,CRW_STATE_SIZE) || !WORD_AT(CRW_FX0) || !WORD_AT(CRW_FX1)) {
        crw_log("no level memory for the split Crowe; bot behaviour",st,WORD_AT(CRW_FX0));
        WORD_AT(CRW_STATE)=0;
        return;
    }
    old=WORD_AT(CRW_PARTS);                            /* another boss's block of this level stays usable */
    WORD_AT(CRW_PARTS)=0;
    guest_call(sub_002A62C0,0);
    parts=WORD_AT(CRW_PARTS);
    if(!parts || !MAPMAKER_PTR_OK(parts,CRW_PARTS_SIZE)) {
        WORD_AT(CRW_PARTS)=old;WORD_AT(CRW_STATE)=0;
        crw_log("no level memory for the split Crowe's particles; bot behaviour",parts,old);
        return;
    }
    crw_lv.state=st;
    crw_log(has<0?"split Crowe state allocated (map container unreadable)":"split Crowe state allocated",st,parts);
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

/* WRAP sub_0015B830 (cdecl AI pool entry; story mode copies record +0/+4 to
 * AI +0x10/+0x14 at 0x0015B9A3): one split Crowe per map gets class 0/0x40000. */
void pc_mapmaker_crowe_ai_start(void (*original)(void))
{
    uint32_t a,ent,rec,id;
    if(!MAPMAKER_STACK_OK(8) || !crw_play()) { original();return; }
    a=WORD_AT(g_esp+4u);
    if(!MAPMAKER_PTR_OK(a,CRW_ACTOR) || !MAPMAKER_PTR_OK(rec=WORD_AT(a+CRW_ACTOR_RECORD),12u)) { original();return; }
    ent=WORD_AT(a);
    if(CRW_H(a+CRW_ACTOR_CHAR)!=CRW_ID && !(MAPMAKER_PTR_OK(ent,0x100u) && CRW_H(ent+CRW_ENT_MODEL)==CRW_MODEL)) { original();return; }
    if(crw.optout) crw_log("TSFP_MAPMAKER_REAL_BOSS=0: split Crowe with bot behaviour",a,0);
    else if(!crw_pak_mounted()) crw_log("lv14.pak not mounted: split Crowe with bot behaviour",a,0);
    else if(!crw_state_ok()) crw_log("no split Crowe state this level: bot behaviour",a,WORD_AT(CRW_STATE));
    else if(crw_lv.record && crw_lv.record!=rec) {
        if(crw.extras++<4u) crw_log("only one split Crowe per map: this one keeps the bot behaviour",a,WORD_AT(rec+8u));
    } else {
        id=WORD_AT(rec+8u);
        WORD_AT(rec)=0;WORD_AT(rec+4u)=CRW_CLASS;
        crw_lv.record=rec;crw_lv.actor=a;
        crw_lv.boss=pc_mapmaker_area_enabled("bosses","pc_mapmaker_bosses_hud")?crw_boss_flag(id-WORD_AT(CRW_SAI_BASE)-1u):0u;
        crw_log("split Crowe gets its story AI (class 0x40000)",a,id);
    }
    original();
}

/* This level's state is in place and ent is the boss. */
static int crw_boss(uint32_t ent)
{
    uint32_t a;
    if(!crw_lv.record || !crw_state_ok()) return 0;
    if(!MAPMAKER_PTR_OK(ent,0x100u) || !MAPMAKER_PTR_OK(a=WORD_AT(ent+CRW_ENT_AI),CRW_ACTOR)) return 0;
    return WORD_AT(a+CRW_ACTOR_RECORD)==crw_lv.record && WORD_AT(CRW_SCENE)==100u && crw_on();
}

/* Move the arena grid under the spawn point (x/z), note the floor offset. */
static void crw_relocate(uint32_t ent)
{
    uint32_t st=crw_lv.state,g=st+CRW_GRID,o=st+CRW_OUTER,c,best=0,k;
    float sx=crw_getf(ent+CRW_ENT_X),sy=crw_getf(ent+CRW_ENT_X+4u),sz=crw_getf(ent+CRW_ENT_X+8u);
    float ox=crw_getf(o+0x10u),oz=crw_getf(o+0x14u),bd=-1.0f,d,dx,dz;
    static const uint32_t fx[3]={0u,8u,0x10u},fz[3]={4u,0xCu,0x14u};
    crw_lv.relocated=1;
    if(!crw_sane(sx) || !crw_sane(sy) || !crw_sane(sz) || !crw_sane(ox) || !crw_sane(oz)) {
        crw_log("split Crowe spawn or arena not a usable point; mission 10 coordinates kept",WORD_AT(ent+CRW_ENT_X),WORD_AT(o+0x10u));
        return;
    }
    for(k=0;k<CRW_CELLS;k++) {
        c=g+k*CRW_CELL;
        if(WORD_AT(c+0x20u)!=0) continue;                  /* type 0: a full platform */
        dx=crw_getf(c+0x10u)-ox;dz=crw_getf(c+0x14u)-oz;d=dx*dx+dz*dz;
        if(bd<0.0f || d<bd) { bd=d;best=c; }
    }
    if(!best) { crw_log("split Crowe arena has no platform cell; mission 10 coordinates kept",st,0);return; }
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
    fprintf(stderr,"[mapmaker] crowe: arena moved by (%.2f, %.2f), floor %.2f (mission 10: 86)\n",
            (double)crw_lv.dx,(double)crw_lv.dz,(double)sy);
    fflush(stderr);
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

/* WRAP sub_0028FE30 (cdecl entity; class 0x40000, per AI frame). */
void pc_mapmaker_crowe_fight(void (*original)(void))
{
    uint32_t ent,a,player;
    if(!crw_lv.record || !MAPMAKER_STACK_OK(8) || !crw_boss(ent=WORD_AT(g_esp+4u))) { original();return; }
    a=WORD_AT(ent+CRW_ENT_AI);
    if(WORD_AT(a+CRW_AI_STATE)==0x12u) {                  /* defeated: stock waits for the level script */
        if(crw_lv.removed) { original();return; }
        crw_lv.removed=1;
        player=WORD_AT(CRW_PLAYER);
        if(MAPMAKER_PTR_OK(player,CRW_FOCUS+4u) && WORD_AT(player+CRW_FOCUS)==ent) WORD_AT(player+CRW_FOCUS)=0;
        guest_call(sub_0015DDA0,1,ent);
        crw_log("split Crowe defeated: removed",a,ent);
        MAPMAKER_GUEST_RET();
        return;
    }
    if(WORD_AT(a+CRW_AI_STATE)==0u && !crw_lv.relocated) crw_relocate(ent);
    if(WORD_AT(a+CRW_AI_STATE)==1u) {                     /* no intro cutscene in MapMaker */
        WORD_AT(a+0x9D4u)=1u;WORD_AT(a+CRW_AI_STATE)=2u;WORD_AT(a+0x9D0u)=2u;crw_setf(a+0x9D8u,0.0f);
        crw_log("split Crowe: no intro cutscene; the fight starts",a,0);
    }
    crw_shift(1);
    original();
    crw_shift(0);
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

/* WRAP sub_00126560 (frame step before the scene-81 draw; the Crowe's own
 * draw sub_0028D710 runs only in scene 14). */
void pc_mapmaker_crowe_fx(void (*original)(void))
{
    int here=crw_lv.state && MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==CRW_RET_FX;
    original();
    if(!here || !crw_state_ok() || WORD_AT(CRW_SCENE)!=100u || !crw_on()) return;
    guest_call(sub_0028D710,0);
    if(!crw.fx++) crw_log("split Crowe effects drawn",crw_lv.state,0);
}

/* WRAP sub_0014CC80 (story boss bar; chain inside pc_mapmaker_bosses_hud):
 * never the stock bar (Boss Off = none, Boss bar = the bosses area's). */
void pc_mapmaker_crowe_hud(void (*original)(void))
{
    uint32_t player,focus,a;
    if(!crw_lv.record) { original();return; }
    player=WORD_AT(CRW_PLAYER);
    if(!MAPMAKER_PTR_OK(player,CRW_FOCUS+4u) || !MAPMAKER_PTR_OK(focus=WORD_AT(player+CRW_FOCUS),0x100u) ||
       CRW_H(focus+CRW_ENT_MODEL)!=CRW_MODEL || !MAPMAKER_PTR_OK(a=WORD_AT(focus+CRW_ENT_AI),CRW_ACTOR) ||
       WORD_AT(a+CRW_ACTOR_RECORD)!=crw_lv.record || !crw_play()) { original();return; }
    WORD_AT(player+CRW_FOCUS)=0;
    original();
    WORD_AT(player+CRW_FOCUS)=focus;
}

/* WRAP sub_0013A400 (cdecl actor, 0, 0x7F, 0x6E; overhead meter). */
void pc_mapmaker_crowe_meter(void (*original)(void))
{
    uint32_t a;
    if(!crw_lv.record || !MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=CRW_RET_METER) { original();return; }
    a=WORD_AT(g_esp+4u);
    if(!MAPMAKER_PTR_OK(a,CRW_ACTOR) || WORD_AT(a+CRW_ACTOR_RECORD)!=crw_lv.record ||
       crw_lv.boss==CRW_BOSS_SUB || !crw_play()) { original();return; }
    if(!crw.meters++) crw_log("overhead health meter hidden on the split Crowe",a,0);
    MAPMAKER_GUEST_RET();
}
