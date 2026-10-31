/* US 45410066 MapMaker memory headroom (round 3, step A; docs/mapmaker/mapmaker-memory.md).
 *
 * Heap guard (always on, read-only: nothing about an allocation changes).
 * sub_0003DFA0(size, heap) and sub_0003E0B0(size, align, heap) are the game's
 * bump allocators over the arena that sub_0003DF00 lays out:
 *   heap 2  Eternal, bottom-up [0x006B8370], free [0x006B8378]
 *   heap 1  Level, bottom-up [0x006B837C]   } share the end [0x006B8380]
 *   heap 0  top-down [0x006B8388]           } and the free count [0x006B8384]
 *   heaps 3/4 have no free count.
 * When the request is larger than the free count the retail code calls the
 * empty assert sub_000389B0 twice (0x0003DFCC..0x0003DFD5, 0x0003E03A..
 * 0x0003E043) and allocates anyway, so Level and heap 0 silently overlap and
 * the free count goes negative (research/capacity-constraints.md 1b). The
 * guard reads the free count after the original and logs the first time it
 * turns negative (and up to MAPMAKER_MEMORY_LOG_LINES such events per run)
 * with the size, heap, caller and scene, and counts every allocation made
 * while the heap is overrun:
 *   [mapmaker] heap full: heap 1 size 123456 caller 0x0009A1B2 scene 100 free 1000 -> -122456 ...
 * Not per frame in the paths measured so far (level setup); the hook reads
 * only fixed image addresses and the guest stack (MAPMAKER_STACK_OK).
 *
 * Arena switch (off by default): TSFP_MAPMAKER_ARENA_MB=1..16 adds that many
 * MB to the arena size dword [0x004C0450] (stock 0x02CEC000 = 47,104,000 B)
 * before sub_00023930 reads it. Its two readers are 0x00023CDA (the arena's
 * MmAllocateContiguousMemoryEx(size, 0, -1, 0x1000, 4) at 0x00023CE9, result
 * [0x006B7A24]) and 0x0003C34E (sub_0003DF00(base, size), which stores
 * base/size in [0x007E72D4]/[0x007E72D8] and puts the Eternal heap at the
 * bottom and the top-down heap at base + size), so the extra memory becomes
 * free space shared by the Level heap and heap 0 in every scene. The arena
 * comes from the 64 MB contiguous window that also holds textures, vertex and
 * push buffers and the MapMaker container, so the hook first takes and
 * releases a block of the raised size; when the window cannot give it the
 * stock size is kept. Either way one line says what happened. Measure first:
 * TSFP_MAPMAKER_PROBE=32 prints contig_free_min, the window headroom at its
 * peak; a raise must stay well below it (docs/mapmaker/mapmaker-memory.md).
 * 0 or unset: stock; any other value is ignored with a warning.
 *
 * Requires the includer's WORD_AT, g_esp/g_eax and xbox_ContiguousAlloc /
 * xbox_ContiguousFree (runtime). */
#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAPMAKER_MEMORY_SHARED_FREE 0x006B8384u
#define MAPMAKER_MEMORY_ETERNAL_FREE 0x006B8378u
#define MAPMAKER_MEMORY_SCENE 0x0079094Cu
#define MAPMAKER_MEMORY_LOG_LINES 8u
#define MAPMAKER_ARENA_SIZE 0x004C0450u
#define MAPMAKER_ARENA_STOCK 0x02CEC000u
#define MAPMAKER_ARENA_BASE 0x006B7A24u
#define MAPMAKER_ARENA_MAX_MB 16u

extern uint32_t xbox_ContiguousAlloc(uint32_t size,uint32_t alignment);
extern int xbox_ContiguousFree(uint32_t address);

static struct {
    int over[2];                       /* pool 0 (heaps 0/1), pool 1 (heap 2) */
    unsigned lines;
    unsigned long long overrun_allocs;
    int32_t min_free[2];               /* lowest negative free count, else 0 */
} mapmaker_memory;

