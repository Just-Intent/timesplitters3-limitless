/* US 45410066 MapMaker level memory budget (opt-in; off unless
 * TSFP_MAPMAKER_MEMORY_SCALE is 2..8).
 *
 * sub_002FA720 refuses placement when the editor's estimate of the level's
 * play-time memory (sub_002FA060 for the placed pieces plus sub_002FA0D0 for
 * the held piece) exceeds 2,048,000 bytes (cmp at 0x002FAEF2), and the meter
 * shows the same estimate times 1/2048000. Measured in play (2026-10-23,
 * research/capacity-constraints.md): a map at that estimate really used
 * about 0.95 MB and left 12.8 MB of the Level heap free; the estimate is
 * about 2.1x conservative.
 *
 * Dividing both results by N raises the budget N times and keeps the meter
 * consistent. The other placement limits (cost 4400, 200 pieces, global 300,
 * sub-entries 50) and the 0x7FE0 map container check are untouched, so the
 * 200-piece cap and the original save format still bound a map. Never
 * writes guest memory; only the return register of the wrapped functions. */
#include <stdlib.h>

static struct { int initialized; unsigned scale; } mapmaker_budget;

static unsigned mapmaker_budget_scale(void)
{
    if(!mapmaker_budget.initialized) {
        const char *v=getenv("TSFP_MAPMAKER_MEMORY_SCALE");
        char *end=NULL;
        unsigned long n=v?strtoul(v,&end,10):1;
        mapmaker_budget.initialized=1;
        mapmaker_budget.scale=(v && end!=v && !*end && n>=2 && n<=8)?(unsigned)n:1u;
    }
    return mapmaker_budget.scale;
}

/* Round up, so a nonzero cost never becomes free. */
static uint32_t mapmaker_budget_apply(uint32_t bytes)
{
    unsigned n=mapmaker_budget_scale();
    return n<=1 ? bytes : (uint32_t)(((uint64_t)bytes+n-1)/n);
}

void pc_display_mapmaker_memory(void (*original)(void))
{
    original();
    g_eax=mapmaker_budget_apply(g_eax);
}
