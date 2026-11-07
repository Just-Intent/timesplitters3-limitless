/* US 45410066 MapMaker play: why a giant skeleton (model 0x5E9, the mansion
 * boss, several per map) is not drawn in a frame. DIAGNOSTIC LOGGING ONLY:
 * nothing here writes guest memory or changes what the game does. Area
 * `bigdraw` (its own wrapper below); docs/mapmaker/mapmaker-realbosses.md
 * ("Skeleton visibility log"). Tests: tests/mapmaker/test_mapmaker_skelvis.py.
 *
 * User report (after .45): skeletons briefly vanish when the player walks
 * close or while they are mid-animation. How a character is drawn (CONFIRMED
 * decode, test_mapmaker_bigdraw.py and the test here):
 *  - Only through the prop lists of drawn rooms: per drawn room
 *    sub_000A79B0 runs sub_000A2430(prop) for every prop of 0x8C7CC0[room].
 *  - sub_000A2430: a prop whose list byte +0x30 is 0 (after a rebuild,
 *    sub_0009FBE0) is culled at 0x000A2455 (flags +0x28 |= 0x802000);
 *    else the draw rectangle sub_000862C0 (pc_mapmaker_bigdraw_rect), the
 *    box sub_000A1FA0, the frustum test sub_0008C020 (its only caller,
 *    return 0x000A24A3; < 0 culls at 0x000A24AA, |= 0x800000), the view
 *    plane test (0x000A2515, |= 0x802000) and the screen rectangle test
 *    (0x000A271D, |= 0x800000); a pass clears both bits (0x000A2713).
 *  - sub_000A2740 draws a prop that passed (pc_mapmaker_tank_draw wraps it).
 *  - The room list (+0x30 count, +0x38 ids) is sub_0009FBE0's query
 *    sub_000873C0(0, box, +0x38, 10) -> sub_00087310 (our
 *    pc_mapmaker_rooms_query, return 0x000873E0 under 0x0009FCF5): at most
 *    10 rooms of those the box position +0x64 + centre +0x1A8 +- half size
 *    +0x1C0 touches. sub_0009EAB0 sets half = model radius (all axes) and
 *    centre = the root matrix translation [[+4]+0x10]+0x30 (moves with the
 *    animation) unless flags +0x28 bit 0x100 (fixed centre).
 * Two suspects (INFERRED, this log decides): the box touches more than 10
 * one-floor tile rooms so the list misses the rooms near the camera, or the
 * frustum box follows the root bone and is culled while limbs are on screen.
 *
 * Hooks (all read-only; no new behaviour):
 *  - mapmaker_skelvis_list from pc_mapmaker_rooms_query: rooms touched vs the
 *    cap, truncation, the camera's room (view +0x2A0) in / dropped / untouched.
 *  - mapmaker_skelvis_rect from pc_mapmaker_bigdraw_rect: reached this pass,
 *    rectangle empty or not; arms the frustum wrapper.
 *  - NEW WRAP sub_0008C020 -> pc_mapmaker_skelvis_frustum: its result.
 *  - mapmaker_skelvis_drawn from pc_mapmaker_tank_draw (sub_000A2740).
 *  - mapmaker_skelvis_pass from pc_mapmaker_openview_walk, BEFORE the walk:
 *    ends the previous pass of the main view (the first view slot seen in a
 *    level; the view's room rectangles are still the previous pass's).
 *  - mapmaker_skelvis_reset from pc_mapmaker_realbosses_restart.
 * Lines (prefix "[mapmaker] skelvis: "), at most SKV_LINES per level (state
 * changes only) plus SKV_SUMMARIES summaries (every SKV_SUMMARY_PASSES main
 * passes, only when a hidden count or a truncation changed). Scene 100 with
 * TSFP_MAPMAKER_LIMITS=1 only. Hot paths (per prop of a drawn room) test the
 * model word first and make no system calls. Requires the includer's
 * WORD_AT, FLOAT_AT, g_esp, g_eax, g_xbox_mem_offset and pc_mapmaker_limits.inc. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#define MAPMAKER_SKELVIS 1
#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) MAPMAKER_PTR_OK(g_esp,(n))
#endif

#define SKV_SCENE 0x0079094Cu
#define SKV_PLAY 100u
#define SKV_MODEL 0x5E9u                /* CHR_MANSION_MANSIONBOSS */
#define SKV_PROP_SIZE 0x2E8u
#define SKV_TYPE 0x20u
#define SKV_CHARACTER 8u
#define SKV_FLAGS 0x28u
#define SKV_FIXED_CENTRE 0x100u         /* sub_0009EAB0: centre (0, [0x475CD0], 0) */
#define SKV_NO_ROOM 0x2000u             /* with 0x800000: 0x000A2455 / 0x000A2515 */
#define SKV_LIST_COUNT 0x30u            /* signed byte */
#define SKV_HOME 0x34u
#define SKV_LIST 0x38u                  /* 10 dwords */
#define SKV_POS 0x64u
#define SKV_CENTRE 0x1A8u
#define SKV_HALF 0x1C0u
#define SKV_CAP 10u                     /* push 0xA at 0x0009FCC2 */
#define SKV_MODEL_WORD 0x94u
#define SKV_ANIM 0xD4u                  /* animation controller (+4): current animation */
#define SKV_VIEW 0x007B0CC0u            /* current view */
#define SKV_VIEW_SLOT 0x214u
#define SKV_CAMERA_ROOM 0x2A0u
#define SKV_RECTS 0x1000u               /* view shadow (rooms.json; stock +0x3BC) + 12*room */
#define SKV_ROOM_COUNT 0x007B975Cu
#define SKV_PLAYER 0x007B0C7Cu          /* player 1 entity, +0x64 position */
#define SKV_RET_QUERY 0x000873E0u       /* sub_000873C0 -> sub_00087310 */
#define SKV_RET_LIST 0x0009FCF5u        /* sub_0009FBE0 -> sub_000873C0 */
#define SKV_RET_FRUSTUM 0x000A24A3u     /* sub_000A2430 -> sub_0008C020 */
#define SKV_MAX 8u
#define SKV_LINES 60u
#define SKV_UNSEEN_LINES 12u            /* of SKV_LINES: "none of its rooms visible" (looking away) */
#define SKV_SUMMARIES 40u
#define SKV_SUMMARY_PASSES 600u
#define SKV_B(a) (*(int8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define SKV_H(a) (*(int16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

