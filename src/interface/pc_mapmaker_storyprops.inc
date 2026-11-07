/* US 45410066 MapMaker story props (area `storyprops`, story assets wave 1,
 * docs/mapmaker/mapmaker-story-assets.md items 2 and 21; every site and its
 * evidence: tests/mapmaker/test_mapmaker_storyprops.py).
 *
 * Two variants of existing MapMaker items, chosen per placed item in its
 * options menu, saved in the item record byte +0xC (stock-unused for these
 * items: the compiler zeroes the buffer and writes +0..+0xA only, 0x00300FFA /
 * 0x003010F6; the editor loader 0x003022D3 and the play fill read +0..+8):
 *  - the explosive drum (item 0x49, category 26 Props) as story mission 1's
 *    power cell (prop 0x53D farfuture2/powercell, init 20 = sub_0021A710 case
 *    12: health 2, damage sub_00218650, kind-1 physics body). At 0 health a
 *    cell whose level record's tag (+0) is not 'CEL1'..'CEL6' explodes with
 *    type 0x2F (10, 12, 25) instead of the drum's 0x0A (10, 6, 25). The fill
 *    builds a real level record for every MapMaker prop (stride 0x60) and
 *    sub_0006C740 stores it at obj+0x2D8, so the damage path's record read is
 *    safe; the record's +0 is the item's logic id + 1 or -1, never a CEL tag.
 *    Model from pak/challenge/l_64_ch.pak (as the mothership port mounts it);
 *  - the fixed guns (items 0x32..0x34, category 29) as story mission 1's
 *    prison fixed gun (prop 0xE6 level29/props/prisonfixedgun, gun type 7, the
 *    intro's records; cat. 29 already gets flags 0x10040, 0x00241A88). Model
 *    from pak/story/l_26_st.pak, index only (its shared files are identical).
 * A stock build plays such a map with the ordinary drum and gun; nothing else
 * in the map changes, so the save stays stock-sized and System Link sends it.
 * Several per map: every one is its own prop / turret with its own record.
 *
 * Editor (scene 102, TSFP_MAPMAKER_LIMITS=1):
 *  - category (sub_002F7770 at 0x003051B5 / 0x003051D7, the Objects-mode
 *    options button): for a selected drum, a copy of category 26's entry
 *    with +8 = a heap descriptor {sub_00319300, sub_003192E0, 0, 0}, so the
 *    stock sub_002FB230 opens an options menu (category 26 has none);
 *  - build (sub_00319300 for that descriptor): the stock frame, one "Type"
 *    choice (Explosive drum / Power cell) bound to node +0x30, and Done;
 *  - turret options (sub_000783D0 at 0x00318A8C, after the fixed gun's weapon
 *    choice in sub_003188C0): a "Model" choice (MapMaker gun / Prison gun);
 *  - node +0x30 holds SP_NODE_TAG | variant (no stock code writes it; the
 *    weaponset area uses it with its own tag for pickups only); compile and
 *    load pair nodes and records by piece and item position (as weaponset).
 * Play (scene 100, switch on, any mode):
 *  - mount (sub_00062790): the paks the map's variants need, kept open;
 *  - preload (sub_0006C090 at 0x0023F5DB): the variant's model is requested
 *    instead of the stock one (sub_00257150 adds resource 0xCFB for 0x53D);
 *  - fill (sub_002F7790 at 0x00241787 prop / 0x002419C4 turret): the record
 *    gets the variant model once it is read (textures registered once per
 *    level, as the mothership: scene 100 skips it at creation, 0x00096556);
 *    else the stock model (logged);
 *  - gun type (sub_0033DD80 at 0x00241AA6): 7 for a swapped turret.
 * Requires WORD_AT, guest_call, g_eax, g_esi, g_esp, g_xbox_mem_offset,
 * xbox_HeapAlloc, pc_mapmaker_limits.inc (and MAPMAKER_* from the areas). */
#include <stdio.h>
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

