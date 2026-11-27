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
 * guest regeneration) draws a resource panel instead, at the meter's own
 * position and in its font, over a dark backdrop (sub_0001EFB0, the stock
 * meter's backdrop colour):
 *
 *   Collapsed (the default): one line, "+ Fullest: Objects 128/1023", the
 *   row nearest its cap (state first, then used/max), in that row's colour.
 *   Expanded: a "- Resources" header and one row per resource, the short
 *   title left-aligned and "used/max" right-aligned in a fixed column, at
 *   \vs60 and 11.5 units apart (font 0x12 is ~16 units tall at 100%), or
 *   the measured glyph height + 1.5 when that is more.
 *
 * Toggle: a click (left button = A, or the stick pointer and A) anywhere on
 * the panel, or the I key when no action is bound to it
 * (pc_controls_mapmaker_table_key). The click's A press is taken from the
 * editor (menu +0xC4/+0xC8 bits 0x10, and their repeat/release bits, until
 * A is released), so it neither places nor selects under the panel. The
 * state is kept for the session (this process).
 *
 * Rows (short titles; full names in the doc):
 *   Tiles, Items, Objects (engine objects), Pads, Cover, Links, Groups,
 *   Memory (KB): the placed counts and limits of the editor's own check,
 *   pc_display_mapmaker_limits (mapmaker_limits_view, written by the same
 *   frame's sub_002FA720 wrapper); a row is red when that check refuses the
 *   held piece on its cap (placed + held), as the stock meter label.
 *   Pickups: placed pickups of categories 8-13, 19-21, 31 against the
 *   768 placed-pickup records of play (MAPMAKER_OBJECTS_PICKUPS; the editor
 *   item cap 500 keeps it below).
 *   Movers, Props, Turrets (27-29), Teleports (14-18), Vehicles (35, the
 *   4-block vehicle pool, stock and story vehicles alike): the per-category
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
 * Widths come from the game's own text measure sub_0003FCF0(&w, &h, text, 0)
 * in the current font (it reads the \vs scale codes; sub_00040A40 and the
 * menu mouse use it), with an estimate when it returns nothing sensible.
 * Layout is in guest 640 x 480 units, so it shares the editor HUD's 4:3 box
 * on a wide output (docs/mapmaker/mapmaker-widescreen.md) and stays in the
 * left third of the canvas, left of the toolbar (checked by the fixture).
 * The stock meter is drawn instead (and the meter probe keeps its scope) when
 * the limits are off, the check has not run for this editor,
 * TSFP_MAPMAKER_TABLE=0, probe flag 1 is set or the text buffer is missing.
 * Game state is only read, except the A bits of a toggle click; the guest
 * writes are its own text buffer (xbox_HeapAlloc), the measure's scratch
 * below the guest stack and the text alignment word [0x007E7260], zeroed
 * around the draw and put back (as pc_keyboard_menu.inc does). Replacing the
 * original, it pops the return address (MAPMAKER_GUEST_RET).
 *
 * Requires the includer's WORD_AT, FLOAT_AT, guest_call,
 * save_registers/restore_registers, g_esp, g_xbox_mem_offset, xbox_HeapAlloc,
 * pc_controls_mapmaker_table_key, mapmaker_probe_meter (pc_mapmaker_probe.inc
 * with MAPMAKER_RESTABLE), mapmaker_limits/_view (pc_mapmaker_limits.inc),
 * MAPMAKER_EDITOR_* and MAPMAKER_STACK_OK (pc_mapmaker_editor.inc),
 * MAPMAKER_OBJECTS_PICKUPS (pc_mapmaker_objects.inc). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void sub_0003FA10(void),sub_0003FCF0(void),sub_0001EFB0(void),sub_00041E30(void);
extern uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment);
int pc_controls_mapmaker_table_key(void);

#define MAPMAKER_TABLE_ROWS 16u
#define MAPMAKER_TABLE_SLOTS (1u+2u*MAPMAKER_TABLE_ROWS) /* header, titles, values */
#define MAPMAKER_TABLE_TEXT 64u             /* bytes per text slot */
#define MAPMAKER_TABLE_FONT 0x12u           /* the meter's font (0x0030C214) */
#define MAPMAKER_TABLE_SCALE 60             /* \vs text scale, percent */
#define MAPMAKER_TABLE_LINE 11.5f           /* font 0x12 ~16 units at 100%: 9.6 at 60% + lead */
#define MAPMAKER_TABLE_LEAD 1.5f            /* kept between rows over a measured glyph height */
#define MAPMAKER_TABLE_GLYPH 5.2f           /* estimated advance at 60%, when measuring fails */
#define MAPMAKER_TABLE_PAD 3.0f             /* backdrop around the text */
#define MAPMAKER_TABLE_GAP 8.0f             /* between the title and value columns */
#define MAPMAKER_TABLE_CANVAS_W 640.0f
#define MAPMAKER_TABLE_CANVAS_H 480.0f
#define MAPMAKER_TABLE_MARGIN 8.0f          /* kept clear of the canvas edges */
#define MAPMAKER_TABLE_OUTLINE 0x00000080u  /* stock label outline (0x0030C348) */
#define MAPMAKER_TABLE_PANEL 0x0F0F0F64u    /* stock meter backdrop (0x0030C2E8) */
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
/* Editor input (pc_mapmaker_pointer.inc): scene word, the game pointer of the
 * current player, the menu's pressed (+0xC4) and held/released (+0xC8) bits
 * from sub_00073870 (button i: bit i edge, bit i+16 repeat or release). */
#define MAPMAKER_TABLE_SCENE 102
#define MAPMAKER_TABLE_A 0x00000010u
#define MAPMAKER_TABLE_A_HIGH 0x00100000u
#define MAPMAKER_TABLE_STALE 4u             /* editor ticks without a draw: hidden */

enum { MAPMAKER_TABLE_STATE_OK, MAPMAKER_TABLE_STATE_NEAR, MAPMAKER_TABLE_STATE_FULL };
typedef struct MapMakerTableRow { const char *title, *unit; uint32_t used, max; unsigned state; } MapMakerTableRow;
/* Placed items per category (sub_00308420's counts) and the live maxima. */
typedef struct MapMakerTableCategories { uint32_t count[MAPMAKER_TABLE_CATEGORY_N], max[MAPMAKER_TABLE_CATEGORY_N]; } MapMakerTableCategories;
/* The panel in guest units: backdrop, text column, value column right edge. */
typedef struct MapMakerTableBox { float x0,y0,x1,y1,text_x,value_right,line; unsigned lines; } MapMakerTableBox;

static struct {
    int initialized, enabled, expanded, shown, swallow, key_was;
    unsigned stale;
    uint32_t text;
    MapMakerTableBox box;
} mapmaker_table;

/* Categories of the rows (pc_mapmaker_limits.inc mapmaker_categories). */
static const unsigned mapmaker_table_pickups[]={8u,9u,10u,11u,12u,13u,19u,20u,21u,31u};
static const unsigned mapmaker_table_turrets[]={27u,28u,29u};
static const unsigned mapmaker_table_teleporters[]={14u,15u,16u,17u,18u};
static const unsigned mapmaker_table_movers[]={25u};
static const unsigned mapmaker_table_props[]={26u};
static const unsigned mapmaker_table_vehicles[]={35u};

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
    MAPMAKER_TABLE_ROW("Objects",v->objects,MAPMAKER_LIMIT_OBJECTS,MAPMAKER_CAP_OBJECTS);
    for(i=0;i<sizeof mapmaker_table_pickups/sizeof mapmaker_table_pickups[0];i++)
        pickups+=c->count[mapmaker_table_pickups[i]];
    MAPMAKER_TABLE_ROW("Pickups",pickups,MAPMAKER_OBJECTS_PICKUPS,0u);
    MAPMAKER_TABLE_ROW("Pads",v->pads,MAPMAKER_LIMIT_PADS,MAPMAKER_CAP_PADS);
    MAPMAKER_TABLE_ROW("Cover",v->cover,MAPMAKER_LIMIT_COVER,MAPMAKER_CAP_COVER);
    MAPMAKER_TABLE_ROW("Links",v->links,MAPMAKER_LIMIT_LINKS,MAPMAKER_CAP_LINKS);
    MAPMAKER_TABLE_ROW("Groups",groups,(uint32_t)MAPMAKER_LIMIT_GROUPS,MAPMAKER_CAP_GROUPS);
    MAPMAKER_TABLE_ROW("Memory",memory_kb,MAPMAKER_LIMIT_MEMORY/MAPMAKER_TABLE_KB,MAPMAKER_CAP_MEMORY);
    rows[n-1].unit=" KB";
    MAPMAKER_TABLE_ROW("Story AI",story_ai,MAPMAKER_EDITOR_SAI_LIMIT,0u);
    MAPMAKER_TABLE_ROW("Logic",logic_ops,MAPMAKER_EDITOR_OPS_CAP,0u);
#undef MAPMAKER_TABLE_ROW
    rows[n++]=mapmaker_table_group("Movers",c,mapmaker_table_movers,1);
    rows[n++]=mapmaker_table_group("Props",c,mapmaker_table_props,1);
    rows[n++]=mapmaker_table_group("Turrets",c,mapmaker_table_turrets,3);
    rows[n++]=mapmaker_table_group("Teleports",c,mapmaker_table_teleporters,5);
    rows[n++]=mapmaker_table_group("Vehicles",c,mapmaker_table_vehicles,1);
    return n;
}

