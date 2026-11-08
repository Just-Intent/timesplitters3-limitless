/* US 45410066 MapMaker map report (docs/mapmaker/mapmaker-report.md).
 *
 * One bounded report per MapMaker level start, in the log (stderr,
 * game.err): what the map holds against its caps and the problems that
 * cause bugs seen in playtests, each with the grid position to look at in
 * the editor. Read-only: it never writes guest memory.
 *
 * Called by pc_mapmaker_validate_level (WRAP sub_0006D7A0, area bugfix,
 * pc_mapmaker_validate.inc) after the original, in scene 100 on the one-time
 * map pass ([0x006F32EC] == 0) and before the validator puts back the tags
 * it hid, so chunks the validator refused are skipped here too. By then the
 * level load (sub_001CDE30, 0x0003D645, before sub_0006D7A0 at 0x0003D64A)
 * has built the navigation: the stitcher sub_0009DEB0 (0x001CE0CE) and the
 * group matrix sub_00275F70 (0x001CE14A via sub_00158BA0) have run.
 *
 * Inputs (CONFIRMED unless marked):
 *  - the map container [0x00784014] (pc_mapmaker_validate.inc header):
 *    0x68 info (+4 piece count, +0x62 name, 32 bytes); 0x69 piece records
 *    of +0x24 low word bytes (0x28 + pads*8), +0 tile id, +4..+7 bytes
 *    x, z, floor, rotation (sub_0008A780 0x0008A87E..0x0008A912), word +0x26
 *    the roof mark 0x5201..0x5203 (pc_mapmaker_roof.inc), then +0x1E items
 *    of (byte +0x14 * 4 + 0x1B) & ~3 bytes, item id int16 +0xE;
 *    0x6C Story AI: name +0, point count word +0x2C (<= 8), points
 *    {u16 pad id, u8 piece, u8 flags} at +0x44 + 4k (play uses the pad id
 *    only, 0x00241E28), boss byte +0x29 (0xB0 | sub-boss 4 | level - 1);
 *    0x6F logic records: type +0x80, type 3 count +0x20, type 0xA count
 *    +0x70, piece indices at +0x24 + 8k (type 0xA one-based); the saver
 *    writes the piece count for a piece it cannot find (0x0030177E);
 *  - the item catalogue 0x00524F58 + id * 0xC4 + 2 (int16 category) and
 *    the live category maxima 0x00524BE0 + category * 24 + 4;
 *  - navigation: regular pads [0x00715F2C] (0x20 each: +0 id, +8 group,
 *    +0xC/+0x10/+0x14 x, y, z), count [0x007B6664]; links [0x00715F30]
 *    (0x14 each: +0 flags, +4/+8 pad handles), count [0x007B6660]; pad
 *    groups [0x007B666C]; cover pads [0x007B65C0]; the group A* crosses a
 *    link only with flag 0x10000 (pc_mapmaker_astar.inc); pad ids: regular
 *    0x3E9 + index below 8000, 0x5B29 + index - 8000 above, cover 0x2329..
 *    (pc_mapmaker_pads.inc); cover pads [0x00715F34] (+0 id, +8 owner: the
 *    regular pad id a cover pad belongs to); world x = cell * 6.0 [0x00475D30], y = floor *
 *    5.0 [0x004760CC] (0x0008A8D3..0x0008A905);
 *  - memory: the free count of the Level/top-down heaps [0x006B8384]
 *    (negative = overrun, pc_mapmaker_memory.inc) and the arena [0x004C0450].
 *
 * Checks. Reliable (from the data the game uses): counts against caps
 * (over = problem, > 90% = near limit for caps of 10 and up), chunks the validator skipped, a
 * Story AI with no point (no position), a point whose pad id is no pad, a
 * logic entry naming a piece past the placed ones, two tiles with the same
 * grid cell and floor (overlap) when TSFP_MAPMAKER_OVERLAP is off, a
 * teleporter colour with a single teleporter, a level heap overrun.
 * Heuristic (marked "possible problem"): pad areas not linked to the main
 * area (the largest one; doors and one-way links count as open and two-way),
 * Story AI starting in or patrolling into such an area, odd teleporter
 * counts, little level memory left.
 *
 * Requires the includer's WORD_AT and g_xbox_mem_offset; uses
 * mapmaker_limits (pc_mapmaker_limits.inc) when it is included. */
#ifndef PC_MAPMAKER_REPORT_INC
#define PC_MAPMAKER_REPORT_INC 1
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAPMAKER_PTR_OK
#define MAPMAKER_PTR_OK(p,n) ((((uint32_t)(p))>=0x00010000u && ((uint32_t)(p))<0x04000000u-(uint32_t)(n)) || \
                              (((uint32_t)(p))>=0x80000000u && ((uint32_t)(p))<0x84000000u-(uint32_t)(n)))
#endif

