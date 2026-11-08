/* US 45410066 MapMaker story creatures: one generic deployer for story-level
 * creatures as MapMaker Story AIs, driven by the rows of
 * pc_mapmaker_creatures.def (picker row, character id, look, AI class, anim
 * pak and keep-list, optional level hook). Area `creatures`; every hook is
 * inert unless TSFP_MAPMAKER_LIMITS=1, and only acts in MapMaker play (scene
 * 100, story mode byte [0x007DE455] == 10) for set members that are a row's
 * id. TSFP_MAPMAKER_CREATURES=0 keeps every creature on the MapMaker bot AI.
 * Docs: docs/mapmaker/mapmaker-creatures.md. Tests:
 * tests/mapmaker/test_mapmaker_creatures.py.
 *
 * Editor (scene 102): the rows are added to the custom-set picker by
 * pc_mapmaker_realbosses.inc (rbs_extra, from the same .def): label, pick
 * (id, variant -1), slot reload, Story AI name and preview. Save format
 * unchanged: a stock set member (word id, sbyte variant -1); a stock or older
 * build spawns the id with the MapMaker bot class on its own model.
 *
 * Play, per level (CONFIRMED in the decode, the test names the sites):
 *  - anims (sub_001C8540, the level's anim pak mount; sub_0005C420 calls it at
 *    0x0005C501 and registers every mounted anim pak's names with
 *    sub_001CA2F0 at 0x0005C558): after the original, for each row whose id
 *    the map's own set holds (slots 1-10 in the map-info chunk, 11-20 in the
 *    sets chunk), mount its pak whole with sub_0005A130 (= sub_000622D0(path,
 *    1, 1), the stock per-level anim pak call) unless mounted, while 8 of the
 *    64 mount slots stay free. The stock teardown unmounts it. Whole, not
 *    index-only: sub_00057F30 keeps a pointer into the anim file and fixes it
 *    up in place, so the file must stay in memory (as every boss pak). The
 *    anim slot budget (pc_mapmaker_animslots.inc) registers only the row's
 *    entries. Then the row's level hook, if any.
 *  - request (sub_0015E7C0, story level init): after the original, for a row
 *    with a look whose look id the set does not hold, request the look's
 *    models (character table +4/+8, below 0x9D0) with sub_000A5E40 for the
 *    level's load pass, as the set preload sub_000D7780 does for members.
 *  - create (sub_000DAC30, cdecl character id, ..., pool slot): for a row id
 *    with a look whose models are loaded or read (resource row and request
 *    data, as the khallos area checks), the character table's +4/+8 for the
 *    id hold the look's models for this call only, so the creature is made
 *    with them (skeleton from the model); else its own models.
 *  - ai_start (sub_0015B830(a); story mode copies spawn record +0/+4 to AI
 *    +0x10/+0x14 at 0x0015B9A3): before the original, a pool entry whose
 *    character a+0x48 is a row id, whose pak is mounted and every kept anim
 *    id resolves (id map 0x007E03C0, not 0xFFFF): record +0 = class_lo,
 *    +4 = class_hi (MapMaker wrote 0x11000000/0 at 0x002421A7). Otherwise the
 *    bot class stays and the reason is logged. Several per map: no per-AI
 *    state anywhere (the handlers keep theirs in the AI entry).
 * Restart (sub_001CDD60): nothing to do. The kept entries' slot words have
 * no 0x4000 flag, so the restart's sub_0005C2F0 keeps them mapped; the
 * record keeps its class for the restarted AI start.
 *
 * Requires the includer's WORD_AT, guest_call, g_esp, g_eax,
 * g_xbox_mem_offset and pc_mapmaker_limits.inc. Statics are prefixed crt/CRT_. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif

#define CRT_SCENE 0x0079094Cu
#define CRT_MODE 0x007DE455u            /* byte: 10 = story */
#define CRT_CONTAINER 0x00784014u
#define CRT_MAGIC 0x00C77667u
#define CRT_TAG_INFO 0x00C77668u        /* map info: +0x14 word set, +0x16 count, +0x18 members (4 B, word id) */
#define CRT_TAG_SETS 0x00C776F5u        /* sets area: +0 word 1, +2 10, +3 count, +4 members (slots 11-20) */
#define CRT_TAG_END 0x00C7766Au
#define CRT_SCAN 0x00080000u
#define CRT_SET_MAP 0x11u               /* the map's own set */
#define CRT_SLOTS 20u
#define CRT_INFO_SLOTS 10u
#define CRT_CHARS 0x0053A3CCu           /* character table, 0x30 per id: +4 model, +8 second model */
#define CRT_CHAR_STRIDE 0x30u
#define CRT_CHAR_MODEL 4u
#define CRT_NO_LOOK 0xFFu
#define CRT_MODEL_MAX 0x9D0u            /* sub_000A5E40 ignores ids >= this */
#define CRT_RESMAP 0x008CA000u          /* model -> resource row (moved there by the rooms area) */
#define CRT_RESMAP_MODELS 0xA00u
#define CRT_RES 0x00791D4Cu             /* resource table, 0x30 per entry */
#define CRT_RES_LOADED 0x4u
#define CRT_RES_PATH 0x10u
#define CRT_RES_LOD 0x14u
#define CRT_REQ_DATA 0x10u
#define CRT_MOUNTS 0x006D7880u          /* mounted paks */
#define CRT_MOUNT_MAX 64u               /* paktable.json: 64 slots at 0x00907000 */
#define CRT_MOUNT_KEEP 8u
#define CRT_ANIM_MAP 0x007E03C0u        /* int16 anim slot per id, 0xFFFF free */
#define CRT_ANIM_IDS 0xC03u
#define CRT_ENTRY 0x18u                 /* registration entry: +0x10 anim id */
#define CRT_ENTRY_ID 0x10u
#define CRT_ACTOR 0xEA0u
#define CRT_ACTOR_CHAR 0x48u
#define CRT_ACTOR_RECORD 0xBF0u
#define CRT_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define CRT_H(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

