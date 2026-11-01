/* US 45410066 MapMaker real story boss: the far-future giant robot of story
 * mission 10 (scene 14) as a MapMaker Story AI with its story behaviour.
 * Docs: docs/mapmaker/mapmaker-robot.md. Research basis:
 * logs/limits/features/real-bosses.md section 6 (main checkout, ignored).
 * Area `robot`; every hook is inert unless TSFP_MAPMAKER_LIMITS=1, and
 * TSFP_MAPMAKER_REAL_BOSS=0 keeps the robot on the normal bot AI.
 *
 * The robot (CONFIRMED, US XBE data and tests/mapmaker/test_mapmaker_robot.py):
 * character id 0x42 (character table 0x0053A3CC + 0x42*0x30: voice set
 * 0x0053A0C8 = the Arcade giant robot's, model 0x5F2 chrs/FF2GiantRobot, no
 * LOD, story-only flag 0x2000000), no Arcade-table row. The model is in
 * pak/chr.pak, which a MapMaker map in play mounts; sub_000A5E40 skips the
 * story-only flag in scene 100. The picker row, its names and its preview
 * stand-in (the Arcade giant robot, id 0xA8) are added by the integrator in
 * pc_mapmaker_realbosses.inc (docs: "Wrappers for the integrator").
 *
 * Behaviour: AI class 0x80000 (record +4 -> AI +0x14, +0x10 = 0;
 * sub_0027D590 0x0027DDF5 -> call sub_00293730(entity) at 0x0027DE18, no scene
 * test; its message handler sub_00294520 at 0x0027D447). Its three animations
 * giantrob_startthroughdoor 0x807, giantrob_walkcycle 0x808 and
 * giantrob_death 0x809 are named only by pak/stream/lv14/anim/lv14.pak
 * (string 0x0049DC78, 258 KB, registered by sub_001CA2F0 at 0x001CA316). The
 * "scripted door entry" is state 1 playing 0x807 until sub_00059DD0 says the
 * animation ended: no level door is involved, so it completes anywhere.
 * Per-level state: sub_00292630 = [0x0078B1DC] = sub_0003DFA0(0x4C, 1), then
 * sub_00292550 sets every field; the handler dereferences it unchecked
 * (0x002937DC). Its particles use the shared 800 x 0x28 block [0x0078B0D0]
 * (sub_002A62C0). Stock allocates both from sub_0025F720 for scene 14 only
 * (0x0025F760..0x0025F76A, with sub_0028D6D0, the split Crowe's block, which
 * no robot function reads). The fight works from the robot's and the first
 * player's positions (+0x64, copied into the state each frame); it forces no
 * height and looks up no tagged object.
 *
 *  - anims (sub_001C8540): after the original, in a MapMaker story map whose
 *    custom set (slots 1-20) holds id 0x42 (or an unreadable container),
 *    mount lv14.pak with sub_0005A130 like the stock per-level anim paks when
 *    it is not mounted yet (the split Crowe area may have mounted it) and two
 *    slots of the 30-slot table stay free.
 *  - level (sub_0025F720): before the original the particle pointer, a
 *    dangling one of an earlier level in scene 100, is cleared; after it, for
 *    such a map, unless the real bosses area already made one this level,
 *    sub_002A62C0 the particle block (32 KB), then RBT_MAX (4) state blocks:
 *    sub_00292630 each time (a fresh 76-byte block, set). Outside any robot
 *    [0x0078B1DC] holds the first. Every level start forgets the last level.
 *  - ai_start (sub_0015B830): each robot up to the blocks made gets record
 *    +0/+4 = 0/0x80000 (lv14.pak mounted) and its own block and statics;
 *    the same record again (restart) keeps its block; more keep the
 *    MapMaker class (logged).
 *  - handler (sub_00293730), message (sub_00294520), callback (sub_002926B0)
 *    and damage (sub_002936C0, entity at hit record +0): the only ways into
 *    the code that reads the robot's globals (test). Each runs with that
 *    robot's state pointer [0x0078B1DC] and its three statics (shot counter
 *    [0x0075F5EC], walk/attack timers [0x0075F5F0/F4]) loaded, then the
 *    robot's statics are kept and the previous values put back (nests). The
 *    particle pool [0x0078B0D0] stays shared (same parameters for all). The
 *    split Crowe's [0x0075F5E8] is never touched by robot code.
 *  - nav (sub_0009CC40(pad id) -> pad index or -1): the walk picks hangar
 *    pad ids (0x46E, 0x6A4, 0x465, 0x466, 0x473, 0x468, 0x690) from the
 *    player's pad position against hangar coordinates and falls back to the
 *    player's own pad (+0x678) when the id is missing. MapMaker pad ids are
 *    0x3E9 + i, so a map with more than 124 pads would send the robot to
 *    arbitrary pads: for the seven existence tests in the handler, in scene
 *    100 with the robot, -1 (the robot walks to the player's pad). The two
 *    calls that fetch the index run only after a test found the id.
 *  - music (sub_000462B0(track)): its death sets the music track to 0 every
 *    frame until the event (0x002944D4); dropped in scene 100.
 *  - event (sub_002274C0): when its death animation has ended the robot fires
 *    level event 2 every frame (call 0x002944E0, return 0x002944E5) and stays
 *    in state 6, spawning explosions; in MapMaker that is the map's own logic
 *    marker 2: dropped, and the robot whose code runs is removed with
 *    sub_0015DDA0 (as the skeleton's stock death), a story focus on it cleared.
 *  - fx (sub_00126560, called by the frame sub_001CEBC0 at 0x001CEE87): its
 *    explosions (state +0x48 = 1 from the death message on, 0x00294697) are
 *    drawn by sub_00292650 only in scene 14 (frame 0x001CEE49..0x001CEE58)
 *    and only with a 'MECH' object: sub_00019D70(1), sub_002A7F30(view, 2.5).
 *    After the original, in scene 100 while any of this level's robots is
 *    dying (its own state), once a frame, the same two calls (with the stock one-player test of 0x00292676). The
 *    particles age only when new ones spawn, so nothing is drawn after the
 *    robot is removed.
 *  - reset (sub_0025F6A0, story soft restart: sub_0015E4F0 tail-jumps to it
 *    after the level was initialised, 0x0015E7B1): resets the state with
 *    sub_00292550 only in scene 14 (0x0025F6D2); in scene 100, after the
 *    original, the same reset on every robot's block, statics cleared.
 *  - hud (sub_0014CC80; chain INSIDE pc_mapmaker_bosses_hud): the stock bar
 *    for model 0x5F2 (label 0x163D, a level-local text id, 0x0014D076) is
 *    never drawn for the robot: the focus is cleared for the original call
 *    and restored after, so the bosses area draws its Boss bar (Boss flag),
 *    or none (Sub-boss bar choice, Boss Off). The robot makes no overhead
 *    meter record (no sub_0012C800 call), so no meter hook is needed.
 *
 * Save format: unchanged (the id is a stock custom-set member). Requires the
 * includer's WORD_AT, guest_call, g_esp/g_eax, g_xbox_mem_offset and
 * pc_mapmaker_limits.inc (mapmaker_limits, MAPMAKER_GUEST_RET,
 * MAPMAKER_PTR_OK). Hot hooks (nav: AI pathing; fx: every frame; hud: every
 * HUD frame) test a fixed value first. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif

#define RBT_SCENE 0x0079094Cu
#define RBT_MODE 0x007DE455u            /* byte: 10 = story */
#define RBT_ID 0x42u                    /* character id of the giant robot */
#define RBT_MODEL 0x5F2u                /* chrs/FF2GiantRobot, no LOD */
#define RBT_PAK 0x0049DC78u             /* "pak/stream/lv14/anim/lv14.pak" */
#define RBT_MOUNTS 0x006D7880u          /* mounted paks, table of 30 (0x000620F9) */
#define RBT_MOUNT_MAX 64u               /* paktable.json: 64 slots at 0x00907000 */
#define RBT_MOUNT_KEEP 8u               /* leave this many slots for stock streaming */
#define RBT_CLASS 0x80000u              /* record +4 -> AI +0x14: sub_00293730 */
#define RBT_STATE 0x0078B1DCu           /* 0x4C bytes, sub_00292630 */
#define RBT_STATE_SIZE 0x4Cu
#define RBT_DYING 0x48u                 /* state: 1 from the death message on */
#define RBT_MAX 4u                      /* robots per map, each with its own state */
#define RBT_STATICS 0x0075F5ECu         /* shot counter (sub_00293100), walk and attack timers (sub_00293730) */
#define RBT_STATICS_N 3u
#define RBT_PARTS 0x0078B0D0u           /* 0x7D00 bytes, sub_002A62C0 (shared) */
#define RBT_PARTS_SIZE 0x7D00u
#define RBT_CONTAINER 0x00784014u
#define RBT_MAGIC 0x00C77667u
#define RBT_TAG_INFO 0x00C77668u
#define RBT_TAG_END 0x00C7766Au
#define RBT_TAG_SETS 0x00C776F5u        /* pc_mapmaker_sets.inc: +0 word 1, +2 10, +3 count, +4 members */
#define RBT_SCAN 0x00080000u
#define RBT_INFO_SET 0x14u
#define RBT_INFO_COUNT 0x16u
#define RBT_INFO_MEMBERS 0x18u
#define RBT_INFO_SLOTS 10u
#define RBT_SET_MAX 20u
#define RBT_SET_MAP 0x11u               /* the map's own set */
#define RBT_SETS_SIZE 0x30u
#define RBT_ACTOR 0xEA0u
#define RBT_ACTOR_CHAR 0x48u
#define RBT_ACTOR_RECORD 0xBF0u
#define RBT_ENT_AI 0x7Cu
#define RBT_ENT_MODEL 0x94u
#define RBT_PLAYER 0x007B0C7Cu
#define RBT_FOCUS 0xFF4u
#define RBT_PLAYERS 0x00790950u         /* player count */
#define RBT_CAMERA 0x007B0CC0u          /* pointer: +0x208 the particle view */
#define RBT_CAMERA_VIEW 0x208u
#define RBT_FX_SCALE 0x40200000u        /* 2.5f, as 0x00292698 */
#define RBT_RET_FX 0x001CEE8Cu          /* sub_001CEBC0: call sub_00126560 at 0x001CEE87 */
#define RBT_RET_EVENT 0x002944E5u       /* sub_00293730: call sub_002274C0(2, 0.5) at 0x002944E0 */
#define RBT_RET_MUSIC 0x002944D9u       /* sub_00293730: call sub_000462B0(0) at 0x002944D4 */
#define RBT_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define RBT_H(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

