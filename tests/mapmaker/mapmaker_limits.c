/* Asset-free contract for src/interface/pc_mapmaker_limits.inc (MapMaker capacity
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
#define TYPEE 0x00330040u
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
    CHECK(alloc_size[0]==8000u*32u && alloc_size[1]==32512u*20u && alloc_size[2]==8000u*44u &&
          alloc_size[3]==10496u*32u && alloc_size[4]==0x280u);
    CHECK(alloc_heap[0]==1 && alloc_heap[4]==1);
    CHECK(alloc_size[1]/20u<=0x7FFFu);                                  /* int16 A* link indices */
    CHECK(WORD_AT(0x00715F2Cu)==0x00500000u && WORD_AT(0x00715F30u)==0x00500000u+8000u*32u);
    CHECK(WORD_AT(WORD_AT(0x00715F2Cu))==0 && WORD_AT(WORD_AT(0x00715F30u)+32512u*20u-4u)==0 &&
          WORD_AT(WORD_AT(0x00715F3Cu)+8000u*44u-4u)==0);           /* zeroed */
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
    WORD_AT(0x00762CB8u)=700;                                          /* > 600 (round 4), < 1024 */
    /* Placed + held memory as the wrapped originals report it: with the limits
     * on and no TSFP_MAPMAKER_MEMORY_SCALE the budget scale is 8. */
    WORD_AT(ED+0x278)=(90u*10000u+1000u)/8u;
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
    /* meter: max(900/7680, 900/9984, (2700+1440)/32256, 700/1024, 90/255, 0, 112500/2048000) */
    CHECK(NEAR(FLOAT_AT(ED+0x270),700.f/1024.f));
    /* held preview: max(2/7680, 9/9984, (20+16)/32256, 1/255, 0, 125/2048000) */
    CHECK(NEAR(FLOAT_AT(ED+0x274),1.f/255.f));

    WORD_AT(0x00762CB8u)=1024;                                         /* groups: 1024 allowed */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(!(WORD_AT(ED+0x25c)&0x400000u));
    WORD_AT(0x00762CB8u)=1025;                                         /* over 1024 */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(WORD_AT(ED+0x25c)&0x400000u);
    WORD_AT(0x00762CB8u)=700;
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
    WORD_AT(ED+0x278)=(90u*10000u+1000u)/8u;
    /* Items do not cost cover pads (measured in play): 400 items with tile cover 909. */
    WORD_AT(ED+0x306c)=400u<<16;
    pc_display_mapmaker_limits(fake_limits);
    CHECK(!(WORD_AT(ED+0x25c)&0x400000u));
    CHECK(NEAR(FLOAT_AT(ED+0x270),400.f/500.f));                      /* meter: items (above groups 700/1024), no cover */
    WORD_AT(ED+0x306c)=0;
    /* The item limit itself, with cost-free tiles: 500 allowed, 501 refused. */
    WORD_AT(0x0078400Cu)=make_list(0x00440000u,TYPEC,10);
    WORD_AT(ED+0x2f7c)=0;
    WORD_AT(ED+0x306c)=500u<<16;
    pc_display_mapmaker_limits(fake_limits);
    CHECK(!(WORD_AT(ED+0x25c)&0x400000u));
    CHECK(NEAR(FLOAT_AT(ED+0x270),1.0f));
    WORD_AT(ED+0x2f34)=12;
    pc_display_mapmaker_limits(fake_limits);
    CHECK(WORD_AT(ED+0x25c)&0x400000u);
    WORD_AT(ED+0x2f34)=(uint32_t)-1;WORD_AT(ED+0x306c)=0;
    WORD_AT(0x0078400Cu)=pieces;
    WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEB,1);
    WORD_AT(0x0078400Cu)=make_list(0x00440000u,TYPEC,255);             /* 255 cost-free placed */
    WORD_AT(ED+0x2f7c)=0;
    pc_display_mapmaker_limits(fake_limits);
    CHECK(!(WORD_AT(ED+0x25c)&0x400000u));
    WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEC,1);                 /* + 1 held: refused */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(WORD_AT(ED+0x25c)&0x400000u);
    /* The heaviest stock tile (type 14/42: 14 pads, 14 cover, 46 links): 255 of
     * them fit every other limit, so only the piece limit refuses the 256th. */
    WORD_AT(TYPEE)=14;WORD_AT(0x00529400u+14u*0x140u)=14;WORD_AT(0x00529404u+14u*0x140u)=14;
    WORD_AT(0x00529408u+14u*0x140u)=46;WORD_AT(0x00529410u+14u*0x140u)=0;
    WORD_AT(0x0078400Cu)=make_list(0x00440000u,TYPEE,255);
    WORD_AT(ED+0x2f7c)=0;
    pc_display_mapmaker_limits(fake_limits);
    CHECK(!(WORD_AT(ED+0x25c)&0x400000u));
    CHECK(NEAR(FLOAT_AT(ED+0x270),1.0f));                             /* pieces 255/255 */
    WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEE,1);
    pc_display_mapmaker_limits(fake_limits);
    CHECK(WORD_AT(ED+0x25c)&0x400000u);
    /* Links themselves: template links + 16 stitch links per piece, placed + held,
     * up to 32,256 (the 32,512-entry pool less 256 for teleporters and doors). */
    WORD_AT(0x0078400Cu)=make_list(0x00440000u,TYPEE,200);            /* 9,200 + 3,200 stitch */
    WORD_AT(TYPEB)=2;                                                  /* type 2 */
    WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEB,16);                /* + 16 x (L + 16) */
    WORD_AT(0x00529408u+2u*0x140u)=1225;                               /* 12,400 + 19,856 = 32,256 */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(!(WORD_AT(ED+0x25c)&0x400000u));
    CHECK(NEAR(FLOAT_AT(ED+0x274),(16.f*1241.f)/32256.f));            /* held preview counts stitches */
    WORD_AT(0x00529408u+2u*0x140u)=1226;                               /* 32,272: refused */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(WORD_AT(ED+0x25c)&0x400000u);
    WORD_AT(0x00529408u+2u*0x140u)=20;
    /* 511 pieces of the heaviest tile (7,154 pads, 7,154 cover, 23,506 + 8,176
     * links): no limit but the piece count (rooms area) refuses them; links
     * stop at 521 of them and pads at 549. */
    {
        MapMakerTotals heavy,none;
        memset(&none,0,sizeof none);
        heavy=none;heavy.pieces=511;heavy.cost[0]=14u*511u;heavy.cost[1]=14u*511u;heavy.cost[2]=46u*511u;
        CHECK((mapmaker_limits_over(&heavy,&none,MAPMAKER_LIMIT_MEMORY,MAPMAKER_LIMIT_GROUPS,MAPMAKER_LIMIT_SUB)&
               ~(unsigned)MAPMAKER_CAP_PIECES)==0);
        heavy.pieces=520;heavy.cost[2]=46u*520u;
        CHECK(!(mapmaker_limits_over(&heavy,&none,0,0,0)&MAPMAKER_CAP_LINKS));
        heavy.pieces=521;heavy.cost[2]=46u*521u;
        CHECK(mapmaker_limits_over(&heavy,&none,0,0,0)&MAPMAKER_CAP_LINKS);
        heavy.pieces=548;heavy.cost[0]=14u*548u;heavy.cost[2]=0;
        CHECK(!(mapmaker_limits_over(&heavy,&none,0,0,0)&MAPMAKER_CAP_PADS));
        heavy.pieces=549;heavy.cost[0]=14u*549u;
        CHECK(mapmaker_limits_over(&heavy,&none,0,0,0)&MAPMAKER_CAP_PADS);
        /* Pad groups at the measured 1.5 per piece: 767 for 511 pieces. */
        CHECK(!(mapmaker_limits_over(&none,&none,0,767,0)&MAPMAKER_CAP_GROUPS));
        /* 1024 light pieces (15 links each) fit the link budget. */
        heavy=none;heavy.pieces=1024;heavy.cost[2]=15u*1024u;
        CHECK(!(mapmaker_limits_over(&heavy,&none,0,0,0)&MAPMAKER_CAP_LINKS));
    }
    WORD_AT(0x0078400Cu)=pieces;
    WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEB,1);

    /* Category maxima: raised while the limits are on, values not at stock kept. */
    WORD_AT(0x00524CD4u)=99;                                           /* red keycard: not stock */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(WORD_AT(0x00524CD4u)==99);
    CHECK(WORD_AT(0x00524CECu)==16 && WORD_AT(0x00524D04u)==16 && WORD_AT(0x00524D1Cu)==16); /* keycards x8 */
    CHECK(WORD_AT(0x00524E3Cu)==160 && WORD_AT(0x00524E54u)==160 && WORD_AT(0x00524EE4u)==160); /* movers/props/markers x8 */
    CHECK(WORD_AT(0x00524CA4u)==128);
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
