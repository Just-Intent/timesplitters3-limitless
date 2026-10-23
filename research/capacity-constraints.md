# MapMaker capacity constraints

Date: 2026-10-22. Labels as in `research/mapmaker-investigation.md`
(CONFIRMED / COMMUNITY / HYPOTHESIS). No MapMaker code has been disassembled
yet; this file lists what is known, the candidate limits, and how to test each.

## 1. What the meter is

- CONFIRMED: `sub_0030C210` draws the MapMaker capacity bar through
  `sub_000814B0`, whose first stack argument is the fill fraction (float)
  (`STATUS.md:2552-2556`; `src/pc_menu_sliders.inc:55-69`;
  `src/pc_display_menu.c:547`). The meter is display-only in the PC port.
- COMMUNITY: the in-game label is "FREE"; items cost more than tiles; using many
  different tile/item types costs more than repeating one; Future Perfect has
  about 190-200 "tile units"; separate pools are reported for tiles, items,
  starts, Story AI and possibly logic; one TS2 report says logic has a hidden cap
  that silently ignores additions while the bar still shows space. Sources:
  the MapMaker guide PDF
  (<https://timesplitters-world.de/tsfp/mapmaker/TSFP_MapMaker_Swatt.pdf>),
  Neoseeker and Rec Room threads (search snippets only; see the investigation
  file). The "Map Memory Glitch" for extra space is recorded as debunked
  (<https://timesplitters.fandom.com/wiki/Map_Memory_Glitch>).
- No developer statement says whether the meter measures real console memory or
  a designer cap.

## 1a. Meter formula (static analysis of local disassembly, 2026-10-22)

Read from the local XBE disassembly (`private/pipeline/disasm`, never
committed); constants read from the local `default.xbe`.

- CONFIRMED (code): the editor object pointer is the global `0x007844A8`.
  `sub_0030C210` passes `1.0 - max(sub_0030C180(), editor[+0x270])` as the
  bar fraction. After drawing, it tints the bar red when
  `editor[+0x270] + editor[+0x274] > 1.0`; `+0x274` is probably the cost of
  the piece about to be placed (HYPOTHESIS).
- CONFIRMED (code): `sub_0030C180` combines two integers, each multiplied by
  the float at `0x0049F7AC` = 3.0547e-5 = **1/32736** (`0x7FE0`):
  `sub_002FDF10()` (a constant `0x514` = 1300) and
  `sub_002FE0C0(*0x00784014)`, which returns field `+0x248` of the object
  found by `sub_0032A5B0(obj, 0x00C77668)`.
- HYPOTHESIS (strong): the meter measures **bytes of map data against a
  ~32 KiB budget** (1300 bytes fixed overhead), not object counts. That links
  C1 directly to C3/C6. Next: `RECOMP_WATCH_WRITE` on that `+0x248` field
  while placing pieces, and compare with saved map file sizes (H-SV2).
- First run (flags 11, scene 102): fractions 1.000, 0.984, 0.993, 0.984,
  0.973, 0.953, 0.940 as pieces were placed; the step list was not recorded.

## 1b. Two budgets behind the meter (2026-10-23: saves, probe run, static analysis)

The meter shows `1 - max(byte budget, category budget)`; either one fills it.

**Byte budget (the map container).** CONFIRMED:
- Every saved map (`UserData/45410066/<slot>/anicemap.mkr`, name string at
  `0x0047DF80`) is exactly 32756 bytes: a 0x7FE0 (32736) byte container plus
  a 20-byte signature (likely HMAC-SHA1 as for profiles; unverified).
- The container is a tagged-chunk blob: dword `0x00C77667`, version `0x12D`,
  then `{tag, size, data}` chunks (map info `0x00C77668`, 0x3DC bytes, whose
  `+0x248` is the bytes used including the terminator; `0x69`, `0x6B`..`0x75`
  per editor collection), ended by `0x00C7766A`. It lives at `[0x00784014]`
  in a 0x8000 allocation (`sub_002FE200`/`230`), is rebuilt from the editor
  object by `sub_003006C0` (no bounds check while writing), saved by
  `sub_00190900` and loaded by `sub_00190530` (exact size 0x7FE0 required,
  signature verified), and read at play time by walkers bounded by
  `+0x7FE0` (`sub_00085920`, `sub_0008A780`, `sub_0023F390`.. and others).
- Placement is refused when `used + cost >= 0x7FE0` (`sub_00300670`, 13
  callers; three inline compares). Undo/redo keeps a shadow container
  (`[0x765E38]`, 0x8000) and a 4x0x7FE0 diff table (`[0x765C2C]`).
- 0x7FE0 is an immediate in about 25 places; the meter divides by 32736 and
  adds a fixed 1300-byte base.
- Measured, one session: empty map 1520 bytes; 5 pieces 1952; about half the
  meter 4144; "full" 5728. The earlier test maps used 3252 and 16396.

**Category budget (`editor+0x270`, written by `sub_002FA720`).** CONFIRMED
code, roles HYPOTHESIS: the largest of seven fractions `count / limit`:
three sums of per-piece-type costs from the table at `0x00529400` (stride
0x140; fields +0, +4, +8; limits 2400, 1024, 4400), a global count
`[0x00762CB8]` (limit 300), a list count (limit 200), type-9 entries plus
`editor+0x306E` (limit 50) and `sub_002FA060()` (limit about 2,048,000).
In the measured session the meter fell in exact 0.05 steps per piece from
the half-way point to empty while the container stayed under 6 KiB: the
category budget, not the 32 KiB container, is what stopped placement.
Which of the seven categories bound is not yet known.

**What binds first: the level memory estimate.** CONFIRMED (probe flag 16,
2026-10-23, tiles only, placed one by one until refused at 112 pieces):
the six recomputed categories peaked at 0.65 (cost1 642/1024; pieces
112/200; global 171/300) while `editor+0x270` reached 1.0, and the
container held 13788/32736 bytes. The binding category is the seventh,
`sub_002FA060() / 2048000` (factor at `0x0049F770`): per placed piece the
row field `+0x10` of `0x00529400` (3 to 32 KB per tile type; these 112
tiles averaged about 18 KB), plus per attached sub-entry 40 KB (`0xA000`)
the first time its id is seen (ids above 9 except 0x45; table
`0x00762AE0`, 80 entries) and 1 KB (`0x400`) for each repeat. It reads as
an estimate of the level's play-time memory with a 2,048,000-byte budget
(HYPOTHESIS: the matching runtime pool and its real size are not found yet).

**Play-time memory (static analysis, 2026-10-23).** CONFIRMED code,
sizes HYPOTHESIS until measured:
- No pool of about 2 MB exists; the 2,048,000 limit appears only in the
  editor (`cmp eax,0x1F4000` at `0x002FAEF2`): a designer budget.
- `sub_0003DFA0(size, heap)` is a bump allocator over one arena of
  47,104,000 bytes (`[0x004C0450]`; base/size `[0x007E72D4]`/`[0x007E72D8]`):
  heap 2 "Eternal" (first 0x9DC00 bytes, holds the map container), heap 1
  "Level" bottom-up and heap 0 top-down, sharing the free count
  `[0x006B8384]`. Per level `sub_0003E650` resets Level space (about
  43.9 MB after the 0x266000 reserve, 42.7 MB if `[0x007DE398]&0x40`).
- A MapMaker match (scene 100) loads its tileset assets
  (`tileset%d/tile%d_%d`, pads `pad/data/tile%d_%d.raw`, via `sub_00085920`
  and `sub_00063390`) into that same Level heap, shared with characters,
  weapons, bots and per-frame buffers (display 2x0xBB800 in scene 100).
- On exhaustion the allocator only calls an assert stub that is empty in
  retail: heaps overlap silently (corruption, no message).
- Arcade level paks are 5-8.4 MB versus 3.0-3.2 MB for the MapMaker base, so
  there is plausibly room for several MB more tiles. Measure with probe
  flag 32 (minimum free during a full match) before raising anything.
- The 200-piece limit matches fixed 200-entry play-time tables
  (`sub_0008A780`) and must stay.

**Measured headroom (probe flag 32, 2026-10-23, Windows).** CONFIRMED for
these runs (team deathmatch with bots; bot sets not identical):
- Arena 47,104,000 bytes; Level heap free stays constant during a match
  (everything is allocated at level start).
- MapMaker match, map 5 tiles (s1): minimum free 13,717,248 bytes.
- MapMaker match, map at the 2,048,000 estimate (112 pieces, several tile
  types): minimum free 12,771,120 bytes.
- Large arcade map (scene 53): 15,922,448; main menu: 16,808,352; editor:
  13,997,808.
- So the full map's tiles really use about 946,128 bytes while the editor
  charges about 1.99 MB: the estimate is about 2.1x conservative (real /
  estimate 0.475). A full map still leaves 12.8 MB free.
- Projected at the hard 200-piece cap with the same ratio: about 12.0 MB
  free with average tiles, about 10.7 MB if every piece were the heaviest
  type (32,085-byte estimate). Attached-asset entries (40 KB estimate each,
  table of 80) are not covered by these runs.
- Conclusion: raising the editor's memory estimate so that the 200-piece cap
  (and the container) bind instead is well within the measured memory. Each
  raised map must still be checked in play with flag 32 (minimum free must
  stay well above zero; negative means corruption).

**Raised budget (opt-in, 2026-10-23).** `TSFP_MAPMAKER_MEMORY_SCALE=N`
(2..8) divides both estimate functions' results (rounded up), so placement
and the meter allow N x 2,048,000 bytes of estimate. With the measured ratio
N=3 covers 200 pieces of the heaviest tile type (projected free about
10.7 MB). Maps made this way stay in the original container format while
under 0x7FE0 bytes; the stock editor will still load them but would refuse
further placement and show a full (or overfull) meter. Unvalidated in play.

After it, in this tile-only map: the piece count (200) at about 200 tiles,
then the container at roughly 280 tiles (about 111 bytes per tile). So the
memory estimate could rise about 1.8x before the save format must change.

Other fixed limits seen: `editor+0x280 < 50`, `+0x41D8 < 10`, `+0x41E0 < 30`,
play-time tables of 45 and 200 entries (`sub_00085920`, `sub_0008A780`), and
50 saved maps.

**Consequences.** Raising capacity has two parts. (1) The category budgets
are probably design caps standing in for play-time pools (geometry, lights,
AI, navigation); raising one safely needs the matching runtime pool checked.
(2) Bigger containers need every 0x7FE0 site, the buffers above and a
size-gated save format (stock 32756-byte files must still load). Bypassing
`sub_00300670` or hiding the meter without (2) would let the compiler write
past the 0x8000 buffer: heap corruption.

## 1c. Limit inventory (static analysis, 2026-10-23)

Four parallel read-only passes over the disassembly. "Designer" = an editor
estimate with dynamically sized play-time data; "hard" = a fixed table or
pool that overflows silently (no retail error path). All UNVERIFIED in play
unless noted.

| Limit (editor check) | Guards at play time | Kind | Raise cost | Target |
| --- | --- | --- | --- | --- |
| Memory estimate 2,048,000 (`0x002FAEF2`) | Level heap (~12 MB free, measured) | Designer (measured) | Done: `TSFP_MAPMAKER_MEMORY_SCALE` | x3 |
| cost0 2400 (`cmp 0x960` at `0x002FAE0A`) | Regular AI nav pads, pool `[0x715F2C]` 2400x32 B + AI data `[0x715F3C]` 2400x44 B (`sub_0009A140`) | Hard pool | Pool size immediates + editor compare; ids must stay < 8000 | x2 (4800) |
| cost1 1024 (`cmp 0x400` at `0x002FAE5C`) | Secondary (cover) pads `[0x715F34]`: 1280x32 B in scene 100 | Hard pool | Editor compare only up to 1280; beyond needs the handle range 0x1F40..0x209E remapped (~75 sites) | 1280 |
| cost2 4400 (`cmp 0x1130` at `0x002FAEAE`) | Pad links `[0x715F30]` 4400x20 B, plus cross-tile/object links | Hard pool | Pool size immediates + editor compare | x2 (8800) |
| Pieces 200 (`cmp 0xC8` at `0x002FAEE0`) | Engine-wide 200 visibility rooms: per-room arrays inside the 0xFFC character struct (+0x3C8, +0xD2C, +0xE08), `0x7042B8` [80][200], a 0xE0-byte per-room alloc, byte room ids | Hard, engine-wide | Rework of the visibility system (30+ per-frame functions) | Keep 200 (or count rooms instead of pieces) |
| MapMaker room tables (`0x702E20`..`0x7038F8`, `0x708138`, 200 each) | Per-room tile transforms | Hard, local | 4 functions | Only with the room limit |
| 45-entry model/pad tables (`sub_00085920`) | Unique tile models per map (`0x785000` maybe 32) | Per catalogue | None needed | n/a |
| Global 300 (`0x002FAEF9`) | AI nav pad groups; N x N distance matrix on the heap (`sub_00275F70`) | Designer (N^2 load cost) | Editor compare | 500-600 after timing load |
| Sub-entries 50 (`0x002FAF2E`) | Attached items/objects, heap by count (`sub_00240590`); but `sub_0023FD10` writes objects of kinds 10..14 into two 10-slot stack arrays without a bound | Designer, with an unchecked stack array | Bound or relocate those arrays first | Keep 50 |
| Story AI 50 (`editor+0x280`, `0x0031BE8B`) | Editor array 50x0xE4 (full) and an unchecked 50-slot stack array in `sub_00240590` | Hard | Relocate editor array, reimplement `sub_00240590` | Keep 50 |
| Logic operations 30 (`+0x41E0`), objectives 10 (`+0x41D8`) | Editor arrays sized exactly 30 (and nodes 130, records 100); heap at play time | Hard in editor only | Relocate editor arrays (~40-50 sites each) | Keep until relocated |
| Map container 0x7FE0 | Save/load, undo, play-time walkers, System Link transfer (fixed 0x8000) | Hard format | ~33 functions, mostly constant swaps; 16-bit size fields cap at 64 KB | 64 KB (size-gated) |
| Header byte +9 (`sub_00300630`) | Spawn points required | Validity check | Not a budget | n/a |

Other: the save signature is HMAC-SHA1 with a zero key over the body (same
as `tools/testing_profile.py`) and the body ends with a zlib CRC-32 at
`used`; System Link sends the map in 239-byte fragments with byte indices
(max 60,945 bytes) and no receive bound check.

Tooling: the generator has no address-keyed constant override; raising a
compare or pool-size immediate needs either that feature or a wrapper that
reimplements the function (practical for `sub_0009A140` and `sub_002FA720`).

**Stage A (implemented, 2026-10-23, UNVERIFIED in play).** Wrappers in
`patches/*-mapmaker-meter-stagea.patch`, policy `src/pc_mapmaker_limits.inc`:

- `sub_0009A140` in scene 100 (on by default, `TSFP_MAPMAKER_POOLS=0` restores
  it): pads 4800x32 B, links 8800x20 B, AI data 4800x44 B, cover 1280x32 B,
  runtime 0x280 B; same heap (1), order, zeroing and counters. About 290 KB
  more Level heap.
- `sub_002FA720` (opt-in `TSFP_MAPMAKER_LIMITS=1`, only with the pools on):
  after the original, the decision bit (`editor+0x25C` 0x400000), meter
  (`+0x270`) and held preview (`+0x274`) are recomputed with pads 4800, cover
  1280, links 8800 and pad groups 600. Pieces 200, sub-entries 50 and the
  memory estimate (scaled separately) are unchanged.
- Open questions for play: stability with cover pads above 1024 (cover
  handles 0x1F40 + index already overlap the runtime pad handles
  0x209E..0x20B1 at index 350..369 in stock; 1280 is the pool size the game
  itself allocates in scene 100, so no new handle range is used); load time
  with 600 pad groups (N^2 matrix). Probe flag 64 logs the counts.

## 2. Host memory is not the obvious constraint

CONFIRMED (runtime source):

- The guest has the retail 64 MiB (`runtime:src/kernel/xbox_memory_layout.h:47`;
  `xbox_SetTotalRam`/`xbox_SetMapSize` are never called).
- GPU/contiguous memory has its own separate 64 MiB mapping at `0x80000000`
  (`xbox_memory_layout.c:2548`), more than real hardware.
- Save storage is plain host files with no quota; free space is the host disk
  (`kernel_file.c:582-596`).

So the PC runtime does not add a MapMaker limit of its own. Whatever limits
exist come from the game code and its fixed data layout.

## 3. Candidate limits

Each candidate needs evidence before any budget change. "Risk if raised"
describes what could break.

| # | Candidate | Evidence so far | How to confirm | Risk if raised |
| --- | --- | --- | --- | --- |
| C1 | **Meter denominator**: a constant or global capacity the meter divides by. | COMMUNITY numbers (190/200 units). An unverified PS2 NTSC-U CodeBreaker line `206c42d4 00000400`, offered as "any size level", may write 1024 to a 32-bit word, consistent with a writable capacity value; another user reported it did not work. Platform/address do not transfer to Xbox. | Trace the float passed by `sub_0030C210` back to its operands. | Low on its own; display only. |
| C2 | **Placement checks**: comparisons that refuse to place a tile/item/light/AI/logic entry when its pool is full. | COMMUNITY reports of separate pools and a silent logic cap. | `RECOMP_WATCH_WRITE` on the counters from C1, then find the compare that blocks placement at the limit. | Medium: only safe if C3 holds. |
| C3 | **Fixed-size editor arrays**: per-category arrays (tiles, items, lights, AI, logic) sized at the limits. Writing beyond them corrupts adjacent data. | Inferred from fixed counts; the 7th-floor glitch suggests the grid storage is larger than the UI exposes. | Find the array base and element size from the placement routine; check what follows each array in memory. | **High**: overflow, silent corruption, crashes. |
| C4 | **Grid dimensions/floors**: fixed X/Y/floor extents. | COMMUNITY: 40 rows; floors 1-6 connect normally. | Cursor clamp routine (H-IN4) and save layout. | High: the level compiler, navigation and rendering may assume them. |
| C5 | **Built-level budget**: when a map is played, tiles are instantiated into level geometry, collision, AI navigation (pads/paths), lights and portals. Those runtime systems may have their own fixed pools sized for the maximum legal map. | Inferred; costs scaling with the variety of tile types suggests per-unique-asset loading (textures/meshes). | Play a maximum-size map and watch heap (`[HEAP]` histogram), pool allocations and frame time; then try a map slightly over the limit (only after C3 is solved). | **High**: the main risk to "bigger maps that actually play". |
| C6 | **Save buffer / format**: the map file may be a fixed-size buffer or have fixed counts in its header. | Community files of different sizes (4-100 KiB, unknown platform). | H-SV1/H-SV2 in the investigation file. | High for compatibility: original-format maps must keep loading, and larger maps must not be offered to an unmodified game. |
| C7 | **Network map transfer**: System Link/online sharing of maps may assume the maximum size. | Online MapMaker existed on Xbox. | Out of scope until C1-C6 are understood; keep larger maps local-only initially. | Medium. |
| C8 | **Memory per unique asset**: tile variety cost suggests tileset assets are loaded per type into a fixed heap. | COMMUNITY. | Correlate the meter fraction with unique-type count, not placement count. | Medium: real guest heap (~47 MiB general heap, estimated) could become the true limit. |

## 4. Conclusions so far

- CONFIRMED: no PC-runtime-imposed MapMaker limit exists; the PC port has more
  GPU memory and unlimited save storage.
- HYPOTHESIS (most likely): the budget is several fixed per-category capacities
  checked at placement time, backed by fixed arrays and a fixed save layout.
  Raising the displayed limit alone (C1) is not enough and would be unsafe
  without C3, C5 and C6.
- Per the project constraint, the budget must not be bypassed until the
  underlying arrays, level-building pools and save format are identified and
  tested.

## 5. Next measurements (in order)

1. E1 (read-only meter probe, implemented; run it per
   `docs/mapmaker-meter-probe.md`): record the meter fraction while placing one tile, one item, one light, etc. in an empty
   map. Each category's cost and whether costs depend on type variety follow
   directly.
2. Trace the fraction to its counters and capacity constants (offline, local
   generated C).
3. Watch-write the counters to find placement checks and arrays.
4. Save empty/half/full maps in a disposable save root and compare files.
5. Only then design an expanded budget with a size-gated save format.
