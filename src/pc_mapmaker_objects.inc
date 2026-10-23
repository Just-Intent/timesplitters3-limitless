/* US 45410066 MapMaker capacity, objects area: turrets, object lists,
 * the engine object pool, teleporters and logic tags (docs/mapmaker-objects.md,
 * logs: turrets.md, items.md 2.2-2.5).
 *
 * Capacity is raised by config/guest-code-patches/objects.json (every scene):
 * turret gun/owner pools 32 -> 80 and the System Link turret sync over all 80
 * slots; the per-player perception array 0x3E0 -> 0xA00 B (32 characters +
 * 128 objects); the sub_000A5CE0 object lists relocated into the reserved
 * block (slot 9 at 0x008A0000, 30 -> 128; slot 8 at 0x008A0200, 50 -> 200;
 * slot 0 at 0x008A0600, 112 -> 448); the room bucket buffer doubled; the logic
 * tag links (0x008A2400) and tags (0x008A4400) relocated at 250 -> 1000 each.
 * Logic tag groups stay at 150 (sub_0023FFA0 and sub_00240360 drop safely when
 * full).
 *
 * The C hooks below are guards that make an overflow of those tables
 * impossible, plus two reimplemented allocators and the teleporter pairing:
 *
 * - pc_mapmaker_objects_turret_create (sub_0006CE80): a turret record of the
 *   level loader sub_0006D7A0 (return address 0x0006E818, which tolerates 0)
 *   is skipped when the gun or owner free list is empty, instead of the
 *   original writing a gun struct at guest address 0.
 * - pc_mapmaker_objects_turret_init (sub_00245990, class 0x200 init): any
 *   other creator that finds a list empty gets a spare block from the reserved
 *   block (8 of each, reused in turn) instead of address 0.
 * - pc_mapmaker_objects_list_add (sub_0009E650) and
 *   pc_mapmaker_objects_register (sub_000A5CE0): appends to the slot 9, 8 and
 *   0 lists stop at the relocated capacity; the object's own fields are still
 *   written as the original does.
 * - pc_mapmaker_objects_pool / _twin_pool (sub_000A5C00, sub_0008DBF0): while
 *   a MapMaker map is edited or played (scene 102/100, TSFP_MAPMAKER_POOLS not
 *   0) the engine object pool and its twin hold 1000 objects instead of 550.
 *   Object handles keep the slot in their top 10 bits (handle >> 22), so 1024
 *   is the ceiling; 1000 keeps every handle away from -1. Other scenes run the
 *   original (550, or 20 in scene 101), so story levels keep their memory use.
 * - pc_mapmaker_objects_teleporters (sub_0009CD00, scene 100): the original
 *   gathers teleporter pads into two 10-slot stack arrays without a bound
 *   (sub_0023FD10); this reads the same container walk into larger arrays,
 *   bounded, and pairs each teleporter with the next one of the same colour
 *   exactly as the original, skipping a nav link when the link pool is full.
 * - pc_mapmaker_objects_tag (sub_002402B0): when every tag slot is used the
 *   original writes at index -1; the object is then registered without a tag
 *   slot (tag field and serial written as the original).
 *
 * Requires the includer's WORD_AT, guest_writable, guest_call,
 * save_registers/restore_registers, g_esp/g_eax/g_ebx/g_edi,
 * g_xbox_mem_offset and pc_mapmaker_limits.inc (mapmaker_limits,
 * MAPMAKER_LIMIT_LINKS). */
#ifndef MAPMAKER_GUEST_RET
#define MAPMAKER_GUEST_RET() (g_esp+=4u) /* see pc_mapmaker_limits.inc */
#endif
#include <string.h>

#define MAPMAKER_OBJECTS_TURRETS 80u          /* objects.json obj-turret-* */
#define MAPMAKER_OBJECTS_LIST9 0x008A0000u    /* obj-list9-relocate */
#define MAPMAKER_OBJECTS_LIST9_CAP 128u
#define MAPMAKER_OBJECTS_LIST8 0x008A0200u    /* obj-list8-relocate */
#define MAPMAKER_OBJECTS_LIST8_CAP 200u
#define MAPMAKER_OBJECTS_LIST0 0x008A0600u    /* obj-list0-relocate */
#define MAPMAKER_OBJECTS_LIST0_CAP 448u
#define MAPMAKER_OBJECTS_SPARE_GUNS 0x008A9000u   /* 8 x 0x1E0 */
#define MAPMAKER_OBJECTS_SPARE_OWNERS 0x008AA000u /* 8 x 0x130 */
#define MAPMAKER_OBJECTS_SPARES 8u
#define MAPMAKER_OBJECTS_POOL 1000u           /* < 1024: slot = handle >> 22 */
#define MAPMAKER_OBJECTS_BUCKET_BYTES 0xA680u /* obj-bucket-bytes */
#define MAPMAKER_OBJECTS_TAGS 0x008A4400u     /* obj-tag-entries-relocate */
#define MAPMAKER_OBJECTS_TAGS_CAP 1000u
#define MAPMAKER_OBJECTS_STOCK_LINKS 4400u    /* stock sub_0009A140 link pool */
#define MAPMAKER_OBJECTS_TELEPORTER_CAP 1200u /* > 0x7FE0 / 0x1C items */
/* sub_0023FD10 walk bound: the largest map body (src/pc_mapmaker_container.inc,
 * included first; the stock 0x7FE0 when built without it, e.g. in fixtures). */