#define SP_SCENE 0x0079094Cu
#define SP_EDITOR 0x007844A8u
#define SP_PIECES 0x0078400Cu            /* editor pieces: next +0x40, nodes +0x3C, node next +0x14 */
#define SP_CONTAINER 0x00784014u
#define SP_SELECTED 0x2F1Cu              /* editor: the selected node (options menus) */
#define SP_MAGIC 0x00C77667u
#define SP_VERSION 0x12Du
#define SP_INFO 0x00C77668u
#define SP_PIECECHUNK 0x00C77669u
#define SP_END 0x00C7766Au
#define SP_SCAN 0x7FFE0u
#define SP_REC_VAR 0x0Cu                 /* item record byte: 0 stock, 1 story variant */
#define SP_NODE_VAR 0x30u                /* editor node dword: SP_NODE_TAG | variant */
#define SP_NODE_TAG 0x53500000u          /* 'SP' */
#define SP_DRUM 0x49u                    /* Props: explosive drum (labdrumexplosive 0x8B1) */
#define SP_GUN_FIRST 0x32u               /* Fixed guns 0x32..0x34 (category 29) */
#define SP_GUN_LAST 0x34u
#define SP_CELL_MODEL 0x53Du
#define SP_PRISON_MODEL 0xE6u
#define SP_PRISON_GUN_TYPE 7u
#define SP_CAT_RET1 0x003051B5u          /* sub_003050xx: options button, category +8 test */
#define SP_CAT_RET2 0x003051D7u          /* ... and the descriptor pushed to sub_002FB230 */
#define SP_TURRET_OPT_RET 0x00318A8Cu    /* sub_003188C0: after the weapon choice's sub_000783D0 */
#define SP_PRELOAD_RET 0x0023F5DBu       /* sub_0023F390: sub_0006C090(catalogue, theme) -> request */
#define SP_PROP_RET 0x00241787u          /* sub_00240C50 prop case: record +0xC = sub_002F7790(id) */
#define SP_GUN_RET 0x002419C4u           /* turret case: record +0xC = sub_002F7790(id) */
#define SP_GUNTYPE_RET 0x00241AA6u       /* cat. 29 case: record +0x1C = sub_0033DD80(theme, id) */
#define SP_BUILD 0x00319300u
#define SP_TICK 0x003192E0u
#define SP_CLOSE 0x002FB3C0u
#define SP_DONE_TEXT 0xB5Eu
#define SP_POS_BASE 0x0049F7E8u
#define SP_POS_LIST 0x007BB1E8u
#define SP_RESMAP 0x008CA000u            /* int16 per model (moved by the rooms area) */
#define SP_RESMAP_MODELS 0xA00u
#define SP_RES 0x00791D4Cu               /* resource table, 0x30 per entry */
#define SP_MOUNTS 0x006D7880u
#define SP_MOUNT_MAX 64u
#define SP_MOUNT_KEEP 8u
#define SP_MOUNT_SKIP 0x006D7C4Cu        /* sub_00062790 does nothing when set */
#define SP_TEXT_BYTES 32u
#define SP_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define SP_H(a) (*(int16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define SP_F(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

extern void sub_00075B70(void),sub_00071B10(void),sub_00071C50(void),sub_00079DE0(void),sub_0007D4A0(void),
    sub_00071800(void),sub_00078590(void),sub_000783D0(void),sub_00079D60(void),sub_00075E70(void),
    sub_00077320(void),sub_0003D7B0(void),sub_00333700(void),sub_002FDF50(void),
    sub_000622D0(void),sub_00062260(void),sub_000631C0(void),sub_00096330(void);

