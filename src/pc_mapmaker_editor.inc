/* US 45410066 MapMaker editor object: Story AI, logic and free-standing
 * object capacity (docs/mapmaker-editor.md).
 *
 * The editor object (sub_002FE360, pointer [0x007844A8], Level heap) packs
 * fixed arrays against each other. config/guest-code-patches/editor.json
 * grows the object from 0x11950 to MAPMAKER_EDITOR_SIZE bytes, keeps every
 * existing field where it is and rewrites every displacement that names an
 * old array so the arrays live after +0x11950:
 *
 *   array          stock                enlarged (offset, entries)   count
 *   Story AI       +0x288   50 x 0xE4   +0x11950  255                +0x280
 *   logic ops      +0x41E4  30 x 0x220  +0x1FC70  120                +0x41E0
 *   logic nodes    +0x81A8 130 x 0xB4   +0x2FB70  520                +0x81A4
 *   logic records  +0xDD14 100 x 0x8C   +0x46910  400                +0xDD10
 *   free objects   +0x3070 170 x 12     +0x543D0  680                +0x3068..+0x306E
 *   clipboard      +0x3870 170 x 12     +0x563B0  680                +0x5048..+0x504E (moved)
 *
 * The free-object sets are {u16 count[3], u16 total; entry[]} and the set
 * functions address entries as set + 8 + 12*i for either set, so both
 * arrays move by the same delta from their counts: the main counts stay,
 * the clipboard counts move into the vacated old ops array (+0x5048).
 *
 * Counts stay where they were. This file holds the C policy around the
 * enlarged arrays. Editor limits (what the player may place) follow
 * TSFP_MAPMAKER_LIMITS=1 (mapmaker_limits.limits); capacity guards are
 * always on.
 *
 *  - pc_mapmaker_editor_sai_add (WRAP sub_0031BE70): add a story AI. Stock
 *    refuses at 50; opted in, 255. Same container-space test and messages
 *    (the "limit reached" text gets the limit in force). Entries past the 50
 *    editor colours get colour (index mod 50) instead of the stock 50.
 *  - pc_mapmaker_editor_logic_room (WRAP sub_00300670): the container-space
 *    test every logic editing action calls first. For those callers it also
 *    answers "no room" when ops, nodes or records are within
 *    MAPMAKER_LOGIC_HEADROOM of capacity: the node and record allocators
 *    (sub_00320B20, sub_00320CC0, sub_00325820) have no bound and cannot fail.
 *  - pc_mapmaker_editor_logic_can_add / _logic_full (WRAP sub_00321E60,
 *    sub_00321E80): ops 30 -> 120 and objectives 10 -> 40.
 *  - pc_mapmaker_editor_objective (WRAP sub_00325F60, whose own test
 *    editor.json raises to 40): stock 10 when the limits are off; never past
 *    the ops capacity (it creates an op).
 *  - pc_mapmaker_editor_node_room (WRAP sub_0031DB00): nodes per type
 *    30/50/50 -> 120/200/200 (sum = node capacity). Kind-0 ops (data table
 *    0x0052E8C0 + kind*0x44 + 0x28) 30 -> 120 while the limits are on;
 *    kind-1 ops stay at 8 (sub_003242B0 sizes a heap block for 8).
 *  - pc_mapmaker_editor_load (WRAP sub_00301A90): the editor map loader
 *    appends chunks to the arrays with no bound. Chunks past an array's
 *    capacity (and kind-1 ops past 8) are hidden from the loader (tag 0,
 *    which its switch skips) and restored afterwards.
 *  - pc_mapmaker_editor_level_load (WRAP sub_00240C50): a MapMaker map being
 *    started. Its count pass sub_00240590 keeps one stack slot per story AI
 *    chunk with no bound; editor.json grows that frame to
 *    MAPMAKER_LEVEL_SAI_SLOTS slots and chunks past that are hidden for the
 *    whole load the same way.
 *  - pc_mapmaker_editor_free_place / _free_move (WRAP sub_00304330,
 *    sub_002F7F30): free-standing objects per type 40/30/100 -> 160/120/400
 *    (sum = capacity 680) while the limits are on. The originals keep their
 *    stock compares; the per-type count is shown to them as stock-1 for the
 *    call and corrected by what they added.
 *  - pc_mapmaker_editor_logic_slot (WRAP sub_0033F480, play time): the
 *    45-slot table at [0x00765E44]+0xB4 takes one slot per distinct record
 *    with no bound. Its index is also the HUD marker id, and sub_002287E0
 *    reserves exactly 45 logic ids before the object markers, so the table is
 *    guarded, not enlarged: a 46th distinct record's action is skipped.
 *
 * Requires the includer's WORD_AT, guest_writable, guest_call,
 * g_esp/g_eax and mapmaker_limits (pc_mapmaker_limits.inc). */
