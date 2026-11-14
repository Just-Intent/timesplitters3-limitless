/* System Link receive bounds, US 45410066.
 *
 * Three original System Link message handlers trust counts and indices read
 * from the packet. A malformed or hostile peer can make them write outside
 * their buffers. Each wrapper reads the same fields the original reads and
 * drops the message (returns as the original's own early exit does) when one
 * is out of range; every message a well-behaved peer sends is passed to the
 * original unchanged. Stock sizes only: no pool is resized here.
 *
 *  - pc_net_guard_fragment (sub_00352DF0, message fragment, type 0x2E,
 *    eax = packet): the original copies the fragment's word length (packet+8)
 *    into a 0x100-byte pool slot at +0x10 (0x00352E5E) and later into
 *    [0x0076ADF4] + index*0xEF (0x00352F7A), index = packet byte +10, with
 *    no bound; the reassembly buffer is 0x8000 bytes (sub_003526E0).
 *    Dropped: length > 0xF0 or index*0xEF + length > 0x8000.
 *  - pc_net_guard_turret_sync (sub_003596F0(packet)): count byte, then per
 *    entry a pool slot byte (x0x1DC into [0x00732D3C], 0x0035974B) and a
 *    player byte (x0x1584 into [0x007B0C48], 0x00359763; 0xFF = none). The
 *    whole message is dropped when a slot is past the gun pool (its free-list
 *    total [0x00732D38], 32 in the stock game) or a player is past the player
 *    structs ([0x00790950] of them, sub_000D3B00).
 *  - pc_net_guard_death (sub_00357D70(packet)): a peer's death effect. The
 *    player byte indexes [0x007B0C48]; the effect is appended by sub_002B23D0
 *    with no bound (0x002B24B1..CF) to the 30-entry list whose count
 *    [0x0075F6E8] follows it, so a 31st entry overwrote the count. The local
 *    adder sub_002B24E0 stops at 30. Dropped: player past the player structs,
 *    or the list already full.
 *
 * Every dropped message is counted and the first few of each kind logged.
 * Requires the includer's g_eax, g_esp and g_xbox_mem_offset. */
#include <stdint.h>
#include <stdio.h>

#ifndef PC_NET_GUARD_PTR_OK
#define PC_NET_GUARD_PTR_OK(p,n) ((((uint32_t)(p))>=0x00010000u && ((uint32_t)(p))<0x04000000u-(uint32_t)(n)) || \
                                  (((uint32_t)(p))>=0x80000000u && ((uint32_t)(p))<0x84000000u-(uint32_t)(n)))
#endif
#ifndef PC_NET_GUARD_STACK_OK
#define PC_NET_GUARD_STACK_OK(n) PC_NET_GUARD_PTR_OK(g_esp,n)
#endif
/* The handlers end in a plain `ret`: pop the return address. */
#define PC_NET_GUARD_RET() (g_esp+=4u)
#define PC_NET_GUARD_U32(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define PC_NET_GUARD_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

#define PC_NET_GUARD_HUMANS 0x00790950u        /* player structs allocated (sub_000D3B00) */
#define PC_NET_GUARD_PLAYERS 0x007B0C48u       /* player structs, 0x1584 B each */
#define PC_NET_GUARD_PLAYER_SIZE 0x1584u
/* fragment */
#define PC_NET_GUARD_FRAG_SLOT 0xF0u           /* 0x100-byte slot, data at +0x10 (0x00352E5B) */
#define PC_NET_GUARD_FRAG_STRIDE 0xEFu         /* imul edi,edi,0xEF @0x00352F7A */
#define PC_NET_GUARD_FRAG_BUFFER 0x8000u       /* [0x0076ADF4], sub_003526E0 push 0x8000 */
/* turret sync */
#define PC_NET_GUARD_GUN_TOTAL 0x00732D38u     /* gun free list {base, head, free, total} total (sub_00156DB0) */
/* death effect */
#define PC_NET_GUARD_EFFECTS 0x0075F6E8u       /* effect list count */
#define PC_NET_GUARD_EFFECTS_MAX 30u           /* stock list size; sub_002B24E0 stops here */

enum { PC_NET_GUARD_DROP_FRAGMENT, PC_NET_GUARD_DROP_TURRET, PC_NET_GUARD_DROP_DEATH_PLAYER,
       PC_NET_GUARD_DROP_DEATH_FULL, PC_NET_GUARD_DROPS };
