# MapMaker rooms: 200 -> 241

The MapMaker's 200-piece limit is the engine's limit of 200 visibility rooms:
every placed tile becomes one room (INFERRED: one room per stock tile, see
"Open"), and the room id is a byte in one engine-wide id space. This change
raises the room limit to 241, the most a byte id allows once the 12 special
ids sit at the top of the byte range. Research: `logs/limits/rooms.md` (main
checkout). Static analysis of the US XBE only; nothing here was run in the game.

Parts:

- `config/guest-code-patches/rooms.json`: 323 operand entries and 19
  relocations (generated from a reviewed site list; `check-tables.py` prints
  "table verified").
- `src/interface/pc_mapmaker_rooms.inc`: five function wrappers (below).
- `src/interface/pc_mapmaker_limits.inc`: `MAPMAKER_LIMIT_PIECES` 200 -> 241 (editor,
  opt-in `TSFP_MAPMAKER_LIMITS=1`; without it the stock 200 stays).
- Tests: `tests/mapmaker/mapmaker_rooms.c` + `tests/mapmaker/test_mapmaker_rooms.py` (fixture,
  18 mutations, table shape).

## Wrappers for the integrator

```
WRAP sub_00087310 -> pc_mapmaker_rooms_query
WRAP sub_0008A780 -> pc_mapmaker_rooms_compile
WRAP sub_00083F80 -> pc_mapmaker_rooms_piece
WRAP sub_00085030 -> pc_mapmaker_rooms_piece
WRAP sub_00242780 -> pc_mapmaker_rooms_props
WRAP sub_001121E0 -> pc_mapmaker_rooms_light_reset
WRAP sub_001123C0 -> pc_mapmaker_rooms_light_unlink
WRAP sub_001124D0 -> pc_mapmaker_rooms_light_colour
WRAP sub_00114030 -> pc_mapmaker_rooms_light_link
WRAP sub_00114290 -> pc_mapmaker_rooms_light_create
```

The first five are cdecl with a plain `ret`. The light hooks (stage 3 of the
255 raise) always call their original and change no stack: `sub_001121E0`
(no arguments; also reached by a tail `jmp` from `0x00112396`),
`sub_001123C0` (light in `eax`; tail `jmp` from `sub_00112990`),
`sub_001124D0` (cdecl light, colour; returns `eax`), `sub_00114030` (room in
`eax`, light at `[esp+4]`), `sub_00114290` (source in `eax`, `ebx`, cdecl
stack arguments, room the 4th; returns the light in `eax`). None wraps another
area's hook, so order does not matter; `sub_00114290` calls `sub_001123C0` and
`sub_00114030` through their wrappers. `sub_00242780` is reached by a tail
`jmp` from `sub_00242BA0`; the wrapper still runs after the body. The table
patches the bodies of `sub_0008A780` (portal buffer), so it must stay a
wrapper calling `sub_0008A780_gen`, not a replacement.

## Room id layout (CONFIRMED sites, uniform shift of 0x29)

| id | stock | new |
| --- | --- | --- |
| none | 0 | 0 |
| real rooms | 1..0xC8 (200) | 1..0xF1 (241) |
| spare | 0xC9 | 0xF2 |
| held by player p | 0xCA+p (p < 4), 0xCE = nobody | 0xF3+p, 0xF7 |
| room of view v | 0xCF+v (v < 6) | 0xF8+v |
| global / unplaced | 0xD5 | 0xFE |
| id-space size | 0xD6 | 0xFF |

0xFF is not used as an id, so a byte read with `movsx` never turns an id into
-1. Every immediate equal to 0xC9..0xD6 in the game's code was reviewed (377
immediates, plus negative forms and `lea` displacements); the room ones are:

- 98 x `0xD5` (global room in prop/particle/effect/view code, the four light
  row widths `imul 0xD5`, the light clear count, the view room-list cap
  `cmp ax,0xD5` at `0x000899CA`). Not rooms (left alone): `0x001364EA`,
  `0x001C5EC3` (sound ids, `sub_000810C0`/`sub_00080F20`), `0x001C9E45` (menu),
  `0x002D0F24` (event switch `sub_001AF950`).
- Players: 9 x `cmp 0xCA` / 9 x `cmp 0xCE` / 8 x `add -0xCA` + `lea [esi-0xCA]`
  (`0x0008CE9D`); 10 x `add 0xCA` (`sub_001B9120` x4, `sub_001BDFD0` x2,
  `sub_001C0200` x2, `sub_001C7C52`, `sub_001C8010`; the last four were missing
  from the research); 2 x `mov [prop+0x34],0xCE`.
- Views: 3 x `add 0xCF` (`0x00086B30`, `0x00086B77`, `0x0011D333`).
- Id-space counts: 12 x `mov ecx,0xD6`, loop ends `0x0009E4E6` (0x358 -> 0x3FC)
  and `0x000A1ED5` (0x354 -> 0x3F8).
- Per-id flag bytes `[0x007B977C]`: allocations 0xE0 -> 0x110 (`0x0008A6FD`,
  `0x0008BA63`), clear 0x35 -> 0x40 dwords (`0x00089734`), displacements
  +0xD5/+0xCA/+0xCF (`0x00089746`, `0x00089756`, `0x00089776`).
- 21 x `cmp r,0xC8` "is a real room" -> 0xF1 (list in rooms.md §1d; all
  re-checked: prop `+0x34`/`+0x38`, view `+0x2A4`, `sub_000888A0` results).
  The other `cmp 0xC8` sites are sound slots, ring counters and so on.

## Static per-room tables (relocated into 0x008C0000..)

| table | stock | new | new size |
| --- | --- | --- | --- |
| `0x00702E20` float/room (piece +0x60) | 200 | `0x008C0000` | 256 |
| `0x00703140` dword/room (+0x50) | 200 | `0x008C0400` | 256 |
| `0x00703460` dword/room (+0x44) | 200 | `0x008C0800` | 256 |
| `0x007038F8` float[3]/room | 200 | `0x008C0C00` | 256 |
| `0x007042B8` draw buckets byte[80][200] | 200 | `0x008C1800` | [80][256] |
| `0x00708138` dword/room (+0x64) | 200 | `0x008C6800` | 256 |
| `0x00708458` room list (`sub_00088CA0`) | 200 | `0x008C6C00` | 256 |
| `0x007B54E0`, `0x7B5840`, `0x7B5BA0`, `0x7B5F00`, `0x7B6260` per-id prop tables | 0xD6 | `0x008C7000`.. step 0x400 | 0xFF (+1) |
| `0x0073C350` light heads dword[3][0xD5] | | `0x008C8400` | [3][0xFE] |
| `0x007AC880` room light colour float[0xD5][3] | | `0x008C9000` | [0xFE][3] |
| `0x0078B860` model -> slot word[0x9D0] | 0x9D0 | `0x008CA000` | 0xA00 |
| `0x0073D910` light round robin per room/16 | 14 | `0x008CB440` | 32 |
| `0x00749890` front-end static view (0xFFC) | | `0x008CD000` | 0x7000 (shadow) |

`0x008CB800..0x008CC800`: two 0x800-byte buffers of the room query.
`0x008CC800..0x008CCC40`: per-id drawn flag `0x007B5180` (stage 2 of the
255 raise, below). Free: `0x008CCC40..0x008CD000` and `0x008D4000..0x008DFFFF`.

Notes on how the relocations are declared (each verified by the full scan):

- Tables packed against the next one are declared 4 bytes short, so the
  shared boundary is one table's base; the new tables keep the same order and
  are contiguous, so the boundary means the same address either way.
- `0x00703460`: `0x0070377C` is the 45-entry table at `0x00703780` indexed from
  1 (`sub_00085920`), kept.
- `0x007042B8`: only the base is referenced in code; the declared range stops at
  `0x00705000`, an unrelated XGRPH constant (`0x003DFCA4`, kept). Data words in
  the range are ASCII text.
