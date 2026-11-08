# MapMaker AI navigation pads: cover range, A* pools, link pool

Implementation notes for the "pads" area of the MapMaker limit raise
(2026-10-23). Static analysis of the US XBE (title 45410066) disassembly and
the generated C; nothing here has been tested in play yet. CONFIRMED = seen
in the disassembly at the given address; INFERRED = reasoning, not observed.

## What changed

| Item | Stock | Now | Where |
| --- | --- | --- | --- |
| Cover handle range | 350 (1280 in scene 100, overlapping runtime handles) | 14,336 at 0x4000 in every scene (Layout C, 2026-11-07; 10,240 at 0x1F40 in Layout B) | `config/guest-code-patches/pads.json` (88 runtime + 45 cover-base rewrites) |
| Runtime pad handles | 0x209E..0x20B1 | 0x7800..0x7813 (Layout B: 0x4740) | same |
| Scene-100 cover pool | 1280 x 32 B | 14,592 x 32 B (range + 256 guard) | `MAPMAKER_POOL_COVER`, `pc_display_mapmaker_pools` |
| Editor cover limit (opt-in) | 1024 | 14,080 (range - 256, the stock spare; 9,984 before) | `MAPMAKER_LIMIT_COVER` |
| Link pool (scene 100) | 4400 x 20 B (editor 4400) | 32,512 x 20 B, fixed (int16 A* indices); editor: template links + half the side slots <= 32,256 (2026-11-07; 16 per piece from 2026-11-02; was 16,336 / 12,000); stitcher stops at the pool | `MAPMAKER_POOL_LINKS`, `pc_display_mapmaker_pools`, `pc_mapmaker_pads_stitch` |
| Group A* node pool | 1000 nodes .bss 0x0070E178, no bound | 8192 nodes at 0x008E0000, bounded | `pads.json` relocation + `pc_mapmaker_pads_group_node` |
| Pad A* node pool | 1000 nodes .bss 0x00712058, no bound | 1000 nodes, bounded | `pc_mapmaker_pads_pad_node` |
| Start-point cover top-up | unbounded | refused past the cover capacity (scene 100) | `pc_mapmaker_pads_cover_topup` |
| Pad id threshold (13 compares) | id > 8000 is cover | id > 9000 (0x2328) is cover | `pads.json` `pads-idthreshold-*` (2026-11-02) |
| Regular pads (scene 100) | pool 2400, editor 2400 | pool 16,384 (handles 0..0x3FFF), editor 16,128 (8000 / 7,680 before 2026-11-07) | `MAPMAKER_POOL_PADS`, `MAPMAKER_LIMIT_PADS` |
| Pad ids past 8000 | none | regular pads 8000.. take 0x5B29.. (above the cover ids) | `pc_mapmaker_pads.inc` hooks, `pads-regid-0009a37e` |
| Pad groups | 300 | 2047; distances computed on first use above 1024 | `MAPMAKER_LIMIT_GROUPS`, `pc_mapmaker_pads_group_matrix` |

Unchanged on purpose: every existing pad id (regular 0x3E9 + i below 8000,
cover 0x2329 + i, runtime -2), so saved maps and story files read the same.
The cover handle base moved 0x1F40 -> 0x4000 on 2026-11-07 ("Layout C"
below); before that it was kept.

