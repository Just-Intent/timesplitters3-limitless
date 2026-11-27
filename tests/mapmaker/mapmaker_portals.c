/* Asset-free contract for src/interface/pc_mapmaker_portals.inc (the MapMaker
 * play build's three portal passes in C): synthetic maps (a grid of tiles with
 * one to three portals per side, jittered so some sides join and some do not,
 * stacked duplicate tiles, floors, odd vertex counts, NaN and degenerate
 * portals) give the same guest memory with the centroid grid as with every
 * pair tested, and small hand-checked maps give the expected joins, removals
 * and merged quads. With arguments `dump <seed> <tiles> <pre> <post>` it
 * writes the guest memory before and after the passes (whole fixture image)
 * for tests/mapmaker/test_mapmaker_portals.py, which runs the original guest
 * loops (0x0008ADDD..0x0008BA61 of the XBE) on the pre image in an emulator
 * and compares. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMORY_BYTES 0x00920000u
static uint32_t memory[MEMORY_BYTES/4], g_eax, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<MEMORY_BYTES && n<=MEMORY_BYTES-p; }
#include "pc_mapmaker_portals.inc"
#define MAPMAKER_GUEST_RET() (g_esp+=4u)
#include "pc_mapmaker_roomray.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

#define HEAP_BASE 0x00100000u
#define HEAP_END 0x00460000u
#define FRAME 0x00880000u
#define MAX_PORTALS 6000u
#define MAX_ROOMS 600u
#define LIST_CAP 64u

static uint32_t heap,rooms_n,portals_n,records,table_ptrs[MAX_PORTALS+2],lists[MAX_ROOMS+1];
static uint32_t rng=1;
static int walls_inward;               /* wall normals into the tile instead of out of it */
static uint32_t next_rand(void) { rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng; }
static float frand(float lo,float hi) { return lo+(hi-lo)*(float)(next_rand()%100000u)/100000.0f; }
static void set_f(uint32_t a,float f) { uint32_t w; memcpy(&w,&f,4); WORD_AT(a)=w; }
static float get_f(uint32_t a) { float f; uint32_t w=WORD_AT(a); memcpy(&f,&w,4); return f; }
static uint32_t alloc(uint32_t n)
{
    uint32_t r=heap;
    heap+=(n+15u)&~15u;
    if(heap>HEAP_END) { fprintf(stderr,"fixture heap full\n");exit(2); }
    memset((char *)memory+r,0,n);
    return r;
}

static void reset(uint32_t seed)
{
    static const uint32_t k[][2]={{0x0047D3A4u,0xBF7FBE77u},{0x00475DBCu,0x3A83126Fu},{0x00475D5Cu,0x7F7FFFFFu},
        {0x00478618u,0xFF7FFFFFu},{0x00475C78u,0x3F800000u},{0x0047D3A0u,0x3E800D1Bu},{0x0047D39Cu,0x3FA00347u},
        {0x00476040u,0x3F666666u},{0x0047D398u,0xBF666666u},{0x004765F4u,0x33D6BF95u},
        {0x0047D394u,0xBF11EB85u},{0x0047D390u,0x3F11EB85u}};   /* ray step: -0.57, 0.57 */
    unsigned i;
    memset(memory,0,sizeof memory);
    g_xbox_mem_offset=(uintptr_t)memory;
    for(i=0;i<sizeof k/sizeof k[0];i++) WORD_AT(k[i][0])=k[i][1];
    heap=HEAP_BASE;rooms_n=portals_n=0;rng=seed?seed:1u;
    memset(table_ptrs,0,sizeof table_ptrs);memset(lists,0,sizeof lists);
}
static uint32_t add_room(void)
{
    rooms_n++;
    lists[rooms_n]=alloc(4u+4u*LIST_CAP);
    return rooms_n;
}
static void list_add(uint32_t room,uint32_t id)
{
    uint32_t l=lists[room],n=WORD_AT(l);
    if(n>=LIST_CAP) { fprintf(stderr,"fixture list full\n");exit(2); }
    WORD_AT(l+4u+4u*n)=id;WORD_AT(l)=n+1u;
}
/* A portal of cnt vertices (v: x,y,z each); rooms as after the build's marking. */
static uint32_t add_portal(int32_t r0,int32_t r1,const float *n,const float *v,int cnt,int16_t stored_cnt)
{
    uint32_t p=alloc(0x18u+12u*(uint32_t)(cnt>0?cnt:0)+4u),k;
    if(portals_n>=MAX_PORTALS) { fprintf(stderr,"fixture portals full\n");exit(2); }
    WORD_AT(p)=(uint32_t)r0;WORD_AT(p+4u)=(uint32_t)r1;
    set_f(p+8u,n[0]);set_f(p+0xCu,n[1]);set_f(p+0x10u,n[2]);
    *(uint16_t *)((char *)memory+p+0x14u)=(uint16_t)stored_cnt;
    for(k=0;k<(uint32_t)(cnt>0?cnt:0)*3u;k++) set_f(p+0x18u+4u*k,v[k]);
    table_ptrs[++portals_n]=p;
    if(r0>=0) list_add((uint32_t)r0,portals_n);
    if(r1>=0 && r1!=r0) list_add((uint32_t)r1,portals_n);
    return portals_n;
}
/* A vertical quad in the plane axis = at (0: x, 2: z), spanning [a0,a1] on the
 * other horizontal axis and y [y0,y1], with jitter; normal +-axis. */
