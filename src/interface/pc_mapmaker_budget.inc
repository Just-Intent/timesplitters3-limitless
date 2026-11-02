/* US 45410066 MapMaker level memory budget (opt-in; off unless
 * TSFP_MAPMAKER_MEMORY_SCALE is 2..16, or 8 when TSFP_MAPMAKER_LIMITS=1 and no
 * scale is given; plus 1 for every 2 MB TSFP_MAPMAKER_ARENA_MB really added,
 * so 16 with the largest arena).
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
 * consistent. With the raised limits on (TSFP_MAPMAKER_LIMITS=1, pools not
 * off) and no explicit scale, N is 8: measured on a 186-piece, 103-item map
 * full at N = 3 (play min free 8.79 MB), N = 8 projects to about 4 MB free.
 * An explicit valid scale always wins. The other placement limits (cost 4400, 200 pieces, global 300,
 * sub-entries 50) and the 0x7FE0 map container check are untouched, so the
 * 200-piece cap and the original save format still bound a map. Never
 * writes guest memory; only the return register of the wrapped functions. */
#include <stdlib.h>

/* arena_mb: MB the arena switch really added (pc_mapmaker_memory.inc sets it
 * through MAPMAKER_BUDGET_ARENA before the editor runs). */
#define MAPMAKER_BUDGET_ARENA 1
#define MAPMAKER_BUDGET_SCALE_MAX 16u
#define MAPMAKER_BUDGET_SCALE_LIMITS 8u
static struct { int initialized; unsigned scale, arena_mb; } mapmaker_budget;

static unsigned mapmaker_budget_scale(void)
{
    if(!mapmaker_budget.initialized) {
        const char *v=getenv("TSFP_MAPMAKER_MEMORY_SCALE");
        const char *l=getenv("TSFP_MAPMAKER_LIMITS"),*p=getenv("TSFP_MAPMAKER_POOLS");
        char *end=NULL;
        unsigned long n=v?strtoul(v,&end,10):1;
        mapmaker_budget.initialized=1;
        mapmaker_budget.scale=(v && end!=v && !*end && n>=2 && n<=MAPMAKER_BUDGET_SCALE_MAX)?(unsigned)n:1u;
        /* The raised limits come with the largest budget the heap has unless a
         * scale is set: 8 for the stock arena (measured), one more for every
         * 2 MB of real Level heap the arena switch added (the estimate is about
         * 2x the real use, so this keeps that margin). */
        if((!v || !*v) && l && l[0]=='1' && !l[1] && !(p && p[0]=='0' && !p[1])) {
            unsigned s=MAPMAKER_BUDGET_SCALE_LIMITS+mapmaker_budget.arena_mb/2u;
            mapmaker_budget.scale=s<MAPMAKER_BUDGET_SCALE_MAX?s:MAPMAKER_BUDGET_SCALE_MAX;
        }
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
