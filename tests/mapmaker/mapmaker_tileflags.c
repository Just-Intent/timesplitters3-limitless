/* Asset-free contract for src/interface/pc_mapmaker_tileflags.inc (MapMaker
 * right-click tile roof menu) and the flagged-tile hover label in
 * pc_mapmaker_roof.inc: a right click on a placed tile in Tiles mode with
 * nothing held is taken (no Back) and opens the menu on the next Tiles input
 * frame through sub_002FB230 with a heap descriptor {sub_00319300,
 * sub_003192E0}; the build lays out one "Roof" choice (Normal / No roof /
 * Open air) bound to a heap word plus the stock Done row; the tick applies the
 * choice as a roof mark to the tile (and to every selected tile when it is
 * selected); other menus, modes and scenes are untouched; every hook pops only
 * its return address. */
#define TSFP_MAPMAKER_ROOF_TEST 1
#define TSFP_MAPMAKER_TILEFLAGS_TEST 1
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

/* distinct bodies: identical ones may be folded */
void sub_0003DFA0(void) { g_ecx=1; }
void sub_002FB230(void) { g_ecx=2; }
void sub_00075B70(void) { g_ecx=3; }
void sub_00071B10(void) { g_ecx=4; }
void sub_00071C50(void) { g_ecx=5; }
void sub_00079DE0(void) { g_ecx=6; }
void sub_0007D4A0(void) { g_ecx=7; }
void sub_00071800(void) { g_ecx=8; }
void sub_00078590(void) { g_ecx=9; }
void sub_000783D0(void) { g_ecx=10; }
void sub_00079D60(void) { g_ecx=11; }
void sub_00075E70(void) { g_ecx=12; }
void sub_00077320(void) { g_ecx=13; }
void sub_0003D7B0(void) { g_ecx=14; }

#define DONE_STRING 0x00444000u
static unsigned nopen, nframe, nrows, nopts, nbind, nend, ndone, nlast, npos;
static uint32_t open_arg, frame_args[5], row_text, row_args[5], opt_text[4], opt_value[4], bind_arg, done_args[6], pos_args[2];
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[12]={0};
    int i;
    va_start(a,n);
    for(i=0;i<n && i<12;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_002FB230) { nopen++;open_arg=x[0];return 0; }
    if(f==sub_00075B70) { nframe++;memcpy(frame_args,x,sizeof frame_args);return 0; }
    if(f==sub_00079DE0) { nrows++;row_text=x[0];memcpy(row_args,x,sizeof row_args);return 0; }
    if(f==sub_00071800) { if(nopts<4u) { opt_text[nopts]=x[0];opt_value[nopts]=x[1]; } nopts++;return 0; }
    if(f==sub_00078590) { nbind++;bind_arg=x[0];return 0; }
    if(f==sub_000783D0) { nend++;return 0; }
    if(f==sub_00079D60) { ndone++;memcpy(done_args,x,sizeof done_args);return 0; }
    if(f==sub_00075E70) { nlast++;return 0; }
    if(f==sub_00077320) { npos++;pos_args[0]=x[0];pos_args[1]=x[1];return 0; }
    if(f==sub_0003D7B0) return x[0]==0xB5Eu?DONE_STRING:0u;
    return 0;
}
static uint32_t heap_result=0x00300000u;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment)
{
    uint32_t p=heap_result;
    (void)alignment;
    if(p) { memset((char *)memory+p,0xCD,bytes);heap_result+=0x1000u; }
    return p;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_roof.inc"
#include "pc_mapmaker_tileflags.inc"

static int failures, esp_errors, originals;
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

#define ED 0x00100000u            /* editor object [0x007844A8] */
#define PIECES 0x00110000u        /* editor pieces, 0x48 bytes */
#define MENU 0x00120000u          /* an options menu */
#define NAME 0x00130000u          /* a tile name */
#define P(i) (PIECES+(uint32_t)(i)*0x48u)
static void fake(void) { originals++;g_esp+=4u; }
/* Calls hook fn with return address ret and two args; checks esp and how
 * often the original ran. */
static void call(void (*fn)(void (*)(void)),uint32_t ret,uint32_t a1,uint32_t a2,uint32_t a3,int expect_orig,int line)
{
    int before=originals;
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4u)=a1;WORD_AT(ESP0+8u)=a2;WORD_AT(ESP0+12u)=a3;
    fn(fake);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"%s:%d: esp %+d\n",__FILE__,line,(int)(g_esp-ESP0));esp_errors++; }
    if(expect_orig>=0 && originals-before!=expect_orig) { fprintf(stderr,"%s:%d: original ran %d times\n",__FILE__,line,originals-before);failures++; }
}
#define CALL(fn,ret,a1,a2,a3,n) call(fn,ret,a1,a2,a3,n,__LINE__)
static void editor(void)
{
    unsigned i;
    memset((char *)memory+ED,0,0x3000u);
    WORD_AT(0x007844A8u)=ED;WORD_AT(0x0079094Cu)=102u;
    for(i=0;i<4u;i++) {
        memset((char *)memory+P(i),0,0x48u);
        WORD_AT(P(i)+0x10u)=i==0?8u:20u+i;                     /* tile ids: 8, 21, 22, 23 */
        WORD_AT(P(i)+0x40u)=i<3u?P(i+1u):0u;
    }
    WORD_AT(0x0078400Cu)=P(0);
    WORD_AT(ED+0x254u)=0;WORD_AT(ED+0x2F18u)=P(1);WORD_AT(ED+0x2F7Cu)=0;WORD_AT(ED+0x25Cu)=0;
}

