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

## Links (2026-11-02; stitches counted 2026-11-07)

- `pc_mapmaker_limits.inc`: `MAPMAKER_POOL_LINKS` 32,512 (scene-100 pool,
  fixed; int16 A* link indices), `MAPMAKER_LINK_SLACK` 256,
  `MAPMAKER_LIMIT_LINKS` = pool - slack. `mapmaker_link_total(p, h)` =
  template links + ceil(side slots of placed + held / 2) (`MapMakerTotals.slots`
  from `mapmaker_row_slots`: catalogue count `row + 0x14`, entries `row + 0x18`,
  types 1..4, at most `MAPMAKER_LINK_SLOTS_MAX` 32 per piece) feeds
  `mapmaker_limits_over`; `mapmaker_link_cost` (one list) the meter, held
  preview and resource table. Was a flat 16 per piece (`MAPMAKER_LINK_STITCH`,
  removed). Why half: one stitch pair per facing slot, the loader drops the
  reversed pair (decode in [pads](../mapmaker/mapmaker-pads.md)). Tile 17 costs
  35 (was 45; user's .57 full grid refused at ~716), tiles 14/42 58 (was 62).
- `pc_mapmaker_pads.inc` `pc_mapmaker_pads_stitch` (WRAP `sub_0009DC70`, new
  2026-11-07, needs regeneration): scene 100 with the pools, cuts the stitch
  table at the first active entry past the pool; teleporter links
  (`pc_mapmaker_objects.inc`) stop at the pool, not the editor limit.
- Map files: unchanged (stitches are rebuilt by the writer on every compile).
- Tests: `mapmaker_limits.c` (exact 32,256 boundary with held pieces and
  slots; heavy 547 + 1 fit, pads refuse the 549th, links alone 556; tile 17
  852 + 1 fit, pads refuse, links alone 921; odd slots round up over placed +
  held; row reading), `mapmaker_pads.c` (stitch cut, inactive pads, scenes,
  pools off), `test_mapmaker_limits_classified.py` (decode of the reverse-pair
  drop, append, sole caller, catalogue offset; XBE slots per tile, max 24),
  mutations in `test_mapmaker_limits.py` / `test_mapmaker_pads.py`.

## Pad groups (2026-11-02)

- `MAPMAKER_LIMIT_GROUPS` 600 -> 1024 (editor global groups `[0x00762CB8]`).
  Matrix 4.2 MB at 1024; load time of N x N route searches unmeasured.
- Tests: `mapmaker_limits.c` (1024 allowed, 1025 refused, 767 for 511
  pieces), mutations `groups-*`.

## Engine objects (2026-11-02; 2047 since 2026-11-07, see "Engine object handles 2047")

- `pc_mapmaker_objects.inc`: `MAPMAKER_OBJECTS_POOL` and the slot-8/slot-0
  list caps 1000 -> 1023 (handle serial 1..0x3FFFFF, so slots 0..1022 never
  form -1); `objects.json` list sizes 0xFFC, slot-0 clear 1023 dwords.
- `pc_mapmaker_limits.inc`: `MAPMAKER_LIMIT_OBJECTS` 1023,
  `MAPMAKER_OBJECTS_RESERVE` 128 (64 characters + 64 effects, 2026-11-03);
  placed + held pieces + items + 128 must fit (`MAPMAKER_CAP_OBJECTS`, meter
  share). Compile checks tie it to the pool and the effects to 30 + 20 + 5.
- Tests: `mapmaker_limits.c` (511 pieces + 384 items fit, 385 refused, held
  pieces count, 459 + 500 refused, 395 pieces keep 500 items, 396 not), `mapmaker_objects.c` (1023
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
- Barrels that do not explode: pool headroom, see "Prop blocks" below.
  Ruled out: handle signs, the object lists (no `object list full` line in
  m36.err), the area-effect global room id (rooms.json 0x000BBA05/0x000BBA33
  = 0x20C).
- Tests: `test_mapmaker_pickups_classified.py` (table sites, `cmp ah,3`
  semantics, handle-sign scan, 2 mutations), `test_mapmaker_objects.py`
  (table policy, `pickups-*` mutations).
- Play check: a map with more than 120 pickups (all health and armour placed
  last) shows them all; probe flag 64 for `[0x007B4FAC]` minus
  `[0x007B65B8]` (live objects) on a full map; shoot a barrel at once after
  load and again with bots active.

## Prop blocks and the effects reserve (2026-11-03, v0.4.0-mapmaker.37)

Symptom: 459 pieces + about 500 items (engine objects at the 1023 cap):
shooting an exploding barrel did nothing; with 9 armour or 17 barrels
removed it crashed (`logs/m37c/crash-20261102-233938555-*`).

- Crash cause (CONFIRMED in code; the dump agrees, the index itself was not
  captured): class 0x80 (props, barrels; registered by `sub_000AE8B0`, init
  `sub_000AC940`, release `sub_000A92F0`) gives each prop a 0x160-byte block
  at object +0x7C. Template flag 8 props use a 30-block ring (`[0x0072F730]`,
  owners `0x0072F6B0`, size `[0x007313EC]` <= 30, oldest released). Others
  take entry `[0x007327A8]` of the pointer table `[0x0072F728]` (0 allocates
  a block and stores it there), then `inc [0x007327A8]`; only the level reset
  (0x000A9221) and the loader's cancel (`sub_000AB610` from 0x0006D1F9) lower
  it, releases never do. `sub_000A92A0` sized the table 60 (push 0xF0, rep
  stosd 0x3C). Prop 61 on read past it: a non-zero foreign dword became its
  block, a zero one made the init write a block pointer into the next
  allocation. The release walks the ten effect handles at block+0x100 into
  `sub_000FF200`; the dump has ebx 7 (fourth handle) and esi 0xFFFF0000,
  chain 0x000A77E8 -> `sub_000AC560` (prop broken, 0x000AC641) ->
  `sub_000A9070` -> `sub_0009E330` -> 0x000A9321.
- Fix: `objects.json` `obj-propblocks-alloc` / `-clear`: 2048 entries
  (8 KB Level heap, every scene, static so no wrapper);
  `MAPMAKER_OBJECTS_PROP_BLOCKS` in `pc_mapmaker_objects.inc` (compile check:
  two per pool slot). That runtime creations are flag 8 (ring) is INFERRED:
  stock 60 would otherwise overflow in stock levels.
- Did-nothing cause (CONFIRMED in code, INFERRED for this map): breaking a
  prop spawns debris through `sub_000AB590` (up to 20 in `0x00732688`),
  which refuses flag-8 objects unless 5 slots stay free (`sub_000A4C70`:
  live + 1 + 4 < `[0x0072F5E4]`). Pieces + items + 64 characters left none.
  `MAPMAKER_OBJECTS_EFFECTS` 64 joins the reserve; 511 pieces now allow 384
  items and 500 items 395 pieces.
- Tests: `test_mapmaker_propblocks_classified.py` (every table/ring/count
  site, count writers, ring <= 30, 2 mutations), `test_mapmaker_objects.py`
  (policy, `propblocks-*`), `test_mapmaker_limits.py` (`objects-*`);
  `check-tables.py`.
- Play check: the m37 map refuses placement at the pool (log line
  `engine objects n of 1023 (pieces + items + 128 reserved)`); a map past 60
  barrels/props (about 395 pieces + 500 items): shoot several barrels at once,
  restart the level, shoot again; story level 1 barrels.

## Physics bodies: barrels that do not explode (2026-11-06, after .40)

Symptom (.39/.40, maps near the object cap, `logs/m40a.err`, `m40b.err`):
a shot barrel no longer crashed but neither exploded nor hurt anyone. The
logs have no object, pool or effect line for it (no counter was logged).

- Cause (CONFIRMED in code; the full pool in play is INFERRED, not measured):
  the prop break `sub_000AE4F0(prop, 1)` (callers 0x000AE7F2, 0x000AE8A4,
  0x001EE132) first asks `sub_0016C020(1)` for a kind-1 rigid body (full when
  word `[0x0074C45E]` >= word P `[0x0074C49C]` + 0x58; never steals). With
  none free it sets state 9 and schedules `sub_000AC560`: the prop fades and
  is destroyed with no explosion (`sub_000EFBD0`), effects or damage. The
  object-slot refusal of `sub_000AB590` only drops debris. P comes from
  `sub_0016C430` (level init, called once at 0x001CDC0C after the object
  pools): 50 unless mode `[0x007DE455]` is 0xA or `[0x007DE458]` & 0x40, 40
  above scene 100, else a scene table (scene 100: 100). Kind-1 bodies (props
  knocked loose, dropped objects; one per object at +0x80) count up from 0x58,
  kind 2 down from P + 0x60 and steals its oldest; only the free (0x0016C401)
  lowers kind 1.
- Fix: `pc_mapmaker_objects_physics` (new `WRAP sub_0016C430`) runs the same
  nine allocations in scene 100 (pools on) with P = 1024
  (`MAPMAKER_OBJECTS_PHYSICS` >= the 1023 slots, so kind 1 cannot fill; about
  610 KB Level heap). The 0x5D-record contact table stays stock. Other scenes
  run the original. The editor's 128-slot reserve is unchanged.
- Log: `pc_mapmaker_objects_prop_break` (new `WRAP sub_000AE4F0`) prints
  `[mapmaker] prop break: object ...: no explosion or damage, refused: no
  physics body free` / `explosion; debris refused: fewer than 5 object slots
  free` / `explosion`, with bodies, live objects of the pool and debris of 20
  (16 refusals, 3 normal breaks per run).
- Tests: `mapmaker_objects.c` (sizes, order, heap, alignment, word store,
  reset, floats; scene/pool gates; log cases) with 19 `physics-*`/`break-*`
  mutations; `test_mapmaker_physics_classified.py` (P's only writer, every
  reader, sizer calls, kind-1 writers, break order, 2 mutations).
- Play check: on the m40 map shoot several barrels, push crates around, then
  shoot again; each break should log `explosion` and hurt nearby characters.

## Engine object pool end: many Story AI (2026-11-07, after .45)

Symptom (`logs/m45c.err`, crash `logs/m45c/crash-20261107-080316885-*`):
BigMap, 395 pieces and about 106 Story AI, crashed about a second into
play, every load; no MapMaker line before it.

- Cause (CONFIRMED in the dump and the code): access violation in
  `sub_0009FF10` (0x0009FF79, room table `[0x007B0CC0]` + id x 12 + 0x1000)
  on object slot 2 (pool `[0x0072F5E8]` = 0x8174AA70, edi 0x5D0): its room
  count byte +0x30 was 0x40 and room id +0x38 the pointer 0x81E718EC. The
  last model drawn (`model-history.txt`) was 0x8174B070 = slot 2 + 0x30 =
  twin 1026 (twin pool `[0x0070AADC]` = 0x816CAC70, 0x200 per slot, ends where
  the object pool starts), and that model's +0 is object 1026. So slots past
  1022 were handed out: `sub_0009EE00` (only caller `sub_000A6880`, the
  create) takes slot = count `[0x007B4FAC]` with no bound once no freed slot
  (`[0x007B65B8]`, +0x2B & 0x80) exists. The editor's budget (pieces + items
  + 64 characters + 64 effects <= 1023) holds, so each AI takes more than its
  body (held weapons: INFERRED, not measured); 63 alive AI need 106+ placed.
- Fix 1 (actors, no new wrapper): `pc_mapmaker_actors_slot` refuses a new
  AI (the stock -1; the spawner retries later frames) while fewer than
  `MAPMAKER_ACTORS_OBJECT_HEADROOM` 72 slots are free in local scene-100 play
  (any mode; not System Link, not other scenes); `pc_mapmaker_actors_placed`
  clamps the level-start count to free - 72. Log
  `[mapmaker] engine objects nearly full: AI not spawned yet, free object
  slots (n of 1023)` (first 3).
- Fix 2: `pc_mapmaker_objects_create` (new `WRAP sub_000A6880`): in scene
  100/102 with pools on, a create with no free slot below the pool returns 0,
  the original's own model-failure result (0x000A695C), and logs
  `[mapmaker] engine objects full (n of 1023): object of template t not
  created` (first 8, then every 256th). `sub_000A6EC0` and `sub_000DAC30`
  test it; `sub_000A6FD0`, `sub_0016EAB0`, `sub_00174120`, `sub_001F6660`,
  `sub_001F9800` do not (stock behaviour at a failed model). Last guard only.
- Editor: unchanged. The 1024-handle ceiling remains; the editor's 128-slot
  reserve already counts 64 characters, and the AI headroom is enforced in
  play, where the real per-character cost is known.
- Tests: `mapmaker_actors.c` (72/71 free, freed slots, past the pool, modes,
  System Link, story levels, no pool, placed clamp; 10 `objects-*`
  mutations), `mapmaker_objects.c` (append at 1022 allowed, full/past/stale
  free count refused, freed slot reused, editor, story, pools off; 7
  `full-*` mutations), `test_mapmaker_objpool_classified.py` (count writers,
  one caller, no bound, null tests, twin sizing, headroom > effects).
- Play check: the m45c BigMap loads and plays; stderr shows the headroom
  line once AI fill the pool, no `engine objects full`; kill AI and watch new
  ones spawn; shoot barrels with many AI alive.

## Engine object handles 2047 (2026-11-07, static analysis and fixtures only)

Handles were `slot << 22 | serial`, so the pool ended at 1024 slots and the
objects cap (pieces + items + 128) bound before the 1023-piece cap. Now:

- `config/guest-code-patches/handles.json` (every scene; written by
  `tools/analysis/object-handles.py`, `--check` compares): the encode
  0x000A69BE `shl ecx,0x16` -> 0x15, the serial wrap 0x000A69C4 `test
  eax,0xFFC00000` -> 0xFFE00000 (serial 1..0x1FFFFF), and all 167 decodes
  `shr r,0x16; imul r,r,0x2E8` -> `shr r,0x15` (one byte each). Handles are
  `slot << 21 | serial`; slots 0..2046 never form -1 (2047 would).
- `pc_mapmaker_objects.inc`: `MAPMAKER_OBJECTS_POOL` 2047 (object and twin
  pools in scene 100/102), `MAPMAKER_OBJECTS_HANDLE_SHIFT` 21 (compile check:
  pool < 2^(32-21)), slot-8/slot-0 lists 2047 (`objects.json`: 0x008AC000 and
  0x008AE000, 0x1FFC each, slot-0 clear 2047 dwords), prop blocks 4096 (16 KB,
  every scene), room buckets 0x14D00 B (4x stock, bound 0x533D, every scene),
  physics bodies P = 2048 (scene 100). `MAPMAKER_LIMIT_OBJECTS` 2047: 1023
  pieces + 500 items + 128 = 1651 fit, so tiles no longer take items away.
  The AI headroom (72) and the create guard read the pool size, unchanged.
- Twins (model instances, 0x200 B, `[0x0070AADC]`): the allocator
  `sub_00097C00` (count `[0x007B67D0]`, freed `[0x007B6984]`, no bound) is
  called by the create and by `sub_0009F490` (characters) and `sub_0009F510`
  (`sub_000A7070`, attachments at obj+0xA4: held weapons etc.), so twins can
  outnumber objects (CONFIRMED callers; counts in play unmeasured; possibly
  part of the m45c overrun). The twin pool is 2047 + 512 = 2559
  (`MAPMAKER_OBJECTS_TWIN_EXTRA`; its size dword is read only by the sizer and
  the clear `sub_0008DBB0`), and `pc_mapmaker_objects_create` also refuses a
  create while fewer than 32 twins are left and none is freed (log adds
  `model instances n of 2559`).
- `src/input/pc_mounted.h` (host-side vehicle gun lookup) decodes `>> 21`;
  its fixtures patch the XBE with `handles.json` before translating.
- Wire and saves (CONFIRMED in code): System Link names objects by the +0x230
  network id, a player byte (+0x150) or a character index (`sub_00359400`;
  receiver `sub_00359390`); none of the 21 senders (22 calls of
  `sub_00353990`) stores a handle, and the five handle decodes in
  0x00351000..0x0035E000 read local tables. The fork's own network code uses
  content keys (`pc_story_world.inc`) or local identity only. Saves and map
  files hold no handles (the serial restarts at 1 with every pool reset,
  0x000A5B62). So stock System Link peers are unaffected and the change is
  not gated; scenes other than 100/102 keep 550 slots (20 in scene 101).
- Ruled out (`test_mapmaker_handles_classified.py`): other `shr/sar 0x16`
  (12: vector unpacks, flag bits, MD5) and `shl 0x16` (colour, MD5) sites, the
  9 `imul 0x2E8` uses fed by counts, no division by the stride anywhere, the
  serial written only by the encode and the reset, the 2512-bit visibility
  set (`sub_000A0130`) above 2047.
- Level heap (scenes 100/102 unless noted): object pool +762 KB (2047 x
  0x2E8 = 1.52 MB), twins +768 KB (2559 x 0x200 = 1.25 MB), physics +572 KB
  (1.2 MB, scene 100), buckets +42 KB and prop blocks +8 KB in every scene:
  about 2.15 MB more on a MapMaker map, 50 KB on story levels. Not measured
  in play.
- Tests: `test_mapmaker_handles_classified.py` (7: table = scan, mutation of a
  missing/extra row, reviewed sites, encode/serial writers, wire, host
  decoder), `mapmaker_objects.c`/`test_mapmaker_objects.py` (2047 pools,
  lists, 4096 blocks, P 2048, create guard at 2046/2047; `pool-slot22`,
  `pool-minus-one`, `handle-shift-stock`, `list0-old-place`,
  `physics-old-pool` mutations), `mapmaker_limits.c` (1023 + 896 = 2047,
  every piece count keeps 500 items), `mapmaker_restable.c` (`Objects
  n/2047`), `test_mounted_mouse.py`; `check-tables.py`.
- Play check: a map of 1023 pieces + 500 items loads and plays (stderr: no
  `engine objects full`); System Link between two builds and against a stock
  build on a stock map; a story level with vehicles (mounted guns use the
  host decoder); shoot barrels on a large map.

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

## Tiles 511 -> 1023 (2026-11-07, static analysis and fixtures only)

`MAPMAKER_LIMIT_PIECES` and `MAPMAKER_ROOMS_MAX` are 1023 (rooms stage 12 in
[rooms](limitless-rooms.md)). Each piece is one engine object; with the
2047-slot pool (handles slot << 21, below) 1023 pieces + 500 items fit, so
row 1 no longer binds (it did at 895 - items). Old maps load unchanged; stock
behaviour (limits off, story levels) keeps every editor limit; the rooms
tables run in every scene as before.

What binds past 511, ranked (for 1023 pieces; heaviest tile 14 pads, 46 links):

| # | cap | where | now | 1023 pieces need | cost / status |
| --- | --- | --- | --- | --- | --- |
| 1 | engine object handles | pool `[0x0072F5E8]`, handle slot << 21 (`handles.json`, 167 decodes) | 1023 -> **2047** | 1023 + items + 128 | done: 1651 at the piece and item caps; +2.15 MB Level heap |
| 2 | pad groups | `[0x00762CB8]`, matrix `sub_00275F70` (N+1) x N x 4 in the 6 MB nav block | 1024 | ~1,535 (1.5/piece) | binds ~680 pieces; 1,536 groups = 9.4 MB matrix and 2.4 M route searches at load |
| 3 | regular pads (ids <= 0x2328) | `pads.json` thresholds; regular handles < cover base 0x1F40 | 7,680 | 14 K heavy, 9.2 K tile 17 | 549 heavy, 853 tile 17: binds first since links are counted (2026-11-07); past 8000 the cover handle base and cover ids (0x2329) move |
| 4 | links (int16 A* indices) | `MAPMAKER_POOL_LINKS` 32,512 | 32,256 (template + half side slots) | 59 K heavy, 36 K tile 17 | 556 heavy, 921 tile 17 (was 521 / 716 at 16 per piece); past 32,767 the int16 node/route sites need widening |
| 5 | memory estimate | scale 8, 16.4 MB | 2,048,000 x 8 | | 386 heaviest-memory, 1,960 light |
| 6 | Level heap in play | arena 47 MB (+16 MB by `TSFP_MAPMAKER_ARENA_MB`) | unmeasured | ~30-48 KB per tile with content (two maps) | real limit for heavy content; probe flag 32 |
| 7 | cover pads | 10,240 handles | 9,984 | 14 K heavy | 713 heavy |
| 8 | portal flags | stack 0x4000 (`MAPMAKER_ROOMS_PORTALS`) | 16,383 | ~14 per piece | ~1,170 average; the compile guard drops pieces past it |
| 9 | light records per room group | pool 500 / (rooms / 16) | ~7 per 16 rooms | | quality only (sparser lights), unmeasured |
| 10 | editor piece pool | `0x00302FF0`, `0x00307C43/4F/61` | 1024 -> **2048** (items.json, +72 KB editor) | 1023 placed + a copy of all + 1 | done; a group copy past the pool wrote through a null node (`sub_002F9920`) |
| 11 | level fill frame | `level.json` `sub_00240C50` | 512 -> **1024** records (frame 0x2090) | 1023 | done (+4 KB stack) |
| 12 | room machinery | `rooms.json` | 511 -> **1023** ids | | done: +48 KB view array, +12 KB per-model array (every scene), +25 KB prop slots (scene 100) |
| 13 | container / undo | 512 KB, tiers to 0x7FFE0 | | 1023 x 248 B (type 41) = 254 KB + items | fits |
| 14 | loader piece table, Story AI piece byte | `sub_00301A90` 1024 pointers; retries +256/+512/+768 | 1024 | | fits |
| 15 | pickups 768, physics bodies 1024, prop blocks 2048 | objects area | | follow items/objects, not tiles | fit within the object pool |
| 16 | room AABB tree staging | `sub_00086FC0` in `sub_0003D6E0(3)` (12 KB of a 16 KB pool) | 438 -> **1023** (C staging `0x00911000`) | 1023 x 0x1C | done 2026-11-07: past 585 it overwrote the heap handle `[0x007EB4B4]` (room 602; the 676-tile load crash). [rooms](limitless-rooms.md) stage 13 |

New table entries: `items.json` `mm-piece-pool-bytes/-loop/-last/-count`
(2048 nodes); `level.json` re-valued (frame 0x2090, arguments +0x1060);
`rooms.json` re-valued (see rooms stage 12). No new wrapper. Tests:
`mapmaker_limits.c` (895 cost-free fit, 896th refused on objects, 1024
also on pieces, heavy tiles refused on pads at 549 (links counted, see Links), 600 + 295),
`test_mapmaker_limits.py` (`pieces-511`, `pieces-past-rooms`, refusal lines
`engine objects 1024 of 1023` and `pieces 1024 of 1023; engine objects`),
`test_mapmaker_limits_classified.py` (`test_piece_pool_*`: four sites in two
functions, the count only set/dec/inc), `mapmaker_restable.c` (Tiles 1023 row
red, collapsed `Objects 1156/1023`), `mapmaker_validate.c` (1024 kept, 1025
hidden).

Save/load delay (decoded; fixed 2026-11-07, see "Save/load delay" below): `sub_002FC470` (piece variant)
first calls `sub_002FC3D0`, which walks the whole placed list (loop
0x002FC3F5..0x002FC41C, maximum z + height), so each call is O(pieces).
The writer's pad-id helper `sub_00331E50` (once per item, Story AI point and
logic reference) walks the pieces before its target calling `sub_002FC470`
for each (0x00331EC2..0x00331EF6), and `sub_002FC680` per piece calls it
~30 times: about (items + points) x n^2 / 2 list steps, ~50 M at 511 pieces
+ 384 items, 4x at 1023. A save compiles twice (`sub_00312DF0` 0x00312E05,
then the undo shadow `sub_00336640`), a load once, every edit once
(`sub_00337600` 0x003376BA).

