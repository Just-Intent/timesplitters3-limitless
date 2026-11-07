/* US 45410066 MapMaker capacity, objects area: turrets, object lists,
 * the engine object pool, teleporters and logic tags (docs/mapmaker/mapmaker-objects.md,
 * logs: turrets.md, items.md 2.2-2.5).
 *
 * Capacity is raised by config/guest-code-patches/objects.json (every scene):
 * turret gun/owner pools 32 -> 80 and the System Link turret sync over all 80
 * slots; the per-player perception array 0x3E0 -> 0xA00 B (32 characters +
 * 128 objects); the sub_000A5CE0 object lists relocated into the reserved
 * block (slot 9 at 0x008A0000, 30 -> 128; slot 8 at 0x008AC000 and slot 0 at
 * 0x008AE000, 50 / 112 -> 2047); the room bucket buffer 4x; the logic
 * tag links (0x008A2400) and tags (0x008A4400) relocated at 250 -> 1000 each;
 * the placed-pickup records 120 -> 768 (MAPMAKER_OBJECTS_PICKUPS below); the
 * class-0x80 prop block pointers 60 -> 4096 (MAPMAKER_OBJECTS_PROP_BLOCKS).
 * config/guest-code-patches/handles.json (every scene, written by
 * tools/analysis/object-handles.py) moves object handles from slot << 22 to
 * slot << 21 (encode 0x000A69BE, serial wrap 0x000A69C4, all 167 decodes).
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
 *   0) the engine object pool and its twin hold 2047 objects instead of 550.
 *   Object handles keep the slot in their top 11 bits (handle >> 21, the
 *   handles.json table; stock 10 bits, >> 22) and a serial 1..0x1FFFFF
 *   ([0x0072F5E0], 0x000A69B9..0x000A69D3), so 2048 is the ceiling and only
 *   slot 2047 could make -1; 2047 slots never do. Handles never leave the
 *   process: System Link names objects by +0x230 (sub_00359400), saves hold
 *   no handles. Other scenes run the original sizes (550, or 20 in scene
 *   101), so story levels keep their memory use.
 * - pc_mapmaker_objects_teleporters (sub_0009CD00, scene 100): the original
 *   gathers teleporter pads into two 10-slot stack arrays without a bound
 *   (sub_0023FD10); this reads the same container walk into larger arrays,
 *   bounded, and pairs each teleporter with the next one of the same colour
 *   exactly as the original, skipping a nav link when the link pool is full
 *   and the self link pad -> pad of a pair stacked on one cell (the stock
 *   sub_0009A5B0 would add it; TSFP_MAPMAKER_STACK_ITEMS makes such maps).
 * - pc_mapmaker_objects_tag (sub_002402B0): when every tag slot is used the
 *   original writes at index -1; the object is then registered without a tag
 *   slot (tag field and serial written as the original).
 *
 * Requires the includer's WORD_AT, guest_writable, guest_call,
 * save_registers/restore_registers, g_esp/g_eax/g_ebx/g_edi,
 * g_xbox_mem_offset and pc_mapmaker_limits.inc (mapmaker_limits,
 * MAPMAKER_POOL_LINKS). */
#ifndef MAPMAKER_STACK_OK
/* Hot hooks check the guest stack with this instead of guest_writable,
 * whose VirtualQuery made a 186-piece map's AI setup take 13 minutes. */
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif
#ifndef MAPMAKER_GUEST_RET
#define MAPMAKER_GUEST_RET() (g_esp+=4u) /* see pc_mapmaker_limits.inc */
#endif
#include <string.h>

#define MAPMAKER_OBJECTS_TURRETS 80u          /* objects.json obj-turret-* */
#define MAPMAKER_OBJECTS_LIST9 0x008A0000u    /* obj-list9-relocate */
#define MAPMAKER_OBJECTS_LIST9_CAP 128u
#define MAPMAKER_OBJECTS_LIST8 0x008AC000u    /* obj-list8-relocate */
#define MAPMAKER_OBJECTS_LIST8_CAP 2047u      /* = the object pool: the guard never drops */
#define MAPMAKER_OBJECTS_LIST0 0x008AE000u    /* obj-list0-relocate */
#define MAPMAKER_OBJECTS_LIST0_CAP 2047u      /* = the object pool: the guard never drops */

/* Every guard that turns an object away counts it and logs the first few,
 * so a missing object in play is never silent (probe flag 64 prints the counts). */
