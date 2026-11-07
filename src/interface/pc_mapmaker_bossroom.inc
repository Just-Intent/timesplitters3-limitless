/* US 45410066 MapMaker play: giant bosses vanish while their rooms are on
 * screen (user report after .45; skelvis log m46.err). Area `rooms` (called
 * from pc_mapmaker_rooms_buckets, no new wrapper). Docs:
 * docs/mapmaker/mapmaker-realbosses.md ("Drawn through the camera's room").
 * Tests: tests/mapmaker/test_mapmaker_bossroom.py.
 *
 * Evidence (m46.err, CONFIRMED by the log): every hidden pass of every
 * skeleton was "listed rooms visible but not reached by their prop lists"
 * (e.g. 10 listed rooms, 9 visible, sub_000A2430 never called for it), never
 * frustum, view planes or the rectangle; the skeletons were drawn while the
 * camera's room was in their list and hidden while it was not (skeleton 3,
 * whose 2-6 rooms never held the camera room, was hidden 1137 of 1200 passes).
 * A prop is only reached through the prop lists of drawn rooms: per view the
 * readers sub_00086480/sub_000865E0 run sub_000A79B0(room) (cull) and
 * sub_000A2900(room) (draw) over count [0x008CC000 + 4*room] entries of list
 * [0x008CD040 + 4*room]. Those lists are rebuilt each frame by sub_0009E4B0
 * (0x001CE368/0x001CE38B, before the views): per room a chunk of the bucket
 * pool [0x00715FB4] sized by last frame's capacity, then for every non-room
 * prop (index [0x007B975C]..[0x007B4FAC], table [0x0072F5E8]) and each room of
 * its +0x38 list, sub_0009E430 appends it. sub_0009E430 (CONFIRMED decode):
 * a full room grows its capacity (0x008CF0C0) and takes a new chunk at the
 * pool end; when the pool is past 0x299D dwords (0x0009E46B) the prop is NOT
 * linked to that room, but the raised capacity stays, so the next appends to
 * that room write past its chunk into the next room's. Props also skip
 * linking for flags (0x0009E531..0x0009E55B). INFERRED (not proven by the
 * log, which did not read the room lists): the bosses are missing from the
 * lists of their visible rooms through one of these; the camera's room,
 * always drawn first with the whole viewport, is the one room where the log
 * shows them reached. Pool use is logged below to decide.
 *
 * Fix: per view, before its rooms are bucketed and drawn, for each giant boss
 * (character prop, model 0x5E9 skeleton, 0x5BE/0x5BF trophy monster, 0x637
 * Crow walker, 0x5F2 giant robot, 0x5E8 split Crowe) with a room list one of
 * whose rooms is reached by this view's portal walk (depth >= 0) and that is
 * not already in the camera room's prop list, append it to that list. The
 * camera room's list is first copied to a buffer of its own per view slot
 * (BSR_BUFFER, free extension memory 0x008D4000..0x008DBFFF, 1024 entries
 * each), so the pool and the other rooms' chunks are never written; the next
 * sub_0009E4B0 lays every room out again. The camera room's draw then runs the
 * stock cull and draw for the boss: its rectangle is still the union of its
 * own listed rooms (pc_mapmaker_bigdraw_rect widens it), so a boss none of
 * whose rooms is visible stays culled. Skipped (counted): no real camera room
 * (global 0x40C), a capacity or count past the buffer. Scene 100 with
 * TSFP_MAPMAKER_LIMITS=1; TSFP_MAPMAKER_BOSSROOM=0 turns it off. Lines
 * "[mapmaker] bossroom: ..." (BSR_LINES per level). Requires the includer's
 * WORD_AT, g_xbox_mem_offset and pc_mapmaker_limits.inc. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAPMAKER_BOSSROOM 1
#define BSR_SCENE 0x0079094Cu
#define BSR_PLAY 100u
#define BSR_TABLE 0x0072F5E8u          /* -> props, 0x2E8 each */
#define BSR_PROP 0x2E8u
#define BSR_FIRST 0x007B975Cu          /* room count: props 0..n-1 are the rooms' own (not linked) */
#define BSR_END 0x007B4FACu            /* prop count */
#define BSR_PROPS_MAX 4096u
#define BSR_COUNTS 0x008CC000u         /* rooms.json p1: prop count per room */
#define BSR_LISTS 0x008CD040u          /* rooms.json p4: prop list per room */
#define BSR_CAPS 0x008CF0C0u           /* rooms.json p3: capacity per room */
#define BSR_POOL_USED 0x0072F298u      /* dwords of the bucket pool in use */
#define BSR_POOL_PTR 0x00715FB4u       /* bucket pool (new per level) */
#define BSR_POOL_BOUND 0x299Du         /* objects.json obj-bucket-bound (0x0009E46B) */
#define BSR_ROOMS_MAX 0x40Bu           /* real rooms 1..0x3FF; 0x40C global */
#define BSR_GLOBAL 0x40Cu
#define BSR_VIEW_SLOT 0x214u
#define BSR_CAMERA_ROOM 0x2A0u
#define BSR_RECTS 0x1000u              /* view shadow: room r at +0x1000+12*r, int32 depth */
#define BSR_BUFFER 0x008D4000u         /* 8 view slots x 0x1000 (free rooms sub-range) */
#define BSR_SLOT_BYTES 0x1000u
#define BSR_SLOT_ENTRIES (BSR_SLOT_BYTES/4u)
#define BSR_SLOTS 8u
#define BSR_LINES 32u
#define BSR_SEEN 16u

