/* Asset-free contract for src/interface/pc_mapmaker_report.inc: the map report
 * that pc_mapmaker_validate_level (src/interface/pc_mapmaker_validate.inc) logs
 * at a MapMaker level start. Fake guest memory holds a map container, the
 * item catalogue and category maxima, and the play-time pad, link and group
 * tables; each case checks the report text, and every case checks that guest
 * memory is unchanged by the call. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x01000000/4], g_eax, g_ecx, g_edx, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define BYTE_AT(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
#define MAPMAKER_PTR_OK(p,n) guest_writable((uint32_t)(p),(size_t)(n))
void sub_00333700(void) { g_ecx=1; }
void sub_002FDF50(void) { g_ecx=2; }
static uint32_t pads_a(uint32_t id) { return id%3u; }
static uint32_t pads_b(uint32_t id) { return id==44u?2u:1u; }
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x=0;
    va_start(a,n);
    if(n) x=va_arg(a,uint32_t);
    va_end(a);
    return f==sub_00333700?pads_a(x):f==sub_002FDF50?pads_b(x):0u;
}
/* Raised category maxima as pc_mapmaker_limits.inc gives them (weapons 32 -> 128). */
static uint32_t fixture_raised(uint32_t a,uint32_t max){return a==0x00524CA4u && max<128u?128u:max;}
#define MAPMAKER_REPORT_RAISED(a,max) fixture_raised((a),(max))
#include "pc_mapmaker_report.inc"
#include "pc_mapmaker_validate.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
#define HAS(s) (strstr(mapmaker_report.text,(s))!=NULL)
#define EXPECT(s) do { if(!HAS(s)) { fprintf(stderr,"%s:%d: missing \"%s\" in:\n%s\n",__FILE__,__LINE__,(s),mapmaker_report.text);failures++; } } while(0)
#define REFUSE(s) do { if(HAS(s)) { fprintf(stderr,"%s:%d: unexpected \"%s\" in:\n%s\n",__FILE__,__LINE__,(s),mapmaker_report.text);failures++; } } while(0)

/* ---- guest tables --------------------------------------------------------- */
#define C 0x00200000u
#define T(n) (0x00C77600u+(n))
#define POOL 0x00300000u
#define LPOOL 0x00380000u
#define CPOOL 0x003E0000u

static int category_of(int id)
{
    static const int8_t cat[80]={0,0,0,0,0,1,1,2,2,3,14,15,16,17,18,4,5,8,8,8,8,8,8,9,9,9,9,9,9,10,11,12,13,22,23,
        19,19,20,20,21,21,21,21,35,33,33,33,33,33,33,29,29,29,27,27,28,30,30,30,30,30,30,30,30,30,30,31,25,6,32,34,34,36,
        26,26,26,26,31,31,31};
    return cat[id];
}
static void tables(void)
{
    unsigned i;
    for(i=0;i<80u;i++) { BYTE_AT(0x00524F58u+i*0xC4u+2u)=(uint8_t)category_of((int)i);BYTE_AT(0x00524F58u+i*0xC4u+3u)=0; }
    for(i=0;i<37u;i++) WORD_AT(0x00524BE0u+i*24u+4u)=32u;
    for(i=14;i<=18u;i++) WORD_AT(0x00524BE0u+i*24u+4u)=8u;
}

