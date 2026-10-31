/* US 45410066 MapMaker map validator (docs/mapmaker-bugfix.md, fix 4;
 * research: logs/limits/features/bugs.md section 4).
 *
 * The editor loader sub_00301A90 and the play-time passes sub_00240590 /
 * sub_00240C50 trust every count, index and size in a map container. Map
 * files can be shared (their signature key is zero), so a crafted or damaged
 * file overflows heap, stack and globals. This validator walks the container
 * as the loaders do and checks ONLY what would make a loader read or write
 * outside its tables, buffers or frame. It never looks at tile positions,
 * floors, rotations, overlaps, items per tile or anything else the editor or
 * its glitches (tile merging, 7th floor, sky windows, stacked items, copying
 * past the meter) can produce; every bound below is at or above what the
 * editor can write.
 *
 *   chunk         field                         bound      why (CONFIRMED)
 *   walk          sizes, end tag 0xC7766A       inside the 512 KB container; the loader's walk has no bound
 *   0x68 header   tileset word +0xA             < 7        catalogue 0x00524F64 + (id*7 + tileset)*0x1C
 *                 word count byte +0x42         <= 17      editor +0x1190C dwords up to the stock end +0x11950
 *                 piece count dword +4          <= 1024    editor: piece buffer sub_0003DFA0(0x1000) @0x00301BCA, piece pool 0x400
 *                                               <= 505     level: sub_00240C50 frame array [esp+i*8+0xA0] (0x1030 frame)
 *                 string +0x162 (level)         NUL < 0xE0 strcpy @0x00240DE2 (the editor loader forces it, @0x00301FB5)
 *   0x69 pieces   piece records (count above)   inside the chunk; tile id < 45 (tables 0x00529400, 0x00783F40, 0x00785000)
 *                 items word +0x1E, each item   inside the chunk; item id (word +0xE) 0..79 (catalogue,
 *                                               category callback sub_002F7770, jump bound 0x0024069B)
 *                 all piece chunks (editor)     <= 1024 pieces (pool); before any header none is read
 *   0x6C story AI patrol count word +0x2C       <= 8       16 B per point from record +0x64 (0xE4 B) @0x00302450
 *                 name (editor)                 NUL < 0x24 strcpy @0x003024E0, fields from +0x24
 *   0x6F logic    type 3: count +0x20           <= 13      8 B per entry from +0x24 in the 0x8C record @0x00302780
 *                 type 0xA: count +0x70         <= 10      entries from +0x20 reach the count itself at the 11th
 *                 entry index +0x24+8k          buffer     [buf + idx*4] (type 3) / [buf + (idx-1)*4] (type 0xA), 1024 slots
 *   0x73          size                          <= 0x4B00  copy then zero-fill 0x4B00-size @0x00302886
 *   0x74          size                          <= 2000    [esp+0x78], 0x1F4 dwords @0x003028E0
 *   0x75          size                          <= 3000    [esp+0x848], 0x2EE dwords, ends at the frame top @0x00302925
 *
 * Indices are checked against the buffer, not the pieces present: the saver
 * writes the list length for a piece it cannot find (@0x0030177E), which the
 * stock loader reads from the buffer without harm.
 *
 * - pc_mapmaker_validate_editor_load(container, original): called first by
 *   pc_mapmaker_editor_load (WRAP sub_00301A90). A bad map is refused the
 *   stock way: the container's magic is cleared and the original takes its
 *   own magic-mismatch path (@0x003029C7: sub_002FEBF0(0), return 0), which
 *   also leaves the game's copy unusable, as a corrupt file would. Returns 1
 *   when the caller should go on.
 * - pc_mapmaker_validate_level (WRAP sub_0006D7A0, level setup): in scene 100
 *   before the one-time map pass (sub_00240C50 runs when [0x006F32EC] == 0).
 *   There is no stock refusal at that point, so a bad chunk is hidden (tag 0,
 *   skipped by every walker) for the call; a bad header hides the header and
 *   every piece chunk (their count comes from it); a malformed walk ends the
 *   map there (end tag). Tags are put back afterwards.
 *
 * Requires the includer's WORD_AT, guest_writable, guest_call,
 * g_xbox_mem_offset; calls sub_00333700 / sub_002FDF50 (a tile's pad
 * counts, read from the static tile table). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAPMAKER_VALIDATE_MAGIC 0x00C77667u
#define MAPMAKER_VALIDATE_SPAN 0x00080000u      /* container.json: 512 KB (stock 0x8000) */
#define MAPMAKER_VALIDATE_SPAN_STOCK 0x00008000u
#define MAPMAKER_VALIDATE_TAG(n) (0x00C77600u+(uint32_t)(n))
#define MAPMAKER_VALIDATE_END MAPMAKER_VALIDATE_TAG(0x6A)
#define MAPMAKER_VALIDATE_STEPS 0x40000u
#define MAPMAKER_VALIDATE_TILES 45u             /* rows 0..44 of 0x00529400 (+0x110 = type+1) */
#define MAPMAKER_VALIDATE_ITEMS 80u             /* 0x0024069B cmp eax,0x4F / ja */
#define MAPMAKER_VALIDATE_TILESETS 7u
#define MAPMAKER_VALIDATE_HEADER_WORDS 17u
#define MAPMAKER_VALIDATE_EDITOR_PIECES 1024u
#define MAPMAKER_VALIDATE_LEVEL_PIECES 505u     /* (0x1030-0xA0)/8 */
#define MAPMAKER_VALIDATE_BUFFER 1024u
#define MAPMAKER_VALIDATE_PATROL 8u
#define MAPMAKER_VALIDATE_SAI_NAME 0x24u
#define MAPMAKER_VALIDATE_TITLE 0xE0u           /* +0x162 .. +0x241 */
#define MAPMAKER_VALIDATE_LINKS3 13u
#define MAPMAKER_VALIDATE_LINKSA 10u
#define MAPMAKER_VALIDATE_73 0x4B00u
#define MAPMAKER_VALIDATE_74 2000u
#define MAPMAKER_VALIDATE_75 3000u
#define MAPMAKER_VALIDATE_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

