/* US 45410066 MapMaker editor: sixteen floors, an 80 x 80 grid, overlapping
 * tiles and stacked items (logs/limits/features/floors-grid.md and
 * overlap-items.md; docs/mapmaker/mapmaker-grid.md; docs/agent-context/limitless-floors.md).
 *
 * The editor keeps one occupancy grid, [0x00784004]: a Level-heap array of
 * piece pointers, x + 40*(y + 40*(z+2)), 40 x 40 x 6 levels (z -2..3). Five
 * functions own it and nothing in play reads it: sub_00307CC0 allocates and
 * clears it (editor init), sub_00307C20 clears it (reset), sub_00307CF0
 * writes a piece over its whole footprint box, sub_00307DA0 zeroes that box,
 * sub_00307E60(x, y, z floats) reads one cell (0 = empty, also outside).
 *
 * TSFP_MAPMAKER_LIMITS=1 (with the pools on, as mapmaker_limits.limits):
 * - grid 80 x 80 x 17 levels (z -2..14, 425 KB of Level heap): the five
 *   functions are reimplemented here with per-axis bounds (stock bounds only
 *   the flat index, which would wrap x into the next row);
 * - floors -2..13 can be selected: floor up past 2 is refused by
 *   sub_00306460 (`cmp ecx,2` at 0x00306E14) with text/sound 0x1576 through
 *   sub_002F79E0 (return 0x00306E99); that refusal is turned into the stock
 *   success (floor +1, "below" = old floor, sound 0x157D) up to floor 13;
 *   the 3D view's own floor up (sub_0032EDA0, `cmp edx,2` at 0x0032EEBC)
 *   reaches floor 13 too (pc_mapmaker_grid_preview_floor);
 * - placement is refused when any layer of a held piece would be above the
 *   top grid level (sub_002F8DE0 tests only the bottom, so stacked
 *   selections could leave the grid; z >= 4 was the stock "7th floor");
 * - the settings block (pc_mapmaker_grid.h) gives the floor menu 16 bars, the
 *   placement bound, view-centre clamp and out-of-grid shading 80.0 and the
 *   widest zoom 100 cells (grid.json redirects the editor's reads there).
 * Without the switch every hook runs its original and the block holds the
 * stock constants. The floor menu's loop count is a stack local
 * ([esp+0x18] of the loop frame, set at 0x0030BF3F, 16 in the image): after
 * the first bar's sub_0001ED50 (floor -2, return 0x0030BFB5) the hook sets
 * it to this mode's bar count (5 stock, 16 with the limits); the loop reads
 * it back at 0x0030C008. Should that slot not hold the table's 16, stock
 * mode falls back to moving the bars for floors 3+ off screen by writing
 * their y slot, which nothing reads after the loop.
 *
 * TSFP_MAPMAKER_OVERLAP=1 (editor only, scene 102): the occupancy probe of
 * the placement test (sub_002F8140 called from 0x002F8F8E, return
 * 0x002F8F93) answers "free"; bounds still apply and pieces leaving the grid
 * are refused as above (with the stock 6 levels unless the limits are on).
 * TSFP_MAPMAKER_OVERLAP=1 or the limits: after a piece is removed
 * (sub_00307DA0, called by sub_00308480 once the piece is unlinked), every
 * other placed piece's cells inside the removed box are written back in list
 * (= placement/file) order, so the last placed piece owns each cell again and
 * no cell still covered by another piece reads as free.
 *
 * TSFP_MAPMAKER_STACK_ITEMS=1 (editor only): sub_00303460 neither replaces
 * nor uncounts the item already in the cell (sub_00308260 answers "none" at
 * returns 0x003034AD/0x00303696), and the validity test sub_00334770 does
 * not refuse an occupied cell (matching items' and story AI points' x word
 * hidden as 0x7FFF for the call and restored). The per-frame "placement valid" bit (editor
 * +0x25C value 4; clear = red X, and the drop refuses with 0x1576) has a
 * second occupancy test in sub_00306460: after sub_003348C0 passes for a
 * held item (return 0x003070A7) it is set only when no item is under the
 * cursor ([editor+0x2F1C] at 0x003070B4); that hook sets it itself when
 * the held item may stack. Holding an item, the button always drops
 * (pick-up of the item under the cursor runs only with empty hands,
 * sub_00304C30 0x00304D7D). Only for allowed categories (weapons, ammo,
 * keycards, health, armour, teleporters, power-ups, special pickups,
 * props), held and already in the cell; starts, flags, turrets, movers,
 * markers, the single item and unknown categories keep the stock
 * one-per-cell rule (nothing joins them, they join nothing). Story AI (a
 * held story AI, editor +0x2F10 >= 0, story AI mode sub_003051F0) may go
 * on a cell with allowed items and with points of other story AI, and
 * allowed items on story AI points: sub_00334770 hides those points too,
 * and sub_002F8B40 (point under the cursor, return 0x0030723F) answers
 * none while a story AI is held, which both the valid bit (0x003072B4)
 * and the drop (0x00305238) require. A story AI's own points still may
 * not share a cell.
 *
 * The map format is unchanged: absolute signed-byte cells, rebuilt into the
 * grid at load. Requires the includer's WORD_AT, FLOAT_AT, guest_writable,
 * guest_call, save_registers/restore_registers, g_eax/g_ecx/g_ebx/g_ebp/
 * g_esp, g_xbox_mem_offset and pc_mapmaker_limits.inc. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_mapmaker_grid.h"

#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif

#define MAPMAKER_GRID_SCENE 102u
#define MAPMAKER_GRID_PTR 0x00784004u
#define MAPMAKER_GRID_PLACED 0x0078400Cu
#define MAPMAKER_GRID_EDITOR 0x007844A8u
#define MAPMAKER_GRID_STOCK_W 40
#define MAPMAKER_GRID_STOCK_L 6
#define MAPMAKER_GRID_WIDE_W 80
#define MAPMAKER_GRID_WIDE_L 17     /* levels z -2..14: floors -2..13 plus a 2-tall piece on 13 */
#define MAPMAKER_GRID_TOP_FLOOR 13  /* highest selectable floor with the limits */
#define MAPMAKER_GRID_STOCK_BARS 5  /* floor menu bars: floors -2..2 */
#define MAPMAKER_GRID_WIDE_BARS 16  /* floors -2..13; also the loop count in grid.json (test-checked) */
#define MAPMAKER_GRID_DIM_MAX 64    /* footprint sanity cap per axis */
#define MAPMAKER_GRID_HIDDEN_Y (-100000.f)
#define MAPMAKER_GRID_RET_PROBE 0x002F8F93u
#define MAPMAKER_GRID_RET_FLOOR 0x00306E99u
#define MAPMAKER_GRID_RET_BAR 0x0030BFB5u
#define MAPMAKER_GRID_RET_PREVIEW 0x0032F20Fu  /* sub_0032F0E0 after its call of sub_0032EDA0 */
#define MAPMAKER_GRID_PREVIEW 0x00778D34u      /* the editor's 3D view: +0x98 floor, +0x80 & 0x800 gliding */
#define MAPMAKER_STACK_RET_COUNT 0x003034ADu
#define MAPMAKER_STACK_RET_REPLACE 0x00303696u
#define MAPMAKER_STACK_RET_VALID 0x003070A7u
#define MAPMAKER_STACK_RET_SAI_POINT 0x0030723Fu
#define MAPMAKER_STACK_HIDE 0x7FFFu
#define MAPMAKER_GRID_U16(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