static uint32_t wall(uint32_t room,int axis,float at,float a0,float a1,float y0,float y1,float sign,float jitter,int order)
{
    float v[12],n[3]={0,0,0};
    static const int corner[4][2]={{0,0},{1,0},{1,1},{0,1}};
    int c;
    for(c=0;c<4;c++) {
        int cc=(c+order)&3;
        float a=corner[cc][0]?a1:a0,y=corner[cc][1]?y1:y0;
        float *w=v+3*c;
        w[axis]=at+frand(-jitter,jitter);
        w[1]=y+frand(-jitter,jitter);
        w[2-axis]=a+frand(-jitter,jitter);
    }
    n[axis]=walls_inward?-sign:sign;    /* inward: the side a ray leaves the room through (ray tests) */
    if(order==7) { n[axis]=sign*0.99f;n[1]=0.1410674f*sign; }  /* slightly tilted normal: joins only another tilted one */
    if(next_rand()%5u==0) return add_portal((int32_t)room,-1,n,v,4,4);   /* the room in the first slot */
    return add_portal(-1,(int32_t)room,n,v,4,4);
}
static void finish(void)
{
    uint32_t r,p;
    records=alloc((rooms_n+1u)*0xB4u);
    for(r=1;r<=rooms_n;r++) WORD_AT(records+r*0xB4u+4u)=lists[r];
    WORD_AT(0x00708788u)=records;
    WORD_AT(0x007B975Cu)=rooms_n;
    WORD_AT(0x007B9754u)=portals_n;
    p=alloc((portals_n+1u)*4u+0x10u);
    memcpy((char *)memory+p,table_ptrs,(portals_n+1u)*4u);
    WORD_AT(0x0070878Cu)=p;
    memset((char *)memory+FRAME+0x1280u,1,0x4000u);   /* 0x0008ADF0 */
}

/* A tiles x tiles grid (4 units per tile, one room each, seg portals per
 * side), plus stacked duplicates, a floor above part of it and odd portals. */