## Wrappers for the integrator

    WRAP sub_0009B090 -> pc_mapmaker_pads_group_node
    WRAP sub_0009B730 -> pc_mapmaker_pads_pad_node
    WRAP sub_0023F9D0 -> pc_mapmaker_pads_cover_topup
    WRAP sub_0009DC70 -> pc_mapmaker_pads_stitch
    WRAP sub_0009B540 -> pc_mapmaker_pads_group_route      (pc_mapmaker_astar.inc)
    WRAP sub_0009BCF0 -> pc_mapmaker_pads_nearest          (pc_mapmaker_nearpad.inc, after .73)
    WRAP sub_0006A8B0 -> pc_mapmaker_pads_random           (pc_mapmaker_randpad.inc, after .73)
    # 2026-11-07, Layout C (need regeneration; all area "pads"):
    WRAP sub_0009A3C0 -> pc_mapmaker_pads_cover_lookup
    WRAP sub_0009CCA0 -> pc_mapmaker_pads_pointer
    WRAP sub_0009D3B0 -> pc_mapmaker_pads_runtime_new
    WRAP sub_0009A7A0 -> pc_mapmaker_pads_tile_load
    WRAP sub_00332490 -> pc_mapmaker_pads_editor_id
    WRAP sub_002FA150 -> pc_mapmaker_pads_editor_groups
    WRAP sub_00334B20 -> pc_mapmaker_pads_editor_item_pads
    WRAP sub_00275F70 -> pc_mapmaker_pads_group_matrix
    WRAP sub_00275F40 -> pc_mapmaker_pads_group_distance

`src/interface/pc_mapmaker_pads.inc` must be included after `pc_mapmaker_limits.inc`
(it uses `mapmaker_limits`, `mapmaker_limits_read`, `MAPMAKER_LIMIT_COVER`
and `MAPMAKER_POOL_COVER`, and checks at compile time that the editor limit
leaves 256 spare below the handle range and the pool holds the range). All
three originals are cdecl and every caller ignores eax (CONFIRMED:
`0x0009B4F6`, `0x0009BAA7`, `0x0023FBFD`, `0x0023FC50`, `0x0023FCDF`), so a
refusal is a plain return. Each refusal is counted in `mapmaker_pads`
(`group_refused`, `pad_refused`, `cover_refused`) for a probe.

## Layout B: runtime handles above a 10,240-entry cover range

Pad handles (int32, run time only): regular = index (< 0x1F40), cover =
0x1F40 + index, runtime = base + index (20 slots, `cmp edi,0x14` at
`0x0009D3CD`). Layout B keeps the cover base and moves the runtime base
from 0x209E to 0x1F40 + 10,240 = 0x4740.

### The sites (CONFIRMED, each against text.asm and the generated C)

A scan of `text.asm` for every non-branch instruction using 0x209E, 0x20B2,
0x2440, 0xFFFFDF62 (-0x209E) or the folded runtime displacements -0x413BC /
-0x413D8 gives 89 instructions in 25 functions (the list in
`logs/limits/pads.md` §1.4, minus the cover-base and id-threshold sites that
stay). Wider searches found nothing else: 0x209C..0x209F, 0x20B0..0x20B3,
0x243F/0x2441, -0x209D/-0x209F, any folded `handle*4/8/32/44` displacement of
the runtime base, and the same values in the generated C (decimal forms
included). The only initialised data word in 0x209E..0x20B2 is 0x0052 97D0 =
0x20A5, the memory estimate (+0x10) of tile type 3, not a handle.

| Original constant | New | Sites | Meaning |
| --- | --- | --- | --- |
| 0x209E (cmp, sub, add, lea disp) | 0x4740 | 51 | runtime base / cover end |
| 0x20B2 (cmp) | 0x4754 | 24 | runtime end |
| 0xFFFFDF62 (add, lea disp) | 0xFFFFB8C0 | 11 | runtime handle -> index |
| 0x2440 at `0x0009A45B` | 0x4740 | 1 | scene-100 cover end in `sub_0009A420` (now redundant, harmless) |
| -0x413BC at `0x002B0456` | -0x8E7FC | 1 | folded `runtime[h - base].flags` (+4) |

`tests/mapmaker/test_mapmaker_pads.py` re-derives the counts, checks every new
value, and (where the listing exists) re-runs the scan and requires the
table to cover it exactly. The generated C was checked per function: the
number of instructions using each constant matches the table in all 25
functions (`lea esi,[eax-0x209e]` is lifted as `eax + -8350`).

### The one site kept: `0x002B0363` in `sub_002B0320`

