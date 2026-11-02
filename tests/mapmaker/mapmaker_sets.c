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
STUB(sub_00075B70,11) STUB(sub_00071BE0,12) STUB(sub_00079D60,13) STUB(sub_002C1830,14) STUB(sub_00079710,15)
STUB(sub_000780E0,16) STUB(sub_00078320,17) STUB(sub_000792B0,18) STUB(sub_00078380,19) STUB(sub_000781A0,20)
STUB(sub_00077140,21) STUB(sub_00077360,22) STUB(sub_00074A00,23) STUB(sub_00074460,24)
STUB(sub_002F7920,25)
static struct { void (*f)(void); uint32_t a[7]; } build[96];   /* the pane builder's calls */
static unsigned nbuild;
static uint32_t advanced[8][2];                                /* sub_00074460(pane, value, 0) */
static unsigned nadvanced;

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
    if(f==sub_00075B70 || f==sub_00071BE0 || f==sub_00079D60 || f==sub_002C1830 || f==sub_00079710 ||
       f==sub_000780E0 || f==sub_00078320 || f==sub_000792B0 || f==sub_00078380 || f==sub_000781A0 ||
       f==sub_00077140 || f==sub_00077360 || f==sub_00074A00) {
        if(nbuild<96u) { build[nbuild].f=f;memcpy(build[nbuild].a,x,sizeof build[nbuild].a); }
        nbuild++;
    }
    if(f==sub_00074460) { if(nadvanced<8u) { advanced[nadvanced][0]=x[0];advanced[nadvanced][1]=x[1]; } nadvanced++;CHECK(x[2]==0u); }
    if(f==sub_00074670) r=(x[0]==PARENT && x[1]==0x0052372Cu)?PANE:0u;
    else if(f==sub_00071970) r=(x[0]==PANE && x[1]<10u && x[2]==0u)?ITEMS+x[1]*0x10u:0u;
    else if(f==sub_00076EF0) { if(x[0]==PANE) selected=x[1]; }
    else if(f==sub_002BFFD0) hook_store_entry();
    else if(f==sub_0003D7B0) r=0x00700000u+x[0];
    else if(f==sub_00303350) r=fake_slot_name(x[0]);
    else if(f==sub_00071800) { if(noptions<32u) { options[noptions][0]=x[0];options[noptions][1]=x[1]; } noptions++; }
    else if(f==sub_002C0310) fake_refill();
    else if(f==sub_002F7920) r=x[0]==0x38u?0x700u:x[0]==0x44u?0x20u:0xBADu;   /* item masks (XBE 0x525018) */
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
        /* portraits: sub_002BE600(id, variant) returning to 0x002C054C */
        for(k=0;k<10u;k++) {
            int32_t id=(int32_t)WORD_AT(WORK+k*0x1Cu);
            WORD_AT(WORK+k*0x1Cu+0x14u)=id<0?0xFFFFFFFFu:(uint32_t)id+0x1000u;
        }
        WORD_AT(0x0078AEACu)=idx;
    }
    hook_store_entry();
    g_esp+=4u;
}
static void frame(void) { CALL(pc_mapmaker_sets_screen,fake_screen,0x0007AD7Bu,PARENT,0u); }
static int work_id(unsigned k) { return (int)(int32_t)WORD_AT(WORK+k*0x1Cu); }
static int shadow_id(unsigned k) { int32_t id;memcpy(&id,sl.rows+k*0x1Cu,4);return id; }
static int member(unsigned k) { return HALF_AT(ESET+4u+8u*k); }
static uint32_t work_sum(void)
{
    uint32_t s=0,k;
    for(k=0;k<10u*0x1Cu;k+=4u) s=s*31u+WORD_AT(WORK+k);
    return s;
}

