/* Asset-free contract for src/pc_mapmaker_limits.inc (MapMaker capacity
 * stage A): enlarged AI pools only in scene 100 and on by default; raised
 * editor limits only when opted in and only with the enlarged pools; the
 * placement decision and meter recomputed with the documented limits. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x800000/4], g_eax, g_ecx, g_edx, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }

/* Fake bump allocator for sub_0003DFA0(size, heap). */
void sub_0003DFA0(void) {}
static uint32_t heap_next, alloc_size[8], alloc_heap[8];
static unsigned allocs;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t size,heap,p=0;
    va_start(a,n);
    if(f==sub_0003DFA0 && n==2) {
        size=va_arg(a,uint32_t);heap=va_arg(a,uint32_t);
        p=heap_next;heap_next+=(size+15u)&~15u;
        memset((char *)memory+p,0xCD,size); /* dirty: the wrapper must clear it */
        if(allocs<8) { alloc_size[allocs]=size;alloc_heap[allocs]=heap; }
        allocs++;
    }
    va_end(a);
    return p;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
#define NEAR(a,b) ((a)-(b)<1e-4f && (b)-(a)<1e-4f)

static int originals;
static void fake_pools(void) { originals++; g_esp+=4u; }
static void fake_limits(void)
{
    uint32_t e=WORD_AT(0x007844A8u);
    originals++;
    FLOAT_AT(e+0x270)=0.99f;FLOAT_AT(e+0x274)=0.5f;
    WORD_AT(e+0x25c)|=0x400000u|0x2u; /* stock limits refused; another flag set */
}
static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}

#define ED 0x00320000u
#define TYPEA 0x00330000u
#define TYPEB 0x00330010u
#define TYPEC 0x00330020u
#define TYPED 0x00330030u
/* Piece list of n pieces of one tile type, starting at base. */
static uint32_t make_list(uint32_t base,uint32_t type_cell,unsigned n)
{
    unsigned i;
    for(i=0;i<n;i++) {
        uint32_t p=base+i*0x50u;
        WORD_AT(p+0x2c)=type_cell;WORD_AT(p+0x3c)=0;WORD_AT(p+0x40)=i+1<n?p+0x50u:0u;
    }
    return n?base:0u;
}

/* Stock category maxima (category table 0x00524BE0, max at +4). */
static void stock_categories(void)
{
    static const uint32_t a[][2]={
        {0x00524CA4u,32},{0x00524CBCu,32},{0x00524CD4u,2},{0x00524CECu,2},{0x00524D04u,2},
        {0x00524D1Cu,2},{0x00524D34u,2},{0x00524D4Cu,2},{0x00524D64u,2},{0x00524D7Cu,2},{0x00524D94u,2},{0x00524DACu,16},{0x00524DC4u,16},
        {0x00524DDCu,12},{0x00524ECCu,32},{0x00524E3Cu,20},{0x00524E54u,20},{0x00524EE4u,20},
        {0x00524E6Cu,8},{0x00524E84u,8},{0x00524E9Cu,8},{0x00524F2Cu,4},{0x00524F44u,4},
        {0x00524EFCu,66}};
    unsigned i;
    for(i=0;i<sizeof a/sizeof a[0];i++) WORD_AT(a[i][0])=a[i][1];
}

