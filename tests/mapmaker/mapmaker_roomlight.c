/* Asset-free contract for src/interface/pc_mapmaker_roomlight.inc (sub_00115F50,
 * a room light's colour spread to the room's objects, in host code): synthetic
 * object pools (objects in the room, in other rooms, type 2, in player-held
 * room ids, with and without model instances), players and room records give
 * the expected guest calls and flag bits, and match recorded results. With
 * arguments `dump <seed> <objects> <pre> <post>` it writes the guest memory before and
 * after a series of calls and prints every guest call it made, for
 * tests/mapmaker/test_mapmaker_roomlight.py, which runs the original guest
 * code (0x00115F50..0x0011612B of the XBE) on the pre image in an emulator
 * with the same callee fakes. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMORY_BYTES 0x00920000u
static uint32_t memory[MEMORY_BYTES/4], g_eax, g_esp, g_ebx;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<MEMORY_BYTES && n<=MEMORY_BYTES-p; }
#define MAPMAKER_GUEST_RET() (g_esp+=4u)

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

/* The callee fakes (the emulator test implements the same three). */
static volatile int stub_body[3];
void sub_00094250(void) { stub_body[0]++; }
void sub_00094290(void) { stub_body[1]++; }
void sub_00356800(void) { stub_body[2]++; }
static uint32_t calls[20000][6],calls_n;
static int print_calls;
static uint32_t guest_call(void (*f)(void),unsigned n,...)
{
    va_list ap;
    uint32_t a[5]={0},i,r=0;
    va_start(ap,n);
    for(i=0;i<n && i<5;i++) a[i]=va_arg(ap,uint32_t);
    va_end(ap);
    if(f==sub_00094250) {                         /* (inst, 1.0, r, g, b) */
        uint32_t m=WORD_AT(a[0]+4u);
        if(m) WORD_AT(m+8u)=(WORD_AT(m+8u)+1u)&1u;
        WORD_AT(a[0]+0x10u)=a[2]^a[3]^a[4]^a[1];
    } else if(f==sub_00094290) {                  /* (inst, 0x1D) */
        WORD_AT(a[0]+0x14u)+=a[1];
    } else if(f==sub_00356800) {                  /* (held room - 0x401) -> player */
        r=(a[0]*3u+1u)&3u;
    }
    if(calls_n<20000u) {
        calls[calls_n][0]=f==sub_00094250?1u:f==sub_00094290?2u:3u;
        for(i=0;i<5;i++) calls[calls_n][1+i]=a[i];
        calls_n++;
    }
    if(print_calls) printf("%u %u %u %u %u %u\n",f==sub_00094250?1u:f==sub_00094290?2u:3u,a[0],a[1],a[2],a[3],a[4]);
    return r;
}
#include "pc_mapmaker_roomlight.inc"

#define POOL 0x00100000u
#define PLAYERS 0x00300000u
#define RECORDS 0x00320000u
#define INSTS 0x00360000u
#define STACK 0x00500000u

static uint32_t rng=1;
static uint32_t next_rand(void) { rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng; }

