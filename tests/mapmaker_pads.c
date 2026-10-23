/* Asset-free contract for src/pc_mapmaker_pads.inc: the A* node allocators
 * refuse a node only when the pool is really full (free list empty, counter
 * at capacity), and the start-point cover top-up is skipped only in scene 100
 * when it would pass the cover capacity (10,240 with the pools, stock 1280
 * without). */
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
void sub_0003DFA0(void) {}
static uint32_t guest_call(void (*f)(void),int n,...) { (void)f;(void)n;return 0; }
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

int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;

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
    WORD_AT(0x007B65C0u)=10240u-15u;                 /* 15 more fit exactly */
    CHECK(ran(pc_mapmaker_pads_cover_topup));
    WORD_AT(0x007B65C0u)=10240u-14u;
    CHECK(!ran(pc_mapmaker_pads_cover_topup) && mapmaker_pads.cover_refused==1);
    WORD_AT(g_esp+8)=2;                              /* n = 2 adds 14: fits */
    CHECK(ran(pc_mapmaker_pads_cover_topup));
    WORD_AT(g_esp+8)=0;                              /* n = 0 adds nothing (original returns) */
    WORD_AT(0x007B65C0u)=10240u;
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

    /* The editor limit and pool agree with the handle range. */
    CHECK(MAPMAKER_LIMIT_COVER==MAPMAKER_PADS_COVER_RANGE-MAPMAKER_PADS_SPARE);
    CHECK(MAPMAKER_POOL_COVER>=MAPMAKER_PADS_COVER_RANGE);

    if(failures)return 1;
    puts("mapmaker pads ok");
    return 0;
}