static struct {
    int read, off;
    uint32_t pool;                       /* the level's bucket pool pointer */
    unsigned lines;
    uint32_t seen[BSR_SEEN];             /* boss props logged (first add) */
    unsigned seen_n;
    int last_unlinked[BSR_SEEN];
    unsigned long long adds, no_room, full, views;
} bsr;

static int bsr_boss_model(uint32_t m)
{
    return m==0x5E9u || m==0x5BEu || m==0x5BFu || m==0x637u || m==0x5F2u || m==0x5E8u;
}
static int bsr_on(void)
{
    if(!bsr.read) { const char *v=getenv("TSFP_MAPMAKER_BOSSROOM");bsr.read=1;bsr.off=v && v[0]=='0' && !v[1]; }
    if(bsr.off || WORD_AT(BSR_SCENE)!=BSR_PLAY) return 0;
    mapmaker_limits_read();
    return mapmaker_limits.limits;
}
static int bsr_in_list(uint32_t room,uint32_t p)
{
    uint32_t n=WORD_AT(BSR_COUNTS+room*4u),l=WORD_AT(BSR_LISTS+room*4u),i;
    if(n>BSR_PROPS_MAX || !MAPMAKER_PTR_OK(l,n*4u)) return 0;
    for(i=0;i<n;i++) if(WORD_AT(l+i*4u)==p) return 1;
    return 0;
}
static int bsr_line(void)
{
    if(bsr.lines>=BSR_LINES) return 0;
    if(++bsr.lines==BSR_LINES) fprintf(stderr,"[mapmaker] bossroom: %u lines this level, quiet now\n",BSR_LINES);
    return 1;
}
/* One line the first time each boss is added, and again whenever its
 * "linked to none of its visible rooms" state changes (the root-cause
 * evidence: linked 0 = missing from those rooms' lists). */
static void bsr_note(uint32_t p,uint32_t model,uint32_t cam,unsigned listed,unsigned visible,unsigned linked)
{
    unsigned k;
    int unlinked=linked==0u;
    for(k=0;k<bsr.seen_n;k++) if(bsr.seen[k]==p) break;
    if(k==bsr.seen_n) {
        if(k>=BSR_SEEN) return;
        bsr.seen[bsr.seen_n++]=p;bsr.last_unlinked[k]=-1;
    }
    if(bsr.last_unlinked[k]==unlinked || !bsr_line()) return;
    bsr.last_unlinked[k]=unlinked;
    fprintf(stderr,"[mapmaker] bossroom: boss prop 0x%X (model 0x%X) drawn through camera room %u: linked to %u of its %u reached "
            "listed rooms (%u listed), bucket pool %u of %u used\n",(unsigned)p,(unsigned)model,(unsigned)cam,linked,visible,listed,
            (unsigned)WORD_AT(BSR_POOL_USED),BSR_POOL_BOUND);
    fflush(stderr);
}

