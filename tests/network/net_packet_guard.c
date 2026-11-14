/* Asset-free contract for src/network/pc_net_packet_guard_guest.inc: System
 * Link fragment, turret sync and death-effect bounds. Every hook must leave
 * esp as the original's plain `ret` does (return address popped), whether it
 * calls the original or not. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t memory[0x800000/4], g_eax, g_esp;
static ptrdiff_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define BYTE_AT(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#include "network/pc_net_packet_guard_guest.inc"

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
    if(!calls && g_eax!=0) failures++;
    g_esp=ESP0;
    return calls==1;
}

#define P 0x00300000u
static int fragment(uint32_t len,uint32_t index)
{
    WORD_AT(P+8u)=len|0xAB0000u;BYTE_AT(P+10u)=(uint8_t)index;BYTE_AT(P+11u)=200;
    g_eax=P;calls=0;g_esp=ESP0;WORD_AT(g_esp)=0x003555F3u;
    pc_net_guard_fragment(fake);
    if(g_esp!=ESP0+4u) esp_errors++;
    if(!calls && g_eax!=0) failures++;
    return calls==1;
}
static int turret(unsigned n,const uint8_t *e)
{
    unsigned i;
    BYTE_AT(P)=(uint8_t)n;
    for(i=0;i<3u*n;i++) BYTE_AT(P+1u+i)=e[i];
    return run(pc_net_guard_turret_sync,P);
}

int main(void)
{
    uint8_t e[12];
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;

    /* ---- fragments: slot 0xF0 bytes, buffer 0x8000 = 137*0xEF + 25 ---- */
    CHECK(fragment(0xEF,0) && fragment(0xF0,0) && fragment(0,255-118));
    CHECK(fragment(0xEF,136) && fragment(25,137) && !fragment(26,137));
    CHECK(!fragment(0xF1,0) && !fragment(0xFFFF,0) && !fragment(1,255) && !fragment(0xEF,137));
    CHECK(fragment(0x80,1));
    g_eax=0x00000010u;calls=0;g_esp=ESP0;pc_net_guard_fragment(fake);  /* not guest RAM: original */
    CHECK(calls==1 && g_esp==ESP0+4u);
    CHECK(pc_net_guard_drops[PC_NET_GUARD_DROP_FRAGMENT]==5u);

    /* ---- turret sync: stock pool of 32 guns, 4 players ---- */
    WORD_AT(0x00732D38u)=32u;WORD_AT(0x00790950u)=4u;
    e[0]=31;e[1]=1;e[2]=3; e[3]=0;e[4]=0;e[5]=0xFF;
    CHECK(turret(2,e));
    e[3]=32;CHECK(!turret(2,e));                              /* second entry past the pool */
    e[3]=0;e[2]=4;CHECK(!turret(2,e));                        /* player past the structs */
    e[2]=3;CHECK(turret(0,e) && turret(1,e));
    e[0]=32;CHECK(!turret(1,e));
    WORD_AT(0x00732D38u)=0u;e[0]=0;CHECK(!turret(1,e));       /* no pool: every slot is out */
    WORD_AT(0x00732D38u)=64u;e[0]=63;CHECK(turret(1,e));      /* the pool's own total, not a constant */

    /* ---- death effects: stock 30-entry list ---- */
    WORD_AT(0x007B0C48u)=0x00310000u;BYTE_AT(P)=2;WORD_AT(P+1u)=0x55u;
    WORD_AT(0x00310000u+2u*0x1584u+4u)=0xFFFFFFFFu;            /* the add path */
    WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;WORD_AT(0x0076B108u)=0u;
    WORD_AT(0x0075F6E8u)=29u;CHECK(run(pc_net_guard_death,P));
    WORD_AT(0x0075F6E8u)=30u;CHECK(!run(pc_net_guard_death,P)); /* any mode: never past 30 */
    WORD_AT(0x0075F6E8u)=31u;CHECK(!run(pc_net_guard_death,P));
    WORD_AT(0x00310000u+2u*0x1584u+4u)=0u;WORD_AT(0x0075F6E8u)=30u;
    CHECK(run(pc_net_guard_death,P));                          /* no effect added: count irrelevant */
    BYTE_AT(P)=4;CHECK(!run(pc_net_guard_death,P));            /* player past the structs */
    BYTE_AT(P)=3;CHECK(run(pc_net_guard_death,P));

    if(failures || esp_errors) { fprintf(stderr,"%d failures, %d esp errors\n",failures,esp_errors);return 1; }
    printf("net packet guard ok\n");
    return 0;
}
