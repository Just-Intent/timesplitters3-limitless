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
 * adds its own models. Its fight state lives on the object and in statics
 * (0x0075C0DC.., 0x0078D500..), so one mothership per map.
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
 *    ship and put the rebased constants back to their stock values; when the
 *    map's own custom set holds MSH_ID (or the container cannot be read),
 *    mount pak/challenge/l_64_ch.pak with sub_000622D0(path, 0, 1): kept
 *    open, entries read on demand, as chr.pak.
 *  - request (sub_0015E7C0): request the ship and wreck models
 *    (sub_000A5E40) so the level's load pass reads them.
 *  - ai_start (sub_0015B830): after the original, for the first pool entry
 *    whose character is MSH_ID, when both models are ready: create the ship
 *    MSH_HOVER metres above the placeholder, facing its yaw
 *    (sub_000A6F90(0x539, &pos, &rot, 0, 0); the init runs inside), then
 *    state 2 as sub_00217030 does, and rebase the firing box to MSH_RANGE
 *    metres around the spawn (x/z) and the dust height to the placeholder's
 *    floor. A second placeholder keeps its bot behaviour.
 *  - update (sub_00217050, the ship's per-frame callback): on its first
 *    frame the placeholder AI is removed (sub_0015DDA0). While falling or
 *    crashing (states 3 and 4 at entry) the ship's height is moved down by
 *    the floor for the call and back after, so the fall ends 6.6 m above
 *    the map floor instead of at the intro's absolute 6.6.
 *  - text (sub_0003D7B0): the bar label 0x164B (pushed by the 0x539 case of
 *    sub_0014CC80, call 0x0014D09A, return 0x0014D09F) is a level-local
 *    text id; in scene 100 our label is returned there.
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
#define MSH_BAR_TEXT 0x164Bu
#define MSH_RET_BAR_TEXT 0x0014D09Fu     /* sub_0014CC80: call sub_0003D7B0 at 0x0014D09A */
#define MSH_LABEL "Timesplitter Mothership"
#define MSH_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define MSH_H(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

static const uint32_t msh_models[2]={MSH_MODEL,MSH_WRECK};

extern void sub_00062260(void),sub_000622D0(void),sub_000A6F90(void),sub_0015DDA0(void);
extern void sub_000A5E40(void),sub_000631C0(void);
extern uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment);

/* Kept for the session: settings, strings, spawn arguments, stock constants. */
static struct { int read, optout, saved; uint32_t label, pak, args, stock[5]; unsigned logged; } msh;
/* This level's ship (cleared by every level's pak mount). */
static struct {
    uint32_t obj, place, place_ent, place_rec;
    float floor;
    int requested, rebased;
    unsigned extras, shifts;
} msh_lv;