/* ---- map builder ---------------------------------------------------------- */
typedef struct Ai { const char *name; int points; uint16_t pad[8]; uint8_t boss; } Ai;
typedef struct Map {
    const char *name;
    int pieces;
    int dup_a, dup_b;                          /* piece dup_b copies piece dup_a's cell (when dup_b > 0) */
    int items[8]; int nitems;                  /* item ids on piece 0 */
    int open_mode;                             /* roof mark of piece 2 */
    int bad_tile;                              /* tile id of piece 1 (0 = default): the validator hides the chunk */
    Ai ai[40]; int nai;
    int logic_type, logic_idx[13], nlogic;     /* one logic record */
} Map;
static uint32_t cur;
static uint32_t begin(uint32_t tag) { uint32_t c=cur;WORD_AT(c)=tag;WORD_AT(c+4)=0;cur+=8;return c; }
static void finish(uint32_t c) { WORD_AT(c+4)=cur-(c+8u); }
static void fill(uint32_t n,uint8_t v) { while(n--) BYTE_AT(cur++)=v; }
static void build(const Map *m)
{
    uint32_t c,i,j;
    memset((char *)memory+C,0,0x00080000u);
    WORD_AT(C)=0x00C77667u;WORD_AT(C+4)=0x12Du;
    cur=C+8u;
    c=begin(T(0x68));fill(0x3E0,0);
    WORD_AT(c+8+4)=(uint32_t)m->pieces;
    BYTE_AT(c+8+0xA)=3;BYTE_AT(c+8+0x42)=15;
    for(i=0;m->name[i];i++) BYTE_AT(c+8+0x62+i)=(uint8_t)m->name[i];
    for(i=0;i<20u;i++) BYTE_AT(c+8+0x162+i)='T';
    finish(c);
    c=begin(T(0x69));
    for(i=0;i<(uint32_t)m->pieces;i++) {
        uint32_t id=i==1 && m->bad_tile?(uint32_t)m->bad_tile:i%45u,p=cur,pads=pads_a(id)+pads_b(id),k=i;
        int n=i==0?m->nitems:0;
        fill(0x28,0);
        if(m->dup_b>0 && (int)i==m->dup_b) k=(uint32_t)m->dup_a;
        WORD_AT(p)=id;
        BYTE_AT(p+4)=(uint8_t)(k%40u);BYTE_AT(p+5)=(uint8_t)(k/40u*2u);BYTE_AT(p+6)=(uint8_t)(k==7u && i==7u?1:0);BYTE_AT(p+7)=(uint8_t)(i&3u);
        BYTE_AT(p+0x1E)=(uint8_t)n;
        WORD_AT(p+0x24)=0x28u+pads*8u;
        if(i==2 && m->open_mode) WORD_AT(p+0x24)|=(0x5200u|(uint32_t)m->open_mode)<<16;
        fill(pads*8u,0x11);
        for(j=0;j<(uint32_t)n;j++) {
            uint32_t q=cur;
            fill(0x18u+4u*(j%2u),0);
            BYTE_AT(q+0xE)=(uint8_t)m->items[j];BYTE_AT(q+0x14)=(uint8_t)(j%2u);
        }
    }
    finish(c);
    for(i=0;i<(uint32_t)m->nai;i++) {
        const Ai *a=&m->ai[i];
        c=begin(T(0x6C));fill(0x44u+4u*(uint32_t)a->points,0);
        for(j=0;a->name[j] && j<0x23u;j++) BYTE_AT(c+8+j)=(uint8_t)a->name[j];
        BYTE_AT(c+8+0x2C)=(uint8_t)a->points;BYTE_AT(c+8+0x29)=a->boss;
        for(j=0;j<(uint32_t)a->points;j++) { BYTE_AT(c+8+0x44+4*j)=(uint8_t)a->pad[j];BYTE_AT(c+8+0x45+4*j)=(uint8_t)(a->pad[j]>>8);BYTE_AT(c+8+0x46+4*j)=0; }
        finish(c);
    }
    if(m->logic_type) {
        c=begin(T(0x6F));fill(0x8C,0);
        WORD_AT(c+8+0x80)=(uint32_t)m->logic_type;
        WORD_AT(c+8+(m->logic_type==3?0x20:0x70))=(uint32_t)m->nlogic;
        for(i=0;i<(uint32_t)m->nlogic;i++) WORD_AT(c+8+0x24+8*i)=(uint32_t)m->logic_idx[i];
        finish(c);
    }
    WORD_AT(cur)=T(0x6A);WORD_AT(cur+4)=0;
}
static Map def(void)
{
    Map m;
    memset(&m,0,sizeof m);
    m.name="Test Lab";m.pieces=12;m.nitems=4;m.items[0]=0x0A;m.items[1]=0x0A;m.items[2]=0x11;m.items[3]=0x05;
    m.nai=1;m.ai[0].name="Guard";m.ai[0].points=2;m.ai[0].pad[0]=0x3E9u+2u;m.ai[0].pad[1]=0x3E9u+5u;
    m.logic_type=3;m.nlogic=2;m.logic_idx[0]=0;m.logic_idx[1]=11;
    return m;
}

