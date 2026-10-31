/* Asset-free contract for src/pc_mapmaker_sky.inc (MapMaker per-map cloud
 * layer): inert without TSFP_MAPMAKER_LIMITS=1 and outside scenes 100/102;
 * the editor's Clouds checkbox follows a loaded map, a new map and the
 * checkbox, and survives the stock rebuild as info flag 0x40000 without
 * touching stock flags; in play only a flagged map's MapMaker sky record gets
 * the cloud fields, the level's sky is set only while the original runs, and
 * the copy is undone at the next MapMaker map. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00900000/4], g_eax, g_ecx, g_edx, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }

void sub_0003DFA0(void) { g_ecx=6; }
void sub_00079D60(void) { g_ecx=1; }  /* distinct bodies: identical ones may be folded */
void sub_0007D4F0(void) { g_ecx=2; }
void sub_00078560(void) { g_ecx=3; }
#define CALLS 16
static struct { void (*f)(void); int n; uint32_t a[6]; } calls[CALLS];
static int call_count;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    int i;
    va_start(a,n);
    if(call_count<CALLS) {
        calls[call_count].f=f;calls[call_count].n=n;
        for(i=0;i<n && i<6;i++) calls[call_count].a[i]=va_arg(a,uint32_t);
    }
    call_count++;
    va_end(a);
    return 0;                                         /* the real guest_call restores every register */
}
static uint32_t heap_result=0x00300000u;
static int heap_calls;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment)
{
    (void)alignment;heap_calls++;
    if(heap_result) memset((char *)memory+heap_result,0xCD,bytes);
    return heap_result;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_sky.inc"

static int failures, esp_errors;
#define ESP0 0x00600000u
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}

/* ---- fake guest ---------------------------------------------------------- */
#define ED 0x00100000u          /* editor object */
#define C 0x00200000u           /* map container [0x00784014] */
#define C2 0x00280000u          /* another container */
#define INFO (C+0x20u+8u)       /* info chunk data: after one 0x18-byte chunk */
static void make_container(uint32_t c,uint32_t flags,int sky,uint32_t tileset)
{
    memset((char *)memory+c,0,0x1000);
    WORD_AT(c)=0x00C77667u;WORD_AT(c+4)=0x12Du;
    WORD_AT(c+8)=0x00C77669u;WORD_AT(c+12)=0x10u;               /* a piece chunk first */
    WORD_AT(c+0x20)=0x00C77668u;WORD_AT(c+0x24)=0x3DCu;
    WORD_AT(c+0x28+0x08)=tileset<<16;
    WORD_AT(c+0x28+0x10)=(uint32_t)sky&0xFFFFu;
    WORD_AT(c+0x28+0x244)=flags;
    WORD_AT(c+0x28+0x3DC)=0x00C7766Au;
}
static int rebuild_calls;
static uint32_t rebuild_flags=0x00010000u;      /* what the stock rebuild writes */
static void rebuild_original(void)               /* sub_003006C0: rewrites the info flags */
{
    uint32_t c=WORD_AT(g_esp+4);
    rebuild_calls++;
    WORD_AT(c+0x28+0x244)=rebuild_flags;
    g_esp+=4u;
}
static int originals;
static void plain_original(void) { originals++;g_esp+=4u; }

static void hook(void (*fn)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t arg0,uint32_t arg1)
{
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4)=arg0;WORD_AT(ESP0+8)=arg1;
    g_eax=0x1234u;
    fn(orig);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"esp %+d after a hook\n",(int)(g_esp-ESP0));esp_errors++; }
    g_esp=ESP0;
}
static void settings(uint32_t ret) { call_count=0;hook(pc_mapmaker_sky_settings,plain_original,ret,ED+0x21Cu,0); }
static void reset(uint32_t ret) { hook(pc_mapmaker_sky_reset,plain_original,ret,0,0); }
static uint32_t compile(uint32_t c)
{
    hook(pc_mapmaker_sky_compile,rebuild_original,0x00313B01u,c,3u);
    return WORD_AT(c+0x28+0x244);
}

/* ---- play ---------------------------------------------------------------- */
static uint32_t seen_level_sky,seen_record[5],seen_template[5];
static int seen_sky_index=-1;
static void clouds_original(void)                /* sub_0011C9F0 reads the level's record */
{
    int sky=(int)WORD_AT(MAPMAKER_SKY_LEVEL+0x10u);
    unsigned i;
    seen_level_sky=(uint32_t)sky;seen_sky_index=sky;
    for(i=0;i<5;i++) seen_record[i]=WORD_AT(MAPMAKER_SKY_TABLE+(uint32_t)sky*0x34u+0x10u+i*4u);
    originals++;g_esp+=4u;
}
static void play(void) { hook(pc_mapmaker_sky_clouds,clouds_original,0x001CDBBCu,0,0); }
static uint32_t rec(int sky,unsigned i) { return WORD_AT(MAPMAKER_SKY_TABLE+(uint32_t)sky*0x34u+0x10u+i*4u); }
static uint32_t pristine_sum(void)
{
    uint32_t h=0;int s;unsigned i;
    for(s=0;s<70;s++) for(i=0;i<13;i++) h=h*31u+WORD_AT(MAPMAKER_SKY_TABLE+(uint32_t)s*0x34u+i*4u);
    return h;
}