/* From pc_mapmaker_rooms_buckets (sub_00086C40), first thing, per view. */
static void mapmaker_bossroom_view(uint32_t view)
{
    uint32_t n,cam,table,end,i,slot,buf,count,list;
    int own;
    if(!bsr_on()) return;
    if(WORD_AT(BSR_POOL_PTR)!=bsr.pool) {           /* a new level */
        int read=bsr.read,off=bsr.off;
        memset(&bsr,0,sizeof bsr);
        bsr.read=read;bsr.off=off;bsr.pool=WORD_AT(BSR_POOL_PTR);
    }
    if(!MAPMAKER_PTR_OK(view,BSR_RECTS+(BSR_ROOMS_MAX+1u)*12u)) return;
    bsr.views++;
    n=WORD_AT(BSR_FIRST);cam=WORD_AT(view+BSR_CAMERA_ROOM);
    table=WORD_AT(BSR_TABLE);end=WORD_AT(BSR_END);
    if(n>BSR_ROOMS_MAX || end>BSR_PROPS_MAX || end<=n || !MAPMAKER_PTR_OK(table,end*BSR_PROP)) return;
    slot=WORD_AT(view+BSR_VIEW_SLOT)&(BSR_SLOTS-1u);
    buf=BSR_BUFFER+slot*BSR_SLOT_BYTES;
    list=0;count=0;own=0;
    for(i=n;i<end;i++) {
        uint32_t p=table+i*BSR_PROP,model=*(uint16_t *)((uintptr_t)(p+0x94u)+g_xbox_mem_offset),r,k;
        int listed;
        unsigned visible=0,linked=0;
        if(!bsr_boss_model(model) || WORD_AT(p+0x20u)!=8u) continue;
        listed=*(int8_t *)((uintptr_t)(p+0x30u)+g_xbox_mem_offset);
        if(listed<=0) continue;
        if(listed>10) listed=10;
        for(k=0;k<(uint32_t)listed;k++) {
            r=WORD_AT(p+0x38u+k*4u);
            if(r<1u || r>n || (int32_t)WORD_AT(view+BSR_RECTS+r*12u)<0) continue;
            visible++;linked+=(unsigned)bsr_in_list(r,p);
        }
        if(!visible) continue;
        if(cam<1u || cam>n || cam==BSR_GLOBAL) { bsr.no_room++;continue; }
        if(!list) {
            list=WORD_AT(BSR_LISTS+cam*4u);count=WORD_AT(BSR_COUNTS+cam*4u);
            own=list==buf;
        }
        if(bsr_in_list(cam,p)) continue;
        if(!own) {                                   /* the camera room's list, copied to this slot's buffer */
            if(count>=BSR_SLOT_ENTRIES || WORD_AT(BSR_CAPS+cam*4u)>BSR_SLOT_ENTRIES || (count && !MAPMAKER_PTR_OK(list,count*4u))) {
                bsr.full++;continue;
            }
            if(count) memmove((void *)((uintptr_t)buf+g_xbox_mem_offset),(void *)((uintptr_t)list+g_xbox_mem_offset),count*4u);
            WORD_AT(BSR_LISTS+cam*4u)=buf;list=buf;own=1;
        }
        if(count>=BSR_SLOT_ENTRIES) { bsr.full++;continue; }
        WORD_AT(buf+count*4u)=p;
        WORD_AT(BSR_COUNTS+cam*4u)=++count;
        bsr.adds++;
        bsr_note(p,model,cam,(unsigned)listed,visible,linked);
    }
}
