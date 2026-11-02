# MapMaker memory headroom and round 3 raises

Round 3 of the limit raise (2026-10-31; design in the ignored
`logs/limits/features/limits-round3.md`, steps A and B). Nothing here has
been built or played yet. CONFIRMED = seen at the address (disassembly or the
lifter's decoder); INFERRED = reasoning.

## Why memory comes first

The 2026-10-30 capacity test (236 tiles, 200 items, 102+ Story AI, 64
characters) left a minimum of 5.56 MB in the game's Level heap, and the
game's allocator never stops: when a request is larger than the free count
it calls an empty assert and allocates anyway, so the Level heap and the
top-down heap 0 overlap silently (CONFIRMED, `sub_0003DFA0` 0x0003DFBF..
0x0003DFDA and 0x0003E032..0x0003E05B). Before the next large raise (rooms,
characters) the real headroom has to be measured and an overrun has to be
visible.

## A1. Measuring: probe flag 32

`TSFP_MAPMAKER_PROBE=32` already logs the game heap once a second when it
changes. Each line now also has the host side:

    ... reserves=3 host contig_extent=... contig_extent_peak=... contig_live=...
        contig_live_peak=... contig_free_min=... runtime_heap_live=...
        runtime_heap_peak=... runtime_heap_free_min=...

- `contig_*`: the 64 MB contiguous window at 0x80000000 (less the 128 KB GPU
  instance at its top). The game arena (47,104,000 bytes), textures, vertex
  and push buffers and the MapMaker map container and undo ring come from
  it. `contig_extent` is `xbox_ContiguousAllocatedBytes()` (the highest
  live block, not a sum); `contig_live` is the sum of live blocks from the
  runtime's allocation observer. `contig_free_min` = 0x83FE0000 minus the
  highest end ever seen: the most a larger arena could take.
- `runtime_heap_*`: the runtime heap (`xbox_HeapAlloc`, guest RAM below
  0x04000000). `runtime_heap_free_min` = 0x04000000 minus the highest end
  ever seen.
- The observer (`pc_capture_allocation`, registered at start-up by the
  title's `main.c`) is called under the allocator's lock for every
  successful allocation and release; the counters are
  `pc_allocation_usage_note` in `src/diagnostics/pc_breadcrumbs.h`, kept in
  `src/diagnostics/pc_crash.c` (Windows) and `src/native/crash_posix.c`
  (Mac). Peaks are for the whole run, not per scene.
- Also fixed: the flag 16 container walk now reads the whole 512 KB
  container (it stopped at 32 KB).

## A2. Heap guard (always on, read-only)

`pc_mapmaker_memory_heap_alloc` (`sub_0003DFA0(size, heap)`) and
`pc_mapmaker_memory_heap_alloc_aligned` (`sub_0003E0B0(size, align, heap)`)
run the original unchanged, then read the free count the heap uses: heaps 0
and 1 share `[0x006B8384]`, heap 2 (Eternal) has `[0x006B8378]`, heaps 3 and
4 have none (CONFIRMED, jump table at 0x0003E094). The first allocation that
leaves it negative is logged with size, heap, caller and scene:

    [mapmaker] heap full: heap 1 size 123456 caller 0x0009A1B2 scene 100 free 1000 -> -122456 (Level heap and heap 0 overlap; 1 allocations while overrun; minimum -122456)

Further allocations while it stays negative are only counted; when a new
level resets the heap and it overruns again, that is logged too, at most 8
lines per run. Allocation behaviour is unchanged (refusing would need every
caller to tolerate 0; INFERRED that most do not).

## A3. Arena switch (off by default)

`TSFP_MAPMAKER_ARENA_MB=1..16` adds that many MB to the arena size dword
`[0x004C0450]` (stock 0x02CEC000) in `pc_mapmaker_memory_arena`
(`sub_00023930`, called once at start-up from 0x0003C330). CONFIRMED by a
decoder scan: the dword has exactly two readers, 0x00023CDA (the arena's
`MmAllocateContiguousMemoryEx(size, 0, -1, 0x1000, 4)` at 0x00023CE9,
result `[0x006B7A24]`) and 0x0003C34E (`sub_0003DF00(base, size)`: Eternal
heap 0x9DC00 at the base, Level heap above it, heap 0 down from base+size,
shared free count = size - 0x9DC00, less the 0x266000 reserve). So the extra
MB become free space shared by the Level heap and heap 0, **in every scene**
(story levels too; INFERRED harmless, the heaps are used by count). Before
writing the dword the hook takes and frees a block of the raised size from
the window; when that fails the stock size is kept. One line reports the
outcome. Values outside 0..16 are ignored with a warning.

Why it is not on by default: the window's headroom is unmeasured. Textures
(and the enhanced texture pack) and the MapMaker container share it; if a
later allocation finds the window full the runtime logs
`[CONTIG] arena exhausted` and the game fails. Use a value well below
`contig_free_min` from a probe run (keep at least 2 MB spare), measured
with the heaviest content you use (enhanced textures on, the biggest map,
a story level). Growing the extension block or moving pools out of the Level
heap (plan step A3/A5) is not done in this round.

## B. Cheap raises (`TSFP_MAPMAKER_LIMITS=1`)

| Limit | Round 2 | Round 3 | Deciding constraint | Cost |
| --- | --- | --- | --- | --- |
| Items (incl. free objects) | 200 | 500 | engine objects: about one per item in the 1000-slot pool (462 used at 236 tiles + 200 items); 500 items stay below it with 64 characters, INFERRED | node pool 5300 x 60 B = 311 KB editor heap (+184 KB) |
| Keycards | 8 per colour | 16 | none of their own (pickup records by count, 0x58 B each, Level heap) | none |
| Movers / props / markers | 80 each | 160 each | the item total and the object pool; the slot-0/8 object lists (448/200) drop and log when full | none |
| Story AI | 255 | 511 | the editor's menu item pool (one list row per AI, up to 18 for an 8-point patrol): raised 3072 -> 6144 items, 12 rows per AI on average; 1023 would need 0x3000 items (+3.6 MB editor heap, unmeasured) | editor object +116 KB (0x58390 -> 0x74C00), menu pool +0.9 MB |

Items: `MAPMAKER_LIMIT_SUB` 500; `items.json` sizes the editor's item node
pool for 5300 nodes (bytes 0x4DA30, loop end 0x4D9F4, last next pointer
0x4DA08, free count 0x14B4); the grid stack hook's arrays hold 512 items of
one cell plus 511 x 8 Story AI points. Decoder (CONFIRMED,
`test_mapmaker_limits_classified.py`): the pool pointer `[0x00784008]` is
used only by `sub_002FE430` (clear), `sub_00302A60` (allocation) and
`sub_00307C20` (free-list init), and the four size constants are the four
table sites. Play time: items are Level-heap arrays by count (`sub_00240590`).

Story AI 511:
- `editor.json`: the 136 Story AI operand rewrites now move the array to the
  end of the editor object, +0x58390 (delta 0x58108), the object is 0x74C00
  bytes (allocation and clear), the two Story AI clears are 0x71C7 dwords
  and `sub_00323220`'s unchecked pointer array has 530 slots (frame 0x84C).
  The old +0x11950 block is unused. `sub_00240590` already has 1024 slots.
- `sai-index.json`: the selection index `[0x00762F94]` becomes a word with
  11 same-length rewrites and no wrapper (CONFIRMED sites; the bytes
  0x00762F95..97 have no references): the getter `sub_0031BA80` returns the
  dword (`a0`->`a1`), the setter `sub_0031BA90` loads and stores the dword
  argument (`8a`->`8b`, `a2`->`a3`; its one caller 0x003057E3 pushes the
  dword `[editor+0x2F20]`), `sub_0031BAA0` and the four readers zero-extend a
  word (`0fb6`->`0fb7`), and the list row callback `sub_0031BF00` reads the
  row id `+0x18C` as a dword, indexes with its low word and stores the dword.
  Below 256 every value is the same as stock, so this applies in every scene
  without a switch.
- C: `MAPMAKER_EDITOR_SAI_CAP`/`_LIMIT` 511 (loader cap and editor limit),
  aitemplates/bosses tables 511 and their selected index read as a word, the
  grid stack hook's array offset, `aitemplates-pool.json` 6144 items.
- Container: 511 x 120 B = 60 KB of chunks at most, inside the 512 KB
  container. Simultaneous AI are unchanged (64 characters).

## Save format: maps past the stock limits

The current branch already saved maps larger than 32 KB in a larger tier,
which the stock loader refuses (error 7). A map within 32 KB but past a
stock count (51 Story AI, 60 items, 3 keycards of a colour, 201 tiles, ...)
still saved stock-identical, and a stock build would load it and overflow a
fixed table (stack arrays, the editor object, the turret and object pools).
`mapmaker_container_over_stock` now forces at least the 64 KB tier for such
a map: from the chunk counts of the buffer being saved (free objects > 170,
Story AI > 50, nodes > 130, ops > 30, records > 100, header pieces > 200)
and, in the editor (scene 102), from the editor's counts (tiles > 200, pad
costs > 2400/1024/4400, pad groups > 300, items + free objects > 50, any
category over its stock maximum, free objects per type > 40/30/100, Story
AI > 50, ops > 30, nodes > 130, records > 100). Maps within every stock
limit are unchanged byte for byte. This build loads both (stock size first,
then the tiers). System Link only carries stock-size maps, so such a map
cannot be played over System Link (as with large maps already).

## Round 4 (2026-11-02): nav block, memory budget, map-start line

Static analysis and fixtures only; nothing built or played.

Nav block. The AI navigation data of a MapMaker map is CPU-only and grows
with the raised caps: regular pads 8000 x 32 B, per-pad AI data 8000 x 44 B,
links 32,512 x 20 B, cover 10,496 x 32 B, runtime pads 0x280 B (1.59 MB in
all, was 1.03 MB) and the group distance matrix ((N+1) x N x 4 B, 4.2 MB at
1024 groups, 1.44 MB at 600). In scene 100 it now comes from one 6 MB block
of the runtime heap (`xbox_HeapAlloc`, guest RAM), reserved at the first
MapMaker map and kept for the run, instead of the Level heap:

- `pc_display_mapmaker_pools` takes each pool through `mapmaker_pool_source`
  (same order, zeroed; Level heap when the block refuses);
- the heap hook answers `sub_0003DFA0(size, 1)` from return address
  0x00275F95 (the matrix in `sub_00275F70`, which zeroes it itself) from the
  block, skipping the original, so the Level heap's free count is untouched;
  the aligned allocator and every other caller run the original;
- `pc_mapmaker_memory_level_reset` (WRAP `sub_0003E650`, the Level heap reset,
  single caller 0x0003D504; the only writer of the Level base besides
  start-up `sub_0003DF00`/`sub_0003DF70`) empties the block after the
  original, so its lifetime is exactly the Level heap's. Until that hook has
  run once nothing is taken from the block (no wrapper, no block).
- `TSFP_MAPMAKER_NAV=0` keeps everything in the Level heap.

Why the runtime heap and not the window: GPU and APU data must be in the
contiguous window (physical address = window offset; `pc_mapmaker_container.inc`
notes a runtime-heap buffer that hung the front end), so only data no device
reads moves. Pads, links, AI data and the matrix are read by AI code only.
Why it is free (INFERRED, not measured): guest RAM is 64 MB; the image and
the extension end at 0x00920000 and the main stack at 0x011A0000, leaving a
46.4 MB runtime heap (`src/runtime/guest_thread_memory.c`). On the Xbox the
title's virtual/pool allocations shared 64 MB with the 9 MB image and its
47.1 MB arena (at most about 8 MB); thread stacks add at most 4 MB; the
MapMaker host tables are small. So a 6 MB block leaves more than 25 MB. The
map-start line prints `runtime_heap_free_min` (when the probe include is
present, always in the game) to confirm it.

Effect: about 1.6 MB of pools plus the matrix (up to 4.2 MB) leave the
Level heap on every MapMaker map; for the 236-piece test map (about 350
groups) that is about 1.5 MB more Level headroom at the same content, and at
511 pieces about 4 MB (pools plus a 770-group matrix). Not done: other
per-tile Level allocations (geometry, collision, props) stay, since some may
be device-visible.

Map-start line (stderr, once per map, at the first nav allocation):

    [mapmaker] map start memory: level heap free F, level used U, heap 0 used H, arena A at 0x..., eternal free E; nav block N of 6291456 bytes at 0x... (runtime heap), K fallbacks; host runtime_heap_live L runtime_heap_free_min R contig_free_min C
    [mapmaker] nav: group matrix of G groups, B bytes, in the nav block (nav block N of 6291456 used); level heap free F

The second line comes when the matrix is built (mode 2/3 maps), late in the
load, so its `level heap free` shows the headroom after the level's own
allocations. A failed reservation logs once and the Level heap is used.

Memory budget (`pc_mapmaker_budget.inc`): `TSFP_MAPMAKER_MEMORY_SCALE` now
accepts 2..16 (was 2..8). With `TSFP_MAPMAKER_LIMITS=1` and no explicit scale
the default stays 8 for the stock arena and gains one step per 2 MB the arena
switch really added (`mapmaker_budget.arena_mb`, set by
`pc_mapmaker_memory_arena` after its trial allocation succeeded), up to 16 at
`TSFP_MAPMAKER_ARENA_MB=16`. The editor estimate is about 2x the real Level
use, so this keeps that margin. 511 pieces of the heaviest tile estimate
21.7 MB plus 40 KB per item type: scale 8 (16.4 MB) stops them at about 386
pieces, scale 11 (`ARENA_MB=6`) fits them with about 20 item types, scale 16
(32.8 MB) with every type. The arena itself stays off by default: the
window's headroom is unmeasured (`contig_free_min`), and on the Xbox the
non-arena window use was up to about 8 MB, so only a measured value is safe.

Container: no change. 511 pieces (about 40-100 B each with their headers),
500 items (28 B and up), 511 Story AI (120 B) and logic stay below about
200 KB, inside the 256 KB tier of the 512 KB container; the undo ring is
2 x 512 KB. Old saves load as before (stock size first, then the tiers).

## Wrappers for the integrator

    WRAP sub_0003DFA0 -> pc_mapmaker_memory_heap_alloc          (area "memory")
    WRAP sub_0003E0B0 -> pc_mapmaker_memory_heap_alloc_aligned  (area "memory")
    WRAP sub_00023930 -> pc_mapmaker_memory_arena               (area "memory")
    WRAP sub_0003E650 -> pc_mapmaker_memory_level_reset         (area "memory", 2026-11-02)

`sub_0003E650` takes no arguments, ends in a plain `ret` (0x0003E731), and the
hook calls the original exactly once before emptying the nav block.

All three call their original exactly once and leave esp to it (plain
`ret`, CONFIRMED 0x0003E031/0x0003E062/0x0003E07A/0x0003E08C/0x0003E090 for
`sub_0003DFA0`). `pc_mapmaker_memory.inc` is included first among the area
includes in `pc_menu_mouse_guest.inc`; declarations go between the area hook
markers in `src/interface/pc_display_menu.h`. The C hooks of other areas that
allocate with `guest_call(sub_0003DFA0, ...)` go through the guard as well.

## Tests

- `tests/mapmaker/test_mapmaker_memory.py`: guard and arena fixture, 13
  mutations, and a decoder check that `[0x004C0450]` has two readers.
- `tests/mapmaker/test_mapmaker_limits_classified.py`: Story AI index sites,
  helper callers, widening-only bytes; item pool sites.
- `tests/mapmaker/test_mapmaker_probe.py` (host fields, 32 KB walk),
  `test_mapmaker_container.py` (stock-limit tier, 13 new mutations),
  `test_mapmaker_limits.py`, `test_mapmaker_editor.py`,
  `test_mapmaker_aitemplates.py`, `test_mapmaker_bosses.py`,
  `test_mapmaker_grid.py` follow the new values.

## User test plan

1. Measure first (no `TSFP_MAPMAKER_ARENA_MB`): `TSFP_MAPMAKER_PROBE=96`,
   `TSFP_MAPMAKER_PROBE_LOG=<file>`, `TSFP_MAPMAKER_LIMITS=1`, enhanced
   textures on. Open the editor with the biggest map, play it, then play
   story level 1. Report the last `heap ... host ...` line of each scene
   (`min_free`, `contig_free_min`, `runtime_heap_free_min`), the `pools`
   line (`objects=N/1000`, `lists=`), and any `[mapmaker] heap full` or
   `[CONTIG]` line on stderr.
2. Only if `contig_free_min` is above about 10 MB: repeat with
   `TSFP_MAPMAKER_ARENA_MB=8` (or `contig_free_min` minus 2 MB, rounded
   down); check the start-up line `[mapmaker] arena: ... -> ... bytes`, that
   `arena=` in the probe shows the new size, that `min_free` rose by that
   much, and that the front end, a story level and the texture pack still
   load.
3. Max content (`LIMITS=1`): 500 items (with 16 keycards of one colour, 160
   movers and 160 props), 511 Story AI (Duplicate/linked copies make this
   quick; scroll the list to the bottom, select #300 and #511, edit, close),
   save, reload, play. Watch for `menu item pool full`, `heap full`, the
   probe's `objects=` (must stay below 1000) and the list guards
   (`[mapmaker] ... full`).
4. Save format: save a small map with 51 Story AI or 3 keycards of one
   colour and check its file is 64 KB (not 32 KB); it must still load here,
   and the original game must refuse it.
