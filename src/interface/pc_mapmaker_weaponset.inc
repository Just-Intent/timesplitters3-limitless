/* US 45410066 MapMaker custom weapon set: 6 -> 20 slots.
 * See docs/mapmaker/mapmaker-weaponset.md for every site and its evidence.
 *
 * Stock: the play-time weapon set is a 0x44 struct (+0 word, +4 count, +8 up
 * to 15 dword ids) returned by sub_0006BD30 (scene 100: rebuilt every call
 * into 0x006F1E28 from the map info's count byte 6 and 6 word ids). Every
 * consumer indexes it by slot: pickups (record +0x10 = catalogue +8 = slot,
 * sub_0006BF10 and the inline copies in sub_0006C620 / sub_0006C920), the
 * Story AI weapon (chunk word +0x26 is a SLOT; sub_0033E6C0 checks it and the
 * spawn reads set[slot] unbounded, 0x0015B885 / 0x002B9D51), the weapon file
 * loader (sub_001C8540 walks count), membership (sub_0006BEC0) and the AI
 * random pick (0x002B50D0, set[rand % count]). So:
 *  - slots 7-20 travel in a fork-only chunk 0x00C776F6 written before the
 *    terminator only when used (an extra slot set, an item or Story AI on
 *    slot 7+); a map without it is byte-identical to stock. Its presence
 *    makes mapmaker_container_over_stock save the 64 KB tier, which a stock
 *    build and System Link refuse (pc_mapmaker_container.inc);
 *  - in play, when the map's custom set is the source, sub_0006BD30 returns a
 *    29-id copy (ids 0..5 stock, 6..19 the extra slots filtered like the
 *    stock ones by sub_00064E70, then any stock-appended ids) with count 20+;
 *    the AI random pick gets the used ids only;
 *  - a weapon or ammo pickup picks its slot with the stock-unused record byte
 *    +0xC (0 = the item's own slot 1-6, 7..20 = an extra slot): the pickup
 *    record +0x10, the play vetoes and the preload follow it;
 *  - editor: the weapon set screen pages (1-6, 7-12, 13-18, 15-20) with
 *    Down/Up at its ends and Right/Left on any row, its title "(1-6 of 20)";
 *    the front end's setup shows the MapMaker set's pages read-only (as
 *    stock shows its slots 1-6); the Story AI weapon choice lists 20 slots; a placed
 *    weapon/ammo pickup's options get a "Weapon" slot choice, kept in node
 *    +0x30 (tagged 'WS'; no stock code writes it) and matched to the saved
 *    records by piece and item position on save and load.
 * Editing needs TSFP_MAPMAKER_LIMITS=1; loading and playing such a map works
 * without it. Empty extra slots are id 0, as stock "None".
 *
 * Extension memory: 0x00906000..0x009064FF (sets 0x00906000, title
 * 0x00906100, choice texts 0x00906140..0x00906410).
 * Requires the includer's WORD_AT, FLOAT_AT, guest_call, g_eax, g_esp,
 * g_xbox_mem_offset and pc_mapmaker_limits.inc. */
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

#define WS_SCENE 0x0079094Cu
#define WS_EDITOR 0x007844A8u
#define WS_PIECES 0x0078400Cu           /* editor piece list: next +0x40, nodes +0x3C, node next +0x14 */
#define WS_CONTAINER 0x00784014u
#define WS_SHADOW 0x00765E38u           /* undo shadow container (sub_003365F0) */
#define WS_EINDEX 0x00784028u           /* editor weapon set index (sub_00312AD0); >= 10 custom */
#define WS_FRONTEND 0x00761404u         /* 1 = played from the front end (sub_002D6E10), 0 = editor test */
#define WS_GAMETYPE 0x0078AD2Cu
#define WS_TYPEBYTE 0x007DE455u         /* byte: 10 = MapMaker story */
#define WS_FILTERBYTE 0x007DE456u       /* byte: sub_00064E70's mode argument */
#define WS_PLAYSET 0x006F1E28u          /* sub_0006BD30 scene-100 set */
#define WS_STOCK 6u
#define WS_SLOTS 20u
#define WS_EXTRA (WS_SLOTS-WS_STOCK)
#define WS_MAGIC 0x00C77667u
#define WS_VERSION 0x12Du
#define WS_INFO 0x00C77668u
#define WS_PIECECHUNK 0x00C77669u
#define WS_SAI 0x00C7766Cu
#define WS_END 0x00C7766Au
#define WS_TAG 0x00C776F6u              /* fork-only; no game code uses it */
#define WS_CHUNK 0x20u                  /* +0 word 1, +2 first 6, +3 count 14, +4 14 word ids */
#define WS_SCAN 0x7FFE0u                /* container body (pc_mapmaker_container.inc) */
#define WS_SAI_WEAPON 0x26u             /* Story AI chunk data word: weapon slot (-1 none) */
#define WS_REC_SLOT 0x0Cu               /* weapon/ammo item record byte: 0 or slot 7..20 */
#define WS_NODE_SLOT 0x30u              /* editor node dword: WS_NODE_TAG | slot */
#define WS_NODE_TAG 0x57530000u
#define WS_HELD 0x2F34u                 /* editor +0x2F34: held item node */
#define WS_SELECTED 0x2F1Cu             /* editor +0x2F1C: selected node (item options menu) */
#define WS_BIGSET 0x00906000u           /* 0x80: +0 word, +4 count, +8 29 ids */
#define WS_COMPACT 0x00906080u          /* 0x80 */
#define WS_TITLE 0x00906100u            /* 0x40 */
#define WS_TEXT 0x00906140u             /* 15 x 0x30 */
#define WS_TEXT_N 0x30u
#define WS_BIGMAX 29u
/* return addresses (call site + length, checked in the decode test) */
#define WS_RANDOM_RET 0x002B50D5u       /* sub_002B4C70: AI random weapon from the set */
#define WS_ROW_RET 0x002D3D8Cu          /* sub_002D3D30: weapon slot row draw, sub_002D3B00(row) */
#define WS_PREVIEW_RET 0x002D410Fu      /* sub_002D40A0: highlighted row's weapon model, sub_002D3B00(value) */
#define WS_NET 0x0076B108u              /* System Link session (sub_003533F0) */
#define WS_LIST_MAP 0xFu                /* front-end set list value: the MapMaker map's set (mode 9 only) */
#define WS_PICK_RET 0x002BF95Au         /* sub_002BF910: weapon picked for row */
#define WS_CLEAR_RET 0x002D41C7u        /* sub_002D40A0: X clears the row */
#define WS_TITLE_RET 0x002D4254u        /* sub_002D4220: title text 0xE86 */
#define WS_SAI_RET 0x0031CDB7u          /* sub_0031CB90: Story AI weapon choice ends */
#define WS_OPTIONS_RET 0x003193A2u      /* sub_00319300: pickup options, after the respawn time item */
#define WS_SAVE_COMPILE_RET 0x0032C6FFu
#define WS_H(a) (*(int16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define WS_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define WS_F(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define WS_PICKUP_ID(id) ((id)>=0x11u && (id)<=0x1Cu)   /* weapon slot 1-6, ammo slot 1-6 items */

