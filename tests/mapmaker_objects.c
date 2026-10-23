/* Asset-free contract for src/pc_mapmaker_objects.inc (MapMaker objects
 * area): turret pool guards, object list guards, the raised engine object
 * pools (MapMaker scenes only), teleporter pairing without the 10-slot
 * arrays, and the logic tag guard. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x1000000/4], g_eax, g_ebx, g_ecx, g_edx, g_esi, g_edi, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,b,c,d,si,di,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->b=g_ebx;r->c=g_ecx;r->d=g_edx;r->si=g_esi;r->di=g_edi;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ebx=r->b;g_ecx=r->c;g_edx=r->d;g_esi=r->si;g_edi=r->di;g_esp=r->sp; }

/* Distinct bodies, so the linker cannot fold the stubs into one address. */
static volatile int stub_id;
void sub_0003DFA0(void) { stub_id=1; }
void sub_000B05B0(void) { stub_id=2; }
void sub_000B3BE0(void) { stub_id=3; }
void sub_00248970(void) { stub_id=4; }
void sub_000C3540(void) { stub_id=5; }
void sub_001806B0(void) { stub_id=6; }
void sub_000AEAC0(void) { stub_id=7; }
void sub_00242BA0(void) { stub_id=8; }
void sub_000A5AF0(void) { stub_id=9; }
void sub_0009CC40(void) { stub_id=10; }
void sub_0009A2C0(void) { stub_id=11; }
void sub_0009A5B0(void) { stub_id=12; }
void sub_0009CCA0(void) { stub_id=13; }
void sub_00333700(void) { stub_id=14; }
void sub_002FDF50(void) { stub_id=15; }

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

/* Fakes. */
static uint32_t heap_next, alloc_size[8], alloc_heap[8];
static unsigned allocs;
static char calls[256];
static uint32_t callback_arg=0xFFFFFFFFu;
static uint32_t links[64][2], existing[2];
static unsigned nlinks;
#define COVER_STRUCT 0x00610000u
static void log_call(const char *s) { strncat(calls,s,sizeof calls-strlen(calls)-1); }
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t args[4]={0},p=0;
    GuestRegisters r;
    int i;
    va_start(a,n);
    for(i=0;i<n && i<4;i++) args[i]=va_arg(a,uint32_t);
    va_end(a);
    save_registers(&r);
    if(f==sub_0003DFA0) {
        p=heap_next;heap_next+=(args[0]+15u)&~15u;
        memset((char *)memory+p,0xCD,args[0]);        /* dirty: must be cleared where the original clears */
        if(allocs<8) { alloc_size[allocs]=args[0];alloc_heap[allocs]=args[1]; }
        allocs++;
    }
    else if(f==sub_000B05B0) log_call("B05B0,");
    else if(f==sub_000B3BE0) log_call("B3BE0,");
    else if(f==sub_00248970) log_call("248970,");
    else if(f==sub_000C3540) log_call("C3540,");
    else if(f==sub_001806B0) log_call("1806B0,");
    else if(f==sub_000AEAC0) log_call("AEAC0,");
    else if(f==sub_00242BA0) { log_call("242BA0,");callback_arg=args[0]; }
    else if(f==sub_000A5AF0) log_call("A5AF0,");
    else if(f==sub_0009CC40) p=args[0]==UINT32_MAX?UINT32_MAX:args[0]+0x100u;   /* pad id -> index */
    else if(f==sub_0009A2C0) p=(g_edi==existing[0] && args[0]==existing[1]);
    else if(f==sub_0009A5B0) {
        if(nlinks<64) { links[nlinks][0]=g_ebx;links[nlinks][1]=g_edi; }
        nlinks++;WORD_AT(0x007B6660u)++;
    }
    else if(f==sub_0009CCA0) { p=COVER_STRUCT;WORD_AT(COVER_STRUCT+8)=(args[0]-0x1F40u+700u)&0xFFFFu; }
    else if(f==sub_00333700) p=args[0]&3u;
    else if(f==sub_002FDF50) p=1;
    restore_registers(&r);
    return p;
}

#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_objects.inc"

static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}