#include <stdio.h>
#include <stdlib.h>

#define MAPMAKER_EDITOR_SIZE 0x58390u
#define MAPMAKER_EDITOR_SAI 0x11950u            /* 255 x 0xE4 */
#define MAPMAKER_EDITOR_SAI_CAP 255u
#define MAPMAKER_EDITOR_SAI_STOCK 50u
#define MAPMAKER_EDITOR_SAI_LIMIT 255u          /* byte index [0x00762F94] is unsigned */
#define MAPMAKER_LEVEL_SAI_SLOTS 1024u          /* sub_00240590 frame, editor.json */
#define MAPMAKER_EDITOR_OPS 0x1FC70u            /* 120 x 0x220 */
#define MAPMAKER_EDITOR_OPS_CAP 120u
#define MAPMAKER_EDITOR_NODES 0x2FB70u          /* 520 x 0xB4 */
#define MAPMAKER_EDITOR_NODES_CAP 520u
#define MAPMAKER_EDITOR_RECORDS 0x46910u        /* 400 x 0x8C */
#define MAPMAKER_EDITOR_RECORDS_CAP 400u
#define MAPMAKER_OBJECTIVES_LIMIT 40u           /* editor.json 0x00325F65 */
#define MAPMAKER_OBJECTIVES_STOCK 10u
#define MAPMAKER_OPS_STOCK 30u
#define MAPMAKER_KIND1_OPS 8u
#define MAPMAKER_LOGIC_HEADROOM 8u
#define MAPMAKER_LOGIC_SLOTS 45u
#define MAPMAKER_EDITOR_FREE 0x543D0u           /* 680 x 12 */
#define MAPMAKER_EDITOR_CLIP 0x563B0u           /* 680 x 12 */
#define MAPMAKER_EDITOR_CLIP_COUNTS 0x5048u
#define MAPMAKER_EDITOR_FREE_CAP 680u
#define MAPMAKER_FREE_DELTA (MAPMAKER_EDITOR_FREE-0x3070u) /* set + 8 + DELTA + 12*i */

#define MAPMAKER_TAG_FREE 0x00C7766Bu
#define MAPMAKER_TAG_SAI 0x00C7766Cu
#define MAPMAKER_TAG_NODE 0x00C7766Du
#define MAPMAKER_TAG_OP 0x00C7766Eu
#define MAPMAKER_TAG_RECORD 0x00C7766Fu
#define MAPMAKER_TAG_END 0x00C7766Au
#define MAPMAKER_CONTAINER_LIMIT 0x00080000u    /* scan bound: 512 KB */

/* Chunk capacities for the editor loader. kind: data byte +4 must equal
 * kind_value (op kind) for the entry to apply; 0 = any chunk of the tag. */
typedef struct MapMakerCap { uint32_t tag, capacity, kind, kind_value; } MapMakerCap;
static const MapMakerCap mapmaker_editor_caps[]={
    {MAPMAKER_TAG_SAI,MAPMAKER_EDITOR_SAI_CAP,0,0},
    {MAPMAKER_TAG_FREE,MAPMAKER_EDITOR_FREE_CAP,0,0},
    {MAPMAKER_TAG_NODE,MAPMAKER_EDITOR_NODES_CAP,0,0},
    {MAPMAKER_TAG_OP,MAPMAKER_KIND1_OPS,1,1},  /* before the total: a hidden chunk counts once */
    {MAPMAKER_TAG_OP,MAPMAKER_EDITOR_OPS_CAP,0,0},
    {MAPMAKER_TAG_RECORD,MAPMAKER_EDITOR_RECORDS_CAP,0,0},
};
#define MAPMAKER_EDITOR_CAPS (sizeof mapmaker_editor_caps/sizeof mapmaker_editor_caps[0])

