/* US 45410066 MapMaker map container: 0x7FE0-byte body -> 0x7FFE0 (512 KB
 * buffer). See docs/mapmaker/mapmaker-container.md for every site and its evidence.
 *
 * Constants (buffer, zero/copy counts, chunk-walk bounds, undo shadow, diff
 * table and ring) are raised by config/guest-code-patches/container.json.
 * These hooks do what a same-length rewrite cannot:
 *  - the container (N) and the undo ring (2N) are too big for the Eternal
 *    heap (0x9DC00 bytes in all) and must outlive level loads, which wipe the
 *    Level heap; they come once from the contiguous window
 *    (xbox_ContiguousAlloc, the allocator behind MmAllocateContiguousMemory,
 *    where the stock container lives in the Eternal heap) and are reused for
 *    the whole run. Not the runtime heap: guest code may convert a buffer
 *    address with (va & 0x0FFFFFFF) | 0x80000000, which is only an identity
 *    in that window (a runtime-heap container hung the front end);
 *  - files are saved in the smallest size tier that holds the map, so a map
 *    that fits the stock body saves byte-identical to stock; the loader tries
 *    the stock size first and the larger tiers on error 7 (file too large);
 *    a map holding more of anything than the stock game allows saves in the
 *    64 KB tier at least, so a stock build refuses it instead of overflowing
 *    (mapmaker_container_over_stock, round 3);
 *  - the save CRC and the load check use the 16-bit word [0x00765AEE]; for
 *    maps past 64 KB the CRC is computed over the full length here;
 *  - System Link only ever carries the stock size: in a network session a
 *    map needing a larger tier is refused at load (error 7, the stock path),
 *    and the host never sends a larger map;
 *  - the editor capacity is raised only with TSFP_MAPMAKER_LIMITS=1 (the
 *    placement checks see the used size moved down by L - 0x7FE0) and the
 *    free-space meter constant follows it.
 * Requires the includer's WORD_AT, FLOAT_AT, guest_writable, guest_call,
 * g_eax, g_esp, g_xbox_mem_offset and pc_mapmaker_limits.inc. */
#ifndef MAPMAKER_GUEST_RET
#define MAPMAKER_GUEST_RET() (g_esp+=4u) /* see pc_mapmaker_limits.inc */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAPMAKER_CONTAINER_N 0x80000u          /* buffer (stock 0x8000) */
#define MAPMAKER_CONTAINER_B 0x7FFE0u          /* largest body (stock 0x7FE0) */
#define MAPMAKER_CONTAINER_STOCK 0x7FE0u
#define MAPMAKER_CONTAINER_L (MAPMAKER_CONTAINER_B-4u) /* editor limit: used+4 <= B */
#define MAPMAKER_CONTAINER_RING (2u*MAPMAKER_CONTAINER_N)
#define MAPMAKER_CONTAINER_SCRATCH 0x40u       /* after the buffer: capacity view */
#define MMC_HALF(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define MMC_BYTE(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

extern uint32_t xbox_ContiguousAlloc(uint32_t size,uint32_t alignment);
extern void sub_000827B0(void),sub_000827F0(void),sub_0035E960(void),sub_00025280(void),
    sub_00336600(void);

/* File body sizes, smallest first. The stock loader needs the exact body. */
static const uint32_t mapmaker_container_tiers[]={
    MAPMAKER_CONTAINER_STOCK,0xFFE0u,0x1FFE0u,0x3FFE0u,MAPMAKER_CONTAINER_B};
#define MAPMAKER_CONTAINER_TIER_COUNT (sizeof mapmaker_container_tiers/sizeof mapmaker_container_tiers[0])

static struct { uint32_t buffer,ring; int warned_send,warned_buffer; } mapmaker_container;

static void mapmaker_container_zero(uint32_t a,uint32_t n)
{
    if(guest_writable(a,n)) memset((void *)((uintptr_t)a+g_xbox_mem_offset),0,n);
}
static uint32_t mapmaker_container_alloc(uint32_t bytes)
{
    uint32_t p=xbox_ContiguousAlloc(bytes,0x1000u);
    if(!p) {
        /* Every size constant already assumes the large buffers. */
        fprintf(stderr,"[mapmaker] cannot allocate %u bytes for the map container\n",bytes);
        fflush(stderr);
        abort();
    }
    return p;
}

