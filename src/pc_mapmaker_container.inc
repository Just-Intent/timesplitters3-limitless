/* US 45410066 MapMaker map container: 0x7FE0-byte body -> 0x7FFE0 (512 KB
 * buffer). See docs/mapmaker-container.md for every site and its evidence.
 *
 * Constants (buffer, zero/copy counts, chunk-walk bounds, undo shadow, diff
 * table and ring) are raised by config/guest-code-patches/container.json.
 * These hooks do what a same-length rewrite cannot:
 *  - the container (N) and the undo ring (2N) are too big for the Eternal
 *    heap (0x9DC00 bytes in all) and must outlive level loads, which wipe the
 *    Level heap; they come once from the runtime heap (xbox_HeapAlloc, the
 *    allocator behind MmAllocateSystemMemory, outside the game's arena) and
 *    are reused for the whole run;
 *  - files are saved in the smallest size tier that holds the map, so a map
 *    that fits the stock body saves byte-identical to stock; the loader tries
 *    the stock size first and the larger tiers on error 7 (file too large);
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

extern uint32_t xbox_HeapAlloc(uint32_t size,uint32_t alignment);
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
    uint32_t p=xbox_HeapAlloc(bytes,0x1000u);
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
/* Body size a save of this buffer writes: smallest tier holding used + CRC. */
static uint32_t mapmaker_container_tier(uint32_t c)
{
    uint32_t used=mapmaker_container_used(c);
    unsigned i;
    if(used && used<=MAPMAKER_CONTAINER_B-4u)
        for(i=0;i<MAPMAKER_CONTAINER_TIER_COUNT;i++)
            if(used+4u<=mapmaker_container_tiers[i]) return mapmaker_container_tiers[i];
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
    if(mapmaker_container_raised() && guest_writable(0x0049F7ACu,4) &&
       WORD_AT(0x0049F7ACu)==0x38002008u)
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
void pc_mapmaker_container_load(void (*original)(void))
{
    uint32_t entry=WORD_AT(g_esp+4),buf=WORD_AT(g_esp+8),flags,r,r2,size=MAPMAKER_CONTAINER_STOCK,info;
    (void)original;
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