extern void sub_00064E70(void),sub_00069640(void),sub_000BE860(void),sub_0006B2D0(void),
    sub_00333700(void),sub_002FDF50(void),sub_00074460(void),sub_0003D7B0(void),sub_00071800(void),
    sub_00079DE0(void),sub_0007D4A0(void),sub_000716D0(void),sub_00078590(void),sub_000783D0(void),
    sub_00303350(void),sub_00071940(void);

static const uint8_t ws_page_first[4]={0u,6u,12u,14u};
static struct {
    int page,title_ok,view;          /* view: 1 editor custom set, 2 front-end map set (read-only) */
    unsigned title_len;
    char title_orig[0x2A];
    int16_t ed[WS_EXTRA];            /* editor: slots 7-20 */
    int16_t fe[WS_EXTRA];            /* front end: the loaded map's slots 7-20 */
    /* play cache (scene 100) */
    int valid,filter_ok;
    uint32_t info;
    int16_t ids[WS_EXTRA];
    uint32_t filtered[WS_EXTRA];
    uint8_t filter_mode;
    unsigned logged;
} ws;

static void ws_log(const char *what,int a,int b)
{
    if(ws.logged>=16u) return;
    ws.logged++;
    fprintf(stderr,"[mapmaker] weaponset: %s (%d, %d)\n",what,a,b);
    fflush(stderr);
}
static int ws_on(void)
{
    mapmaker_limits_read();
    return mapmaker_limits.limits;
}
static uint32_t ws_editor(void)
{
    uint32_t e=WORD_AT(WS_EDITOR);
    return e && MAPMAKER_PTR_OK(e,0x11950u)?e:0u;
}

/* ---- the container ----------------------------------------------------- */
typedef void (*WsChunkFn)(uint32_t data,uint32_t size,void *ctx);
static uint32_t ws_walk(uint32_t c,uint32_t tag,WsChunkFn fn,void *ctx)
{
    uint32_t p,end;
    unsigned steps;
    if(!c || !MAPMAKER_PTR_OK(c,WS_SCAN+8u) || WORD_AT(c)!=WS_MAGIC || WORD_AT(c+4u)!=WS_VERSION) return 0;
    end=c+WS_SCAN;
    for(p=c+8u,steps=0;steps<0x40000u && end-p>=8u;steps++) {
        uint32_t t=WORD_AT(p),size=WORD_AT(p+4u);
        if(t==WS_END) return p;
        if(size>end-p-8u) return 0;
        if(t==tag && fn) fn(p+8u,size,ctx);
        p+=8u+size;
    }
    return 0;
}
static void ws_take(uint32_t data,uint32_t size,void *ctx)
{
    uint32_t *out=(uint32_t *)ctx;
    if(!*out && size>=WS_CHUNK && WS_H(data)==1 && WS_B(data+2u)==WS_STOCK &&
       WS_B(data+3u)>=1u && WS_B(data+3u)<=WS_EXTRA) *out=data;
}
static uint32_t ws_chunk(uint32_t c)
{
    uint32_t data=0;
    ws_walk(c,WS_TAG,ws_take,&data);
    return data;
}
static void ws_take_info(uint32_t data,uint32_t size,void *ctx)
{
    if(size>=0x62u && !*(uint32_t *)ctx) *(uint32_t *)ctx=data;
}
static void ws_sai_high(uint32_t data,uint32_t size,void *ctx)
{
    if(size>WS_SAI_WEAPON+1u && WS_H(data+WS_SAI_WEAPON)>=(int16_t)WS_STOCK) *(int *)ctx=1;
}
static void ws_extras(uint32_t data,int16_t *out)
{
    unsigned k,n=data?WS_B(data+3u):0u;
    for(k=0;k<WS_EXTRA;k++) out[k]=k<n?WS_H(data+4u+2u*k):0;
}

