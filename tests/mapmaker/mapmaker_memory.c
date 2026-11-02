/* Asset-free contract for src/interface/pc_mapmaker_memory.inc: the heap
 * guard reads only and logs an overrun once per event with size, heap and
 * caller; the arena switch is off by default, bounded, and keeps the stock
 * size when the contiguous window cannot give the raised one. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x1000000/4],g_esp,g_eax;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

/* Contiguous window: one fake block at a time, or exhausted. */
static int window_full;
static uint32_t live_block,live_size,allocs,frees;
uint32_t xbox_ContiguousAlloc(uint32_t size,uint32_t alignment)
{
    (void)alignment;allocs++;
    if(window_full || live_block) return 0;
    live_block=0x80001000u;live_size=size;
    return live_block;
}
int xbox_ContiguousFree(uint32_t address)
{
    frees++;
    if(!live_block || address!=live_block) return 0;
    live_block=0;live_size=0;
    return 1;
}

/* Runtime heap: one 6 MB block above the fixture's image addresses, or none. */
static int heap_full;
static uint32_t heap_allocs,heap_last_size;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment)
{
    (void)alignment;heap_allocs++;heap_last_size=bytes;
    return heap_full?0u:0x00800000u;
}

#define TSFP_MAPMAKER_MEMORY_TEST
#include "pc_mapmaker_memory.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stdout,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

static FILE *log_file;
static char log_text[65536];
static const char *log_path;
static void read_log(void)
{
    FILE *f;size_t n;
    fflush(stderr);
    f=fopen(log_path,"rb");
    if(!f) { log_text[0]=0;return; }
    n=fread(log_text,1,sizeof log_text-1,f);fclose(f);log_text[n]=0;
}
static size_t count(const char *needle)
{
    size_t n=0;const char *p=log_text;
    while((p=strstr(p,needle))) { n++;p+=strlen(needle); }
    return n;
}
static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value) setenv(name,value,1); else unsetenv(name);
#endif
}

/* sub_0003DFA0 / sub_0003E0B0 fakes: take from the free count the heap uses
 * (the shared one for heaps 0 and 1, Eternal for 2, none for 3/4) and pop
 * the return address as a lifted callee does. */
static unsigned originals;
static void fake_alloc(unsigned heap_arg)
{
    uint32_t size=(WORD_AT(g_esp+4u)+15u)&~15u,heap=WORD_AT(g_esp+4u+4u*heap_arg);
    originals++;
    if(heap<=1u) WORD_AT(MAPMAKER_MEMORY_SHARED_FREE)-=size+(heap==0u?16u:0u);
    else if(heap==2u) WORD_AT(MAPMAKER_MEMORY_ETERNAL_FREE)-=size;
    g_eax=0x00400000u;
    g_esp+=4u;
}
static void fake_alloc_plain(void) { fake_alloc(1u); }
static void fake_alloc_aligned(void) { fake_alloc(2u); }
static void call_alloc(uint32_t ret,uint32_t size,uint32_t heap)
{
    uint32_t esp=0x00700000u;
    g_esp=esp;WORD_AT(esp)=ret;WORD_AT(esp+4u)=size;WORD_AT(esp+8u)=heap;
    pc_mapmaker_memory_heap_alloc(fake_alloc_plain);
    CHECK(g_esp==esp+4u && g_eax==0x00400000u);
}
static void call_aligned(uint32_t ret,uint32_t size,uint32_t heap)
{
    uint32_t esp=0x00700000u;
    g_esp=esp;WORD_AT(esp)=ret;WORD_AT(esp+4u)=size;WORD_AT(esp+8u)=16u;WORD_AT(esp+12u)=heap;
    pc_mapmaker_memory_heap_alloc_aligned(fake_alloc_aligned);
    CHECK(g_esp==esp+4u);
}

