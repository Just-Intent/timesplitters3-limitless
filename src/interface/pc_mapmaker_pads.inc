/* US 45410066 MapMaker capacity: AI navigation pads (docs/mapmaker/mapmaker-pads.md).
 *
 * Handles ("Layout C", config/guest-code-patches/pads.json, 2026-11-07;
 * Layout B before): regular = index (0..0x3FFF), cover = 0x4000 + index
 * (14,336), runtime = 0x7800 + slot (20). The table moves the cover base
 * 0x1F40 -> 0x4000 (45 sites) and the runtime base 0x4740 -> 0x7800 (88
 * sites; stock 0x209E). Handles are created at run time only (sub_0009A3C0,
 * sub_0009A510, sub_0009D3B0) and never saved, so every scene decodes the
 * same pads as before (story levels: at most 2400 regular, 350 cover).
 *
 * Pad ids (pad+0, owner word pad+8, saved maps, story files) keep every
 * stock value: regular 0..7999 = 0x3E9 + index, cover = 0x2329 + index.
 * Regular pads from 8000 take 0x5B29 + (index - 8000) ("high" ids, above the
 * 14,336 cover ids, at most 0x7BE8, so int16 owner words and the editor's
 * uint16 words hold them). A map of at most 8000 regular pads therefore has
 * exactly the ids it had in every earlier build and in the stock game. The
 * hooks below make the id code accept high ids; each passes ids below
 * 0x5B29 to the original untouched:
 *  - pc_mapmaker_pads_cover_lookup (sub_0009A3C0, cover id -> handle, the
 *    branch sub_0009CC40 and sub_0006C1E0 take for ids >= 0x2329): a high id
 *    is looked up in the regular pool with sub_0009CC40's search;
 *  - pc_mapmaker_pads_pointer (sub_0009CCA0, id -> pad): high ids resolve to
 *    the regular pad; the 13 "id above 0x2328 is a cover id, take its owner"
 *    callers and sub_0033B100 get a scratch pad whose owner word holds the id
 *    they expect (the id itself, or 0x3E9 + index where the caller subtracts
 *    0x3E9);
 *  - pc_mapmaker_pads_runtime_new (sub_0009D3B0): a runtime pad whose owner
 *    has a high id gets the owner's +0x18 (the inline decode cannot find it);
 *  - pc_mapmaker_pads_tile_load (sub_0009A7A0): the loader numbers regular
 *    pads 0x3E9 + index and cover owners likewise; pads from 8000 are
 *    renumbered after each tile;
 *  - pc_mapmaker_pads_editor_id (sub_00332490, the editor's id of a regular
 *    pad) returns the high id; sub_002FA150 (group count) and sub_00334B20
 *    (item pads, base + local) work on 0x3E9 + index inside their scope and
 *    sub_00334B20's list is renumbered after it;
 *  - the stitcher and the start-point top-up (below) translate high ids.
 * pads.json also lets sub_0009A330 accept regular ids up to 0x7BE8.
 *
 * Bound checks the stock code lacks; each only refuses work that would
 * otherwise write past a fixed pool:
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
 *    passes the cover capacity (14,336 with the pools on, else stock 1280).
 *  - pc_mapmaker_pads_stitch wraps sub_0009DC70 (the stitcher): with the
 *    pools on, in scene 100, it cuts the stitch table where the next link
 *    would pass the link pool (MAPMAKER_POOL_LINKS), then runs the original.
 * All are cdecl; a hook that answers itself sets eax as the original would
 * and does the guest ret.
 *
 * Group distances (sub_00275F70 builds an N x N matrix of route costs with
 * N x N searches at load, read by sub_00275F40): above
 * MAPMAKER_PADS_MATRIX_DENSE groups pc_mapmaker_pads_group_matrix keeps the
 * load-time link state instead and pc_mapmaker_pads_group_distance computes
 * each entry on its first read, with the same search on that state.
 *
 * Requires the includer's WORD_AT, guest_writable, guest_call, g_esp, g_eax,
 * save_registers/restore_registers and, from pc_mapmaker_limits.inc,
 * mapmaker_limits, mapmaker_limits_read, MAPMAKER_LIMIT_COVER,
 * MAPMAKER_POOL_COVER, MAPMAKER_POOL_PADS, MAPMAKER_POOL_LINKS and
 * MAPMAKER_PTR_OK. */
#ifndef MAPMAKER_STACK_OK
/* Hot hooks check the guest stack with this instead of guest_writable,
 * whose VirtualQuery made a 186-piece map's AI setup take 13 minutes. */
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif
#ifndef MAPMAKER_GUEST_RET
#define MAPMAKER_GUEST_RET() (g_esp+=4u) /* see pc_mapmaker_limits.inc */
#endif