static struct { unsigned list[3], turret, tag; } mapmaker_objects_drops;
static void mapmaker_objects_dropped(unsigned *n,const char *what,uint32_t obj)
{
    if((*n)++<3u) { fprintf(stderr,"[mapmaker] %s full: object 0x%08X not added\n",what,obj);fflush(stderr); }
}
#define MAPMAKER_OBJECTS_SPARE_GUNS 0x008A9000u   /* 8 x 0x1E0 */
#define MAPMAKER_OBJECTS_SPARE_OWNERS 0x008AA000u /* 8 x 0x130 */
#define MAPMAKER_OBJECTS_SPARES 8u
#define MAPMAKER_OBJECTS_POOL 2047u           /* slots 0..2046: slot = handle >> 21, never -1 */
#define MAPMAKER_OBJECTS_HANDLE_SHIFT 21u     /* handles.json: handle = slot << 21 | serial */
/* The editor's object limit (pc_mapmaker_limits.inc) is this pool. */
typedef char mapmaker_objects_pool_limit[(MAPMAKER_OBJECTS_POOL<(1u<<(32u-MAPMAKER_OBJECTS_HANDLE_SHIFT)) &&
                                          MAPMAKER_LIMIT_OBJECTS==MAPMAKER_OBJECTS_POOL &&
                                          MAPMAKER_OBJECTS_LIST8_CAP==MAPMAKER_OBJECTS_POOL &&
                                          MAPMAKER_OBJECTS_LIST0_CAP==MAPMAKER_OBJECTS_POOL)?1:-1];
/* Placed pickups (2026-11-02): every placed weapon, ammo, health, armour,
 * power-up, keycard, flag and special pickup (sub_000C1950 with arg0 bit 0
 * clear) takes a 0xD8-byte record from [0x00732E60] (count [0x00732E64]);
 * stock 120, after which the pickup is silently dropped (0x000C1DC7). The
 * table, its clear and the full test are raised to 768 by objects.json
 * obj-pickups-* in every scene (the test is `cmp ah,3`, so a multiple of
 * 256). Every reader loops to the count (test_mapmaker_pickups_classified.py).
 * The editor's 500-item total stays below it. */
#define MAPMAKER_OBJECTS_PICKUPS 768u
#define MAPMAKER_OBJECTS_PICKUP_BYTES 0xD8u
typedef char mapmaker_objects_pickup_table[(MAPMAKER_OBJECTS_PICKUPS%256u==0u && MAPMAKER_OBJECTS_PICKUPS<0x8000u &&
                                            MAPMAKER_LIMIT_SUB<MAPMAKER_OBJECTS_PICKUPS)?1:-1];
/* Prop blocks (2026-11-03, v0.4.0-mapmaker.37 barrel crash): sub_000AC940, the
 * init of class 0x80 (props, barrels), gives every prop whose template lacks
 * flag 8 a 0x160-byte block (+0x7C) through the pointer table [0x0072F728]
 * indexed by [0x007327A8], which only the loader's cancel (0x0006D1F9) ever
 * lowers; stock sized it 60 (sub_000A92A0). objects.json obj-propblocks-*
 * make it 2048 in every scene: every placed prop holds a pool slot, plus as
 * many again for creations during play (2048 for the 1023-slot pool, 4096
 * since 2026-11-07). Flag-8 props use the 30-block ring. */
#define MAPMAKER_OBJECTS_PROP_BLOCKS 4096u
typedef char mapmaker_objects_prop_blocks[(MAPMAKER_OBJECTS_PROP_BLOCKS>=2u*MAPMAKER_OBJECTS_POOL &&
                                           MAPMAKER_OBJECTS_PROP_BLOCKS*4u<=0x4000u)?1:-1];
