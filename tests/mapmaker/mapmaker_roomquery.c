/* Asset-free contract for src/interface/pc_mapmaker_roomquery.inc (the room
 * tree walk of sub_00087310 in host code: sub_00157440, sub_001571C0, the
 * callback sub_00087260 and sub_00047560): random trees (all three split
 * axes, leaves with repeated rooms, rooms already flagged, a NaN room box),
 * boxes inside, across and outside the tree, recorded series and hand-checked
 * cases. With arguments `dump <seed> <rooms> <pre>` it writes the guest memory
 * and prints each query and its list for tests/mapmaker/test_mapmaker_roomquery.py,
 * which runs the original guest code in an emulator. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMORY_BYTES 0x00920000u
static uint32_t memory[MEMORY_BYTES/4];
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#include "pc_mapmaker_roomquery.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

#define TREE 0x00100000u
#define NODES 0x00101000u          /* u16 per node id (ids below 0x4000) */
#define LISTS 0x00110000u
#define RECORDS 0x00200000u        /* 0xB4 per room */
#define BOX 0x00300000u
#define LIST 0x00301000u
#define NODE_IDS 0x4000u
static uint32_t rng=1,list_cursor;
static uint32_t next_rand(void) { rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng; }
static float frand(float lo,float hi) { return lo+(hi-lo)*(float)(next_rand()%100000u)/100000.0f; }
static void set_f(uint32_t a,float f) { uint32_t w; memcpy(&w,&f,4); WORD_AT(a)=w; }
static uint32_t bits(float f) { uint32_t w; memcpy(&w,&f,4); return w; }
static uint16_t get16(uint32_t a) { return *(uint16_t *)((uintptr_t)a+g_xbox_mem_offset); }
static void put16(uint32_t a,uint32_t v) { *(uint16_t *)((uintptr_t)a+g_xbox_mem_offset)=(uint16_t)v; }

static void build_node(uint32_t node,unsigned depth,unsigned max_depth,uint32_t rooms)
{
    if(node*2u+1u>=NODE_IDS || depth>=max_depth || (depth>1u && next_rand()%4u==0)) {
        uint32_t n=next_rand()%6u,i;
        put16(NODES+node*2u,0xC000u|list_cursor);
        put16(LISTS+list_cursor*2u,n);
        for(i=0;i<n;i++) put16(LISTS+(list_cursor+1u+i)*2u,1u+next_rand()%rooms);
        list_cursor+=n+1u;
        return;
    }
    put16(NODES+node*2u,(next_rand()%3u)<<14|next_rand()%0x4000u);
    build_node(node*2u,depth+1u,max_depth,rooms);
    build_node(node*2u+1u,depth+1u,max_depth,rooms);
}
static void build(uint32_t seed,uint32_t rooms)
{
    uint32_t r,k;
    memset(memory,0,sizeof memory);
    g_xbox_mem_offset=(uintptr_t)memory;
    rng=seed?seed:1u;list_cursor=0;
    WORD_AT(0x00708788u)=RECORDS;
    WORD_AT(MAPMAKER_ROOMS_SPLIT_K)=0x38800200u;     /* 1/16383, the XBE's constant */
    WORD_AT(TREE)=NODES;WORD_AT(TREE+4u)=LISTS;
    for(k=0;k<3u;k++) { set_f(TREE+8u+k*4u,-100.0f);set_f(TREE+0x14u+k*4u,100.0f); }
    for(r=1;r<=rooms;r++) {
        uint32_t rec=RECORDS+r*0xB4u;
        for(k=0;k<3u;k++) {
            float lo=frand(-110.0f,100.0f);
            set_f(rec+0x10u+k*4u,lo);set_f(rec+0x1Cu+k*4u,lo+frand(0.0f,30.0f));
        }
        if(next_rand()%11u==0) *(uint8_t *)((uintptr_t)rec+0xB2u+g_xbox_mem_offset)|=0x10u;   /* already listed */
        *(uint8_t *)((uintptr_t)rec+0xB2u+g_xbox_mem_offset)|=(uint8_t)(next_rand()&0xEFu);
    }
    if(rooms>3u) WORD_AT(RECORDS+3u*0xB4u+0x14u)=0x7FC00000u;   /* a NaN room box: overlaps on that axis */
    build_node(1u,0u,4u+seed%5u,rooms);
}
/* one query as pc_mapmaker_rooms_query does it: the walk, then the listed rooms' flags cleared */
static uint32_t query(const float *min,const float *max)
{
    uint32_t k,n,i;
    for(k=0;k<3u;k++) { set_f(BOX+k*4u,min[k]);set_f(BOX+0xCu+k*4u,max[k]); }
    put16(LIST,0);
    mapmaker_rooms_tree_query(TREE,BOX,BOX+0xCu,LIST);
    n=get16(LIST);
    for(i=0;i<n;i++) *(uint8_t *)((uintptr_t)RECORDS+(get16(LIST+2u+i*2u)>>1)*0xB4u+0xB2u+g_xbox_mem_offset)&=0xEFu;
    return n;
}
static unsigned long long series_hash(uint32_t seed,uint32_t rooms,int print)
{
    unsigned long long h=1469598103934665603ull;
    uint32_t q,k,n,i;
    build(seed,rooms);
    for(q=0;q<200u;q++) {
        float min[3],max[3];
        for(k=0;k<3u;k++) {
            float c=frand(-130.0f,130.0f),s=(q%7u==0)?frand(80.0f,200.0f):frand(0.0f,25.0f);
            min[k]=c-s;max[k]=c+s;
        }
        if(q%23u==5u) min[q%3u]=max[q%3u]+1.0f;          /* an empty box */
        n=query(min,max);
        if(print) {
            printf("%u %u %u %u %u %u %u",bits(min[0]),bits(min[1]),bits(min[2]),bits(max[0]),bits(max[1]),bits(max[2]),n);
            for(i=0;i<n;i++) printf(" %u",get16(LIST+2u+i*2u));
            printf("\n");
        }
        h^=n;h*=1099511628211ull;
        for(i=0;i<n;i++) { h^=get16(LIST+2u+i*2u);h*=1099511628211ull; }
    }
    return h;
}

