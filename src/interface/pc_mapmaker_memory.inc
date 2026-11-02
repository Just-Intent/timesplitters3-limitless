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
/* ---- nav block --------------------------------------------------------- */
/* The AI navigation data of a MapMaker map is CPU-only (pads, links, per-pad
 * AI data, cover and runtime pads, the group distance matrix), and at 511
 * pieces it is several MB of the Level heap. In scene 100 it comes from one
 * block of the runtime heap (guest RAM below 0x04000000, xbox_HeapAlloc),
 * reserved at the first MapMaker map and kept for the run:
 *  - the pools of pc_display_mapmaker_pools (pc_mapmaker_limits.inc, through
 *    mapmaker_pool_source),
 *  - the matrix sub_00275F70 asks for at 0x00275F90 (return 0x00275F95,
 *    (groups + 1) x groups x 4 B, heap 1; the caller zeroes it, so do we).
 * Nothing else: Level-heap data the GPU or the APU reads must stay in the
 * contiguous window (guest code may form physical addresses from it; see the
 * container note in pc_mapmaker_container.inc).
 * Lifetime: the Level heap is reset only by sub_0003E650 (the one writer of
 * its base besides start-up, called from 0x0003D504 per level), so
 * pc_mapmaker_memory_level_reset (WRAP sub_0003E650) empties the block right
 * after it, and nothing is taken from the block before that hook has run.
 * When the block is full or cannot be reserved, the Level heap is used as
 * before. TSFP_MAPMAKER_NAV=0 turns it off.
 * Free runtime heap: guest RAM is 64 MB; the image and this fork's extension
 * end at 0x00920000, the main stack at 0x011A0000, so the runtime heap is
 * 46.4 MB (src/runtime/guest_thread_memory.c). On the Xbox the title's
 * virtual and pool allocations shared 64 MB with the 9 MB image and its
 * 47.1 MB arena, so they were at most about 8 MB; thread stacks add at most
 * 8 x 512 KB. The block (6 MB) therefore leaves more than 25 MB (INFERRED;
 * the map-start line prints runtime_heap_free_min to confirm). */
#ifndef MAPMAKER_GUEST_RET
#define MAPMAKER_GUEST_RET() (g_esp+=4u) /* see pc_mapmaker_limits.inc */
#endif
#define MAPMAKER_NAV_BYTES 0x00600000u        /* pools 1.59 MB + matrix 4.2 MB at 1024 groups */
#define MAPMAKER_NAV_MATRIX_RET 0x00275F95u   /* sub_00275F70: call sub_0003DFA0 at 0x00275F90 */
#define MAPMAKER_NAV_GROUPS 0x007B666Cu       /* group count in play, read by sub_00275F70 */
#define MAPMAKER_NAV_SCENE 100u
extern uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment);
static struct {
    int initialized, off, tried, reported;
    uint32_t base, size, used, peak, resets, fallbacks;
} mapmaker_nav;
static void mapmaker_nav_read(void)
{
    const char *v;
    if(mapmaker_nav.initialized) return;
    v=getenv("TSFP_MAPMAKER_NAV");
    mapmaker_nav.initialized=1;
    mapmaker_nav.off=v && v[0]=='0' && !v[1];
}
/* One line per map at its first nav allocation (the start of the AI
 * navigation load): Level heap, arena, Eternal heap and nav block. */
