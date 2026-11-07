/* US 45410066 MapMaker story vehicles (area `vehicles`, story assets wave 3,
 * docs/mapmaker/mapmaker-story-assets.md items 19 and 20; every site and its
 * evidence: tests/mapmaker/test_mapmaker_vehicles.py; player and design notes:
 * docs/mapmaker/mapmaker-vehicles.md).
 *
 * The MapMaker vehicle (catalogue item 0x2B, category 35, cap 4 = the stock
 * pool of 4 vehicle blocks, sub_0017F2F0) gets an options menu with a
 * "Vehicle" choice, saved in the item record byte +0xC (stock-unused for
 * 0x2B: the compiler's default branch 0x00300FFA zeroes +0..+0xA only; the
 * editor loader and the play fill read +0xC only for 0x35..0x37):
 *  1 the Machine Wars future tank: vehicle def 2 (builder sub_00181D40),
 *    model 0xB rbcar/futuretank, main gun template 1 (model 0xC, gun type
 *    0x3B) and AA gun template 2 (model 0xD, gun type 0x3C), the story
 *    records' tracks row 4 (l_5_st 'TANK'/'TNK2'/'TNK3': +0x24 2, +0x28 4,
 *    +0x2C 4, +0x30 1, +0x34 2). Driver and gunner seats have no character
 *    anims (all seat anim ids 0), as in the story;
 *  2 the Scotland the Brave truck with its Vickers gun: def 0 (sub_00180BA0),
 *    model 7 rbcar/truck, record +0x30 >= 0 adds the gunner seat (anim 0x83
 *    'gunner') with template 0 (model 0xE, gun type 0x2A) (l_4_st 'TRCK':
 *    +0x28 0, +0x2C 1, +0x30 0, +0x34 -1);
 *  3 the same truck without the gun (+0x30 -1, the second l_4_st truck).
 * The truck's seat anims 0x78..0x83 are all in pak/anim/animcar.pak (table
 * 0x0047FDF0, 27 entries), which scene 100 mounts for every map (0x001C878F);
 * the stock buggy uses 0x7F..0x8A from the same table.
 * A stock build plays such a map with the theme vehicle; the save stays
 * stock-sized and System Link sends it. Up to 4 vehicles per map in all (the
 * pool; category 35 is not raised): any mix of tanks, trucks and buggies.
 *
 * Editor (scene 102, TSFP_MAPMAKER_LIMITS=1):
 *  - category (sub_002F7770 at 0x003051B5 / 0x003051D7): for a selected
 *    vehicle, a copy of category 35's entry with +8 = a heap descriptor
 *    {sub_00319300, sub_003192E0, 0, 0} (category 35 has no options menu);
 *  - build (sub_00319300 for that descriptor): the stock frame, one
 *    "Vehicle" choice bound to node +0x30 (VH_NODE_TAG | variant), Done;
 *  - compile / load: node <-> record pairing as pc_mapmaker_storyprops.inc.
 * Play (scene 100, switch on, any mode):
 *  - mount (sub_00062790): pak/stream/lv5/prop1.pak (tank: models 0xB, 0xC,
 *    0xD, gibs 0x54D..0x54F and all their textures) and/or
 *    pak/stream/lv4/prop3.pak (truck 7, Vickers 0xE and their textures),
 *    kept open (index only; every file they share with another pak is
 *    byte-identical there);
 *  - preload model (sub_0006C090 at 0x0023F5DB, esi = item): the variant's
 *    model is requested instead of the theme buggy;
 *  - def (sub_0033DDA0): at the preload (0x0023F637, esi = item) the variant
 *    def goes to sub_00180700 (its sounds; def 2 also its gib models) and the
 *    gun templates are preloaded the story way, sub_000B3B20 (model, gun
 *    type sub_001B6010, sounds), as sub_001807A0 does in story levels; at the
 *    fill (0x00241D39, esi = item, ebp = the 0x40-byte vehicle record whose
 *    model +8 is already written) the variant def and model, only when every
 *    model it creates is read (sub_000B3B80 uses its turret object without a
 *    null test, 0x000B3BBF), else the theme vehicle stays and is logged;
 *  - textures (sub_00063900, before the texture queue loads): each variant
 *    model the load pass deferred (request +2 & 0x10) is registered the load
 *    pass's way, sub_00096220(path), as the storyprops area does;
 *  - create (sub_00182A20 at 0x0006E949, also the restart's re-create, whose
 *    records keep the fill's values): +0x28, +0x2C, +0x30 (the fill writes
 *    0, 1, -1 after the def call) and +0x34 of a filled variant record.
 * Not changed: the death handler sub_00174120 makes gibs and explosions only
 * in scene 5 (0x00174152) and the impact hook only calls the scene-5 script
 * (0x0017BB19): in scene 100 a tank, like the stock buggies, is not destroyed.
 * Requires WORD_AT, guest_call, g_eax, g_ebp, g_esi, g_esp, g_xbox_mem_offset,
 * xbox_HeapAlloc, pc_mapmaker_limits.inc (and MAPMAKER_* from the areas). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAPMAKER_GUEST_RET
#define MAPMAKER_GUEST_RET() (g_esp+=4u)
#endif
#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif
#ifndef MAPMAKER_PTR_OK
#define MAPMAKER_PTR_OK(p,n) ((((uint32_t)(p))>=0x00010000u && ((uint32_t)(p))<0x04000000u-(uint32_t)(n)) || \
                              (((uint32_t)(p))>=0x80000000u && ((uint32_t)(p))<0x84000000u-(uint32_t)(n)))
#endif

#define VH_SCENE 0x0079094Cu
#define VH_EDITOR 0x007844A8u
#define VH_PIECES 0x0078400Cu            /* editor pieces: next +0x40, nodes +0x3C, node next +0x14 */
#define VH_CONTAINER 0x00784014u
#define VH_SELECTED 0x2F1Cu              /* editor: the selected node (options menus) */
#define VH_MAGIC 0x00C77667u
#define VH_VERSION 0x12Du
#define VH_INFO 0x00C77668u
#define VH_PIECECHUNK 0x00C77669u
#define VH_END 0x00C7766Au
#define VH_SCAN 0x7FFE0u
#define VH_ITEM 0x2Bu                    /* the MapMaker vehicle (category 35) */
#define VH_REC_VAR 0x0Cu                 /* item record byte: 0 theme vehicle, 1..3 story variant */
#define VH_NODE_VAR 0x30u                /* editor node dword: VH_NODE_TAG | variant */
#define VH_NODE_TAG 0x56480000u          /* 'VH' */
#define VH_KINDS 3u                      /* variants 1..3 */
#define VH_PAKS 2u
#define VH_PAKTABLE 0x00907000u          /* mount table (paktable.json): 64 x 0x1C, +8 count, +0x10 index */
#define VH_SLOT 0x1Cu
#define VH_SLOTS 64u
#define VH_INDEX_MAX 0x4000u
#define VH_TEX_MAX 64u
#define VH_CAT_RET1 0x003051B5u          /* sub_003050xx: options button, category +8 test */
#define VH_CAT_RET2 0x003051D7u          /* ... and the descriptor pushed to sub_002FB230 */
#define VH_PRELOAD_RET 0x0023F5DBu       /* sub_0023F390: sub_0006C090(catalogue, theme) -> request */
#define VH_DEF_PRELOAD_RET 0x0023F637u   /* sub_0023F390 vehicle case: sub_00180700(sub_0033DDA0(id, theme)) */
#define VH_DEF_FILL_RET 0x00241D39u      /* sub_00240C50 vehicle case: record +0x24 = sub_0033DDA0(id, theme) */
#define VH_CREATE_RET 0x0006E949u        /* sub_0006D7A0: sub_00182A20(object, record) */
#define VH_BUILD 0x00319300u
#define VH_TICK 0x003192E0u
#define VH_CLOSE 0x002FB3C0u
#define VH_DONE_TEXT 0xB5Eu
#define VH_POS_BASE 0x0049F7E8u
#define VH_POS_LIST 0x007BB1E8u
#define VH_RESMAP 0x008CA000u            /* int16 per model (moved by the rooms area) */
#define VH_RESMAP_MODELS 0xA00u
#define VH_RES 0x00791D4Cu               /* resource table, 0x30 per entry */
#define VH_MOUNTS 0x006D7880u
#define VH_MOUNT_MAX 64u
#define VH_MOUNT_KEEP 8u
#define VH_MOUNT_SKIP 0x006D7C4Cu        /* sub_00062790 does nothing when set */
#define VH_TEMPLATES 0x004E8300u         /* vehicle gun templates, 0x8C each: 0 Vickers, 1 tank main, 2 tank AA */
#define VH_TEMPLATE 0x8Cu
#define VH_RECS 8u                       /* filled variant records per level (4 in the pool) */
#define VH_TEXT_BYTES 32u
#define VH_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define VH_H(a) (*(int16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define VH_F(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

