/* Asset-free contract for src/interface/pc_mapmaker_pads.inc: the A* node allocators
 * refuse a node only when the pool is really full (free list empty, counter
 * at capacity), the start-point cover top-up is skipped only in scene 100
 * when it would pass the cover capacity (14,336 with the pools, stock 1280
 * without), pad ids past 8000 resolve everywhere the stock code reads ids,
 * and group distances above 1024 groups are computed on first use from the
 * load-time link state.
 *
 * "matrix" mode (test_mapmaker_pads.py's emulator check): builds a group
 * graph, reads every entry through the lazy reader with the fake route
 * search below and prints them, for comparison with the original builder
 * sub_00275F70 run on the same image with the same fake search. */
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
static double g_fp_stack[8];
static int g_fp_top;
/* distinct bodies: MSVC folds identical functions, and guest_call below dispatches by address */
static volatile int stub_marks;
void sub_0003DFA0(void) { stub_marks|=1; }
void sub_001596F0(void) { stub_marks|=2; }
void sub_0009C550(void) { stub_marks|=4; }
static uint32_t heap_next=0x00650000u, route_calls;
/* The fake route search (the same model as test_mapmaker_pads.py's stub):
 * cost from the two pads and the flags of link 0 (so the snapshot shows),
 * no route when (start + dest) % 7 == 3. */
static void fake_route(uint32_t route)
{
    uint32_t a=WORD_AT(route),b=WORD_AT(route+0xCu),f=WORD_AT(WORD_AT(0x00715F30u));
    route_calls++;
    if((a+b)%7u==3u) { g_eax=(uint32_t)-2; return; }
    FLOAT_AT(0x00715F44u)=(float)((a*7u+b*3u)%97u)*0.25f;
    FLOAT_AT(0x00715F48u)=(float)((f>>16)&0xFFu)+0.125f*(float)(b%5u);
    WORD_AT(0x00715EDCu)=b;WORD_AT(0x0071204Cu)=0x1234u;
    g_eax=1;
}
static uint32_t guest_call(void (*f)(void),int n,...)
{
    uint32_t a[8]={0};
    int i;
    va_list ap;
    va_start(ap,n);
    for(i=0;i<n && i<8;i++) a[i]=va_arg(ap,uint32_t);
    va_end(ap);
    if(f==sub_0003DFA0) { uint32_t p=heap_next;heap_next+=(a[0]+15u)&~15u;return p; }
    if(f==sub_0009C550) {
        uint32_t rec=WORD_AT(WORD_AT(0x007B6770u)+a[0]*4u);
        WORD_AT(a[1])=rec+0x18u;
        return WORD_AT(rec);
    }
    if(f==sub_001596F0) { if(a[0]!=0u) return 0xBADu; fake_route(a[1]);return g_eax; }
    return 0;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_pads.inc"

static int failures, originals;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static void fake_original(void) { originals++; g_esp+=4u; }
static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}
/* Call a hook; report whether the original ran. */
static int ran(void (*hook)(void (*)(void)))
{
    int before=originals;
    uint32_t esp=g_esp;
    hook(fake_original);
    CHECK(g_esp==esp+4u);              /* the original's ret, run or replaced */
    g_esp=esp;
    return originals==before+1;
}