## Save/load delay: the piece top remembered (2026-11-07)

- `pc_mapmaker_editor.inc`: new `WRAP sub_002FC3D0 -> pc_mapmaker_editor_top`
  and `WRAP sub_003006C0 -> pc_mapmaker_editor_compile`; the existing
  `pc_mapmaker_editor_load` (`sub_00301A90`) also opens the scope. Inside a
  compile or load (nesting counted) the first call walks and later calls
  return the same eax/ecx/edx while editor, flag `+0x21C`, head `[0x0078400C]`
  and count `[0x00784000]` are unchanged (the original writes no memory;
  `sub_00307EF0` appends and counts, `sub_00308480` unlinks and counts down,
  so the loader's appends recompute). Outside the scope (3D view, placement,
  pieces moving) always the original. `TSFP_MAPMAKER_SAVE_MEMO=0` turns it
  off. stderr per scope with 1000+ calls (first 4): `[mapmaker] save
  compile|load: piece top remembered (n walks, m remembered), t ms`.
- Effect: a compile is (items + points) x pieces mask calls; the walk per call
  goes away. Fixture: 65,629 calls on 1023 pieces take 7 walks (6,145 list
  steps) instead of 67 M steps. Not measured in the game: compare the stderr
  `ms` with `TSFP_MAPMAKER_SAVE_MEMO=0` on the same map.
