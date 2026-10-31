/* US 45410066 bug fixes next to MapMaker (docs/mapmaker/mapmaker-bugfix.md;
 * research: logs/limits/features/bugs.md, fixes 1, 3, 5, 6, 7).
 *
 * Always on (memory safety; a packet a well-behaved peer sends is passed to
 * the original unchanged):
 *  - pc_mapmaker_bugfix_net_fragment (WRAP sub_00352DF0, System Link message
 *    fragment, type 0x2E): the original copies the fragment's word length
 *    (packet+8) into a 0x100-byte pool slot at +0x10 (0x00352E5E) and later
 *    into [0x0076ADF4] + index*0xEF (0x00352F7A), index = packet byte +10,
 *    with no bound; the reassembly buffer is 0x8000 bytes (sub_003526E0) and
 *    the per-index flags are a byte array in its own frame (0x00352EA9).
 *    Dropped: length > 0xF0 or index*0xEF + length > 0x8000.
 *  - pc_mapmaker_bugfix_turret_sync (WRAP sub_003596F0): count byte, then per
 *    entry a pool slot byte (x0x1DC into [0x00732D3C], 0x0035974B) and a
 *    player byte (x0x1584 into [0x007B0C48], 0x00359763; 0xFF = none). The
 *    whole message is dropped when a slot is past the gun pool (its free list
 *    total [0x00732D38], 80 with objects.json) or a player is past the player
 *    structs ([0x00790950] of them, sub_000D3B00).
 *  - pc_mapmaker_bugfix_net_death (WRAP sub_00357D70): a peer's death
 *    effect. The player byte indexes [0x007B0C48] and the actor pool; the
 *    effect is appended by sub_002B23D0 with no bound (0x002B24B1..CF). Same
 *    rule as the local adder's hook (pc_mapmaker_actors_effects, sub_002B24E0):
 *    64 entries in wide play (scene 100, story, no session), else the stock 30.
 *    Dropped: player past the player structs, or the list at that bound.
 *  - pc_mapmaker_bugfix_bot_goal (WRAP sub_002B0320): arcade mode byte 1, a
 *    bot carrying ([esi+0x80]), its pad [esi+0x684] a runtime pad handle:
 *    0x002B035E loads the runtime pool [0x00715F38] over the handle and
 *    0x002B0363 returns the word 0x413D8 bytes before the pool (the
 *    function's initial -1 used as the handle). The intended value is the
 *    runtime pad's owner pad, word +8 of runtime[handle - base] (pads.json:
 *    runtime handles 0x4740..0x4753, 32-byte records; owner stores in
 *    sub_0009D3B0 @0x0009D40A/0x0009D428). That path returns it here; every
 *    other path runs the original.
 *
 * Behind TSFP_MAPMAKER_LIMITS=1 (visible change):
 *  - pc_mapmaker_bugfix_hud (WRAP sub_00349E40) and pc_mapmaker_bugfix_hud_turret
 *    (WRAP sub_00349970): in story mode the HUD walks gun pool slots
 *    0..[0x00732D44]-1 (live count, 0x0034A75E/0x0034A796), but the free list
 *    (sub_00156DB0) hands out the highest slot first, so live turrets are never
 *    in the scanned slots and their icons never appear. In scene 100 the hook
 *    shows the HUD the pool size as the count for the call (and puts the live
 *    count back, plus anything the call changed) and the per-turret hook skips
 *    slots on the free list (freed blocks keep stale flags).
 *
 * Every dropped packet or skipped call is counted and the first few logged.
 * Hot hooks use MAPMAKER_PTR_OK / MAPMAKER_STACK_OK only (no guest_writable).
 * Requires the includer's WORD_AT, g_eax, g_esp, g_xbox_mem_offset and
 * mapmaker_limits (pc_mapmaker_limits.inc). */
#ifndef MAPMAKER_GUEST_RET
#define MAPMAKER_GUEST_RET() (g_esp+=4u) /* see pc_mapmaker_limits.inc */
#endif
#ifndef MAPMAKER_PTR_OK
#define MAPMAKER_PTR_OK(p,n) ((((uint32_t)(p))>=0x00010000u && ((uint32_t)(p))<0x04000000u-(uint32_t)(n)) || \
                              (((uint32_t)(p))>=0x80000000u && ((uint32_t)(p))<0x84000000u-(uint32_t)(n)))
#endif
#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif
#include <stdio.h>
#include <string.h>