#define MAPMAKER_REPORT_PROBLEMS 20u           /* problem lines shown; the rest are counted */
#define MAPMAKER_REPORT_LINE 200u
#define MAPMAKER_REPORT_TEXT 8192u             /* the last report, kept for the fixture */
#define MAPMAKER_REPORT_NEAR 90u               /* percent */
#define MAPMAKER_REPORT_NEAR_MIN 10u           /* caps below this are not "near" when reached */
#define MAPMAKER_REPORT_SPAN 0x00080000u       /* container.json: 512 KB */
#define MAPMAKER_REPORT_MAGIC 0x00C77667u
#define MAPMAKER_REPORT_TAG(n) (0x00C77600u+(uint32_t)(n))
#define MAPMAKER_REPORT_INFO_NAME 0x62u
#define MAPMAKER_REPORT_NAME 32u
#define MAPMAKER_REPORT_RECORD 0x28u
#define MAPMAKER_REPORT_ITEM_MIN 0x18u
#define MAPMAKER_REPORT_ITEM_IDS 80u
#define MAPMAKER_REPORT_FIRST_ITEM 10u         /* ids 0..9 are not counted as items (restable) */
#define MAPMAKER_REPORT_CATEGORIES 37u
#define MAPMAKER_REPORT_CATALOGUE 0x00524F58u
#define MAPMAKER_REPORT_CATALOGUE_STRIDE 0xC4u
#define MAPMAKER_REPORT_MAXIMA 0x00524BE0u
#define MAPMAKER_REPORT_TELEPORT_FIRST 14u     /* categories 14..18: one per colour */
#define MAPMAKER_REPORT_TELEPORT_LAST 18u
#define MAPMAKER_REPORT_PIECES_MAX 2048u       /* records kept for the overlap test */
#define MAPMAKER_REPORT_SAI_MAX 1024u
#define MAPMAKER_REPORT_POINTS 8u
#define MAPMAKER_REPORT_SAI_POINTS 0x44u
#define MAPMAKER_REPORT_SAI_COUNT 0x2Cu
#define MAPMAKER_REPORT_SAI_BOSS 0x29u
#define MAPMAKER_REPORT_LOGIC_TYPE 0x80u
#define MAPMAKER_REPORT_LOGIC_LINKS 13u
#define MAPMAKER_REPORT_ROOF_MARK 0x5200u
#define MAPMAKER_REPORT_CELL 6.0f              /* [0x00475D30] */
#define MAPMAKER_REPORT_FLOOR 5.0f             /* [0x004760CC] */
/* Caps with TSFP_MAPMAKER_LIMITS=1 (test_mapmaker_report.py checks them
 * against pc_mapmaker_limits.inc / pc_mapmaker_editor.inc) and stock. */
#define MAPMAKER_REPORT_CAP_PIECES 1023u       /* MAPMAKER_LIMIT_PIECES */
#define MAPMAKER_REPORT_CAP_ITEMS 500u         /* MAPMAKER_LIMIT_SUB */
#define MAPMAKER_REPORT_CAP_OBJECTS 2047u      /* MAPMAKER_LIMIT_OBJECTS */
#define MAPMAKER_REPORT_OBJECT_RESERVE 128u    /* MAPMAKER_OBJECTS_RESERVE */
#define MAPMAKER_REPORT_CAP_SAI 511u           /* MAPMAKER_EDITOR_SAI_LIMIT */
#define MAPMAKER_REPORT_CAP_LOGIC 400u         /* MAPMAKER_EDITOR_RECORDS_CAP */
#define MAPMAKER_REPORT_STOCK_PIECES 200u
#define MAPMAKER_REPORT_STOCK_ITEMS 50u
#define MAPMAKER_REPORT_STOCK_SAI 50u          /* MAPMAKER_EDITOR_SAI_STOCK */
#define MAPMAKER_REPORT_STOCK_LOGIC 100u
/* Play pools with TSFP_MAPMAKER_POOLS on (the default). */
#define MAPMAKER_REPORT_POOL_PADS 16384u       /* MAPMAKER_POOL_PADS */
#define MAPMAKER_REPORT_POOL_COVER 14336u      /* MAPMAKER_PADS_COVER_RANGE */
#define MAPMAKER_REPORT_POOL_LINKS 32512u      /* MAPMAKER_POOL_LINKS */
#define MAPMAKER_REPORT_POOL_GROUPS 2047u      /* MAPMAKER_LIMIT_GROUPS */
#define MAPMAKER_REPORT_SCENE 0x0079094Cu
#define MAPMAKER_REPORT_MODE 0x007DE455u       /* byte: 10 = story */
#define MAPMAKER_REPORT_PADS 0x00715F2Cu
#define MAPMAKER_REPORT_PAD_COUNT 0x007B6664u
#define MAPMAKER_REPORT_LINKS 0x00715F30u
#define MAPMAKER_REPORT_LINK_COUNT 0x007B6660u
#define MAPMAKER_REPORT_GROUP_COUNT 0x007B666Cu
#define MAPMAKER_REPORT_COVER_COUNT 0x007B65C0u
#define MAPMAKER_REPORT_COVER_PADS 0x00715F34u  /* 0x20 each: +0 id, +8 owner (a regular pad id) */
#define MAPMAKER_REPORT_LINK_GROUPS 0x00010000u
#define MAPMAKER_REPORT_HEAP_FREE 0x006B8384u
#define MAPMAKER_REPORT_ARENA 0x004C0450u
#define MAPMAKER_REPORT_LOW_MEMORY 1024u       /* KB: heuristic */
#define MAPMAKER_REPORT_ISLANDS 3u             /* separate pad areas listed */
#define MAPMAKER_REPORT_REGULAR_ID 0x3E9u
#define MAPMAKER_REPORT_COVER_ID 0x2329u
#define MAPMAKER_REPORT_HIGH_ID 0x5B29u
#define MAPMAKER_REPORT_HIGH_END 0x7BE9u
#define MAPMAKER_REPORT_LOW_PADS 8000u
#define MAPMAKER_REPORT_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