/* The row nearest its cap: the worst state, then the largest used/max (a
 * zero maximum counts as full); the first such row on a tie. */
static int mapmaker_table_fuller(const MapMakerTableRow *a,const MapMakerTableRow *b)
{
    if(a->state!=b->state) return a->state>b->state;
    if(!b->max) return 0;
    if(!a->max) return 1;
    return (uint64_t)a->used*b->max>(uint64_t)b->used*a->max;
}
static unsigned mapmaker_table_fullest(const MapMakerTableRow *rows,unsigned n)
{
    unsigned i,best=0;
    for(i=1;i<n;i++) if(mapmaker_table_fuller(&rows[i],&rows[best])) best=i;
    return best;
}

/* Line pitch: the default, or a measured glyph height plus the lead when
 * that is taller (a font that measures taller never makes rows touch). */
static float mapmaker_table_line(float glyph_h)
{
    if(isfinite(glyph_h) && glyph_h>=4.f && glyph_h<=32.f && glyph_h+MAPMAKER_TABLE_LEAD>MAPMAKER_TABLE_LINE)
        return glyph_h+MAPMAKER_TABLE_LEAD;
    return MAPMAKER_TABLE_LINE;
}

/* The panel at the meter's position, moved inside the canvas if it would
 * leave it. lines: 1 collapsed, 1 + rows expanded. title_w/value_w: the
 * widest title and value (expanded), header_w: the header line. */
