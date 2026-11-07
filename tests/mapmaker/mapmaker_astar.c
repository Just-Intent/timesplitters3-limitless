/* Asset-free contract for src/interface/pc_mapmaker_astar.inc (the group
 * route search sub_0009B540 in C): synthetic pad graphs (groups on a grid,
 * links with every flag the search reads, doors, closed links, isolated
 * groups, links inside a group) give the same guest memory and return values
 * with the membership counts as with every list walked, match recorded
 * results, and a hand-built chain gives the expected route. With arguments
 * `dump <seed> <pre> <post>` it writes the guest memory before and after a
 * series of searches and prints each search (start target route max result)
 * for tests/mapmaker/test_mapmaker_astar.py, which runs the original guest
 * code (0x0009B090..0x0009B629 of the XBE) on the pre image in an emulator. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMORY_BYTES 0x00920000u
static uint32_t memory[MEMORY_BYTES/4], g_eax, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<MEMORY_BYTES && n<=MEMORY_BYTES-p; }
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#define MAPMAKER_GUEST_RET() (g_esp+=4u)
#include "pc_mapmaker_astar.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

#define PADS 0x00100000u
#define LINKS 0x00140000u
#define GROUP_PTRS 0x00180000u
#define GROUPS 0x00190000u
#define ROUTES 0x001F0000u
#define STACK 0x00300000u
#define MAX_PADS 2000u
#define MAX_LINKS 4000u
#define MAX_GROUPS 400u

static uint32_t pads_n,links_n,groups_n,group_links[MAX_GROUPS];
static uint32_t rng=1;
static uint32_t next_rand(void) { rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng; }
static float frand(float lo,float hi) { return lo+(hi-lo)*(float)(next_rand()%100000u)/100000.0f; }
static void set_f(uint32_t a,float f) { uint32_t w; memcpy(&w,&f,4); WORD_AT(a)=w; }

static void reset(uint32_t seed)
{
    memset(memory,0,sizeof memory);
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x0047D408u)=0x48F42400u;            /* 500000.0 */
    WORD_AT(0x00475D18u)=0x40400000u;            /* 3.0 */
    WORD_AT(MAPMAKER_ASTAR_PADS)=PADS;
    WORD_AT(MAPMAKER_ASTAR_LINKS)=LINKS;
    WORD_AT(MAPMAKER_ASTAR_GROUPS)=GROUP_PTRS;
    WORD_AT(MAPMAKER_ASTAR_MASK)=0x8u;
    pads_n=links_n=groups_n=0;
    memset(group_links,0,sizeof group_links);
    rng=seed?seed:1u;
}
static uint32_t add_group(uint32_t header)
{
    uint32_t g=groups_n++,grp=GROUPS+g*0x400u;
    WORD_AT(GROUP_PTRS+g*4u)=grp;
    WORD_AT(grp)=header;                         /* the list starts at +0x18 + header*4 */
    return g;
}
static uint32_t add_pad(uint32_t group,float x,float y,float z)
{
    uint32_t p=pads_n++,a=PADS+p*0x20u;
    *(uint16_t *)((char *)memory+a+8u)=(uint16_t)group;
    set_f(a+0xCu,x);set_f(a+0x10u,y);set_f(a+0x14u,z);
    return p;
}
static void group_add_link(uint32_t g,uint32_t link)
{
    uint32_t grp=GROUPS+g*0x400u,n=WORD_AT(grp+4u);
    WORD_AT(grp+0x18u+WORD_AT(grp)*4u+4u*n)=link;
    WORD_AT(grp+4u)=n+1u;
}
static uint32_t add_link(uint32_t flags,uint32_t p1,uint32_t p2,float factor)
{
    uint32_t l=links_n++,a=LINKS+l*0x14u;
    uint32_t g1=*(uint16_t *)((char *)memory+PADS+p1*0x20u+8u),g2=*(uint16_t *)((char *)memory+PADS+p2*0x20u+8u);
    WORD_AT(a)=flags;WORD_AT(a+4u)=p1;WORD_AT(a+8u)=p2;set_f(a+0xCu,factor);
    group_add_link(g1,l);
    if(g2!=g1) group_add_link(g2,l);
    return l;
}