static struct {
    int initialized, wide, overlap, stack;
    unsigned refused, overlaps, stacked, reregistered, stack_valid, stack_sai, stack_newest, preview_up;
    uint32_t allocated;   /* the grid this hook allocated (wide mode) */
} mapmaker_grid;

static int mapmaker_grid_switch(const char *name)
{
    const char *v=getenv(name);
    return v && v[0]=='1' && !v[1];
}
static void mapmaker_grid_config(void)
{
    int w=mapmaker_grid.wide;
    FLOAT_AT(MAPMAKER_GRID_CFG_FLOORS)=w?(float)MAPMAKER_GRID_WIDE_BARS:FLOAT_AT(0x004760CCu);
    FLOAT_AT(MAPMAKER_GRID_CFG_EXTENT)=w?(float)MAPMAKER_GRID_WIDE_W:FLOAT_AT(0x004788B8u);
    FLOAT_AT(MAPMAKER_GRID_CFG_ZOOM)=w?100.f:FLOAT_AT(0x004760ACu);
    FLOAT_AT(MAPMAKER_GRID_CFG_ZOOM_INV)=w?.01f:FLOAT_AT(0x00478394u);
}
static void mapmaker_grid_read(void)
{
    if(mapmaker_grid.initialized) return;
    mapmaker_limits_read();
    mapmaker_grid.initialized=1;
    mapmaker_grid.wide=mapmaker_limits.limits;
    mapmaker_grid.overlap=mapmaker_grid_switch("TSFP_MAPMAKER_OVERLAP");
    mapmaker_grid.stack=mapmaker_grid_switch("TSFP_MAPMAKER_STACK_ITEMS");
    mapmaker_grid_config();
}
static int mapmaker_grid_w(void) { return mapmaker_grid.wide?MAPMAKER_GRID_WIDE_W:MAPMAKER_GRID_STOCK_W; }
static int mapmaker_grid_l(void) { return mapmaker_grid.wide?MAPMAKER_GRID_WIDE_L:MAPMAKER_GRID_STOCK_L; }
static uint32_t mapmaker_grid_bytes(void) { return (uint32_t)(mapmaker_grid_w()*mapmaker_grid_w()*mapmaker_grid_l())*4u; }
static int mapmaker_grid_editor(void) { return WORD_AT(0x0079094Cu)==MAPMAKER_GRID_SCENE; }