/* Every weapon/ammo item record of the piece chunks: fn(item, piece). */
typedef void (*WsItemFn)(uint32_t item,uint32_t piece,void *ctx);
typedef struct { WsItemFn fn; void *ctx; uint32_t count; int bad; } WsItemWalk;
static void ws_piece_chunk(uint32_t data,uint32_t size,void *ctx)
{
    WsItemWalk *w=(WsItemWalk *)ctx;
    uint32_t p=data,end=data+size,i;
    for(i=0;i<w->count && end-p>=0x28u && i<0x10000u;i++) {
        uint32_t type=WORD_AT(p),pads=guest_call(sub_00333700,1,type)+guest_call(sub_002FDF50,1,type),items,j,piece=p;
        items=(uint16_t)WS_H(p+0x1Eu);
        if(pads>(end-p-0x28u)/8u) { w->bad=1;return; }
        p+=0x28u+pads*8u;
        for(j=0;j<items;j++) {
            uint32_t n;
            if(end-p<0x18u) { w->bad=1;return; }
            n=((uint32_t)WS_B(p+0x14u)*4u+0x1Bu)&~3u;
            if(n>end-p) { w->bad=1;return; }
            if(WS_PICKUP_ID((uint32_t)(int32_t)WS_H(p+0xEu))) w->fn(p,piece,w->ctx);
            p+=n;
        }
    }
}
static int ws_items(uint32_t c,WsItemFn fn,void *ctx)
{
    uint32_t info=0;
    WsItemWalk w;
    ws_walk(c,WS_INFO,ws_take_info,&info);
    if(!info) return 0;
    w.fn=fn;w.ctx=ctx;w.count=WORD_AT(info+4u);w.bad=0;
    if((int32_t)w.count<0) w.count=0;
    ws_walk(c,WS_PIECECHUNK,ws_piece_chunk,&w);
    return !w.bad;
}

/* ---- editor nodes <-> saved records ------------------------------------
 * The record of a node: the piece's cvttss2si x/y/z bytes and byte +0xC
 * (compile 0x00300BBF..0x00300BED), the item id, rotation and cell bytes
 * (0x00300D98..0x00300DB4, 0x00300ED8). Identical keys pair in order. */
#define WS_MAXN 4096u
typedef struct { uint32_t at; uint8_t key[11]; } WsKey;
static WsKey ws_nodes[WS_MAXN],ws_recs[WS_MAXN];
static unsigned ws_nn,ws_nr;
static uint8_t ws_trunc8(float f)
{
    if(!(f>-2147483648.0f && f<2147483648.0f)) return 0;   /* cvttss2si: 0x80000000 */
    return (uint8_t)(int32_t)f;
}
static void ws_collect_nodes(void)
{
    uint32_t piece=WORD_AT(WS_PIECES),i,j;
    ws_nn=0;
    for(i=0;piece && i<0x10000u && MAPMAKER_PTR_OK(piece,0x48u);i++,piece=WORD_AT(piece+0x40u)) {
        uint32_t n=WORD_AT(piece+0x3Cu);
        uint8_t pk[4];
        pk[0]=ws_trunc8(WS_F(piece));pk[1]=ws_trunc8(WS_F(piece+4u));pk[2]=ws_trunc8(WS_F(piece+8u));pk[3]=WS_B(piece+0xCu);
        for(j=0;n && j<0x1000u && MAPMAKER_PTR_OK(n,0x3Cu);j++,n=WORD_AT(n+0x14u)) {
            WsKey *k;
            if(!WS_PICKUP_ID(WORD_AT(n)) || ws_nn>=WS_MAXN) continue;
            k=&ws_nodes[ws_nn++];k->at=n;
            memcpy(k->key,pk,4);
            k->key[4]=WS_B(n);k->key[5]=WS_B(n+8u);k->key[6]=WS_B(n+9u);
            k->key[7]=WS_B(n+0xCu);k->key[8]=WS_B(n+0xEu);k->key[9]=WS_B(n+0x10u);k->key[10]=0;
        }
    }
}
static void ws_collect_rec(uint32_t item,uint32_t piece,void *ctx)
{
    WsKey *k;
    (void)ctx;
    if(ws_nr>=WS_MAXN) return;
    k=&ws_recs[ws_nr++];k->at=item;
    k->key[0]=WS_B(piece+4u);k->key[1]=WS_B(piece+5u);k->key[2]=WS_B(piece+6u);k->key[3]=WS_B(piece+7u);
    k->key[4]=WS_B(item+0xEu);k->key[5]=WS_B(item+0x10u);k->key[6]=WS_B(item+0x11u);
    k->key[7]=WS_B(item+0x15u);k->key[8]=WS_B(item+0x16u);k->key[9]=WS_B(item+0x17u);k->key[10]=0;
}
/* The node paired with record r, or 0. */
static uint32_t ws_pair(unsigned r)
{
    unsigned i,nth=0;
    for(i=0;i<r;i++) if(!memcmp(ws_recs[i].key,ws_recs[r].key,sizeof ws_recs[r].key)) nth++;
    for(i=0;i<ws_nn;i++)
        if(!memcmp(ws_nodes[i].key,ws_recs[r].key,sizeof ws_recs[r].key) && !nth--) return ws_nodes[i].at;
    return 0;
}
static uint32_t ws_node_slot(uint32_t n)
{
    uint32_t v=WORD_AT(n+WS_NODE_SLOT);
    if((v&0xFFFF0000u)!=WS_NODE_TAG || (v&0xFFFFu)<WS_STOCK+1u || (v&0xFFFFu)>WS_SLOTS) return 0;
    return v&0xFFFFu;
}