extern void sub_00075B70(void),sub_00071B10(void),sub_00071C50(void),sub_00079DE0(void),sub_0007D4A0(void),
    sub_00071800(void),sub_00078590(void),sub_000783D0(void),sub_00079D60(void),sub_00075E70(void),
    sub_00077320(void),sub_0003D7B0(void),sub_00333700(void),sub_002FDF50(void),
    sub_000622D0(void),sub_00062260(void),sub_000631C0(void),sub_00096220(void),sub_000B3B20(void);

/* Variant k (1..3) is vh_kind[k-1]: def, model, record +0x28/+0x2C/+0x30/
 * +0x34, gun templates (-1 none), the models it creates (all need a scene-100
 * resource row; the first `ready` must be read before it is placed). */
static const struct {
    uint32_t def, model, r28, r2c, r30, r34;
    int32_t templates[2];
    uint32_t models[6]; unsigned nmodels, ready, pak;
    const char *what;
} vh_kind[VH_KINDS]={
    {2u,0xBu,4u,4u,1u,2u,{1,2},{0xBu,0xCu,0xDu,0x54Du,0x54Eu,0x54Fu},6u,3u,0u,"future tank"},
    {0u,7u,0u,1u,0u,0xFFFFFFFFu,{0,-1},{7u,0xEu},2u,2u,1u,"truck with Vickers gun"},
    {0u,7u,0u,1u,0xFFFFFFFFu,0xFFFFFFFFu,{-1,-1},{7u},1u,1u,1u,"truck"},
};
static const char *const vh_pak_path[VH_PAKS]={"pak/stream/lv5/prop1.pak","pak/stream/lv4/prop3.pak"};
/* Editor texts: label + four choices (guest copies on the heap). */
static const char vh_texts[5][VH_TEXT_BYTES]={"Vehicle","Theme vehicle","Future tank (Machine Wars)",
                                              "Castle truck + Vickers gun","Castle truck"};