extern void sub_0005A130(void),sub_00062260(void),sub_000A5E40(void),sub_000631C0(void);

typedef struct {
    uint32_t row, id, look, preview;
    const char *label;
    uint32_t lo, hi, pak;
    const char *pak_name;
    uint32_t table, count, first, n;
    void (*level)(unsigned kind);
} CrtKind;
static const CrtKind crt_kinds[]={
#define MAPMAKER_CREATURE(row,id,look,preview,label,lo,hi,pak,pak_name,table,count,first,n,level) \
    {row,id,look,preview,label,lo,hi,pak,pak_name,table,count,first,n,level},
#include "pc_mapmaker_creatures.def"
#undef MAPMAKER_CREATURE
};
#define CRT_KINDS (sizeof crt_kinds/sizeof crt_kinds[0])

static struct {
    int read, optout;
    unsigned logged;
    unsigned started[CRT_KINDS], refused[CRT_KINDS], made[CRT_KINDS], own[CRT_KINDS];
} crt;

static void crt_log(const char *what,unsigned k,uint32_t a,uint32_t b)
{
    if(crt.logged>=32u) return;
    crt.logged++;
    fprintf(stderr,"[mapmaker] creatures: %s: %s (0x%X, 0x%X)\n",k<CRT_KINDS?crt_kinds[k].label:"",what,(unsigned)a,(unsigned)b);
    fflush(stderr);
}
static int crt_on(void)
{
    mapmaker_limits_read();
    if(!crt.read) {
        const char *v=getenv("TSFP_MAPMAKER_CREATURES");
        crt.read=1;
        crt.optout=v && v[0]=='0' && !v[1];
    }
    return mapmaker_limits.limits;
}
static int crt_play(void) { return crt_on() && WORD_AT(CRT_SCENE)==100u && CRT_B(CRT_MODE)==10u; }
static unsigned crt_kind_of(uint32_t id)
{
    unsigned k;
    for(k=0;k<CRT_KINDS && crt_kinds[k].id!=id;k++) {}
    return k;
}

/* The map's own set, slots 1-20 (0xFFFF empty). 1: read; 0: a preset or
 * profile set (no creature); -1: the container cannot be read. */