/* ---- pad ids past 8000 ---------------------------------------------------- */
#define REG 0x00400000u
#define COV 0x00480000u
#define RUN 0x00500000u
static void pools(uint32_t regular,uint32_t cover)
{
    uint32_t i;
    WORD_AT(0x00715F2Cu)=REG;WORD_AT(0x00715F34u)=COV;WORD_AT(0x00715F38u)=RUN;
    WORD_AT(0x007B6664u)=regular;WORD_AT(0x007B65C0u)=cover;
    for(i=0;i<regular;i++) { WORD_AT(REG+i*32u)=mapmaker_pads_id(i);WORD_AT(REG+i*32u+4u)=1u;
                             WORD_AT(REG+i*32u+8u)=0xABCD0000u|(i/9u);WORD_AT(REG+i*32u+0x18u)=0x18000000u+i; }
    for(i=0;i<cover;i++) { WORD_AT(COV+i*32u)=0x2329u+i;WORD_AT(COV+i*32u+4u)=0x801u;
                           WORD_AT(COV+i*32u+8u)=0x77770000u|mapmaker_pads_id(i%regular); }
}
static uint32_t call_hook(void (*hook)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t a0,uint32_t a1,uint32_t a2,uint32_t a3)
{
    g_esp=0x00200000u;
    WORD_AT(g_esp)=ret;WORD_AT(g_esp+4)=a0;WORD_AT(g_esp+8)=a1;WORD_AT(g_esp+12)=a2;WORD_AT(g_esp+16)=a3;
    g_eax=0xEEEEEEEEu;
    hook(orig);
    CHECK(g_esp==0x00200004u);
    return g_eax;
}
static uint32_t orig_eax=0x0BADF00Du;
static void fake_original_eax(void) { originals++; g_esp+=4u; g_eax=orig_eax; }
static void test_ids(void)
{
    uint32_t i,before;
    /* the mapping keeps every stock id and stays an int16 */
    CHECK(mapmaker_pads_id(0)==0x3E9u && mapmaker_pads_id(7999)==0x2328u && mapmaker_pads_id(8000)==0x5B29u);
    CHECK(mapmaker_pads_id(16383)==0x7BE8u && mapmaker_pads_id(16383)<0x7FFFu);
    for(i=0;i<16384u;i++) CHECK(mapmaker_pads_index((int32_t)mapmaker_pads_id(i))==(int32_t)i);
    CHECK(mapmaker_pads_index(0x3E8)==-1 && mapmaker_pads_index(0x2329)==-1 && mapmaker_pads_index(0x5B28)==-1 &&
          mapmaker_pads_index(0x7BE9)==-1 && mapmaker_pads_index(-2)==-1);
    pools(16384u,14336u);
    /* sub_0009CC40's search finds every id, misses gaps */
    for(i=0;i<16384u;i+=37u) CHECK(mapmaker_pads_search((int32_t)mapmaker_pads_id(i))==(int32_t)i);
    CHECK(mapmaker_pads_search(0x7BE8)==16383 && mapmaker_pads_search(0x3E9)==0);
    CHECK(mapmaker_pads_search(0x2400)==-1 && mapmaker_pads_search(0x7BE9)==-1 && mapmaker_pads_search(5)==-1);
    WORD_AT(0x007B6664u)=0;CHECK(mapmaker_pads_search(0x3E9)==0);   /* stock: count 0 still tests entry 0 */
    CHECK(mapmaker_pads_search(0x3EA)==-1);
    WORD_AT(0x007B6664u)=1;CHECK(mapmaker_pads_search(0x3E9)==0 && mapmaker_pads_search(0x3EA)==-1);
    WORD_AT(REG-32u)=0x3E8u;WORD_AT(REG-64u)=0x3E9u;WORD_AT(0x007B6664u)=(uint32_t)-2;   /* mid -1: stock stops at once */
    CHECK(mapmaker_pads_search(0x3E9)==-1);
    WORD_AT(0x007B6664u)=16384u;
    /* cover lookup: low ids run the original untouched, high ids answer the regular handle */
    before=originals;
    CHECK(call_hook(pc_mapmaker_pads_cover_lookup,fake_original_eax,0x0009CC53u,0x2329u,0,0,0)==orig_eax && originals==before+1);
    CHECK(call_hook(pc_mapmaker_pads_cover_lookup,fake_original_eax,0x0009CC53u,0x5B28u,0,0,0)==orig_eax);
    CHECK(call_hook(pc_mapmaker_pads_cover_lookup,fake_original_eax,0x0009CC53u,0x7BE9u,0,0,0)==orig_eax);
    before=originals;
    CHECK(call_hook(pc_mapmaker_pads_cover_lookup,fake_original_eax,0x0009CC53u,0x5B29u,0,0,0)==8000u && originals==before);
    CHECK(call_hook(pc_mapmaker_pads_cover_lookup,fake_original_eax,0x0006C3CEu,0x7BE8u,0,0,0)==16383u);
    WORD_AT(0x007B6664u)=9000u;                      /* past the count: not found */
    CHECK(call_hook(pc_mapmaker_pads_cover_lookup,fake_original_eax,0x0009CC53u,0x5B29u+1000u,0,0,0)==(uint32_t)-1);
    WORD_AT(0x007B6664u)=16384u;
    /* pointer: low ids -> original; high -> the regular pad; threshold callers -> owner word = id */
    before=originals;
    CHECK(call_hook(pc_mapmaker_pads_pointer,fake_original_eax,0x00100000u,0x3E9u,0,0,0)==orig_eax && originals==before+1);
    CHECK(call_hook(pc_mapmaker_pads_pointer,fake_original_eax,0x00100000u,0x5000u,0,0,0)==orig_eax);
    before=originals;
    CHECK(call_hook(pc_mapmaker_pads_pointer,fake_original_eax,0x00100000u,0x5B29u+5u,0,0,0)==REG+8005u*32u && originals==before);
    for(i=0;i<13u;i++) {
        uint32_t p=call_hook(pc_mapmaker_pads_pointer,fake_original_eax,mapmaker_pads_owner_rets[i],0x6000u,0,0,0);
        CHECK(p && p!=REG+(0x6000u-0x5B29u+8000u)*32u && (int16_t)(WORD_AT(p+8u)&0xFFFFu)==0x6000);
        CHECK(p<0x00200000u && p+32u<=0x00200000u);  /* below the guest stack */
    }
    /* a threshold caller with a cover id still gets the cover pad (owner = its high owner id) */
    orig_eax=COV+9000u*32u;
    CHECK(call_hook(pc_mapmaker_pads_pointer,fake_original_eax,0x0023F30Du,0x2329u+9000u,0,0,0)==COV+9000u*32u);
    /* sub_0033B100: owner - 0x3E9 must index the pool: high owners give 0x3E9 + index */
    {
        uint32_t p=call_hook(pc_mapmaker_pads_pointer,fake_original_eax,0x0033B179u,0x2329u+9000u,0,0,0);
        CHECK(p!=COV+9000u*32u && (int16_t)(WORD_AT(p+8u)&0xFFFFu)-0x3E9==9000);
        orig_eax=COV+100u*32u;                         /* low owner: the cover pad itself */
        CHECK(call_hook(pc_mapmaker_pads_pointer,fake_original_eax,0x0033B179u,0x2329u+100u,0,0,0)==COV+100u*32u);
        orig_eax=REG+9000u*32u;                        /* a regular pad (no cover flag): untouched */
        CHECK(call_hook(pc_mapmaker_pads_pointer,fake_original_eax,0x0033B179u,0x2000u,0,0,0)==REG+9000u*32u);
        orig_eax=0x0BADF00Du;
    }
    /* not found high id: 0, as the original answers an unknown id */
    WORD_AT(0x007B6664u)=8500u;
    CHECK(call_hook(pc_mapmaker_pads_pointer,fake_original_eax,0x00100000u,0x5B29u+600u,0,0,0)==0u);
    WORD_AT(0x007B6664u)=16384u;
    /* runtime pad: owner with a high id gets the owner's +0x18 */
    WORD_AT(RUN+3u*32u+8u)=0x99990000u|0x5B29u;WORD_AT(RUN+3u*32u+0x18u)=0xDEADu;
    orig_eax=0x7803u;
    CHECK(call_hook(pc_mapmaker_pads_runtime_new,fake_original_eax,0x000C2B90u,0,0,0,0)==0x7803u &&
          WORD_AT(RUN+3u*32u+0x18u)==0x18000000u+8000u);
    WORD_AT(RUN+3u*32u+8u)=0x99990000u|0x2000u;WORD_AT(RUN+3u*32u+0x18u)=0xDEADu;   /* low owner: as the original */
    CHECK(call_hook(pc_mapmaker_pads_runtime_new,fake_original_eax,0x000C2B90u,0,0,0,0)==0x7803u &&
          WORD_AT(RUN+3u*32u+0x18u)==0xDEADu);
    orig_eax=(uint32_t)-1;                             /* no slot */
    CHECK(call_hook(pc_mapmaker_pads_runtime_new,fake_original_eax,0x000C2B90u,0,0,0,0)==(uint32_t)-1);
    orig_eax=0x0BADF00Du;
}
/* sub_0009A7A0's fake: appends `add` regular pads numbered 0x3E9 + index and
 * `add` cover pads owned (0x3E9 + index) by them, as the original does. */