/* ---- editor set -------------------------------------------------------- */
static int ws_extras_used(void)
{
    unsigned k;
    for(k=0;k<WS_EXTRA;k++) if(ws.ed[k]) return 1;
    return 0;
}

/* WRAP sub_003006C0 (cdecl container, flags; returns used): give the weapon
 * and ammo records their slot byte from the editor nodes, then append the
 * chunk before the terminator when slots 7-20, such an item or a Story AI
 * on slot 7+ are used. The compiler zeroes the whole buffer first. */
void pc_mapmaker_weaponset_compile(void (*original)(void))
{
    uint32_t ret,c,info,used,term,data,nterm;
    unsigned k,slotted=0,unpaired=0;
    int high=0;
    if(!MAPMAKER_STACK_OK(8)) { original();return; }
    ret=WORD_AT(g_esp);c=WORD_AT(g_esp+4u);
    original();
    if(!ws_editor() || !(term=ws_walk(c,WS_SAI,ws_sai_high,&high)) || WORD_AT(c+8u)!=WS_INFO) return;
    ws_collect_nodes();
    ws_nr=0;
    if(ws_nn) ws_items(c,ws_collect_rec,0);
    for(k=0;k<ws_nr;k++) {
        uint32_t n=ws_pair(k),s=n?ws_node_slot(n):0u;
        if(!n) unpaired++;
        if(s) { WS_B(ws_recs[k].at+WS_REC_SLOT)=(uint8_t)s;slotted++; }
    }
    if(unpaired) ws_log("weapon/ammo items not paired with the editor; saved on their own slot",(int)unpaired,(int)ws_nr);
    if(!high && !slotted && !ws_extras_used()) return;
    info=c+0x10u;used=WORD_AT(info+0x248u);
    if(term!=c+used-8u || used+8u+WS_CHUNK+4u>WS_SCAN) {
        for(k=0;k<ws_nr;k++) WS_B(ws_recs[k].at+WS_REC_SLOT)=0;
        ws_log("no room for weapon slots 7-20; not saved",(int)used,(int)(term-c));
        return;
    }
    WORD_AT(term)=WS_TAG;WORD_AT(term+4u)=WS_CHUNK;data=term+8u;
    memset((void *)((uintptr_t)data+g_xbox_mem_offset),0,WS_CHUNK);
    WS_H(data)=1;WS_B(data+2u)=WS_STOCK;WS_B(data+3u)=WS_EXTRA;
    for(k=0;k<WS_EXTRA;k++) WS_H(data+4u+2u*k)=ws.ed[k];
    nterm=data+WS_CHUNK;
    WORD_AT(nterm)=WS_END;WORD_AT(nterm+4u)=nterm-c;
    used=nterm-c+8u;
    WORD_AT(info+0x248u)=used;
    if(g_eax) {
        g_eax=used;
        if(ret==WS_SAVE_COMPILE_RET && !(g_eax&0xFFFFu)) g_eax|=4u;   /* as the container hook */
    }
}

/* WRAP sub_00301A90 (cdecl container, flags): editor load; slots 7-20 from
 * the chunk (empty without it), every weapon/ammo node's slot from its
 * record (the loader's node template is not cleared, so +0x30 is reset). */
void pc_mapmaker_weaponset_load(void (*original)(void))
{
    uint32_t c=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u;
    unsigned k;
    original();
    if(!ws_editor() || !ws_walk(c,0,0,0) || WORD_AT(c+8u)!=WS_INFO) return;
    ws_extras(ws_chunk(c),ws.ed);
    ws_collect_nodes();
    for(k=0;k<ws_nn;k++) WORD_AT(ws_nodes[k].at+WS_NODE_SLOT)=WS_NODE_TAG;
    ws_nr=0;
    if(ws_nn) ws_items(c,ws_collect_rec,0);
    for(k=0;k<ws_nr;k++) {
        uint32_t s=WS_B(ws_recs[k].at+WS_REC_SLOT),n;
        if(s<WS_STOCK+1u || s>WS_SLOTS) continue;
        if((n=ws_pair(k))) WORD_AT(n+WS_NODE_SLOT)=WS_NODE_TAG|s;
    }
}

/* WRAP sub_002FEBF0 (new map). */
void pc_mapmaker_weaponset_reset(void (*original)(void))
{
    original();
    memset(ws.ed,0,sizeof ws.ed);
    ws.page=0;
}

