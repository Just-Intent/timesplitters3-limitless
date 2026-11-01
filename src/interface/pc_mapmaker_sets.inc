/* US 45410066 MapMaker custom character (bot) set: 10 -> 20 slots.
 * See docs/mapmaker/mapmaker-sets.md for every site and its evidence.
 *
 * The set struct is already 20 members wide everywhere it lives (dword count
 * + 20 x 8 bytes = 0xA4: the presets 0x0053E41C, the editor's custom set
 * editor +0x11860, the play set [desc+0x150]+0xE0 in a 0x190 heap block, the
 * builders' rep stosd 0x29), and the spawn lookup sub_0006BC50 accepts slots
 * 0..19 (cmp 0x13). Ten is fixed in the map-info chunk (count byte 10 and
 * 10 x 4-byte members, the chunk is full), the editor loader (10 unrolled
 * copies), the slot screen (a 10-row work copy 0x0075FD90 shared with the
 * Arcade front end) and the Story AI character choice (10 options). So:
 *  - members 11-20 travel in a fork-only chunk (tag 0x00C776F5) appended
 *    before the terminator at compile, only when one of them is used or a
 *    Story AI uses slot >= 10; a map without it is byte-identical to stock.
 *    Its presence makes mapmaker_container_over_stock save the 64 KB tier,
 *    which a stock build (and System Link) refuses (pc_mapmaker_container.inc);
 *  - editor load reads it into editor +0x11860 members 10..19 (empty = id
 *    -1), a new map clears them (the reset copies a preset whose members
 *    10..19 are zero = character 0), and Map Settings' "set changed" test
 *    (undo step) compares them too;
 *  - the editor's slot screen is a 20-row scrolling list, built like the
 *    weapon set's (the engine's scrolling region, ten rows tall, so the
 *    cursor, pointer, scrollbar and wheel reach slots 11-20). The 10-row
 *    work copy stays slots 1-10; rows 11-20 are ten shadow rows, loaded by
 *    the stock reload with the fill getters pointed at members 10..19, and
 *    lent to work-copy row k while each stock row callback runs (draw, row
 *    action, pane update, picker pick), then stored to members 10..19;
 *  - the in-game pause and Arcade setup Bots screen is the same screen, but
 *    there (scenes 100/101, MapMaker Arcade mode 9, index 0x11) its rows are
 *    the match's bot roster (at most 10 rows, 16 actors per match); its page
 *    2 (more bots) is pc_mapmaker_matchbots.inc, which uses sets_title and
 *    sets_select below;
 *  - the Story AI character choice lists slots 11-20 after the stock ten,
 *    each named by the stock slot-name path (sub_00303350 case 3 for slot
 *    k - 10 with the set read shifted 10 members, so realbosses' names of
 *    rows 150/151 apply as for slots 1-10), and a Story AI on slot 11-20
 *    gets the stock Story AI icon (list icon row, overview marker) instead
 *    of item 0x42.. (0x38 + slot);
 *  - play: the level load's set copy (sub_0033EBF0) gets members 10..19 and
 *    count 20 (the per-member loops that use the count skip id -1), the
 *    custom-set preload also preloads them, and a Story AI that names an
 *    empty slot gets the first used slot instead of id -1 (logged).
 * Weapon sets are not raised: weapon slots are bound to the six weapon and
 * six ammo item types (catalogue +8 slot index); see the doc.
 * Editing (the 20-row list, extra choices) needs TSFP_MAPMAKER_LIMITS=1;
 * loading and playing a map that has the chunk works without it.
 *
 * Extension memory: 0x00903700..0x0090373F title text (matchbots' pages),
 * 0x00903740..0x009037E3 store scratch for members 10..19
 * (0x009037E4..0x009037FF reserved for this area). The shadow rows are host
 * memory. Requires the includer's WORD_AT, guest_call, g_eax, g_esp,
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

#define SETS_SCENE 0x0079094Cu
#define SETS_EDITOR 0x007844A8u
#define SETS_CONTAINER 0x00784014u
#define SETS_SHADOW 0x00765E38u         /* undo shadow container (sub_003365F0) */
#define SETS_INDEX 0x00784024u          /* character set index; >= 0xC = custom */
#define SETS_LOADED 0x0078AEACu         /* sub_002C0490: index last loaded into the work copy */
#define SETS_EDITOR_SET 0x11860u        /* editor +0x11860: count + 20 x 8 */
#define SETS_SLOTS 20u
#define SETS_STOCK 10u
#define SETS_MEMBER(set,k) ((set)+4u+8u*(uint32_t)(k))   /* +0 word id, +3 byte, +6 word variant */
#define SETS_MAGIC 0x00C77667u
#define SETS_VERSION 0x12Du
#define SETS_INFO 0x00C77668u
#define SETS_SAI 0x00C7766Cu
#define SETS_END 0x00C7766Au
#define SETS_TAG 0x00C776F5u            /* fork-only; no game code uses it */
#define SETS_CHUNK 0x30u                /* +0 word 1, +2 first 10, +3 count 10, +4 10 x 4, +0x2C 0 */
#define SETS_SCAN 0x7FFE0u              /* container body (pc_mapmaker_container.inc) */
#define SETS_SAI_SLOT 0x28u             /* Story AI chunk data byte: character slot */
#define SETS_TITLE 0x00903700u          /* 0x40 bytes */
#define SETS_SCRATCH 0x00903740u        /* 0xA4 bytes */
#define SETS_PANE 0x0052372Cu           /* slot pane descriptor inside 0x0052375C */
/* return addresses (call site + length, checked in the decode test) */
#define SETS_FILL_RET 0x002C0319u       /* sub_002C0310: call sub_002C0290 */
#define SETS_FILLB_RET 0x003141B5u      /* sub_003141B0: call sub_00312950 */
#define SETS_STORE_RET 0x00314218u      /* sub_00314210: call sub_00312950 */
#define SETS_CHOICE_RET 0x0031CD32u     /* sub_0031CB90: call sub_00078590 */
#define SETS_PLAY_RET 0x00240DD3u       /* sub_00240C50: call sub_0033EBF0 */
#define SETS_PRELOAD_RET 0x0023F477u    /* sub_0023F390: call sub_0023F320 */
#define SETS_SAVE_COMPILE_RET 0x0032C6FFu
#define SETS_ICON_LIST_RET 0x0031D460u  /* sub_0031D410: call sub_00315D20 */
#define SETS_ICON_MAP_RET 0x00309AC7u   /* sub_00309A80: call sub_00315D20 */
#define SETS_SLOT_ITEM 0x38u            /* item 0x38 + k = slot k (sub_00303350 case 3) */
#define SETS_NAME_SET_RET 0x003033A9u  /* sub_00303350 case 3: call sub_002C0290 */
#define SETS_H(a) (*(int16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define SETS_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
/* the 20-row slot list (editor) */
#define SETS_WORK 0x0075FD90u           /* 10 x 0x1C: +0 id, +8 difficulty, +0x10 override, +0x14 portrait, +0x18 variant */
#define SETS_ROW 0x1Cu
#define SETS_SUBMODE 0x0078ADAEu        /* byte: setup sub-mode (0x13 limits the rows) */
#define SETS_BUILDING 0x007BB1E8u       /* the menu being built */
#define SETS_ROW_ACTION 0x002C1490u     /* slot row callbacks (pushed at 0x002C1A8D / 0x002C1A88) */
#define SETS_ROW_DRAW 0x002C0660u
#define SETS_ROW_HEIGHT 0x40800000u     /* 4.0f, the rows' height argument (0x002C1A92) */
#define SETS_VIEW 0x42200000u           /* 40.0f = ten rows of 4.0: the pane keeps its stock size */
#define SETS_F5 0x40A00000u             /* 5.0f */
#define SETS_F9_2 0x41133333u           /* 9.2f (0x002C1C24) */
#define SETS_F40 0x42200000u            /* 40.0f, the rows' width argument (0x002C1A97) */
#define SETS_F45 0x42340000u            /* 45.0f (0x002C19FA) */

