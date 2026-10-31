/* Asset-free contract for src/interface/pc_mapmaker_characters.inc: in the
 * editor (scene 102) with TSFP_MAPMAKER_LIMITS=1, the character-set picker's
 * call of sub_001CD980 (return 0x002C1F35) reports every Arcade-table row with
 * a real unlock rule as available; every other caller, scene and switch state
 * gets the original's answer. The preload hook only logs. Fake originals pop
 * their return address like the lifted callees; every call checks esp.
 * The Arcade table is a synthetic copy of the stock layout (the Python test
 * checks the real rows). */
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
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }
void sub_0003DFA0(void) {}
static uint32_t guest_call(void (*f)(void),int n,...) { (void)f;(void)n;return 0; }
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_characters.inc"

#define ESP0 0x00600000u
#define TABLE 0x00521A18u
static int failures, originals;
static uint32_t stock_answer, seen_args[2];
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static void fake_eligible(void) { originals++;seen_args[0]=WORD_AT(g_esp+4u);seen_args[1]=WORD_AT(g_esp+8u);g_eax=stock_answer;g_esp+=4u; }
static void fake_preload(void) { originals++;g_eax=0x77u;g_esp+=4u; }
static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}
/* rows: id, variant, rule type */
static const struct { uint32_t id; int8_t variant; uint32_t rule; } rows[]={
    {0x0C,-1,1},  /* Cortez */
    {0x0A,2,1},{0x0A,0,5},{0x0A,1,5},  /* British soldiers: variants */
    {0x1B,-1,3},  /* Crow */
    {0x02,-1,1},  /* Khallos */
    {0xA8,-1,2},  /* giant robot */
    {0x50,-1,0},  /* synthetic: never-unlockable rule */
    {0x51,-1,9},  /* synthetic: unknown rule */
};
static void table(void)
{
    unsigned i;
    memset((void *)((uintptr_t)TABLE+g_xbox_mem_offset),0,150u*0x30u);
    for(i=0;i<150u;i++) WORD_AT(TABLE+i*0x30u+4u)=0xFFFFu;          /* no id */
    for(i=0;i<sizeof rows/sizeof rows[0];i++) {
        uint32_t r=TABLE+(i+3u)*0x30u;                               /* not from row 0 */
        WORD_AT(r+4u)=rows[i].id;BYTE_AT(r+1u)=(uint8_t)rows[i].variant;WORD_AT(r+0x14u)=rows[i].rule;
    }
    /* the last row is used too: the walk covers all 150 */
    WORD_AT(TABLE+149u*0x30u+4u)=0xAF;BYTE_AT(TABLE+149u*0x30u+1u)=0xFF;WORD_AT(TABLE+149u*0x30u+0x14u)=1;
}
static uint32_t ask(uint32_t ret,uint32_t id,int32_t variant,uint32_t stock,int line)
{
    int before=originals;
    stock_answer=stock;g_eax=0xEEEEu;
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4u)=id;WORD_AT(ESP0+8u)=(uint32_t)variant;
    pc_mapmaker_characters_eligible(fake_eligible);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"%s:%d: esp %+d\n",__FILE__,line,(int)(g_esp-ESP0));failures++; }
    if(originals!=before+1) { fprintf(stderr,"%s:%d: original ran %d times\n",__FILE__,line,originals-before);failures++; }
    if(seen_args[0]!=id || seen_args[1]!=(uint32_t)variant) { fprintf(stderr,"%s:%d: args changed\n",__FILE__,line);failures++; }
    if(WORD_AT(ESP0)!=ret || WORD_AT(ESP0+4u)!=id) { fprintf(stderr,"%s:%d: stack written\n",__FILE__,line);failures++; }
    g_esp=ESP0;
    return g_eax;
}
#define ASK(ret,id,v,stock) ask(ret,id,v,stock,__LINE__)
static void preload(uint32_t ret,uint32_t id,int line)
{
    int before=originals;
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4u)=id;WORD_AT(ESP0+8u)=id;
    pc_mapmaker_characters_preload(fake_preload);
    if(g_esp!=ESP0+4u || originals!=before+1 || g_eax!=0x77u) { fprintf(stderr,"%s:%d: preload contract\n",__FILE__,line);failures++; }
    g_esp=ESP0;
}

