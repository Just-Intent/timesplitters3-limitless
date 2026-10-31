/* US 45410066 MapMaker "Open roof" and "Open air" tiles (docs/mapmaker/mapmaker-sky.md,
 * "Open-air tiles"; research logs/limits/features/sky-tile.md).
 *
 * A tile room is one model whose nodes are hidden when their mask (+0x5C)
 * shares no bit with the instance mask (sub_0008F5E0, 0x0008F64A). In play
 * that mask is the piece record's +8 (sub_0008A780 0x0008A91D -> sub_00085710
 * 0x000857A3 -> inst+0x30 0x00085DD0), computed by the editor's writer with
 * sub_002FC680 (0x00300CA4/0x00300CB5). Bits (CONFIRMED in sub_00311C00 and
 * sub_002FC680): a connection point of type 5 (up) sets 0x02000000
 * (0x00311C2D, "closed top"; clearing it removed the ceiling in game), type 6
 * (down) sets 0x01000000 (0x00311C3C, floor), side points set
 * 1 << (bank*12 + side*3 + slot) (0x00311C67.., 0x002FC95E, 0x002FD20D),
 * i.e. bits 0..23 are the wall/doorway nodes of the 4 sides; bits 26..31 come
 * from attached decorations (0x002FD8F4..0x002FD917).
 *
 * With TSFP_MAPMAKER_LIMITS=1, in the editor (scene 102):
 * - the last tile category of the palette (sub_00316B50 reads the 12-byte
 *   category tables 0x0052DAC0 (8 rows, editor +0x200 set) / 0x0052DB20
 *   (3 rows) only at 0x00316C1E/0x00316C26) gets two more entries while the
 *   menu is built: tile MAPMAKER_ROOF_BASE_TILE as "Open roof" and as "Open
 *   air". The table is restored right after the build. The items are told
 *   apart by pointer (sub_000792B0 returns the item; the menu's highlighted
 *   item is +0x198 and +0x18C its data +0x14, 0x00077061..0x0007706A);
 * - picking one (sub_00316500, which creates the held piece at editor
 *   +0x2F7C, 0x0031659E) marks that editor piece; the hover preview
 *   (sub_00316920 sets the palette instance mask at 0x00316AF8..0x00316B16)
 *   shows the same mask;
 * - for marked pieces sub_002FC680 returns the mask without 0x02000000 (open
 *   roof) or without 0x02FFFFFF (open air: ceiling and all side nodes; the
 *   floor bit 24 and decorations stay), and sub_002FC470 without 0x02000000,
 *   so the writer also stores the record's +0x23 "exposed" byte
 *   (0x00300C2F..0x00300C93). The editor's 3D view uses sub_002FC680 too
 *   (0x0032FEBF; it always hides bit 25 there, 0x0032FEEF);
 * - after every rebuild (sub_003006C0) the marked pieces' records get word
 *   +0x26 = 0x5201 / 0x5202 and their +8 is cleared again (the catalogue
 *   callback at 0x00300CD7 ORs into +8 after the mask). The writer never
 *   stores +0x26/+0x27 of a piece record and no piece-chunk reader reads them
 *   (decoder check in tests/mapmaker/test_mapmaker_roof.py); the compiler zeroes the
 *   container first (0x0030070D). After a load (sub_00301A90; undo reloads
 *   the same way, sub_00337600) the marks are put back on the matching
 *   pieces. The editor piece pool is 0x48-byte records from one constructor
 *   (sub_00307FD0, callers 0x003021C1 loader and 0x0031658E palette), which
 *   forgets a reused pointer;
 * - the mark follows the piece when the editor copies it: placing the held
 *   piece (sub_003037A0) and picking up a placed one (0x002F9A3E..0x002F9CC7)
 *   allocate a new piece (sub_00307EC0) and `rep movsd` the 0x48 bytes, then
 *   call sub_002F95E0(old, new) for a move or sub_00308120(src, dst) for a
 *   copy; both pass the mark on. sub_00308480 (free one piece; sub_00308560
 *   frees a held list through it) forgets it. Before this, only the held
 *   piece was marked and every placed tile was a plain tile 8;
 * - the palette caption (0x0032E5CF, tile [palette+0x90]) and the selected
 *   piece's name (sub_00317C40, 0x00317C5B) read the tile name text
 *   (catalogue +8, tile 8 = 0x0A3F "Small Open") through sub_0003D7B0; for an
 *   open entry/piece the hook returns "Small Open (No Roof)" / "Small Open
 *   (Open Air)" instead (the stock names use suffixes such as "S" and
 *   "Alt.", no numbers).
 * Play reads record +8 unchanged (sub_0008A780 0x0008A91D); it never
 * recomputes the mask, so no play-time hook is needed.
 * The saved tile is the ordinary base tile id with the open mask in +8, so the
 * stock game loads and plays it open (no new id: the 45-entry tables
 * 0x00783F40, 0x00785000, 0x00789440 and 0x00785080 + (tileset*45)*64 index
 * every tile id). Requires the includer's WORD_AT, FLOAT_AT, g_esp, g_eax,
 * xbox_HeapAlloc, mapmaker_limits/mapmaker_limits_read and MAPMAKER_PTR_OK. */