static void mapmaker_nav_report(void)
{
    uint32_t arena=WORD_AT(0x007E72D4u),low=WORD_AT(0x006B837Cu),end=WORD_AT(0x006B8380u),top=WORD_AT(0x006B8388u);
    fprintf(stderr,"[mapmaker] map start memory: level heap free %d, level used %u, heap 0 used %u, arena %u at 0x%08X,"
        " eternal free %d; nav block %u of %u bytes at 0x%08X (runtime heap), %u fallbacks",
        (int)(int32_t)WORD_AT(MAPMAKER_MEMORY_SHARED_FREE),low-(arena+0x9DC00u),end-top,WORD_AT(0x007E72D8u),arena,
        (int)(int32_t)WORD_AT(MAPMAKER_MEMORY_ETERNAL_FREE),mapmaker_nav.used,mapmaker_nav.size,mapmaker_nav.base,
        mapmaker_nav.fallbacks);
#ifdef MAPMAKER_PROBE_HEAP_TOP
    {
        uint32_t usage[PC_USAGE_WORDS],contig_end;
        memset(usage,0,sizeof usage);
        pc_capture_allocation_usage(usage);
        contig_end=MAPMAKER_PROBE_CONTIG_BASE+xbox_ContiguousAllocatedBytes();
        if(usage[PC_USAGE_PHYS_END_PEAK]>contig_end) contig_end=usage[PC_USAGE_PHYS_END_PEAK];
        fprintf(stderr,"; host runtime_heap_live %u runtime_heap_free_min %d contig_free_min %d",
            usage[PC_USAGE_HEAP_LIVE],
            usage[PC_USAGE_HEAP_END_PEAK]?(int)(MAPMAKER_PROBE_HEAP_TOP-usage[PC_USAGE_HEAP_END_PEAK]):0,
            (int)(MAPMAKER_PROBE_CONTIG_TOP-contig_end));
    }
#endif
    fputc('\n',stderr);
    fflush(stderr);
}
/* bytes from the block, zeroed and 16-aligned as the Level heap's, or 0. */
static uint32_t mapmaker_nav_alloc(uint32_t bytes)
{
    uint32_t need=(bytes+15u)&~15u,p;
    mapmaker_nav_read();
    if(mapmaker_nav.off || !mapmaker_nav.resets || WORD_AT(MAPMAKER_MEMORY_SCENE)!=MAPMAKER_NAV_SCENE || !bytes) return 0;
    if(!mapmaker_nav.tried) {
        mapmaker_nav.tried=1;
        mapmaker_nav.base=xbox_HeapAlloc(MAPMAKER_NAV_BYTES,4096u);
        mapmaker_nav.size=mapmaker_nav.base?MAPMAKER_NAV_BYTES:0u;
        if(!mapmaker_nav.base) {
            fprintf(stderr,"[mapmaker] nav block: the runtime heap cannot give %u bytes; AI navigation data stays in the Level heap\n",
                MAPMAKER_NAV_BYTES);
            fflush(stderr);
        }
    }
    if(!mapmaker_nav.reported) { mapmaker_nav.reported=1;mapmaker_nav_report(); }
    if(!mapmaker_nav.base || need<bytes || need>mapmaker_nav.size-mapmaker_nav.used) {
        mapmaker_nav.fallbacks++;
        return 0;
    }
    p=mapmaker_nav.base+mapmaker_nav.used;
    mapmaker_nav.used+=need;
    if(mapmaker_nav.used>mapmaker_nav.peak) mapmaker_nav.peak=mapmaker_nav.used;
    memset((void *)((uintptr_t)p+g_xbox_mem_offset),0,need);
    return p;
}
/* WRAP sub_0003E650 (Level heap reset, cdecl, plain ret). */
void pc_mapmaker_memory_level_reset(void (*original)(void))
{
    original();
    mapmaker_nav.used=0;mapmaker_nav.reported=0;
    mapmaker_nav.resets++;
#ifdef MAPMAKER_POOL_SOURCE
    mapmaker_pool_source=mapmaker_nav_alloc;   /* pc_mapmaker_limits.inc pools */
#endif
}
/* The group distance matrix (sub_00275F70) from the nav block. */
static int mapmaker_nav_matrix(uint32_t caller,uint32_t size,uint32_t heap)
{
    uint32_t p;
    if(caller!=MAPMAKER_NAV_MATRIX_RET || heap!=1u || WORD_AT(MAPMAKER_MEMORY_SCENE)!=MAPMAKER_NAV_SCENE) return 0;
    p=mapmaker_nav_alloc(size);
    fprintf(stderr,"[mapmaker] nav: group matrix of %u groups, %u bytes, in the %s (nav block %u of %u used);"
        " level heap free %d\n",
        WORD_AT(MAPMAKER_NAV_GROUPS),size,p?"nav block":"Level heap",mapmaker_nav.used,mapmaker_nav.size,
        (int)(int32_t)WORD_AT(MAPMAKER_MEMORY_SHARED_FREE));
    fflush(stderr);
    if(!p) return 0;
    g_eax=p;
    MAPMAKER_GUEST_RET();                      /* the original's ret: the caller pops the arguments */
    return 1;
}

static void mapmaker_memory_guard(void (*original)(void),unsigned heap_arg)
{
    uint32_t caller=0,size=0,heap=0xFFFFFFFFu;
    int32_t before=0;
    int pool;
    if(MAPMAKER_STACK_OK(4u+4u*(heap_arg+1u))) {
        caller=WORD_AT(g_esp);size=WORD_AT(g_esp+4u);heap=WORD_AT(g_esp+4u+4u*heap_arg);
    }
    if(heap_arg==1u && mapmaker_nav_matrix(caller,size,heap)) return;
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
#ifdef MAPMAKER_BUDGET_ARENA
            mapmaker_budget.arena_mb=mb;mapmaker_budget.initialized=0;  /* editor memory budget follows */
#endif
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
