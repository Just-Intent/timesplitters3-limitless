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
 * - names: the palette caption (0x0032E5CF, tile [palette+0x90]), the tile
 *   label over the grid (0x00311718: piece editor +0x2F18 under the cursor,
 *   else the held piece +0x2F7C) and the selected piece's name (sub_00317C40,
 *   0x00317C5B) read the tile name text (catalogue +8, tile 8 = 0x0A3F
 *   "Small Open") through sub_0003D7B0; for an open entry/piece the hook
 *   returns "Small Open (No Roof)" / "Small Open (Open Air)". The number in
 *   front is the catalogue's +4 (tiles 1..45 in palette order): the caption
 *   draws "%d. " (0x0049F838, sub_0032D9F0; -1 = none in the 3-category
 *   palette), the grid label "%d.%s" (0x0049F7C0, sub_00151030). The stock
 *   formats are kept and the open entries get 46 / 47: "46. Small Open (No
 *   Roof)" in the palette, "46.Small Open (No Roof)" over the grid (stock
 *   "1.Small Open"); the selected piece's name has no number in stock either.
 * - piece records are 0x28 bytes + pads*8 (word +0x24) + the piece's items
 *   (word +0x1E of them, each (byte +0x14 * 4 + 0x1B) & ~3 bytes); a walk
 *   that skipped only +0x24 stopped at the first piece carrying items (a
 *   start point, a weapon): no +0x26 mark after it was stored. Play was open
 *   anyway (the writer's own sub_002FC680 call is hooked), but the editor
 *   reload after the play (editor start sub_00302A60 -> sub_00301A90
 *   ([0x00784014], 2) at 0x00303076; the pool is rebuilt, pointers change)
 *   found no marks, so the next rebuild closed every tile.
 * Play reads record +8 unchanged (sub_0008A780 0x0008A91D); it never
 * recomputes the mask, so No roof and Open air (unbounded) need no play-time
 * hook. Open air (bounded), mode 3 / mark 0x5203 (user request 2026-11-01,
 * set from the tile roof menu only), saves the No roof mask so the walls keep
 * their collision; the play hooks at the end of this file leave its side
 * nodes out of the draw. Marks 0x5201/0x5202 keep their meaning, so maps
 * saved before the split load unchanged (0x5202 = Open air (unbounded)).
 * The saved tile is the ordinary base tile id with the open mask in +8, so the
 * stock game loads and plays it open (no new id: the 45-entry tables
 * 0x00783F40, 0x00785000, 0x00789440 and 0x00785080 + (tileset*45)*64 index
 * every tile id). Requires the includer's WORD_AT, FLOAT_AT, g_esp, g_eax,
 * xbox_HeapAlloc, mapmaker_limits/mapmaker_limits_read and MAPMAKER_PTR_OK, and
 * guest_call for the fall kill (sub_000BD090). */
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
#define MAPMAKER_ROOF_OPEN_ROOF 1u              /* No roof */
#define MAPMAKER_ROOF_OPEN_AIR 2u               /* Open air (unbounded): no walls, no wall collision */
#define MAPMAKER_ROOF_OPEN_BOUNDED 3u           /* Open air (bounded): walls not drawn, collision kept */
#define MAPMAKER_ROOF_CEILING 0x02000000u
#define MAPMAKER_ROOF_AIR 0x02FFFFFFu           /* ceiling + side banks 0..23; keeps floor 0x01000000 */
#define MAPMAKER_ROOF_SIDES 0x00FFFFFFu         /* side banks only: hidden from the draw of bounded rooms */
#define MAPMAKER_ROOF_MARK 0x5200u              /* record word +0x26: 0x5200 | mode */
#define MAPMAKER_ROOF_RET_VIEW 0x0032FEC4u      /* editor 3D view: sub_002FC680 at 0x0032FEBF */
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
#define MAPMAKER_ROOF_LABEL_BYTES 40u
#define MAPMAKER_ROOF_RET_HOVER 0x0031171Du      /* tile label: sub_0003D7B0 at 0x00311718 */
#define MAPMAKER_ROOF_RET_HOVER_FORMAT 0x00311729u /* sub_00151030("%d.%s", number, name) at 0x00311724 */
#define MAPMAKER_ROOF_HOVER_FORMAT 0x0049F7C0u   /* "%d.%s" */
#define MAPMAKER_ROOF_RET_CAPTION_DRAW 0x0032E5E3u /* sub_0032D9F0 (eax number, ecx name) at 0x0032E5DE */
#define MAPMAKER_ROOF_NUMBER 46u                /* stock tiles are numbered 1..45 (catalogue +4) */
#define MAPMAKER_ROOF_SUFFIX_BYTES 64u
#define MAPMAKER_ROOF_ITEM_MIN 0x18u           /* item sub-record: (count byte +0x14 * 4 + 0x1B) & ~3 */
#define MAPMAKER_ROOF_SCENE_PLAY 100u
#define MAPMAKER_ROOF_ROOMS 256u                /* per-room tables (rooms.json: 256 entries) */
#define MAPMAKER_ROOF_ROOM_MODEL 0x907u         /* room r is drawn by a prop of model 0x907 + r */
#define MAPMAKER_ROOF_PLAY_PIECES 2048u
#define MAPMAKER_ROOF_BUILD_SIZE 0x48u          /* reads +0x14 rooms, +0x28 first room, +0x44 mask */
#define MAPMAKER_ROOF_FLOOR_HEIGHT 5.0f         /* play y = record floor byte +6 * [0x004760CC] */
#define MAPMAKER_ROOF_KILL_DEPTH 20.0f          /* kill this far below the lowest floor (4 floors) */
#define MAPMAKER_ROOF_OUT_DEPTH 5.0f            /* one floor below the lowest floor: out of the map */
#define MAPMAKER_ROOF_OUT_FRAMES 60u            /* ... for this many ticks in a row also kills */
#define MAPMAKER_ROOF_NODE 0xC8u                /* node stride (sub_0008F5E0 0x0008F5EB), mask +0x5C */
#define MAPMAKER_ROOF_NODES_MAX 1024u
#define MAPMAKER_ROOF_ACTORS 0x00900100u        /* live characters (actors-list.json, was 0x007A3580) */
#define MAPMAKER_ROOF_ACTOR_COUNT 0x0074C37Cu
#define MAPMAKER_ROOF_ACTORS_MAX 64u
#define MAPMAKER_ROOF_PLAYERS 0x007B0C48u        /* [ ] + i*0x1584: human player i (0x0003C46D..0x0003C47E) */
#define MAPMAKER_ROOF_PLAYER_SIZE 0x1584u
#define MAPMAKER_ROOF_PLAYER_COUNT 0x00790950u   /* humans (0x000671ED) */
#define MAPMAKER_ROOF_PLAYERS_MAX 4u
#define MAPMAKER_ROOF_SLOTS_FALL (MAPMAKER_ROOF_ACTORS_MAX+MAPMAKER_ROOF_PLAYERS_MAX)
#define MAPMAKER_ROOF_SECOND 60u                 /* ticks between the fall logs */
#define MAPMAKER_ROOF_QUERY_EVERY 6u             /* ticks between room queries per character */
#define MAPMAKER_ROOF_NOROOM_QUERIES 8u          /* in no room this many queries in a row: dead (~0.8 s) */
#define MAPMAKER_ROOF_DROP 1.5f                  /* outside every room and this far under the lowest floor: dead */
#define MAPMAKER_ROOF_ENT_MODEL 0x94u            /* entity: model id (halfword) */
#define MAPMAKER_ROOF_WORM_MODEL 0x5EAu          /* the mansion worm: starts and burrows under the floor */
#define MAPMAKER_ROOF_BODY_LOW 0.25f             /* body box above the feet (y +0x68 is the feet: */
#define MAPMAKER_ROOF_BODY_HIGH 1.5f             /* the hazard kill compares it with the floor) */
#define MAPMAKER_ROOF_BODY_HALF 0.3f
#define MAPMAKER_ROOF_QUERY_OUT 8u               /* room ids returned at most */
/* Palette entries 46 / 47 (tile 8 presets). Bounded has no palette entry. */
static const char mapmaker_roof_names[2][MAPMAKER_ROOF_LABEL_BYTES]={"Small Open (No Roof)","Small Open (Open Air, Unbounded)"};
static const char mapmaker_roof_tails[3][24]={" (No Roof)"," (Open Air, Unbounded)"," (Open Air, Bounded)"};

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
    uint32_t suffixed;             /* guest string: another tile's name + its flag */
    unsigned long long placed, mismatch, unmatched, restored, full, no_list, carried, freed;
} mapmaker_roof;