/* WRAP sub_00312880 (returns 1 when the weapon set differs from the undo
 * shadow [0x00765E38]; Map Settings' close sub_00313310 then records an undo
 * step): also compare slots 7-20. */
void pc_mapmaker_weaponset_changed(void (*original)(void))
{
    int16_t old[WS_EXTRA];
    original();
    if(g_eax || !ws_editor()) return;
    ws_extras(ws_chunk(WORD_AT(WS_SHADOW)),old);
    if(memcmp(old,ws.ed,sizeof old)) g_eax=1u;
}

/* WRAP sub_00316D80 (held item defaults): a new weapon/ammo pickup takes its
 * own slot. */
void pc_mapmaker_weaponset_held(void (*original)(void))
{
    uint32_t e;
    original();
    if((e=ws_editor()) && WS_PICKUP_ID(WORD_AT(e+WS_HELD))) WORD_AT(e+WS_HELD+WS_NODE_SLOT)=WS_NODE_TAG;
}

/* ---- weapon set screen paging ------------------------------------------ */
/* The pane 0x005236EC is reached only through screen 0x0052371C (builder
 * sub_002D44D0): from the editor's Map Settings (sub_003128F0, scene 102) or
 * its Test map setup, and from the front end's setup Weapons item
 * (sub_002C7710, scene 101). The in-play pause setup has no Weapons item
 * (sub_002C78F0 skips it when its second argument is 1, 0x002C808A). */
static int ws_custom(void)
{
    return WORD_AT(WS_SCENE)==102u && (int32_t)WORD_AT(WS_EINDEX)>=10;
}
static int ws_fe_scene(void)
{
    return WORD_AT(WS_SCENE)==101u && WORD_AT(WS_GAMETYPE)==9u && WS_B(WS_TYPEBYTE)!=10u && !WORD_AT(WS_NET);
}
/* 1: the editor's custom set (editable), 2: the front end showing the
 * MapMaker map's set (stock: read-only, 0x002D415B / 0x002D406F), else 0. */
static int ws_view(uint32_t pane)
{
    uint32_t item;
    if(ws_custom()) return 1;
    if(!ws_fe_scene()) return 0;
    item=guest_call(sub_00071940,2,pane,0xFFFFFFFBu);           /* the set list item, value -5 */
    return item && MAPMAKER_PTR_OK(item,0xCCu) && WORD_AT(item+0xC8u)==WS_LIST_MAP?2:0;
}
static int ws_view_live(void)
{
    return ws.view==1?ws_custom():ws.view==2 && ws_fe_scene();
}
static void ws_title_suffix(void)
{
    char *t=(char *)((uintptr_t)WS_TITLE+g_xbox_mem_offset);
    unsigned a=ws_page_first[ws.page]+1u,b=a+5u;
    if(!ws.title_ok) return;
    memcpy(t,ws.title_orig,ws.title_len);
    if(ws.view) snprintf(t+ws.title_len,0x40u-ws.title_len,"  (%u-%u of %u)",a,b,WS_SLOTS);
    else t[ws.title_len]=0;
}
/* WRAP sub_002D4220 (cdecl menu): builder of the weapon slot pane 0x005236EC;
 * the screen opens on slots 1-6. */
void pc_mapmaker_weaponset_build(void (*original)(void))
{
    ws.page=0;ws.title_ok=0;ws.view=0;
    original();
}
/* WRAP sub_0003D7B0 (cdecl text id; returns the text): the pane's title
 * (0xE86) gets the page and the slot count, kept in a buffer the update
 * rewrites (INFERRED: the item keeps the pointer). */
void pc_mapmaker_weaponset_title(void (*original)(void))
{
    uint32_t ret=MAPMAKER_STACK_OK(4)?WORD_AT(g_esp):0u;
    original();
    if(ret!=WS_TITLE_RET || !g_eax || !(ws_custom() || ws_fe_scene()) || !ws_on() || !MAPMAKER_PTR_OK(g_eax,1)) return;
    {
        const char *o=(const char *)((uintptr_t)g_eax+g_xbox_mem_offset);
        size_t n=strnlen(o,0x2Au);
        uint32_t p;
        if(n>=0x2Au) return;                      /* too long to extend: stock title */
        memcpy(ws.title_orig,o,n);
        ws.title_len=(unsigned)n;ws.title_ok=1;
        if(ws_custom()) ws.view=1;
        else {                                    /* the set list starts on profile +0x6A4 (sub_002BF8F8) */
            p=guest_call(sub_00069640,0);
            ws.view=MAPMAKER_PTR_OK(p,0x6A8u) && WORD_AT(p+0x6A4u)==WS_LIST_MAP?2:0;
        }
        ws_title_suffix();
        g_eax=WS_TITLE;
    }
}
/* WRAP sub_002D40A0 (cdecl pane): update of the weapon slot pane, called by
 * the pane tick before its own Up/Down handling (0x0007AD76 vs 0x0007AE2E).
 * Its rows have values 0..5 (the set list -5), the highlighted value is
 * +0x18C, input +0xC4. Down on row 6 shows the next six slots, Up on row 1
 * the previous ones (cursor on the near row, sub_00074460 as the pick does);
 * Right / Left on any row turn the page (wrapping; stock moves only choice
 * items with them, 0x0007AFB1). In the front end the MapMaker set's pages
 * 2-4 are read-only as its page 1 is in stock (no X clear there). */