typedef struct MapReportPiece { uint16_t tile; int8_t x,z,floor; uint8_t mode; uint16_t index; } MapReportPiece;
typedef struct MapReportAi {
    char name[MAPMAKER_REPORT_NAME+1];
    uint8_t points,boss;
    uint16_t pad[MAPMAKER_REPORT_POINTS];
} MapReportAi;

static struct {
    unsigned pieces,items,sai,bosses,subbosses,logic,unplaced,open[4],hidden,reports;
    unsigned category[MAPMAKER_REPORT_CATEGORIES];
    int walk_ok,story,limits,overlap;
    char name[MAPMAKER_REPORT_NAME+1];
    MapReportPiece piece[MAPMAKER_REPORT_PIECES_MAX];
    unsigned kept;
    MapReportAi ai[MAPMAKER_REPORT_SAI_MAX];
    unsigned problems;
    char problem[MAPMAKER_REPORT_PROBLEMS][MAPMAKER_REPORT_LINE];
    char text[MAPMAKER_REPORT_TEXT];
    size_t length;
    /* navigation */
    int nav;
    uint32_t pads,links,groups,cover,pool,cover_pool;
    int32_t *parent;
    uint32_t *size;
    uint32_t main;             /* root of the main (largest) area */
    uint32_t areas;
} mapmaker_report;

/* Category names (docs/mapmaker/mapmaker-restable.md, pc_mapmaker_limits.inc). */
static const char *const mapmaker_report_names[MAPMAKER_REPORT_CATEGORIES]={
    [8]="weapons",[9]="ammo",[10]="red keycards",[11]="blue keycards",[12]="yellow keycards",[13]="green keycards",
    [14]="teleporters (colour 1)",[15]="teleporters (colour 2)",[16]="teleporters (colour 3)",[17]="teleporters (colour 4)",
    [18]="teleporters (colour 5)",[19]="health",[20]="armour",[21]="power-ups",[25]="movers",[26]="props",
    [27]="turrets (type 1)",[28]="turrets (type 2)",[29]="turrets (type 3)",[31]="special pickups",[32]="markers",
    [33]="cover props",[35]="vehicles",
};

static uint32_t mapmaker_report_u16(uint32_t a) { return (uint32_t)MAPMAKER_REPORT_B(a)|(uint32_t)MAPMAKER_REPORT_B(a+1u)<<8; }

static void mapmaker_report_emit(const char *format,...)
{
    char line[MAPMAKER_REPORT_LINE+64];
    va_list a;
    int n;
    va_start(a,format);
    n=vsnprintf(line,sizeof line,format,a);
    va_end(a);
    if(n<0) return;
    fprintf(stderr,"[mapmaker] map report: %s\n",line);
    if(mapmaker_report.length+strlen(line)+2u<MAPMAKER_REPORT_TEXT) {
        memcpy(mapmaker_report.text+mapmaker_report.length,line,strlen(line));
        mapmaker_report.length+=strlen(line);
        mapmaker_report.text[mapmaker_report.length++]='\n';
        mapmaker_report.text[mapmaker_report.length]=0;
    }
}

/* Problem lines are kept in the order the checks run (most serious first). */
static void mapmaker_report_problem(int heuristic,const char *format,...)
{
    va_list a;
    unsigned k=mapmaker_report.problems++;
    size_t n;
    if(k>=MAPMAKER_REPORT_PROBLEMS) return;
    n=(size_t)snprintf(mapmaker_report.problem[k],MAPMAKER_REPORT_LINE,"%s",heuristic?"possible problem: ":"problem: ");
    va_start(a,format);
    vsnprintf(mapmaker_report.problem[k]+n,MAPMAKER_REPORT_LINE-n,format,a);
    va_end(a);
}

static void mapmaker_report_cap(const char *what,unsigned used,unsigned cap,char *nearby,size_t room)
{
    size_t n;
    if(!cap) return;
    if(used>cap) mapmaker_report_problem(0,"%s %u is over the cap of %u",what,used,cap);
    else if((unsigned long long)used*100u>(unsigned long long)cap*MAPMAKER_REPORT_NEAR && cap>=MAPMAKER_REPORT_NEAR_MIN) {
        n=strlen(nearby);
        snprintf(nearby+n,room-n,"%s%s %u/%u (%u%%)",n?", ":"",what,used,cap,(unsigned)((unsigned long long)used*100u/cap));
    }
}

static int mapmaker_report_category(uint32_t id)
{
    int32_t c;
    uint32_t a=MAPMAKER_REPORT_CATALOGUE+id*MAPMAKER_REPORT_CATALOGUE_STRIDE+2u;
    if(id>=MAPMAKER_REPORT_ITEM_IDS || !MAPMAKER_PTR_OK(a,2u)) return -1;
    c=(int32_t)(int16_t)mapmaker_report_u16(a);
    return c>=0 && c<(int32_t)MAPMAKER_REPORT_CATEGORIES?c:-1;
}