static const char *const pc_net_guard_drop_name[PC_NET_GUARD_DROPS]={
    "System Link fragment out of range: dropped","turret sync slot/player out of range: message dropped",
    "System Link death effect for an unknown player: dropped","death effect list full: System Link effect skipped"};
static unsigned pc_net_guard_drops[PC_NET_GUARD_DROPS];

static void pc_net_guard_drop(unsigned kind,uint32_t a,uint32_t b)
{
    if(pc_net_guard_drops[kind]++<3u) {
        fprintf(stderr,"[PC-NET] %s (%u, %u)\n",pc_net_guard_drop_name[kind],(unsigned)a,(unsigned)b);
        fflush(stderr);
    }
}

/* ---- message fragment receiver (sub_00352DF0, eax = packet) ------------ */
static void pc_net_guard_fragment(void (*original)(void))
{
    uint32_t p=g_eax,len,index;
    if(!PC_NET_GUARD_PTR_OK(p,12)) { original();return; }
    len=PC_NET_GUARD_U32(p+8u)&0xFFFFu;            /* 0x00352E33 */
    index=PC_NET_GUARD_B(p+10u);                   /* 0x00352E3E -> slot +0xE */
    if(len>PC_NET_GUARD_FRAG_SLOT || index*PC_NET_GUARD_FRAG_STRIDE+len>PC_NET_GUARD_FRAG_BUFFER) {
        pc_net_guard_drop(PC_NET_GUARD_DROP_FRAGMENT,index,len);
        g_eax=0;                                   /* as the original's "no free slot" exit */
        PC_NET_GUARD_RET();                        /* sub_00352DF0: ret */
        return;
    }
    original();
}

/* ---- turret sync receiver (sub_003596F0(packet)) ----------------------- */
static void pc_net_guard_turret_sync(void (*original)(void))
{
    uint32_t p,n,i,slots,humans;
    if(!PC_NET_GUARD_STACK_OK(8)) { original();return; }
    p=PC_NET_GUARD_U32(g_esp+4u);
    if(!PC_NET_GUARD_PTR_OK(p,1)) { original();return; }
    n=PC_NET_GUARD_B(p);
    if(!PC_NET_GUARD_PTR_OK(p,1u+3u*n)) { original();return; }
    slots=PC_NET_GUARD_U32(PC_NET_GUARD_GUN_TOTAL);
    humans=PC_NET_GUARD_U32(PC_NET_GUARD_HUMANS);
    for(i=0;i<n;i++) {
        uint32_t slot=PC_NET_GUARD_B(p+1u+3u*i),player=PC_NET_GUARD_B(p+3u+3u*i);
        if(slot>=slots || (player!=0xFFu && player>=humans)) {
            pc_net_guard_drop(PC_NET_GUARD_DROP_TURRET,slot,player);
            g_eax=0;
            PC_NET_GUARD_RET();                    /* sub_003596F0: ret */
            return;
        }
    }
    original();
}

/* ---- death effect (sub_00357D70(packet)) ------------------------------- */
static void pc_net_guard_death(void (*original)(void))
{
    uint32_t p,player,pl;
    if(!PC_NET_GUARD_STACK_OK(8)) { original();return; }
    p=PC_NET_GUARD_U32(g_esp+4u);
    if(!PC_NET_GUARD_PTR_OK(p,3)) { original();return; }
    player=PC_NET_GUARD_B(p);                      /* 0x00357D74 */
    if(player>=PC_NET_GUARD_U32(PC_NET_GUARD_HUMANS)) {
        pc_net_guard_drop(PC_NET_GUARD_DROP_DEATH_PLAYER,player,PC_NET_GUARD_U32(PC_NET_GUARD_HUMANS));
        g_eax=0;PC_NET_GUARD_RET();return;         /* sub_00357D70: ret */
    }
    pl=PC_NET_GUARD_U32(PC_NET_GUARD_PLAYERS)+player*PC_NET_GUARD_PLAYER_SIZE;
    if(!PC_NET_GUARD_PTR_OK(pl,8)) { original();return; }
    if((int32_t)PC_NET_GUARD_U32(pl+4u)<0 &&       /* 0x00357D90: the path that adds an effect */
       PC_NET_GUARD_U32(PC_NET_GUARD_EFFECTS)>=PC_NET_GUARD_EFFECTS_MAX) {
        pc_net_guard_drop(PC_NET_GUARD_DROP_DEATH_FULL,PC_NET_GUARD_U32(PC_NET_GUARD_EFFECTS),PC_NET_GUARD_EFFECTS_MAX);
        g_eax=0;PC_NET_GUARD_RET();return;
    }
    original();
}