void pc_mapmaker_weaponset_screen(void (*original)(void))
{
    uint32_t pane,in;
    int32_t v;
    int view,step=0;
    if(!MAPMAKER_STACK_OK(8) || !ws_on() || !MAPMAKER_PTR_OK(pane=WORD_AT(g_esp+4u),0x200u) || !(view=ws_view(pane))) {
        ws.page=0;
        if(ws.view) { ws.view=0;ws_title_suffix(); }
        original();return;
    }
    ws.view=view;                                 /* a page is kept only while a view lasts (above) */
    if(view==2) ws_extras(ws_chunk(WORD_AT(WS_CONTAINER)),ws.fe);
    v=(int32_t)WORD_AT(pane+0x18Cu);in=WORD_AT(pane+0xC4u);
    if(v>=0 && v<(int32_t)WS_STOCK) {
        if(v==(int32_t)WS_STOCK-1 && ws.page<3 && (in&0x00020002u)) step=1;
        else if(v==0 && ws.page>0 && (in&0x00010001u)) step=-1;
        else if(in&0x00080008u) step=2;
        else if(in&0x00040004u) step=-2;
    }
    if(step) {
        uint32_t to=step==1?0u:step==-1?WS_STOCK-1u:(uint32_t)v;
        ws.page=(ws.page+(step>0?1:3))%4;
        WORD_AT(pane+0xC4u)&=~0x000F000Fu;WORD_AT(pane+0xC8u)&=~0xFu;
        WORD_AT(pane+0x18Cu)=to;
        guest_call(sub_00074460,3,pane,to,0u);
    }
    if(view==2 && ws.page) WORD_AT(pane+0xC4u)&=~0x00800000u;   /* X clear writes the profile (0x002D41D5) */
    ws_title_suffix();
    original();
}
static int ws_paged(uint32_t ret,uint32_t want)
{
    return ret==want && ws.page && ws_view_live();
}
/* WRAP sub_002D3B00 (cdecl slot; returns the weapon id): the row draw and
 * the highlighted weapon's model show slot row + page start. */
void pc_mapmaker_weaponset_get(void (*original)(void))
{
    uint32_t ret=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp):0u,s;
    if(!ws_paged(ret,WS_ROW_RET) && !ws_paged(ret,WS_PREVIEW_RET)) { original();return; }
    s=WORD_AT(g_esp+4u)+ws_page_first[ws.page];
    if(s<WS_STOCK || s>=WS_SLOTS) { original();return; }
    g_eax=(uint32_t)(int32_t)(ws.view==2?ws.fe:ws.ed)[s-WS_STOCK];
    MAPMAKER_GUEST_RET();
}
/* WRAP sub_00312AE0 (cdecl slot, weapon): editor set store from the pick and
 * the X clear; on pages 2-4 it lands in slots 7-20. */
void pc_mapmaker_weaponset_set(void (*original)(void))
{
    uint32_t ret=MAPMAKER_STACK_OK(12)?WORD_AT(g_esp):0u,s;
    if(ws.view!=1 || (!ws_paged(ret,WS_PICK_RET) && !ws_paged(ret,WS_CLEAR_RET))) { original();return; }
    s=WORD_AT(g_esp+4u)+ws_page_first[ws.page];
    if(s<WS_STOCK || s>=WS_SLOTS) { original();return; }
    ws.ed[s-WS_STOCK]=(int16_t)WORD_AT(g_esp+8u);
    MAPMAKER_GUEST_RET();
}

/* ---- slot choices ------------------------------------------------------- */
static uint32_t ws_weapon_text(uint32_t slot,int32_t id)
{
    uint32_t t=WS_TEXT+(slot-WS_STOCK)*WS_TEXT_N,name;
    const char *s;
    if(id<0 || id>=0x46) id=0;
    name=guest_call(sub_0003D7B0,1,WORD_AT(0x004FB190u+(uint32_t)id*0x54u));   /* as sub_00303350 */
    s=name && MAPMAKER_PTR_OK(name,1)?(const char *)((uintptr_t)name+g_xbox_mem_offset):"";
    snprintf((char *)((uintptr_t)t+g_xbox_mem_offset),WS_TEXT_N,"%u: %.40s",(unsigned)slot+1u,s);
    return t;
}
/* WRAP sub_00078590 (cdecl value pointer): ends a choice item. At 0x0031CDB2
 * it ends the Story AI weapon choice (values -1, 0..5 = slots); add 6..19. */
void pc_mapmaker_weaponset_sai_choice(void (*original)(void))
{
    uint32_t value;
    unsigned k;
    if(!MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=WS_SAI_RET || WORD_AT(WS_SCENE)!=102u || !ws_editor()) { original();return; }
    value=WORD_AT(g_esp+4u);
    if(!ws_on() && !(MAPMAKER_PTR_OK(value,4) && (int32_t)WORD_AT(value)>=(int32_t)WS_STOCK)) { original();return; }
    for(k=WS_STOCK;k<WS_SLOTS;k++) guest_call(sub_00071800,2,ws_weapon_text(k,ws.ed[k-WS_STOCK]),(uint32_t)k);
    original();
}
/* WRAP sub_000783D0 (ends a menu item): in the pickup options menu
 * sub_00319300 (weapon, ammo, health, armour, power-ups) after the respawn
 * time, add a "Weapon" slot choice for weapon/ammo pickups, bound to node
 * +0x30 (built like the Story AI weapon choice 0x0031CD44..0x0031CDB2). */