/* sub_00293730: returns of the seven sub_0009CC40 existence tests */
static const uint32_t rbt_nav_rets[]={0x00293949u,0x00293BC9u,0x00293C10u,0x00293C2Bu,0x00293C64u,0x00293C9Eu,0x00293CD0u};

extern void sub_0005A130(void),sub_00062260(void),sub_00292630(void),sub_002A62C0(void),sub_00292550(void);
extern void sub_0015DDA0(void),sub_00019D70(void),sub_002A7F30(void);

static struct { int read, optout; unsigned logged; } rbt;
/* This level's robots: up to RBT_MAX, each with its own state block, its
 * values of the three robot statics, its spawn record and pool entry. */
static struct { uint32_t state, record, actor, statics[RBT_STATICS_N]; int removed; } rbt_inst[RBT_MAX];
static struct { unsigned blocks, n, extras, events, navs, music, fx, resets, swaps; int cur; } rbt_lvl;

static void rbt_log(const char *what,uint32_t a,uint32_t b)
{
    if(rbt.logged>=24u) return;
    rbt.logged++;
    fprintf(stderr,"[mapmaker] robot: %s (0x%X, 0x%X)\n",what,(unsigned)a,(unsigned)b);
    fflush(stderr);
}
static int rbt_on(void)
{
    mapmaker_limits_read();
    if(!rbt.read) {
        const char *v=getenv("TSFP_MAPMAKER_REAL_BOSS");
        rbt.read=1;
        rbt.optout=v && v[0]=='0' && !v[1];
    }
    return mapmaker_limits.limits;
}
static int rbt_play(void)
{
    return rbt_on() && WORD_AT(RBT_SCENE)==100u && RBT_B(RBT_MODE)==10u;
}