`mov eax,[0x715f38]; movsx eax, word [eax-0x413d8]` overwrites the handle
before the read, so it always reads a fixed place 0x413D8 bytes before the
runtime pool (CONFIRMED). Rewriting the displacement would not make it read
`runtime[h].owner`; it would only move the stray read further. Keeping it
preserves the stock behaviour relative to the pool. INFERRED: with the
scene-100 pools allocated in order (regular, links, AI data, cover, runtime)
the address now falls inside the cover pool, so the read stays in mapped heap
memory. It is reached only for bot AI in mode byte 1 whose `+0x684` holds a
runtime handle. A proper fix is a reimplementation of `sub_002B0320`.

### Story levels and saves

- Ids are not touched, so saved maps and story files decode the same.
- Story levels have at most 350 cover pads (pool 0x2BC0 at `0x0009A1C3`),
  whose handles 0x1F40..0x209D decode as cover before and after.
- Runtime handles are created only by `sub_0009D3B0` (`0x0009D488`) and
  `sub_0009A510` (`0x0009A53C`) and are never written to files; there is no
  other instruction or data word holding a runtime handle (scan above). So the
  rewrites are safe in every scene without a gate (INFERRED from that
  exhaustive constant scan, not from play).
- Every handle stays below 0x7FFF (cover 0x1F40..0x473F, runtime
  0x4740..0x4753), so even an unseen int16 store of a handle would survive.

### The stock overlap this removes

In scene 100 the stock cover range was 0x1F40..0x243F (1280) but the runtime
range stayed 0x209E..0x20B1 (CONFIRMED, `sub_0009A420` `0x0009A43D..0x0009A46E`):

- `sub_0009A420` decoded the 20 runtime handles as cover pads 350..369;
- `sub_0009A4C0` (40 callers) and the ~25 inlined decoders returned -2
  (runtime) for cover pads 350..369 and read `regular_pool + h*32` (past the
  2400-entry pool) for cover pads 370 and up.

So stock maps with more than 350 cover pads (about 40 tiles) already had
misdecoded cover pads for bots (INFERRED effect). With Layout B the ranges
are disjoint and every decoder agrees.

### Cover capacity and the editor limit

- Cover pads come from tiles (`sub_0009A7A0`, `add eax,0x2329` at
  `0x0009AC14`) and from `sub_0023F9D0`, which tops a start-point set up to
  16 pads (CONFIRMED: `mov eax,0x10; sub eax,edx` at `0x0023F9D7`, nothing
  added when n <= 0 or n >= 16). Its only caller, `sub_0023FB90` (once per
  map from `sub_00240C50`), calls it 4 times, once, or 2 x N times by mode
  (`0x0023FB9B`, `0x0023FBA7`, `0x0023FC5E`); the matching allocation is 16,
  64 or 32·N extra pads (`0x002408A3..0x002408C3`). Measured +60 = 4 x 15.
- Editor limit 9,984 keeps the stock 256 spare (4 x the 64-pad case);
  `pc_mapmaker_pads_cover_topup` refuses a top-up that would pass 10,240,
  so even mode 3 with many teams cannot misdecode.
- The tile appender itself has no bound; the editor limit covers it (placed
  + held pieces, the same accounting as stock).

### What tiles can reach

Tile types 0..45 (rows at `0x00529400`, read from the XBE): at most 14 cover
pads, 14 regular pads and 46 links per tile; 142- and 110-tile test maps
averaged 9 cover pads per tile.

| Pieces | Cover needed (max / typical) | Fits 10,240? |
| --- | --- | --- |
| 200 (room limit now) | 2,800 + 64 / ~1,800 | yes |
| 242 (byte room ids) | 3,388 + 64 / ~2,180 | yes |
| ~700 / ~1,100 | 9,984 | the cover limit binds here |

So at 242 pieces the cover target needed is about 3,452 (worst case); the
10,240 range is not the binding limit until rooms allow ~700 heavy tiles.