int main(void)
{
    static const uint32_t tmpl[5]={0x5A5A6420u,0u,0x43480000u,0x42480000u,0u};
    uint32_t cell,h0,v;
    int s;unsigned i;
    g_xbox_mem_offset=(uintptr_t)memory;
    /* Sky table as in the XBE: record 5 has clouds, 58/59 floats only, 60..67 zero. */
    for(i=0;i<5;i++) WORD_AT(MAPMAKER_SKY_TABLE+5u*0x34u+0x10u+i*4u)=tmpl[i];
    WORD_AT(MAPMAKER_SKY_TABLE+34u*0x34u+0x18u)=0x43480000u;
    for(s=58;s<=59;s++) { WORD_AT(MAPMAKER_SKY_TABLE+(uint32_t)s*0x34u+0x18u)=0x43480000u;WORD_AT(MAPMAKER_SKY_TABLE+(uint32_t)s*0x34u+0x1Cu)=0x42480000u; }
    for(s=0;s<70;s++) WORD_AT(MAPMAKER_SKY_TABLE+(uint32_t)s*0x34u+4u)=0x700u+(uint32_t)s;
    /* Tileset default skies (+2): 60, 65, 61, 66, 59, 60. */
    { static const uint8_t def[6]={0x3C,0x41,0x3D,0x42,0x3B,0x3C};
      for(i=0;i<6;i++) WORD_AT(MAPMAKER_SKY_TILESETS+i*0x58u)=0xBCu|(uint32_t)def[i]<<16; }
    WORD_AT(0x007844A8u)=ED;WORD_AT(0x00784014u)=C;
    WORD_AT(MAPMAKER_SKY_LEVEL+0x10u)=7u;           /* stale sky of an earlier map */
    h0=pristine_sum();

    /* ---- limits off: every hook only runs its original ---- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);set_env("TSFP_MAPMAKER_POOLS",NULL);
    WORD_AT(0x0079094Cu)=102u;
    make_container(C,0x00050000u,-1,0);
    reset(0x00302A7Bu);
    settings(MAPMAKER_SKY_RET_SETTINGS_BIND);
    CHECK(call_count==0 && heap_calls==0);
    CHECK(compile(C)==0x00010000u);                   /* the stock rebuild drops the flag */
    WORD_AT(0x0079094Cu)=100u;
    make_container(C,0x00050000u,-1,0);
    originals=0;play();
    CHECK(originals==1 && seen_level_sky==7u && pristine_sum()==h0);

    /* ---- limits on, editor ---- */
    set_env("TSFP_MAPMAKER_LIMITS","1");
    WORD_AT(0x0079094Cu)=102u;
    make_container(C,0x00050000u,62,1);               /* a saved map with clouds */
    reset(MAPMAKER_SKY_RET_LOADER_RESET);             /* loading it */
    settings(0x0031378Bu);                            /* another binding: nothing added */
    CHECK(call_count==0 && heap_calls==0);
    settings(MAPMAKER_SKY_RET_SETTINGS_BIND);
    cell=mapmaker_sky.storage;
    CHECK(heap_calls==1 && cell==0x00300000u && WORD_AT(cell)==1u);
    CHECK(!strcmp((const char *)memory+cell+4u,"Clouds"));
    CHECK(call_count==3);
    CHECK(calls[0].f==sub_00079D60 && calls[0].n==6 && calls[0].a[0]==cell+4u && calls[0].a[1]==0xFFFFFFFFu &&
          calls[0].a[2]==0x8800u && !calls[0].a[3] && !calls[0].a[4] && !calls[0].a[5]);
    CHECK(calls[1].f==sub_0007D4F0 && calls[1].n==2 && !calls[1].a[0] && !calls[1].a[1]);
    CHECK(calls[2].f==sub_00078560 && calls[2].n==1 && calls[2].a[0]==cell);
    CHECK(compile(C)==0x00050000u);                   /* kept through the rebuild */
    WORD_AT(cell)=0;                                  /* unchecked */
    CHECK(compile(C)==0x00010000u);
    rebuild_flags=0x00038001u;                        /* stock bits survive either way */
    CHECK(compile(C)==0x00038001u);
    WORD_AT(cell)=1;
    CHECK(compile(C)==0x00078001u);
    rebuild_flags=0x00010000u;
    settings(MAPMAKER_SKY_RET_SETTINGS_BIND);        /* reopened: shows the current choice */
    CHECK(heap_calls==1 && WORD_AT(cell)==1u);

    reset(0x00311AA7u);                               /* New map: no clouds */
    CHECK(compile(C)==0x00010000u);
    make_container(C,0x00050000u,62,1);
    reset(0x00311AA7u);
    CHECK(compile(C)==0x00010000u);                   /* not inherited from the old container */
    settings(MAPMAKER_SKY_RET_SETTINGS_BIND);
    CHECK(WORD_AT(cell)==0u);

    make_container(C,0x00050000u,62,1);
    reset(MAPMAKER_SKY_RET_LOADER_RESET);             /* load, rebuild without opening the menu */
    CHECK(compile(C)==0x00050000u);
    make_container(C,0x00010000u,62,1);
    reset(MAPMAKER_SKY_RET_LOADER_RESET);
    CHECK(compile(C)==0x00010000u);

    reset(0x00311AA7u);                               /* a new editor object follows the container */
    WORD_AT(0x007844A8u)=ED+0x1000u;
    make_container(C,0x00050000u,62,1);
    CHECK(compile(C)==0x00050000u);
    WORD_AT(0x007844A8u)=ED;

    make_container(C2,0x00050000u,62,1);              /* the rebuilt container is the argument */
    make_container(C,0x00010000u,62,1);
    reset(MAPMAKER_SKY_RET_LOADER_RESET);
    CHECK(compile(C2)==0x00050000u && WORD_AT(C+0x28+0x244)==0x00010000u);

    WORD_AT(0x0079094Cu)=101u;                        /* front end: not the editor */
    make_container(C,0x00050000u,62,1);
    reset(0x00311AA7u);
    settings(MAPMAKER_SKY_RET_SETTINGS_BIND);
    CHECK(call_count==0);
    CHECK(compile(C)==0x00010000u);
    WORD_AT(0x0079094Cu)=102u;

    memset((char *)memory+C,0,16);                    /* no readable container: left to the original */
    CHECK(compile(C)==0x00010000u && rebuild_calls>0);

    /* ---- limits on, play ---- */
    WORD_AT(0x0079094Cu)=100u;
    make_container(C,0x00050000u,-1,2);               /* tileset 2 default: sky 61 */
    originals=0;play();
    CHECK(originals==1 && seen_level_sky==61u && WORD_AT(MAPMAKER_SKY_LEVEL+0x10u)==7u);
    for(i=0;i<5;i++) { CHECK(seen_record[i]==tmpl[i]);CHECK(rec(61,i)==tmpl[i]); }
    CHECK(rec(5,0)==tmpl[0]);
    WORD_AT(0x0079094Cu)=20u;                         /* a story level afterwards: untouched */
    for(i=0;i<5;i++) seen_template[i]=rec(61,i);
    originals=0;play();
    CHECK(originals==1 && seen_level_sky==7u && rec(61,0)==seen_template[0]);
    WORD_AT(0x0079094Cu)=100u;
    make_container(C,0x00010000u,65,0);               /* next map without clouds: undone */
    play();
    CHECK(seen_level_sky==7u && pristine_sum()==h0);
    make_container(C,0x00050000u,58,0);               /* sky 58 keeps the template's floats */
    play();
    CHECK(seen_level_sky==58u && seen_record[0]==tmpl[0] && seen_record[2]==0x43480000u);
    make_container(C,0x00050000u,34,0);               /* not a MapMaker sky: ignored */
    play();
    CHECK(seen_level_sky==7u && pristine_sum()==h0);
    make_container(C,0x00050000u,-1,9);               /* bad tileset: ignored */
    play();
    CHECK(seen_level_sky==7u && pristine_sum()==h0);
    make_container(C,0x00050000u,66,0);
    play();
    CHECK(rec(66,0)==tmpl[0]);
    set_env("TSFP_MAPMAKER_LIMITS",NULL);             /* switched off: undone, not applied */
    play();
    CHECK(seen_level_sky==7u && pristine_sum()==h0);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    WORD_AT(0x00784014u)=0;
    play();
    CHECK(seen_level_sky==7u && pristine_sum()==h0);
    WORD_AT(0x00784014u)=C;

    /* Allocation failure: no checkbox, counted. */
    mapmaker_sky.storage=0;heap_result=0;
    WORD_AT(0x0079094Cu)=102u;
    settings(MAPMAKER_SKY_RET_SETTINGS_BIND);
    CHECK(call_count==0 && mapmaker_sky.no_storage==1u);
    v=WORD_AT(0x007844A8u);(void)v;

    if(failures || esp_errors) { fprintf(stderr,"%d failures, %d esp errors\n",failures,esp_errors);return 1; }
    puts("mapmaker sky ok");
    return 0;
}