extern void sub_00074670(void),sub_00071970(void),sub_00076EF0(void),sub_002BFFD0(void),
    sub_0003D7B0(void),sub_00303350(void),sub_00071800(void),sub_000D7780(void),sub_002C0310(void),
    sub_00075B70(void),sub_00071BE0(void),sub_00079D60(void),sub_002C1830(void),sub_00079710(void),
    sub_000780E0(void),sub_00078320(void),sub_000792B0(void),sub_00078380(void),sub_000781A0(void),
    sub_00077140(void),sub_00077360(void),sub_00074A00(void),sub_00074460(void);

static struct {
    int page,in_update,in_store,name_shift;
    uint32_t title_menu,title_original;
    unsigned logged;
} sets;
/* The editor's 20-row slot list: rows 11-20 are ten shadow work-copy rows. */
static struct {
    int on,valid,opened,last_row;      /* on: the pane was built with 20 rows */
    uint32_t index;                     /* set index the shadow rows were loaded for */
    float last_scroll;
    uint8_t rows[SETS_STOCK*SETS_ROW];
} sl;

static void sets_log(const char *what,int a,int b)
{
    if(sets.logged>=16u) return;
    sets.logged++;
    fprintf(stderr,"[mapmaker] sets: %s (%d, %d)\n",what,a,b);
    fflush(stderr);
}
static int sets_on(void)
{
    mapmaker_limits_read();
    return mapmaker_limits.limits;
}
static uint32_t sets_editor(void)
{
    uint32_t e=WORD_AT(SETS_EDITOR);
    return e && MAPMAKER_PTR_OK(e,SETS_EDITOR_SET+0xA4u)?e:0u;
}

/* ---- the container ----------------------------------------------------- */
typedef void (*SetsChunkFn)(uint32_t data,uint32_t size,void *ctx);
/* Calls fn for every chunk of tag; returns the terminator or 0. */
static uint32_t sets_walk(uint32_t c,uint32_t tag,SetsChunkFn fn,void *ctx)
{
    uint32_t p,end;
    unsigned steps;
    if(!c || !MAPMAKER_PTR_OK(c,SETS_SCAN+8u) || WORD_AT(c)!=SETS_MAGIC || WORD_AT(c+4u)!=SETS_VERSION) return 0;
    end=c+SETS_SCAN;
    for(p=c+8u,steps=0;steps<0x40000u && end-p>=8u;steps++) {
        uint32_t t=WORD_AT(p),size=WORD_AT(p+4u);
        if(t==SETS_END) return p;
        if(size>end-p-8u) return 0;
        if(t==tag && fn) fn(p+8u,size,ctx);
        p+=8u+size;
    }
    return 0;
}
static void sets_take(uint32_t data,uint32_t size,void *ctx)
{
    uint32_t *out=(uint32_t *)ctx;
    if(!*out && size>=SETS_CHUNK && SETS_H(data)==1 && SETS_B(data+2u)==SETS_STOCK &&
       SETS_B(data+3u)>=1u && SETS_B(data+3u)<=SETS_SLOTS-SETS_STOCK) *out=data;
}
static uint32_t sets_chunk(uint32_t c)
{
    uint32_t data=0;
    sets_walk(c,SETS_TAG,sets_take,&data);
    return data;
}
static void sets_sai_high(uint32_t data,uint32_t size,void *ctx)
{
    if(size>SETS_SAI_SLOT && SETS_B(data+SETS_SAI_SLOT)>=SETS_STOCK) *(int *)ctx=1;
}
static void sets_sai_used(uint32_t data,uint32_t size,void *ctx)
{
    if(size>SETS_SAI_SLOT && SETS_B(data+SETS_SAI_SLOT)<SETS_SLOTS) ((uint8_t *)ctx)[SETS_B(data+SETS_SAI_SLOT)]=1;
}