/* Results of the fixed walk (`golden` prints them); the dump seeds are also
 * compared with the original guest code by test_mapmaker_roomquery.py. */
static const unsigned long long golden[9]={0,
    0x6F1F98F4867E984Dull,0xA6D7BCD27F31018Cull,0x99BBF5F98AA08954ull,0xF144CA300217BE58ull,
    0x8D85E0A5F908512Dull,0xC2E1329EEE7F8727ull,0x2FA965AA13C511DBull,0x0C0DB315ABF88A43ull};
static const uint32_t counts[9]={0,1,2,5,9,40,120,400,1023};

static void test_kinds(void)
{
    float lo[3]={-1,-1,-1},hi[3]={1,1,1};
    uint32_t k;
    build(1,4);
    /* a hand tree: root splits x at the middle; left leaf rooms 1, 2, 1; right leaf room 3 */
    put16(NODES+2u,0u<<14|8191u);                  /* split = -100 + 200 * (8191/16383) */
    put16(NODES+4u,0xC000u|0u);put16(LISTS,3u);put16(LISTS+2u,1u);put16(LISTS+4u,2u);put16(LISTS+6u,1u);
    put16(NODES+6u,0xC000u|4u);put16(LISTS+8u,1u);put16(LISTS+10u,3u);
    for(k=1;k<=4u;k++) *(uint8_t *)((uintptr_t)RECORDS+k*0xB4u+0xB2u+g_xbox_mem_offset)=0;
    for(k=0;k<3u;k++) { set_f(RECORDS+1u*0xB4u+0x10u+k*4u,-0.5f);set_f(RECORDS+1u*0xB4u+0x1Cu+k*4u,0.5f);
                        set_f(RECORDS+2u*0xB4u+0x10u+k*4u,5.0f);set_f(RECORDS+2u*0xB4u+0x1Cu+k*4u,6.0f);
                        set_f(RECORDS+3u*0xB4u+0x10u+k*4u,0.0f);set_f(RECORDS+3u*0xB4u+0x1Cu+k*4u,2.0f); }
    CHECK(query(lo,hi)==3u);                       /* the box crosses the split: both leaves */
    CHECK(get16(LIST+2u)==(1u*2u|1u) && get16(LIST+4u)==2u*2u && get16(LIST+6u)==(3u*2u|1u));
    lo[0]=-50.0f;hi[0]=-40.0f;
    CHECK(query(lo,hi)==2u);                       /* left only; room 1 once */
    lo[0]=1.0f;hi[0]=1.0f;                         /* touching: max >= split, and room 3's box from 0 */
    CHECK(query(lo,hi)==1u && get16(LIST+2u)==(3u*2u|1u));
    lo[0]=150.0f;hi[0]=160.0f;
    CHECK(query(lo,hi)==0u);                       /* outside the tree's box */
    *(uint8_t *)((uintptr_t)RECORDS+2u*0xB4u+0xB2u+g_xbox_mem_offset)=0x10u;   /* already listed: skipped and left */
    lo[0]=-50.0f;hi[0]=-40.0f;
    CHECK(query(lo,hi)==1u && (*(uint8_t *)((uintptr_t)RECORDS+2u*0xB4u+0xB2u+g_xbox_mem_offset)&0x10u));
    *(uint8_t *)((uintptr_t)RECORDS+2u*0xB4u+0xB2u+g_xbox_mem_offset)=0;
    lo[0]=2.0f;hi[0]=3.0f;                         /* touching room 3's max x: still overlaps */
    CHECK(query(lo,hi)==1u && get16(LIST+2u)==(3u*2u|1u));
    WORD_AT(RECORDS+3u*0xB4u+0x14u)=0x7FC00000u;   /* NaN min y: no axis fails on NaN */
    lo[0]=1.0f;hi[0]=1.0f;
    CHECK(query(lo,hi)==1u && get16(LIST+2u)==(3u*2u|1u));
    put16(NODES+2u,0u<<14|0u);                     /* split = -100 exactly, the box min */
    lo[0]=-100.0f;hi[0]=-90.0f;
    CHECK(query(lo,hi)==3u && get16(LIST+2u)==(1u*2u));   /* left (split >= min) before right */
}

int main(int argc,char **argv)
{
    uint32_t s;
    if(argc==5 && !strcmp(argv[1],"dump")) {
        FILE *f;
        uint32_t seed=(uint32_t)strtoul(argv[2],NULL,0),n=(uint32_t)strtoul(argv[3],NULL,0);
        build(seed,n);
        if(!(f=fopen(argv[4],"wb"))) return 2;
        fwrite(memory,1,sizeof memory,f);fclose(f);
        series_hash(seed,n,1);
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"golden")) {
        for(s=1;s<=8;s++) printf("0x%016llXull,\n",series_hash(s*2654435761u,counts[s],0));
        return 0;
    }
    test_kinds();
    for(s=1;s<=8;s++) {
        unsigned long long h=series_hash(s*2654435761u,counts[s],0);
        if(h!=golden[s]) { fprintf(stderr,"seed %u: 0x%016llX\n",s,h);failures++; }
    }
    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    puts("mapmaker roomquery ok");
    return 0;
}