/* The two variants: kind 0 drum -> power cell, kind 1 fixed gun -> prison gun. */
#define SP_KINDS 2u
static const struct { uint32_t model; const char *pak; const char *what; } sp_kind[SP_KINDS]={
    {SP_CELL_MODEL,"pak/challenge/l_64_ch.pak","power cell"},
    {SP_PRISON_MODEL,"pak/story/l_26_st.pak","prison gun"},
};
static int sp_kind_of(uint32_t id)
{
    if(id==SP_DRUM) return 0;
    if(id>=SP_GUN_FIRST && id<=SP_GUN_LAST) return 1;
    return -1;
}
/* Editor texts: label + two choices per kind (guest copies on the heap). */
static const char sp_texts[6][SP_TEXT_BYTES]={"Type","Explosive drum","Power cell (big explosion)",
                                              "Model","MapMaker gun","Prison gun (mission 1)"};

static struct {
    uint32_t desc, cat, texts, paks[SP_KINDS];
    unsigned logged, failed;
} sp;
/* This level (cleared by every level's pak mount). */
static struct {
    unsigned want[SP_KINDS];             /* variant items in the map */
    int textured[SP_KINDS];
    unsigned swapped[SP_KINDS], refused[SP_KINDS];
    uint32_t gun_item;                   /* the turret item whose model was just swapped */
} sp_lv;

static void sp_log(const char *what,uint32_t a,uint32_t b)
{
    if(sp.logged>=32u) return;
    sp.logged++;
    fprintf(stderr,"[mapmaker] storyprops: %s (0x%X, 0x%X)\n",what,(unsigned)a,(unsigned)b);
    fflush(stderr);
}
static int sp_on(void)
{
    mapmaker_limits_read();
    return mapmaker_limits.limits;
}
static uint32_t sp_editor(void)
{
    uint32_t e;
    if(!sp_on() || WORD_AT(SP_SCENE)!=102u) return 0;
    e=WORD_AT(SP_EDITOR);
    return MAPMAKER_PTR_OK(e,0x11950u)?e:0u;
}
static int sp_play(void) { return sp_on() && WORD_AT(SP_SCENE)==100u; }
static uint32_t sp_node_var(uint32_t n)
{
    uint32_t v=WORD_AT(n+SP_NODE_VAR);
    return (v&0xFFFF0000u)==SP_NODE_TAG && (v&0xFFFFu)==1u?1u:0u;
}
static int sp_alloc(void)
{
    if(!sp.texts) {
        uint32_t d=xbox_HeapAlloc(16u,4u),c=xbox_HeapAlloc(24u,4u),t=xbox_HeapAlloc(sizeof sp_texts,4u);
        if(!d || !c || !t || !MAPMAKER_PTR_OK(d,16u) || !MAPMAKER_PTR_OK(c,24u) || !MAPMAKER_PTR_OK(t,sizeof sp_texts)) {
            if(sp.failed++<4u) sp_log("no memory for the story prop menus",0,0);
            return 0;
        }
        WORD_AT(d)=SP_BUILD;WORD_AT(d+4u)=SP_TICK;WORD_AT(d+8u)=0;WORD_AT(d+12u)=0;
        memcpy((void *)((uintptr_t)t+g_xbox_mem_offset),sp_texts,sizeof sp_texts);
        sp.desc=d;sp.cat=c;sp.texts=t;
    }
    return 1;
}
static uint32_t sp_text(unsigned k) { return sp.texts+k*SP_TEXT_BYTES; }