/* the pane builder as the pane system calls it (sub_00078E50 0x00078F72) */
static void build_pane(void)
{
    originals=0;nbuild=0;
    WORD_AT(0x007BB1E8u)=PANE;
    CALL(pc_mapmaker_sets_build,fake_plain,0x00078F74u,PANE,0u);
}
#define B(i) build[i].f
#define A(i,k) build[i].a[k]
static void check_build(void)
{
    unsigned i,v;
    CHECK(nbuild==73u);
    if(nbuild!=73u) return;
    CHECK(B(0)==sub_00075B70 && A(0,0)==0x10u && A(0,1)==0u && A(0,2)==0x42340000u && A(0,3)==0u && A(0,4)==0x002C1450u);
    CHECK(B(1)==sub_00071BE0 && A(1,0)==1u && A(1,1)==0x40A00000u && A(1,2)==0x40A00000u);
    CHECK(B(2)==sub_00079D60 && A(2,0)==0x00700000u+0xE8Bu && A(2,1)==0xFFFFFFFFu && A(2,2)==0x04032404u && !A(2,3) && !A(2,4) && !A(2,5));
    CHECK(B(3)==sub_00079D60 && A(3,0)==0x004766D8u && A(3,1)==0xFFFFFFFEu && A(3,2)==0x08030C00u && !A(3,3));
    CHECK(B(4)==sub_002C1830 && A(4,0)==PANE);
    CHECK(B(5)==sub_00079710 && A(5,0)==0u);
    CHECK(B(6)==sub_000780E0 && A(6,0)==0x42200000u && A(6,1)==0u);       /* ten rows of 4.0 */
    for(v=0;v<20u;v++) {
        i=7u+3u*v;
        CHECK(B(i)==sub_00078320 && A(i,0)==0u && A(i,1)==1u && A(i,2)==4u);
        CHECK(B(i+1)==sub_000792B0 && A(i+1,0)==0u && A(i+1,1)==v && A(i+1,2)==0u && A(i+1,3)==0x42200000u &&
              A(i+1,4)==0x40800000u && A(i+1,5)==0x002C1490u && A(i+1,6)==0x002C0660u);
        CHECK(B(i+2)==sub_00078380);
    }
    i=67u;
    CHECK(B(i)==sub_000781A0);
    CHECK(B(i+1)==sub_00077140 && A(i+1,0)==PANE);
    CHECK(B(i+2)==sub_00077360 && A(i+2,0)==PANE && A(i+2,1)==0x40A00000u && A(i+2,2)==0x41133333u);
    CHECK(B(i+3)==sub_00074A00 && A(i+3,0)==0u && A(i+3,1)==0x00700000u+0xE88u && A(i+3,2)==1u && A(i+3,3)==0u);
    CHECK(B(i+4)==sub_00074A00 && A(i+4,0)==3u && A(i+4,1)==0x00700000u+0x3B2u && A(i+4,2)==1u && A(i+4,3)==0u);
    CHECK(B(i+5)==sub_00074A00 && A(i+5,0)==1u && A(i+5,1)==0x00700000u+0x1A27u && A(i+5,2)==0u && A(i+5,3)==0u);
}

/* Row callbacks as the stock code reads them: the draw (pane, item, rect)
 * reads item +0x14 and that work row; the action and the pane update read
 * pane +0x184 - 2 (and +0x18C); the update clears the row on X. */
static uint32_t seen_value,seen_index,seen_hl,seen_id,seen_portrait;
static void fake_draw(void)
{
    uint32_t item=WORD_AT(g_esp+8u);
    originals++;seen_value=WORD_AT(item+0x14u);
    seen_id=seen_value<10u?WORD_AT(WORK+seen_value*0x1Cu):0xDEADu;
    seen_portrait=seen_value<10u?WORD_AT(WORK+seen_value*0x1Cu+0x14u):0xDEADu;
    g_esp+=4u;
}
static void fake_row_cb(void)
{
    uint32_t pane=WORD_AT(g_esp+4u),row;
    originals++;seen_index=WORD_AT(pane+0x184u);seen_hl=WORD_AT(pane+0x18Cu);row=seen_index-2u;
    seen_id=row<10u?WORD_AT(WORK+row*0x1Cu):0xDEADu;
    if(row<10u && (WORD_AT(pane+0xC4u)&0x00800000u) && (int32_t)WORD_AT(WORK+row*0x1Cu)>=0) {    /* X, 0x002C1718 */
        WORD_AT(WORK+row*0x1Cu)=0xFFFFFFFFu;WORD_AT(WORK+row*0x1Cu+0x14u)=0xFFFFFFFFu;
    }
    g_esp+=4u;
}
/* sub_002C1C90: row = slot pane +0x184 - 2 gets the picked character, then
 * the highlight moves on while its value is below 9. */
