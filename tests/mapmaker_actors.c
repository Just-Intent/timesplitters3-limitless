/* Asset-free contract for src/pc_mapmaker_actors.inc (MapMaker actors
 * area): the spawner's alive gate (raised only in a MapMaker map played
 * locally) and the AI slot guard that keeps every slot inside the actor pool
 * and the pair table. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x1000000/4], g_eax, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

#include "pc_mapmaker_actors.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

#define SCENE 0x0079094Cu
#define NETWORK 0x0076B108u
#define COUNT 0x0074C37Cu
#define HUMANS 0x00790950u
#define USED 0x007A2958u
#define POOLPTR 0x007356D8u
#define MODE 0x007DE454u                 /* byte +1 = [0x007DE455] */
#define POOL 0x00400000u
#define STACK 0x00700000u

static void set_env(const char *value)
{
#ifdef _WIN32
    _putenv_s("TSFP_MAPMAKER_ALIVE",value?value:"");
#else
    if(value) setenv("TSFP_MAPMAKER_ALIVE",value,1); else unsetenv("TSFP_MAPMAKER_ALIVE");
#endif
    memset(&mapmaker_actors,0,sizeof mapmaker_actors);
}

/* Fake originals pop their return address, as the lifted callee does. */
static int originals;
static void fake_check(void) { originals++; g_eax=1u; g_esp+=4u; }
static void fake_slot(void) { originals++; g_eax=0x1234u; g_esp+=4u; }
#define HOOK(h,f) do { uint32_t hook_esp_=g_esp; h(f); CHECK(g_esp==hook_esp_+4u); g_esp=hook_esp_; } while(0)

/* The two stock comparisons of sub_00164010 with the gate:
 * 0x001640EC cmp [gate],0x14 / jl (else only flag 0x20000 continues) and,
 * in scene 100, 0x001641A2 cmp [gate],0x14 / jle (else no spawn). */
static int spawner_allows(int flagged)
{
    int32_t gate=(int32_t)WORD_AT(MAPMAKER_ACTORS_GATE);
    if(!(gate<0x14) && !flagged) return 0;
    if(WORD_AT(SCENE)==100u && !(gate<=0x14)) return 0;
    return 1;
}
static int gate_allows(uint32_t count,int flagged)
{
    g_esp=STACK;WORD_AT(STACK)=0x001640E4u;
    WORD_AT(COUNT)=count;
    originals=0;g_eax=0;
    HOOK(pc_mapmaker_actors_spawn_gate,fake_check);
    CHECK(originals==1 && g_eax==1u);          /* the record check result is kept */
    return spawner_allows(flagged);
}
/* Highest live count from which a spawn is still allowed. */
static int last_allowed(int flagged)
{
    int c,last=-1;
    for(c=0;c<=40;c++) if(gate_allows((uint32_t)c,flagged)) last=c;
    return last;
}

static void actors(uint32_t humans,uint32_t used,int story)
{
    WORD_AT(POOLPTR)=POOL;WORD_AT(HUMANS)=humans;WORD_AT(USED)=used;
    WORD_AT(MODE)=story?0x0A00u:0x0500u;
    memset((char *)memory+POOL,0,32u*0xEA0u+0x100u);
}
static void occupy(uint32_t slot,int story)
{
    WORD_AT(POOL+slot*0xEA0u+(story?0x10u:0x18u))=0xA0000000u+slot;
}
/* Returns 1 when the original ran, 0 when the guard refused (eax -1). */
static int slot_call(void)
{
    g_esp=STACK;WORD_AT(STACK)=0x0016420Cu;
    originals=0;g_eax=0;
    HOOK(pc_mapmaker_actors_slot,fake_slot);
    if(originals) { CHECK(g_eax==0x1234u); return 1; }
    CHECK(g_eax==0xFFFFFFFFu);
    return 0;
}

