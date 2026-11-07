/* Asset-free contract for src/interface/pc_mapmaker_bigdraw.inc: after the
 * stock draw rectangle (sub_000862C0), a real story boss (character prop,
 * model 0x5E9 or 0x5BE) in MapMaker play (scene 100, TSFP_MAPMAKER_LIMITS=1)
 * gets the whole viewport when one of its rooms is visible; every other
 * prop, scene, caller and switch state keeps the stock rectangle. The fake
 * original pops its return address like the lifted callee; every hook call
 * checks esp. The Python side checks the stderr text. */
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
#include "pc_mapmaker_bigdraw.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stdout,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

#define SCENE 0x0079094Cu
#define VP_PTR 0x007E71C8u
#define VP 0x00610000u
#define PROPS 0x00400000u
#define PROP(i) (PROPS+(uint32_t)(i)*0x2E8u)
#define STACK 0x006F0000u
#define RET 0x000A2470u

/* Stock sub_000862C0, reduced to its result: the union of the visible
 * listed rooms' rectangles (here a fixed portal rectangle, or the empty
 * start values when no listed room is visible); plain ret. */
static unsigned originals;
static int visible=1;
static uint32_t seen_prop;
static void original(void)
{
    uint32_t p=WORD_AT(g_esp+4u);
    originals++;seen_prop=p;
    if(p<sizeof memory-0x2E8u) {
        if(visible) { FLOAT_AT(p+0x118u)=100.0f;FLOAT_AT(p+0x11Cu)=200.0f;FLOAT_AT(p+0x120u)=300.0f;FLOAT_AT(p+0x124u)=260.0f; }
        else { FLOAT_AT(p+0x118u)=632.0f;FLOAT_AT(p+0x11Cu)=464.0f;FLOAT_AT(p+0x120u)=8.0f;FLOAT_AT(p+0x124u)=16.0f; }
    }
    g_esp+=4u;
}
static void call_ret(uint32_t prop,uint32_t ret)
{
    uint32_t sp=g_esp;unsigned before=originals;
    g_esp-=4u;WORD_AT(g_esp)=prop;
    g_esp-=4u;WORD_AT(g_esp)=ret;
    pc_mapmaker_bigdraw_rect(original);
    CHECK(originals==before+1u);   /* the original runs exactly once */
    CHECK(seen_prop==prop);        /* with the caller's argument */
    CHECK(g_esp==sp-4u);           /* it popped its return address; the caller pops the argument */
    g_esp=sp;
}
static void call(uint32_t prop) { call_ret(prop,RET); }
static int stock(uint32_t p) { return FLOAT_AT(p+0x118u)==100.0f && FLOAT_AT(p+0x11Cu)==200.0f && FLOAT_AT(p+0x120u)==300.0f && FLOAT_AT(p+0x124u)==260.0f; }
static int whole(uint32_t p) { return FLOAT_AT(p+0x118u)==8.0f && FLOAT_AT(p+0x11Cu)==16.0f && FLOAT_AT(p+0x120u)==632.0f && FLOAT_AT(p+0x124u)==464.0f; }
static uint32_t prop(unsigned i,uint32_t type,uint16_t model)
{
    uint32_t p=PROP(i);
    WORD_AT(p+0x20u)=type;
    *(uint16_t *)((uintptr_t)(p+0x94u)+g_xbox_mem_offset)=model;
    WORD_AT(p+0x34u)=3u;           /* a real room */
    return p;
}
static void set_limits(const char *value)
{
#ifdef _WIN32
    _putenv_s("TSFP_MAPMAKER_LIMITS",value?value:"");
#else
    if(value)setenv("TSFP_MAPMAKER_LIMITS",value,1); else unsetenv("TSFP_MAPMAKER_LIMITS");
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}
static uint64_t checksum_outside(uint32_t p)
{
    uint64_t h=1469598103934665603ull;size_t i;
    for(i=0;i<sizeof memory/4;i++) {
        if(i>=(p+0x118u)/4u && i<(p+0x128u)/4u)continue;
        if(i>=(STACK-0x1000u)/4u && i<STACK/4u)continue;
        h^=memory[i];h*=1099511628211ull;
    }
    return h;
}

