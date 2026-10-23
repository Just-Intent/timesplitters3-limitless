/* US 45410066 MapMaker editor object: Story AI, logic and free-standing
 * object capacity (docs/mapmaker-editor.md).
 *
 * The editor object (sub_002FE360, pointer [0x007844A8], Level heap) packs
 * fixed arrays against each other. config/guest-code-patches/editor.json
 * grows the object from 0x11950 to MAPMAKER_EDITOR_SIZE bytes, keeps every
 * existing field where it is and rewrites every displacement that names an
 * old array so the arrays live after +0x11950:
 *
 *   array          stock               enlarged (offset, entries)
 *   Story AI       +0x288   50 x 0xE4  +0x11950 255
 *   (logic and free objects: later stages, see the doc)
 *
 * Counts stay where they were. This file holds the C policy around the
 * enlarged arrays:
 *
 *  - pc_mapmaker_editor_sai_add (WRAP sub_0031BE70): the editor's "add a
 *    story AI" action. Stock refuses at 50; with TSFP_MAPMAKER_LIMITS=1 the
 *    limit is MAPMAKER_EDITOR_SAI_LIMIT. Same placement-memory test, same
 *    messages (the "limit reached" text gets the limit in force). A new
 *    entry past the 50 editor colours gets colour (index mod 50) instead of
 *    the stock out-of-table value 50.
 *  - pc_mapmaker_editor_load (WRAP sub_00301A90): the editor map loader
 *    appends chunks to the arrays with no bound. Chunks past an array's
 *    capacity are hidden from the loader (tag 0, which its switch skips) and
 *    restored afterwards, so a large or damaged map cannot overflow the
 *    editor object. Capacity, not the opt-in limit, applies here.
 *  - pc_mapmaker_editor_level_load (WRAP sub_00240C50): a MapMaker map being
 *    started. Its count pass sub_00240590 keeps one stack slot per story AI
 *    chunk with no bound (stock 50 slots). The table grows its frame to
 *    MAPMAKER_LEVEL_SAI_SLOTS slots; chunks past that are hidden for the
 *    whole load the same way.
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

/* Capacities by chunk tag for the editor loader: tag, editor count field
 * (informational) and entries the editor arrays hold. Raised by later stages. */
#define MAPMAKER_TAG_FREE 0x00C7766Bu
#define MAPMAKER_TAG_SAI 0x00C7766Cu
#define MAPMAKER_TAG_NODE 0x00C7766Du
#define MAPMAKER_TAG_OP 0x00C7766Eu
#define MAPMAKER_TAG_RECORD 0x00C7766Fu
#define MAPMAKER_TAG_END 0x00C7766Au
#define MAPMAKER_CONTAINER_LIMIT 0x00080000u    /* scan bound: 512 KB */

static const struct { uint32_t tag, capacity; } mapmaker_editor_caps[]={
    {MAPMAKER_TAG_SAI,MAPMAKER_EDITOR_SAI_CAP},
    {MAPMAKER_TAG_FREE,170u},   /* stock arrays */
    {MAPMAKER_TAG_NODE,130u},
    {MAPMAKER_TAG_OP,30u},
    {MAPMAKER_TAG_RECORD,100u},
};
#define MAPMAKER_EDITOR_CAPS (sizeof mapmaker_editor_caps/sizeof mapmaker_editor_caps[0])

extern void sub_00300670(void),sub_0031BB60(void),sub_0003D7B0(void),sub_0007A270(void),sub_003C85D4(void);

/* ---- hiding chunks past a capacity ----------------------------------- */
typedef struct MapMakerHidden { uint32_t chunk, tag; } MapMakerHidden;

static void mapmaker_unhide(MapMakerHidden *h,unsigned n)
{
    while(n--) WORD_AT(h[n].chunk)=h[n].tag;
}
/* Walk the {tag, size, data[size]} chunks from first to the end tag and hide
 * (tag 0, outside both loaders' tag switches, so skipped) every chunk of a
 * capped tag past its capacity; run original; put the tags back. A walk that
 * leaves [first, limit_end) or is malformed hides nothing: the original then
 * sees exactly what it would have seen. */
static void mapmaker_hide_run(uint32_t first,uint32_t limit_end,const uint32_t *tags,
                              const uint32_t *caps,unsigned ntags,void (*original)(void))
{
    MapMakerHidden *h=NULL;
    unsigned seen[8]={0},n=0,size=0,i,steps;
    uint32_t c=first;
    int ok=0;
    if(ntags>8) ntags=8;
    for(steps=0;steps<0x40000u;steps++) {
        uint32_t tag,sz;
        if(c<first || c>=limit_end || limit_end-c<8u || !guest_writable(c,8)) break;
        tag=WORD_AT(c);sz=WORD_AT(c+4);
        if(tag==MAPMAKER_TAG_END) { ok=1;break; }
        if(sz>limit_end-c-8u) break;
        for(i=0;i<ntags;i++)
            if(tag==tags[i] && ++seen[i]>caps[i]) {
                if(n==size) {
                    unsigned m=size?size*2u:64u;
                    MapMakerHidden *p=(MapMakerHidden *)realloc(h,m*sizeof *p);
                    if(!p) { ok=0;goto done; }
                    h=p;size=m;
                }
                h[n].chunk=c;h[n].tag=tag;n++;
                WORD_AT(c)=0;
            }
        c+=8u+sz;
    }
done:
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
    uint32_t tags[MAPMAKER_EDITOR_CAPS],caps[MAPMAKER_EDITOR_CAPS],container;
    unsigned i;
    container=guest_writable(g_esp+4u,4)?WORD_AT(g_esp+4u):0u;
    if(!container || !guest_writable(container,8) || WORD_AT(container)!=0x00C77667u) {
        original();
        return;
    }
    for(i=0;i<MAPMAKER_EDITOR_CAPS;i++) { tags[i]=mapmaker_editor_caps[i].tag;caps[i]=mapmaker_editor_caps[i].capacity; }
    mapmaker_hide_run(container+8u,container+MAPMAKER_CONTAINER_LIMIT,tags,caps,MAPMAKER_EDITOR_CAPS,original);
}

/* ---- MapMaker level start (scene 100) --------------------------------- */
void pc_mapmaker_editor_level_load(void (*original)(void))
{
    const uint32_t tag=MAPMAKER_TAG_SAI,cap=MAPMAKER_LEVEL_SAI_SLOTS;
    uint32_t container=guest_writable(0x00784014u,4)?WORD_AT(0x00784014u):0u;
    if(!container || !guest_writable(container,8)) {
        original();
        return;
    }
    mapmaker_hide_run(container+8u,container+MAPMAKER_CONTAINER_LIMIT,&tag,&cap,1,original);
}

/* ---- add a story AI (sub_0031BE70) ------------------------------------ */
void pc_mapmaker_editor_sai_add(void (*original)(void))
{
    uint32_t editor,count,arg,limit,room,result;
    mapmaker_limits_read();
    if(!guest_writable(0x007844A8u,4) || !guest_writable(g_esp+4u,4)) { original();return; }
    editor=WORD_AT(0x007844A8u);
    if(!guest_writable(editor,MAPMAKER_EDITOR_SIZE)) { original();return; }
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
}