static void build_grid(uint32_t seed,uint32_t tiles)
{
    uint32_t gx,gz,s,room[64][64],extra;
    reset(seed);
    for(gz=0;gz<tiles;gz++) for(gx=0;gx<tiles;gx++) room[gz][gx]=add_room();
    /* a side's split depends on its boundary line only, so both tiles of a shared side agree */
    for(gz=0;gz<tiles;gz++) for(gx=0;gx<tiles;gx++) {
        float x0=(float)gx*4.0f,z0=(float)gz*4.0f;
        int side;
        for(side=0;side<4;side++) {
            uint32_t line=side==0?gx:side==1?gx+1u:side==2?gz:gz+1u;
            uint32_t seg=side<2?1u+(line*7u+gz)%3u:1u+(gx+line*5u)%3u;
            for(s=0;s<seg;s++) {
                float a0=(float)s*4.0f/(float)seg,a1=(float)(s+1u)*4.0f/(float)seg;
                float jit=(next_rand()%11u)==0?0.6f:0.05f;   /* some sides do not join */
                int order=(int)(next_rand()%8u);
                if(side==0) wall(room[gz][gx],0,x0,z0+a0,z0+a1,0.0f,3.0f,-1.0f,jit,order);
                if(side==1) wall(room[gz][gx],0,x0+4.0f,z0+a0,z0+a1,0.0f,3.0f,1.0f,jit,order);
                if(side==2) wall(room[gz][gx],2,z0,x0+a0,x0+a1,0.0f,3.0f,-1.0f,jit,order);
                if(side==3) wall(room[gz][gx],2,z0+4.0f,x0+a0,x0+a1,0.0f,3.0f,1.0f,jit,order);
            }
        }
    }
    /* stacked duplicates (placing over an occupied cell): copies of grid portals, same or turned normal */
    for(extra=0;extra<6u+tiles;extra++) {
        uint32_t r=add_room(),src=1u+next_rand()%portals_n,k;
        uint32_t sp=table_ptrs[src];
        float v[12],n[3];
        int cnt=(int16_t)*(uint16_t *)((char *)memory+sp+0x14u);
        if(cnt!=4) continue;
        for(k=0;k<12;k++) v[k]=get_f(sp+0x18u+4u*k)+frand(-0.01f,0.01f);
        for(k=0;k<3;k++) n[k]=get_f(sp+8u+4u*k)*((next_rand()&1u)?-1.0f:1.0f);
        add_portal(-1,(int32_t)r,n,v,4,4);
        if(next_rand()&1u) add_portal(-1,(int32_t)r,n,v,4,4);
    }
    /* floor portals between room 1 and a room above, twice (horizontal: pass 3 skips them) */
    {
        uint32_t up=add_room();
        float v[12]={0,3,0, 4,3,0, 4,3,4, 0,3,4},w[12]={4,3,0, 8,3,0, 8,3,4, 4,3,4},n1[3]={0,1,0},n2[3]={0,-1,0};
        add_portal(-1,(int32_t)room[0][0],n1,v,4,4);
        add_portal(-1,(int32_t)up,n2,v,4,4);
        add_portal(-1,(int32_t)up,n2,v,4,4);              /* a second copy: its partner is taken */
        add_portal(-1,(int32_t)room[0][0],n1,w,4,4);
        add_portal(-1,(int32_t)up,n2,w,4,4);
    }
    /* far from the grid: equal centroids but boxes 0.4 apart (no join); minima equal and the
     * maxima 1 apart (joins only with the far threshold 1.2501) */
    {
        uint32_t a=add_room(),b=add_room(),c=add_room(),d=add_room();
        float wide[12]={100,0,0, 100,3,0, 100,3,4, 100,0,4},narrow[12]={100,0.4f,0.4f, 100,2.6f,0.4f, 100,2.6f,3.6f, 100,0.4f,3.6f};
        float lo[12]={200,0,0, 200,3,0, 200,3,4, 200,0,4},hi[12]={200,0,0, 200,4,0, 200,4,5, 200,0,5};
        float nx[3]={1,0,0},mx[3]={-1,0,0};
        add_portal(-1,(int32_t)a,nx,wide,4,4);
        add_portal(-1,(int32_t)b,mx,narrow,4,4);
        add_portal((int32_t)c,-1,nx,lo,4,4);
        add_portal(-1,(int32_t)d,mx,hi,4,4);
    }
    /* odd portals: no vertices, a triangle, five vertices, a NaN vertex, a negative count, pre-paired */
    {
        float tri[15]={0,0,0, 0,3,0, 0,3,4, 0,0,4, 0,1.5f,2},n[3]={-1,0,0},nan_v[12];
        uint32_t k;
        union { uint32_t u; float f; } q; q.u=0x7FC00000u;
        add_portal(-1,1,n,tri,0,0);
        add_portal(-1,1,n,tri,3,3);
        add_portal(-1,1,n,tri,5,5);
        for(k=0;k<12;k++) nan_v[k]=tri[k];
        nan_v[4]=q.f;
        add_portal(-1,1,n,nan_v,4,4);
        add_portal(-1,1,n,tri,0,-2);
        add_portal(1,2,n,tri,4,4);                          /* already between rooms 1 and 2 */
        add_portal(2,1,n,tri,4,4);                          /* same pair, reversed: pass 3 group */
    }
    finish();
}

static void run(int brute)
{
#ifdef _WIN32
    _putenv(brute?"TSFP_MAPMAKER_PORTAL_INDEX=0":"TSFP_MAPMAKER_PORTAL_INDEX=1");
#else
    setenv("TSFP_MAPMAKER_PORTAL_INDEX",brute?"0":"1",1);
#endif
    g_esp=FRAME-MAPMAKER_PORTALS_FRAME;
    WORD_AT(g_esp)=MAPMAKER_PORTALS_RET;
    mapmaker_portals_link(FRAME);
}
static void fake_original(void) { g_esp+=4u; }

