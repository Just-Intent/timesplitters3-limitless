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
  heavy pass every cap but pieces, links stop at 521, pads at 549, 1024 x 15
  links fit), `mapmaker_objects.c` (links past the editor limit);
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