#define MAPMAKER_PADS_COVER_BASE 0x4000u   /* pads.json pads-coverbase-*: regular handles 0..0x3FFF */
#define MAPMAKER_PADS_COVER_RANGE 14336u   /* handles 0x4000..0x77FF */
#define MAPMAKER_PADS_RUNTIME_BASE 0x7800u /* pads.json pads-layoutc-*: 20 runtime handles 0x7800..0x7813 */
#define MAPMAKER_PADS_STOCK_COVER 1280u    /* stock scene-100 cover pool (sub_0009A140) */
#define MAPMAKER_PADS_GROUP_NODES 8192u    /* pads.json: 0x20000 B of 16-byte nodes */
#define MAPMAKER_PADS_PAD_NODES 1000u      /* stock: 0x3E80 B at 0x00712058 */
#define MAPMAKER_PADS_SPARE 256u           /* stock spare: 1280 - 1024 */
/* Pad ids: regular 0x3E9 + index below 8000, cover 0x2329 + index. The 13
 * "id above the threshold is a cover id, take its owner" compares
 * (sub_0023F300, sub_0023FD10, 9 in sub_00240C50, sub_00339410,
 * sub_00339560) read 0x1F40 in stock, which misreads regular pads
 * 7000..7999; pads.json moves them to the last low regular id. High regular
 * ids (index 8000 and up) follow the cover ids. */
#define MAPMAKER_PADS_ID_THRESHOLD 0x2328u /* pads.json pads-idthreshold-* */
#define MAPMAKER_PADS_REGULAR_ID 0x3E9u
#define MAPMAKER_PADS_COVER_ID 0x2329u
#define MAPMAKER_PADS_LOW_PADS 8000u       /* regular pads with stock ids */
#define MAPMAKER_PADS_HIGH_ID (MAPMAKER_PADS_COVER_ID+MAPMAKER_PADS_COVER_RANGE) /* 0x5B29 */
#define MAPMAKER_PADS_HIGH_END 0x7BE9u     /* pads.json pads-regid-0009a37e: last high id + 1 */

typedef char mapmaker_pads_regular_ids[(MAPMAKER_PADS_REGULAR_ID+MAPMAKER_PADS_LOW_PADS-1u<=MAPMAKER_PADS_ID_THRESHOLD &&
                                        MAPMAKER_PADS_ID_THRESHOLD+1u==MAPMAKER_PADS_COVER_ID &&
                                        MAPMAKER_POOL_PADS<=MAPMAKER_PADS_COVER_BASE &&
                                        MAPMAKER_PADS_HIGH_ID+MAPMAKER_POOL_PADS-MAPMAKER_PADS_LOW_PADS==MAPMAKER_PADS_HIGH_END &&
                                        MAPMAKER_PADS_HIGH_END<=0x7FFFu)?1:-1];
typedef char mapmaker_pads_handles[(MAPMAKER_PADS_COVER_BASE+MAPMAKER_PADS_COVER_RANGE==MAPMAKER_PADS_RUNTIME_BASE &&
                                    MAPMAKER_PADS_RUNTIME_BASE+20u<=0x7FFFu)?1:-1];

/* The editor limit leaves the stock spare below the handle range, and the
 * pool holds the whole handle range. */
typedef char mapmaker_pads_cover_sizes[(MAPMAKER_LIMIT_COVER+MAPMAKER_PADS_SPARE<=MAPMAKER_PADS_COVER_RANGE &&
                                        MAPMAKER_POOL_COVER>=MAPMAKER_PADS_COVER_RANGE)?1:-1];

static struct {
    uint32_t group_refused, pad_refused, cover_refused, stitch_refused;
    uint32_t high_lookups, high_renumbered, scratch;   /* counters for probes and fixtures */
    int linear;                                        /* > 0: editor ids stay 0x3E9 + index */
    uint32_t alias_n, alias_id[16];                    /* start-point top-up: high seed ids */
} mapmaker_pads;

/* Regular pad index -> id and back (-1: not a regular pad id). */
static uint32_t mapmaker_pads_id(uint32_t index)
{
    return index<MAPMAKER_PADS_LOW_PADS?MAPMAKER_PADS_REGULAR_ID+index:MAPMAKER_PADS_HIGH_ID+(index-MAPMAKER_PADS_LOW_PADS);
}
static int32_t mapmaker_pads_index(int32_t id)
{
    if(id>=(int32_t)MAPMAKER_PADS_REGULAR_ID && id<(int32_t)MAPMAKER_PADS_COVER_ID) return id-(int32_t)MAPMAKER_PADS_REGULAR_ID;
    if(id>=(int32_t)MAPMAKER_PADS_HIGH_ID && id<(int32_t)MAPMAKER_PADS_HIGH_END)
        return id-(int32_t)MAPMAKER_PADS_HIGH_ID+(int32_t)MAPMAKER_PADS_LOW_PADS;
    return -1;
}
/* sub_0009CC40's search of the regular pool ([0x00715F2C], [0x007B6664]
 * entries sorted by id), step for step: the handle, or -1. */