/* cvttss2si: truncation toward zero, 0x80000000 for NaN and out of range. */
static int32_t mapmaker_grid_trunc(float f)
{
    return (f>-2147483648.f && f<2147483648.f)?(int32_t)f:INT32_MIN;
}

/* The grid array when it is the size this mode expects (0 otherwise). */
static uint32_t mapmaker_grid_array(void)
{
    uint32_t g=WORD_AT(MAPMAKER_GRID_PTR);
    return g && MAPMAKER_PTR_OK(g,mapmaker_grid_bytes())?g:0u;
}
static uint32_t mapmaker_grid_index(int32_t x,int32_t y,int32_t level)
{
    int32_t w=mapmaker_grid_w();
    if(x<0 || x>=w || y<0 || y>=w || level<0 || level>=mapmaker_grid_l()) return UINT32_MAX;
    return (uint32_t)(x+w*(y+w*level));
}

/* A placed piece's footprint box, cell by cell as sub_00307CF0 computes it:
 * level = trunc(l + z - -2.0), y = trunc(j + y), x = trunc(i + x). */
typedef struct MapMakerGridBox { int32_t x0,x1,y0,y1,l0,l1; } MapMakerGridBox; /* inclusive */
typedef void (*MapMakerGridCell)(void *ctx,uint32_t piece,int32_t x,int32_t y,int32_t level);
static int mapmaker_grid_cells(uint32_t piece,MapMakerGridCell fn,void *ctx)
{
    uint32_t rot,desc;
    int32_t w,d,h,i,j,l;
    float px,py,pz;
    if(!MAPMAKER_PTR_OK(piece,0x48)) return 0;
    rot=WORD_AT(piece+0x0Cu);
    if(rot>3u) return 0;
    desc=WORD_AT(piece+0x2Cu+rot*4u);
    if(!MAPMAKER_PTR_OK(desc,0x20)) return 0;
    w=(int32_t)WORD_AT(desc+0x14u);d=(int32_t)WORD_AT(desc+0x18u);h=(int32_t)WORD_AT(desc+0x1Cu);
    if(w>MAPMAKER_GRID_DIM_MAX || d>MAPMAKER_GRID_DIM_MAX || h>MAPMAKER_GRID_DIM_MAX) return 0;
    px=FLOAT_AT(piece);py=FLOAT_AT(piece+4u);pz=FLOAT_AT(piece+8u);
    for(l=0;l<h;l++) for(j=0;j<d;j++) for(i=0;i<w;i++)
        fn(ctx,piece,mapmaker_grid_trunc((float)i+px),mapmaker_grid_trunc((float)j+py),
           mapmaker_grid_trunc(((float)l+pz)-(-2.f)));
    return 1;
}
static void mapmaker_grid_put(void *ctx,uint32_t piece,int32_t x,int32_t y,int32_t level)
{
    uint32_t g=*(uint32_t *)ctx,i=mapmaker_grid_index(x,y,level);
    if(i!=UINT32_MAX) WORD_AT(g+i*4u)=piece;
}
static void mapmaker_grid_clear(void *ctx,uint32_t piece,int32_t x,int32_t y,int32_t level)
{
    uint32_t g=*(uint32_t *)ctx,i=mapmaker_grid_index(x,y,level);
    (void)piece;
    if(i!=UINT32_MAX) WORD_AT(g+i*4u)=0;
}
static void mapmaker_grid_extent(void *ctx,uint32_t piece,int32_t x,int32_t y,int32_t level)
{
    MapMakerGridBox *b=ctx;
    (void)piece;
    if(mapmaker_grid_index(x,y,level)==UINT32_MAX) return;
    if(b->x0>b->x1) { b->x0=b->x1=x;b->y0=b->y1=y;b->l0=b->l1=level;return; }
    if(x<b->x0) b->x0=x; if(x>b->x1) b->x1=x;
    if(y<b->y0) b->y0=y; if(y>b->y1) b->y1=y;
    if(level<b->l0) b->l0=level; if(level>b->l1) b->l1=level;
}
typedef struct MapMakerGridRestore { uint32_t grid; MapMakerGridBox box; unsigned cells; } MapMakerGridRestore;
static void mapmaker_grid_restore(void *ctx,uint32_t piece,int32_t x,int32_t y,int32_t level)
{
    MapMakerGridRestore *r=ctx;
    uint32_t i;
    if(x<r->box.x0 || x>r->box.x1 || y<r->box.y0 || y>r->box.y1 || level<r->box.l0 || level>r->box.l1) return;
    if((i=mapmaker_grid_index(x,y,level))==UINT32_MAX) return;
    WORD_AT(r->grid+i*4u)=piece;r->cells++;
}
/* After `removed` left the grid: write back, in list order, the cells of the
 * other placed pieces inside its box (later pieces win, as when placed). */