/* Play time data for bounded open-air rooms (map build sub_0008A780). */
static struct {
    uint8_t mode[MAPMAKER_ROOF_PLAY_PIECES];  /* per piece record, in record order */
    uint32_t mask[MAPMAKER_ROOF_PLAY_PIECES]; /* the record's +8, checked against the build */
    unsigned pieces, next, rooms, bounded, unbounded;
    int building, kill, low, high, armed_logged;
    float kill_y, out_y;                       /* below kill_y: dead; below out_y for OUT_FRAMES: dead */
    uint32_t hide[MAPMAKER_ROOF_ROOMS];        /* node bits left out of the draw, by room index */
    uint8_t shown[MAPMAKER_ROOF_ROOMS];        /* room's hidden-node count logged in this build */
    uint8_t open[MAPMAKER_ROOF_ROOMS];         /* open-air room (bounded or unbounded), by room index */
    unsigned open_rooms, generation;           /* open[] count; play builds seen (pc_mapmaker_openview.inc) */
    uint8_t out[MAPMAKER_ROOF_SLOTS_FALL];     /* ticks below out_y: live-list slots, then human players */
    uint8_t below[MAPMAKER_ROOF_SLOTS_FALL];   /* "below the lowest floor" logged */
    float last_y[MAPMAKER_ROOF_SLOTS_FALL];    /* y at the last once-a-second log */
    uint8_t noroom[MAPMAKER_ROOF_SLOTS_FALL];  /* room queries in a row that found no room */
    uint32_t rooms_seen[MAPMAKER_ROOF_SLOTS_FALL]; /* last query's room count (log) */
    uint32_t query;                            /* guest scratch: box (6 floats) + room ids */
    unsigned long long query_fail, outside_logs;
    unsigned ticks, fall_logs, player_logs;
    unsigned long long builds, mismatch, beyond, malformed, kills, rejected, hidden_logs, falls;
} mapmaker_roof_play;
extern void sub_000BD090(void),sub_00087310(void);

