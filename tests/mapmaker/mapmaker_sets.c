/* Asset-free contract for src/interface/pc_mapmaker_sets.inc (custom
 * character set slots 11-20). Fake guest memory holds a map container, the
 * editor's set, the 10-row slot work copy 0x0075FD90 and a play set; the
 * fake originals follow the stock code's reads and writes (see the doc) and
 * pop their return address like the lifted callees; every hook call checks
 * esp. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00A00000/4], g_eax, g_ecx, g_edx, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define BYTE_AT(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define HALF_AT(a) (*(int16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }
/* distinct bodies: the linker folds identical functions into one address */
static volatile uint32_t stub_ran;
#define STUB(name,n) void name(void) { stub_ran=n; }
STUB(sub_0003DFA0,1) STUB(sub_00074670,2) STUB(sub_00071970,3) STUB(sub_00076EF0,4) STUB(sub_002BFFD0,5)
STUB(sub_0003D7B0,6) STUB(sub_00303350,7) STUB(sub_00071800,8) STUB(sub_000D7780,9)
STUB(sub_002C0310,10)

#define ESP0 0x00600000u
#define CONT 0x00100000u
#define EDITOR 0x00200000u
#define ESET (EDITOR+0x11860u)
#define WORK 0x0075FD90u
#define PARENT 0x00300000u
#define PANE 0x00301000u
#define ITEMS 0x00302000u
#define DST 0x00303000u
#define TITLE_TEXT 0x00304000u

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static uint32_t selected, options[32][2], preloaded[32];
static unsigned noptions, npreloaded, stores, originals;
static unsigned refills;
static uint32_t refill_index;
static void fake_refill(void);
static void hook_store_entry(void);
static uint32_t fake_slot_name(uint32_t item);
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[8]={0},r=0;
    int i;
    va_start(a,n);
    for(i=0;i<n && i<8;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00074670) r=(x[0]==PARENT && x[1]==0x0052372Cu)?PANE:0u;
    else if(f==sub_00071970) r=(x[0]==PANE && x[1]<10u && x[2]==0u)?ITEMS+x[1]*0x10u:0u;
    else if(f==sub_00076EF0) { if(x[0]==PANE) selected=x[1]; }
    else if(f==sub_002BFFD0) hook_store_entry();
    else if(f==sub_0003D7B0) r=0x00700000u+x[0];
    else if(f==sub_00303350) r=fake_slot_name(x[0]);
    else if(f==sub_00071800) { if(noptions<32u) { options[noptions][0]=x[0];options[noptions][1]=x[1]; } noptions++; }
    else if(f==sub_002C0310) fake_refill();
    else if(f==sub_000D7780) { if(npreloaded<32u && x[0]==x[1]) preloaded[npreloaded]=x[0]; npreloaded++; }
    return r;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_sets.inc"

static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}
static void call_hook(void (*hook)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t a0,uint32_t a1,int line)
{
    uint32_t sp=g_esp;
    g_esp-=12u;WORD_AT(g_esp)=ret;WORD_AT(g_esp+4u)=a0;WORD_AT(g_esp+8u)=a1;
    hook(orig);
    if(g_esp!=sp-8u) { fprintf(stderr,"%s:%d: esp %+d\n",__FILE__,line,(int)(g_esp-(sp-8u)));failures++; }
    g_esp=sp;
}
#define CALL(h,o,ret,a0,a1) call_hook(h,o,ret,a0,a1,__LINE__)