static int32_t mapmaker_pads_search(int32_t id)
{
    int32_t hi=(int32_t)WORD_AT(0x007B6664u),lo=0,mid=hi>>1,prev;
    uint32_t pool=WORD_AT(0x00715F2Cu);
    unsigned guard=0;
    if(mid==-1) return -1;
    do {
        int32_t v;
        if(!MAPMAKER_PTR_OK(pool+(uint32_t)mid*32u,4)) return -1;
        v=(int32_t)WORD_AT(pool+(uint32_t)mid*32u);
        if(id<v) hi=mid;
        else if(id>v) lo=mid;
        else return mid;
        prev=mid;
        mid=lo+((hi-lo)>>1);
    } while(mid!=prev && ++guard<64u);
    return -1;
}


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
 * and one of the n listed pads is eligible) at cover index [0x007B65C0].
 * A listed seed (record [set+0x24] + list[k] * 32, id at +4) is eligible
 * unless its id is a cover id (>= 0x2329) or -2 (0x0023FA20); a high regular
 * id would count as cover, so for the call each high seed id is replaced by
 * an alias (0x80000000 + k: below 0x2329, not -2) that pc_mapmaker_pads_pointer
 * resolves back, and restored afterwards (the original appends new records
 * after the listed ones and copies only +0/+8/+0x1C of a seed). */
void pc_mapmaker_pads_cover_topup(void (*original)(void))
{
    uint32_t capacity,n,add,count,set,list,base,k,rec[16],aliased=0;
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
    set=WORD_AT(g_esp+4u);list=WORD_AT(g_esp+12u);
    if(add && MAPMAKER_PTR_OK(set+0x24u,4) && MAPMAKER_PTR_OK(list,n*4u)) {
        base=WORD_AT(set+0x24u);
        for(k=0;k<n;k++) {
            uint32_t r=base+WORD_AT(list+k*4u)*32u;
            int32_t id;
            if(!MAPMAKER_PTR_OK(r+4u,4)) continue;
            id=(int32_t)WORD_AT(r+4u);
            if(id<(int32_t)MAPMAKER_PADS_HIGH_ID || id>=(int32_t)MAPMAKER_PADS_HIGH_END) continue;
            rec[aliased]=r+4u;mapmaker_pads.alias_id[aliased]=(uint32_t)id;
            WORD_AT(r+4u)=0x80000000u|aliased;
            aliased++;
        }
    }
    mapmaker_pads.alias_n=aliased;
    original();
    mapmaker_pads.alias_n=0;
    for(k=0;k<aliased;k++) WORD_AT(rec[k])=mapmaker_pads.alias_id[k];
}

/* WRAP sub_0009DC70 (stitcher, scene 100; called once per load from
 * sub_0009DEB0 0x0009DEDD): for each of the [0x007B677C] 6-byte entries
 * {u16 pad id a, u16 pad id b, u8 flags} of the table [0x007B6774] whose two
 * pads are active (pad +4 bit 0), it appends a link at [0x007B6660] with no
 * bound (0x0009DCDB..0x0009DD19), then calls sub_0009CD00 (teleporter and
 * door links) and clears the count. The editor counts half the side slots
 * (pc_mapmaker_limits.inc), which a map of mismatched doorways can pass; so
 * with the pools on the table is cut at the first entry that would take the
 * link pool past MAPMAKER_POOL_LINKS (those doorways get no AI link instead
 * of writing past the pool). Entries whose pads cannot be read count as
 * appending. */