/* The map's custom set, slots 1-20 (as pc_mapmaker_realbosses.inc reads it):
 * 1 = it holds the robot, 0 = it does not or another set plays, -1 = unreadable. */
static int rbt_map_has(void)
{
    uint32_t container=WORD_AT(RBT_CONTAINER),c,end,steps,info=0,isz=0,extra=0,n,k;
    if(!container || !MAPMAKER_PTR_OK(container,RBT_SCAN) || WORD_AT(container)!=RBT_MAGIC) return -1;
    c=container+8u;end=container+RBT_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u),d=c+8u;
        if(tag==RBT_TAG_END || sz>end-c-8u) break;
        if(tag==RBT_TAG_INFO && sz>=RBT_INFO_MEMBERS && !info) { info=d;isz=sz; }
        else if(tag==RBT_TAG_SETS && sz>=RBT_SETS_SIZE && !extra && RBT_H(d)==1u && RBT_B(d+2u)==RBT_INFO_SLOTS &&
                RBT_B(d+3u)>=1u && RBT_B(d+3u)<=RBT_SET_MAX-RBT_INFO_SLOTS) extra=d;
        c+=8u+sz;
    }
    if(!info) return -1;
    if(RBT_H(info+RBT_INFO_SET)!=RBT_SET_MAP) return 0;   /* a preset or profile set */
    n=RBT_B(info+RBT_INFO_COUNT);
    if(n>RBT_INFO_SLOTS) n=RBT_INFO_SLOTS;
    if(RBT_INFO_MEMBERS+n*4u>isz) n=(isz-RBT_INFO_MEMBERS)/4u;
    for(k=0;k<n;k++) if(RBT_H(info+RBT_INFO_MEMBERS+k*4u)==RBT_ID) return 1;
    for(k=0;extra && k<RBT_B(extra+3u);k++) if(RBT_H(extra+4u+k*4u)==RBT_ID) return 1;   /* slots 11-20 */
    return 0;
}
static int rbt_pak_mounted(void) { return (int32_t)guest_call(sub_00062260,1,RBT_PAK)!=-1; }