static struct {
    uint32_t desc, cat, texts, paks[VH_PAKS];
    unsigned logged, failed;
} vh;
/* This level (cleared by every level's pak mount; a restart keeps it). */
static struct {
    unsigned want[VH_KINDS+1u];          /* variant items in the map, per variant */
    int textured[VH_PAKS];               /* textures registered this level, per pak */
    unsigned refused[VH_KINDS+1u], placed[VH_KINDS+1u], created;
    uint32_t rec[VH_RECS]; uint8_t rec_var[VH_RECS]; unsigned nrec;
} vh_lv;

static void vh_log(const char *what,uint32_t a,uint32_t b)
{
    if(vh.logged>=32u) return;
    vh.logged++;
    fprintf(stderr,"[mapmaker] vehicles: %s (0x%X, 0x%X)\n",what,(unsigned)a,(unsigned)b);
    fflush(stderr);
}
static int vh_on(void)
{
    mapmaker_limits_read();
    return mapmaker_limits.limits;
}
static uint32_t vh_editor(void)
{
    uint32_t e;
    if(!vh_on() || WORD_AT(VH_SCENE)!=102u) return 0;
    e=WORD_AT(VH_EDITOR);
    return MAPMAKER_PTR_OK(e,0x11950u)?e:0u;
}
static int vh_play(void) { return vh_on() && WORD_AT(VH_SCENE)==100u; }
static unsigned vh_node_var(uint32_t n)
{
    uint32_t v=WORD_AT(n+VH_NODE_VAR);
    return (v&0xFFFF0000u)==VH_NODE_TAG && (v&0xFFFFu)>=1u && (v&0xFFFFu)<=VH_KINDS?(v&0xFFFFu):0u;
}
static unsigned vh_rec_var(uint32_t item)
{
    unsigned v=VH_B(item+VH_REC_VAR);
    return v>=1u && v<=VH_KINDS?v:0u;
}
static int vh_alloc(void)
{
    if(!vh.texts) {
        uint32_t d=xbox_HeapAlloc(16u,4u),c=xbox_HeapAlloc(24u,4u),t=xbox_HeapAlloc(sizeof vh_texts,4u);
        if(!d || !c || !t || !MAPMAKER_PTR_OK(d,16u) || !MAPMAKER_PTR_OK(c,24u) || !MAPMAKER_PTR_OK(t,sizeof vh_texts)) {
            if(vh.failed++<4u) vh_log("no memory for the vehicle menu",0,0);
            return 0;
        }
        WORD_AT(d)=VH_BUILD;WORD_AT(d+4u)=VH_TICK;WORD_AT(d+8u)=0;WORD_AT(d+12u)=0;
        memcpy((void *)((uintptr_t)t+g_xbox_mem_offset),vh_texts,sizeof vh_texts);
        vh.desc=d;vh.cat=c;vh.texts=t;
    }
    return 1;
}
static uint32_t vh_text(unsigned k) { return vh.texts+k*VH_TEXT_BYTES; }