int main(void)
{
    uint32_t desc;
    g_xbox_mem_offset=(uintptr_t)memory;
    FLOAT_AT(0x0049F7E8u)=300.0f;WORD_AT(0x007BB1E8u)=0x00555000u;
    set_env("TSFP_MAPMAKER_POOLS",NULL);

    /* ---- off: nothing taken, every hook is the original ----------------- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    editor();
    CHECK(!pc_mapmaker_tileflags_click());
    CALL(pc_mapmaker_tileflags_tiles,0x003063DEu,0x00700000u,0,0,1);
    CHECK(nopen==0);
    set_env("TSFP_MAPMAKER_LIMITS","1");

    /* ---- the click: Tiles mode, nothing held, a piece under the pointer -- */
    WORD_AT(ED+0x254u)=1u;CHECK(!pc_mapmaker_tileflags_click());        /* Objects mode */
    WORD_AT(ED+0x254u)=0;WORD_AT(ED+0x2F7Cu)=P(3);CHECK(!pc_mapmaker_tileflags_click());   /* holding */
    WORD_AT(ED+0x2F7Cu)=0;WORD_AT(ED+0x2F18u)=0;CHECK(!pc_mapmaker_tileflags_click());     /* empty cell */
    WORD_AT(ED+0x2F18u)=P(1);WORD_AT(ED+0x25Cu)=0x4000000u;CHECK(!pc_mapmaker_tileflags_click());   /* a menu open */
    WORD_AT(ED+0x25Cu)=0;WORD_AT(0x0079094Cu)=100u;CHECK(!pc_mapmaker_tileflags_click());  /* play */
    WORD_AT(0x0079094Cu)=102u;
    CHECK(!tf.request);
    /* the piece moved away before the next tile frame: nothing opens */
    CHECK(pc_mapmaker_tileflags_click() && tf.request==P(1));
    WORD_AT(ED+0x2F18u)=P(2);
    CALL(pc_mapmaker_tileflags_tiles,0x003063DEu,0x00700000u,0,0,1);
    CHECK(nopen==0 && !tf.request);
    /* the menu opens on the next tile frame, without that frame's tile input */
    WORD_AT(ED+0x2F18u)=P(1);
    CHECK(pc_mapmaker_tileflags_click());
    CALL(pc_mapmaker_tileflags_tiles,0x003063DEu,0x00700000u,0,0,0);
    CHECK(nopen==1 && (desc=open_arg)!=0 && !tf.request && tf.piece==P(1) && tf.tile==21u && !tf.group);
    CHECK(WORD_AT(desc)==0x00319300u && WORD_AT(desc+4u)==0x003192E0u && !WORD_AT(desc+8u) && !WORD_AT(desc+12u));
    CHECK(WORD_AT(tf.value)==0);
    CALL(pc_mapmaker_tileflags_tiles,0x003063DEu,0x00700000u,0,0,1);       /* no request: stock */
    CHECK(nopen==1);

    /* ---- build: the stock frame, one Roof choice, the Done row ---------- */
    memset((char *)memory+MENU,0,0x300u);
    WORD_AT(MENU+4u)=0x0052DECCu;                                          /* the pickup options */
    CALL(pc_mapmaker_tileflags_build,0x00079900u,MENU,0,0,1);
    CHECK(nrows==0 && WORD_AT(MENU+0x218u)==0);
    WORD_AT(MENU+4u)=desc;FLOAT_AT(MENU+0x68u)=40.0f;
    CALL(pc_mapmaker_tileflags_build,0x00079900u,MENU,0,0,0);
    CHECK(nframe==1 && frame_args[0]==0x8400u && frame_args[2]==0x42480000u && frame_args[4]==0x002FB3C0u);
    CHECK(WORD_AT(MENU+0x218u)==1u);
    CHECK(nrows==1 && !strcmp((const char *)((uintptr_t)row_text+g_xbox_mem_offset),"Roof"));
    CHECK(row_args[1]==1u && row_args[2]==0x08000800u);
    CHECK(nopts==3 && opt_value[0]==0 && opt_value[1]==1u && opt_value[2]==2u);
    CHECK(!strcmp((const char *)((uintptr_t)opt_text[0]+g_xbox_mem_offset),"Normal"));
    CHECK(!strcmp((const char *)((uintptr_t)opt_text[1]+g_xbox_mem_offset),"No roof"));
    CHECK(!strcmp((const char *)((uintptr_t)opt_text[2]+g_xbox_mem_offset),"Open air"));
    CHECK(nbind==1 && bind_arg==tf.value && nend==1);
    CHECK(ndone==1 && done_args[0]==DONE_STRING && done_args[1]==0xFFFFFFFFu && done_args[3]==0x40000000u && done_args[4]==0x002FB3C0u);
    CHECK(nlast==1 && npos==1 && pos_args[0]==0x00555000u && *(float *)&pos_args[1]==260.0f);

    /* ---- tick: the choice becomes the tile's roof mark ------------------- */
    CALL(pc_mapmaker_tileflags_tick,0x00079920u,MENU,0,0,1);
    CHECK(!mapmaker_roof_mode(P(1)));                                      /* unchanged */
    WORD_AT(tf.value)=2u;
    WORD_AT(MENU+4u)=0x0052DECCu;
    CALL(pc_mapmaker_tileflags_tick,0x00079920u,MENU,0,0,1);               /* another menu */
    CHECK(!mapmaker_roof_mode(P(1)));
    WORD_AT(MENU+4u)=desc;
    CALL(pc_mapmaker_tileflags_tick,0x00079920u,MENU,0,0,1);
    CHECK(mapmaker_roof_mode(P(1))==MAPMAKER_ROOF_OPEN_AIR && !mapmaker_roof_mode(P(2)) && tf.applied==2u);
    WORD_AT(tf.value)=1u;
    CALL(pc_mapmaker_tileflags_tick,0x00079920u,MENU,0,0,1);
    CHECK(mapmaker_roof_mode(P(1))==MAPMAKER_ROOF_OPEN_ROOF);
    WORD_AT(tf.value)=7u;                                                  /* not a choice */
    CALL(pc_mapmaker_tileflags_tick,0x00079920u,MENU,0,0,1);
    CHECK(mapmaker_roof_mode(P(1))==MAPMAKER_ROOF_OPEN_ROOF);
    WORD_AT(tf.value)=0;WORD_AT(P(1)+0x10u)=30u;                           /* the piece was reused */
    CALL(pc_mapmaker_tileflags_tick,0x00079920u,MENU,0,0,1);
    CHECK(mapmaker_roof_mode(P(1))==MAPMAKER_ROOF_OPEN_ROOF);
    WORD_AT(P(1)+0x10u)=21u;
    CALL(pc_mapmaker_tileflags_tick,0x00079920u,MENU,0,0,1);
    CHECK(!mapmaker_roof_mode(P(1)) && !mapmaker_roof.marked);             /* Normal again */

    /* ---- a selected tile: every selected tile gets the choice ------------ */
    WORD_AT(P(1)+0x14u)=2u;WORD_AT(P(3)+0x14u)=2u;WORD_AT(P(2)+0x14u)=1u;
    CHECK(pc_mapmaker_tileflags_click());
    CALL(pc_mapmaker_tileflags_tiles,0x003063DEu,0x00700000u,0,0,0);
    CHECK(nopen==2 && open_arg==desc && tf.group);
    WORD_AT(tf.value)=2u;
    CALL(pc_mapmaker_tileflags_tick,0x00079920u,MENU,0,0,1);
    CHECK(mapmaker_roof_mode(P(1))==2u && mapmaker_roof_mode(P(3))==2u && !mapmaker_roof_mode(P(2)) && !mapmaker_roof_mode(P(0)));
    /* the menu starts on the tile's current flag */
    CHECK(pc_mapmaker_tileflags_click());
    CALL(pc_mapmaker_tileflags_tiles,0x003063DEu,0x00700000u,0,0,0);
    CHECK(WORD_AT(tf.value)==2u && tf.applied==2u);

    /* ---- hover label: any flagged tile names its flag -------------------- */
    strcpy((char *)memory+NAME,"Big Room");
    WORD_AT(ED+0x2F18u)=P(3);
    g_esp=ESP0;WORD_AT(ESP0)=0x00311729u;WORD_AT(ESP0+4u)=0x0049F7C0u;WORD_AT(ESP0+8u)=12u;WORD_AT(ESP0+12u)=NAME;
    pc_mapmaker_roof_format(fake);
    CHECK(WORD_AT(ESP0+8u)==12u && WORD_AT(ESP0+12u)!=NAME &&
          !strcmp((const char *)((uintptr_t)WORD_AT(ESP0+12u)+g_xbox_mem_offset),"Big Room (Open Air)"));
    WORD_AT(ED+0x2F18u)=P(2);                                              /* not flagged */
    g_esp=ESP0;WORD_AT(ESP0)=0x00311729u;WORD_AT(ESP0+4u)=0x0049F7C0u;WORD_AT(ESP0+8u)=12u;WORD_AT(ESP0+12u)=NAME;
    pc_mapmaker_roof_format(fake);
    CHECK(WORD_AT(ESP0+12u)==NAME);
    WORD_AT(ED+0x2F18u)=P(3);                                              /* another sprintf caller */
    g_esp=ESP0;WORD_AT(ESP0)=0x00311000u;WORD_AT(ESP0+4u)=0x0049F7C0u;WORD_AT(ESP0+8u)=12u;WORD_AT(ESP0+12u)=NAME;
    pc_mapmaker_roof_format(fake);
    CHECK(WORD_AT(ESP0+12u)==NAME);

    if(esp_errors) failures+=esp_errors;
    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    puts("mapmaker tileflags ok");
    return 0;
}