/* ---- editor set -------------------------------------------------------- */
static void sets_clear_extras(uint32_t e)
{
    unsigned k;
    for(k=SETS_STOCK;k<SETS_SLOTS;k++) {
        uint32_t m=SETS_MEMBER(e+SETS_EDITOR_SET,k);
        SETS_H(m)=-1;SETS_H(m+2u)=0;SETS_H(m+4u)=0;SETS_H(m+6u)=-1;
    }
}
static int sets_extras_used(uint32_t e)
{
    unsigned k;
    for(k=SETS_STOCK;k<SETS_SLOTS;k++) if(SETS_H(SETS_MEMBER(e+SETS_EDITOR_SET,k))!=-1) return 1;
    return 0;
}

/* WRAP sub_003006C0 (cdecl container, flags; returns used): append the
 * chunk before the terminator when slots 11-20 or a Story AI slot >= 10 are
 * used. The compiler zeroes the whole buffer first, so no old chunk stays. */
void pc_mapmaker_sets_compile(void (*original)(void))
{
    uint32_t ret,c,e,info,used,term,data,nterm;
    unsigned k;
    int high=0;
    if(!MAPMAKER_STACK_OK(8)) { original();return; }
    ret=WORD_AT(g_esp);c=WORD_AT(g_esp+4u);
    original();
    if(!(e=sets_editor()) || !(term=sets_walk(c,SETS_SAI,sets_sai_high,&high)) ||
       WORD_AT(c+8u)!=SETS_INFO) return;
    if(!high && !sets_extras_used(e)) return;
    info=c+0x10u;used=WORD_AT(info+0x248u);
    if(term!=c+used-8u || used+8u+SETS_CHUNK+4u>SETS_SCAN) {
        sets_log("no room for character slots 11-20; not saved",(int)used,(int)(term-c));
        return;
    }
    WORD_AT(term)=SETS_TAG;WORD_AT(term+4u)=SETS_CHUNK;data=term+8u;
    memset((void *)((uintptr_t)data+g_xbox_mem_offset),0,SETS_CHUNK);
    SETS_H(data)=1;SETS_B(data+2u)=SETS_STOCK;SETS_B(data+3u)=SETS_SLOTS-SETS_STOCK;
    for(k=0;k<SETS_SLOTS-SETS_STOCK;k++) {           /* the map-info member form */
        uint32_t m=SETS_MEMBER(e+SETS_EDITOR_SET,SETS_STOCK+k),d=data+4u+4u*k;
        SETS_H(d)=SETS_H(m);SETS_B(d+2u)=SETS_B(m+3u);SETS_B(d+3u)=SETS_B(m+6u);
    }
    nterm=data+SETS_CHUNK;
    WORD_AT(nterm)=SETS_END;WORD_AT(nterm+4u)=nterm-c;
    used=nterm-c+8u;
    WORD_AT(info+0x248u)=used;
    if(g_eax) {
        g_eax=used;
        if(ret==SETS_SAVE_COMPILE_RET && !(g_eax&0xFFFFu)) g_eax|=4u;   /* as the container hook */
    }
}

/* WRAP sub_00301A90 (cdecl container, flags): editor load; members 10..19
 * from the chunk, empty without it. */
void pc_mapmaker_sets_load(void (*original)(void))
{
    uint32_t c=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u,e,data;
    unsigned k,n;
    original();
    if(!(e=sets_editor()) || !sets_walk(c,0,0,0) || WORD_AT(c+8u)!=SETS_INFO) return;
    sets_clear_extras(e);
    if(!(data=sets_chunk(c))) return;
    n=SETS_B(data+3u);
    for(k=0;k<n;k++) {
        uint32_t m=SETS_MEMBER(e+SETS_EDITOR_SET,SETS_STOCK+k),d=data+4u+4u*k;
        SETS_H(m)=SETS_H(d);SETS_B(m+3u)=SETS_B(d+2u);SETS_H(m+6u)=(int16_t)(int8_t)SETS_B(d+3u);
    }
}

/* WRAP sub_00312810 (returns 1 when the set differs from the undo shadow
 * [0x00765E38]; Map Settings' close, sub_00313310, then records an undo
 * step): it compares members 0..9 only, so also compare 10..19. */