static void mapmaker_table_layout(float x,float y,unsigned lines,float header_w,float title_w,float value_w,
                                  float line,MapMakerTableBox *b)
{
    float body=lines>1?title_w+MAPMAKER_TABLE_GAP+value_w:0.f;
    float w=(header_w>body?header_w:body)+2.f*MAPMAKER_TABLE_PAD,h=(float)lines*line+2.f*MAPMAKER_TABLE_PAD;
    if(!isfinite(x)) x=MAPMAKER_TABLE_MARGIN;
    if(!isfinite(y)) y=MAPMAKER_TABLE_MARGIN;
    if(x+w>MAPMAKER_TABLE_CANVAS_W-MAPMAKER_TABLE_MARGIN) x=MAPMAKER_TABLE_CANVAS_W-MAPMAKER_TABLE_MARGIN-w;
    if(y+h>MAPMAKER_TABLE_CANVAS_H-MAPMAKER_TABLE_MARGIN) y=MAPMAKER_TABLE_CANVAS_H-MAPMAKER_TABLE_MARGIN-h;
    if(x<MAPMAKER_TABLE_MARGIN) x=MAPMAKER_TABLE_MARGIN;
    if(y<MAPMAKER_TABLE_MARGIN) y=MAPMAKER_TABLE_MARGIN;
    b->x0=x;b->y0=y;b->x1=x+w;b->y1=y+h;b->line=line;b->lines=lines;
    b->text_x=x+MAPMAKER_TABLE_PAD;
    b->value_right=b->text_x+title_w+MAPMAKER_TABLE_GAP+value_w;
}
/* Top of text line i (0 the header). */
static float mapmaker_table_line_y(const MapMakerTableBox *b,unsigned i)
{
    return b->y0+MAPMAKER_TABLE_PAD+(float)i*b->line;
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

/* Text slot i of the guest buffer, filled as "\vs60<text>\vs100". */
static uint32_t mapmaker_table_slot(unsigned i) { return mapmaker_table.text+i*MAPMAKER_TABLE_TEXT; }
static unsigned mapmaker_table_put(unsigned i,const char *text)
{
    snprintf((char *)((uintptr_t)mapmaker_table_slot(i)+g_xbox_mem_offset),MAPMAKER_TABLE_TEXT,
             "\vs%d%s\vs100",MAPMAKER_TABLE_SCALE,text);
    return (unsigned)strlen(text);
}

/* sub_0003FCF0(&w, &h, text, 0) in the current font, into scratch below the
 * guest stack (the caller saved the registers). An estimate from the visible
 * length when it returns nothing sensible. */
static float mapmaker_table_measure(unsigned slot,unsigned chars,float *glyph_h)
{
    uint32_t sp=g_esp-32u;
    float w,h;
    FLOAT_AT(sp)=0.f;FLOAT_AT(sp+4u)=0.f;
    g_esp=sp;
    guest_call(sub_0003FCF0,4,sp,sp+4u,mapmaker_table_slot(slot),0u);
    g_esp=sp+32u;
    w=FLOAT_AT(sp);h=FLOAT_AT(sp+4u);
    if(glyph_h && isfinite(h) && h>*glyph_h) *glyph_h=h;
    return isfinite(w) && w>0.f && w<MAPMAKER_TABLE_CANVAS_W?w:(float)chars*MAPMAKER_TABLE_GLYPH;
}

/* Is (x, y) on the panel drawn within the last few editor ticks? */
static int mapmaker_table_hit(float x,float y)
{
    const MapMakerTableBox *b=&mapmaker_table.box;
    return mapmaker_table.shown && mapmaker_table.stale<=MAPMAKER_TABLE_STALE &&
        x>=b->x0 && x<b->x1 && y>=b->y0 && y<b->y1;
}

/* Called after the original sub_00073870 for every dispatched menu
 * (pc_display_menu_mouse_input). On the editor screen itself (scene 102, no
 * rows, no child menu): the I key or an A press on the panel toggles it, and
 * that A press is removed from the editor's input until it is released. */
static void mapmaker_table_input(uint32_t menu)
{
    uint32_t player,b;
    int key;
    if(!mapmaker_table_enabled() || !MAPMAKER_PTR_OK(0x0079094Cu,4) ||
       (int32_t)WORD_AT(0x0079094Cu)!=MAPMAKER_TABLE_SCENE || !MAPMAKER_PTR_OK(menu,0x23c) ||
       WORD_AT(menu+0x178) || WORD_AT(menu+0x170)) return;
    if(mapmaker_table.stale<=MAPMAKER_TABLE_STALE) mapmaker_table.stale++;
    if(mapmaker_table.swallow) {
        WORD_AT(menu+0xc4)&=~(MAPMAKER_TABLE_A|MAPMAKER_TABLE_A_HIGH);
        if(!(WORD_AT(menu+0xc8)&MAPMAKER_TABLE_A)) mapmaker_table.swallow=0;
        WORD_AT(menu+0xc8)&=~(MAPMAKER_TABLE_A|MAPMAKER_TABLE_A_HIGH);
    }
    key=pc_controls_mapmaker_table_key()!=0;
    if(key && !mapmaker_table.key_was && mapmaker_table.shown && mapmaker_table.stale<=MAPMAKER_TABLE_STALE)
        mapmaker_table.expanded=!mapmaker_table.expanded;
    mapmaker_table.key_was=key;
    if(mapmaker_table.swallow || !(WORD_AT(menu+0xc4)&MAPMAKER_TABLE_A) || !MAPMAKER_PTR_OK(0x007BA9E0u,4)) return;
    player=WORD_AT(0x007BA9E0u);
    b=0x007BA260u+player*0x1E0u;
    if(player>=4 || !MAPMAKER_PTR_OK(b,0x28) || !mapmaker_table_hit(FLOAT_AT(b+0x20),FLOAT_AT(b+0x24))) return;
    mapmaker_table.expanded=!mapmaker_table.expanded;
    mapmaker_table.swallow=1;
    WORD_AT(menu+0xc4)&=~(MAPMAKER_TABLE_A|MAPMAKER_TABLE_A_HIGH);
    WORD_AT(menu+0xc8)&=~(MAPMAKER_TABLE_A|MAPMAKER_TABLE_A_HIGH);
}

/* WRAP sub_0030C210 (cdecl x, y; plain ret): the editor capacity meter. */
void pc_display_mapmaker_meter(void (*original)(void))
{
    MapMakerTableRow rows[MAPMAKER_TABLE_ROWS];
    MapMakerTableCategories cats;
    MapMakerTableBox box;
    GuestRegisters r;
    char line[MAPMAKER_TABLE_TEXT];
    unsigned chars[MAPMAKER_TABLE_SLOTS];
    float widths[MAPMAKER_TABLE_SLOTS],title_w=0.f,value_w=0.f,glyph_h=0.f,x,y;
    uint32_t editor,font,align;
    unsigned n,i,worst,lines;
    mapmaker_limits_read();
    editor=MAPMAKER_PTR_OK(0x007844A8u,4)?WORD_AT(0x007844A8u):0u;
    if(!mapmaker_table_enabled() || !mapmaker_limits.limits || !mapmaker_limits_view.valid ||
       mapmaker_limits_view.editor!=editor || !MAPMAKER_PTR_OK(editor,0x41E4) ||
       (mapmaker_probe_flags()&MAPMAKER_PROBE_METER) || !MAPMAKER_STACK_OK(12)) {
        mapmaker_table.shown=0;           /* the stock meter: clicks there are the editor's */
        mapmaker_probe_meter(original);
        return;
    }
    if(!mapmaker_table.text) {
        mapmaker_table.text=xbox_HeapAlloc(MAPMAKER_TABLE_SLOTS*MAPMAKER_TABLE_TEXT,4u);
        if(mapmaker_table.text && !MAPMAKER_PTR_OK(mapmaker_table.text,MAPMAKER_TABLE_SLOTS*MAPMAKER_TABLE_TEXT))
            mapmaker_table.text=0;
    }
    if(!mapmaker_table.text) { mapmaker_table.shown=0;mapmaker_probe_meter(original);return; }
    mapmaker_table_categories(MAPMAKER_PTR_OK(0x0078400Cu,4)?WORD_AT(0x0078400Cu):0u,&cats);
    n=mapmaker_table_rows(&mapmaker_limits_view,&cats,WORD_AT(editor+0x280u),WORD_AT(editor+0x41E0u),rows);
    worst=mapmaker_table_fullest(rows,n);
    if(mapmaker_table.expanded) snprintf(line,sizeof line,"- Resources");
    else snprintf(line,sizeof line,"+ Fullest: %s %u/%u%s",rows[worst].title,
                  (unsigned)rows[worst].used,(unsigned)rows[worst].max,rows[worst].unit);
    chars[0]=mapmaker_table_put(0,line);
    lines=1;
    if(mapmaker_table.expanded) {
        for(i=0;i<n;i++) {
            chars[1+i]=mapmaker_table_put(1+i,rows[i].title);
            /* The space ends the scale code: \vs reads every digit that follows
             * (0x00040082). Right-aligned, it only widens the measured text. */
            snprintf(line,sizeof line," %u/%u%s",(unsigned)rows[i].used,(unsigned)rows[i].max,rows[i].unit);
            chars[1+n+i]=mapmaker_table_put(1+n+i,line);
        }
        lines=1+n;
    }
    x=FLOAT_AT(g_esp+4u);y=FLOAT_AT(g_esp+8u);
    save_registers(&r);
    align=WORD_AT(MAPMAKER_TABLE_ALIGN_WORD);WORD_AT(MAPMAKER_TABLE_ALIGN_WORD)=0;
    font=guest_call(sub_0003FA10,1,MAPMAKER_TABLE_FONT);
    /* Measured in the drawing font: header, then every title and value. */
    widths[0]=mapmaker_table_measure(0,chars[0],&glyph_h);
    if(lines>1)
        for(i=0;i<n;i++) {
            widths[1+i]=mapmaker_table_measure(1+i,chars[1+i],&glyph_h);
            widths[1+n+i]=mapmaker_table_measure(1+n+i,chars[1+n+i],&glyph_h);
            if(widths[1+i]>title_w) title_w=widths[1+i];
            if(widths[1+n+i]>value_w) value_w=widths[1+n+i];
        }
    mapmaker_table_layout(x,y,lines,widths[0],title_w,value_w,mapmaker_table_line(glyph_h),&box);
    guest_call(sub_0001EFB0,5,(uint32_t)(int32_t)floorf(box.x0),(uint32_t)(int32_t)floorf(box.y0),
               (uint32_t)(int32_t)ceilf(box.x1),(uint32_t)(int32_t)ceilf(box.y1),MAPMAKER_TABLE_PANEL);
    guest_call(sub_00041E30,5,mapmaker_table_bits(box.text_x),mapmaker_table_bits(mapmaker_table_line_y(&box,0)),
               mapmaker_table_slot(0),mapmaker_table_colour(rows[worst].state),MAPMAKER_TABLE_OUTLINE);
    if(lines>1)
        for(i=0;i<n;i++) {
            uint32_t colour=mapmaker_table_colour(rows[i].state),ty=mapmaker_table_bits(mapmaker_table_line_y(&box,1+i));
            guest_call(sub_00041E30,5,mapmaker_table_bits(box.text_x),ty,mapmaker_table_slot(1+i),colour,
                       MAPMAKER_TABLE_OUTLINE);
            guest_call(sub_00041E30,5,mapmaker_table_bits(box.value_right-widths[1+n+i]),ty,
                       mapmaker_table_slot(1+n+i),colour,MAPMAKER_TABLE_OUTLINE);
        }
    guest_call(sub_0003FA10,1,font);
    WORD_AT(MAPMAKER_TABLE_ALIGN_WORD)=align;
    restore_registers(&r);
    mapmaker_table.box=box;mapmaker_table.shown=1;mapmaker_table.stale=0;
    MAPMAKER_GUEST_RET();
}
