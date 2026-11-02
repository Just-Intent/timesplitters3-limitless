/* US 45410066 MapMaker: the Timesplitter mothership boss of story mission 1
 * (intro, scene 2) in MapMaker story play (scene 100, story mode). Area
 * `mothership`; every hook is inert unless TSFP_MAPMAKER_LIMITS=1 (opt out of
 * the mothership alone with TSFP_MAPMAKER_MOTHERSHIP=0).
 * Docs: docs/mapmaker/mapmaker-mothership.md, survey
 * docs/mapmaker/mapmaker-boss-survey.md. CONFIRMED facts are checked by
 * tests/mapmaker/test_mapmaker_mothership.py.
 *
 * The ship is a prop, not a character: model 0x539
 * INTRO_TIMESPLITTERS_MOTHERSHIP (intro/intro_mothership, type 1, no
 * animation group, init index 20 -> sub_0021A710, case 0x0021AADA: health
 * 250, update sub_00217050 in slot 3, damage sub_002193A0, state +0x1A0 = 0)
 * and its wreck 0x53A intro_mscrash. Both are rows of the scene-100 resource
 * table, but their model files are only in story/l_2_st.pak, story/l_26_st.pak
 * and challenge/l_64_ch.pak. Every file l_64_ch.pak shares with any other pak
 * is byte-identical (checked against all paks), so mounting it kept open only
 * adds its own models. Its fight state lives on the object and in statics,
 * so this area keeps one copy of those statics per ship (up to MSH_MAX per
 * map) and swaps it in around every entry into the ship's code:
 *  - the dust height and firing box 0x0049EA84..0x0049EA97 (read only by
 *    the update; each ship's own values are rebased to its spawn, the stock
 *    constants are never changed), 0x0075C0DC..0x0075C0E7 (update timer,
 *    the intro HUD's shake value read by sub_00217020 in scenes 2/0x1A only,
 *    the engine sound level the init zeroes), the hit ring index 0x0075C0EC
 *    and its three 0x1C hit records 0x0078D500..0x0078D553 (damage writes,
 *    the update's sub_002157B0 decays, slot 10 sub_00215B50 draws) and the
 *    smoke effect handle 0x0078D360 (sub_00111AE0 creates it from the
 *    template 0x0078D040 when sub_0010E210 says it is gone);
 *  - entries: the update (slot 3), slot 10 sub_00215B50 (object at [esp+4]),
 *    the damage sub_002193A0 (shared by several props: record at [esp+4],
 *    object at +0) and the spawn itself (the init zeroes 0x0075C0E4).
 * After every entry the statics hold exactly what they held before.
 *
 * The fight (sub_00217050, states +0x1A0, table 0x00218630): 0 flies to the
 * intro's fixed point (-500, 50, -220); 1 hovers; 2 attacks: every frame
 * the focus of every player is the ship while its health is above 0, and on
 * a timer it fires a homing shot sub_000EA9C0(0x32, ..) at a random player
 * standing inside the box x -67..-56, z -88.04..-47.8 (constants
 * 0x0049EA90/0x0049EA94/0x0049EA88/0x0049EA8C, each read only there); 3 falls
 * (no lift) until its height is below 6.6 (0x0049E324, shared with other
 * code); 4 crash explosions, dust at height -45.5 (0x0049EA84, read only
 * there); 5 spawns the wreck 0x53A and removes the ship. The intro's level
 * script starts the attack with logic action 0x5B sub_00217030, which only
 * sets +0x1A0 = 2 on the 'MTHS' object. In states other than 0 the lift
 * balances gravity and the velocity is damped, so the ship hovers where it
 * is. Death: the damage handler clears the focus and calls
 * sub_00234CD0('MTHS') (null-safe); state 5 moves the focus to 'ZCTP' only
 * when that actor exists. No level event is reachable (test).
 *
 * Placement (no new save bytes): a Story AI whose character is the
 * placeholder id MSH_ID 0x75 (CHR_FARFUTURE_GUARDROBOT, story-only, no Arcade
 * row, in no story set, model in chr.pak). The map stores it as a stock
 * custom-set member; a stock or older build plays it as a guard robot. The
 * editor picker row is the integrator's (pc_mapmaker_realbosses.inc
 * rbs_extra; see the doc).
 *
 * Play, per level:
 *  - mount (sub_00062790): after the original, forget the previous level's
 *    ships; when the
 *    map's own custom set holds MSH_ID (or the container cannot be read),
 *    mount pak/challenge/l_64_ch.pak with sub_000622D0(path, 0, 1): kept
 *    open, entries read on demand, as chr.pak.
 *  - request (sub_0015E7C0): request the ship and wreck models
 *    (sub_000A5E40) so the level's load pass reads them.
 *  - ai_start (sub_0015B830): after the original, for each pool entry
 *    whose character is MSH_ID (up to MSH_MAX), when both models are ready: create the ship
 *    MSH_HOVER metres above the placeholder, facing its yaw
 *    (sub_000A6F90(0x539, &pos, &rot, 0, 0); the init runs inside), then
 *    state 2 as sub_00217030 does, and rebase its firing box to MSH_RANGE
 *    metres around the spawn (x/z) and its dust height to the placeholder's
 *    floor, in its own copy. A placeholder past MSH_MAX keeps its bot behaviour.
 *  - textures (v0.4.0-mapmaker.25: a grey, untextured ship): the level's
 *    load pass (sub_00063390) registers a read model's textures
 *    (sub_00096220 at 0x0006355D) only when its texture block's +0x48 is
 *    0x10; otherwise it marks the request (+2 |= 0x10, 0x00063548) and the
 *    model's creation registers them, sub_00096330(file, 1) at 0x0009658F,
 *    but never in scene 100 (0x00096556). The intro ship comes from a
 *    level pak and is such a model. Before the first ship is created, each
 *    of the ship and wreck models whose request carries that mark gets the
 *    same call (once per level, logged), and whether the hit glow effect
 *    texture 0x3BC (slot 10, sub_00080F20) is loaded is logged.
 *  - update (sub_00217050, the ship's per-frame callback): on its first
 *    frame the placeholder AI is removed (sub_0015DDA0). While falling or
 *    crashing (states 3 and 4 at entry) the ship's height is moved down by
 *    the floor for the call and back after, so the fall ends 6.6 m above
 *    the map floor instead of at the intro's absolute 6.6.
 *  - text (sub_0003D7B0): the bar label 0x164B (pushed by the 0x539 case of
 *    sub_0014CC80, call 0x0014D09A, return 0x0014D09F) is a level-local
 *    text id; in scene 100 the focus ship's Story AI name is returned there
 *    ("Timesplitter Mothership" when unnamed). The bosses area lists each
 *    ship (bos_bar_extra_add) for Boss-flagged bars, as the tank area does.
 *  - ai_start "started again": a ship whose placeholder is still this
 *    entity, or made from this spawn record and id (a removed placeholder's
 *    entity is reused); every refusal logs (msh_refuse, 16 per level).
 *
 * Requires the includer's WORD_AT, guest_call, g_esp/g_eax,
 * g_xbox_mem_offset, xbox_HeapAlloc and pc_mapmaker_limits.inc
 * (mapmaker_limits, MAPMAKER_GUEST_RET, MAPMAKER_PTR_OK). Hot hooks: text
 * (every text lookup) and update (every frame of every ship) test a fixed
 * value first. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif

#define MSH_SCENE 0x0079094Cu
#define MSH_MODE 0x007DE455u             /* byte: 10 = story */
#define MSH_ID 0x75u                     /* placeholder character: CHR_FARFUTURE_GUARDROBOT */
#define MSH_MODEL 0x539u                 /* INTRO_TIMESPLITTERS_MOTHERSHIP */
#define MSH_WRECK 0x53Au                 /* intro_mscrash, made in state 5 */
#define MSH_INIT 0x0021A710u             /* resource init index 20 */
#define MSH_UPDATE 0x00217050u           /* slot-3 callback the init installs */
#define MSH_RESMAP 0x008CA000u           /* int16 per model (moved by the rooms area) */
#define MSH_RESMAP_MODELS 0xA00u
#define MSH_RES 0x00791D4Cu              /* resource table, 0x30 per entry */
#define MSH_RES_LOADED 0x4u
#define MSH_RES_PATH 0x10u
#define MSH_RES_LOD 0x14u
#define MSH_RES_INIT 0x2Cu
#define MSH_REQ_DATA 0x10u               /* request (sub_000631C0): loaded file or 0 */
#define MSH_REQ_FLAGS 2u                 /* request byte: 0x10 = textures left to the model's creation */
#define MSH_REQ_DEFERRED 0x10u
#define MSH_FX 0x004D2D50u               /* effect textures by id, 0x18 each, the loaded one at +0x14 */
#define MSH_FX_HIT 0x3BCu                /* slot 10's hit glow (0x00215B5F) */
#define MSH_PAK "pak/challenge/l_64_ch.pak"
#define MSH_MOUNTS 0x006D7880u           /* mounted paks */
#define MSH_MOUNT_MAX 64u               /* paktable.json: 64 slots */
#define MSH_MOUNT_KEEP 8u
#define MSH_MOUNT_SKIP 0x006D7C4Cu       /* sub_00062790 does nothing when set */
#define MSH_CONTAINER 0x00784014u
#define MSH_MAGIC 0x00C77667u
#define MSH_TAG_INFO 0x00C77668u
#define MSH_TAG_END 0x00C7766Au
#define MSH_TAG_SETS 0x00C776F5u         /* pc_mapmaker_sets.inc: slots 11-20 */
#define MSH_SCAN 0x00080000u
#define MSH_INFO_SET 0x14u
#define MSH_INFO_COUNT 0x16u
#define MSH_INFO_MEMBERS 0x18u
#define MSH_INFO_SLOTS 10u
#define MSH_SET_MAX 20u
#define MSH_SET_MAP 0x11u
#define MSH_SETS_SIZE 0x30u
#define MSH_ACTOR 0xEA0u
#define MSH_ACTOR_CHAR 0x48u
#define MSH_ACTOR_RECORD 0xBF0u
#define MSH_ENT_AI 0x7Cu
#define MSH_ENT_POS 0x64u                /* x, y (up), z */
#define MSH_ENT_YAW 0x70u
#define MSH_ENT_MODEL 0x94u
#define MSH_STATE 0x1A0u                 /* ship: fight state 0..5 */
#define MSH_ATTACK 2u                    /* as sub_00217030 (logic action 0x5B) */
#define MSH_FALL 3u
#define MSH_CRASH 4u
#define MSH_PLAYER 0x007B0C7Cu
#define MSH_FOCUS 0xFF4u
#define MSH_HOVER 30.0f                  /* metres above the placeholder (INFERRED) */
#define MSH_RANGE 60.0f                  /* firing box half-width around the spawn (x/z) */
#define MSH_BOX 0x0049EA88u              /* z low, z high, x low, x high */
#define MSH_DUST 0x0049EA84u             /* crash dust height */
#define MSH_MAX 4u                       /* motherships per map */
#define MSH_WORDS 31u                    /* the statics kept per ship (msh_spans) */
#define MSH_BAR_TEXT 0x164Bu
#define MSH_RET_BAR_TEXT 0x0014D09Fu     /* sub_0014CC80: call sub_0003D7B0 at 0x0014D09A */
#define MSH_LABEL "Timesplitter Mothership"
#define MSH_NAME 32u                     /* a Story AI's saved name (pc_mapmaker_bosses.inc BOS_NAME) */
#define MSH_OBJ_MODEL 0x94u              /* prop: model word (the stock bar's switch, 0x0014CCD6) */
#define MSH_REFUSE_LOGS 16u              /* "stays a bot" / "started again" lines per level */
#define MSH_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define MSH_H(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