/* FNV-1a 64 of the fixture heap and the frame's flag bytes */
static unsigned long long image_hash(void)
{
    unsigned long long h=1469598103934665603ull;
    uint32_t a;
    for(a=HEAP_BASE;a<heap;a++) { h^=((uint8_t *)memory)[a];h*=1099511628211ull; }
    for(a=FRAME+0x1280u;a<FRAME+0x5280u;a++) { h^=((uint8_t *)memory)[a];h*=1099511628211ull; }
    return h;
}
/* Results of the fixed passes (`golden` prints them); the seeds that
 * test_mapmaker_portals.py dumps are also compared with the original guest
 * loops there. */
static const unsigned long long golden[25]={0,
    0xFF8C2C91CAE3EE3Dull,0x192CF360B03BF7CBull,0x55248F51279A4C21ull,0x141DF9FFB8D5105Aull,0x7B5262D6118E7723ull,
    0x90E7BEE9851EF63Bull,0xDFBDBF68B806AF72ull,0x19B58AA7A93E2D71ull,0x8997D6D5DBB3F597ull,0x4578FA645538F689ull,
    0xCA05E7FD13DA2E5Cull,0x78DFA16311BA94D4ull,0x533C10F890F25C9Dull,0x1887D8ADC0E6D33Full,0xBCF8DC01FC808877ull,
    0x9FC1D61741D05B38ull,0x0FF749B71AD5EC07ull,0xD2CB677F984A8002ull,0x748E5D640CECD1D1ull,0x023C6A3E63644EDFull,
    0x11E4B3A096CDB3C3ull,0xDC6CDA4C2347F8F3ull,0xD12990D010DBF349ull,0xE1B1FEC403003751ull};

static void test_index_equals_every_pair(int print)
{
    static uint32_t a[MEMORY_BYTES/4];
    uint32_t seed,tiles;
    for(seed=1;seed<=24;seed++) {
        tiles=2u+seed%9u;
        build_grid(seed*2654435761u,tiles);
        run(1);
        memcpy(a,memory,sizeof memory);
        build_grid(seed*2654435761u,tiles);
        run(0);
        CHECK(!memcmp(a,memory,sizeof memory));
        if(memcmp(a,memory,sizeof memory)) fprintf(stderr,"seed %u tiles %u differs\n",seed,tiles);
        if(print) printf("0x%016llXull,\n",image_hash());
        else if(image_hash()!=golden[seed]) { fprintf(stderr,"seed %u: hash 0x%016llX\n",seed,image_hash());failures++; }
    }
}