extern void sub_00300670(void),sub_0031BB60(void),sub_0003D7B0(void),sub_0007A270(void),sub_003C85D4(void);

/* A hook that replaces its original must also do the original's `ret n`:
 * the lifted caller pushed a return address and checks esp afterwards. */
static void mapmaker_ret(uint32_t n) { g_esp+=4u+n; }

static uint32_t mapmaker_editor(void)
{
    uint32_t e=guest_writable(0x007844A8u,4)?WORD_AT(0x007844A8u):0u;
    return e && guest_writable(e,MAPMAKER_EDITOR_SIZE)?e:0u;
}

/* ---- hiding chunks past a capacity ----------------------------------- */
typedef struct MapMakerHidden { uint32_t chunk, tag; } MapMakerHidden;

static void mapmaker_unhide(MapMakerHidden *h,unsigned n)
{
    while(n--) WORD_AT(h[n].chunk)=h[n].tag;
}
/* Walk the {tag, size, data[size]} chunks from first to the end tag and hide
 * (tag 0, outside both loaders' tag switches, so skipped) every chunk past
 * the capacity of an entry it matches; run original; put the tags back. A
 * walk that leaves [first, limit_end) or is malformed hides nothing: the
 * original then sees exactly what it would have seen. */
static void mapmaker_hide_run(uint32_t first,uint32_t limit_end,const MapMakerCap *caps,
                              unsigned ncaps,void (*original)(void))
{
    MapMakerHidden *h=NULL;
    unsigned seen[8]={0},n=0,size=0,i,steps;
    uint32_t c=first;
    int ok=0;
    if(ncaps>8) ncaps=8;
    for(steps=0;steps<0x40000u;steps++) {
        uint32_t tag,sz;
        int hide=0;
        if(c<first || c>=limit_end || limit_end-c<8u || !guest_writable(c,8)) break;
        tag=WORD_AT(c);sz=WORD_AT(c+4);
        if(tag==MAPMAKER_TAG_END) { ok=1;break; }
        if(sz>limit_end-c-8u) break;
        for(i=0;i<ncaps && !hide;i++) {
            if(tag!=caps[i].tag) continue;
            if(caps[i].kind && (sz<5u || (WORD_AT(c+8u+4u)&0xFFu)!=caps[i].kind_value)) continue;
            if(++seen[i]>caps[i].capacity) hide=1;
        }
        if(hide) {
            if(n==size) {
                unsigned m=size?size*2u:64u;
                MapMakerHidden *p=(MapMakerHidden *)realloc(h,m*sizeof *p);
                if(!p) { ok=0;break; }
                h=p;size=m;
            }
            h[n].chunk=c;h[n].tag=tag;n++;
            WORD_AT(c)=0;
        }
        c+=8u+sz;
    }
    if(!ok) {
        mapmaker_unhide(h,n);
        free(h);
        original();
        return;
    }
    if(n) fprintf(stderr,"[mapmaker] %u map chunks past the editor/level capacity were skipped\n",n);
    original();
    mapmaker_unhide(h,n);
    free(h);
}

/* ---- editor map loader ------------------------------------------------ */
void pc_mapmaker_editor_load(void (*original)(void))
{
    uint32_t container=guest_writable(g_esp+4u,4)?WORD_AT(g_esp+4u):0u;
    if(!container || !guest_writable(container,8) || WORD_AT(container)!=0x00C77667u) {
        original();
        return;
    }
    mapmaker_hide_run(container+8u,container+MAPMAKER_CONTAINER_LIMIT,mapmaker_editor_caps,
                      MAPMAKER_EDITOR_CAPS,original);
}

/* ---- MapMaker level start (scene 100) --------------------------------- */
void pc_mapmaker_editor_level_load(void (*original)(void))
{
    static const MapMakerCap sai={MAPMAKER_TAG_SAI,MAPMAKER_LEVEL_SAI_SLOTS,0,0};
    uint32_t container=guest_writable(0x00784014u,4)?WORD_AT(0x00784014u):0u;
    if(!container || !guest_writable(container,8)) {
        original();
        return;
    }
    mapmaker_hide_run(container+8u,container+MAPMAKER_CONTAINER_LIMIT,&sai,1,original);
}

