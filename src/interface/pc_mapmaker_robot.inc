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
 *    passable (stock). v0.4.0-mapmaker.44: each MapMaker tile side is a
 *    portal one floor (5) tall, so a tall open area is a column of portals:
 *    the opening combines the portals stacked above the doorway in its plane
 *    while the floors between are open (rbt_stacked); a floor or ceiling
 *    portal is no doorway (open).
 *  - hold (in the handler wrapper, before the original): the robot is moved
 *    by its animation's root motion in the dispatcher (sub_0015F0D0, call
 *    0x0027DE12), not by its pads, and its attack states walk on with no
 *    goal; when its room (+0x34) changed through an opening lower than it,
 *    its previous position and room are put back.
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
#include <math.h>
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
#define RBT_PORTAL_MAX 16384u           /* stacked openings are combined up to this many portals */
#define RBT_WALL_MIN 1.0f               /* a portal less tall than this is a floor/ceiling opening: no doorway */
#define RBT_STACK_MAX 8u                /* portals stacked above one doorway, at most */
#define RBT_STACK_GAP 0.3f              /* a stacked portal starts this close to the top of the one below */
#define RBT_STACK_PLANE 0.3f            /* ... and lies this close to its vertical plane */
#define RBT_ENT_POS 0x64u               /* entity: x/y/z (sub_0015F0D0 0x0015F861..0x0015F871) */
#define RBT_ENT_ROOM 0x34u              /* entity: room, set from the position by sub_0015AB80 (0x0015F889) */
#define RBT_HOLD_JUMP 6.0f              /* a move farther than this in one frame is a teleport/restart: kept */
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
static struct { uint32_t pads, links; unsigned npads, nlinks, built, crossings, told, stacked; uint32_t *off, *adj; float *open, *stack; } rbt_nav;
static struct {
    float height, hold[3];
    uint32_t from, goal_for, goal_sub, dist_for, hold_room;
    unsigned blocked, reached, subs, steers, logged, holds;
    int built, dist_ok, hold_ok;
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
    free(rbt_nav.off);free(rbt_nav.adj);free(rbt_nav.open);free(rbt_nav.stack);
    memset(&rbt_nav,0,sizeof rbt_nav);
}
static int16_t rbt_room(uint32_t pad) { return (int16_t)RBT_H(WORD_AT(RBT_PADS)+pad*RBT_PAD_SIZE+0xAu); }
static float rbt_padf(uint32_t pad,uint32_t off) { return rbt_getf(WORD_AT(RBT_PADS)+pad*RBT_PAD_SIZE+off); }
static int rbt_room_ok(uint32_t r) { return r>=1u && r<=WORD_AT(RBT_ROOM_COUNT); }
/* Room r's portal id list (count, then ids), or 0. */
static uint32_t rbt_room_list(uint32_t r,uint32_t *count)
{
    uint32_t recs=WORD_AT(RBT_ROOM_RECS),list;
    if(!rbt_room_ok(r) || !MAPMAKER_PTR_OK(recs,(r+1u)*RBT_ROOM_REC) || !MAPMAKER_PTR_OK(list=WORD_AT(recs+r*RBT_ROOM_REC+4u),4u)) return 0;
    *count=WORD_AT(list);
    return *count<=RBT_PORTALS_PER_ROOM && MAPMAKER_PTR_OK(list,4u+*count*4u)?list:0;
}
/* A portal: its rooms, y extent, horizontal centre and its widest horizontal
 * segment (a..b, the doorway's span in a wall). */
