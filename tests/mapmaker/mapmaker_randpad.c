/* Asset-free contract for src/interface/pc_mapmaker_randpad.inc (sub_0006A8B0,
 * a random pad in a ring, in host code): random pad pools (disabled pads,
 * pads without the scene-100 flag, a NaN position) and rings (inner and/or
 * outer radius, or neither) match recorded results, generator states and
 * argument-slot draws, and hand-checked cases. With arguments
 * `dump <seed> <pads> <pre>` it writes the guest memory and prints each query
 * and result for tests/mapmaker/test_mapmaker_randpad.py, which runs the
 * original guest code (0x0006A8B0..0x0006A9EB and the generator
 * 0x00153B40..0x00153B79 of the XBE) in an emulator. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMORY_BYTES 0x00920000u
static uint32_t memory[MEMORY_BYTES/4], g_eax, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<MEMORY_BYTES && n<=MEMORY_BYTES-p; }
#define MAPMAKER_GUEST_RET() (g_esp+=4u)
#include "pc_mapmaker_randpad.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

#define PADS 0x00100000u
#define QUERY 0x00300000u
#define STACK 0x00400000u
static uint32_t rng=1;
static uint32_t next_rand(void) { rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng; }
static float frand(float lo,float hi) { return lo+(hi-lo)*(float)(next_rand()%100000u)/100000.0f; }
static void set_f(uint32_t a,float f) { uint32_t w; memcpy(&w,&f,4); WORD_AT(a)=w; }
static uint32_t bits(float f) { uint32_t w; memcpy(&w,&f,4); return w; }

static void build(uint32_t seed,uint32_t n)
{
    uint32_t i;
    memset(memory,0,sizeof memory);
    g_xbox_mem_offset=(uintptr_t)memory;
    rng=seed?seed:1u;
    WORD_AT(MAPMAKER_RANDPAD_PADS)=PADS;WORD_AT(MAPMAKER_RANDPAD_COUNT)=n;
    WORD_AT(MAPMAKER_RANDPAD_K_HALF)=0x3F000000u;
    WORD_AT(MAPMAKER_RANDPAD_SCENE)=(seed&1u)?100u:7u;
    WORD_AT(MAPMAKER_RANDPAD_RNG_LO)=next_rand();WORD_AT(MAPMAKER_RANDPAD_RNG_HI)=next_rand();
    WORD_AT(MAPMAKER_RANDPAD_RNG_CALLS)=next_rand()&0xFFFFu;
    for(i=0;i<n;i++) {
        uint32_t p=PADS+i*0x20u;
        WORD_AT(p)=0x5000u+i*3u;                    /* the id the picker returns */
        *(uint8_t *)((char *)memory+p+4u)=(uint8_t)((next_rand()%5u)?(1u|(next_rand()&0x10u)|(next_rand()%3u?0x10u:0u)):(next_rand()&0xFEu));
        set_f(p+0xCu,frand(-200.0f,200.0f));set_f(p+0x10u,frand(-10.0f,10.0f));set_f(p+0x14u,frand(-200.0f,200.0f));
        if(i==n/2 && n>6) WORD_AT(p+0x10u)=0x7FC00000u;   /* a NaN height: the ring tests accept it */
    }
}
static uint32_t ring_bits(void)
{
    uint32_t r=next_rand()&0x9F0F0000u,inner=next_rand()%60u,outer=next_rand()%256u;   /* other bits are ignored */
    r|=inner|outer<<8;
    if(next_rand()%4u) r|=0x20000000u;
    if(next_rand()%2u) r|=0x40000000u;
    return r;
}
static uint32_t slot_after;
static uint32_t query(float x,float y,float z,uint32_t ring)
{
    set_f(QUERY,x);set_f(QUERY+4u,y);set_f(QUERY+8u,z);
    g_esp=STACK-0xCu;
    WORD_AT(g_esp)=0xDEAD0000u;WORD_AT(g_esp+4u)=ring;WORD_AT(g_esp+8u)=QUERY;
    pc_mapmaker_pads_random(NULL);
    CHECK(g_esp==STACK-0xCu+4u);
    slot_after=WORD_AT(STACK-0xCu+4u);
    return g_eax;
}
static unsigned long long mix(unsigned long long h,uint32_t v) { h^=v;return h*1099511628211ull; }
static unsigned long long series_hash(uint32_t seed,uint32_t n,int print)
{
    unsigned long long h=1469598103934665603ull;
    uint32_t k;
    build(seed,n);
    for(k=0;k<200;k++) {
        float x=frand(-220.0f,220.0f),y=frand(-12.0f,12.0f),z=frand(-220.0f,220.0f);
        uint32_t ring=ring_bits(),r;
        if(k%11u==0 && n) {                         /* exactly on a pad */
            uint32_t p=PADS+(next_rand()%n)*0x20u;
            memcpy(&x,(char *)memory+p+0xCu,4);memcpy(&y,(char *)memory+p+0x10u,4);memcpy(&z,(char *)memory+p+0x14u,4);
        }
        r=query(x,y,z,ring);
        if(print) printf("%u %u %u %u %u %u %u %u %u\n",bits(x),bits(y),bits(z),ring,r,slot_after,
            WORD_AT(MAPMAKER_RANDPAD_RNG_LO),WORD_AT(MAPMAKER_RANDPAD_RNG_HI),WORD_AT(MAPMAKER_RANDPAD_RNG_CALLS));
        h=mix(h,r);h=mix(h,slot_after);h=mix(h,WORD_AT(MAPMAKER_RANDPAD_RNG_LO));
        h=mix(h,WORD_AT(MAPMAKER_RANDPAD_RNG_HI));h=mix(h,WORD_AT(MAPMAKER_RANDPAD_RNG_CALLS));
    }
    return h;
}