/* sub_0003E650 fake: the Level heap reset. */
static unsigned reset_originals;
static void fake_reset(void) { reset_originals++; g_esp+=4u; }
static void call_reset(void)
{
    uint32_t esp=0x00700000u;
    g_esp=esp;WORD_AT(esp)=0x0003D509u;
    pc_mapmaker_memory_level_reset(fake_reset);
    CHECK(g_esp==esp+4u);
}
/* The matrix request of sub_00275F70 (or another caller): returns eax. */
static uint32_t call_matrix(uint32_t ret,uint32_t size,uint32_t heap,unsigned *original_ran)
{
    uint32_t esp=0x00700000u;
    unsigned before=originals;
    g_esp=esp;WORD_AT(esp)=ret;WORD_AT(esp+4u)=size;WORD_AT(esp+8u)=heap;g_eax=0;
    pc_mapmaker_memory_heap_alloc(fake_alloc_plain);
    CHECK(g_esp==esp+4u);
    *original_ran=originals-before;
    return g_eax;
}

static unsigned arena_originals;
static uint32_t arena_seen;
static void fake_arena(void)
{
    arena_originals++;
    arena_seen=WORD_AT(MAPMAKER_ARENA_SIZE);   /* what 0x00023CDA would read */
    WORD_AT(MAPMAKER_ARENA_BASE)=0x80002000u;
    g_esp+=4u;
}
static void call_arena(const char *mb)
{
    uint32_t esp=0x00700000u;
    set_env("TSFP_MAPMAKER_ARENA_MB",mb);
    g_esp=esp;WORD_AT(esp)=0x0003C335u;
    pc_mapmaker_memory_arena(fake_arena);
    CHECK(g_esp==esp+4u);
}