static int originals;
static void fake_original(void) { originals++; g_eax=0x1234u; }
/* sub_00245990's two pops (sub_000B3AC0 / sub_000B3AF0). */
static uint32_t popped_gun, popped_owner;
static uint32_t pop(uint32_t list)
{
    uint32_t h=WORD_AT(list+4);
    if(h) { WORD_AT(list+4)=WORD_AT(h);WORD_AT(list+8)--; }
    return h;
}
static void fake_turret_init(void) { originals++;popped_gun=pop(0x00732D2Cu);popped_owner=pop(0x00732D1Cu); }

#define STACK 0x00700000u
static void frame(uint32_t ret,uint32_t a,uint32_t b,uint32_t c)
{
    g_esp=STACK;WORD_AT(STACK)=ret;WORD_AT(STACK+4)=a;WORD_AT(STACK+8)=b;WORD_AT(STACK+12)=c;
}

/* MapMaker container with one piece-count chunk and one piece chunk. */
#define CONTAINER 0x00620000u
static uint32_t item(uint32_t q,int id,uint32_t pad,unsigned npads)
{
    memset((char *)memory+q,0,0x40);
    WORD_AT(q+0xC)=(uint32_t)(uint16_t)id<<16;
    WORD_AT(q+0x14)=npads;
    WORD_AT(q+0x18)=pad;
    return q+(((npads*4u)+0x1Bu)&~3u);
}
static void colour(int id,uint32_t c) { WORD_AT(0x00524F60u+(uint32_t)id*0xC4u)=c; }

