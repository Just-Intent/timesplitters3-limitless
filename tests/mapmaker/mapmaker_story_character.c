/* Asset-free contract for src/interface/pc_mapmaker_story_character.inc: each wrapper
 * sets the gate byte the four patched Story branches read before running the
 * original: the front-end sub-mode byte when TSFP_MAPMAKER_LIMITS is off
 * (stock Cortez-only Story select), 0 when it is on (the Arcade path). It
 * never writes the sub-mode or any byte next to the gate, and always runs the
 * original exactly once (whose ret pops 4). */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0xA00000/4], g_eax, g_ecx, g_edx, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define BYTE_AT(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }
void sub_0003DFA0(void) {}
static uint32_t guest_call(void (*f)(void),int n,...) { (void)f;(void)n;return 0; }
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_story_character.inc"

static int failures, originals;
static uint8_t gate_seen;                  /* the gate as the original saw it */
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static void fake_original(void) { originals++; gate_seen=BYTE_AT(0x00903500u); g_esp+=4u; }
static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}
static void (*const hooks[4])(void (*)(void))={
    pc_mapmaker_story_char_roster,pc_mapmaker_story_char_preview,
    pc_mapmaker_story_char_confirm,pc_mapmaker_story_char_config};
/* Run a hook with sub-mode `sub` and a stale gate; return the gate the original saw. */
static int gate_for(int hook,uint8_t sub,uint8_t stale)
{
    int before=originals;
    uint32_t esp=g_esp;
    BYTE_AT(0x0078ADAEu)=sub;
    BYTE_AT(0x00903500u)=stale;
    BYTE_AT(0x009034FFu)=0xA5;BYTE_AT(0x00903501u)=0x5A;  /* neighbours: actors' effects end, spare */
    hooks[hook](fake_original);
    CHECK(originals==before+1);
    CHECK(g_esp==esp+4u);
    CHECK(BYTE_AT(0x0078ADAEu)==sub);
    CHECK(BYTE_AT(0x009034FFu)==0xA5 && BYTE_AT(0x00903501u)==0x5A);
    CHECK(BYTE_AT(0x00903500u)==gate_seen);           /* set before the original, not after */
    g_esp=esp;
    return gate_seen;
}

int main(void)
{
    int h;
    g_xbox_mem_offset=(uintptr_t)memory;
    g_esp=0x00200000u;
    WORD_AT(0x0078AD2Cu)=9;                              /* MapMaker */

    /* Off (unset, "0", other values): the gate mirrors the sub-mode. */
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    for(h=0;h<4;h++) {
        CHECK(gate_for(h,2,0)==2);                       /* Story: stock Cortez */
        CHECK(gate_for(h,0,2)==0);                       /* MapMaker Arcade-type: stale Story cleared */
        CHECK(gate_for(h,0x13,0)==0x13);
    }
    set_env("TSFP_MAPMAKER_LIMITS","0");
    CHECK(gate_for(0,2,0)==2);
    set_env("TSFP_MAPMAKER_LIMITS","10");
    CHECK(gate_for(3,2,0)==2);
    /* Limits need the raised pools: pools off means stock here too. */
    set_env("TSFP_MAPMAKER_LIMITS","1");
    set_env("TSFP_MAPMAKER_POOLS","0");
    CHECK(gate_for(1,2,0)==2);

    /* On: the gate is 0 (never the Story value 2) for every sub-mode. */
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    for(h=0;h<4;h++) {
        CHECK(gate_for(h,2,2)==0);
        CHECK(gate_for(h,0,2)==0);
        CHECK(gate_for(h,0x13,2)==0);
    }

    if(failures)return 1;
    puts("mapmaker story character ok");
    return 0;
}
