/* Asset-free contract for src/interface/pc_mapmaker_tilesets.inc (the hidden
 * Abstract tileset, index 0): the Map Settings row is added only with
 * TSFP_MAPMAKER_LIMITS=1, in the editor, after the stock Virtual row of
 * sub_003133E0, with value 0 and the stock text; in play an Abstract map
 * mounts pak/arcade/l_102.pak whole on the level heap before its tileset pak
 * (any switch state), from a path below the guest stack, once; when that is
 * impossible the map plays as Lab (getter argument, header word +0xA and
 * [0x00784020]); every hook leaves esp as the original would and other
 * callers, rows and tilesets untouched. */
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
void sub_0003D7B0(void) { g_ecx=1; }  /* distinct bodies: identical ones may be folded */
void sub_00071800(void) { g_ecx=2; }
void sub_000622D0(void) { g_ecx=3; }
void sub_00062260(void) { g_ecx=4; }

static int failures, esp_errors;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

#define ESP0 0x00600000u
#define TEXT_BASE 0x00400000u
#define PAK_PATH "pak/arcade/l_102.pak"
#define TEX_PATH "pak/arcademem/l_102m.pak"
static int rows, mounts, lookups, texts, mount_fails, tex_fails, already, tex_already;
static uint32_t row_text[8], row_value[8], mount_args[3], tex_args[3], call_esp_min;
static int string_below_frame, tex_below_frame, mount_order_bad;
static uint32_t heap_next=0x00700000u;
static int heap_fail;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment)
{
    uint32_t p=heap_next;
    (void)alignment;
    if(heap_fail) return 0;
    heap_next+=(bytes+15u)&~15u;
    return p;
}
static void note_esp(void) { if(g_esp<call_esp_min) call_esp_min=g_esp; }
/* A guest-side model of the calls the hooks make. */
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t v[6]={0},r=0;
    int i;
    va_start(a,n);
    for(i=0;i<n && i<6;i++) v[i]=va_arg(a,uint32_t);
    va_end(a);
    note_esp();
    if(f==sub_0003D7B0) { texts++;r=v[0]==0xAAFu?TEXT_BASE+v[0]:0u; }
    else if(f==sub_00071800) {
        if(rows<8) { row_text[rows]=v[0];row_value[rows]=v[1]; }
        rows++;
    } else if(f==sub_00062260) {
        int tex=!strcmp((const char *)memory+v[0],TEX_PATH);
        lookups++;
        r=(tex?tex_already:already)?3u:0xFFFFFFFFu;
        if(!tex && strcmp((const char *)memory+v[0],PAK_PATH)) { fprintf(stderr,"lookup of a wrong path\n");failures++; }
    } else if(f==sub_000622D0) {
        int tex=!strcmp((const char *)memory+v[0],TEX_PATH);
        mounts++;
        if(tex) {
            tex_args[0]=v[0];tex_args[1]=v[1];tex_args[2]=v[2];
            tex_below_frame=v[0]+sizeof TEX_PATH<=ESP0 && g_esp<=v[0];
            if(!already) mount_order_bad=1;            /* textures only after the tiles' pak */
            if(mount_fails || tex_fails) r=0xFFFFFFFFu; else { r=WORD_AT(0x006D7880u);WORD_AT(0x006D7880u)++;tex_already=1; }
        } else {
            mount_args[0]=v[0];mount_args[1]=v[1];mount_args[2]=v[2];
            if(strcmp((const char *)memory+v[0],PAK_PATH)) { fprintf(stderr,"mount of a wrong path\n");failures++; }
            string_below_frame=v[0]+sizeof PAK_PATH<=ESP0 && g_esp<=v[0];
            if(mount_fails) r=0xFFFFFFFFu; else { r=WORD_AT(0x006D7880u);WORD_AT(0x006D7880u)++;already=1; }
        }
    } else { fprintf(stderr,"unexpected guest call\n");failures++; }
    return r;                                         /* the real guest_call restores every register */
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_tilesets.inc"

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
#define C 0x00200000u           /* map container [0x00784014] */
#define INFO (C+16u)            /* info chunk data */
static void make_container(uint32_t tileset)
{
    memset((char *)memory+C,0,0x1000);
    WORD_AT(C)=0x00C77667u;WORD_AT(C+4)=0x12Du;
    WORD_AT(C+8)=0x00C77668u;WORD_AT(C+12)=0x3DCu;
    WORD_AT(INFO+0x08)=0x5A5Au|tileset<<16;
    WORD_AT(INFO+0x3DC)=0x00C7766Au;
    WORD_AT(0x00784014u)=C;WORD_AT(0x00784020u)=tileset;
}
static int originals;
static uint32_t seen_arg, seen_value;
static void row_original(void) { originals++;seen_value=WORD_AT(g_esp+8u);g_eax=0x00350000u;g_esp+=4u; }
static void path_original(void) { originals++;seen_arg=WORD_AT(g_esp+4u);g_eax=0x0049FA94u;g_esp+=4u; }

