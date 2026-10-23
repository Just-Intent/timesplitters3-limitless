/* Asset-free contract for src/pc_mapmaker_budget.inc: inert by default,
 * divides the wrapped estimate by TSFP_MAPMAKER_MEMORY_SCALE (2..8), rounds
 * up, and changes nothing but the return register. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t g_eax, g_ecx, g_edx, g_esp;
#include "pc_mapmaker_budget.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

static uint32_t estimate;
static void fake_original(void) { g_eax=estimate; g_ecx=0x11u; g_edx=0x22u; }

static void set_scale(const char *value)
{
#ifdef _WIN32
    _putenv_s("TSFP_MAPMAKER_MEMORY_SCALE",value?value:"");
#else
    if(value)setenv("TSFP_MAPMAKER_MEMORY_SCALE",value,1); else unsetenv("TSFP_MAPMAKER_MEMORY_SCALE");
#endif
    memset(&mapmaker_budget,0,sizeof mapmaker_budget);
}
static uint32_t run(uint32_t bytes)
{
    estimate=bytes;g_esp=0x1000u;
    pc_display_mapmaker_memory(fake_original);
    CHECK(g_ecx==0x11u && g_edx==0x22u && g_esp==0x1000u); /* only eax changes */
    return g_eax;
}

int main(void)
{
    set_scale(NULL);
    CHECK(run(2048000u)==2048000u && run(7u)==7u);        /* off by default */
    set_scale("1");
    CHECK(run(2048000u)==2048000u);
    set_scale("2");
    CHECK(run(2048000u)==1024000u);
    CHECK(run(7u)==4u && run(1u)==1u && run(0u)==0u);    /* rounded up: never free */
    set_scale("3");
    CHECK(run(4096000u)==1365334u);
    set_scale("8");
    CHECK(run(0xFFFFFFFFu)==0x20000000u);                 /* no overflow */
    set_scale("9");                                       /* out of range: off */
    CHECK(run(2048000u)==2048000u);
    set_scale("2x");
    CHECK(run(2048000u)==2048000u);
    set_scale("0");
    CHECK(run(2048000u)==2048000u);
    if(failures)return 1;
    puts("mapmaker budget ok");
    return 0;
}
