/* Asset-free contract for src/interface/pc_mapmaker_bugfix.inc: System Link fragment,
 * turret sync and death-effect bounds, the arcade bot goal read and the
 * opt-in story HUD turret icons. Every hook must leave esp as the original's
 * plain `ret` does (return address popped), whether it calls it or not. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x800000/4], g_eax, g_ecx, g_edx, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define BYTE_AT(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }
void sub_0003DFA0(void) { g_ecx=6; }
static uint32_t guest_call(void (*f)(void),int n,...) { (void)f;(void)n;return 0; }
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_bugfix.inc"

static int failures, esp_errors;
#define ESP0 0x00600000u
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static unsigned calls;
static void fake(void) { calls++;g_eax=0x77u;g_esp+=4u; }
/* Call hook with arg at [esp+4]; returns 1 when the original ran. */
static int run(void (*hook)(void (*)(void)),uint32_t arg)
{
    calls=0;g_eax=0xEEEEu;
    g_esp=ESP0;WORD_AT(g_esp)=0x00123456u;WORD_AT(g_esp+4u)=arg;
    hook(fake);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"esp %+d\n",(int)(g_esp-ESP0));esp_errors++; }
    g_esp=ESP0;
    return calls==1;
}
static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}

/* ---- packets ----------------------------------------------------------- */
#define P 0x00300000u
static int fragment(uint32_t len,uint32_t index)
{
    WORD_AT(P+8u)=len|0xAB0000u;BYTE_AT(P+10u)=(uint8_t)index;BYTE_AT(P+11u)=200;
    g_eax=P;calls=0;g_esp=ESP0;WORD_AT(g_esp)=0x003555F3u;
    pc_mapmaker_bugfix_net_fragment(fake);
    if(g_esp!=ESP0+4u) esp_errors++;
    if(!calls && g_eax!=0) failures++;
    return calls==1;
}
static int turret(unsigned n,const uint8_t *e)
{
    unsigned i;
    BYTE_AT(P)=(uint8_t)n;
    for(i=0;i<3u*n;i++) BYTE_AT(P+1u+i)=e[i];
    return run(pc_mapmaker_bugfix_turret_sync,P);
}

/* ---- HUD: the original's loop over [0x00732D44] slots ------------------ */
#define G 0x00320000u
static uint32_t drawn[96];
static unsigned ndrawn;
static int hud_free_during;
static uint32_t icon_ret=0x0034A793u;
static void fake_icon(void) { drawn[ndrawn++]=WORD_AT(g_esp+8u);g_esp+=4u; }
static void fake_hud(void)
{
    uint32_t i;
    if(hud_free_during) WORD_AT(0x00732D44u)-=1u;          /* a turret freed during the call */
    for(i=0;i<WORD_AT(0x00732D44u)+(uint32_t)hud_free_during;i++) {
        uint32_t sp=g_esp;
        g_esp-=4u;WORD_AT(g_esp)=G+i*0x1DCu;                /* push edi (block) */
        g_esp-=4u;WORD_AT(g_esp)=0x00777000u;               /* push esi (hud) */
        g_esp-=4u;WORD_AT(g_esp)=icon_ret;                  /* call */
        pc_mapmaker_bugfix_hud_turret(fake_icon);
        if(g_esp!=sp-8u) esp_errors++;
        g_esp=sp;
    }
    g_esp+=4u;
}
static void hud_pool(void)
{
    uint32_t i;
    memset((char *)memory+G,0,80u*0x1DCu);
    for(i=0;i<80u;i++) WORD_AT(G+i*0x1DCu)=i?G+(i-1u)*0x1DCu:0u;   /* sub_00156DB0 */
    /* allocate 79, 78, 77 (highest first), then free 78 */
    WORD_AT(0x00732D30u)=G+78u*0x1DCu;WORD_AT(G+78u*0x1DCu)=G+76u*0x1DCu;
    WORD_AT(0x00732D3Cu)=G;WORD_AT(0x00732D38u)=80u;WORD_AT(0x00732D44u)=2u;
}
static void hud(void)
{
    ndrawn=0;g_esp=ESP0;WORD_AT(g_esp)=0x0034ABCDu;
    pc_mapmaker_bugfix_hud(fake_hud);
    if(g_esp!=ESP0+4u) esp_errors++;
    g_esp=ESP0;
}

