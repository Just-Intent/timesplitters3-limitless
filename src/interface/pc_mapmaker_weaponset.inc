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
 *  - editor: the weapon set screen's slot pane is built with 20 slot rows in
 *    the engine's own scrolling region (sub_000780E0 .. sub_000781A0, as the
 *    weapon list beside it), six rows tall, so the stock cursor, the pointer,
 *    the scrollbar and the mouse wheel reach slots 7-20; rows 7-20 edit the
 *    slots through the get/set hooks. The front end's setup shows the
 *    MapMaker set's 20 rows read-only (as stock shows its slots 1-6); the
 *    Story AI weapon choice lists 20 slots; a placed
 *    weapon/ammo pickup's options get a "Weapon" slot choice, kept in node
 *    +0x30 (tagged 'WS'; no stock code writes it) and matched to the saved
 *    records by piece and item position on save and load.
 * Editing needs TSFP_MAPMAKER_LIMITS=1; loading and playing such a map works
 * without it. Empty extra slots are id 0, as stock "None".
 *
 * Extension memory: 0x00906000..0x009064FF (sets 0x00906000, choice texts
 * 0x00906140..0x00906410; 0x00906100..0x0090613F, the retired paged title,
 * is unused).
 * Requires the includer's WORD_AT, FLOAT_AT, guest_call, g_eax, g_ebx, g_esp,
 * g_xbox_mem_offset and pc_mapmaker_limits.inc. */
#include <stdarg.h>
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
#define WS_LABEL_RET 0x002D3DF6u        /* sub_002D3D30: sub_00064E70(filter, weapon) after the row label */
#define WS_BUILDING 0x007BB1E8u         /* the menu being built */
#define WS_ROW_ACTION 0x002D4040u       /* slot row callbacks (0x002D4295 / 0x002D4290) */
#define WS_ROW_DRAW 0x002D3D30u
#define WS_ROW_HEIGHT 4.6f              /* the rows' height argument (0x002D429A) */
#define WS_VIEW_ROWS 6u                 /* the list is as tall as the stock six rows */
#define WS_POINTER_MODE 0x007BA270u     /* + player * 0x1E0: editor pointer menus (sub_00071010) */
#define WS_PLAYER 0x007BA9E0u           /* menu player index */
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
    sub_00303350(void),sub_00075B70(void),sub_00079D60(void),sub_002BF830(void),
    sub_00079710(void),sub_000780E0(void),sub_000781A0(void),sub_000772E0(void),sub_00077320(void),
    sub_00074A00(void),sub_00077110(void),sub_00075E70(void);