/* ---- add a story AI (sub_0031BE70) ------------------------------------ */
void pc_mapmaker_editor_sai_add(void (*original)(void))
{
    uint32_t editor,count,arg,limit,room,result;
    mapmaker_limits_read();
    editor=mapmaker_editor();
    if(!editor || !guest_writable(g_esp+4u,4)) { original();return; }
    limit=mapmaker_limits.limits?MAPMAKER_EDITOR_SAI_LIMIT:MAPMAKER_EDITOR_SAI_STOCK;
    arg=WORD_AT(g_esp+4u);
    count=WORD_AT(editor+0x280u);
    room=guest_call(sub_00300670,1,0x58u)!=0;          /* container space for one more */
    if((int32_t)count<(int32_t)limit && room) {
        result=guest_call(sub_0031BB60,2,0u,arg);
        count=WORD_AT(editor+0x280u);
        if(count>MAPMAKER_EDITOR_SAI_STOCK && count<=MAPMAKER_EDITOR_SAI_CAP) {
            uint32_t entry=editor+MAPMAKER_EDITOR_SAI+(count-1u)*0xE4u;
            if(WORD_AT(entry+0x60u)>=50u) WORD_AT(entry+0x60u)=(count-1u)%50u; /* colour, 0x0052CE00[50] */
        }
    } else if(!room) {
        result=guest_call(sub_0007A270,3,arg,guest_call(sub_0003D7B0,1,0x9B6u),0u);
    } else {
        guest_call(sub_003C85D4,3,editor,guest_call(sub_0003D7B0,1,0xBB0u),limit);
        result=guest_call(sub_0007A270,3,arg,editor,0u);
    }
    g_eax=result;
    mapmaker_ret(0);                                   /* sub_0031BE70: ret */
}

/* ---- logic: editor limits --------------------------------------------- */
static uint32_t mapmaker_ops_limit(void)
{
    mapmaker_limits_read();
    return mapmaker_limits.limits?MAPMAKER_EDITOR_OPS_CAP:MAPMAKER_OPS_STOCK;
}
static uint32_t mapmaker_objectives_limit(void)
{
    mapmaker_limits_read();
    return mapmaker_limits.limits?MAPMAKER_OBJECTIVES_LIMIT:MAPMAKER_OBJECTIVES_STOCK;
}
/* Kind-0 op maximum in the per-kind data table (read by sub_00322AC0 and
 * the message in sub_0031DF80). Raised only from its stock value. */
static void mapmaker_logic_kinds_apply(void)
{
    mapmaker_limits_read();
    if(mapmaker_limits.limits && guest_writable(0x0052E8E8u,4) && WORD_AT(0x0052E8E8u)==MAPMAKER_OPS_STOCK)
        WORD_AT(0x0052E8E8u)=MAPMAKER_EDITOR_OPS_CAP;
}

/* sub_00321E60: 1 when another op (and objective) may be added. */
void pc_mapmaker_editor_logic_can_add(void (*original)(void))
{
    uint32_t editor=mapmaker_editor();
    if(!editor) { original();return; }
    mapmaker_logic_kinds_apply();
    g_eax=(int32_t)WORD_AT(editor+0x41D8u)<(int32_t)mapmaker_objectives_limit() &&
          (int32_t)WORD_AT(editor+0x41E0u)<(int32_t)mapmaker_ops_limit();
    mapmaker_ret(0);                                   /* sub_00321E60: ret */
}
/* sub_00321E80(arg): the "logic full" message, with the limits in force.
 * Stock formats the objective limit, or objectives-1 when ops are full. */