/* side x side groups 10 units apart, 2..4 pads each; links to the right and
 * lower neighbours with random flags; a few links inside a group; the last
 * two groups have no links (unreachable). */
static void build_graph(uint32_t seed,uint32_t side)
{
    uint32_t gx,gz,first[20][20],count[20][20],k;
    reset(seed);
    for(gz=0;gz<side;gz++) for(gx=0;gx<side;gx++) {
        uint32_t g=add_group(1u+next_rand()%3u),n=2u+next_rand()%3u;
        first[gz][gx]=pads_n;count[gz][gx]=n;
        for(k=0;k<n;k++) add_pad(g,(float)gx*10.0f+frand(-3,3),frand(0,2),(float)gz*10.0f+frand(-3,3));
    }
    for(k=0;k<2;k++) { uint32_t g=add_group(1);add_pad(g,500.0f+(float)k,0,0);add_pad(g,501.0f+(float)k,0,1); }
    for(gz=0;gz<side;gz++) for(gx=0;gx<side;gx++) {
        int dir;
        for(dir=0;dir<2;dir++) {
            uint32_t nx=gx+(dir==0),nz=gz+(dir==1),a,b,flags=0x10000u,r;
            if(nx>=side || nz>=side) continue;
            a=first[gz][gx]+next_rand()%count[gz][gx];
            b=first[nz][nx]+next_rand()%count[nz][nx];
            r=next_rand()%40u;
            if(r<3) flags=0;                          /* not a usable link */
            else if(r<6) flags|=0x20000u;             /* blocked: cost 500000 */
            else if(r<10) flags|=0x40u;               /* slow: x3 */
            else if(r<14) flags|=0x2u;                /* door: one way unless [0x00715F50] */
            else if(r<16) flags|=0x8u;                /* closed by the mask */
            if(next_rand()&1u) add_link(flags,a,b,frand(0.5f,2.0f)); else add_link(flags,b,a,frand(0.5f,2.0f));
            if(next_rand()%7u==0) add_link(0x10000u,a,b,1.0f);   /* a second link between the same pads */
        }
        if(count[gz][gx]>=2 && next_rand()%4u==0)
            add_link(0x10000u,first[gz][gx],first[gz][gx]+1u,1.0f);  /* inside the group */
    }
}

static void call_search(uint32_t start,uint32_t target,uint32_t route,uint32_t max)
{
    g_esp=STACK-0x14u;
    WORD_AT(g_esp)=0xDEAD0000u;WORD_AT(g_esp+4u)=start;WORD_AT(g_esp+8u)=target;
    WORD_AT(g_esp+12u)=route;WORD_AT(g_esp+16u)=max;
    pc_mapmaker_pads_group_route(NULL);
}

/* searches of a series: start, target, route buffer, max */
static uint32_t series_n,series[200][4],results[200];
static void make_series(uint32_t seed)
{
    uint32_t s;
    static const uint32_t maxes[]={300,3,1,0,300,300,8};
    rng=seed*7u+1u;
    series_n=60;
    for(s=0;s<series_n;s++) {
        series[s][0]=next_rand()%pads_n;
        series[s][1]=(s%9==4)?pads_n-1u:next_rand()%pads_n;       /* every ninth: an isolated group */
        series[s][2]=ROUTES+s*0x400u;
        series[s][3]=maxes[s%7];
    }
}
static void set_mode(int mode)
{
#ifdef _WIN32
    _putenv(mode==2?"TSFP_MAPMAKER_ASTAR=2":"TSFP_MAPMAKER_ASTAR=1");
#else
    setenv("TSFP_MAPMAKER_ASTAR",mode==2?"2":"1",1);
#endif
}

