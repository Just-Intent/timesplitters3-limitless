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