#include <stdio.h>
#include <string.h>

#ifndef MAPMAKER_PTR_OK
#define MAPMAKER_PTR_OK(p,n) ((((uint32_t)(p))>=0x00010000u && ((uint32_t)(p))<0x04000000u-(uint32_t)(n)) || \
                              (((uint32_t)(p))>=0x80000000u && ((uint32_t)(p))<0x84000000u-(uint32_t)(n)))
#endif
#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) MAPMAKER_PTR_OK(g_esp,(n))
#endif
#ifndef MAPMAKER_GUEST_RET
#define MAPMAKER_GUEST_RET() (g_esp+=4u) /* see pc_mapmaker_limits.inc */
#endif

#define MAPMAKER_ROOF_BASE_TILE 8u              /* 1x1, four sides + up (connections 0x00528F48) */
#define MAPMAKER_ROOF_OPEN_ROOF 1u
#define MAPMAKER_ROOF_OPEN_AIR 2u
#define MAPMAKER_ROOF_CEILING 0x02000000u
#define MAPMAKER_ROOF_AIR 0x02FFFFFFu           /* ceiling + side banks 0..23; keeps floor 0x01000000 */
#define MAPMAKER_ROOF_MARK 0x5200u              /* record word +0x26: 0x5200 | mode */
#define MAPMAKER_ROOF_RECORD 0x28u
#define MAPMAKER_ROOF_CATS_FULL 0x0052DAC0u     /* 8 x {text, list, count} */
#define MAPMAKER_ROOF_CATS_BASIC 0x0052DB20u    /* 3 x {text, list, count} */
#define MAPMAKER_ROOF_RET_MENU_ADD 0x00316C84u  /* sub_00316B50 -> sub_000792B0 */
#define MAPMAKER_ROOF_LIST_MAX 32u
#define MAPMAKER_ROOF_SLOTS 2048u               /* > the 1024-piece pool */
#define MAPMAKER_ROOF_CONTAINER_MAX 0x00080000u
#define MAPMAKER_ROOF_LIST_STEPS 4096u
#define MAPMAKER_ROOF_NAME_TEXT 0x0A3Fu         /* catalogue[8] +8 (0x00529F18): "Small Open" */
#define MAPMAKER_ROOF_RET_CAPTION 0x0032E5D4u   /* palette caption, sub_0003D7B0 at 0x0032E5CF */
#define MAPMAKER_ROOF_RET_PIECE_NAME 0x00317C60u /* sub_00317C40: name of piece editor +0x2F18 */
#define MAPMAKER_ROOF_LABEL_BYTES 32u
static const char mapmaker_roof_names[2][MAPMAKER_ROOF_LABEL_BYTES]={"Small Open (No Roof)","Small Open (Open Air)"};

static struct {
    uint32_t key[MAPMAKER_ROOF_SLOTS];
    uint8_t mode[MAPMAKER_ROOF_SLOTS];
    unsigned keys, marked;
    /* palette */
    uint32_t list;                 /* guest buffer for the patched category list */
    int building;
    uint32_t ordinal, first, item[2];
    uint32_t preview_inst, preview_saved, preview_applied;
    unsigned preview_mode;         /* open entry highlighted in the palette */
    uint32_t labels;               /* guest strings, 2 x MAPMAKER_ROOF_LABEL_BYTES */
    unsigned long long placed, mismatch, unmatched, restored, full, no_list, carried, freed;
} mapmaker_roof;