#ifdef MAPMAKER_CONTAINER_B
#define MAPMAKER_OBJECTS_CONTAINER_END MAPMAKER_CONTAINER_B
#else
#define MAPMAKER_OBJECTS_CONTAINER_END 0x7FE0u
#endif

extern void sub_0003DFA0(void),sub_000B05B0(void),sub_000B3BE0(void),sub_00248970(void),
    sub_000C3540(void),sub_001806B0(void),sub_000AEAC0(void),sub_00242BA0(void),sub_000A5AF0(void),
    sub_0009CC40(void),sub_0009A2C0(void),sub_0009A5B0(void),sub_0009CCA0(void),
    sub_00333700(void),sub_002FDF50(void);

static unsigned mapmaker_objects_spares_used;

static int mapmaker_objects_scene(uint32_t scene)
{
    return guest_writable(0x0079094Cu,4) && WORD_AT(0x0079094Cu)==scene;
}

/* ---- turrets ---------------------------------------------------------- */
/* Free lists (sub_00156DB0): +0 base, +4 head, +8 free count. */
#define MAPMAKER_OBJECTS_GUN_LIST 0x00732D2Cu
#define MAPMAKER_OBJECTS_OWNER_LIST 0x00732D1Cu
static int mapmaker_objects_turret_full(void)
{
    return !WORD_AT(MAPMAKER_OBJECTS_GUN_LIST+4) || !WORD_AT(MAPMAKER_OBJECTS_OWNER_LIST+4);
}
void pc_mapmaker_objects_turret_create(void (*original)(void))
{
    if(guest_writable(g_esp,4) && WORD_AT(g_esp)==0x0006E818u && mapmaker_objects_turret_full()) {
        g_eax=0;      /* sub_0006D7A0 skips a 0 result; esi/edi/ebx untouched */
        MAPMAKER_GUEST_RET();
        return;
    }
    original();
}
static void mapmaker_objects_spare(uint32_t list,uint32_t base,uint32_t size)
{
    uint32_t node=base+(mapmaker_objects_spares_used%MAPMAKER_OBJECTS_SPARES)*size;
    if(WORD_AT(list+4) || !guest_writable(node,size)) return;
    memset((void *)((uintptr_t)node+g_xbox_mem_offset),0,size);
    WORD_AT(node)=0;                               /* push, as sub_00156E30 */
    WORD_AT(list+4)=node;
    WORD_AT(list+8)++;
}
void pc_mapmaker_objects_turret_init(void (*original)(void))
{
    if(mapmaker_objects_turret_full()) {
        mapmaker_objects_spare(MAPMAKER_OBJECTS_GUN_LIST,MAPMAKER_OBJECTS_SPARE_GUNS,0x1E0u);
        mapmaker_objects_spare(MAPMAKER_OBJECTS_OWNER_LIST,MAPMAKER_OBJECTS_SPARE_OWNERS,0x130u);
        mapmaker_objects_spares_used++;
    }
    original();
}

/* ---- object lists ----------------------------------------------------- */
void pc_mapmaker_objects_list_add(void (*original)(void))
{
    if((int32_t)WORD_AT(0x007B625Cu)>=(int32_t)MAPMAKER_OBJECTS_LIST9_CAP) { MAPMAKER_GUEST_RET(); return; }
    original();
}
static int mapmaker_objects_listed(uint32_t base,uint32_t count,uint32_t obj)
{
    uint32_t i;
    for(i=0;i<count;i++) if(WORD_AT(base+i*4u)==obj) return 1;
    return 0;
}
/* sub_000A5CE0(obj, slot, flag): obj+0x90 = [0x007DE318], obj+0x154+slot*4 =
 * flag, then flag != 0 appends obj to the slot's list (0, 8, 9) and 0 removes it. */