/* ---- the container -------------------------------------------------------- */
static void mapmaker_report_pieces(uint32_t r,uint32_t end)
{
    while(end-r>=MAPMAKER_REPORT_RECORD) {
        uint32_t length=mapmaker_report_u16(r+0x24u),items=mapmaker_report_u16(r+0x1Eu),mark=mapmaker_report_u16(r+0x26u),at,k;
        if(length<MAPMAKER_REPORT_RECORD || length>end-r) { mapmaker_report.walk_ok=0;return; }
        if(mapmaker_report.kept<MAPMAKER_REPORT_PIECES_MAX) {
            MapReportPiece *p=&mapmaker_report.piece[mapmaker_report.kept++];
            p->tile=(uint16_t)mapmaker_report_u16(r);
            p->x=(int8_t)MAPMAKER_REPORT_B(r+4u);p->z=(int8_t)MAPMAKER_REPORT_B(r+5u);p->floor=(int8_t)MAPMAKER_REPORT_B(r+6u);
            p->mode=(uint8_t)((mark&0xFF00u)==MAPMAKER_REPORT_ROOF_MARK && (mark&0xFFu)<=3u?mark&0xFFu:0u);
            p->index=(uint16_t)mapmaker_report.pieces;
            mapmaker_report.open[p->mode]++;
        }
        mapmaker_report.pieces++;
        for(at=r+length,k=0;k<items;k++) {
            uint32_t bytes,id;
            int c;
            if(end-at<MAPMAKER_REPORT_ITEM_MIN) { mapmaker_report.walk_ok=0;return; }
            bytes=(MAPMAKER_REPORT_B(at+0x14u)*4u+0x1Bu)&~3u;
            if(bytes>end-at) { mapmaker_report.walk_ok=0;return; }
            id=mapmaker_report_u16(at+0xEu);
            if(id>=MAPMAKER_REPORT_FIRST_ITEM && id<MAPMAKER_REPORT_ITEM_IDS) mapmaker_report.items++;
            if((c=mapmaker_report_category(id))>=0) mapmaker_report.category[c]++;
            at+=bytes;
        }
        r=at;
    }
}

static void mapmaker_report_ai(uint32_t d,uint32_t size)
{
    MapReportAi *ai;
    uint32_t n,k,b;
    if(size<MAPMAKER_REPORT_SAI_POINTS) return;
    n=mapmaker_report_u16(d+MAPMAKER_REPORT_SAI_COUNT);
    b=MAPMAKER_REPORT_B(d+MAPMAKER_REPORT_SAI_BOSS);
    if((b&0xF8u)==0xB0u) mapmaker_report.bosses++;
    else if((b&0xF8u)==0xB4u) { mapmaker_report.bosses++;mapmaker_report.subbosses++; }
    if(!n) mapmaker_report.unplaced++;
    if(mapmaker_report.sai<MAPMAKER_REPORT_SAI_MAX) {
        ai=&mapmaker_report.ai[mapmaker_report.sai];
        for(k=0;k<MAPMAKER_REPORT_NAME;k++) {
            char c=(char)MAPMAKER_REPORT_B(d+k);
            if(!c) break;
            ai->name[k]=c>=0x20 && c<0x7F?c:'?';
        }
        ai->name[k]=0;
        if(n>MAPMAKER_REPORT_POINTS) n=MAPMAKER_REPORT_POINTS;
        if(n>(size-MAPMAKER_REPORT_SAI_POINTS)/4u) n=(size-MAPMAKER_REPORT_SAI_POINTS)/4u;
        ai->points=(uint8_t)n;ai->boss=(uint8_t)b;
        for(k=0;k<n;k++) ai->pad[k]=(uint16_t)mapmaker_report_u16(d+MAPMAKER_REPORT_SAI_POINTS+4u*k);
    }
    mapmaker_report.sai++;
}

/* Logic entries naming a piece past the placed ones (pass 2: the piece
 * count is known). */
static void mapmaker_report_logic(uint32_t d,uint32_t size,unsigned record)
{
    uint32_t type,n,k;
    int base;
    if(size<0x8Cu) return;
    type=WORD_AT(d+MAPMAKER_REPORT_LOGIC_TYPE);
    if(type==3u) { n=WORD_AT(d+0x20u);base=0; }
    else if(type==0xAu) { n=WORD_AT(d+0x70u);base=1; }
    else return;
    if(n>MAPMAKER_REPORT_LOGIC_LINKS) n=MAPMAKER_REPORT_LOGIC_LINKS;
    for(k=0;k<n;k++) {
        int32_t idx=(int32_t)WORD_AT(d+0x24u+8u*k)-base;
        if(idx>=(int32_t)mapmaker_report.pieces)
            mapmaker_report_problem(0,"logic entry %u (link %u) names tile #%d, which is not on the map (deleted?)",
                                    record+1u,k+1u,idx+1);
    }
}

/* One pass over [base+8, base+span): pass 0 counts and keeps records, pass
 * 1 checks the logic entries (the piece count is known by then). 0 = no
 * usable container. */