void pc_mapmaker_weaponset_options(void (*original)(void))
{
    uint32_t e,n,id,cur;
    unsigned k;
    if(!MAPMAKER_STACK_OK(4) || WORD_AT(g_esp)!=WS_OPTIONS_RET || WORD_AT(WS_SCENE)!=102u || !(e=ws_editor())) {
        original();return;
    }
    n=WORD_AT(e+WS_SELECTED);
    original();
    if(!MAPMAKER_PTR_OK(n,0x3Cu) || !WS_PICKUP_ID(id=WORD_AT(n))) return;
    cur=ws_node_slot(n);
    if(!ws_on() && !cur) return;
    WORD_AT(n+WS_NODE_SLOT)=WS_NODE_TAG|cur;                /* a stale value becomes the item's own slot */
    guest_call(sub_00079DE0,5,guest_call(sub_0003D7B0,1,0xD76u),1u,0x08000800u,0u,0u);
    guest_call(sub_0007D4A0,2,0u,0u);
    guest_call(sub_000716D0,1,0xB9u);
    guest_call(sub_00071800,2,guest_call(sub_00303350,1,id),WS_NODE_TAG);
    for(k=WS_STOCK;k<WS_SLOTS;k++)
        guest_call(sub_00071800,2,ws_weapon_text(k,ws.ed[k-WS_STOCK]),WS_NODE_TAG|(k+1u));
    guest_call(sub_00078590,1,n+WS_NODE_SLOT);
    guest_call(sub_000783D0,0);
}

/* ---- play -------------------------------------------------------------- */
/* The map's slots 7-20 in scene 100 (read once per level), or 0. */
static int ws_play(void)
{
    uint32_t c,data;
    if(WORD_AT(WS_SCENE)!=100u) { ws.valid=0;return 0; }
    if(ws.valid) return ws.valid>0;
    c=WORD_AT(WS_CONTAINER);ws.info=0;
    ws_walk(c,WS_INFO,ws_take_info,&ws.info);
    data=ws.info?ws_chunk(c):0u;
    ws_extras(data,ws.ids);
    ws.filter_ok=0;
    ws.valid=data?1:-1;
    return ws.valid>0;
}
/* The map's custom set is what sub_0006BD30 / sub_0033E6C0 read: editor
 * test (sub_002BE7C0: index >= 10), front end (sub_002D3A30 / sub_002BFF90:
 * index 0xF; MapMaker Arcade types take the setup's choice, profile +0x6A4). */
static int ws_active(void)
{
    int32_t idx=WS_H(ws.info+0x40u),t=(int32_t)WORD_AT(WS_GAMETYPE);
    if(!WORD_AT(WS_FRONTEND)) return idx>=10;
    if(WS_B(WS_TYPEBYTE)!=10u && (t==9 || t==0xB || t==0xC || t==0xD)) {
        uint32_t p=guest_call(sub_00069640,0);
        idx=MAPMAKER_PTR_OK(p,0x6A8u)?(int32_t)WORD_AT(p+0x6A4u):-1;
    }
    return idx==0xF;
}
static void ws_filter(void)
{
    uint8_t mode=WS_B(WS_FILTERBYTE);
    unsigned k;
    if(ws.filter_ok && ws.filter_mode==mode) return;
    for(k=0;k<WS_EXTRA;k++) {
        int32_t id=ws.ids[k];
        ws.filtered[k]=id>0 && !guest_call(sub_00064E70,2,(uint32_t)mode,(uint32_t)id)?(uint32_t)id:0u;  /* as 0x0006BE40 */
    }
    ws.filter_mode=mode;ws.filter_ok=1;
}
/* WRAP sub_0006BD30 (no arguments; returns the weapon set). */
void pc_mapmaker_weaponset_play_set(void (*original)(void))
{
    uint32_t ret=MAPMAKER_STACK_OK(4)?WORD_AT(g_esp):0u,n,k,m;
    original();
    if(g_eax!=WS_PLAYSET || !ws_play() || !ws_active()) return;
    ws_filter();
    n=WORD_AT(WS_PLAYSET+4u);
    if(n>15u) n=15u;
    WORD_AT(WS_BIGSET)=WORD_AT(WS_PLAYSET);
    for(k=0;k<WS_STOCK;k++) WORD_AT(WS_BIGSET+8u+4u*k)=k<n?WORD_AT(WS_PLAYSET+8u+4u*k):0u;
    for(k=0;k<WS_EXTRA;k++) WORD_AT(WS_BIGSET+8u+4u*(WS_STOCK+k))=ws.filtered[k];
    m=WS_SLOTS;
    for(k=WS_STOCK;k<n && m<WS_BIGMAX;k++) WORD_AT(WS_BIGSET+8u+4u*m++)=WORD_AT(WS_PLAYSET+8u+4u*k);   /* appended (0x002BE827) */
    WORD_AT(WS_BIGSET+4u)=m;
    g_eax=WS_BIGSET;
    if(ret==WS_RANDOM_RET) {                      /* set[rand % count]: used slots only */
        uint32_t used=0;
        WORD_AT(WS_COMPACT)=WORD_AT(WS_BIGSET);
        for(k=0;k<m;k++) if((int32_t)WORD_AT(WS_BIGSET+8u+4u*k)>0) WORD_AT(WS_COMPACT+8u+4u*used++)=WORD_AT(WS_BIGSET+8u+4u*k);
        WORD_AT(WS_COMPACT+4u)=used;
        if(used) g_eax=WS_COMPACT;
    }
}
/* WRAP sub_0033E6C0 (cdecl slot; returns the set's raw weapon id): the
 * Story AI fill (0x0024205F) and the pickup vetoes ask it. */