enum { MAPMAKER_VALIDATE_EDITOR=1, MAPMAKER_VALIDATE_LEVEL=2 };

extern void sub_00333700(void),sub_002FDF50(void);

typedef struct MapMakerValidate {
    uint32_t base,end;          /* container, container + span */
    int mode;
    int header;                 /* a header chunk was seen */
    int32_t pieces;             /* piece count of the last header */
    uint32_t total;             /* pieces in the piece chunks so far */
    uint32_t value;             /* the offending value */
    int hide_pieces;            /* level: header refused, so are its pieces */
} MapMakerValidate;

typedef struct MapMakerValidateEdit { uint32_t address, old, value; } MapMakerValidateEdit;

static unsigned mapmaker_validate_refused;

/* Bytes [a, a+n) lie inside the container. */
static int mapmaker_validate_in(const MapMakerValidate *v,uint32_t a,uint32_t n)
{
    return a>=v->base && a<=v->end && n<=v->end-a;
}
static uint32_t mapmaker_validate_u16(uint32_t a)
{
    return (uint32_t)MAPMAKER_VALIDATE_B(a)|(uint32_t)MAPMAKER_VALIDATE_B(a+1u)<<8;
}
static int mapmaker_validate_nul(uint32_t a,uint32_t n)
{
    uint32_t i;
    for(i=0;i<n;i++) if(!MAPMAKER_VALIDATE_B(a+i)) return 1;
    return 0;
}

static const char *mapmaker_validate_pieces(MapMakerValidate *v,uint32_t p,uint32_t end)
{
    int32_t i;
    uint32_t j;
    if(!v->header) return NULL;                   /* count -1 (editor @0x00301AC9) / 0 (level): not read */
    v->total+=v->pieces>0?(uint32_t)v->pieces:0u;
    if(v->mode==MAPMAKER_VALIDATE_EDITOR && v->total>MAPMAKER_VALIDATE_EDITOR_PIECES) { v->value=v->total;return "pieces past the piece pool"; }
    for(i=0;i<v->pieces;i++) {
        uint32_t id,pads,items;
        if(p>end || end-p<0x28u) { v->value=(uint32_t)i;return "piece records past the piece chunk"; }
        id=WORD_AT(p);
        if(id>=MAPMAKER_VALIDATE_TILES) { v->value=id;return "tile id"; }
        pads=guest_call(sub_00333700,1,id)+guest_call(sub_002FDF50,1,id);
        items=mapmaker_validate_u16(p+0x1Eu);
        if(pads>(end-p-0x28u)/8u) { v->value=(uint32_t)i;return "piece pads past the piece chunk"; }
        p+=0x28u+pads*8u;
        for(j=0;j<items;j++) {
            int32_t item;
            uint32_t size;
            if(end-p<0x18u) { v->value=(uint32_t)i;return "items past the piece chunk"; }
            item=(int32_t)(int16_t)mapmaker_validate_u16(p+0xEu);
            if(item<0 || item>=(int32_t)MAPMAKER_VALIDATE_ITEMS) { v->value=(uint32_t)item;return "item id"; }
            size=((uint32_t)MAPMAKER_VALIDATE_B(p+0x14u)*4u+0x1Bu)&~3u;
            if(size>end-p) { v->value=(uint32_t)i;return "items past the piece chunk"; }
            p+=size;
        }
    }
    return NULL;
}