/* ---- the container ----------------------------------------------------- */
typedef void (*VhItemFn)(uint32_t item,uint32_t piece,void *ctx);
static uint32_t vh_chunk(uint32_t c,uint32_t tag,uint32_t *size)
{
    uint32_t p,end;
    unsigned steps;
    if(!c || !MAPMAKER_PTR_OK(c,VH_SCAN+8u) || WORD_AT(c)!=VH_MAGIC || WORD_AT(c+4u)!=VH_VERSION) return 0;
    end=c+VH_SCAN;
    for(p=c+8u,steps=0;steps<0x40000u && end-p>=8u;steps++) {
        uint32_t t=WORD_AT(p),s=WORD_AT(p+4u);
        if(t==VH_END || s>end-p-8u) return 0;
        if(t==tag) { *size=s;return p+8u; }
        p+=8u+s;
    }
    return 0;
}
/* Every vehicle item record: fn(item, piece). 0 when unreadable. */
static int vh_items(uint32_t c,VhItemFn fn,void *ctx)
{
    uint32_t isz=0,info=vh_chunk(c,VH_INFO,&isz),size=0,data,p,end,count,i,j;
    if(!info || isz<8u || !(data=vh_chunk(c,VH_PIECECHUNK,&size))) return 0;
    count=WORD_AT(info+4u);
    if((int32_t)count<0) count=0;
    for(p=data,end=data+size,i=0;i<count && end-p>=0x28u && i<0x10000u;i++) {
        uint32_t type=WORD_AT(p),pads=guest_call(sub_00333700,1,type)+guest_call(sub_002FDF50,1,type),items,piece=p;
        items=(uint16_t)VH_H(p+0x1Eu);
        if(pads>(end-p-0x28u)/8u) return 0;
        p+=0x28u+pads*8u;
        for(j=0;j<items;j++) {
            uint32_t n;
            if(end-p<0x18u) return 0;
            n=((uint32_t)VH_B(p+0x14u)*4u+0x1Bu)&~3u;
            if(n>end-p) return 0;
            if((uint32_t)(int32_t)VH_H(p+0xEu)==VH_ITEM) fn(p,piece,ctx);
            p+=n;
        }
    }
    return 1;
}

/* ---- editor nodes <-> saved records (as pc_mapmaker_storyprops.inc) ------
 * Key: the piece's cvttss2si x/y/z bytes and byte +0xC (compile
 * 0x00300BBF..0x00300BED), the item id, rotation and cell bytes
 * (0x00300D98..0x00300DB4, 0x00300ED8). Identical keys pair in order. */
#define VH_MAXN 512u
typedef struct { uint32_t at; uint8_t key[11]; } VhKey;
static VhKey vh_nodes[VH_MAXN],vh_recs[VH_MAXN];
static unsigned vh_nn,vh_nr;
static uint8_t vh_trunc8(float f)
{
    if(!(f>-2147483648.0f && f<2147483648.0f)) return 0;   /* cvttss2si: 0x80000000 */
    return (uint8_t)(int32_t)f;
}
static void vh_collect_nodes(void)
{
    uint32_t piece=WORD_AT(VH_PIECES),i,j;
    vh_nn=0;
    for(i=0;piece && i<0x10000u && MAPMAKER_PTR_OK(piece,0x48u);i++,piece=WORD_AT(piece+0x40u)) {
        uint32_t n=WORD_AT(piece+0x3Cu);
        uint8_t pk[4];
        pk[0]=vh_trunc8(VH_F(piece));pk[1]=vh_trunc8(VH_F(piece+4u));pk[2]=vh_trunc8(VH_F(piece+8u));pk[3]=VH_B(piece+0xCu);
        for(j=0;n && j<0x1000u && MAPMAKER_PTR_OK(n,0x3Cu);j++,n=WORD_AT(n+0x14u)) {
            VhKey *k;
            if(WORD_AT(n)!=VH_ITEM || vh_nn>=VH_MAXN) continue;
            k=&vh_nodes[vh_nn++];k->at=n;
            memcpy(k->key,pk,4);
            k->key[4]=VH_B(n);k->key[5]=VH_B(n+8u);k->key[6]=VH_B(n+9u);
            k->key[7]=VH_B(n+0xCu);k->key[8]=VH_B(n+0xEu);k->key[9]=VH_B(n+0x10u);k->key[10]=0;
        }
    }
}
static void vh_collect_rec(uint32_t item,uint32_t piece,void *ctx)
{
    VhKey *k;
    (void)ctx;
    if(vh_nr>=VH_MAXN) return;
    k=&vh_recs[vh_nr++];k->at=item;
    k->key[0]=VH_B(piece+4u);k->key[1]=VH_B(piece+5u);k->key[2]=VH_B(piece+6u);k->key[3]=VH_B(piece+7u);
    k->key[4]=VH_B(item+0xEu);k->key[5]=VH_B(item+0x10u);k->key[6]=VH_B(item+0x11u);
    k->key[7]=VH_B(item+0x15u);k->key[8]=VH_B(item+0x16u);k->key[9]=VH_B(item+0x17u);k->key[10]=0;
}
static uint32_t vh_pair(unsigned r)
{
    unsigned i,nth=0;
    for(i=0;i<r;i++) if(!memcmp(vh_recs[i].key,vh_recs[r].key,sizeof vh_recs[r].key)) nth++;
    for(i=0;i<vh_nn;i++)
        if(!memcmp(vh_nodes[i].key,vh_recs[r].key,sizeof vh_recs[r].key) && !nth--) return vh_nodes[i].at;
    return 0;
}