void pc_mapmaker_objects_register(void (*original)(void))
{
    uint32_t obj,slot,flag,count,base,cap;
    if(!guest_writable(g_esp,16)) { original(); return; }
    obj=WORD_AT(g_esp+4);slot=WORD_AT(g_esp+8);flag=WORD_AT(g_esp+12);
    if(slot==0) { count=0x007B6258u;base=MAPMAKER_OBJECTS_LIST0;cap=MAPMAKER_OBJECTS_LIST0_CAP; }
    else if(slot==8) { count=0x007B4FA8u;base=MAPMAKER_OBJECTS_LIST8;cap=MAPMAKER_OBJECTS_LIST8_CAP; }
    else if(slot==9) { count=0x007B625Cu;base=MAPMAKER_OBJECTS_LIST9;cap=MAPMAKER_OBJECTS_LIST9_CAP; }
    else { original(); return; }
    if(flag && (int32_t)WORD_AT(count)>=(int32_t)cap && guest_writable(obj,0x154u+slot*4u+4u) &&
       !mapmaker_objects_listed(base,cap,obj)) {
        WORD_AT(obj+0x90)=WORD_AT(0x007DE318u);
        WORD_AT(obj+0x154u+slot*4u)=flag;
        MAPMAKER_GUEST_RET();
        return;
    }
    original();
}

/* ---- engine object pool ----------------------------------------------- */
static int mapmaker_objects_pool_on(void)
{
    mapmaker_limits_read();
    return mapmaker_limits.pools && (mapmaker_objects_scene(100u) || mapmaker_objects_scene(102u));
}
static void mapmaker_objects_clear(uint32_t a,uint32_t bytes)
{
    if(guest_writable(a,bytes)) memset((void *)((uintptr_t)a+g_xbox_mem_offset),0,bytes);
}
/* sub_000A5C00 with the pool size raised; same allocations, order and calls.
 * Runs only with the callback sub_001B5260 registers at [0x00715FAC] (or
 * none), which it calls directly; anything else keeps the original. */
void pc_mapmaker_objects_pool(void (*original)(void))
{
    GuestRegisters r;
    uint32_t callback;
    if(!mapmaker_objects_pool_on()) { original(); return; }
    callback=WORD_AT(0x00715FACu);
    if(callback && callback!=0x00242BA0u) { original(); return; }
    save_registers(&r);
    WORD_AT(0x0072F5E4u)=MAPMAKER_OBJECTS_POOL;
    WORD_AT(0x0072F5E8u)=guest_call(sub_0003DFA0,2,(MAPMAKER_OBJECTS_POOL*0x2E8u+15u)&~15u,1u);
    WORD_AT(0x0072F5ECu)=guest_call(sub_0003DFA0,2,0x190u,1u);
    WORD_AT(0x00715FB4u)=guest_call(sub_0003DFA0,2,MAPMAKER_OBJECTS_BUCKET_BYTES,1u);
    mapmaker_objects_clear(0x007B54E0u,0xD6u*4u);
    mapmaker_objects_clear(0x007B5840u,0xD6u*4u);
    mapmaker_objects_clear(0x007B6260u,0xD6u*4u);
    guest_call(sub_000B05B0,0);guest_call(sub_000B3BE0,0);guest_call(sub_00248970,0);
    guest_call(sub_000C3540,0);guest_call(sub_001806B0,0);   /* two calls of the empty sub_000389B0 omitted */
    guest_call(sub_000AEAC0,0);
    WORD_AT(0x00716290u)=0;WORD_AT(0x00716294u)=0;          /* the list walk from 0x00716290 */
    WORD_AT(0x0072F5F8u)=0;
    if(callback) guest_call(sub_00242BA0,1,0u);
    guest_call(sub_000A5AF0,0);                              /* tail call: clears the pool */
    restore_registers(&r);
    MAPMAKER_GUEST_RET();
}
/* sub_0008DBF0 (the 0x200-byte twin pool) with the same size. */
void pc_mapmaker_objects_twin_pool(void (*original)(void))
{
    GuestRegisters r;
    uint32_t bytes,p;
    if(!mapmaker_objects_pool_on()) { original(); return; }
    save_registers(&r);
    WORD_AT(0x0070AAD8u)=MAPMAKER_OBJECTS_POOL;
    bytes=((MAPMAKER_OBJECTS_POOL<<9)+15u)&~15u;
    p=guest_call(sub_0003DFA0,2,bytes,1u);
    WORD_AT(0x0070AADCu)=p;
    WORD_AT(0x007B67D0u)=0;WORD_AT(0x007B6984u)=0;
    mapmaker_objects_clear(p,bytes);
    WORD_AT(0x004D9700u)=WORD_AT(0x006F73B0u)==0;
    restore_registers(&r);
    MAPMAKER_GUEST_RET();
}