int main(void)
{
    uint32_t i,q,piece;
    g_xbox_mem_offset=(uintptr_t)memory;
    heap_next=0x00900000u;
    set_env("TSFP_MAPMAKER_POOLS",NULL);set_env("TSFP_MAPMAKER_LIMITS",NULL);

    /* ---- turret creation: skipped only for sub_0006D7A0 when a pool is empty ---- */
    WORD_AT(0x00732D30u)=0x00500000u;WORD_AT(0x00732D20u)=0x00510000u;
    frame(0x0006E818u,0,0,0);originals=0;
    pc_mapmaker_objects_turret_create(fake_original);
    CHECK(originals==1 && g_eax==0x1234u);
    WORD_AT(0x00732D30u)=0;g_eax=77;
    pc_mapmaker_objects_turret_create(fake_original);
    CHECK(originals==1 && g_eax==0);
    WORD_AT(0x00732D30u)=0x00500000u;WORD_AT(0x00732D20u)=0;
    pc_mapmaker_objects_turret_create(fake_original);
    CHECK(originals==1 && g_eax==0);
    frame(0x0006D4DAu,0,0,0);                         /* sub_0006D230 needs an object */
    pc_mapmaker_objects_turret_create(fake_original);
    CHECK(originals==2);

    /* ---- turret init: spares instead of guest address 0 ---- */
    memset((char *)memory+0x00500000u,0,0x100);
    WORD_AT(0x00732D30u)=0x00500000u;WORD_AT(0x00500000u)=0;WORD_AT(0x00732D34u)=1;
    WORD_AT(0x00732D20u)=0x00510000u;WORD_AT(0x00510000u)=0;WORD_AT(0x00732D24u)=1;
    originals=0;
    pc_mapmaker_objects_turret_init(fake_turret_init);
    CHECK(originals==1 && popped_gun==0x00500000u && popped_owner==0x00510000u && mapmaker_objects_spares_used==0);
    WORD_AT(MAPMAKER_OBJECTS_SPARE_GUNS+0x10)=0xDEADu;
    pc_mapmaker_objects_turret_init(fake_turret_init);  /* both lists empty now */
    CHECK(originals==2 && popped_gun==MAPMAKER_OBJECTS_SPARE_GUNS && popped_owner==MAPMAKER_OBJECTS_SPARE_OWNERS);
    CHECK(WORD_AT(MAPMAKER_OBJECTS_SPARE_GUNS+0x10)==0 && WORD_AT(0x00732D34u)==0 && WORD_AT(0x00732D30u)==0);
    pc_mapmaker_objects_turret_init(fake_turret_init);
    CHECK(popped_gun==MAPMAKER_OBJECTS_SPARE_GUNS+0x1E0u && popped_owner==MAPMAKER_OBJECTS_SPARE_OWNERS+0x130u);
    for(i=0;i<7;i++) pc_mapmaker_objects_turret_init(fake_turret_init);
    CHECK(popped_gun==MAPMAKER_OBJECTS_SPARE_GUNS && popped_gun+0x1E0u*8u<=MAPMAKER_OBJECTS_SPARE_OWNERS);
    CHECK(MAPMAKER_OBJECTS_SPARE_OWNERS+0x130u*8u<=0x008C0000u && MAPMAKER_OBJECTS_SPARE_GUNS>=MAPMAKER_OBJECTS_TAGS+16u*1000u);

    /* ---- object list 9 append guard ---- */
    WORD_AT(0x007B625Cu)=127u;originals=0;frame(0x1000u,0x00520000u,0,0);
    pc_mapmaker_objects_list_add(fake_original);
    CHECK(originals==1);
    WORD_AT(0x007B625Cu)=128u;
    pc_mapmaker_objects_list_add(fake_original);
    CHECK(originals==1);

    /* ---- sub_000A5CE0 list guards ---- */
    {
        static const uint32_t slot[3]={9,8,0},count[3]={0x007B625Cu,0x007B4FA8u,0x007B6258u},
            base[3]={0x008A0000u,0x008A0200u,0x008A0600u},cap[3]={128,200,448};
        unsigned s;
        const uint32_t obj=0x00530000u;
        WORD_AT(0x007DE318u)=0x55u;
        for(s=0;s<3;s++) {
            memset((char *)memory+obj,0,0x200);
            memset((char *)memory+base[s],0,cap[s]*4u);
            WORD_AT(count[s])=cap[s]-1u;originals=0;
            frame(0x1000u,obj,slot[s],1u);
            pc_mapmaker_objects_register(fake_original);
            CHECK(originals==1);                          /* room left */
            WORD_AT(count[s])=cap[s];
            pc_mapmaker_objects_register(fake_original);
            CHECK(originals==1 && WORD_AT(obj+0x90)==0x55u && WORD_AT(obj+0x154+slot[s]*4)==1u);
            CHECK(WORD_AT(obj+0x154+slot[s]*4+4)==0);
            WORD_AT(base[s]+(cap[s]-1u)*4u)=obj;           /* already listed: original (no append) */
            pc_mapmaker_objects_register(fake_original);
            CHECK(originals==2);
            WORD_AT(base[s]+(cap[s]-1u)*4u)=0;
            frame(0x1000u,obj,slot[s],0);                   /* removal */
            pc_mapmaker_objects_register(fake_original);
            CHECK(originals==3);
        }
        WORD_AT(0x007B625Cu)=500u;frame(0x1000u,obj,3u,1u);originals=0;  /* other slots: original */
        pc_mapmaker_objects_register(fake_original);
        CHECK(originals==1);
    }

    /* ---- engine object pool: MapMaker scenes only ---- */
    WORD_AT(0x00715FACu)=0x00242BA0u;
    WORD_AT(0x0079094Cu)=100u;
    WORD_AT(0x007B54E0u)=1;WORD_AT(0x007B5840u+0xD5*4)=1;WORD_AT(0x007B6260u+4)=1;
    WORD_AT(0x00716290u)=0x00540000u;WORD_AT(0x00716294u)=9;WORD_AT(0x0072F5F8u)=9;
    allocs=0;calls[0]=0;originals=0;callback_arg=7;
    pc_mapmaker_objects_pool(fake_original);
    CHECK(originals==0 && allocs==3 && WORD_AT(0x0072F5E4u)==1000u);
    CHECK(alloc_size[0]==((1000u*0x2E8u+15u)&~15u) && alloc_size[1]==0x190u && alloc_size[2]==0xA680u);
    CHECK(alloc_heap[0]==1 && alloc_heap[1]==1 && alloc_heap[2]==1);
    CHECK(WORD_AT(0x0072F5E8u)==0x00900000u && WORD_AT(0x00715FB4u)==WORD_AT(0x0072F5ECu)+0x190u);
    CHECK(WORD_AT(0x007B54E0u)==0 && WORD_AT(0x007B5840u+0xD5*4)==0 && WORD_AT(0x007B6260u+4)==0);
    CHECK(WORD_AT(0x00716290u)==0 && WORD_AT(0x00716294u)==0 && WORD_AT(0x0072F5F8u)==0);
    CHECK(!strcmp(calls,"B05B0,B3BE0,248970,C3540,1806B0,AEAC0,242BA0,A5AF0,") && callback_arg==0);
    WORD_AT(0x0079094Cu)=102u;allocs=0;calls[0]=0;    /* editor too */
    pc_mapmaker_objects_pool(fake_original);
    CHECK(originals==0 && allocs==3);
    WORD_AT(0x00715FACu)=0;allocs=0;calls[0]=0;       /* no callback registered */
    pc_mapmaker_objects_pool(fake_original);
    CHECK(!strcmp(calls,"B05B0,B3BE0,248970,C3540,1806B0,AEAC0,A5AF0,"));
    WORD_AT(0x00715FACu)=0x00111110u;allocs=0;          /* unknown callback: original */
    pc_mapmaker_objects_pool(fake_original);
    CHECK(originals==1 && allocs==0);
    WORD_AT(0x00715FACu)=0x00242BA0u;
    WORD_AT(0x0079094Cu)=53u;                          /* story levels: original */
    pc_mapmaker_objects_pool(fake_original);
    CHECK(originals==2 && allocs==0);
    WORD_AT(0x0079094Cu)=101u;
    pc_mapmaker_objects_pool(fake_original);
    CHECK(originals==3 && allocs==0);
    WORD_AT(0x0079094Cu)=100u;set_env("TSFP_MAPMAKER_POOLS","0");
    pc_mapmaker_objects_pool(fake_original);
    CHECK(originals==4 && allocs==0);
    set_env("TSFP_MAPMAKER_POOLS",NULL);

    /* twin pool */
    originals=0;allocs=0;WORD_AT(0x007B67D0u)=5;WORD_AT(0x007B6984u)=5;WORD_AT(0x006F73B0u)=0;
    pc_mapmaker_objects_twin_pool(fake_original);
    CHECK(originals==0 && allocs==1 && alloc_size[0]==1000u*0x200u && WORD_AT(0x0070AAD8u)==1000u);
    CHECK(WORD_AT(WORD_AT(0x0070AADCu))==0 && WORD_AT(WORD_AT(0x0070AADCu)+1000u*0x200u-4u)==0);
    CHECK(WORD_AT(0x007B67D0u)==0 && WORD_AT(0x007B6984u)==0 && WORD_AT(0x004D9700u)==1);
    WORD_AT(0x006F73B0u)=3;
    pc_mapmaker_objects_twin_pool(fake_original);
    CHECK(WORD_AT(0x004D9700u)==0);
    WORD_AT(0x0079094Cu)=101u;allocs=0;
    pc_mapmaker_objects_twin_pool(fake_original);
    CHECK(originals==1 && allocs==0);
    WORD_AT(0x0079094Cu)=100u;

    /* ---- teleporters: 14 of 3 colours (stock arrays hold 10) ---- */
    for(i=0x0A;i<=0x0E;i++) colour((int)i,i==0x0Bu?0x22u:i==0x0Cu?0x33u:0x11u);
    colour(0x11,0x11u);
    WORD_AT(0x00784014u)=CONTAINER;
    q=CONTAINER+8;
    WORD_AT(q)=0x00C77667u;WORD_AT(q+4)=4;q+=12;                         /* unrelated chunk */
    WORD_AT(q)=0x00C77668u;WORD_AT(q+4)=8;WORD_AT(q+8)=0;WORD_AT(q+12)=2;q+=16; /* 2 pieces */
    WORD_AT(q)=0x00C77669u;piece=q+8;
    /* piece 0: type 5 -> (1+1)*8+0x28 header; 9 items */
    WORD_AT(piece)=5;WORD_AT(piece+0x1C)=9u<<16;
    q=piece+0x38;
    q=item(q,0x0A,1,1);q=item(q,0x0B,2,1);q=item(q,0x11,3,1);  /* a weapon pickup, colour 0x11 too */
    q=item(q,0x0D,4,3);q=item(q,0x0C,5,1);q=item(q,0x0A,0x1F45u,1); /* cover handle -> pad 705 */
    q=item(q,0x0B,7,1);q=item(q,0x0E,8,1);q=item(q,0x0C,9,1);
    /* piece 1: type 4 -> header 0x30; 8 items */
    WORD_AT(q)=4;WORD_AT(q+0x1C)=8u<<16;piece=q;q=piece+0x30;
    q=item(q,0x0A,10,1);q=item(q,0x0A,11,1);q=item(q,0x0B,12,1);q=item(q,0x0D,13,1);
    q=item(q,0x0A,14,1);q=item(q,0x0A,15,1);q=item(q,0x09,16,1);
    q=item(q,0x0C,0x1F40u,1);                                  /* 0x1F40 is not a cover handle */
    WORD_AT(CONTAINER+8+12+16+4)=q-(CONTAINER+8+12+16+8);
    WORD_AT(q)=0x00C7766Au;
    WORD_AT(0x007B6660u)=100;nlinks=0;existing[0]=0x100u+705u;existing[1]=0x100u+8u; /* 705 -> 8 exists */
    originals=0;g_ebx=0xB0B0u;g_edi=0xD0D0u;
    pc_mapmaker_objects_teleporters(fake_original);
    CHECK(originals==0 && g_ebx==0xB0B0u && g_edi==0xD0D0u);
    /* colour 0x11: pads 1,4,705,8,10,11,13,14,15 -> chain (705->8 already exists); 0x22: 2,7,12; 0x33: 5,9,0x1F40 */
    {
        static const uint32_t want[][2]={{1,4},{2,7},{4,705},{5,9},{7,12},{8,10},{9,0x1F40},{10,11},{11,13},{13,14},{14,15}};
        unsigned k;
        CHECK(nlinks==11);
        for(k=0;k<11 && k<nlinks;k++)
            CHECK(links[k][0]==want[k][0]+0x100u && links[k][1]==want[k][1]+0x100u);
        CHECK(WORD_AT(0x007B6660u)==111u);
    }
    WORD_AT(0x007B6660u)=MAPMAKER_LIMIT_LINKS-2u;nlinks=0;            /* link pool nearly full */
    pc_mapmaker_objects_teleporters(fake_original);
    CHECK(nlinks==2 && WORD_AT(0x007B6660u)==MAPMAKER_LIMIT_LINKS);
    set_env("TSFP_MAPMAKER_POOLS","0");WORD_AT(0x007B6660u)=4399u;nlinks=0; /* stock link pool */
    pc_mapmaker_objects_teleporters(fake_original);
    CHECK(nlinks==1);
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    WORD_AT(0x0079094Cu)=53u;nlinks=0;
    pc_mapmaker_objects_teleporters(fake_original);
    CHECK(originals==1 && nlinks==0);
    WORD_AT(0x0079094Cu)=100u;
    WORD_AT(0x00784014u)=0;WORD_AT(0x007B6660u)=0;                     /* no container: nothing */
    pc_mapmaker_objects_teleporters(fake_original);
    CHECK(nlinks==0 && originals==1);

    /* ---- logic tag guard ---- */
    {
        const uint32_t obj=0x00560000u;
        for(i=0;i<1000u;i++) WORD_AT(MAPMAKER_OBJECTS_TAGS+i*16u)=0x00400000u|i;
        WORD_AT(0x00519750u)=40u;originals=0;
        frame(0x1000u,7u,obj,0);
        pc_mapmaker_objects_tag(fake_original);
        CHECK(originals==0 && WORD_AT(obj+0x60)==7u && WORD_AT(obj+0x230)==40u && WORD_AT(0x00519750u)==41u);
        WORD_AT(MAPMAKER_OBJECTS_TAGS+999u*16u)=0;                     /* one free slot */
        pc_mapmaker_objects_tag(fake_original);
        CHECK(originals==1);
        WORD_AT(MAPMAKER_OBJECTS_TAGS+999u*16u)=1;
        frame(0x1000u,0,obj,0);pc_mapmaker_objects_tag(fake_original);  /* no tag */
        frame(0x1000u,0x3F3F3F3Fu,obj,0);pc_mapmaker_objects_tag(fake_original);
        frame(0x1000u,7u,0,0);pc_mapmaker_objects_tag(fake_original);   /* no object */
        CHECK(originals==4);
    }

    if(failures)return 1;
    puts("mapmaker objects ok");
    return 0;
}