static unsigned long long image_hash(void)
{
    unsigned long long h=1469598103934665603ull;
    uint32_t a,s;
    for(a=ROUTES;a<ROUTES+200u*0x400u;a++) { h^=((uint8_t *)memory)[a];h*=1099511628211ull; }
    for(a=MAPMAKER_ASTAR_POOL;a<MAPMAKER_ASTAR_POOL+0x20000u;a++) { h^=((uint8_t *)memory)[a];h*=1099511628211ull; }
    for(s=0;s<series_n;s++) { h^=results[s];h*=1099511628211ull; }
    h^=WORD_AT(MAPMAKER_ASTAR_GOAL_F);h*=1099511628211ull;
    h^=WORD_AT(MAPMAKER_ASTAR_FREE);h*=1099511628211ull;
    h^=WORD_AT(MAPMAKER_ASTAR_COUNTER);h*=1099511628211ull;
    return h;
}

/* the mode is read once per process: the fixture switches walk_all directly */
static void run_mode(uint32_t seed,uint32_t side,int walk_all)
{
    build_graph(seed,side);
    make_series(seed);
    mapmaker_astar.walk_all=walk_all;
    {
        uint32_t s;
        for(s=0;s<series_n;s++) {
            WORD_AT(MAPMAKER_ASTAR_DOORS)=s&1u;
            g_esp=STACK-0x14u;
            g_eax=mapmaker_astar_search((int32_t)series[s][0],(int32_t)series[s][1],series[s][2],(int32_t)series[s][3]);
            results[s]=g_eax;
        }
    }
}

/* Results of the fixed search (`golden` prints them); seeds 1..3 are also
 * compared with the original guest code by test_mapmaker_astar.py. */
static const unsigned long long golden[13]={0,
    0x4EACB652BD44A36Full,0x3211E02141656D8Eull,0xFA02E0B60EB80DADull,0xCE9DA616DC0E162Dull,
    0xAF55170C03FDACE0ull,0xC75A5C13A9B5963Dull,0xD0F0CE3A85293374ull,0xB8DAD36709A0CFC6ull,
    0x10946BE56D446C54ull,0x189305AF67BDD50Dull,0x305FC3C8F8EE89C9ull,0xC64C3680EEF3B029ull};

static void test_counts_equal_walks(int print)
{
    static uint32_t a[MEMORY_BYTES/4],ra[200];
    uint32_t seed;
    for(seed=1;seed<=12;seed++) {
        uint32_t side=3u+seed%8u;
        unsigned long long skipped=mapmaker_astar.skipped;
        run_mode(seed*2654435761u,side,1);
        memcpy(a,memory,sizeof memory);memcpy(ra,results,sizeof ra);
        run_mode(seed*2654435761u,side,0);
        CHECK(!memcmp(a,memory,sizeof memory) && !memcmp(ra,results,sizeof ra));
        CHECK(mapmaker_astar.skipped>skipped);       /* the counts did skip walks */
        if(print) printf("0x%016llXull,\n",image_hash());
        else if(image_hash()!=golden[seed]) { fprintf(stderr,"seed %u: hash 0x%016llX\n",seed,image_hash());failures++; }
    }
}

