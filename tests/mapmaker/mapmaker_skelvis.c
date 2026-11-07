/* Asset-free contract for src/interface/pc_mapmaker_skelvis.inc: the giant
 * skeleton draw log (log only). Drives the hooks the way the game reaches
 * them: the room list query (sub_0009FBE0 -> pc_mapmaker_rooms_query), the
 * per-pass rectangle / frustum / draw calls and the pass boundary (portal
 * walk), and checks that guest memory is never written. The Python side
 * compares the exact stderr text. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00800000/4], g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp,b,si,di; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp;r->b=g_ebx;r->si=g_esi;r->di=g_edi; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp;g_ebx=r->b;g_esi=r->si;g_edi=r->di; }
void sub_0003DFA0(void) { g_ecx=1; }
static uint32_t guest_call(void (*f)(void),int n,...) { (void)f;(void)n;return 0; }
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_skelvis.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stdout,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

#define SCENE 0x0079094Cu
#define VIEW_PTR 0x007B0CC0u
#define VIEW 0x00500000u
#define ROOMS 0x007B975Cu
#define NROOMS 20u
#define PLAYER_PTR 0x007B0C7Cu
#define PLAYER 0x00480000u
#define PROPS 0x00400000u
#define PROP(i) (PROPS+(uint32_t)(i)*0x2E8u)
#define CTL 0x00490000u
#define BUFFER 0x00600000u
#define STACK 0x006F0000u

static void set_limits(const char *value)
{
#ifdef _WIN32
    _putenv_s("TSFP_MAPMAKER_LIMITS",value?value:"");
#else
    if(value)setenv("TSFP_MAPMAKER_LIMITS",value,1); else unsetenv("TSFP_MAPMAKER_LIMITS");
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}
static void rect(uint32_t room,int visible)
{
    uint32_t e=VIEW+0x1000u+room*12u;
    WORD_AT(e)=visible?1u:0xFFFFFFFFu;
    *(int16_t *)((uintptr_t)(e+4u)+g_xbox_mem_offset)=visible?10:600;
    *(int16_t *)((uintptr_t)(e+8u)+g_xbox_mem_offset)=visible?200:0;
}
static uint32_t prop(unsigned i,uint32_t type,uint16_t model)
{
    uint32_t p=PROP(i),k;
    WORD_AT(p+0x20u)=type;
    *(uint16_t *)((uintptr_t)(p+0x94u)+g_xbox_mem_offset)=model;
    WORD_AT(p+4u)=CTL+i*0x200u;WORD_AT(CTL+i*0x200u+0xD4u)=0x1A0u+i;
    FLOAT_AT(p+0x64u)=10.0f;FLOAT_AT(p+0x68u)=0.5f;FLOAT_AT(p+0x6Cu)=-4.0f;
    FLOAT_AT(p+0x1A8u)=0.25f;FLOAT_AT(p+0x1ACu)=5.0f;FLOAT_AT(p+0x1B0u)=-0.5f;
    FLOAT_AT(p+0x1C0u)=FLOAT_AT(p+0x1C4u)=FLOAT_AT(p+0x1C8u)=6.5f;
    WORD_AT(p+0x34u)=3u;
    *(int8_t *)((uintptr_t)(p+0x30u)+g_xbox_mem_offset)=4;
    for(k=0;k<4u;k++) WORD_AT(p+0x38u+k*4u)=3u+k;    /* rooms 3..6 */
    return p;
}
/* sub_0009FBE0 -> sub_000873C0 -> the room query: the stack as the hook sees it. */
static void list(uint32_t p,uint32_t touched,uint32_t found,int camera_dropped)
{
    uint32_t sp=g_esp,i;
    g_esp-=0x40u;
    WORD_AT(g_esp)=SKV_RET_QUERY;WORD_AT(g_esp+16u)=SKV_RET_LIST;
    WORD_AT(BUFFER)=touched;
    for(i=0;i<touched;i++)
        *(uint16_t *)((uintptr_t)(BUFFER+2u+2u*i)+g_xbox_mem_offset)=(uint16_t)(((camera_dropped && i==touched-1u)?7u:(20u-i))<<1|1u);
    if(MAPMAKER_PTR_OK(g_esp,20u) && WORD_AT(g_esp)==SKV_RET_QUERY && WORD_AT(g_esp+16u)==SKV_RET_LIST)
        mapmaker_skelvis_list(p+0x38u,10u,touched,found,BUFFER,touched);
    g_esp=sp;
}
static unsigned originals;
static int32_t frustum_result;
static void frustum_original(void) { originals++;g_eax=(uint32_t)frustum_result;g_esp+=4u; }
static void frustum(uint32_t ret,int32_t result)
{
    uint32_t sp=g_esp;unsigned before=originals;
    frustum_result=result;
    g_esp-=4u;WORD_AT(g_esp)=ret;
    pc_mapmaker_skelvis_frustum(frustum_original);
    CHECK(originals==before+1u);
    CHECK(g_esp==sp);
    CHECK(g_eax==(uint32_t)result);
}
/* One room-pass call of sub_000A2430 for the prop: rectangle, frustum. */
static void reach(uint32_t p,int empty,int32_t f)
{
    FLOAT_AT(p+0x118u)=empty?600.0f:10.0f;FLOAT_AT(p+0x11Cu)=empty?400.0f:20.0f;
    FLOAT_AT(p+0x120u)=empty?10.0f:300.0f;FLOAT_AT(p+0x124u)=empty?20.0f:400.0f;
    mapmaker_skelvis_rect(p);
    frustum(SKV_RET_FRUSTUM,f);
}
static void pass(unsigned slot) { WORD_AT(VIEW+0x214u)=slot;mapmaker_skelvis_pass(); }
static uint64_t checksum(void)
{
    uint64_t h=1469598103934665603ull;size_t i;
    for(i=0;i<sizeof memory/4;i++) {
        if(i>=(STACK-0x1000u)/4u && i<STACK/4u)continue;
        h^=memory[i];h*=1099511628211ull;
    }
    return h;
}
/* A hook call that must leave guest memory (outside the stack) as it was. */
#define RO(call) do { uint64_t ro_=checksum();call;CHECK(checksum()==ro_); } while(0)
#define PHASE(s) do { fflush(stderr);puts(s);fflush(stdout); } while(0)