static uint32_t mapmaker_roof_clear(unsigned mode)
{
    return mode==MAPMAKER_ROOF_OPEN_AIR?MAPMAKER_ROOF_AIR:mode==MAPMAKER_ROOF_OPEN_ROOF?MAPMAKER_ROOF_CEILING:0u;
}
static int mapmaker_roof_editor(void)
{
    mapmaker_limits_read();
    return mapmaker_limits.limits && WORD_AT(0x0079094Cu)==102u;
}

static unsigned mapmaker_roof_slot(uint32_t piece)
{
    unsigned i=(unsigned)(piece>>3)&(MAPMAKER_ROOF_SLOTS-1u),n;
    for(n=0;n<MAPMAKER_ROOF_SLOTS;n++,i=(i+1u)&(MAPMAKER_ROOF_SLOTS-1u))
        if(!mapmaker_roof.key[i] || mapmaker_roof.key[i]==piece) return i;
    return MAPMAKER_ROOF_SLOTS;
}
static unsigned mapmaker_roof_mode(uint32_t piece)
{
    unsigned i;
    if(!mapmaker_roof.marked || !piece) return 0;
    i=mapmaker_roof_slot(piece);
    return i<MAPMAKER_ROOF_SLOTS && mapmaker_roof.key[i]==piece?mapmaker_roof.mode[i]:0u;
}
static void mapmaker_roof_set(uint32_t piece,unsigned mode)
{
    unsigned i=mapmaker_roof_slot(piece);
    if(i>=MAPMAKER_ROOF_SLOTS) {
        if(mapmaker_roof.full++<4u) fprintf(stderr,"[mapmaker] roof: piece table full; tile 0x%08X stays closed\n",piece);
        return;
    }
    if(!mapmaker_roof.key[i]) { mapmaker_roof.key[i]=piece;mapmaker_roof.keys++; }
    if(!mapmaker_roof.mode[i] && mode) mapmaker_roof.marked++;
    else if(mapmaker_roof.mode[i] && !mode) mapmaker_roof.marked--;
    mapmaker_roof.mode[i]=(uint8_t)mode;
}
static void mapmaker_roof_forget_all(void)
{
    memset(mapmaker_roof.key,0,sizeof mapmaker_roof.key);
    memset(mapmaker_roof.mode,0,sizeof mapmaker_roof.mode);
    mapmaker_roof.keys=mapmaker_roof.marked=0;
}

/* WRAP sub_002FC680 (piece mask: piece, record or 0). Per placed piece in
 * the editor's 3D view, so the idle path is one counter test. */
void pc_mapmaker_roof_mask(void (*original)(void))
{
    uint32_t piece=mapmaker_roof.marked && MAPMAKER_STACK_OK(8u)?WORD_AT(g_esp+4u):0u;
    unsigned mode;
    original();
    if(!piece || !(mode=mapmaker_roof_mode(piece)) || !mapmaker_roof_editor()) return;
    g_eax&=~mapmaker_roof_clear(mode);
}

/* WRAP sub_002FC470 (vertical bits only: piece). */
void pc_mapmaker_roof_reduced(void (*original)(void))
{
    uint32_t piece=mapmaker_roof.marked && MAPMAKER_STACK_OK(8u)?WORD_AT(g_esp+4u):0u;
    original();
    if(!piece || !mapmaker_roof_mode(piece) || !mapmaker_roof_editor()) return;
    g_eax&=~MAPMAKER_ROOF_CEILING;
}

/* WRAP sub_00307FD0 (editor piece constructor, returns the zeroed piece). */
void pc_mapmaker_roof_piece_new(void (*original)(void))
{
    original();
    if(mapmaker_roof.marked && mapmaker_roof_mode(g_eax)) mapmaker_roof_set(g_eax,0);
}

/* WRAP sub_002FEBF0 (editor map reset; the loader calls it first). */
void pc_mapmaker_roof_reset(void (*original)(void))
{
    original();
    if(mapmaker_roof.keys) mapmaker_roof_forget_all();
}

