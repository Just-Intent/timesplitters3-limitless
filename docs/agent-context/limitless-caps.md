# MapMaker capacity caps other than rooms (Limitless fork)

Extends [limitless.md](limitless.md) for the pads, limits, objects and memory
areas: what a map of about 511 pieces of the heaviest tile needs besides the
room count ([rooms](limitless-rooms.md) owns `MAPMAKER_LIMIT_PIECES`,
`MAPMAKER_ROOMS_MAX`, the view stride and the special ids). Static analysis
and fixtures only; nothing here was built or played by the agent that wrote
it. Heaviest stock tile (rows 0..45 of `0x00529400`): 14 pads, 14 cover pads,
46 links, 42,427 B memory estimate.

## Regular pads (2026-11-02)

- `config/guest-code-patches/pads.json` `pads-idthreshold-*`: the 13
  `cmp id,0x1F40; jle; call sub_0009CCA0` compares now use 0x2328, so pad ids
  up to 9000 are regular (cover ids start at 0x2329 everywhere else).
- `pc_mapmaker_limits.inc`: `MAPMAKER_POOL_PADS` 8000 (regular pool and
  per-pad AI data in scene 100), `MAPMAKER_LIMIT_PADS` 4800 -> 7,680.
  Compile checks in `pc_mapmaker_limits.inc` and `pc_mapmaker_pads.inc`
  (`MAPMAKER_PADS_ID_THRESHOLD`).
- Tests: `test_mapmaker_pads.py` (table: 13 sites, values; listing pattern;
  2 C + 3 table mutations), `test_mapmaker_pads_classified.py` (88 runtime +
  13 threshold rewrites; four mislabelled sites corrected).
- Contract and evidence: `docs/mapmaker/mapmaker-pads.md`.

## Links (2026-11-02)

- `pc_mapmaker_limits.inc`: `MAPMAKER_POOL_LINKS` 32,512 (scene-100 pool,
  fixed; int16 A* link indices), `MAPMAKER_LINK_STITCH` 16 per piece,
  `MAPMAKER_LINK_SLACK` 256, `MAPMAKER_LIMIT_LINKS` = pool - slack.
  `mapmaker_link_cost` (template + 16 x pieces) feeds the meter, the held
  preview and `mapmaker_limits_over` (bit mask of the limits passed; the
  placement decision is `!= 0`).
- `pc_mapmaker_objects.inc` teleporter links stop at the pool, not the
  editor limit.
- Tests: `mapmaker_limits.c` (exact 32,256 boundary with held pieces; 511
  heavy-link pieces pass every cap, the 512th only the piece cap; links stop
  at 521, pads at 549, 1024 x 15 links fit), `mapmaker_objects.c` (links past the editor limit);
  mutations in `test_mapmaker_limits.py` / `test_mapmaker_objects.py`.

## Pad groups (2026-11-02)

- `MAPMAKER_LIMIT_GROUPS` 600 -> 1024 (editor global groups `[0x00762CB8]`).
  Matrix 4.2 MB at 1024; load time of N x N route searches unmeasured.
- Tests: `mapmaker_limits.c` (1024 allowed, 1025 refused, 767 for 511
  pieces), mutations `groups-*`.

## Engine objects (2026-11-02)

- `pc_mapmaker_objects.inc`: `MAPMAKER_OBJECTS_POOL` and the slot-8/slot-0
  list caps 1000 -> 1023 (handle serial 1..0x3FFFFF, so slots 0..1022 never
  form -1); `objects.json` list sizes 0xFFC, slot-0 clear 1023 dwords.
- `pc_mapmaker_limits.inc`: `MAPMAKER_LIMIT_OBJECTS` 1023,
  `MAPMAKER_OBJECTS_RESERVE` 64; placed + held pieces + items + 64 must fit
  (`MAPMAKER_CAP_OBJECTS`, meter share). Compile check ties it to the pool.
- Tests: `mapmaker_limits.c` (511 pieces + 448 items fit, 449 refused, held
  pieces count, 255 pieces keep 500 items), `mapmaker_objects.c` (1023
  pools and lists), mutations `objects-*`, `pool-round3`; `check-tables.py`.

## Placed pickups (2026-11-02, v0.4.0-mapmaker.36 playtest)