static int mapmaker_report_scan(uint32_t base,uint32_t span,int pass)
{
    uint32_t c=base+8u,end=base+span;
    unsigned record=0,steps;
    if(!MAPMAKER_PTR_OK(base,span) || WORD_AT(base)!=MAPMAKER_REPORT_MAGIC) return 0;
    if(!pass) mapmaker_report.walk_ok=1;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),size=WORD_AT(c+4u),d=c+8u;
        if(tag==MAPMAKER_REPORT_TAG(0x6A)) break;
        if(size>end-d) { if(!pass) mapmaker_report.walk_ok=0;break; }
        if(pass) { if(tag==MAPMAKER_REPORT_TAG(0x6F)) mapmaker_report_logic(d,size,record++); }
        else if(tag==MAPMAKER_REPORT_TAG(0x68) && size>=MAPMAKER_REPORT_INFO_NAME+MAPMAKER_REPORT_NAME) {
            unsigned k;
            for(k=0;k<MAPMAKER_REPORT_NAME;k++) {
                char ch=(char)MAPMAKER_REPORT_B(d+MAPMAKER_REPORT_INFO_NAME+k);
                if(!ch) break;
                mapmaker_report.name[k]=ch>=0x20 && ch<0x7F?ch:'?';
            }
            mapmaker_report.name[k]=0;
        } else if(tag==MAPMAKER_REPORT_TAG(0x69)) mapmaker_report_pieces(d,d+size);
        else if(tag==MAPMAKER_REPORT_TAG(0x6C)) mapmaker_report_ai(d,size);
        else if(tag==MAPMAKER_REPORT_TAG(0x6F)) mapmaker_report.logic++;
        c=d+size;
    }
    return 1;
}

static int mapmaker_report_compare(const void *a,const void *b)
{
    const MapReportPiece *p=(const MapReportPiece *)a,*q=(const MapReportPiece *)b;
    if(p->floor!=q->floor) return p->floor<q->floor?-1:1;
    if(p->x!=q->x) return p->x<q->x?-1:1;
    if(p->z!=q->z) return p->z<q->z?-1:1;
    return p->index<q->index?-1:p->index>q->index;
}

/* ---- navigation --------------------------------------------------------- */
static uint32_t mapmaker_report_root(uint32_t i)
{
    int32_t *p=mapmaker_report.parent;
    uint32_t r=i;
    while(p[r]!=(int32_t)r) r=(uint32_t)p[r];
    while(p[i]!=(int32_t)r) { uint32_t n=(uint32_t)p[i];p[i]=(int32_t)r;i=n; }
    return r;
}
static void mapmaker_report_join(uint32_t a,uint32_t b)
{
    a=mapmaker_report_root(a);b=mapmaker_report_root(b);
    if(a!=b) mapmaker_report.parent[b]=(int32_t)a;
}
static float mapmaker_report_f(uint32_t a) { float f; uint32_t w=WORD_AT(a); memcpy(&f,&w,4); return f; }
static void mapmaker_report_where(uint32_t pad,int *x,int *z,int *f)
{
    uint32_t p=mapmaker_report.pool+pad*0x20u;
    float fx=mapmaker_report_f(p+0xCu),fy=mapmaker_report_f(p+0x10u),fz=mapmaker_report_f(p+0x14u);
    if(!isfinite(fx) || !isfinite(fy) || !isfinite(fz)) { *x=*z=*f=0;return; }
    *x=(int)floorf(fx/MAPMAKER_REPORT_CELL);*z=(int)floorf(fz/MAPMAKER_REPORT_CELL);
    *f=(int)floorf(fy/MAPMAKER_REPORT_FLOOR+0.5f);
}

/* Pad handle of a saved regular pad id, -1 = no pad. */
static int32_t mapmaker_report_regular(uint32_t id)
{
    uint32_t h;
    if(id>=MAPMAKER_REPORT_REGULAR_ID && id<MAPMAKER_REPORT_COVER_ID) h=id-MAPMAKER_REPORT_REGULAR_ID;
    else if(id>=MAPMAKER_REPORT_HIGH_ID && id<MAPMAKER_REPORT_HIGH_END) h=id-MAPMAKER_REPORT_HIGH_ID+MAPMAKER_REPORT_LOW_PADS;
    else return -1;
    if(h>=mapmaker_report.pads || WORD_AT(mapmaker_report.pool+h*0x20u)!=id) return -1;
    return (int32_t)h;
}
/* Regular pad handle of any saved pad id: a cover pad (where Story AI points
 * usually are) stands for its owner. -1 = no pad, -2 = not traced (cover
 * pool unreadable or a cover pad without an owner). */
static int32_t mapmaker_report_pad(uint32_t id)
{
    uint32_t h,owner;
    if(id<MAPMAKER_REPORT_COVER_ID || id>=MAPMAKER_REPORT_HIGH_ID) return mapmaker_report_regular(id);
    h=id-MAPMAKER_REPORT_COVER_ID;
    if(!mapmaker_report.cover_pool) return -2;
    if(h>=mapmaker_report.cover || WORD_AT(mapmaker_report.cover_pool+h*0x20u)!=id) return -1;
    owner=mapmaker_report_u16(mapmaker_report.cover_pool+h*0x20u+8u);
    if(owner==0xFFFFu) return -2;
    return mapmaker_report_regular(owner);
}

