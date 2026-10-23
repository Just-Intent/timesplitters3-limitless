# MapMaker rooms: 200 -> 241

The MapMaker's 200-piece limit is the engine's limit of 200 visibility rooms:
every placed tile becomes one room (INFERRED: one room per stock tile, see
"Open"), and the room id is a byte in one engine-wide id space. This change
raises the room limit to 241, the most a byte id allows once the 12 special
ids sit at the top of the byte range. Research: `logs/limits/rooms.md` (main
checkout). Static analysis of the US XBE only; nothing here was run in the game.

Parts:

- `config/guest-code-patches/rooms.json`: 322 operand entries and 19
  relocations (generated from a reviewed site list; `check-tables.py` prints
  "table verified").
- `src/pc_mapmaker_rooms.inc`: five function wrappers (below).
- `src/pc_mapmaker_limits.inc`: `MAPMAKER_LIMIT_PIECES` 200 -> 241 (editor,
  opt-in `TSFP_MAPMAKER_LIMITS=1`; without it the stock 200 stays).
- Tests: `tests/mapmaker_rooms.c` + `tests/test_mapmaker_rooms.py` (fixture,
  18 mutations, table shape).

## Wrappers for the integrator

```
WRAP sub_00087310 -> pc_mapmaker_rooms_query
WRAP sub_0008A780 -> pc_mapmaker_rooms_compile
WRAP sub_00083F80 -> pc_mapmaker_rooms_piece
WRAP sub_00085030 -> pc_mapmaker_rooms_piece
WRAP sub_00242780 -> pc_mapmaker_rooms_props
```

All five are cdecl with a plain `ret`. `sub_00242780` is reached by a tail
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

- 97 x `0xD5` (global room in prop/particle/effect/view code, the four light
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
| `0x0073D910` light round robin per room/16 | 14 | `0x008CB400` | 16 |
| `0x00749890` front-end static view (0xFFC) | | `0x008CD000` | 0x7000 (shadow) |

`0x008CB800..0x008CC800`: two 0x800-byte buffers of the room query. Free:
`0x008CC800..0x008CD000` and `0x008D4000..0x008DFFFF`.

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
at `0x0073D948`/`0x0073D94C`, so it is relocated with 16 entries. Its pool is
sized from `[0x007B975C]/16` (`sub_001121E0`), already dynamic.

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
  `sub_00114290`, `sub_00356490` (rooms.md §5c). The table and shadow layout
  here already reserve 256 entries per room table.

## Validation done

- `check-tables.py` on the folder (00-header, items, rooms): table verified.
- `tests/test_mapmaker_rooms.py`: fixture + 18 mutations, table shape.
- `tests/test_mapmaker_limits.py`: 241 placed allowed, +1 held refused; two
  new mutations.

## What to test in play

1. Story levels (several, including splitscreen with 4 players and a level with
   security-camera screens): rendering, lights, held weapons/items (player
   ids), thrown/held props, particles.
2. MapMaker with `TSFP_MAPMAKER_LIMITS=1`: a map of 241 pieces; check the log
   line (rooms = pieces, nothing dropped), draw distance/culling at the far
   end, lights, doors, items in rooms 201..241, and the front-end menus after
   leaving the map.
3. A map of 200 pieces built in the stock game still loads and plays the same.