static void mapmaker_grid_reregister(uint32_t removed)
{
    MapMakerGridRestore r;
    uint32_t p;
    unsigned n;
    if(!(r.grid=mapmaker_grid_array())) return;
    r.box.x0=1;r.box.x1=0;r.cells=0;
    if(!mapmaker_grid_cells(removed,mapmaker_grid_extent,&r.box) || r.box.x0>r.box.x1) return;
    p=WORD_AT(MAPMAKER_GRID_PLACED);
    for(n=0;p && n<4096u && MAPMAKER_PTR_OK(p,0x48);n++,p=WORD_AT(p+0x40u))
        if(p!=removed) mapmaker_grid_cells(p,mapmaker_grid_restore,&r);
    if(r.cells && mapmaker_grid.reregistered++<3u)
        fprintf(stderr,"[mapmaker] grid: %u cells of overlapping pieces kept after a removal\n",r.cells);
}

/* ---- the five grid functions ----------------------------------------- */
extern void sub_0003DFA0(void);
/* sub_00307CC0: allocate and clear the grid (editor init). */
void pc_mapmaker_grid_alloc(void (*original)(void))
{
    GuestRegisters r;
    uint32_t bytes,g;
    mapmaker_grid_read();
    mapmaker_grid_config();
    if(!mapmaker_grid.wide) { original();return; }
    bytes=mapmaker_grid_bytes();
    save_registers(&r);
    g=guest_call(sub_0003DFA0,2,bytes,1u); /* Level heap, as the original */
    restore_registers(&r);
    WORD_AT(MAPMAKER_GRID_PTR)=g;
    mapmaker_grid.allocated=g;
    if(g && guest_writable(g,bytes)) memset((void *)((uintptr_t)g+g_xbox_mem_offset),0,bytes);
    else fprintf(stderr,"[mapmaker] grid: no %u-byte grid from the Level heap\n",bytes);
    g_eax=0;g_ecx=0;                       /* as after the original's rep stosd */
    MAPMAKER_GUEST_RET();
}
/* sub_00307C20: piece pools and grid reset; the original clears 40x40x6. */
void pc_mapmaker_grid_reset(void (*original)(void))
{
    uint32_t g;
    mapmaker_grid_read();
    original();
    if(mapmaker_grid.wide && (g=mapmaker_grid_array()) && g==mapmaker_grid.allocated)
        memset((void *)((uintptr_t)g+g_xbox_mem_offset),0,mapmaker_grid_bytes());
}
/* sub_00307CF0(piece): register the piece over its footprint box. */
void pc_mapmaker_grid_insert(void (*original)(void))
{
    uint32_t g;
    mapmaker_grid_read();
    if(!mapmaker_grid.wide) { original();return; }
    if(MAPMAKER_STACK_OK(8) && (g=mapmaker_grid_array())) mapmaker_grid_cells(WORD_AT(g_esp+4u),mapmaker_grid_put,&g);
    MAPMAKER_GUEST_RET();
}
/* sub_00307DA0(piece): unregister; then keep the cells other pieces cover. */
void pc_mapmaker_grid_remove(void (*original)(void))
{
    uint32_t piece,g;
    mapmaker_grid_read();
    piece=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u;
    if(!mapmaker_grid.wide) original();
    else {
        if(piece && (g=mapmaker_grid_array())) mapmaker_grid_cells(piece,mapmaker_grid_clear,&g);
        MAPMAKER_GUEST_RET();
    }
    if(piece && (mapmaker_grid.wide || mapmaker_grid.overlap)) mapmaker_grid_reregister(piece);
}
/* sub_00307E60(float x, float y, float z): the piece in a cell, 0 if none or
 * outside. Hot (per frame, save-time connections): no guest_writable. */
void pc_mapmaker_grid_lookup(void (*original)(void))
{
    uint32_t g,i;
    mapmaker_grid_read();
    if(!mapmaker_grid.wide) { original();return; }
    g_eax=0;
    if(MAPMAKER_STACK_OK(16) && (g=mapmaker_grid_array())) {
        int32_t z=mapmaker_grid_trunc(FLOAT_AT(g_esp+12u));
        i=mapmaker_grid_index(mapmaker_grid_trunc(FLOAT_AT(g_esp+4u)),mapmaker_grid_trunc(FLOAT_AT(g_esp+8u)),
                              z==INT32_MIN?-1:z+2);
        if(i!=UINT32_MAX) g_eax=WORD_AT(g+i*4u);
    }
    MAPMAKER_GUEST_RET();
}

/* ---- floors ---------------------------------------------------------- */
/* sub_002F79E0(text): the floor-up refusal at the stock top floor becomes
 * the stock floor-up, to floor 13. A refusal with the floor above -2 can
 * only be floor up (floor down refuses at -2). */