static void mapmaker_report_nav_free(void)
{
    free(mapmaker_report.parent);free(mapmaker_report.size);
    mapmaker_report.parent=NULL;mapmaker_report.size=NULL;mapmaker_report.nav=0;
}

/* Connected pad areas: pads of one group, and the two pads of each link the
 * group A* may cross (flag 0x10000), whatever the door state. */
static void mapmaker_report_nav(void)
{
    uint32_t pads=WORD_AT(MAPMAKER_REPORT_PAD_COUNT),links=WORD_AT(MAPMAKER_REPORT_LINK_COUNT);
    uint32_t pool=WORD_AT(MAPMAKER_REPORT_PADS),lpool=WORD_AT(MAPMAKER_REPORT_LINKS),i,best=0;
    int32_t *first;
    mapmaker_report.pads=pads;mapmaker_report.links=links;mapmaker_report.pool=pool;
    mapmaker_report.groups=WORD_AT(MAPMAKER_REPORT_GROUP_COUNT);mapmaker_report.cover=WORD_AT(MAPMAKER_REPORT_COVER_COUNT);
    if(mapmaker_report.cover<=0x8000u && MAPMAKER_PTR_OK(WORD_AT(MAPMAKER_REPORT_COVER_PADS),mapmaker_report.cover*0x20u+0x20u))
        mapmaker_report.cover_pool=WORD_AT(MAPMAKER_REPORT_COVER_PADS);
    if(!pads || pads>0x8000u || links>0x10000u || !MAPMAKER_PTR_OK(pool,pads*0x20u) ||
       (links && !MAPMAKER_PTR_OK(lpool,links*0x14u))) return;
    mapmaker_report.parent=(int32_t *)malloc(pads*sizeof(int32_t));
    mapmaker_report.size=(uint32_t *)calloc(pads,sizeof(uint32_t));
    first=(int32_t *)malloc(0x10000u*sizeof(int32_t));
    if(!mapmaker_report.parent || !mapmaker_report.size || !first) { free(first);mapmaker_report_nav_free();return; }
    for(i=0;i<0x10000u;i++) first[i]=-1;
    for(i=0;i<pads;i++) {
        uint16_t g=(uint16_t)mapmaker_report_u16(pool+i*0x20u+8u);
        mapmaker_report.parent[i]=(int32_t)i;
        if(g==0xFFFFu) continue;
        if(first[g]<0) first[g]=(int32_t)i; else mapmaker_report_join((uint32_t)first[g],i);
    }
    free(first);
    for(i=0;i<links;i++) {
        uint32_t l=lpool+i*0x14u,a=WORD_AT(l+4u),b=WORD_AT(l+8u);
        if((WORD_AT(l)&MAPMAKER_REPORT_LINK_GROUPS) && a<pads && b<pads) mapmaker_report_join(a,b);
    }
    for(i=0;i<pads;i++) {
        uint32_t r=mapmaker_report_root(i);
        if(!mapmaker_report.size[r]++) mapmaker_report.areas++;
        if(mapmaker_report.size[r]>mapmaker_report.size[best]) best=r;
    }
    mapmaker_report.main=best;
    mapmaker_report.nav=1;
}

static void mapmaker_report_nav_problems(void)
{
    uint32_t i,k,listed=0,others=0,other_pads=0;
    uint32_t top[MAPMAKER_REPORT_ISLANDS];
    if(!mapmaker_report.nav) return;
    /* Story AI: start pad, then each patrol point against the start's area */
    for(i=0;i<mapmaker_report.sai && i<MAPMAKER_REPORT_SAI_MAX;i++) {
        const MapReportAi *ai=&mapmaker_report.ai[i];
        int32_t start=-2;
        int x,z,f;
        for(k=0;k<ai->points;k++) {
            int32_t h=mapmaker_report_pad(ai->pad[k]);
            if(h==-1) { mapmaker_report_problem(0,"Story AI \"%s\" point %u is on no pad (pad id %u): move it in the editor",ai->name,k+1u,ai->pad[k]);continue; }
            if(h<0) continue;
            if(start<0) {
                start=h;
                if(mapmaker_report_root((uint32_t)h)!=mapmaker_report.main) {
                    mapmaker_report_where((uint32_t)h,&x,&z,&f);
                    mapmaker_report_problem(1,"Story AI \"%s\" starts at grid %d,%d floor %d in an area of %u pads not linked to the main area",
                                            ai->name,x,z,f,mapmaker_report.size[mapmaker_report_root((uint32_t)h)]);
                }
            } else if(mapmaker_report_root((uint32_t)h)!=mapmaker_report_root((uint32_t)start)) {
                mapmaker_report_where((uint32_t)h,&x,&z,&f);
                mapmaker_report_problem(1,"Story AI \"%s\" point %u (grid %d,%d floor %d) cannot be reached from its first point",ai->name,k+1u,x,z,f);
            }
        }
    }
    /* the largest separate areas */
    for(k=0;k<MAPMAKER_REPORT_ISLANDS;k++) top[k]=0xFFFFFFFFu;
    for(i=0;i<mapmaker_report.pads;i++) {
        uint32_t s;
        if(mapmaker_report.parent[i]!=(int32_t)i || i==mapmaker_report.main) continue;
        s=mapmaker_report.size[i];others++;other_pads+=s;
        for(k=0;k<MAPMAKER_REPORT_ISLANDS;k++)
            if(top[k]==0xFFFFFFFFu || s>mapmaker_report.size[top[k]]) {
                memmove(&top[k+1u],&top[k],(MAPMAKER_REPORT_ISLANDS-1u-k)*sizeof top[0]);
                top[k]=i;break;
            }
    }
    for(k=0;k<MAPMAKER_REPORT_ISLANDS && top[k]!=0xFFFFFFFFu;k++) {
        int x,z,f;
        mapmaker_report_where(top[k],&x,&z,&f);
        mapmaker_report_problem(1,"%u pads around grid %d,%d floor %d are not linked to the main area (AI cannot walk there or back)",
                                mapmaker_report.size[top[k]],x,z,f);
        listed++;
    }
    if(others>listed)
        mapmaker_report_problem(1,"%u more separate pad areas (%u pads in all areas outside the main one)",others-listed,other_pads);
}