enum { SKV_SHOWN, SKV_NOLIST, SKV_UNSEEN, SKV_UNREACHED, SKV_NORECT, SKV_FRUSTUM, SKV_PLANES, SKV_LATE, SKV_STATES };
static const char *const skv_reason[SKV_STATES]={
    "drawn",
    "no rooms in its list (0x000A2455)",
    "none of its listed rooms visible",
    "listed rooms visible but not reached by their prop lists",
    "reached but its listed rooms give an empty rectangle",
    "frustum culled at 0x000A24AA",
    "view-plane culled at 0x000A2515",
    "culled after the frustum test (screen rectangle 0x000A271D) or not drawn",
};

typedef struct {
    uint32_t prop;
    unsigned rects, empty, front, behind, draws;   /* this pass */
    int32_t frustum;
    int state;                                     /* last pass, -1 none yet */
    unsigned run;                                  /* passes in that state */
    unsigned long long count[SKV_STATES];
    unsigned builds, truncated, dropped, peak;
    int list_key;
} SkvSkel;

static struct {
    SkvSkel s[SKV_MAX];
    unsigned n, lines, unseen_lines, summaries, passes, other;
    unsigned long long frustum_calls, sum_key;
    uint32_t view, rooms, cur;
    int slot, in_pass;
} skv={.slot=-1};