#define MAPMAKER_BUGFIX_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define MAPMAKER_BUGFIX_SCENE 0x0079094Cu
#define MAPMAKER_BUGFIX_MODE 0x007DE455u          /* byte: 1 arcade sub-mode, 10 story */
#define MAPMAKER_BUGFIX_SESSION 0x0076B108u       /* System Link session (sub_003533F0) */
#define MAPMAKER_BUGFIX_HUMANS 0x00790950u        /* player structs allocated (sub_000D3B00) */
#define MAPMAKER_BUGFIX_PLAYERS 0x007B0C48u       /* player structs, 0x1584 B each */
#define MAPMAKER_BUGFIX_PLAYER_SIZE 0x1584u
/* 1 */
#define MAPMAKER_BUGFIX_FRAG_SLOT 0xF0u           /* 0x100-byte slot, data at +0x10 (0x00352E5B) */
#define MAPMAKER_BUGFIX_FRAG_STRIDE 0xEFu         /* imul edi,edi,0xEF @0x00352F7A */
#define MAPMAKER_BUGFIX_FRAG_BUFFER 0x8000u       /* [0x0076ADF4], sub_003526E0 push 0x8000 */
/* 3, 6 */
#define MAPMAKER_BUGFIX_GUNS 0x00732D3Cu          /* gun pool, 0x1DC B per slot */
#define MAPMAKER_BUGFIX_GUN_FREE 0x00732D2Cu      /* free list {base, head, free, total} (sub_00156DB0) */
#define MAPMAKER_BUGFIX_GUN_COUNT 0x00732D44u     /* live guns */
#define MAPMAKER_BUGFIX_GUN_SIZE 0x1DCu
#define MAPMAKER_BUGFIX_GUN_MAX 80u               /* objects.json obj-turret-gun-nodes */
#define MAPMAKER_BUGFIX_HUD_CALL 0x0034A793u      /* return address of the HUD loop's call */
/* 5 */
#define MAPMAKER_BUGFIX_EFFECTS 0x0075F6E8u       /* effect list count (lists: actors.json, 64) */
#define MAPMAKER_BUGFIX_EFFECTS_WIDE 64u
#define MAPMAKER_BUGFIX_EFFECTS_STOCK 30u
/* 7 */
#define MAPMAKER_BUGFIX_RUNTIME 0x00715F38u       /* runtime pads, 20 x 32 B */
#define MAPMAKER_BUGFIX_RUNTIME_BASE 0x4740u      /* pads.json layout B (stock 0x209E) */
#define MAPMAKER_BUGFIX_RUNTIME_PADS 20u

enum { MAPMAKER_BUGFIX_DROP_FRAGMENT, MAPMAKER_BUGFIX_DROP_TURRET, MAPMAKER_BUGFIX_DROP_DEATH_PLAYER,
       MAPMAKER_BUGFIX_DROP_DEATH_FULL, MAPMAKER_BUGFIX_DROPS };
static const char *const mapmaker_bugfix_drop_name[MAPMAKER_BUGFIX_DROPS]={
    "System Link fragment out of range: dropped","turret sync slot/player out of range: message dropped",
    "System Link death effect for an unknown player: dropped","death effect list full: System Link effect skipped"};
static unsigned mapmaker_bugfix_drops[MAPMAKER_BUGFIX_DROPS];

static void mapmaker_bugfix_drop(unsigned kind,uint32_t a,uint32_t b)
{
    if(mapmaker_bugfix_drops[kind]++<3u) {
        fprintf(stderr,"[mapmaker] %s (%u, %u)\n",mapmaker_bugfix_drop_name[kind],(unsigned)a,(unsigned)b);
        fflush(stderr);
    }
}

/* ---- 1. System Link fragment receiver (sub_00352DF0, eax = packet) ------ */
void pc_mapmaker_bugfix_net_fragment(void (*original)(void))
{
    uint32_t p=g_eax,len,index;
    if(!MAPMAKER_PTR_OK(p,12)) { original();return; }
    len=WORD_AT(p+8u)&0xFFFFu;                     /* 0x00352E33 */
    index=MAPMAKER_BUGFIX_B(p+10u);                /* 0x00352E3E -> slot +0xE */
    if(len>MAPMAKER_BUGFIX_FRAG_SLOT || index*MAPMAKER_BUGFIX_FRAG_STRIDE+len>MAPMAKER_BUGFIX_FRAG_BUFFER) {
        mapmaker_bugfix_drop(MAPMAKER_BUGFIX_DROP_FRAGMENT,index,len);
        g_eax=0;                                   /* as the original's "no free slot" exit */
        MAPMAKER_GUEST_RET();                      /* sub_00352DF0: ret */
        return;
    }
    original();
}