/* ---- navigation tables: groups of 4 pads in a row, linked in a chain ---- */
typedef struct Nav { int pads, island_from; int weak_link; int high; } Nav;
static uint32_t links_n;
static void link(uint32_t a,uint32_t b,uint32_t flags)
{
    uint32_t l=LPOOL+links_n*0x14u;
    WORD_AT(l)=flags;WORD_AT(l+4)=a;WORD_AT(l+8)=b;links_n++;
}
static void nav(const Nav *n)
{
    int i;
    float f;
    memset((char *)memory+POOL,0,0x00100000u);
    links_n=0;
    for(i=0;i<n->pads;i++) {
        uint32_t p=POOL+(uint32_t)i*0x20u,id=i<8000?0x3E9u+(uint32_t)i:0x5B29u+(uint32_t)(i-8000);
        WORD_AT(p)=id;WORD_AT(p+8)=(uint32_t)(i/4)&0xFFFFu;
        f=(float)(i*6+3);memcpy((char *)memory+p+0xC,&f,4);
        f=i>=n->island_from && n->island_from>0?4.8f:0.3f;memcpy((char *)memory+p+0x10,&f,4);
        f=27.0f;memcpy((char *)memory+p+0x14,&f,4);
    }
    for(i=4;i<n->pads;i+=4) {
        uint32_t flags=0x10000u|((i/4)%2?2u:0u);             /* door/one-way bits do not matter */
        if(n->island_from>0 && i==n->island_from) flags=n->weak_link?0x40u:0u;  /* no group link: island */
        if(flags) link((uint32_t)i-1u,(uint32_t)i,flags);
    }
    link(0,(uint32_t)n->pads+5u,0x10000u);                  /* a link to a pad past the count is ignored */
    for(i=0;i<40;i++) {                                      /* cover pad i belongs to regular pad i + 20 */
        uint32_t p=CPOOL+(uint32_t)i*0x20u;
        WORD_AT(p)=0x2329u+(uint32_t)i;WORD_AT(p+8)=i==5?0xFFFFu:0x3E9u+(uint32_t)i+20u;
    }
    WORD_AT(CPOOL+7u*0x20u)=0x2329u+99u;                    /* a cover pad that holds another id */
    WORD_AT(0x00715F34u)=CPOOL;
    WORD_AT(0x00715F2Cu)=POOL;WORD_AT(0x007B6664u)=(uint32_t)n->pads;
    WORD_AT(0x00715F30u)=LPOOL;WORD_AT(0x007B6660u)=links_n;
    WORD_AT(0x007B666Cu)=(uint32_t)(n->pads+3)/4u;WORD_AT(0x007B65C0u)=40u;
}
static Nav ndef(void) { Nav n; memset(&n,0,sizeof n); n.pads=64; return n; }

/* ---- running the hook ------------------------------------------------------ */
static unsigned level_calls;
static void fake_level(void) { level_calls++;g_esp+=4u; }
static uint32_t *snapshot;
static void run(const Map *m,const Nav *n)
{
    build(m);nav(n);
    memcpy(snapshot,memory,sizeof memory);
    level_calls=0;g_esp=0x00F00000u;
    mapmaker_report.text[0]=0;
    pc_mapmaker_validate_level(fake_level);
    CHECK(level_calls==1 && g_esp==0x00F00004u);
    if(memcmp(snapshot,memory,sizeof memory)) { fprintf(stderr,"guest memory changed by the report\n");failures++; }
}
static unsigned count_lines(const char *prefix)
{
    unsigned n=0;
    const char *p=mapmaker_report.text;
    while((p=strstr(p,prefix))!=NULL) { n++;p+=strlen(prefix); }
    return n;
}