Symptom: 459 pieces + about 500 items; no placed health or armour, some ammo,
turrets and barrels loaded. Cause (CONFIRMED in code, not measured in play):
every placed pickup (`sub_000C2D40` from the loader loop 0x0006D983, then
`sub_000C1950` with arg0 bit 0 clear: weapons, ammo, health, armour,
power-ups, keycards, flags, special pickups) takes a 0xD8-byte record from
`[0x00732E60]` (count `[0x00732E64]`), 120 records (`sub_000BEEE0` push
0x6540, clear 0x1950 dwords in `sub_000BEE60`). At 120, 0x000C1DC7
`cmp eax,0x78; jge` frees the new object and returns 0, so later pickups in
container order vanish silently (and the stock caller then writes through
the null result into guest 0x28..0x60). Dropped weapons use the separate
30-entry table at 0x007B0D20 (arg0 bit 0). Turrets and props are not pickups.

- `objects.json` `obj-pickups-alloc` / `-clear` / `-cap`: 768 records in
  every scene (+140 KB Level heap, also story; static so no wrapper and no
  scene gating); the test becomes `cmp ah,3` (count >= 768 = 3 << 8), so the
  size must stay a multiple of 256. `MAPMAKER_OBJECTS_PICKUPS` in
  `pc_mapmaker_objects.inc` (compile check: above `MAPMAKER_LIMIT_SUB` 500).
- Every reader loops to the count or uses an index below it (34 sites
  reviewed); its collect buffers are sized by a
  counting call (`sub_00232270`). Known latent stock bug, not changed: `sub_0033B290` clamps
  the count to 10 for its buffer but `sub_003384F0` then writes every match
  (more than 10 matching pickups of one kind overflow it in that AI command).
- AI awareness lists stay stock and drop silently past them: weapons 55
  (`sub_00160820`), health + armour 25 (`sub_00160940`), power-ups 20,
  flags 20; bots ignore pickups past those.
- Ruled out (decode, `test_mapmaker_pickups_classified.py`): handles of slots
  512..1022 are negative as int32, but none of the 167 handle decodes
  (`shr r,0x16` + `imul r,r,0x2E8`, 8 other shr sites reviewed) is guarded by
  a signed test of the handle (null is 0, tested with `je`/`jbe`), and the 14
  signed tests of a loaded dword +0x2C are other structs. So no slot-order
  change (option a) or sign rewrites (option b) are needed.
- Not explained: barrels that do not explode. Ruled out: handle signs, the
  object lists (no `object list full` line in m36.err), the area-effect
  global room id (rooms.json 0x000BBA05/0x000BBA33 = 0x20C). Open: pool
  headroom (`sub_000AB590` refuses flag-8 objects when fewer than 5 slots are
  free; `sub_0009EE00` itself has no bound) and room-bucket state.
- Tests: `test_mapmaker_pickups_classified.py` (table sites, `cmp ah,3`
  semantics, handle-sign scan, 2 mutations), `test_mapmaker_objects.py`
  (table policy, `pickups-*` mutations).
- Play check: a map with more than 120 pickups (all health and armour placed
  last) shows them all; probe flag 64 for `[0x007B4FAC]` minus
  `[0x007B65B8]` (live objects) on a full map; shoot a barrel at once after
  load and again with bots active.

## Play memory and the memory estimate (2026-11-02)

- `pc_mapmaker_memory.inc` nav block: 6 MB (`MAPMAKER_NAV_BYTES`) from the
  runtime heap at the first MapMaker map, for CPU-only AI data only: the
  pools (through `mapmaker_pool_source` in `pc_mapmaker_limits.inc`) and the
  group matrix (`sub_0003DFA0` from return 0x00275F95, heap 1, scene 100).
  New wrapper `WRAP sub_0003E650 -> pc_mapmaker_memory_level_reset` empties
  it with every Level heap reset; without it nothing uses the block.
  `TSFP_MAPMAKER_NAV=0` turns it off. Never for device-visible data (window).
- Map-start stderr line (Level heap, arena, Eternal, nav block, host runtime
  heap and window headroom) and a matrix line with the Level free count.
