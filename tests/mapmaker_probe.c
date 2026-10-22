/* Asset-free contract for src/pc_mapmaker_probe.inc: inert by default,
 * read-only when enabled, attributes bars to the meter scope, and bounds output. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x800000/4],g_esp,g_ecx;
static uintptr_t g_xbox_mem_offset;
static unsigned long long tick=1000;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
static unsigned long long GetTickCount64(void) { return tick; }
#define TSFP_MAPMAKER_PROBE_TEST
#include "pc_mapmaker_probe.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

static const char *log_path;
static void set_probe(const char *flags)
{
#ifdef _WIN32
    _putenv_s("TSFP_MAPMAKER_PROBE",flags?flags:"");
    _putenv_s("TSFP_MAPMAKER_PROBE_LOG",log_path);
#else
    if(flags)setenv("TSFP_MAPMAKER_PROBE",flags,1); else unsetenv("TSFP_MAPMAKER_PROBE");
    setenv("TSFP_MAPMAKER_PROBE_LOG",log_path,1);
#endif
    mapmaker_probe_reset();
    remove(log_path);
}
static char log_text[8u*1024u*1024u];
static size_t read_log(void)
{
    FILE *f;size_t n;
    if(mapmaker_probe.file)fflush(mapmaker_probe.file);
    f=fopen(log_path,"rb");
    if(!f) { log_text[0]=0;return 0; }
    n=fread(log_text,1,sizeof log_text-1,f);fclose(f);log_text[n]=0;
    return n;
}
static size_t count(const char *needle)
{
    size_t n=0;const char *p=log_text;
    while((p=strstr(p,needle))) { n++;p+=strlen(needle); }
    return n;
}

static uint64_t checksum(void)
{
    uint64_t h=1469598103934665603ull;size_t i;
    /* The fake guest's own stack pushes are checked by the originals. */
    for(i=0;i<sizeof memory/4;i++) {
        if(i>=0x006F0000u/4 && i<0x00700000u/4)continue;
        h^=memory[i];h*=1099511628211ull;
    }
    return h;
}

/* Fake guest: the meter painter pushes nine bar arguments and calls the
 * existing sub_000814B0 wrapper hook, which forwards to the original bar. */
static float fraction=.25f;
static unsigned bar_originals,meter_originals;
static uint32_t bar_esp_seen,bar_fraction_seen;
static void push(uint32_t w) { g_esp-=4;WORD_AT(g_esp)=w; }
static uint32_t bar_expected[10];
static void bar_original(void)
{
    unsigned i;
    bar_originals++;bar_esp_seen=g_esp;bar_fraction_seen=WORD_AT(g_esp+4);
    for(i=0;i<10;i++)CHECK(WORD_AT(g_esp+i*4u)==bar_expected[i]);
}
static void draw_bar(uint32_t ret,float value)
{
    uint32_t bits,sp=g_esp;memcpy(&bits,&value,4);
    push(0);push(0xFF202020u);push(0xFF404040u);push(0xFF808080u);
    push(8);push(100);push(200);push(40);push(bits);push(ret);
    memcpy(bar_expected,(const void *)((uintptr_t)g_esp+g_xbox_mem_offset),sizeof bar_expected);
    mapmaker_probe_bar();bar_original();
    g_esp=sp;
}
static void meter_original(void)
{
    CHECK(WORD_AT(g_esp)==0x00401234u && WORD_AT(g_esp+4)==0x00310000u &&
          WORD_AT(g_esp+8)==0x00200000u && WORD_AT(g_esp+12)==0x00123456u &&
          WORD_AT(g_esp+16)==0x00000007u && g_ecx==0x00200000u);
    meter_originals++;
    draw_bar(0x0030C2A0u,fraction);
}
static void call_meter(void)
{
    uint32_t sp=g_esp,ecx=g_ecx;
    push(0x00000007u);push(0x00123456u);push(0x00200000u);push(0x00310000u);
    push(0x00401234u); /* return address */
    pc_display_mapmaker_meter(meter_original);
    CHECK(g_esp==sp-20 && g_ecx==ecx);
    g_esp=sp;
}