void pc_mapmaker_sets_changed(void (*original)(void))
{
    uint32_t e,data;
    unsigned k;
    original();
    if(g_eax || !(e=sets_editor())) return;
    data=sets_chunk(WORD_AT(SETS_SHADOW));
    for(k=0;k<SETS_SLOTS-SETS_STOCK;k++) {
        uint32_t m=SETS_MEMBER(e+SETS_EDITOR_SET,SETS_STOCK+k);
        int16_t id=data && k<SETS_B(data+3u)?SETS_H(data+4u+4u*k):-1;
        int16_t variant=data && k<SETS_B(data+3u)?(int16_t)(int8_t)SETS_B(data+4u+4u*k+3u):SETS_H(m+6u);
        if(SETS_H(m)!=id || (id!=-1 && SETS_H(m+6u)!=variant)) { g_eax=1u;return; }
    }
}

/* WRAP sub_002FEBF0 (new map): its preset copy (sub_002F9E90, 0xA4 bytes)
 * leaves members 10..19 as id 0. */
void pc_mapmaker_sets_reset(void (*original)(void))
{
    uint32_t e;
    original();
    if((e=sets_editor())) sets_clear_extras(e);
}

/* ---- slot screen: a 20-row scrolling list ------------------------------ */
static int sets_custom(void)
{
    return WORD_AT(SETS_SCENE)==102u && (int32_t)WORD_AT(SETS_INDEX)>=0xC;
}
/* Screen diagnostics (own budget, as the weapon set's): the pane build, its
 * first update and moves past row 10 or of the scroll. */
static unsigned sets_diag_opens,sets_diag_moves;
static void sets_diag(int moves,const char *format,...)
{
    va_list ap;
    if(moves ? sets_diag_moves>=48u : sets_diag_opens>=256u) return;
    if(moves) sets_diag_moves++; else sets_diag_opens++;
    fputs("[mapmaker] sets: ",stderr);
    va_start(ap,format);vfprintf(stderr,format,ap);va_end(ap);
    fputc('\n',stderr);
    fflush(stderr);
}
static float sets_f(uint32_t a) { uint32_t u=WORD_AT(a);float f;memcpy(&f,&u,4);return f; }
static uint8_t *sets_work(unsigned k) { return (uint8_t *)((uintptr_t)(SETS_WORK+k*SETS_ROW)+g_xbox_mem_offset); }
static int sets_list(void) { return sl.on && WORD_AT(SETS_SCENE)==102u; }
/* Row value v (pane +0x184 - 2, item +0x14) of slots 11-20 -> shadow row. */
static int sets_list_row(uint32_t v) { return sets_list() && v>=SETS_STOCK && v<SETS_SLOTS ? (int)(v-SETS_STOCK) : -1; }
/* Lend shadow row k (slot 11 + k) to work-copy row k, or take it back. */
static void sets_borrow(unsigned k)
{
    uint8_t t[SETS_ROW],*w=sets_work(k),*s=sl.rows+k*SETS_ROW;
    memcpy(t,w,SETS_ROW);memcpy(w,s,SETS_ROW);memcpy(s,t,SETS_ROW);
}
/* Rows with no character (a preset: its members 10..19 are id 0, a real
 * character), team and difficulty as row k of the work copy. */
static void sets_list_empty(void)
{
    unsigned k;
    int32_t none=-1;
    memcpy(sl.rows,sets_work(0),sizeof sl.rows);
    for(k=0;k<SETS_STOCK;k++) {
        uint8_t *r=sl.rows+k*SETS_ROW;
        memcpy(r,&none,4);memcpy(r+0x10,&none,4);memcpy(r+0x14,&none,4);memcpy(r+0x18,&none,4);
    }
}
/* Shadow row k into editor member 10 + k, as sub_00314210 stores a row:
 * word id (+0), byte difficulty (+8 -> +3), word variant (+0x18 -> +6). */
static void sets_list_keep(unsigned k)
{
    uint32_t e,m;
    const uint8_t *r=sl.rows+k*SETS_ROW;
    int32_t id,diff,variant;
    if(!sets_custom() || !(e=sets_editor())) return;
    memcpy(&id,r,4);memcpy(&diff,r+8,4);memcpy(&variant,r+0x18,4);
    m=SETS_MEMBER(e+SETS_EDITOR_SET,SETS_STOCK+k);
    SETS_H(m)=(int16_t)id;SETS_B(m+3u)=(uint8_t)diff;SETS_H(m+6u)=(int16_t)variant;
}
/* Move the highlight to row value v (the pick's own advance, 0x002C1DB2). */
static void sets_advance(uint32_t pane,uint32_t v)
{
    WORD_AT(pane+0x18Cu)=v;
    guest_call(sub_00074460,3,pane,v,0u);
}

/* WRAP sub_002C19F0 (cdecl menu): builder of the character slot pane
 * 0x0052372C. In the editor (scene 102) stock adds the title 0xE8B, the set
 * list (value -2), ten row groups (sub_00078320(0, 1, 4), one item
 * sub_000792B0(0, row, 0, 40.0f, 4.0f, sub_002C1490, sub_002C0660),
 * sub_00078380()) and the footer. With the switch this replays the same
 * calls with twenty row groups inside the scrolling region the picker list
 * sub_002C1EA0 and the weapon slot pane use (sub_000780E0(height, 0) ..
 * sub_000781A0()), ten rows tall: every item made in it gets flag 0x40
 * (0x0007914A) and its group adds the row height to the content +0x5C
 * (0x000753CD), so the stock cursor, the pointer, the scrollbar and the wheel
 * reach slots 11-20. The row index stays value + 2. Rows 11-20 borrow a
 * work-copy row while their callbacks run (sets_borrow). */
