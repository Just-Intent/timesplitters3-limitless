/* US 45410066 MapMaker editor resource table (area restable,
 * docs/mapmaker/mapmaker-restable.md).
 *
 * The stock editor shows one capacity meter: sub_0030C210 (cdecl, x and y
 * floats; called from sub_0030C3C0 at 0x0030C42E with the top-left of the
 * safe area, itself from the editor HUD sub_003111F0 at 0x0031150C and
 * 0x00311743) sets font 0x12 (0x0030C216), draws label text 0xA7F with
 * sub_00041E30(x, y, text, colour, 0x80) and an 80 x 16 bar of
 * 1 - max(fractions) beside it (sub_0001EFB0 backdrop, sub_000814B0 bar).
 * With the raised limits that bar is the fullest of eight caps and says
 * nothing about which one.
 *
 * While TSFP_MAPMAKER_LIMITS=1 is in force this wrapper of sub_0030C210 (the
 * existing manual wrapper from patches/*-mapmaker-meter-probe.patch, so no
 * guest regeneration) draws a table instead, one outlined row per resource,
 * "Title: used/max", at the meter's own position and in its font:
 *
 *   Tiles, Items, Engine objects, Regular pads, Cover pads, Links, Pad
 *   groups, Memory (KB): the placed counts and limits of the editor's own
 *   check, pc_display_mapmaker_limits (mapmaker_limits_view, written by the
 *   same frame's sub_002FA720 wrapper); a row is red when that check refuses
 *   the held piece on its cap (placed + held), as the stock meter label.
 *   Pickups: placed pickups of categories 8-13, 19-21, 31 against the
 *   768 placed-pickup records of play (MAPMAKER_OBJECTS_PICKUPS; the editor
 *   item cap 500 keeps it below).
 *   Movers, Props, Turrets (27-29), Teleporters (14-18): the per-category
 *   counts sub_00303460 compares (sub_00308420 over every placed piece's
 *   items, category int16 at catalogue 0x00524F58 + id * 0xC4 + 2) against
 *   the live maxima at 0x00524BE0 + c * 24 + 4; groups add their members and
 *   are red when any member is at its maximum (that type is refused).
 *   Story AI (editor+0x280, MAPMAKER_EDITOR_SAI_LIMIT, sub_0031BE70) and
 *   logic ops (editor+0x41E0, MAPMAKER_EDITOR_OPS_CAP, sub_00321E60).
 *
 * Colours: normal 0x6E6E6E7F (the stock label colour), amber from 90%, red
 * at the maximum or when refused, blinking like the stock refused label
 * (0x80500080 / 0x80800080 on [0x007497B0] bit 0x20, 0x0030C2CA).
 *
 * Always visible while the limits are on: it replaces an always-visible
 * meter, and every pad button already has an editor action. Layout is in
 * guest 640 x 480 units, so it shares the editor HUD's 4:3 box on a wide
 * output (docs/mapmaker/mapmaker-widescreen.md) and stays in the left third
 * of the canvas (pc_ui_side_anchor, checked by the fixture). The stock
 * meter is drawn instead (and the meter probe keeps its scope) when the
 * limits are off, the check has not run for this editor,
 * TSFP_MAPMAKER_TABLE=0, probe flag 1 is set or the text buffer is missing.
 * Game state is only read; the guest writes are its own text buffer
 * (xbox_HeapAlloc) and the text alignment word [0x007E7260], zeroed around
 * the draw and put back (as pc_keyboard_menu.inc does). Replacing the
 * original, it pops the return address (MAPMAKER_GUEST_RET).
 *
 * Requires the includer's WORD_AT, FLOAT_AT, guest_call,
 * save_registers/restore_registers, g_esp, g_xbox_mem_offset, xbox_HeapAlloc,
 * mapmaker_probe_meter (pc_mapmaker_probe.inc with MAPMAKER_RESTABLE),
 * mapmaker_limits/_view (pc_mapmaker_limits.inc), MAPMAKER_EDITOR_* and
 * MAPMAKER_STACK_OK (pc_mapmaker_editor.inc), MAPMAKER_OBJECTS_PICKUPS
 * (pc_mapmaker_objects.inc). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void sub_0003FA10(void),sub_00041E30(void);
extern uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment);

#define MAPMAKER_TABLE_ROWS 15u
#define MAPMAKER_TABLE_TEXT 64u             /* bytes per row text */
#define MAPMAKER_TABLE_FONT 0x12u           /* the meter's font (0x0030C214) */
#define MAPMAKER_TABLE_SCALE 75             /* \vs text scale, percent */
#define MAPMAKER_TABLE_LINE 12.0f           /* font 0x12 is ~16 units tall at 100% */
#define MAPMAKER_TABLE_GLYPH 6.5f           /* estimated advance at 75% (unmeasured) */
#define MAPMAKER_TABLE_CANVAS_W 640.0f
#define MAPMAKER_TABLE_CANVAS_H 480.0f
#define MAPMAKER_TABLE_MARGIN 8.0f          /* kept clear of the canvas edges */
#define MAPMAKER_TABLE_OUTLINE 0x00000080u  /* stock label outline (0x0030C348) */
#define MAPMAKER_TABLE_OK 0x6E6E6E7Fu       /* stock label colour (0x0030C2BE) */
#define MAPMAKER_TABLE_NEAR 0x80680080u     /* amber, 90% and over */
#define MAPMAKER_TABLE_FULL 0x80500080u     /* stock refused label (0x0030C2D1) */
#define MAPMAKER_TABLE_BLINK 0x80800080u    /* its blink (0x0030C2D8) */
#define MAPMAKER_TABLE_BLINK_WORD 0x007497B0u
#define MAPMAKER_TABLE_ALIGN_WORD 0x007E7260u
#define MAPMAKER_TABLE_CATEGORIES 0x00524BE0u /* 37 x 24 B, max at +4 */
#define MAPMAKER_TABLE_CATEGORY_N 37u
#define MAPMAKER_TABLE_CATALOGUE 0x00524F58u  /* 80 items x 0xC4 B, category int16 at +2 */
#define MAPMAKER_TABLE_CATALOGUE_N 80u
#define MAPMAKER_TABLE_KB 1024u