/* WRAP sub_001C8540 (mount the level's anim paks; names registered next). */
void pc_mapmaker_robot_anims(void (*original)(void))
{
    int has;
    uint32_t mounted;
    original();
    if(!rbt_play() || rbt.optout || (has=rbt_map_has())==0) return;
    if(rbt_pak_mounted()) { rbt_log("lv14.pak already mounted",0,(uint32_t)has);return; }
    if((mounted=WORD_AT(RBT_MOUNTS))+RBT_MOUNT_KEEP>RBT_MOUNT_MAX) {
        rbt_log("pak mount table full; the giant robot keeps the bot behaviour",mounted,RBT_MOUNT_MAX);
        return;
    }
    if(!guest_call(sub_0005A130,1,RBT_PAK)) rbt_log("lv14.pak did not mount; bot behaviour",mounted,(uint32_t)has);
    else rbt_log(has<0?"mounted lv14.pak (map container unreadable)":"mounted lv14.pak for the giant robot",mounted+1u,0);
}

/* WRAP sub_0025F720 (story level init, no args): one state block per robot. */
void pc_mapmaker_robot_level(void (*original)(void))
{
    int play=rbt_play() && !rbt.optout,has;
    uint32_t st,pt,k;
    if(play) WORD_AT(RBT_PARTS)=0;                     /* an earlier level's block; scene 100 init never uses it */
    original();
    memset(&rbt_lvl,0,sizeof rbt_lvl);                /* a new level: nothing of the last one */
    memset(rbt_inst,0,sizeof rbt_inst);
    rbt_lvl.cur=-1;
    if(!play || (has=rbt_map_has())==0) return;
    if(!WORD_AT(RBT_PARTS)) guest_call(sub_002A62C0,0);   /* the skeleton or walker may have made it */
    pt=WORD_AT(RBT_PARTS);
    if(!pt || !MAPMAKER_PTR_OK(pt,RBT_PARTS_SIZE)) { rbt_log("no level memory for the giant robot's particles; bot behaviour",pt,0);return; }
    for(k=0;k<RBT_MAX;k++) {
        WORD_AT(RBT_STATE)=0;                          /* stale pointer of an earlier level */
        guest_call(sub_00292630,0);                    /* allocates and sets a fresh block */
        st=WORD_AT(RBT_STATE);
        if(!st || !MAPMAKER_PTR_OK(st,RBT_STATE_SIZE)) break;
        rbt_inst[k].state=st;
    }
    rbt_lvl.blocks=k;
    WORD_AT(RBT_STATE)=rbt_inst[0].state;              /* outside any robot: the first block (0 if none) */
    if(!k) { rbt_log("no level memory for the giant robot; bot behaviour",0,pt);return; }
    rbt_log(has<0?"giant robot states allocated (map container unreadable)":"giant robot states allocated",k,pt);
}

