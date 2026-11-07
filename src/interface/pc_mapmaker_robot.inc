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
 *  - goal (sub_0015D500(entity, pad index), the walk goal the handler sets
 *    at 0x00293D22 every walk frame): v0.4.0-mapmaker.40 "giant robots
 *    follow the player through walls ... as long as there is some opening":
 *    the stock AI walks the human-sized pad graph, whose links cross every
 *    doorway. The robot gets its own reachable pad set instead: a link
 *    between pads of two rooms (pad +0xA) crosses the portal joining them
 *    (nearest one to the link), and the robot passes it only when that
 *    opening (portal vertex y extent) is at least its height (entity box top
 *    +0x1AC + +0x1C4, 6.0 when unusable). A player pad outside the set is
 *    replaced by the reachable pad nearest to it (the robot waits at the
 *    opening and fights from there); when its set has blocked links, the goal
 *    is the next pad along a path of passable links, one link at a time
 *    (v0.4.0-mapmaker.42 "still walked through a one-floor opening": a goal
 *    farther away let the stock route, sub_001596F0, cut through a low door
 *    even within the robot's own room).
 *    Rooms that are unknown, or joined by no portal (teleporters), stay
 *    passable (stock).
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
#define RBT_RET_GOAL 0x00293D27u        /* sub_00293730: call sub_0015D500(entity, [actor+0x684]) at 0x00293D22 */
#define RBT_PADS 0x00715F2Cu            /* pad table: 0x20 each; +4 bit 0 used, +0xA int16 room, +0xC/+0x10/+0x14 x/y/z */
#define RBT_PAD_COUNT 0x007B6664u
#define RBT_PAD_SIZE 0x20u
#define RBT_PAD_MAX 8192u               /* the scene-100 regular pad pool holds 8000 */
#define RBT_LINKS 0x00715F30u           /* link pool: 0x14 each; +0 flags (0x10000 used), +4/+8 pad indices */
#define RBT_LINK_COUNT 0x007B6660u
#define RBT_LINK_SIZE 0x14u
#define RBT_LINK_MAX 32768u
#define RBT_LINK_USED 0x10000u
#define RBT_ROOM_COUNT 0x007B975Cu      /* rooms 1..n */
#define RBT_ROOM_RECS 0x00708788u       /* room r: + r*0xB4, +4 -> {count, portal ids} */
#define RBT_ROOM_REC 0xB4u
#define RBT_PORTALS 0x0070878Cu         /* portal id -> struct: +0/+4 rooms, +0x14 int16 vertices, +0x18 x/y/z each 0xC */
#define RBT_PORTAL_COUNT 0x007B9754u
#define RBT_PORTALS_PER_ROOM 64u
#define RBT_PORTAL_VERTS 32u
#define RBT_AI_PAD 0x678u               /* actor: the pad it is at */
#define RBT_AI_GOAL 0x684u              /* actor: the walk goal */
#define RBT_ENT_BOX_Y 0x1ACu            /* entity: box centre y above the position, +0x1C4 its half height */
#define RBT_ENT_BOX_H 0x1C4u
#define RBT_HEIGHT 6.0f                 /* INFERRED: used when the box is not usable */
#define RBT_FIT_SLACK 0.25f             /* an opening this much lower than the robot still lets it through */
#define RBT_TELL_MAX 32u                /* portal decision lines per level */
#define RBT_OPEN 1.0e9f                 /* a link inside one room, or not across a portal */
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
/* The pad graph as the robot sees it (built once per level): each pad's
 * links (CSR) and the opening each link crosses. Per robot: its height,
 * the pads it can reach, the substitute for an unreachable goal and the
 * hop distances to its goal. */
static struct { uint32_t pads, links; unsigned npads, nlinks, built, crossings, told; uint32_t *off, *adj; float *open; } rbt_nav;
static struct {
    float height;
    uint32_t from, goal_for, goal_sub, dist_for;
    unsigned blocked, reached, subs, steers, logged;
    int built, dist_ok;
    uint8_t reach[RBT_PAD_MAX];
    uint16_t dist[RBT_PAD_MAX];
} rbt_walk[RBT_MAX];
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

static float rbt_getf(uint32_t a) { float f;memcpy(&f,(const void *)((uintptr_t)a+g_xbox_mem_offset),4);return f; }
static int rbt_sane(float f) { return f==f && f>-100000.0f && f<100000.0f; }
static void rbt_nav_free(void)
{
    free(rbt_nav.off);free(rbt_nav.adj);free(rbt_nav.open);
    memset(&rbt_nav,0,sizeof rbt_nav);
}
static int16_t rbt_room(uint32_t pad) { return (int16_t)RBT_H(WORD_AT(RBT_PADS)+pad*RBT_PAD_SIZE+0xAu); }
static float rbt_padf(uint32_t pad,uint32_t off) { return rbt_getf(WORD_AT(RBT_PADS)+pad*RBT_PAD_SIZE+off); }
/* The opening a link between pads a and b crosses: the y extent of the
 * portal joining their rooms nearest to the link's midpoint; RBT_OPEN when
 * both are in one room, a room is unknown or no portal joins them. */
static float rbt_opening(uint32_t a,uint32_t b)
{
    int ra=rbt_room(a),rb=rbt_room(b);
    uint32_t recs=WORD_AT(RBT_ROOM_RECS),portals=WORD_AT(RBT_PORTALS),pmax=WORD_AT(RBT_PORTAL_COUNT),list,count,i,v,id,pt;
    float mx=(rbt_padf(a,0xCu)+rbt_padf(b,0xCu))*0.5f,mz=(rbt_padf(a,0x14u)+rbt_padf(b,0x14u))*0.5f,best=-1.0f,open=RBT_OPEN;
    if(ra==rb || ra<=0 || rb<=0 || (uint32_t)ra>WORD_AT(RBT_ROOM_COUNT) || (uint32_t)rb>WORD_AT(RBT_ROOM_COUNT)) return RBT_OPEN;
    if(!MAPMAKER_PTR_OK(recs,((uint32_t)ra+1u)*RBT_ROOM_REC) || !MAPMAKER_PTR_OK(portals,(pmax+1u)*4u) ||
       !MAPMAKER_PTR_OK(list=WORD_AT(recs+(uint32_t)ra*RBT_ROOM_REC+4u),4u)) return RBT_OPEN;
    count=WORD_AT(list);
    if(count>RBT_PORTALS_PER_ROOM || !MAPMAKER_PTR_OK(list,4u+count*4u)) return RBT_OPEN;
    for(i=0;i<count;i++) {
        unsigned nv;
        float lo=1.0e9f,hi=-1.0e9f,cx=0.0f,cz=0.0f,d;
        if((id=WORD_AT(list+4u+i*4u))>pmax || !MAPMAKER_PTR_OK(pt=WORD_AT(portals+id*4u),0x18u)) continue;
        if(!((WORD_AT(pt)==(uint32_t)ra && WORD_AT(pt+4u)==(uint32_t)rb) || (WORD_AT(pt)==(uint32_t)rb && WORD_AT(pt+4u)==(uint32_t)ra))) continue;
        nv=RBT_H(pt+0x14u);
        if(!nv || nv>RBT_PORTAL_VERTS || !MAPMAKER_PTR_OK(pt+0x18u,nv*0xCu)) continue;
        for(v=0;v<nv;v++) {
            float x=rbt_getf(pt+0x18u+v*0xCu),y=rbt_getf(pt+0x1Cu+v*0xCu),z=rbt_getf(pt+0x20u+v*0xCu);
            if(y<lo) lo=y;
            if(y>hi) hi=y;
            cx+=x;cz+=z;
        }
        cx/=(float)nv;cz/=(float)nv;d=(cx-mx)*(cx-mx)+(cz-mz)*(cz-mz);
        if(!rbt_sane(lo) || !rbt_sane(hi) || !rbt_sane(d)) continue;
        if(best<0.0f || d<best) { best=d;open=hi-lo; }
    }
    return open;
}
/* The pads' links as lists per pad, with the opening of each (once per level). */
static int rbt_nav_build(void)
{
    uint32_t pads=WORD_AT(RBT_PADS),links=WORD_AT(RBT_LINKS),np=WORD_AT(RBT_PAD_COUNT),nl=WORD_AT(RBT_LINK_COUNT),k,a,b,*fill;
    if(rbt_nav.built) return rbt_nav.pads==pads && rbt_nav.links==links && rbt_nav.npads==np && rbt_nav.nlinks==nl && rbt_nav.off;
    rbt_nav.built=1;rbt_nav.pads=pads;rbt_nav.links=links;rbt_nav.npads=np;rbt_nav.nlinks=nl;
    if(!np || np>RBT_PAD_MAX || nl>RBT_LINK_MAX || !MAPMAKER_PTR_OK(pads,np*RBT_PAD_SIZE) || (nl && !MAPMAKER_PTR_OK(links,nl*RBT_LINK_SIZE))) return 0;
    rbt_nav.off=(uint32_t *)calloc(np+1u,sizeof(uint32_t));
    rbt_nav.adj=(uint32_t *)malloc((2u*nl+1u)*sizeof(uint32_t));
    rbt_nav.open=(float *)malloc((2u*nl+1u)*sizeof(float));
    fill=(uint32_t *)calloc(np+1u,sizeof(uint32_t));
    if(!rbt_nav.off || !rbt_nav.adj || !rbt_nav.open || !fill) { free(fill);free(rbt_nav.off);free(rbt_nav.adj);free(rbt_nav.open);rbt_nav.off=0;rbt_nav.adj=0;rbt_nav.open=0;return 0; }
    for(k=0;k<nl;k++) {
        uint32_t l=links+k*RBT_LINK_SIZE;
        if(!(WORD_AT(l)&RBT_LINK_USED) || (a=WORD_AT(l+4u))>=np || (b=WORD_AT(l+8u))>=np || a==b) continue;
        rbt_nav.off[a+1u]++;rbt_nav.off[b+1u]++;
    }
    for(k=0;k<np;k++) rbt_nav.off[k+1u]+=rbt_nav.off[k];
    for(k=0;k<nl;k++) {
        uint32_t l=links+k*RBT_LINK_SIZE,ia,ib;
        float o;
        if(!(WORD_AT(l)&RBT_LINK_USED) || (a=WORD_AT(l+4u))>=np || (b=WORD_AT(l+8u))>=np || a==b) continue;
        o=rbt_opening(a,b);
        if(o<RBT_OPEN) rbt_nav.crossings++;
        ia=rbt_nav.off[a]+fill[a]++;ib=rbt_nav.off[b]+fill[b]++;
        rbt_nav.adj[ia]=b;rbt_nav.open[ia]=o;rbt_nav.adj[ib]=a;rbt_nav.open[ib]=o;
    }
    free(fill);
    return 1;
}
/* Breadth-first over the links robot k fits through: from pad 'from' into
 * reach[] (mark 1), or the hop distance to 'to' into dist[] (mode 2). */
static unsigned rbt_bfs(unsigned k,uint32_t start,int dist)
{
    static uint16_t queue[RBT_PAD_MAX];
    unsigned head=0,tail=0,n=0;
    uint32_t p,e;
    float need=rbt_walk[k].height-RBT_FIT_SLACK;
    if(dist) { memset(rbt_walk[k].dist,0xFF,sizeof rbt_walk[k].dist);rbt_walk[k].dist[start]=0; }
    else { memset(rbt_walk[k].reach,0,sizeof rbt_walk[k].reach);rbt_walk[k].reach[start]=1u;rbt_walk[k].blocked=0; }
    queue[tail++]=(uint16_t)start;
    while(head<tail) {
        p=queue[head++];n++;
        for(e=rbt_nav.off[p];e<rbt_nav.off[p+1u];e++) {
            uint32_t q=rbt_nav.adj[e];
            if(rbt_nav.open[e]<need) { if(!dist) rbt_walk[k].blocked++;continue; }
            if(dist) { if(rbt_walk[k].dist[q]!=0xFFFFu) continue;rbt_walk[k].dist[q]=(uint16_t)(rbt_walk[k].dist[p]+1u); }
            else { if(rbt_walk[k].reach[q]) continue;rbt_walk[k].reach[q]=1u; }
            queue[tail++]=(uint16_t)q;
        }
    }
    return n;
}
/* The robot's height: the top of its entity box above its position. */
static float rbt_height(uint32_t ent)
{
    float h=rbt_getf(ent+RBT_ENT_BOX_Y)+rbt_getf(ent+RBT_ENT_BOX_H);
    return rbt_sane(h) && h>=1.0f && h<=40.0f?h:RBT_HEIGHT;
}
/* Once per level: the height used and the decision for each link crossing
 * a portal (at most RBT_TELL_MAX lines). */
static void rbt_tell(unsigned k)
{
    uint32_t p,e;
    unsigned n=0;
    float need=rbt_walk[k].height-RBT_FIT_SLACK;
    if(rbt_nav.told) return;
    rbt_nav.told=1;
    fprintf(stderr,"[mapmaker] robot: giant robot height %.2f (entity box top; openings of at least %.2f let it through), %u links cross a portal\n",
            (double)rbt_walk[k].height,(double)need,rbt_nav.crossings);
    for(p=0;p<rbt_nav.npads;p++)
        for(e=rbt_nav.off[p];e<rbt_nav.off[p+1u];e++) {
            if(rbt_nav.adj[e]<=p || rbt_nav.open[e]>=RBT_OPEN) continue;
            if(n++>=RBT_TELL_MAX) continue;
            fprintf(stderr,"[mapmaker] robot: link pad %u (room %d) - pad %u (room %d): opening %.2f -> %s\n",(unsigned)p,(int)rbt_room(p),
                    (unsigned)rbt_nav.adj[e],(int)rbt_room(rbt_nav.adj[e]),(double)rbt_nav.open[e],rbt_nav.open[e]<need?"closed":"open");
        }
    if(n>RBT_TELL_MAX) fprintf(stderr,"[mapmaker] robot: %u more portal links not listed\n",n-RBT_TELL_MAX);
    fflush(stderr);
}
/* The goal robot k walks to instead of pad 'goal' (the same pad when it may). */
static uint32_t rbt_goal_for(unsigned k,uint32_t ent,uint32_t at,uint32_t goal)
{
    uint32_t np=rbt_nav.npads,p,best,e;
    float gx,gy,gz,bd=-1.0f;
    if(!rbt_walk[k].height) rbt_walk[k].height=rbt_height(ent);
    if(!rbt_walk[k].built || (rbt_walk[k].from!=at && !rbt_walk[k].reach[at])) {
        rbt_walk[k].built=1;rbt_walk[k].from=at;rbt_walk[k].dist_ok=0;rbt_walk[k].goal_for=0xFFFFFFFFu;
        rbt_walk[k].reached=rbt_bfs(k,at,0);
        rbt_tell(k);
        if(rbt_walk[k].logged++<4u) {
            fprintf(stderr,"[mapmaker] robot: giant robot %u (height %.2f) reaches %u of %u pads from pad %u; %u links through openings lower than it blocked (%u room crossings)\n",
                    k,(double)rbt_walk[k].height,rbt_walk[k].reached,np,(unsigned)at,rbt_walk[k].blocked/2u+rbt_walk[k].blocked%2u,rbt_nav.crossings);
            fflush(stderr);
        }
    }
    if(!rbt_walk[k].reach[goal]) {                     /* unreachable: the reachable pad nearest to it */
        if(rbt_walk[k].goal_for!=goal) {
            gx=rbt_padf(goal,0xCu);gy=rbt_padf(goal,0x10u);gz=rbt_padf(goal,0x14u);best=at;
            for(p=0;p<np;p++) {
                float dx,dy,dz,d;
                if(!rbt_walk[k].reach[p] || !(RBT_B(WORD_AT(RBT_PADS)+p*RBT_PAD_SIZE+4u)&1u)) continue;
                dx=rbt_padf(p,0xCu)-gx;dy=rbt_padf(p,0x10u)-gy;dz=rbt_padf(p,0x14u)-gz;d=dx*dx+dy*dy+dz*dz;
                if(rbt_sane(d) && (bd<0.0f || d<bd)) { bd=d;best=p; }
            }
            rbt_walk[k].goal_for=goal;rbt_walk[k].goal_sub=best;
            if(rbt_walk[k].subs++<4u) rbt_log("giant robot cannot fit to the player's pad: it waits at the nearest pad it reaches (goal, pad)",goal,best);
        }
        goal=rbt_walk[k].goal_sub;
    }
    if(!rbt_walk[k].blocked || goal==at) return goal;  /* every link fits: the stock route is fine */
    if(!rbt_walk[k].dist_ok || rbt_walk[k].dist_for!=goal) { rbt_bfs(k,goal,1);rbt_walk[k].dist_ok=1;rbt_walk[k].dist_for=goal; }
    if(rbt_walk[k].dist[at]==0xFFFFu) return at;      /* no way it fits: it stays */
    /* One link at a time: the stock route to any farther pad (sub_001596F0,
     * group then pad A* over every link, recomputed each frame) may cut
     * through an opening lower than the robot, even inside one room. */
    for(e=rbt_nav.off[at];e<rbt_nav.off[at+1u];e++)
        if(rbt_nav.open[e]>=rbt_walk[k].height-RBT_FIT_SLACK && rbt_walk[k].dist[rbt_nav.adj[e]]+1u==rbt_walk[k].dist[at]) return rbt_nav.adj[e];
    return at;
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
    rbt_nav_free();
    memset(rbt_walk,0,sizeof rbt_walk);
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

/* WRAP sub_0015D500 (cdecl entity, pad index; the walk goal, call 0x00293D22
 * in the robot's handler): a goal the robot does not fit to is replaced
 * (actor +0x684 too, so the stock goal compare sees it) for the call. */
void pc_mapmaker_robot_goal(void (*original)(void))
{
    uint32_t sp,ent,a,at,goal,to;
    int k;
    if(!rbt_lvl.n || !MAPMAKER_STACK_OK(12) || WORD_AT(g_esp)!=RBT_RET_GOAL) { original();return; }
    sp=g_esp;ent=WORD_AT(sp+4u);goal=WORD_AT(sp+8u);
    if((k=rbt_find(ent))<0 || !rbt_nav_build() || !MAPMAKER_PTR_OK(a=WORD_AT(ent+RBT_ENT_AI),RBT_ACTOR) ||
       (at=WORD_AT(a+RBT_AI_PAD))>=rbt_nav.npads || goal>=rbt_nav.npads) { original();return; }
    to=rbt_goal_for((unsigned)k,ent,at,goal);
    if(to==goal) { original();return; }
    if(rbt_walk[k].steers++<4u && rbt_walk[k].reach[goal]) rbt_log("giant robot steps one link it fits through toward its goal (goal, next pad)",goal,to);
    WORD_AT(sp+8u)=to;WORD_AT(a+RBT_AI_GOAL)=to;
    original();
    WORD_AT(sp+8u)=goal;
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