int main(int argc,char **argv)
{
    unsigned i;
    if(argc<2) return 2;
    log_path=argv[1];
    g_xbox_mem_offset=(uintptr_t)memory;
    log_file=freopen(log_path,"wb",stderr);
    if(!log_file) return 3;

    /* Guard: fine allocations are untouched and not logged. */
    WORD_AT(MAPMAKER_MEMORY_SCENE)=100;
    WORD_AT(MAPMAKER_MEMORY_SHARED_FREE)=100000u;WORD_AT(MAPMAKER_MEMORY_ETERNAL_FREE)=5000u;
    call_alloc(0x0009A1B2u,1000u,1u);
    call_alloc(0x0009A1B2u,1000u,0u);
    read_log();
    CHECK(originals==2 && !log_text[0]);
    CHECK(WORD_AT(MAPMAKER_MEMORY_SHARED_FREE)==100000u-1008u-1024u);
    /* The overrun: one line with the size, heap, caller, scene and free counts. */
    call_alloc(0x0009A1B2u,200000u,1u);
    read_log();
    CHECK(strstr(log_text,"[mapmaker] heap full: heap 1 size 200000 caller 0x0009A1B2 scene 100 free 97968 -> -102032"
                          " (Level heap and heap 0 overlap; 1 allocations while overrun; minimum -102032)\n")!=NULL);
    /* Still overrun: counted, not logged again. */
    call_alloc(0x00011111u,64u,0u);
    call_aligned(0x00022222u,64u,1u);
    read_log();
    CHECK(count("heap full")==1);
    CHECK(mapmaker_memory.overrun_allocs==3u && mapmaker_memory.min_free[0]==-102032-80-64);
    /* Back above zero (a new level resets the heap), then a new overrun is logged. */
    WORD_AT(MAPMAKER_MEMORY_SHARED_FREE)=1000u;
    call_alloc(0x00033333u,16u,1u);
    call_aligned(0x00044444u,5000u,1u);
    read_log();
    CHECK(count("heap full")==2 && strstr(log_text,"heap 1 size 5000 caller 0x00044444 scene 100 free 984 -> -4024")!=NULL);
    /* The Eternal heap has its own count; heaps 3 and 4 have none. */
    call_alloc(0x00055555u,6000u,2u);
    call_alloc(0x00066666u,6000u,3u);
    call_alloc(0x00077777u,6000u,4u);
    read_log();
    CHECK(count("heap full")==3 && strstr(log_text,"heap 2 size 6000 caller 0x00055555 scene 100 free 5000 -> -1000"
                                         " (Eternal heap past its end;")!=NULL);
    /* Bounded: at most MAPMAKER_MEMORY_LOG_LINES lines per run. */
    for(i=0;i<20u;i++) {
        WORD_AT(MAPMAKER_MEMORY_SHARED_FREE)=5000u;
        call_alloc(0x00088888u,16u,1u);
        call_alloc(0x00088888u,8192u,1u);
    }
    read_log();
    CHECK(count("heap full")==MAPMAKER_MEMORY_LOG_LINES);

    /* Nav block: nothing before the Level heap reset hook has run. */
    {
        unsigned ran;
        uint32_t p,q,free_before;
        WORD_AT(MAPMAKER_MEMORY_SCENE)=100;
        WORD_AT(MAPMAKER_MEMORY_SHARED_FREE)=3000000u;
        WORD_AT(0x007B666Cu)=1024u;
        p=call_matrix(MAPMAKER_NAV_MATRIX_RET,1025u*1024u*4u,1u,&ran);
        CHECK(ran==1 && p==0x00400000u && heap_allocs==0);
        call_reset();
        CHECK(reset_originals==1 && mapmaker_nav.resets==1);
        /* Other scenes, other callers, other heaps: the original. */
        WORD_AT(MAPMAKER_MEMORY_SCENE)=53;
        p=call_matrix(MAPMAKER_NAV_MATRIX_RET,4096u,1u,&ran);
        CHECK(ran==1 && heap_allocs==0);
        CHECK(mapmaker_nav_alloc(64u)==0 && heap_allocs==0);              /* pools: scene 100 only */
        WORD_AT(MAPMAKER_MEMORY_SCENE)=100;
        p=call_matrix(0x0009A1B2u,4096u,1u,&ran);
        CHECK(ran==1 && heap_allocs==0);
        p=call_matrix(MAPMAKER_NAV_MATRIX_RET,4096u,0u,&ran);
        CHECK(ran==1 && heap_allocs==0);
        /* The matrix at 1024 groups: from the block, zeroed, Level heap untouched. */
        memset((char *)memory+0x00800000u,0xCD,0x00600000u);
        WORD_AT(MAPMAKER_MEMORY_SHARED_FREE)=3000000u;free_before=3000000u;
        p=call_matrix(MAPMAKER_NAV_MATRIX_RET,1025u*1024u*4u,1u,&ran);
        read_log();
        CHECK(ran==0 && p==0x00800000u && heap_allocs==1 && heap_last_size==MAPMAKER_NAV_BYTES);
        CHECK(WORD_AT(p)==0 && WORD_AT(p+1025u*1024u*4u-4u)==0 && WORD_AT(MAPMAKER_MEMORY_SHARED_FREE)==free_before);
        CHECK(strstr(log_text,"[mapmaker] map start memory: level heap free 3000000")!=NULL && count("map start memory")==1);
        CHECK(strstr(log_text,"[mapmaker] nav: group matrix of 1024 groups, 4198400 bytes, in the nav block (nav block 4198400 of 6291456 used)")!=NULL);
        /* The pool source the limits area uses: 16-aligned after it. */
        q=mapmaker_nav_alloc(100u);
        CHECK(q==0x00800000u+4198400u && mapmaker_nav.used==4198400u+112u);
        CHECK(mapmaker_nav_alloc(0x00600000u)==0 && mapmaker_nav.fallbacks==1);   /* too big: Level heap */
        p=call_matrix(MAPMAKER_NAV_MATRIX_RET,0x00300000u,1u,&ran);                 /* no room: original */
        read_log();
        CHECK(ran==1 && p==0x00400000u && strstr(log_text,"in the Level heap")!=NULL);
        CHECK(count("map start memory")==1);                                        /* once per map */
        /* The next level resets the block. */
        call_reset();
        CHECK(mapmaker_nav.used==0 && mapmaker_nav_alloc(64u)==0x00800000u && heap_allocs==1);
        read_log();
        CHECK(count("map start memory")==2);
        /* Off switch. */
        set_env("TSFP_MAPMAKER_NAV","0");mapmaker_nav.initialized=0;
        p=call_matrix(MAPMAKER_NAV_MATRIX_RET,4096u,1u,&ran);
        CHECK(ran==1 && mapmaker_nav_alloc(64u)==0);
        set_env("TSFP_MAPMAKER_NAV",NULL);mapmaker_nav.initialized=0;
        /* The runtime heap cannot give the block: one line, the Level heap from then on. */
        memset(&mapmaker_nav,0,sizeof mapmaker_nav);heap_full=1;
        call_reset();
        p=call_matrix(MAPMAKER_NAV_MATRIX_RET,4096u,1u,&ran);
        read_log();
        CHECK(ran==1 && count("the runtime heap cannot give 6291456 bytes")==1);
        CHECK(mapmaker_nav_alloc(64u)==0 && heap_allocs==2);                         /* tried once */
        heap_full=0;
        /* The aligned allocator is never redirected. */
        memset(&mapmaker_nav,0,sizeof mapmaker_nav);call_reset();
        {
            unsigned before=originals;
            call_aligned(MAPMAKER_NAV_MATRIX_RET,4096u,1u);
            CHECK(originals==before+1);
        }
    }

    /* Arena: unset or 0 is stock. */
    WORD_AT(MAPMAKER_ARENA_SIZE)=MAPMAKER_ARENA_STOCK;
    allocs=frees=0;
    call_arena(NULL);
    CHECK(arena_originals==1 && arena_seen==MAPMAKER_ARENA_STOCK && allocs==0);
    call_arena("0");
    CHECK(arena_originals==2 && arena_seen==MAPMAKER_ARENA_STOCK && allocs==0);
    /* 8 MB: the trial block is taken and released, the dword is raised before the original. */
    call_arena("8");
    read_log();
    CHECK(arena_seen==MAPMAKER_ARENA_STOCK+(8u<<20) && allocs==1 && frees==1 && !live_block);
    CHECK(strstr(log_text,"[mapmaker] arena: TSFP_MAPMAKER_ARENA_MB=8, 47104000 -> 55492608 bytes")!=NULL);
    CHECK(strstr(log_text,"[mapmaker] arena: allocated at 0x80002000\n")!=NULL);
    /* Not the stock value any more: left alone. */
    call_arena("8");
    read_log();
    CHECK(arena_seen==MAPMAKER_ARENA_STOCK+(8u<<20) && allocs==1 && strstr(log_text,"not the stock 0x02CEC000; unchanged")!=NULL);
    /* The window cannot give it: stock kept. */
    WORD_AT(MAPMAKER_ARENA_SIZE)=MAPMAKER_ARENA_STOCK;
    window_full=1;
    call_arena("16");
    read_log();
    CHECK(arena_seen==MAPMAKER_ARENA_STOCK && strstr(log_text,"cannot give 63881216 bytes (+16 MB); stock 47104000 bytes kept")!=NULL);
    window_full=0;
    /* Out of range or malformed: stock, with a warning. */
    call_arena("17");
    call_arena("4x");
    read_log();
    CHECK(arena_seen==MAPMAKER_ARENA_STOCK && count("ignored (0..16); stock arena")==2);
    /* The maximum. */
    call_arena("16");
    CHECK(arena_seen==MAPMAKER_ARENA_STOCK+(16u<<20));

    fflush(stderr);
    if(failures) { read_log();fprintf(stdout,"%d failures\nlog:\n%s",failures,log_text);return 1; }
    fprintf(stdout,"mapmaker memory ok\n");
    return 0;
}