/* The instance whose robot this entity is (its AI's record and pool entry), or -1. */
static int rbt_find(uint32_t ent)
{
    uint32_t a,rec;
    unsigned k;
    if(!rbt_lvl.n || !MAPMAKER_PTR_OK(ent,0x100u) || !MAPMAKER_PTR_OK(a=WORD_AT(ent+RBT_ENT_AI),RBT_ACTOR) ||
       WORD_AT(RBT_SCENE)!=100u || !rbt_on()) return -1;
    rec=WORD_AT(a+RBT_ACTOR_RECORD);
    for(k=0;k<rbt_lvl.n;k++) if(rbt_inst[k].actor==a && rbt_inst[k].record==rec) return (int)k;
    return -1;
}
/* The robot code with this robot's globals: its state pointer and statics
 * loaded, the original run, its statics kept, the previous values put back
 * (nests: a robot's code reaching another robot's handler swaps again). */
static void rbt_swapped(void (*original)(void),uint32_t ent)
{
    int k=rbt_find(ent),prev;
    uint32_t keep[1u+RBT_STATICS_N],i;
    if(k<0) { original();return; }
    keep[0]=WORD_AT(RBT_STATE);
    for(i=0;i<RBT_STATICS_N;i++) { keep[1u+i]=WORD_AT(RBT_STATICS+i*4u);WORD_AT(RBT_STATICS+i*4u)=rbt_inst[k].statics[i]; }
    WORD_AT(RBT_STATE)=rbt_inst[k].state;
    prev=rbt_lvl.cur;rbt_lvl.cur=k;
    original();
    rbt_lvl.cur=prev;
    for(i=0;i<RBT_STATICS_N;i++) { rbt_inst[k].statics[i]=WORD_AT(RBT_STATICS+i*4u);WORD_AT(RBT_STATICS+i*4u)=keep[1u+i]; }
    WORD_AT(RBT_STATE)=keep[0];
    rbt_lvl.swaps++;
}
/* WRAP sub_00293730 (cdecl entity; the robot's AI, dispatcher 0x0027DE18). */
void pc_mapmaker_robot_handler(void (*original)(void))
{
    if(!rbt_lvl.n || !MAPMAKER_STACK_OK(8)) { original();return; }
    rbt_swapped(original,WORD_AT(g_esp+4u));
}
/* WRAP sub_00294520 (cdecl entity, message, ..; the robot's messages, 0x0027D447). */
void pc_mapmaker_robot_message(void (*original)(void))
{
    if(!rbt_lvl.n || !MAPMAKER_STACK_OK(8)) { original();return; }
    rbt_swapped(original,WORD_AT(g_esp+4u));
}
/* WRAP sub_002926B0 (cdecl entity; the callback the door entry installs, slot 2). */
void pc_mapmaker_robot_callback(void (*original)(void))
{
    if(!rbt_lvl.n || !MAPMAKER_STACK_OK(8)) { original();return; }
    rbt_swapped(original,WORD_AT(g_esp+4u));
}
/* WRAP sub_002936C0 (cdecl hit record, entity at +0; the robot's damage callback). */
void pc_mapmaker_robot_damage(void (*original)(void))
{
    uint32_t r;
    if(!rbt_lvl.n || !MAPMAKER_STACK_OK(8) || !MAPMAKER_PTR_OK(r=WORD_AT(g_esp+4u),4u)) { original();return; }
    rbt_swapped(original,WORD_AT(r));
}