static void hook(void (*fn)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t arg0,uint32_t arg1)
{
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4)=arg0;WORD_AT(ESP0+8)=arg1;
    g_eax=0x1234u;call_esp_min=0xFFFFFFFFu;
    fn(orig);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"esp %+d after a hook\n",(int)(g_esp-ESP0));esp_errors++; }
    if(WORD_AT(ESP0)!=ret) { fprintf(stderr,"return address overwritten\n");esp_errors++; }
    g_esp=ESP0;
}
static void row(uint32_t ret,uint32_t text,uint32_t value)
{
    originals=0;rows=0;texts=0;
    hook(pc_mapmaker_tilesets_menu_row,row_original,ret,text,value);
}
static void path(uint32_t ret,uint32_t tileset)
{
    originals=0;mounts=0;lookups=0;seen_arg=0xFFu;
    hook(pc_mapmaker_tilesets_play_pak,path_original,ret,tileset,0);
}
#define IMAGE (0x0052CF38u+0x54u)

int main(void)
{
    uint32_t v;
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x0052CF38u)=0xAAFu;                       /* Abstract's name text */
    WORD_AT(0x006D7880u)=7u;                           /* paks mounted so far */

    /* ---- limits off: the editor shows the stock five rows ---- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);set_env("TSFP_MAPMAKER_POOLS",NULL);
    WORD_AT(0x0079094Cu)=102u;
    row(TS_RET_MENU_ROW,0x00410000u,5u);
    CHECK(originals==1 && seen_value==5u && rows==0 && texts==0 && WORD_AT(IMAGE)==0u);
    CHECK(g_eax==0x00350000u);

    /* ---- limits on ---- */
    set_env("TSFP_MAPMAKER_LIMITS","1");
    for(v=1;v<5u;v++) { row(TS_RET_MENU_ROW,0x00410000u,v);CHECK(originals==1 && rows==0); }
    row(0x003135D4u,0x00410000u,5u);                   /* the sky list's rows */
    CHECK(originals==1 && rows==0);
    row(0x0031B5E1u,0x00410000u,5u);                   /* any other list */
    CHECK(originals==1 && rows==0);
    WORD_AT(0x0079094Cu)=100u;
    row(TS_RET_MENU_ROW,0x00410000u,5u);              /* not the editor */
    CHECK(originals==1 && rows==0);
    WORD_AT(0x0079094Cu)=102u;
    heap_fail=1;
    row(TS_RET_MENU_ROW,0x00410000u,5u);              /* no memory for the label: no Disco row */
    CHECK(originals==1 && texts==1 && rows==1 && row_value[0]==0u);
    heap_fail=0;
    row(TS_RET_MENU_ROW,0x00410000u,5u);              /* after the Virtual row */
    CHECK(originals==1 && texts==1 && rows==2);
    CHECK(row_value[0]==6u && row_text[0]>=0x00700000u && !strcmp((const char *)memory+row_text[0],"Disco"));
    CHECK(row_text[1]==TEXT_BASE+0xAAFu && row_value[1]==0u);
    CHECK(g_eax==0x00350000u);                        /* the original's result */
    CHECK(WORD_AT(IMAGE)==0x17Du);                    /* Lab's image for Abstract */
    v=row_text[0];
    row(TS_RET_MENU_ROW,0x00410000u,5u);              /* the label is made once */
    CHECK(rows==2 && row_text[0]==v);
    WORD_AT(0x0052CF38u)=0x123u;                      /* no text: no Abstract row */
    row(TS_RET_MENU_ROW,0x00410000u,5u);
    CHECK(originals==1 && rows==1 && row_value[0]==6u);
    WORD_AT(0x0052CF38u)=0xAAFu;
    WORD_AT(IMAGE)=0x180u;                            /* an image already there is kept */
    row(TS_RET_MENU_ROW,0x00410000u,5u);
    CHECK(rows==2 && WORD_AT(IMAGE)==0x180u);
    WORD_AT(IMAGE)=0u;

    /* ---- play: stock tilesets and other callers are untouched ---- */
    WORD_AT(0x0079094Cu)=100u;
    for(v=1;v<6u;v++) {
        make_container(v);
        path(TS_RET_PAK_PATH,v);
        CHECK(originals==1 && seen_arg==v && mounts==0 && lookups==0 && WORD_AT(0x00784020u)==v);
    }
    make_container(0);
    path(0x00338200u,0);                               /* not the level loader */
    CHECK(originals==1 && seen_arg==0u && mounts==0 && lookups==0);
    CHECK(g_eax==0x0049FA94u);

    /* ---- play: an Abstract map mounts the editor pak (switch off too) ---- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    make_container(0);already=0;
    path(TS_RET_PAK_PATH,0);
    CHECK(originals==1 && seen_arg==0u && lookups==2 && mounts==2);
    CHECK(mount_args[1]==1u && mount_args[2]==1u);   /* read whole, level heap */
    CHECK(tex_args[1]==0u && tex_args[2]==1u);       /* its textures' pak: index-only */
    CHECK(string_below_frame && tex_below_frame && !mount_order_bad);
    CHECK(WORD_AT(0x006D7880u)==9u && WORD_AT(0x00784020u)==0u && (WORD_AT(INFO+0x08)>>16)==0u);
    CHECK(WORD_AT(IMAGE)==0x17Du);
    CHECK(g_eax==0x0049FA94u);
    path(TS_RET_PAK_PATH,0);                          /* mounted already: not again */
    CHECK(originals==1 && seen_arg==0u && lookups==2 && mounts==0 && WORD_AT(0x006D7880u)==9u);
    already=0;tex_already=0;tex_fails=1;WORD_AT(0x006D7880u)=8u;make_container(0);
    path(TS_RET_PAK_PATH,0);                          /* no textures pak: still Abstract (untextured) */
    CHECK(originals==1 && mounts==2 && seen_arg==0u && (WORD_AT(INFO+0x08)>>16)==0u && WORD_AT(0x00784020u)==0u);
    tex_fails=0;tex_already=0;

    /* ---- play: no mount possible -> the map plays as Lab ---- */
    set_env("TSFP_MAPMAKER_LIMITS","1");
    already=0;mount_fails=1;make_container(0);
    path(TS_RET_PAK_PATH,0);
    CHECK(originals==1 && mounts==1 && seen_arg==1u);
    CHECK((WORD_AT(INFO+0x08)>>16)==1u && (WORD_AT(INFO+0x08)&0xFFFFu)==0x5A5Au);
    CHECK(WORD_AT(0x00784020u)==1u);
    mount_fails=0;WORD_AT(0x006D7880u)=57u;make_container(0);
    path(TS_RET_PAK_PATH,0);                          /* the mount table is too full */
    CHECK(originals==1 && mounts==0 && seen_arg==1u && (WORD_AT(INFO+0x08)>>16)==1u && WORD_AT(0x00784020u)==1u);
    WORD_AT(0x006D7880u)=56u;make_container(0);
    path(TS_RET_PAK_PATH,0);                          /* 56 + 8 still fits; then no room for l_102m */
    CHECK(originals==1 && mounts==1 && seen_arg==0u && (WORD_AT(INFO+0x08)>>16)==0u);
    already=0;WORD_AT(0x006D7880u)=7u;mount_fails=1;
    make_container(0);WORD_AT(0x00784014u)=0u;         /* no container: still Lab, no crash */
    path(TS_RET_PAK_PATH,0);
    CHECK(originals==1 && seen_arg==1u && WORD_AT(0x00784020u)==1u);
    mount_fails=0;

    if(failures || esp_errors) return 1;
    printf("mapmaker tilesets ok\n");
    return 0;
}