- `pc_mapmaker_budget.inc`: explicit scale 2..16; with the limits the
  default is 8 + arena MB / 2 (16 at `TSFP_MAPMAKER_ARENA_MB=16`).
  Arena stays off by default (window headroom unmeasured).
- Container: unchanged (a 511-piece map stays below about 200 KB of 512 KB).
- Tests: `mapmaker_memory.c` (redirect only for the matrix in scene 100 after
  a reset, zeroed, Level free untouched, fallbacks, reset, off switch,
  failed reservation, aligned path; 11 `nav-*` mutations),
  `mapmaker_limits.c` (pool source and fallback), `mapmaker_budget.c`.
- Contract: `docs/mapmaker/mapmaker-memory.md` "Round 4".

## What bounds 511 pieces now (cap 511, 2026-11-02)

`MAPMAKER_LIMIT_PIECES` is 511 ([rooms](limitless-rooms.md) stage 11; the
level fill pass frame `level.json` holds 512). Pads stop at 548, links at
520, cover at 713, groups at about 680 (1.5 per piece), objects at
1023 - 64 - items. Memory estimate per tile (XBE rows 0..45): light tiles
(19, <= 8.4 KB) and medium (15, 10-24 KB) stop at the piece cap (1,960 / 676
would fit scale 8); heavy (12, 32-42 KB) stop on the memory estimate at 386
(type 21/33, 42,427 B) to 513 (31,882 B), so 511 of the heaviest need scale
11 = `TSFP_MAPMAKER_ARENA_MB=6`. A map of all types mixed (mean 17.6 KB)
estimates 9.0 MB of 16.4 MB at 511. Past that: the Level heap itself
(unmeasured: probe flag 32 and the map-start line). Defaults unchanged
(scale 8, arena off): no 511-piece or larger-arena play measurement.

Refusal reason: `mapmaker_limits_log` prints `[MAPMAKER] placement refused:`
with every cap passed (`pads|cover pads|links|pad groups|pieces|items|memory
estimate|engine objects n of m`) once each time that set changes. Tests:
`mapmaker_limits.c` (387 heaviest refused on memory alone, 386 fit, 511 at
scale 11, not 10), `reasons_ok` and `reason-*` mutations in
`test_mapmaker_limits.py`.

## Resource table (area restable, 2026-11-03)

`src/interface/pc_mapmaker_restable.inc` (included last among the capacity
areas; `#define MAPMAKER_RESTABLE` before `pc_mapmaker_probe.inc`, whose
meter scope becomes `static mapmaker_probe_meter`) now defines
`pc_display_mapmaker_meter`, the existing manual wrapper of the meter painter
`sub_0030C210` (cdecl x, y floats; probe patch, no new wrapper or
regeneration). With `TSFP_MAPMAKER_LIMITS=1` it replaces the painter (pops
the return address) with 15 outlined rows `Title: used/max` at the meter's
x, y, font 0x12 at `\vs75`, 12 units apart, via `sub_00041E30` (alignment
`[0x007E7260]` zeroed and restored): the caps of `pc_display_mapmaker_limits`
from `mapmaker_limits_view` (placed counts; red on its `over` bits), pickups
of 768, Story AI `editor+0x280`/511, logic ops `editor+0x41E0`/120, and the
per-category counts of `sub_00303460`/`sub_00308420` (catalogue
`0x00524F58 + id*0xC4 + 2`, maxima `0x00524BE0 + c*24 + 4`) for movers,
props, turrets 27-29 and teleporters 14-18. Stock meter when the limits are
off, the view is not valid for this editor, `TSFP_MAPMAKER_TABLE=0`, probe
flag 1, or no text buffer. Whole-chain fixtures link `sub_0003FA10`/
`sub_00041E30` from `mapmaker_area_stubs.h`. Tests:
`tests/mapmaker/test_mapmaker_restable.py` (rows, colours, fallbacks, 4:3 and
wide layout, 39 mutations). Contract and playtest steps:
`docs/mapmaker/mapmaker-restable.md`. Not built or played.

`mapmaker_limits_view` (in `pc_mapmaker_limits.inc`): placed pieces, items +
free objects, objects, pads, cover, links, groups, placed memory, `over` and
the editor pointer of the last completed check; `valid` is cleared at the
start of each check. A cap changed here changes its row; the fixture reads
the editor/objects limits from their sources.