void pc_mapmaker_sets_build(void (*original)(void))
{
    uint32_t ret=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp):0u,menu=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u,built,v;
    int scene=(int)WORD_AT(SETS_SCENE);
    sl.on=0;sl.valid=0;sl.opened=0;sl.last_row=-1;sl.last_scroll=-1.f;
    if(scene!=102 || !sets_on() || !MAPMAKER_PTR_OK(menu,0x200u)) {
        if(scene==102) sets_diag(0,"slot pane build (stock 10 rows: %s) ret=0x%08X menu=0x%08X",
            !sets_on()?"TSFP_MAPMAKER_LIMITS is not 1":"no menu",ret,menu);
        original();return;
    }
    guest_call(sub_00075B70,5,0x10u,0u,SETS_F45,0u,0x002C1450u);
    guest_call(sub_00071BE0,3,1u,SETS_F5,SETS_F5);
    guest_call(sub_00079D60,6,guest_call(sub_0003D7B0,1,0xE8Bu),0xFFFFFFFFu,0x04032404u,0u,0u,0u);
    guest_call(sub_00079D60,6,0x004766D8u,0xFFFFFFFEu,0x08030C00u,0u,0u,0u);
    guest_call(sub_002C1830,1,menu);
    guest_call(sub_00079710,1,0u);
    guest_call(sub_000780E0,2,SETS_VIEW,0u);
    for(v=0;v<SETS_SLOTS;v++) {
        guest_call(sub_00078320,3,0u,1u,4u);
        guest_call(sub_000792B0,7,0u,v,0u,SETS_F40,SETS_ROW_HEIGHT,SETS_ROW_ACTION,SETS_ROW_DRAW);
        guest_call(sub_00078380,0);
    }
    guest_call(sub_000781A0,0);
    built=WORD_AT(SETS_BUILDING);
    guest_call(sub_00077140,1,built);
    guest_call(sub_00077360,3,WORD_AT(SETS_BUILDING),SETS_F5,SETS_F9_2);
    guest_call(sub_00074A00,4,0u,guest_call(sub_0003D7B0,1,0xE88u),1u,0u);
    guest_call(sub_00074A00,4,3u,guest_call(sub_0003D7B0,1,0x3B2u),1u,0u);
    guest_call(sub_00074A00,4,1u,guest_call(sub_0003D7B0,1,0x1A27u),0u,0u);
    sl.on=1;
    sets_diag(0,"slot pane build (20 rows, scrolling) ret=0x%08X menu=0x%08X set=%d",ret,menu,(int)WORD_AT(SETS_INDEX));
    MAPMAKER_GUEST_RET();
}
/* WRAP sub_002C0660 (cdecl pane, item, rect): a slot row's draw. It reads
 * the row value at item +0x14 (0x002C0667) and the work-copy row of it (in
 * the editor only rows below 10 get a name, 0x002C08A6); the highlight is
 * the item pointer (sub_00071A60, 0x002C0A2B). */
void pc_mapmaker_sets_row_draw(void (*original)(void))
{
    uint32_t item=MAPMAKER_STACK_OK(0x10)?WORD_AT(g_esp+8u):0u,v;
    int k;
    if(!MAPMAKER_PTR_OK(item,0x18u) || (k=sets_list_row(v=WORD_AT(item+0x14u)))<0) { original();return; }
    WORD_AT(item+0x14u)=(uint32_t)k;sets_borrow((unsigned)k);
    original();
    sets_borrow((unsigned)k);WORD_AT(item+0x14u)=v;
}
/* Run a slot pane callback (row = pane +0x184 - 2, 0x002C1499 / 0x002C157E)
 * for slot 11 + k as for slot 1 + k; the editor stops at row 10
 * (0x002C14B3, 0x002C16D8). +0x18C (the highlighted value) follows. */
static void sets_list_call(void (*original)(void),uint32_t pane,unsigned k)
{
    uint32_t idx=WORD_AT(pane+0x184u),val=WORD_AT(pane+0x18Cu);
    WORD_AT(pane+0x184u)=k+2u;
    if(val==idx-2u) WORD_AT(pane+0x18Cu)=k;
    sets_borrow(k);
    original();
    sets_borrow(k);
    if(WORD_AT(pane+0x184u)==k+2u) WORD_AT(pane+0x184u)=idx;
    if(val==idx-2u && WORD_AT(pane+0x18Cu)==k) WORD_AT(pane+0x18Cu)=val;
    sets_list_keep(k);
}
/* WRAP sub_002C1490 (cdecl pane): a slot row's action (opens the picker
 * 0x0052374C when the set is custom). */
void pc_mapmaker_sets_row_action(void (*original)(void))
{
    uint32_t pane=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u;
    int k;
    if(!MAPMAKER_PTR_OK(pane,0x200u) || (k=sets_list_row(WORD_AT(pane+0x184u)-2u))<0) { original();return; }
    sets_list_call(original,pane,(unsigned)k);
}
/* WRAP sub_002C1570 (cdecl pane): the slot pane's update. For the
 * highlighted row it shows the button hints, clears the row on X (+0x800000:
 * id and portrait -1, 0x002C1718) and picks the preview ([0x0078AEA8], from
 * the row's id and variant, 0x002C17F2). Logs the first update and moves of
 * the highlight past row 10 or of the scroll. */