static float skv_f(uint32_t a) { return FLOAT_AT(a); }
static int skv_on(void)
{
    if(WORD_AT(SKV_SCENE)!=SKV_PLAY) return 0;
    mapmaker_limits_read();
    return mapmaker_limits.limits;
}
static int skv_is_skel(uint32_t p)
{
    return MAPMAKER_PTR_OK(p,SKV_PROP_SIZE) && *(uint16_t *)((uintptr_t)(p+SKV_MODEL_WORD)+g_xbox_mem_offset)==SKV_MODEL &&
           WORD_AT(p+SKV_TYPE)==SKV_CHARACTER;
}
/* One budgeted line; the last one says the log goes quiet. */
static int skv_line(int unseen)
{
    if(skv.lines>=SKV_LINES || (unseen && skv.unseen_lines>=SKV_UNSEEN_LINES)) return 0;
    skv.lines++;skv.unseen_lines+=(unsigned)unseen;
    return 1;
}
static void skv_line_done(void)
{
    if(skv.lines==SKV_LINES) fprintf(stderr,"[mapmaker] skelvis: %u lines this level, quiet now (summaries go on)\n",SKV_LINES);
    fflush(stderr);
}
static SkvSkel *skv_find(uint32_t p,int add)
{
    unsigned k;
    SkvSkel *s;
    for(k=0;k<skv.n;k++) if(skv.s[k].prop==p) return &skv.s[k];
    if(!add || skv.n>=SKV_MAX) return NULL;
    s=&skv.s[skv.n++];
    memset(s,0,sizeof *s);
    s->prop=p;s->state=-1;s->list_key=-1;
    if(skv_line(0)) {
        uint32_t f=WORD_AT(p+SKV_FLAGS);
        fprintf(stderr,"[mapmaker] skelvis: skeleton %u: tracked (prop 0x%X): radius %.2f (half size %.2f, %.2f, %.2f), "
                "box centre offset (%.2f, %.2f, %.2f) %s, at (%.2f, %.2f, %.2f)\n",
                skv.n,(unsigned)p,(double)skv_f(p+SKV_HALF),(double)skv_f(p+SKV_HALF),(double)skv_f(p+SKV_HALF+4u),(double)skv_f(p+SKV_HALF+8u),
                (double)skv_f(p+SKV_CENTRE),(double)skv_f(p+SKV_CENTRE+4u),(double)skv_f(p+SKV_CENTRE+8u),
                (f&SKV_FIXED_CENTRE)?"(fixed)":"(root bone, moves with the animation)",
                (double)skv_f(p+SKV_POS),(double)skv_f(p+SKV_POS+4u),(double)skv_f(p+SKV_POS+8u));
        skv_line_done();
    }
    return s;
}
static unsigned skv_index(const SkvSkel *s) { return (unsigned)(s-skv.s)+1u; }

static void skv_summary(int final)
{
    unsigned k;
    unsigned long long key=skv.n;
    for(k=0;k<skv.n;k++) {
        unsigned r;
        for(r=SKV_NOLIST;r<SKV_STATES;r++) key=key*1099511628211ull+skv.s[k].count[r];
        key=key*31u+skv.s[k].truncated;
    }
    if(!skv.n || key==skv.sum_key || skv.summaries>=SKV_SUMMARIES) return;
    skv.sum_key=key;skv.summaries++;
    for(k=0;k<skv.n;k++) {
        const SkvSkel *s=&skv.s[k];
        fprintf(stderr,"[mapmaker] skelvis: %s after %u passes: skeleton %u drawn %llu, hidden: no list %llu, rooms unseen %llu, "
                "not reached %llu, empty rect %llu, frustum %llu, planes %llu, later %llu; list builds %u, truncated %u, "
                "camera room dropped %u, most rooms touched %u (cap %u); other views %u%s\n",
                final?"final summary":"summary",skv.passes,k+1u,s->count[SKV_SHOWN],s->count[SKV_NOLIST],s->count[SKV_UNSEEN],
                s->count[SKV_UNREACHED],s->count[SKV_NORECT],s->count[SKV_FRUSTUM],s->count[SKV_PLANES],s->count[SKV_LATE],
                s->builds,s->truncated,s->dropped,s->peak,SKV_CAP,skv.other,
                skv.frustum_calls?"":" (frustum hook not seen: frustum culls count as later)");
    }
    fflush(stderr);
}

/* Forget the level (map left, restart, rooms rebuilt). */
static void mapmaker_skelvis_reset(const char *why)
{
    if(skv.n) {
        skv_summary(1);
        if(skv.lines<SKV_LINES) fprintf(stderr,"[mapmaker] skelvis: %s: forgetting %u skeleton%s\n",why,skv.n,skv.n==1u?"":"s");
        fflush(stderr);
    }
    memset(&skv,0,sizeof skv);
    skv.slot=-1;
}

static int skv_visible(uint32_t view,uint32_t room)
{
    uint32_t n=skv.rooms,e;
    if(room<1u || room>n || !MAPMAKER_PTR_OK(view,SKV_RECTS+(n+1u)*12u)) return 0;
    e=view+SKV_RECTS+room*12u;
    return (int32_t)WORD_AT(e)>=0 && SKV_H(e+4u)<SKV_H(e+8u);
}
static uint32_t skv_camera_room(uint32_t view)
{
    return MAPMAKER_PTR_OK(view,SKV_CAMERA_ROOM+4u)?WORD_AT(view+SKV_CAMERA_ROOM):0u;
}