typedef struct { uint32_t r0, r1, pt; unsigned nv; float lo, hi, cx, cz, ax, az, bx, bz; } rbt_portal_t;
static int rbt_portal(uint32_t id,rbt_portal_t *p)
{
    uint32_t portals=WORD_AT(RBT_PORTALS),pmax=WORD_AT(RBT_PORTAL_COUNT),v,w;
    float far=-1.0f;
    if(id>pmax || !MAPMAKER_PTR_OK(portals,(pmax+1u)*4u) || !MAPMAKER_PTR_OK(p->pt=WORD_AT(portals+id*4u),0x18u)) return 0;
    p->r0=WORD_AT(p->pt);p->r1=WORD_AT(p->pt+4u);p->nv=RBT_H(p->pt+0x14u);
    if(!p->nv || p->nv>RBT_PORTAL_VERTS || !MAPMAKER_PTR_OK(p->pt+0x18u,p->nv*0xCu)) return 0;
    p->lo=1.0e9f;p->hi=-1.0e9f;p->cx=p->cz=0.0f;
    for(v=0;v<p->nv;v++) {
        float x=rbt_getf(p->pt+0x18u+v*0xCu),y=rbt_getf(p->pt+0x1Cu+v*0xCu),z=rbt_getf(p->pt+0x20u+v*0xCu);
        if(!rbt_sane(x) || !rbt_sane(y) || !rbt_sane(z)) return 0;
        if(y<p->lo) p->lo=y;
        if(y>p->hi) p->hi=y;
        p->cx+=x;p->cz+=z;
        for(w=0;w<v;w++) {
            float dx=x-rbt_getf(p->pt+0x18u+w*0xCu),dz=z-rbt_getf(p->pt+0x20u+w*0xCu);
            if(dx*dx+dz*dz>far) { far=dx*dx+dz*dz;p->ax=rbt_getf(p->pt+0x18u+w*0xCu);p->az=rbt_getf(p->pt+0x20u+w*0xCu);p->bx=x;p->bz=z; }
        }
    }
    if(far<0.0f) { p->ax=p->bx=p->cx;p->az=p->bz=p->cz; }
    p->cx/=(float)p->nv;p->cz/=(float)p->nv;
    return 1;
}
/* Rooms x and y are one room or joined by an open floor (a portal less tall
 * than RBT_WALL_MIN: stacked tiles; a doorway between them does not count). */
static int rbt_joined(uint32_t x,uint32_t y)
{
    uint32_t list,count,i;
    rbt_portal_t q;
    if(x==y) return 1;
    if(!(list=rbt_room_list(x,&count))) return 0;
    for(i=0;i<count;i++)
        if(rbt_portal(WORD_AT(list+4u+i*4u),&q) && q.hi-q.lo<RBT_WALL_MIN && ((q.r0==x && q.r1==y) || (q.r0==y && q.r1==x))) return 1;
    return 0;
}
/* Portal q lies in p's vertical plane and shares a good part of its span. */
static int rbt_coplanar(const rbt_portal_t *p,const rbt_portal_t *q)
{
    float dx=p->bx-p->ax,dz=p->bz-p->az,len=(float)sqrt((double)(dx*dx+dz*dz)),lo=1.0e9f,hi=-1.0e9f,over;
    unsigned v;
    if(!(len>=0.1f)) return 0;
    dx/=len;dz/=len;
    for(v=0;v<q->nv;v++) {
        float ux=rbt_getf(q->pt+0x18u+v*0xCu)-p->ax,uz=rbt_getf(q->pt+0x20u+v*0xCu)-p->az,t=ux*dx+uz*dz,off=uz*dx-ux*dz;
        if(off>RBT_STACK_PLANE || off<-RBT_STACK_PLANE) return 0;
        if(t<lo) lo=t;
        if(t>hi) hi=t;
    }
    over=(hi<len?hi:len)-(lo>0.0f?lo:0.0f);
    return over>=0.1f && over>=0.5f*((hi-lo)<len?(hi-lo):len);
}
/* v0.4.0-mapmaker.44: the height of the opening above portal id. MapMaker
 * stacks tiles of one floor (5 units), so each tile side is its own portal of
 * one floor, and a tall open area is a column of them. A portal starting at
 * the top of this one (RBT_STACK_GAP), in its plane (RBT_STACK_PLANE), over
 * the same span, whose rooms are the rooms above this one's (one room, or an
 * open floor between: a floor portal joins them, rbt_joined), raises the
 * opening; a floor slab between two doors (no floor portal) ends it. A portal
 * less tall than RBT_WALL_MIN is an open floor or ceiling: RBT_OPEN. */