static const char *mapmaker_validate_record(MapMakerValidate *v,uint32_t d)
{
    uint32_t type=WORD_AT(d+0x80u),k;
    int32_t n,lo,hi;
    if(type==3u) { n=(int32_t)WORD_AT(d+0x20u);lo=0;hi=(int32_t)MAPMAKER_VALIDATE_BUFFER-1; }
    else if(type==0xAu) { n=(int32_t)WORD_AT(d+0x70u);lo=1;hi=(int32_t)MAPMAKER_VALIDATE_BUFFER; }
    else return NULL;
    if(n>(int32_t)(type==3u?MAPMAKER_VALIDATE_LINKS3:MAPMAKER_VALIDATE_LINKSA)) { v->value=(uint32_t)n;return "logic link count"; }
    for(k=0;(int32_t)k<n;k++) {
        int32_t idx=(int32_t)WORD_AT(d+0x24u+8u*k);
        if(idx<lo || idx>hi) { v->value=(uint32_t)idx;return "logic link index"; }
    }
    return NULL;
}

/* One chunk (data d, size sz) as the loader of v->mode reads it; NULL = fine. */
static const char *mapmaker_validate_chunk(MapMakerValidate *v,uint32_t tag,uint32_t d,uint32_t sz)
{
    static const uint16_t need[0x0E]={0x3DC,0,0,10,0x64,0xB4,0x220,0x8C,0x20,0,0x100,0,0,0};
    int editor=v->mode==MAPMAKER_VALIDATE_EDITOR;
    uint32_t t=tag-MAPMAKER_VALIDATE_TAG(0x68);
    if(t>=0x0Eu) return NULL;                      /* not a loader tag: skipped by both */
    if(!mapmaker_validate_in(v,d,need[t])) { v->value=sz;return "chunk read past the container"; }
    switch(t+0x68u) {
    case 0x68: {
        int32_t pieces=(int32_t)WORD_AT(d+4u);
        uint32_t cap=editor?MAPMAKER_VALIDATE_EDITOR_PIECES:MAPMAKER_VALIDATE_LEVEL_PIECES;
        v->header=1;v->pieces=pieces;
        if(mapmaker_validate_u16(d+0xAu)>=MAPMAKER_VALIDATE_TILESETS) { v->value=mapmaker_validate_u16(d+0xAu);return "tileset"; }
        if(pieces>(int32_t)cap) { v->value=(uint32_t)pieces;return "header piece count"; }
        if(editor && MAPMAKER_VALIDATE_B(d+0x42u)>MAPMAKER_VALIDATE_HEADER_WORDS) { v->value=MAPMAKER_VALIDATE_B(d+0x42u);return "header word count"; }
        if(!editor && !mapmaker_validate_nul(d+0x162u,MAPMAKER_VALIDATE_TITLE)) { v->value=0;return "header string"; }
        return NULL;
    }
    case 0x69: return mapmaker_validate_pieces(v,d,d+sz);
    case 0x6C:
        if(mapmaker_validate_u16(d+0x2Cu)>MAPMAKER_VALIDATE_PATROL) { v->value=mapmaker_validate_u16(d+0x2Cu);return "story AI patrol count"; }
        if(editor && !mapmaker_validate_nul(d,MAPMAKER_VALIDATE_SAI_NAME)) { v->value=0;return "story AI name"; }
        return NULL;
    case 0x6F: return editor?mapmaker_validate_record(v,d):NULL;
    case 0x73: if(editor && sz>MAPMAKER_VALIDATE_73) { v->value=sz;return "chunk 0x73 size"; } return NULL;
    case 0x74: if(editor && sz>MAPMAKER_VALIDATE_74) { v->value=sz;return "chunk 0x74 size"; } return NULL;
    case 0x75: if(editor && sz>MAPMAKER_VALIDATE_75) { v->value=sz;return "chunk 0x75 size"; } return NULL;
    default: return NULL;
    }
}

static void mapmaker_validate_report(const char *where,const char *why,uint32_t chunk,uint32_t base,uint32_t value)
{
    if(mapmaker_validate_refused++<4u) {
        fprintf(stderr,"[mapmaker] %s: %s (chunk +0x%X, value %d)\n",where,why,(unsigned)(chunk-base),(int)value);
        fflush(stderr);
    }
}

/* Walk [base+8, base+span). Editor mode: stop at the first problem and return
 * its chunk (0 = valid). Level mode: append the tag rewrites to *edits and go
 * on; returns the number of problems. */