static const uint32_t msh_models[2]={MSH_MODEL,MSH_WRECK};
/* The statics each ship keeps its own copy of: start, words. Dust and box first. */
static const uint32_t msh_spans[5][2]={
    {MSH_DUST,5u},                       /* dust height, box z low/high, x low/high */
    {0x0075C0DCu,3u},                    /* update timer, intro shake, engine sound level */
    {0x0075C0ECu,1u},                    /* hit ring index */
    {0x0078D360u,1u},                    /* smoke effect handle */
    {0x0078D500u,21u},                   /* three hit records of 0x1C */
};

extern void sub_00062260(void),sub_000622D0(void),sub_000A6F90(void),sub_0015DDA0(void);
extern void sub_000A5E40(void),sub_000631C0(void),sub_00096330(void);
extern uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment);

/* Kept for the session: settings, strings, spawn arguments, stock constants. */
static struct { int read, optout; uint32_t label, pak, args, names; unsigned logged; } msh;
/* One ship: its object and placeholder (place_id: its spawn record's id,
 * record +8), floor, and its copy of the statics. */
struct msh_one {
    uint32_t obj, place, place_ent, place_rec, place_id;
    float floor;
    unsigned shifts;
    uint32_t st[MSH_WORDS];
};
/* This level's ships (cleared by every level's pak mount). */
static struct {
    struct msh_one s[MSH_MAX];
    unsigned n;                          /* ships created */
    unsigned act;                        /* 1 + the ship whose statics are in place; 0: none */
    uint32_t rest[MSH_WORDS];            /* the statics outside any ship */
    int requested, textured;
    unsigned refused;
} msh_lv;
#define MS (msh_lv.s[msh_lv.act-1u])     /* the active ship (only with msh_lv.act) */