int main(void)
{
    uint32_t skel,trophy,human,object,walker,robot,crowe;uint64_t before;
    g_xbox_mem_offset=(uintptr_t)memory;g_esp=STACK;
    WORD_AT(VP_PTR)=VP;
    WORD_AT(VP)=8u;WORD_AT(VP+4u)=632u;WORD_AT(VP+0x14u)=16u;WORD_AT(VP+0x18u)=464u;
    WORD_AT(VP+8u)=0x11111111u;WORD_AT(VP+0x10u)=0x22222222u;   /* other viewport fields: not a rectangle */
    skel=prop(0,8u,0x5E9u);trophy=prop(1,8u,0x5BEu);human=prop(2,8u,0x0634u);object=prop(3,1u,0x5E9u);walker=prop(4,8u,0x637u);robot=prop(5,8u,0x5F2u);crowe=prop(6,8u,0x5E8u);

    /* Switch off: stock rectangles. */
    set_limits(NULL);WORD_AT(SCENE)=100u;
    call(skel);call(trophy);
    CHECK(stock(skel) && stock(trophy));
    set_limits("0");call(skel);CHECK(stock(skel));
    puts("phase off");fflush(stdout);fflush(stderr);

    /* MapMaker play: both real bosses get the whole view; nothing else changes. */
    set_limits("1");
    before=checksum_outside(skel);
    call(skel);
    CHECK(whole(skel));
    CHECK(checksum_outside(skel)==before);
    call(trophy);CHECK(whole(trophy));
    call(walker);CHECK(whole(walker));             /* the Crow walker */
    call(robot);CHECK(whole(robot));call(crowe);CHECK(whole(crowe));   /* giant robot, split Crowe */
    call(skel);CHECK(whole(skel));                  /* logged once */
    call(human);CHECK(stock(human));                /* a normal character */
    call(object);CHECK(stock(object));              /* the model on a non-character prop */
    {   /* the pit skeleton's satellites, any type: gas bottle, rack, bandage props and the ragbug */
        uint32_t bottle=prop(7,1u,0x24Au),rack=prop(8,1u,0x246u),band=prop(9,1u,0x24Bu),bug=prop(10,8u,0x6CBu),coll=prop(11,1u,0x249u);
        call(bottle);CHECK(whole(bottle));call(rack);CHECK(whole(rack));call(band);CHECK(whole(band));call(bug);CHECK(whole(bug));
        call(coll);CHECK(stock(coll));               /* the invisible collision prop: stock */
    }
    fflush(stderr);puts("phase play");fflush(stdout);

    /* Other scenes: mission 5 (1), the catacombs (17), the editor (102). */
    WORD_AT(SCENE)=1u;call(skel);CHECK(stock(skel));
    WORD_AT(SCENE)=17u;call(skel);CHECK(stock(skel));
    WORD_AT(SCENE)=102u;call(trophy);CHECK(stock(trophy));
    WORD_AT(SCENE)=100u;
    /* Another caller (none in the stock code): untouched. */
    call_ret(skel,0x000A2469u);CHECK(stock(skel));
    puts("phase scenes");fflush(stdout);fflush(stderr);

    /* None of its rooms visible: the stock (empty) rectangle stays, so the stock cull applies. */
    visible=0;
    call(skel);
    CHECK(FLOAT_AT(skel+0x118u)==632.0f && FLOAT_AT(skel+0x120u)==8.0f);
    call(trophy);
    CHECK(FLOAT_AT(trophy+0x118u)==632.0f && FLOAT_AT(trophy+0x120u)==8.0f);
    visible=1;
    fflush(stderr);puts("phase unseen");fflush(stdout);

    /* Bad pointers: no write. */
    call(0x00000010u);
    WORD_AT(VP_PTR)=0u;call(skel);CHECK(stock(skel));
    WORD_AT(VP_PTR)=0x00000100u;call(skel);CHECK(stock(skel));   /* below guest RAM */
    WORD_AT(VP_PTR)=VP;
    g_esp=0x00000100u;   /* a stack the hook may not read: original only */
    {
        unsigned before_n=originals;
        WORD_AT(g_esp-4u)=skel;WORD_AT(g_esp-8u)=RET;g_esp-=8u;
        pc_mapmaker_bigdraw_rect(original);
        CHECK(originals==before_n+1u && g_esp==0x000000FCu && stock(skel));
        g_esp=STACK;
    }
    puts("phase bad");fflush(stdout);fflush(stderr);

    if(failures)return 1;
    puts("mapmaker bigdraw ok");
    return 0;
}