static float rbt_stacked(uint32_t id)
{
    uint32_t pmax=WORD_AT(RBT_PORTAL_COUNT),used[RBT_STACK_MAX+1u],a,b,q,n=1,i,ba=0,bb=0;
    rbt_portal_t p,c;
    float top,best;
    if(rbt_nav.stack && id<=pmax && rbt_nav.stack[id]>=0.0f) return rbt_nav.stack[id];
    if(!rbt_portal(id,&p)) return RBT_OPEN;
    if(p.hi-p.lo<RBT_WALL_MIN) return RBT_OPEN;
    top=p.hi;a=p.r0;b=p.r1;used[0]=id;
    while(pmax<=RBT_PORTAL_MAX && n<=RBT_STACK_MAX) {
        best=top;
        for(q=0;q<=pmax;q++) {
            for(i=0;i<n && used[i]!=q;i++) {}
            if(i<n || !rbt_portal(q,&c) || c.hi-c.lo<RBT_WALL_MIN || c.hi<=best) continue;
            if(c.lo>top+RBT_STACK_GAP || c.lo<top-RBT_STACK_GAP || !rbt_coplanar(&p,&c)) continue;
            if(rbt_joined(c.r0,a) && rbt_joined(c.r1,b)) { best=c.hi;ba=c.r0;bb=c.r1;used[n]=q; }
            else if(rbt_joined(c.r0,b) && rbt_joined(c.r1,a)) { best=c.hi;ba=c.r1;bb=c.r0;used[n]=q; }
        }
        if(best<=top) break;
        top=best;a=ba;b=bb;n++;
    }
    if(n>1u) rbt_nav.stacked++;
    if(rbt_nav.stack && id<=pmax) rbt_nav.stack[id]=top-p.lo;
    return top-p.lo;
}
/* The opening between rooms ra and rb near (mx, mz): the stacked height of
 * the portal joining them nearest to that point; RBT_OPEN when both are one
 * room, a room is unknown or no portal joins them. */