/* ---- 3. turret sync receiver (sub_003596F0(packet)) --------------------- */
static uint32_t mapmaker_bugfix_gun_slots(void)
{
    uint32_t total=WORD_AT(MAPMAKER_BUGFIX_GUN_FREE+0xCu);
    return total>MAPMAKER_BUGFIX_GUN_MAX?MAPMAKER_BUGFIX_GUN_MAX:total;
}
void pc_mapmaker_bugfix_turret_sync(void (*original)(void))
{
    uint32_t p,n,i,slots,humans;
    if(!MAPMAKER_STACK_OK(8)) { original();return; }
    p=WORD_AT(g_esp+4u);
    if(!MAPMAKER_PTR_OK(p,1)) { original();return; }
    n=MAPMAKER_BUGFIX_B(p);
    if(!MAPMAKER_PTR_OK(p,1u+3u*n)) { original();return; }
    slots=mapmaker_bugfix_gun_slots();
    humans=WORD_AT(MAPMAKER_BUGFIX_HUMANS);
    for(i=0;i<n;i++) {
        uint32_t slot=MAPMAKER_BUGFIX_B(p+1u+3u*i),player=MAPMAKER_BUGFIX_B(p+3u+3u*i);
        if(slot>=slots || (player!=0xFFu && player>=humans)) {
            mapmaker_bugfix_drop(MAPMAKER_BUGFIX_DROP_TURRET,slot,player);
            g_eax=0;
            MAPMAKER_GUEST_RET();                  /* sub_003596F0: ret */
            return;
        }
    }
    original();
}

/* ---- 5. System Link death effect (sub_00357D70(packet)) ----------------- */
static int mapmaker_bugfix_wide(void)
{
    return WORD_AT(MAPMAKER_BUGFIX_SCENE)==100u && MAPMAKER_BUGFIX_B(MAPMAKER_BUGFIX_MODE)==10u &&
           !WORD_AT(MAPMAKER_BUGFIX_SESSION);
}
void pc_mapmaker_bugfix_net_death(void (*original)(void))
{
    uint32_t p,player,pl,cap;
    if(!MAPMAKER_STACK_OK(8)) { original();return; }
    p=WORD_AT(g_esp+4u);
    if(!MAPMAKER_PTR_OK(p,3)) { original();return; }
    player=MAPMAKER_BUGFIX_B(p);                   /* 0x00357D74 */
    if(player>=WORD_AT(MAPMAKER_BUGFIX_HUMANS)) {
        mapmaker_bugfix_drop(MAPMAKER_BUGFIX_DROP_DEATH_PLAYER,player,WORD_AT(MAPMAKER_BUGFIX_HUMANS));
        g_eax=0;MAPMAKER_GUEST_RET();return;       /* sub_00357D70: ret */
    }
    pl=WORD_AT(MAPMAKER_BUGFIX_PLAYERS)+player*MAPMAKER_BUGFIX_PLAYER_SIZE;
    if(!MAPMAKER_PTR_OK(pl,8)) { original();return; }
    if((int32_t)WORD_AT(pl+4u)<0) {                /* 0x00357D90: the path that adds an effect */
        cap=mapmaker_bugfix_wide()?MAPMAKER_BUGFIX_EFFECTS_WIDE:MAPMAKER_BUGFIX_EFFECTS_STOCK;
        if(WORD_AT(MAPMAKER_BUGFIX_EFFECTS)>=cap) {
            mapmaker_bugfix_drop(MAPMAKER_BUGFIX_DROP_DEATH_FULL,WORD_AT(MAPMAKER_BUGFIX_EFFECTS),cap);
            g_eax=0;MAPMAKER_GUEST_RET();return;
        }
    }
    original();
}