/* sub_0032A5B0 with the raised bound: data of the map info chunk 0xC77668,
 * or 0. Stops at the terminator 0xC7766A like the original. */
static uint32_t mapmaker_container_info(uint32_t c)
{
    uint32_t p,end;
    if(!guest_writable(c,MAPMAKER_CONTAINER_B+8u) || WORD_AT(c)!=0x00C77667u || WORD_AT(c+4)!=0x12Du) return 0;
    end=c+MAPMAKER_CONTAINER_B;
    for(p=c+8;p<end;) {
        uint32_t tag,next;
        tag=WORD_AT(p);
        if(tag==0x00C77668u) return guest_writable(p+8+0x248,4)?p+8:0u;
        if(tag==0x00C7766Au) return 0;
        next=p+8+WORD_AT(p+4);
        if(next<=p) return 0;
        p=next;
    }
    return 0;
}
/* Bytes of map data (map info +0x248) or 0 when the container has none. */
static uint32_t mapmaker_container_used(uint32_t c)
{
    uint32_t info=mapmaker_container_info(c);
    return info?WORD_AT(info+0x248):0u;
}
/* ---- maps past the stock limits ---------------------------------------- */
/* A stock build loads any file whose body is exactly 0x7FE0 and trusts every
 * count in it: more than 50 story AI smash sub_00240590's stack array, more
 * than 30 ops / 130 nodes / 100 records / 170 free objects run past the
 * stock editor object, more than 10 teleporters past sub_0023FD10's stack
 * arrays, more than 32 turrets give a gun struct at address 0, and 200+
 * rooms, 2400+ pads or 550+ objects overrun their pools. So a map holding
 * more of anything than the stock game allows is saved in the 64 KB tier
 * even when it would fit 32 KB: the stock loader refuses that file (error 7,
 * file larger than the body), this build reads it through its tiers. Maps
 * within every stock limit still save byte-identical to stock.
 *
 * Two sources, either one is enough: the chunk counts of the buffer being
 * saved (always), and while the editor runs (scene 102) the editor's own
 * counts, which also cover the items attached to tiles: the item total, the
 * per-category counts of sub_00303460 (catalogue 0x00524F58, 80 x 0xC4,
 * category word +2; stock maxima from mapmaker_categories[] or the table
 * itself), the tile pad costs and the global pad groups. */
