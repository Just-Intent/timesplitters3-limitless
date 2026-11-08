/* US 45410066 MapMaker play: rooms seen across Open air tiles are drawn over
 * the whole opening, not clipped to the portal rectangles of the open sides.
 * Area `openview`. Docs: docs/mapmaker/mapmaker-sky.md ("Seeing across open
 * air"). Tests: tests/mapmaker/test_mapmaker_openview.py.
 *
 * Cause (CONFIRMED, decode checks in the test): the per-view portal walk
 * sub_00089B90 (tail-jumped per view from sub_001CE650, 0x001CE688, and from
 * 0x001CE8E3) resets every room (sub_000878A0: depth -1, empty rectangle)
 * and calls sub_00087F80(camera room, 0, viewport x0, x1, y0, y1)
 * (0x00089BD9). sub_00087F80 recurses through the room's portals (room
 * record [0x00708788] + room*0xB4, +4 -> {count, portal ids}; portal
 * [[0x0070878C] + id*4]: +0 room A, +4 room B), projects each portal, clips
 * it to the incoming rectangle and stores the union in the view's room entry
 * (stock view +0x3BC + 12*room, moved to the shadow +0x1000 by rooms.json:
 * int32 depth, int16 x0, y0, x1, y1). sub_000897D0 lists the rooms with a
 * depth >= 0 and x1 > x0, and every prop of a room (its geometry included)
 * is culled and scissored to that rectangle (sub_000862C0, see
 * pc_mapmaker_bigdraw.inc). A MapMaker tile is one room; between two tiles
 * there is a portal one floor high, also when an Open air tile hides its
 * walls (the mask only hides nodes). So from an open tile, the next tile is
 * drawn only inside the projected rectangle of the shared side, the tile
 * after it inside the intersection of two such rectangles, and a tile
 * reached through no portal on screen (a diagonal neighbour, or a portal
 * behind the camera) not at all: the sky shows around a sharp-edged box.
 *
 * Fix: WRAP sub_00089B90. After the stock walk, in scene 100 with
 * TSFP_MAPMAKER_LIMITS=1 on a map with open-air rooms (pc_mapmaker_roof.inc
 * marks the rooms of bounded and unbounded pieces in mapmaker_roof_play.open):
 * the open rooms are grouped once per build into groups joined by portals
 * (an open area); each group's touched set is its rooms, every room one
 * portal away, and every room whose box touches a member's box (a tile
 * beside, diagonal to, above or below the area with no portal into it; box
 * = room record +0x10 min xyz, +0x1C max xyz, as sub_000873F0 returns it and
 * sub_00087450 tests a point against it). Per
 * frame, for each group with a visible room, the union U of its visible
 * rooms' rectangles (the whole viewport when the camera stands in it; the
 * doorway's rectangle when the area is seen from inside a closed tile) is
 * merged into every touched room's rectangle, and a touched room the walk
 * did not reach gets depth (smallest visible depth in the group) + 1, capped
 * at OPENVIEW_DEPTH_CAP, with the view's deepest depth +0xD28 raised to it.
 * Depth testing still hides what is behind drawn walls.
 * Walls seen from outside (v0.4.0-mapmaker.41 report: "you can see the
 * ceiling and floors of some of the tiles but not the walls of any"): a
 * tile's geometry faces into its room and the stock room draw culls back
 * faces, so a closed tile seen across open air showed its floor and
 * ceiling (front faces from outside, at eye level between them) through
 * its culled walls; and a tile beside the area with no portal into it was
 * not drawn at all. Every touched room is marked two-sided
 * (mapmaker_roof_play.twosided), and pc_mapmaker_roof_draw draws its
 * geometry without back-face culling. Rooms beyond a
 * closed tile's doorway stay as the stock walk left them; open areas joined
 * by no portal (an island across a gap) are not reached. Closed tiles away
 * from open air, other scenes and story levels: stock. The room id width,
 * the draw buckets (sub_00086C40, sub_000897D0) and the shadow layout are
 * unchanged; ids are read as dwords and kept below MAPMAKER_ROOF_ROOMS.
 * Requires the includer's WORD_AT, g_xbox_mem_offset, pc_mapmaker_limits.inc
 * and pc_mapmaker_roof.inc (mapmaker_roof_play). */