static struct {
    unsigned rows;                   /* slot rows of the last pane built (6 stock, 20) */
    uint32_t menu;                   /* that pane */
    int opened,last_slot;            /* diagnostics: first update seen, highlighted slot */
    float last_scroll;
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
/* Screen diagnostics (own budget): the pane build, its first update and
 * moves of the highlight / scroll, so a run shows whether the hooks act. */
static unsigned ws_diag_n,ws_diag_moves,ws_diag_opens;
/* moves: 0 = a build or first-update line (one each per screen open, own
 * budget of 256), 1 = a highlight/scroll move (48 in total). */
static void ws_diag(int moves,const char *format,...)
{
    va_list ap;
    if(moves ? ws_diag_moves>=48u : ws_diag_opens>=256u) return;
    ws_diag_n++;
    if(moves) ws_diag_moves++; else ws_diag_opens++;
    fputs("[mapmaker] weaponset: ",stderr);
    va_start(ap,format);vfprintf(stderr,format,ap);va_end(ap);
    fputc('\n',stderr);
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

/* ---- weapon set screen: a 20-row scrolling slot list --------------------- */
/* The pane 0x005236EC is reached only through screen 0x0052371C (builder
 * sub_002D44D0): from the editor's Map Settings (sub_003128F0, scene 102) or
 * its Test map setup, and from the front end's setup Weapons item
 * (sub_002C7710, scene 101). The in-play pause setup has no Weapons item
 * (sub_002C78F0 skips it when its second argument is 1, 0x002C808A).
 *
 * Why the earlier hidden paging never showed in game: the editor turns its
 * menus into pointer menus (sub_00071010 from 0x00302FE9 sets
 * [0x007BA270 + player*0x1E0]); their tick (sub_0007A7B0) then picks the row
 * under the pointer in sub_000777E0 and jumps over the stock Up/Down handling
 * (0x0007ADEC -> 0x0007AFF6), and the PC mouse (wheel steps, hovered row)
 * reaches the pane in sub_00073750 at 0x0007ADC5, after the update at
 * 0x0007AD76 where the paging looked for "Down on row 6". The six rows also
 * fit their view, so there was nothing for the scrollbar or the wheel
 * (pc_mapmaker_pointer.inc needs content > view) to scroll. */
static int ws_custom(void)
{
    return WORD_AT(WS_SCENE)==102u && (int32_t)WORD_AT(WS_EINDEX)>=10;
}
static int ws_fe_scene(void)
{
    return WORD_AT(WS_SCENE)==101u && WORD_AT(WS_GAMETYPE)==9u && WS_B(WS_TYPEBYTE)!=10u && !WORD_AT(WS_NET);
}
/* The set the front end's rows show: profile +0x6A4 (sub_002BFF30, scene 101). */
static int ws_fe_map_set(void)
{
    uint32_t p=guest_call(sub_00069640,0);
    return MAPMAKER_PTR_OK(p,0x6A8u) && WORD_AT(p+0x6A4u)==WS_LIST_MAP;
}
/* 20 rows: the editor (any set; rows 7-20 of a preset are empty, as stock
 * slots past a set's count) and the front end's MapMaker Arcade setup. */
static int ws_list_wanted(void)
{
    return ws_on() && (WORD_AT(WS_SCENE)==102u || ws_fe_scene());
}
static uint32_t ws_float(float f) { uint32_t u;memcpy(&u,&f,4);return u; }
static unsigned ws_pointer_mode(void)
{
    uint32_t p=WORD_AT(WS_PLAYER);
    return p<4u?WORD_AT(WS_POINTER_MODE+p*0x1E0u):0u;
}
/* WRAP sub_002D4220 (cdecl menu): builder of the weapon slot pane 0x005236EC.
 * Stock adds the title (0xE86), the set list (value -5), six slot rows
 * (sub_00079D60(0, row, 0, 4.6f, sub_002D4040, sub_002D3D30), values 0..5)
 * and the footer. With the list wanted this replays the same calls, but the
 * rows 0..19 go into a scrolling region six rows tall (sub_000780E0(height,
 * 0) .. sub_000781A0(), as the weapon list builder sub_002BFA70 does), which
 * gives each row the scrolling flag 0x40 (sub_00078FF0 0x0007914A). The row
 * index stays value + 2 (the pick and X clear store slot index - 2). */
void pc_mapmaker_weaponset_build(void (*original)(void))
{
    uint32_t ret=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp):0u,menu=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u,built,v;
    int scene=(int)WORD_AT(WS_SCENE),set=scene==102?(int)WORD_AT(WS_EINDEX):-1;
    ws.opened=0;ws.last_slot=-1;ws.last_scroll=-1.f;ws.menu=menu;
    if(!ws_list_wanted() || !MAPMAKER_PTR_OK(menu,0x200u)) {
        ws.rows=WS_STOCK;
        ws_diag(0,"slot pane build (stock 6 rows: %s) ret=0x%08X menu=0x%08X descriptor=0x%08X scene=%d set=%d",
            !ws_on()?"TSFP_MAPMAKER_LIMITS is not 1":"not the editor or MapMaker Arcade",ret,menu,
            MAPMAKER_PTR_OK(menu,8u)?WORD_AT(menu+4u):0u,scene,set);
        original();return;
    }
    if(scene!=102) ws_extras(ws_chunk(WORD_AT(WS_CONTAINER)),ws.fe);   /* the loaded map's slots 7-20 */
    guest_call(sub_00075B70,5,0x00020010u,0u,ws_float(40.f),0u,0x002C1450u);
    guest_call(sub_00079D60,6,guest_call(sub_0003D7B0,1,0xE86u),0xFFFFFFFFu,0x00030404u,0u,0u,0u);
    guest_call(sub_00079D60,6,0x004766D8u,0xFFFFFFFBu,0x08030C00u,0u,0u,0u);
    guest_call(sub_002BF830,1,menu);
    guest_call(sub_00079710,1,0u);
    guest_call(sub_000780E0,2,ws_float(WS_ROW_HEIGHT*(float)WS_VIEW_ROWS),0u);
    for(v=0;v<WS_SLOTS;v++) guest_call(sub_00079D60,6,0u,v,0u,ws_float(WS_ROW_HEIGHT),WS_ROW_ACTION,WS_ROW_DRAW);
    guest_call(sub_000781A0,0);
    built=WORD_AT(WS_BUILDING);
    guest_call(sub_000772E0,2,built,ws_float(5.f));
    guest_call(sub_00077320,2,WORD_AT(WS_BUILDING),ws_float(scene==102?7.5f:5.5f));
    guest_call(sub_00074A00,4,0u,guest_call(sub_0003D7B0,1,0xE88u),1u,0u);
    guest_call(sub_00074A00,4,3u,guest_call(sub_0003D7B0,1,0xE89u),1u,0u);
    guest_call(sub_00074A00,4,1u,guest_call(sub_0003D7B0,1,0x1A27u),0u,0u);
    guest_call(sub_00077110,2,menu,0xFFFFFFFBu);
    guest_call(sub_00075E70,0);
    ws.rows=WS_SLOTS;
    ws_diag(0,"slot pane build (20 rows, scrolling) ret=0x%08X menu=0x%08X descriptor=0x%08X scene=%d set=%d pointer=%u",
        ret,menu,WORD_AT(menu+4u),scene,set,ws_pointer_mode());
    MAPMAKER_GUEST_RET();
}
/* WRAP sub_0003D7B0: retired (the paged title). Kept as a pass-through so the
 * existing wrapper chain links; the integrator may drop its WRAP line. */
void pc_mapmaker_weaponset_title(void (*original)(void))
{
    original();
}
/* WRAP sub_002D40A0 (cdecl pane): update of the weapon slot pane, called by
 * the pane tick before its own row handling (0x0007AD76). Outside the editor
 * the X clear writes the profile set at row index - 2 (0x002D41D5), which is
 * only six slots: suppressed on rows 7-20 (read-only there, as stock's
 * MapMaker set). Logs the first update and moves of the highlight/scroll. */
void pc_mapmaker_weaponset_screen(void (*original)(void))
{
    uint32_t pane;
    int slot;
    float scroll;
    if(!MAPMAKER_STACK_OK(8) || ws.rows!=WS_SLOTS || !MAPMAKER_PTR_OK(pane=WORD_AT(g_esp+4u),0x200u)) {
        if(!ws.opened && MAPMAKER_STACK_OK(8)) {
            ws.opened=1;
            ws_diag(0,"slot pane update (%u rows, stock) pane=0x%08X",ws.rows,WORD_AT(g_esp+4u));
        }
        original();return;
    }
    slot=(int)WORD_AT(pane+0x184u)-2;
    if(WORD_AT(WS_SCENE)!=102u && slot>=(int)WS_STOCK) WORD_AT(pane+0xC4u)&=~0x00800000u;
    scroll=FLOAT_AT(pane+0x88u);
    if(!ws.opened) {
        ws.opened=1;
        ws_diag(0,"slot pane open: pane=0x%08X built=0x%08X items=%u content=%.1f view=%.1f..%.1f scroll=%.1f slot=%d pointer=%u",
            pane,ws.menu,WORD_AT(pane+0x180u),(double)FLOAT_AT(pane+0x5Cu),(double)FLOAT_AT(pane+0x6Cu),
            (double)FLOAT_AT(pane+0x70u),(double)scroll,slot+1,ws_pointer_mode());
    } else if(slot!=ws.last_slot || scroll!=ws.last_scroll) {
        if(slot>=(int)WS_STOCK || scroll!=ws.last_scroll)
            ws_diag(1,"list: slot %d highlighted, scroll %.1f of %.1f",slot+1,(double)scroll,
                (double)(FLOAT_AT(pane+0x5Cu)-(FLOAT_AT(pane+0x70u)-FLOAT_AT(pane+0x6Cu))));
    }
    ws.last_slot=slot;ws.last_scroll=scroll;
    original();
}
/* WRAP sub_002D4040 (cdecl pane): a slot row's action (opens the weapon list
 * when the set is editable). Outside the editor rows 7-20 are read-only: the
 * front end's pick stores into the profile set at row index - 2 (0x002BF982),
 * which has six slots. */
void pc_mapmaker_weaponset_row(void (*original)(void))
{
    uint32_t pane=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u,item,v;
    if(WORD_AT(WS_SCENE)!=102u && MAPMAKER_PTR_OK(pane,0x200u) && (item=WORD_AT(pane+0x198u))!=0 &&
       MAPMAKER_PTR_OK(item,0x18u) && (v=WORD_AT(item+0x14u))>=WS_STOCK && v<WS_SLOTS) {
        MAPMAKER_GUEST_RET();
        return;
    }
    original();
}
/* WRAP sub_002D3B00 (cdecl slot; returns the weapon id): the row draw
 * (0x002D3D8C) and the highlighted weapon's model (0x002D410F) ask for rows
 * 7-20 by their value 6..19. Editor custom set: slots 7-20; front end with
 * the MapMaker set: the map's (read-only); anything else: the original
 * (0 for a row past 6). */
void pc_mapmaker_weaponset_get(void (*original)(void))
{
    uint32_t ret=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp):0u,s;
    if((ret!=WS_ROW_RET && ret!=WS_PREVIEW_RET) || (s=WORD_AT(g_esp+4u))<WS_STOCK || s>=WS_SLOTS || !ws_on()) { original();return; }
    if(ws_custom()) g_eax=(uint32_t)(int32_t)ws.ed[s-WS_STOCK];
    else if(ws_fe_scene() && ws_fe_map_set()) g_eax=(uint32_t)(int32_t)ws.fe[s-WS_STOCK];
    else { original();return; }
    MAPMAKER_GUEST_RET();
}
/* WRAP sub_00312AE0 (cdecl slot, weapon): editor set store from the pick
 * (slot = row index - 2) and the X clear; slots 7-20 land in the hook. The
 * pick moves the cursor on only while the value is below 5 (0x002BF990):
 * from rows 6-19 it moves on here (the slot pane is ebx, 0x002BF935). */