/* Bits the saved and played mask loses (what draw and collision both see). */
static uint32_t mapmaker_roof_clear(unsigned mode)
{
    return mode==MAPMAKER_ROOF_OPEN_AIR?MAPMAKER_ROOF_AIR:
           mode==MAPMAKER_ROOF_OPEN_ROOF || mode==MAPMAKER_ROOF_OPEN_BOUNDED?MAPMAKER_ROOF_CEILING:0u;
}
/* Bits that are not drawn (editor 3D view, palette preview). */
static uint32_t mapmaker_roof_shown(unsigned mode)
{
    return mode==MAPMAKER_ROOF_OPEN_BOUNDED?MAPMAKER_ROOF_AIR:mapmaker_roof_clear(mode);
}
static int mapmaker_roof_known(unsigned mode) { return mode>=MAPMAKER_ROOF_OPEN_ROOF && mode<=MAPMAKER_ROOF_OPEN_BOUNDED; }
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
 * the editor's 3D view, so the idle path is one counter test. The writer
 * (0x00300CA4/0x00300CB5) gets the saved mask, the 3D view (0x0032FEBF) the
 * drawn one: a bounded tile shows without walls but saves them. */
void pc_mapmaker_roof_mask(void (*original)(void))
{
    uint32_t piece=mapmaker_roof.marked && MAPMAKER_STACK_OK(8u)?WORD_AT(g_esp+4u):0u,ret=piece?WORD_AT(g_esp):0u;
    unsigned mode;
    original();
    if(!piece || !(mode=mapmaker_roof_mode(piece)) || !mapmaker_roof_editor()) return;
    g_eax&=~(ret==MAPMAKER_ROOF_RET_VIEW?mapmaker_roof_shown(mode):mapmaker_roof_clear(mode));
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
            /* +0x24 is 0x28 + pads*8 only (0x00300D60); the piece's items
             * (count word +0x1E) follow the pads (0x00300D93..0x0030116E,
             * loader 0x00302260..0x003023F8). */
            uint32_t length=WORD_AT(r+0x24u)&0xFFFFu,items=WORD_AT(r+0x1Cu)>>16,at,k;
            if(length<MAPMAKER_ROOF_RECORD || length>end-r) return -1;
            for(at=r+length,k=0;k<items;k++) {
                uint32_t bytes;
                if(end-at<MAPMAKER_ROOF_ITEM_MIN) return -1;
                bytes=((WORD_AT(at+0x14u)&0xFFu)*4u+0x1Bu)&~3u;
                if(bytes>end-at) return -1;
                at+=bytes;
            }
            if(!fn(r,ctx)) return n;
            r=at;
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
    if((mark&0xFF00u)!=MAPMAKER_ROOF_MARK || !mapmaker_roof_known(mode)) return 1;
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
    mapmaker_roof.preview_applied=mapmaker_roof.preview_saved&~mapmaker_roof_shown(mode);
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
        fprintf(stderr,"[mapmaker] roof: %s tile picked\n",mode==MAPMAKER_ROOF_OPEN_AIR?"open air (unbounded)":"open roof");
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
    else if(ret==MAPMAKER_ROOF_RET_HOVER && MAPMAKER_PTR_OK(editor=WORD_AT(0x007844A8u),0x2F80u) &&
            ((piece=WORD_AT(editor+0x2F18u))!=0 || (piece=WORD_AT(editor+0x2F7Cu))!=0)) mode=mapmaker_roof_mode(piece);
    if((mode!=MAPMAKER_ROOF_OPEN_ROOF && mode!=MAPMAKER_ROOF_OPEN_AIR) || !mapmaker_roof_editor()) { original();return; }
    g_eax=mapmaker_roof.labels+(mode-1u)*MAPMAKER_ROOF_LABEL_BYTES;
    MAPMAKER_GUEST_RET();
}

/* Which label a guest string is (0: none). Only the text hook hands them out. */
static unsigned mapmaker_roof_label_mode(uint32_t s)
{
    if(!mapmaker_roof.labels) return 0;
    return s==mapmaker_roof.labels?MAPMAKER_ROOF_OPEN_ROOF:
           s==mapmaker_roof.labels+MAPMAKER_ROOF_LABEL_BYTES?MAPMAKER_ROOF_OPEN_AIR:0u;
}

/* Any other flagged tile (set from the right-click roof menu,
 * pc_mapmaker_tileflags.inc), and a bounded tile 8 (it has no palette entry
 * and keeps the stock name): the grid label's name gets " (No Roof)" /
 * " (Open Air, Unbounded)" / " (Open Air, Bounded)" in a heap copy; the
 * number stays the tile's own. */
static unsigned mapmaker_roof_hover_mode(void)
{
    uint32_t editor=WORD_AT(0x007844A8u),piece;
    if(!MAPMAKER_PTR_OK(editor,0x2F80u)) return 0;
    if(!(piece=WORD_AT(editor+0x2F18u))) piece=WORD_AT(editor+0x2F7Cu);
    return piece && mapmaker_roof_editor()?mapmaker_roof_mode(piece):0u;
}
static void mapmaker_roof_suffix(unsigned mode)
{
    const char *tail;
    uint32_t name=WORD_AT(g_esp+12u);
    char *out;
    size_t n=0;
    if(!mapmaker_roof_known(mode)) return;
    tail=mapmaker_roof_tails[mode-1u];
    if(!mapmaker_roof.suffixed && !(mapmaker_roof.suffixed=xbox_HeapAlloc(MAPMAKER_ROOF_SUFFIX_BYTES,4u))) return;
    if(!MAPMAKER_PTR_OK(mapmaker_roof.suffixed,MAPMAKER_ROOF_SUFFIX_BYTES) || !MAPMAKER_PTR_OK(name,1u)) return;
    out=(char *)((uintptr_t)mapmaker_roof.suffixed+g_xbox_mem_offset);
    while(n<MAPMAKER_ROOF_SUFFIX_BYTES-sizeof mapmaker_roof_tails[0] && MAPMAKER_PTR_OK(name+n,1u) &&
          (out[n]=*(const char *)((uintptr_t)name+n+g_xbox_mem_offset))!=0) n++;
    memcpy(out+n,tail,strlen(tail)+1u);
    WORD_AT(g_esp+12u)=mapmaker_roof.suffixed;
}