void pc_mapmaker_grid_floor_sound(void (*original)(void))
{
    mapmaker_grid_read();
    if(mapmaker_grid.wide && MAPMAKER_STACK_OK(8) && WORD_AT(g_esp)==MAPMAKER_GRID_RET_FLOOR &&
       WORD_AT(g_esp+4u)==0x1576u) {
        uint32_t editor=WORD_AT(MAPMAKER_GRID_EDITOR);
        if(MAPMAKER_PTR_OK(editor,0x270)) {
            int32_t f=(int32_t)WORD_AT(editor+0x264u);
            if(f>=2 && f<MAPMAKER_GRID_TOP_FLOOR) {
                WORD_AT(editor+0x264u)=(uint32_t)(f+1);
                WORD_AT(editor+0x268u)=(uint32_t)f;
                WORD_AT(g_esp+4u)=0x157Du;     /* the success text/sound of the floor change */
            }
        }
    }
    original();
}
/* sub_0001ED50 (sprite select; generic): in the floor menu loop of
 * sub_0030BE60 (ebx = floor, from -2). After the original has popped its
 * return address the loop frame is g_esp+8 (the arguments of sub_00081C10
 * and sub_0001ED50 are still pushed): bar y [frame+0x10], remaining count
 * [frame+0x18]. At floor -2 the count still holds the image's 16; it becomes
 * this mode's bar count, so the stock menu draws its 5 bars. */
static int mapmaker_grid_bars(void) { return mapmaker_grid.wide?MAPMAKER_GRID_WIDE_BARS:MAPMAKER_GRID_STOCK_BARS; }
void pc_mapmaker_grid_floor_bar(void (*original)(void))
{
    int bar;
    mapmaker_grid_read();  /* early and often: sets the block before any editor code reads it */
    bar=MAPMAKER_STACK_OK(0x28) && WORD_AT(g_esp)==MAPMAKER_GRID_RET_BAR;
    original();
    if(!bar || !MAPMAKER_STACK_OK(0x24)) return;
    if((int32_t)g_ebx==-2 && WORD_AT(g_esp+0x20u)==(uint32_t)MAPMAKER_GRID_WIDE_BARS)
        WORD_AT(g_esp+0x20u)=(uint32_t)mapmaker_grid_bars();
    if(!mapmaker_grid.wide && (int32_t)g_ebx>2)
        FLOAT_AT(g_esp+0x18u)=MAPMAKER_GRID_HIDDEN_Y;
}
/* sub_0032EDA0 (3D view floor change; no arguments, called only from
 * 0x0032F20A): up is refused at floor 2 (`cmp edx,2`, 0x0032EEBC), down at
 * -2 (0x0032EEEB); either only steps +0x98 by one. With the limits, for a
 * floor f in 2..12 with no glide running, the original sees floor 1 and the
 * difference f-1 is added back afterwards: up gives f+1, down f-1, both or
 * neither f, as at a floor below 2. The glide branch (+0x80 & 0x800) copies
 * +0x98 to the editor floor (0x0032EE7F) and is left alone. */
void pc_mapmaker_grid_preview_floor(void (*original)(void))
{
    uint32_t view;
    int32_t f,shift=0;
    mapmaker_grid_read();
    view=WORD_AT(MAPMAKER_GRID_PREVIEW);
    if(mapmaker_grid.wide && MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==MAPMAKER_GRID_RET_PREVIEW &&
       mapmaker_grid_editor() && MAPMAKER_PTR_OK(view,0xA0) && !(WORD_AT(view+0x80u)&0x800u) &&
       (f=(int32_t)WORD_AT(view+0x98u))>=2 && f<MAPMAKER_GRID_TOP_FLOOR) {
        shift=f-1;
        WORD_AT(view+0x98u)=1u;
    }
    original();
    if(!shift) return;
    f=(int32_t)WORD_AT(view+0x98u)+shift;
    WORD_AT(view+0x98u)=(uint32_t)f;
    if(f>shift+1 && mapmaker_grid.preview_up++<3u)
        fprintf(stderr,"[mapmaker] grid: 3D view floor up to %d\n",(int)f);
}
/* sub_002F8DE0: placement test of the held pieces; also refuse any layer
 * above the grid's top level (the stock test bounds only the bottom). */