#define PICKER 0x00306000u
#define PITEM 0x00307000u
static void fake_pick(void)
{
    uint32_t row=WORD_AT(PANE+0x184u)-2u,v;
    originals++;seen_index=WORD_AT(PANE+0x184u);
    if(!(BYTE_AT(PITEM+0x28u)&1u)) {
        if(row<10u) { WORD_AT(WORK+row*0x1Cu)=77u;WORD_AT(WORK+row*0x1Cu+0x14u)=0x99u;WORD_AT(WORK+row*0x1Cu+0x18u)=1u;WORD_AT(WORK+row*0x1Cu+8u)=2u; }
        else seen_index=0xBADu;                  /* past the work copy */
        if((v=WORD_AT(PANE+0x18Cu))<9u) { WORD_AT(PANE+0x18Cu)=v+1u;guest_call(sub_00074460,3,PANE,v+1u,0u); }
    }
    g_esp+=4u;
}
static void row_draw(uint32_t value)
{
    WORD_AT(ITEMS+0x14u)=value;
    CALL(pc_mapmaker_sets_row_draw,fake_draw,0x00073E00u,PANE,ITEMS);
}
static void pane_at(void (*hook)(void (*)(void)),uint32_t value,uint32_t input)
{
    WORD_AT(PANE+0x184u)=value+2u;WORD_AT(PANE+0x18Cu)=value;WORD_AT(PANE+0xC4u)=input;
    CALL(hook,fake_row_cb,0x0007AD7Bu,PANE,0u);
}
static void pick_at(uint32_t value,int disabled)
{
    WORD_AT(PICKER+0x2Cu)=PARENT;WORD_AT(PICKER+0x198u)=PITEM;BYTE_AT(PITEM+0x28u)=(uint8_t)(disabled?1u:0u);
    WORD_AT(PANE+0x184u)=value+2u;WORD_AT(PANE+0x18Cu)=value;nadvanced=0;
    CALL(pc_mapmaker_sets_pick,fake_pick,0x00073E00u,PICKER,0u);
}
static void screen_tests(void)
{
    unsigned k,before;
    uint32_t sum;
    editor_set(some_extras);
    WORD_AT(0x0079094Cu)=102u;WORD_AT(0x00784024u)=0x11u;WORD_AT(0x0078AEACu)=0;
    memset((void *)((uintptr_t)WORK+g_xbox_mem_offset),0x5A,10u*0x1Cu);      /* stale */
    /* switch off: the stock builder, the screen and the rows stock */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    build_pane();CHECK(originals==1u && nbuild==0u && !sl.on);
    originals=0;frame();CHECK(originals==1u && work_id(0)==100);
    row_draw(12u);CHECK(seen_value==12u);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    /* outside the editor: stock */
    WORD_AT(0x0079094Cu)=101u;build_pane();CHECK(originals==1u && nbuild==0u && !sl.on);
    WORD_AT(0x0079094Cu)=102u;
    /* the editor: the stock calls with 20 row groups in a scrolling region ten rows tall */
    WORD_AT(0x0078AEACu)=0;
    memset((void *)((uintptr_t)WORK+g_xbox_mem_offset),0x5A,10u*0x1Cu);
    build_pane();
    CHECK(originals==0u && sl.on && !sl.valid);
    check_build();
    /* the first update: the shadow load (members 10..19), then the stock run (1-10) */
    originals=0;stores=0;frame();
    CHECK(originals==2u && stores==2u && sl.valid && sl.index==0x11u && WORD_AT(0x0078AEACu)==0x11u);
    for(k=0;k<10u;k++) CHECK(work_id(k)==100+(int)k && member(k)==100+(int)k && member(10u+k)==some_extras[10u+k]);
    for(k=0;k<10u;k++) CHECK(shadow_id(k)==some_extras[10u+k]);
    { int32_t d,p,var;memcpy(&d,sl.rows+2u*0x1Cu+8u,4);memcpy(&p,sl.rows+2u*0x1Cu+0x14u,4);memcpy(&var,sl.rows+2u*0x1Cu+0x18u,4);
      CHECK(d==0x1C && p==202+0x1000 && var==-1); }                          /* slot 13 */
    CHECK(WORD_AT(ESET)==10u && !sets.page && !sets.in_update);
    /* later frames: stock only */
    originals=0;frame();CHECK(originals==1u && work_id(0)==100);
    /* the row draw: rows 11-20 show their shadow row in work row k, then put it back */
    sum=work_sum();
    row_draw(12u);CHECK(seen_value==2u && seen_id==202u && seen_portrait==202u+0x1000u && WORD_AT(ITEMS+0x14u)==12u);
    CHECK(work_sum()==sum && shadow_id(2)==202);
    row_draw(19u);CHECK(seen_value==9u && seen_id==0xA8u);
    row_draw(11u);CHECK(seen_value==1u && seen_id==0xFFFFFFFFu);
    row_draw(3u);CHECK(seen_value==3u && seen_id==103u);
    row_draw(20u);CHECK(seen_value==20u);                                    /* not a slot row: stock */
    CHECK(work_sum()==sum);
    /* the row action: row index k + 2 while it runs */
    pane_at(pc_mapmaker_sets_row_action,15u,0u);
    CHECK(seen_index==7u && seen_id==0xFFFFFFFFu && WORD_AT(PANE+0x184u)==17u && WORD_AT(PANE+0x18Cu)==15u && work_sum()==sum);
    pane_at(pc_mapmaker_sets_row_action,4u,0u);CHECK(seen_index==6u && seen_id==104u);
    /* the pane update: hints/preview for the shadow row; X clears slot 13 */
    pane_at(pc_mapmaker_sets_pane,12u,0u);
    CHECK(seen_index==4u && seen_hl==2u && seen_id==202u && WORD_AT(PANE+0x184u)==14u && WORD_AT(PANE+0x18Cu)==12u);
    pane_at(pc_mapmaker_sets_pane,12u,0x00800000u);
    CHECK(shadow_id(2)==-1 && member(12)==-1 && work_id(2)==102 && member(2)==102 && work_sum()==sum);
    pane_at(pc_mapmaker_sets_pane,2u,0u);CHECK(seen_index==4u && seen_hl==2u && seen_id==102u);
    /* nothing highlighted (-2): +0x18C left alone */
    WORD_AT(PANE+0x184u)=14u;WORD_AT(PANE+0x18Cu)=0xFFFFFFFEu;WORD_AT(PANE+0xC4u)=0;
    CALL(pc_mapmaker_sets_pane,fake_row_cb,0x0007AD7Bu,PANE,0u);
    CHECK(seen_hl==0xFFFFFFFEu && WORD_AT(PANE+0x18Cu)==0xFFFFFFFEu && WORD_AT(PANE+0x184u)==14u);
    /* the pick: slot 16 gets it, the highlight moves to 17 (stock's own move stays off) */
    pick_at(15u,0);
    CHECK(seen_index==7u && member(15)==77 && HALF_AT(ESET+4u+8u*15u+6u)==1 && BYTE_AT(ESET+4u+8u*15u+3u)==2u);
    CHECK(shadow_id(5)==77 && work_id(5)==105 && member(5)==105 && work_sum()==sum);
    CHECK(nadvanced==1u && advanced[0][0]==PANE && advanced[0][1]==16u && WORD_AT(PANE+0x18Cu)==16u && WORD_AT(PANE+0x184u)==17u);
    row_draw(15u);CHECK(seen_id==77u && seen_portrait==0x99u);
    /* slot 20: nowhere to go; slot 10 moves on to 11; slots 1-9: stock moves on */
    pick_at(19u,0);CHECK(member(19)==77 && nadvanced==0u);
    pick_at(9u,0);CHECK(work_id(9)==77 && nadvanced==1u && advanced[0][1]==10u && WORD_AT(PANE+0x18Cu)==10u);
    pick_at(3u,0);CHECK(work_id(3)==77 && nadvanced==1u && advanced[0][1]==4u);
    /* a disabled row picks nothing and does not move */
    pick_at(17u,1);CHECK(member(17)==-1 && nadvanced==0u && WORD_AT(PANE+0x18Cu)==17u);
    pick_at(9u,1);CHECK(nadvanced==0u);
    /* sub-mode 0x13 limits the rows: no extra move */
    BYTE_AT(0x0078ADAEu)=0x13u;pick_at(13u,0);CHECK(member(13)==77 && nadvanced==0u);BYTE_AT(0x0078ADAEu)=0;
    /* a preset: empty rows 11-20, no shadow load; slots 11-20 are not written */
    WORD_AT(0x00784024u)=3u;originals=0;frame();
    CHECK(originals==1u && sl.index==3u);
    for(k=0;k<10u;k++) CHECK(shadow_id(k)==-1);
    pane_at(pc_mapmaker_sets_pane,15u,0u);CHECK(seen_id==0xFFFFFFFFu && member(15)==77);
    row_draw(15u);CHECK(seen_id==0xFFFFFFFFu && member(15)==77);
    /* back to the custom set: loaded again from members 10..19 */
    WORD_AT(0x00784024u)=0x11u;originals=0;frame();
    CHECK(originals==2u && shadow_id(5)==77 && shadow_id(0)==200 && work_id(0)==100);
    /* a new build (the screen reopened) loads again */
    build_pane();CHECK(sl.on && !sl.valid);originals=0;frame();
    CHECK(originals==2u && sl.valid && shadow_id(0)==200 && shadow_id(5)==77 && work_id(0)==100);
    /* outside the editor the list is off */
    WORD_AT(0x0079094Cu)=101u;row_draw(12u);CHECK(seen_value==12u);
    pane_at(pc_mapmaker_sets_pane,12u,0u);CHECK(seen_index==14u);
    originals=0;frame();CHECK(originals==1u);
    WORD_AT(0x0079094Cu)=102u;
    /* diagnostics: the first pane update, moves past row 10, scroll changes */
    sl.opened=0;before=sets_diag_opens;
    pane_at(pc_mapmaker_sets_pane,0u,0u);CHECK(sl.opened && sets_diag_opens==before+1u);
    before=sets_diag_moves;
    pane_at(pc_mapmaker_sets_pane,3u,0u);CHECK(sets_diag_moves==before);       /* rows 1-10 */
    pane_at(pc_mapmaker_sets_pane,12u,0u);CHECK(sets_diag_moves==before+1u);   /* slot 13 */
    pane_at(pc_mapmaker_sets_pane,12u,0u);CHECK(sets_diag_moves==before+1u);   /* unchanged */
    FLOAT_AT(PANE+0x88u)=4.f;pane_at(pc_mapmaker_sets_pane,12u,0u);CHECK(sets_diag_moves==before+2u);
    FLOAT_AT(PANE+0x88u)=0.f;
    /* every build logs */
    before=sets_diag_opens;build_pane();CHECK(sets_diag_opens==before+1u);
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
    unsigned k;
    editor_set(some_extras);
    WORD_AT(0x0079094Cu)=102u;WORD_AT(0x00784024u)=0x11u;WORD_AT(0x0078AEACu)=0;
    /* the 20-row list leaves rows 1-10 in the work copy and the getters stock */
    build_pane();frame();
    CHECK(work_id(0)==100 && sets.page==0);
    refills=0;originals=0;
    CALL(pc_mapmaker_sets_match_start,fake_match_start,0x002D3933u,0u,0u);
    CHECK(refills==0u && originals==1u && roster[0]==100u && roster[9]==109u);
    /* the getters still shifted: the Test map start refills rows 1-10 */
    for(k=0;k<10u;k++) WORD_AT(WORK+k*0x1Cu)=(uint32_t)(int32_t)some_extras[10u+k];
    sets.page=1;
    refills=0;originals=0;g_eax=0x77u;
    CALL(pc_mapmaker_sets_match_start,fake_match_start,0x002D3933u,0u,0u);
    CHECK(refills==1u && refill_index==0x11u && originals==1u && g_eax==0x77u);
    CHECK(roster[0]==100u && roster[9]==109u && sets.page==0);
    CHECK(WORD_AT(0x0078AEACu)==0x11u);
    /* page 1 left: stock */
    refills=0;CALL(pc_mapmaker_sets_match_start,fake_match_start,0x002D3933u,0u,0u);
    CHECK(refills==0u && originals==2u);
    /* outside the editor: stock, the flag is dropped */
    for(k=0;k<10u;k++) WORD_AT(WORK+k*0x1Cu)=(uint32_t)(int32_t)some_extras[10u+k];
    sets.page=1;
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

/* ---- play: Story AIs on set slots 11-20 spawn in story mode ------------ */
#define LVL 0x00305000u
#define RECS 0x00306000u
static void spawn_mask_tests(void)
{
    static const uint16_t slot[6]={0,9,10,12,13,19};
    static const uint32_t mask[6]={0x700u,0x600u,0x701u,0x20u,0u,0xE00077Fu};
    unsigned k;
    WORD_AT(0x0074C36Cu)=LVL;WORD_AT(LVL+0x74u)=RECS;WORD_AT(LVL+0x78u)=6u;
    for(k=0;k<6u;k++) { HALF_AT(RECS+k*0x98u+0x16u)=slot[k];WORD_AT(RECS+k*0x98u+0xCu)=mask[k]; }
    WORD_AT(0x0079094Cu)=100u;originals=0;
    CALL(pc_mapmaker_sets_spawn_masks,fake_plain,0x0024245Bu,0u,0u);
    CHECK(originals==1u);
    CHECK(WORD_AT(RECS+0xCu)==0x700u && WORD_AT(RECS+0x98u+0xCu)==0x600u);           /* slots 1-10: as they were */
    for(k=2;k<6u;k++) CHECK(WORD_AT(RECS+k*0x98u+0xCu)==0x700u);                      /* 11-20: slot 1's mask */
    WORD_AT(RECS+3u*0x98u+0xCu)=0x20u;
    WORD_AT(0x0079094Cu)=102u;                                     /* the editor: stock */
    CALL(pc_mapmaker_sets_spawn_masks,fake_plain,0x0024245Bu,0u,0u);
    CHECK(WORD_AT(RECS+3u*0x98u+0xCu)==0x20u);
    WORD_AT(0x0079094Cu)=100u;
    set_env("TSFP_MAPMAKER_LIMITS",NULL);                          /* off: stock */
    CALL(pc_mapmaker_sets_spawn_masks,fake_plain,0x0024245Bu,0u,0u);
    CHECK(WORD_AT(RECS+3u*0x98u+0xCu)==0x20u);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    WORD_AT(LVL+0x78u)=0x401u;                                     /* an unreadable count: untouched */
    CALL(pc_mapmaker_sets_spawn_masks,fake_plain,0x0024245Bu,0u,0u);
    CHECK(WORD_AT(RECS+3u*0x98u+0xCu)==0x20u);
    WORD_AT(LVL+0x78u)=6u;WORD_AT(0x0074C36Cu)=0;
    CALL(pc_mapmaker_sets_spawn_masks,fake_plain,0x0024245Bu,0u,0u);
    CHECK(WORD_AT(RECS+3u*0x98u+0xCu)==0x20u);
    WORD_AT(0x0079094Cu)=102u;
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
    spawn_mask_tests();
    if(failures) { fprintf(stderr,"%d failure(s)\n",failures);return 1; }
    printf("mapmaker sets ok\n");
    return 0;
}