static int crt_set(uint16_t ids[CRT_SLOTS])
{
    uint32_t container=WORD_AT(CRT_CONTAINER),c,end,steps,info=0,isz=0,extra=0,n,k;
    for(k=0;k<CRT_SLOTS;k++) ids[k]=0xFFFFu;
    if(!container || !MAPMAKER_PTR_OK(container,CRT_SCAN) || WORD_AT(container)!=CRT_MAGIC) return -1;
    c=container+8u;end=container+CRT_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u),d=c+8u;
        if(tag==CRT_TAG_END || sz>end-c-8u) break;
        if(tag==CRT_TAG_INFO && sz>=0x18u && !info) { info=d;isz=sz; }
        else if(tag==CRT_TAG_SETS && sz>=0x30u && !extra && CRT_H(d)==1u && CRT_B(d+2u)==CRT_INFO_SLOTS &&
                CRT_B(d+3u)>=1u && CRT_B(d+3u)<=CRT_SLOTS-CRT_INFO_SLOTS) extra=d;
        c+=8u+sz;
    }
    if(!info) return -1;
    if(CRT_H(info+0x14u)!=CRT_SET_MAP) return 0;
    n=CRT_B(info+0x16u);
    if(n>CRT_INFO_SLOTS) n=CRT_INFO_SLOTS;
    if(0x18u+n*4u>isz) n=(isz-0x18u)/4u;
    for(k=0;k<n;k++) ids[k]=CRT_H(info+0x18u+k*4u);
    for(k=0;extra && k<CRT_B(extra+3u);k++) ids[CRT_INFO_SLOTS+k]=CRT_H(extra+4u+k*4u);
    return 1;
}
static int crt_has(const uint16_t ids[CRT_SLOTS],uint32_t id)
{
    unsigned k;
    for(k=0;k<CRT_SLOTS;k++) if(ids[k]==id) return 1;
    return 0;
}
static int crt_mounted(uint32_t path) { return (int32_t)guest_call(sub_00062260,1,path)!=-1; }
/* Kept anim ids of a row that resolve now (id map), *of: how many it keeps. */
static unsigned crt_anims(unsigned k,unsigned *of)
{
    const CrtKind *r=&crt_kinds[k];
    uint32_t i,id;
    unsigned ok=0;
    *of=r->n;
    for(i=0;i<r->n;i++) {
        uint32_t e=r->table+(r->first+i)*CRT_ENTRY;
        if(!MAPMAKER_PTR_OK(e,CRT_ENTRY)) continue;
        id=CRT_H(e+CRT_ENTRY_ID);
        if(id<CRT_ANIM_IDS && CRT_H(CRT_ANIM_MAP+id*2u)!=0xFFFFu) ok++;
    }
    return ok;
}
static uint32_t crt_model(uint32_t id,unsigned i) { return WORD_AT(CRT_CHARS+id*CRT_CHAR_STRIDE+CRT_CHAR_MODEL+i*4u); }
static uint32_t crt_resource(uint32_t model)
{
    int16_t i;
    uint32_t t=WORD_AT(CRT_RES),e;
    if(model>=CRT_RESMAP_MODELS) return 0;
    i=(int16_t)CRT_H(CRT_RESMAP+model*2u);
    if(i<0 || !t) return 0;
    e=t+(uint32_t)i*0x30u;
    return MAPMAKER_PTR_OK(e,0x30u)?e:0;
}
static int crt_path_ready(uint32_t path)
{
    uint32_t r;
    if(!path || !MAPMAKER_PTR_OK(path,4u)) return 0;
    r=guest_call(sub_000631C0,1,path);
    return r && MAPMAKER_PTR_OK(r,0x14u) && WORD_AT(r+CRT_REQ_DATA)!=0;
}
/* The look's models (+4, and +8 unless none) are loaded or read this level. */
static int crt_look_ready(uint32_t look)
{
    uint32_t m,e,path;
    unsigned i;
    for(i=0;i<2u;i++) {
        if((m=crt_model(look,i))==0xFFFFFFFFu && i) continue;
        if(m>=CRT_MODEL_MAX || !(e=crt_resource(m))) return 0;
        if(WORD_AT(e+CRT_RES_LOADED)) continue;
        if(!crt_path_ready(WORD_AT(e+CRT_RES_PATH)) || ((path=WORD_AT(e+CRT_RES_LOD))!=0 && !crt_path_ready(path))) return 0;
    }
    return 1;
}