int main(int argc,char **argv)
{
    uint64_t before;size_t n;unsigned i;
    if(argc<2) { fprintf(stderr,"usage: mapmaker_probe <log>\n");return 2; }
    log_path=argv[1];
    g_xbox_mem_offset=(uintptr_t)memory;
    g_esp=0x00700000u;g_ecx=0x00200000u;
    for(i=0;i<16;i++)WORD_AT(0x00200000u+i*4u)=0xA0000000u+i;
    WORD_AT(0x0079094Cu)=0;

    /* Disabled: original runs once, no log, no state changes. */
    set_probe(NULL);
    before=checksum();
    call_meter();
    CHECK(meter_originals==1 && bar_originals==1);
    CHECK(bar_fraction_seen==0x3E800000u);
    CHECK(checksum()==before);
    CHECK(read_log()==0);
    CHECK(mapmaker_probe.flags==0 && mapmaker_probe.file==NULL);

    /* Invalid values stay disabled. */
    set_probe("abc");call_meter();CHECK(mapmaker_probe.flags==0 && read_log()==0);
    set_probe("0");call_meter();CHECK(mapmaker_probe.flags==0 && read_log()==0);

    /* Meter only: record caller, args and the attributed bar; read-only. */
    set_probe("1");
    before=checksum();
    call_meter();
    CHECK(checksum()==before);
    n=read_log();
    CHECK(n>0);
    CHECK(strstr(log_text,"[mapmaker-probe] v2 flags=1")!=NULL);
    CHECK(strstr(log_text,"caller=0x00401234 scene=0 ecx=0x00200000 args=0x00310000 0x00200000 0x00123456 0x00000007")!=NULL);
    CHECK(strstr(log_text,"bar[0] ret=0x0030C2A0 fraction=0.250000 (0x3E800000) rect=40,200,100,8")!=NULL);
    CHECK(strstr(log_text,"bars=1")!=NULL);
    CHECK(strstr(log_text,"dump ")==NULL);
    CHECK(count("] t=")==1);

    /* Identical frames collapse; a change inside 250 ms waits; then emits
     * the latest state with the repeat count. */
    for(i=0;i<10;i++)call_meter();
    read_log();CHECK(count("] t=")==1);
    fraction=.5f;tick+=100;call_meter();
    read_log();CHECK(count("] t=")==1);
    tick+=200;call_meter();
    read_log();
    CHECK(count("] t=")==2);
    CHECK(strstr(log_text,"repeats=11\n")!=NULL);
    CHECK(strstr(log_text,"fraction=0.500000 (0x3F000000)")!=NULL);

    /* A bar outside the meter scope is not attributed without flag 4. */
    WORD_AT(0x0079094Cu)=100;
    draw_bar(0x00123400u,.75f);
    read_log();CHECK(strstr(log_text,"other-bar")==NULL);

    /* Snapshots of pointer arguments. */
    set_probe("3");
    before=checksum();
    call_meter();
    CHECK(checksum()==before);
    read_log();
    CHECK(strstr(log_text,"dump ecx@0x00200000: A0000000 A0000001")!=NULL);
    CHECK(strstr(log_text,"dump arg1@0x00200000:")!=NULL);
    CHECK(strstr(log_text,"dump arg0@")!=NULL); /* 0x00310000 is in fixture RAM */
    CHECK(strstr(log_text,"dump arg3@")==NULL); /* 7 is not a pointer */

    /* Other bars in scene 100 only. */
    set_probe("5");
    WORD_AT(0x0079094Cu)=100;
    before=checksum();
    draw_bar(0x00123400u,.75f);
    draw_bar(0x00123400u,.75f);
    CHECK(checksum()==before);
    read_log();
    CHECK(count("other-bar")==1);
    CHECK(strstr(log_text,"other-bar scene=100 ret=0x00123400 fraction=0.750000")!=NULL);
    WORD_AT(0x0079094Cu)=5;
    draw_bar(0x00999900u,.1f);
    read_log();CHECK(strstr(log_text,"0x00999900")==NULL);
    WORD_AT(0x0079094Cu)=0;

    /* Flag 8: menu layouts and clicks in scene 100 only, read-only. */
    set_probe("8");
    WORD_AT(0x00300000u+4)=0x004E1234u;WORD_AT(0x00300000u+0x1b0)=3;
    FLOAT_AT(0x00300000u+0xe4)=10.f;FLOAT_AT(0x00300000u+0xe8)=20.f;
    FLOAT_AT(0x00300000u+0xec)=300.f;FLOAT_AT(0x00300000u+0xf0)=60.f;
    WORD_AT(0x00300000u+0x178)=0x00301000u;
    WORD_AT(0x00301000u)=4;WORD_AT(0x00301000u+4)=0x00302000u;WORD_AT(0x00301000u+0x38)=0x0007DA20u;
    FLOAT_AT(0x00301000u+0x78)=12.f;FLOAT_AT(0x00301000u+0x7c)=22.f;
    FLOAT_AT(0x00301000u+0x80)=40.f;FLOAT_AT(0x00301000u+0x84)=50.f;
    WORD_AT(0x00302000u)=9;WORD_AT(0x00302000u+4)=0;WORD_AT(0x00302000u+0x14)=3;
    WORD_AT(0x0079094Cu)=5;
    mapmaker_probe_menu(0x00300000u,0x00012345u);
    read_log();CHECK(strstr(log_text,"menu-layout")==NULL);
    WORD_AT(0x0079094Cu)=100;
    before=checksum();
    mapmaker_probe_menu(0x00300000u,0x00012345u);
    mapmaker_probe_menu(0x00300000u,0x00012345u);
    call_meter(); /* flag 8 alone does not log the meter */
    mapmaker_probe_click(0x00300000u,25.5f,30.f,0x00301000u,1u); /* hover only */
    mapmaker_probe_click(0x00300000u,25.5f,30.f,0x00301000u,2u);
    mapmaker_probe_click(0x00300000u,150.f,40.f,0u,2u);
    CHECK(checksum()==before);
    read_log();
    CHECK(strstr(log_text,"[mapmaker-probe] v2 flags=8")!=NULL);
    CHECK(count("menu-layout")==1);
    CHECK(strstr(log_text,"menu=0x00300000 caller=0x00012345 descriptor=0x004E1234")!=NULL);
    CHECK(strstr(log_text,"state(+1B0)=3")!=NULL && strstr(log_text,"rect=10.0,20.0,300.0,60.0")!=NULL);
    CHECK(strstr(log_text,"item[0]=0x00301000 type=4 id=0 flags=00000000 callback=0x0007DA20")!=NULL);
    CHECK(strstr(log_text,"rect=12.0,22.0,40.0,50.0")!=NULL);
    CHECK(strstr(log_text,"item[1]=0x00302000 type=9 id=3")!=NULL);
    CHECK(strstr(log_text,"   raw: 00000004 00302000")!=NULL);
    CHECK(strstr(log_text,"items=2\n")!=NULL);
    CHECK(strstr(log_text,"meter call")==NULL);
    CHECK(count("] t=")==3); /* one layout, two clicks */
    CHECK(strstr(log_text,"click menu=0x00300000 pointer=25.5,30.0 hit=0x00301000 type=4")!=NULL);
    CHECK(strstr(log_text,"click menu=0x00300000 pointer=150.0,40.0 hit=0x00000000 type=0")!=NULL);
    /* A changed layout is logged again once its interval has passed. */
    FLOAT_AT(0x00301000u+0x80)=41.f;
    mapmaker_probe_menu(0x00300000u,0x00012345u);
    read_log();CHECK(count("menu-layout")==1);
    tick+=MAPMAKER_PROBE_MENU_INTERVAL;
    mapmaker_probe_menu(0x00300000u,0x00012345u);
    read_log();CHECK(count("menu-layout")==2 && strstr(log_text,"menu-layout repeats=2")!=NULL);
    CHECK(strchr(log_text,'\r')==NULL); /* binary log on every host */
    WORD_AT(0x0079094Cu)=0;

    /* Output is capped. */
    set_probe("1");
    for(i=0;i<40000 && !mapmaker_probe.capped;i++) {
        fraction=(float)(i%1000)/1000.f;tick+=MAPMAKER_PROBE_INTERVAL;call_meter();
    }
    n=read_log();
    CHECK(mapmaker_probe.capped);
    CHECK(n<=MAPMAKER_PROBE_CAP+128);
    CHECK(strstr(log_text,"log cap reached")!=NULL);
    mapmaker_probe_reset();

    if(failures)return 1;
    puts("mapmaker probe ok");
    return 0;
}