/* From pc_mapmaker_rooms_query, called by sub_0009FBE0 (the room list):
 * out = prop +0x38; touched = rooms the box touches (query words with bit
 * 0), found = rooms stored (<= max); the query buffer (count, words). */
static void mapmaker_skelvis_list(uint32_t out,uint32_t max,uint32_t touched,uint32_t found,uint32_t buffer,uint32_t count)
{
    uint32_t p=out-SKV_LIST,cam,i;
    int where=0,key,trunc;      /* where: 0 not touched, 1 in the list, 2 dropped by the cap */
    SkvSkel *s;
    if(!skv_is_skel(p) || !skv_on() || !(s=skv_find(p,1))) return;
    s->builds++;
    trunc=touched>max;
    if(trunc) s->truncated++;
    if(touched>s->peak) s->peak=touched;
    cam=skv_camera_room(WORD_AT(SKV_VIEW));
    for(i=0;i<found && cam;i++) if(WORD_AT(out+i*4u)==cam) { where=1;break; }
    if(!where && cam)
        for(i=0;i<count;i++) {
            uint32_t w=*(uint16_t *)((uintptr_t)(buffer+2u+2u*i)+g_xbox_mem_offset);
            if((w&1u) && (w>>1)==cam) { where=2;break; }
        }
    if(where==2) s->dropped++;
    key=trunc|where<<1;
    if(key==s->list_key) return;
    s->list_key=key;
    if(!skv_line(0)) return;
    fprintf(stderr,"[mapmaker] skelvis: skeleton %u: room list %u of %u touched rooms (cap %u)%s, home room %u, camera room %u %s\n",
            skv_index(s),(unsigned)found,(unsigned)touched,(unsigned)max,trunc?" TRUNCATED":"",(unsigned)WORD_AT(p+SKV_HOME),
            (unsigned)cam,where==1?"in the list":where==2?"DROPPED by the cap":"not touched by its box");
    skv_line_done();
}

/* From pc_mapmaker_bigdraw_rect after the stock rectangle (before widening). */
static void mapmaker_skelvis_rect(uint32_t p)
{
    SkvSkel *s;
    skv.cur=0;
    if(!skv_is_skel(p) || !skv_on() || !(s=skv_find(p,1))) return;
    s->rects++;
    if(!(skv_f(p+0x118u)<skv_f(p+0x120u) && skv_f(p+0x11Cu)<skv_f(p+0x124u))) s->empty++;
    skv.cur=p;
}

/* WRAP sub_0008C020 (cdecl box, out, matrix; result in eax, plain ret; only
 * caller sub_000A2430 0x000A249E): log only, the result for a skeleton. */
void pc_mapmaker_skelvis_frustum(void (*original)(void))
{
    uint32_t p=skv.cur;
    SkvSkel *s;
    skv.cur=0;
    if(!p || !MAPMAKER_STACK_OK(4u) || WORD_AT(g_esp)!=SKV_RET_FRUSTUM) { original();return; }
    original();
    skv.frustum_calls++;
    if(!(s=skv_find(p,0))) return;
    s->frustum=(int32_t)g_eax;
    if((int32_t)g_eax<0) s->behind++; else s->front++;
}

/* From pc_mapmaker_tank_draw (sub_000A2740, the prop draw). */
static void mapmaker_skelvis_drawn(uint32_t p)
{
    unsigned k;
    for(k=0;k<skv.n;k++) if(skv.s[k].prop==p) { skv.s[k].draws++;return; }
}

static int skv_classify(const SkvSkel *s,unsigned *listed,unsigned *visible,int *cam_in)
{
    uint32_t p=s->prop,cam=skv_camera_room(skv.view);
    int n=SKV_B(p+SKV_LIST_COUNT);
    unsigned i;
    *listed=n>0?(unsigned)n:0u;*visible=0;*cam_in=0;
    for(i=0;i<*listed && i<SKV_CAP;i++) {
        uint32_t r=WORD_AT(p+SKV_LIST+i*4u);
        *visible+=(unsigned)skv_visible(skv.view,r);
        if(r==cam) *cam_in=1;
    }
    if(s->draws) return SKV_SHOWN;
    if(n<=0) return SKV_NOLIST;
    if(!s->rects) return *visible?SKV_UNREACHED:SKV_UNSEEN;
    if(s->empty==s->rects) return SKV_NORECT;
    if(s->behind && !s->front) return SKV_FRUSTUM;
    if(WORD_AT(p+SKV_FLAGS)&SKV_NO_ROOM) return SKV_PLANES;
    return SKV_LATE;
}