/* ---- the container ----------------------------------------------------- */
static uint32_t used_now(void) { return WORD_AT(CONT+0x10u+0x248u); }
static uint32_t sai_at;
/* header, map info (0x3DC), story AI chunks with slots, terminator */
static void container(const int *sai_slots,unsigned nsai)
{
    uint32_t p;
    unsigned i;
    memset((void *)((uintptr_t)CONT+g_xbox_mem_offset),0,0x80010u);
    WORD_AT(CONT)=0x00C77667u;WORD_AT(CONT+4u)=0x12Du;
    WORD_AT(CONT+8u)=0x00C77668u;WORD_AT(CONT+0xCu)=0x3DCu;
    p=CONT+0x3ECu;sai_at=p;
    for(i=0;i<nsai;i++) {
        WORD_AT(p)=0x00C7766Cu;WORD_AT(p+4u)=0x78u;BYTE_AT(p+8u+0x28u)=(uint8_t)sai_slots[i];p+=8u+0x78u;
    }
    WORD_AT(p)=0x00C7766Au;WORD_AT(p+4u)=p-CONT;
    WORD_AT(CONT+0x10u+0x248u)=p-CONT+8u;
}
static uint32_t compile_used;
static void fake_compile(void) { originals++;g_eax=compile_used=used_now();g_esp+=4u; }
static void editor_set(const int16_t *ids)
{
    unsigned k;
    WORD_AT(ESET)=10u;
    for(k=0;k<20u;k++) {
        uint32_t m=ESET+4u+8u*k;
        HALF_AT(m)=ids[k];BYTE_AT(m+2u)=0;BYTE_AT(m+3u)=(uint8_t)(0x10u+k);HALF_AT(m+4u)=0;HALF_AT(m+6u)=(int16_t)(k%3u==0?-1:(int)(k%3u));
    }
}
static const int16_t empty_extras[20]={100,101,102,103,104,105,106,107,108,109,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const int16_t some_extras[20]={100,101,102,103,104,105,106,107,108,109,200,-1,202,203,-1,-1,-1,-1,-1,0xA8};

static void compile_tests(void)
{
    static uint8_t before[0x80010];
    uint32_t used0,data,term;
    unsigned k;
    int none[1]={3},high[2]={1,12};
    /* a stock map: nothing appended, byte-identical */
    container(none,1);editor_set(empty_extras);used0=used_now();
    memcpy(before,(void *)((uintptr_t)CONT+g_xbox_mem_offset),sizeof before);
    CALL(pc_mapmaker_sets_compile,fake_compile,0x0032C6FFu,CONT,1u);
    CHECK(!memcmp(before,(void *)((uintptr_t)CONT+g_xbox_mem_offset),sizeof before));
    CHECK(g_eax==used0);
    /* slots 11-20 used: chunk before the terminator */
    container(none,1);editor_set(some_extras);used0=used_now();term=CONT+used0-8u;
    CALL(pc_mapmaker_sets_compile,fake_compile,0x00337677u,CONT,1u);
    CHECK(WORD_AT(term)==0x00C776F5u && WORD_AT(term+4u)==0x30u);
    data=term+8u;
    CHECK(HALF_AT(data)==1 && BYTE_AT(data+2u)==10u && BYTE_AT(data+3u)==10u && WORD_AT(data+0x2Cu)==0u);
    for(k=0;k<10u;k++) {
        uint32_t m=ESET+4u+8u*(10u+k),d=data+4u+4u*k;
        CHECK(HALF_AT(d)==HALF_AT(m) && BYTE_AT(d+2u)==BYTE_AT(m+3u) && (int8_t)BYTE_AT(d+3u)==HALF_AT(m+6u));
    }
    CHECK(WORD_AT(term+0x38u)==0x00C7766Au && WORD_AT(term+0x3Cu)==term+0x38u-CONT);
    CHECK(used_now()==used0+0x38u && g_eax==used0+0x38u);
    CHECK(sets_chunk(CONT)==data);
    CHECK(sets_walk(CONT,0,0,0)==term+0x38u);
    /* a Story AI on slot 13 with slots 11-20 empty: still written */
    container(high,2);editor_set(empty_extras);used0=used_now();
    CALL(pc_mapmaker_sets_compile,fake_compile,0x00337677u,CONT,1u);
    CHECK(sets_chunk(CONT)!=0 && used_now()==used0+0x38u);
    /* the save compile keeps the container hook's low-word rule */
    container(none,1);editor_set(some_extras);
    WORD_AT(CONT+0x10u+0x248u)=0x10000u-0x38u;
    WORD_AT(CONT+0x10000u-0x38u-8u)=0x00C7766Au;WORD_AT(CONT+0x10000u-0x38u-4u)=0x10000u-0x38u-8u;
    WORD_AT(CONT+0x3ECu)=0x00C77670u;WORD_AT(CONT+0x3F0u)=0x10000u-0x38u-8u-0x3ECu-8u;  /* filler chunk */
    CALL(pc_mapmaker_sets_compile,fake_compile,0x0032C6FFu,CONT,1u);
    CHECK(used_now()==0x10000u && g_eax==0x10004u);
    /* no room: nothing written, logged */
    container(none,1);editor_set(some_extras);
    WORD_AT(CONT+0x3ECu)=0x00C77670u;WORD_AT(CONT+0x3F0u)=0x7FFE0u-0x20u-0x3ECu-8u;
    WORD_AT(CONT+0x7FFE0u-0x20u)=0x00C7766Au;WORD_AT(CONT+0x7FFE0u-0x1Cu)=0x7FFE0u-0x20u;
    WORD_AT(CONT+0x10u+0x248u)=0x7FFE0u-0x18u;used0=used_now();
    CALL(pc_mapmaker_sets_compile,fake_compile,0x00337677u,CONT,1u);
    CHECK(used_now()==used0 && !sets_chunk(CONT) && g_eax==used0);
    /* not a container: original only */
    WORD_AT(CONT)=0;compile_used=0;
    CALL(pc_mapmaker_sets_compile,fake_compile,0x00337677u,CONT,1u);
    CHECK(originals>0);
}

/* ---- editor load / reset ----------------------------------------------- */
static void fake_plain(void) { originals++;g_esp+=4u; }
static uint32_t changed_answer;
static void fake_changed(void) { originals++;g_eax=changed_answer;g_esp+=4u; }
static void load_tests(void)
{
    static const int16_t junk[20]={0};
    int none[1]={0};
    unsigned k;
    /* with the chunk: members 10..19 from it */
    container(none,1);editor_set(some_extras);
    CALL(pc_mapmaker_sets_compile,fake_compile,0x00337677u,CONT,1u);
    editor_set(junk);
    CALL(pc_mapmaker_sets_load,fake_plain,0x00301000u,CONT,0u);
    for(k=10;k<20u;k++) {
        uint32_t m=ESET+4u+8u*k;
        CHECK(HALF_AT(m)==some_extras[k] && BYTE_AT(m+3u)==(uint8_t)(0x10u+k) && HALF_AT(m+6u)==(int16_t)(k%3u==0?-1:(int)(k%3u)));
    }
    for(k=0;k<10u;k++) CHECK(HALF_AT(ESET+4u+8u*k)==0);           /* stock members untouched */
    /* without it: empty */
    container(none,1);editor_set(junk);
    CALL(pc_mapmaker_sets_load,fake_plain,0x00301000u,CONT,0u);
    for(k=10;k<20u;k++) CHECK(HALF_AT(ESET+4u+8u*k)==-1 && HALF_AT(ESET+4u+8u*k+6u)==-1);
    /* not a container: untouched */
    editor_set(junk);WORD_AT(CONT)=0;
    CALL(pc_mapmaker_sets_load,fake_plain,0x00301000u,CONT,0u);
    CHECK(HALF_AT(ESET+4u+8u*15u)==0);
    /* Map Settings close: members 11-20 count as a change against the undo shadow */
    container(none,1);editor_set(some_extras);
    CALL(pc_mapmaker_sets_compile,fake_compile,0x00337677u,CONT,1u);
    WORD_AT(0x00765E38u)=CONT;
    changed_answer=0;CALL(pc_mapmaker_sets_changed,fake_changed,0x00313358u,0u,0u);CHECK(g_eax==0u);
    HALF_AT(ESET+4u+8u*12u+6u)=1;               /* variant of slot 13 */
    CALL(pc_mapmaker_sets_changed,fake_changed,0x00313358u,0u,0u);CHECK(g_eax==1u);
    HALF_AT(ESET+4u+8u*12u+6u)=-1;HALF_AT(ESET+4u+8u*17u)=55;
    CALL(pc_mapmaker_sets_changed,fake_changed,0x00313358u,0u,0u);CHECK(g_eax==1u);
    HALF_AT(ESET+4u+8u*17u)=-1;
    CALL(pc_mapmaker_sets_changed,fake_changed,0x00313358u,0u,0u);CHECK(g_eax==0u);
    changed_answer=1;CALL(pc_mapmaker_sets_changed,fake_changed,0x00313358u,0u,0u);CHECK(g_eax==1u);
    changed_answer=0;container(none,1);         /* shadow without the chunk */
    CALL(pc_mapmaker_sets_changed,fake_changed,0x00313358u,0u,0u);CHECK(g_eax==1u);
    editor_set(empty_extras);
    CALL(pc_mapmaker_sets_changed,fake_changed,0x00313358u,0u,0u);CHECK(g_eax==0u);
    /* new map: the preset copy's zero members are cleared */
    editor_set(junk);
    CALL(pc_mapmaker_sets_reset,fake_plain,0x00302000u,0u,0u);
    for(k=10;k<20u;k++) CHECK(HALF_AT(ESET+4u+8u*k)==-1);
    for(k=0;k<10u;k++) CHECK(HALF_AT(ESET+4u+8u*k)==0);
}

/* ---- slot screen ------------------------------------------------------- */
/* sub_00312950 in the editor */
static void fake_editor_set(void) { g_eax=ESET;g_esp+=4u; }
/* sub_002C0290 for a custom index in the editor */
static void fake_set_ptr(void) { g_eax=ESET;g_esp+=4u; }
static uint32_t inner_call(void (*hook)(void (*)(void)),void (*orig)(void),uint32_t ret)
{
    uint32_t sp=g_esp,r;
    g_esp-=8u;WORD_AT(g_esp)=ret;WORD_AT(g_esp+4u)=0x11u;
    hook(orig);
    if(g_esp!=sp-4u) { fprintf(stderr,"inner esp %+d\n",(int)(g_esp-(sp-4u)));failures++; }
    r=g_eax;g_esp=sp;
    return r;
}
/* sub_00314210: 10 rows into the set from sub_00312950, count of used rows */
static void fake_store(void)
{
    uint32_t set=inner_call(pc_mapmaker_sets_editor_set,fake_editor_set,0x00314218u),n=0;
    unsigned k;
    stores++;
    for(k=0;k<10u;k++) {
        uint32_t w=WORK+k*0x1Cu,m=set+4u+8u*k;
        HALF_AT(m)=(int16_t)WORD_AT(w);HALF_AT(m+6u)=(int16_t)WORD_AT(w+0x18u);BYTE_AT(m+3u)=(uint8_t)WORD_AT(w+8u);
        n+=WORD_AT(w)!=0xFFFFFFFFu;
    }
    WORD_AT(set)=n;
    g_esp+=4u;
}
static void hook_store_entry(void)                  /* sub_002BFFD0: push 0x75FD90; call sub_00314210 */
{
    uint32_t sp=g_esp;
    g_esp-=8u;WORD_AT(g_esp)=0x002BFFE8u;WORD_AT(g_esp+4u)=WORK;
    pc_mapmaker_sets_store(fake_store);
    if(g_esp!=sp-4u) { fprintf(stderr,"store esp\n");failures++; }
    g_esp=sp;
}
/* sub_002C0490: reload when the index differs, then store every frame */
static void fake_screen(void)
{
    uint32_t idx=WORD_AT(0x00784024u);
    originals++;
    if(WORD_AT(0x0078AEACu)!=idx) {
        uint32_t set=inner_call(pc_mapmaker_sets_set_ptr,fake_set_ptr,0x002C0319u),b;
        unsigned k;
        for(k=0;k<10u;k++) {
            uint32_t w=WORK+k*0x1Cu;
            int16_t id=HALF_AT(set+4u+8u*k);
            WORD_AT(w)=id<=-1?0xFFFFFFFFu:(uint32_t)(int32_t)id;
            WORD_AT(w+0x18u)=id<=-1?0xFFFFFFFFu:(uint32_t)(int32_t)HALF_AT(set+0xAu+8u*k);
        }
        b=inner_call(pc_mapmaker_sets_editor_set,fake_editor_set,0x003141B5u);
        for(k=0;k<10u;k++) WORD_AT(WORK+k*0x1Cu+8u)=BYTE_AT(b+7u+8u*k);
        WORD_AT(0x0078AEACu)=idx;
    }
    hook_store_entry();
    g_esp+=4u;
}
static void frame(int32_t row,uint32_t input)
{
    WORD_AT(PANE+0x18Cu)=(uint32_t)row;WORD_AT(PANE+0xC4u)=input;WORD_AT(PANE+0xC8u)=input&0xFu;
    CALL(pc_mapmaker_sets_screen,fake_screen,0x0007AD7Bu,PARENT,0u);
}
static int work_id(unsigned k) { return (int)(int32_t)WORD_AT(WORK+k*0x1Cu); }
static int member(unsigned k) { return HALF_AT(ESET+4u+8u*k); }
static const char *title(void) { return (const char *)((uintptr_t)WORD_AT(PARENT+0x204u)+g_xbox_mem_offset); }
static void screen_tests(void)
{
    unsigned k;
    editor_set(some_extras);
    strcpy((char *)((uintptr_t)TITLE_TEXT+g_xbox_mem_offset),"Characters");
    WORD_AT(PARENT+0x204u)=TITLE_TEXT;
    WORD_AT(0x0079094Cu)=102u;WORD_AT(0x00784024u)=0x11u;WORD_AT(0x0078AEACu)=0;
    /* opened: page 1-10 */
    frame(0,0);
    for(k=0;k<10u;k++) CHECK(work_id(k)==100+(int)k);
    CHECK(!strcmp(title(),"Characters  (1-10)"));
    WORD_AT(WORK+3u*0x1Cu)=150u;frame(3,0);
    CHECK(member(3)==150 && WORD_AT(ESET)==10u);
    /* Down on row 10 -> slots 11-20; a pick made this frame is kept */
    WORD_AT(WORK+5u*0x1Cu)=155u;
    frame(9,0x00020002u);
    CHECK(member(5)==155);
    CHECK(work_id(0)==200 && work_id(1)==-1 && work_id(2)==202 && work_id(9)==0xA8);
    CHECK(WORD_AT(WORK+2u*0x1Cu+8u)==0x1Cu && (int32_t)WORD_AT(WORK+2u*0x1Cu+0x18u)==-1);    /* slot 13 */
    CHECK(selected==ITEMS && (WORD_AT(PANE+0xC4u)&0x000F000Fu)==0u);
    CHECK(!strcmp(title(),"Characters  (11-20)"));
    for(k=0;k<10u;k++) CHECK(member(k)==(k==3?150:k==5?155:100+(int)k));
    CHECK(WORD_AT(ESET)==10u);
    /* a pick on page 2 lands in slot 13; slots 1-10 and the count stay */
    WORD_AT(WORK+2u*0x1Cu)=77u;WORD_AT(WORK+2u*0x1Cu+0x18u)=1u;
    frame(2,0);
    CHECK(member(12)==77 && HALF_AT(ESET+4u+8u*12u+6u)==1 && member(2)==102 && WORD_AT(ESET)==10u);
    WORD_AT(WORK+1u*0x1Cu)=33u;frame(1,0);
    CHECK(member(11)==33 && member(1)==101);
    /* Down on the last row of page 2: stock (no wrap to page 1) */
    frame(9,0x00020002u);
    CHECK(work_id(0)==200 && !strcmp(title(),"Characters  (11-20)"));
    /* Up on row 11 -> back to slots 1-10, cursor on row 10 */
    selected=0;
    frame(0,0x00010001u);
    CHECK(work_id(3)==150 && work_id(5)==155 && work_id(0)==100 && selected==ITEMS+9u*0x10u);
    CHECK(member(12)==77 && member(11)==33);
    CHECK(!strcmp(title(),"Characters  (1-10)"));
    /* Up on row 1 of page 1: stock */
    frame(0,0x00010001u);
    CHECK(work_id(0)==100);
    /* reopening resets to page 1 */
    frame(9,0x00020002u);CHECK(work_id(0)==200);
    WORD_AT(0x0078AEACu)=0;frame(0,0);
    CHECK(work_id(0)==100 && !strcmp(title(),"Characters  (1-10)"));
    /* preset set: no paging */
    WORD_AT(0x00784024u)=3u;frame(9,0x00020002u);
    CHECK(WORD_AT(PANE+0xC4u)==0x00020002u);
    WORD_AT(0x00784024u)=0x11u;WORD_AT(0x0078AEACu)=0x11u;
    /* switch off: no paging, title stays */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    WORD_AT(PARENT+0x204u)=TITLE_TEXT;
    frame(9,0x00020002u);
    CHECK(work_id(0)==100 && WORD_AT(PANE+0xC4u)==0x00020002u && WORD_AT(PARENT+0x204u)==TITLE_TEXT);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    /* outside the screen update the getters are stock */
    sets.page=1;
    CHECK(inner_call(pc_mapmaker_sets_set_ptr,fake_set_ptr,0x002C0319u)==ESET);
    CHECK(inner_call(pc_mapmaker_sets_editor_set,fake_editor_set,0x003141B5u)==ESET);
    sets.page=0;
}

/* ---- match start ------------------------------------------------------ */
/* sub_002C0310 (index in eax): rows 1-10 from the set the getter returns */
static void fake_refill(void)
{
    unsigned k;
    refills++;refill_index=g_eax;
    CHECK(!sets.page);                          /* the getters are not shifted */
    for(k=0;k<10u;k++) {
        int16_t id=HALF_AT(ESET+4u+8u*k);
        WORD_AT(WORK+k*0x1Cu)=id<=-1?0xFFFFFFFFu:(uint32_t)(int32_t)id;
    }
    g_eax=0x12345u;                             /* clobbered like the original */
}
static uint32_t roster[10];
static void fake_match_start(void)
{
    unsigned k;
    originals++;
    for(k=0;k<10u;k++) roster[k]=WORD_AT(WORK+k*0x1Cu);
    g_esp+=4u;
}
static void match_tests(void)
{
    editor_set(some_extras);
    strcpy((char *)((uintptr_t)TITLE_TEXT+g_xbox_mem_offset),"Characters");
    WORD_AT(PARENT+0x204u)=TITLE_TEXT;
    WORD_AT(0x0079094Cu)=102u;WORD_AT(0x00784024u)=0x11u;WORD_AT(0x0078AEACu)=0;
    frame(0,0);frame(9,0x00020002u);
    CHECK(work_id(0)==200 && sets.page==1);
    /* the screen closed on slots 11-20; the Test map start sees rows 1-10 */
    refills=0;originals=0;g_eax=0x77u;
    CALL(pc_mapmaker_sets_match_start,fake_match_start,0x002D3933u,0u,0u);
    CHECK(refills==1u && refill_index==0x11u && originals==1u && g_eax==0x77u);
    CHECK(roster[0]==100u && roster[9]==109u && sets.page==0);
    CHECK(WORD_AT(0x0078AEACu)==0x11u);
    /* page 1 left: stock */
    refills=0;CALL(pc_mapmaker_sets_match_start,fake_match_start,0x002D3933u,0u,0u);
    CHECK(refills==0u && originals==2u);
    /* outside the editor: stock, the flag is dropped */
    frame(9,0x00020002u);CHECK(sets.page==1);
    WORD_AT(0x0079094Cu)=100u;originals=0;
    CALL(pc_mapmaker_sets_match_start,fake_match_start,0x002D5E52u,0u,0u);
    CHECK(refills==0u && sets.page==0 && originals==1u && work_id(0)==200);
    WORD_AT(0x0079094Cu)=102u;
}

/* ---- Story AI choice --------------------------------------------------- */
/* sub_00303350 case 3 (items 0x38..0x41): member item - 0x38 of the set
 * sub_002C0290 returns to 0x003033A9, named by sub_002BE650: row of (id,
 * variant) -1 past the Arcade table (the realbosses 0x15/0x28, 0x1FF, -1),
 * row -> text 0 for -1 (this fixture has no realbosses area; see
 * mapmaker_sets_names.c for the whole chain). Other items: 0. */
static uint32_t fake_slot_name(uint32_t item)
{
    uint32_t set,m,row;
    int id;
    if(item<0x38u || item>=0x42u) return 0;
    set=inner_call(pc_mapmaker_sets_set_ptr,fake_set_ptr,0x003033A9u);
    m=set+4u+8u*(item-0x38u);id=HALF_AT(m);
    row=(id==-1 || id==0x15 || id==0x28 || id==0x1FF)?0xFFFFFFFFu:(uint32_t)id*16u+((uint32_t)(int32_t)HALF_AT(m+6u)&0xFu);
    return 0x00700000u+(row==0xFFFFFFFFu?0u:0x5000u+row);
}
static void fake_choice(void) { originals++;CHECK(noptions==0u || noptions==10u);g_esp+=4u; }
static void choice_tests(void)
{
    uint32_t value=0x00305000u;
    unsigned k,before;
    editor_set(some_extras);
    WORD_AT(0x0079094Cu)=102u;WORD_AT(0x00784024u)=0x11u;
    noptions=0;WORD_AT(value)=2u;before=originals;
    CALL(pc_mapmaker_sets_sai_choice,fake_choice,0x0031CD32u,value,0u);
    CHECK(originals==before+1u && noptions==10u);
    for(k=0;k<10u;k++) {
        int id=some_extras[10u+k];
        CHECK(options[k][1]==10u+k);
        CHECK(options[k][0]==(id==-1?0x00700000u+0xD7Cu:0x00700000u+0x5000u+(uint32_t)id*16u+(uint32_t)((10u+k)%3u==0?0xFu:(10u+k)%3u)));
    }
    /* the names come from the stock path for member k (not k - 10, which is
       a different character); ids without an Arcade row get what slots 1-10
       would get there (here, no realbosses area: stock's empty text 0) */
    HALF_AT(ESET+4u+8u*13u)=0x15;HALF_AT(ESET+4u+8u*13u+6u)=-1;
    HALF_AT(ESET+4u+8u*14u)=0x1FF;
    noptions=0;CALL(pc_mapmaker_sets_sai_choice,fake_choice,0x0031CD32u,value,0u);
    CHECK(noptions==10u && options[3][1]==13u && options[3][0]==0x00700000u);
    CHECK(options[4][1]==14u && options[4][0]==0x00700000u);
    CHECK(options[0][0]==0x00700000u+0x5000u+200u*16u+1u);
    CHECK(options[9][0]==0x00700000u+0x5000u+0xA8u*16u+1u);
    /* the stock name path afterwards (slots 1-10) reads members 0..9 */
    CHECK(!sets.name_shift && inner_call(pc_mapmaker_sets_set_ptr,fake_set_ptr,0x003033A9u)==ESET);
    CHECK(fake_slot_name(0x38u+3u)==0x00700000u+0x5000u+103u*16u+0xFu);
    editor_set(some_extras);
    noptions=0;CALL(pc_mapmaker_sets_sai_choice,fake_choice,0x0031CD37u,value,0u);CHECK(noptions==0u);
    WORD_AT(0x00784024u)=4u;noptions=0;
    CALL(pc_mapmaker_sets_sai_choice,fake_choice,0x0031CD32u,value,0u);CHECK(noptions==0u);
    WORD_AT(0x00784024u)=0x11u;
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    noptions=0;CALL(pc_mapmaker_sets_sai_choice,fake_choice,0x0031CD32u,value,0u);CHECK(noptions==0u);
    WORD_AT(value)=13u;noptions=0;                 /* a map made with the switch on */
    CALL(pc_mapmaker_sets_sai_choice,fake_choice,0x0031CD32u,value,0u);CHECK(noptions==10u);
    set_env("TSFP_MAPMAKER_LIMITS","1");
}

/* ---- Story AI icons ---------------------------------------------------- */
static uint32_t icon_item;
static void fake_icon(void) { originals++;icon_item=WORD_AT(g_esp+4u);g_eax=0x00710000u+icon_item;g_esp+=4u; }
static void icon_tests(void)
{
    static const uint32_t rets[2]={0x0031D460u,0x00309AC7u};
    unsigned r,item;
    for(r=0;r<2u;r++)
        for(item=0;item<0x50u;item++) {
            CALL(pc_mapmaker_sets_icon,fake_icon,rets[r],item,0u);
            CHECK(icon_item==(item>=0x42u && item<0x4Cu?item-10u:item) && g_eax==0x00710000u+icon_item);
        }
    /* every other caller: the item itself (0x42 is a real item there) */
    CALL(pc_mapmaker_sets_icon,fake_icon,0x0032054Au,0x42u,0u);CHECK(icon_item==0x42u);
    CALL(pc_mapmaker_sets_icon,fake_icon,0x003362B0u,0x45u,0u);CHECK(icon_item==0x45u);
    CALL(pc_mapmaker_sets_icon,fake_icon,0x0030DA3Au,0x4Bu,0u);CHECK(icon_item==0x4Bu);
}

/* ---- play -------------------------------------------------------------- */
/* sub_0033EBF0: count byte and ids from map info +0x16 */
static void fake_play_copy(void)
{
    unsigned k;
    originals++;
    WORD_AT(DST)=10u;
    for(k=0;k<10u;k++) HALF_AT(DST+4u+8u*k)=(int16_t)(k==4u?-1:300+(int)k);
    g_esp+=4u;
}
static void play_tests(void)
{
    int slots[3]={2,15,4};
    unsigned k;
    container(slots,3);editor_set(some_extras);
    CALL(pc_mapmaker_sets_compile,fake_compile,0x00337677u,CONT,1u);
    WORD_AT(0x00784014u)=CONT;
    memset((void *)((uintptr_t)DST+g_xbox_mem_offset),0,0xA4u);
    WORD_AT(0x0079094Cu)=100u;
    CALL(pc_mapmaker_sets_play_set,fake_play_copy,0x00240DD3u,DST,CONT+0x10u+0x16u);
    CHECK(WORD_AT(DST)==20u);
    for(k=0;k<10u;k++) CHECK(HALF_AT(DST+4u+8u*(10u+k))==(k==5u?300:some_extras[10u+k]));   /* slot 16 used, empty */
    CHECK(HALF_AT(DST+4u+8u*4u)==300);                 /* slot 5 used by a story AI, empty */
    CHECK(HALF_AT(DST+4u+8u*16u)==-1);                 /* unused empty slot stays empty */
    /* other caller or no chunk: stock */
    memset((void *)((uintptr_t)DST+g_xbox_mem_offset),0,0xA4u);
    CALL(pc_mapmaker_sets_play_set,fake_play_copy,0x00240DD4u,DST,0u);
    CHECK(WORD_AT(DST)==10u && HALF_AT(DST+4u+8u*12u)==0 && HALF_AT(DST+4u+8u*4u)==-1);
    container(slots,1);
    CALL(pc_mapmaker_sets_play_set,fake_play_copy,0x00240DD3u,DST,0u);
    CHECK(WORD_AT(DST)==10u && HALF_AT(DST+4u+8u*12u)==0);
    /* preload: members 11-20 that are used */
    container(slots,3);
    CALL(pc_mapmaker_sets_compile,fake_compile,0x00337677u,CONT,1u);
    npreloaded=0;
    CALL(pc_mapmaker_sets_preload,fake_plain,0x0023F477u,0u,0u);
    CHECK(npreloaded==4u && preloaded[0]==200u && preloaded[1]==202u && preloaded[2]==203u && preloaded[3]==0xA8u);
    npreloaded=0;CALL(pc_mapmaker_sets_preload,fake_plain,0x0023F47Cu,0u,0u);CHECK(npreloaded==0u);
}

int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;
    g_esp=ESP0;
    WORD_AT(0x007844A8u)=EDITOR;
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    compile_tests();
    load_tests();
    screen_tests();
    choice_tests();
    icon_tests();
    play_tests();
    match_tests();
    if(failures) { fprintf(stderr,"%d failure(s)\n",failures);return 1; }
    printf("mapmaker sets ok\n");
    return 0;
}