void pc_mapmaker_pads_stitch(void (*original)(void))
{
    uint32_t n,table,pads,links,room,added=0,i;
    if(WORD_AT(0x0079094Cu)!=100u) { original(); return; }
    mapmaker_limits_read();
    if(!mapmaker_limits.pools) { original(); return; }
    n=WORD_AT(0x007B677Cu);table=WORD_AT(0x007B6774u);pads=WORD_AT(0x00715F2Cu);links=WORD_AT(0x007B6660u);
    /* The original takes pad index = id - 0x3E9 (0x0009DCB9, 0x0009DCC4):
     * a high id becomes 0x3E9 + index, which fits the u16 (at most 0x43E8). */
    for(i=0;(int32_t)n>0 && i<n && i<0x100000u;i++) {
        uint32_t e=table+i*6u,w,k;
        if(!MAPMAKER_PTR_OK(e,4)) continue;
        w=WORD_AT(e);
        for(k=0;k<2;k++) {
            int32_t id=(int32_t)(w>>(16*k)&0xFFFFu),g=mapmaker_pads_index(id);
            if(id>=(int32_t)MAPMAKER_PADS_HIGH_ID && g>=0)
                w=(w&~(0xFFFFu<<(16*k)))|((MAPMAKER_PADS_REGULAR_ID+(uint32_t)g)<<(16*k));
        }
        WORD_AT(e)=w;
    }
    room=links<MAPMAKER_POOL_LINKS?MAPMAKER_POOL_LINKS-links:0u;
    if((int32_t)n<=0 || n<=room) { original(); return; }     /* every entry fits */
    for(i=0;i<n;i++) {
        uint32_t e=table+i*6u,a,b,ok=1;
        if(MAPMAKER_PTR_OK(e,4)) {
            a=(WORD_AT(e)&0xFFFFu)-0x3E9u;b=(WORD_AT(e)>>16)-0x3E9u;
            if(a<MAPMAKER_POOL_PADS && b<MAPMAKER_POOL_PADS && MAPMAKER_PTR_OK(pads+a*32u+4u,4) &&
               MAPMAKER_PTR_OK(pads+b*32u+4u,4))
                ok=(WORD_AT(pads+a*32u+4u)&1u) && (WORD_AT(pads+b*32u+4u)&1u);
        }
        if(!ok) continue;
        if(added==room) break;
        added++;
    }
    if(i<n) {
        mapmaker_pads.stitch_refused+=n-i;
        fprintf(stderr,"[mapmaker] pads: link pool full (%u of %u): %u doorway entries not stitched\n",
                (unsigned)(links+added),(unsigned)MAPMAKER_POOL_LINKS,(unsigned)(n-i));
        fflush(stderr);
        WORD_AT(0x007B677Cu)=i;
    }
    original();
}

/* ---- pad ids past 8000 (see the header) -------------------------------- */
/* sub_0009CCA0's callers that read the owner word +8 of the result as "the
 * regular id": the 13 id-threshold sites (return addresses, each followed by
 * movsx reg, word [eax+8]). */
static const uint32_t mapmaker_pads_owner_rets[13]={
    0x0023F30Du,0x0023FDDFu,0x00241208u,0x00241310u,0x002413E0u,0x002416C8u,0x00241867u,
    0x00241F72u,0x00242042u,0x002420A2u,0x00242115u,0x00339455u,0x003395A4u};
/* sub_0033B100 0x0033B179: movsx eax, word [eax+8]; sub eax, 0x3E9 (owner -> index). */
#define MAPMAKER_PADS_OWNER_INDEX_RET 0x0033B179u
static int mapmaker_pads_owner_ret(uint32_t ret)
{
    unsigned i;
    for(i=0;i<13u;i++) if(mapmaker_pads_owner_rets[i]==ret) return 1;
    return 0;
}
/* A 32-byte pad below the guest stack whose owner word +8 holds v: the caller
 * reads it at once, before any push. */
static uint32_t mapmaker_pads_scratch(int32_t v)
{
    uint32_t p=(g_esp-0x80u)&~3u;
    memset((void *)((uintptr_t)p+g_xbox_mem_offset),0,32);
    WORD_AT(p+8u)=(uint32_t)v&0xFFFFu;
    mapmaker_pads.scratch++;
    return p;
}

/* WRAP sub_0009A3C0(id) -> cover handle or -1 (binary search of the cover
 * pool, then + 0x4000). Reached for every id >= 0x2329 (sub_0009CC40,
 * sub_0006C1E0, sub_0009CCA0, sub_0009D3B0, story sub_0009CE00): a high
 * regular id answers its regular handle from sub_0009CC40's search. */