static void msh_log(const char *what,uint32_t a,uint32_t b)
{
    if(msh.logged>=24u) return;
    msh.logged++;
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
static int msh_mine(uint32_t obj) { return msh_lv.obj && obj==msh_lv.obj && WORD_AT(MSH_SCENE)==100u && msh_on(); }

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

/* The firing box and the dust height back to stock (another level may be the
 * intro); saved once, before the first rebase. */
static void msh_restore(void)
{
    uint32_t k;
    if(!msh.saved) return;
    for(k=0;k<4u;k++) WORD_AT(MSH_BOX+k*4u)=msh.stock[k];
    WORD_AT(MSH_DUST)=msh.stock[4];
}
static void msh_rebase(float x,float floor,float z)
{
    uint32_t k;
    if(!msh.saved) {
        for(k=0;k<4u;k++) msh.stock[k]=WORD_AT(MSH_BOX+k*4u);
        msh.stock[4]=WORD_AT(MSH_DUST);
        msh.saved=1;
    }
    msh_setf(MSH_BOX,z-MSH_RANGE);msh_setf(MSH_BOX+4u,z+MSH_RANGE);
    msh_setf(MSH_BOX+8u,x-MSH_RANGE);msh_setf(MSH_BOX+12u,x+MSH_RANGE);
    msh_setf(MSH_DUST,floor);
    msh_lv.rebased=1;
}

/* WRAP sub_00062790 (the level's pak mounts, no args). */
void pc_mapmaker_mothership_mount(void (*original)(void))
{
    int has;
    uint32_t mounted;
    if(WORD_AT(MSH_MOUNT_SKIP)) { original();return; }    /* the original does nothing */
    original();
    memset(&msh_lv,0,sizeof msh_lv);                       /* a new level: forget the last ship */
    msh_restore();
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
    uint32_t a,ent,e,obj,args,i,m;
    float pos[3],yaw;
    if(!MAPMAKER_STACK_OK(8) || !msh_play() || msh.optout) { original();return; }
    a=WORD_AT(g_esp+4u);
    original();
    if(!MAPMAKER_PTR_OK(a,MSH_ACTOR) || MSH_H(a+MSH_ACTOR_CHAR)!=MSH_ID || !MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u)) return;
    if(msh_lv.obj) {
        if(msh_lv.extras++<4u) msh_log("only one mothership per map: this placeholder stays a bot",a,msh_lv.obj);
        return;
    }
    if(!msh_pak_mounted()) { msh_log("l_64_ch.pak not mounted: the mothership placeholder stays a bot",a,0);return; }
    if(!(e=msh_resource(MSH_MODEL)) || WORD_AT(e+MSH_RES_INIT)!=MSH_INIT || !msh_resource(MSH_WRECK)) {
        msh_log("mothership models missing from the resource table: the placeholder stays a bot",e,e?WORD_AT(e+MSH_RES_INIT):0);
        return;
    }
    if((m=msh_models_unready())!=0) {
        msh_log(msh_lv.requested?"a mothership model was requested but not read: the placeholder stays a bot":
                                 "mothership models not requested this level: the placeholder stays a bot",m,0);
        return;
    }
    for(i=0;i<3u;i++) if(!msh_sane(pos[i]=msh_getf(ent+MSH_ENT_POS+i*4u))) { msh_log("placeholder position unusable",a,i);return; }
    if(!msh_sane(yaw=msh_getf(ent+MSH_ENT_YAW))) yaw=0.0f;
    if(!msh.args) {
        uint32_t p=xbox_HeapAlloc(32u,16u);
        if(!p || !MAPMAKER_PTR_OK(p,32u)) { msh_log("no memory for the spawn arguments",32u,0);return; }
        msh.args=p;
    }
    args=msh.args;
    msh_setf(args,pos[0]);msh_setf(args+4u,pos[1]+MSH_HOVER);msh_setf(args+8u,pos[2]);
    msh_setf(args+12u,0.0f);msh_setf(args+16u,yaw);msh_setf(args+20u,0.0f);
    obj=guest_call(sub_000A6F90,5,MSH_MODEL,args,args+12u,0u,0u);
    if(!obj || !MAPMAKER_PTR_OK(obj,0x200u)) { msh_log("the mothership was not created: the placeholder stays a bot",a,obj);return; }
    WORD_AT(obj+MSH_STATE)=MSH_ATTACK;                        /* no fly-in: attack where it stands */
    msh_lv.obj=obj;msh_lv.place=a;msh_lv.place_ent=ent;msh_lv.place_rec=WORD_AT(a+MSH_ACTOR_RECORD);
    msh_lv.floor=pos[1];
    msh_rebase(pos[0],pos[1],pos[2]);
    msh_string(&msh.label,MSH_LABEL,sizeof MSH_LABEL);         /* off the text hook's path */
    msh_log("mothership created above the placeholder",a,obj);
    fprintf(stderr,"[mapmaker] mothership: spawn (%.2f, %.2f, %.2f), floor %.2f, firing box %.0f m around it\n",
            (double)pos[0],(double)(pos[1]+MSH_HOVER),(double)pos[2],(double)pos[1],(double)MSH_RANGE);
    fflush(stderr);
}

/* WRAP sub_00217050 (cdecl ship; its per-frame callback). */
void pc_mapmaker_mothership_update(void (*original)(void))
{
    uint32_t obj,a,ent,player,state;
    int shift;
    if(!msh_lv.obj || !MAPMAKER_STACK_OK(8) || !msh_mine(obj=WORD_AT(g_esp+4u))) { original();return; }
    if(msh_lv.place) {
        a=msh_lv.place;ent=msh_lv.place_ent;msh_lv.place=0;
        if(MAPMAKER_PTR_OK(a,MSH_ACTOR) && WORD_AT(a+MSH_ACTOR_RECORD)==msh_lv.place_rec && WORD_AT(a)==ent &&
           MAPMAKER_PTR_OK(ent,0x100u) && WORD_AT(ent+MSH_ENT_AI)==a) {
            player=WORD_AT(MSH_PLAYER);
            if(MAPMAKER_PTR_OK(player,MSH_FOCUS+4u) && WORD_AT(player+MSH_FOCUS)==ent) WORD_AT(player+MSH_FOCUS)=0;
            guest_call(sub_0015DDA0,1,ent);
            msh_log("mothership placeholder AI removed",a,ent);
        } else msh_log("mothership placeholder AI already gone",a,ent);
    }
    state=WORD_AT(obj+MSH_STATE);
    shift=(state==MSH_FALL || state==MSH_CRASH) && msh_sane(msh_lv.floor);
    if(!shift) { original();return; }
    if(!msh_lv.shifts++) msh_log("mothership shot down: falling to the map floor",state,obj);
    msh_setf(obj+MSH_ENT_POS+4u,msh_getf(obj+MSH_ENT_POS+4u)-msh_lv.floor);
    original();
    msh_setf(obj+MSH_ENT_POS+4u,msh_getf(obj+MSH_ENT_POS+4u)+msh_lv.floor);
}

/* WRAP sub_0003D7B0 (cdecl text id -> string): the bar's label. */
void pc_mapmaker_mothership_text(void (*original)(void))
{
    if(!MAPMAKER_STACK_OK(8) || WORD_AT(g_esp+4u)!=MSH_BAR_TEXT || WORD_AT(g_esp)!=MSH_RET_BAR_TEXT || !msh.label ||
       !msh_lv.obj || !msh_play()) { original();return; }
    g_eax=msh.label;
    MAPMAKER_GUEST_RET();
}