/* groups 0 - 1 - 2 in a row, one pad each plus a far pad in group 1 */
static void test_chain(void)
{
    uint32_t g0,g1,g2,p0,p1,p2,l01,l12,route=ROUTES,n;
    reset(9);
    g0=add_group(1);g1=add_group(2);g2=add_group(1);
    p0=add_pad(g0,0,0,0);p1=add_pad(g1,10,0,0);p2=add_pad(g2,20,0,0);
    add_pad(g1,10,0,30);
    l01=add_link(0x10000u,p0,p1,1.0f);
    l12=add_link(0x10000u,p2,p1,1.0f);             /* stored backwards: the search turns it */
    g_esp=STACK-0x14u;
    n=mapmaker_astar_search((int32_t)p0,(int32_t)p2,route,300);
    /* the goal link, then the links back to the start */
    CHECK(n==2 && *(uint16_t *)((char *)memory+route)==l12 && *(uint16_t *)((char *)memory+route+2u)==l01);
    CHECK(WORD_AT(MAPMAKER_ASTAR_GOAL_GROUP)==g2 && WORD_AT(MAPMAKER_ASTAR_TARGET)==p2);
    {   /* f of the goal node: g = 10 + 10 (link costs; the links start at the pads), h = 0 */
        float f;uint32_t w=WORD_AT(MAPMAKER_ASTAR_GOAL_F);memcpy(&f,&w,4);
        CHECK(f==20.0f);
    }
    /* max 1: only the goal link */
    n=mapmaker_astar_search((int32_t)p0,(int32_t)p2,route+0x100u,1);
    CHECK(n==1 && *(uint16_t *)((char *)memory+route+0x100u)==l12);
    /* a door crossed backwards needs [0x00715F50] */
    WORD_AT(LINKS+l12*0x14u)|=2u;
    WORD_AT(MAPMAKER_ASTAR_DOORS)=0;
    CHECK(mapmaker_astar_search((int32_t)p0,(int32_t)p2,route,300)==0);
    WORD_AT(MAPMAKER_ASTAR_DOORS)=1;
    CHECK(mapmaker_astar_search((int32_t)p0,(int32_t)p2,route,300)==2);
    /* masked or unusable links close the way */
    WORD_AT(LINKS+l01*0x14u)|=8u;
    CHECK(mapmaker_astar_search((int32_t)p0,(int32_t)p2,route,300)==0);
    WORD_AT(LINKS+l01*0x14u)=0x00000u;
    CHECK(mapmaker_astar_search((int32_t)p0,(int32_t)p2,route,300)==0);
    /* the node pool bound: a full pool takes no node */
    WORD_AT(LINKS+l01*0x14u)=0x10000u;
    WORD_AT(MAPMAKER_ASTAR_FREE)=0xFFFFFFFFu;WORD_AT(MAPMAKER_ASTAR_COUNTER)=MAPMAKER_ASTAR_NODES;
    mapmaker_astar_insert(-1,1,1,0,1.0f);
    CHECK(WORD_AT(MAPMAKER_ASTAR_COUNTER)==MAPMAKER_ASTAR_NODES);
}

/* the wrapper reads the cdecl arguments and returns like the original's ret */
static void original_called(void) { failures++; }
static void test_wrapper(void)
{
    reset(9);
    build_graph(5,4);
    call_search(0,3,ROUTES,300);
    CHECK(g_esp==STACK-0x10u && mapmaker_astar.searches>=1);
    (void)original_called;
}

static int dump(const char *path)
{
    FILE *f=fopen(path,"wb");
    if(!f) return 0;
    fwrite(memory,1,sizeof memory,f);
    fclose(f);
    return 1;
}

int main(int argc,char **argv)
{
    set_mode(1);
    if(argc==6 && !strcmp(argv[1],"dump")) {
        uint32_t seed=(uint32_t)strtoul(argv[2],NULL,0),side=(uint32_t)strtoul(argv[3],NULL,0),s;
        build_graph(seed,side);
        make_series(seed);
        for(s=0;s<series_n;s++) memset((char *)memory+series[s][2],0xCC,0x400u);
        memset((char *)memory+MAPMAKER_ASTAR_POOL,0xCD,0x20000u);
        if(!dump(argv[4])) return 2;
        for(s=0;s<series_n;s++) {
            WORD_AT(MAPMAKER_ASTAR_DOORS)=s&1u;
            g_esp=STACK-0x14u;
            g_eax=mapmaker_astar_search((int32_t)series[s][0],(int32_t)series[s][1],series[s][2],(int32_t)series[s][3]);
            printf("%u %u %u %u %u\n",series[s][0],series[s][1],series[s][2],series[s][3],g_eax);
        }
        return dump(argv[5])?0:2;
    }
    if(argc==2 && !strcmp(argv[1],"golden")) { test_counts_equal_walks(1);return 0; }
    test_chain();
    test_wrapper();
    test_counts_equal_walks(0);
    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    puts("mapmaker astar ok");
    return 0;
}
