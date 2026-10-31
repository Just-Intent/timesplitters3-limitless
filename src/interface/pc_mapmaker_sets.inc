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
 *  - the slot screen pages: Down on row 10 shows slots 11-20, Up on row 11
 *    returns; the fill/store of the shared work copy are pointed at members
 *    10..19 while page 2 is shown; the title gets "(1-10)" / "(11-20)";
 *  - the Story AI character choice lists slots 11-20 after the stock ten;
 *  - play: the level load's set copy (sub_0033EBF0) gets members 10..19 and
 *    count 20 (the per-member loops that use the count skip id -1), the
 *    custom-set preload also preloads them, and a Story AI that names an
 *    empty slot gets the first used slot instead of id -1 (logged).
 * Weapon sets are not raised: weapon slots are bound to the six weapon and
 * six ammo item types (catalogue +8 slot index); see the doc.
 * Editing (paging, extra choices) needs TSFP_MAPMAKER_LIMITS=1; loading and
 * playing a map that has the chunk works without it.
 *
 * Extension memory: 0x00903700..0x0090373F title text, 0x00903740..0x009037E3
 * page-2 store scratch (0x009037E4..0x009037FF reserved for this area).
 * Requires the includer's WORD_AT, guest_call, g_eax, g_esp,
 * g_xbox_mem_offset and pc_mapmaker_limits.inc. */
#include <stdio.h>
#include <string.h>

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
#define SETS_H(a) (*(int16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define SETS_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

extern void sub_00074670(void),sub_00071970(void),sub_00076EF0(void),sub_002BFFD0(void),
    sub_0003D7B0(void),sub_002BE650(void),sub_00071800(void),sub_000D7780(void);

static struct {
    int page,forced,in_update,in_store;
    uint32_t title_menu,title_original;
    unsigned logged;
} sets;

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

/* ---- slot screen paging ------------------------------------------------ */
static int sets_custom(void)
{
    return WORD_AT(SETS_SCENE)==102u && (int32_t)WORD_AT(SETS_INDEX)>=0xC;
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
 * from [0x0078AEAC] and stores it back (sub_002BFFD0) every frame. */
void pc_mapmaker_sets_screen(void (*original)(void))
{
    uint32_t menu,pane,in;
    int32_t row;
    if(!sets_custom() || !MAPMAKER_STACK_OK(8) || !sets_on()) { sets.page=0;original();return; }
    menu=WORD_AT(g_esp+4u);
    if(!sets.forced && WORD_AT(SETS_LOADED)!=WORD_AT(SETS_INDEX)) sets.page=0;   /* opened or index changed */
    sets.forced=0;
    pane=MAPMAKER_PTR_OK(menu,0x208u)?guest_call(sub_00074670,2,menu,SETS_PANE):0u;
    if(pane && MAPMAKER_PTR_OK(pane,0x200u)) {
        row=(int32_t)WORD_AT(pane+0x18Cu);in=WORD_AT(pane+0xC4u);
        if((!sets.page && row==(int32_t)SETS_STOCK-1 && (in&0x00020002u)) ||
           (sets.page && row==0 && (in&0x00010001u))) {
            sets.in_update=1;
            guest_call(sub_002BFFD0,0);           /* keep this page's last change */
            sets.page^=1;
            WORD_AT(pane+0xC4u)&=~0x000F000Fu;WORD_AT(pane+0xC8u)&=~0xFu;
            sets_select(pane,sets.page?0u:SETS_STOCK-1u);
            WORD_AT(SETS_LOADED)=0xFFFFFFFFu;     /* the original reloads the work copy */
            sets.forced=1;
        }
    }
    if(MAPMAKER_PTR_OK(menu,0x208u)) sets_title(menu);
    sets.in_update=1;
    original();                                   /* pops the return address */
    sets.in_update=0;
    if(sets.forced && WORD_AT(SETS_LOADED)!=0xFFFFFFFFu) sets.forced=0;
}
/* WRAP sub_002C0290 (cdecl index; returns a set): the fill sub_002C0310
 * reads members +4.. of it; page 2 reads members 10..19. */
void pc_mapmaker_sets_set_ptr(void (*original)(void))
{
    uint32_t ret=MAPMAKER_STACK_OK(4)?WORD_AT(g_esp):0u;
    original();
    if(ret==SETS_FILL_RET && sets.in_update && sets.page && g_eax) g_eax+=8u*SETS_STOCK;
}
/* WRAP sub_00312950 (returns the editor set in scene 102): page 2 for the
 * byte fill sub_003141B0, and the store's scratch. */
void pc_mapmaker_sets_editor_set(void (*original)(void))
{
    uint32_t ret=MAPMAKER_STACK_OK(4)?WORD_AT(g_esp):0u;
    original();
    if(!sets.in_update || !sets.page || !g_eax) return;
    if(ret==SETS_FILLB_RET) g_eax+=8u*SETS_STOCK;
    else if(ret==SETS_STORE_RET && sets.in_store) g_eax=SETS_SCRATCH;
}
/* WRAP sub_00314210 (cdecl work copy): stores 10 rows into members 0..9 and
 * the count of used rows into +0. Page 2 stores into a scratch set and
 * copies the rows to members 10..19; the count (rows 1-10, stock) stays. */
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

/* ---- Story AI character choice ----------------------------------------- */
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
    for(k=SETS_STOCK;k<SETS_SLOTS;k++) {
        uint32_t m=SETS_MEMBER(set,k),text;
        if(SETS_H(m)==-1) text=guest_call(sub_0003D7B0,1,0xD7Cu);          /* as 0x0031CD0A */
        else text=guest_call(sub_0003D7B0,1,guest_call(sub_002BE650,2,(uint32_t)(int32_t)SETS_H(m),
                                                        (uint32_t)(int32_t)SETS_H(m+6u)));
        guest_call(sub_00071800,2,text,(uint32_t)k);
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