int main(void)
{
    uint32_t pieces;
    g_xbox_mem_offset=(uintptr_t)memory;

    /* ---- pools ---- */
    heap_next=0x00500000u;
    WORD_AT(0x0079094Cu)=100;
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    WORD_AT(0x007B6664u)=7;WORD_AT(0x007B666Cu)=9;
    { uint32_t s_=g_esp; pc_display_mapmaker_pools(fake_pools); CHECK(g_esp==s_+4u); g_esp=s_; }
    CHECK(originals==0 && allocs==5);
    CHECK(alloc_size[0]==4800u*32u && alloc_size[1]==(8800u+3200u+256u)*20u && alloc_size[2]==4800u*44u &&
          alloc_size[3]==10496u*32u && alloc_size[4]==0x280u);
    CHECK(alloc_heap[0]==1 && alloc_heap[4]==1);
    CHECK(WORD_AT(0x00715F2Cu)==0x00500000u && WORD_AT(0x00715F30u)==0x00500000u+4800u*32u);
    CHECK(WORD_AT(WORD_AT(0x00715F2Cu))==0 && WORD_AT(WORD_AT(0x00715F30u)+12256u*20u-4u)==0 &&
          WORD_AT(WORD_AT(0x00715F3Cu)+4800u*44u-4u)==0);           /* zeroed */
    CHECK(WORD_AT(0x007B6664u)==0 && WORD_AT(0x007B666Cu)==0);
    allocs=0;
    WORD_AT(0x007DE454u)=0x00000A00u;                                  /* byte 0x007DE455 == 10 */
    { uint32_t s_=g_esp; pc_display_mapmaker_pools(fake_pools); CHECK(g_esp==s_+4u); g_esp=s_; }
    CHECK(allocs==4 && alloc_size[2]==10496u*32u);                      /* no AI data pool */
    WORD_AT(0x007DE454u)=0;
    allocs=0;
    WORD_AT(0x0079094Cu)=53;                                           /* other levels: original */
    { uint32_t s_=g_esp; pc_display_mapmaker_pools(fake_pools); CHECK(g_esp==s_+4u); g_esp=s_; }
    CHECK(originals==1 && allocs==0);
    WORD_AT(0x0079094Cu)=100;
    set_env("TSFP_MAPMAKER_POOLS","0");
    { uint32_t s_=g_esp; pc_display_mapmaker_pools(fake_pools); CHECK(g_esp==s_+4u); g_esp=s_; }
    CHECK(originals==2 && allocs==0);

    /* ---- editor limits ---- */
    WORD_AT(0x0079094Cu)=102;
    WORD_AT(0x007844A8u)=ED;
    WORD_AT(TYPEA)=1;WORD_AT(TYPEB)=2;WORD_AT(TYPEC)=3;          /* type 3 costs nothing */
    /* type 1: costs 10/10/30, memory 10000; type 2: 2/9/20, memory 1000 */
    WORD_AT(0x00529400u+1u*0x140u)=10;WORD_AT(0x00529404u+1u*0x140u)=10;WORD_AT(0x00529408u+1u*0x140u)=30;
    WORD_AT(0x00529410u+1u*0x140u)=10000;
    WORD_AT(0x00529400u+2u*0x140u)=2;WORD_AT(0x00529404u+2u*0x140u)=9;WORD_AT(0x00529408u+2u*0x140u)=20;
    WORD_AT(0x00529410u+2u*0x140u)=1000;
    /* 90 placed pieces of type 1 (cover 900), one held of type 2 */
    pieces=make_list(0x00400000u,TYPEA,90);
    WORD_AT(0x0078400Cu)=pieces;
    WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEB,1);
    WORD_AT(ED+0x2f34)=(uint32_t)-1;WORD_AT(ED+0x306c)=0;
    WORD_AT(0x00762CB8u)=400;                                          /* > 300, < 600 */
    WORD_AT(ED+0x278)=90u*10000u+1000u;                               /* placed + held memory */
    WORD_AT(ED+0x25c)=0;

    stock_categories();
    set_env("TSFP_MAPMAKER_POOLS",NULL);set_env("TSFP_MAPMAKER_LIMITS",NULL);
    originals=0;
    pc_display_mapmaker_limits(fake_limits);                          /* off by default */
    CHECK(originals==1 && (WORD_AT(ED+0x25c)&0x400000u) && NEAR(FLOAT_AT(ED+0x270),0.99f));
    CHECK(WORD_AT(0x00524CD4u)==2);                                    /* categories stock */

    set_env("TSFP_MAPMAKER_LIMITS","1");
    WORD_AT(ED+0x25c)=0;
    pc_display_mapmaker_limits(fake_limits);
    CHECK(!(WORD_AT(ED+0x25c)&0x400000u) && (WORD_AT(ED+0x25c)&2u));  /* allowed; other flags kept */
    /* meter: max(900/4800, 900/9984, 2700/8800, 400/600, 90/241, 0, 900000/2048000) */
    CHECK(NEAR(FLOAT_AT(ED+0x270),400.f/600.f));
    /* held preview: max(2/4800, 9/9984, 20/8800, 1/241, 0, 1000/2048000) */
    CHECK(NEAR(FLOAT_AT(ED+0x274),1.f/241.f));

    WORD_AT(0x00762CB8u)=601;                                          /* groups over 600 */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(WORD_AT(ED+0x25c)&0x400000u);
    WORD_AT(0x00762CB8u)=400;
    /* Cover limit 9984: 100 placed pieces of 99 cover (type 4) and held type 2 (9 each). */
    WORD_AT(TYPED)=4;WORD_AT(0x00529404u+4u*0x140u)=99;
    WORD_AT(0x0078400Cu)=make_list(0x00440000u,TYPED,100);
    WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEB,9);                 /* 9900+81 = 9981: allowed */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(!(WORD_AT(ED+0x25c)&0x400000u));
    CHECK(NEAR(FLOAT_AT(ED+0x270),9900.f/9984.f));
    WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEB,10);                /* 9990 > 9984: refused */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(WORD_AT(ED+0x25c)&0x400000u);
    WORD_AT(0x0078400Cu)=pieces;
    WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEB,1);
    WORD_AT(ED+0x278)=2048001u;                                        /* memory over budget */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(WORD_AT(ED+0x25c)&0x400000u);
    WORD_AT(ED+0x278)=90u*10000u+1000u;
    /* Items do not cost cover pads (measured in play): 150 items with tile cover 909. */
    WORD_AT(ED+0x306c)=150u<<16;
    pc_display_mapmaker_limits(fake_limits);
    CHECK(!(WORD_AT(ED+0x25c)&0x400000u));
    CHECK(NEAR(FLOAT_AT(ED+0x270),150.f/200.f));                      /* meter: items, no cover */
    WORD_AT(ED+0x306c)=0;
    /* The item limit itself, with cost-free tiles: 200 allowed, 201 refused. */
    WORD_AT(0x0078400Cu)=make_list(0x00440000u,TYPEC,10);
    WORD_AT(ED+0x2f7c)=0;
    WORD_AT(ED+0x306c)=200u<<16;
    pc_display_mapmaker_limits(fake_limits);
    CHECK(!(WORD_AT(ED+0x25c)&0x400000u));
    CHECK(NEAR(FLOAT_AT(ED+0x270),1.0f));
    WORD_AT(ED+0x2f34)=12;
    pc_display_mapmaker_limits(fake_limits);
    CHECK(WORD_AT(ED+0x25c)&0x400000u);
    WORD_AT(ED+0x2f34)=(uint32_t)-1;WORD_AT(ED+0x306c)=0;
    WORD_AT(0x0078400Cu)=pieces;
    WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEB,1);
    WORD_AT(0x0078400Cu)=make_list(0x00440000u,TYPEC,241);             /* 241 cost-free placed */
    WORD_AT(ED+0x2f7c)=0;
    pc_display_mapmaker_limits(fake_limits);
    CHECK(!(WORD_AT(ED+0x25c)&0x400000u));
    WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEC,1);                 /* + 1 held: refused */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(WORD_AT(ED+0x25c)&0x400000u);
    WORD_AT(0x0078400Cu)=pieces;
    WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEB,1);

    /* Category maxima: raised while the limits are on, values not at stock kept. */
    WORD_AT(0x00524CD4u)=99;                                           /* red keycard: not stock */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(WORD_AT(0x00524CD4u)==99);
    CHECK(WORD_AT(0x00524CECu)==8 && WORD_AT(0x00524D04u)==8 && WORD_AT(0x00524D1Cu)==8);
    CHECK(WORD_AT(0x00524E3Cu)==80 && WORD_AT(0x00524E54u)==80 && WORD_AT(0x00524CA4u)==128);
    CHECK(WORD_AT(0x00524E6Cu)==24 && WORD_AT(0x00524E84u)==24 && WORD_AT(0x00524E9Cu)==24); /* turrets: 72 <= 80 */
    CHECK(WORD_AT(0x00524D34u)==8 && WORD_AT(0x00524D4Cu)==8 && WORD_AT(0x00524D64u)==8 &&
          WORD_AT(0x00524D7Cu)==8 && WORD_AT(0x00524D94u)==8);          /* teleporters x4 */
    CHECK(WORD_AT(0x00524EFCu)==132);                                  /* cover props x2 */

    set_env("TSFP_MAPMAKER_POOLS","0");set_env("TSFP_MAPMAKER_LIMITS","1"); /* limits need the pools */
    stock_categories();
    WORD_AT(ED+0x25c)=0;
    pc_display_mapmaker_limits(fake_limits);
    CHECK(WORD_AT(ED+0x25c)&0x400000u);
    CHECK(WORD_AT(0x00524CECu)==2 && WORD_AT(0x00524E3Cu)==20);       /* untouched when off */
    CHECK(WORD_AT(0x00524E6Cu)==8 && WORD_AT(0x00524D64u)==2 && WORD_AT(0x00524EFCu)==66);

    if(failures)return 1;
    puts("mapmaker limits ok");
    return 0;
}
