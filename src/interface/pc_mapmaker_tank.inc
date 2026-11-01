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
 * fight state is static (0x0078B560..0x0078B5F4), so one tank per map.
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
 *  - ai_start (sub_0015B830): after the original, for the first pool entry
 *    whose character (a+0x48) is TNK_ID: when the pak is mounted and the
 *    resource map (0x008CA000, moved by the rooms area) has 0x281 (init 0x002560C0), 0x297 and 0x298,
 *    create the tank at the placeholder's position, facing its yaw:
 *    sub_000A6F90(0x281, &pos, &rot, 0, 0) (pos -> +0x64, rot -> +0xC0/+0x70/
 *    +0xC8, sub_000A6880; the init runs inside, sub_000A6EC0 0x000A6F3A).
 *    The castle frame offset d = spawn - TNK_C (the castle arena centre) is
 *    kept; the init's first drive target (-9, y, -3.2) is moved by d.
 *    A second placeholder keeps its bot behaviour (logged).
 *  - update (sub_00255550, the tank's per-frame callback, slot 3): on the
 *    first frame, the placeholder AI is removed (sub_0015DDA0, after checking
 *    it is still that AI) so only the tank stands there.
 *  - wake (sub_00253140, the fight's start test in state 0): stock looks up
 *    the castle object 'RDPC' (sub_000B2350 -> 0 in a MapMaker map, so the
 *    fight never starts) and then fires level marker 5 (sub_00227650(5, 3.0)).
 *    For our tank: 1 when a living player stands within TNK_WAKE metres
 *    (x/z) of it, else 0; no marker.
 *  - area (sub_00251CD0, ecx = point: inside one of three castle gate
 *    rectangles 0x0051BB58.. -> states 6/8 and the random-target rejection):
 *    0 for our tank; a MapMaker map has no castle gate.
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
 *  - text (sub_0003D7B0): the boss bar for model 0x281 (sub_0014CC80, focus
 *    set by the update at 0x002555AA) names its two meters with text 0x165B
 *    (return 0x0014CD42) and 0x1635 (0x0014CD83, the state-9 timer from
 *    sub_00251800); in scene 100 those ids are the MapMaker level's own text,
 *    so our labels are returned there.
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
#define TNK_MOUNT_MAX 30u
#define TNK_MOUNT_KEEP 2u
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

extern void sub_00062260(void),sub_000622D0(void),sub_000A6F90(void),sub_0015DDA0(void);
extern uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment);

/* Kept for the session: settings, labels and the spawn argument buffer. */
static struct { int read, optout; uint32_t label, timer, args; unsigned logged; } tnk;
/* This level's tank (cleared by every level's pak mount). */
static struct {
    uint32_t obj, place, place_ent, place_rec;
    float d[3], spawn[3];
    int shifted;
    unsigned extras, events, wakes;
} tnk_lv;

static void tnk_log(const char *what,uint32_t a,uint32_t b)
{
    if(tnk.logged>=24u) return;
    tnk.logged++;
    fprintf(stderr,"[mapmaker] tank: %s (0x%X, 0x%X)\n",what,(unsigned)a,(unsigned)b);
    fflush(stderr);
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
/* This level's tank, in scene 100. */
static int tnk_mine(uint32_t obj) { return tnk_lv.obj && obj==tnk_lv.obj && WORD_AT(TNK_SCENE)==100u && tnk_on(); }

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
    uint32_t a,ent,e,obj,args,i;
    float pos[3],yaw;
    if(!MAPMAKER_STACK_OK(8) || !tnk_play() || tnk.optout) { original();return; }
    a=WORD_AT(g_esp+4u);
    original();
    if(!MAPMAKER_PTR_OK(a,TNK_ACTOR) || TNK_H(a+TNK_ACTOR_CHAR)!=TNK_ID || !MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u)) return;
    if(tnk_lv.obj) {
        if(tnk_lv.extras++<4u) tnk_log("only one castle tank per map: this placeholder stays a bot",a,tnk_lv.obj);
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
    for(i=0;i<3u;i++) if(!tnk_sane(pos[i]=tnk_getf(ent+TNK_ENT_POS+i*4u))) { tnk_log("placeholder position unusable",a,i);return; }
    if(!tnk_sane(yaw=tnk_getf(ent+TNK_ENT_YAW))) yaw=0.0f;
    if(!tnk.args) {
        uint32_t p=xbox_HeapAlloc(32u,16u);
        if(!p || !MAPMAKER_PTR_OK(p,32u)) { tnk_log("no memory for the spawn arguments",32u,0);return; }
        tnk.args=p;
    }
    args=tnk.args;
    for(i=0;i<3u;i++) tnk_setf(args+i*4u,pos[i]);
    tnk_setf(args+12u,0.0f);tnk_setf(args+16u,yaw);tnk_setf(args+20u,0.0f);
    obj=guest_call(sub_000A6F90,5,TNK_MODEL,args,args+12u,0u,0u);
    if(!obj || !MAPMAKER_PTR_OK(obj,0x100u)) { tnk_log("the tank was not created (model not loaded?): placeholder stays a bot",a,obj);return; }
    tnk_lv.obj=obj;tnk_lv.place=a;tnk_lv.place_ent=ent;tnk_lv.place_rec=WORD_AT(a+TNK_ACTOR_RECORD);
    for(i=0;i<3u;i++) tnk_lv.spawn[i]=pos[i];
    tnk_lv.d[0]=pos[0]-TNK_CX;tnk_lv.d[1]=pos[1]-TNK_CY;tnk_lv.d[2]=pos[2]-TNK_CZ;
    tnk_setf(TNK_GOAL,tnk_getf(TNK_GOAL)+tnk_lv.d[0]);           /* the init's first drive target */
    tnk_setf(TNK_GOAL+8u,tnk_getf(TNK_GOAL+8u)+tnk_lv.d[2]);
    tnk_string(&tnk.label,TNK_LABEL,sizeof TNK_LABEL);              /* off the text hook's path */
    tnk_string(&tnk.timer,TNK_TIMER_LABEL,sizeof TNK_TIMER_LABEL);
    tnk_log("castle tank created at the placeholder",a,obj);
    fprintf(stderr,"[mapmaker] tank: spawn (%.2f, %.2f, %.2f), castle frame offset (%.2f, %.2f, %.2f)\n",
            (double)pos[0],(double)pos[1],(double)pos[2],(double)tnk_lv.d[0],(double)tnk_lv.d[1],(double)tnk_lv.d[2]);
    fflush(stderr);
}

/* WRAP sub_00255550 (cdecl tank; its per-frame callback). */
void pc_mapmaker_tank_update(void (*original)(void))
{
    uint32_t a,ent,player;
    if(!tnk_lv.place || !MAPMAKER_STACK_OK(8) || !tnk_mine(WORD_AT(g_esp+4u))) { original();return; }
    a=tnk_lv.place;ent=tnk_lv.place_ent;tnk_lv.place=0;
    if(MAPMAKER_PTR_OK(a,TNK_ACTOR) && WORD_AT(a+TNK_ACTOR_RECORD)==tnk_lv.place_rec && WORD_AT(a)==ent &&
       MAPMAKER_PTR_OK(ent,0x100u) && WORD_AT(ent+TNK_ENT_AI)==a) {
        player=WORD_AT(TNK_PLAYER);
        if(MAPMAKER_PTR_OK(player,TNK_FOCUS+4u) && WORD_AT(player+TNK_FOCUS)==ent) WORD_AT(player+TNK_FOCUS)=0;
        guest_call(sub_0015DDA0,1,ent);
        tnk_log("tank placeholder AI removed",a,ent);
    } else tnk_log("tank placeholder AI already gone",a,ent);
    original();
}

/* WRAP sub_00253140 (no args -> 1 start the fight): a living player is near. */
void pc_mapmaker_tank_wake(void (*original)(void))
{
    uint32_t n,base,k,ent;
    float x,z,dx,dz,h;
    if(!tnk_lv.obj || !MAPMAKER_STACK_OK(4) || WORD_AT(TNK_SCENE)!=100u || !tnk_on()) { original();return; }
    g_eax=0;
    x=tnk_getf(tnk_lv.obj+TNK_ENT_POS);z=tnk_getf(tnk_lv.obj+TNK_ENT_POS+8u);
    n=WORD_AT(TNK_PLAYERS);base=WORD_AT(TNK_PLAYER_BASE);
    if(n>4u) n=4u;
    for(k=0;k<n && MAPMAKER_PTR_OK(base,TNK_PLAYER_STRIDE*4u);k++) {
        ent=WORD_AT(base+k*TNK_PLAYER_STRIDE+TNK_PLAYER_ENT);
        if(!MAPMAKER_PTR_OK(ent,0x200u) || !((h=tnk_getf(ent+TNK_ENT_HEALTH))>0.0f)) continue;
        dx=tnk_getf(ent+TNK_ENT_POS)-x;dz=tnk_getf(ent+TNK_ENT_POS+8u)-z;
        if(dx*dx+dz*dz<=TNK_WAKE*TNK_WAKE) { g_eax=1;break; }
    }
    if(g_eax && !tnk_lv.wakes++) tnk_log("castle tank fight started: a player came near",k,tnk_lv.obj);
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_00251CD0 (ecx = point -> inside a castle gate rectangle): never. */
void pc_mapmaker_tank_area(void (*original)(void))
{
    if(!tnk_lv.obj || !MAPMAKER_STACK_OK(4) || WORD_AT(TNK_SCENE)!=100u || !tnk_on()) { original();return; }
    g_eax=0;
    MAPMAKER_GUEST_RET();
}

/* The original in the castle frame: tank and target moved by -d and
 * restored; the drive target handed in and out in castle coordinates; with
 * guns, state 7's five aim points moved by +d afterwards. */
static void tnk_shifted(void (*original)(void),int guns)
{
    uint32_t tank=tnk_lv.obj,t=WORD_AT(TNK_TARGET),i,k,g,cas[3],old[3];
    float tk[3],tp[3];
    int tok=t && t!=tank && MAPMAKER_PTR_OK(t,0x100u);
    for(i=0;i<3u;i++) {
        tk[i]=tnk_getf(tank+TNK_ENT_POS+i*4u);tnk_setf(tank+TNK_ENT_POS+i*4u,tk[i]-tnk_lv.d[i]);
        if(tok) { tp[i]=tnk_getf(t+TNK_ENT_POS+i*4u);tnk_setf(t+TNK_ENT_POS+i*4u,tp[i]-tnk_lv.d[i]); }
        old[i]=WORD_AT(TNK_GOAL+i*4u);
        tnk_setf(TNK_GOAL+i*4u,tnk_getf(TNK_GOAL+i*4u)-tnk_lv.d[i]);
        cas[i]=WORD_AT(TNK_GOAL+i*4u);
    }
    tnk_lv.shifted=1;
    original();
    tnk_lv.shifted=0;
    for(i=0;i<3u;i++) {
        tnk_setf(tank+TNK_ENT_POS+i*4u,tk[i]);
        if(tok) tnk_setf(t+TNK_ENT_POS+i*4u,tp[i]);
        if(WORD_AT(TNK_GOAL+i*4u)==cas[i]) WORD_AT(TNK_GOAL+i*4u)=old[i];   /* not chosen: exactly as before */
        else tnk_setf(TNK_GOAL+i*4u,tnk_getf(TNK_GOAL+i*4u)+tnk_lv.d[i]);
    }
    if(guns && MAPMAKER_PTR_OK(g=WORD_AT(TNK_GUNS),TNK_GUN_STRIDE*TNK_GUN_COUNT))
        for(k=0;k<TNK_GUN_COUNT;k++)
            for(i=0;i<3u;i++) {
                uint32_t p=g+k*TNK_GUN_STRIDE+TNK_GUN_AIM+i*4u;
                tnk_setf(p,tnk_getf(p)+tnk_lv.d[i]);
            }
}

/* WRAP sub_00254690 (edi = tank; picks the drive target). */
void pc_mapmaker_tank_target(void (*original)(void))
{
    if(tnk_lv.shifted || !tnk_mine(g_edi)) { original();return; }
    tnk_shifted(original,0);
}

/* WRAP sub_00255390 (eax = tank; runs a state's entry once per change). */
void pc_mapmaker_tank_state(void (*original)(void))
{
    if(!tnk_mine(g_eax) || WORD_AT(TNK_STATE)!=TNK_STATE_LOW || WORD_AT(TNK_PREV)==TNK_STATE_LOW) { original();return; }
    tnk_shifted(original,1);
}

/* WRAP sub_00255C20 (esi = tank; wreck and turret, then the tank is parked). */
void pc_mapmaker_tank_wreck(void (*original)(void))
{
    uint32_t tank=g_esi;
    original();
    if(!tnk_mine(tank)) return;
    tnk_setf(tank+TNK_ENT_POS,tnk_lv.spawn[0]);
    tnk_setf(tank+TNK_ENT_POS+4u,tnk_lv.spawn[1]+TNK_PARK);
    tnk_setf(tank+TNK_ENT_POS+8u,tnk_lv.spawn[2]);
    tnk_log("castle tank destroyed: wreck made, tank parked under its spawn",tank,0);
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