void pc_mapmaker_editor_logic_full(void (*original)(void))
{
    uint32_t editor=mapmaker_editor(),arg,objectives;
    if(!editor || !guest_writable(g_esp+4u,4)) { original();return; }
    arg=WORD_AT(g_esp+4u);
    objectives=WORD_AT(editor+0x41D8u);
    if((int32_t)objectives>=(int32_t)mapmaker_objectives_limit())
        guest_call(sub_003C85D4,3,editor,guest_call(sub_0003D7B0,1,0xBB1u),mapmaker_objectives_limit());
    else if((int32_t)WORD_AT(editor+0x41E0u)>=(int32_t)mapmaker_ops_limit())
        guest_call(sub_003C85D4,3,editor,guest_call(sub_0003D7B0,1,0xBB1u),objectives-1u);
    g_eax=guest_call(sub_0007A270,3,arg,editor,0u);
    mapmaker_ret(0);                                   /* sub_00321E80: ret */
}
/* sub_00325F60: turn the selected mover into an objective (creates an op
 * and nodes). Its own test is raised to 40 by editor.json; refuse here at
 * the stock 10 when the limits are off, and at the ops capacity. The stock
 * refusal returns with eax = the editor pointer. */
void pc_mapmaker_editor_objective(void (*original)(void))
{
    uint32_t editor=mapmaker_editor();
    if(!editor) { original();return; }
    if((int32_t)WORD_AT(editor+0x41D8u)>=(int32_t)mapmaker_objectives_limit() ||
       WORD_AT(editor+0x41E0u)>=MAPMAKER_EDITOR_OPS_CAP ||
       WORD_AT(editor+0x81A4u)+MAPMAKER_LOGIC_HEADROOM>MAPMAKER_EDITOR_NODES_CAP ||
       WORD_AT(editor+0xDD10u)+MAPMAKER_LOGIC_HEADROOM>MAPMAKER_EDITOR_RECORDS_CAP) {
        g_eax=editor;
        mapmaker_ret(0);                               /* sub_00325F60: ret at 0x0032609D */
        return;
    }
    original();
}
/* sub_0031DB00(type): 1 while fewer than the per-type maximum nodes of
 * that type exist (stock: type 0 -> 30, types 1 and 2 -> 50, others 0). */
void pc_mapmaker_editor_node_room(void (*original)(void))
{
    uint32_t editor=mapmaker_editor(),type,count,n=0,i,cap;
    if(!editor || !guest_writable(g_esp+4u,4)) { original();return; }
    mapmaker_logic_kinds_apply();
    type=WORD_AT(g_esp+4u);
    count=WORD_AT(editor+0x81A4u);
    if(count>MAPMAKER_EDITOR_NODES_CAP) count=MAPMAKER_EDITOR_NODES_CAP;
    for(i=0;i<count;i++) n+=WORD_AT(editor+MAPMAKER_EDITOR_NODES+i*0xB4u)==type;
    cap=type==0u?30u:(type==1u || type==2u)?50u:0u;
    if(mapmaker_limits.limits) cap*=4u;
    g_eax=n<cap;
    mapmaker_ret(0);                                   /* sub_0031DB00: ret */
}

/* ---- logic: capacity backstop in the container-space test ------------- */
/* Return addresses of the calls to sub_00300670 from the logic editing
 * actions (sub_00324F50, _4FA0, _5040, _50C0, _5140, _6550, _8630, _86E0,
 * _87B0). */
static const uint32_t mapmaker_logic_room_callers[]={
    0x00324F70u,0x00324FC0u,0x00325060u,0x003250E0u,0x003251C7u,
    0x0032656Cu,0x0032865Bu,0x00328700u,0x003287D0u,
};
void pc_mapmaker_editor_logic_room(void (*original)(void))
{
    uint32_t editor,ret=guest_writable(g_esp,4)?WORD_AT(g_esp):0u;  /* before the original pops it */
    unsigned i;
    original();
    if(!g_eax || !(editor=mapmaker_editor())) return;
    mapmaker_logic_kinds_apply();
    for(i=0;i<sizeof mapmaker_logic_room_callers/sizeof mapmaker_logic_room_callers[0];i++)
        if(ret==mapmaker_logic_room_callers[i]) break;
    if(i==sizeof mapmaker_logic_room_callers/sizeof mapmaker_logic_room_callers[0]) return;
    if(WORD_AT(editor+0x41E0u)>=MAPMAKER_EDITOR_OPS_CAP ||
       WORD_AT(editor+0x81A4u)+MAPMAKER_LOGIC_HEADROOM>MAPMAKER_EDITOR_NODES_CAP ||
       WORD_AT(editor+0xDD10u)+MAPMAKER_LOGIC_HEADROOM>MAPMAKER_EDITOR_RECORDS_CAP)
        g_eax=0;
}

