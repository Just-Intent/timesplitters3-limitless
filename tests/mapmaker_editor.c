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
        WORD_AT(ED+0x238u)|=0x40u;                     /* stock: scroll the list to the new row */
    } else if(f==sub_0003D7B0) { last_text=x[0];r=0x00700000u+x[0]; }
    else if(f==sub_003C85D4) last_sprintf_value=x[2];
    else if(f==sub_0007A270) { messages++;last_message_text=x[1];r=0x77u; }
    return r;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_editor.inc"

static int failures, esp_errors;
#define ESP0 0x00600000u
/* Every hook (replacing or calling its original) must leave esp as the
 * original's plain `ret` does: return address popped. */
#define HOOK(fn,orig) do { g_esp=ESP0; fn(orig); if(g_esp!=ESP0+4u) { fprintf(stderr,"%s:%d: esp %+d after %s\n",__FILE__,__LINE__,(int)(g_esp-ESP0),#fn);esp_errors++; } g_esp=ESP0; } while(0)
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
    g_esp+=4u;
}
static void fake_original_sai(void) { original_calls++; g_eax=0x55u; g_esp+=4u; }
static void fake_room(void) { g_eax=(uint32_t)has_room; g_esp+=4u; }
/* sub_00304330 stand-in: reads the per-type count of the held id and
 * increments it when below the stock cap it compares against (40/30). */