static void msh_log(const char *what,uint32_t a,uint32_t b)
{
    if(msh.logged>=24u) return;
    msh.logged++;
    fprintf(stderr,"[mapmaker] mothership: %s (0x%X, 0x%X)\n",what,(unsigned)a,(unsigned)b);
    fflush(stderr);
}
/* Why a placeholder made no ship: a budget per level, so every refusal shows. */
static void msh_refuse(const char *what,uint32_t a,uint32_t b)
{
    if(msh_lv.refused>=MSH_REFUSE_LOGS) return;
    msh_lv.refused++;
    fprintf(stderr,"[mapmaker] mothership: %s (0x%X, 0x%X)\n",what,(unsigned)a,(unsigned)b);
    fflush(stderr);
}
static int msh_on(void)
{
    mapmaker_limits_read();
    if(!msh.read) {
        const char *v=getenv("TSFP_MAPMAKER_MOTHERSHIP");
        msh.read=1;
        msh.optout=v && v[0]=='0' && !v[1];
    }
    return mapmaker_limits.limits;
}
static int msh_play(void) { return msh_on() && WORD_AT(MSH_SCENE)==100u && MSH_B(MSH_MODE)==10u; }
/* 1 + the index of this level's ship obj, in scene 100; 0 for any other object. */
static unsigned msh_find(uint32_t obj)
{
    unsigned k;
    if(!obj || WORD_AT(MSH_SCENE)!=100u || !msh_on()) return 0;
    for(k=0;k<msh_lv.n && k<MSH_MAX;k++) if(msh_lv.s[k].obj==obj) return k+1u;
    return 0;
}
static void msh_save(uint32_t *w)
{
    uint32_t k,i,n=0;
    for(k=0;k<5u;k++) for(i=0;i<msh_spans[k][1];i++) w[n++]=WORD_AT(msh_spans[k][0]+i*4u);
}
static void msh_load(const uint32_t *w)
{
    uint32_t k,i,n=0;
    for(k=0;k<5u;k++) for(i=0;i<msh_spans[k][1];i++) WORD_AT(msh_spans[k][0]+i*4u)=w[n++];
}
/* Puts ship k (1-based) in the statics; returns what was active before. */
static unsigned msh_enter(unsigned k)
{
    unsigned p=msh_lv.act;
    if(p==k) return p;
    msh_save(p?msh_lv.s[p-1u].st:msh_lv.rest);
    msh_load(msh_lv.s[k-1u].st);
    msh_lv.act=k;
    return p;
}
/* Takes ship k out of the statics and puts back what msh_enter found. */
static void msh_leave(unsigned k,unsigned p)
{
    if(p==k) return;
    msh_save(msh_lv.s[k-1u].st);
    msh_load(p?msh_lv.s[p-1u].st:msh_lv.rest);
    msh_lv.act=p;
}
/* The original with obj's statics in place, when obj is one of ours. */
static void msh_run(void (*original)(void),uint32_t obj)
{
    unsigned k=msh_find(obj),p;
    if(!k) { original();return; }
    p=msh_enter(k);
    original();
    msh_leave(k,p);
}