static uint32_t new_inst(uint32_t *at)
{
    uint32_t inst=*at,model=inst+0x40u;
    *at+=0x80u;
    WORD_AT(inst+4u)=model;              /* every instance has a model (the original reads [model+8] unchecked) */
    (void)next_rand();
    WORD_AT(model+8u)=next_rand()&1u;
    return inst;
}
/* a held item: its +4 is a model instance */
static uint32_t new_held(uint32_t *at)
{
    uint32_t held=*at;
    *at+=0x80u;
    WORD_AT(held+4u)=new_inst(at);
    return held;
}
static void build(uint32_t seed,uint32_t objects)
{
    uint32_t i,k,insts=INSTS;
    memset(memory,0,sizeof memory);
    g_xbox_mem_offset=(uintptr_t)memory;
    rng=seed?seed:1u;
    WORD_AT(MAPMAKER_ROOMLIGHT_POOL)=POOL;WORD_AT(MAPMAKER_ROOMLIGHT_COUNT)=objects;
    WORD_AT(MAPMAKER_ROOMLIGHT_PLAYERS)=PLAYERS;WORD_AT(MAPMAKER_ROOMLIGHT_RECORDS)=RECORDS;
    for(i=0;i<objects;i++) {
        uint32_t o=POOL+i*MAPMAKER_ROOMLIGHT_STRIDE,r=next_rand()%12u,room;
        room=r<8u?1u+next_rand()%6u:r<10u?0x401u+next_rand()%4u:r<11u?0x400u+next_rand()%7u:0x40Cu;
        WORD_AT(o+0x34u)=room;
        WORD_AT(o+0x20u)=(next_rand()%4u==0)?2u:next_rand()%5u;
        WORD_AT(o+4u)=(next_rand()%4u)?new_inst(&insts):0u;
    }
    for(k=0;k<4;k++) {
        uint32_t p=PLAYERS+k*MAPMAKER_ROOMLIGHT_PLAYER_STRIDE,body=POOL+(next_rand()%objects)*MAPMAKER_ROOMLIGHT_STRIDE;
        WORD_AT(p+0x14u)=(next_rand()%5u)?body:0u;
        WORD_AT(p+0x1D8u)=(next_rand()%4u)?new_held(&insts):0u;
        WORD_AT(p+0x5B0u)=(next_rand()%4u)?new_held(&insts):0u;
        WORD_AT(p+0xE84u)=(next_rand()%4u)?new_held(&insts):0u;
    }
    for(i=0;i<0x410u;i++) *(uint16_t *)((char *)memory+RECORDS+i*0xB4u+0xB2u)=(uint16_t)(next_rand()&0x1FFu);
}
static void spread(uint32_t room,uint32_t red,uint32_t green,uint32_t blue)
{
    g_esp=STACK-0x10u;
    WORD_AT(g_esp)=0x001163ECu;WORD_AT(g_esp+4u)=room;WORD_AT(g_esp+8u)=red;WORD_AT(g_esp+12u)=green;
    g_ebx=blue;
    pc_mapmaker_rooms_light_spread(NULL);
}
/* a series: each room 1..6 and a held id, with [0x004D1984] on and off */
static void series(void)
{
    uint32_t k;
    for(k=0;k<16;k++) {
        WORD_AT(MAPMAKER_ROOMLIGHT_SELF_ONLY)=k<8u;     /* own room only first, then every type-2 room */
        spread(k%8u<6u?1u+k%8u:0x401u+k%8u,0x10u+k,0x20u+k,0x30u+k);
    }
}

static unsigned long long image_hash(void)
{
    unsigned long long h=1469598103934665603ull;
    uint32_t a,i,j;
    for(a=0x00100000u;a<0x00400000u;a++) { h^=((uint8_t *)memory)[a];h*=1099511628211ull; }
    for(i=0;i<calls_n;i++) for(j=0;j<6;j++) { h^=calls[i][j];h*=1099511628211ull; }
    return h;
}

/* Results of the fixed walk (`golden` prints them); seeds 1..3 are also
 * compared with the original guest code by test_mapmaker_roomlight.py. */
static const unsigned long long golden[9]={0,
    0x4732C9A479781B07ull,0x26944D0DDCFB0608ull,0x825C85F7B12ADF8Cull,0xC27C40CACD761815ull,
    0x085B6BE4AA9BB47Aull,0xDE515181AE07C5EAull,0x89DEDF8D43613C7Eull,0x52DF3FB8DC3E9246ull};

static void test_series(int print)
{
    uint32_t s;
    for(s=1;s<=8;s++) {
        build(s*2654435761u,40u+s*37u);
        calls_n=0;
        series();
        if(print) printf("0x%016llXull,\n",image_hash());
        else if(image_hash()!=golden[s]) { fprintf(stderr,"seed %u: hash 0x%016llX\n",s,image_hash());failures++; }
    }
}