#define MAPMAKER_CONTAINER_OVER_TIER 0xFFE0u
static const struct { uint32_t tag, stock; } mapmaker_container_stock_chunks[]={
    {0x00C7766Bu,170u},   /* free objects: editor +0x3070, 170 x 12 */
    {0x00C7766Cu,50u},    /* story AI: sub_00240590 50-slot stack array */
    {0x00C7766Du,130u},   /* logic nodes */
    {0x00C7766Eu,30u},    /* logic ops */
    {0x00C7766Fu,100u},   /* logic records */
};
#define MAPMAKER_CONTAINER_STOCK_CHUNKS (sizeof mapmaker_container_stock_chunks/sizeof mapmaker_container_stock_chunks[0])
#define MAPMAKER_CONTAINER_STOCK_PIECES 200u
static int mapmaker_container_over_chunks(uint32_t c)
{
    uint32_t counts[MAPMAKER_CONTAINER_STOCK_CHUNKS],p,end;
    unsigned i,steps;
    if(!guest_writable(c,MAPMAKER_CONTAINER_B+8u) || WORD_AT(c)!=0x00C77667u || WORD_AT(c+4)!=0x12Du) return 0;
    memset(counts,0,sizeof counts);
    end=c+MAPMAKER_CONTAINER_B;
    for(p=c+8,steps=0;p+8u<=end && steps<0x40000u;steps++) {
        uint32_t tag=WORD_AT(p),size=WORD_AT(p+4),next=p+8u+size;
        if(tag==0x00C7766Au || next<=p || next>end) break;
        if(tag==0x00C776F5u || tag==0x00C776F6u) return 1;   /* character slots 11-20 (pc_mapmaker_sets.inc), weapon slots 7-20 (pc_mapmaker_weaponset.inc) */
        if(tag==0x00C77668u && size>=8u && WORD_AT(p+8u+4u)>MAPMAKER_CONTAINER_STOCK_PIECES &&
           (int32_t)WORD_AT(p+8u+4u)>0) return 1;          /* header piece count */
        if(tag==0x00C77668u && size>=0x0Cu && (WORD_AT(p+8u+8u)>>16)==0u) return 1; /* tileset 0, Abstract (pc_mapmaker_tilesets.inc) */
        for(i=0;i<MAPMAKER_CONTAINER_STOCK_CHUNKS;i++)
            if(tag==mapmaker_container_stock_chunks[i].tag && ++counts[i]>mapmaker_container_stock_chunks[i].stock) return 1;
        p=next;
    }
    return 0;
}
static uint32_t mapmaker_container_stock_max(unsigned cat)
{
    uint32_t a=0x00524BE4u+cat*24u;
    unsigned i;
    for(i=0;i<sizeof mapmaker_categories/sizeof mapmaker_categories[0];i++)
        if(mapmaker_categories[i].address==a) return mapmaker_categories[i].stock;
    return WORD_AT(a);
}
static int mapmaker_container_over_editor(void)
{
    uint32_t editor,piece,counts[37];
    MapMakerTotals t;
    unsigned i,j;
    if(!guest_writable(0x0079094Cu,4) || WORD_AT(0x0079094Cu)!=102u ||
       !guest_writable(0x007844A8u,4) || !guest_writable(0x0078400Cu,4)) return 0;
    editor=WORD_AT(0x007844A8u);
    if(!guest_writable(editor,0xDD14u)) return 0;
    piece=WORD_AT(0x0078400Cu);
    mapmaker_list_totals(piece,&t);
    if(t.pieces>MAPMAKER_CONTAINER_STOCK_PIECES || t.cost[0]>2400u || t.cost[1]>1024u || t.cost[2]>4400u ||
       t.sub+(WORD_AT(editor+0x306Cu)>>16)>50u || (int32_t)WORD_AT(0x00762CB8u)>300) return 1;
    if((int32_t)WORD_AT(editor+0x280u)>50 || (int32_t)WORD_AT(editor+0x41E0u)>30 ||
       (int32_t)WORD_AT(editor+0x81A4u)>130 || (int32_t)WORD_AT(editor+0xDD10u)>100) return 1;
    if((WORD_AT(editor+0x3068u)&0xFFFFu)>40u || (WORD_AT(editor+0x3068u)>>16)>30u ||
       (WORD_AT(editor+0x306Cu)&0xFFFFu)>100u) return 1;   /* free objects per type */
    memset(counts,0,sizeof counts);
    for(i=0;piece && i<4096u && MAPMAKER_PTR_OK(piece,0x44);i++,piece=WORD_AT(piece+0x40)) {
        uint32_t sub=WORD_AT(piece+0x3c);
        for(j=0;sub && j<4096u && MAPMAKER_PTR_OK(sub,0x18);j++,sub=WORD_AT(sub+0x14)) {
            uint32_t id=WORD_AT(sub),cat;
            if(id>=80u) continue;
            cat=WORD_AT(0x00524F58u+id*0xC4u)>>16;            /* word +2 */
            if(cat<37u) counts[cat]++;
        }
    }
    for(i=0;i<37u;i++) if(counts[i]>mapmaker_container_stock_max(i)) return 1;
    return 0;
}
static int mapmaker_container_over_stock(uint32_t c)
{
    return mapmaker_container_over_chunks(c) || mapmaker_container_over_editor();
}

/* Body size a save of this buffer writes: smallest tier holding used + CRC,
 * at least 64 KB for a map past the stock limits. */
static uint32_t mapmaker_container_tier(uint32_t c)
{
    uint32_t used=mapmaker_container_used(c);
    unsigned i;
    int over;
    if(used && used<=MAPMAKER_CONTAINER_B-4u) {
        over=mapmaker_container_over_stock(c);
        for(i=0;i<MAPMAKER_CONTAINER_TIER_COUNT;i++)
            if(used+4u<=mapmaker_container_tiers[i] && (!over || mapmaker_container_tiers[i]>=MAPMAKER_CONTAINER_OVER_TIER))
                return mapmaker_container_tiers[i];
    }
    return MAPMAKER_CONTAINER_STOCK;
}