static float msh_getf(uint32_t a) { float f;memcpy(&f,(const void *)((uintptr_t)a+g_xbox_mem_offset),4);return f; }
static void msh_setf(uint32_t a,float f) { memcpy((void *)((uintptr_t)a+g_xbox_mem_offset),&f,4); }
static int msh_sane(float f) { return f==f && f>-100000.0f && f<100000.0f; }

static uint32_t msh_string(uint32_t *slot,const char *text,size_t n)
{
    if(!*slot) {
        uint32_t p=xbox_HeapAlloc((uint32_t)n,4u);
        if(p && MAPMAKER_PTR_OK(p,n)) {
            memcpy((void *)((uintptr_t)p+g_xbox_mem_offset),text,n);
            *slot=p;
        } else msh_log("no memory for a string",(uint32_t)n,0);
    }
    return *slot;
}

/* The map's own custom set (slots 1-20), as pc_mapmaker_realbosses.inc reads it.
 * 1: holds MSH_ID; 0: does not (or a preset/profile set); -1: unreadable. */
static int msh_map_has(void)
{
    uint32_t container=WORD_AT(MSH_CONTAINER),c,end,steps,info=0,isz=0,extra=0,n,k;
    if(!container || !MAPMAKER_PTR_OK(container,MSH_SCAN) || WORD_AT(container)!=MSH_MAGIC) return -1;
    c=container+8u;end=container+MSH_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u),d=c+8u;
        if(tag==MSH_TAG_END || sz>end-c-8u) break;
        if(tag==MSH_TAG_INFO && sz>=MSH_INFO_MEMBERS && !info) { info=d;isz=sz; }
        else if(tag==MSH_TAG_SETS && sz>=MSH_SETS_SIZE && !extra && MSH_H(d)==1u && MSH_B(d+2u)==MSH_INFO_SLOTS &&
                MSH_B(d+3u)>=1u && MSH_B(d+3u)<=MSH_SET_MAX-MSH_INFO_SLOTS) extra=d;
        c+=8u+sz;
    }
    if(!info) return -1;
    if(MSH_H(info+MSH_INFO_SET)!=MSH_SET_MAP) return 0;
    n=MSH_B(info+MSH_INFO_COUNT);
    if(n>MSH_INFO_SLOTS) n=MSH_INFO_SLOTS;
    if(MSH_INFO_MEMBERS+n*4u>isz) n=(isz-MSH_INFO_MEMBERS)/4u;
    for(k=0;k<n;k++) if(MSH_H(info+MSH_INFO_MEMBERS+k*4u)==MSH_ID) return 1;
    for(k=0;extra && k<MSH_B(extra+3u);k++) if(MSH_H(extra+4u+k*4u)==MSH_ID) return 1;
    return 0;
}
static int msh_pak_mounted(void)
{
    return msh.pak && (int32_t)guest_call(sub_00062260,1,msh.pak)!=-1;
}
/* The resource row of a model, or 0 when scene 100's table lacks it. */
static uint32_t msh_resource(uint32_t model)
{
    int16_t i;
    uint32_t t=WORD_AT(MSH_RES),e;
    if(model>=MSH_RESMAP_MODELS) return 0;
    i=(int16_t)MSH_H(MSH_RESMAP+model*2u);
    if(i<0 || !t) return 0;
    e=t+(uint32_t)i*0x30u;
    return MAPMAKER_PTR_OK(e,0x30u)?e:0;
}
/* A model path the level's load pass has read (as the tank area checks it). */
static int msh_path_ready(uint32_t path)
{
    uint32_t r;
    if(!path || !MAPMAKER_PTR_OK(path,4u)) return 0;
    r=guest_call(sub_000631C0,1,path);
    return r && MAPMAKER_PTR_OK(r,0x14u) && WORD_AT(r+MSH_REQ_DATA)!=0;
}
/* 0 when the ship and its wreck can both be created, else the model that cannot. */
static uint32_t msh_models_unready(void)
{
    uint32_t k,e,path;
    for(k=0;k<2u;k++) {
        if(!(e=msh_resource(msh_models[k]))) return msh_models[k];
        if(WORD_AT(e+MSH_RES_LOADED)) continue;
        if(!msh_path_ready(WORD_AT(e+MSH_RES_PATH))) return msh_models[k];
        if((path=WORD_AT(e+MSH_RES_LOD))!=0 && !msh_path_ready(path)) return msh_models[k];
    }
    return 0;
}