/* WRAP sub_003006C0 (cdecl container, flags; returns used): the records of
 * vehicle items get byte +0xC = the node's variant (0 for the theme vehicle). */
void pc_mapmaker_vehicles_compile(void (*original)(void))
{
    uint32_t c;
    unsigned k,set=0,unpaired=0;
    if(!MAPMAKER_STACK_OK(8)) { original();return; }
    c=WORD_AT(g_esp+4u);
    original();
    if(!vh_editor()) return;
    vh_collect_nodes();
    vh_nr=0;
    if(!vh_nn || !vh_items(c,vh_collect_rec,0)) return;
    for(k=0;k<vh_nr;k++) {
        uint32_t n=vh_pair(k);
        unsigned v=n?vh_node_var(n):0u;
        if(!n) unpaired++;
        VH_B(vh_recs[k].at+VH_REC_VAR)=(uint8_t)v;
        set+=v!=0u;
    }
    if(unpaired) vh_log("vehicle items not paired with the editor; saved as theme vehicles",unpaired,vh_nr);
    if(set) vh_log("saved story vehicles (items, records)",set,vh_nr);
}

/* WRAP sub_00301A90 (cdecl container, flags): editor load. The loader's
 * node template is not cleared, so every vehicle node is reset first. */
void pc_mapmaker_vehicles_load(void (*original)(void))
{
    uint32_t c=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u;
    unsigned k;
    original();
    if(!vh_editor()) return;
    vh_collect_nodes();
    for(k=0;k<vh_nn;k++) WORD_AT(vh_nodes[k].at+VH_NODE_VAR)=VH_NODE_TAG;
    vh_nr=0;
    if(!vh_nn || !vh_items(c,vh_collect_rec,0)) return;
    for(k=0;k<vh_nr;k++) {
        uint32_t n;
        unsigned v=vh_rec_var(vh_recs[k].at);
        if(v && (n=vh_pair(k))) WORD_AT(n+VH_NODE_VAR)=VH_NODE_TAG|v;
    }
}

/* WRAP sub_002F7770 (cdecl item id -> category entry 0x00524BE0 + cat*24):
 * the Objects-mode options button of a selected vehicle gets an entry whose
 * +8 is our descriptor; every other caller and item the stock entry. */
void pc_mapmaker_vehicles_category(void (*original)(void))
{
    uint32_t ret,e,n;
    if(!MAPMAKER_STACK_OK(8) || ((ret=WORD_AT(g_esp))!=VH_CAT_RET1 && ret!=VH_CAT_RET2) || WORD_AT(g_esp+4u)!=VH_ITEM) {
        original();return;
    }
    original();
    if(!(e=vh_editor()) || !MAPMAKER_PTR_OK(g_eax,24u) || WORD_AT(g_eax+8u) || !(n=WORD_AT(e+VH_SELECTED)) ||
       !MAPMAKER_PTR_OK(n,0x3Cu) || WORD_AT(n)!=VH_ITEM || !vh_alloc()) return;
    memcpy((void *)((uintptr_t)vh.cat+g_xbox_mem_offset),(const void *)((uintptr_t)g_eax+g_xbox_mem_offset),24u);
    WORD_AT(vh.cat+8u)=vh.desc;
    g_eax=vh.cat;
}

/* WRAP sub_00319300 (options menu build, cdecl menu): for our descriptor the
 * stock frame, a "Vehicle" choice bound to the selected vehicle's node +0x30,
 * and Done. */
void pc_mapmaker_vehicles_build(void (*original)(void))
{
    uint32_t menu=MAPMAKER_STACK_OK(8u)?WORD_AT(g_esp+4u):0u,e,n,y;
    unsigned k;
    float f;
    if(!vh.desc || !MAPMAKER_PTR_OK(menu,0x220u) || WORD_AT(menu+4u)!=vh.desc) { original();return; }
    e=vh_editor();n=e?WORD_AT(e+VH_SELECTED):0u;
    guest_call(sub_00075B70,5,0x8400u,0u,0x42480000u,0u,VH_CLOSE);
    WORD_AT(menu+0x218u)=1u;
    guest_call(sub_00071B10,4,1u,0x3F000000u,1u,1u);
    guest_call(sub_00071C50,1,0x20000u);
    if(MAPMAKER_PTR_OK(n,0x3Cu) && WORD_AT(n)==VH_ITEM) {
        WORD_AT(n+VH_NODE_VAR)=VH_NODE_TAG|vh_node_var(n);      /* a stale value becomes the theme vehicle */
        guest_call(sub_00079DE0,5,vh_text(0),1u,0x08000800u,0u,0u);
        guest_call(sub_0007D4A0,2,0u,0u);
        for(k=0;k<=VH_KINDS;k++) guest_call(sub_00071800,2,vh_text(1u+k),VH_NODE_TAG|k);
        guest_call(sub_00078590,1,n+VH_NODE_VAR);
        guest_call(sub_000783D0,0);
    } else vh_log("vehicle options opened without a selected vehicle",n,0);
    guest_call(sub_00079D60,6,guest_call(sub_0003D7B0,1,VH_DONE_TEXT),0xFFFFFFFFu,0u,0x40000000u,VH_CLOSE,0u);
    guest_call(sub_00075E70,0);
    f=VH_F(VH_POS_BASE)-VH_F(menu+0x68u);
    memcpy(&y,&f,4);
    guest_call(sub_00077320,2,WORD_AT(VH_POS_LIST),y);
    MAPMAKER_GUEST_RET();
}

