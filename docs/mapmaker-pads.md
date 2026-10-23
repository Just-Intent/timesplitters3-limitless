# MapMaker AI navigation pads: cover range, A* pools, link pool

Implementation notes for the "pads" area of the MapMaker limit raise
(2026-10-23). Static analysis of the US XBE (title 45410066) disassembly and
the generated C; nothing here has been tested in play yet. CONFIRMED = seen
in the disassembly at the given address; INFERRED = reasoning, not observed.

## What changed

| Item | Stock | Now | Where |
| --- | --- | --- | --- |
| Cover handle range | 350 (1280 in scene 100, overlapping runtime handles) | 10,240 in every scene | `config/guest-code-patches/pads.json` (88 rewrites) |
| Runtime pad handles | 0x209E..0x20B1 | 0x4740..0x4753 | same |
| Scene-100 cover pool | 1280 x 32 B | 10,496 x 32 B (range + 256 guard) | `MAPMAKER_POOL_COVER`, `pc_display_mapmaker_pools` |
| Editor cover limit (opt-in) | 1024 | 9,984 (range - 256, the stock spare) | `MAPMAKER_LIMIT_COVER` |
| Link pool (scene 100) | 4400 x 20 B (editor 4400) | 12,256 x 20 B (editor 8800 + 16 per piece stitch + 256) | `pc_display_mapmaker_pools` |
| Group A* node pool | 1000 nodes .bss 0x0070E178, no bound | 8192 nodes at 0x008E0000, bounded | `pads.json` relocation + `pc_mapmaker_pads_group_node` |
| Pad A* node pool | 1000 nodes .bss 0x00712058, no bound | 1000 nodes, bounded | `pc_mapmaker_pads_pad_node` |
| Start-point cover top-up | unbounded | refused past the cover capacity (scene 100) | `pc_mapmaker_pads_cover_topup` |

Unchanged on purpose: pad ids (regular 0x3E9 + i, cover 0x2329 + i,
runtime -2), so saved maps and story files read the same; the cover handle
base 0x1F40; the regular range; the 13 pad-id threshold compares; regular
pads 4800 and links 8800 in the editor; pad groups 600.

## Wrappers for the integrator

    WRAP sub_0009B090 -> pc_mapmaker_pads_group_node
    WRAP sub_0009B730 -> pc_mapmaker_pads_pad_node
    WRAP sub_0023F9D0 -> pc_mapmaker_pads_cover_topup

`src/pc_mapmaker_pads.inc` must be included after `pc_mapmaker_limits.inc`
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

`tests/test_mapmaker_pads.py` re-derives the counts, checks every new
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

- The 13 id-threshold compares (`cmp reg,0x1f40; jle`, pads.md §1.3) treat
  an id above 8000 as a cover id. Regular ids reach 0x3E9 + 4799 = 5800 with
  4800 pads, cover ids start at 9001, so they stay correct; they would only
  misread regular pads past 7000. Unchanged.
- Cover ids reach 0x2329 + 10,239 = 19,240 < 32,767 (int16 owner field at
  pad+8 and the `[0x78CCE4]` records) and < 65,535 (uint16 save words).
- Links: the editor's 8800 counts template links only. The stitcher
  `sub_0009DC70` appends one link per stitch entry with no bound (`inc
  [0x7B6660]` at `0x0009DCEE`), from a table of 16 entries per piece
  (`sub_0009A0F0`, 6-byte entries; INFERRED that the size argument is the
  piece count), and `sub_0009CD00` adds teleporter/door links
  (`sub_0009A5B0`). The pool is now 8800 + 16 x 200 + 256 = 12,256.
  Link indices stay far below the int16 limit of the A* nodes and routes.

## Pad groups and the A* pools

- Group-level A* (`sub_0009B540`/`sub_0009B630` -> `sub_0009B2F0`): nodes are
  keyed by link (`+6`); a link already open or closed is only replaced when
  cheaper, through the free list (`sub_0009B200`, `sub_0009B160`). So the
  live nodes per search are at most the number of inter-group links, and the
  counter `[0x00711FF8]` is reset per search (`0x0009B576`, `0x0009B666`)
  (CONFIRMED code, INFERRED bound). With 600 groups and up to 12,256 links
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
- Group limit 600 is kept: memory-safe with the guards. Not measured: load
  time of the N x N distance matrix (`sub_00275F70`, 359,400 route searches
  at 600 groups vs 89,700 stock; 1.44 MB). Above ~1000 groups that matrix
  needs a lazy replacement.

## Open risks

- Nothing played yet. The constant scan is exhaustive for the encodings
  listed; a runtime handle computed some other way (for example from a
  table) would not be caught (none found).
- `0x002B0363` still reads a stray, handle-independent word (stock bug).
- The link slack for stitches assumes the stitch table holds 16 per piece.
- Shared docs not updated here (owned elsewhere): the header comment of
  `src/pc_mapmaker_limits.inc` (still says cover is capped at 1280/1024),
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

- `tests/test_mapmaker_pads.py`: fixture (12 C mutations), table invariants
  (6 table mutations), completeness against the listing when available.
- `tests/test_mapmaker_limits.py`: updated expectations (cover 9,984, pool
  10,496, link pool 12,256).
- `logs/limits/check-tables.py` over this folder: "table verified".