void pc_mapmaker_pads_cover_lookup(void (*original)(void))
{
    int32_t id;
    if(!MAPMAKER_STACK_OK(8)) { original(); return; }
    id=(int32_t)WORD_AT(g_esp+4u);
    if(id<(int32_t)MAPMAKER_PADS_HIGH_ID || id>=(int32_t)MAPMAKER_PADS_HIGH_END) { original(); return; }
    mapmaker_pads.high_lookups++;
    g_eax=(uint32_t)mapmaker_pads_search(id);
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_0009CCA0(id) -> pad pointer or 0. Ids below 0x5B29 run the
 * original. A high id answers its regular pad; to an id-threshold caller a
 * scratch pad whose owner word is the id. After the original, sub_0033B100's
 * call gets a scratch owner of 0x3E9 + index when the cover pad's owner is a
 * high id (it subtracts 0x3E9 to index the pool). A start-point alias
 * (pc_mapmaker_pads_cover_topup) stands for its high seed id. */
void pc_mapmaker_pads_pointer(void (*original)(void))
{
    uint32_t esp=g_esp,ret,arg;
    int32_t id;
    if(!MAPMAKER_STACK_OK(8)) { original(); return; }
    ret=WORD_AT(esp);arg=WORD_AT(esp+4u);id=(int32_t)arg;
    if(mapmaker_pads.alias_n && (arg&0x80000000u) && (arg&0x7FFFFFFFu)<mapmaker_pads.alias_n)
        id=(int32_t)mapmaker_pads.alias_id[arg&0x7FFFFFFFu];
    if(id>=(int32_t)MAPMAKER_PADS_HIGH_ID && id<(int32_t)MAPMAKER_PADS_HIGH_END) {
        int32_t h=mapmaker_pads_search(id);
        uint32_t p=h<0?0u:WORD_AT(0x00715F2Cu)+(uint32_t)h*32u;
        mapmaker_pads.high_lookups++;
        if(mapmaker_pads_owner_ret(ret)) p=mapmaker_pads_scratch(id);
        g_eax=p;
        MAPMAKER_GUEST_RET();
        return;
    }
    original();
    if(ret==MAPMAKER_PADS_OWNER_INDEX_RET && g_eax && MAPMAKER_PTR_OK(g_eax,12) && (WORD_AT(g_eax+4u)&0x800u)) {
        int32_t owner=(int16_t)(WORD_AT(g_eax+8u)&0xFFFFu),g=mapmaker_pads_index(owner);
        if(owner>=(int32_t)MAPMAKER_PADS_HIGH_ID && g>=0)
            g_eax=mapmaker_pads_scratch((int32_t)(MAPMAKER_PADS_REGULAR_ID+(uint32_t)g));
    }
}

/* WRAP sub_0009D3B0(position, owner id or -1, flags) -> runtime handle or -1
 * (single caller 0x000C2B8B): copies the owner's +0x18 through an inline
 * id decode (0x0009D430..0x0009D482) that sends ids >= 0x2329 to the cover
 * pool; for a high owner it read past the cover pool's start (this file's
 * cover lookup answers a regular handle). Copy the right +0x18 after it. */
void pc_mapmaker_pads_runtime_new(void (*original)(void))
{
    uint32_t slot,pad;
    int32_t owner,h;
    original();
    slot=g_eax-MAPMAKER_PADS_RUNTIME_BASE;
    if(slot>=20u) return;
    pad=WORD_AT(0x00715F38u)+slot*32u;
    if(!MAPMAKER_PTR_OK(pad,0x20)) return;
    owner=(int16_t)(WORD_AT(pad+8u)&0xFFFFu);
    if(owner<(int32_t)MAPMAKER_PADS_HIGH_ID) return;
    h=mapmaker_pads_search(owner);
    WORD_AT(pad+0x18u)=h<0?0u:WORD_AT(WORD_AT(0x00715F2Cu)+(uint32_t)h*32u+0x18u);
    mapmaker_pads.high_lookups++;
}

/* WRAP sub_0009A7A0 (one tile's pads, links and stitch pairs, appended to the
 * pools; callers 0x00084FA9, 0x00085700): it numbers regular pads 0x3E9 +
 * index (0x0009A8BE) and cover owners 0x3E9 + owner index (0x0009AC24) and
 * reads the owner back by - 0x3E9 (0x0009AC4E) within the call. Afterwards
 * the pads from index 8000 get their high ids and this tile's cover pads
 * their owners' high ids (low word of +8 only; +0xA is another field). */
void pc_mapmaker_pads_tile_load(void (*original)(void))
{
    uint32_t rb=WORD_AT(0x007B6664u),cb=WORD_AT(0x007B65C0u),ra,ca,pool,cover,i;
    original();
    ra=WORD_AT(0x007B6664u);ca=WORD_AT(0x007B65C0u);
    if(ra<=MAPMAKER_PADS_LOW_PADS || ra>0x10000u || ca>0x10000u) return;
    pool=WORD_AT(0x00715F2Cu);cover=WORD_AT(0x00715F34u);
    for(i=rb>MAPMAKER_PADS_LOW_PADS?rb:MAPMAKER_PADS_LOW_PADS;i<ra && i<MAPMAKER_POOL_PADS;i++)
        if(MAPMAKER_PTR_OK(pool+i*32u,4) && WORD_AT(pool+i*32u)==MAPMAKER_PADS_REGULAR_ID+i) {
            WORD_AT(pool+i*32u)=mapmaker_pads_id(i);
            mapmaker_pads.high_renumbered++;
        }
    for(i=cb;i<ca && i<MAPMAKER_PADS_COVER_RANGE;i++) {
        uint32_t p=cover+i*32u+8u,w;
        int32_t owner;
        if(!MAPMAKER_PTR_OK(p,4)) continue;
        w=WORD_AT(p);owner=(int16_t)(w&0xFFFFu);
        if(owner>=(int32_t)MAPMAKER_PADS_COVER_ID && (uint32_t)owner-MAPMAKER_PADS_REGULAR_ID<MAPMAKER_POOL_PADS) {
            WORD_AT(p)=(w&0xFFFF0000u)|mapmaker_pads_id((uint32_t)owner-MAPMAKER_PADS_REGULAR_ID);
            mapmaker_pads.high_renumbered++;
        }
    }
}

/* WRAP sub_00332490(local id, piece) -> 0x3E9 + regular pads of the earlier
 * pieces + local index (the editor's id of a regular pad; 14 callers store it
 * in the map). Indices from 8000 answer their high id, except inside
 * sub_002FA150 / sub_00334B20, which add and subtract ids of one piece. */
void pc_mapmaker_pads_editor_id(void (*original)(void))
{
    uint32_t g;
    original();
    if(mapmaker_pads.linear>0 || (int32_t)g_eax<(int32_t)MAPMAKER_PADS_COVER_ID) return;
    g=g_eax-MAPMAKER_PADS_REGULAR_ID;
    if(g<MAPMAKER_POOL_PADS) g_eax=mapmaker_pads_id(g);
}
/* WRAP sub_002FA150(container, piece) -> pad groups of the piece (the
 * editor's group count, 0x0030117E): id - base arithmetic, ids stay linear. */
void pc_mapmaker_pads_editor_groups(void (*original)(void))
{
    mapmaker_pads.linear++;
    original();
    mapmaker_pads.linear--;
}
/* WRAP sub_00334B20(piece, position (8 bytes), u32 *out) -> count of the
 * piece's pads in the item's box, written to out as base + local index
 * (0x00334C48). Callers: the compile 0x00300E0F (into the map) and
 * sub_002FA150. Outside sub_002FA150 the list gets the high ids. */
void pc_mapmaker_pads_editor_item_pads(void (*original)(void))
{
    uint32_t out=MAPMAKER_STACK_OK(20)?WORD_AT(g_esp+16u):0u,n,k;
    mapmaker_pads.linear++;
    original();
    mapmaker_pads.linear--;
    if(mapmaker_pads.linear>0) return;
    n=g_eax&0xFFu;
    if(n>64u || !MAPMAKER_PTR_OK(out,n*4u)) return;
    for(k=0;k<n;k++) {
        uint32_t v=WORD_AT(out+k*4u);
        if(v>=MAPMAKER_PADS_COVER_ID && v-MAPMAKER_PADS_REGULAR_ID<MAPMAKER_POOL_PADS)
            WORD_AT(out+k*4u)=mapmaker_pads_id(v-MAPMAKER_PADS_REGULAR_ID);
    }
}

/* ---- pad group distances ------------------------------------------------ */
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* sub_00275F70 (no arguments; jumped to by sub_00158BA0 when the mode byte
 * [0x007DE455] is 2 or 3): allocates the (N+1) x N x 4 B matrix [0x0078B2D8]
 * (N row pointers, then N x N floats; zeroed), sets the diagonal to 0 and for
 * every i != j runs sub_001596F0([[0x007B0C48]+0x14], route, 0) with route +0
 * = the first pad of group i, +0xC = the first pad of group j (sub_0009C550:
 * group record [0x007B6770][g], its list at +0x18) and +0x330 = +0x334 = 0,
 * after zeroing [0x00715F44] and [0x00715F48]; the entry is [0x00715F48] +
 * [0x00715F44] (one addss) for a route, the float [0x00475D5C] for none
 * (0x00276022..0x002760C6). sub_00275F40(i, j) answers matrix[i][j] (fld) if
 * i and j are below [0x007B666C] and the matrix exists, else [0x004760D0].
 *
 * Up to MAPMAKER_PADS_MATRIX_DENSE groups the original runs. Above it (scene
 * 100, pools on; TSFP_MAPMAKER_GROUP_MATRIX=dense keeps the original) the
 * builder keeps the load-time link flags (doors and movers change them in
 * play: sub_0009A570/sub_0009A590), the closing mask [0x004D9788] and the
 * door flag [0x00715F50], leaves [0x0078B2D8] at 0, and the reader computes
 * an entry the first time it is read: the same calls with those values put
 * back for the search (and the globals it writes restored after), cached per
 * row in host memory. The character argument is 0: it only selects the AI
 * struct the search writes to (its +0x684 for a cover destination, which
 * the first pads of groups never are, and +0x28 |= 0x200 when the route
 * starts at a pad listed in [0x007B65E0]), never the cost. stderr: one line
 * per map, then a summary every 4096 computed entries. */
#define MAPMAKER_PADS_MATRIX_DENSE 1024u
#define MAPMAKER_PADS_MATRIX_PTR 0x0078B2D8u
#define MAPMAKER_PADS_GROUP_COUNT 0x007B666Cu
#define MAPMAKER_PADS_NO_ROUTE 0x00475D5Cu       /* float: the cost of no route */
#define MAPMAKER_PADS_NO_ENTRY 0x004760D0u       /* float: sub_00275F40 out of range */
#define MAPMAKER_PADS_COST_A 0x00715F44u
#define MAPMAKER_PADS_COST_B 0x00715F48u
#ifndef MAPMAKER_FP_PUSH
#define MAPMAKER_FP_PUSH(v) do { g_fp_top=(g_fp_top-1)&7; g_fp_stack[g_fp_top]=(double)(v); } while(0)
#endif
extern void sub_001596F0(void);
extern void sub_0009C550(void);
static struct {
    int lazy;
    uint32_t groups, links, link_pool, route, mask, doors;
    uint32_t *flags;                 /* load-time link flags, links entries */
    float **row; uint8_t **known;    /* per group i: N entries, allocated on first read */
    unsigned long long computed, reads;
    double ms;
} mapmaker_matrix;
static void mapmaker_matrix_free(void)
{
    uint32_t i;
    if(mapmaker_matrix.row) for(i=0;i<mapmaker_matrix.groups;i++) { free(mapmaker_matrix.row[i]);free(mapmaker_matrix.known[i]); }
    free(mapmaker_matrix.row);free(mapmaker_matrix.known);free(mapmaker_matrix.flags);
    memset(&mapmaker_matrix,0,sizeof mapmaker_matrix);
}
static int mapmaker_matrix_wanted(uint32_t groups)
{
    const char *e=getenv("TSFP_MAPMAKER_GROUP_MATRIX");
    if(e && !strcmp(e,"dense")) return 0;
    if(e && !strcmp(e,"lazy")) return groups>0u;
    return groups>MAPMAKER_PADS_MATRIX_DENSE;
}
/* WRAP sub_00275F70 (no arguments, plain ret). */
void pc_mapmaker_pads_group_matrix(void (*original)(void))
{
    uint32_t n=WORD_AT(MAPMAKER_PADS_GROUP_COUNT),links,pool,route,i;
    mapmaker_matrix_free();
    mapmaker_limits_read();
    if(WORD_AT(0x0079094Cu)!=100u || !mapmaker_limits.pools || (int32_t)n<=0 || n>0x8000u ||
       !mapmaker_matrix_wanted(n)) { original(); return; }
    links=WORD_AT(0x007B6660u);pool=WORD_AT(0x00715F30u);
    if(links>0x100000u || !MAPMAKER_PTR_OK(pool,links*20u+4u)) { original(); return; }
    route=guest_call(sub_0003DFA0,2,0x340u,1u);          /* Level heap: lives as long as the level */
    mapmaker_matrix.flags=(uint32_t *)malloc((links?links:1u)*sizeof(uint32_t));
    mapmaker_matrix.row=(float **)calloc(n,sizeof(float *));
    mapmaker_matrix.known=(uint8_t **)calloc(n,sizeof(uint8_t *));
    if(!route || !MAPMAKER_PTR_OK(route,0x340) || !mapmaker_matrix.flags || !mapmaker_matrix.row || !mapmaker_matrix.known) {
        mapmaker_matrix_free();
        original();
        return;
    }
    for(i=0;i<links;i++) mapmaker_matrix.flags[i]=WORD_AT(pool+i*20u);
    mapmaker_matrix.groups=n;mapmaker_matrix.links=links;mapmaker_matrix.link_pool=pool;mapmaker_matrix.route=route;
    mapmaker_matrix.mask=WORD_AT(0x004D9788u);mapmaker_matrix.doors=WORD_AT(0x00715F50u);
    mapmaker_matrix.lazy=1;
    WORD_AT(MAPMAKER_PADS_MATRIX_PTR)=0;
    fprintf(stderr,"[mapmaker] pads: %u pad groups: distances computed on first use from the load-time"
            " state of %u links (a full matrix would take %llu bytes and %llu route searches;"
            " TSFP_MAPMAKER_GROUP_MATRIX=dense builds it)\n",
            (unsigned)n,(unsigned)links,(unsigned long long)(n+1u)*n*4u,(unsigned long long)n*(n-1u));
    fflush(stderr);
    MAPMAKER_GUEST_RET();
}
/* One entry, as the builder's loop body computes it. */
static float mapmaker_matrix_compute(uint32_t i,uint32_t j)
{
    uint32_t r=mapmaker_matrix.route,out=r+0x338u,pool=mapmaker_matrix.link_pool,k,n=mapmaker_matrix.links;
    uint32_t keep_a=WORD_AT(MAPMAKER_PADS_COST_A),keep_b=WORD_AT(MAPMAKER_PADS_COST_B);
    uint32_t keep_t=WORD_AT(0x00715EDCu),keep_g=WORD_AT(0x0071204Cu),keep_m=WORD_AT(0x004D9788u),keep_d=WORD_AT(0x00715F50u);
    uint32_t *now;
    int32_t result;
    float v;
    clock_t t0;
    if(i==j) return 0.0f;                      /* the builder's diagonal */
    t0=clock();now=(uint32_t *)malloc((n?n:1u)*sizeof(uint32_t));
    if(now) for(k=0;k<n;k++) { now[k]=WORD_AT(pool+k*20u);WORD_AT(pool+k*20u)=mapmaker_matrix.flags[k]; }
    WORD_AT(0x004D9788u)=mapmaker_matrix.mask;WORD_AT(0x00715F50u)=mapmaker_matrix.doors;
    guest_call(sub_0009C550,2,i,out);
    WORD_AT(r)=WORD_AT(WORD_AT(out));
    guest_call(sub_0009C550,2,j,out);
    WORD_AT(r+0xCu)=WORD_AT(WORD_AT(out));
    WORD_AT(r+0x330u)=0;WORD_AT(r+0x334u)=0;
    FLOAT_AT(MAPMAKER_PADS_COST_A)=0.0f;FLOAT_AT(MAPMAKER_PADS_COST_B)=0.0f;
    result=(int32_t)guest_call(sub_001596F0,3,0u,r,0u);
    if(result<0) v=FLOAT_AT(MAPMAKER_PADS_NO_ROUTE);
    else { volatile float b=FLOAT_AT(MAPMAKER_PADS_COST_B),a=FLOAT_AT(MAPMAKER_PADS_COST_A); v=b+a; }
    if(now) { for(k=0;k<n;k++) WORD_AT(pool+k*20u)=now[k]; free(now); }
    WORD_AT(0x004D9788u)=keep_m;WORD_AT(0x00715F50u)=keep_d;
    WORD_AT(MAPMAKER_PADS_COST_A)=keep_a;WORD_AT(MAPMAKER_PADS_COST_B)=keep_b;
    WORD_AT(0x00715EDCu)=keep_t;WORD_AT(0x0071204Cu)=keep_g;
    mapmaker_matrix.computed++;
    mapmaker_matrix.ms+=(double)(clock()-t0)*1000.0/CLOCKS_PER_SEC;
    if((mapmaker_matrix.computed&4095u)==0u) {
        fprintf(stderr,"[mapmaker] pads: %llu group distances computed of %llu read, %.0f ms in all\n",
                mapmaker_matrix.computed,mapmaker_matrix.reads,mapmaker_matrix.ms);
        fflush(stderr);
    }
    return v;
}
/* WRAP sub_00275F40(i, j) -> st0 (fld; plain ret). */
void pc_mapmaker_pads_group_distance(void (*original)(void))
{
    int32_t i,j,n;
    float v;
    if(!mapmaker_matrix.lazy || !MAPMAKER_STACK_OK(12) || WORD_AT(0x0079094Cu)!=100u ||
       WORD_AT(0x00715F30u)!=mapmaker_matrix.link_pool || WORD_AT(MAPMAKER_PADS_GROUP_COUNT)!=mapmaker_matrix.groups) {
        if(mapmaker_matrix.lazy) mapmaker_matrix_free();      /* another level: the snapshot is stale */
        original();
        return;
    }
    i=(int32_t)WORD_AT(g_esp+4u);j=(int32_t)WORD_AT(g_esp+8u);n=(int32_t)mapmaker_matrix.groups;
    mapmaker_matrix.reads++;
    if(i>=n || j>=n || i<0 || j<0) v=FLOAT_AT(MAPMAKER_PADS_NO_ENTRY);
    else {
        if(!mapmaker_matrix.row[i]) {
            mapmaker_matrix.row[i]=(float *)calloc((size_t)n,sizeof(float));
            mapmaker_matrix.known[i]=(uint8_t *)calloc((size_t)n,1);
        }
        if(!mapmaker_matrix.row[i] || !mapmaker_matrix.known[i]) v=mapmaker_matrix_compute((uint32_t)i,(uint32_t)j);
        else {
            if(!mapmaker_matrix.known[i][j]) {
                mapmaker_matrix.row[i][j]=mapmaker_matrix_compute((uint32_t)i,(uint32_t)j);
                mapmaker_matrix.known[i][j]=1;
            }
            v=mapmaker_matrix.row[i][j];
        }
    }
    MAPMAKER_FP_PUSH(v);
    MAPMAKER_GUEST_RET();
}
#include "pc_mapmaker_astar.inc"          /* WRAP sub_0009B540: the group route search in C */
