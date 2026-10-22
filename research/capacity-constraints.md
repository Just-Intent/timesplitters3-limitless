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