/* ---- the container ----------------------------------------------------- */
typedef void (*SpItemFn)(uint32_t item,uint32_t piece,void *ctx);
static uint32_t sp_chunk(uint32_t c,uint32_t tag,uint32_t *size)
{
    uint32_t p,end;
    unsigned steps;
    if(!c || !MAPMAKER_PTR_OK(c,SP_SCAN+8u) || WORD_AT(c)!=SP_MAGIC || WORD_AT(c+4u)!=SP_VERSION) return 0;
    end=c+SP_SCAN;
    for(p=c+8u,steps=0;steps<0x40000u && end-p>=8u;steps++) {
        uint32_t t=WORD_AT(p),s=WORD_AT(p+4u);
        if(t==SP_END || s>end-p-8u) return 0;
        if(t==tag) { *size=s;return p+8u; }
        p+=8u+s;
    }
    return 0;
}
/* Every item record of our ids: fn(item, piece). 0 when unreadable. */
static int sp_items(uint32_t c,SpItemFn fn,void *ctx)
{
    uint32_t isz=0,info=sp_chunk(c,SP_INFO,&isz),size=0,data,p,end,count,i,j;
    if(!info || isz<8u || !(data=sp_chunk(c,SP_PIECECHUNK,&size))) return 0;
    count=WORD_AT(info+4u);
    if((int32_t)count<0) count=0;
    for(p=data,end=data+size,i=0;i<count && end-p>=0x28u && i<0x10000u;i++) {
        uint32_t type=WORD_AT(p),pads=guest_call(sub_00333700,1,type)+guest_call(sub_002FDF50,1,type),items,piece=p;
        items=(uint16_t)SP_H(p+0x1Eu);
        if(pads>(end-p-0x28u)/8u) return 0;
        p+=0x28u+pads*8u;
        for(j=0;j<items;j++) {
            uint32_t n;
            if(end-p<0x18u) return 0;
            n=((uint32_t)SP_B(p+0x14u)*4u+0x1Bu)&~3u;
            if(n>end-p) return 0;
            if(sp_kind_of((uint32_t)(int32_t)SP_H(p+0xEu))>=0) fn(p,piece,ctx);
            p+=n;
        }
    }
    return 1;
}

/* ---- editor nodes <-> saved records (as pc_mapmaker_weaponset.inc) -------
 * Key: the piece's cvttss2si x/y/z bytes and byte +0xC (compile
 * 0x00300BBF..0x00300BED), the item id, rotation and cell bytes
 * (0x00300D98..0x00300DB4, 0x00300ED8). Identical keys pair in order. */
#define SP_MAXN 4096u
typedef struct { uint32_t at; uint8_t key[11]; } SpKey;
static SpKey sp_nodes[SP_MAXN],sp_recs[SP_MAXN];
static unsigned sp_nn,sp_nr;
static uint8_t sp_trunc8(float f)
{
    if(!(f>-2147483648.0f && f<2147483648.0f)) return 0;   /* cvttss2si: 0x80000000 */
    return (uint8_t)(int32_t)f;
}
static void sp_collect_nodes(void)
{
    uint32_t piece=WORD_AT(SP_PIECES),i,j;
    sp_nn=0;
    for(i=0;piece && i<0x10000u && MAPMAKER_PTR_OK(piece,0x48u);i++,piece=WORD_AT(piece+0x40u)) {
        uint32_t n=WORD_AT(piece+0x3Cu);
        uint8_t pk[4];
        pk[0]=sp_trunc8(SP_F(piece));pk[1]=sp_trunc8(SP_F(piece+4u));pk[2]=sp_trunc8(SP_F(piece+8u));pk[3]=SP_B(piece+0xCu);
        for(j=0;n && j<0x1000u && MAPMAKER_PTR_OK(n,0x3Cu);j++,n=WORD_AT(n+0x14u)) {
            SpKey *k;
            if(sp_kind_of(WORD_AT(n))<0 || sp_nn>=SP_MAXN) continue;
            k=&sp_nodes[sp_nn++];k->at=n;
            memcpy(k->key,pk,4);
            k->key[4]=SP_B(n);k->key[5]=SP_B(n+8u);k->key[6]=SP_B(n+9u);
            k->key[7]=SP_B(n+0xCu);k->key[8]=SP_B(n+0xEu);k->key[9]=SP_B(n+0x10u);k->key[10]=0;
        }
    }
}
static void sp_collect_rec(uint32_t item,uint32_t piece,void *ctx)
{
    SpKey *k;
    (void)ctx;
    if(sp_nr>=SP_MAXN) return;
    k=&sp_recs[sp_nr++];k->at=item;
    k->key[0]=SP_B(piece+4u);k->key[1]=SP_B(piece+5u);k->key[2]=SP_B(piece+6u);k->key[3]=SP_B(piece+7u);
    k->key[4]=SP_B(item+0xEu);k->key[5]=SP_B(item+0x10u);k->key[6]=SP_B(item+0x11u);
    k->key[7]=SP_B(item+0x15u);k->key[8]=SP_B(item+0x16u);k->key[9]=SP_B(item+0x17u);k->key[10]=0;
}
static uint32_t sp_pair(unsigned r)
{
    unsigned i,nth=0;
    for(i=0;i<r;i++) if(!memcmp(sp_recs[i].key,sp_recs[r].key,sizeof sp_recs[r].key)) nth++;
    for(i=0;i<sp_nn;i++)
        if(!memcmp(sp_nodes[i].key,sp_recs[r].key,sizeof sp_recs[r].key) && !nth--) return sp_nodes[i].at;
    return 0;
}

