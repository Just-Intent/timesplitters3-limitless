/* Asset-free contract for src/interface/pc_mapmaker_skymusic.inc (extra MapMaker
 * skies, weather and music): rows only in the editor's three lists with the
 * switch on; the rebuild never leaves an extra value in a stock field and
 * keeps the row numbers in info flag bits 19..30 without touching the stock
 * or Clouds bits; the loader turns them back; in play the right level pak is
 * mounted index-only within the slot budget, the map start sees the extra
 * sky only while it runs and only when its pak and model row are there, the
 * sandstorm is armed and its list set only after its buffers exist, and the
 * extra music replaces only the map's own -1. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00920000/4], g_eax, g_ecx, g_edx, g_esp;
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
void sub_00071800(void) { g_ecx=4; }
void sub_00062260(void) { g_ecx=5; }
void sub_000622D0(void) { g_ecx=7; }
void sub_000A5E40(void) { g_ecx=8; }
#define CALLS 64
static struct { void (*f)(void); int n; uint32_t a[6]; } calls[CALLS];
static int call_count;
static char mounted[8][32];
static int mounted_count, mount_fails;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    int i;
    uint32_t arg[6]={0};
    va_start(a,n);
    for(i=0;i<n && i<6;i++) arg[i]=va_arg(a,uint32_t);
    va_end(a);
    if(call_count<CALLS) {
        calls[call_count].f=f;calls[call_count].n=n;
        memcpy(calls[call_count].a,arg,sizeof arg);
    }
    call_count++;
    if(f==sub_00062260) {                            /* is mounted: index or -1 */
        for(i=0;i<mounted_count;i++) if(!strcmp(mounted[i],(const char *)memory+arg[0])) return (uint32_t)i;
        return 0xFFFFFFFFu;
    }
    if(f==sub_000622D0) {                            /* mount */
        if(mount_fails || mounted_count>=8) return 0xFFFFFFFFu;
        strncpy(mounted[mounted_count],(const char *)memory+arg[0],31);
        WORD_AT(0x006D7880u)++;
        return (uint32_t)mounted_count++;
    }
    return 0;
}
static uint32_t heap_next=0x00300000u;
static int heap_calls, heap_fail;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment)
{
    uint32_t p;
    (void)alignment;heap_calls++;
    if(heap_fail) return 0;
    p=heap_next;heap_next+=(bytes+15u)&~15u;
    memset((char *)memory+p,0xCD,bytes);
    return p;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_sky.inc"
#include "pc_mapmaker_skymusic.inc"

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
#define C2 0x00280000u
#define INFO(c) ((c)+0x20u+8u)  /* info chunk data: after one 0x18-byte chunk */
static uint8_t B(uint32_t a) { return *((uint8_t *)memory+a); }
static int H(uint32_t a) { return (int16_t)(B(a)|B(a+1)<<8); }
static void setH(uint32_t a,int v) { *((uint8_t *)memory+a)=(uint8_t)v;*((uint8_t *)memory+a+1)=(uint8_t)((uint32_t)v>>8); }
static void make_container(uint32_t c,uint32_t flags,int music,int sky,int weather)
{
    memset((char *)memory+c,0,0x1000);
    WORD_AT(c)=0x00C77667u;WORD_AT(c+4)=0x12Du;
    WORD_AT(c+8)=0x00C77669u;WORD_AT(c+12)=0x10u;
    WORD_AT(c+0x20)=0x00C77668u;WORD_AT(c+0x24)=0x3DCu;
    setH(INFO(c)+0x0E,music);setH(INFO(c)+0x10,sky);*((uint8_t *)memory+INFO(c)+0x12)=(uint8_t)weather;
    WORD_AT(INFO(c)+0x244)=flags;
    WORD_AT(INFO(c)+0x3DC)=0x00C7766Au;
}
static void editor(int music,int sky,int weather)
{
    WORD_AT(ED+0x210)=(uint32_t)music;WORD_AT(ED+0x220)=(uint32_t)sky;WORD_AT(ED+0x224)=(uint32_t)weather;
}
static int originals, calls_at_original;
static uint32_t eax_at_original;
static void plain_original(void) { originals++;calls_at_original=call_count;eax_at_original=g_eax;g_esp+=4u; }
static uint32_t rebuild_flags=0x00050000u;     /* what the stock rebuild writes (+ Clouds) */
static void rebuild_original(void)              /* sub_003006C0: info fields from the editor */
{
    uint32_t c=WORD_AT(g_esp+4);
    setH(INFO(c)+0x0E,(int)WORD_AT(ED+0x210));
    setH(INFO(c)+0x10,(int)WORD_AT(ED+0x220));
    *((uint8_t *)memory+INFO(c)+0x12)=(uint8_t)WORD_AT(ED+0x224);
    WORD_AT(INFO(c)+0x244)=rebuild_flags;
    originals++;g_esp+=4u;
}
static void load_original(void)                 /* sub_00301A90: editor fields from the info */
{
    uint32_t c=WORD_AT(g_esp+4);
    WORD_AT(ED+0x210)=(uint32_t)H(INFO(c)+0x0E);
    WORD_AT(ED+0x220)=(uint32_t)H(INFO(c)+0x10);
    WORD_AT(ED+0x224)=B(INFO(c)+0x12);
    originals++;g_esp+=4u;
}
static int seen_sky=-999;
static void level_original(void) { seen_sky=H(INFO(WORD_AT(0x00784014u))+0x10);originals++;g_esp+=4u; }
static void weather_start_original(void)        /* sub_0011B310: default list */
{
    WORD_AT(0x007AC804u)=0x004E63F8u;WORD_AT(0x007AC800u)=1u;WORD_AT(0x007AC808u)=0;
    originals++;g_esp+=4u;
}
static void weather_load_original(void) { WORD_AT(0x0073D9ACu)=0;originals++;g_esp+=4u; }

static void hook(void (*fn)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t arg0,uint32_t arg1,uint32_t eax)
{
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4)=arg0;WORD_AT(ESP0+8)=arg1;
    g_eax=eax;call_count=0;originals=0;
    fn(orig);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"esp %+d after a hook\n",(int)(g_esp-ESP0));esp_errors++; }
    if(originals!=1) { fprintf(stderr,"original ran %d times\n",originals);failures++; }
    g_esp=ESP0;
}
static void rows(uint32_t ret) { hook(pc_mapmaker_skymusic_rows,plain_original,ret,ED+0x210u,0,0x55u); }
static void compile(uint32_t c) { hook(pc_mapmaker_skymusic_compile,rebuild_original,0x00313B01u,c,3u,0); }
static void load(uint32_t c) { hook(pc_mapmaker_skymusic_load,load_original,0x0032B4A4u,c,2u,0); }
static void mount(void) { hook(pc_mapmaker_skymusic_mount,plain_original,0x001CDB8Au,0,0,0); }
static void level(void) { hook(pc_mapmaker_skymusic_level,level_original,0x0006C963u,0,0,0); }
static void wx_load(void) { hook(pc_mapmaker_skymusic_weather_load,weather_load_original,0x001CDCF0u,0,0,0); }
static void wx_start(void) { hook(pc_mapmaker_skymusic_weather_start,weather_start_original,0x001CDDB5u,0,0,0); }
static uint32_t music(uint32_t ret,uint32_t eax) { hook(pc_mapmaker_skymusic_music,plain_original,ret,0x1A0u,0,eax);return eax_at_original; }
static const char *label(uint32_t p) { return (const char *)memory+p; }
static void unmount_all(void) { mounted_count=0;WORD_AT(0x006D7880u)=12u; }