/* ---- teleporters ------------------------------------------------------ */
static uint32_t mapmaker_objects_tele_pad[MAPMAKER_OBJECTS_TELEPORTER_CAP];
static uint32_t mapmaker_objects_tele_colour[MAPMAKER_OBJECTS_TELEPORTER_CAP];
/* sub_0023FD10's walk of the map container [0x00784014], bounded. */
static unsigned mapmaker_objects_teleporters_read(void)
{
    uint32_t base,p,end,pieces=0;
    unsigned n=0,guard;
    if(!guest_writable(0x00784014u,4)) return 0;
    base=WORD_AT(0x00784014u);p=base+8u;end=base+MAPMAKER_OBJECTS_CONTAINER_END;
    for(guard=0;p<end && guard<65536u && guest_writable(p,8);guard++) {
        uint32_t chunk=p,tag=WORD_AT(chunk);
        p=chunk+8u;
        if(tag==0x00C7766Au) break;
        if(tag==0x00C77668u) {
            if(!guest_writable(p+4u,4)) break;
            pieces=WORD_AT(p+4u);
        } else if(tag==0x00C77669u && (int32_t)pieces>0) {
            uint32_t q=p,k;
            for(k=0;k<pieces;k++) {
                uint32_t type,items,m;
                if(!guest_writable(q,0x28)) return n;
                type=WORD_AT(q);items=WORD_AT(q+0x1Cu)>>16;
                q+=(guest_call(sub_00333700,1,type)+guest_call(sub_002FDF50,1,type))*8u+0x28u;
                for(m=0;m<items;m++) {
                    int32_t id;
                    uint32_t pad;
                    if(!guest_writable(q,0x1C)) return n;
                    id=(int16_t)(WORD_AT(q+0xCu)>>16);
                    pad=WORD_AT(q+0x18u);
                    if(id>=0x0A && id<=0x0E && n<MAPMAKER_OBJECTS_TELEPORTER_CAP) {
                        if((int32_t)pad>0x1F40) {
                            uint32_t cover=guest_call(sub_0009CCA0,1,pad);
                            pad=guest_writable(cover,12)?(uint32_t)(int32_t)(int16_t)WORD_AT(cover+8u):UINT32_MAX;
                        }
                        mapmaker_objects_tele_pad[n]=pad;
                        mapmaker_objects_tele_colour[n]=WORD_AT(0x00524F60u+(uint32_t)id*0xC4u);
                        n++;
                    }
                    q+=((WORD_AT(q+0x14u)&0xFFu)*4u+0x1Bu)&~3u;
                }
            }
        }
        if(!guest_writable(chunk+4u,4)) break;
        p=chunk+8u+WORD_AT(chunk+4u);
        if(p<=chunk) break;
    }
    return n;
}
void pc_mapmaker_objects_teleporters(void (*original)(void))
{
    GuestRegisters r;
    unsigned n,i,j;
    uint32_t links;
    if(!mapmaker_objects_scene(100u)) { original(); return; }
    save_registers(&r);
    mapmaker_limits_read();
    links=mapmaker_limits.pools?MAPMAKER_LIMIT_LINKS:MAPMAKER_OBJECTS_STOCK_LINKS;
    n=mapmaker_objects_teleporters_read();
    for(i=0;i<n;i++) {
        uint32_t a=guest_call(sub_0009CC40,1,mapmaker_objects_tele_pad[i]);
        for(j=i+1;j<n;j++) {
            uint32_t b;
            if(mapmaker_objects_tele_colour[j]!=mapmaker_objects_tele_colour[i]) continue;
            b=guest_call(sub_0009CC40,1,mapmaker_objects_tele_pad[j]);
            g_edi=a;                                   /* sub_0009A2C0(edi = a, b): link exists? */
            if(!guest_call(sub_0009A2C0,1,b) && WORD_AT(0x007B6660u)<links) {
                g_ebx=a;g_edi=b;                       /* sub_0009A5B0: link ebx -> edi */
                guest_call(sub_0009A5B0,0);
            }
            break;
        }
    }
    restore_registers(&r);
    MAPMAKER_GUEST_RET();
}

/* ---- logic tags ------------------------------------------------------- */
static int mapmaker_objects_tags_full(void)
{
    uint32_t i;
    for(i=0;i<MAPMAKER_OBJECTS_TAGS_CAP;i++) if(!WORD_AT(MAPMAKER_OBJECTS_TAGS+i*16u)) return 0;
    return 1;
}
void pc_mapmaker_objects_tag(void (*original)(void))
{
    uint32_t tag,obj;
    if(!guest_writable(g_esp,12)) { original(); return; }
    tag=WORD_AT(g_esp+4);obj=WORD_AT(g_esp+8);
    if((int32_t)tag>0 && tag!=0x3F3F3F3Fu && guest_writable(obj,0x234) && mapmaker_objects_tags_full()) {
        WORD_AT(obj+0x60)=tag;
        WORD_AT(obj+0x230)=WORD_AT(0x00519750u);
        WORD_AT(0x00519750u)++;
        MAPMAKER_GUEST_RET();
        return;
    }
    original();
}