void pc_mapmaker_sets_pane(void (*original)(void))
{
    uint32_t pane=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u;
    int k,row;
    float scroll;
    if(!sets_list() || !MAPMAKER_PTR_OK(pane,0x200u)) { original();return; }
    row=(int)WORD_AT(pane+0x184u)-2;scroll=sets_f(pane+0x88u);
    if(!sl.opened) {
        sl.opened=1;
        sets_diag(0,"slot pane open: pane=0x%08X items=%u content=%.1f view=%.1f..%.1f scroll=%.1f slot=%d",
            pane,WORD_AT(pane+0x180u),(double)sets_f(pane+0x5Cu),(double)sets_f(pane+0x6Cu),(double)sets_f(pane+0x70u),
            (double)scroll,row+1);
    } else if((row!=sl.last_row && row>=(int)SETS_STOCK) || scroll!=sl.last_scroll)
        sets_diag(1,"list: slot %d highlighted, scroll %.1f of %.1f",row+1,(double)scroll,
            (double)(sets_f(pane+0x5Cu)-(sets_f(pane+0x70u)-sets_f(pane+0x6Cu))));
    sl.last_row=row;sl.last_scroll=scroll;
    if((k=sets_list_row((uint32_t)row))<0) { original();return; }
    sets_list_call(original,pane,(unsigned)k);
}
/* WRAP sub_002C1C90 (cdecl picker pane): the character picker's row
 * callback; stores the pick into work-copy row (slot pane +0x184 - 2),
 * 0x002C1CC7..0x002C1D56, then moves the slot highlight on while its value
 * is below 9 (0x002C1D8E). Chained OUTSIDE pc_mapmaker_realbosses_pick so its
 * fix-up of the work copy sees the borrowed row. Slots 11-20 store into
 * their shadow row and move on to the next slot; slot 10 moves on to 11. */
void pc_mapmaker_sets_pick(void (*original)(void))
{
    uint32_t picker=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u,slot,idx,val,item;
    int k,advance;
    if(!sets_list() || !MAPMAKER_PTR_OK(picker,0x200u) ||
       !MAPMAKER_PTR_OK(slot=guest_call(sub_00074670,2,WORD_AT(picker+0x2Cu),SETS_PANE),0x200u)) { original();return; }
    idx=WORD_AT(slot+0x184u);val=WORD_AT(slot+0x18Cu);item=WORD_AT(picker+0x198u);
    advance=MAPMAKER_PTR_OK(item,0x2Cu) && !(SETS_B(item+0x28u)&1u) &&    /* a disabled row picks nothing (0x002C1CA9) */
            SETS_B(SETS_SUBMODE)!=0x13u && val==idx-2u;                   /* sub-mode 0x13 limits the rows (0x002C1D96) */
    if((k=sets_list_row(idx-2u))<0) {
        original();
        if(advance && val==SETS_STOCK-1u && WORD_AT(slot+0x184u)==idx && WORD_AT(slot+0x18Cu)==val) sets_advance(slot,val+1u);
        return;
    }
    WORD_AT(slot+0x184u)=(uint32_t)k+2u;WORD_AT(slot+0x18Cu)=SETS_STOCK-1u;   /* the stock advance stays off */
    sets_borrow((unsigned)k);
    original();
    sets_borrow((unsigned)k);
    WORD_AT(slot+0x184u)=idx;WORD_AT(slot+0x18Cu)=val;
    sets_list_keep((unsigned)k);
    if(advance && val+1u<SETS_SLOTS) sets_advance(slot,val+1u);
}
static void sets_title(uint32_t menu)
{
    char *t=(char *)((uintptr_t)SETS_TITLE+g_xbox_mem_offset);
    uint32_t cur=WORD_AT(menu+0x204u);
    if(menu!=sets.title_menu || cur!=SETS_TITLE) {
        if(cur==SETS_TITLE) return;               /* another menu: leave it */
        sets.title_menu=menu;sets.title_original=cur;
    }
    if(sets.title_original && MAPMAKER_PTR_OK(sets.title_original,1)) {
        const char *o=(const char *)((uintptr_t)sets.title_original+g_xbox_mem_offset);
        size_t n=strnlen(o,0x2Au);
        if(n>=0x2Au) return;                      /* too long to extend: keep the stock title */
        memcpy(t,o,n);
        snprintf(t+n,0x40u-n,sets.page?"  (11-20)":"  (1-10)");
    } else snprintf(t,0x40u,sets.page?"(11-20)":"(1-10)");
    WORD_AT(menu+0x204u)=SETS_TITLE;
}
static void sets_select(uint32_t pane,uint32_t row)
{
    uint32_t item=guest_call(sub_00071970,3,pane,row,0u);
    if(item) guest_call(sub_00076EF0,2,pane,item);
}
/* WRAP sub_002C0490 (cdecl menu): update of the character set screen
 * 0x0052375C; it reloads the 10-row work copy when the set index differs
 * from [0x0078AEAC] (sub_002C0310, then the portraits sub_002BE600 returning
 * to 0x002C054C) and stores it back (sub_002BFFD0) every frame. With the
 * 20-row list, when the screen opens or the index changes, it first runs once
 * with the fill getters on members 10..19 (sets.page) and the reload forced:
 * the stock code, every hook on it included (realbosses names its rows for
 * that return address), fills the work copy with slots 11-20, which become
 * the shadow rows; the work copy and [0x0078AEAC] are then put back and the
 * original runs as stock for slots 1-10. Presets get empty rows 11-20. */