/* ---- play time: the 45-slot logic table (sub_0033F480) ----------------- */
void pc_mapmaker_editor_logic_slot(void (*original)(void))
{
    uint32_t obj=guest_writable(0x00765E44u,4)?WORD_AT(0x00765E44u):0u,arg,count,i;
    int32_t record;
    if(!obj || !guest_writable(obj,0x1960) || !guest_writable(g_esp+4u,4)) { original();return; }
    arg=WORD_AT(g_esp+4u);
    if(!guest_writable(arg,0x10)) { original();return; }
    count=WORD_AT(obj+0x1950u);
    if(count<MAPMAKER_LOGIC_SLOTS) { original();return; }
    record=(int32_t)(int16_t)(WORD_AT(arg+0xCu)&0xFFFFu);
    for(i=0;i<MAPMAKER_LOGIC_SLOTS;i++)
        if((int32_t)WORD_AT(obj+0x13Cu+i*0x8Cu)==record) { original();return; } /* already has a slot */
    {
        static unsigned reported;
        if(!reported++) fprintf(stderr,"[mapmaker] logic table full (%u slots): action skipped\n",MAPMAKER_LOGIC_SLOTS);
    }
    g_eax=0;
    mapmaker_ret(0);                                   /* sub_0033F480: ret */
}

/* ---- free-standing objects -------------------------------------------- */
static const uint32_t mapmaker_free_stock[3]={40u,30u,100u};

/* Run original with the per-type count at count_addr shown as stock-1 when
 * the opted-in limit allows more; then put back the real count plus what
 * the original added (it only compares and increments this word). */
static void mapmaker_free_shim(uint32_t count_addr,unsigned type,void (*original)(void))
{
    uint32_t word,count,shown;
    mapmaker_limits_read();
    if(type>2u || !mapmaker_limits.limits || !guest_writable(count_addr&~3u,4)) { original();return; }
    word=WORD_AT(count_addr&~3u);
    count=(count_addr&2u)?word>>16:word&0xFFFFu;
    if(count<mapmaker_free_stock[type] || count>=4u*mapmaker_free_stock[type]) { original();return; }
    shown=mapmaker_free_stock[type]-1u;
#define MAPMAKER_PUT16(a,v) (WORD_AT((a)&~3u)=((a)&2u)?(WORD_AT((a)&~3u)&0xFFFFu)|((uint32_t)(v)<<16):(WORD_AT((a)&~3u)&0xFFFF0000u)|((uint32_t)(v)&0xFFFFu))
    MAPMAKER_PUT16(count_addr,shown);
    original();
    word=WORD_AT(count_addr&~3u);
    word=(count_addr&2u)?word>>16:word&0xFFFFu;
    MAPMAKER_PUT16(count_addr,count+(word-shown));
#undef MAPMAKER_PUT16
}
/* sub_00304330: place the held item; free objects are ids 0x46 (type 0,
 * count +0x3068) and 0x47 (type 1, count +0x306A). */
void pc_mapmaker_editor_free_place(void (*original)(void))
{
    uint32_t editor=mapmaker_editor(),id;
    if(!editor) { original();return; }
    id=WORD_AT(editor+0x2F34u);
    if(id==0x46u) mapmaker_free_shim(editor+0x3068u,0,original);
    else if(id==0x47u) mapmaker_free_shim(editor+0x306Au,1,original);
    else original();
}
/* sub_002F7F30(eax=index, ecx=destination set, edx=source set): copy one
 * object between the main set and the clipboard; type byte at entry +7. */
void pc_mapmaker_editor_free_move(void (*original)(void))
{
    uint32_t entry=g_edx+8u+MAPMAKER_FREE_DELTA+g_eax*12u,type;
    if(!guest_writable(entry,12) || !guest_writable(g_ecx,8)) { original();return; }
    type=WORD_AT(entry+4u)>>24&0xFFu;
    mapmaker_free_shim(g_ecx+type*2u,type,original);
}