/* WRAP sub_003006C0 (cdecl container, flags; returns used): the records of
 * variant items get byte +0xC = 1 (the compiler zeroes the buffer first). */
void pc_mapmaker_storyprops_compile(void (*original)(void))
{
    uint32_t c;
    unsigned k,set=0,unpaired=0;
    if(!MAPMAKER_STACK_OK(8)) { original();return; }
    c=WORD_AT(g_esp+4u);
    original();
    if(!sp_editor()) return;
    sp_collect_nodes();
    sp_nr=0;
    if(!sp_nn || !sp_items(c,sp_collect_rec,0)) return;
    for(k=0;k<sp_nr;k++) {
        uint32_t n=sp_pair(k),v=n?sp_node_var(n):0u;
        if(!n) unpaired++;
        SP_B(sp_recs[k].at+SP_REC_VAR)=(uint8_t)v;
        set+=v;
    }
    if(unpaired) sp_log("drum/fixed gun items not paired with the editor; saved as stock",unpaired,sp_nr);
    if(set) sp_log("saved story prop variants (items, records)",set,sp_nr);
}

/* WRAP sub_00301A90 (cdecl container, flags): editor load. The loader's
 * node template is not cleared, so every drum/fixed gun node is reset first. */
void pc_mapmaker_storyprops_load(void (*original)(void))
{
    uint32_t c=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u;
    unsigned k;
    original();
    if(!sp_editor()) return;
    sp_collect_nodes();
    for(k=0;k<sp_nn;k++) WORD_AT(sp_nodes[k].at+SP_NODE_VAR)=SP_NODE_TAG;
    sp_nr=0;
    if(!sp_nn || !sp_items(c,sp_collect_rec,0)) return;
    for(k=0;k<sp_nr;k++) {
        uint32_t n;
        if(SP_B(sp_recs[k].at+SP_REC_VAR)!=1u) continue;
        if((n=sp_pair(k))) WORD_AT(n+SP_NODE_VAR)=SP_NODE_TAG|1u;
    }
}

/* WRAP sub_002F7770 (cdecl item id -> category entry 0x00524BE0 + cat*24):
 * the Objects-mode options button of a selected drum gets an entry whose +8
 * is our descriptor; every other caller and item the stock entry. */
void pc_mapmaker_storyprops_category(void (*original)(void))
{
    uint32_t ret,e,n;
    if(!MAPMAKER_STACK_OK(8) || ((ret=WORD_AT(g_esp))!=SP_CAT_RET1 && ret!=SP_CAT_RET2) || WORD_AT(g_esp+4u)!=SP_DRUM) {
        original();return;
    }
    original();
    if(!(e=sp_editor()) || !MAPMAKER_PTR_OK(g_eax,24u) || WORD_AT(g_eax+8u) || !(n=WORD_AT(e+SP_SELECTED)) ||
       !MAPMAKER_PTR_OK(n,0x3Cu) || WORD_AT(n)!=SP_DRUM || !sp_alloc()) return;
    memcpy((void *)((uintptr_t)sp.cat+g_xbox_mem_offset),(const void *)((uintptr_t)g_eax+g_xbox_mem_offset),24u);
    WORD_AT(sp.cat+8u)=sp.desc;
    g_eax=sp.cat;
}