/* WRAP sub_0015B830 (cdecl AI pool entry): give each robot its class and its own state. */
void pc_mapmaker_robot_ai_start(void (*original)(void))
{
    uint32_t a,ent,rec,id;
    unsigned k;
    if(!MAPMAKER_STACK_OK(8) || !rbt_play()) { original();return; }
    a=WORD_AT(g_esp+4u);
    if(!MAPMAKER_PTR_OK(a,RBT_ACTOR) || !MAPMAKER_PTR_OK(rec=WORD_AT(a+RBT_ACTOR_RECORD),12u)) { original();return; }
    ent=WORD_AT(a);
    if(RBT_H(a+RBT_ACTOR_CHAR)!=RBT_ID && !(MAPMAKER_PTR_OK(ent,0x100u) && RBT_H(ent+RBT_ENT_MODEL)==RBT_MODEL)) { original();return; }
    for(k=0;k<rbt_lvl.n && rbt_inst[k].record!=rec;k++) {}
    if(rbt.optout) rbt_log("TSFP_MAPMAKER_REAL_BOSS=0: giant robot with bot behaviour",a,0);
    else if(!rbt_pak_mounted()) rbt_log("lv14.pak not mounted: giant robot with bot behaviour",a,0);
    else if(!rbt_lvl.blocks) rbt_log("no giant robot state this level: bot behaviour",a,WORD_AT(RBT_STATE));
    else if(k==rbt_lvl.n && rbt_lvl.n>=rbt_lvl.blocks) {
        if(rbt_lvl.extras++<4u) rbt_log("more giant robots than states (4 per map): this one keeps the bot behaviour",a,WORD_AT(rec+8u));
    } else {
        id=WORD_AT(rec+8u);
        WORD_AT(rec)=0;WORD_AT(rec+4u)=RBT_CLASS;
        if(k==rbt_lvl.n) rbt_lvl.n++;                  /* a new robot; the same record again keeps its state */
        rbt_inst[k].record=rec;rbt_inst[k].actor=a;rbt_inst[k].removed=0;
        memset(rbt_inst[k].statics,0,sizeof rbt_inst[k].statics);
        rbt_log("giant robot gets its story AI (class 0x80000) and state",a,(uint32_t)k<<16|(id&0xFFFFu));
    }
    original();
}

/* WRAP sub_0009CC40 (cdecl pad id -> pad index or -1): no hangar pads. */
void pc_mapmaker_robot_nav(void (*original)(void))
{
    uint32_t ret;
    unsigned k;
    if(!rbt_lvl.n || !MAPMAKER_STACK_OK(8)) { original();return; }
    ret=WORD_AT(g_esp);
    for(k=0;k<sizeof rbt_nav_rets/sizeof rbt_nav_rets[0] && rbt_nav_rets[k]!=ret;k++) {}
    if(k==sizeof rbt_nav_rets/sizeof rbt_nav_rets[0] || WORD_AT(RBT_SCENE)!=100u || !rbt_on()) { original();return; }
    if(!rbt_lvl.navs++) rbt_log("giant robot: hangar pad ids not used; it walks to the player's pad",WORD_AT(g_esp+4u),ret);
    g_eax=0xFFFFFFFFu;
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_000462B0 (cdecl track): the death's music change is dropped. */
void pc_mapmaker_robot_music(void (*original)(void))
{
    if(!rbt_lvl.n || !MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=RBT_RET_MUSIC || WORD_AT(RBT_SCENE)!=100u || !rbt_on()) { original();return; }
    if(!rbt_lvl.music++) rbt_log("giant robot death: music change dropped (the map keeps its track)",WORD_AT(g_esp+4u),0);
    MAPMAKER_GUEST_RET();
}

/* This robot's death animation has ended: remove it and drop a story focus on it. */
static void rbt_remove(unsigned k)
{
    uint32_t a=rbt_inst[k].actor,ent,player=WORD_AT(RBT_PLAYER);
    if(rbt_inst[k].removed) return;
    rbt_inst[k].removed=1;
    if(!MAPMAKER_PTR_OK(a,RBT_ACTOR) || WORD_AT(a+RBT_ACTOR_RECORD)!=rbt_inst[k].record ||
       !MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u) || WORD_AT(ent+RBT_ENT_AI)!=a) return;
    if(MAPMAKER_PTR_OK(player,RBT_FOCUS+4u) && WORD_AT(player+RBT_FOCUS)==ent) WORD_AT(player+RBT_FOCUS)=0;
    guest_call(sub_0015DDA0,1,ent);
    rbt_log("giant robot defeated: removed",a,k);
}