void pc_mapmaker_sets_screen(void (*original)(void))
{
    uint32_t ret,index,loaded;
    uint8_t save[sizeof sl.rows];
    sets.page=0;
    if(!sets_list() || !MAPMAKER_STACK_OK(8) || !sets_on()) { original();return; }
    ret=WORD_AT(g_esp);index=WORD_AT(SETS_INDEX);
    if(!sl.valid || sl.index!=index) {
        if((int32_t)index>=0xC && sets_editor()) {
            memcpy(save,sets_work(0),sizeof save);loaded=WORD_AT(SETS_LOADED);
            sets.page=1;sets.in_update=1;WORD_AT(SETS_LOADED)=0xFFFFFFFFu;
            original();                           /* pops the return address */
            g_esp-=4u;WORD_AT(g_esp)=ret;         /* for the stock run below */
            sets.page=0;sets.in_update=0;
            memcpy(sl.rows,sets_work(0),sizeof sl.rows);
            memcpy(sets_work(0),save,sizeof save);WORD_AT(SETS_LOADED)=loaded;
        } else sets_list_empty();
        sl.index=index;sl.valid=1;
    }
    sets.in_update=1;
    original();                                   /* pops the return address */
    sets.in_update=0;
}
/* WRAP sub_002C0290 (cdecl index; returns a set): the fill sub_002C0310
 * reads members +4.. of it; the slot list's shadow load (sets.page inside
 * the screen update) reads members 10..19. The slot name of
 * sub_00303350 case 3 (0x003033A4) reads member item - 0x38; while
 * sets_label asks for slot k (11-20) as item 0x38 + k - 10, member k. */
void pc_mapmaker_sets_set_ptr(void (*original)(void))
{
    uint32_t ret=MAPMAKER_STACK_OK(4)?WORD_AT(g_esp):0u;
    original();
    if(ret==SETS_FILL_RET && sets.in_update && sets.page && g_eax) g_eax+=8u*SETS_STOCK;
    else if(ret==SETS_NAME_SET_RET && sets.name_shift && g_eax) g_eax+=8u*SETS_STOCK;
}
/* WRAP sub_00312950 (returns the editor set in scene 102): members 10..19
 * for the byte fill sub_003141B0 during the shadow load, and the store's
 * scratch. */
void pc_mapmaker_sets_editor_set(void (*original)(void))
{
    uint32_t ret=MAPMAKER_STACK_OK(4)?WORD_AT(g_esp):0u;
    original();
    if(!sets.in_update || !sets.page || !g_eax) return;
    if(ret==SETS_FILLB_RET) g_eax+=8u*SETS_STOCK;
    else if(ret==SETS_STORE_RET && sets.in_store) g_eax=SETS_SCRATCH;
}
/* WRAP sub_00314210 (cdecl work copy): stores 10 rows into members 0..9 and
 * the count of used rows into +0. During the shadow load (the update's tail
 * store) it stores into a scratch set and copies the rows to members
 * 10..19; the count (rows 1-10, stock) stays. */
void pc_mapmaker_sets_store(void (*original)(void))
{
    uint32_t e;
    unsigned k;
    if(!sets.in_update || !sets.page || !(e=sets_editor())) { original();return; }
    sets.in_store=1;
    original();
    sets.in_store=0;
    for(k=0;k<SETS_STOCK;k++) {
        uint32_t s=SETS_MEMBER(SETS_SCRATCH,k),m=SETS_MEMBER(e+SETS_EDITOR_SET,SETS_STOCK+k);
        SETS_H(m)=SETS_H(s);SETS_B(m+3u)=SETS_B(s+3u);SETS_H(m+6u)=SETS_H(s+6u);
    }
}

/* WRAP sub_002D2C70 (cdecl, no arguments): match start. Its roster loop
 * (0x002D36EA..0x002D3822) turns the 10 work-copy rows 0x0075FD90 into the
 * match's bots (0x0078AD6C.. per-actor bytes, 16 actors). The editor's Test
 * map setup (sub_002EFBF0 -> sub_002D38C0) reaches it in scene 102 with the
 * work copy the slot screen left behind. The 20-row list always leaves rows
 * 1-10 there (sets.page is set only inside its shadow load); should the
 * getters still be shifted, refill rows 1-10 (sub_002C0310, index in eax)
 * first, as stock fills them when the screen opens. */
void pc_mapmaker_sets_match_start(void (*original)(void))
{
    if(sets.page && WORD_AT(SETS_SCENE)==102u && (int32_t)WORD_AT(SETS_INDEX)>=0xC) {
        uint32_t a=g_eax;
        sets.page=0;
        g_eax=WORD_AT(SETS_INDEX);
        guest_call(sub_002C0310,0);
        g_eax=a;
        WORD_AT(SETS_LOADED)=WORD_AT(SETS_INDEX);
    }
    sets.page=0;
    original();
}

/* ---- Story AI character choice ----------------------------------------- */
/* Slot k's label, made by the stock code that names slots 1-10 (0x0031CD03:
 * sub_00303350(0x38 + k), case 3 at 0x0030339E: sub_0003D7B0(sub_002BE650(
 * id, variant)) of member item - 0x38 of sub_002C0290(index)). Slots 11-20
 * are items 0x38..0x41 with that set read 10 members on (name_shift), so
 * every hook on the stock name path applies unchanged: realbosses names
 * ids 0x15 / 0x28 (rows 150 / 151) only for sub_002BE600 returning to
 * 0x002BE65F inside sub_002BE650 called from 0x003033C0. Asking for the
 * row from here (another return address) gave -1, so no name. */