/* WRAP sub_00319300 (options menu build, cdecl menu): for our descriptor the
 * stock frame (as the pickup options build), a "Type" choice bound to the
 * selected drum's node +0x30, and Done. */
void pc_mapmaker_storyprops_build(void (*original)(void))
{
    uint32_t menu=MAPMAKER_STACK_OK(8u)?WORD_AT(g_esp+4u):0u,e,n,y;
    float f;
    if(!sp.desc || !MAPMAKER_PTR_OK(menu,0x220u) || WORD_AT(menu+4u)!=sp.desc) { original();return; }
    e=sp_editor();n=e?WORD_AT(e+SP_SELECTED):0u;
    guest_call(sub_00075B70,5,0x8400u,0u,0x42480000u,0u,SP_CLOSE);
    WORD_AT(menu+0x218u)=1u;
    guest_call(sub_00071B10,4,1u,0x3F000000u,1u,1u);
    guest_call(sub_00071C50,1,0x20000u);
    if(MAPMAKER_PTR_OK(n,0x3Cu) && WORD_AT(n)==SP_DRUM) {
        WORD_AT(n+SP_NODE_VAR)=SP_NODE_TAG|sp_node_var(n);       /* a stale value becomes the drum */
        guest_call(sub_00079DE0,5,sp_text(0),1u,0x08000800u,0u,0u);
        guest_call(sub_0007D4A0,2,0u,0u);
        guest_call(sub_00071800,2,sp_text(1),SP_NODE_TAG);
        guest_call(sub_00071800,2,sp_text(2),SP_NODE_TAG|1u);
        guest_call(sub_00078590,1,n+SP_NODE_VAR);
        guest_call(sub_000783D0,0);
    } else sp_log("drum options opened without a selected drum",n,0);
    guest_call(sub_00079D60,6,guest_call(sub_0003D7B0,1,SP_DONE_TEXT),0xFFFFFFFFu,0u,0x40000000u,SP_CLOSE,0u);
    guest_call(sub_00075E70,0);
    f=SP_F(SP_POS_BASE)-SP_F(menu+0x68u);
    memcpy(&y,&f,4);
    guest_call(sub_00077320,2,WORD_AT(SP_POS_LIST),y);
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_000783D0 (ends a menu item): in the fixed gun options
 * sub_003188C0, after its weapon choice, add a "Model" choice bound to node
 * +0x30 (built like the weaponset area's pickup Weapon choice). */
void pc_mapmaker_storyprops_turret_options(void (*original)(void))
{
    uint32_t e,n;
    if(!MAPMAKER_STACK_OK(4) || WORD_AT(g_esp)!=SP_TURRET_OPT_RET || !(e=sp_editor())) { original();return; }
    n=WORD_AT(e+SP_SELECTED);
    original();
    if(!MAPMAKER_PTR_OK(n,0x3Cu) || sp_kind_of(WORD_AT(n))!=1 || !sp_alloc()) return;
    WORD_AT(n+SP_NODE_VAR)=SP_NODE_TAG|sp_node_var(n);
    guest_call(sub_00079DE0,5,sp_text(3),1u,0x08000800u,0u,0u);
    guest_call(sub_0007D4A0,2,0u,0u);
    guest_call(sub_00071800,2,sp_text(4),SP_NODE_TAG);
    guest_call(sub_00071800,2,sp_text(5),SP_NODE_TAG|1u);
    guest_call(sub_00078590,1,n+SP_NODE_VAR);
    guest_call(sub_000783D0,0);
}

/* ---- play -------------------------------------------------------------- */
static void sp_want(uint32_t item,uint32_t piece,void *ctx)
{
    int k=sp_kind_of((uint32_t)(int32_t)SP_H(item+0xEu));
    (void)piece;(void)ctx;
    if(k>=0 && SP_B(item+SP_REC_VAR)==1u) sp_lv.want[k]++;
}
static uint32_t sp_pak(unsigned k)
{
    if(!sp.paks[k]) {
        size_t n=strlen(sp_kind[k].pak)+1u;
        uint32_t p=xbox_HeapAlloc((uint32_t)n,4u);
        if(!p || !MAPMAKER_PTR_OK(p,n)) { sp_log("no memory for a pak path",k,0);return 0; }
        memcpy((void *)((uintptr_t)p+g_xbox_mem_offset),sp_kind[k].pak,n);
        sp.paks[k]=p;
    }
    return sp.paks[k];
}
static int sp_mounted(unsigned k) { return sp.paks[k] && (int32_t)guest_call(sub_00062260,1,sp.paks[k])!=-1; }
static uint32_t sp_resource(uint32_t model)
{
    int16_t i;
    uint32_t t=WORD_AT(SP_RES),e;
    if(model>=SP_RESMAP_MODELS) return 0;
    i=SP_H(SP_RESMAP+model*2u);
    if(i<0 || !t) return 0;
    e=t+(uint32_t)i*0x30u;
    return MAPMAKER_PTR_OK(e,0x30u)?e:0;
}
static uint32_t sp_request(uint32_t path)
{
    uint32_t r;
    if(!path || !MAPMAKER_PTR_OK(path,4u)) return 0;
    r=guest_call(sub_000631C0,1,path);
    return r && MAPMAKER_PTR_OK(r,0x14u)?r:0u;
}
/* The variant model of kind k is in this level: its pak mounted and its
 * file read (resource +4 loaded, or the load pass's request +0x10). */
static int sp_ready(unsigned k)
{
    uint32_t e=sp_resource(sp_kind[k].model),r;
    if(!sp_lv.want[k] || !sp_mounted(k) || !e) return 0;
    if(WORD_AT(e+4u)) return 1;
    r=sp_request(WORD_AT(e+0x10u));
    return r && WORD_AT(r+0x10u)!=0;
}
/* Once per level: the textures scene 100 leaves unregistered at creation. */
static void sp_textures(unsigned k)
{
    uint32_t e,r,file;
    if(sp_lv.textured[k]) return;
    sp_lv.textured[k]=1;
    if(!(e=sp_resource(sp_kind[k].model)) || !(r=sp_request(WORD_AT(e+0x10u))) || !(file=WORD_AT(r+0x10u)) ||
       !MAPMAKER_PTR_OK(file,0x10u)) { sp_log("variant model file not read: textures not registered",sp_kind[k].model,0);return; }
    if(!(SP_B(r+2u)&0x10u)) { sp_log("variant model textures registered by the load pass",sp_kind[k].model,SP_B(r+2u));return; }
    guest_call(sub_00096330,2,file,1u);
    sp_log("registered the variant model's textures (scene 100 skips it at creation)",sp_kind[k].model,file);
}

/* WRAP sub_00062790 (the level's pak mounts, no args): after the original,
 * the paks this map's variants need, kept open (index only). */
void pc_mapmaker_storyprops_mount(void (*original)(void))
{
    unsigned k;
    if(WORD_AT(SP_MOUNT_SKIP)) { original();return; }
    original();
    memset(&sp_lv,0,sizeof sp_lv);
    if(!sp_play()) return;
    if(!sp_items(WORD_AT(SP_CONTAINER),sp_want,0)) return;
    for(k=0;k<SP_KINDS;k++) {
        uint32_t mounted;
        if(!sp_lv.want[k] || !sp_pak(k) || sp_mounted(k)) continue;
        if((mounted=WORD_AT(SP_MOUNTS))+SP_MOUNT_KEEP>SP_MOUNT_MAX) {
            sp_log(k?"pak mount table full: prison guns stay MapMaker guns":"pak mount table full: power cells stay drums",mounted,SP_MOUNT_MAX);
            continue;
        }
        if((int32_t)guest_call(sub_000622D0,3,sp.paks[k],0u,1u)<0) sp_log(k?"l_26_st.pak did not mount: prison guns stay MapMaker guns":
                                                                          "l_64_ch.pak did not mount: power cells stay drums",mounted,0);
        else sp_log(k?"mounted l_26_st.pak for the prison guns (table, guns)":"mounted l_64_ch.pak for the power cells (table, cells)",
                    mounted+1u,sp_lv.want[k]);
    }
}

/* WRAP sub_0006C090 (cdecl catalogue entry, theme -> model): at the MapMaker
 * preload's request (esi = the item record) the variant model is requested. */
void pc_mapmaker_storyprops_preload_model(void (*original)(void))
{
    int k;
    if(!MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=SP_PRELOAD_RET) { original();return; }
    original();
    if(!sp_play() || !MAPMAKER_PTR_OK(g_esi,0x18u) || (k=sp_kind_of((uint32_t)(int32_t)SP_H(g_esi+0xEu)))<0 ||
       SP_B(g_esi+SP_REC_VAR)!=1u || !sp_lv.want[k]) return;
    if(!sp_mounted((unsigned)k) || !sp_resource(sp_kind[k].model)) {
        if(!sp_lv.refused[k]++) sp_log(k?"prison gun model not available: MapMaker gun requested":
                                         "power cell model not available: drum requested",sp_kind[k].model,0);
        return;
    }
    g_eax=sp_kind[k].model;
}

/* WRAP sub_002F7790 (cdecl item id -> model): in the play fill's prop and
 * turret cases (esi = the item record) a variant record gets its model. */
void pc_mapmaker_storyprops_model(void (*original)(void))
{
    uint32_t ret;
    int k;
    if(!MAPMAKER_STACK_OK(8) || ((ret=WORD_AT(g_esp))!=SP_PROP_RET && ret!=SP_GUN_RET)) { original();return; }
    original();
    if(ret==SP_GUN_RET) sp_lv.gun_item=0;
    if(!sp_play() || !MAPMAKER_PTR_OK(g_esi,0x18u) || (k=sp_kind_of((uint32_t)(int32_t)SP_H(g_esi+0xEu)))<0 ||
       (ret==SP_PROP_RET)!=(k==0) || SP_B(g_esi+SP_REC_VAR)!=1u) return;
    if(!sp_ready((unsigned)k)) {
        if(sp_lv.refused[k]++<4u) sp_log(k?"prison gun model not read this level: it stays a MapMaker gun":
                                           "power cell model not read this level: it stays a drum",sp_kind[k].model,g_eax);
        return;
    }
    sp_textures((unsigned)k);
    g_eax=sp_kind[k].model;
    if(k==1) sp_lv.gun_item=g_esi;
    if(sp_lv.swapped[k]++<4u) sp_log(k?"fixed gun placed as the mission 1 prison gun":"drum placed as the mission 1 power cell",
                                     g_esi,sp_lv.swapped[k]);
}

/* WRAP sub_0033DD80 (cdecl theme, item -> gun type): the swapped prison
 * gun's record +0x1C gets the intro's gun type 7. */
void pc_mapmaker_storyprops_gun_type(void (*original)(void))
{
    if(!MAPMAKER_STACK_OK(12) || WORD_AT(g_esp)!=SP_GUNTYPE_RET) { original();return; }
    original();
    if(sp_lv.gun_item && sp_lv.gun_item==g_esi && sp_play()) g_eax=SP_PRISON_GUN_TYPE;
    sp_lv.gun_item=0;
}

#ifdef TSFP_MAPMAKER_STORYPROPS_TEST
static void sp_test_reset(void) { memset(&sp,0,sizeof sp);memset(&sp_lv,0,sizeof sp_lv); }
#endif