static uint32_t place_seen;
static int place_refuse;
static void fake_place(void)
{
    uint32_t e=WORD_AT(0x007844A8u),id=WORD_AT(e+0x2F34u);
    uint32_t a=id==0x46u?e+0x3068u:id==0x47u?e+0x306Au:0,cap=id==0x46u?40u:30u;
    g_esp+=4u;
    if(!a) { place_seen=WORD_AT(e+0x3068u)&0xFFFFu;return; }
    place_seen=(a&2u)?WORD_AT(a&~3u)>>16:WORD_AT(a)&0xFFFFu;
    if(place_seen<cap && !place_refuse) { if(a&2u) WORD_AT(a&~3u)+=1u<<16; else WORD_AT(a)+=1u; }
}
/* sub_002F7F30 stand-in: dst count of the source type vs stock cap. */
static uint32_t move_seen,move_type;
static void fake_move(void)
{
    static const uint32_t cap[3]={40u,30u,100u};
    uint32_t e=g_edx+8u+(0x543D0u-0x3070u)+g_eax*12u,a;
    move_type=WORD_AT(e+4u)>>24;a=g_ecx+move_type*2u;
    move_seen=(a&2u)?WORD_AT(a&~3u)>>16:WORD_AT(a)&0xFFFFu;
    if(move_seen<cap[move_type]) { if(a&2u) WORD_AT(a&~3u)+=1u<<16; else WORD_AT(a)+=1u; }
    g_eax=0x66u;
    g_esp+=4u;
}
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
    WORD_AT(ED+0x238u)=0x1001u;
    HOOK(pc_mapmaker_editor_sai_add,fake_original_sai);
    CHECK(adds==1 && WORD_AT(ED+0x280u)==50 && g_eax==0x1234u && original_calls==0);
    CHECK(WORD_AT(ED+0x238u)==0x1041u);                      /* limits off: stock jump kept */
    HOOK(pc_mapmaker_editor_sai_add,fake_original_sai);
    CHECK(adds==1 && WORD_AT(ED+0x280u)==50 && messages==1 && last_sprintf_value==50u &&
          last_message_text==ED && g_eax==0x77u);
    has_room=0;WORD_AT(ED+0x280u)=10;
    HOOK(pc_mapmaker_editor_sai_add,fake_original_sai);
    CHECK(adds==1 && messages==2 && last_text==0x9B6u && last_message_text==0x00700000u+0x9B6u);
    has_room=1;

    /* ---- opted in: up to 255 ---- */
    set_env("TSFP_MAPMAKER_LIMITS","1");
    WORD_AT(ED+0x280u)=50;
    WORD_AT(ED+0x238u)=0x1001u;
    HOOK(pc_mapmaker_editor_sai_add,fake_original_sai);
    CHECK(WORD_AT(ED+0x280u)==51 && WORD_AT(ED+0x238u)==0x1001u); /* no jump; other flags kept */
    WORD_AT(ED+0x238u)=0x40u;has_room=0;                        /* refused add: flag untouched */
    HOOK(pc_mapmaker_editor_sai_add,fake_original_sai);
    CHECK(WORD_AT(ED+0x280u)==51 && WORD_AT(ED+0x238u)==0x40u);
    has_room=1;WORD_AT(ED+0x238u)=0;
    for(i=0;i<204;i++) HOOK(pc_mapmaker_editor_sai_add,fake_original_sai);
    CHECK(WORD_AT(ED+0x280u)==255 && adds==206 && !(WORD_AT(ED+0x238u)&0x40u));
    /* colours: entries past 50 get index mod 50 instead of the stock 50 */
    CHECK(WORD_AT(ED+0x11950u+50u*0xE4u+0x60u)==0u && WORD_AT(ED+0x11950u+123u*0xE4u+0x60u)==123u%50u);
    CHECK(WORD_AT(ED+0x11950u+254u*0xE4u+0x60u)==254u%50u);
    HOOK(pc_mapmaker_editor_sai_add,fake_original_sai);
    CHECK(WORD_AT(ED+0x280u)==255 && adds==206 && last_sprintf_value==255u);
    has_room=0;WORD_AT(ED+0x280u)=255;
    HOOK(pc_mapmaker_editor_sai_add,fake_original_sai);
    CHECK(last_text==0x9B6u);
    has_room=1;

    /* ---- editor loader: chunks past capacity hidden, then restored ---- */
    c=0x00200000u;
    WORD_AT(c)=0x00C77667u;WORD_AT(c+4)=0x12Du;
    end=put_chunks(c+8u,0x00C77669u,5,0x40u);
    end=put_chunks(end,0x00C7766Cu,300,0x50u);   /* SAI: 255 fit */
    end=put_chunks(end,0x00C7766Bu,700,0x0Cu);   /* free objects 680 */
    end=put_chunks(end,0x00C7766Du,600,0xC0u);   /* nodes 520 */
    for(i=0;i<20;i++) {                          /* ops: 20 kind-1 (8 fit) then 110 kind-0 */
        WORD_AT(end)=0x00C7766Eu;WORD_AT(end+4)=0x220u;WORD_AT(end+8)=i;WORD_AT(end+12)=1u;end+=8u+0x220u;
    }
    end=put_chunks(end,0x00C7766Eu,110,0x220u);
    end=put_chunks(end,0x00C7766Fu,450,0x90u);   /* records 400 */
    WORD_AT(end)=0x00C7766Au;WORD_AT(end+4)=0;
    walk_from=c+8u;
    WORD_AT(g_esp+4u)=c;
    original_calls=0;
    HOOK(pc_mapmaker_editor_load,count_chunks);
    CHECK(original_calls==1 && g_eax==1);
    CHECK(seen_by_original[1]==255 && seen_by_original[0]==680 && seen_by_original[2]==520 &&
          seen_by_original[3]==118 && seen_by_original[4]==400 && seen_by_original[5]==5);
    CHECK(hidden_by_original==45u+20u+80u+12u+50u);
    CHECK(count_tag(c+8u,0x00C7766Cu)==300 && count_tag(c+8u,0x00C7766Bu)==700 &&
          count_tag(c+8u,0x00C7766Eu)==130 && count_tag(c+8u,0x00C7766Fu)==450 && count_tag(c+8u,0)==0);
    /* more than 120 ops of kind 0: the total applies */
    end=put_chunks(c+8u,0x00C7766Eu,125,0x220u);WORD_AT(end)=0x00C7766Au;
    HOOK(pc_mapmaker_editor_load,count_chunks);
    CHECK(seen_by_original[3]==120 && hidden_by_original==5);
    /* a map within capacity is passed through untouched */
    end=put_chunks(c+8u,0x00C7766Cu,10,0x50u);WORD_AT(end)=0x00C7766Au;
    HOOK(pc_mapmaker_editor_load,count_chunks);
    CHECK(seen_by_original[1]==10 && hidden_by_original==0);
    /* malformed (a chunk size past the scan bound): nothing hidden */
    end=put_chunks(c+8u,0x00C7766Cu,300,0x50u);
    WORD_AT(end)=0x00C7766Cu;WORD_AT(end+4)=0x7FFFFFF0u;
    original_calls=0;
    HOOK(pc_mapmaker_editor_load,count_chunks);           /* stops at the oversized chunk */
    CHECK(original_calls==1 && hidden_by_original==0 && seen_by_original[1]==300);
    WORD_AT(end)=0x00C7766Au;
    CHECK(count_tag(c+8u,0)==0 && count_tag(c+8u,0x00C7766Cu)==300);
    /* wrong header: original only */
    WORD_AT(c)=0;original_calls=0;
    HOOK(pc_mapmaker_editor_load,fake_original_sai);
    CHECK(original_calls==1);
    WORD_AT(c)=0x00C77667u;

    /* ---- level start: 1024 SAI slots ---- */
    WORD_AT(0x00784014u)=c;
    end=put_chunks(c+8u,0x00C77669u,3,0x40u);
    end=put_chunks(end,0x00C7766Cu,1100,0x50u);
    end=put_chunks(end,0x00C7766Du,600,0xC0u);  /* not capped at play time (heap by count) */
    WORD_AT(end)=0x00C7766Au;
    walk_from=c+8u;
    HOOK(pc_mapmaker_editor_level_load,count_chunks);
    CHECK(seen_by_original[1]==1024 && seen_by_original[2]==600 && hidden_by_original==76);
    CHECK(count_tag(c+8u,0x00C7766Cu)==1100 && count_tag(c+8u,0)==0);

    /* ---- logic limits ---- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    WORD_AT(0x0052E8E8u)=30u;
    WORD_AT(ED+0x41D8u)=9;WORD_AT(ED+0x41E0u)=29;
    HOOK(pc_mapmaker_editor_logic_can_add,fake_original_sai);
    CHECK(g_eax==1);
    WORD_AT(ED+0x41E0u)=30;
    HOOK(pc_mapmaker_editor_logic_can_add,fake_original_sai);
    CHECK(g_eax==0);
    WORD_AT(ED+0x41D8u)=10;WORD_AT(ED+0x41E0u)=12;
    HOOK(pc_mapmaker_editor_logic_can_add,fake_original_sai);
    CHECK(g_eax==0 && WORD_AT(0x0052E8E8u)==30u);          /* off: kind table stock */
    set_env("TSFP_MAPMAKER_LIMITS","1");
    HOOK(pc_mapmaker_editor_logic_can_add,fake_original_sai);
    CHECK(g_eax==1 && WORD_AT(0x0052E8E8u)==120u);
    WORD_AT(ED+0x41D8u)=39;WORD_AT(ED+0x41E0u)=119;
    HOOK(pc_mapmaker_editor_logic_can_add,fake_original_sai);
    CHECK(g_eax==1);
    WORD_AT(ED+0x41E0u)=120;
    HOOK(pc_mapmaker_editor_logic_can_add,fake_original_sai);
    CHECK(g_eax==0);
    WORD_AT(ED+0x41E0u)=50;WORD_AT(ED+0x41D8u)=40;
    HOOK(pc_mapmaker_editor_logic_can_add,fake_original_sai);
    CHECK(g_eax==0);
    /* full message */
    WORD_AT(g_esp+4u)=0x4343u;last_sprintf_value=0;
    HOOK(pc_mapmaker_editor_logic_full,fake_original_sai);
    CHECK(last_sprintf_value==40u && last_text==0xBB1u && last_message_text==ED && g_eax==0x77u);
    WORD_AT(ED+0x41D8u)=7;WORD_AT(ED+0x41E0u)=120;
    HOOK(pc_mapmaker_editor_logic_full,fake_original_sai);
    CHECK(last_sprintf_value==6u);
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    WORD_AT(ED+0x41D8u)=10;
    HOOK(pc_mapmaker_editor_logic_full,fake_original_sai);
    CHECK(last_sprintf_value==10u);
    /* objective: stock 10 when off, 40 when on, never past the ops capacity */
    original_calls=0;WORD_AT(ED+0x41D8u)=10;WORD_AT(ED+0x41E0u)=20;WORD_AT(ED+0x81A4u)=0;WORD_AT(ED+0xDD10u)=0;
    HOOK(pc_mapmaker_editor_objective,fake_original_sai);
    CHECK(original_calls==0 && g_eax==ED);
    WORD_AT(ED+0x41D8u)=9;
    HOOK(pc_mapmaker_editor_objective,fake_original_sai);
    CHECK(original_calls==1);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    WORD_AT(ED+0x41D8u)=39;
    HOOK(pc_mapmaker_editor_objective,fake_original_sai);
    CHECK(original_calls==2);
    WORD_AT(ED+0x41D8u)=40;
    HOOK(pc_mapmaker_editor_objective,fake_original_sai);
    CHECK(original_calls==2);
    WORD_AT(ED+0x41D8u)=5;WORD_AT(ED+0x41E0u)=120;
    HOOK(pc_mapmaker_editor_objective,fake_original_sai);
    CHECK(original_calls==2);
    WORD_AT(ED+0x41E0u)=5;WORD_AT(ED+0xDD10u)=393;
    HOOK(pc_mapmaker_editor_objective,fake_original_sai);
    CHECK(original_calls==2);
    WORD_AT(ED+0xDD10u)=392;
    HOOK(pc_mapmaker_editor_objective,fake_original_sai);
    CHECK(original_calls==3);
    WORD_AT(ED+0xDD10u)=0;
    /* nodes per type, counted in the enlarged node array */
    WORD_AT(ED+0x81A4u)=130;
    for(i=0;i<130;i++) WORD_AT(ED+0x2FB70u+i*0xB4u)=i<30?0u:i<80?1u:2u;
    WORD_AT(ED+0x81A8u)=7;                                    /* old array: ignored */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    WORD_AT(g_esp+4u)=0;HOOK(pc_mapmaker_editor_node_room,fake_original_sai);CHECK(g_eax==0);
    WORD_AT(g_esp+4u)=1;HOOK(pc_mapmaker_editor_node_room,fake_original_sai);CHECK(g_eax==0);
    WORD_AT(g_esp+4u)=3;HOOK(pc_mapmaker_editor_node_room,fake_original_sai);CHECK(g_eax==0);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    WORD_AT(g_esp+4u)=0;HOOK(pc_mapmaker_editor_node_room,fake_original_sai);CHECK(g_eax==1);
    WORD_AT(g_esp+4u)=2;HOOK(pc_mapmaker_editor_node_room,fake_original_sai);CHECK(g_eax==1);
    WORD_AT(ED+0x81A4u)=320;
    for(i=0;i<320;i++) WORD_AT(ED+0x2FB70u+i*0xB4u)=i<120?0u:1u;
    WORD_AT(g_esp+4u)=0;HOOK(pc_mapmaker_editor_node_room,fake_original_sai);CHECK(g_eax==0);
    WORD_AT(g_esp+4u)=1;HOOK(pc_mapmaker_editor_node_room,fake_original_sai);CHECK(g_eax==0);
    WORD_AT(g_esp+4u)=2;HOOK(pc_mapmaker_editor_node_room,fake_original_sai);CHECK(g_eax==1);
    /* container-space test: logic callers also see the array capacities */
    has_room=1;
    WORD_AT(g_esp+4u)=0x58u;WORD_AT(g_esp)=0x00324F70u;
    WORD_AT(ED+0x41E0u)=10;WORD_AT(ED+0x81A4u)=512;WORD_AT(ED+0xDD10u)=0;
    HOOK(pc_mapmaker_editor_logic_room,fake_room);
    CHECK(g_eax==1);
    WORD_AT(ED+0x81A4u)=513;
    HOOK(pc_mapmaker_editor_logic_room,fake_room);
    CHECK(g_eax==0);
    WORD_AT(g_esp)=0x00303A8Du;                               /* item placement: unaffected */
    HOOK(pc_mapmaker_editor_logic_room,fake_room);
    CHECK(g_eax==1);
    WORD_AT(g_esp)=0x003287D0u;WORD_AT(ED+0x81A4u)=0;WORD_AT(ED+0xDD10u)=393;
    HOOK(pc_mapmaker_editor_logic_room,fake_room);
    CHECK(g_eax==0);
    WORD_AT(ED+0xDD10u)=0;WORD_AT(ED+0x41E0u)=120;
    HOOK(pc_mapmaker_editor_logic_room,fake_room);
    CHECK(g_eax==0);
    WORD_AT(ED+0x41E0u)=0;has_room=0;
    HOOK(pc_mapmaker_editor_logic_room,fake_room);
    CHECK(g_eax==0);
    has_room=1;

    /* ---- play time: 45-slot logic table ---- */
    WORD_AT(0x00765E44u)=0x00300000u;
    WORD_AT(0x00300000u+0x1950u)=44;
    for(i=0;i<45;i++) WORD_AT(0x00300000u+0x13Cu+i*0x8Cu)=100u+i;
    WORD_AT(g_esp+4u)=0x00310000u;WORD_AT(0x00310000u+0xCu)=0x0000FFF0u;   /* record -16 */
    original_calls=0;
    HOOK(pc_mapmaker_editor_logic_slot,fake_original_sai);
    CHECK(original_calls==1);
    WORD_AT(0x00300000u+0x1950u)=45;
    HOOK(pc_mapmaker_editor_logic_slot,fake_original_sai);
    CHECK(original_calls==1 && g_eax==0);                     /* new record, table full: skipped */
    WORD_AT(0x00310000u+0xCu)=100u+44u;
    HOOK(pc_mapmaker_editor_logic_slot,fake_original_sai);
    CHECK(original_calls==2);                                 /* already has a slot */

    /* ---- free-standing objects ---- */
    {
        uint32_t clip=ED+0x5048u,main=ED+0x3068u,e;
        unsigned t;
        set_env("TSFP_MAPMAKER_LIMITS",NULL);
        /* placement: type 0 (id 0x46) at the stock 40 is left to the original */
        WORD_AT(ED+0x2F34u)=0x46u;WORD_AT(main)=40u|(3u<<16);WORD_AT(main+4)=0u|(43u<<16);
        place_seen=0xFFFFu;HOOK(pc_mapmaker_editor_free_place,fake_place);
        CHECK(place_seen==40u && (WORD_AT(main)&0xFFFFu)==40u && (WORD_AT(main)>>16)==3u); /* stock refusal */
        set_env("TSFP_MAPMAKER_LIMITS","1");
        WORD_AT(main)=40u|(3u<<16);
        HOOK(pc_mapmaker_editor_free_place,fake_place);                  /* shown 39, placed: 41 */
        CHECK(place_seen==39u && (WORD_AT(main)&0xFFFFu)==41u && (WORD_AT(main)>>16)==3u);
        WORD_AT(main)=159u;
        HOOK(pc_mapmaker_editor_free_place,fake_place);
        CHECK(place_seen==39u && (WORD_AT(main)&0xFFFFu)==160u);
        HOOK(pc_mapmaker_editor_free_place,fake_place);                  /* at 160: original sees 160 */
        CHECK(place_seen==160u);
        WORD_AT(ED+0x2F34u)=0x47u;WORD_AT(main)=5u|(30u<<16);   /* type 1: high half */
        place_refuse=1;HOOK(pc_mapmaker_editor_free_place,fake_place);   /* refused: count unchanged */
        CHECK(place_seen==29u && (WORD_AT(main)>>16)==30u && (WORD_AT(main)&0xFFFFu)==5u);
        place_refuse=0;
        WORD_AT(ED+0x2F34u)=0x30u;place_seen=7;
        HOOK(pc_mapmaker_editor_free_place,fake_place);                  /* other items: untouched */
        CHECK(place_seen==5u);
        /* move between sets: type from the source entry in the enlarged array */
        for(t=0;t<3;t++) {
            WORD_AT(clip)=0;WORD_AT(clip+4)=0;
            e=clip+8u+(ED+0x543D0u-0x3070u-ED)+2u*12u;          /* clipboard entry 2 */
            WORD_AT(e+4u)=(WORD_AT(e+4u)&0x00FFFFFFu)|(t<<24);
            WORD_AT(main)=0;WORD_AT(main+4)=0;
            if(t==0) WORD_AT(main)=40u;
            if(t==1) WORD_AT(main)=30u<<16;
            if(t==2) WORD_AT(main+4)=100u;
            g_eax=2;g_ecx=main;g_edx=clip;
            HOOK(pc_mapmaker_editor_free_move,fake_move);
            CHECK(move_type==t && move_seen==(t==0?39u:t==1?29u:99u));
            CHECK((t==0?WORD_AT(main)&0xFFFFu:t==1?WORD_AT(main)>>16:WORD_AT(main+4)&0xFFFFu)==(t==0?41u:t==1?31u:101u));
            CHECK(g_eax==0x66u);
        }
        set_env("TSFP_MAPMAKER_LIMITS",NULL);
        WORD_AT(main+4)=100u;g_eax=2;g_ecx=main;g_edx=clip;
        HOOK(pc_mapmaker_editor_free_move,fake_move);
        CHECK(move_seen==100u);                                      /* stock: original decides */
    }

    CHECK(esp_errors==0);
    if(failures) return 1;
    puts("mapmaker editor ok");
    return 0;
}