/* ---- 7. arcade bot goal (sub_002B0320(object)) ------------------------- */
void pc_mapmaker_bugfix_bot_goal(void (*original)(void))
{
    uint32_t obj,actor,h,pool;
    if(MAPMAKER_BUGFIX_B(MAPMAKER_BUGFIX_MODE)!=1u || !MAPMAKER_STACK_OK(8)) { original();return; }
    obj=WORD_AT(g_esp+4u);
    if(!MAPMAKER_PTR_OK(obj,0x80)) { original();return; }
    actor=WORD_AT(obj+0x7Cu);
    if(!MAPMAKER_PTR_OK(actor,0x688) || !WORD_AT(actor+0x80u)) { original();return; }
    h=WORD_AT(actor+0x684u);
    pool=WORD_AT(MAPMAKER_BUGFIX_RUNTIME);
    if(h-MAPMAKER_BUGFIX_RUNTIME_BASE>=MAPMAKER_BUGFIX_RUNTIME_PADS ||
       !MAPMAKER_PTR_OK(pool,MAPMAKER_BUGFIX_RUNTIME_PADS*32u)) { original();return; }
    g_eax=(uint32_t)(int32_t)(int16_t)(WORD_AT(pool+(h-MAPMAKER_BUGFIX_RUNTIME_BASE)*32u+8u)&0xFFFFu);
    MAPMAKER_GUEST_RET();                          /* sub_002B0320: ret (0x002B036D) */
}

/* ---- 6. story HUD turret icons (sub_00349E40 / sub_00349970) ----------- */
static struct {
    int active;
    uint32_t pool,slots;
    unsigned char free_slot[MAPMAKER_BUGFIX_GUN_MAX];
    unsigned skipped;
} mapmaker_bugfix_hud_state;

/* Mark the free-list blocks; 0 when the list does not look like the pool's. */
static int mapmaker_bugfix_free_list(uint32_t pool,uint32_t slots)
{
    uint32_t node=WORD_AT(MAPMAKER_BUGFIX_GUN_FREE+4u),k,steps;
    memset(mapmaker_bugfix_hud_state.free_slot,0,sizeof mapmaker_bugfix_hud_state.free_slot);
    for(steps=0;node;steps++) {
        if(steps>=slots || node<pool || (node-pool)%MAPMAKER_BUGFIX_GUN_SIZE) return 0;
        k=(node-pool)/MAPMAKER_BUGFIX_GUN_SIZE;
        if(k>=slots || mapmaker_bugfix_hud_state.free_slot[k]) return 0;
        mapmaker_bugfix_hud_state.free_slot[k]=1;
        node=WORD_AT(node);
    }
    return 1;
}
void pc_mapmaker_bugfix_hud(void (*original)(void))
{
    uint32_t live,pool,slots,after;
    mapmaker_limits_read();
    if(!mapmaker_limits.limits || WORD_AT(MAPMAKER_BUGFIX_SCENE)!=100u ||
       MAPMAKER_BUGFIX_B(MAPMAKER_BUGFIX_MODE)!=10u || mapmaker_bugfix_hud_state.active) { original();return; }
    live=WORD_AT(MAPMAKER_BUGFIX_GUN_COUNT);
    pool=WORD_AT(MAPMAKER_BUGFIX_GUNS);
    slots=mapmaker_bugfix_gun_slots();
    if(!live || live>slots || !MAPMAKER_PTR_OK(pool,slots*MAPMAKER_BUGFIX_GUN_SIZE) ||
       !mapmaker_bugfix_free_list(pool,slots)) { original();return; }
    mapmaker_bugfix_hud_state.pool=pool;mapmaker_bugfix_hud_state.slots=slots;
    mapmaker_bugfix_hud_state.active=1;
    WORD_AT(MAPMAKER_BUGFIX_GUN_COUNT)=slots;
    original();
    after=WORD_AT(MAPMAKER_BUGFIX_GUN_COUNT);
    WORD_AT(MAPMAKER_BUGFIX_GUN_COUNT)=live+(after-slots); /* the call changed it by after-slots */
    mapmaker_bugfix_hud_state.active=0;
}
/* sub_00349970(hud, block): one turret's icon. */
void pc_mapmaker_bugfix_hud_turret(void (*original)(void))
{
    uint32_t block,k;
    if(!mapmaker_bugfix_hud_state.active || !MAPMAKER_STACK_OK(12) ||
       WORD_AT(g_esp)!=MAPMAKER_BUGFIX_HUD_CALL) { original();return; }
    block=WORD_AT(g_esp+8u);
    k=(block-mapmaker_bugfix_hud_state.pool)/MAPMAKER_BUGFIX_GUN_SIZE;
    if(block>=mapmaker_bugfix_hud_state.pool && k<mapmaker_bugfix_hud_state.slots &&
       mapmaker_bugfix_hud_state.free_slot[k]) {
        mapmaker_bugfix_hud_state.skipped++;
        MAPMAKER_GUEST_RET();                      /* sub_00349970: ret */
        return;
    }
    original();
}