## Regular pads, ids and links

- The 13 id-threshold compares (`cmp reg,0x1f40; jle; call sub_0009CCA0;
  movsx [pad+8]`: an id above the threshold is a cover id and is replaced by
  its owner) are in `sub_0023F300` (0x0023F300), `sub_0023FD10` (0x0023FDCF),
  `sub_00240C50` (0x002411F6, 0x00241300, 0x002413D2, 0x002416BA, 0x00241853,
  0x00241F64, 0x00242035, 0x00242094, 0x00242107), `sub_00339410`
  (0x00339448) and `sub_00339560` (0x00339597) (CONFIRMED, each followed by
  `jle` and the call). With 0x1F40 they misread regular pads 7000..7999 (ids
  8001..9000) as cover pads, taking `+8` (the group id) as an owner pad id.
  2026-11-02: `pads-idthreshold-*` rewrites them to 0x2328, the last regular
  id. Every other cover-id test already uses `>= 0x2329` (`sub_0009CC40`,
  `sub_0009CCA0`, `sub_0009A330`, `sub_0009CE00`, `sub_0023F9D0`,
  `sub_00332330`, `sub_0006C1E0`), so the 13 now agree with them. Ids up to
  8000 decode as before, so story levels (at most 2400 regular, 350 cover) and
  saved maps are unaffected; no scene gate. The earlier decoder classification
  called four handle compares (`0x0009D5FE`, `0x0009D622`, `0x0015D51A`,
  `0x0015E4B5`, all `jl` on handles) id thresholds and two id thresholds
  (`0x00339448`, `0x00339597`) handle compares; corrected.
- Regular pads: the scene-100 pool and the per-pad AI data hold 8000 entries
  (the whole regular handle range, 256 KB + 352 KB), the editor allows 7,680
  (320 spare). 511 pieces of the heaviest tile need 7,154. Regular ids reach
  0x3E9 + 7999 = 9000 < 0x2329, cover ids still start at 9001. Not measured:
  the per-pad AI data writer `sub_0027C4A0` does route searches per regular
  pad at load, so load time grows with the pad count.
- Cover ids reach 0x2329 + 10,239 = 19,240 < 32,767 (int16 owner field at
  pad+8 and the `[0x78CCE4]` records) and < 65,535 (uint16 save words).
- Links: the editor's 12,000 counts template links only (8800 until
  2026-11-01: 241 pieces of the heaviest tiles, 46 links each, need 11,086,
  so 8800 refused tiles before the piece limit). The stitcher
  `sub_0009DC70` appends one link per stitch entry with no bound (`inc
  [0x7B6660]` at `0x0009DCEE`), from a table of 16 entries per piece
  (`sub_0009A0F0`, 6-byte entries; INFERRED that the size argument is the
  piece count), and `sub_0009CD00` adds teleporter/door links
  (`sub_0009A5B0`). The pool was 12,000 + 16 x 255 + 256 = 16,336 (255 pieces, 2026-11-01).
  Link indices stay below the int16 limit (32,767) of the A* nodes and
  routes; the fixture checks the pool against it.