/* ---- play -------------------------------------------------------------- */
static void vh_want(uint32_t item,uint32_t piece,void *ctx)
{
    (void)piece;(void)ctx;
    vh_lv.want[vh_rec_var(item)]++;
}
static int vh_pak_wanted(unsigned j)
{
    unsigned v;
    for(v=1u;v<=VH_KINDS;v++) if(vh_lv.want[v] && vh_kind[v-1u].pak==j) return 1;
    return 0;
}
static uint32_t vh_pak(unsigned j)
{
    if(!vh.paks[j]) {
        const char *s=vh_pak_path[j];
        size_t n=strlen(s)+1u;
        uint32_t p=xbox_HeapAlloc((uint32_t)n,4u);
        if(!p || !MAPMAKER_PTR_OK(p,n)) { vh_log("no memory for a pak path",j,0);return 0; }
        memcpy((void *)((uintptr_t)p+g_xbox_mem_offset),s,n);
        vh.paks[j]=p;
    }
    return vh.paks[j];
}
static int32_t vh_slot(unsigned j) { return vh.paks[j]?(int32_t)guest_call(sub_00062260,1,vh.paks[j]):-1; }
static uint32_t vh_resource(uint32_t model)
{
    int16_t i;
    uint32_t t=WORD_AT(VH_RES),e;
    if(model>=VH_RESMAP_MODELS) return 0;
    i=VH_H(VH_RESMAP+model*2u);
    if(i<0 || !t) return 0;
    e=t+(uint32_t)i*0x30u;
    return MAPMAKER_PTR_OK(e,0x30u)?e:0;
}
static uint32_t vh_request(uint32_t path)
{
    uint32_t r;
    if(!path || !MAPMAKER_PTR_OK(path,4u)) return 0;
    r=guest_call(sub_000631C0,1,path);
    return r && MAPMAKER_PTR_OK(r,0x14u)?r:0u;
}
static int vh_model_read(uint32_t model)
{
    uint32_t e=vh_resource(model),r;
    if(!e) return 0;
    if(WORD_AT(e+4u)) return 1;
    r=vh_request(WORD_AT(e+0x10u));
    return r && WORD_AT(r+0x10u)!=0;
}
/* Variant v can be requested: its pak is mounted and every model it creates
 * has a scene-100 resource row (sub_000A5E40 indexes the row unchecked). */
static int vh_available(unsigned v)
{
    unsigned i;
    if(!v || v>VH_KINDS || !vh_lv.want[v] || vh_slot(vh_kind[v-1u].pak)==-1) return 0;
    for(i=0;i<vh_kind[v-1u].nmodels;i++) if(!vh_resource(vh_kind[v-1u].models[i])) return 0;
    return 1;
}
/* ... and every model it creates at once (vehicle, guns) was read. */
static int vh_ready(unsigned v)
{
    unsigned i;
    if(!vh_available(v)) return 0;
    for(i=0;i<vh_kind[v-1u].ready;i++) if(!vh_model_read(vh_kind[v-1u].models[i])) return 0;
    return 1;
}
/* CRC-32 (zlib) of a pak entry name, as the game hashes it (sub_00150530). */
static uint32_t vh_crc(const char *s)
{
    uint32_t c=0xFFFFFFFFu;
    int k;
    for(;*s;s++) {
        c^=(uint8_t)*s;
        for(k=0;k<8;k++) c=c&1u?(c>>1)^0xEDB88320u:c>>1;
    }
    return ~c;
}
static int vh_slot_has(int32_t s,uint32_t id)
{
    char name[24];
    uint32_t base,n,index,i,crc;
    if(s<0 || (uint32_t)s>=VH_SLOTS) return 0;
    base=VH_PAKTABLE+(uint32_t)s*VH_SLOT;
    n=WORD_AT(base+8u);index=WORD_AT(base+0x10u);
    if(!n || n>VH_INDEX_MAX || !MAPMAKER_PTR_OK(index,n*16u)) return 0;
    snprintf(name,sizeof name,"textures/%06x.xbt",(unsigned)(id&0xFFFFFFu));
    crc=vh_crc(name);
    for(i=0;i<n;i++) if(WORD_AT(index+i*16u)==crc) return 1;
    return 0;
}
/* One model's textures: log which the pak holds; register a deferred model
 * the load pass's way. */