static uint32_t tile_add;
static void fake_tile(void)
{
    uint32_t r=WORD_AT(0x007B6664u),c=WORD_AT(0x007B65C0u),i;
    for(i=0;i<tile_add;i++) WORD_AT(REG+(r+i)*32u)=0x3E9u+r+i;
    for(i=0;i<tile_add && c+i<14336u;i++) {
        WORD_AT(COV+(c+i)*32u)=0x2329u+c+i;
        WORD_AT(COV+(c+i)*32u+8u)=0x55550000u|(0x3E9u+r+i);
    }
    WORD_AT(0x007B6664u)=r+tile_add;WORD_AT(0x007B65C0u)=c+i;
    originals++;g_esp+=4u;
}
static void test_tiles(void)
{
    uint32_t i,ok=1;
    WORD_AT(0x007B6664u)=0;WORD_AT(0x007B65C0u)=0;mapmaker_pads.high_renumbered=0;
    tile_add=14;
    for(i=0;i<1170u;i++) call_hook(pc_mapmaker_pads_tile_load,fake_tile,0x00084FAEu,0,0,0,0);   /* 16,380 pads */
    for(i=0;i<16380u;i++) ok&=WORD_AT(REG+i*32u)==mapmaker_pads_id(i);
    for(i=0;i<14336u;i++) ok&=(WORD_AT(COV+i*32u+8u)>>16)==0x5555u && (WORD_AT(COV+i*32u+8u)&0xFFFFu)==mapmaker_pads_id(i);
    CHECK(ok);
    CHECK(mapmaker_pads.high_renumbered==(16380u-8000u)+(14336u-8000u));
    /* a small map is left exactly as the original numbered it */
    WORD_AT(0x007B6664u)=0;WORD_AT(0x007B65C0u)=0;mapmaker_pads.high_renumbered=0;
    for(i=0;i<500u;i++) call_hook(pc_mapmaker_pads_tile_load,fake_tile,0x00084FAEu,0,0,0,0);    /* 7,000 */
    CHECK(mapmaker_pads.high_renumbered==0u && WORD_AT(REG+6999u*32u)==0x3E9u+6999u);
}
/* The editor: sub_00332490's fake answers 0x3E9 + index; sub_00334B20's fake
 * writes base + local ids from it, as the original does. */