static int mapmaker_container_raised(void)
{
    mapmaker_limits_read();
    return mapmaker_limits.limits;
}
/* Free-space meter (sub_0030C180 multiplies by the float at 0x0049F7AC,
 * 1/32736; its only two readers are there). Raised with the editor limit. */
void mapmaker_container_editor_apply(void)
{
    /* called every editor frame: 0x0049F7AC is a fixed .rdata/.data float */
    if(mapmaker_container_raised() && WORD_AT(0x0049F7ACu)==0x38002008u)
        FLOAT_AT(0x0049F7ACu)=(float)(1.0/(double)MAPMAKER_CONTAINER_L);
}

/* ---- buffers ----------------------------------------------------------- */
static void mapmaker_container_install(void)
{
    if(!mapmaker_container.buffer)
        mapmaker_container.buffer=mapmaker_container_alloc(MAPMAKER_CONTAINER_N+MAPMAKER_CONTAINER_SCRATCH);
    WORD_AT(0x00784014u)=mapmaker_container.buffer;
    mapmaker_container_zero(mapmaker_container.buffer,MAPMAKER_CONTAINER_N+MAPMAKER_CONTAINER_SCRATCH);
}
/* sub_002FE200 (front end, only when [0x00784014] is 0): allocate, zero,
 * then the undo ring (the original tail-calls sub_00336600). */
void pc_mapmaker_container_frontend_init(void (*original)(void))
{
    uint32_t r;
    (void)original;
    mapmaker_container_install();
    r=guest_call(sub_00336600,0);
    g_eax=r;
    MAPMAKER_GUEST_RET();
}
/* sub_002FE230 (editor entry): the original allocates only when
 * [0x00784014] is 0, so provide the buffer first, then run it. */
void pc_mapmaker_container_editor_init(void (*original)(void))
{
    uint32_t cur;
    mapmaker_container_editor_apply();
    cur=WORD_AT(0x00784014u);
    if(!cur) {
        mapmaker_container_install();
        guest_call(sub_00336600,0);
    } else if(cur!=mapmaker_container.buffer && !mapmaker_container.warned_buffer) {
        mapmaker_container.warned_buffer=1;
        fprintf(stderr,"[mapmaker] container 0x%08X was not allocated by the container hooks\n",cur);
        fflush(stderr);
    }
    original();
}
/* sub_00336600: undo history ring [0x00765C24], lists and write offset. */
void pc_mapmaker_container_undo_ring(void (*original)(void))
{
    (void)original;
    if(!mapmaker_container.ring) mapmaker_container.ring=mapmaker_container_alloc(MAPMAKER_CONTAINER_RING);
    WORD_AT(0x00765C24u)=mapmaker_container.ring;
    mapmaker_container_zero(mapmaker_container.ring,MAPMAKER_CONTAINER_RING);
    WORD_AT(0x00765C28u)=0;WORD_AT(0x00765C30u)=0;WORD_AT(0x00765C20u)=0;
    g_eax=0;
    MAPMAKER_GUEST_RET();
}

/* ---- load --------------------------------------------------------------- */
static uint32_t mapmaker_container_read(uint32_t entry,uint32_t buf,uint32_t flags,uint32_t *size)
{
    unsigned i;
    uint32_t r=7;
    for(i=0;i<MAPMAKER_CONTAINER_TIER_COUNT;i++) {
        /* System Link carries one stock-size message: refuse larger maps. */
        if(i && guest_call(sub_0035E960,0)) return 7;
        r=guest_call(sub_00025280,6,guest_call(sub_000827F0,0),entry+0x20u,0x0047DF80u,buf,
                     mapmaker_container_tiers[i],flags);
        *size=mapmaker_container_tiers[i];
        if(r!=7) return r;
    }
    return r;
}
/* sub_00190530(entry, buffer), reimplemented: the same calls and results,
 * with the buffer zeroed first and the size tiers. Error 7 is the stock
 * "file larger than the body" result, which the caller already reports. */
