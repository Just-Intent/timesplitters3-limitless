/* Asset-free contract for src/interface/pc_mapmaker_turret_icons_guest.inc:
 * MapMaker Story HUD turret icons. The fake HUD repeats the original loop over
 * [0x00732D44] gun slots and calls the per-turret hook as 0x0034A78E does.
 * Every hook must leave esp as the original's plain `ret` does. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x800000/4], g_esp;
static ptrdiff_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define BYTE_AT(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#include "pc_mapmaker_turret_icons_guest.inc"

static int failures, esp_errors;
#define ESP0 0x00600000u
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

#define G 0x00320000u
#define SLOTS 32u                                       /* stock gun pool */
static uint32_t drawn[SLOTS+8u];
static unsigned ndrawn;
static int hud_free_during;
static uint32_t icon_ret=0x0034A793u;
static void fake_icon(void) { if(ndrawn<SLOTS+8u) drawn[ndrawn]=WORD_AT(g_esp+8u);ndrawn++;g_esp+=4u; }
static void fake_hud(void)
{
    uint32_t i;
    if(hud_free_during) WORD_AT(0x00732D44u)-=1u;          /* a turret freed during the call */
    for(i=0;i<WORD_AT(0x00732D44u)+(uint32_t)hud_free_during;i++) {
        uint32_t sp=g_esp;
        g_esp-=4u;WORD_AT(g_esp)=G+i*0x1DCu;                /* push edi (block) */
        g_esp-=4u;WORD_AT(g_esp)=0x00777000u;               /* push esi (hud) */
        g_esp-=4u;WORD_AT(g_esp)=icon_ret;                  /* call */
        pc_turret_icons_turret(fake_icon);
        if(g_esp!=sp-8u) esp_errors++;
        g_esp=sp;
    }
    g_esp+=4u;
}
/* sub_00156DB0 links block i to block i-1, head = the last block; allocate
 * 31, 30, 29 (highest first), then free 30. Live: 29 and 31. */
static void hud_pool(void)
{
    uint32_t i;
    memset((char *)memory+G,0,SLOTS*0x1DCu);
    for(i=0;i<SLOTS;i++) WORD_AT(G+i*0x1DCu)=i?G+(i-1u)*0x1DCu:0u;
    WORD_AT(0x00732D30u)=G+30u*0x1DCu;WORD_AT(G+30u*0x1DCu)=G+28u*0x1DCu;
    WORD_AT(0x00732D3Cu)=G;WORD_AT(0x00732D38u)=SLOTS;WORD_AT(0x00732D44u)=2u;
}
static void hud(void)
{
    ndrawn=0;g_esp=ESP0;WORD_AT(g_esp)=0x0034ABCDu;
    pc_turret_icons_hud(fake_hud);
    if(g_esp!=ESP0+4u) esp_errors++;
    g_esp=ESP0;
}
static void set_switch(const char *value)
{
#ifdef _WIN32
    _putenv_s("TSFP_MAPMAKER_TURRET_ICONS",value?value:"");
#else
    if(value) setenv("TSFP_MAPMAKER_TURRET_ICONS",value,1); else unsetenv("TSFP_MAPMAKER_TURRET_ICONS");
#endif
    pc_turret_icons_switch=-1;
}

int main(void)
{
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;

    /* default: on in MapMaker Story */
    set_switch(NULL);
    hud_pool();hud();
    CHECK(ndrawn==2 && drawn[0]==G+29u*0x1DCu && drawn[1]==G+31u*0x1DCu);
    CHECK(WORD_AT(0x00732D44u)==2u && !pc_turret_icons_state.active && pc_turret_icons_state.skipped==30u);
    set_switch("1");hud_pool();hud();CHECK(ndrawn==2 && drawn[0]==G+29u*0x1DCu);
    /* off switch: the original scan of slots 0..live-1 */
    set_switch("0");hud_pool();hud();
    CHECK(ndrawn==2 && drawn[0]==G && drawn[1]==G+0x1DCu && WORD_AT(0x00732D44u)==2u);
    set_switch(NULL);
    /* the call's own change to the live count is kept */
    hud_pool();hud_free_during=1;hud();hud_free_during=0;
    CHECK(WORD_AT(0x00732D44u)==1u);
    /* another caller of the icon routine: original */
    hud_pool();icon_ret=0x00400000u;hud();icon_ret=0x0034A793u;
    CHECK(ndrawn==SLOTS);
    /* story levels (not MapMaker) and other modes: stock */
    hud_pool();WORD_AT(0x0079094Cu)=1u;hud();WORD_AT(0x0079094Cu)=100u;
    CHECK(ndrawn==2 && drawn[0]==G);
    hud_pool();BYTE_AT(0x007DE455u)=3u;hud();BYTE_AT(0x007DE455u)=10u;
    CHECK(ndrawn==2 && drawn[0]==G);
    /* a free list that does not look like the pool's: stock */
    hud_pool();WORD_AT(G+28u*0x1DCu)=G+30u*0x1DCu;hud();     /* loops */
    CHECK(ndrawn==2 && drawn[0]==G && WORD_AT(0x00732D44u)==2u);
    hud_pool();WORD_AT(0x00732D38u)=PC_TURRET_ICONS_MAX+1u;hud();
    CHECK(ndrawn==2 && drawn[0]==G);
    hud_pool();WORD_AT(0x00732D44u)=0;hud();CHECK(ndrawn==0);
    /* outside the HUD call: original */
    g_esp=ESP0;WORD_AT(g_esp)=0x0034A793u;WORD_AT(g_esp+8u)=G+5u*0x1DCu;ndrawn=0;
    pc_turret_icons_turret(fake_icon);
    CHECK(ndrawn==1 && g_esp==ESP0+4u);

    if(failures || esp_errors) { fprintf(stderr,"%d failures, %d esp errors\n",failures,esp_errors);return 1; }
    printf("mapmaker turret icons ok\n");
    return 0;
}