/* The editor copies pieces instead of moving them: placing the held piece
 * (sub_003037A0: sub_00307EC0, rep movsd 0x12 dwords at 0x0030387C, then
 * sub_002F95E0(held, new) at 0x0030387E and sub_00308560(held) at
 * 0x0030398A) and picking up a placed piece (0x002F9A3E/0x002F9BE9/
 * 0x002F9C8F; sub_002F95E0(placed, held) and sub_00308480(placed) for a
 * move, sub_00308120(placed, held) for a copy). Both helpers are cdecl with
 * (source, destination). */
static void mapmaker_roof_carry(void)
{
    uint32_t from,to;
    unsigned mode;
    if(!mapmaker_roof.marked || !MAPMAKER_STACK_OK(12u)) return;
    from=WORD_AT(g_esp+4u);to=WORD_AT(g_esp+8u);
    if(!from || !to || from==to || !(mode=mapmaker_roof_mode(from))) return;
    if(mapmaker_roof_mode(to)!=mode) { mapmaker_roof_set(to,mode);mapmaker_roof.carried++; }
}

/* WRAP sub_002F95E0 (editor: replace references to a piece, old -> new). */
void pc_mapmaker_roof_move(void (*original)(void))
{
    mapmaker_roof_carry();
    original();
}

/* WRAP sub_00308120 (editor: copy a piece's item list, src -> dst). */
void pc_mapmaker_roof_copy(void (*original)(void))
{
    mapmaker_roof_carry();
    original();
}

/* WRAP sub_00308480 (editor: free one piece; its pointer goes back to the
 * pool [0x00762CC4] that sub_00307EC0 and sub_00307FD0 allocate from). */
void pc_mapmaker_roof_piece_free(void (*original)(void))
{
    uint32_t piece=mapmaker_roof.marked && MAPMAKER_STACK_OK(8u)?WORD_AT(g_esp+4u):0u;
    original();
    if(piece && mapmaker_roof_mode(piece)) { mapmaker_roof_set(piece,0);mapmaker_roof.freed++; }
}

/* Piece records of a container: calls fn(record) in order; returns the count
 * or -1 for a malformed chunk. */
static int mapmaker_roof_records(uint32_t c,int (*fn)(uint32_t record,void *ctx),void *ctx)
{
    uint32_t off,tag,size,r,end;
    int n=0;
    if(!MAPMAKER_PTR_OK(c,16u) || WORD_AT(c)!=0x00C77667u) return -1;
    for(off=8u;off+8u<=MAPMAKER_ROOF_CONTAINER_MAX;off+=8u+size) {
        if(!MAPMAKER_PTR_OK(c+off,8u)) return -1;
        tag=WORD_AT(c+off);size=WORD_AT(c+off+4u);
        if(tag==0x00C7766Au) return n;
        if(size>MAPMAKER_ROOF_CONTAINER_MAX-off || !MAPMAKER_PTR_OK(c+off+8u,size)) return -1;
        if(tag!=0x00C77669u) continue;
        for(r=c+off+8u,end=r+size;r+MAPMAKER_ROOF_RECORD<=end;n++) {
            uint32_t length=WORD_AT(r+0x24u)&0xFFFFu;
            if(length<MAPMAKER_ROOF_RECORD || length>end-r) return -1;
            if(!fn(r,ctx)) return n;
            r+=length;
        }
    }
    return -1;
}

static int mapmaker_roof_store(uint32_t record,void *ctx)
{
    uint32_t *piece=(uint32_t *)ctx;
    unsigned mode;
    if(!*piece || !MAPMAKER_PTR_OK(*piece,0x48u) || WORD_AT(record)!=WORD_AT(*piece+0x10u)) {
        if(mapmaker_roof.mismatch++<4u) fprintf(stderr,"[mapmaker] roof: rebuilt pieces do not follow the editor list; open tiles not stored\n");
        return 0;
    }
    if((mode=mapmaker_roof_mode(*piece))!=0) {
        WORD_AT(record+0x24u)=(WORD_AT(record+0x24u)&0xFFFFu)|(MAPMAKER_ROOF_MARK|mode)<<16;
        WORD_AT(record+8u)&=~mapmaker_roof_clear(mode);
    }
    *piece=WORD_AT(*piece+0x40u);
    return 1;
}