/* ---- the report --------------------------------------------------------- */
static int mapmaker_report_env(const char *name)
{
    const char *e=getenv(name);
    return e && e[0]=='1' && !e[1];
}

/* container = [0x00784014]; hidden = chunks the validator skipped. */
static void mapmaker_report_level(uint32_t container,uint32_t span,unsigned hidden)
{
    char nearby[MAPMAKER_REPORT_LINE];
    unsigned i,k;
    uint32_t arena=WORD_AT(MAPMAKER_REPORT_ARENA);
    int32_t heap=(int32_t)WORD_AT(MAPMAKER_REPORT_HEAP_FREE);
    unsigned reports=mapmaker_report.reports;
    mapmaker_report_nav_free();
    memset(&mapmaker_report,0,sizeof mapmaker_report);
    mapmaker_report.reports=reports+1u;
    mapmaker_report.hidden=hidden;
#ifdef MAPMAKER_LIMIT_PIECES         /* pc_mapmaker_limits.inc is included */
    mapmaker_limits_read();
    mapmaker_report.limits=mapmaker_limits.limits;
#else
    mapmaker_report.limits=mapmaker_report_env("TSFP_MAPMAKER_LIMITS");
#endif
    mapmaker_report.overlap=mapmaker_report_env("TSFP_MAPMAKER_OVERLAP");
    mapmaker_report.story=MAPMAKER_REPORT_B(MAPMAKER_REPORT_MODE)==10u;
    if(!mapmaker_report_scan(container,span,0)) return;
    mapmaker_report_nav();
    nearby[0]=0;

    /* 1. caps */
    {
        int on=mapmaker_report.limits;
        mapmaker_report_cap("tiles",mapmaker_report.pieces,on?MAPMAKER_REPORT_CAP_PIECES:MAPMAKER_REPORT_STOCK_PIECES,nearby,sizeof nearby);
        mapmaker_report_cap("items",mapmaker_report.items,on?MAPMAKER_REPORT_CAP_ITEMS:MAPMAKER_REPORT_STOCK_ITEMS,nearby,sizeof nearby);
        mapmaker_report_cap("engine objects (estimate)",mapmaker_report.pieces+mapmaker_report.items+MAPMAKER_REPORT_OBJECT_RESERVE,
                            MAPMAKER_REPORT_CAP_OBJECTS,nearby,sizeof nearby);
        mapmaker_report_cap("Story AI",mapmaker_report.sai,on?MAPMAKER_REPORT_CAP_SAI:MAPMAKER_REPORT_STOCK_SAI,nearby,sizeof nearby);
        mapmaker_report_cap("logic entries",mapmaker_report.logic,on?MAPMAKER_REPORT_CAP_LOGIC:MAPMAKER_REPORT_STOCK_LOGIC,nearby,sizeof nearby);
        if(mapmaker_report.nav) {
            mapmaker_report_cap("pads",mapmaker_report.pads,MAPMAKER_REPORT_POOL_PADS,nearby,sizeof nearby);
            mapmaker_report_cap("cover pads",mapmaker_report.cover,MAPMAKER_REPORT_POOL_COVER,nearby,sizeof nearby);
            mapmaker_report_cap("links",mapmaker_report.links,MAPMAKER_REPORT_POOL_LINKS,nearby,sizeof nearby);
            mapmaker_report_cap("pad groups",mapmaker_report.groups,MAPMAKER_REPORT_POOL_GROUPS,nearby,sizeof nearby);
        }
        for(i=0;i<MAPMAKER_REPORT_CATEGORIES;i++) {
            uint32_t a=MAPMAKER_REPORT_MAXIMA+i*24u+4u,max;
            char what[32];
            if(!mapmaker_report.category[i] || !MAPMAKER_PTR_OK(a,4u)) continue;
            max=WORD_AT(a);
            if(!max || max>0x10000u) continue;
            if(mapmaker_report_names[i]) snprintf(what,sizeof what,"%s",mapmaker_report_names[i]);
            else snprintf(what,sizeof what,"category %u items",i);
            mapmaker_report_cap(what,mapmaker_report.category[i],max,nearby,sizeof nearby);
        }
        if(heap<0) mapmaker_report_problem(0,"the level memory heap ran out by %d KB (expect missing objects or a crash)",(int)((-(int64_t)heap+1023)/1024));
        else if((uint32_t)heap/1024u<MAPMAKER_REPORT_LOW_MEMORY)
            mapmaker_report_problem(1,"only %u KB of level memory left after loading",(unsigned)heap/1024u);
    }
    /* 2. damaged map data */
    if(hidden) mapmaker_report_problem(0,"%u damaged part%s of the map file %s skipped (see the \"map chunk skipped\" lines)",hidden,hidden==1u?"":"s",hidden==1u?"was":"were");
    if(!mapmaker_report.walk_ok) mapmaker_report_problem(0,"the tile records end unexpectedly; the report counts only those before the damage");
    /* 3. Story AI without a position */
    for(i=0;i<mapmaker_report.sai && i<MAPMAKER_REPORT_SAI_MAX;i++)
        if(!mapmaker_report.ai[i].points)
            mapmaker_report_problem(0,"Story AI \"%s\" has no position (shown with ! in the editor list): it is not placed",mapmaker_report.ai[i].name);
    /* 4. logic entries naming deleted tiles */
    mapmaker_report_scan(container,span,1);
    /* 5. navigation: Story AI points, separate pad areas */
    mapmaker_report_nav_problems();
    /* 6. teleporters */
    for(i=MAPMAKER_REPORT_TELEPORT_FIRST;i<=MAPMAKER_REPORT_TELEPORT_LAST;i++) {
        unsigned n=mapmaker_report.category[i];
        if(n==1u) mapmaker_report_problem(0,"teleporter colour %u has a single teleporter: it needs a partner of the same colour",i-MAPMAKER_REPORT_TELEPORT_FIRST+1u);
        else if(n&1u) mapmaker_report_problem(1,"teleporter colour %u has an odd number (%u): one is left without a partner",i-MAPMAKER_REPORT_TELEPORT_FIRST+1u,n);
    }
    /* 7. overlapping tiles */
    {
        unsigned overlaps=0;
        qsort(mapmaker_report.piece,mapmaker_report.kept,sizeof mapmaker_report.piece[0],mapmaker_report_compare);
        for(k=1;k<mapmaker_report.kept;k++) {
            const MapReportPiece *a=&mapmaker_report.piece[k-1u],*b=&mapmaker_report.piece[k];
            if(a->x!=b->x || a->z!=b->z || a->floor!=b->floor) continue;
            overlaps++;
            if(!mapmaker_report.overlap)
                mapmaker_report_problem(0,"tiles #%u (tile %u) and #%u (tile %u) are both at grid %d,%d floor %d (overlap)",
                                        a->index+1u,a->tile+1u,b->index+1u,b->tile+1u,b->x,b->z,b->floor);
        }
        if(overlaps && mapmaker_report.overlap)
            mapmaker_report_problem(1,"%u tile%s share a grid cell with another (OVERLAP is on; overlaps can open sky windows or cut AI paths)",
                                    overlaps,overlaps==1u?"":"s");
    }

    /* print */
    mapmaker_report_emit("\"%s\" (%s%s), %u tiles, %u items, %u Story AI (%u boss%s), %u logic entries",
                         mapmaker_report.name[0]?mapmaker_report.name:"unnamed",mapmaker_report.story?"story":"arcade",
                         mapmaker_report.limits?", raised limits":"",mapmaker_report.pieces,mapmaker_report.items,
                         mapmaker_report.sai,mapmaker_report.bosses,mapmaker_report.bosses==1u?"":"es",mapmaker_report.logic);
    if(mapmaker_report.nav)
        mapmaker_report_emit("navigation: %u pads, %u cover pads, %u links, %u pad groups; %u connected area%s, the main one %u pads",
                             mapmaker_report.pads,mapmaker_report.cover,mapmaker_report.links,mapmaker_report.groups,
                             mapmaker_report.areas,mapmaker_report.areas==1u?"":"s",mapmaker_report.size[mapmaker_report.main]);
    else mapmaker_report_emit("navigation: not readable (pad tables missing)");
    if(heap>=0) mapmaker_report_emit("level memory: %u KB free of a %u KB arena",(unsigned)heap/1024u,arena/1024u);
    if(mapmaker_report.open[1] || mapmaker_report.open[2] || mapmaker_report.open[3])
        mapmaker_report_emit("open tiles: %u no roof, %u open air (unbounded: falling off the map kills), %u open air (bounded)",
                             mapmaker_report.open[1],mapmaker_report.open[2],mapmaker_report.open[3]);
    if(nearby[0]) mapmaker_report_emit("near limit: %s",nearby);
    if(!mapmaker_report.problems) mapmaker_report_emit("no problems found");
    else {
        mapmaker_report_emit("%u problem%s (\"possible\" = a heuristic check):",mapmaker_report.problems,mapmaker_report.problems==1u?"":"s");
        for(k=0;k<mapmaker_report.problems && k<MAPMAKER_REPORT_PROBLEMS;k++) mapmaker_report_emit("  %s",mapmaker_report.problem[k]);
        if(mapmaker_report.problems>MAPMAKER_REPORT_PROBLEMS)
            mapmaker_report_emit("  ... and %u more not listed",mapmaker_report.problems-MAPMAKER_REPORT_PROBLEMS);
    }
    mapmaker_report_emit("end");
    fflush(stderr);
    mapmaker_report_nav_free();
}
#endif