/* ---- Prometheus robots: why a fire pose launches a rocket or not (log only) ----
 * CONFIRMED by decode (test_mapmaker_creatures.py, robot fire tests):
 *  - the robot handler sub_00283730 only chooses poses. State 0 sends a
 *    target nearer than 17 units (squared distance AI +0xAF4 < 289,
 *    0x0049F33C) to state 2 (fire still: SK/8 bigbotrocket 0x9A5) and any
 *    farther one to state 3, which advances with the advance-fire pose
 *    (0x9A7) at any distance (the .73 log's 126 / 48 unit poses); state 1
 *    picks a fire pose below 100 (10 units) or one time in three below 289;
 *  - the built-in weapon (+0x94 0x42/0x43, or +0x2BC) is fired by
 *    sub_002B8F90 (every AI each frame, from sub_001B8C40) while the current
 *    anim's slot word has 0x100000 (a fire pose), with the ammo state of
 *    sub_001B9340, sight sub_0015E120(ai, 0) (the pair table [0x0074C368],
 *    one byte per AI pair) and the pose's fire event 0x200 (sub_00061570);
 *    then sub_001BB5C0 (defined by the runtime, not wrapped), whose projectile
 *    path casts a 4-unit muzzle ray (sub_00088C60, return 0x001BC420);
 *  - the pair table is written by sub_0025FB70, one AI per frame in turn
 *    (index [0x007A3500]): a ray sub_00088C60 (return 0x00260134, flags
 *    0x2410270) from the AI's collision object toward the target's eye; the
 *    pair is visible when the ray's result is the target entity.
 * These hooks log per robot, once per outcome kind per level, each fire
 * pose's end (squared distance at its start, frames, frames without sight,
 * fire events 0x200 seen, muzzle tests and hits: a pose "launched" when it
 * had a muzzle test without a hit) and, once per robot, what the first
 * blocked sight rays hit. */
#define CRT_RB_MAX 16u
#define CRT_SLOT_WORD2 0x007DE664u      /* second slot word per anim slot: 0x100000 = a fire pose */
#define CRT_FIRE_POSE 0x100000u
#define CRT_FIRE_EVENT 0x200u           /* sub_002B8F90 at 0x002B97F1: sub_00061570(actor, 0x200) */
#define CRT_ANIM_CUR 0xD4u              /* animation controller (entity +4): current anim id */
#define CRT_AI_DIST2 0xAF4u             /* squared distance to the target */
#define CRT_AI_BUILTIN 0x94u
#define CRT_RET_MUZZLE 0x001BC420u      /* sub_001BB5C0: call sub_00088C60 at 0x001BC41B */
#define CRT_RET_SIGHT 0x00260134u       /* sub_0025FB70: call sub_00088C60 at 0x0026012F */
#define CRT_SIGHT_TURN 0x007A3500u      /* the AI whose sight is updated this frame */
#define CRT_POOL 0x007356D8u            /* -> AI pool, 0xEA0 per entry, +0 entity */
#define CRT_HUMANS 0x00790950u          /* the first pool slots are the players */
extern void sub_0015E120(void),sub_00061570(void);
static struct {
    uint32_t cur, seeing;                /* the AI inside sub_002B8F90 / sub_0025FB70 now (0 none) */
    struct { uint32_t ai; unsigned pose, frames, blind, events, tests, hits, launched, declined, logged;
             unsigned rays, clear, player, other, told; float dist2; } r[CRT_RB_MAX];
    unsigned n;
} crt_rb;
static unsigned crt_rb_of(uint32_t ai)
{
    unsigned k;
    for(k=0;k<crt_rb.n && crt_rb.r[k].ai!=ai;k++) {}
    if(k==crt_rb.n && k<CRT_RB_MAX) { memset(&crt_rb.r[k],0,sizeof crt_rb.r[k]);crt_rb.r[k].ai=ai;crt_rb.n++; }
    return k;
}
static int crt_rb_pose(uint32_t ent)
{
    uint32_t ctl,id,slot;
    if(!MAPMAKER_PTR_OK(ent,0x100u) || !MAPMAKER_PTR_OK(ctl=WORD_AT(ent+4u),CRT_ANIM_CUR+4u)) return 0;
    if((id=WORD_AT(ctl+CRT_ANIM_CUR))>=CRT_ANIM_IDS || (slot=CRT_H(CRT_ANIM_MAP+id*2u))>=940u) return 0;
    return (WORD_AT(CRT_SLOT_WORD2+slot*8u)&CRT_FIRE_POSE)!=0;
}
static float crt_getf(uint32_t a) { float f;memcpy(&f,(const void *)((uintptr_t)a+g_xbox_mem_offset),4);return f; }
/* A class-4 robot row's index for this AI, or CRT_KINDS. */
static unsigned crt_rb_kind(uint32_t ai)
{
    unsigned k;
    if(!MAPMAKER_PTR_OK(ai,CRT_ACTOR) || (k=crt_kind_of(CRT_H(ai+CRT_ACTOR_CHAR)))>=CRT_KINDS ||
       (crt_kinds[k].lo&0x1FFFu)==0u) return CRT_KINDS;
    return k;
}
static void crt_rb_end(unsigned i,unsigned k)
{
    int launched=crt_rb.r[i].tests>crt_rb.r[i].hits;
    unsigned *n=launched?&crt_rb.r[i].launched:&crt_rb.r[i].declined;
    if((*n)++>=2u || crt_rb.r[i].logged>=8u) return;
    crt_rb.r[i].logged++;
    fprintf(stderr,"[mapmaker] creatures: %s #%u: fire pose %s (squared distance %.1f at its start, %u frames, %u without sight, "
            "%u fire events, %u muzzle tests, %u muzzle hits, built-in weapon 0x%X)\n",crt_kinds[k].label,i+1u,
            launched?"launched a rocket":"ended without a rocket",(double)crt_rb.r[i].dist2,crt_rb.r[i].frames,
            crt_rb.r[i].blind,crt_rb.r[i].events,crt_rb.r[i].tests,crt_rb.r[i].hits,(unsigned)WORD_AT(crt_rb.r[i].ai+CRT_AI_BUILTIN));
    fflush(stderr);
}