/* WRAP sub_003006C0 (rebuild: container, mode). The writer emits one record
 * per editor piece in list order ([0x0078400C], next +0x40, 0x00300B88..
 * 0x0030116E). Marks exist only for editor pieces, so any scene that
 * rebuilds the editor's map (the editor itself, or the hand-over to a test
 * play) stores them. */
void pc_mapmaker_roof_compile(void (*original)(void))
{
    uint32_t c=MAPMAKER_STACK_OK(8u)?WORD_AT(g_esp+4u):0u,piece;
    original();
    if(!c || !mapmaker_roof.marked) return;
    mapmaker_limits_read();
    if(!mapmaker_limits.limits) return;
    piece=WORD_AT(0x0078400Cu);
    mapmaker_roof_records(c,mapmaker_roof_store,&piece);
}

static int mapmaker_roof_match(uint32_t record,void *ctx)
{
    uint32_t mark=WORD_AT(record+0x24u)>>16,piece=WORD_AT(0x0078400Cu);
    unsigned mode=mark&0xFFu,steps;
    (void)ctx;
    if((mark&0xFF00u)!=MAPMAKER_ROOF_MARK || (mode!=MAPMAKER_ROOF_OPEN_ROOF && mode!=MAPMAKER_ROOF_OPEN_AIR)) return 1;
    for(steps=0;piece && steps<MAPMAKER_ROOF_LIST_STEPS && MAPMAKER_PTR_OK(piece,0x48u);steps++,piece=WORD_AT(piece+0x40u)) {
        uint32_t xyz=WORD_AT(record+4u);
        if(mapmaker_roof_mode(piece) || WORD_AT(piece+0x10u)!=WORD_AT(record)) continue;
        if((int32_t)FLOAT_AT(piece)!=(int8_t)(xyz&0xFFu) || (int32_t)FLOAT_AT(piece+4u)!=(int8_t)(xyz>>8&0xFFu) ||
           (int32_t)FLOAT_AT(piece+8u)!=(int8_t)(xyz>>16&0xFFu) || (int32_t)WORD_AT(piece+0xCu)!=(int8_t)(xyz>>24)) continue;
        mapmaker_roof_set(piece,mode);
        mapmaker_roof.restored++;
        return 1;
    }
    if(mapmaker_roof.unmatched++<4u) fprintf(stderr,"[mapmaker] roof: an open tile in the map has no loaded piece; left closed\n");
    return 1;
}

/* WRAP sub_00301A90 (editor load: container, flags). It builds editor pieces
 * in whatever scene it runs (the editor start and resume load at 0x00303076
 * are INFERRED to run in scene 102), so only the limits switch gates it. */
void pc_mapmaker_roof_load(void (*original)(void))
{
    uint32_t c=MAPMAKER_STACK_OK(8u)?WORD_AT(g_esp+4u):0u;
    original();
    if(!c) return;
    mapmaker_limits_read();
    if(mapmaker_limits.limits) mapmaker_roof_records(c,mapmaker_roof_match,NULL);
}

/* ---- palette ------------------------------------------------------------ */