int main(void)
{
    uint32_t i;
    unsigned drops;
    g_xbox_mem_offset=(uintptr_t)memory;
    set_env(NULL);

    /* Story level (scene 1): the stock throttle, exactly. */
    WORD_AT(SCENE)=1u;WORD_AT(NETWORK)=0;
    CHECK(last_allowed(0)==19);                 /* unflagged: count < 20 */
    CHECK(last_allowed(1)==40);                 /* flagged: no limit outside scene 100 */
    gate_allows(25u,0);CHECK(WORD_AT(MAPMAKER_ACTORS_GATE)==25u);

    /* MapMaker map in play: everything up to the pool. */
    WORD_AT(SCENE)=100u;
    CHECK(last_allowed(0)==31);                 /* -> at most 32 characters */
    CHECK(last_allowed(1)==32);
    gate_allows(25u,0);CHECK(WORD_AT(MAPMAKER_ACTORS_GATE)==13u);
    gate_allows(2u,0);CHECK((int32_t)WORD_AT(MAPMAKER_ACTORS_GATE)==-10);

    /* Scene 100 in a System Link session: stock (20 alive). */
    WORD_AT(NETWORK)=1u;
    CHECK(last_allowed(0)==19);
    CHECK(last_allowed(1)==20);
    WORD_AT(NETWORK)=0;

    /* The editor (scene 102) and other scenes keep the count. */
    WORD_AT(SCENE)=102u;gate_allows(25u,0);CHECK(WORD_AT(MAPMAKER_ACTORS_GATE)==25u);
    WORD_AT(SCENE)=100u;

    /* TSFP_MAPMAKER_ALIVE lowers the limit; out-of-range values are ignored. */
    set_env("24");CHECK(last_allowed(0)==23);CHECK(last_allowed(1)==24);
    set_env("20");CHECK(last_allowed(0)==19);CHECK(last_allowed(1)==20);
    set_env("8");CHECK(last_allowed(0)==7);
    set_env("33");CHECK(last_allowed(0)==31);
    set_env("0");CHECK(last_allowed(0)==31);
    set_env("12x");CHECK(last_allowed(0)==31);
    set_env(NULL);

    /* ---- AI slot guard (scene 100) ---- */
    WORD_AT(SCENE)=100u;
    /* One human, 10 used, all occupied: slot 11, inside. */
    actors(1u,10u,1);for(i=1;i<11u;i++) occupy(i,1);
    CHECK(slot_call()==1);
    /* 30 used: the original refuses by itself, the guard leaves it alone. */
    actors(1u,30u,1);for(i=1;i<31u;i++) occupy(i,1);
    CHECK(slot_call()==1);
    actors(2u,30u,1);for(i=2;i<32u;i++) occupy(i,1);
    CHECK(slot_call()==1);
    CHECK(mapmaker_actors.drops==0u);           /* not a drop: the original refuses */
    /* Two humans, 30 AI: slot 31 is the last pool slot. */
    actors(2u,29u,1);for(i=2;i<31u;i++) occupy(i,1);
    CHECK(slot_call()==1);
    /* Four humans: the 29th AI would take slot 32, past the 32-slot pool. */
    drops=mapmaker_actors.drops;
    actors(4u,28u,1);for(i=4;i<32u;i++) occupy(i,1);
    CHECK(slot_call()==0);
    CHECK(mapmaker_actors.drops==drops+1u);
    /* ...but a freed slot below is reused. */
    WORD_AT(POOL+9u*0xEA0u+0x10u)=0;
    CHECK(slot_call()==1);
    /* Story mode tests +0x10/+0x14, other modes +0x18/+0x1C. */
    actors(4u,28u,1);for(i=4;i<32u;i++) occupy(i,1);
    WORD_AT(POOL+9u*0xEA0u+0x18u)=1u;WORD_AT(POOL+9u*0xEA0u+0x10u)=0;
    CHECK(slot_call()==1);
    actors(4u,28u,1);for(i=4;i<32u;i++) occupy(i,1);
    WORD_AT(POOL+9u*0xEA0u+0x14u)=0;           /* +0x10 still occupied */
    CHECK(slot_call()==0);
    /* Outside story mode the pair table holds humans + 10 characters. */
    actors(2u,9u,0);for(i=2;i<11u;i++) occupy(i,0);
    CHECK(slot_call()==1);                      /* slot 11 < 12 */
    actors(2u,10u,0);for(i=2;i<12u;i++) occupy(i,0);
    CHECK(slot_call()==0);                      /* slot 12 */
    actors(2u,10u,1);for(i=2;i<12u;i++) occupy(i,1);
    CHECK(slot_call()==1);                      /* story: 32 */
    /* Other scenes and an unset pool run the original. */
    WORD_AT(SCENE)=1u;
    actors(4u,28u,1);for(i=4;i<32u;i++) occupy(i,1);
    CHECK(slot_call()==1);
    WORD_AT(SCENE)=100u;WORD_AT(POOLPTR)=0;
    CHECK(slot_call()==1);

    if(failures) { fprintf(stderr,"%d failures\n",failures); return 1; }
    printf("mapmaker actors ok\n");
    return 0;
}