- Tests: `mapmaker_editor.c` `test_top_memo` (equal eax/ecx/edx on every call
  through appends, flag, unlink, unlink + append at the same count, nested
  load in a compile, switch off; esp), 10 `top-*` mutations in
  `test_mapmaker_editor.py`.

Play check (user): see [rooms](limitless-rooms.md) stage 12.

## What bounded 511 pieces (cap 511, 2026-11-02; see above for 1023)

`MAPMAKER_LIMIT_PIECES` is 511 ([rooms](limitless-rooms.md) stage 11; the
level fill pass frame `level.json` holds 512). Pads stop at 548, links at
520, cover at 713, groups at about 680 (1.5 per piece), objects at
1023 - 128 - items. Memory estimate per tile (XBE rows 0..45): light tiles
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
the return address) with a panel at the meter's x, y in font 0x12 at `\vs60`
(alignment `[0x007E7260]` zeroed and restored): a `sub_0001EFB0` backdrop
(`0x0F0F0F64`), then outlined `sub_00041E30` text. Collapsed (default) it is
one line `+ Fullest: <row> used/max` (worst state, then used/max); expanded,
`- Resources` and 15 rows, short titles left and `" used/max"` right-aligned
at one edge, widths from `sub_0003FCF0(&w,&h,text,0)` (scratch below
`g_esp`; 5.2/char fallback), pitch 11.5 or measured glyph + 1.5. Values
start with a space because `\vs` reads all following digits (0x00040082).
`mapmaker_table_input(menu)` (called in `pc_display_menu_mouse_input` after
`mapmaker_pointer_input`) toggles it on the editor screen (scene 102, menu
`+0x178`/`+0x170` zero, panel drawn within 4 ticks) for an A press
(`+0xC4 & 0x10`) with the game pointer (`0x007BA260 + player*0x1E0 +
0x20/0x24`) on the panel, clearing `0x00100010` from `+0xC4`/`+0xC8` until
release, or for the I key (`pc_controls_mapmaker_table_key`, Windows
`pc_controls.c`, Mac `input_sdl.c`; off when any binding uses I,
`pc_mapmaker_table_key` in `pc_mapmaker_pointer.h`). State is per process.
Rows: the caps of `pc_display_mapmaker_limits`
from `mapmaker_limits_view` (placed counts; red on its `over` bits), pickups
of 768, Story AI `editor+0x280`/511, logic ops `editor+0x41E0`/120, and the
per-category counts of `sub_00303460`/`sub_00308420` (catalogue
`0x00524F58 + id*0xC4 + 2`, maxima `0x00524BE0 + c*24 + 4`) for movers,
props, turrets 27-29 and teleporters 14-18. Stock meter when the limits are
off, the view is not valid for this editor, `TSFP_MAPMAKER_TABLE=0`, probe
flag 1, or no text buffer. Whole-chain fixtures link `sub_0003FA10`/
`sub_0001EFB0`/`sub_00041E30` from `mapmaker_area_stubs.h` and stub
`pc_controls_mapmaker_table_key` (`menu_mouse.c`, `fov_settings.c`); the
restable fixture gives its guest-call stubs distinct bodies (MSVC folds
identical functions, and its `guest_call` dispatches by address). Tests:
`tests/mapmaker/test_mapmaker_restable.py` (rows, colours, collapsed/expanded,
click/key toggle and A removal, column alignment, pitch, 4:3 and wide layout,
64 mutations). Contract and playtest steps:
`docs/mapmaker/mapmaker-restable.md`. Not built or played.

`mapmaker_limits_view` (in `pc_mapmaker_limits.inc`): placed pieces, items +
free objects, objects, pads, cover, links, groups, placed memory, `over` and
the editor pointer of the last completed check; `valid` is cleared at the
start of each check. A cap changed here changes its row; the fixture reads
the editor/objects limits from their sources.

## Story AI points past piece 255 (2026-11-06)

Entry point: `pc_mapmaker_editor_sai_point` in `src/interface/pc_mapmaker_editor.inc`,
a new `WRAP sub_00332330` that acts only on the editor loader's call (return
address `0x0030246B`). Saved Story AI points keep their piece as one byte
(`0x00301570`). On maps of more than 256 pieces, points on piece 256 and above
reloaded onto the wrong piece. They then vanished from the map, drew at the
pointer (`sub_003096A0`, `0x00309727`) and were dropped at the next save. A
failed pad-id decode is now retried on the pieces whose index has the same low
byte (+256, +512). The save format is unchanged. Contract:
`tests/mapmaker/test_mapmaker_editor.py` (`sai-point-*`). Details:
`docs/mapmaker/mapmaker-editor.md`.