static uint32_t editor_index;
static void fake_editor_id(void) { originals++;g_esp+=4u;g_eax=0x3E9u+editor_index; }
static void fake_item_pads(void)
{
    uint32_t out=WORD_AT(g_esp+16u),k,base;
    g_esp-=4u;pc_mapmaker_pads_editor_id(fake_editor_id);             /* the nested call at 0x00334BE0 */
    base=g_eax;
    for(k=0;k<4u;k++) WORD_AT(out+k*4u)=base+k;
    originals++;g_esp+=4u;g_eax=4u;
}
static uint32_t groups_seen[4];
static void fake_groups(void)
{
    uint32_t k;
    for(k=0;k<2u;k++) { editor_index=7998u+k*2u;g_esp-=4u;pc_mapmaker_pads_editor_id(fake_editor_id);groups_seen[k]=g_eax; }
    editor_index=8000u;
    g_esp-=16u;WORD_AT(g_esp+12u)=0x00340000u;g_esp-=4u;            /* 4 argument dwords, then the call */
    pc_mapmaker_pads_editor_item_pads(fake_item_pads);g_esp+=16u;
    groups_seen[2]=WORD_AT(0x00340000u+12u);
    originals++;g_esp+=4u;g_eax=3u;
}
static void test_editor(void)
{
    editor_index=7999u;CHECK(call_hook(pc_mapmaker_pads_editor_id,fake_editor_id,0x00332DB4u,0x3E9u,0,0,0)==0x2328u);
    editor_index=8000u;CHECK(call_hook(pc_mapmaker_pads_editor_id,fake_editor_id,0x00332DB4u,0x3E9u,0,0,0)==0x5B29u);
    editor_index=16383u;CHECK(call_hook(pc_mapmaker_pads_editor_id,fake_editor_id,0x00332DB4u,0x3E9u,0,0,0)==0x7BE8u);
    /* item pads of a piece across 8000 (7998..8001): mapped one by one */
    editor_index=7998u;
    call_hook(pc_mapmaker_pads_editor_item_pads,fake_item_pads,0x00300E14u,0x00330000u,0,0,0x00340000u);
    CHECK(WORD_AT(0x00340000u)==0x3E9u+7998u && WORD_AT(0x00340004u)==0x2328u &&
          WORD_AT(0x00340008u)==0x5B29u && WORD_AT(0x0034000Cu)==0x5B2Au);
    /* inside the group count everything stays 0x3E9 + index, nested item pads included */
    CHECK(call_hook(pc_mapmaker_pads_editor_groups,fake_groups,0x00301183u,0,0,0,0)==3u);
    CHECK(groups_seen[0]==0x3E9u+7998u && groups_seen[1]==0x3E9u+8000u && groups_seen[2]==0x3E9u+8000u+3u);
    CHECK(mapmaker_pads.linear==0);
    editor_index=8000u;CHECK(call_hook(pc_mapmaker_pads_editor_id,fake_editor_id,0x00332DB4u,0x3E9u,0,0,0)==0x5B29u);
}
/* sub_0023F9D0's fake: what the original reads of the listed seeds. */
static uint32_t topup_eligible,topup_seed_ptr;
static void fake_topup(void)
{
    uint32_t set=WORD_AT(g_esp+4u),n=WORD_AT(g_esp+8u),list=WORD_AT(g_esp+12u),k,base=WORD_AT(set+0x24u);
    topup_eligible=0;topup_seed_ptr=0;
    for(k=0;k<n;k++) {
        int32_t id=(int32_t)WORD_AT(base+WORD_AT(list+k*4u)*32u+4u);
        if(id<0x2329 && id!=-2) {                      /* 0x0023FA20 */
            uint32_t esp=g_esp;
            topup_eligible++;
            g_esp-=8u;WORD_AT(g_esp)=0x0023FA90u;WORD_AT(g_esp+4u)=(uint32_t)id;
            pc_mapmaker_pads_pointer(fake_original_eax);
            g_esp=esp;
            if(!topup_seed_ptr) topup_seed_ptr=g_eax;
        }
    }
    originals++;g_esp+=4u;
}
static void test_topup_alias(void)
{
    uint32_t set=0x00360000u,recs=0x00361000u,list=0x00362000u;
    pools(16384u,14336u);
    WORD_AT(0x0079094Cu)=100;WORD_AT(0x007B65C0u)=100;
    WORD_AT(set+0x24u)=recs;
    WORD_AT(recs+0u*32u+4u)=0x5B29u+10u;WORD_AT(recs+1u*32u+4u)=0x2329u+4u;WORD_AT(recs+2u*32u+4u)=0x400u;
    WORD_AT(list)=0;WORD_AT(list+4)=1;WORD_AT(list+8)=2;
    orig_eax=0x0BADF00Du;
    call_hook(pc_mapmaker_pads_cover_topup,fake_topup,0x0023FBFDu,set,3u,list,0);
    CHECK(topup_eligible==2u);                         /* the high seed and the low regular one, not the cover */
    CHECK(topup_seed_ptr==REG+8010u*32u);              /* the alias resolved to the high pad */
    CHECK(WORD_AT(recs+4u)==0x5B29u+10u && WORD_AT(recs+32u+4u)==0x2329u+4u && WORD_AT(recs+64u+4u)==0x400u);
    CHECK(mapmaker_pads.alias_n==0u);
    /* an alias value outside a top-up is just an id (the original's answer) */
    CHECK(call_hook(pc_mapmaker_pads_pointer,fake_original_eax,0x0023FA90u,0x80000000u,0,0,0)==orig_eax);
}
static void test_stitch_ids(void)
{
    uint32_t table=0x00380000u;
    unsigned char *e=(unsigned char *)memory+table;
    WORD_AT(0x0079094Cu)=100;WORD_AT(0x007B6774u)=table;
    e[0]=0x29;e[1]=0x5B;e[2]=0xE8;e[3]=0x7B;e[4]=7;                  /* 0x5B29, 0x7BE8 */
    e[6]=0x28;e[7]=0x23;e[8]=0xE9;e[9]=0x03;e[10]=9;                 /* 0x2328, 0x03E9 */
    WORD_AT(0x007B677Cu)=2;WORD_AT(0x007B6660u)=0;
    CHECK(ran(pc_mapmaker_pads_stitch));
    CHECK((WORD_AT(table)&0xFFFFu)==0x3E9u+8000u && (WORD_AT(table)>>16)==0x3E9u+16383u && e[4]==7);
    CHECK((WORD_AT(table+6u)&0xFFFFu)==0x2328u && (WORD_AT(table+6u)>>16)==0x3E9u && e[10]==9);
    WORD_AT(0x007B677Cu)=0;
}
/* ---- group distances --------------------------------------------------- */
#define GROUPS 0x00600000u
#define LINKS 0x00510000u
static void graph(uint32_t n)
{
    uint32_t g;
    WORD_AT(0x007B666Cu)=n;WORD_AT(0x007B6770u)=GROUPS;
    for(g=0;g<n;g++) {
        uint32_t rec=GROUPS+0x10000u+g*0x20u;
        WORD_AT(GROUPS+g*4u)=rec;WORD_AT(rec)=1u;WORD_AT(rec+0x18u)=g*3u+1u;   /* first pad of group g */
    }
    WORD_AT(0x00715F30u)=LINKS;WORD_AT(0x007B6660u)=3u;
    WORD_AT(LINKS)=0x00050000u;WORD_AT(LINKS+20u)=0x10000u;WORD_AT(LINKS+40u)=0x10002u;
}
static float model(uint32_t i,uint32_t j,uint32_t link0)
{
    uint32_t a=i*3u+1u,b=j*3u+1u;
    if(i==j) return 0.0f;
    if((a+b)%7u==3u) return FLOAT_AT(MAPMAKER_PADS_NO_ROUTE);
    { volatile float x=(float)((link0>>16)&0xFFu)+0.125f*(float)(b%5u),y=(float)((a*7u+b*3u)%97u)*0.25f; return x+y; }
}
static float read_entry(int32_t i,int32_t j)
{
    int top=g_fp_top;
    g_esp=0x00200000u;WORD_AT(g_esp)=0x002332ACu;WORD_AT(g_esp+4)=(uint32_t)i;WORD_AT(g_esp+8)=(uint32_t)j;
    pc_mapmaker_pads_group_distance(fake_original);
    CHECK(g_esp==0x00200004u && g_fp_top==((top-1)&7));
    return (float)g_fp_stack[g_fp_top];
}
static void test_matrix(void)
{
    uint32_t i,j,calls,ok=1;
    FLOAT_AT(MAPMAKER_PADS_NO_ROUTE)=1.0e9f;FLOAT_AT(MAPMAKER_PADS_NO_ENTRY)=-7.5f;
    WORD_AT(0x0079094Cu)=100;set_env("TSFP_MAPMAKER_POOLS",NULL);set_env("TSFP_MAPMAKER_GROUP_MATRIX",NULL);
    /* up to 1024 groups: the original builds the matrix */
    graph(1024u);
    CHECK(ran(pc_mapmaker_pads_group_matrix) && !mapmaker_matrix.lazy);
    CHECK(ran(pc_mapmaker_pads_group_distance));
    /* 1025: lazy, no matrix pointer */
    graph(1025u);WORD_AT(MAPMAKER_PADS_MATRIX_PTR)=0x12345678u;
    CHECK(!ran(pc_mapmaker_pads_group_matrix) && mapmaker_matrix.lazy && WORD_AT(MAPMAKER_PADS_MATRIX_PTR)==0u);
    /* doors change link 0 after the load: entries still use the load-time flags */
    WORD_AT(LINKS)=0x00090000u;
    route_calls=0;
    for(i=0;i<1025u;i+=97u) for(j=0;j<1025u;j+=89u) {
        float v=read_entry((int32_t)i,(int32_t)j);
        ok&=v==model(i,j,0x00050000u);
    }
    CHECK(ok);
    CHECK(WORD_AT(LINKS)==0x00090000u && WORD_AT(LINKS+20u)==0x10000u);   /* play state restored */
    calls=route_calls;
    CHECK(read_entry(97,89)==model(97,89,0x00050000u) && route_calls==calls);   /* cached */
    CHECK(read_entry(5,5)==0.0f && route_calls==calls);                  /* diagonal: no search */
    CHECK(read_entry(1025,0)==-7.5f && read_entry(0,1025)==-7.5f && read_entry(-1,0)==-7.5f);
    /* globals the search writes are put back */
    FLOAT_AT(0x00715F44u)=3.0f;FLOAT_AT(0x00715F48u)=4.0f;WORD_AT(0x00715EDCu)=77u;WORD_AT(0x0071204Cu)=88u;
    read_entry(3,900);
    CHECK(FLOAT_AT(0x00715F44u)==3.0f && FLOAT_AT(0x00715F48u)==4.0f && WORD_AT(0x00715EDCu)==77u && WORD_AT(0x0071204Cu)==88u);
    /* another level (other link pool or group count): stock reader */
    WORD_AT(0x007B666Cu)=1000u;
    CHECK(ran(pc_mapmaker_pads_group_distance) && !mapmaker_matrix.lazy);
    /* story scene, pools off, or TSFP_MAPMAKER_GROUP_MATRIX=dense: the original */
    graph(2047u);WORD_AT(0x0079094Cu)=53;CHECK(ran(pc_mapmaker_pads_group_matrix) && !mapmaker_matrix.lazy);
    WORD_AT(0x0079094Cu)=100;set_env("TSFP_MAPMAKER_POOLS","0");CHECK(ran(pc_mapmaker_pads_group_matrix));
    set_env("TSFP_MAPMAKER_POOLS",NULL);set_env("TSFP_MAPMAKER_GROUP_MATRIX","dense");
    CHECK(ran(pc_mapmaker_pads_group_matrix) && !mapmaker_matrix.lazy);
    set_env("TSFP_MAPMAKER_GROUP_MATRIX","lazy");graph(40u);
    CHECK(!ran(pc_mapmaker_pads_group_matrix) && mapmaker_matrix.lazy);
    set_env("TSFP_MAPMAKER_GROUP_MATRIX",NULL);
    mapmaker_matrix_free();
}
/* "matrix" mode: n groups, lazy forced, every entry printed as hex. */
static int dump_matrix(uint32_t n,const char *image)
{
    uint32_t i,j;
    FILE *f;
    FLOAT_AT(MAPMAKER_PADS_NO_ROUTE)=1.0e9f;FLOAT_AT(MAPMAKER_PADS_NO_ENTRY)=-7.5f;
    WORD_AT(0x0079094Cu)=100;set_env("TSFP_MAPMAKER_GROUP_MATRIX","lazy");
    graph(n);
    f=fopen(image,"wb");
    if(!f || fwrite(memory,1,sizeof memory,f)!=sizeof memory) return 2;
    fclose(f);
    if(ran(pc_mapmaker_pads_group_matrix) || !mapmaker_matrix.lazy) return 3;
    WORD_AT(LINKS)=0x00090000u;                        /* the doors move: must not show */
    for(i=0;i<n;i++) for(j=0;j<n;j++) { float v=read_entry((int32_t)i,(int32_t)j);uint32_t b;memcpy(&b,&v,4);printf("%08x\n",b); }
    return failures?1:0;
}