static uint32_t mapmaker_validate_walk(MapMakerValidate *v,uint32_t base,uint32_t span,int mode,
                                       MapMakerValidateEdit **edits,unsigned *nedits)
{
    uint32_t c=base+8u,steps,problems=0,size=0;
    memset(v,0,sizeof *v);
    v->base=base;v->end=base+span;v->mode=mode;
#define MAPMAKER_VALIDATE_EDIT(a,val) do { \
        if(*nedits==size) { unsigned m_=size?size*2u:16u; \
            MapMakerValidateEdit *e_=(MapMakerValidateEdit *)realloc(*edits,m_*sizeof *e_); \
            if(!e_) return problems+1u; *edits=e_;size=m_; } \
        (*edits)[*nedits].address=(a);(*edits)[*nedits].old=WORD_AT(a);(*edits)[*nedits].value=(val);(*nedits)++; } while(0)
    for(steps=0;steps<MAPMAKER_VALIDATE_STEPS;steps++) {
        uint32_t tag,sz;
        const char *why;
        if(!mapmaker_validate_in(v,c,8u) || !mapmaker_validate_in(v,c+8u,WORD_AT(c+4u))) {
            if(mapmaker_validate_in(v,c,8u)) v->value=WORD_AT(c+4u); else v->value=c-base;
            if(mode==MAPMAKER_VALIDATE_EDITOR) { mapmaker_validate_report("map refused","malformed chunk walk",c,base,v->value);return c?c:1u; }
            if(mapmaker_validate_in(v,c,4u)) {
                mapmaker_validate_report("map cut at level start","malformed chunk walk",c,base,v->value);
                MAPMAKER_VALIDATE_EDIT(c,MAPMAKER_VALIDATE_END);
            }
            return problems+1u;
        }
        tag=WORD_AT(c);sz=WORD_AT(c+4u);
        if(tag==MAPMAKER_VALIDATE_END) return mode==MAPMAKER_VALIDATE_EDITOR?0u:problems;
        if(mode==MAPMAKER_VALIDATE_LEVEL && v->hide_pieces && tag==MAPMAKER_VALIDATE_TAG(0x69)) {
            MAPMAKER_VALIDATE_EDIT(c,0u);
        } else if((why=mapmaker_validate_chunk(v,tag,c+8u,sz))!=NULL) {
            if(mode==MAPMAKER_VALIDATE_EDITOR) { mapmaker_validate_report("map refused",why,c,base,v->value);return c; }
            mapmaker_validate_report("map chunk skipped at level start",why,c,base,v->value);
            problems++;
            MAPMAKER_VALIDATE_EDIT(c,0u);
            if(tag==MAPMAKER_VALIDATE_TAG(0x68)) v->hide_pieces=1;
        }
        c+=8u+sz;
    }
#undef MAPMAKER_VALIDATE_EDIT
    v->value=steps;
    if(mode==MAPMAKER_VALIDATE_EDITOR) { mapmaker_validate_report("map refused","no end tag",c,base,steps);return c?c:1u; }
    return problems+1u;
}

/* How much of the container the validator may read: 0 when not even the
 * stock size is guest memory (the caller then leaves everything as it was). */
static uint32_t mapmaker_validate_span(uint32_t container)
{
    if(guest_writable(container,MAPMAKER_VALIDATE_SPAN)) return MAPMAKER_VALIDATE_SPAN;
    if(guest_writable(container,MAPMAKER_VALIDATE_SPAN_STOCK)) return MAPMAKER_VALIDATE_SPAN_STOCK;
    return 0;
}

/* ---- editor loader ----------------------------------------------------- */
int pc_mapmaker_validate_editor_load(uint32_t container,void (*original)(void))
{
    MapMakerValidate v;
    MapMakerValidateEdit *edits=NULL;
    unsigned n=0;
    uint32_t span=mapmaker_validate_span(container);
    if(!span || WORD_AT(container)!=MAPMAKER_VALIDATE_MAGIC) return 1;
    if(!mapmaker_validate_walk(&v,container,span,MAPMAKER_VALIDATE_EDITOR,&edits,&n)) { free(edits);return 1; }
    free(edits);
    WORD_AT(container)=0;                          /* sub_00301A90 @0x00301BAC: magic mismatch */
    original();
    return 0;
}

/* ---- level setup (scene 100) ------------------------------------------- */
void pc_mapmaker_validate_level(void (*original)(void))
{
    MapMakerValidate v;
    MapMakerValidateEdit *edits=NULL;
    unsigned n=0,i;
    uint32_t container,span;
    if(!guest_writable(0x0079094Cu,4) || WORD_AT(0x0079094Cu)!=100u || WORD_AT(0x006F32ECu) ||
       !(container=WORD_AT(0x00784014u)) || !(span=mapmaker_validate_span(container))) {
        original();
        return;
    }
    mapmaker_validate_walk(&v,container,span,MAPMAKER_VALIDATE_LEVEL,&edits,&n);
    for(i=0;i<n;i++) WORD_AT(edits[i].address)=edits[i].value;
    original();
    while(n--) if(WORD_AT(edits[n].address)==edits[n].value) WORD_AT(edits[n].address)=edits[n].old;
    free(edits);
}