static void vh_register_model(uint32_t model,int32_t slot,const char *pak)
{
    uint32_t e,path,r,file,list,i,found=0,total=0,missing=0;
    if(!(e=vh_resource(model)) || !(r=vh_request(path=WORD_AT(e+0x10u))) || !(file=WORD_AT(r+0x10u)) ||
       !MAPMAKER_PTR_OK(file,0x10u)) { vh_log("vehicle model file not read: textures not registered",model,0);return; }
    list=file+WORD_AT(file);
    for(i=0;i<VH_TEX_MAX && MAPMAKER_PTR_OK(list+i*16u,16u) && WORD_AT(list+i*16u)!=0xFFFFFFFFu;i++) {
        uint32_t id=WORD_AT(list+i*16u+4u);           /* word 1 keeps the id after registration */
        total++;
        if(vh_slot_has(slot,id)) found++;
        else if(!missing) missing=id&0xFFFFFFu;
    }
    fprintf(stderr,"[mapmaker] vehicles: model 0x%X textures: %u of %u in %s (first missing %06X)\n",(unsigned)model,
            (unsigned)found,(unsigned)total,pak,(unsigned)missing);
    fflush(stderr);
    if(!(VH_B(r+2u)&0x10u)) return;                    /* registered by the load pass */
    guest_call(sub_00096220,1,path);
    VH_B(r+2u)&=(uint8_t)~0x10u;                       /* registered: no creation-time pass */
    vh_log("registered a vehicle model's textures before the texture queue loads (model, file)",model,file);
}

/* WRAP sub_00062790 (the level's pak mounts, no args): after the original,
 * the stream paks this map's variants need, index only. */
void pc_mapmaker_vehicles_mount(void (*original)(void))
{
    unsigned j;
    if(WORD_AT(VH_MOUNT_SKIP)) { original();return; }
    original();
    memset(&vh_lv,0,sizeof vh_lv);
    if(!vh_play()) return;
    if(!vh_items(WORD_AT(VH_CONTAINER),vh_want,0)) return;
    for(j=0;j<VH_PAKS;j++) {
        uint32_t mounted;
        if(!vh_pak_wanted(j) || !vh_pak(j) || vh_slot(j)!=-1) continue;
        if((mounted=WORD_AT(VH_MOUNTS))+VH_MOUNT_KEEP>VH_MOUNT_MAX) {
            vh_log(j?"pak mount table full: trucks stay theme vehicles (pak)":"pak mount table full: tanks stay theme vehicles (pak)",
                   mounted,j);
            continue;
        }
        if((int32_t)guest_call(sub_000622D0,3,vh.paks[j],0u,1u)<0) {
            fprintf(stderr,"[mapmaker] vehicles: %s did not mount: %s stay theme vehicles\n",vh_pak_path[j],j?"trucks":"tanks");
            fflush(stderr);
        } else {
            fprintf(stderr,"[mapmaker] vehicles: mounted %s for the %s (table %u; tanks %u, trucks %u)\n",vh_pak_path[j],
                    j?"trucks":"tanks",(unsigned)mounted+1u,vh_lv.want[1],vh_lv.want[2]+vh_lv.want[3]);
            fflush(stderr);
        }
    }
}

/* WRAP sub_00063900 (the texture queue's load, no args; only 0x0003D5E5):
 * before it, the variant models' textures are queued. */
void pc_mapmaker_vehicles_textures(void (*original)(void))
{
    unsigned v,i,j;
    if(vh_play()) for(j=0;j<VH_PAKS;j++) {
        int32_t slot;
        uint32_t done[8];
        unsigned nd=0;
        if(vh_lv.textured[j]) continue;
        for(v=1u;v<=VH_KINDS;v++) {
            if(vh_kind[v-1u].pak!=j || !vh_available(v)) continue;
            vh_lv.textured[j]=1;
            slot=vh_slot(j);
            for(i=0;i<vh_kind[v-1u].ready;i++) {
                uint32_t m=vh_kind[v-1u].models[i];
                unsigned d;
                for(d=0;d<nd && done[d]!=m;d++) {}
                if(d<nd || nd>=8u) continue;
                done[nd++]=m;
                vh_register_model(m,slot,vh_pak_path[j]);
            }
        }
    }
    original();
}

/* WRAP sub_0006C090 (cdecl catalogue entry, theme -> model): at the MapMaker
 * preload's request (esi = the item record) the variant's model is requested. */