static void mapmaker_container_load_body(void)
{
    uint32_t entry=WORD_AT(g_esp+4),buf=WORD_AT(g_esp+8),flags,r,r2,size=MAPMAKER_CONTAINER_STOCK,info;
    guest_call(sub_000827B0,1,1u);
    flags=guest_call(sub_000827F0,0)==0xAu?0xEu:2u;
    if(!guest_call(sub_0035E960,0)) flags&=~4u;
    mapmaker_container_zero(buf,MAPMAKER_CONTAINER_N);
    r=mapmaker_container_read(entry,buf,flags,&size);
    if(r!=1) { g_eax=r; return; }
    info=mapmaker_container_info(buf);
    if(!info) { g_eax=7; return; }               /* the original would dereference 0 */
    MMC_HALF(entry+0x66)=(uint16_t)(WORD_AT(info+0x248)+4u); /* stock: 16-bit, +0x68 kept */
    if(flags&4u) {
        MMC_BYTE(info+8)=1;
        WORD_AT(0x00762CBCu)=1;
        g_eax=r;
        return;
    }
    if(!MMC_BYTE(info+8) || !guest_call(sub_0035E960,0)) {
        WORD_AT(0x00762CBCu)=0;
        g_eax=r;
        return;
    }
    r2=guest_call(sub_00025280,6,guest_call(sub_000827F0,0),entry+0x20u,0x0047DF80u,buf,size,flags|0x14u);
    if(r2==r) WORD_AT(0x00762CBCu)=r;
    g_eax=r2;
}
void pc_mapmaker_container_load(void (*original)(void))
{
    (void)original;
    mapmaker_container_load_body();
    MAPMAKER_GUEST_RET();
}

/* ---- save (sub_00190900 passes the stock body 0x7FE0) ------------------ */
static void mapmaker_container_resize(uint32_t ret,uint32_t buf_arg,uint32_t size_arg)
{
    uint32_t a=g_esp+4u+4u*size_arg,size,tier;
    if(WORD_AT(g_esp)!=ret) return;
    size=WORD_AT(a);
    tier=mapmaker_container_tier(WORD_AT(g_esp+4u+4u*buf_arg));
    if(tier!=MAPMAKER_CONTAINER_STOCK && size>=MAPMAKER_CONTAINER_STOCK &&
       size-MAPMAKER_CONTAINER_STOCK<0x100u)        /* body, or body + signature */
        WORD_AT(a)=size-MAPMAKER_CONTAINER_STOCK+tier;
}
/* sub_00026740(dev, name, path, key, buffer, body + signature, flags) */
void pc_mapmaker_container_save_async(void (*original)(void))
{
    mapmaker_container_resize(0x001909E3u,4,5);
    original();
}
/* sub_00025710(buffer, body + signature, &body, flags) */
void pc_mapmaker_container_save_sign(void (*original)(void))
{
    mapmaker_container_resize(0x00190A1Fu,0,1);
    original();
}
/* sub_000263E0(dev, name, path, buffer, body, flags) */
void pc_mapmaker_container_save_sync(void (*original)(void))
{
    mapmaker_container_resize(0x00190A54u,3,4);
    original();
}

/* ---- CRC over maps past 64 KB ------------------------------------------- */
/* sub_001504F0(buffer, length) called by the save at 0x0032C71F with the
 * 16-bit length; the caller then stores the result at buffer + length. For a
 * longer map store the real CRC at buffer + used and return the word already
 * at buffer + length, so the caller's store changes nothing. */
void pc_mapmaker_container_crc(void (*original)(void))
{
    if(WORD_AT(g_esp)==0x0032C724u) {
        uint32_t buf=WORD_AT(g_esp+4),len=WORD_AT(g_esp+8),used=mapmaker_container_used(buf);
        if(used>0xFFFFu && used<=MAPMAKER_CONTAINER_B-4u && len<used &&
           guest_writable(buf,MAPMAKER_CONTAINER_B)) {
            uint32_t crc;
            WORD_AT(g_esp+8)=used;
            original();
            WORD_AT(g_esp+8)=len;
            crc=g_eax;
            WORD_AT(buf+used)=crc;
            g_eax=WORD_AT(buf+len);
            return;
        }
    }
    original();
}
/* sub_00150560(buffer, length, expected) called by the load check at
 * 0x0032B425 with length and expected taken from the 16-bit word. */
