/* US 45410066 MapMaker editor: seven floors, an 80 x 80 grid, overlapping
 * tiles and stacked items (logs/limits/features/floors-grid.md and
 * overlap-items.md; docs/mapmaker-grid.md).
 *
 * The editor keeps one occupancy grid, [0x00784004]: a Level-heap array of
 * piece pointers, x + 40*(y + 40*(z+2)), 40 x 40 x 6 levels (z -2..3). Five
 * functions own it and nothing in play reads it: sub_00307CC0 allocates and
 * clears it (editor init), sub_00307C20 clears it (reset), sub_00307CF0
 * writes a piece over its whole footprint box, sub_00307DA0 zeroes that box,
 * sub_00307E60(x, y, z floats) reads one cell (0 = empty, also outside).
 *
 * TSFP_MAPMAKER_LIMITS=1 (with the pools on, as mapmaker_limits.limits):
 * - grid 80 x 80 x 8 levels (z -2..5, 200 KB of Level heap): the five
 *   functions are reimplemented here with per-axis bounds (stock bounds only
 *   the flat index, which would wrap x into the next row);
 * - floors -2..4 can be selected: floor up past 2 is refused by
 *   sub_00306460 (`cmp ecx,2` at 0x00306E14) with text/sound 0x1576 through
 *   sub_002F79E0 (return 0x00306E99); that refusal is turned into the stock
 *   success (floor +1, "below" = old floor, sound 0x157D) up to floor 4;
 * - placement is refused when any layer of a held piece would be above the
 *   top grid level (sub_002F8DE0 tests only the bottom, so stacked
 *   selections could leave the grid; z >= 4 was the stock "7th floor");
 * - the settings block (pc_mapmaker_grid.h) gives the floor menu 7 bars, the
 *   placement bound, view-centre clamp and out-of-grid shading 80.0 and the
 *   widest zoom 100 cells (grid.json redirects the editor's reads there).
 * Without the switch every hook runs its original and the block holds the
 * stock constants. The floor menu's loop count is a stack local
 * (0x0030BF3F, now 7 for both): in stock mode the two extra bars (floors 3
 * and 4, drawn last) are moved off screen after sub_0001ED50 (return
 * 0x0030BFB5) by writing their y slot, which nothing reads after the loop.
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
 * not refuse an occupied cell (matching items' x word hidden as 0x7FFF for
 * the call and restored). Only for allowed categories (weapons, ammo,
 * keycards, health, armour, power-ups, special pickups, props); starts,
 * flags, teleporters, turrets, movers, markers, the single item and unknown
 * categories keep the stock one-per-cell rule.
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
#define MAPMAKER_GRID_WIDE_L 8      /* levels z -2..5: floors -2..4 plus a 2-tall piece on 4 */
#define MAPMAKER_GRID_TOP_FLOOR 4   /* highest selectable floor with the limits */
#define MAPMAKER_GRID_DIM_MAX 64    /* footprint sanity cap per axis */
#define MAPMAKER_GRID_HIDDEN_Y (-100000.f)
#define MAPMAKER_GRID_RET_PROBE 0x002F8F93u
#define MAPMAKER_GRID_RET_FLOOR 0x00306E99u
#define MAPMAKER_GRID_RET_BAR 0x0030BFB5u
#define MAPMAKER_STACK_RET_COUNT 0x003034ADu
#define MAPMAKER_STACK_RET_REPLACE 0x00303696u
#define MAPMAKER_STACK_HIDE 0x7FFFu
#define MAPMAKER_GRID_U16(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