- 2026-11-02: the pool is fixed at 32,512 entries (650 KB, the most below
  the int16 limit with a margin) and the editor counts what play adds: it
  refuses placement when template links + 16 x pieces (placed + held) pass
  32,256, keeping 256 for teleporter and door links. 511 pieces of the
  heaviest tile need 23,506 + 8,176 = 31,682 (links refuse the 521st);
  1024 pieces fit with up to 15 template links each. The meter's link share
  divides the same sum by 32,256. The teleporter pairing
  (`pc_mapmaker_objects_teleporters`) stops adding links at the pool size
  (it compared with the editor's 12,000 before, so past 12,000 links a
  map's teleporters would have had no nav links). Widening the A* indices to
  32 bits was not attempted: nodes (`+0`..`+6` int16 in 16-byte nodes) and
  route buffers (int16 arrays of 100/300 in a 0x338-byte struct) are used by
  13 functions plus every route reader.
- 2026-11-07, stitches counted (CONFIRMED code and XBE data, not played).
  The writer's neighbour pass (`sub_002FC680`, 12 calls of `sub_00333BA0`)
  stores one pad pair per slot of each side connection point that meets a
  neighbour (catalogue of tile N at `row(N) - 0x30`: count `+0x44` =
  `row + 0x14`, 6-byte entries `+0x48` with byte 0 type 1..4 = side, byte 4
  = slots; `sub_00311C00` reads `0x00529554 + N' * 0x140`). The loader's
  table builder (`sub_0009A7A0`, `0x0009AEA0..0x0009AED1`) skips a pair whose
  reverse is listed, so a doorway is one link. Stock side slots per tile: 2..24
  (tile 17, 3 x 3: 12; tiles 14/42: 24); half of any tile is at most 12, inside
  the stock table of 16 per piece. The editor now costs template links +
  ceil(side slots of placed + held / 2): tile 17 35 (was 45), tiles 14/42 58
  (was 62; the flat 16 under-counted them). Pieces meeting with different
  widths can list unmatched pairs; the 256 slack and `pc_mapmaker_pads_stitch`
  (WRAP `sub_0009DC70`, scene 100 with the pools: cuts the table at the first
  entry that would pass `MAPMAKER_POOL_LINKS`, logs `[mapmaker] pads: link pool
  full`) keep that from writing past the pool. Per-tile link use (template +
  stitch, packed grid): tile 17 29 + 6, tile 14 46 + 12, 1x1 tiles 1 + 1..2.
  Link indices > 32,767 still need the int16 sites widened (partial, not
  exhaustive inventory: nodes
  `+6` read at `0x0009B175`, `0x0009B1E4`, `0x0009B458`, `0x0009B815`; route
  words read at `0x0009BBB6`, `0x0009BCCE`, `0x001598AD`, `0x00159901`,
  `0x00159A6D`, `sub_0006D7A0` `[route+0x10/0x12]`; 29 functions use the link
  pool); not needed until pads pass 8000.

## Pad groups and the A* pools

- Group-level A* (`sub_0009B540`/`sub_0009B630` -> `sub_0009B2F0`): nodes are
  keyed by link (`+6`); a link already open or closed is only replaced when
  cheaper, through the free list (`sub_0009B200`, `sub_0009B160`). So the
  live nodes per search are at most the number of inter-group links, and the
  counter `[0x00711FF8]` is reset per search (`0x0009B576`, `0x0009B666`)
  (CONFIRMED code, INFERRED bound). With 600 groups and up to 16,336 links
  that can pass 1000, which overwrote `0x00711FF8`, the list heads at
  `0x00712040..0x0071204C` and the pad-level pool.
- Relocation: 0x0070E178 (0x3E80 B) -> 0x008E0000 (0x20000 B, 8192 nodes,
  int16 indices still valid). 27 offset sites and 4 `keep` sites (the counter
  `0x00711FF8` right after the table) in `sub_0009B090`..`sub_0009B630`;
  `data_ok` 0x004A26B8 is a uint16 ramp table in .rdata.
- `sub_000984D0` compares a loop pointer with 0x70E240, the exit value of a
  10-iteration loop over a 60 x 0xE8 table ending at 0x70E158 (CONFIRMED,
  `0x0009853B..0x00098545`). It is not a pool reference, but lies in the
  relocated range, so `pads-astar-loop-bound` rewrites it to 0x70E170: the
  pointer steps 0x570 from 0x70ABE0, the last value tested in the loop is
  0x70DD30, so any bound in (0x70DD30, 0x70E240] runs the same iterations.
- `pc_mapmaker_pads_group_node` refuses a node only when the free list
  `[0x0070E170]` is empty and the counter has reached 8192; the search then
  skips that link (a longer or failed route instead of corruption).
- The pad-level pool (`sub_0009B900`) searches within one group, whose pads
  come from one tile (group ids are offset per tile, `0x0009A8D9`), so its
  nodes are bounded by one tile's links (46 at most) (INFERRED). It stays at 1000 in place, with
  the same guard (`[0x00712044]`, `[0x00715F20]`).
- Group limit 600 -> 1024 (2026-11-02): 511 pieces at the measured 1.5
  groups per piece need about 770. Memory-safe with the guards: group ids are
  int16, the group table `[0x7B6770]` is sized by count, the group A* pool is
  bounded. The N x N distance matrix (`sub_00275F70`, (N+1) x N x 4 B from
  heap 1 at `0x00275F90`, zeroed by the caller) is 4.2 MB at 1024 groups
  (1.44 MB at 600); see `docs/mapmaker/mapmaker-memory.md` for where it now
  lives. Not measured: its load time (N x N route searches, 1.05 M at 1024
  vs 359,400 at 600 and 89,700 stock). Past 1024 the matrix needs a lazy
  replacement (wrap `sub_00275F70` and its reader `sub_00275F40`).

## Open risks

- Nothing played yet. The constant scan is exhaustive for the encodings
  listed; a runtime handle computed some other way (for example from a
  table) would not be caught (none found).
- `0x002B0363` still reads a stray, handle-independent word (stock bug).
- The stitch table holds 16 entries per piece (`sub_0009A0F0(pieces * 16)`,
  one table for the map, unbounded writer); stock tiles leave at most 12 per
  piece on average after the reverse-pair drop when every doorway matches. Pieces meeting with different widths are
  INFERRED to list a few more; not measured in play.
- Shared docs not updated here (owned elsewhere): the header comment of
  `src/interface/pc_mapmaker_limits.inc` (still says cover is capped at 1280/1024),
  `docs/agent-context/systems.md` (cover 1024 / pool 1536) and
  `research/capacity-constraints.md`.

## Test in play

1. A map with more than 350 cover pads (40+ tiles) and bots: bots should take
   cover and move normally (the stock overlap is gone).
2. With `TSFP_MAPMAKER_LIMITS=1`, place past 1024 cover pads (the meter's
   cover share now divides by 9,984), save, reload, play with bots in each
   mode; probe the cover count in play (expect editor count + about 60) and
   the three refusal counters (expect 0).
3. A story level with bots and spawned runtime pads, to confirm it behaves as
   before.
4. Load time of a 600-group map.

## Validation

- `tests/mapmaker/test_mapmaker_pads.py`: fixture (12 C mutations), table invariants
  (6 table mutations), completeness against the listing when available.
- `tests/mapmaker/test_mapmaker_limits.py`: updated expectations (cover 9,984, pool
  10,496, link pool 16,112; 241 of the heaviest tile allowed).
- `logs/limits/check-tables.py` over this folder: "table verified".

## Decoder classification (2026-10-30)

`tests/mapmaker/test_mapmaker_pads_classified.py` decodes all game code with the lifter's decoder (the listing scan above misses elided long instructions) and requires every 0x209D/0x209E/0x20B1/0x20B2/0x243F/0x2440/0x1F40 constant (any sign) and every negative displacement within 0x40 of a handle base x 2..64 to be in pads.json or reviewed: 88 rewritten (all of them in the table), `0x002B0363` kept, 59 cover-base/id-threshold 0x1F40 sites, 7 unrelated 0x1F40 fields. No missed site.

## Layout C: 16,384 regular pads, ids past 8000 (2026-11-07)

Static analysis, decoder and emulator checks only; not built or played.

Handles: regular = index 0..0x3FFF, cover = 0x4000 + index (14,336),
runtime = 0x7800 + slot. `pads.json` re-values the 88 runtime sites
(`pads-layoutc-*`: 0x209E/0x4740 -> 0x7800, 0x20B2 -> 0x7814, -base,
folded `-0x413BC` -> `-0xEFFFC`) and rewrites the 45 cover-base sites
(`pads-coverbase-*`: 27 `cmp`/`add` 0x1F40, 16 `-0x1F40`, the two folded
`cover[h].owner` reads `-0x3E7F8` -> `-0x7FFF8`). `0x003BC6EC cmp
eax,0x1F40` is a network port compare (`sub_003BC6D0`), not a handle: it
was mislabelled as cover base and stays. All handles stay below 0x7FFF.

Ids: regular index i < 8000 keeps 0x3E9 + i; cover j keeps 0x2329 + j;
regular i >= 8000 takes 0x5B29 + (i - 8000) (the "high" ids, at most
0x7BE8, above the 14,336 cover ids). One-sided tests ("id >= 0x2329 is
cover", the 13 "id > 0x2328 take the owner") meet high ids, so each place
that reads ids is covered (inventory: `test_mapmaker_padids_classified.py`,
19 `0x2329` sites, 26 `0x3E9` sites):

- `sub_0009A3C0` (cover id -> handle; reached by `sub_0009CC40`,
  `sub_0006C1E0`, `sub_0009CCA0`, `sub_0009D3B0`, story `sub_0009CE00`): a
  high id answers its regular handle with `sub_0009CC40`'s search,
  step for step (`mapmaker_pads_search`).
- `sub_0009CCA0` (id -> pad): high ids give the regular pad; to the 13
  threshold callers (return addresses `0x0023F30D` ... `0x003395A4`, each
  followed by `movsx reg,[eax+8]`) a scratch pad below the guest stack
  whose owner word is the id itself; to `sub_0033B100` (`0x0033B179`, which
  subtracts 0x3E9) a scratch owner 0x3E9 + index when a cover pad's owner is
  high.
- `sub_0009D3B0` (runtime pad): fixes `+0x18` for a high owner after the
  call. `sub_0009A330`: `pads-regid-0009a37e` (`cmp ecx,0x2329` -> 0x7BE9).
- `sub_0009A7A0` (tile loader) numbers 0x3E9 + index and owners likewise
  and reads owners back by `- 0x3E9` inside the call; the hook renumbers
  pads 8000.. and this tile's cover owners after it (low word of `+8`).
- Stitcher (`sub_0009DC70`, id - 0x3E9): high ids become 0x3E9 + index
  first (fits the u16). Start-point top-up (`0x0023FA20` filters ids
  >= 0x2329): high seed ids are replaced by aliases 0x80000000 + k for the
  call (resolved by the pointer hook) and restored.
- Host code: the teleporter pairing (`pc_mapmaker_objects.inc`) read pad
  ids `> 0x1F40` as cover (wrong for regular pads 7000..7999 too); now the
  cover range 0x2329..0x5B28. `pc_mapmaker_bugfix.inc` runtime base 0x7800.
- Editor: `sub_00332490` returns the high id; `sub_002FA150` (group count)
  and `sub_00334B20` (item pads as base + local) run on 0x3E9 + index in a
  scope, and `sub_00334B20`'s list is mapped afterwards outside it.

Map files: a map with at most 8000 regular pads is written byte for byte as
before (no id changes), so it loads in every build and, within its size,
in the stock game. Maps with more get high ids: older Limitless builds
(.66 and before, pool 8000) would overflow their regular pool loading them
(do not open them there), and the stock game cannot open maps of that size
(500+ tiles, well past its 32 KB file). No version flag is written: the
container version 0x12D is checked strictly by callers that crash on a
refusal (`logs/limits/container.md`). Saved Story AI points are cover ids
and decode as before.

Memory: pools 2.36 MB in the nav block (was 1.59 MB): regular 512 KB, AI
data 704 KB, cover 467 KB, links 650 KB. The pit skeleton's rim pads
(`pc_mapmaker_pitboss.inc`) use the pool's tail (16,384 - 13 x (n + 1)).

## Group distances above 1024 groups (2026-11-07)

`sub_00275F70` (modes 2/3 only, from `sub_00158BA0`) builds an
(N+1) x N x 4 B matrix with N x N route searches (`sub_001596F0`, group A*
plus a pad A* step): 4.2 MB and about 1 M searches at 1024 groups, 16.8 MB
and 4.2 M at 2047. Above 1024 groups (scene 100, pools on)
`pc_mapmaker_pads_group_matrix` keeps the link flags, the mask
`[0x004D9788]` and the door flag `[0x00715F50]` as they are at load and
leaves `[0x0078B2D8]` at 0; `pc_mapmaker_pads_group_distance` (reader
`sub_00275F40`, 5 callers in bot AI) computes an entry on its first read:
the same `sub_0009C550` first pads, route struct, zeroed `[0x00715F44]`/
`[0x00715F48]`, `sub_001596F0` and `B + A` (or `[0x00475D5C]` for no route),
with the load-time flags put back for the search and the globals it writes
restored; cached per row in host memory, pushed on the x87 stack. The
character argument is 0 (it selects only which AI struct the search writes
to: `+0x684` for cover destinations, never a group's first pad, and
`+0x28 |= 0x200` when the route starts at a pad listed in `[0x007B65E0]`,
whose count has no writer in the decode). Difference from stock: that flag
is no longer set on `[[0x007B0C48]+0x14]`'s AI at load. Up to 1024 groups,
`TSFP_MAPMAKER_GROUP_MATRIX=dense`, other scenes or pools off: the original
(`=lazy` forces the lazy path). stderr: `[mapmaker] pads: N pad groups:
distances computed on first use ...`, then every 4096 entries a summary.

The group A* node pool stays 8192 (`pc_mapmaker_astar.inc`): searches that
explore more than 8192 links (unreachable targets on the biggest maps) are
cut short and report no route.

Per-bot pad queries in host code (after .73, identical results):
`sub_0009BCF0` (nearest enabled pad at least min away, -1; 37 callers) walks
every pad with the original's two squared-distance orders (first of each
group of four z^2+x^2+y^2, others x^2+y^2+z^2) and strict "nearer" test;
`sub_0006A8B0` (random pad in a ring, the wander goal of bots/AIs:
count/2 tries) draws with the game generator stepped exactly as
`sub_00153B40` (state, counter, draw left in the argument slot), index
floor(r x count / 2^32), SSE distance and the same ring/flag tests.
`TSFP_MAPMAKER_NEAR_PAD=0` / `TSFP_MAPMAKER_RANDOM_PAD=0` run the originals.
stderr: `[mapmaker] nearest pad: n calls, m pads tested` and `[mapmaker]
random pad: n calls, m tries` (first call, then every 65536th). Tests:
`test_mapmaker_nearpad.py` / `test_mapmaker_randpad.py` (fixture series and
mutations; the original XBE code in `guest_emulator.py` must give the same
pad, and for the picker the same draws and generator state).

Tests: `tests/mapmaker/mapmaker_pads.c` (ids, search, hooks, tiles, editor
scopes, alias, stitch, lazy matrix; their mutations in
`test_mapmaker_pads.py`), `test_mapmaker_padids.py` (emulator: the guest
search vs C on 10 pool sizes; stock vs Layout C decoders on a story-sized
map in scenes 53/100; Layout C on the largest pools; the original
`sub_00275F70` vs the lazy reader with a link changed after load; 4
mutations caught), `test_mapmaker_padids_classified.py`,
`test_mapmaker_pads_classified.py` (every handle constant: 88 + 45 + 13).

Test in play: a full grid of tile 12 or 26 (1023 x 9 = 9,207 pads) with
bots in each mode, and a mode 2/3 match on a map of more than 1024 groups
(stderr line above): bots should move between tiles past index 8000
(probe the pad count), shoot, take cover; save, reload, play again.