/* The ship and wreck models' textures, which scene 100 never registers when
 * the load pass left them to the model's creation (once per level). */
static void msh_textures(void)
{
    uint32_t k,e,path,req,file;
    if(msh_lv.textured) return;
    msh_lv.textured=1;
    for(k=0;k<2u;k++) {
        if(!(e=msh_resource(msh_models[k])) || !(path=WORD_AT(e+MSH_RES_PATH)) || !MAPMAKER_PTR_OK(path,4u)) continue;
        req=guest_call(sub_000631C0,1,path);
        if(!req || !MAPMAKER_PTR_OK(req,0x14u) || !(file=WORD_AT(req+MSH_REQ_DATA)) || !MAPMAKER_PTR_OK(file,0x10u)) {
            msh_log("model file not read: its textures not registered",msh_models[k],req);
            continue;
        }
        if(!(MSH_B(req+MSH_REQ_FLAGS)&MSH_REQ_DEFERRED)) {
            msh_log("model textures registered by the level's load pass",msh_models[k],MSH_B(req+MSH_REQ_FLAGS));
            continue;
        }
        guest_call(sub_00096330,2,file,1u);
        msh_log("registered the model's textures (story levels do it at creation; scene 100 skips it)",msh_models[k],file);
    }
    msh_log("hit glow effect texture 0x3BC this level (0 = not loaded)",MSH_FX_HIT,WORD_AT(MSH_FX+MSH_FX_HIT*0x18u+0x14u));
}