/* WRAP sub_00151030 (sprintf into a static buffer, cdecl). The tile label
 * over the grid (0x0031171D..0x00311724) prints "%d.%s" with the tile's
 * number (catalogue +4, 1..45) and name; for an open piece's label the
 * number becomes 46 / 47, the next numbers of the list. One compare per call
 * while no label exists. */
void pc_mapmaker_roof_format(void (*original)(void))
{
    unsigned mode;
    if(mapmaker_roof.labels && MAPMAKER_STACK_OK(16u) && WORD_AT(g_esp)==MAPMAKER_ROOF_RET_HOVER_FORMAT &&
       WORD_AT(g_esp+4u)==MAPMAKER_ROOF_HOVER_FORMAT && (mode=mapmaker_roof_label_mode(WORD_AT(g_esp+12u)))!=0)
        WORD_AT(g_esp+8u)=MAPMAKER_ROOF_NUMBER+mode-1u;
    else if(mapmaker_roof.marked && MAPMAKER_STACK_OK(16u) && WORD_AT(g_esp)==MAPMAKER_ROOF_RET_HOVER_FORMAT &&
            WORD_AT(g_esp+4u)==MAPMAKER_ROOF_HOVER_FORMAT && (mode=mapmaker_roof_hover_mode())!=0)
        mapmaker_roof_suffix(mode);
    original();
}

/* WRAP sub_0032D9F0 (draw "%d. " + name: eax number or -1, ecx name, cdecl
 * stack arguments). The palette caption (0x0032E5DE) passes catalogue +4 in
 * the 8-category palette and -1 (no number) in the 3-category one. */
void pc_mapmaker_roof_caption(void (*original)(void))
{
    unsigned mode;
    if(mapmaker_roof.labels && MAPMAKER_STACK_OK(4u) && WORD_AT(g_esp)==MAPMAKER_ROOF_RET_CAPTION_DRAW &&
       (int32_t)g_eax>=0 && (mode=mapmaker_roof_label_mode(g_ecx))!=0)
        g_eax=MAPMAKER_ROOF_NUMBER+mode-1u;
    original();
}

/* ---- play: Open air (bounded) ---------------------------------------------
 * A bounded tile is saved like No roof (record +8 keeps its wall bits, so the
 * room's instance mask inst+0x30 has them and the node-mask tests of the
 * geometry queries, 0x00166C77 etc., keep the walls) and is marked 0x5203.
 * Only the draw leaves the side nodes out: sub_0008F5E0 tests the same
 * inst+0x30 per node (0x0008F64A), and every call of it is inside
 * sub_0008F6B0(inst, pass, layer) (0x0008F66F..0x0008F8BC), so that hook
 * clears the side bits of a bounded room's geometry instance for the call and
 * puts them back. A stock game plays the tile as No roof (walls drawn). */

static int mapmaker_roof_play_record(uint32_t record,void *ctx)
{
    uint32_t mark=WORD_AT(record+0x24u)>>16;
    unsigned k=mapmaker_roof_play.pieces,mode=(mark&0xFF00u)==MAPMAKER_ROOF_MARK?mark&0xFFu:0u;
    int floor=(int8_t)(WORD_AT(record+4u)>>16&0xFFu);       /* byte +6 (0x0008A87E) */
    (void)ctx;
    if(!k || floor<mapmaker_roof_play.low) mapmaker_roof_play.low=floor;
    if(!k || floor>mapmaker_roof_play.high) mapmaker_roof_play.high=floor;
    if(mode==MAPMAKER_ROOF_OPEN_AIR) mapmaker_roof_play.unbounded++;
    if(k>=MAPMAKER_ROOF_PLAY_PIECES) return 0;
    mapmaker_roof_play.mode[k]=(uint8_t)(mode==MAPMAKER_ROOF_OPEN_BOUNDED || mode==MAPMAKER_ROOF_OPEN_AIR?mode:0u);
    mapmaker_roof_play.mask[k]=WORD_AT(record+8u);
    if(mode==MAPMAKER_ROOF_OPEN_BOUNDED) mapmaker_roof_play.bounded++;
    mapmaker_roof_play.pieces=k+1u;
    return 1;
}

/* WRAP sub_0008A780 (MapMaker play map build from the container
 * [0x00784014]). It makes one 0x68-byte build entry per piece record, in
 * record order (sub_00083F80 / sub_00085030, 0x0008AAAA/0x0008AACE), then
 * calls sub_00085710(entry) for each in order (0x0008AC70..0x0008ACAB). The
 * records are read before the original; the rooms hook does the rest. */