/* Two tiles: their shared side joins, the outer sides leave the lists. */
static void test_two_tiles(void)
{
    uint32_t a,b,r1,r2,l1,l2,i;
    float n0[3]={1,0,0},n1[3]={-1,0,0};
    float va[12]={4,0,0, 4,3,0, 4,3,4, 4,0,4},vb[12]={4,0,4, 4,3,4, 4,3,0, 4,0,0};
    float vo[12]={0,0,0, 0,3,0, 0,3,4, 0,0,4};
    reset(7);
    r1=add_room();r2=add_room();
    add_portal(-1,(int32_t)r1,n1,vo,4,4);              /* 1: room 1's outer side */
    a=add_portal(-1,(int32_t)r1,n0,va,4,4);            /* 2 */
    b=add_portal(-1,(int32_t)r2,n1,vb,4,4);            /* 3 */
    finish();
    run(0);
    CHECK((int32_t)WORD_AT(table_ptrs[a])==(int32_t)r2 && WORD_AT(table_ptrs[a]+4u)==r1);
    CHECK(WORD_AT(table_ptrs[b])==r1 && WORD_AT(table_ptrs[b]+4u)==r2);
    l1=lists[r1];l2=lists[r2];
    /* equal boxes: the later one is kept (0x0008B2B8), so room 1 lists 3; portal 1 was removed */
    CHECK(WORD_AT(l1)==1u && WORD_AT(l1+4u)==b);
    CHECK(WORD_AT(l2)==1u && WORD_AT(l2+4u)==b);
    CHECK(*((uint8_t *)memory+FRAME+0x1280u+a)==0 && *((uint8_t *)memory+FRAME+0x1280u+1u)==0);
    CHECK(*((uint8_t *)memory+FRAME+0x1280u+b)==1);
    CHECK((int32_t)WORD_AT(table_ptrs[1])==-1);
    for(i=0;i<3;i++) CHECK(*((uint8_t *)memory+FRAME+0x1280u+4u+i)==1);   /* past the count: untouched */
    /* the larger box is kept: grow portal 3 a little */
    reset(7);
    r1=add_room();r2=add_room();
    add_portal(-1,(int32_t)r1,n1,vo,4,4);
    a=add_portal(-1,(int32_t)r1,n0,va,4,4);
    vb[4]=3.2f;
    b=add_portal(-1,(int32_t)r2,n1,vb,4,4);
    vb[4]=3.0f;
    finish();
    run(1);
    CHECK(WORD_AT(lists[r1])==1u && WORD_AT(lists[r1]+4u)==b);
    reset(7);
    r1=add_room();r2=add_room();
    add_portal(-1,(int32_t)r1,n1,vo,4,4);
    va[4]=3.2f;
    a=add_portal(-1,(int32_t)r1,n0,va,4,4);
    va[4]=3.0f;
    b=add_portal(-1,(int32_t)r2,n1,vb,4,4);
    finish();
    run(1);
    CHECK(WORD_AT(lists[r2])==1u && WORD_AT(lists[r2]+4u)==a);   /* 2 kept, 3 replaced by 2 */
    CHECK(*((uint8_t *)memory+FRAME+0x1280u+b)==0);
    /* the last portal is never an outer i, but it is a partner j */
    CHECK(portals_n==3u && (int32_t)WORD_AT(table_ptrs[b])>=0);
}

/* Perpendicular walls (dot 0) and centroids just apart do not join. */
static void test_no_join(void)
{
    float n0[3]={1,0,0},n1[3]={0,0,1},nf[3]={-1,0,0};
    float va[12]={4,0,0, 4,3,0, 4,3,4, 4,0,4};
    float vf[12]={4.6f,0,0, 4.6f,3,0, 4.6f,3,4, 4.6f,0,4};
    reset(3);
    add_room();add_room();add_room();
    add_portal(-1,1,n0,va,4,4);
    add_portal(-1,2,n1,va,4,4);
    add_portal(-1,3,nf,vf,4,4);
    list_add(1,1);                                       /* listed twice: only the first place goes */
    finish();
    run(1);
    CHECK((int32_t)WORD_AT(table_ptrs[1])==-1 && (int32_t)WORD_AT(table_ptrs[2])==-1 && (int32_t)WORD_AT(table_ptrs[3])==-1);
    CHECK(WORD_AT(lists[1])==1 && WORD_AT(lists[1]+4u)==1 && WORD_AT(lists[2])==0 && WORD_AT(lists[3])==0);
}

/* Three coplanar portals between rooms 1 and 2 become one quad of their box. */
static void test_merge(void)
{
    float n[3]={-1,0,0};
    float v1[12]={4,0,0, 4,3,0, 4,3,1, 4,0,1},v2[12]={4,0,1, 4,3,1, 4,3,2.5f, 4,0,2.5f},v3[12]={4,0,2.5f, 4,3,2.5f, 4,3.5f,4, 4,0,4};
    uint32_t f,i;
    reset(5);
    add_room();add_room();
    add_portal(1,2,n,v1,4,4);
    add_portal(2,1,n,v2,4,4);
    add_portal(1,2,n,v3,4,4);
    finish();
    run(1);
    f=table_ptrs[1];
    /* (max x,max y,max z) (max x,min y,max z) (min x,min y,min z) (min x,max y,min z) */
    CHECK(get_f(f+0x18u)==4.0f && get_f(f+0x1Cu)==3.5f && get_f(f+0x20u)==4.0f);
    CHECK(get_f(f+0x24u)==4.0f && get_f(f+0x28u)==0.0f && get_f(f+0x2Cu)==4.0f);
    CHECK(get_f(f+0x30u)==4.0f && get_f(f+0x34u)==0.0f && get_f(f+0x38u)==0.0f);
    CHECK(get_f(f+0x3Cu)==4.0f && get_f(f+0x40u)==3.5f && get_f(f+0x44u)==0.0f);
    /* normal (dz*dy, 0, -dx*dy) normalised = (1,0,0): turned from (-1,0,0), so the rooms swap */
    CHECK(get_f(f+8u)==1.0f && get_f(f+0x10u)==0.0f);
    CHECK(WORD_AT(f)==2u && WORD_AT(f+4u)==1u);
    for(i=1;i<=2;i++) CHECK(WORD_AT(lists[i])==1u && WORD_AT(lists[i]+4u)==1u);
    CHECK(*((uint8_t *)memory+FRAME+0x1280u+2u)==0 && *((uint8_t *)memory+FRAME+0x1280u+3u)==0);
    /* at most 24 in one group: the 25th is merged by the next outer portal */
    reset(5);
    add_room();add_room();
    for(i=0;i<26;i++) {
        float v[12]={4,0,(float)i, 4,3,(float)i, 4,3,(float)i+1.0f, 4,0,(float)i+1.0f},nn[3]={1,0,0};
        add_portal(1,2,nn,v,4,4);
    }
    finish();
    run(0);
    CHECK(WORD_AT(lists[1])==2u && WORD_AT(lists[1]+4u)==1u && WORD_AT(lists[1]+8u)==25u);
    CHECK(get_f(table_ptrs[25]+0x2Cu)==26.0f && get_f(table_ptrs[1]+0x2Cu)==24.0f);
}