- Entries 0xD5 of `0x7B54E0` and `0x7B5F00` are referenced directly
  (`0x007B5834`, `0x007B6254`); each is its own piece mapped to entry 0xFE.
- Light heads: `cmp eax,0x0073C378` (x2) is the end of the 0x64-stride array at
  `0x0073BD9C`, so the first 0x28 bytes are a piece of their own; the clear
  loop's start `0x0073C6A0` is mapped to row 0 entry 0xFD, its end `0x0073D09C`
  (inside the light pool) is an operand entry.
- The model table is referenced from an instruction (`0x00070067`) shared by
  four overlapping functions, which the relocation checker refuses ("two
  relocated operands"), so it is moved by 45 operand entries generated from
  the same full scan (references to its end `0x0078CC00` kept).

## Model-indexed arrays

- `[0x007087A4]` (Level heap, 0x18 B per model id, used for room models from
  0x908): allocation 0xEB80 -> 0xF000 (`0x0008392D`), clear 0x3AE0 -> 0x3C00
  dwords (`0x00083947`). Missing from the research: rooms 201+ would have
  overrun it.
- Model id bounds `cmp 0x9D0` -> 0xA00 at `0x000A5E4E`, `0x001B59B8`,
  `0x001B5C09`, `0x001B618E` (bounds checks before the table; no stock model id
  is 0x9D0 or above, since the table had 0x9D0 entries).
- Prop slots `[0x00791D4C]` (0x30 B): room models 0x9D0..0x9F8 get slots in
  `pc_mapmaker_rooms_props` (scene 100 only), copied from room 200's
  (model 0x9CF), with the geometry fields (+4, +8) cleared for the compiler's
  `sub_0009E630`. INFERRED: all room models share one prop-table template.
  The compile guard keeps the limit at 200 unless all 241 room models have slots.

## View struct (option: shadow arrays at view+0x6000)

The per-room arrays of the 0xFFC view struct (`+0x3BC + 12*id` rects,
`+0xD2C + id` visible flags, `+0xE0C` sorted room list) move to a shadow at
the same offset 0x6000 from every view: `+0x6000` rects (256 x 12), `+0x6C00`
flags (256), `+0x6D00` list (256 words). The six views are allocated together
(`sub_000C61D0`), so the allocation grows from 0x5FF0 to 0xBFF0 and its clear
to 0x2FFC dwords; the stride (0xFFC), the 38 stride sites and the two
divisions by 0xFFC are unchanged. The counts `+0xE08`/`+0xE0A` and the view
prop pointer `+0xE04` stay in place.

Why not grow the struct (option A, ~60 stride/size/init sites plus 2 magic
divisors) or wrap ~27 accessors (option B)? The shadow needs only the 67
displacement swaps that A would also need (`sub_000CC920`'s 12 special-id
flags included), plus the allocation size and clear count: fewer unverifiable
sites, and nothing that divides or strides by the struct size changes. The one
view struct outside the array, the front-end's static `0x00749890`
(`sub_00156A80`), is relocated with room for its own shadow. The other writers
of the current view `[0x007B0CC0]` take it from `[0x007B0CE4]` (CONFIRMED in
`sub_0003C440`, `sub_000C61D0`) or from the player's `+8`, saved copies of it,
`sub_000CCA10` (allocates from the array) and `sub_0022C340`
(`[[0x0075CCA0]+0x10]`); that those all hold array views is INFERRED.

Accessors patched: `sub_00083980`, `sub_000862C0`, `sub_00086480`,
`sub_000865E0`, `sub_00086C40`, `sub_000878A0`, `sub_00087F80`,
`sub_000897D0`, `sub_0009FF30`, `sub_000CC920` (special-id flags),
`sub_000F7E20`, `sub_0010A760`, `sub_0010E410`, `sub_00116EC0`,
`sub_0011A100`, `sub_0011ACF0`, `sub_00176A90`, `sub_00184850` (also the
fixed rooms 0x25/0x35/0x36), `sub_00185EB0`, `sub_001FD950`, `sub_002007D0`,
`sub_00247DA0`, `sub_00342F60`, `sub_00344150`, `sub_0036B610`, and two that
reach the list from a pointer to its counts, `sub_00160A50` (`view+0xE08`, list
at `+4`) and `sub_0025F2A0` (`view+0xE0A`, list at `+2`). Candidates were
found by scanning every instruction for the rect/flag/list displacements and
index patterns (`r*3*4+0x3BC..0x3C8`, `r*3+0xF0`, byte `+0xD2C..0xE0C`,
`+0xE08..0xE0C`, and `add`/`lea` of those offsets) outside `esp` frames; the
rest are other structs.

## Room query and portal flags

- `sub_00087310` collected overlapping rooms in a 0x1B0-byte frame with no
  bound (about 209 words; past that it overwrites its return address).
  `pc_mapmaker_rooms_query` makes the same tree walk (`sub_00157440` on
  `0x00702E00` with the guest callback `sub_00087260`) into a static 1023-word
  buffer, then lists and un-flags rooms exactly as the original (including the
  `max` cut-off, which still clears every flag). Two nesting levels have their
  own buffer; deeper calls run the original.
- `sub_0008A780` kept one flag byte per portal in a 0x800-byte stack buffer
  indexed by portal number without a bound. The frame grows by 0x4000
  (`0x0008A786`), the buffer moves to `esp+0x1280` (six displacements) and its
  fill becomes 0x1000 dwords: 16384 portals. The compile guard keeps the total
  at 0x3FFF.
- Recursion: `sub_00087F80` walks portals recursively with about 0x1E4 bytes of
  stack per room; the recomp gives the guest main thread an 8 MB stack
  (`XBOX_STACK_SIZE`), so 241 levels (~110 KB) are fine (INFERRED: the level
  runs on the main guest thread).
- The room AABB tree (`sub_00086FC0`) stages 0x1C bytes per room in the 32 KB
  scratch block of `sub_0003D6E0(3)` (room for ~1170 rooms) and allocates its
  nodes from the Level heap (`sub_00157970`).

## Lights

`light+0x99` holds the room as a byte (unchanged). Rows of the light heads are
0xD5 -> 0xFE wide (the stock aliasing of a row's last+1 entry to the next
row's entry 0 is kept). `sub_00114030`/`sub_00114290` use `room/16` into the
14-entry `0x0073D910`; rooms 224..241 would have written over the light lists
at `0x0073D948`/`0x0073D94C`, so it is relocated (now 32 entries at
`0x008CB440`). Its pool is sized from `[0x007B975C]/16` (`sub_001121E0`),
already dynamic.

Room ids above 0xFF (stage 3, `pc_mapmaker_rooms.inc`): the record has no
spare byte, so the full id of a light whose room does not fit is kept in a C
side table, by pool index (`[0x0073D950]`, stride 0xB8, at most 500 records)
or for the 0xC0 record `[0x0073D960]`, else by address (a group past the
level's groups indexes records past the pool, as stock does with its special
ids; 256 such records). `sub_00114290` picks the record with the full room
argument; the hook keeps the id while it runs so `sub_00114030` (only called
from `0x00114859`, with the byte) gets it back, and redoes the round-robin
step of `0x00114829` (which used the byte's group) on the full id's group.
`sub_001123C0` unlinks the head entry `[row][id]` itself and lets the original
see room 0; `sub_001124D0` sees room 0 (colour unchanged, the stock result for
an id above the real rooms). `sub_001121E0` clears the table. Lights in rooms
0..0xFF take the original paths unchanged, so with the 241 layout the hooks
only look.

## Story levels

Story levels use the same tables (`sub_0008A330` loads `bg` rooms with no
bound). Nothing changes for them because:

- every story level has at most 200 rooms (INFERRED from correctness: with a
  201st room stock, room 202 would be the player-held id 0xCA);
- every site that compares with, stores or derives a special id moves by the
  same 0x29, and every array indexed by id grows to cover the new ids, so the
  ids keep their meaning (the hard-coded story rooms 0x07..0x5C in
  `sub_001DA990`, `sub_0020FBD0`, `sub_0036B7A0` etc. are real rooms and do not
  move);
- the relocations move bytes, not layout: offsets inside every table are the
  same, except the bucket rows (stride 200 -> 256, consistently in all six
  users) and the light rows (0xD5 -> 0xFE, in all four users);
- `pc_mapmaker_rooms_props` and the compile guard act only in scene 100
  (`sub_0008A780` is only called for scene 100, `sub_0008BC10`).

Room ids are not read from level data: props get their room from position
(`sub_0009FBE0`, `sub_000888A0`) or from the room-model id (`sub_000A6880`:
model - 0x907). Special ids found in data: none (the two data words of room ids
at `0x005476C4`/`0x005476C8` hold real rooms 0x23/0x25).

## System Link

No room id was found in the System Link encoders: the game's network range
(`sub_00350320`..`sub_00359AA0`) reads `+0x34` only as fields of other structs,
the turret sync (`sub_00359550`) sends 3 bytes per turret without rooms, and
`sub_003520B0` only validates local props (room 1..0xD5, now 0xFE). Both sides
compute rooms locally from the same map (INFERRED: positions are what is
synced). Mixed builds are already unsupported for raised maps.

## Open, risks, what blocks more

- One room per tile is INFERRED. The compile log line
  `[MAPMAKER] rooms R of 241, portals P, pieces N` shows it in play: R must
  equal N. If a tile has more rooms, pieces past 241 rooms are dropped (logged
  as DROPPED) rather than overflowing; the editor's piece count would then
  need to count rooms.
- The dropped-piece path zeroes a piece's room, portal and 0x2C-record counts
  before the compiler sums them; its in-game effect (a missing tile) is not
  verified. It should not happen below the editor limit.
- Room props: each room takes one object of the 550-object pool, which has no
  bound check (`logs/limits/items.md` §2.3). 241 rooms plus every placed object
  must fit; the objects area owns that pool.
- Above 241: the byte ids (`0x7042B8`/`0x702A40` bucket rows, bucket counts,
  `light+0x99`) and the per-view byte flags cap ids at 255 with the 12 special
  ids. More rooms need word ids: reimplement `sub_00086C40`, `sub_00086480`,
  `sub_000865E0`, `sub_000897D0` (draw buckets) and the light functions
  `sub_001121E0`, `sub_001123C0`, `sub_001124D0`, `sub_00112EA0`, `sub_00114030`,
  `sub_00114290` (rooms.md §5c; its `sub_00356490` is not a light, see "Byte-width
  room ids"). The table and shadow layout
  here already reserve 256 entries per room table.

## Tile caps audit (2026-11-01): what bounds placement

Every cap on tiles with `TSFP_MAPMAKER_LIMITS=1`, against 241 pieces of the
heaviest stock tile (tile rows 0..45 of `0x00529400` read from the XBE: at
most 14 pads, 14 cover pads, 46 links, 42,427 B memory estimate):

| Cap | Value | Needed at 241 | Binds? |
| --- | --- | --- | --- |
| Pieces = rooms (byte room ids) | 241 | 241 | **yes, first** |
| Links (editor) | 8800 -> 12,000 | 11,086 | was first for tiles with 37+ links (types 14, 25, 40, 42, 43) |
| Pads / cover | 4800 / 9984 | 3,374 / 3,374 | no |
| Memory estimate (x8) | 16.4 MB | 10.2 MB + 40 KB per item type | no |
| Pad groups | 600 | ~370 measured (1.5 per tile) | no (worst case unmeasured) |
| Portals | 0x3FFF | ~3,400 (14 per tile measured) | no |
| Container / undo ring | 512 KB / 1 MB | tens of KB | no |
| Grid | 80x80 x 16 floors | | no |
| Per-room tables, view shadow | 256 entries | 255 ids | no |

So after the link raise only the room id width stops placement. Above 241
(not done; found in this audit, static analysis):

- Draw buckets: byte ids written only by `sub_00086C40` and `sub_000897D0`
  (C reimplementation needed) and read by `sub_00086480`/`sub_000865E0`
  (`movzx esi, byte [edx+eax]` -> `movzx esi, word [edx+eax*2]` is the same
  length); bucket counts can stay bytes if the writer spills a full bucket.
- Lights: `light+0x99` is the only room byte; the 0xB8-byte light record has
  no spare field (+0x98..+0x9B bytes, up to +0xB4 used). Creation
  (`sub_00114290`) stores its room argument as a byte and passes the byte to
  `sub_00114030`, so ids above 255 would alias a low room, and room 256 would
  be 0, which `sub_001123C0` skips when unlinking (dangling list head). Needs
  a side table by light index (pool `[0x0073D950]`, `[0x004E62DC]` records,
  plus `[0x0073D960]`) and wrappers of the 9 sites in 5 functions.
- View shadow: one displacement per array, views 0xFFC apart, so the
  12-byte rects hold at most 341 ids (about 327 real rooms); beyond that the
  stride (38 sites, 2 divisions) or the ~27 accessors must change.
- Then the same steps as 200 -> 241 again: special ids, `cmp 0xF1`, table
  relocations, model table/slots, light tables and the 1000-object pool
  (500 items + rooms + 64 characters).

## Past 241: feasibility check (2026-11-01, not implemented)

Attempted target: 327 real rooms (ids 1..0x147, spare 0x148, players
0x149..0x14D, views 0x14E..0x153, global 0x154, id space 0x155 = 341, the
most the 12-byte view rects allow at stride 0xFFC). Stopped at 241: the
stage cannot be shown safe for story levels with the evidence below, and
half of it would only add risk. Static analysis only.

- Ids above 255 need the special ids above 255 too, so story levels would
  run with 13 new special values. Each must stay intact wherever it is
  stored. They reach 7 callees as arguments (`sub_000A2900`,
  `sub_000A79B0`, `sub_000A46A0`, `sub_000F85C0` effect `+0x48`,
  `sub_00341850`, `sub_000EA9C0` particle `+0xF0`, `sub_000DFEC0`) and 65
  register loads. A first-order scan of 8-bit register stores in the 276
  functions that hold a room constant or call a room lookup
  (`sub_000888A0`, `sub_000873C0`, `sub_00087310`, `sub_0009FBE0`) found
  none besides the known ones (buckets, `light+0x99`, view/id flags).
  `prop+0x30`, stored from `sub_000873C0`'s result, is a count of rooms.
  Not done: the second-order pass through struct fields. Engine-wide there
  are 50 byte accesses at `+0x34`, 14 at `+0x38` and 1 at `+0xF0`, each to
  be assigned to its struct. Tables of 256 entries indexed by room in stock
  code were not found by the 0xC9..0xD6 scan and are not listed either.
- Draw buckets hold only real rooms (`sub_00086C40` loops ids
  1..`[0x7B975C]`; the global room is drawn by `push 0xFE` to
  `sub_000A79B0`), so a word-id writer would not see special ids. The
  writers' byte stores (`88 94 2b`, 7 B) need 8 B as word stores, so
  `sub_00086C40`/`sub_000897D0` must be reimplemented. The byte counts
  (`0x702DB0`/`0x702D60`) also cap a bucket at 255 rooms. A word table
  [80][341] is 54,560 B.
- Light record bytes `+0x96` word, `+0x98` (4 at `0x001145FB`), `+0x99` room,
  `+0x9A` (argument, 0xFF = none), `+0x9B` (the source's `+0x30`), `+0x9C` dword: no
  adjacent spare byte, so the room cannot become a word in place. A side
  table needs the light index, which `sub_001123C0` only gets as a pointer
  (pools `[0x0073D950]` stride 0xB8 and `[0x0073D960]` stride 0xC0). That
  means reimplementing or wrapping `sub_001121E0`, `sub_001123C0`,
  `sub_001124D0`, `sub_00112EA0`, `sub_00114030` and `sub_00114290` (which
  reads `+0x99` at `0x00114829`/`0x00114851` in its body). Corrected by the
  classification below: `sub_00356490` stores a System Link settings byte, not
  a light room, and `sub_00112EA0` only indexes the heads (row width).
- View shadow: the flags at `+0x6C00` (256 B) run into the list at
  `+0x6D00` past id 255, so flags and list need a second shadow region (a
  0x11FF0 allocation, front-end view relocated again).
- Memory map: the model table `0x008CA000` (0xA00 words, used by the
  bosses, characters, khallos, mothership, sky music and tank areas) must
  grow to about 0xA5C words over the light round robin `0x008CB400` and the
  query buffers. The per-id tables `0x008C7000..0x008C8000` (objects,
  probe) and `0x008C0800` (sky mask, roof: 256 entries) are also hard-coded.
  The "spare" `0x008E0000..` in `00-header.json` is taken by the pads group
  A* pool (0x20000 B) and from `0x00900000` by actors, story character and
  grid.

Smallest safe order for a later round: (1) the second-order byte audit as
a decoder classification test, like `test_every_room_constant_is_classified`;
(2) the light side table and the bucket writers, gated on scene 100 and on
ids above 0xF1 so stock ids keep the original code paths; (3) the
renumbering and relocations; (4) a story-level playtest pass before raising
`MAPMAKER_LIMIT_PIECES`. Step (1) is done below; the stage list there
replaces this order.

## Byte-width room ids: classification (2026-11-01, stage 1, static only)

Test: `tests/mapmaker/test_mapmaker_rooms_bytes_classified.py` (lifter decode
of all game code, skips without the private XBE). Nothing was changed in the
runtime; nothing was run in the game.

| Check | Sites | Rooms among them |
| --- | --- | --- |
| Byte accesses at `+0x34..+0x3B`, `+0xB0..+0xB3`, `+0xF0..+0xF3`, `+0x99`, `+0x2A0..+0x2A7` (no index, base not `esp`) | 130 in 77 functions | 9: `light+0x99` |
| Special-id stores (`0xD5`/`0xCE`) into `+0x34/+0x38/+0xF0/+0x48/+0x2A0/+0x2A4` | all dword | (fields are dwords) |
| 8-bit instructions with an immediate 0xC8..0xD6 | 15 | 0 (masks, fill bytes, data) |
| Bucket tables `0x7042B8`/`0x702A40`, counts `0x702DB0`/`0x702D60` | users only in `sub_00086C40`, `sub_000897D0`, `sub_00086480`, `sub_000865E0` | 4 byte writers, 4 byte readers |
| Byte room flow (seeded at the bucket/light byte loads, through registers, stack slots, call arguments) | 3 byte stores, 1 word store, 1 byte compare | all listed below |

The 121 non-room byte sites, by record: System Link session/player records
(`[0x774028]`, `0x774180+n*0x30C`; 30), MapMaker editor records (28), flag
bits tested with masks (25), C++/library objects past `0x3B0000` (15), the
`sub_00040FA0` record (+0x34 = word*16, 5), room records `+0xB2` flag bits
(3, indexed by room, hold none), colour bytes (3), static tables
`0x7A18A0`/`0x74FE7C`/`0x4FA128` (5), `+0xB0` flags of `sub_00236070` (4),
the level number word `+0x3A` (2) and the System Link settings copy
`0x78AD58+0x99` (`0x003564F9`, written from `[0x541CD8]`; earlier lists had it
as a light, it is not). So the open `+0x34`/`+0x38`/`+0xF0` accesses are all
other records; prop, particle, effect and view rooms are dwords everywhere.

Every byte that holds a room:

| Storage | Writers | Readers | Values it can hold |
| --- | --- | --- | --- |
| Bucket rows `0x7042B8[80][200]` (now `0x008C1800` [80][256]) | `0x00086CF6`, `0x00086D80` (copy), `0x000898A3` | `0x000864F4`, `0x00086644`, `0x000898F0` (movzx) | real rooms only: loop counter `edx = 1..[0x7B975C]` |
| Sorted rows `0x702A40[80][10]` | `0x00086CD4` | `0x00086D77` (mov) | same loop counter |
| Counts `0x702DB0`/`0x702D60` [80] | `inc cl` stores | movzx | rooms per bucket, at most R |
| `light+0x99` | `0x001122DE`/`0x001124B9` (0), `0x0011408E`, `0x00114390` | `0x001123D5`, `0x0011241C`, `0x001124FC`, `0x00114829`, `0x00114851` | the prop's `+0x34`, or `[0x0073D95C]` (= R, from `[0x7B975C]` at `0x001121F5`) when the prop is in the global room or several rooms; a held prop keeps its player id (INFERRED: no substitution) |

No reader sign-extends (all movzx or a plain byte copy), so 0xFF is a valid
room in every byte. The global room is never bucketed (drawn by `push 0xD5`
at `0x00086DDF`). The flow reaches only these tables (with element size,
stock entries): `0x7B54E0`/`0x7B5F00` dword[0xD6] (relocated, 256),
`0x73C350` light heads [3][0xD5], `0x73D910` round robin (room/16, 14),
`0x7AC880` light colour float[0xD5][3], and `0x7B5180` dword[0xD6]; plus
view rects (`+0x3BC..`) and room records (`+0xB2`). No stock table of 256
entries is indexed by a room: every room table is sized 200, 0xD5 or 0xD6.

Found by the flow, not by the earlier constant scan: **`0x7B5180`, a per-id
"drawn" flag dword[0xD6] written only by `sub_000A79B0` (`0x000A79C8`,
`[room*4+0x7B5180] = 1`, room from the bucket byte or the global id), never
relocated** (`objects.json` keeps that operand). With the 241 layout, ids
0xD6..0xFE write past it: id 0xD6 into `[0x7B54D8]` (a list count that is only
reset and incremented), 0xD7 into the unreferenced `0x7B54DC`, the rest into
the stock `0x7B54E0..` tables that `rooms.json` moved away. INFERRED benign
today (nothing reads those words as data); it is moved first (stage 2).

### Special ids per scene?

No. The special ids are immediates in 155 operand entries of `rooms.json`
over 85 functions, the same code for story levels and scene 100. A
scene-dependent value would need a memory operand at each site (longer
instructions, not a table patch) or wrapping those 85 functions. They stay one
global layout, as in the 241 change, which already runs story levels with
moved ids. The audit shows why moving them above 255 is safe for code that
stores rooms: no special id is stored, compared or loaded as a byte, except a
held prop's light (above).

### Safe targets

- **255 real rooms, special ids 0x100..0x10C, byte buckets kept** (+14
  pieces). Buckets hold only real rooms and counts at most R, so
  `sub_00086C40`/`sub_000897D0` stay as they are. Needed: the light room
  (held ids) widened, `0x7B5180` relocated, the renumbering and table sizes
  for an id space of 0x10D, and the view shadow re-packed in place (rects
  0x10D*12 + flags 0x110 + list 0x21A = 0xFC6 < 0xFFC, so the stride and the
  0xBFF0 allocation stay).
- No target above 241 avoids moving the special ids: ids 0xF2..0xFE are taken.
- **Above 255** needs word buckets (both writers reimplemented, readers
  `movzx byte` -> `movzx word`, same length) and, past 341 ids, a second view
  shadow region.

### Stages (each with its sites and test)

1. Done: this classification. Any new byte site, bucket user, flow table or
   room immediate fails `test_mapmaker_rooms_bytes_classified.py` or
   `test_every_room_constant_is_classified`.
2. Done (2026-11-01): room flag table `0x7B5180` relocated to `0x008CC800`
   (0x110 entries; declared 0x354 so the count `0x7B54D8` is outside it), the
   `keep` dropped from `objects.json` (slot-0 list declared 0x1BC). Test:
   `check-tables.py`, `test_room_flag_table_7b5180_is_relocated` (+4 mutations).
3. Done (2026-11-01): light room side table, every scene (the special ids
   move in story levels too), five wrappers; see "Lights". The round robin
   moved to `0x008CB440` with 32 entries. Head rows widen in stage 4. Test:
   the light part of `mapmaker_rooms.c` (+13 mutations) and
   `test_light_side_table_matches_the_code`.
4. Done (2026-11-01): the renumbering and table sizes, see "Rooms 241 ->
   255" below. Test: `test_table_shape`, `test_id_space_tables` (+19
   mutations), `test_other_areas_use_the_moved_tables`,
   `test_extension_map_rooms_sub_range`, `check-tables.py`.
5. Done (2026-11-01, prepared for the playtest): `MAPMAKER_LIMIT_PIECES` and
   `MAPMAKER_ROOMS_MAX` 241 -> 255, gated as before (`TSFP_MAPMAKER_LIMITS=1`;
   the compile guard keeps 200 unless room models 0x908..0xA06 all have prop
   slots, scene 100 only). Play the story list first (S1..S7).
6. Only for more than 255: word buckets (`sub_00086C40`, `sub_000897D0`
   reimplemented; `0x000864F4`, `0x00086644`, `0x000898F0` widened; the copy
   `0x00086D77`/`0x00086D80`), then the second shadow region past 341 ids.

Risks: the flow is linear per function (no path merging) and seeded at the
known byte loads, so a room reaching a byte through a dword struct field and
back is covered by the field scan only at the listed displacements; held-prop
lights are INFERRED from the substitution code, not observed; the
`0x7B54D8` overwrite is static reasoning.

## Rooms 241 -> 255 (stages 2-5, 2026-11-01, static only)

Nothing here was built or played by the agent that wrote it. The sections
above describe the 241 layout; where they differ, this section wins.

Room ids (every scene: the same 155 operand entries run in story levels):

| id | stock | 241 | 255 |
| --- | --- | --- | --- |
| real rooms | 1..0xC8 | 1..0xF1 | 1..0xFF (`cmp 0xC8` x21 -> 0xFF) |
| spare (unused) | 0xC9 | 0xF2 | 0x100 |
| held by player p / nobody | 0xCA+p / 0xCE | 0xF3+p / 0xF7 | 0x101+p / 0x105 (`add -0xCA` x9 -> -0x101) |
| room of view v | 0xCF+v | 0xF8+v | 0x106+v |
| global / unplaced | 0xD5 | 0xFE | 0x10C |
| id space | 0xD6 | 0xFF | 0x10D |

Every special id is now above every byte, so a byte can only hold a real
room or 0: the draw buckets (byte rows and counts) stay as they are, and the
one byte that could hold a special id, `light+0x99`, has the stage-3 side
table. The checker accepts every new value at its operand width (all
imm32/imm16/disp32; `cmp ax,0xD5` is the view list cap, 16-bit).

Tables (all in `0x008C0000..0x008D4000`; `test_extension_map_rooms_sub_range`
proves no other area's constant points into the gaps it uses):

| table | 241 | 255 |
| --- | --- | --- |
| per-id prop tables `0x7B54E0`, `0x7B5840`, `0x7B5BA0`, `0x7B5F00`, `0x7B6260` | `0x008C7000` step 0x400, 0xFF | `0x008C7000` step 0x440, 0x10D (global entry 0x10C at +0x430 of the first and fourth) |
| loop ends `0x0009E4E6` / `0x000A1ED5`, `mov ecx,0xD6` x12 | 0x3FC / 0x3F8, 0xFF | 0x434 / 0x430, 0x10D |
| per-id flag bytes `[0x7B977C]` | alloc 0x110, clear 0x40 dwords | alloc 0x110, clear 0x43 dwords + stosw (0x10E); +0x10C/+0x101/+0x106 |
| per-id drawn flag `0x7B5180` (stage 2) | not moved | `0x008CC800`, 0x110 |
| light heads `0x73C350` [3][row] | `0x008C8400`, row 0xFE | `0x008C8540`, row 0x10C (clear loop: start row 0 entry 0x10B, stride 0x430, end `0x008C95FC`) |
| room light colour `0x7AC880` float[][3] | `0x008C9000`, 0xFE | `0x008C9200`, 0x10C |
| light round robin `0x73D910` | `0x008CB400`, 16 | `0x008CB440`, 32 (stage 3) |
| model -> slot `0x78B860` (operand entries) | `0x008CA000`, 0xA00 | `0x008CA000`, 0xA08 (room models 0x908..0xA06; clear 0x504 dwords; bounds `cmp 0x9D0` x4 -> 0xA08) |
| per-model 0x18 B array `[0x7087A4]` | 0xF000 | 0xF0C0 (clear 0x3C30 dwords) |
| view shadow (per view, +0x6000) | rects 256x12, flags +0x6C00, list +0x6D00 | rects 0x10Dx12 (+0x6000..+0x6C9C), flags +0x6CA0 (0x110), list +0x6DB0 (0x10D words, to +0x6FCA < 0x6FFC) |

The view stride (0xFFC), the 0xBFF0 view array, the front-end static view
(`0x008CD000`, 0x7000) and the rect displacements are unchanged; the open-air
hook (`pc_mapmaker_openview.inc`, rects at +0x6000 + 12*room, rooms below
`MAPMAKER_ROOF_ROOMS` = 256) needs no change. Other areas: the objects pool
clear (`pc_mapmaker_objects.inc`) clears the moved per-id tables at their new
addresses with 0x10D entries; the probe (`MAPMAKER_PROBE_ROOM_*`) reads them
there; the bosses/characters/khallos/mothership/sky music/tank areas keep
reading the model table at `0x008CA000` (their own models are below 0xA00).
The `rooms` area can no longer be switched off by `TSFP_MAPMAKER_AREA_OFF`
(the light side table is needed in every scene once ids pass 0xFF).

Free in the rooms sub-range: `0x008C91D0..0x008C9200`, `0x008C9E94..0x008CA000`,
`0x008CB410..0x008CB440`, `0x008CB4C0..0x008CB800`, `0x008CCC40..0x008CD000`;
`0x008D4000..0x008DFFFF` (no reference in tracked code); `0x008E0000..` is
the pads A* group pool and `0x00900000..` actors and later areas.

Stage 5 (`MAPMAKER_LIMIT_PIECES` and `MAPMAKER_ROOMS_MAX` 241 -> 255, gated as
before by `TSFP_MAPMAKER_LIMITS=1` and the room-model slots of scene 100) is
the last commit; it changes nothing outside MapMaker play. Story-level risk
of stages 2-4 (they are not gated): see "What to test in play".

## Rooms 255 -> 1024: feasibility plan (2026-11-02, static only)

Goal (user): "keep bumping tile limits up ... make 1024 a goal". Basis: the
255 layout above, the tile caps audit, `logs/limits/*.md`, the disassembly.
Nothing here was built or played. Counts per piece are the heaviest stock
tile (14 pads, 14 cover, 46 links) and the measured average (14 portals,
1.5 pad groups).

### Layouts

The 255 layout repeated at a larger id width: real rooms 1..R, then spare,
5 player ids, 6 view ids and the global id (13 ids), id space R+14.

| target | real rooms | special ids | id space | view shadow per view |
| --- | --- | --- | --- | --- |
| 255 (now) | 1..0xFF | 0x100..0x10C | 0x10D | 0xFC6 of 0xFFC (stride kept) |
| 327 | 1..0x147 | 0x148..0x154 | 0x155 | rects 0xFFC (full), flags + list in a 2nd region |
| 511 | 1..0x1FF | 0x200..0x20C | 0x20D | 0x1ECA: new view stride 0x3000 |
| 1023 | 1..0x3FF | 0x400..0x40C | 0x40D | 0x3CCA: new view stride 0x5000 |

1023 rather than 1024: with the specials at 0x400 (as asked) the last real
room is 0x3FF; 1024 needs them one higher (0x401..0x40D), same work.

### Every cap between 255 and 1024

| # | cap | now | binds at | needed for 1023 | stage |
| --- | --- | --- | --- | --- | --- |
| 1 | draw-bucket room bytes and byte counts (255 per depth) | byte | 256 rooms | words | 6 (done) |
| 2 | view shadow (12-byte rects per id, stride 0xFFC) | 0x10D ids | 342 ids | stride 0x5000 | 7 |
| 3 | special ids 0x100..0x10C, 21 `cmp 0xFF` real-room tests, 12 id counts | 255 | 256 | 0x400.. | 8 |
| 4 | per-room tables (`0x008C0000`.., 256 entries) | 256 | 256 | 0x400 | 9 |
| 5 | per-id prop tables, drawn flag, id flag bytes `[0x7B977C]` | 0x10D | 0x10D | 0x40D | 9 |
| 6 | light heads row 0x10C, colour 0x10C, round robin 32 groups | 0x10C | 0x200 | 0x40C, 65 groups | 9 |
| 7 | model table 0xA08 words, `[0x7087A4]` 0x18 B per model, 4 bounds, prop slots | 0xA06 | 256 | 0xD08 | 10 |
| 8 | room query buffers (1023 words) | 1023 | 1024 | 2047 | 9 |
| 9 | C side tables: light side table (uint16, 32 groups), roof/open-air rooms (`MAPMAKER_ROOF_ROOMS` 256), word bucket lists (2047) | 256 | 256 | 0x40D | 9 |
| 10 | object pool (each room takes one) | 1000 | 1000 - 500 items - 64 chars = 436 | 1600 | 11 (objects area) |
| 11 | light pool records per room group (`[0x0073D954]`, pool 500 / groups) | ~31 | lights get sparse | larger pool | 11 |
| 12 | links: editor 12,000; A* int16 indices 32,767 | 12,000 | 260 heavy pieces | 47,058 heavy (above int16: ~712 heavy pieces max) | 12 (pads area) |
| 13 | regular pads: editor 4,800; ids 7,000..8,000 (13 id compares) | 4,800 | 342 heavy | 14,322 (above the id cap: ~500-570 heavy max) | 12 |
| 14 | cover pads 9,984 (Layout B) | 9,984 | 713 heavy | 14,322 (int16 room to ~23,766) | 12 |
| 15 | pad groups 600 (N x N matrix, A* pool 0x20000) | 600 | 400 | ~1,535 | 12 |
| 16 | portals 0x3FFF (stack flag buffer 0x4000) | 16,383 | ~1,170 average | 14,322 average; heavy unmeasured | 13 |
| 17 | play memory (Level heap; arena 47,104,000 B, +16 MB max by `TSFP_MAPMAKER_ARENA_MB`) | measured | see below | see below | 14 |
| 18 | editor memory estimate x8 (16.4 MB) | 16.4 MB | ~390 heavy | 43 MB heavy | 14 |
| 19 | map container / undo ring | 512 KB / 1 MB | ~1,000+ (tens of KB at 241) | ~250 KB | no |
| 20 | room AABB scratch (32 KB, 0x1C per room), portal walk recursion (0x1E4 per room, 8 MB stack) | ~1,170 / ~17,000 | | fits | no |
| 21 | view room list (shadow, cap `cmp ax,0x10C`) | 0x10C | 0x10D | 0x40D | 7 |
| 22 | editor piece walk (`mapmaker_list_totals`, 4096) and grid 80x80x16 | 4096 | | fits | no |

### Memory budget for 1023

Extension block, rooms sub-range `0x008C0000..0x008E0000` (128 KB; the pads
A* pool starts at `0x008E0000`): per-room tables 32 B x 0x400 = 32 KB;
per-id prop tables 5 x 0x40D x 4 = 20.3 KB; light heads 12.1 KB, colour
12.1 KB, round robin 0.3 KB; drawn flag 4.1 KB; model table 0xD08 words
6.5 KB (base `0x008CA000` stays: the bosses, characters, khallos and
mothership areas read it there); query buffers 2 x 4 KB; bucket staging row
0.5 KB; front-end static view one stride, 20 KB. Total about 116 KB of 128 KB:
it fits only repacked, with the model table in place.

Heaps: view array 6 x 0x5000 = 120 KB (now 48 KB; every scene, story levels
too), per-model array 0xD08 x 0x18 = 79 KB (now 60 KB), prop slots +0x30 per
room model, link pool (links + 16 per piece + 256) x 20 B (about 1 MB at
32,767 links).

Play memory, from the two measured maps (CONFIRMED numbers, INFERRED rate):
110 tiles + 67 items left 11.56 MB free, 236 tiles + 200 items 5.56 MB, so
a tile with its share of content costs roughly 30-48 KB. 1023 tiles would
need 24-38 MB more than the 236-tile map; the arena switch adds at most
16 MB of the 64 MB window (textures and the container share it). So 1023
heavy tiles do not fit in the guest's 64 MB; about 550-650 do with the full
arena (estimate; to be measured with `TSFP_MAPMAKER_PROBE=32`).

### Verdict

- The room machinery (stages 6-10) can reach 1023 ids; nothing in it caps
  below that once the view stride changes.
- Real maps are capped before 1023 by pads (stage 12: ~500-570 heaviest
  tiles at the regular pad id limit), links (~712 heaviest tiles at int16 A*
  indices) and play memory (~550-650 tiles, unmeasured). Light tiles (few
  pads/links) can go further, up to memory.
- Highest safe next target: **511 pieces** (ids 1..0x1FF, specials
  0x200..0x20C, view stride 0x3000). Its blockers: stages 7-11, links 12,000
  -> 23,506, pads (heavy tiles stop at 4,800 / 14 = 342 pieces until stage
  12; the editor counts pads, so placement stops cleanly), objects 1,075 >
  1,000, and a probe measurement with the arena raised. Then 1023 for light
  maps once pads, links and memory allow.

### Stages (sites)

6. Done (this round): word draw buckets, below ("Stage 6").
7. View stride 0xFFC -> S (0x3000 for 511, 0x5000 for 1023): the 34 `add/imul
   r, 0xFFC` sites found by a scan (`0x0003C524`, `0x0003C576`, `0x0003C711`,
   `0x0003C7ED`, `0x00083847`, `0x00083A37`, `0x00088F82`, `0x000899FC`,
   `0x0008A253`, `0x0008BDB4`, `0x0009FFA6`, `0x000C622E`, `0x000C63C4`,
   `0x000C79B6`, `0x000C8E2C`, `0x000CCA3B`, `0x000CCA4F`, `0x0010A9B3`,
   `0x0010AA83`, `0x0010E4C7`, `0x00117D70`, `0x0011A5D2`, `0x0011A8FB`,
   `0x0011AFE1`, `0x001291FC`, `0x00160AA5`, `0x00176B15`, `0x001848E2`,
   `0x00185BEE`, `0x00185C08`, `0x00185CC0`, `0x001E4607`, `0x00247E34`,
   `0x0025F304`, `0x00343055`) plus the remaining stride uses of the earlier
   count of 38 (`0x001B45B6` lea, `0x001B773E`, `0x00188F70` to classify),
   the two divisions by 0xFFC (magic multiplies; new magic, same length),
   the view array allocation/clear (`0x000C61D3`/`0x000C61E7`), the
   front-end static view (relocated, one stride) and the 67 shadow
   displacements moved from +0x6000 to +0x1000 (inside the stride: rects
   +0x1000, flags and list after them). `MAPMAKER_ROOMS_VIEW_STRIDE`,
   `_VIEW_RECTS`, `_VIEW_LIST`, `_LIST_WORDS` (this file's C) and
   `OPENVIEW_RECT` follow. Test: decode of every 0xFFC immediate classified
   (like `test_every_room_constant_is_classified`), allocation = 6 x S.
   Overlap: the camera view is one of the six views (see "Camera work").
8. Special ids to R+1.. (155 operand entries: values only), 21 real-room
   compares, 12 id counts, loop ends, `[0x7B977C]` sizes and displacements,
   the view list cap; C: `MAPMAKER_ROOMS_IDS_REAL`, `_ID_GLOBAL`,
   `_VIEW_LIST_CAP`, `_LIGHT_ROW`; `test_table_shape` / `id_space_problems`
   parameterised by the id space.
9. Tables (rows 4-6, 8, 9): relocation sizes and the light clear loop as in
   stage 4 of the 255 raise, a repacked rooms sub-range, `MAPMAKER_ROOF_ROOMS`
   and the open-air/roof C arrays, light side table groups.
10. Model table and room-model prop slots for 0x9D0..0x907+R (the compile
   guard stays: 200 unless all have slots), `[0x7087A4]` allocation/clear,
   the four model bounds; check no area places its own model ids in
   0xA08..0xD08.
11. Objects pool and light pool (owned by the objects area and the light
   hooks).
12. Pads area: links to 23,506 (511) / 32,767, regular pad ids past 8,000,
   pad groups past 600 (A* pools).
13. Portal flags past 0x3FFF if heavy maps need it (frame 0x8000), and an
   audit of 16-bit portal ids.
14. Memory: probe with the arena raised, then `MAPMAKER_LIMIT_PIECES` to the
   reached target (still gated by `TSFP_MAPMAKER_LIMITS=1` and the
   room-model slots).

Story-level risk per stage: 6-10 run in every scene (immediates and table
addresses cannot be per scene), as stages 2-4 of the 255 raise did; only the
compile guard, prop slots and the editor limit are scene-100 gated. Stage 7
changes every view's layout (splitscreen, camera screens, front end).

### Camera work (overlap)

Another branch changes the security-camera view (sweep, widescreen aspect,
rooms missing above about floor 10). The camera view is one of the six
0xFFC views: it uses the same shadow rects/flags/list, the same depth
buckets and `[view+0xD28]` (deepest depth, capped at 0x4F: 80 depths, which
may be why rooms above about floor 10 stop drawing) and `sub_000897D0`'s list
cap. Stage 6 reimplements `sub_00086C40` and `sub_000897D0` (same results
below 256 rooms per depth); stage 7 would move every view field above +0xFFC.
A camera fix that wraps either function, changes the depth cap or adds
fields to the view must be merged against these hooks.

## Stage 6: word draw buckets (2026-11-02, static only)

`sub_00086C40` (bucket the current view's rooms by portal-walk depth, then
draw) and `sub_000897D0` (each view's sorted room list) are reimplemented in
C (`pc_mapmaker_rooms_buckets`, `pc_mapmaker_rooms_view_lists`); the depth
lists are word lists in C (room ids up to 2047, any number per depth). The
two readers `sub_00086480`/`sub_000865E0` keep their bodies: rooms.json sets
their row stride to 0 (`0x000864CB`, `0x00086621`: every depth reads the one
staging row `0x008C1800`, the relocated row base) and makes the room load a
word (`0x000864F4`, `0x00086644`: `movzx esi, byte [edx+eax]` ->
`movzx esi, word [edx+eax*2]`, same length and mnemonic). Their hook
(`pc_mapmaker_rooms_bucket_draw`) copies the depth's list there and sets its
count byte (`0x702DB0+depth`), at most 255 per call, and calls the reader
again with the same return address for the rest. So with up to 255 rooms per
depth every reader runs once per depth, as stock; the order (normal rooms
ascending, 10 "sorted" rooms per depth appended as new depths after
`[view+0xD28]`, readers in `sub_000867F0`'s order) and the calls
(`sub_0001A010` with the viewport, `sub_000A79B0(0x10C)`, `sub_003442B0`,
`sub_00086AE0(deepest)`, `sub_00344350`, `[view+0xD28]` restored) are the
stock ones. Deviations, all where stock wrote out of bounds: depths >= 80
are skipped (stock wrote past the 80 rows and counts), the room list never
passes its shadow capacity (0x10D words; stock had no bound on the first
pass). The old byte rows past the staging row, the sorted rows `0x702A40`
and their counts `0x702D60` are no longer used (the dead bodies still name
them). Room ids above 0x10C still need stage 7 (rects) and stage 8.

Wrappers (all four ship together with the rooms.json entries: with the
table but without the hooks every depth would read one stale row):

```
WRAP sub_00086C40 -> pc_mapmaker_rooms_buckets      (no args, plain ret; replaces the body)
WRAP sub_000897D0 -> pc_mapmaker_rooms_view_lists   (no args, plain ret; replaces the body)
WRAP sub_00086480 -> pc_mapmaker_rooms_bucket_draw  (eax = depth, plain ret; calls the original 1+ times)
WRAP sub_000865E0 -> pc_mapmaker_rooms_bucket_draw  (same)
```

Tests: `tests/mapmaker/test_mapmaker_rooms_buckets.py` (fixture
`mapmaker_rooms_buckets.c`: 400 random views against a model of the stock
byte-table code, 1500 rooms with 700 in one depth, 300 random room lists
against stock `sub_000897D0`, 290-room list; 27 C mutations; the reader
patches decoded with the lifter's decoder; 6 table mutations).

## Stage 7: view stride 0xFFC -> 0x3000 (2026-11-02, static only)

Target 511 (0x5000 for 1023 was not taken: the pads, links and memory caps
stop real maps near 500 pieces, and 1023 needs the rooms sub-range repacked
around the model table). Every view in the six-view array `[0x7B0CE4]` is
now 0x3000 apart: the 0xFFC struct, then its per-room shadow inside the
stride. Layout per view (sized for the 511 id space 0x20D):

| field | stock | 255 layout | now |
| --- | --- | --- | --- |
| rects 12 B per id (depth, x0, y0, x1, y1) | +0x3BC | +0x6000 | +0x1000 (to +0x289C) |
| visible flag byte per id | +0xD2C | +0x6CA0 | +0x28A0 (0x210) |
| sorted room list (words) | +0xE0C | +0x6DB0 | +0x2AB0 (to +0x2ECA) |
| stride / array | 0xFFC / 0x5FF0 | 0xFFC / 0xBFF0 | 0x3000 / 0x12008 (clear 0x4802 dwords) |
| front-end static view `0x008CD000` | 0xFFC | 0x7000 | 0x3000 |

`rooms.json` (63 new operand entries, plus the 67 shadow displacements and
the allocation/clear re-valued): the 35 `add/imul r, 0xFFC` steps (the plan's
34 plus `0x0009FFA6`, shared by `sub_0009FF10`/`sub_0009FF30`);
`sub_000C61D0`'s unrolled per-view init (`add ecx, k*0xFFC` x4 and 20 fields
`[ecx + k*0xFFC + 0x214/0x21C/0x2A0/0x2A4]`, view index k = 1..5, which the
plan's list missed); the two `(view - views) / 0xFFC` divisions
(`0x000CC92C`/`0x000CC936` in `sub_000CC920`, `0x000D2D33`/`0x000D2D3C` in
`sub_000D2D20`): magic `0x80200803` -> `0xAAAAAAAB` and `sar edx, 0xB` ->
`0xD` (2^45/0x3000 rounded up, exact for every signed 32-bit difference).
Not view strides (reviewed, in `NOT_VIEW_STRIDE`): `0x00188F70`/`0x001B773E`
(player array `[0x7B0C48]`, stride 0x1584, field +0xFFC), `0x001B45B6`
(string copy into another record), the 0x1FF8/0x17FC/0x3FF buffer sizes and
masks. C: `MAPMAKER_ROOMS_VIEW_STRIDE`/`_RECTS`/`_FLAGS`/`_LIST`,
`OPENVIEW_RECT` (openview), `MAPMAKER_CAMERA_VIEW_RECTS` (camera
diagnostics), `SMX_PLAYER` (sky music sand grains walk the views by stride).

Tests (`test_mapmaker_rooms.py`): `test_view_stride_table` (+10 mutations:
a step missing or stock, an init multiple/field stock or missing, magic or
shift stock, a division missing, the allocation/clear short; the division is
evaluated for every multiple of the stride, off-by-one values and the 32-bit
extremes), `test_every_view_stride_is_classified` (decode: every k*0xFFC,
0x5FF0, 0x17FC, 0x3FF, the magic, and every displacement past 0xFFC in a
function that reads the view array base or count is patched or reviewed),
the shadow checks of `id_space_problems`, the other areas' defines in
`test_other_areas_use_the_moved_tables`; the bucket, camera and openview
fixtures use the defines. Story risk: every scene allocates the view array
(72 KB instead of 48 KB, `sub_0003DFA0(.., 1)`); splitscreen, camera screens
and the front end use the new layout.

## Stage 9: rooms sub-range repacked for 511 (2026-11-02, static only)

Done before stage 8 (the renumbering), so that every table already has the
room the 511 id space needs when the ids move: each commit is safe alone.
Blocks (`ROOMS_BLOCKS` in `test_mapmaker_rooms.py`; a table can use less
than its block until stage 8):

| block | base | bytes | was |
| --- | --- | --- | --- |
| t1 float/room | `0x008C0000` | 0x800 (512 rooms) | 0x400 |
| t3 dword/room (sky mask reads it) | `0x008C0800` | 0x800 | 0x400 |
| t2 dword/room | `0x008C1000` | 0x800 | `0x008C0400` |
| t4 float[3]/room | `0x008C1800` | 0x1800 | `0x008C0C00` |
| bucket staging row (`MAPMAKER_ROOMS_BUCKET_ROW`) | `0x008C3000` | 0x1000 | `0x008C1800`, 0x5000 (the byte rows were only read by the replaced bodies) |
| t6, t7 (room list, capacity `0x00088D71` 0x200) | `0x008C4000`, `0x008C4800` | 0x800 each | `0x008C6800`, `0x008C6C00` |
| per-id p2, p3, p5 | `0x008C5000`, `0x008C5840`, `0x008C6080` | 0x840 each | `0x008C7440`, `0x008C7880`, `0x008C8100` |
| per-id p1, p4 (+ global entry) | `0x008C7000`, `0x008C7CC0` | 0x840 each | same bases |
| per-id drawn flag | `0x008C8500` | 0x840 | `0x008CC800` |
| model table (stage 10) | `0x008CA000` | 0xB08 words | 0xA08 words |
| light round robin | `0x008CB700` | 0x100 (64 groups) | `0x008CB440`, 32 |
| query buffers | `0x008CB800` | 2 x 0x800 | same |
| front-end view | `0x008CD000` | 0x3000 | same |
| light heads [3][0x20C] | `0x008D0000` | 0x1890 | `0x008C8540` |
| light colour [0x20C][3] (+4) | `0x008D18A0` | 0x1894 | `0x008C9200` |

Free: `0x008C68C0..0x008C7000`, `0x008C7840..0x008C7CC0`,
`0x008C8D40..0x008CA000`, `0x008CB610..0x008CB700`, `0x008CC800..0x008CD000`,
`0x008D3140..0x008DFFFF`. Per-room clears `0x0008A796/A2/B0` count 0x200.
Other areas follow: objects pool clear (p2/p5 addresses), `MAPMAKER_ROOF_ROOMS`
512 (roof and openview arrays), `MAPMAKER_CAMERA_ROOMS` 511 (camera
diagnostics), light side table groups 64; the dev sky-mask switch keeps rooms
0..255 (`MAPMAKER_SKYMASK_ROOMS`, a diagnostic). Tests: `test_rooms_blocks`
(+4 mutations), `test_other_areas_use_the_moved_tables` (per-room sizes and
C arrays), `test_extension_map_rooms_sub_range` (blocks), objects/openview/
skymask fixtures.

## Fix 2026-10-30: crash after an exploding barrel (story level 1)

Cause: `sub_000BB850` (explosion/area effect over the rooms touching a box)
gets the rooms from `sub_000873C0`, then appends the global room with
`mov [esp+eax*4+0x180], 0xD5` at `0x000BBA05` and skips it in the loop with
`cmp eax, 0xD5`. The compare had been renumbered to 0xFE but the store had
not, so 0xD5 (213) was treated as a real room and indexed the per-room prop
list `[0x7B9784]` (R+1 entries) and the prop table past the level's rooms:
garbage pointer into `sub_0012A590`. The store was missed because the text
listing elides the bytes of 11-byte instructions (`c7848480010000d50000..`)
and the inventory parser skipped those lines; the 288 such lines contain no
other room constant. Fix: `0x000BBA05` 0xD5 -> 0xFE.

The stale id did not come from level data. Room fields are written by code:
prop rooms from position (`sub_0009FBE0`, `sub_000888A0`) or the room-model id
(`sub_000A6880`), effect/particle rooms from immediates passed to
`sub_000EA9C0` (`+0xF0`) and `sub_000F85C0` (`+0x48`), light rooms from the
position lookup. So no translation at load is needed, and bounding every
room-table lookup (dozens of per-frame sites) was not done; instead the test
`test_every_room_constant_is_classified` decodes the game code with the same
decoder as the lifter and fails if any immediate 0xC9..0xD6 (or its negative),
any `cmp ..., 0xC8` or any `lea [r-0xC9..0xD6]` is neither patched nor in the
reviewed not-a-room list (it fails with `0x000BBA05` removed). It skips when the
private XBE is not present.

## Validation done

- `check-tables.py` on the folder (00-header, items, rooms): table verified.
- `tests/mapmaker/test_mapmaker_rooms.py`: fixture + 18 mutations, table shape, and the
  XBE completeness check of every room constant.
- `tests/mapmaker/test_mapmaker_limits.py`: 241 placed allowed, +1 held refused; two
  new mutations.
- `tests/mapmaker/test_mapmaker_rooms_bytes_classified.py` (7 checks, 2026-11-01):
  byte-width room-id classification; 5 hand mutations (seed, field site,
  immediate, bucket writer, flow table removed) each fail it.
- Rooms 255 (2026-11-01, static only): `check-tables.py` "table verified" for
  the folder after each stage; the rooms fixture with the light side table
  (13 light mutations), the id-space and extension-map table tests (19
  mutations), the stage-2 flag table test (5 mutations), the objects and
  probe fixtures with the moved tables. Not built, not played.

## What to test in play

Rooms 255 (stages 2-4 run in every scene, so story levels come first; no
`TSFP_MAPMAKER_*` switch is needed for them):

- S1. Story mission 1: shoot the exploding barrels and throw grenades in
  several rooms (global room 0x10C in the area-effect room list, the
  0x000BBA05 crash path), then play on for a few minutes.
- S2. Two story missions with many dynamic lights (muzzle flashes,
  explosions, fire): lighting and room light colours look as in the 241
  build; no flicker or lights left behind when they fade (light heads, colour
  table, round robin; the side table only acts for ids above 0xFF).
- S3. Held and thrown things (player ids 0x101..0x104): pick up, carry, drop
  and throw props and weapons, swap weapons often; the held item draws and
  lights correctly and stays drawn when dropped.
- S4. Two-player splitscreen story co-op and four-player Arcade splitscreen
  (view ids 0x106..0x10B, the view shadow re-pack): every view draws its rooms
  and its first-person weapon; walk through doors and portals in each view.
- S5. A mission with security-camera screens or other in-world views: the
  screens show their rooms.
- S6. Front-end and pause menus before, between and after levels (the static
  front-end view at `0x008CD000`), and level restarts / loading a second level
  (light and per-id table resets).
- S7. Any crash, missing room geometry, missing props or wrong lighting: note
  the level and spot; `TSFP_MAPMAKER_PROBE=128` logs room draws in scene 100
  only, so story issues need the stderr log and the place.

Then MapMaker (stage 5):

1. Story levels (several, including splitscreen with 4 players and a level with
   security-camera screens): rendering, lights, held weapons/items (player
   ids), thrown/held props, particles.
2. MapMaker with `TSFP_MAPMAKER_LIMITS=1`: a map of 255 pieces (and one of
   241 built in the previous build); check the log line `rooms 255 of 255`
   (rooms = pieces, nothing dropped), draw distance/culling at the far end,
   lights, doors, items in rooms 242..255, open-air tiles near room 255, and
   the front-end menus after leaving the map.
3. A map of 200 pieces built in the stock game still loads and plays the same.