void pc_mapmaker_roof_play(void (*original)(void))
{
    uint32_t c=0;
    int read=-1;
    memset(mapmaker_roof_play.hide,0,sizeof mapmaker_roof_play.hide);
    memset(mapmaker_roof_play.shown,0,sizeof mapmaker_roof_play.shown);
    memset(mapmaker_roof_play.open,0,sizeof mapmaker_roof_play.open);
    mapmaker_roof_play.open_rooms=0;mapmaker_roof_play.generation++;
    memset(mapmaker_roof_play.out,0,sizeof mapmaker_roof_play.out);
    memset(mapmaker_roof_play.below,0,sizeof mapmaker_roof_play.below);
    memset(mapmaker_roof_play.noroom,0,sizeof mapmaker_roof_play.noroom);
    memset(mapmaker_roof_play.rooms_seen,0,sizeof mapmaker_roof_play.rooms_seen);
    mapmaker_roof_play.ticks=mapmaker_roof_play.fall_logs=mapmaker_roof_play.player_logs=0;
    mapmaker_roof_play.pieces=mapmaker_roof_play.next=mapmaker_roof_play.rooms=0;
    mapmaker_roof_play.bounded=mapmaker_roof_play.unbounded=0;
    mapmaker_roof_play.building=mapmaker_roof_play.kill=mapmaker_roof_play.low=mapmaker_roof_play.high=0;
    mapmaker_roof_play.armed_logged=0;
    mapmaker_limits_read();
    if(mapmaker_limits.limits && (c=WORD_AT(0x00784014u))!=0) {
        read=mapmaker_roof_records(c,mapmaker_roof_play_record,NULL);
        /* A walk that went wrong after some records keeps what it read: the
         * rooms hook checks every entry against its record's mask anyway. */
        if(read<0 && mapmaker_roof_play.malformed++<4u)
            fprintf(stderr,"[mapmaker] roof: map records end unexpectedly after %u pieces (container 0x%08X); using those\n",
                    mapmaker_roof_play.pieces,c);
    }
    if(mapmaker_roof_play.pieces) {
        mapmaker_roof_play.building=mapmaker_roof_play.bounded!=0 || mapmaker_roof_play.unbounded!=0;
        mapmaker_roof_play.kill=mapmaker_roof_play.unbounded!=0;
        mapmaker_roof_play.kill_y=(float)mapmaker_roof_play.low*MAPMAKER_ROOF_FLOOR_HEIGHT-MAPMAKER_ROOF_KILL_DEPTH;
        mapmaker_roof_play.out_y=(float)mapmaker_roof_play.low*MAPMAKER_ROOF_FLOOR_HEIGHT-MAPMAKER_ROOF_OUT_DEPTH;
        if(mapmaker_roof_play.kill && !mapmaker_roof_play.query) {
            uint32_t q=xbox_HeapAlloc(0x18u+MAPMAKER_ROOF_QUERY_OUT*4u,16u);
            if(q && MAPMAKER_PTR_OK(q,0x18u+MAPMAKER_ROOF_QUERY_OUT*4u)) mapmaker_roof_play.query=q;
            else if(mapmaker_roof_play.query_fail++<4u)
                fprintf(stderr,"[mapmaker] roof: no memory for the room query; falls only use the heights\n");
        }
    }
    if(mapmaker_limits.limits && mapmaker_roof_play.builds<32u)
        fprintf(stderr,"[mapmaker] roof: play build: %u pieces (walk %d), %u bounded, %u unbounded, floors %d..%d%s\n",
                mapmaker_roof_play.pieces,read,mapmaker_roof_play.bounded,mapmaker_roof_play.unbounded,
                mapmaker_roof_play.low,mapmaker_roof_play.high,c?"":" (no map container)");
    original();
    if(mapmaker_limits.limits && mapmaker_roof_play.builds++<32u) {
        if(mapmaker_roof_play.bounded)
            fprintf(stderr,"[mapmaker] roof: %u bounded open-air room%s (walls hidden, collision kept; %u of %u pieces checked)\n",
                    mapmaker_roof_play.rooms,mapmaker_roof_play.rooms==1u?"":"s",mapmaker_roof_play.next,mapmaker_roof_play.pieces);
        if(mapmaker_roof_play.kill)
            fprintf(stderr,"[mapmaker] roof: fall check on: characters die below y %.1f, or below y %.1f for %u ticks (lowest floor %d)\n",
                    (double)mapmaker_roof_play.kill_y,(double)mapmaker_roof_play.out_y,MAPMAKER_ROOF_OUT_FRAMES,mapmaker_roof_play.low);
        fflush(stderr);
    }
    mapmaker_roof_play.building=0;
}

/* WRAP sub_00085710 (store one piece's per-room data, cdecl: the build entry;
 * table[+0x28 + j] = +0x44 for j < +0x14). Only during a build that has
 * open-air pieces: their rooms j are marked open (pc_mapmaker_openview.inc
 * draws across them); bounded ones also hide their side nodes in the draw.
 * Entry k is record k (0x0008AC70..0x0008ACAB); its +0x44
 * is expected to be the record's +8 (0x0008A91D -> the copy's +0x14 ->
 * +0x44 at 0x00084F98 / 0x000856EB), and a difference is logged. */
void pc_mapmaker_roof_rooms(void (*original)(void))
{
    uint32_t build=mapmaker_roof_play.building && MAPMAKER_STACK_OK(8u)?WORD_AT(g_esp+4u):0u,base,count,j;
    unsigned k;
    original();
    if(!build) return;
    k=mapmaker_roof_play.next++;
    if(k>=mapmaker_roof_play.pieces || !MAPMAKER_PTR_OK(build,MAPMAKER_ROOF_BUILD_SIZE)) return;
    /* The order is fixed by the code (one call per record, in order); the
     * mask comparison is a cross-check that is only logged. */
    if(WORD_AT(build+0x44u)!=mapmaker_roof_play.mask[k] && mapmaker_roof_play.mismatch++<4u)
        fprintf(stderr,"[mapmaker] roof: room build entry %u mask 0x%08X, its record's +8 0x%08X\n",k,
                WORD_AT(build+0x44u),mapmaker_roof_play.mask[k]);
    if(mapmaker_roof_play.mode[k]!=MAPMAKER_ROOF_OPEN_BOUNDED && mapmaker_roof_play.mode[k]!=MAPMAKER_ROOF_OPEN_AIR) return;
    base=WORD_AT(build+0x28u);count=WORD_AT(build+0x14u);
    for(j=0;(int32_t)count>0 && j<count;j++) {
        if(base+j>=MAPMAKER_ROOF_ROOMS) { mapmaker_roof_play.beyond++;break; }
        if(!mapmaker_roof_play.open[base+j]) mapmaker_roof_play.open_rooms++;
        mapmaker_roof_play.open[base+j]=1;          /* open sides: drawn across (openview) */
        if(mapmaker_roof_play.mode[k]!=MAPMAKER_ROOF_OPEN_BOUNDED) continue;
        if(!mapmaker_roof_play.hide[base+j]) mapmaker_roof_play.rooms++;
        mapmaker_roof_play.hide[base+j]=MAPMAKER_ROOF_SIDES;
    }
    if(mapmaker_roof_play.mode[k]==MAPMAKER_ROOF_OPEN_BOUNDED && mapmaker_roof_play.hidden_logs<64u) {
        mapmaker_roof_play.hidden_logs++;
        fprintf(stderr,"[mapmaker] roof: bounded piece %u: rooms %u..%u (mask 0x%08X)\n",k,base+1u,base+count,
                mapmaker_roof_play.mask[k]);
    }
}

