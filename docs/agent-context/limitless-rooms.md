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
Next safe target: 511 (view stride 0x3000), reached in stage 11; 1023 in stage 12.

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

## Stage 7: view stride 0x3000 (every scene)

Views `[0x7B0CE4]` are 0x3000 apart (stock 0xFFC); the shadow is inside the
stride: rects +0x1000 + 12*id, flags +0x28A0, list +0x2AB0 (room for id space
0x20D). Sites: 35 stride steps, `sub_000C61D0`'s unrolled init (4 multiples,
20 fields), two magic divisions (`0xAAAAAAAB`, shift 13), allocation 0x12008.
C readers: `MAPMAKER_ROOMS_VIEW_*`, `OPENVIEW_RECT`, `MAPMAKER_CAMERA_VIEW_RECTS`,
`SMX_PLAYER` (pinned by `test_other_areas_use_the_moved_tables`). Tests:
`test_view_stride_table` (+10 mutations), `test_every_view_stride_is_classified`
(decode). Details: mapmaker-rooms.md "Stage 7".

## Stage 9: sub-range repacked for 511 (before stage 8)

Per-room tables 512 entries, per-id tables room for 0x20D ids, light heads
`0x008D0000`, colour `0x008D18A0`, round robin `0x008CB700` (64), drawn flag
`0x008C8500`, staging row `0x008C3000`; fixed: t3 `0x008C0800` (sky mask),
p1/p4 `0x008C7000`/`0x008C7CC0` (probe), model table `0x008CA000`. Objects
clears p2/p5 at `0x008C5000`/`0x008C6080`; `MAPMAKER_ROOF_ROOMS` 512,
`MAPMAKER_CAMERA_ROOMS` 511. Map: `ROOMS_BLOCKS` (`test_rooms_blocks`) and
mapmaker-rooms.md "Stage 9".

## Stage 8: special ids 0x200..0x20C (every scene)

Real rooms 1..0x1FF, spare 0x200, players 0x201..0x205, views 0x206..0x20B,
global 0x20C, id space 0x20D; the same 155 operand entries, sizes tied to the
id space follow (loop ends, `[0x7B977C]` 0x210, light row 0x20C, colour,
per-id tables 0x20D). C: `MAPMAKER_ROOMS_IDS_REAL` 0x1FF, `_ID_GLOBAL` 0x20C,
probe/objects 0x20D. Sourceless lights in rooms 0x100..0x1FF keep their colour
untinted. mapmaker-rooms.md "Stage 8".

## Stage 10: model table 0xB08 (every scene)