#include <stdio.h>
#include <string.h>

#define OPENVIEW_SCENE 0x0079094Cu
#define OPENVIEW_PLAY 100u
#define OPENVIEW_VIEW 0x007B0CC0u        /* current view (0xFFC struct, 0x5000 apart with its shadow) */
#define OPENVIEW_ROOM_COUNT 0x007B975Cu  /* rooms 1..n */
#define OPENVIEW_PORTAL_COUNT 0x007B9754u /* portal ids 0..n (sub_00088430 0x000884E4) */
#define OPENVIEW_RECORDS 0x00708788u     /* room r: + r*0xB4, +4 -> {count, ids} */
#define OPENVIEW_RECORD 0xB4u
#define OPENVIEW_PORTALS 0x0070878Cu     /* portal id -> struct: +0 room A, +4 room B */
#define OPENVIEW_RECT 0x1000u            /* view shadow (rooms.json; stock +0x3BC) + 12*room */
#define OPENVIEW_DEEPEST 0xD28u          /* view: deepest walk depth (sub_000897D0 caps it at 0x4F) */
#define OPENVIEW_DEPTH_CAP 0x40u         /* below the 0x50 depth buckets, room for sub_00086C40's extra */
#define OPENVIEW_PORTALS_PER_ROOM 64u
#define OPENVIEW_TOUCHED 4096u
#define OPENVIEW_BOX 0x10u               /* room record: min xyz +0x10, max xyz +0x1C (sub_000873F0) */
#define OPENVIEW_TOUCH 0.5f              /* boxes this close count as touching (tiles share faces) */
#define OPENVIEW_FAR 100000.0f           /* larger coordinates: not a usable box */
#define OPENVIEW_H(a) (*(int16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define OPENVIEW_F(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

static struct {
    unsigned generation, rooms, portals;   /* what the groups were built for */
    int ready;                             /* groups built (0: rebuild; -1: nothing to do) */
    unsigned groups, touched_total, open, beside;
    uint16_t group_first[MAPMAKER_ROOF_ROOMS+1u]; /* touched[group_first[g]..group_first[g+1]) */
    uint16_t members[MAPMAKER_ROOF_ROOMS+1u];     /* member count of group g: first entries are its open rooms */
    uint16_t touched[OPENVIEW_TOUCHED];          /* room ids (words: rooms past 255) */
    uint16_t stamp[MAPMAKER_ROOF_ROOMS];         /* group mark + 1 per room */
    float box[MAPMAKER_ROOF_ROOMS][6];           /* room r-1: min xyz, max xyz (this build) */
    uint8_t has_box[MAPMAKER_ROOF_ROOMS];
    unsigned build_logs, peak_logs, peak;
    unsigned long long frames, widened, added, overflow;
} mapmaker_openview;

/* The other room of each portal of room r (1..n) into out[]; returns the
 * count. Bad pointers and out-of-range ids are skipped. */
static unsigned mapmaker_openview_neighbours(uint32_t r,uint32_t n,uint32_t *out)
{
    uint32_t records=WORD_AT(OPENVIEW_RECORDS),portals=WORD_AT(OPENVIEW_PORTALS),pmax=WORD_AT(OPENVIEW_PORTAL_COUNT);
    uint32_t list,count,i,id,p,a,b;
    unsigned k=0;
    if(!MAPMAKER_PTR_OK(records,(r+1u)*OPENVIEW_RECORD) || !MAPMAKER_PTR_OK(list=WORD_AT(records+r*OPENVIEW_RECORD+4u),4u))
        return 0;
    count=WORD_AT(list);
    if(count>OPENVIEW_PORTALS_PER_ROOM || !MAPMAKER_PTR_OK(list,4u+count*4u) || !MAPMAKER_PTR_OK(portals,(pmax+1u)*4u)) return 0;
    for(i=0;i<count;i++) {
        if((id=WORD_AT(list+4u+i*4u))>pmax || !MAPMAKER_PTR_OK(p=WORD_AT(portals+id*4u),8u)) continue;
        a=WORD_AT(p);b=WORD_AT(p+4u);
        if(a!=r && b!=r) continue;
        if((a=a==r?b:a)<1u || a>n) continue;
        out[k++]=a;
    }
    return k;
}