enum { MAPMAKER_TABLE_STATE_OK, MAPMAKER_TABLE_STATE_NEAR, MAPMAKER_TABLE_STATE_FULL };
typedef struct MapMakerTableRow { const char *title, *unit; uint32_t used, max; unsigned state; } MapMakerTableRow;
/* Placed items per category (sub_00308420's counts) and the live maxima. */
typedef struct MapMakerTableCategories { uint32_t count[MAPMAKER_TABLE_CATEGORY_N], max[MAPMAKER_TABLE_CATEGORY_N]; } MapMakerTableCategories;

static struct { int initialized, enabled; uint32_t text; } mapmaker_table;

/* Categories of the rows (pc_mapmaker_limits.inc mapmaker_categories). */
static const unsigned mapmaker_table_pickups[]={8u,9u,10u,11u,12u,13u,19u,20u,21u,31u};
static const unsigned mapmaker_table_turrets[]={27u,28u,29u};
static const unsigned mapmaker_table_teleporters[]={14u,15u,16u,17u,18u};
static const unsigned mapmaker_table_movers[]={25u};
static const unsigned mapmaker_table_props[]={26u};

static unsigned mapmaker_table_state(uint32_t used,uint32_t max,int refused)
{
    if(refused || used>=max) return MAPMAKER_TABLE_STATE_FULL;
    return (uint64_t)used*10u>=(uint64_t)max*9u?MAPMAKER_TABLE_STATE_NEAR:MAPMAKER_TABLE_STATE_OK;
}

/* sub_00308420 for every placed piece: one count per item of the piece's
 * list, by the category of its catalogue entry. */
static void mapmaker_table_categories(uint32_t piece,MapMakerTableCategories *c)
{
    unsigned i,j;
    memset(c,0,sizeof *c);
    for(i=0;i<MAPMAKER_TABLE_CATEGORY_N;i++) {
        uint32_t a=MAPMAKER_TABLE_CATEGORIES+i*24u+4u;
        c->max[i]=MAPMAKER_PTR_OK(a,4)?WORD_AT(a):0u;
    }
    for(i=0;piece && i<4096 && MAPMAKER_PTR_OK(piece,0x44);i++,piece=WORD_AT(piece+0x40)) {
        uint32_t sub=WORD_AT(piece+0x3c);
        for(j=0;sub && j<4096 && MAPMAKER_PTR_OK(sub,0x18);j++,sub=WORD_AT(sub+0x14)) {
            uint32_t id=WORD_AT(sub),row=MAPMAKER_TABLE_CATALOGUE+id*0xC4u;
            int32_t cat;
            if(id>=MAPMAKER_TABLE_CATALOGUE_N || !MAPMAKER_PTR_OK(row,4)) continue;
            cat=(int32_t)(int16_t)(WORD_AT(row)>>16);
            if(cat>=0 && (uint32_t)cat<MAPMAKER_TABLE_CATEGORY_N) c->count[cat]++;
        }
    }
}

/* A row over categories: the sums, and refused when any member is full. */
static MapMakerTableRow mapmaker_table_group(const char *title,const MapMakerTableCategories *c,
                                             const unsigned *cats,unsigned n)
{
    MapMakerTableRow r={title,"",0,0,0};
    int full=0;
    unsigned i;
    for(i=0;i<n;i++) {
        r.used+=c->count[cats[i]];r.max+=c->max[cats[i]];
        if(c->count[cats[i]]>=c->max[cats[i]]) full=1;
    }
    r.state=mapmaker_table_state(r.used,r.max,full);
    return r;
}