/* A ship's firing box (z low, z high, x low, x high) around its spawn and its
 * dust height on its floor, in its own copy (st[0..4], as msh_spans). */
static void msh_rebase(struct msh_one *m,float x,float floor,float z)
{
    float v[5];
    v[0]=floor;v[1]=z-MSH_RANGE;v[2]=z+MSH_RANGE;v[3]=x-MSH_RANGE;v[4]=x+MSH_RANGE;
    memcpy(m->st,v,sizeof v);
}

#ifdef BOS_BAR_EXTRA
/* pc_mapmaker_bosses.inc's Boss bars: ship k as the bar of its removed
 * placeholder AI (record id); entity 0 while that AI still stands there or
 * once the object is no longer the ship (state 5 removes it). */
static int msh_bar_boss(unsigned k,uint32_t *entity,uint32_t *id)
{
    uint32_t obj;
    if(k>=msh_lv.n || k>=MSH_MAX || WORD_AT(MSH_SCENE)!=100u) return 0;
    obj=msh_lv.s[k].obj;
    *entity=msh_lv.s[k].place || !MAPMAKER_PTR_OK(obj,0x200u) || MSH_H(obj+MSH_OBJ_MODEL)!=MSH_MODEL?0u:obj;
    *id=msh_lv.s[k].place_id;
    return 1;
}
#endif
/* The stock bar's label: the focus ship's Story AI name, else "Timesplitter Mothership". */
static uint32_t msh_bar_label(void)
{
#ifdef BOS_BAR_EXTRA
    uint32_t pl=WORD_AT(MSH_PLAYER),buf;
    unsigned k;
    const char *name;
    size_t n;
    if(msh.names && MAPMAKER_PTR_OK(pl,MSH_FOCUS+4u) && (k=msh_find(WORD_AT(pl+MSH_FOCUS)))!=0 &&
       (name=bos_story_name(msh_lv.s[k-1u].place_id))!=0) {
        buf=msh.names+(k-1u)*MSH_NAME;
        n=strlen(name);
        if(n>MSH_NAME-1u) n=MSH_NAME-1u;
        memcpy((void *)((uintptr_t)buf+g_xbox_mem_offset),name,n);
        MSH_B(buf+(uint32_t)n)=0;
        return buf;
    }
#endif
    return msh.label;
}

/* WRAP sub_00062790 (the level's pak mounts, no args). */
void pc_mapmaker_mothership_mount(void (*original)(void))
{
    int has;
    uint32_t mounted;
#ifdef BOS_BAR_EXTRA
    bos_bar_extra_add(msh_bar_boss);                       /* the bosses area's bars: a ship shows its AI's */
#endif
    if(WORD_AT(MSH_MOUNT_SKIP)) { original();return; }    /* the original does nothing */
    original();
    memset(&msh_lv,0,sizeof msh_lv);                       /* a new level: forget the last ships */
    if(!msh_play() || msh.optout || (has=msh_map_has())==0) return;
    if(!msh_string(&msh.pak,MSH_PAK,sizeof MSH_PAK)) return;
    if(msh_pak_mounted()) return;
    if((mounted=WORD_AT(MSH_MOUNTS))+MSH_MOUNT_KEEP>MSH_MOUNT_MAX) {
        msh_log("pak mount table full; the mothership placeholder stays a bot",mounted,MSH_MOUNT_MAX);
        return;
    }
    if((int32_t)guest_call(sub_000622D0,3,msh.pak,0u,1u)<0) msh_log("l_64_ch.pak did not mount; the mothership placeholder stays a bot",mounted,(uint32_t)has);
    else msh_log(has<0?"mounted l_64_ch.pak (map container unreadable)":"mounted l_64_ch.pak for the mothership",mounted+1u,0);
}

/* WRAP sub_0015E7C0 (story level init, no args): request the two models. */
void pc_mapmaker_mothership_request(void (*original)(void))
{
    uint32_t k;
    original();
    if(!msh_play() || msh.optout || msh_map_has()==0) return;
    if(!msh_pak_mounted()) { msh_log("l_64_ch.pak not mounted at level init: models not requested",0,0);return; }
    for(k=0;k<2u;k++) if(!msh_resource(msh_models[k])) { msh_log("no resource row: mothership models not requested",msh_models[k],0);return; }
    for(k=0;k<2u;k++) guest_call(sub_000A5E40,1,msh_models[k]);
    msh_lv.requested=1;
    msh_log("requested the mothership and wreck models",MSH_MODEL,MSH_WRECK);
}