int main(void)
{
    uint32_t skel,human,other;unsigned i;
    g_xbox_mem_offset=(uintptr_t)memory;g_esp=STACK;
    WORD_AT(VIEW_PTR)=VIEW;WORD_AT(ROOMS)=NROOMS;WORD_AT(VIEW+0x2A0u)=7u;
    WORD_AT(PLAYER_PTR)=PLAYER;FLOAT_AT(PLAYER+0x64u)=13.25f;FLOAT_AT(PLAYER+0x68u)=1.5f;FLOAT_AT(PLAYER+0x6Cu)=0.0f;
    for(i=1;i<=NROOMS;i++) rect(i,0);
    skel=prop(0,8u,0x5E9u);human=prop(1,8u,0x634u);other=prop(2,1u,0x5E9u);

    /* Switch off, or another scene: nothing tracked, nothing said. */
    set_limits(NULL);WORD_AT(SCENE)=100u;
    pass(0);list(skel,14u,10u,1);reach(skel,0,1);pass(0);
    set_limits("1");WORD_AT(SCENE)=102u;
    pass(0);list(skel,14u,10u,1);reach(skel,0,1);pass(0);
    CHECK(skv.n==0u);
    PHASE("phase off");

    /* MapMaker play: the list build, once per state. */
    WORD_AT(SCENE)=100u;
    list(human,14u,10u,1);list(other,14u,10u,1);CHECK(skv.n==0u);
    list(skel,14u,10u,1);          /* tracked + truncated, camera room 7 dropped */
    skv.s[0].list_key=-1;RO(mapmaker_skelvis_list(skel+0x38u,10u,14u,10u,BUFFER,14u));  /* again, read-only */
    list(skel,14u,10u,1);          /* same: quiet */
    list(skel,8u,8u,0);            /* fits, camera room not touched */
    WORD_AT(skel+0x38u)=7u;list(skel,8u,8u,0);WORD_AT(skel+0x38u)=3u;   /* camera room in the list */
    list(skel,10u,10u,0);          /* exactly the cap: not truncated */
    CHECK(skv.n==1u && skv.s[0].builds==6u && skv.s[0].truncated==3u && skv.s[0].dropped==3u && skv.s[0].peak==14u);
    PHASE("phase list");

    /* Passes: drawn, frustum culled twice, drawn again. */
    rect(3,1);
    pass(0);                                            /* first main pass (slot 0) */
    reach(skel,0,1);mapmaker_skelvis_drawn(skel);mapmaker_skelvis_drawn(human);pass(0);
    RO(mapmaker_skelvis_rect(skel));frustum(SKV_RET_FRUSTUM,-1);RO(pass(0));
    reach(skel,0,-1);reach(skel,0,-1);pass(0);
    reach(skel,0,0);RO(mapmaker_skelvis_drawn(skel));RO(pass(0)); /* 0 is in front */
    PHASE("phase frustum");

    /* Each other reason. */
    pass(0);                                            /* not reached, room 3 visible */
    rect(3,0);pass(0);                                  /* no listed room visible */
    reach(skel,1,1);pass(0);                            /* empty rectangle */
    reach(skel,0,1);WORD_AT(skel+0x28u)=0x802000u;pass(0);   /* view planes */
    reach(skel,0,1);WORD_AT(skel+0x28u)=0x800000u;pass(0);   /* screen rectangle */
    *(int8_t *)((uintptr_t)(skel+0x30u)+g_xbox_mem_offset)=0;pass(0);   /* no rooms */
    *(int8_t *)((uintptr_t)(skel+0x30u)+g_xbox_mem_offset)=4;WORD_AT(skel+0x28u)=0;
    rect(3,1);reach(skel,0,1);mapmaker_skelvis_drawn(skel);pass(0);
    /* Reached, a frustum call from another caller (not counted), then 0
     * (in front) and a second call with nothing armed: later, frustum 0. */
    mapmaker_skelvis_rect(skel);frustum(0x000A24A8u,-1);pass(0);
    reach(skel,0,1);mapmaker_skelvis_drawn(skel);pass(0);
    mapmaker_skelvis_rect(skel);frustum(SKV_RET_FRUSTUM,0);frustum(SKV_RET_FRUSTUM,-1);pass(0);
    reach(skel,0,1);mapmaker_skelvis_drawn(skel);pass(0);
    PHASE("phase reasons");

    /* Another view (slot 1): not judged; the main view's next pass is. A
     * frustum call from another caller or with no armed skeleton: counted for nobody. */
    frustum(0x000A24A8u,-1);
    reach(skel,0,1);mapmaker_skelvis_drawn(skel);pass(1);
    pass(0);                                            /* slot 1's pass dropped: nothing */
    reach(skel,0,1);mapmaker_skelvis_rect(human);frustum(SKV_RET_FRUSTUM,-1);mapmaker_skelvis_drawn(skel);pass(0);
    CHECK(skv.other==1u && skv.s[0].behind==0u);
    PHASE("phase views");

    /* Summary every 600 main passes, only when a hidden count changed. */
    while(skv.passes%600u!=599u) { reach(skel,0,1);mapmaker_skelvis_drawn(skel);pass(0); }
    reach(skel,0,1);mapmaker_skelvis_drawn(skel);pass(0);
    for(i=0;i<601u;i++) { reach(skel,0,1);mapmaker_skelvis_drawn(skel);pass(0); }   /* 1200: no change, no summary */
    PHASE("phase summary");

    /* Flicker: the line budget holds (60 per level, 12 of them "unseen"). */
    for(i=0;i<40u;i++) { rect(3,0);pass(0);rect(3,1);reach(skel,0,1);mapmaker_skelvis_drawn(skel);pass(0); }
    for(i=0;i<40u;i++) { reach(skel,0,-1);pass(0);reach(skel,0,1);mapmaker_skelvis_drawn(skel);pass(0); }
    CHECK(skv.lines==60u && skv.unseen_lines==12u);
    PHASE("phase budget");

    /* Freed prop: not judged. Leaving play: final summary, forgotten. */
    *(uint16_t *)((uintptr_t)(skel+0x94u)+g_xbox_mem_offset)=0x634u;
    pass(0);pass(0);CHECK(skv.s[0].state==SKV_SHOWN);
    *(uint16_t *)((uintptr_t)(skel+0x94u)+g_xbox_mem_offset)=0x5E9u;
    WORD_AT(SCENE)=102u;pass(0);
    CHECK(skv.n==0u && skv.slot==-1);
    WORD_AT(SCENE)=100u;list(skel,8u,8u,0);mapmaker_skelvis_reset("level restart");
    WORD_AT(ROOMS)=NROOMS;pass(0);WORD_AT(ROOMS)=NROOMS+1u;list(skel,8u,8u,0);pass(0);CHECK(skv.n==0u);
    PHASE("phase reset");

    /* Bad stack for the frustum wrapper: original only. */
    skv.cur=skel;g_esp=0x00000100u;
    {
        unsigned b=originals;
        g_esp-=4u;frustum_result=-1;pc_mapmaker_skelvis_frustum(frustum_original);
        CHECK(originals==b+1u && g_esp==0x00000100u);
    }
    g_esp=STACK;
    PHASE("phase bad");

    if(failures)return 1;
    puts("mapmaker skelvis ok");
    return 0;
}