/* WRAP sub_00316B50 (tile menu build). */
void pc_mapmaker_roof_menu_init(void (*original)(void))
{
    uint32_t editor,table,last,list,count,rows,i,before=0;
    if(!mapmaker_roof_editor() || !MAPMAKER_PTR_OK(editor=WORD_AT(0x007844A8u),0x204u)) { original();return; }
    rows=WORD_AT(editor+0x200u)?8u:3u;                     /* 0x00316B55..0x00316B6F */
    table=rows==8u?MAPMAKER_ROOF_CATS_FULL:MAPMAKER_ROOF_CATS_BASIC;
    last=table+(rows-1u)*12u;
    list=WORD_AT(last+4u);count=WORD_AT(last+8u);
    for(i=0;i+1u<rows;i++) before+=WORD_AT(table+i*12u+8u);
    if(count>MAPMAKER_ROOF_LIST_MAX || !MAPMAKER_PTR_OK(list,count*4u)) { original();return; }
    if(!mapmaker_roof.list && !(mapmaker_roof.list=xbox_HeapAlloc((MAPMAKER_ROOF_LIST_MAX+2u)*4u,4u))) {
        if(mapmaker_roof.no_list++<4u) fprintf(stderr,"[mapmaker] roof: no memory for the Open roof/Open air entries\n");
        original();
        return;
    }
    if(!mapmaker_roof.labels) {
        uint32_t p=xbox_HeapAlloc(2u*MAPMAKER_ROOF_LABEL_BYTES,4u);
        if(p && MAPMAKER_PTR_OK(p,2u*MAPMAKER_ROOF_LABEL_BYTES)) {
            memcpy((void *)((uintptr_t)p+g_xbox_mem_offset),mapmaker_roof_names,sizeof mapmaker_roof_names);
            mapmaker_roof.labels=p;
        } else if(mapmaker_roof.no_list++<4u) fprintf(stderr,"[mapmaker] roof: no memory for the tile names; they show as Small Open\n");
    }
    for(i=0;i<count;i++) WORD_AT(mapmaker_roof.list+i*4u)=WORD_AT(list+i*4u);
    WORD_AT(mapmaker_roof.list+count*4u)=MAPMAKER_ROOF_BASE_TILE;
    WORD_AT(mapmaker_roof.list+count*4u+4u)=MAPMAKER_ROOF_BASE_TILE;
    mapmaker_roof.item[0]=mapmaker_roof.item[1]=0;mapmaker_roof.preview_mode=0;
    mapmaker_roof.ordinal=0;mapmaker_roof.first=before+count;mapmaker_roof.building=1;
    WORD_AT(last+4u)=mapmaker_roof.list;WORD_AT(last+8u)=count+2u;
    original();
    WORD_AT(last+4u)=list;WORD_AT(last+8u)=count;
    mapmaker_roof.building=0;
}

/* WRAP sub_000792B0 (menu: add a custom item, returns it). Every menu calls
 * it; it acts only during the tile menu build, for its own call site. */
void pc_mapmaker_roof_menu_add(void (*original)(void))
{
    uint32_t ret=mapmaker_roof.building && MAPMAKER_STACK_OK(4u)?WORD_AT(g_esp):0u,k;
    original();
    if(ret!=MAPMAKER_ROOF_RET_MENU_ADD) return;
    k=mapmaker_roof.ordinal++-mapmaker_roof.first;
    if(k<2u) mapmaker_roof.item[k]=g_eax;
}

static unsigned mapmaker_roof_item_mode(uint32_t menu)
{
    uint32_t item;
    if(!MAPMAKER_PTR_OK(menu,0x19Cu) || !(item=WORD_AT(menu+0x198u))) return 0;
    if(WORD_AT(menu+0x18Cu)!=MAPMAKER_ROOF_BASE_TILE) return 0;
    return item==mapmaker_roof.item[0]?MAPMAKER_ROOF_OPEN_ROOF:item==mapmaker_roof.item[1]?MAPMAKER_ROOF_OPEN_AIR:0u;
}
static void mapmaker_roof_preview_restore(void)
{
    uint32_t inst=mapmaker_roof.preview_inst;
    if(inst && MAPMAKER_PTR_OK(inst,0x34u) && WORD_AT(inst+0x30u)==mapmaker_roof.preview_applied)
        WORD_AT(inst+0x30u)=mapmaker_roof.preview_saved;
}

/* WRAP sub_00316920 (tile menu: highlight changed, argument the menu). */
void pc_mapmaker_roof_menu_hover(void (*original)(void))
{
    uint32_t menu=MAPMAKER_STACK_OK(8u)?WORD_AT(g_esp+4u):0u,palette,prop,inst,now;
    unsigned mode;
    original();
    if(!menu || !(mapmaker_roof.item[0]|mapmaker_roof.item[1]) || !mapmaker_roof_editor()) return;
    mode=mapmaker_roof_item_mode(menu);
    mapmaker_roof.preview_mode=mode;
    if(!mode) {
        mapmaker_roof_preview_restore();
        mapmaker_roof.preview_inst=0;
        return;
    }
    palette=WORD_AT(0x00778D34u);
    if(!MAPMAKER_PTR_OK(palette,0x88u) || !MAPMAKER_PTR_OK(prop=WORD_AT(palette+0x84u),8u) ||
       !MAPMAKER_PTR_OK(inst=WORD_AT(prop+4u),0x34u)) return;
    now=WORD_AT(inst+0x30u);
    if(inst!=mapmaker_roof.preview_inst || now!=mapmaker_roof.preview_applied) mapmaker_roof.preview_saved=now;
    mapmaker_roof.preview_inst=inst;
    mapmaker_roof.preview_applied=mapmaker_roof.preview_saved&~mapmaker_roof_clear(mode);
    WORD_AT(inst+0x30u)=mapmaker_roof.preview_applied;
}