/* Nodes of an instance (sub_0008F5E0 0x0008F5E6..0x0008F5F6: header
 * [inst+4] holds the count n, node i is at header - n*0xC8 + i*0xC8) that
 * the draw shows with `mask` but not with `mask & ~hide` (0x0008F637..). */
static void mapmaker_roof_count_hidden(uint32_t inst,uint32_t mask,uint32_t hide,unsigned *total,unsigned *hidden)
{
    uint32_t header=WORD_AT(inst+4u),n,i,first,m;
    *total=*hidden=0;
    if(!MAPMAKER_PTR_OK(header,4u) || (n=WORD_AT(header))>MAPMAKER_ROOF_NODES_MAX ||
       !MAPMAKER_PTR_OK(first=header-n*MAPMAKER_ROOF_NODE,n*MAPMAKER_ROOF_NODE)) return;
    *total=n;
    for(i=0;i<n;i++) {
        m=WORD_AT(first+i*MAPMAKER_ROOF_NODE+0x5Cu);
        if(m && (m&mask) && !(m&mask&~hide)) (*hidden)++;
    }
}

/* WRAP sub_0008F6B0 (draw one model instance: inst, pass, layer; cdecl).
 * Runs for every drawn instance: without bounded rooms, or outside MapMaker
 * play, it is one counter test and one scene read. A room's geometry is the
 * prop [inst] with +0x34 = room (1-based, dword, 0x00085D9F), the 16-bit
 * model id +0x94 = 0x907 + room (word: 0x000A69E3 stores it, 0x000A6F4F
 * reads it; +0x96 is another word, 0x000A6897) and +4 = inst
 * (sub_00085CC0 0x00085D6B..0x00085DD0). */
void pc_mapmaker_roof_draw(void (*original)(void))
{
    uint32_t inst,prop,room,hide,mask;
    if(!mapmaker_roof_play.rooms || WORD_AT(0x0079094Cu)!=MAPMAKER_ROOF_SCENE_PLAY || !MAPMAKER_STACK_OK(8u) ||
       !MAPMAKER_PTR_OK(inst=WORD_AT(g_esp+4u),0x34u) || !MAPMAKER_PTR_OK(prop=WORD_AT(inst),0x98u) ||
       (room=WORD_AT(prop+0x34u))-1u>=MAPMAKER_ROOF_ROOMS || !(hide=mapmaker_roof_play.hide[room-1u])) { original();return; }
    if(WORD_AT(prop+4u)!=inst || (WORD_AT(prop+0x94u)&0xFFFFu)!=MAPMAKER_ROOF_ROOM_MODEL+room) {
        /* another prop in a bounded room (expected), or a geometry prop the
         * checks do not recognise: the first few are logged */
        if(mapmaker_roof_play.rejected++<6u)
            fprintf(stderr,"[mapmaker] roof: draw in bounded room %u not the room geometry (prop 0x%08X +4 0x%08X inst 0x%08X model 0x%04X)\n",
                    room,prop,WORD_AT(prop+4u),inst,WORD_AT(prop+0x94u)&0xFFFFu);
        original();
        return;
    }
    mask=WORD_AT(inst+0x30u);
    if(!mapmaker_roof_play.shown[room-1u]) {
        unsigned total,hidden;
        mapmaker_roof_play.shown[room-1u]=1;
        mapmaker_roof_count_hidden(inst,mask,hide,&total,&hidden);
        if(mapmaker_roof_play.hidden_logs<128u) {
            mapmaker_roof_play.hidden_logs++;
            fprintf(stderr,"[mapmaker] roof: room %u drawn bounded: %u of %u nodes hidden (instance mask 0x%08X)\n",
                    room,hidden,total,mask);
        }
    }
    WORD_AT(inst+0x30u)=mask&~hide;
    original();
    if(WORD_AT(inst+0x30u)==(mask&~hide)) WORD_AT(inst+0x30u)=mask;
}

/* ---- play: falling off an Open air (unbounded) tile -------------------------
 * The stock game has no death for a fall into the void: the hazard-floor
 * kills (sub_000D5EB0 0x000D6661.., sub_0015F0D0 0x0015F161..) need a ground
 * polygon, fall damage (sub_000D4100 0x000D44E0..) needs a landing, and the
 * only height kill (y < -100, 0x0015F19B..) is for scene 61. A character that
 * walks off an unbounded tile at the map edge would fall forever. On a map
 * with at least one unbounded tile (mark 0x5202, also maps saved before the
 * split), any live character more than MAPMAKER_ROOF_KILL_DEPTH below the
 * lowest floor dies, through the stock kill-now helper sub_000BD090(entity)
 * (cdecl; hit with no attacker -> sub_000BABA0, scored as a suicide), like
 * the stock loop at 0x00067AF1..0x00067B78 does: list[i] = actor, entity =
 * [actor], alive when health +0x1CC > 0. Position y is entity +0x68 (y up). */