#define F_SKY(k) ((uint32_t)(k)<<19)
#define F_MUSIC(k) ((uint32_t)(k)<<24)
#define F_WX(k) ((uint32_t)(k)<<29)

int main(void)
{
    uint32_t k,i,base;
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x007844A8u)=ED;WORD_AT(0x00784014u)=C;
    for(k=0;k<68;k++) WORD_AT(0x005149A8u+k*0x34u+4u)=0x700u+k;      /* sky models */
    for(k=0;k<4;k++) WORD_AT(0x004E62F8u+k*0x44u)=0x527u+k;          /* sandstorm models */
    WORD_AT(0x00791D4Cu)=0x00700000u;                                  /* resource table */
    for(k=0;k<0xA00u;k++) setH(0x008CA000u+k*2u,(int)k);               /* every model has a row */
    WORD_AT(0x006D7880u)=12u;

    /* ---- limits off: no rows; the rebuild still never stores an extra value ---- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);set_env("TSFP_MAPMAKER_POOLS",NULL);
    WORD_AT(0x0079094Cu)=102u;
    rows(0x00313768u);
    CHECK(call_count==0 && heap_calls==0);
    editor(60,35,4);make_container(C,0,0,0,0);
    compile(C);
    CHECK(H(INFO(C)+0x0E)==-1 && H(INFO(C)+0x10)==58 && B(INFO(C)+0x12)==0 && WORD_AT(INFO(C)+0x244)==0x00050000u);
    make_container(C,0x00050000u|F_SKY(2)|F_MUSIC(3)|F_WX(1),-1,58,0);
    editor(0,0,0);load(C);
    CHECK(WORD_AT(ED+0x210)==0xFFFFFFFFu && WORD_AT(ED+0x220)==58u && WORD_AT(ED+0x224)==0u);

    /* ---- limits on, editor rows ---- */
    set_env("TSFP_MAPMAKER_LIMITS","1");
    rows(0x00313768u);                                  /* music */
    CHECK(heap_calls==1 && call_count==28 && calls_at_original==28 && eax_at_original==0x55u);
    for(k=0;k<28 && k<(uint32_t)call_count;k++) CHECK(calls[k].f==sub_00071800 && calls[k].n==2 && calls[k].a[1]==58u+k);
    CHECK(!strcmp(label(calls[0].a[0]),"Front End Menu") && !strcmp(label(calls[27].a[0]),"Cutscene: Anya's Lab"));
    rows(0x00313843u);                                  /* sky */
    CHECK(call_count==17 && calls_at_original==17 && heap_calls==1);
    CHECK(calls[0].a[1]==5u && calls[1].a[1]==35u && calls[16].a[1]==57u && !strcmp(label(calls[1].a[0]),"Story: Castle"));
    for(k=0;k<17;k++) CHECK(calls[k].a[1]!=0u && (calls[k].a[1]<58u || calls[k].a[1]>67u));
    rows(0x00313906u);                                  /* weather */
    CHECK(call_count==1 && calls[0].a[1]==4u && !strcmp(label(calls[0].a[0]),"Sandstorm"));
    rows(0x0031CFD7u);                                  /* another list */
    CHECK(call_count==0);
    WORD_AT(0x0079094Cu)=101u;rows(0x00313768u);CHECK(call_count==0);
    WORD_AT(0x0079094Cu)=102u;

    /* ---- rebuild ---- */
    editor(58+27,35,4);make_container(C,0,0,0,0);
    rebuild_flags=0x00050000u|F_SKY(9)|F_MUSIC(9)|F_WX(3);  /* stale bits from an older rebuild */
    compile(C);
    CHECK(H(INFO(C)+0x0E)==-1 && H(INFO(C)+0x10)==58 && B(INFO(C)+0x12)==0);
    CHECK(WORD_AT(INFO(C)+0x244)==(0x00050000u|F_SKY(2)|F_MUSIC(28)|F_WX(1)));
    editor(12,62,2);compile(C);                         /* stock choices: untouched, bits cleared */
    CHECK(H(INFO(C)+0x0E)==12 && H(INFO(C)+0x10)==62 && B(INFO(C)+0x12)==2 && WORD_AT(INFO(C)+0x244)==0x00050000u);
    editor(-1,-1,0);compile(C);
    CHECK(H(INFO(C)+0x0E)==-1 && H(INFO(C)+0x10)==-1 && WORD_AT(INFO(C)+0x244)==0x00050000u);
    editor(200,5,9);compile(C);                         /* out of range music/weather: stock-safe */
    CHECK(H(INFO(C)+0x0E)==-1 && H(INFO(C)+0x10)==64 && B(INFO(C)+0x12)==0 && WORD_AT(INFO(C)+0x244)==(0x00050000u|F_SKY(1)));
    rebuild_flags=0x00038001u;editor(58,57,0);compile(C);
    CHECK(WORD_AT(INFO(C)+0x244)==(0x00038001u|F_SKY(17)|F_MUSIC(1)) && H(INFO(C)+0x10)==59);
    rebuild_flags=0x00050000u;
    editor(70,43,4);make_container(C2,0,0,0,0);make_container(C,0,0,0,0);
    compile(C2);                                        /* the rebuilt container is the argument */
    CHECK(WORD_AT(INFO(C2)+0x244)==(0x00050000u|F_SKY(6)|F_MUSIC(13)|F_WX(1)) && WORD_AT(INFO(C)+0x244)==0u);
    memset((char *)memory+C,0,16);editor(70,43,4);compile(C);   /* no info: nothing written */
    CHECK(WORD_AT(INFO(C)+0x244)==0x00050000u);

    /* ---- load and round trip ---- */
    make_container(C,0x00050000u|F_SKY(2)|F_MUSIC(28)|F_WX(1),-1,58,0);
    load(C);
    CHECK(WORD_AT(ED+0x210)==85u && WORD_AT(ED+0x220)==35u && WORD_AT(ED+0x224)==4u);
    compile(C);load(C);
    CHECK(WORD_AT(ED+0x210)==85u && WORD_AT(ED+0x220)==35u && WORD_AT(ED+0x224)==4u);
    make_container(C,0x00050000u|F_MUSIC(31)|F_SKY(31)|F_WX(3),7,60,1);   /* unknown rows: stock values */
    load(C);
    CHECK(WORD_AT(ED+0x210)==7u && WORD_AT(ED+0x220)==60u && WORD_AT(ED+0x224)==1u);
    make_container(C,F_SKY(2),-1,58,0);
    WORD_AT(0x0079094Cu)=100u;load(C);CHECK(WORD_AT(ED+0x220)==58u);
    WORD_AT(0x0079094Cu)=102u;

    /* ---- play: mounts ---- */
    WORD_AT(0x0079094Cu)=100u;
    make_container(C,F_SKY(2)|F_WX(1),-1,58,0);unmount_all();
    mount();
    CHECK(mapmaker_sky.extra_ready==1 && smx.sand_ready==1 && mounted_count==2);
    CHECK(!strcmp(mounted[0],"pak/arcade/l_59_ar.pak") && !strcmp(mounted[1],"pak/challenge/l_64_ch.pak"));
    for(i=0;i<(uint32_t)call_count;i++) if(calls[i].f==sub_000622D0) CHECK(calls[i].a[1]==0u && calls[i].a[2]==1u);
    mount();                                            /* a second level load: already mounted */
    CHECK(mapmaker_sky.extra_ready==1 && smx.sand_ready==1 && mounted_count==2);
    for(i=0;i<(uint32_t)call_count;i++) CHECK(calls[i].f!=sub_000622D0);
    make_container(C,F_SKY(6)|F_WX(1),-1,61,0);unmount_all();   /* intro sky + sandstorm: one pak */
    mount();
    CHECK(mapmaker_sky.extra_ready==1 && smx.sand_ready==1 && mounted_count==1);
    unmount_all();WORD_AT(0x006D7880u)=57u;mount();     /* keep 8 of 64 free */
    CHECK(mapmaker_sky.extra_ready==0 && smx.sand_ready==0 && mounted_count==0);
    WORD_AT(0x006D7880u)=56u;mount();
    CHECK(mapmaker_sky.extra_ready==1 && mounted_count==1);
    unmount_all();mount_fails=1;mount();
    CHECK(mapmaker_sky.extra_ready==0 && smx.sand_ready==0);
    mount_fails=0;
    WORD_AT(0x006D7C4Cu)=1u;mapmaker_sky.extra_ready=1;mount();  /* skipped load: untouched */
    CHECK(mapmaker_sky.extra_ready==1 && call_count==0);
    WORD_AT(0x006D7C4Cu)=0;
    make_container(C,0,-1,58,0);unmount_all();mount();
    CHECK(mapmaker_sky.extra_ready==0 && smx.sand_ready==0 && call_count==0);
    WORD_AT(0x0079094Cu)=20u;make_container(C,F_SKY(2)|F_WX(1),-1,58,0);mount();
    CHECK(mapmaker_sky.extra_ready==0 && call_count==0);
    set_env("TSFP_MAPMAKER_LIMITS",NULL);WORD_AT(0x0079094Cu)=100u;mount();
    CHECK(mapmaker_sky.extra_ready==0 && call_count==0);
    set_env("TSFP_MAPMAKER_LIMITS","1");

    /* ---- play: map start ---- */
    make_container(C,F_SKY(2),-1,58,0);unmount_all();mount();
    level();
    CHECK(seen_sky==35 && H(INFO(C)+0x10)==58);
    make_container(C,0,-1,58,0);level();                /* no extra sky */
    CHECK(seen_sky==58);
    make_container(C,F_SKY(2),-1,58,0);mapmaker_sky.extra_ready=0;level();
    CHECK(seen_sky==58);                                /* pak not mounted */
    mapmaker_sky.extra_ready=1;setH(0x008CA000u+(0x700u+35u)*2u,-1);level();
    CHECK(seen_sky==58 && H(INFO(C)+0x10)==58);         /* no resource row */
    setH(0x008CA000u+(0x700u+35u)*2u,1);
    WORD_AT(0x0079094Cu)=20u;level();CHECK(seen_sky==58);
    WORD_AT(0x0079094Cu)=100u;
    /* the cloud setup (pc_mapmaker_sky.inc) sees the extra sky only when ready */
    CHECK(mapmaker_sky_index(INFO(C))==35);
    mapmaker_sky.extra_ready=0;CHECK(mapmaker_sky_index(INFO(C))==58);

    /* ---- play: sandstorm ---- */
    make_container(C,F_WX(1),-1,58,0);unmount_all();mount();
    wx_load();
    CHECK(WORD_AT(0x0073D9ACu)==1u && smx.sand_on==1 && call_count==4);
    for(k=0;k<4 && k<(uint32_t)call_count;k++) CHECK(calls[k].f==sub_000A5E40 && calls[k].a[0]==0x527u+k);
    WORD_AT(0x007AC824u)=0x00400000u;WORD_AT(0x007AC874u)=0x00410000u;
    wx_start();
    CHECK(WORD_AT(0x007AC804u)==0x00507898u && WORD_AT(0x007AC800u)==1u && WORD_AT(0x007AC808u)==0u);
    WORD_AT(0x007AC824u)=0;wx_start();                  /* buffers missing: default list kept */
    CHECK(WORD_AT(0x007AC804u)==0x004E63F8u);
    WORD_AT(0x007AC824u)=0x00400000u;
    make_container(C,0,-1,58,2);mount();wx_load();      /* rain map: nothing */
    CHECK(WORD_AT(0x0073D9ACu)==0u && call_count==0);
    wx_start();CHECK(WORD_AT(0x007AC804u)==0x004E63F8u);
    make_container(C,F_WX(1),-1,58,0);unmount_all();mount_fails=1;mount();mount_fails=0;
    wx_load();CHECK(WORD_AT(0x0073D9ACu)==0u && call_count==0);   /* no pak: no sandstorm */
    unmount_all();mount();setH(0x008CA000u+0x529u*2u,-1);wx_load();
    CHECK(WORD_AT(0x0073D9ACu)==0u && call_count==0);   /* a model without a row */
    setH(0x008CA000u+0x529u*2u,0x529);
    WORD_AT(0x0079094Cu)=20u;mount();wx_load();wx_start(); /* a story level afterwards */
    CHECK(WORD_AT(0x0073D9ACu)==0u && WORD_AT(0x007AC804u)==0x004E63F8u);
    WORD_AT(0x0079094Cu)=100u;

    /* ---- play: music ---- */
    make_container(C,F_MUSIC(1),-1,58,0);
    CHECK(music(0x00045FBCu,0xFFFFFFFFu)==0x18u);
    make_container(C,F_MUSIC(9),-1,58,0);
    CHECK(music(0x00045FBCu,0xFFFFFFFFu)==0x4Bu);
    CHECK(music(0x00045FBCu,0x3Cu)==0x3Cu);             /* a game setup override */
    CHECK(music(0x00045F2Cu,0xFFFFFFFFu)==0xFFFFFFFFu); /* another caller */
    make_container(C,F_MUSIC(31),-1,58,0);
    CHECK(music(0x00045FBCu,0xFFFFFFFFu)==0xFFFFFFFFu);
    make_container(C,F_MUSIC(1),-1,58,0);
    WORD_AT(0x0079094Cu)=20u;CHECK(music(0x00045FBCu,0xFFFFFFFFu)==0xFFFFFFFFu);
    WORD_AT(0x0079094Cu)=100u;
    set_env("TSFP_MAPMAKER_LIMITS",NULL);CHECK(music(0x00045FBCu,0xFFFFFFFFu)==0xFFFFFFFFu);
    set_env("TSFP_MAPMAKER_LIMITS","1");

    /* ---- label allocation failure: no rows ---- */
    smx.labels=0;heap_fail=1;WORD_AT(0x0079094Cu)=102u;
    base=(uint32_t)heap_calls;rows(0x00313768u);
    CHECK(call_count==0 && heap_calls==(int)base+1);
    heap_fail=0;

    if(failures || esp_errors) { fprintf(stderr,"%d failures, %d esp errors\n",failures,esp_errors);return 1; }
    puts("mapmaker skymusic ok");
    return 0;
}