void pc_mapmaker_grid_placement(void (*original)(void))
{
    uint32_t editor,p;
    unsigned n;
    original();
    mapmaker_grid_read();
    if(!g_eax || !(mapmaker_grid.wide || mapmaker_grid.overlap) || !mapmaker_grid_editor()) return;
    editor=WORD_AT(MAPMAKER_GRID_EDITOR);
    if(!MAPMAKER_PTR_OK(editor,0x2F80)) return;
    p=WORD_AT(editor+0x2F7Cu);
    for(n=0;p && n<4096u && MAPMAKER_PTR_OK(p,0x48);n++,p=WORD_AT(p+0x40u)) {
        uint32_t rot=WORD_AT(p+0x0Cu),desc;
        int32_t z,h;
        if(rot>3u) continue;
        desc=WORD_AT(p+0x2Cu+rot*4u);
        if(!MAPMAKER_PTR_OK(desc,0x20)) continue;
        z=mapmaker_grid_trunc((float)(int32_t)WORD_AT(editor+0x264u)+FLOAT_AT(p+8u));
        h=(int32_t)WORD_AT(desc+0x1Cu);
        if(h>0 && (z==INT32_MIN || (int64_t)z+h-1+2>=(int64_t)mapmaker_grid_l())) {
            if(mapmaker_grid.refused++<3u)
                fprintf(stderr,"[mapmaker] grid: placement refused, a layer would be above the top level (z %d + %d)\n",(int)z,(int)h);
            g_eax=0;
            return;
        }
    }
}

/* ---- overlapping tiles ------------------------------------------------- */
/* sub_002F8140(x, y, z): solid at cell? Free for the placement probe. */
void pc_mapmaker_overlap_cell(void (*original)(void))
{
    uint32_t ret=MAPMAKER_STACK_OK(4)?WORD_AT(g_esp):0u;  /* before the original pops it */
    original();
    if(ret!=MAPMAKER_GRID_RET_PROBE || !g_eax) return;
    mapmaker_grid_read();
    if(!mapmaker_grid.overlap || !mapmaker_grid_editor()) return;
    if(mapmaker_grid.overlaps++<3u) fprintf(stderr,"[mapmaker] overlap: placing over an occupied cell\n");
    g_eax=0;
}

/* ---- stacked items and story AI ---------------------------------------- */
/* Catalogue category (word at 0x00524F5A + id*0xC4) may share a cell, both
 * as the held item and as an item already there. */
static int mapmaker_stack_allowed(int32_t id)
{
    static const unsigned char allowed[37]={
        0,0,0,0, 0,0,0,0,      /* 0-3 starts, 4-6 flags, 7 */
        1,1,1,1, 1,1,          /* 8 weapons, 9 ammo, 10-13 keycards */
        1,1,1,1,1,             /* 14-18 teleporters (one colour each) */
        1,1,1,                 /* 19 health, 20 armour, 21 power-ups */
        0,0,0,0,               /* 22 single item, 23, 24, 25 movers */
        1,                     /* 26 props */
        0,0,0,0,               /* 27-29 turrets, 30 marker */
        1,0,1,0,1,1};          /* 31 special pickups, 32 marker, 33 cover props, 34, 35, 36 props */
    uint32_t a;
    int32_t category;
    if(id<0 || id>=1024) return 0;
    a=0x00524F5Au+(uint32_t)id*0xC4u;
    if(!MAPMAKER_PTR_OK(a,2)) return 0;
    category=(int16_t)MAPMAKER_GRID_U16(a);
    return category>=0 && category<37 && allowed[category];
}
/* What the editor holds: an item id (+0x2F34 >= 0, which sub_00306460 tests
 * first), else a story AI (signed word +0x2F10 >= 0; its point being placed
 * was taken out of the array, sub_003051F0 0x003056EF..0x00305735). */
#define MAPMAKER_STACK_HELD_NONE (-2)
#define MAPMAKER_STACK_HELD_ITEM (-1)
static int32_t mapmaker_stack_held(uint32_t editor)
{
    int32_t sai;
    if((int32_t)WORD_AT(editor+0x2F34u)>=0)
        return mapmaker_stack_allowed((int32_t)WORD_AT(editor+0x2F34u))?MAPMAKER_STACK_HELD_ITEM:MAPMAKER_STACK_HELD_NONE;
    sai=(int16_t)MAPMAKER_GRID_U16(editor+0x2F10u);
    return sai>=0?sai:MAPMAKER_STACK_HELD_NONE;
}
/* sub_00308260(piece, cell, id): the item already in the cell (a pure
 * list walk). In sub_00303460 (place) answer none, so nothing is replaced
 * or uncounted; the original still runs, so only a real stack is logged. */