/* WRAP sub_0015B830 (cdecl AI pool entry): the placeholder becomes the ship. */
void pc_mapmaker_mothership_ai_start(void (*original)(void))
{
    uint32_t a,ent,e,obj,args,i,m,keep[MSH_WORDS],rec,id;
    float pos[3],yaw;
    struct msh_one *s;
    if(!MAPMAKER_STACK_OK(8) || !msh_play() || msh.optout) { original();return; }
    a=WORD_AT(g_esp+4u);
    original();
    if(!MAPMAKER_PTR_OK(a,MSH_ACTOR) || MSH_H(a+MSH_ACTOR_CHAR)!=MSH_ID) return;
    if(!MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u)) { msh_refuse("mothership placeholder AI started without an entity: stays a bot",a,0);return; }
    rec=WORD_AT(a+MSH_ACTOR_RECORD);id=MAPMAKER_PTR_OK(rec,12u)?WORD_AT(rec+8u):0u;
    /* Started again: a ship whose placeholder is still this entity, or one
     * made from this spawn record. Not the entity alone: a removed
     * placeholder's entity is reused by the next placeholder (the tank's
     * v0.4.0-mapmaker.28 third placeholder stayed a bot that way). */
    for(i=0;i<msh_lv.n && i<MSH_MAX;i++) {
        s=&msh_lv.s[i];
        if(s->obj && ((s->place && s->place_ent==ent) || (rec && s->place_rec==rec && s->place_id==id))) {
            msh_refuse("mothership placeholder started again: it is already this ship (actor, ship)",a,i+1u);
            return;
        }
    }
    if(msh_lv.n>=MSH_MAX) { msh_refuse("at most 4 motherships per map: this placeholder stays a bot",a,msh_lv.n);return; }
    if(msh_lv.act) { msh_refuse("a mothership is running: this placeholder stays a bot",a,msh_lv.act);return; }
    if(!msh_pak_mounted()) { msh_refuse("l_64_ch.pak not mounted: the mothership placeholder stays a bot",a,0);return; }
    if(!(e=msh_resource(MSH_MODEL)) || WORD_AT(e+MSH_RES_INIT)!=MSH_INIT || !msh_resource(MSH_WRECK)) {
        msh_refuse("mothership models missing from the resource table: the placeholder stays a bot",e,e?WORD_AT(e+MSH_RES_INIT):0);
        return;
    }
    if((m=msh_models_unready())!=0) {
        msh_refuse(msh_lv.requested?"a mothership model was requested but not read: the placeholder stays a bot":
                                    "mothership models not requested this level: the placeholder stays a bot",m,0);
        return;
    }
    for(i=0;i<3u;i++) if(!msh_sane(pos[i]=msh_getf(ent+MSH_ENT_POS+i*4u))) { msh_refuse("placeholder position unusable: it stays a bot",a,i);return; }
    if(!msh_sane(yaw=msh_getf(ent+MSH_ENT_YAW))) yaw=0.0f;
    msh_textures();
    if(!msh.args) {
        uint32_t p=xbox_HeapAlloc(32u,16u);
        if(!p || !MAPMAKER_PTR_OK(p,32u)) { msh_refuse("no memory for the spawn arguments: the placeholder stays a bot",32u,0);return; }
        msh.args=p;
    }
    args=msh.args;
    msh_setf(args,pos[0]);msh_setf(args+4u,pos[1]+MSH_HOVER);msh_setf(args+8u,pos[2]);
    msh_setf(args+12u,0.0f);msh_setf(args+16u,yaw);msh_setf(args+20u,0.0f);
    s=&msh_lv.s[msh_lv.n];
    msh_save(keep);                                            /* the init writes statics: keep them for after */
    obj=guest_call(sub_000A6F90,5,MSH_MODEL,args,args+12u,0u,0u);
    if(obj && MAPMAKER_PTR_OK(obj,0x200u)) { memset(s,0,sizeof *s);msh_save(s->st); }
    msh_load(keep);
    if(!obj || !MAPMAKER_PTR_OK(obj,0x200u)) { msh_refuse("the mothership was not created: the placeholder stays a bot",a,obj);return; }
    WORD_AT(obj+MSH_STATE)=MSH_ATTACK;                        /* no fly-in: attack where it stands */
    s->obj=obj;s->place=a;s->place_ent=ent;s->place_rec=rec;s->place_id=id;
    s->floor=pos[1];
    msh_rebase(s,pos[0],pos[1],pos[2]);
    msh_lv.n++;
    msh_string(&msh.label,MSH_LABEL,sizeof MSH_LABEL);         /* off the text hook's path */
    if(!msh.names) {                                           /* the bar labels: each ship's Story AI name */
        uint32_t p=xbox_HeapAlloc(MSH_MAX*MSH_NAME,4u);
        if(p && MAPMAKER_PTR_OK(p,MSH_MAX*MSH_NAME)) msh.names=p;
        else msh_log("no memory for the bar names: bars read Timesplitter Mothership",MSH_MAX*MSH_NAME,0);
    }
    msh_log("mothership created above the placeholder",a,obj);
    fprintf(stderr,"[mapmaker] mothership %u: spawn (%.2f, %.2f, %.2f), floor %.2f, firing box %.0f m around it\n",msh_lv.n,
            (double)pos[0],(double)(pos[1]+MSH_HOVER),(double)pos[2],(double)pos[1],(double)MSH_RANGE);
    fflush(stderr);
}