static void skv_finish(SkvSkel *s)
{
    uint32_t p=s->prop,ctl,player;
    unsigned listed,visible;
    int cam_in,st,prev=s->state;
    float cx,cy,cz,d=-1.f;
    if(!skv_is_skel(p)) return;                    /* freed or reused: no counts */
    st=skv_classify(s,&listed,&visible,&cam_in);
    s->count[st]++;
    if(st==prev) { s->run++;return; }
    s->state=st;
    if(st==SKV_SHOWN) {
        if(prev>SKV_SHOWN && skv_line(prev==SKV_UNSEEN)) {
            fprintf(stderr,"[mapmaker] skelvis: skeleton %u: drawn again after %u pass%s hidden (%s)\n",
                    skv_index(s),s->run,s->run==1u?"":"es",skv_reason[prev]);
            skv_line_done();
        }
        s->run=1;
        return;
    }
    if(skv_line(st==SKV_UNSEEN)) {
        cx=skv_f(p+SKV_POS)+skv_f(p+SKV_CENTRE);cy=skv_f(p+SKV_POS+4u)+skv_f(p+SKV_CENTRE+4u);cz=skv_f(p+SKV_POS+8u)+skv_f(p+SKV_CENTRE+8u);
        player=WORD_AT(SKV_PLAYER);
        if(MAPMAKER_PTR_OK(player,0x70u)) {
            float dx=cx-skv_f(player+SKV_POS),dy=cy-skv_f(player+SKV_POS+4u),dz=cz-skv_f(player+SKV_POS+8u);
            d=sqrtf(dx*dx+dy*dy+dz*dz);
        }
        ctl=WORD_AT(p+4u);
        fprintf(stderr,"[mapmaker] skelvis: skeleton %u: hidden (%s), was %s for %u pass%s: player %.1f away, box centre (%.2f, %.2f, %.2f) "
                "half %.2f, animation 0x%X, list %u rooms (%u visible), camera room %u %s, reached %u, frustum %d, flags 0x%X\n",
                skv_index(s),skv_reason[st],prev==SKV_SHOWN?"drawn":prev<0?"not judged yet":skv_reason[prev],s->run,s->run==1u?"":"es",
                (double)d,(double)cx,(double)cy,(double)cz,(double)skv_f(p+SKV_HALF),
                MAPMAKER_PTR_OK(ctl,SKV_ANIM+4u)?(unsigned)WORD_AT(ctl+SKV_ANIM):0u,listed,visible,
                (unsigned)skv_camera_room(skv.view),cam_in?"in the list":"not in the list",s->rects,
                s->front||s->behind?(int)s->frustum:0,(unsigned)WORD_AT(p+SKV_FLAGS));
        skv_line_done();
    }
    s->run=1;
}

/* From pc_mapmaker_openview_walk, before the walk of the current view. */
static void mapmaker_skelvis_pass(void)
{
    uint32_t view,rooms;
    unsigned k;
    int slot;
    if(!skv_on()) {
        if(skv.n || skv.passes) mapmaker_skelvis_reset("left MapMaker play");
        return;
    }
    rooms=WORD_AT(SKV_ROOM_COUNT);
    if(skv.passes && rooms!=skv.rooms) mapmaker_skelvis_reset("rooms rebuilt");
    skv.rooms=rooms;
    view=WORD_AT(SKV_VIEW);
    slot=MAPMAKER_PTR_OK(view,SKV_VIEW_SLOT+4u)?(int)(WORD_AT(view+SKV_VIEW_SLOT)&7u):0;
    if(skv.in_pass) {
        for(k=0;k<skv.n;k++) skv_finish(&skv.s[k]);
        if(!(skv.passes%SKV_SUMMARY_PASSES)) skv_summary(0);
    }
    for(k=0;k<skv.n;k++) {
        SkvSkel *s=&skv.s[k];
        s->rects=s->empty=s->front=s->behind=s->draws=0;s->frustum=0;
    }
    skv.cur=0;
    if(skv.slot<0) skv.slot=slot;
    skv.in_pass=slot==skv.slot;
    if(skv.in_pass) { skv.passes++;skv.view=view; } else skv.other++;
}