/* The hook acts only on sub_0008A780's call and the build checks it ran. */
static void test_hook(void)
{
    unsigned long long direct;
    build_grid(11,3);
    run(0);
    direct=image_hash();
    build_grid(11,3);
    g_esp=FRAME-0x1Cu;
    WORD_AT(g_esp)=0x0008A76Du;                          /* sub_0008A330's call: story levels */
    mapmaker_portals.builds=0;
    pc_mapmaker_rooms_portal_link(fake_original);
    CHECK(mapmaker_portals.builds==0 && g_esp==FRAME-0x1Cu+4u);
    g_esp=FRAME-0x1Cu;
    WORD_AT(g_esp)=0x0008BAB2u;
    mapmaker_portals_build_begin();
    CHECK(WORD_AT(MAPMAKER_PORTALS_ZERO)==0 && mapmaker_portals.pending);
    pc_mapmaker_rooms_portal_link(fake_original);
    CHECK(mapmaker_portals.builds==1 && !mapmaker_portals.pending && g_esp==FRAME-0x1Cu+4u);
    CHECK(image_hash()==direct);                         /* the frame (flag bytes) found from the return word */
    mapmaker_portals_build_end();
    CHECK(mapmaker_portals.missed==0);
    mapmaker_portals_build_begin();
    mapmaker_portals_build_end();
    CHECK(mapmaker_portals.missed==1);
}

static int dump(const char *path)
{
    FILE *f=fopen(path,"wb");
    if(!f) return 0;
    fwrite(memory,1,sizeof memory,f);
    fclose(f);
    return 1;
}

/* Ray steps (sub_00088430 in C) on the joined grid: random origins in the
 * grid, vectors (some zero or along an axis), from-rooms, bounded or not.
 * Prints: room from segment ox oy oz dx dy dz (float bits) result, then
 * the hit and rest buffers (bits). The emulator test runs the original. */