void pc_mapmaker_weaponset_set(void (*original)(void))
{
    uint32_t ret=MAPMAKER_STACK_OK(12)?WORD_AT(g_esp):0u,s,pane=g_ebx;
    if((ret!=WS_PICK_RET && ret!=WS_CLEAR_RET) || !ws_on() || ws.rows!=WS_SLOTS) { original();return; }
    s=WORD_AT(g_esp+4u);
    if(s>=WS_STOCK && s<WS_SLOTS) {
        ws.ed[s-WS_STOCK]=(int16_t)WORD_AT(g_esp+8u);
        MAPMAKER_GUEST_RET();
    } else original();
    if(ret==WS_PICK_RET && s>=WS_STOCK-1u && s+1u<WS_SLOTS && MAPMAKER_PTR_OK(pane,0x200u) &&
       WORD_AT(pane+0x184u)==s+2u && WORD_AT(pane+0x18Cu)==s) {
        WORD_AT(pane+0x18Cu)=s+1u;
        guest_call(sub_00074460,3,pane,s+1u,0u);
    }
}
/* WRAP sub_00064E70 (cdecl filter, weapon; returns 1 when not allowed): only
 * its call in the row draw (return 0x002D3DF6) matters. The draw indexes a
 * six-colour table on its stack by the row value ([esp + value*4 + 0x20],
 * 0x002D3D8C) and writes the label '1' + value into the consumed rect
 * argument slot (entry + 0xC, 0x002D3DE8; drawn from there at 0x002D3F27).
 * For rows 7-20 the label becomes "7".."20" and the colour the table's
 * value mod 6 (the read past the table took the return address and args). */
void pc_mapmaker_weaponset_label(void (*original)(void))
{
    uint32_t ret=MAPMAKER_STACK_OK(0x60)?WORD_AT(g_esp):0u,entry=g_esp+0x44u,item,v;
    original();
    if(ret!=WS_LABEL_RET) return;
    item=WORD_AT(entry+8u);
    if(!MAPMAKER_PTR_OK(item,0x18u) || (v=WORD_AT(item+0x14u))<WS_STOCK || v>=WS_SLOTS) return;
    snprintf((char *)((uintptr_t)(entry+0xCu)+g_xbox_mem_offset),4,"%u",(unsigned)v+1u);
    WORD_AT(entry-0x24u)=WORD_AT(entry-0x18u+4u*(v%WS_STOCK));
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