/* WRAP sub_002B8F90 (cdecl actor: built-in weapon update, every AI, every frame). */
void pc_mapmaker_creatures_robot_weapon(void (*original)(void))
{
    uint32_t ent,ai;
    unsigned k,i;
    int pose;
    if(!MAPMAKER_STACK_OK(8) || !MAPMAKER_PTR_OK(ent=WORD_AT(g_esp+4u),0x100u) ||
       (k=crt_rb_kind(ai=WORD_AT(ent+0x7Cu)))>=CRT_KINDS || !crt_play() || crt.optout) { original();return; }
    if((i=crt_rb_of(ai))>=CRT_RB_MAX) { original();return; }
    pose=crt_rb_pose(ent);
    if(pose && !crt_rb.r[i].pose) {                    /* a fire pose starts */
        crt_rb.r[i].frames=crt_rb.r[i].blind=crt_rb.r[i].events=crt_rb.r[i].tests=crt_rb.r[i].hits=0;
        crt_rb.r[i].dist2=crt_getf(ai+CRT_AI_DIST2);
    }
    if(!pose && crt_rb.r[i].pose) crt_rb_end(i,k);
    crt_rb.r[i].pose=(unsigned)pose;
    if(pose) {
        crt_rb.r[i].frames++;
        if(!guest_call(sub_0015E120,2,ai,0u)) crt_rb.r[i].blind++;   /* the weapon code's own sight test (mode 0: no target change) */
        if(guest_call(sub_00061570,2,ent,CRT_FIRE_EVENT)) crt_rb.r[i].events++;   /* the pose's fire event, as 0x002B97F7 asks */
    }
    crt_rb.cur=ai;
    original();
    crt_rb.cur=0;
}
/* WRAP sub_0025FB70 (no args: the sight update of AI [0x007A3500], one per frame). */
void pc_mapmaker_creatures_robot_sight(void (*original)(void))
{
    uint32_t pool=WORD_AT(CRT_POOL),turn=WORD_AT(CRT_SIGHT_TURN);
    if(!crt_rb.n || turn>=0x100u || crt_rb_kind(pool+turn*CRT_ACTOR)>=CRT_KINDS || !crt_play()) { original();return; }
    crt_rb.seeing=pool+turn*CRT_ACTOR;
    original();
    crt_rb.seeing=0;
}
/* Is this entity a player's (the first pool slots)? */
static int crt_rb_player(uint32_t ent)
{
    uint32_t pool=WORD_AT(CRT_POOL),n=WORD_AT(CRT_HUMANS),k;
    for(k=0;k<n && k<8u;k++) if(ent && WORD_AT(pool+k*CRT_ACTOR)==ent) return 1;
    return 0;
}
/* WRAP sub_00088C60 (collision ray; hot: return-address tests first). The
 * robot's muzzle test in sub_001BB5C0 and its sight ray in sub_0025FB70. */