void pc_mapmaker_stack_find(void (*original)(void))
{
    uint32_t ret,held;
    mapmaker_grid_read();
    ret=MAPMAKER_STACK_OK(4)?WORD_AT(g_esp):0u;  /* before the original pops it */
    original();
    if(!g_eax || !mapmaker_grid.stack || (ret!=MAPMAKER_STACK_RET_COUNT && ret!=MAPMAKER_STACK_RET_REPLACE) ||
       !mapmaker_grid_editor() || !MAPMAKER_PTR_OK(g_ebp+0x14u,4)) return;
    held=WORD_AT(g_ebp+0x14u);                /* sub_00303460's held item node */
    if(!MAPMAKER_PTR_OK(held,4) || !mapmaker_stack_allowed((int32_t)WORD_AT(held))) return;
    if(MAPMAKER_PTR_OK(g_eax,4) && !mapmaker_stack_allowed((int32_t)WORD_AT(g_eax))) return;  /* a start etc. is replaced as stock */
    if(ret==MAPMAKER_STACK_RET_REPLACE && mapmaker_grid.stacked++<3u)
        fprintf(stderr,"[mapmaker] stack: item added to an occupied cell\n");
    g_eax=0;
}
/* sub_003348C0(cell): per-frame item validity, called by sub_00306460. At
 * 0x003070A2 (an item held) a pass still leaves the valid bit clear when an
 * item is under the cursor (0x003070B4: [editor+0x2F1C] != 0 -> 0x003070C2,
 * which for every allowed category reaches 0x00307175 without setting it).
 * Set the bit here for an allowed held item; nothing later in the frame
 * clears it (the only clear, 0x00306EA1, runs before). The pass itself
 * already proves every item in the cell may share (pc_mapmaker_stack_cell). */
void pc_mapmaker_stack_valid(void (*original)(void))
{
    uint32_t ret=MAPMAKER_STACK_OK(4)?WORD_AT(g_esp):0u,editor;
    original();
    if(ret!=MAPMAKER_STACK_RET_VALID || !(g_eax&0xFFu)) return;
    mapmaker_grid_read();
    if(!mapmaker_grid.stack || !mapmaker_grid_editor()) return;
    editor=WORD_AT(MAPMAKER_GRID_EDITOR);
    if(!MAPMAKER_PTR_OK(editor,0x2F38) || !WORD_AT(editor+0x2F1Cu) ||
       !mapmaker_stack_allowed((int32_t)WORD_AT(editor+0x2F34u))) return;
    WORD_AT(editor+0x25Cu)|=4u;
    if(mapmaker_grid.stack_valid++<3u)
        fprintf(stderr,"[mapmaker] stack: drop allowed over an existing item\n");
}
/* Story AI array as editor.json relocates it (0x288 -> 0x58390, round 3). */
#define MAPMAKER_STACK_SAI 0x58390u /* editor.json: 0x288 -> 0x58390 */
#define MAPMAKER_STACK_SAI_CAP 511u
#define MAPMAKER_STACK_SAI_POINTS 8u /* 0x64 + 8 x 0x10 fills the 0xE4 entry */
/* sub_002F8B40(piece, x|y<<16, z) cdecl (caller add esp,0xC at 0x0030724B):
 * the story AI point in the cell, (sai << 16) | point, or -1 (a pure walk
 * of the array, lowest index first). Its only caller (0x0030723A, story AI
 * mode of sub_00306460) stores it in [editor+0x2F20]; a point there keeps
 * the valid bit clear (0x003072B4) and the drop refuses it (sub_003051F0
 * 0x00305238). While a story AI is held answer -1, the value the frame
 * already uses for "nothing under the cursor". With empty hands answer the
 * HIGHEST index with a point in the cell, so pick-up takes a shared cell
 * apart newest first: on maps saved while linked copies were born on their
 * leader's points, the copy comes off before the leader. */
void pc_mapmaker_stack_sai_point(void (*original)(void))
{
    uint32_t ret,editor,piece=0,count,s,k,first;
    uint16_t x=0,y=0,z=0;
    int args;
    mapmaker_grid_read();
    ret=MAPMAKER_STACK_OK(4)?WORD_AT(g_esp):0u;  /* before the original pops it */
    if((args=MAPMAKER_STACK_OK(16))!=0) {
        piece=WORD_AT(g_esp+4u);
        x=MAPMAKER_GRID_U16(g_esp+8u);y=MAPMAKER_GRID_U16(g_esp+10u);z=MAPMAKER_GRID_U16(g_esp+12u);
    }
    original();
    if((int32_t)g_eax<0 || ret!=MAPMAKER_STACK_RET_SAI_POINT || !mapmaker_grid.stack || !mapmaker_grid_editor()) return;
    editor=WORD_AT(MAPMAKER_GRID_EDITOR);
    if(!MAPMAKER_PTR_OK(editor,0x2F38)) return;
    if(mapmaker_stack_held(editor)<0) {
        if(!args || (int32_t)WORD_AT(editor+0x2F34u)>=0) return;     /* an item in hand: stock */
        count=WORD_AT(editor+0x280u);
        if((int32_t)count<0) count=0;
        if(count>MAPMAKER_STACK_SAI_CAP) count=MAPMAKER_STACK_SAI_CAP;
        first=g_eax>>16;
        if(!MAPMAKER_PTR_OK(editor+MAPMAKER_STACK_SAI,count*0xE4u)) return;
        for(s=count;s-->first+1u;) {
            uint32_t sai=editor+MAPMAKER_STACK_SAI+s*0xE4u,points=WORD_AT(sai+0x58u);
            points=(int32_t)points<0?0u:points>MAPMAKER_STACK_SAI_POINTS?MAPMAKER_STACK_SAI_POINTS:points;
            for(k=0;k<points;k++) {
                uint32_t pt=sai+0x64u+k*0x10u;
                if(WORD_AT(pt)==piece && MAPMAKER_GRID_U16(pt+4u)==x && MAPMAKER_GRID_U16(pt+6u)==y &&
                   MAPMAKER_GRID_U16(pt+8u)==z) {
                    if(mapmaker_grid.stack_newest++<3u)
                        fprintf(stderr,"[mapmaker] stack: pick-up takes the newest story AI on a shared cell\n");
                    g_eax=(s<<16)|k;
                    return;
                }
            }
        }
        return;
    }
    if(mapmaker_grid.stack_sai++<3u)
        fprintf(stderr,"[mapmaker] stack: story AI over another story AI\n");
    g_eax=0xFFFFFFFFu;
}
/* sub_00334770(piece, x, y, z words): may the held item or story AI point
 * go in this cell? For the call, hide (x word = 0x7FFF, restored after)
 * the piece's items in the cell whose category may share (refusal
 * 0x0033482A..0x00334846) and the story AI points in the cell (refusal
 * 0x0033484F..0x00334893; array at editor+MAPMAKER_STACK_SAI, relocated by
 * editor.json, count +0x280), except the held story AI's own points.
 * Starts, flags, turrets, movers and markers stay visible, so nothing
 * joins them and they join nothing. */
