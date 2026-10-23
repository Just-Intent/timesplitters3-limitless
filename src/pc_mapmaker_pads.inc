/* US 45410066 MapMaker capacity: AI navigation pads (docs/mapmaker-pads.md).
 *
 * Cover pads ("Layout B", config/guest-code-patches/pads.json): pad handles
 * are regular = index (< 0x1F40), cover = 0x1F40 + index, runtime = base +
 * index (20 slots). The 88 same-length rewrites move the runtime base from
 * 0x209E to 0x4740, so the cover handle range is 10,240 entries in every
 * scene (stock: 350, or 1280 in scene 100 where sub_0009A420 alone knew the
 * larger range and runtime handles overlapped cover indices 350..369). Pad
 * ids (pad+0, saved maps, story files) are unchanged; runtime handles are
 * created at run time only (sub_0009D3B0, sub_0009A510), so story levels
 * (at most 350 cover pads) decode exactly as before. The cover pool itself is
 * sized by pc_display_mapmaker_pools (MAPMAKER_POOL_COVER).
 *
 * This file adds three bound checks the stock code lacks; each only refuses
 * work that would otherwise write past a fixed pool:
 *  - pc_mapmaker_pads_group_node wraps sub_0009B090, the group-level A* node
 *    allocator (relocated pool of MAPMAKER_PADS_GROUP_NODES at 0x008E0000):
 *    with the free list empty and the bump counter at capacity the node is
 *    not added, so the search skips that link instead of overwriting the
 *    counter, the list heads and the pad-level pool.
 *  - pc_mapmaker_pads_pad_node does the same for sub_0009B730 (pad-level
 *    pool, stock .bss 0x00712058, 1000 nodes, kept in place).
 *  - pc_mapmaker_pads_cover_topup wraps sub_0023F9D0, which tops a start-point
 *    set up to 16 cover pads with no bound (the "+60 at load"): in scene 100
 *    it is skipped when the cover count plus the 16 - n pads it would add
 *    passes the cover capacity (10,240 with the pools on, else stock 1280).
 * All three are cdecl and their callers ignore eax, so returning without
 * calling the original is a plain guest ret.
 *
 * Requires the includer's WORD_AT, guest_writable, g_esp and, from
 * pc_mapmaker_limits.inc, mapmaker_limits, mapmaker_limits_read,
 * MAPMAKER_LIMIT_COVER and MAPMAKER_POOL_COVER. */
#ifndef MAPMAKER_STACK_OK
/* Hot hooks check the guest stack with this instead of guest_writable,
 * whose VirtualQuery made a 186-piece map's AI setup take 13 minutes. */
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif
#ifndef MAPMAKER_GUEST_RET
#define MAPMAKER_GUEST_RET() (g_esp+=4u) /* see pc_mapmaker_limits.inc */
#endif

#define MAPMAKER_PADS_COVER_RANGE 10240u  /* handles 0x1F40..0x473F; runtime 0x4740..0x4753 */
#define MAPMAKER_PADS_STOCK_COVER 1280u   /* stock scene-100 cover pool (sub_0009A140) */
#define MAPMAKER_PADS_GROUP_NODES 8192u   /* pads.json: 0x20000 B of 16-byte nodes */
#define MAPMAKER_PADS_PAD_NODES 1000u     /* stock: 0x3E80 B at 0x00712058 */
#define MAPMAKER_PADS_SPARE 256u          /* stock spare: 1280 - 1024 */

/* The editor limit leaves the stock spare below the handle range, and the
 * pool holds the whole handle range. */
typedef char mapmaker_pads_cover_sizes[(MAPMAKER_LIMIT_COVER+MAPMAKER_PADS_SPARE<=MAPMAKER_PADS_COVER_RANGE &&
                                        MAPMAKER_POOL_COVER>=MAPMAKER_PADS_COVER_RANGE)?1:-1];

static struct { uint32_t group_refused, pad_refused, cover_refused; } mapmaker_pads;

/* An A* pool is full when its free list (dword head, -1 = empty) is empty and
 * its bump counter has reached capacity: the original would take node
 * [counter] past the end. */
static int mapmaker_pads_pool_full(uint32_t free_head,uint32_t counter,uint32_t capacity)
{
    /* free_head and counter are fixed image (.bss) addresses: always mapped. */
    return (int32_t)WORD_AT(free_head)<0 && WORD_AT(counter)>=capacity;
}

/* WRAP sub_0009B090 (group A* node insert; free list 0x0070E170, counter 0x00711FF8). */
void pc_mapmaker_pads_group_node(void (*original)(void))
{
    if(mapmaker_pads_pool_full(0x0070E170u,0x00711FF8u,MAPMAKER_PADS_GROUP_NODES)) {
        mapmaker_pads.group_refused++;
        MAPMAKER_GUEST_RET();
        return;
    }
    original();
}

/* WRAP sub_0009B730 (pad A* node insert; free list 0x00712044, counter 0x00715F20). */
void pc_mapmaker_pads_pad_node(void (*original)(void))
{
    if(mapmaker_pads_pool_full(0x00712044u,0x00715F20u,MAPMAKER_PADS_PAD_NODES)) {
        mapmaker_pads.pad_refused++;
        MAPMAKER_GUEST_RET();
        return;
    }
    original();
}

/* WRAP sub_0023F9D0(set, n, list): appends 16 - n cover pads (when 0 < n < 16
 * and one of the n listed pads is eligible) at cover index [0x007B65C0]. */
void pc_mapmaker_pads_cover_topup(void (*original)(void))
{
    uint32_t capacity,n,add,count;
    if(WORD_AT(0x0079094Cu)!=100u ||
       !MAPMAKER_STACK_OK(12)) {
        original();
        return;
    }
    mapmaker_limits_read();
    capacity=mapmaker_limits.pools?MAPMAKER_PADS_COVER_RANGE:MAPMAKER_PADS_STOCK_COVER;
    n=WORD_AT(g_esp+8u);
    add=(int32_t)n>0 && (int32_t)n<16?16u-n:0u;   /* 0x0023F9DC..0x0023F9F3 */
    count=WORD_AT(0x007B65C0u);
    if(count>capacity || add>capacity-count) {
        mapmaker_pads.cover_refused++;
        MAPMAKER_GUEST_RET();
        return;
    }
    original();
}