void pc_mapmaker_weaponset_slot(void (*original)(void))
{
    int32_t s=MAPMAKER_STACK_OK(8)?(int32_t)WORD_AT(g_esp+4u):-1;
    if(s<(int32_t)WS_STOCK || s>=(int32_t)WS_SLOTS || !ws_play() || !ws_active()) { original();return; }
    g_eax=(uint32_t)(int32_t)ws.ids[s-(int32_t)WS_STOCK];
    MAPMAKER_GUEST_RET();
}
static uint32_t ws_record_slot(uint32_t item,uint32_t first)
{
    uint32_t id,s;
    if(!MAPMAKER_PTR_OK(item,0x18u)) return 0;
    id=(uint32_t)(int32_t)WS_H(item+0xEu);s=WS_B(item+WS_REC_SLOT);
    if(id<first || id>=first+WS_STOCK || s<WS_STOCK+1u || s>WS_SLOTS || !ws_play()) return 0;
    return s;
}
static uint32_t ws_record_weapon(uint32_t s)
{
    return ws_active()?(uint32_t)(int32_t)ws.ids[s-1u-WS_STOCK]:0u;
}
/* WRAP sub_0033E780 (cdecl item; category 8 play veto): a weapon pickup on
 * slot 7+ exists when its weapon has a pickup model (as the original). */
void pc_mapmaker_weaponset_weapon_veto(void (*original)(void))
{
    uint32_t s=MAPMAKER_STACK_OK(8)?ws_record_slot(WORD_AT(g_esp+4u),0x11u):0u,w;
    if(!s) { original();return; }
    w=ws_record_weapon(s);
    g_eax=w && w<0x46u && (int32_t)WORD_AT(0x004FB194u+w*0x54u)>-1;
    MAPMAKER_GUEST_RET();
}
/* WRAP sub_0033E7B0 (cdecl item; category 9 play veto): ammo on slot 7+
 * exists when its weapon has an ammo type (as the original). */
void pc_mapmaker_weaponset_ammo_veto(void (*original)(void))
{
    uint32_t s=MAPMAKER_STACK_OK(8)?ws_record_slot(WORD_AT(g_esp+4u),0x17u):0u,w,t=0;
    if(!s) { original();return; }
    w=ws_record_weapon(s);
    if(w && w<0x46u) {
        uint32_t a=WORD_AT(0x004F9BACu)+WORD_AT(0x004FA138u+w*0x3Cu)*0x29Cu+0x14u;
        t=MAPMAKER_PTR_OK(a,4)?WORD_AT(a):0u;
    }
    g_eax=t!=0u;
    MAPMAKER_GUEST_RET();
}
/* WRAP sub_0033E3B0 (cdecl pickup, item): the fill sub_00240C50 set the
 * pickup's slot word +0x10 from catalogue +8 just before (0x002414C1). */
void pc_mapmaker_weaponset_pickup(void (*original)(void))
{
    uint32_t p=MAPMAKER_STACK_OK(12)?WORD_AT(g_esp+4u):0u,item=p?WORD_AT(g_esp+8u):0u,s;
    original();
    if(!MAPMAKER_PTR_OK(item,0x18u) || !MAPMAKER_PTR_OK(p,0x58u)) return;
    s=ws_record_slot(item,WS_H(item+0xEu)<0x17?0x11u:0x17u);
    if(s) WS_H(p+0x10u)=(int16_t)(s-1u);
}
static void ws_preload_item(uint32_t item,uint32_t piece,void *ctx)
{
    uint32_t id=(uint32_t)(int32_t)WS_H(item+0xEu),s=WS_B(item+WS_REC_SLOT),w,desc;
    (void)piece;
    if(s<WS_STOCK+1u || s>WS_SLOTS || !(w=ws_record_weapon(s))) return;
    desc=guest_call(sub_000BE860,2,id<0x17u?2u:0x40u,w);   /* as sub_0006BF70 for categories 8/9 */
    if(desc) { guest_call(sub_0006B2D0,1,desc);++*(unsigned *)ctx; }
}
/* WRAP sub_0023F390 (no arguments): the MapMaker level preload; refresh the
 * level's slots, then preload the pickups on slots 7-20 (the original
 * preloads their item type's own slot). */
void pc_mapmaker_weaponset_preload(void (*original)(void))
{
    unsigned n=0;
    ws.valid=0;
    original();
    if(ws_play() && ws_active() && !ws_items(WORD_AT(WS_CONTAINER),ws_preload_item,&n))
        ws_log("piece chunk unreadable; pickups on slots 7-20 not preloaded",(int)n,0);
}