Model -> slot table `0x008CA000` 0xB08 words, bounds 0xB08, per-model array
0x108C0; `MAPMAKER_ROOMS_MODEL_ENTRIES` with a compile-time fit check;
probe 0xB08. Prop slots and the compile guard follow `MAPMAKER_ROOMS_MAX`.
Cap: 255 until stage 11 (values needed: mapmaker-rooms.md "Cap after
stages 7-10").

## Stage 11: cap 511 (2026-11-02)

`MAPMAKER_ROOMS_MAX` and `MAPMAKER_LIMIT_PIECES` 511 (real rooms 1..0x1FF;
a compile-time check refuses more). The compile guard keeps 511 only when
room models 0x908..0xB06 all have prop slots (else 200); prop slots are
synthesized for rooms 201..511 (311). New table
`config/guest-code-patches/level.json` (10 operands): `sub_00240C50`, the
level fill pass, keeps an 8-byte per-piece array at frame+0x90 in a 0x1030
frame (500 pieces; entry 500 is the return address, the validator allowed
505); frame 0x1090 holds 512, arguments `[esp+0x103C/0x1044]` +0x60,
`MAPMAKER_VALIDATE_LEVEL_PIECES` 512. Sky mask switch covers 512 rooms.
The editor logs `[MAPMAKER] placement refused: <cap> n of m` once per change
(`mapmaker_limits_log`). What bounds maps: [caps](limitless-caps.md).
Tests: `test_mapmaker_rooms.py` (511/512 drop, 256 kept, prop slots 611,
`cap-255`, `cap-raised` refused at compile time), `test_mapmaker_limits.py`
(`pieces-255`, `reason-*`, memory first at 387 heaviest),
`test_mapmaker_validate.py` (`test_level_frame_*`: every frame site decoded,
512 kept, 513 hidden), `test_mapmaker_skymask.py`. Playtest: mapmaker-rooms.md
"Stage 11".

## Stage 12: cap 1023 (2026-11-07, static only)

`rooms.json` re-valued by one mapping from the 511 layout (406 entries, same
sites; `check-tables.py` verified): real rooms 1..0x3FF (21 `cmp` -> 0x3FF),
spare 0x400, players 0x401..0x405 (adds -0x401), views 0x406..0x40B, global
0x40C, id space 0x40D (SHIFT 0x337); per-room clears and the t7 capacity
0x400; per-id loop ends 0x1034/0x1030; `[0x7B977C]` 0x410 bytes (clear 0x103
dwords + stosw); model table 0xD08 words (bounds 0xD08, clear 0x684, per-model
array 0x138C0); light row 0x40C (stride 0x1030, clear end `0x008E00BC`, a
bound only); view stride 0x5000 (35 steps, init multiples/fields, array
0x1E008 / clear 0x7802, division magic `0xCCCCCCCD` with `sar 0xE`, exact for
every multiple and the 32-bit extremes), shadow rects +0x1000 (unchanged),
flags +0x40A0, list +0x44B0 (count-relative +0x36A8/+0x36A6), list cap 0x40C.

Blocks (`ROOMS_BLOCKS`): t1 `0x008C0000`, t3 `0x008C1000` (sky mask), t2
`0x008C2000` (0x1000 each), t4 `0x008C3000` (0x3000), t6 `0x008C6000`, t7
`0x008C7000`, staging row `0x008C8000`, query `0x008C9000` (2 x 1023 words:
the tree holds real rooms only), model table `0x008CA000` (unchanged base,
to `0x008CBA10`), round robin `0x008CBC00` (68 groups), p1 `0x008CC000`, p4
`0x008CD040` (globals at +0x1030), p2 `0x008CE080`, p3 `0x008CF0C0`, p5
`0x008D0100`, drawn flag `0x008D1140` (0x1040 each), boss camera lists
`0x008D4000` (unchanged), light heads `0x008DC000` (0x3090); above the pak
table: front-end view `0x00908000` (0x5000) and light colour `0x0090D000`
(0x3094). Free: `0x008D2180..0x008D4000`, `0x008DF090..0x008E0000`. C
followers: rooms defines, objects clears (0x40D dwords), probe (counts/lists,
ids 0x40D, models 0xD08), bossroom (p1/p4/p3, global 0x40C, rooms 0x40B),
skymask (t3, 1024 rooms), roof (1024 rooms, 4096 piece slots), camera 1023,
sky music stride, openview comment. Level heap: view array +48 KB and
per-model array +12 KB in every scene, prop slots +25 KB in scene 100
(rooms 201..1023 copied from room 200's slot), level fill frame +4 KB stack
(`level.json`, 1024 records), editor piece pool +72 KB (`items.json`).

Gating as before (editor limit with `TSFP_MAPMAKER_LIMITS=1`; compile guard
1023 only when room models 0x908..0xD06 all have slots). What bounds a map
now: [caps](limitless-caps.md) "Tiles 511 -> 1023" (engine objects first).
Tests: `test_mapmaker_rooms.py` (`cap-511`, `cap-raised` 1024 refused at
compile time, 1024th piece dropped, 1123 prop slots, `p1-511-size`,
`light-row-511`, `model-*-511`, `front-end-view-511-place`, high range in
the extension map), `test_mapmaker_rooms_bytes_classified.py`, the bossroom,
objects, probe, skymask and sky music fixtures, `test_mapmaker_validate.py`.

Play check (user, not done by the agent): (1) a story level (level 1) and an
Arcade map play as before, splitscreen menus and security-camera screens
draw; (2) an old 511-tile map loads and plays; (3) with `LIMITS`, place
light tiles to the Objects cap (about 895 with no items): the editor stops on
`engine objects 1024 of 1023`, save, reload, play: stderr `rooms n of 1023`
and no `DROPPED`; walk across it and watch for missing rooms or lights;
(4) copy a large group on that map (piece pool); (5) note save/load seconds
at 511 and at the cap.