#define RAY_ORIGIN 0x00870000u
#define RAY_DIR 0x00870010u
#define RAY_HIT 0x00870020u
#define RAY_REST 0x00870030u
static uint32_t ray_bits(float f) { uint32_t w; memcpy(&w,&f,4); return w; }
static void ray_steps(uint32_t count,uint32_t tiles,int print)
{
    uint32_t s,k;
    for(s=0;s<count;s++) {
        uint32_t room=1u+next_rand()%rooms_n,from=next_rand()%3u?0u:1u+next_rand()%rooms_n,segment=next_rand()&1u,r;
        float o[3],d[3];
        if(room<=tiles*tiles && next_rand()%4u) {   /* mostly inside the room's own tile */
            o[0]=4.0f*(float)((room-1u)%tiles)+frand(0.3f,3.7f);o[2]=4.0f*(float)((room-1u)/tiles)+frand(0.3f,3.7f);
        } else { o[0]=frand(-2.0f,(float)tiles*4.0f+2.0f);o[2]=frand(-2.0f,(float)tiles*4.0f+2.0f); }
        o[1]=frand(0.0f,3.0f);
        d[0]=frand(-12.0f,12.0f);d[1]=frand(-1.0f,1.0f);d[2]=frand(-12.0f,12.0f);
        if(next_rand()%3u==0) { d[0]*=0.15f;d[1]*=0.15f;d[2]*=0.15f; }   /* short: ends inside the room */
        if(next_rand()%9u==0) d[0]=d[1]=d[2]=0.0f;
        if(next_rand()%7u==0) { d[1]=0.0f;d[2]=0.0f; }
        if(next_rand()%11u==0) room=0x400u+next_rand()%3u;   /* past the real rooms */
        for(k=0;k<3;k++) { set_f(RAY_ORIGIN+4u*k,o[k]);set_f(RAY_DIR+4u*k,d[k]); }
        for(k=0;k<12u;k++) { WORD_AT(RAY_HIT+4u*k)=0xA5A50000u+k; }
        r=mapmaker_ray_step(RAY_ORIGIN,RAY_DIR,room,from,segment,RAY_HIT,RAY_REST);
        if(print) {
            printf("%u %u %u",room,from,segment);
            for(k=0;k<3;k++) printf(" %u",ray_bits(o[k]));
            for(k=0;k<3;k++) printf(" %u",ray_bits(d[k]));
            printf(" %u",r);
            for(k=0;k<8;k++) printf(" %u",WORD_AT(RAY_HIT+4u*k));
            printf("\n");
        }
    }
}

/* every call of a series lands somewhere: some rays cross, some do not */
static void test_rays(void)
{
    uint32_t crossed=0,s;
    walls_inward=1;
    build_grid(4242,6);
    run(0);
    for(s=0;s<400;s++) {
        uint32_t room=1u+s%rooms_n,r;
        float o[3]={2.0f+4.0f*(float)((room-1u)%6u),1.5f,2.0f+4.0f*(float)((room-1u)/6u%6u)},d[3]={6.0f,0.0f,0.5f};
        uint32_t k;
        for(k=0;k<3;k++) { set_f(RAY_ORIGIN+4u*k,o[k]);set_f(RAY_DIR+4u*k,d[k]); }
        r=mapmaker_ray_step(RAY_ORIGIN,RAY_DIR,room,0,0,RAY_HIT,RAY_REST);
        if(r!=room) crossed++;
    }
    CHECK(crossed>0u && crossed<400u);
    /* a zero vector and a room past the bound return the room */
    WORD_AT(RAY_DIR)=0;WORD_AT(RAY_DIR+4u)=0;WORD_AT(RAY_DIR+8u)=0;
    CHECK(mapmaker_ray_step(RAY_ORIGIN,RAY_DIR,3,0,0,RAY_HIT,RAY_REST)==3u);
    set_f(RAY_DIR,6.0f);
    CHECK(mapmaker_ray_step(RAY_ORIGIN,RAY_DIR,0x400u,0,0,RAY_HIT,RAY_REST)==0x400u);
    CHECK(mapmaker_ray_step(RAY_ORIGIN,RAY_DIR,0,0,0,RAY_HIT,RAY_REST)==0u);
    walls_inward=0;
}

int main(int argc,char **argv)
{
    if(argc==6 && !strcmp(argv[1],"raydump")) {
        FILE *f;
        uint32_t tiles=(uint32_t)strtoul(argv[3],NULL,0);
        walls_inward=1;
        build_grid((uint32_t)strtoul(argv[2],NULL,0),tiles);
        run(0);
        if(!(f=fopen(argv[4],"wb"))) return 2;
        fwrite(memory,1,sizeof memory,f);fclose(f);
        ray_steps((uint32_t)strtoul(argv[5],NULL,0),tiles,1);
        return 0;
    }
    if(argc==6 && !strcmp(argv[1],"dump")) {
        build_grid((uint32_t)strtoul(argv[2],NULL,0),(uint32_t)strtoul(argv[3],NULL,0));
        memset((char *)memory+FRAME+0x1280u,0xCC,0x4000u);   /* the emulator runs the flag clear itself */
        if(!dump(argv[4])) return 2;
        memset((char *)memory+FRAME+0x1280u,1,0x4000u);
        run(argc>6);
        return dump(argv[5])?0:2;
    }
    test_two_tiles();
    test_no_join();
    test_merge();
    test_hook();
    test_rays();
    if(argc==2 && !strcmp(argv[1],"golden")) { test_index_equals_every_pair(1);return 0; }
    test_index_equals_every_pair(0);
    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    puts("mapmaker portals ok");
    return 0;
}