void pc_mapmaker_creatures_robot_muzzle(void (*original)(void))
{
    unsigned i;
    uint32_t ret=MAPMAKER_STACK_OK(4)?WORD_AT(g_esp):0u;
    if(ret==CRT_RET_MUZZLE && crt_rb.cur) {
        original();
        if((i=crt_rb_of(crt_rb.cur))<CRT_RB_MAX) { crt_rb.r[i].tests++;if(g_eax) crt_rb.r[i].hits++; }
        return;
    }
    if(ret!=CRT_RET_SIGHT || !crt_rb.seeing) { original();return; }
    original();
    if((i=crt_rb_of(crt_rb.seeing))>=CRT_RB_MAX) return;
    crt_rb.r[i].rays++;
    if(!g_eax) crt_rb.r[i].clear++;
    else if(crt_rb_player(g_eax)) crt_rb.r[i].player++;
    else crt_rb.r[i].other++;
    if(crt_rb.r[i].rays==64u && !crt_rb.r[i].told++) {
        unsigned k=crt_rb_kind(crt_rb.seeing);
        uint32_t ent=WORD_AT(crt_rb.seeing);
        fprintf(stderr,"[mapmaker] creatures: %s #%u: sight rays: %u reached a player, %u hit something else (last 0x%X, type %d), "
                "%u hit nothing; robot y %.2f, eye +0xBB8 %.2f, height +0x1AC %.2f\n",k<CRT_KINDS?crt_kinds[k].label:"",i+1u,
                crt_rb.r[i].player,crt_rb.r[i].other,(unsigned)g_eax,MAPMAKER_PTR_OK(g_eax,0x24u)?(int)WORD_AT(g_eax+0x20u):-1,
                crt_rb.r[i].clear,MAPMAKER_PTR_OK(ent,0x1B0u)?(double)crt_getf(ent+0x68u):0.0,
                (double)crt_getf(crt_rb.seeing+0xBB8u),MAPMAKER_PTR_OK(ent,0x1B0u)?(double)crt_getf(ent+0x1ACu):0.0);
        fflush(stderr);
    }
}

/* WRAP sub_001C8540 (the level's anim paks; sub_001CA2F0 registers the names next). */
void pc_mapmaker_creatures_anims(void (*original)(void))
{
    uint16_t ids[CRT_SLOTS];
    uint32_t used;
    unsigned k;
    original();
    if(!crt_play()) return;
    crt.logged=0;
    memset(&crt_rb,0,sizeof crt_rb);                   /* robot fire log: per level */
    memset(crt.started,0,sizeof crt.started);memset(crt.refused,0,sizeof crt.refused);memset(crt.made,0,sizeof crt.made);memset(crt.own,0,sizeof crt.own);
    if(crt.optout || crt_set(ids)!=1) return;
    for(k=0;k<CRT_KINDS;k++) {
        const CrtKind *r=&crt_kinds[k];
        if(!crt_has(ids,r->id)) continue;
        if(crt_mounted(r->pak)) crt_log("anim pak already mounted",k,r->pak,0);
        else if((used=WORD_AT(CRT_MOUNTS))+CRT_MOUNT_KEEP>CRT_MOUNT_MAX) { crt_log("pak mount table full; keeps the bot AI",k,used,CRT_MOUNT_MAX);continue; }
        else if(!guest_call(sub_0005A130,1,r->pak)) { crt_log("anim pak did not mount; keeps the bot AI",k,used,0);continue; }
        else {
            fprintf(stderr,"[mapmaker] creatures: %s: mounted %s.pak (anim entries %u..%u kept, mount slots used %u)\n",
                    r->label,r->pak_name,(unsigned)r->first,(unsigned)(r->first+r->n-1u),(unsigned)WORD_AT(CRT_MOUNTS));
            fflush(stderr);
        }
        if(r->level) r->level(k);
    }
}

/* WRAP sub_0015E7C0 (story level init, no args): request each look's models. */
void pc_mapmaker_creatures_request(void (*original)(void))
{
    uint16_t ids[CRT_SLOTS];
    uint32_t m;
    unsigned k,i;
    original();
    if(!crt_play() || crt.optout || crt_set(ids)!=1) return;
    for(k=0;k<CRT_KINDS;k++) {
        const CrtKind *r=&crt_kinds[k];
        if(r->look==CRT_NO_LOOK || !crt_has(ids,r->id) || crt_has(ids,r->look)) continue;   /* the set preload asks for a member's */
        for(i=0;i<2u;i++) if((m=crt_model(r->look,i))<CRT_MODEL_MAX) guest_call(sub_000A5E40,1,m);
        crt_log("requested the look's models",k,crt_model(r->look,0),crt_model(r->look,1));
    }
}