/* hand-built: what each kind of object gets */
static void test_kinds(void)
{
    uint32_t o0=POOL,o1=POOL+0x2E8u,o2=POOL+2u*0x2E8u,o3=POOL+3u*0x2E8u,o4=POOL+4u*0x2E8u,p,insts=INSTS,i0,i3;
    build(1,1);
    memset((char *)memory+POOL,0,0x2E8u);
    WORD_AT(MAPMAKER_ROOMLIGHT_COUNT)=5;
    i0=insts;WORD_AT(o0+4u)=i0;WORD_AT(i0+4u)=i0+0x40u;WORD_AT(i0+0x48u)=1;insts+=0x80u;   /* room 3: coloured, then 0x1D (the fake clears +8) */
    WORD_AT(o0+0x34u)=3;WORD_AT(o0+0x20u)=1;
    WORD_AT(o1+0x34u)=4;WORD_AT(o1+0x20u)=1;WORD_AT(o1+4u)=insts;insts+=0x80u;               /* another room: nothing */
    WORD_AT(o2+0x34u)=5;WORD_AT(o2+0x20u)=2;                                                  /* type 2 elsewhere: flag */
    i3=insts;WORD_AT(o3+0x34u)=0x402u;WORD_AT(o3+0x20u)=0;insts+=0x80u;                        /* held id 0x402: player (1*3+1)&3 = 0 */
    WORD_AT(o4+0x34u)=3;WORD_AT(o4+0x20u)=2;WORD_AT(o4+4u)=insts;                             /* type 2 in the room: flag, no colour */
    p=PLAYERS+0u*MAPMAKER_ROOMLIGHT_PLAYER_STRIDE;
    WORD_AT(p+0x14u)=o0;WORD_AT(p+0x1D8u)=0;WORD_AT(p+0xE84u)=0;WORD_AT(p+0x5B0u)=insts+0x100u;WORD_AT(insts+0x104u)=i3;   /* held item -> its instance */
    WORD_AT(i3+4u)=i3+0x40u;WORD_AT(i3+0x48u)=1;
    *(uint16_t *)((char *)memory+RECORDS+5u*0xB4u+0xB2u)=0x100u;
    *(uint16_t *)((char *)memory+RECORDS+3u*0xB4u+0xB2u)=0x80u;
    calls_n=0;
    WORD_AT(MAPMAKER_ROOMLIGHT_SELF_ONLY)=0;
    spread(3,0x11,0x22,0x33);
    CHECK(calls_n==5);
    CHECK(calls[0][0]==1 && calls[0][1]==i0 && calls[0][2]==0x3F800000u && calls[0][3]==0x11 && calls[0][4]==0x22 && calls[0][5]==0x33);
    CHECK(calls[1][0]==2 && calls[1][1]==i0 && calls[1][2]==0x1D);
    CHECK(calls[2][0]==3 && calls[2][1]==1);                           /* held id 0x402 -> index 1 */
    CHECK(calls[3][0]==1 && calls[3][1]==i3 && calls[4][0]==2 && calls[4][1]==i3);   /* the second held instance */
    CHECK(*(uint16_t *)((char *)memory+RECORDS+5u*0xB4u+0xB2u)==0x180u);
    CHECK(*(uint16_t *)((char *)memory+RECORDS+3u*0xB4u+0xB2u)==0x80u);
    CHECK(g_esp==STACK-0x10u+4u);
    /* [0x004D1984] set: only the room's own type-2 records get the flag */
    *(uint16_t *)((char *)memory+RECORDS+5u*0xB4u+0xB2u)=0x100u;
    *(uint16_t *)((char *)memory+RECORDS+3u*0xB4u+0xB2u)=0x000u;
    WORD_AT(MAPMAKER_ROOMLIGHT_SELF_ONLY)=1;
    spread(3,0x11,0x22,0x33);
    CHECK(*(uint16_t *)((char *)memory+RECORDS+5u*0xB4u+0xB2u)==0x100u);
    CHECK(*(uint16_t *)((char *)memory+RECORDS+3u*0xB4u+0xB2u)==0x080u);
}

int main(int argc,char **argv)
{
#ifdef _WIN32
    _putenv("TSFP_MAPMAKER_ROOM_LIGHT=1");
#endif
    if(argc==6 && !strcmp(argv[1],"dump")) {
        FILE *f;
        build((uint32_t)strtoul(argv[2],NULL,0),(uint32_t)strtoul(argv[3],NULL,0));
        if(!(f=fopen(argv[4],"wb"))) return 2;
        fwrite(memory,1,sizeof memory,f);fclose(f);
        print_calls=1;
        series();
        print_calls=0;
        if(!(f=fopen(argv[5],"wb"))) return 2;
        fwrite(memory,1,sizeof memory,f);fclose(f);
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"golden")) { test_series(1);return 0; }
    test_kinds();
    test_series(0);
    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    puts("mapmaker roomlight ok");
    return 0;
}