/* WRAP sub_00316500 (tile menu: pick, creates the held piece). The stock
 * code derives the new piece's +0x44 from the preview mask (0x00316573..
 * 0x0031657A), so the stock preview value is put back for the call. */
void pc_mapmaker_roof_menu_pick(void (*original)(void))
{
    uint32_t menu=MAPMAKER_STACK_OK(8u)?WORD_AT(g_esp+4u):0u,editor,piece;
    unsigned mode=0;
    int swapped=0;
    if(menu && (mapmaker_roof.item[0]|mapmaker_roof.item[1]) && mapmaker_roof_editor()) mode=mapmaker_roof_item_mode(menu);
    if(mode && mapmaker_roof.preview_inst && MAPMAKER_PTR_OK(mapmaker_roof.preview_inst,0x34u) &&
       WORD_AT(mapmaker_roof.preview_inst+0x30u)==mapmaker_roof.preview_applied) {
        WORD_AT(mapmaker_roof.preview_inst+0x30u)=mapmaker_roof.preview_saved;swapped=1;
    }
    original();
    if(swapped) WORD_AT(mapmaker_roof.preview_inst+0x30u)=mapmaker_roof.preview_applied;
    if(!mode || !MAPMAKER_PTR_OK(editor=WORD_AT(0x007844A8u),0x2F80u)) return;
    piece=WORD_AT(editor+0x2F7Cu);
    if(!MAPMAKER_PTR_OK(piece,0x48u) || WORD_AT(piece+0x10u)!=MAPMAKER_ROOF_BASE_TILE) return;
    mapmaker_roof_set(piece,mode);
    if(mapmaker_roof.placed++<4u)
        fprintf(stderr,"[mapmaker] roof: %s tile picked\n",mode==MAPMAKER_ROOF_OPEN_AIR?"open air":"open roof");
}

/* WRAP sub_0003D7B0 (text id -> string, cdecl, one argument). Runs for
 * every text lookup: without names or for any other id it is one compare.
 * Only the tile 8 name, and only for the palette caption of a highlighted
 * open entry or the name of a selected open piece. */
void pc_mapmaker_roof_text(void (*original)(void))
{
    uint32_t ret,piece,editor;
    unsigned mode=0;
    if(!mapmaker_roof.labels || !MAPMAKER_STACK_OK(8u) || WORD_AT(g_esp+4u)!=MAPMAKER_ROOF_NAME_TEXT) { original();return; }
    ret=WORD_AT(g_esp);
    if(ret==MAPMAKER_ROOF_RET_CAPTION) mode=mapmaker_roof.preview_mode;
    else if(ret==MAPMAKER_ROOF_RET_PIECE_NAME && MAPMAKER_PTR_OK(editor=WORD_AT(0x007844A8u),0x2F1Cu) &&
            (piece=WORD_AT(editor+0x2F18u))!=0) mode=mapmaker_roof_mode(piece);
    if((mode!=MAPMAKER_ROOF_OPEN_ROOF && mode!=MAPMAKER_ROOF_OPEN_AIR) || !mapmaker_roof_editor()) { original();return; }
    g_eax=mapmaker_roof.labels+(mode-1u)*MAPMAKER_ROOF_LABEL_BYTES;
    MAPMAKER_GUEST_RET();
}

#ifdef TSFP_MAPMAKER_ROOF_TEST
static void mapmaker_roof_test_reset(void) { memset(&mapmaker_roof,0,sizeof mapmaker_roof); }
#endif
