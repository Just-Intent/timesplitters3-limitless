/* MapMaker Story HUD turret icons, US 45410066.
 *
 * In story mode ([7DE455] == 10) the HUD (sub_00349E40) walks gun pool slots
 * 0..[732D44]-1, the live gun count (0x0034A75E/0x0034A796), and draws each
 * live turret's icon with sub_00349970(hud, block) (call at 0x0034A78E).
 * The gun free list (sub_00156DB0) links block i to block i-1 with the last
 * block as its head, so allocation hands out the highest slots first. Live
 * turrets therefore sit above the scanned range and their icons never appear
 * in MapMaker Story maps.
 *
 * In MapMaker play (scene 100) the HUD hook shows the HUD the pool size (the
 * free-list total [732D38], 32 in the stock game) as the count for the call
 * and puts the live count back afterwards, keeping any change the call made.
 * The per-turret hook skips blocks on the free list (freed blocks keep stale
 * flags), so only live turrets draw. Other scenes, other modes and other
 * callers of sub_00349970 run the originals unchanged.
 *
 * Default on in MapMaker play; TSFP_MAPMAKER_TURRET_ICONS=0 keeps the
 * original (icon-less) HUD. Requires the includer's g_esp and
 * g_xbox_mem_offset. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef PC_TURRET_ICONS_PTR_OK
#define PC_TURRET_ICONS_PTR_OK(p,n) ((((uint32_t)(p))>=0x00010000u && ((uint32_t)(p))<0x04000000u-(uint32_t)(n)) || \
                                     (((uint32_t)(p))>=0x80000000u && ((uint32_t)(p))<0x84000000u-(uint32_t)(n)))
#endif
#define PC_TURRET_ICONS_U32(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define PC_TURRET_ICONS_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define PC_TURRET_ICONS_RET() (g_esp+=4u)         /* sub_00349970 ends in a plain ret */

#define PC_TURRET_ICONS_SCENE 0x0079094Cu         /* 100: MapMaker play */
#define PC_TURRET_ICONS_MODE 0x007DE455u          /* byte: 10 story */
#define PC_TURRET_ICONS_GUNS 0x00732D3Cu          /* gun pool, 0x1DC B per slot */
#define PC_TURRET_ICONS_FREE 0x00732D2Cu          /* free list {base, head, free, total} (sub_00156DB0) */
#define PC_TURRET_ICONS_COUNT 0x00732D44u         /* live guns */
#define PC_TURRET_ICONS_SIZE 0x1DCu
#define PC_TURRET_ICONS_CALL 0x0034A793u          /* return address of the HUD loop's icon call */
#define PC_TURRET_ICONS_MAX 256u                  /* bookkeeping bound; the stock pool is 32 */

static struct {
    int active;
    uint32_t pool,slots;
    unsigned char free_slot[PC_TURRET_ICONS_MAX];
    unsigned skipped;
} pc_turret_icons_state;

static int pc_turret_icons_switch=-1;           /* read once: -1 unread, 0 off, 1 on */
static int pc_turret_icons_enabled(void)
{
    if(pc_turret_icons_switch<0) {
        const char *value=getenv("TSFP_MAPMAKER_TURRET_ICONS");
        pc_turret_icons_switch=!(value && value[0]=='0' && !value[1]);
    }
    return pc_turret_icons_switch;
}

/* Mark the free-list blocks; 0 when the list does not look like the pool's. */
static int pc_turret_icons_free_list(uint32_t pool,uint32_t slots)
{
    uint32_t node=PC_TURRET_ICONS_U32(PC_TURRET_ICONS_FREE+4u),k,steps;
    memset(pc_turret_icons_state.free_slot,0,sizeof pc_turret_icons_state.free_slot);
    for(steps=0;node;steps++) {
        if(steps>=slots || node<pool || (node-pool)%PC_TURRET_ICONS_SIZE) return 0;
        k=(node-pool)/PC_TURRET_ICONS_SIZE;
        if(k>=slots || pc_turret_icons_state.free_slot[k]) return 0;
        pc_turret_icons_state.free_slot[k]=1;
        node=PC_TURRET_ICONS_U32(node);
    }
    return 1;
}

/* sub_00349E40: the story HUD. */
static void pc_turret_icons_hud(void (*original)(void))
{
    uint32_t live,pool,slots,after;
    if(pc_turret_icons_state.active || PC_TURRET_ICONS_U32(PC_TURRET_ICONS_SCENE)!=100u ||
       PC_TURRET_ICONS_B(PC_TURRET_ICONS_MODE)!=10u || !pc_turret_icons_enabled()) { original();return; }
    live=PC_TURRET_ICONS_U32(PC_TURRET_ICONS_COUNT);
    pool=PC_TURRET_ICONS_U32(PC_TURRET_ICONS_GUNS);
    slots=PC_TURRET_ICONS_U32(PC_TURRET_ICONS_FREE+0xCu);
    if(!live || live>slots || slots>PC_TURRET_ICONS_MAX ||
       !PC_TURRET_ICONS_PTR_OK(pool,slots*PC_TURRET_ICONS_SIZE) ||
       !pc_turret_icons_free_list(pool,slots)) { original();return; }
    pc_turret_icons_state.pool=pool;pc_turret_icons_state.slots=slots;
    pc_turret_icons_state.active=1;
    PC_TURRET_ICONS_U32(PC_TURRET_ICONS_COUNT)=slots;
    original();
    after=PC_TURRET_ICONS_U32(PC_TURRET_ICONS_COUNT);
    PC_TURRET_ICONS_U32(PC_TURRET_ICONS_COUNT)=live+(after-slots); /* the call changed it by after-slots */
    pc_turret_icons_state.active=0;
}

/* sub_00349970(hud, block): one turret's icon. */
static void pc_turret_icons_turret(void (*original)(void))
{
    uint32_t block,k;
    if(!pc_turret_icons_state.active || !PC_TURRET_ICONS_PTR_OK(g_esp,12) ||
       PC_TURRET_ICONS_U32(g_esp)!=PC_TURRET_ICONS_CALL) { original();return; }
    block=PC_TURRET_ICONS_U32(g_esp+8u);
    k=(block-pc_turret_icons_state.pool)/PC_TURRET_ICONS_SIZE;
    if(block>=pc_turret_icons_state.pool && k<pc_turret_icons_state.slots &&
       pc_turret_icons_state.free_slot[k]) {
        pc_turret_icons_state.skipped++;
        PC_TURRET_ICONS_RET();                     /* sub_00349970: ret */
        return;
    }
    original();
}