#define MAPMAKER_STACK_ITEMS 512u   /* items of one piece in one cell (500 items in all) */
#define MAPMAKER_STACK_MAX (MAPMAKER_STACK_ITEMS+MAPMAKER_STACK_SAI_CAP*MAPMAKER_STACK_SAI_POINTS) /* 4600 */
void pc_mapmaker_stack_cell(void (*original)(void))
{
    static uint32_t hidden[MAPMAKER_STACK_MAX];
    uint32_t editor,piece,item,count,s,k;
    int32_t held;
    uint16_t x,y,z;
    unsigned n=0,i;
    mapmaker_grid_read();
    if(!mapmaker_grid.stack || !mapmaker_grid_editor() || !MAPMAKER_STACK_OK(16)) { original();return; }
    editor=WORD_AT(MAPMAKER_GRID_EDITOR);
    if(!MAPMAKER_PTR_OK(editor,0x2F38) || (held=mapmaker_stack_held(editor))==MAPMAKER_STACK_HELD_NONE) { original();return; }
    piece=WORD_AT(g_esp+4u);
    x=MAPMAKER_GRID_U16(g_esp+8u);y=MAPMAKER_GRID_U16(g_esp+10u);z=MAPMAKER_GRID_U16(g_esp+12u);
    if(MAPMAKER_PTR_OK(piece,0x40)) {
        item=WORD_AT(piece+0x3Cu);
        for(i=0;item && i<4096u && n<MAPMAKER_STACK_ITEMS && MAPMAKER_PTR_OK(item,0x18);i++,item=WORD_AT(item+0x14u))
            if(MAPMAKER_GRID_U16(item+0xCu)==x && MAPMAKER_GRID_U16(item+0xEu)==y && MAPMAKER_GRID_U16(item+0x10u)==z &&
               mapmaker_stack_allowed((int32_t)WORD_AT(item))) {
                MAPMAKER_GRID_U16(item+0xCu)=MAPMAKER_STACK_HIDE;
                hidden[n++]=item+0xCu;
            }
    }
    count=WORD_AT(editor+0x280u);
    if((int32_t)count<0) count=0;
    if(count>MAPMAKER_STACK_SAI_CAP) count=MAPMAKER_STACK_SAI_CAP;
    if(MAPMAKER_PTR_OK(editor+MAPMAKER_STACK_SAI,count*0xE4u))
        for(s=0;s<count;s++) {
            uint32_t sai=editor+MAPMAKER_STACK_SAI+s*0xE4u,points=WORD_AT(sai+0x58u);
            if((int32_t)s==held) continue;
            if((int32_t)points<0) points=0;
            if(points>MAPMAKER_STACK_SAI_POINTS) points=MAPMAKER_STACK_SAI_POINTS;
            for(k=0;k<points && n<MAPMAKER_STACK_MAX;k++) {
                uint32_t pt=sai+0x64u+k*0x10u;
                if(WORD_AT(pt)==piece && MAPMAKER_GRID_U16(pt+4u)==x && MAPMAKER_GRID_U16(pt+6u)==y &&
                   MAPMAKER_GRID_U16(pt+8u)==z) {
                    MAPMAKER_GRID_U16(pt+4u)=MAPMAKER_STACK_HIDE;
                    hidden[n++]=pt+4u;
                }
            }
        }
    original();
    for(i=0;i<n;i++) MAPMAKER_GRID_U16(hidden[i])=x;
}