/* Results of the fixed picker (`golden` prints them); the dump seeds are
 * also compared with the original guest code by test_mapmaker_randpad.py. */
static const unsigned long long golden[9]={0,
    0x42E8291BB977D793ull,0x73D3AE1982A89BC4ull,0xDD9EFBDF61BFDE20ull,0x257A554302CF7A3Full,
    0x57EE3F7C1ED2DD88ull,0x18D2F4A1DD6F5C84ull,0x6CC3F042F7365005ull,0x599E87696FC583FEull};
static const uint32_t counts[9]={0,1,2,3,7,40,41,250,1003};

static void test_kinds(void)
{
    uint32_t i,lo,calls;
    build(2,0);
    lo=WORD_AT(MAPMAKER_RANDPAD_RNG_LO);
    CHECK(query(0,0,0,0x60001405u)==0);            /* no pads: no tries, no draw */
    CHECK(WORD_AT(MAPMAKER_RANDPAD_RNG_LO)==lo);
    build(2,1);
    lo=WORD_AT(MAPMAKER_RANDPAD_RNG_LO);
    CHECK(query(0,0,0,0x60001405u)==0);            /* one pad: 1 x 0.5 is no try */
    CHECK(WORD_AT(MAPMAKER_RANDPAD_RNG_LO)==lo);
    build(2,8);                                     /* scene 7 */
    for(i=0;i<8;i++) { uint32_t p=PADS+i*0x20u; *(uint8_t *)((char *)memory+p+4u)=1u;
      set_f(p+0xCu,(float)i*10.0f);set_f(p+0x10u,0);set_f(p+0x14u,0); }
    WORD_AT(MAPMAKER_RANDPAD_RNG_LO)=0;WORD_AT(MAPMAKER_RANDPAD_RNG_HI)=0;calls=WORD_AT(MAPMAKER_RANDPAD_RNG_CALLS);
    /* first draw 0x269EC3: index (0x269EC3 x 8) >> 32 = 0, pad 0 at distance 0 */
    CHECK(query(0,0,0,0x20000000u)==0x5000u);      /* outer 0: only distance 0 */
    CHECK(slot_after==0x269EC3u && WORD_AT(MAPMAKER_RANDPAD_RNG_CALLS)==calls+1u);
    calls=WORD_AT(MAPMAKER_RANDPAD_RNG_CALLS);
    CHECK(query(0,0,0,0x60000501u)==0);            /* ring 1..5: pad 0 is too near, the others too far */
    CHECK(WORD_AT(MAPMAKER_RANDPAD_RNG_CALLS)==calls+4u);   /* 8 pads: 4 tries */
    WORD_AT(MAPMAKER_RANDPAD_RNG_LO)=0xFFFFFFFFu;WORD_AT(MAPMAKER_RANDPAD_RNG_HI)=0;
    /* a draw in the top half of the range: the index is the top word of r x 8 */
    {
        unsigned long long s=0xFFFFFFFFull*0x343FDull+0x269EC3ull;
        uint32_t r=(uint32_t)s,idx=(uint32_t)(((unsigned long long)r*8u)>>32);
        CHECK(query(0,0,0,0x2000FF00u)==0x5000u+idx*3u);   /* outer 255: every pad */
        CHECK(slot_after==r);
    }
    /* a disabled pad is skipped and the next draw tried */
    for(i=0;i<8;i++) *(uint8_t *)((char *)memory+PADS+i*0x20u+4u)=0x10u;
    *(uint8_t *)((char *)memory+PADS+7u*0x20u+4u)=1u;
    calls=WORD_AT(MAPMAKER_RANDPAD_RNG_CALLS);
    {
        uint32_t got=query(0,0,0,0x2000FF00u),used=WORD_AT(MAPMAKER_RANDPAD_RNG_CALLS)-calls;
        CHECK(used>=1u && used<=4u);
        CHECK(got==0 ? used==4u : got==0x5000u+7u*3u);
    }
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
    puts("mapmaker randpad ok");
    return 0;
}