int main(void)
{
    static uint8_t snapshot[0x00A00000];
    g_xbox_mem_offset=(uintptr_t)memory;
    table();
    WORD_AT(0x0079094Cu)=102u;

    /* Off: stock answers everywhere. */
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    CHECK(ASK(0x002C1F35u,0xA8,-1,0)==0);
    CHECK(ASK(0x002C1F35u,0x0C,-1,1)==1);
    set_env("TSFP_MAPMAKER_LIMITS","0");
    CHECK(ASK(0x002C1F35u,0xA8,-1,0)==0);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    set_env("TSFP_MAPMAKER_POOLS","0");                  /* limits need the pools */
    CHECK(ASK(0x002C1F35u,0xA8,-1,0)==0);
    set_env("TSFP_MAPMAKER_POOLS",NULL);

    /* On, editor picker: locked rows with a real rule are offered. */
    CHECK(ASK(0x002C1F35u,0xA8,-1,0)==1);           /* giant robot, story rule */
    CHECK(ASK(0x002C1F35u,0x1B,-1,0)==1);           /* Crow, rule 3 */
    CHECK(ASK(0x002C1F35u,0x0A,0,0)==1);            /* exact variant */
    CHECK(ASK(0x002C1F35u,0x0A,1,0)==1);
    CHECK(ASK(0x002C1F35u,0xAF,-1,0)==1);           /* last row */
    CHECK(ASK(0x002C1F35u,0x0C,-1,1)==1);           /* unlocked stays 1 */
    CHECK(ASK(0x002C1F35u,0x0A,3,0)==0);            /* no such variant row */
    CHECK(ASK(0x002C1F35u,0x99,-1,0)==0);           /* not in the table */
    CHECK(ASK(0x002C1F35u,0x50,-1,0)==0);           /* rule 0: never */
    CHECK(ASK(0x002C1F35u,0x51,-1,0)==0);           /* unknown rule */
    CHECK(ASK(0x002C1F35u,0xFFFF,-1,0)==0);         /* empty rows (rule 0) are never offered */

    /* Other callers and scenes: stock. */
    CHECK(ASK(0x002C6D5Du,0xA8,-1,0)==0);              /* character select roster */
    CHECK(ASK(0x002C1F35u+1u,0xA8,-1,0)==0);
    CHECK(ASK(0x002C1F35u-5u,0xA8,-1,0)==0);
    WORD_AT(0x0079094Cu)=101u;                          /* front end: profile bot sets */
    CHECK(ASK(0x002C1F35u,0xA8,-1,0)==0);
    WORD_AT(0x0079094Cu)=100u;
    CHECK(ASK(0x002C1F35u,0xA8,-1,0)==0);
    WORD_AT(0x0079094Cu)=102u;

    /* Preload: log only, original exactly once, no memory written. */
    WORD_AT(0x00791D4Cu)=0x00700000u;
    WORD_AT(0x0053A3CCu+0xA8u*0x30u+4u)=0x123u;*(int16_t *)((uintptr_t)(0x0078B860u+0x123u*2u)+g_xbox_mem_offset)=5;
    WORD_AT(0x0053A3CCu+0x02u*0x30u+4u)=0x124u;*(int16_t *)((uintptr_t)(0x0078B860u+0x124u*2u)+g_xbox_mem_offset)=-1;
    WORD_AT(0x0053A3CCu+0x0Cu*0x30u+4u)=0xFFFFFFFFu;
    WORD_AT(0x0079094Cu)=100u;
    memcpy(snapshot,memory,sizeof snapshot);
    {
        unsigned before=chr_logged;
        preload(0x0023F33Fu,0xA8,__LINE__);
        CHECK(chr_logged==before);                     /* resolves: silent */
        preload(0x0023F37Fu,0x02,__LINE__);
        CHECK(chr_logged==before+1u);                  /* negative slot */
        preload(0x0023F33Fu,0x0C,__LINE__);
        CHECK(chr_logged==before+2u);                  /* no model */
        preload(0x0023F33Fu,0x99,__LINE__);
        CHECK(chr_logged==before+3u);                  /* not an Arcade character */
        preload(0x000CF2B5u,0x99,__LINE__);
        CHECK(chr_logged==before+3u);                  /* other caller: silent */
        WORD_AT(0x0079094Cu)=102u;
        preload(0x0023F33Fu,0x99,__LINE__);
        WORD_AT(0x0079094Cu)=100u;
        CHECK(chr_logged==before+3u);                  /* other scene: silent */
        /* the fixture writes only the stack frame */
        memset((uint8_t *)memory+ESP0,0,16);memset(snapshot+ESP0,0,16);
        CHECK(memcmp(snapshot,memory,sizeof snapshot)==0);
    }

    if(failures)return 1;
    puts("mapmaker characters ok");
    return 0;
}