/* Rooms whose box overlaps the character's body (sub_00087310(box, out,
 * max), the room query of the camera and the movers: tree 0x00702E00,
 * box = min xyz, max xyz; returns the count). -1: no query memory. */
static int mapmaker_roof_rooms_at(uint32_t ent)
{
    uint32_t q=mapmaker_roof_play.query;
    float x=FLOAT_AT(ent+0x64u),y=FLOAT_AT(ent+0x68u),z=FLOAT_AT(ent+0x6Cu);
    if(!q) return -1;
    FLOAT_AT(q)=x-MAPMAKER_ROOF_BODY_HALF;FLOAT_AT(q+4u)=y+MAPMAKER_ROOF_BODY_LOW;FLOAT_AT(q+8u)=z-MAPMAKER_ROOF_BODY_HALF;
    FLOAT_AT(q+0xCu)=x+MAPMAKER_ROOF_BODY_HALF;FLOAT_AT(q+0x10u)=y+MAPMAKER_ROOF_BODY_HIGH;FLOAT_AT(q+0x14u)=z+MAPMAKER_ROOF_BODY_HALF;
    return (int)guest_call(sub_00087310,3,q,q+0x18u,MAPMAKER_ROOF_QUERY_OUT);
}

static void mapmaker_roof_kill(unsigned slot,uint32_t ent,const char *why)
{
    float y=FLOAT_AT(ent+0x68u);
    mapmaker_roof_play.out[slot]=mapmaker_roof_play.noroom[slot]=0;
    guest_call(sub_000BD090,1,ent);
    if(mapmaker_roof_play.kills++<16u) {
        fprintf(stderr,"[mapmaker] roof: character %u fell off the map (%s; x %.1f y %.1f z %.1f); killed, health now %.1f\n",
                slot,why,(double)FLOAT_AT(ent+0x64u),(double)y,(double)FLOAT_AT(ent+0x6Cu),(double)FLOAT_AT(ent+0x1CCu));
        fflush(stderr);
    }
}

/* One character (slot: live-list index, or 64 + human player index). Three
 * rules, any one kills: in no room's box for MAPMAKER_ROOF_NOROOM_QUERIES
 * queries in a row (a character that left the map, whether it falls, stops
 * in the air or keeps standing on the last floor height), below kill_y, or
 * below out_y for MAPMAKER_ROOF_OUT_FRAMES ticks. */
static void mapmaker_roof_fall_check(unsigned slot,uint32_t ent,int log_now)
{
    float y=FLOAT_AT(ent+0x68u),health=FLOAT_AT(ent+0x1CCu);
    float floor_y=(float)mapmaker_roof_play.low*MAPMAKER_ROOF_FLOOR_HEIGHT;
    if(!(health>0.0f)) { mapmaker_roof_play.out[slot]=mapmaker_roof_play.noroom[slot]=mapmaker_roof_play.below[slot]=0;return; }
    /* the mansion worm lives under the floor (v0.4.0-mapmaker.27: killed at its
     * spawn, y -4.5 outside every room, its burst left on screen) */
    if(*(uint16_t *)((uintptr_t)(ent+MAPMAKER_ROOF_ENT_MODEL)+g_xbox_mem_offset)==MAPMAKER_ROOF_WORM_MODEL) return;
    if((mapmaker_roof_play.ticks+slot)%MAPMAKER_ROOF_QUERY_EVERY==0u) {
        int rooms=mapmaker_roof_rooms_at(ent);
        if(rooms>=0) mapmaker_roof_play.rooms_seen[slot]=(uint32_t)rooms;
        if(rooms==0) {
            if(!mapmaker_roof_play.noroom[slot] && mapmaker_roof_play.outside_logs++<32u) {
                fprintf(stderr,"[mapmaker] roof: character %u is outside every room (x %.1f y %.1f z %.1f)\n",slot,
                        (double)FLOAT_AT(ent+0x64u),(double)y,(double)FLOAT_AT(ent+0x6Cu));
                fflush(stderr);
            }
            /* .27: a player who walked off dropped to y -2.5 outside every room,
             * then the game put them back on the last floor before 8 queries */
            if(y<floor_y-MAPMAKER_ROOF_DROP) { mapmaker_roof_kill(slot,ent,"dropping outside every room");return; }
            if(++mapmaker_roof_play.noroom[slot]>=MAPMAKER_ROOF_NOROOM_QUERIES) { mapmaker_roof_kill(slot,ent,"outside every room");return; }
        } else if(rooms>0) mapmaker_roof_play.noroom[slot]=0;
    }
    /* once a second, every living character under the lowest floor's height
     * or outside every room: shows whether a fall continues or stops */
    if(log_now && (y<floor_y-1.0f || mapmaker_roof_play.noroom[slot]) && mapmaker_roof_play.fall_logs<240u) {
        mapmaker_roof_play.fall_logs++;
        fprintf(stderr,"[mapmaker] roof: character %u off the floors: y %.2f (%+.2f in 1 s) health %.1f, rooms %u, %u ticks below y %.1f\n",
                slot,(double)y,(double)(y-mapmaker_roof_play.last_y[slot]),(double)health,
                mapmaker_roof_play.rooms_seen[slot],mapmaker_roof_play.out[slot],(double)mapmaker_roof_play.out_y);
        fflush(stderr);
    }
    if(log_now) mapmaker_roof_play.last_y[slot]=y;
    if(!(y<mapmaker_roof_play.out_y)) { mapmaker_roof_play.out[slot]=0;mapmaker_roof_play.below[slot]=0;return; }
    if(!mapmaker_roof_play.below[slot]) {
        mapmaker_roof_play.below[slot]=1;
        if(mapmaker_roof_play.falls++<16u) {
            fprintf(stderr,"[mapmaker] roof: character %u is below the lowest floor (y %.1f)\n",slot,(double)y);
            fflush(stderr);
        }
    }
    if(mapmaker_roof_play.out[slot]<255u) mapmaker_roof_play.out[slot]++;
    if(y<mapmaker_roof_play.kill_y) mapmaker_roof_kill(slot,ent,"below the kill height");
    else if(mapmaker_roof_play.out[slot]>=MAPMAKER_ROOF_OUT_FRAMES) mapmaker_roof_kill(slot,ent,"a floor under the map");
}

