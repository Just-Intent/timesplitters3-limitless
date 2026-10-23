/* Asset-free contract for src/pc_mapmaker_editor.inc: the story AI add
 * action (stock 50 unless TSFP_MAPMAKER_LIMITS=1, then 255), the editor
 * loader and level-start capacity guards (chunks past capacity hidden for the
 * duration of the original call and restored). */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x800000/4], g_eax, g_ecx, g_edx, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }

#define ED 0x00100000u
void sub_0003DFA0(void) { g_ecx=6; }
void sub_00300670(void) { g_ecx=1; }  /* distinct bodies: identical ones may be folded */
void sub_0031BB60(void) { g_ecx=2; }
void sub_0003D7B0(void) { g_ecx=3; }
void sub_0007A270(void) { g_ecx=4; }
void sub_003C85D4(void) { g_ecx=5; }
static int has_room=1, adds, messages;
static uint32_t last_text, last_sprintf_value, last_message_text;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t r=0,x[4]={0};
    int i;
    va_start(a,n);
    for(i=0;i<n && i<4;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00300670) r=has_room && x[0]==0x58u;
    else if(f==sub_0031BB60) {                        /* stock add: colour id 50 when exhausted */
        uint32_t c=WORD_AT(ED+0x280u),e=ED+0x11950u+c*0xE4u;
        WORD_AT(e+0x60u)=c<50u?c:50u;WORD_AT(ED+0x280u)=c+1u;adds++;r=0x1234u;
    } else if(f==sub_0003D7B0) { last_text=x[0];r=0x00700000u+x[0]; }
    else if(f==sub_003C85D4) last_sprintf_value=x[2];
    else if(f==sub_0007A270) { messages++;last_message_text=x[1];r=0x77u; }
    return r;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_editor.inc"

static int failures;
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

/* ---- containers --------------------------------------------------------- */
static uint32_t put_chunks(uint32_t c,uint32_t tag,unsigned n,uint32_t size)
{
    unsigned i;
    for(i=0;i<n;i++) { WORD_AT(c)=tag;WORD_AT(c+4)=size;WORD_AT(c+8)=i;c+=8u+size; }
    return c;
}
#define TAGS 6
static const uint32_t tag_list[TAGS]={0x00C7766Bu,0x00C7766Cu,0x00C7766Du,0x00C7766Eu,0x00C7766Fu,0x00C77669u};
static unsigned seen_by_original[TAGS], hidden_by_original, original_calls;
static uint32_t walk_from;
static void count_chunks(void)
{
    uint32_t c=walk_from;
    unsigned i,guard=0;
    memset(seen_by_original,0,sizeof seen_by_original);hidden_by_original=0;original_calls++;
    while(WORD_AT(c)!=0x00C7766Au && WORD_AT(c+4)<0x100000u && guard++<100000u) {
        if(WORD_AT(c)==0) hidden_by_original++;
        for(i=0;i<TAGS;i++) if(WORD_AT(c)==tag_list[i]) seen_by_original[i]++;
        c+=8u+WORD_AT(c+4);
    }
    g_eax=1;
}
static void fake_original_sai(void) { original_calls++; g_eax=0x55u; }
static unsigned count_tag(uint32_t c,uint32_t tag)
{
    unsigned n=0,guard=0;
    while(WORD_AT(c)!=0x00C7766Au && guard++<100000u) { n+=WORD_AT(c)==tag; c+=8u+WORD_AT(c+4); }
    return n;
}