static float rbt_pair_open(uint32_t ra,uint32_t rb,float mx,float mz)
{
    uint32_t list,count,i,id,best_id=0;
    rbt_portal_t p;
    float best=-1.0f,best_lo=0.0f,d;
    if(ra==rb || !rbt_room_ok(ra) || !rbt_room_ok(rb) || !(list=rbt_room_list(ra,&count))) return RBT_OPEN;
    for(i=0;i<count;i++) {
        if(!rbt_portal(id=WORD_AT(list+4u+i*4u),&p)) continue;
        if(!((p.r0==ra && p.r1==rb) || (p.r0==rb && p.r1==ra))) continue;
        d=(p.cx-mx)*(p.cx-mx)+(p.cz-mz)*(p.cz-mz);
        if(!rbt_sane(d)) continue;
        /* nearest; of portals stacked over one spot (two tall rooms), the lowest */
        if(best<0.0f || d<best-0.01f || (d<=best+0.01f && p.lo<best_lo)) { best=d;best_id=id;best_lo=p.lo; }
    }
    return best<0.0f?RBT_OPEN:rbt_stacked(best_id);
}
/* The opening a link between pads a and b crosses (its midpoint). */
static float rbt_opening(uint32_t a,uint32_t b)
{
    int ra=rbt_room(a),rb=rbt_room(b);
    if(ra<=0 || rb<=0) return RBT_OPEN;
    return rbt_pair_open((uint32_t)ra,(uint32_t)rb,(rbt_padf(a,0xCu)+rbt_padf(b,0xCu))*0.5f,(rbt_padf(a,0x14u)+rbt_padf(b,0x14u))*0.5f);
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
    if(WORD_AT(RBT_PORTAL_COUNT)<=RBT_PORTAL_MAX && (rbt_nav.stack=(float *)malloc((WORD_AT(RBT_PORTAL_COUNT)+1u)*sizeof(float)))!=0)
        for(k=0;k<=WORD_AT(RBT_PORTAL_COUNT);k++) rbt_nav.stack[k]=-1.0f;   /* each portal's stacked opening, once */
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
    fprintf(stderr,"[mapmaker] robot: giant robot height %.2f (entity box top; openings of at least %.2f let it through), %u links cross a portal, %u doorways with open floors stacked above\n",
            (double)rbt_walk[k].height,(double)need,rbt_nav.crossings,rbt_nav.stacked);
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
/* v0.4.0-mapmaker.44 "still walks through openings one floor tall": the
 * robot is not moved by its pads. The dispatcher moves it just before this
 * handler (sub_0015F0D0, call 0x0027DE12): the current animation's root
 * motion ([entity+4] +0xFC..) turned by its facing (+0x70), a collision move
 * (sub_000D9230, 0x0015F850), the position stored (0x0015F861..0x0015F871)
 * and its room +0x34 set again from it (sub_0015AB80, 0x0015F889). Every
 * state from 2 to 5 keeps the walk cycle (0x808) playing, and the attack
 * states 3-5 (straight-line distance to the player, 0x002939E9..0x00293A3C)
 * call no walk goal, so it walks on through a doorway wherever it faces.
 * Each frame, before the handler: when its room changed through an opening
 * lower than it (rbt_pair_open nearest the move), the previous position and
 * room are put back. A move of more than RBT_HOLD_JUMP in one frame
 * (teleport, restart), a room joined by no portal or an unknown room is kept. */
static void rbt_hold(unsigned k,uint32_t ent)
{
    uint32_t room=WORD_AT(ent+RBT_ENT_ROOM),i;
    float pos[3],d=0.0f,open=0.0f;
    for(i=0;i<3u;i++) { pos[i]=rbt_getf(ent+RBT_ENT_POS+i*4u);if(!rbt_sane(pos[i])) return; }
    if(rbt_inst[k].removed || !rbt_room_ok(room)) return;
    if(rbt_walk[k].hold_ok && room!=rbt_walk[k].hold_room) {
        for(i=0;i<3u;i++) d+=(pos[i]-rbt_walk[k].hold[i])*(pos[i]-rbt_walk[k].hold[i]);
        if(!rbt_walk[k].height) rbt_walk[k].height=rbt_height(ent);
        if(d<=RBT_HOLD_JUMP*RBT_HOLD_JUMP &&
           (open=rbt_pair_open(rbt_walk[k].hold_room,room,(pos[0]+rbt_walk[k].hold[0])*0.5f,(pos[2]+rbt_walk[k].hold[2])*0.5f))<rbt_walk[k].height-RBT_FIT_SLACK) {
            memcpy((void *)((uintptr_t)(ent+RBT_ENT_POS)+g_xbox_mem_offset),rbt_walk[k].hold,sizeof rbt_walk[k].hold);
            WORD_AT(ent+RBT_ENT_ROOM)=rbt_walk[k].hold_room;
            if(rbt_walk[k].holds++<4u) {
                fprintf(stderr,"[mapmaker] robot: giant robot %u held in room %u: the opening to room %u is %.2f, lower than it (%.2f)\n",
                        k,(unsigned)rbt_walk[k].hold_room,(unsigned)room,(double)open,(double)rbt_walk[k].height);
                fflush(stderr);
            }
            return;
        }
    }
    rbt_walk[k].hold_ok=1;rbt_walk[k].hold_room=room;
    memcpy(rbt_walk[k].hold,pos,sizeof pos);
}
/* WRAP sub_00293730 (cdecl entity; the robot's AI, dispatcher 0x0027DE18). */
void pc_mapmaker_robot_handler(void (*original)(void))
{
    uint32_t ent;
    int k;
    if(!rbt_lvl.n || !MAPMAKER_STACK_OK(8)) { original();return; }
    ent=WORD_AT(g_esp+4u);
    if((k=rbt_find(ent))>=0) rbt_hold((unsigned)k,ent);
    rbt_swapped(original,ent);
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
        rbt_walk[k].hold_ok=0;                         /* (re)spawned: no previous position */
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
        rbt_walk[k].hold_ok=0;                         /* back at its spawn point */
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