int main(void)
{
    Map m;
    Nav n;
    unsigned before,i;
    g_xbox_mem_offset=(uintptr_t)memory;
    snapshot=(uint32_t *)malloc(sizeof memory);
    if(!snapshot) return 2;
    tables();
    WORD_AT(0x00784014u)=C;WORD_AT(0x0079094Cu)=100u;WORD_AT(0x006F32ECu)=0;BYTE_AT(0x007DE455u)=10;
    WORD_AT(0x004C0450u)=0x02CEC000u;WORD_AT(0x006B8384u)=8u<<20;

    /* a clean map */
    m=def();n=ndef();run(&m,&n);
    EXPECT("\"Test Lab\" (story), 12 tiles, 3 items, 1 Story AI (0 bosses), 1 logic entries");
    EXPECT("navigation: 64 pads, 40 cover pads, 16 links, 16 pad groups; 1 connected area, the main one 64 pads");
    EXPECT("level memory: 8192 KB free of a 46000 KB arena");
    EXPECT("no problems found");
    REFUSE("problem:");REFUSE("near limit");REFUSE("open tiles");
    CHECK(mapmaker_report.reports==1u);

    /* not scene 100 / not the first pass: no report */
    before=mapmaker_report.reports;
    WORD_AT(0x0079094Cu)=102u;run(&m,&n);CHECK(mapmaker_report.reports==before);
    WORD_AT(0x0079094Cu)=100u;WORD_AT(0x006F32ECu)=1u;run(&m,&n);CHECK(mapmaker_report.reports==before);
    WORD_AT(0x006F32ECu)=0;BYTE_AT(0x007DE455u)=1;run(&m,&n);EXPECT("(arcade)");BYTE_AT(0x007DE455u)=10;

    /* bosses: any number of copies is fine */
    m=def();m.nai=3;m.ai[1]=m.ai[0];m.ai[2]=m.ai[0];m.ai[0].boss=0xB1;m.ai[1].boss=0xB1;m.ai[2].boss=0xB5;
    n=ndef();run(&m,&n);EXPECT("3 Story AI (3 bosses)");EXPECT("no problems found");

    /* Story AI with no position, on no pad, past the pool, on a cover pad (not traced) */
    m=def();m.nai=7;m.ai[1].name="Ghost";m.ai[1].points=0;
    m.ai[4].name="Far";m.ai[4].points=1;m.ai[4].pad[0]=0x2329u+40u;
    m.ai[5].name="Loose";m.ai[5].points=1;m.ai[5].pad[0]=0x2329u+5u;
    m.ai[6].name="Moved";m.ai[6].points=1;m.ai[6].pad[0]=0x2329u+7u;
    m.ai[2].name="Lost";m.ai[2].points=1;m.ai[2].pad[0]=0xFFFF;
    m.ai[3].name="Cover";m.ai[3].points=2;m.ai[3].pad[0]=0x2329u+3u;m.ai[3].pad[1]=0x3E9u+70u;
    n=ndef();run(&m,&n);
    EXPECT("problem: Story AI \"Ghost\" has no position");
    EXPECT("problem: Story AI \"Lost\" point 1 is on no pad (pad id 65535)");
    EXPECT("problem: Story AI \"Cover\" point 2 is on no pad (pad id 1071)");
    EXPECT("problem: Story AI \"Far\" point 1 is on no pad (pad id 9041)");
    EXPECT("problem: Story AI \"Moved\" point 1 is on no pad (pad id 9008)");
    REFUSE("\"Cover\" point 1");REFUSE("\"Guard\"");REFUSE("Loose");
    EXPECT("5 problems");

    /* a pad id whose pad holds another id is no pad */
    m=def();n=ndef();build(&m);nav(&n);WORD_AT(POOL+5u*0x20u)=0x3E9u+99u;
    memcpy(snapshot,memory,sizeof memory);level_calls=0;g_esp=0x00F00000u;pc_mapmaker_validate_level(fake_level);
    EXPECT("\"Guard\" point 2 is on no pad (pad id 1006)");

    /* high pad ids (regular pads from 8000) */
    m=def();m.ai[0].pad[0]=0x5B29u+3u;m.ai[0].pad[1]=0x3E9u+7999u;n=ndef();n.pads=8010;run(&m,&n);
    EXPECT("no problems found");EXPECT("navigation: 8010 pads");

    /* a separate pad area (only a non-group link joins it), the AI starting there, a patrol into it */
    m=def();m.nai=3;m.ai[0].pad[0]=0x3E9u+50u;m.ai[0].pad[1]=0x3E9u+60u;
    m.ai[2].name="Sniper";m.ai[2].points=1;m.ai[2].pad[0]=0x2329u+30u;          /* cover pad of pad 50 */
    m.ai[1].name="Walker";m.ai[1].points=3;m.ai[1].pad[0]=0x3E9u+1u;m.ai[1].pad[1]=0x3E9u+2u;m.ai[1].pad[2]=0x3E9u+55u;
    n=ndef();n.island_from=48;n.weak_link=1;run(&m,&n);
    EXPECT("2 connected areas, the main one 48 pads");
    EXPECT("possible problem: Story AI \"Guard\" starts at grid 50,4 floor 1 in an area of 16 pads not linked to the main area");
    EXPECT("possible problem: Story AI \"Sniper\" starts at grid 50,4 floor 1");
    EXPECT("possible problem: Story AI \"Walker\" point 3 (grid 55,4 floor 1) cannot be reached from its first point");
    EXPECT("possible problem: 16 pads around grid 48,4 floor 1 are not linked to the main area");
    REFUSE("more separate pad areas");
    /* the main area is the largest, wherever pad 0 is */
    m=def();m.ai[0].pad[0]=0x3E9u+20u;m.ai[0].pad[1]=0x3E9u+30u;n=ndef();n.island_from=16;run(&m,&n);
    EXPECT("the main one 48 pads");
    EXPECT("possible problem: 16 pads around grid 0,4 floor 0 are not linked to the main area");
    REFUSE("Guard");
    /* many islands: three listed, the rest summed */
    m=def();n=ndef();n.pads=64;nav(&n);
    {
        Map k=def();build(&k);
        for(i=0;i<WORD_AT(0x007B6660u);i++) if(WORD_AT(LPOOL+i*0x14u+4u)<64u) WORD_AT(LPOOL+i*0x14u)=0;  /* every group alone */
        memcpy(snapshot,memory,sizeof memory);level_calls=0;g_esp=0x00F00000u;pc_mapmaker_validate_level(fake_level);
        EXPECT("16 connected areas");
        CHECK(count_lines("pads around grid")==3u);
        EXPECT("possible problem: 12 more separate pad areas (60 pads in all areas outside the main one)");
    }

    /* logic entries naming tiles past the placed ones */
    m=def();m.logic_idx[1]=12;n=ndef();run(&m,&n);
    EXPECT("problem: logic entry 1 (link 2) names tile #13, which is not on the map");
    m=def();m.logic_type=0xA;m.nlogic=2;m.logic_idx[0]=1;m.logic_idx[1]=12;run(&m,&n);
    EXPECT("no problems found");
    m.logic_idx[1]=13;run(&m,&n);EXPECT("names tile #13");
    m=def();m.logic_type=7;m.nlogic=3;m.logic_idx[0]=500;run(&m,&n);EXPECT("no problems found");

    /* teleporters: one alone, three of a colour */
    m=def();m.nitems=5;m.items[0]=0x0B;m.items[1]=0x0C;m.items[2]=0x0C;m.items[3]=0x0C;m.items[4]=0x0D;
    m.items[4]=0x0A;run(&m,&n);
    EXPECT("problem: teleporter colour 2 has a single teleporter");
    EXPECT("possible problem: teleporter colour 3 has an odd number (3)");
    EXPECT("problem: teleporter colour 1 has a single teleporter");
    REFUSE("colour 4");

    /* a full small cap is not "near" */
    m=def();m.nitems=8;for(i=0;i<8u;i++) m.items[i]=0x0A;run(&m,&n);EXPECT("no problems found");REFUSE("near limit");

    /* overlapping tiles */
    m=def();m.dup_a=3;m.dup_b=9;run(&m,&n);
    EXPECT("problem: tiles #4 (tile 4) and #10 (tile 10) are both at grid 3,0 floor 0 (overlap)");
    putenv("TSFP_MAPMAKER_OVERLAP=1");run(&m,&n);
    EXPECT("possible problem: 1 tile shares a grid cell");REFUSE("(overlap)");
    putenv("TSFP_MAPMAKER_OVERLAP=0");
    m=def();m.pieces=48;run(&m,&n);EXPECT("no problems found");   /* rows: same x, other z */
    m=def();m.dup_a=7;m.dup_b=9;run(&m,&n);EXPECT("no problems found");   /* same cell, other floor */

    /* open tiles */
    m=def();m.open_mode=2;run(&m,&n);EXPECT("open tiles: 0 no roof, 1 open air (unbounded");

    /* caps: stock without LIMITS, raised with it; near limit past 90% */
    m=def();m.pieces=201;m.logic_type=0;run(&m,&n);EXPECT("problem: tiles 201 is over the cap of 200");
    m.pieces=181;run(&m,&n);EXPECT("near limit: tiles 181/200 (90%)");
    m.pieces=180;run(&m,&n);REFUSE("near limit");
    putenv("TSFP_MAPMAKER_LIMITS=1");
    m.pieces=950;run(&m,&n);EXPECT("raised limits");EXPECT("near limit: tiles 950/1023 (92%)");REFUSE("over the cap");
    putenv("TSFP_MAPMAKER_LIMITS=0");
    m=def();m.nitems=8;for(i=0;i<8u;i++) m.items[i]=0x2B;  /* vehicles, max 4 here */
    WORD_AT(0x00524BE0u+35u*24u+4u)=4u;run(&m,&n);EXPECT("problem: vehicles 8 is over the cap of 4");
    WORD_AT(0x00524BE0u+35u*24u+4u)=9u;run(&m,&n);REFUSE("near limit");
    REFUSE("vehicles 8 is over");WORD_AT(0x00524BE0u+35u*24u+4u)=32u;
    /* weapons (category 8): the table holds a stock value in play; with LIMITS the
     * raised maximum the editor allowed applies (fixture_raised: up to 128) */
    m=def();m.nitems=8;for(i=0;i<8u;i++) m.items[i]=0x11;WORD_AT(0x00524CA4u)=4u;
    run(&m,&n);EXPECT("problem: weapons 8 is over the cap of 4");
    putenv("TSFP_MAPMAKER_LIMITS=1");run(&m,&n);REFUSE("weapons 8 is over");REFUSE("near limit: weapons");
    putenv("TSFP_MAPMAKER_LIMITS=0");WORD_AT(0x00524CA4u)=32u;
    n=ndef();n.pads=15000;m=def();run(&m,&n);EXPECT("near limit: pads 15000/16384 (91%)");

    /* level memory */
    m=def();n=ndef();WORD_AT(0x006B8384u)=(uint32_t)-5000;run(&m,&n);EXPECT("problem: the level memory heap ran out by 5 KB");
    WORD_AT(0x006B8384u)=512u*1024u;run(&m,&n);EXPECT("possible problem: only 512 KB of level memory left");
    WORD_AT(0x006B8384u)=8u<<20;

    /* chunks the validator hid are skipped and reported */
    m=def();m.bad_tile=45;run(&m,&n);
    EXPECT("problem: 1 damaged part of the map file was skipped");
    EXPECT(", 0 tiles,");

    /* at most 20 problem lines */
    m=def();m.nai=30;for(i=0;i<30u;i++) { m.ai[i].name="Nobody";m.ai[i].points=0; }
    run(&m,&n);
    EXPECT("30 problems");CHECK(count_lines("  problem: ")==20u);EXPECT("  ... and 10 more not listed");

    /* no pad tables: the report still runs */
    m=def();n=ndef();build(&m);nav(&n);WORD_AT(0x007B6664u)=0;
    memcpy(snapshot,memory,sizeof memory);level_calls=0;g_esp=0x00F00000u;pc_mapmaker_validate_level(fake_level);
    EXPECT("navigation: not readable");

    free(snapshot);
    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    printf("mapmaker report ok (%u reports)\n",mapmaker_report.reports);
    return 0;
}