/* The table's rows from what the editor's checks read. */
static unsigned mapmaker_table_rows(const MapMakerLimitsView *v,const MapMakerTableCategories *c,
                                    uint32_t story_ai,uint32_t logic_ops,MapMakerTableRow *rows)
{
    unsigned n=0,i;
    uint32_t pickups=0,groups=v->groups>0?(uint32_t)v->groups:0u;
    uint32_t memory_kb=(uint32_t)(((uint64_t)v->memory+MAPMAKER_TABLE_KB-1u)/MAPMAKER_TABLE_KB);
#define MAPMAKER_TABLE_ROW(t,u,m,bit) do { rows[n].title=(t);rows[n].unit="";rows[n].used=(u);rows[n].max=(m); \
        rows[n].state=mapmaker_table_state((u),(m),(v->over&(bit))!=0);n++; } while(0)
    MAPMAKER_TABLE_ROW("Tiles",v->pieces,MAPMAKER_LIMIT_PIECES,MAPMAKER_CAP_PIECES);
    MAPMAKER_TABLE_ROW("Items",v->items,MAPMAKER_LIMIT_SUB,MAPMAKER_CAP_SUB);
    MAPMAKER_TABLE_ROW("Engine objects",v->objects,MAPMAKER_LIMIT_OBJECTS,MAPMAKER_CAP_OBJECTS);
    for(i=0;i<sizeof mapmaker_table_pickups/sizeof mapmaker_table_pickups[0];i++)
        pickups+=c->count[mapmaker_table_pickups[i]];
    MAPMAKER_TABLE_ROW("Pickups",pickups,MAPMAKER_OBJECTS_PICKUPS,0u);
    MAPMAKER_TABLE_ROW("Regular pads",v->pads,MAPMAKER_LIMIT_PADS,MAPMAKER_CAP_PADS);
    MAPMAKER_TABLE_ROW("Cover pads",v->cover,MAPMAKER_LIMIT_COVER,MAPMAKER_CAP_COVER);
    MAPMAKER_TABLE_ROW("Links",v->links,MAPMAKER_LIMIT_LINKS,MAPMAKER_CAP_LINKS);
    MAPMAKER_TABLE_ROW("Pad groups",groups,(uint32_t)MAPMAKER_LIMIT_GROUPS,MAPMAKER_CAP_GROUPS);
    MAPMAKER_TABLE_ROW("Memory",memory_kb,MAPMAKER_LIMIT_MEMORY/MAPMAKER_TABLE_KB,MAPMAKER_CAP_MEMORY);
    rows[n-1].unit=" KB";
    MAPMAKER_TABLE_ROW("Story AI",story_ai,MAPMAKER_EDITOR_SAI_LIMIT,0u);
    MAPMAKER_TABLE_ROW("Logic",logic_ops,MAPMAKER_EDITOR_OPS_CAP,0u);
#undef MAPMAKER_TABLE_ROW
    rows[n++]=mapmaker_table_group("Movers",c,mapmaker_table_movers,1);
    rows[n++]=mapmaker_table_group("Props",c,mapmaker_table_props,1);
    rows[n++]=mapmaker_table_group("Turrets",c,mapmaker_table_turrets,3);
    rows[n++]=mapmaker_table_group("Teleporters",c,mapmaker_table_teleporters,5);
    return n;
}

/* Row origins: the meter's position, moved inside the canvas if the table
 * would leave it. Returns the estimated right edge. */
static float mapmaker_table_layout(float x,float y,unsigned n,unsigned widest,float *xs,float *ys)
{
    float w=(float)widest*MAPMAKER_TABLE_GLYPH,h=(float)n*MAPMAKER_TABLE_LINE;
    unsigned i;
    if(!isfinite(x)) x=MAPMAKER_TABLE_MARGIN;
    if(!isfinite(y)) y=MAPMAKER_TABLE_MARGIN;
    if(x+w>MAPMAKER_TABLE_CANVAS_W-MAPMAKER_TABLE_MARGIN) x=MAPMAKER_TABLE_CANVAS_W-MAPMAKER_TABLE_MARGIN-w;
    if(y+h>MAPMAKER_TABLE_CANVAS_H-MAPMAKER_TABLE_MARGIN) y=MAPMAKER_TABLE_CANVAS_H-MAPMAKER_TABLE_MARGIN-h;
    if(x<MAPMAKER_TABLE_MARGIN) x=MAPMAKER_TABLE_MARGIN;
    if(y<MAPMAKER_TABLE_MARGIN) y=MAPMAKER_TABLE_MARGIN;
    for(i=0;i<n;i++) { xs[i]=x;ys[i]=y+(float)i*MAPMAKER_TABLE_LINE; }
    return x+w;
}