static uint32_t sets_label(unsigned k,uint32_t m)
{
    uint32_t text;
    if(SETS_H(m)==-1) return guest_call(sub_0003D7B0,1,0xD7Cu);           /* as 0x0031CD0A */
    sets.name_shift=1;
    text=guest_call(sub_00303350,1,SETS_SLOT_ITEM+k-SETS_STOCK);
    sets.name_shift=0;
    return text;
}
/* WRAP sub_00078590 (cdecl value pointer): ends a choice item. At
 * 0x0031CD2D it ends the Story AI character choice, whose options 0..9 are
 * the set's slots; add slots 11-20 the same way (name or the empty text). */
void pc_mapmaker_sets_sai_choice(void (*original)(void))
{
    uint32_t e,value,set;
    unsigned k;
    if(!MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=SETS_CHOICE_RET || !sets_custom() || !(e=sets_editor())) {
        original();return;
    }
    value=WORD_AT(g_esp+4u);
    if(!sets_on() && !(MAPMAKER_PTR_OK(value,4) && WORD_AT(value)>=SETS_STOCK && WORD_AT(value)<SETS_SLOTS)) {
        original();return;
    }
    set=e+SETS_EDITOR_SET;
    for(k=SETS_STOCK;k<SETS_SLOTS;k++) guest_call(sub_00071800,2,sets_label(k,SETS_MEMBER(set,k)),(uint32_t)k);
    original();
}

/* WRAP sub_00315D20 (cdecl item type -> icon; catalogue 0x00524F58 +0xB4,
 * flag +0xB8 & 0x400). A Story AI is drawn as item 0x38 + its slot
 * (sub_0031D3E0 0x0031D406, sub_0031D410 0x0031D442; 9 for type 5): the
 * Story AI list's icon row (call 0x0031D45B) and the editor overview's AI
 * marker (sub_00309A80, call 0x00309AC2). Slots 11-20 give items
 * 0x42..0x4B, other items (0x42 is the one with icon 0x45); the ten slot
 * items 0x38..0x41 share icon 0x42, so use slot k - 10's item. */
void pc_mapmaker_sets_icon(void (*original)(void))
{
    uint32_t ret,item;
    if(MAPMAKER_STACK_OK(8)) {
        ret=WORD_AT(g_esp);
        if(ret==SETS_ICON_LIST_RET || ret==SETS_ICON_MAP_RET) {
            item=WORD_AT(g_esp+4u);
            if(item>=SETS_SLOT_ITEM+SETS_STOCK && item<SETS_SLOT_ITEM+SETS_SLOTS) WORD_AT(g_esp+4u)=item-SETS_STOCK;
        }
    }
    original();
}

/* ---- play -------------------------------------------------------------- */
/* WRAP sub_0033EBF0 (cdecl set, map-info +0x16): the level load's copy of the
 * custom set (ids only, count byte). Members 10..19 and count 20 from the
 * chunk; a Story AI naming an empty slot gets the first used one. */
void pc_mapmaker_sets_play_set(void (*original)(void))
{
    uint32_t dst,c,data;
    unsigned k,n,first;
    uint8_t used[SETS_SLOTS];
    if(!MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=SETS_PLAY_RET) { original();return; }
    dst=WORD_AT(g_esp+4u);
    original();
    c=WORD_AT(SETS_CONTAINER);
    if(!MAPMAKER_PTR_OK(dst,0xA4u) || !(data=sets_chunk(c))) return;
    n=SETS_B(data+3u);
    for(k=0;k<SETS_SLOTS-SETS_STOCK;k++)
        SETS_H(SETS_MEMBER(dst,SETS_STOCK+k))=k<n?SETS_H(data+4u+4u*k):-1;
    WORD_AT(dst)=SETS_SLOTS;
    memset(used,0,sizeof used);
    sets_walk(c,SETS_SAI,sets_sai_used,used);
    for(first=0;first<SETS_SLOTS && SETS_H(SETS_MEMBER(dst,first))==-1;first++) {}
    for(k=0;k<SETS_SLOTS;k++)
        if(used[k] && SETS_H(SETS_MEMBER(dst,k))==-1 && first<SETS_SLOTS) {
            SETS_H(SETS_MEMBER(dst,k))=SETS_H(SETS_MEMBER(dst,first));
            sets_log("story AI uses an empty character slot; using the first used slot",(int)k+1,(int)first+1);
        }
}
/* WRAP sub_0023F320 (ebx = map-info +0x16): the custom set's preload; also
 * preload members 11-20. */
void pc_mapmaker_sets_preload(void (*original)(void))
{
    uint32_t ret=MAPMAKER_STACK_OK(4)?WORD_AT(g_esp):0u,data;
    unsigned k,n;
    original();
    if(ret!=SETS_PRELOAD_RET || !(data=sets_chunk(WORD_AT(SETS_CONTAINER)))) return;
    n=SETS_B(data+3u);
    for(k=0;k<n;k++) {
        int32_t id=SETS_H(data+4u+4u*k);
        if(id!=-1) guest_call(sub_000D7780,2,(uint32_t)id,(uint32_t)id);
    }
}