/* Room r's box from its record (min xyz +0x10, max xyz +0x1C); 0 when the
 * records are missing or the box is empty or absurd. */
static int mapmaker_openview_box(uint32_t r,float *b)
{
    uint32_t records=WORD_AT(OPENVIEW_RECORDS),i;
    if(!MAPMAKER_PTR_OK(records,(r+1u)*OPENVIEW_RECORD)) return 0;
    for(i=0;i<6u;i++) {
        b[i]=OPENVIEW_F(records+r*OPENVIEW_RECORD+OPENVIEW_BOX+i*4u);
        if(!(b[i]>-OPENVIEW_FAR && b[i]<OPENVIEW_FAR)) return 0;   /* NaN too */
    }
    return b[0]<b[3] && b[1]<b[4] && b[2]<b[5];
}
static int mapmaker_openview_touch(const float *a,const float *b)
{
    return a[0]-OPENVIEW_TOUCH<=b[3] && b[0]-OPENVIEW_TOUCH<=a[3] && a[1]-OPENVIEW_TOUCH<=b[4] &&
           b[1]-OPENVIEW_TOUCH<=a[4] && a[2]-OPENVIEW_TOUCH<=b[5] && b[2]-OPENVIEW_TOUCH<=a[5];
}

static void mapmaker_openview_build(uint32_t n)
{
    static uint32_t queue[MAPMAKER_ROOF_ROOMS];
    uint32_t next[OPENVIEW_PORTALS_PER_ROOM];
    unsigned at=0,g=0,r,portals=0;
    memset(mapmaker_openview.stamp,0,sizeof mapmaker_openview.stamp);   /* 0: no group yet */
    mapmaker_openview.groups=mapmaker_openview.touched_total=mapmaker_openview.open=mapmaker_openview.beside=0;
    for(r=1;r<=n;r++) mapmaker_openview.has_box[r-1u]=(uint8_t)mapmaker_openview_box(r,mapmaker_openview.box[r-1u]);
    for(r=1;r<=n;r++) {
        unsigned head=0,tail=0,i,k,first=at,mark,o;
        if(!mapmaker_roof_play.open[r-1u] || mapmaker_openview.stamp[r-1u]) continue;
        /* breadth-first over open rooms: the group's members, in order */
        mark=g+1u;
        mapmaker_openview.stamp[r-1u]=(uint16_t)mark;queue[tail++]=r;
        while(head<tail) {
            uint32_t m=queue[head++];
            k=mapmaker_openview_neighbours(m,n,next);portals+=k;
            for(i=0;i<k;i++)
                if(mapmaker_roof_play.open[next[i]-1u] && !mapmaker_openview.stamp[next[i]-1u]) {
                    mapmaker_openview.stamp[next[i]-1u]=(uint16_t)mark;queue[tail++]=next[i];
                }
        }
        if(at+tail>OPENVIEW_TOUCHED) { mapmaker_openview.overflow++;break; }
        for(i=0;i<tail;i++) mapmaker_openview.touched[at++]=(uint16_t)queue[i];
        mapmaker_openview.members[g]=(uint16_t)tail;mapmaker_openview.open+=tail;
        /* closed rooms one portal away, listed once per group (stamped with its
         * mark; marks are unique, so a room next to two areas is in both lists) */
        for(i=0;i<tail;i++) {
            unsigned j;
            k=mapmaker_openview_neighbours(queue[i],n,next);
            for(j=0;j<k;j++) {
                uint32_t o=next[j];
                if(mapmaker_roof_play.open[o-1u] || mapmaker_openview.stamp[o-1u]==mark) continue;
                if(at>=OPENVIEW_TOUCHED) { mapmaker_openview.overflow++;break; }
                mapmaker_openview.stamp[o-1u]=(uint16_t)mark;mapmaker_openview.touched[at++]=(uint16_t)o;
            }
        }
        /* closed rooms whose box touches a member's box, portal or not */
        for(o=1;o<=n;o++) {
            if(mapmaker_roof_play.open[o-1u] || mapmaker_openview.stamp[o-1u]==mark || !mapmaker_openview.has_box[o-1u]) continue;
            for(i=0;i<tail;i++)
                if(mapmaker_openview.has_box[queue[i]-1u] &&
                   mapmaker_openview_touch(mapmaker_openview.box[queue[i]-1u],mapmaker_openview.box[o-1u])) break;
            if(i==tail) continue;
            if(at>=OPENVIEW_TOUCHED) { mapmaker_openview.overflow++;break; }
            mapmaker_openview.stamp[o-1u]=(uint16_t)mark;mapmaker_openview.touched[at++]=(uint16_t)o;
            mapmaker_openview.beside++;
        }
        mapmaker_openview.group_first[g]=(uint16_t)first;
        g++;
        mapmaker_openview.group_first[g]=(uint16_t)at;
        if(g>=MAPMAKER_ROOF_ROOMS) break;
    }
    mapmaker_openview.groups=g;mapmaker_openview.touched_total=at;
    mapmaker_openview.ready=g?1:-1;
    /* every room drawn across open air is drawn two-sided (pc_mapmaker_roof_draw) */
    memset(mapmaker_roof_play.twosided,0,sizeof mapmaker_roof_play.twosided);
    mapmaker_roof_play.twosided_rooms=0;
    for(r=0;r<at;r++) {
        uint32_t room=mapmaker_openview.touched[r];
        if(room<1u || room>MAPMAKER_ROOF_ROOMS || mapmaker_roof_play.twosided[room-1u]) continue;
        mapmaker_roof_play.twosided[room-1u]=1;mapmaker_roof_play.twosided_rooms++;
    }
    mapmaker_openview.peak=mapmaker_openview.peak_logs=0;
    if(mapmaker_openview.build_logs<32u) {
        mapmaker_openview.build_logs++;
        fprintf(stderr,"[mapmaker] openview: %u open-air rooms in %u area%s; %u rooms drawn across them at most (%u portals read, %u rooms, %u beside by box, %u two-sided)%s\n",
                mapmaker_openview.open,g,g==1u?"":"s",at,portals,(unsigned)n,mapmaker_openview.beside,mapmaker_roof_play.twosided_rooms,
                mapmaker_openview.overflow?"; TOO MANY, some left stock":"");
        fflush(stderr);
    }
}