int main(int argc,char **argv)
{
    g_xbox_mem_offset=(uintptr_t)memory;
    if(argc==4 && !strcmp(argv[1],"matrix")) return dump_matrix((uint32_t)strtoul(argv[2],0,10),argv[3]);
    if(argc==3 && !strcmp(argv[1],"search")) {          /* sub_0009CC40's search, ids 0..0x7FFF */
        int32_t id;
        pools((uint32_t)strtoul(argv[2],0,10),0u);
        for(id=0;id<0x8000;id++) printf("%d\n",(int)mapmaker_pads_search(id));
        return 0;
    }

    /* ---- group A* pool: free list 0x0070E170, counter 0x00711FF8, 8192 nodes ---- */
    WORD_AT(0x0070E170u)=(uint32_t)-1;
    WORD_AT(0x00711FF8u)=0;
    CHECK(ran(pc_mapmaker_pads_group_node));
    WORD_AT(0x00711FF8u)=1000;                       /* stock capacity: now fine */
    CHECK(ran(pc_mapmaker_pads_group_node));
    WORD_AT(0x00711FF8u)=8191;                       /* last free node */
    CHECK(ran(pc_mapmaker_pads_group_node));
    WORD_AT(0x00711FF8u)=8192;                       /* full */
    CHECK(!ran(pc_mapmaker_pads_group_node) && mapmaker_pads.group_refused==1);
    WORD_AT(0x0070E170u)=17;                         /* full, but a node is on the free list */
    CHECK(ran(pc_mapmaker_pads_group_node));
    WORD_AT(0x0070E170u)=(uint32_t)-1;WORD_AT(0x00711FF8u)=9000;
    CHECK(!ran(pc_mapmaker_pads_group_node));

    /* ---- pad A* pool: free list 0x00712044, counter 0x00715F20, 1000 nodes ---- */
    WORD_AT(0x00712044u)=(uint32_t)-1;
    WORD_AT(0x00715F20u)=999;
    CHECK(ran(pc_mapmaker_pads_pad_node));
    WORD_AT(0x00715F20u)=1000;
    CHECK(!ran(pc_mapmaker_pads_pad_node) && mapmaker_pads.pad_refused==1);
    WORD_AT(0x00712044u)=0;                          /* node 0 free */
    CHECK(ran(pc_mapmaker_pads_pad_node));
    /* the two pools are independent */
    WORD_AT(0x0070E170u)=(uint32_t)-1;WORD_AT(0x00711FF8u)=0;
    WORD_AT(0x00712044u)=(uint32_t)-1;WORD_AT(0x00715F20u)=1000;
    CHECK(ran(pc_mapmaker_pads_group_node) && !ran(pc_mapmaker_pads_pad_node));

    /* ---- start-point cover top-up: sub_0023F9D0(set, n, list) ---- */
    g_esp=0x00200000u;
    WORD_AT(g_esp)=0x0023FBFDu;WORD_AT(g_esp+4)=0x00300000u;WORD_AT(g_esp+8)=1;WORD_AT(g_esp+12)=0x00310000u;
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    WORD_AT(0x0079094Cu)=100;
    WORD_AT(0x007B65C0u)=14336u-15u;                 /* 15 more fit exactly */
    CHECK(ran(pc_mapmaker_pads_cover_topup));
    WORD_AT(0x007B65C0u)=14336u-14u;
    CHECK(!ran(pc_mapmaker_pads_cover_topup) && mapmaker_pads.cover_refused==1);
    WORD_AT(g_esp+8)=2;                              /* n = 2 adds 14: fits */
    CHECK(ran(pc_mapmaker_pads_cover_topup));
    WORD_AT(g_esp+8)=0;                              /* n = 0 adds nothing (original returns) */
    WORD_AT(0x007B65C0u)=14336u;
    CHECK(ran(pc_mapmaker_pads_cover_topup));
    WORD_AT(g_esp+8)=16;                             /* n = 16 adds nothing */
    CHECK(ran(pc_mapmaker_pads_cover_topup));
    WORD_AT(g_esp+8)=1;
    WORD_AT(0x007B65C0u)=1338;                       /* the measured big map: far below */
    CHECK(ran(pc_mapmaker_pads_cover_topup));
    WORD_AT(0x007B65C0u)=20000;                      /* already past: refused, no wrap */
    CHECK(!ran(pc_mapmaker_pads_cover_topup));
    /* pools off: stock 1280 pool */
    set_env("TSFP_MAPMAKER_POOLS","0");
    WORD_AT(0x007B65C0u)=1265;
    CHECK(ran(pc_mapmaker_pads_cover_topup));
    WORD_AT(0x007B65C0u)=1266;
    CHECK(!ran(pc_mapmaker_pads_cover_topup));
    /* other scenes (story levels, editor): never refused */
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    WORD_AT(0x007B65C0u)=20000;
    WORD_AT(0x0079094Cu)=53;
    CHECK(ran(pc_mapmaker_pads_cover_topup));
    WORD_AT(0x0079094Cu)=102;
    CHECK(ran(pc_mapmaker_pads_cover_topup));

    /* ---- stitcher sub_0009DC70: table [0x007B6774] x [0x007B677C], links [0x007B6660] ---- */
    {
        uint32_t i,table=0x00380000u,pads=0x00390000u;
        WORD_AT(0x0079094Cu)=100;
        WORD_AT(0x007B6774u)=table;WORD_AT(0x00715F2Cu)=pads;
        for(i=0;i<8;i++) WORD_AT(pads+i*32u+4u)=i==5u?0u:1u;          /* pad 5 inactive */
        for(i=0;i<6;i++) {                                            /* entries (i, i+1); 4 and 5 touch pad 5 */
            unsigned char *e=(unsigned char *)memory+table+i*6u;
            e[0]=(unsigned char)((0x3E9u+i)&0xFF);e[1]=(unsigned char)((0x3E9u+i)>>8);
            e[2]=(unsigned char)((0x3EAu+i)&0xFF);e[3]=(unsigned char)((0x3EAu+i)>>8);e[4]=0;e[5]=0;
        }
        WORD_AT(0x007B677Cu)=6;WORD_AT(0x007B6660u)=100;              /* far below the pool */
        CHECK(ran(pc_mapmaker_pads_stitch) && WORD_AT(0x007B677Cu)==6u);
        WORD_AT(0x007B6660u)=32512u-6u;                               /* 6 entries fit exactly */
        CHECK(ran(pc_mapmaker_pads_stitch) && WORD_AT(0x007B677Cu)==6u && !mapmaker_pads.stitch_refused);
        WORD_AT(0x007B677Cu)=6;WORD_AT(0x007B6660u)=32512u-4u;       /* 4 append (entries 0..3), 4 and 5 add none */
        CHECK(ran(pc_mapmaker_pads_stitch) && WORD_AT(0x007B677Cu)==6u && !mapmaker_pads.stitch_refused);
        WORD_AT(0x007B677Cu)=6;WORD_AT(0x007B6660u)=32512u-3u;       /* the 4th appending entry is cut */
        CHECK(ran(pc_mapmaker_pads_stitch) && WORD_AT(0x007B677Cu)==3u && mapmaker_pads.stitch_refused==3u);
        WORD_AT(0x007B677Cu)=6;WORD_AT(0x007B6660u)=40000u;          /* already past: nothing */
        CHECK(ran(pc_mapmaker_pads_stitch) && WORD_AT(0x007B677Cu)==0u);
        set_env("TSFP_MAPMAKER_POOLS","0");                          /* stock pools: original as is */
        WORD_AT(0x007B677Cu)=6;
        CHECK(ran(pc_mapmaker_pads_stitch) && WORD_AT(0x007B677Cu)==6u);
        set_env("TSFP_MAPMAKER_POOLS",NULL);
        WORD_AT(0x0079094Cu)=53;                                      /* story levels: original as is */
        CHECK(ran(pc_mapmaker_pads_stitch) && WORD_AT(0x007B677Cu)==6u);
        WORD_AT(0x007B6660u)=0;WORD_AT(0x007B677Cu)=0;WORD_AT(0x0079094Cu)=100;
    }

    test_ids();
    test_tiles();
    test_editor();
    test_topup_alias();
    test_stitch_ids();
    test_matrix();

    /* The editor limit and pool agree with the handle range. */
    CHECK(MAPMAKER_LIMIT_COVER==MAPMAKER_PADS_COVER_RANGE-MAPMAKER_PADS_SPARE);
    CHECK(MAPMAKER_POOL_COVER>=MAPMAKER_PADS_COVER_RANGE);

    if(failures)return 1;
    puts("mapmaker pads ok");
    return 0;
}