/* WRAP sub_0003C440 (game tick, no arguments; called once per frame from
 * sub_0003D370 at 0x0003D3CA). Outside MapMaker play, or on a map without an
 * unbounded tile, one flag test. A character dies when it is below kill_y,
 * or below out_y (a floor under the lowest floor, outside the map) for
 * MAPMAKER_ROOF_OUT_FRAMES ticks in a row, or when its body overlaps no
 * room for MAPMAKER_ROOF_NOROOM_QUERIES room queries in a row (a fall may
 * slow, stop or never start outside the map: the floor probe sub_000D9130
 * searches only the character's room +0x34 through sub_000C3B10 and keeps
 * the last floor height when it finds none). Human players are taken from their player
 * structs ([0x007B0C48] + i*0x1584, entity +0x14 as sub_000D5EB0 reads it at
 * 0x000D5EB5; +4 < 0 is an empty slot), the rest from the live list. */
void pc_mapmaker_roof_tick(void (*original)(void))
{
    uint32_t n,h,i,actor,ent,players,human[MAPMAKER_ROOF_PLAYERS_MAX]={0};
    int log_now;
    original();
    if(!mapmaker_roof_play.kill || WORD_AT(0x0079094Cu)!=MAPMAKER_ROOF_SCENE_PLAY) return;
    log_now=mapmaker_roof_play.ticks%MAPMAKER_ROOF_SECOND==0u;
    n=WORD_AT(MAPMAKER_ROOF_ACTOR_COUNT);
    if(n>MAPMAKER_ROOF_ACTORS_MAX) n=MAPMAKER_ROOF_ACTORS_MAX;
    h=WORD_AT(MAPMAKER_ROOF_PLAYER_COUNT);
    if(h>MAPMAKER_ROOF_PLAYERS_MAX) h=MAPMAKER_ROOF_PLAYERS_MAX;
    players=WORD_AT(MAPMAKER_ROOF_PLAYERS);
    for(i=0;i<h && MAPMAKER_PTR_OK(players,(i+1u)*MAPMAKER_ROOF_PLAYER_SIZE);i++) {
        uint32_t pl=players+i*MAPMAKER_ROOF_PLAYER_SIZE;
        if((int32_t)WORD_AT(pl+4u)<0 || !MAPMAKER_PTR_OK(ent=WORD_AT(pl+0x14u),0x1D0u)) continue;
        human[i]=ent;
    }
    if(!mapmaker_roof_play.armed_logged) {
        mapmaker_roof_play.armed_logged=1;
        fprintf(stderr,"[mapmaker] roof: fall check running: %u live characters, %u human players",n,h);
        for(i=0;i<h;i++)
            if(human[i]) fprintf(stderr,", player %u at y %.1f health %.1f",i,(double)FLOAT_AT(human[i]+0x68u),(double)FLOAT_AT(human[i]+0x1CCu));
        fprintf(stderr,"\n");
        fflush(stderr);
    }
    /* the first 10 seconds: player 0's height once a second (standing height) */
    if(log_now && human[0] && mapmaker_roof_play.player_logs<10u) {
        mapmaker_roof_play.player_logs++;
        fprintf(stderr,"[mapmaker] roof: player 0 at x %.1f y %.2f z %.1f health %.1f, rooms %d\n",(double)FLOAT_AT(human[0]+0x64u),
                (double)FLOAT_AT(human[0]+0x68u),(double)FLOAT_AT(human[0]+0x6Cu),(double)FLOAT_AT(human[0]+0x1CCu),mapmaker_roof_rooms_at(human[0]));
    }
    for(i=0;i<h;i++) if(human[i]) mapmaker_roof_fall_check(MAPMAKER_ROOF_ACTORS_MAX+i,human[i],log_now);
    for(i=0;i<n;i++) {
        uint32_t k;
        actor=WORD_AT(MAPMAKER_ROOF_ACTORS+i*4u);
        if(!MAPMAKER_PTR_OK(actor,4u) || !MAPMAKER_PTR_OK(ent=WORD_AT(actor),0x1D0u) || WORD_AT(ent+0x7Cu)!=actor) continue;
        for(k=0;k<h && human[k]!=ent;k++) {}
        if(k<h) continue;                                   /* a human player, checked above */
        mapmaker_roof_fall_check(i,ent,log_now);
    }
    mapmaker_roof_play.ticks++;
}

#ifdef TSFP_MAPMAKER_ROOF_TEST
static void mapmaker_roof_test_reset(void) { memset(&mapmaker_roof,0,sizeof mapmaker_roof); }
#endif