/* WRAP sub_000DAC30 (cdecl character id, ..., pool slot -> object or 0). */
void pc_mapmaker_creatures_create(void (*original)(void))
{
    uint32_t id,c,keep[2];
    unsigned k,i;
    if(!MAPMAKER_STACK_OK(8) || (k=crt_kind_of(id=WORD_AT(g_esp+4u)))>=CRT_KINDS || crt_kinds[k].look==CRT_NO_LOOK ||
       !crt_play() || crt.optout) { original();return; }
    if(!crt_look_ready(crt_kinds[k].look)) {
        if(!crt.own[k]++) crt_log("look's models not loaded: made with its own",k,id,crt_kinds[k].look);
        original();
        return;
    }
    c=CRT_CHARS+id*CRT_CHAR_STRIDE+CRT_CHAR_MODEL;
    for(i=0;i<2u;i++) { keep[i]=WORD_AT(c+i*4u);WORD_AT(c+i*4u)=crt_model(crt_kinds[k].look,i); }
    original();
    for(i=0;i<2u;i++) WORD_AT(c+i*4u)=keep[i];
    if(!crt.made[k]++) crt_log("made with the look's models (character, object)",k,crt_kinds[k].look,g_eax);
}

/* Story robots whose only weapon is built in: the AI start (0x0015B87C) takes
 * +0x94 from the record's weapon list +0x18 when it is not -1 (MapMaker
 * writes the Story AI's editor weapon there, 0x0024205A..0x00242155), else
 * the built-in one (0x62: rocket launcher 0x43 at 0x0015B927). Class 4's
 * robot path fires only 0x42/0x43 (sub_002B8F90, 0x002B9145..0x002B9153). */
#define CRT_REC_WEAPONS 0x18u
static int crt_builtin(uint32_t id) { return id==0x62u; }

/* WRAP sub_0015B830 (cdecl AI pool entry): give a creature its story class. */
void pc_mapmaker_creatures_ai_start(void (*original)(void))
{
    uint32_t a,rec;
    unsigned k,ok,of;
    if(!MAPMAKER_STACK_OK(8) || !crt_play()) { original();return; }
    a=WORD_AT(g_esp+4u);
    if(!MAPMAKER_PTR_OK(a,CRT_ACTOR) || (k=crt_kind_of(CRT_H(a+CRT_ACTOR_CHAR)))>=CRT_KINDS ||
       !MAPMAKER_PTR_OK(rec=WORD_AT(a+CRT_ACTOR_RECORD),12u)) { original();return; }
    if(crt.optout) { if(!crt.refused[k]++) crt_log("TSFP_MAPMAKER_CREATURES=0: keeps the bot AI",k,a,0); }
    else if(!crt_mounted(crt_kinds[k].pak)) { if(!crt.refused[k]++) crt_log("anim pak not mounted: keeps the bot AI",k,a,0); }
    else if((ok=crt_anims(k,&of))<of) { if(!crt.refused[k]++) crt_log("anims not registered (resolved, kept): keeps the bot AI",k,ok,of); }
    else if(!crt_kinds[k].lo && !crt_kinds[k].hi) { if(!crt.refused[k]++) crt_log("keeps the MapMaker bot AI (row without a story class)",k,a,0); }
    else {
        if(crt_builtin(crt_kinds[k].id) && CRT_H(rec+CRT_REC_WEAPONS)!=0xFFFFu) {
            if(!crt.made[k]++) crt_log("editor weapon ignored: the story robot fires only its built-in weapon (weapon list, record)",k,
                                       CRT_H(rec+CRT_REC_WEAPONS),rec);
            CRT_H(rec+CRT_REC_WEAPONS)=0xFFFFu;      /* the AI start then gives 0x62 its rocket launcher 0x43 (0x0015B927) */
        }
        WORD_AT(rec)=crt_kinds[k].lo;WORD_AT(rec+4u)=crt_kinds[k].hi;
        if(!crt.started[k]++) {
            fprintf(stderr,"[mapmaker] creatures: %s (character 0x%X) gets its story AI, class 0x%X:0x%08X, %u of %u anims from %s.pak\n",
                    crt_kinds[k].label,(unsigned)crt_kinds[k].id,(unsigned)crt_kinds[k].hi,(unsigned)crt_kinds[k].lo,ok,of,crt_kinds[k].pak_name);
            fflush(stderr);
        }
    }
    original();
}