void pc_mapmaker_container_crc_check(void (*original)(void))
{
    if(WORD_AT(g_esp)==0x0032B42Au) {
        uint32_t buf=WORD_AT(g_esp+4),used=mapmaker_container_used(buf);
        if(used>0xFFFFu-4u) {
            uint32_t len=WORD_AT(g_esp+8),expected=WORD_AT(g_esp+12);
            if(used>MAPMAKER_CONTAINER_B-4u || !guest_writable(buf,used+4u)) { g_eax=0; return; }
            WORD_AT(g_esp+8)=used;WORD_AT(g_esp+12)=WORD_AT(buf+used);
            original();
            WORD_AT(g_esp+8)=len;WORD_AT(g_esp+12)=expected;
            return;
        }
    }
    original();
}
/* The save takes the compiled size in ax and fails when it is 0, so a map of
 * exactly k x 64 KB would not save: give it a non-zero low half (the CRC hook
 * above then works from the real size). sub_003006C0 at 0x0032C6FA and
 * sub_002FE0C0 at 0x0032C6ED. */
static void mapmaker_container_size_low(uint32_t ret,void (*original)(void))
{
    uint32_t r=WORD_AT(g_esp);
    original();
    if(r==ret && g_eax && !(g_eax&0xFFFFu)) g_eax|=4u;
}
void pc_mapmaker_container_compile(void (*original)(void))
{
    mapmaker_container_size_low(0x0032C6FFu,original);
}
void pc_mapmaker_container_compiled_size(void (*original)(void))
{
    mapmaker_container_size_low(0x0032C6F2u,original);
}

/* ---- editor capacity (TSFP_MAPMAKER_LIMITS=1) --------------------------- */
/* sub_0032A5B0(container, 0xC77668) at the four placement checks, which test
 * info+0x248 + cost < 0x7FE0 (signed): hand them a view whose used is lower by
 * L - 0x7FE0, so the stock compare tests used + cost < L. */
void pc_mapmaker_container_find(void (*original)(void))
{
    uint32_t r=WORD_AT(g_esp),scratch;
    original();
    if(!g_eax || (r!=0x00300685u && r!=0x003035D2u && r!=0x00303927u && r!=0x00304432u) ||
       !mapmaker_container.buffer || !mapmaker_container_raised()) return;
    scratch=mapmaker_container.buffer+MAPMAKER_CONTAINER_N;
    if(!guest_writable(g_eax+0x248,4) || !guest_writable(scratch,4)) return;
    WORD_AT(scratch)=WORD_AT(g_eax+0x248)-(MAPMAKER_CONTAINER_L-MAPMAKER_CONTAINER_STOCK);
    g_eax=scratch-0x248u;
}

/* ---- System Link -------------------------------------------------------- */
/* sub_00353FE0(peer, type, data, length, flag) from sub_003563C0 and
 * sub_003563A0, the host sending the map (type 0x30, 0x8000 bytes). A map
 * past the stock body cannot travel in one message; the load refuses it in a
 * session, so this only stops a truncated map from going out. */
void pc_mapmaker_container_net_send(void (*original)(void))
{
    uint32_t r=WORD_AT(g_esp),data=WORD_AT(g_esp+12),used;
    if((r==0x0035645Au || r==0x003563B9u) && WORD_AT(g_esp+8)==0x30u &&
       data==WORD_AT(0x00784014u)) {
        used=mapmaker_container_used(data);
        if(used>MAPMAKER_CONTAINER_STOCK-4u) {
            if(!mapmaker_container.warned_send) {
                mapmaker_container.warned_send=1;
                fprintf(stderr,"[mapmaker] map of %u bytes is larger than System Link carries; not sent\n",used+4u);
                fflush(stderr);
            }
            g_eax=0;
            return;
        }
    }
    original();
}