static uint32_t mapmaker_table_colour(unsigned state)
{
    if(state==MAPMAKER_TABLE_STATE_FULL)
        return MAPMAKER_PTR_OK(MAPMAKER_TABLE_BLINK_WORD,4) && (WORD_AT(MAPMAKER_TABLE_BLINK_WORD)&0x20u)?
            MAPMAKER_TABLE_BLINK:MAPMAKER_TABLE_FULL;
    return state==MAPMAKER_TABLE_STATE_NEAR?MAPMAKER_TABLE_NEAR:MAPMAKER_TABLE_OK;
}
static uint32_t mapmaker_table_bits(float f) { uint32_t u;memcpy(&u,&f,4);return u; }

static int mapmaker_table_enabled(void)
{
    if(!mapmaker_table.initialized) {
        const char *v=getenv("TSFP_MAPMAKER_TABLE");
        mapmaker_table.initialized=1;
        mapmaker_table.enabled=!(v && v[0]=='0' && !v[1]);
    }
    return mapmaker_table.enabled;
}

/* WRAP sub_0030C210 (cdecl x, y; plain ret): the editor capacity meter. */
void pc_display_mapmaker_meter(void (*original)(void))
{
    MapMakerTableRow rows[MAPMAKER_TABLE_ROWS];
    MapMakerTableCategories cats;
    GuestRegisters r;
    char line[MAPMAKER_TABLE_TEXT];
    float xs[MAPMAKER_TABLE_ROWS],ys[MAPMAKER_TABLE_ROWS];
    uint32_t editor,font,align;
    unsigned n,i,widest=0;
    mapmaker_limits_read();
    editor=MAPMAKER_PTR_OK(0x007844A8u,4)?WORD_AT(0x007844A8u):0u;
    if(!mapmaker_table_enabled() || !mapmaker_limits.limits || !mapmaker_limits_view.valid ||
       mapmaker_limits_view.editor!=editor || !MAPMAKER_PTR_OK(editor,0x41E4) ||
       (mapmaker_probe_flags()&MAPMAKER_PROBE_METER) || !MAPMAKER_STACK_OK(12)) {
        mapmaker_probe_meter(original);
        return;
    }
    if(!mapmaker_table.text) {
        mapmaker_table.text=xbox_HeapAlloc(MAPMAKER_TABLE_ROWS*MAPMAKER_TABLE_TEXT,4u);
        if(mapmaker_table.text && !MAPMAKER_PTR_OK(mapmaker_table.text,MAPMAKER_TABLE_ROWS*MAPMAKER_TABLE_TEXT))
            mapmaker_table.text=0;
    }
    if(!mapmaker_table.text) { mapmaker_probe_meter(original);return; }
    mapmaker_table_categories(MAPMAKER_PTR_OK(0x0078400Cu,4)?WORD_AT(0x0078400Cu):0u,&cats);
    n=mapmaker_table_rows(&mapmaker_limits_view,&cats,WORD_AT(editor+0x280u),WORD_AT(editor+0x41E0u),rows);
    for(i=0;i<n;i++) {
        unsigned len=(unsigned)snprintf(line,sizeof line,"%s: %u/%u%s",rows[i].title,
                                        (unsigned)rows[i].used,(unsigned)rows[i].max,rows[i].unit);
        if(len>widest) widest=len;
        snprintf((char *)((uintptr_t)(mapmaker_table.text+i*MAPMAKER_TABLE_TEXT)+g_xbox_mem_offset),
                 MAPMAKER_TABLE_TEXT,"\vs%d%s\vs100",MAPMAKER_TABLE_SCALE,line);
    }
    mapmaker_table_layout(FLOAT_AT(g_esp+4u),FLOAT_AT(g_esp+8u),n,widest,xs,ys);
    save_registers(&r);
    align=WORD_AT(MAPMAKER_TABLE_ALIGN_WORD);WORD_AT(MAPMAKER_TABLE_ALIGN_WORD)=0;
    font=guest_call(sub_0003FA10,1,MAPMAKER_TABLE_FONT);
    for(i=0;i<n;i++)
        guest_call(sub_00041E30,5,mapmaker_table_bits(xs[i]),mapmaker_table_bits(ys[i]),
                   mapmaker_table.text+i*MAPMAKER_TABLE_TEXT,mapmaker_table_colour(rows[i].state),
                   MAPMAKER_TABLE_OUTLINE);
    guest_call(sub_0003FA10,1,font);
    WORD_AT(MAPMAKER_TABLE_ALIGN_WORD)=align;
    restore_registers(&r);
    MAPMAKER_GUEST_RET();
}
