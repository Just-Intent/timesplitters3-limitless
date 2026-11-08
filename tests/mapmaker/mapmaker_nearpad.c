/* Asset-free contract for src/interface/pc_mapmaker_nearpad.inc (sub_0009BCF0,
 * the nearest enabled pad, in host code): random pad pools (disabled pads,
 * duplicate positions for ties, pad counts around the unrolled groups of
 * four) and queries (with and without a minimum distance) match recorded
 * results and hand-checked cases. With arguments `dump <seed> <pads> <pre>`
 * it writes the guest memory and prints each query and result for
 * tests/mapmaker/test_mapmaker_nearpad.py, which runs the original guest
 * code (0x0009BCF0..0x0009BEE9 of the XBE) in an emulator. */
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
#include "pc_mapmaker_nearpad.inc"

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
    WORD_AT(MAPMAKER_NEAR_PADS)=PADS;WORD_AT(MAPMAKER_NEAR_COUNT)=n;
    WORD_AT(MAPMAKER_NEAR_K_FAR)=0x7F7FFFFFu;WORD_AT(MAPMAKER_NEAR_K_PAD)=0x3C23D70Au;
    for(i=0;i<n;i++) {
        uint32_t p=PADS+i*0x20u;
        *(uint8_t *)((char *)memory+p+4u)=(uint8_t)((next_rand()%7u)?(1u|(next_rand()&0x10u)):(next_rand()&0xFEu));
        if(i>4 && next_rand()%9u==0) {             /* the same position as an earlier pad: a tie */
            uint32_t q=PADS+(next_rand()%i)*0x20u;
            memcpy((char *)memory+p+0xCu,(char *)memory+q+0xCu,12u);
        } else {
            set_f(p+0xCu,frand(-200.0f,200.0f));set_f(p+0x10u,frand(-10.0f,10.0f));set_f(p+0x14u,frand(-200.0f,200.0f));
        }
    }
}
static int32_t query(float x,float y,float z,float min)
{
    set_f(QUERY,x);set_f(QUERY+4u,y);set_f(QUERY+8u,z);
    g_esp=STACK-0xCu;
    WORD_AT(g_esp)=0xDEAD0000u;WORD_AT(g_esp+4u)=QUERY;WORD_AT(g_esp+8u)=bits(min);
    pc_mapmaker_pads_nearest(NULL);
    CHECK(g_esp==STACK-0xCu+4u);
    return (int32_t)g_eax;
}
static unsigned long long series_hash(uint32_t seed,uint32_t n,int print)
{
    unsigned long long h=1469598103934665603ull;
    uint32_t k;
    build(seed,n);
    for(k=0;k<300;k++) {
        float x=frand(-220.0f,220.0f),y=frand(-12.0f,12.0f),z=frand(-220.0f,220.0f);
        float min=(k%3u==0)?0.0f:(k%3u==1)?frand(0.0f,60.0f):-frand(0.0f,5.0f);
        int32_t r;
        if(k%17u==0 && n) {                         /* exactly on a pad */
            uint32_t p=PADS+(next_rand()%n)*0x20u;
            memcpy(&x,(char *)memory+p+0xCu,4);memcpy(&y,(char *)memory+p+0x10u,4);memcpy(&z,(char *)memory+p+0x14u,4);
        }
        r=query(x,y,z,min);
        if(print) printf("%u %u %u %u %d\n",bits(x),bits(y),bits(z),bits(min),r);
        h^=(uint32_t)r;h*=1099511628211ull;
    }
    return h;
}

/* Results of the fixed walk (`golden` prints them); the dump seeds are
 * also compared with the original guest code by test_mapmaker_nearpad.py. */
static const unsigned long long golden[9]={0,
    0x188C2EEA29AB6FF1ull,0x62D1209DEBFB4420ull,0xF1C35CFDAEF39FE8ull,0x88F3B523170C94A1ull,
    0xACBFF456C4E54583ull,0xBAB4CA8AD9DDABDDull,0x9EF79C87AADA9AD5ull,0x6515EC9F22F63FB8ull};
static const uint32_t counts[9]={0,1,3,4,5,7,8,250,1003};

static void test_kinds(void)
{
    build(5,0);
    CHECK(query(0,0,0,0)==-1);                     /* no pads */
    build(5,6);
    { uint32_t i; for(i=0;i<6;i++) { uint32_t p=PADS+i*0x20u; *(uint8_t *)((char *)memory+p+4u)=1u;
      set_f(p+0xCu,(float)i*10.0f);set_f(p+0x10u,0);set_f(p+0x14u,0); } }
    CHECK(query(21,0,0,0)==2);
    CHECK(query(21,0,0,5)==3);                     /* 2 is 1 away: below the minimum 5; 3 (9) before 1 (11) */
    *(uint8_t *)((char *)memory+PADS+2u*0x20u+4u)=0x10u;   /* disabled */
    CHECK(query(21,0,0,0)==3);
    set_f(PADS+4u*0x20u+0xCu,30.0f);               /* pads 3 and 4 at the same place: the lower index */
    CHECK(query(31,0,0,0)==3);
    /* the bound is min^2 + 0.01: a pad 5.0005 away is still too near for min 5 */
    set_f(PADS+0u*0x20u+0xCu,0.0f);set_f(PADS+1u*0x20u+0xCu,-40.0f);
    CHECK(query(5.0005f,0,0,5)==3);
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
    puts("mapmaker nearpad ok");
    return 0;
}