static struct {
    int initialized, wide, overlap, stack;
    unsigned refused, overlaps, stacked, reregistered;
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
    FLOAT_AT(MAPMAKER_GRID_CFG_FLOORS)=w?7.f:FLOAT_AT(0x004760CCu);
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
 * the stock floor-up, to floor 4. A refusal with the floor above -2 can
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
 * sub_0030BE60 without the limits, bars for floors 3 and 4 go off screen. */
void pc_mapmaker_grid_floor_bar(void (*original)(void))
{
    int bar;
    mapmaker_grid_read();  /* early and often: sets the block before any editor code reads it */
    bar=MAPMAKER_STACK_OK(0x20) && WORD_AT(g_esp)==MAPMAKER_GRID_RET_BAR;
    original();
    if(!bar) return;
    if(!mapmaker_grid.wide && (int32_t)g_ebx>2 && MAPMAKER_STACK_OK(0x20))
        FLOAT_AT(g_esp+0x18u)=MAPMAKER_GRID_HIDDEN_Y;
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

/* ---- stacked items ----------------------------------------------------- */
/* Catalogue category (word at 0x00524F5A + id*0xC4) may share a cell. */
static int mapmaker_stack_allowed(int32_t id)
{
    static const unsigned char allowed[37]={
        0,0,0,0, 0,0,0,0,      /* 0-3 starts, 4-6 flags, 7 */
        1,1,1,1, 1,1,          /* 8 weapons, 9 ammo, 10-13 keycards */
        0,0,0,0,0,             /* 14-18 teleporters */
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
/* sub_00308260(piece, cell, id): the item already in the cell. In
 * sub_00303460 (place) answer none, so nothing is replaced or uncounted. */
void pc_mapmaker_stack_find(void (*original)(void))
{
    uint32_t ret,held;
    mapmaker_grid_read();
    ret=MAPMAKER_STACK_OK(4)?WORD_AT(g_esp):0u;
    if(!mapmaker_grid.stack || (ret!=MAPMAKER_STACK_RET_COUNT && ret!=MAPMAKER_STACK_RET_REPLACE) ||
       !mapmaker_grid_editor() || !MAPMAKER_PTR_OK(g_ebp+0x14u,4)) { original();return; }
    held=WORD_AT(g_ebp+0x14u);                /* sub_00303460's held item node */
    if(!MAPMAKER_PTR_OK(held,4) || !mapmaker_stack_allowed((int32_t)WORD_AT(held))) { original();return; }
    if(ret==MAPMAKER_STACK_RET_REPLACE && mapmaker_grid.stacked++<3u)
        fprintf(stderr,"[mapmaker] stack: item added to an occupied cell\n");
    g_eax=0;
    MAPMAKER_GUEST_RET();
}
/* sub_00334770(piece, x, y, z words): may the held item go in this cell?
 * Hide the piece's items in the cell from refusal 4 for the call. */
#define MAPMAKER_STACK_MAX 256u
void pc_mapmaker_stack_cell(void (*original)(void))
{
    uint32_t editor,piece,item,hidden[MAPMAKER_STACK_MAX];
    uint16_t x,y,z;
    unsigned n=0,i;
    mapmaker_grid_read();
    if(!mapmaker_grid.stack || !mapmaker_grid_editor() || !MAPMAKER_STACK_OK(16)) { original();return; }
    editor=WORD_AT(MAPMAKER_GRID_EDITOR);
    if(!MAPMAKER_PTR_OK(editor,0x2F38) || !mapmaker_stack_allowed((int32_t)WORD_AT(editor+0x2F34u))) { original();return; }
    piece=WORD_AT(g_esp+4u);
    x=MAPMAKER_GRID_U16(g_esp+8u);y=MAPMAKER_GRID_U16(g_esp+10u);z=MAPMAKER_GRID_U16(g_esp+12u);
    if(MAPMAKER_PTR_OK(piece,0x40)) {
        item=WORD_AT(piece+0x3Cu);
        for(i=0;item && i<4096u && n<MAPMAKER_STACK_MAX && MAPMAKER_PTR_OK(item,0x18);i++,item=WORD_AT(item+0x14u))
            if(MAPMAKER_GRID_U16(item+0xCu)==x && MAPMAKER_GRID_U16(item+0xEu)==y && MAPMAKER_GRID_U16(item+0x10u)==z) {
                MAPMAKER_GRID_U16(item+0xCu)=MAPMAKER_STACK_HIDE;
                hidden[n++]=item;
            }
    }
    original();
    for(i=0;i<n;i++) MAPMAKER_GRID_U16(hidden[i]+0xCu)=x;
}