/* Free count a heap draws from: 0 shared (heaps 0/1), 1 Eternal, -1 none. */
static int mapmaker_memory_pool(uint32_t heap)
{
    return heap<=1u?0:heap==2u?1:-1;
}
static void mapmaker_memory_check(uint32_t caller,uint32_t size,uint32_t heap,int32_t before)
{
    int pool=mapmaker_memory_pool(heap);
    int32_t after;
    if(pool<0) return;
    after=(int32_t)WORD_AT(pool?MAPMAKER_MEMORY_ETERNAL_FREE:MAPMAKER_MEMORY_SHARED_FREE);
    if(after<mapmaker_memory.min_free[pool]) mapmaker_memory.min_free[pool]=after;
    if(after>=0) { mapmaker_memory.over[pool]=0;return; }
    mapmaker_memory.overrun_allocs++;
    if(mapmaker_memory.over[pool]) return;              /* still overrun: counted */
    mapmaker_memory.over[pool]=1;
    if(mapmaker_memory.lines<MAPMAKER_MEMORY_LOG_LINES) {
        mapmaker_memory.lines++;
        fprintf(stderr,"[mapmaker] heap full: heap %u size %u caller 0x%08X scene %d free %d -> %d"
            " (%s; %llu allocations while overrun; minimum %d)\n",
            heap,size,caller,(int)(int32_t)WORD_AT(MAPMAKER_MEMORY_SCENE),(int)before,(int)after,
            pool?"Eternal heap past its end":"Level heap and heap 0 overlap",mapmaker_memory.overrun_allocs,
            (int)mapmaker_memory.min_free[pool]);
        fflush(stderr);
    }
}
static void mapmaker_memory_guard(void (*original)(void),unsigned heap_arg)
{
    uint32_t caller=0,size=0,heap=0xFFFFFFFFu;
    int32_t before=0;
    int pool;
    if(MAPMAKER_STACK_OK(4u+4u*(heap_arg+1u))) {
        caller=WORD_AT(g_esp);size=WORD_AT(g_esp+4u);heap=WORD_AT(g_esp+4u+4u*heap_arg);
    }
    pool=mapmaker_memory_pool(heap);
    if(pool>=0) before=(int32_t)WORD_AT(pool?MAPMAKER_MEMORY_ETERNAL_FREE:MAPMAKER_MEMORY_SHARED_FREE);
    original();
    mapmaker_memory_check(caller,size,heap,before);
}
/* sub_0003DFA0(size, heap), cdecl, plain ret: the original pops. */
void pc_mapmaker_memory_heap_alloc(void (*original)(void))
{
    mapmaker_memory_guard(original,1u);
}
/* sub_0003E0B0(size, align, heap), cdecl, plain ret. */
void pc_mapmaker_memory_heap_alloc_aligned(void (*original)(void))
{
    mapmaker_memory_guard(original,2u);
}

/* ---- arena ------------------------------------------------------------ */
static unsigned mapmaker_memory_arena_mb(void)
{
    const char *t=getenv("TSFP_MAPMAKER_ARENA_MB");
    char *end=NULL;
    unsigned long v;
    if(!t || !*t) return 0;
    v=strtoul(t,&end,10);
    if(end==t || *end || v>MAPMAKER_ARENA_MAX_MB) {
        fprintf(stderr,"[mapmaker] TSFP_MAPMAKER_ARENA_MB=%s ignored (0..%u); stock arena\n",t,MAPMAKER_ARENA_MAX_MB);
        fflush(stderr);
        return 0;
    }
    return (unsigned)v;
}
/* sub_00023930 (game start, called once from 0x0003C330). */
void pc_mapmaker_memory_arena(void (*original)(void))
{
    unsigned mb=mapmaker_memory_arena_mb();
    uint32_t raised=0;
    if(mb) {
        uint32_t stock=WORD_AT(MAPMAKER_ARENA_SIZE),size=MAPMAKER_ARENA_STOCK+((uint32_t)mb<<20),trial;
        if(stock!=MAPMAKER_ARENA_STOCK) {
            fprintf(stderr,"[mapmaker] arena: size dword is 0x%08X, not the stock 0x%08X; unchanged\n",stock,MAPMAKER_ARENA_STOCK);
        } else if((trial=xbox_ContiguousAlloc(size,0x1000u))==0) {
            fprintf(stderr,"[mapmaker] arena: the contiguous window cannot give %u bytes (+%u MB); stock %u bytes kept\n",
                size,mb,MAPMAKER_ARENA_STOCK);
        } else {
            xbox_ContiguousFree(trial);
            WORD_AT(MAPMAKER_ARENA_SIZE)=size;raised=size;
            fprintf(stderr,"[mapmaker] arena: TSFP_MAPMAKER_ARENA_MB=%u, %u -> %u bytes (Level heap and heap 0 gain %u MB in every scene)\n",
                mb,MAPMAKER_ARENA_STOCK,size,mb);
        }
        fflush(stderr);
    }
    original();
    if(raised) {
        fprintf(stderr,"[mapmaker] arena: allocated at 0x%08X\n",WORD_AT(MAPMAKER_ARENA_BASE));
        fflush(stderr);
    }
}

#ifdef TSFP_MAPMAKER_MEMORY_TEST
static void mapmaker_memory_reset(void) { memset(&mapmaker_memory,0,sizeof mapmaker_memory); }
#endif