int main(void)
{
    uint32_t c,end;
    unsigned i;
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x007844A8u)=ED;
    g_esp=0x00600000u;

    /* ---- add story AI: stock 50 when the limits are off ---- */
    set_env("TSFP_MAPMAKER_POOLS",NULL);set_env("TSFP_MAPMAKER_LIMITS",NULL);
    WORD_AT(g_esp+4u)=0x4242u;
    WORD_AT(ED+0x280u)=49;
    pc_mapmaker_editor_sai_add(fake_original_sai);
    CHECK(adds==1 && WORD_AT(ED+0x280u)==50 && g_eax==0x1234u && original_calls==0);
    pc_mapmaker_editor_sai_add(fake_original_sai);
    CHECK(adds==1 && WORD_AT(ED+0x280u)==50 && messages==1 && last_sprintf_value==50u &&
          last_message_text==ED && g_eax==0x77u);
    has_room=0;WORD_AT(ED+0x280u)=10;
    pc_mapmaker_editor_sai_add(fake_original_sai);
    CHECK(adds==1 && messages==2 && last_text==0x9B6u && last_message_text==0x00700000u+0x9B6u);
    has_room=1;

    /* ---- opted in: up to 255 ---- */
    set_env("TSFP_MAPMAKER_LIMITS","1");
    WORD_AT(ED+0x280u)=50;
    for(i=0;i<205;i++) pc_mapmaker_editor_sai_add(fake_original_sai);
    CHECK(WORD_AT(ED+0x280u)==255 && adds==206);
    /* colours: entries past 50 get index mod 50 instead of the stock 50 */
    CHECK(WORD_AT(ED+0x11950u+50u*0xE4u+0x60u)==0u && WORD_AT(ED+0x11950u+123u*0xE4u+0x60u)==123u%50u);
    CHECK(WORD_AT(ED+0x11950u+254u*0xE4u+0x60u)==254u%50u);
    pc_mapmaker_editor_sai_add(fake_original_sai);
    CHECK(WORD_AT(ED+0x280u)==255 && adds==206 && last_sprintf_value==255u);
    has_room=0;WORD_AT(ED+0x280u)=255;
    pc_mapmaker_editor_sai_add(fake_original_sai);
    CHECK(last_text==0x9B6u);
    has_room=1;

    /* ---- editor loader: chunks past capacity hidden, then restored ---- */
    c=0x00200000u;
    WORD_AT(c)=0x00C77667u;WORD_AT(c+4)=0x12Du;
    end=put_chunks(c+8u,0x00C77669u,5,0x40u);
    end=put_chunks(end,0x00C7766Cu,300,0x50u);   /* SAI: 255 fit */
    end=put_chunks(end,0x00C7766Bu,200,0x0Cu);   /* free objects: 170 (stock array) */
    end=put_chunks(end,0x00C7766Du,150,0xC0u);   /* nodes 130 */
    end=put_chunks(end,0x00C7766Eu,40,0x220u);   /* ops 30 */
    end=put_chunks(end,0x00C7766Fu,120,0x90u);   /* records 100 */
    WORD_AT(end)=0x00C7766Au;WORD_AT(end+4)=0;
    walk_from=c+8u;
    WORD_AT(g_esp+4u)=c;
    original_calls=0;
    pc_mapmaker_editor_load(count_chunks);
    CHECK(original_calls==1 && g_eax==1);
    CHECK(seen_by_original[1]==255 && seen_by_original[0]==170 && seen_by_original[2]==130 &&
          seen_by_original[3]==30 && seen_by_original[4]==100 && seen_by_original[5]==5);
    CHECK(hidden_by_original==45u+30u+20u+10u+20u);
    CHECK(count_tag(c+8u,0x00C7766Cu)==300 && count_tag(c+8u,0x00C7766Bu)==200 &&
          count_tag(c+8u,0x00C7766Fu)==120 && count_tag(c+8u,0)==0);   /* restored */
    /* a map within capacity is passed through untouched */
    end=put_chunks(c+8u,0x00C7766Cu,10,0x50u);WORD_AT(end)=0x00C7766Au;
    pc_mapmaker_editor_load(count_chunks);
    CHECK(seen_by_original[1]==10 && hidden_by_original==0);
    /* malformed (a chunk size past the scan bound): nothing hidden */
    end=put_chunks(c+8u,0x00C7766Cu,300,0x50u);
    WORD_AT(end)=0x00C7766Cu;WORD_AT(end+4)=0x7FFFFFF0u;
    original_calls=0;
    pc_mapmaker_editor_load(count_chunks);           /* stops at the oversized chunk */
    CHECK(original_calls==1 && hidden_by_original==0 && seen_by_original[1]==300);
    WORD_AT(end)=0x00C7766Au;
    CHECK(count_tag(c+8u,0)==0 && count_tag(c+8u,0x00C7766Cu)==300);
    /* wrong header: original only */
    WORD_AT(c)=0;original_calls=0;
    pc_mapmaker_editor_load(fake_original_sai);
    CHECK(original_calls==1);
    WORD_AT(c)=0x00C77667u;

    /* ---- level start: 1024 SAI slots ---- */
    WORD_AT(0x00784014u)=c;
    end=put_chunks(c+8u,0x00C77669u,3,0x40u);
    end=put_chunks(end,0x00C7766Cu,1100,0x50u);
    end=put_chunks(end,0x00C7766Du,600,0xC0u);  /* not capped at play time (heap by count) */
    WORD_AT(end)=0x00C7766Au;
    walk_from=c+8u;
    pc_mapmaker_editor_level_load(count_chunks);
    CHECK(seen_by_original[1]==1024 && seen_by_original[2]==600 && hidden_by_original==76);
    CHECK(count_tag(c+8u,0x00C7766Cu)==1100 && count_tag(c+8u,0)==0);

    if(failures) return 1;
    puts("mapmaker editor ok");
    return 0;
}
