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
