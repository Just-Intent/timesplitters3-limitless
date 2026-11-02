# MapMaker rooms past 241 (Limitless fork)

Extends [limitless.md](limitless.md) for the `rooms` area. Player/contract
notes and the full site lists: [mapmaker-rooms.md](../mapmaker/mapmaker-rooms.md)
("Byte-width room ids" and "Rooms 241 -> 255"). Static analysis only; nothing
here was built or played by the agent that wrote it.

## Entry points

- `config/guest-code-patches/rooms.json`: operand entries and relocations
  (special ids, real-room bound, id-space counts, per-room/per-id tables, view
  shadow, model table). Check: `.venv/Scripts/python.exe
  logs/limits/check-tables.py config/guest-code-patches <checkout>` (ignored
  script in the main checkout; needs the private XBE) prints "table verified".
- `src/interface/pc_mapmaker_rooms.inc`: room query, compile guard, room-model
  prop slots.
- Tests: `tests/mapmaker/test_mapmaker_rooms.py` (fixture `mapmaker_rooms.c`,
  table shape, every room constant classified),
  `tests/mapmaker/test_mapmaker_rooms_bytes_classified.py` (byte-width room
  ids, decode of all game code; skips without the private XBE).

## Stage 2: per-id drawn flag table

`0x007B5180` dword[0xD6] has one reference, `0x000A79C8` in `sub_000A79B0`
(`[room*4+0x7B5180] = 1`). It is relocated to `0x008CC800` (0x110 entries,
declared 0x354 so the list count `0x007B54D8` after it stays outside the
range); `objects.json`'s slot-0 list `0x007B4FC0` is declared 4 bytes short
(0x1BC) so the two ranges do not touch. In the 241 layout ids 0xD6..0xFE wrote
past the stock table (into `0x007B54D8` and the moved-away `0x007B54E0`
tables); now they land in the new table. Test:
`test_room_flag_table_7b5180_is_relocated` plus four table mutations.

## Stage 3: light room side table

`light+0x99` is the only room byte the stage-1 audit found. The five light
functions get wrappers in `pc_mapmaker_rooms.inc` (`_light_reset`, `_unlink`,
`_colour`, `_link`, `_create`; conventions in mapmaker-rooms.md "Wrappers").
A light whose room is above 0xFF keeps the full id in a C side table (pool
index, the 0xC0 record, or by address for records past the pool); the bodies
keep reading the byte, so the hooks fix the three byte uses: the head
unlink, the colour lookup and the creation's round-robin step / link call.
Rooms 0..0xFF take the original paths, so with ids below 0x100 nothing
changes. The round robin `0x0073D910` moved to `0x008CB440` (32 entries).
Tests: light part of `tests/mapmaker/mapmaker_rooms.c` (models of the guest
bodies called through the hooks, 13 mutations) and
`test_light_side_table_matches_the_code` (callers, globals, argument slot,
pool sizes, rooms.json heads/row/round robin against the C defines).
Risk: special-id lights in story levels are INFERRED rare (stock would index
past the light pool for them).

## Stage 4: special ids 0x100..0x10C (every scene)

`rooms.json` renumbers the special ids by +0xE (stock +0x37): spare 0x100,
players 0x101..0x105, views 0x106..0x10B, global 0x10C, id space 0x10D; the
21 real-room compares become `cmp 0xFF`. These entries run in story levels
too. Resized/moved: per-id prop tables (`0x008C7000` step 0x440), id loop
ends and `mov ecx` counts, `[0x7B977C]` clear, light heads (`0x008C8540`,
row 0x10C) and colour (`0x008C9200`), model table (0xA08 words at
`0x008CA000`) with its bounds and the `[0x7087A4]` per-model array, and the
view shadow re-packed inside 0xFFC (rects +0x6000 for 0x10D ids, flags
+0x6CA0, list +0x6DB0). Byte draw buckets are untouched (real rooms only).
`pc_mapmaker_objects.inc` and `pc_mapmaker_probe.inc` follow the moved
tables; the `rooms` area can no longer be switched off. Free space and the
extension map: mapmaker-rooms.md "Rooms 241 -> 255" and the 00-header note.
Tests: `test_table_shape`, `test_id_space_tables` (+19 mutations),
`test_other_areas_use_the_moved_tables`, `test_extension_map_rooms_sub_range`
in `tests/mapmaker/test_mapmaker_rooms.py`. Story-level playtest list:
mapmaker-rooms.md "What to test in play" (S1..S7).

## Stage 5: editor limit 255

`MAPMAKER_LIMIT_PIECES` (`pc_mapmaker_limits.inc`) and `MAPMAKER_ROOMS_MAX`
(`pc_mapmaker_rooms.inc`) are 255, gated as the 241 raise was: the editor
limit only with `TSFP_MAPMAKER_LIMITS=1`, the compile guard only in scene
100 and only when room models 0x908..0xA06 all have prop slots (else 200).
Links (255 x 46 = 11,730 of 12,000), pads, cover, pad groups, portals and
the 1000-object pool still fit. Past 255: word draw buckets (mapmaker-rooms.md
stage 6). Tests: `test_mapmaker_rooms.py` fixture at 255 (`cap-241`,
`cap-raised` mutations), `test_mapmaker_limits.py` (`pieces-241`).

## Past 255: plan to 1023 (mapmaker-rooms.md "Rooms 255 -> 1024")

Room machinery reaches 1023 ids (specials 0x400..0x40C) after stages 6-10;
the view shadow stops at 341 ids until the view stride changes (stage 7).
Real maps are capped earlier by pads (~500-570 heaviest tiles), links
(~712, int16 A* indices) and play memory (~550-650 tiles, unmeasured).
Next safe target: 511 (view stride 0x3000). `MAPMAKER_LIMIT_PIECES` stays
255 until a target's stages are all done.

## Stage 6: word draw buckets (every scene)

Hooks in `pc_mapmaker_rooms.inc`: `pc_mapmaker_rooms_buckets` replaces
`sub_00086C40` (depth lists as C word lists, one context per nesting level,
then the stock calls), `pc_mapmaker_rooms_view_lists` replaces
`sub_000897D0` (view room list at shadow +0x6DB0, bounded by 0x10D words),
`pc_mapmaker_rooms_bucket_draw` wraps both readers (`sub_00086480`,
`sub_000865E0`, eax = depth): copies the depth's rooms into the staging row
`0x008C1800`, sets the count byte `0x702DB0+depth`, 255 per call, re-pushing
the return address for each further call. `rooms.json`: reader row stride 0
(`0x000864CB`, `0x00086621`) and word loads (`code` entries `0x000864F4`,
`0x00086644`). Stock results for up to 255 rooms per depth; the four
wrappers and the table must ship together. Tests:
`tests/mapmaker/test_mapmaker_rooms_buckets.py` (fixture
`mapmaker_rooms_buckets.c` against models of the stock byte code, 27 C and 6
table mutations, decode of the word loads). Overlap: the camera view work
(depth cap 0x4F, view fields) touches the same two functions.