/* WRAP sub_00217050 (cdecl ship; its per-frame callback). */
void pc_mapmaker_mothership_update(void (*original)(void))
{
    uint32_t obj,a,ent,player,state;
    unsigned k,p;
    int shift;
    if(!msh_lv.n || !MAPMAKER_STACK_OK(8) || !(k=msh_find(obj=WORD_AT(g_esp+4u)))) { original();return; }
    p=msh_enter(k);
    if(MS.place) {
        a=MS.place;ent=MS.place_ent;MS.place=0;
        if(MAPMAKER_PTR_OK(a,MSH_ACTOR) && WORD_AT(a+MSH_ACTOR_RECORD)==MS.place_rec && WORD_AT(a)==ent &&
           MAPMAKER_PTR_OK(ent,0x100u) && WORD_AT(ent+MSH_ENT_AI)==a) {
            player=WORD_AT(MSH_PLAYER);
            if(MAPMAKER_PTR_OK(player,MSH_FOCUS+4u) && WORD_AT(player+MSH_FOCUS)==ent) WORD_AT(player+MSH_FOCUS)=0;
            guest_call(sub_0015DDA0,1,ent);
            msh_log("mothership placeholder AI removed",a,ent);
        } else msh_log("mothership placeholder AI already gone",a,ent);
    }
    state=WORD_AT(obj+MSH_STATE);
    shift=(state==MSH_FALL || state==MSH_CRASH) && msh_sane(MS.floor);
    if(!shift) { original();msh_leave(k,p);return; }
    if(!MS.shifts++) msh_log("mothership shot down: falling to the map floor",state,obj);
    msh_setf(obj+MSH_ENT_POS+4u,msh_getf(obj+MSH_ENT_POS+4u)-MS.floor);
    original();
    msh_setf(obj+MSH_ENT_POS+4u,msh_getf(obj+MSH_ENT_POS+4u)+MS.floor);
    msh_leave(k,p);
}

/* WRAP sub_00215B50 (cdecl ship; slot 10, draws the hit records). */
void pc_mapmaker_mothership_slot10(void (*original)(void))
{
    if(!msh_lv.n || !MAPMAKER_STACK_OK(8)) { original();return; }
    msh_run(original,WORD_AT(g_esp+4u));
}

/* WRAP sub_002193A0 (cdecl damage record, the hit object at +0; shared by
 * several props, so only our ships' records swap). */
void pc_mapmaker_mothership_damage(void (*original)(void))
{
    uint32_t r;
    if(!msh_lv.n || !MAPMAKER_STACK_OK(8) || !MAPMAKER_PTR_OK(r=WORD_AT(g_esp+4u),4u)) { original();return; }
    msh_run(original,WORD_AT(r));
}

/* WRAP sub_0003D7B0 (cdecl text id -> string): the bar's label. */
void pc_mapmaker_mothership_text(void (*original)(void))
{
    if(!MAPMAKER_STACK_OK(8) || WORD_AT(g_esp+4u)!=MSH_BAR_TEXT || WORD_AT(g_esp)!=MSH_RET_BAR_TEXT || !msh.label ||
       !msh_lv.n || !msh_play()) { original();return; }
    g_eax=msh_bar_label();
    MAPMAKER_GUEST_RET();
}