int main(void)
{
    uint8_t e[12];
    g_xbox_mem_offset=(uintptr_t)memory;

    /* ---- 1. fragments ---- */
    CHECK(fragment(0xEF,0) && fragment(0xF0,0) && fragment(0,255-118));
    CHECK(fragment(0xEF,136) && fragment(25,137) && !fragment(26,137));
    CHECK(!fragment(0xF1,0) && !fragment(0xFFFF,0) && !fragment(1,255) && !fragment(0xEF,137));
    CHECK(fragment(0x80,1));
    g_eax=0x00000010u;calls=0;g_esp=ESP0;pc_mapmaker_bugfix_net_fragment(fake);  /* not guest RAM: original */
    CHECK(calls==1 && g_esp==ESP0+4u);
    CHECK(mapmaker_bugfix_drops[MAPMAKER_BUGFIX_DROP_FRAGMENT]==5u);

    /* ---- 3. turret sync ---- */
    WORD_AT(0x00732D38u)=80u;WORD_AT(0x00790950u)=4u;
    e[0]=79;e[1]=1;e[2]=3; e[3]=0;e[4]=0;e[5]=0xFF;
    CHECK(turret(2,e));
    e[3]=80;CHECK(!turret(2,e));
    e[3]=0;e[2]=4;CHECK(!turret(2,e));
    e[2]=3;CHECK(turret(0,e) && turret(1,e));
    WORD_AT(0x00732D38u)=32u;e[0]=32;CHECK(!turret(1,e));      /* stock pool */
    e[0]=31;CHECK(turret(1,e));
    WORD_AT(0x00732D38u)=500u;e[0]=80;CHECK(!turret(1,e));     /* never past 80 */

    /* ---- 5. death effects ---- */
    WORD_AT(0x007B0C48u)=0x00310000u;BYTE_AT(P)=2;WORD_AT(P+1u)=0x55u;
    WORD_AT(0x00310000u+2u*0x1584u+4u)=0xFFFFFFFFu;            /* the add path */
    WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;WORD_AT(0x0076B108u)=1u; /* System Link */
    WORD_AT(0x0075F6E8u)=29u;CHECK(run(pc_mapmaker_bugfix_net_death,P));
    WORD_AT(0x0075F6E8u)=30u;CHECK(!run(pc_mapmaker_bugfix_net_death,P));
    WORD_AT(0x0076B108u)=0u;                                   /* wide: 64 */
    CHECK(run(pc_mapmaker_bugfix_net_death,P));
    WORD_AT(0x0075F6E8u)=63u;CHECK(run(pc_mapmaker_bugfix_net_death,P));
    WORD_AT(0x0075F6E8u)=64u;CHECK(!run(pc_mapmaker_bugfix_net_death,P));
    BYTE_AT(0x007DE455u)=1u;WORD_AT(0x0075F6E8u)=30u;CHECK(!run(pc_mapmaker_bugfix_net_death,P));
    WORD_AT(0x00310000u+2u*0x1584u+4u)=0u;WORD_AT(0x0075F6E8u)=64u;
    CHECK(run(pc_mapmaker_bugfix_net_death,P));                /* no effect added: count irrelevant */
    BYTE_AT(P)=4;CHECK(!run(pc_mapmaker_bugfix_net_death,P));  /* player past the structs */
    BYTE_AT(P)=3;CHECK(run(pc_mapmaker_bugfix_net_death,P));

    /* ---- 7. arcade bot goal ---- */
    BYTE_AT(0x007DE455u)=1u;
    WORD_AT(0x00330000u+0x7Cu)=0x00331000u;WORD_AT(0x00331000u+0x80u)=1u;
    WORD_AT(0x00331000u+0x684u)=0x4740u+3u;
    WORD_AT(0x00715F38u)=0x00332000u;
    WORD_AT(0x00332000u+3u*32u+8u)=0x1234FFF5u;                /* owner pad word -11 */
    WORD_AT(0x00332000u+3u*32u+4u)=0x00000007u;
    CHECK(!run(pc_mapmaker_bugfix_bot_goal,0x00330000u) && g_eax==0xFFFFFFF5u);
    WORD_AT(0x00332000u+3u*32u+8u)=0x00000102u;
    CHECK(!run(pc_mapmaker_bugfix_bot_goal,0x00330000u) && g_eax==0x102u);
    WORD_AT(0x00331000u+0x684u)=0x4740u+19u;WORD_AT(0x00332000u+19u*32u+8u)=0x2Au;
    CHECK(!run(pc_mapmaker_bugfix_bot_goal,0x00330000u) && g_eax==0x2Au);
    WORD_AT(0x00331000u+0x684u)=0x4740u+20u;CHECK(run(pc_mapmaker_bugfix_bot_goal,0x00330000u));
    WORD_AT(0x00331000u+0x684u)=0x473Fu;CHECK(run(pc_mapmaker_bugfix_bot_goal,0x00330000u));
    WORD_AT(0x00331000u+0x684u)=0x209Eu;CHECK(run(pc_mapmaker_bugfix_bot_goal,0x00330000u)); /* stock base: a cover pad now */
    WORD_AT(0x00331000u+0x684u)=0x4741u;WORD_AT(0x00331000u+0x80u)=0;
    CHECK(run(pc_mapmaker_bugfix_bot_goal,0x00330000u));      /* not carrying */
    WORD_AT(0x00331000u+0x80u)=1u;BYTE_AT(0x007DE455u)=6u;
    CHECK(run(pc_mapmaker_bugfix_bot_goal,0x00330000u));      /* other modes: original */

    /* ---- 6. HUD turret icons ---- */
    set_env("TSFP_MAPMAKER_POOLS",NULL);set_env("TSFP_MAPMAKER_LIMITS",NULL);
    WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;
    hud_pool();hud();
    CHECK(ndrawn==2 && drawn[0]==G && drawn[1]==G+0x1DCu && WORD_AT(0x00732D44u)==2u);   /* stock */
    set_env("TSFP_MAPMAKER_LIMITS","1");
    hud_pool();hud();
    CHECK(ndrawn==2 && drawn[0]==G+77u*0x1DCu && drawn[1]==G+79u*0x1DCu);
    CHECK(WORD_AT(0x00732D44u)==2u && !mapmaker_bugfix_hud_state.active && mapmaker_bugfix_hud_state.skipped==78u);
    hud_pool();hud_free_during=1;hud();hud_free_during=0;
    CHECK(WORD_AT(0x00732D44u)==1u);                           /* the call's own change kept */
    hud_pool();icon_ret=0x00400000u;hud();icon_ret=0x0034A793u;
    CHECK(ndrawn==80u);                                         /* another caller: original */
    hud_pool();WORD_AT(0x0079094Cu)=1u;hud();WORD_AT(0x0079094Cu)=100u;
    CHECK(ndrawn==2 && drawn[0]==G);                           /* story levels: stock */
    hud_pool();BYTE_AT(0x007DE455u)=3u;hud();BYTE_AT(0x007DE455u)=10u;
    CHECK(ndrawn==0 || drawn[0]==G);
    hud_pool();WORD_AT(G+76u*0x1DCu)=G+78u*0x1DCu;hud();        /* free list loops: stock */
    CHECK(ndrawn==2 && drawn[0]==G && WORD_AT(0x00732D44u)==2u);
    hud_pool();WORD_AT(0x00732D44u)=0;hud();CHECK(ndrawn==0);
    g_esp=ESP0;                                                /* outside the HUD call: original */WORD_AT(g_esp)=0x0034A793u;WORD_AT(g_esp+8u)=G+5u*0x1DCu;ndrawn=0;
    pc_mapmaker_bugfix_hud_turret(fake_icon);
    CHECK(ndrawn==1 && g_esp==ESP0+4u);
    g_esp=ESP0;

    if(failures || esp_errors) { fprintf(stderr,"%d failures, %d esp errors\n",failures,esp_errors);return 1; }
    printf("mapmaker bugfix ok\n");
    return 0;
}