void pc_mapmaker_vehicles_preload_model(void (*original)(void))
{
    unsigned v;
    if(!MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=VH_PRELOAD_RET) { original();return; }
    original();
    if(!vh_play() || !MAPMAKER_PTR_OK(g_esi,0x18u) || (uint32_t)(int32_t)VH_H(g_esi+0xEu)!=VH_ITEM ||
       !(v=vh_rec_var(g_esi)) || !vh_lv.want[v]) return;
    if(!vh_available(v)) {
        if(!vh_lv.refused[v]++) vh_log("vehicle model or pak not available: the theme vehicle is requested",v,vh_kind[v-1u].model);
        return;
    }
    g_eax=vh_kind[v-1u].model;
}

/* WRAP sub_0033DDA0 (cdecl item, theme -> vehicle def): the preload gets the
 * variant's def and its gun templates; the fill the def and model of a
 * variant whose models were read. Every other call is stock. */
void pc_mapmaker_vehicles_def(void (*original)(void))
{
    uint32_t ret;
    unsigned v,i;
    if(!MAPMAKER_STACK_OK(12) || ((ret=WORD_AT(g_esp))!=VH_DEF_PRELOAD_RET && ret!=VH_DEF_FILL_RET)) { original();return; }
    original();
    if(!vh_play() || !MAPMAKER_PTR_OK(g_esi,0x18u) || (uint32_t)(int32_t)VH_H(g_esi+0xEu)!=VH_ITEM ||
       !(v=vh_rec_var(g_esi)) || !vh_lv.want[v]) return;
    if(ret==VH_DEF_PRELOAD_RET) {
        if(!vh_available(v)) return;
        for(i=0;i<2u;i++) if(vh_kind[v-1u].templates[i]>=0)
            guest_call(sub_000B3B20,1,VH_TEMPLATES+(uint32_t)vh_kind[v-1u].templates[i]*VH_TEMPLATE);
        g_eax=vh_kind[v-1u].def;
        return;
    }
    if(!MAPMAKER_PTR_OK(g_ebp,0x40u)) return;
    if(!vh_ready(v)) {
        if(vh_lv.refused[v]++<4u) vh_log("story vehicle models not read this level: it stays the theme vehicle (variant, def)",v,g_eax);
        return;
    }
    if(vh_lv.nrec>=VH_RECS) { vh_log("too many story vehicles this level: it stays the theme vehicle",vh_lv.nrec,v);return; }
    vh_lv.rec[vh_lv.nrec]=g_ebp;vh_lv.rec_var[vh_lv.nrec]=(uint8_t)v;vh_lv.nrec++;
    WORD_AT(g_ebp+8u)=vh_kind[v-1u].model;
    g_eax=vh_kind[v-1u].def;
    if(vh_lv.placed[v]++<4u) vh_log(v==1u?"vehicle placed as the Machine Wars future tank (record, count)":
                                    v==2u?"vehicle placed as the Scotland the Brave truck with its Vickers gun (record, count)":
                                    "vehicle placed as the Scotland the Brave truck (record, count)",g_ebp,vh_lv.placed[v]);
}

/* WRAP sub_00182A20 (cdecl object, record: the vehicle create): a filled
 * variant record gets its story fields before the builder reads them. */
void pc_mapmaker_vehicles_create(void (*original)(void))
{
    uint32_t rec;
    unsigned k,v=0;
    if(!MAPMAKER_STACK_OK(12) || WORD_AT(g_esp)!=VH_CREATE_RET || !vh_play()) { original();return; }
    rec=WORD_AT(g_esp+8u);
    for(k=0;k<vh_lv.nrec;k++) if(vh_lv.rec[k]==rec) v=vh_lv.rec_var[k];
    if(v && MAPMAKER_PTR_OK(rec,0x40u) && WORD_AT(rec+0x24u)==vh_kind[v-1u].def && WORD_AT(rec+8u)==vh_kind[v-1u].model) {
        WORD_AT(rec+0x28u)=vh_kind[v-1u].r28;WORD_AT(rec+0x2Cu)=vh_kind[v-1u].r2c;
        WORD_AT(rec+0x30u)=vh_kind[v-1u].r30;WORD_AT(rec+0x34u)=vh_kind[v-1u].r34;
        if(vh_lv.created++<8u) vh_log(v==1u?"created a future tank (record, guns 2)":
                                      v==2u?"created a truck (record, gunner seat 1)":"created a truck (record, no gun)",rec,v);
    } else if(v) vh_log("a story vehicle record changed before its create: left as filled (record, variant)",rec,v);
    original();
}

#ifdef TSFP_MAPMAKER_VEHICLES_TEST
static void vh_test_reset(void) { memset(&vh,0,sizeof vh);memset(&vh_lv,0,sizeof vh_lv); }
#endif