/* WRAP sub_00089B90 (portal walk of the current view; no arguments, plain
 * ret, reached by tail jumps). */
void pc_mapmaker_openview_walk(void (*original)(void))
{
    uint32_t view,n,base,g;
    unsigned widened=0,added=0;
    int32_t deepest;
#ifdef MAPMAKER_SKELVIS
    mapmaker_skelvis_pass();       /* pc_mapmaker_skelvis.inc (log only): ends the previous pass */
#endif
    original();
#ifdef MAPMAKER_CAMERA_VIEW_DIAG
    mapmaker_camera_view_diag();   /* pc_mapmaker_camera.inc: what this view reaches */
#endif
    if(!mapmaker_roof_play.open_rooms || WORD_AT(OPENVIEW_SCENE)!=OPENVIEW_PLAY) return;
    mapmaker_limits_read();
    if(!mapmaker_limits.limits) return;
    view=WORD_AT(OPENVIEW_VIEW);n=WORD_AT(OPENVIEW_ROOM_COUNT);
    if(n<1u || n>=MAPMAKER_ROOF_ROOMS || !MAPMAKER_PTR_OK(view,OPENVIEW_RECT+(n+1u)*12u)) return;
    if(!mapmaker_openview.ready || mapmaker_openview.generation!=mapmaker_roof_play.generation ||
       mapmaker_openview.rooms!=n || mapmaker_openview.portals!=WORD_AT(OPENVIEW_PORTAL_COUNT)) {
        mapmaker_openview.generation=mapmaker_roof_play.generation;
        mapmaker_openview.rooms=n;mapmaker_openview.portals=WORD_AT(OPENVIEW_PORTAL_COUNT);
        mapmaker_openview_build(n);
    }
    if(mapmaker_openview.ready<0) return;
    base=view+OPENVIEW_RECT;
    deepest=(int32_t)WORD_AT(view+OPENVIEW_DEEPEST);
    for(g=0;g<mapmaker_openview.groups;g++) {
        unsigned first=mapmaker_openview.group_first[g],end=mapmaker_openview.group_first[g+1u],i;
        unsigned members=first+mapmaker_openview.members[g];
        int32_t depth=-1;
        int16_t x0=0,y0=0,x1=0,y1=0;
        for(i=first;i<members;i++) {
            uint32_t e=base+mapmaker_openview.touched[i]*12u;
            int32_t d=(int32_t)WORD_AT(e);
            int16_t a=OPENVIEW_H(e+4u),b=OPENVIEW_H(e+6u),c=OPENVIEW_H(e+8u),f=OPENVIEW_H(e+10u);
            if(d<0 || !(a<c && b<f)) continue;
            if(depth<0) { x0=a;y0=b;x1=c;y1=f;depth=d; }
            else {
                if(a<x0) x0=a;
                if(b<y0) y0=b;
                if(c>x1) x1=c;
                if(f>y1) y1=f;
                if(d<depth) depth=d;
            }
        }
        if(depth<0) continue;                       /* no room of this area is visible */
        depth=depth+1>(int32_t)OPENVIEW_DEPTH_CAP?(int32_t)OPENVIEW_DEPTH_CAP:depth+1;
        for(i=first;i<end;i++) {
            uint32_t e=base+mapmaker_openview.touched[i]*12u;
            int changed=0;
            if((int32_t)WORD_AT(e)<0) {
                WORD_AT(e)=(uint32_t)depth;added++;changed=1;
                if(deepest<depth) deepest=depth;
            }
            if(x0<OPENVIEW_H(e+4u)) { OPENVIEW_H(e+4u)=x0;changed=1; }
            if(y0<OPENVIEW_H(e+6u)) { OPENVIEW_H(e+6u)=y0;changed=1; }
            if(x1>OPENVIEW_H(e+8u)) { OPENVIEW_H(e+8u)=x1;changed=1; }
            if(y1>OPENVIEW_H(e+10u)) { OPENVIEW_H(e+10u)=y1;changed=1; }
            widened+=(unsigned)changed;
        }
    }
    if((int32_t)WORD_AT(view+OPENVIEW_DEEPEST)<deepest) WORD_AT(view+OPENVIEW_DEEPEST)=(uint32_t)deepest;
    mapmaker_openview.frames++;mapmaker_openview.widened+=widened;mapmaker_openview.added+=added;
    if(widened>mapmaker_openview.peak && mapmaker_openview.peak_logs<4u) {
        mapmaker_openview.peak=widened;mapmaker_openview.peak_logs++;
        fprintf(stderr,"[mapmaker] openview: %u rooms widened across open air (%u not reached by the portal walk), camera room %u\n",
                widened,added,(unsigned)WORD_AT(view+0x2A0u));
        fflush(stderr);
    }
}