/* WRAP sub_002274C0 (cdecl event, arg): a robot's level event 2 is dropped, that robot removed. */
void pc_mapmaker_robot_event(void (*original)(void))
{
    if(!rbt_lvl.n || !MAPMAKER_STACK_OK(12) || WORD_AT(g_esp)!=RBT_RET_EVENT || WORD_AT(RBT_SCENE)!=100u || !rbt_on()) { original();return; }
    if(rbt_lvl.events++<4u) rbt_log("giant robot level event dropped (MapMaker logic marker untouched)",WORD_AT(g_esp+4u),(uint32_t)rbt_lvl.cur);
    if(rbt_lvl.cur>=0) rbt_remove((unsigned)rbt_lvl.cur);
    MAPMAKER_GUEST_RET();
}

/* A robot of this level is dying (its own state +0x48 == 1) and still there. */
static int rbt_any_dying(void)
{
    unsigned k;
    for(k=0;k<rbt_lvl.n;k++)
        if(!rbt_inst[k].removed && MAPMAKER_PTR_OK(rbt_inst[k].state,RBT_STATE_SIZE) && WORD_AT(rbt_inst[k].state+RBT_DYING)==1u) return 1;
    return 0;
}
/* WRAP sub_00126560 (frame step; return 0x001CEE8C): the death explosions
 * (one shared particle block: drawn once while any robot is dying). */
void pc_mapmaker_robot_fx(void (*original)(void))
{
    uint32_t cam;
    int here=rbt_lvl.n && MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==RBT_RET_FX;
    original();
    if(!here || WORD_AT(RBT_SCENE)!=100u || !rbt_on() || !rbt_any_dying()) return;
    if(!MAPMAKER_PTR_OK(WORD_AT(RBT_PARTS),RBT_PARTS_SIZE) || !MAPMAKER_PTR_OK(cam=WORD_AT(RBT_CAMERA),RBT_CAMERA_VIEW+4u)) return;
    if(RBT_B(RBT_MODE)==10u && (int32_t)WORD_AT(RBT_PLAYERS)>1) return;   /* as 0x00292676: one player only */
    guest_call(sub_00019D70,1,1u);
    guest_call(sub_002A7F30,2,WORD_AT(cam+RBT_CAMERA_VIEW),RBT_FX_SCALE);
    if(!rbt_lvl.fx++) rbt_log("giant robot explosions drawn",rbt_lvl.n,WORD_AT(RBT_PARTS));
}

/* WRAP sub_0025F6A0 (story soft restart; scene 14 resets the robot state):
 * every robot's own state reset (sub_00292550 on its block), statics cleared. */
void pc_mapmaker_robot_reset(void (*original)(void))
{
    uint32_t keep;
    unsigned k;
    original();
    if(!rbt_lvl.blocks || WORD_AT(RBT_SCENE)!=100u || !rbt_on()) return;
    keep=WORD_AT(RBT_STATE);
    for(k=0;k<rbt_lvl.blocks;k++) {
        WORD_AT(RBT_STATE)=rbt_inst[k].state;
        guest_call(sub_00292550,0);
        memset(rbt_inst[k].statics,0,sizeof rbt_inst[k].statics);
        rbt_inst[k].removed=0;
    }
    WORD_AT(RBT_STATE)=keep;
    if(rbt_lvl.resets++<2u) rbt_log("giant robot states reset for the restart",rbt_lvl.blocks,rbt_lvl.n);
}

/* WRAP sub_0014CC80 (story boss bar; chain INSIDE pc_mapmaker_bosses_hud). */
void pc_mapmaker_robot_hud(void (*original)(void))
{
    uint32_t player,focus,a,rec;
    unsigned k;
    if(!rbt_lvl.n) { original();return; }            /* cheap: no robot this level */
    player=WORD_AT(RBT_PLAYER);
    if(!MAPMAKER_PTR_OK(player,RBT_FOCUS+4u) || !MAPMAKER_PTR_OK(focus=WORD_AT(player+RBT_FOCUS),0x100u) ||
       RBT_H(focus+RBT_ENT_MODEL)!=RBT_MODEL || !MAPMAKER_PTR_OK(a=WORD_AT(focus+RBT_ENT_AI),RBT_ACTOR) || !rbt_play()) { original();return; }
    rec=WORD_AT(a+RBT_ACTOR_RECORD);
    for(k=0;k<rbt_lvl.n && rbt_inst[k].record!=rec;k++) {}
    if(k==rbt_lvl.n) { original();return; }
    WORD_AT(player+RBT_FOCUS)=0;                     /* never the stock bar: Boss bar = the bosses area's */
    original();
    WORD_AT(player+RBT_FOCUS)=focus;
}