#define MAPMAKER_OBJECTS_BUCKET_BYTES 0x14D00u /* obj-bucket-bytes (4x stock, as the pool) */
#define MAPMAKER_OBJECTS_TAGS 0x008A4400u     /* obj-tag-entries-relocate */
#define MAPMAKER_OBJECTS_TAGS_CAP 1000u
#define MAPMAKER_OBJECTS_STOCK_LINKS 4400u    /* stock sub_0009A140 link pool */
#define MAPMAKER_OBJECTS_TELEPORTER_CAP 1200u /* > 0x7FE0 / 0x1C items */
/* sub_0023FD10 walk bound: the largest map body (src/interface/pc_mapmaker_container.inc,
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
    return WORD_AT(0x0079094Cu)==scene;        /* fixed .bss scene dword */
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
    if(MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==0x0006E818u && mapmaker_objects_turret_full()) {
        g_eax=0;      /* sub_0006D7A0 skips a 0 result; esi/edi/ebx untouched */
        mapmaker_objects_dropped(&mapmaker_objects_drops.turret,"turret pool",0u);
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
    if((int32_t)WORD_AT(0x007B625Cu)>=(int32_t)MAPMAKER_OBJECTS_LIST9_CAP) {
        mapmaker_objects_dropped(&mapmaker_objects_drops.list[2],"object list 9",MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4):0u);
        MAPMAKER_GUEST_RET(); return;
    }
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
    if(!MAPMAKER_STACK_OK(16)) { original(); return; }
    obj=WORD_AT(g_esp+4);slot=WORD_AT(g_esp+8);flag=WORD_AT(g_esp+12);
    if(slot==0) { count=0x007B6258u;base=MAPMAKER_OBJECTS_LIST0;cap=MAPMAKER_OBJECTS_LIST0_CAP; }
    else if(slot==8) { count=0x007B4FA8u;base=MAPMAKER_OBJECTS_LIST8;cap=MAPMAKER_OBJECTS_LIST8_CAP; }
    else if(slot==9) { count=0x007B625Cu;base=MAPMAKER_OBJECTS_LIST9;cap=MAPMAKER_OBJECTS_LIST9_CAP; }
    else { original(); return; }
    if(flag && (int32_t)WORD_AT(count)>=(int32_t)cap && guest_writable(obj,0x154u+slot*4u+4u) &&
       !mapmaker_objects_listed(base,cap,obj)) {
        WORD_AT(obj+0x90)=WORD_AT(0x007DE318u);
        WORD_AT(obj+0x154u+slot*4u)=flag;
        mapmaker_objects_dropped(&mapmaker_objects_drops.list[slot==0?0:slot==8?1:2],"object list",obj);
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
extern void sub_0003E0B0(void);
/* Level heap allocation (sub_0003DFA0 for align 16, else sub_0003E0B0), or in
 * scene 100 the nav block first (pc_mapmaker_memory.inc). */
static uint32_t mapmaker_objects_take(uint32_t bytes,uint32_t align)
{
#ifdef MAPMAKER_OBJECTS_SOURCE
    if(mapmaker_objects_source && WORD_AT(0x0079094Cu)==100u) {
        uint32_t p=mapmaker_objects_source(bytes,align);
        if(p) return p;
    }
#endif
    return align==16u?guest_call(sub_0003DFA0,2,bytes,1u):guest_call(sub_0003E0B0,3,align,bytes,1u);
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
    WORD_AT(0x0072F5E8u)=mapmaker_objects_take((MAPMAKER_OBJECTS_POOL*0x2E8u+15u)&~15u,16u);   /* nav block in scene 100 */
    WORD_AT(0x0072F5ECu)=guest_call(sub_0003DFA0,2,0x190u,1u);
    WORD_AT(0x00715FB4u)=guest_call(sub_0003DFA0,2,MAPMAKER_OBJECTS_BUCKET_BYTES,1u);
    /* The per-room-id prop tables the original clears here were relocated by
     * the rooms area (config/guest-code-patches/rooms.json: 0x007B54E0,
     * 0x007B5840, 0x007B6260 -> 0x008CC000, 0x008CE080, 0x008D0100, cleared
     * with 0x40D dwords: ids 0..0x40C); clearing the old addresses left the
     * new ones stale. test_mapmaker_rooms.py pins these to rooms.json. */
    mapmaker_objects_clear(0x008CC000u,0x40Du*4u);
    mapmaker_objects_clear(0x008CE080u,0x40Du*4u);
    mapmaker_objects_clear(0x008D0100u,0x40Du*4u);
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
/* sub_0008DBF0 (the 0x200-byte twin pool: model instances). Twins are taken
 * by sub_00097C00 at its count [0x007B67D0] (or a freed one when
 * [0x007B6984] > 0) with no bound: one per object from the create
 * (0x000A6A3D), plus attachments from sub_0009F490 (characters,
 * sub_000DAC30) and sub_0009F510 (sub_000A7070, obj+0xA4, held weapons and
 * other attached models, many callers). So twins can outnumber objects; the
 * pool gets MAPMAKER_OBJECTS_TWIN_EXTRA more (2026-11-07). Its size dword
 * [0x0070AAD8] is read only by the sizer and the clear (sub_0008DBB0), the
 * index (twin - base) >> 9 is a time-slice key only, so a twin pool larger
 * than the object pool is safe. */
#define MAPMAKER_OBJECTS_TWIN_EXTRA 512u
#define MAPMAKER_OBJECTS_TWINS (MAPMAKER_OBJECTS_POOL+MAPMAKER_OBJECTS_TWIN_EXTRA)
#define MAPMAKER_OBJECTS_TWIN_COUNT 0x007B67D0u
#define MAPMAKER_OBJECTS_TWIN_FREED 0x007B6984u
#define MAPMAKER_OBJECTS_TWIN_MARGIN 32u          /* a create and its attachments */
typedef char mapmaker_objects_twin_pool_size[(MAPMAKER_OBJECTS_TWIN_EXTRA>=MAPMAKER_OBJECTS_TWIN_MARGIN*4u &&
                                              MAPMAKER_OBJECTS_TWINS<0x1000u)?1:-1];
void pc_mapmaker_objects_twin_pool(void (*original)(void))
{
    GuestRegisters r;
    uint32_t bytes,p;
    if(!mapmaker_objects_pool_on()) { original(); return; }
    save_registers(&r);
    WORD_AT(0x0070AAD8u)=MAPMAKER_OBJECTS_TWINS;
    bytes=((MAPMAKER_OBJECTS_TWINS<<9)+15u)&~15u;
    p=guest_call(sub_0003DFA0,2,bytes,1u);
    WORD_AT(0x0070AADCu)=p;
    WORD_AT(0x007B67D0u)=0;WORD_AT(0x007B6984u)=0;
    mapmaker_objects_clear(p,bytes);
    WORD_AT(0x004D9700u)=WORD_AT(0x006F73B0u)==0;
    restore_registers(&r);
    MAPMAKER_GUEST_RET();
}

/* ---- object creation at the pool's end -------------------------------- */
/* 2026-11-07 (v0.4.0-mapmaker.45, 106 Story AI, logs/m45c): sub_000A6880,
 * the object create and only caller of the slot allocator sub_0009EE00,
 * takes a slot with no bound. With no freed slot ([0x007B65B8] 0, or none of
 * [0, count) flagged +0x2B & 0x80) it appends at the count [0x007B4FAC], so
 * slot 1023+ lay past the pool and its twin (base [0x0070AADC] + slot x
 * 0x200) inside the object pool that follows the twin pool: twin 1026 wrote
 * a model over slot 2, and sub_0009FF10 faulted on its room list. While the
 * pools are on (scene 100/102) a create that would pass the pool returns 0
 * instead, the original's own result when the model cannot be made
 * (0x000A695C); sub_000A6EC0 and sub_000DAC30 test it. The actors area keeps
 * MAPMAKER_ACTORS_OBJECT_HEADROOM slots free for that reason, so this is the
 * last guard, not the limit. Since the twins also grow with attachments
 * (above), a create is refused the same way while fewer than
 * MAPMAKER_OBJECTS_TWIN_MARGIN twins are left and none is freed. */
#define MAPMAKER_OBJECTS_COUNT 0x007B4FACu
#define MAPMAKER_OBJECTS_FREED 0x007B65B8u
#define MAPMAKER_OBJECTS_SLOT_BYTES 0x2E8u
static unsigned mapmaker_objects_full;
static int mapmaker_objects_slot_free(void)
{
    uint32_t count=WORD_AT(MAPMAKER_OBJECTS_COUNT),cap=WORD_AT(0x0072F5E4u),base=WORD_AT(0x0072F5E8u),i;
    uint32_t twins=WORD_AT(0x0070AAD8u);
    if(twins && WORD_AT(MAPMAKER_OBJECTS_TWIN_COUNT)+MAPMAKER_OBJECTS_TWIN_MARGIN>twins &&
       !WORD_AT(MAPMAKER_OBJECTS_TWIN_FREED)) return 0;  /* model instances nearly full */
    if(count<cap) return 1;
    if(!WORD_AT(MAPMAKER_OBJECTS_FREED) || !base) return 0;
    for(i=0;i<cap;i++)                                  /* the allocator's own scan, within the pool */
        if(*(uint8_t *)((uintptr_t)(base+i*MAPMAKER_OBJECTS_SLOT_BYTES+0x2Bu)+g_xbox_mem_offset)&0x80u) return 1;
    return 0;
}
void pc_mapmaker_objects_create(void (*original)(void))
{
    if(mapmaker_objects_pool_on() && !mapmaker_objects_slot_free()) {
        if(mapmaker_objects_full++<8u || !(mapmaker_objects_full&0xFFu)) {
            fprintf(stderr,"[mapmaker] engine objects full (%u of %u; model instances %u of %u): object of template %u not created (%u so far)\n",
                    WORD_AT(MAPMAKER_OBJECTS_COUNT)-WORD_AT(MAPMAKER_OBJECTS_FREED),WORD_AT(0x0072F5E4u),
                    WORD_AT(MAPMAKER_OBJECTS_TWIN_COUNT)-WORD_AT(MAPMAKER_OBJECTS_TWIN_FREED),WORD_AT(0x0070AAD8u),
                    MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4):0u,mapmaker_objects_full);
            fflush(stderr);
        }
        g_eax=0;
        MAPMAKER_GUEST_RET();
        return;
    }
    original();
}

/* ---- physics bodies --------------------------------------------------- */
/* 2026-11-06 (v0.4.0-mapmaker.39/.40: shot barrels on a map near the object
 * cap neither exploded nor hurt anyone). sub_0016C430 (level init, right
 * after the object pools) sizes the rigid-body pool: P = word [0x0074C49C],
 * 50 unless mode [0x007DE455] is 0xA or [0x007DE458] has 0x40, 40 above scene
 * 100, else a per-scene table (scene 100: the default 100). Of the P + 0x60
 * bodies, kind 1 (objects knocked loose or broken) counts up from 0x58 in
 * [0x0074C45E] and is full at P + 0x58 (sub_0016C020(1); it never steals),
 * kind 2 counts down from the top and steals its oldest. The prop break
 * sub_000AE4F0 asks sub_0016C020(1) first; with no kind-1 body free it only
 * fades the prop out (state 9, then sub_000AC560): no explosion
 * (sub_000EFBD0), no effects, no damage. Each body belongs to one object
 * (+0x80), so P >= the 2047 pool slots can never refuse it. In MapMaker play
 * this wrapper runs the same allocations with P = MAPMAKER_OBJECTS_PHYSICS
 * (2048: about 1.2 MB of Level heap, 558 B per body; 1024 before the
 * 2047-slot pool); other scenes keep the original sizes. */
#define MAPMAKER_OBJECTS_PHYSICS 2048u
#define MAPMAKER_OBJECTS_HALF_AT(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
typedef char mapmaker_objects_physics_bodies[(MAPMAKER_OBJECTS_PHYSICS>=MAPMAKER_OBJECTS_POOL &&
                                              MAPMAKER_OBJECTS_PHYSICS+0x60u<0x8000u)?1:-1];
extern void sub_0003E0B0(void),sub_00165760(void);
void pc_mapmaker_objects_physics(void (*original)(void))
{
    GuestRegisters r;
    uint32_t n=MAPMAKER_OBJECTS_PHYSICS,a,one;
    mapmaker_limits_read();
    if(!mapmaker_limits.pools || !mapmaker_objects_scene(100u)) { original(); return; }
    save_registers(&r);
    MAPMAKER_OBJECTS_HALF_AT(0x0074C49Cu)=(uint16_t)n;
    WORD_AT(0x0074C458u)=mapmaker_objects_take((n+0x60u)<<9,0x40u);
    WORD_AT(0x0074C480u)=mapmaker_objects_take((n+0x60u)*4u,0x40u);
    WORD_AT(0x0074C46Cu)=mapmaker_objects_take((n+0x60u)*4u,0x40u);
    WORD_AT(0x0074C48Cu)=mapmaker_objects_take((n+0x60u)<<5,0x40u);
    WORD_AT(0x0074C494u)=mapmaker_objects_take((n+0x60u)*4u,0x40u);
    WORD_AT(0x0074C488u)=mapmaker_objects_take((n+0x60u)*2u,0x40u);
    WORD_AT(0x0074C464u)=guest_call(sub_0003DFA0,2,0x2E80u,1u);   /* 0x5D x 0x80, not per body */
    WORD_AT(0x0074C498u)=guest_call(sub_0003DFA0,2,0xBAu,1u);
    WORD_AT(0x0074C478u)=guest_call(sub_0003DFA0,2,0x6D60u,1u);
    guest_call(sub_00165760,0);                                   /* free lists and counters */
    WORD_AT(0x007A20CCu)=0;WORD_AT(0x007A20C8u)=0;
    one=WORD_AT(0x00475C78u);
    for(a=0x0074C414u;a<0x0074C450u;a+=4u) WORD_AT(a)=one;
    WORD_AT(0x0074C450u)=0;
    restore_registers(&r);
    MAPMAKER_GUEST_RET();
}
/* sub_000AE4F0(prop, 1), the prop break (barrels, crates): in MapMaker play,
 * says on stderr why a break gets no explosion (no kind-1 body) or no debris
 * (sub_000AB590/sub_000A4C70: live + 5 slots must stay below the pool), with
 * the counts; the first few normal breaks too. Then runs the original. */
static unsigned mapmaker_objects_breaks[2];
void pc_mapmaker_objects_prop_break(void (*original)(void))
{
    if(mapmaker_objects_scene(100u) && MAPMAKER_STACK_OK(8)) {
        int32_t used=(int16_t)MAPMAKER_OBJECTS_HALF_AT(0x0074C45Eu)-0x58;
        int32_t bodies=(int32_t)MAPMAKER_OBJECTS_HALF_AT(0x0074C49Cu);
        int32_t live=(int32_t)(WORD_AT(0x007B4FACu)-WORD_AT(0x007B65B8u)),slots=(int32_t)WORD_AT(0x0072F5E4u);
        int explode=used<bodies,debris=live+1+4<slots;
        unsigned *n=&mapmaker_objects_breaks[explode && debris];
        if((*n)++<(explode && debris?3u:16u)) {
            fprintf(stderr,"[mapmaker] prop break: object 0x%08X: %s; physics bodies %d of %d, objects %d of %d, debris %u of 20\n",
                    WORD_AT(g_esp+4),!explode?"no explosion or damage, refused: no physics body free":
                    !debris?"explosion; debris refused: fewer than 5 object slots free":"explosion",
                    (int)used,(int)bodies,(int)live,(int)slots,WORD_AT(0x0072F6A4u));
            fflush(stderr);
        }
    }
    original();
}

/* ---- teleporters ------------------------------------------------------ */
static uint32_t mapmaker_objects_tele_pad[MAPMAKER_OBJECTS_TELEPORTER_CAP];
static uint32_t mapmaker_objects_tele_colour[MAPMAKER_OBJECTS_TELEPORTER_CAP];
static unsigned mapmaker_objects_self_links;
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
                        /* a cover pad id (0x2329.., below the high regular ids 0x5B29.. of
                         * pc_mapmaker_pads.inc) stands for its owner; until 2026-11-07 this
                         * read "> 0x1F40", which took regular pads 7000..7999 as cover */
                        if((int32_t)pad>=0x2329 && (int32_t)pad<0x5B29) {
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
    links=mapmaker_limits.pools?MAPMAKER_POOL_LINKS:MAPMAKER_OBJECTS_STOCK_LINKS;  /* the pool, not the editor limit */
    n=mapmaker_objects_teleporters_read();
    for(i=0;i<n;i++) {
        uint32_t a=guest_call(sub_0009CC40,1,mapmaker_objects_tele_pad[i]);
        for(j=i+1;j<n;j++) {
            uint32_t b;
            if(mapmaker_objects_tele_colour[j]!=mapmaker_objects_tele_colour[i]) continue;
            b=guest_call(sub_0009CC40,1,mapmaker_objects_tele_pad[j]);
            if(a==b) {                                 /* stacked pair on one pad: no self link */
                if(mapmaker_objects_self_links++<3u) {
                    fprintf(stderr,"[mapmaker] teleporter pair on one pad 0x%X: no nav link\n",a);fflush(stderr);
                }
                break;
            }
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
    if(!MAPMAKER_STACK_OK(12)) { original(); return; }
    tag=WORD_AT(g_esp+4);obj=WORD_AT(g_esp+8);
    if((int32_t)tag>0 && tag!=0x3F3F3F3Fu && guest_writable(obj,0x234) && mapmaker_objects_tags_full()) {
        WORD_AT(obj+0x60)=tag;
        WORD_AT(obj+0x230)=WORD_AT(0x00519750u);
        WORD_AT(0x00519750u)++;
        mapmaker_objects_dropped(&mapmaker_objects_drops.tag,"logic tag table",obj);
        MAPMAKER_GUEST_RET();
        return;
    }
    original();
}
