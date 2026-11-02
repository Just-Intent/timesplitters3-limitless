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
