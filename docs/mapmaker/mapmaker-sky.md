# MapMaker open sky: per-map sky and clouds, sky-tile probe

Feature (c) of `logs/limits/features/sky.md` (ignored, main checkout), plus the
probe for feature (d). Static analysis of the US XBE (title 45410066);
CONFIRMED = read at the given address, INFERRED = deduced. Nothing here has
been built or played yet.

Files: `src/interface/pc_mapmaker_sky.inc` (C policy, wrappers), `tests/mapmaker/mapmaker_sky.c`
+ `tests/mapmaker/test_mapmaker_sky.py` (fixture with mutations, stock-data checks
against the local XBE), probe flag 128 in `src/interface/pc_mapmaker_probe.inc`
(`tests/mapmaker/mapmaker_probe.c`, `docs/mapmaker/mapmaker-meter-probe.md`), and the dev-only
room mask switch in `src/interface/pc_mapmaker_skymask.inc` (`tests/mapmaker/mapmaker_skymask.c`
+ `tests/mapmaker/test_mapmaker_skymask.py`). No guest-code table entries, no extension
memory.

## The sky choice already exists in the stock editor

The research report assumed the editor only writes -1 or 0 to editor `+0x220`.
It missed an indirect writer: the Map Settings menu builder `sub_003133E0`
binds a list row to `&editor+0x220` (`add eax,0x220; push eax; call
sub_00078590` at `0x00313833..0x0031383E`, CONFIRMED). The row's options are
the 11 entries of `0x0052D150` (`0x0031380B..0x00313831`): value -1 (text
0x426, the tileset's own sky) and skies 58..67 (texts 0xD81, 0xD83, 0xD84,
0xD85, 0xD82, 0xD7D, 0xD86, 0xD80, 0xD7E, 0xD7F). The row label is text 0xABE;
in the same row follows a stock checkbox (text 0xABF) bound to editor `+0x21C`
(`0x00313876..0x00313883`), and while that box is set the sky row and the next
row (text 0xAC0, four weather choices bound to `+0x224`) are greyed (`ebp` =
`+0x21C != 0` passed to `sub_00079DE0`/`sub_0007D4A0`, CONFIRMED). The
English captions were not read (they are in the game's text bank).

Rows of the Map Settings menu in build order (`sub_003133E0`): tileset list
(`+0x204`), text 0x3B7 list (`[0x00784028]`) + button 0xB08, text 0x3B8 list
(`[0x00784024]`) + button 0xB09, text 0xABB music list (`+0x210`), text 0xABC
with checkbox 0xABD (`+0x214`), **text 0xABE sky list (`+0x220`) with checkbox
0xABF (`+0x21C`)**, text 0xAC0 weather (`+0x224`), text 0xAC6 game-mode
checkboxes (`0x00762F38..`), text fields 0xAC5 and 0x3DB.

The choice is saved as info data `+0x10` (`0x00300795`) and loaded back
(`0x00301C6D`); at map start `sub_0023F390` uses it when it is not negative,
else the tileset default (`0x0023F482..0x0023F4A1`), and loads the sky model
(`0x0023F4BA`). The menu's close callback `sub_00313310` turns 0 into -1 while
editor `+0x218` (info flag 0x10000, default 1, no menu row) is set
(`0x00313396..0x003133C3`). So any of the 11 skies can already be chosen per
map, stored in the existing field, and the stock game loads it. No change was
made for the choice itself.

## Clouds (new, `TSFP_MAPMAKER_LIMITS=1`)

MapMaker skies have no cloud layer: sky records 58..67 (`0x005149A8`, 0x34
bytes) have zero cloud colours at `+0x10`/`+0x14`, and `sub_0011C9F0` enables
the layer (`[0x007AC7DC]`) only if one is non-zero (`0x0011CA31..0x0011CA45`);
the layer then loads `textures/misc/clouds256x256.raw` (`sub_0011C9B0`) and
draws it after the sky prop (`sub_0011CDC0` from `0x00086B88`). Cloud fields
are `+0x10`, `+0x14` (colours) and `+0x18`, `+0x1C`, `+0x20` (floats read by
`sub_0011C5C0`). Story sky record 5 (used by scenes 20, 57 and 62) has
`0x5A5A6420, 0, 200.0, 50.0, 0`.

### Editor

- A **Clouds** checkbox is appended to the sky row, after the stock checkbox
  0xABF: `pc_mapmaker_sky_settings` runs after `sub_00078560` returns to
  `0x00313888` (the binding of `+0x21C`) in scene 102 and adds the checkbox the
  way the stock code adds a game-mode checkbox (`0x00313940..0x00313974`):
  `sub_00079D60("Clouds", -1, 0x8800, 0, 0, 0)`, `sub_0007D4F0(0, 0)`,
  `sub_00078560(cell)`. The caption and the bound dword live in 16 bytes from
  `xbox_HeapAlloc` (the PC menus' own string storage), not in the extension
  block. The stock `sub_000716D0` item id call is left out, as for the
  game-mode checkboxes.
- Storage: map info flag **0x40000** (data `+0x244`). Stock readers of that
  dword use bit 0, bits 1..14 (`sub_002FDA20`), 0x8000 (`sub_002BE750`,
  `0x00301D20`), byte `+0x246` bit 0 = 0x10000 (`0x00301D38`), 0x20000
  (`0x00301D46`, `0x0023F42F`); the only other access is the rebuild
  (`sub_003006C0`, `0x003007B9..0x0030087A`). All `+0x244`..`+0x247`
  displacements in the listing were checked; the rest belong to other
  structures (editor object, stack frames). CONFIRMED free; the scan was
  text-based, not decoder-based, because no table entry depends on it.
- The rebuild writes the dword from editor fields, so `pc_mapmaker_sky_compile`
  reads the choice before the original and sets or clears the bit in the
  rebuilt chunk afterwards. The choice is: the checkbox once the menu has
  bound it; 0 after a map reset other than a load (`sub_002FEBF0` called from
  `0x00311AA2`, which then rebuilds the empty map: New map, INFERRED; and from
  `0x003029C9`, `0x0030302B` (before the resume load at `0x00303076`) and
  `0x00312122`); after a load (`sub_002FEBF0` returning to `0x00301B4F`) or in
  a new editor object, the flag in the container.
- Limits off: none of this runs; re-saving a map in a session without the
  switch drops the flag (the stock rebuild), the sky choice stays.

### Play (scene 100)

`pc_mapmaker_sky_clouds` wraps `sub_0011C9F0`. Order of a MapMaker map load
(CONFIRMED in the listing): `sub_001CDB80` (`0x0003D590`) calls
`sub_0011C9F0`; then `sub_001CDC50` (`0x0003D5A3`) calls `sub_0011C9B0`
(texture), and its tail `sub_0006C920` calls `sub_0023F390`, which sets the
level's sky only then. Therefore the hook resolves the sky itself from the
container (info `+0x10`, else the tileset table `0x0052CF40` byte `+2`), and:

1. restores any record it changed for an earlier map (also with the switch off);
2. if the map has flag 0x40000 and its sky is 58..67, copies record 5's
   `+0x10..+0x23` into that record;
3. sets the scene-100 level entry `0x00518778 +0x10` to the map's sky only
   while the original runs (the stale value of the previous map is put back;
   `sub_0023F390` writes the same sky later).

Extra skies (`docs/mapmaker/mapmaker-sky-music.md`): when the map has an
extra sky row (info flag bits 19..23) and its paks are mounted this level
(`mapmaker_sky.extra_prepare`: the model's level pak and, for 14 rows, the
level's memory pak with its textures), `mapmaker_sky_index` returns that
story record and the hook sets the level's sky to it while the original
runs, without patching anything, so the record's own cloud fields apply
(record 5 has clouds). With either pak missing the map's MapMaker fallback
sky is used as above.

Story safety: no entry of the 87-entry level table `0x00515FF8` uses skies
58..67 (checked by `tests/mapmaker/test_mapmaker_sky.py` when the XBE is present), the
hook acts only in scene 100, and nothing is written in other scenes. INFERRED:
`sub_0011D240` (start phase, `sub_001CDD60`) runs after the load and reads the
copied floats; the cloud texture loads in scene 100 as in story levels.

### Compatibility

- The stock game ignores flag 0x40000 (it is in no mask it tests): the map
  loads with its sky and without clouds. Maps keep version 0x12D and no chunk
  is added. System Link sends the whole container, so a PC peer with the
  switch sees the clouds.

## Wrappers for the integrator

    WRAP sub_0011C9F0 -> pc_mapmaker_sky_clouds
    WRAP sub_00078560 -> pc_mapmaker_sky_settings
    WRAP sub_002FEBF0 -> pc_mapmaker_sky_reset
    WRAP sub_003006C0 -> pc_mapmaker_sky_compile
    WRAP sub_000A2900 -> pc_mapmaker_probe_room_draw
    WRAP sub_0008F950 -> pc_mapmaker_probe_prop_draw
    WRAP sub_00085710 -> pc_mapmaker_sky_room_mask

`sub_003006C0` is already wrapped by the container area
(`pc_mapmaker_container_compile`). Chain them with the sky hook outside, so it
sees the final rebuilt chunk:

    static void sub_003006C0_container(void)
    { if(pc_mapmaker_area_enabled("container","pc_mapmaker_container_compile")) pc_mapmaker_container_compile(sub_003006C0_gen); else sub_003006C0_gen(); }
    void sub_003006C0(void)
    { if(pc_mapmaker_area_enabled("sky","pc_mapmaker_sky_compile")) pc_mapmaker_sky_compile(sub_003006C0_container); else sub_003006C0_container(); }

All hooks call their original exactly once (none replaces it), so no
`MAPMAKER_GUEST_RET`. Include `pc_mapmaker_sky.inc` after
`pc_mapmaker_limits.inc` (it uses `mapmaker_limits`, `MAPMAKER_PTR_OK`,
`guest_call`). It calls `sub_00079D60` (the existing wrapper dispatcher;
its PC hook acts only on its own call sites), `sub_0007D4F0` and
`sub_00078560` (its own dispatcher; the hook acts only on return address
`0x00313888`); link stubs for these are in `tests/mapmaker/mapmaker_area_stubs.h`.
The two probe hooks are in `pc_mapmaker_probe.inc` and are inert unless
`TSFP_MAPMAKER_PROBE` has flag 128; `sub_0008F950` runs for every model draw,
so its hook only tests a counter unless a capture window is open.
`sub_00085710` is not wrapped by any other area. `pc_mapmaker_sky_room_mask`
is in `src/interface/pc_mapmaker_skymask.inc` (included after `pc_mapmaker_sky.inc`;
prototype in `pc_display_menu.h`, pass-through stub in the non-display branch
of `pc_menu_mouse_guest.inc`); its summary is printed by
`pc_mapmaker_sky_room_mask_flush()`, called once per frame from
`pc_display_menu_mouse_mode` beside the probe's heap/pool ticks. It calls its
original exactly once (no `MAPMAKER_GUEST_RET`).

## Room mask test switch (`TSFP_MAPMAKER_ROOM_MASK_CLEAR`, dev only)

Purpose: one play session that shows which node-mask bit removes a tile's
ceiling (research report `logs/limits/features/sky-tile.md`, sections 2 and 6).

- **Mechanism (CONFIRMED).** A tile room is one model; `sub_0008F5E0` skips a
  node's mesh when its mask `+0x5C` is non-zero and shares no bit with the
  instance mask `inst+0x30` (`0x0008F637..0x0008F64D`). `inst+0x30` is copied
  from the per-room dword table (stock `0x00703460`, `rooms.json` moves it to
  `0x008C0800`, 256 entries) at `0x00085DCA..0x00085DD0` in `sub_00085CC0`.
  A decoder scan of the stock range (`tools/analysis/guest-refs.py 0x703460 0x31C`,
  repeated by `tests/mapmaker/test_mapmaker_skymask.py`) finds four references: the only
  store `0x000857A3` in `sub_00085710` (`table[build+0x28 + j] = build+0x44`
  for `j < build+0x14`), the clear `0x0008A79B` at the start of `sub_0008A780`,
  the read `0x00085DCA`, and the getter `0x00083974` (`sub_00083970`: camera
  `0x000CAB14`, weather `0x003381DC`). `sub_0008A780` calls `sub_00085710`
  once per piece (`0x0008AC71`, cdecl, one argument, plain `ret`) before it
  creates the room props (`sub_000895F0` at `0x0008BAAD`).
- **Hook.** `WRAP sub_00085710 -> pc_mapmaker_sky_room_mask`: after the
  original store, in scene 100 only, the bits of the switch are cleared in the
  table entries of that piece's rooms. The draw mask, the camera's bit-25 test
  and the weather test then all see the same value. Nothing is written in the
  build record, the editor or the saved map; other scenes run the stock code
  and log `room build in scene N ignored` once. Independent of
  `TSFP_MAPMAKER_LIMITS`; default off; not for release.
- **Value.** Hex, with or without `0x` (`2000000` = bit 25). Anything that is
  not a 32-bit hex number is refused with a message; `0` is off.
- **Log (stderr, `game.err`).** At start: `[mapmaker] room mask: dev switch
  TSFP_MAPMAKER_ROOM_MASK_CLEAR=0x02000000 (MapMaker play only)`. After each map
  build (next frame): `[mapmaker] room mask: cleared 0x02000000 on N rooms
  (build K: R rooms in P pieces)`, then up to 64 lines `room <id> <old> -> <new>`
  (room id = table index + 1, the probe's `room=`; model `0x908 + room - 1`), a
  count of further rooms, and any rooms beyond the 256-entry table (left
  unchanged).
- **Side effects to expect (INFERRED).** Clearing bit 25 marks rooms as open to
  the sky for the weather test (`sub_003381D0`), the camera (`0x000CAB1C`) and,
  on tileset 2, `sub_0010EC20` (`0x00085DFA..0x00085E26`). If collision follows
  the mask, ceilings may lose collision as well.

## Node dump (probe flag 128)

Once per run for each distinct `{geometry model, node table [inst+4]}` seen in
a capture window, the probe writes the node tree that `sub_0008F5E0` walks:

    nodes model=0x908 room=1 inst=0x… inst+30=0x020001C7 count=N always=A hidden-now=H masks=0x…
     node model=0x908 index=I type(+48)=T child(+4B)=C sibling(+4C)=S mask(+5C)=0x… always|drawn|hidden

The header `[inst+4]` holds the count `n`; node `i` (0xC8 bytes) is at
`header - n*0xC8 + i*0xC8` (`0x0008F5E6..0x0008F637`, CONFIRMED). `drawn` /
`hidden` is evaluated against the instance mask at that moment (so it already
reflects `TSFP_MAPMAKER_ROOM_MASK_CLEAR`); `masks` is the OR of all node masks.
Bounds: 96 node lines per model (`nodes truncated ...`), 256 models and 8192
lines per run (`nodes cap reached` once), plus the probe's 4 MiB file cap. The
dump is read-only (fixture checksum).

## Probe for the sky tile (flag 128)

`sub_000A2900(room, pass)` draws a room's props: count
`[0x007B54E0+room*4]`, list `[0x007B5F00+room*4]` (relocated to `0x008C7000`
/ `0x008C7C00` by `rooms.json`). It skips a prop when `+0x34` is a real room and
`+0x28 & 0x800000` (`0x000A2945`), or when the instance `[prop+4]` has byte
`+0x1F6` bit 3 (`0x000A2951`); flags 0x200000 send it to `sub_000A1B80`;
otherwise it sets the view bit in `+0x0C` and, in the second loop, draws layers
1..3 with `sub_0008F950(instance, pass, layer)` (`0x000A2AC5`). Prop `+0x94` is
the model id (`0x000A6F4F`); room geometry props are created by `sub_00085CC0`
with model `0x907 + room` and `+0x34 = room` (`0x00085D6B`, `0x00085D9F`).
These are CONFIRMED statically; the probe shows the values in play so the
"hidden" bit for feature (d) can be chosen. Capture steps: "Room draw capture"
in `docs/mapmaker/mapmaker-meter-probe.md`.

## Open-air tiles (`TSFP_MAPMAKER_LIMITS=1`)

User request: a tile that looks like the overlap glitch (floor only, no roof,
no walls, sky visible). It began as two tile-8 palette presets ("46. Small
Open (No Roof)", "47. Small Open (Open Air)"); since the tile roof menu
(below) flags any tile, the presets were removed (user, 2026-11-07: "Remove
two tiles I had you create as bounded and unbounded. They are not needed now
that we have the tile flag."). Files: `src/interface/pc_mapmaker_roof.inc`, `tests/mapmaker/mapmaker_roof.c`,
`tests/mapmaker/test_mapmaker_roof.py`. Built and played since v0.4.0-mapmaker.7; the bounded/unbounded split is not built or played yet.

### Mask bits (CONFIRMED)

The mask is the piece record's `+8` (play reads it at `0x0008A91D`; it reaches
`inst+0x30` through `sub_00085710` / `0x00085DD0`; nodes are skipped in
`sub_0008F5E0` at `0x0008F64A`). The isolated-tile mask `sub_00311C00` and
the neighbour mask `sub_002FC680` build it from the catalogue connection
points (`cat+0x44` count, 6-byte entries at `+0x48`: byte 0 type, byte 3 bank):

- type 5 (up) -> `0x02000000` (`0x00311C2D`, `0x002FD693`, `0x002FD795`):
  closed top; clearing it removed the ceiling in game (dev switch, user test);
- type 6 (down) -> `0x01000000` (`0x00311C3C`, `0x002FD76C`): floor, kept;
- types 1..4 (the four sides) -> `1 << (bank*12 + side*3 + slot)`
  (`0x00311C67..0x00311CA0`, `0x002FC95E`, `0x002FCB1C`, `0x002FD20D`,
  `0x002FD302`, `0x002FD530`): bits 0..23 are the wall and doorway nodes;
- `0x04000000..0x80000000` from attached decorations (`0x002FD8F4..
  0x002FD917`), kept.

Open roof clears `0x02000000`; open air clears `0x02FFFFFF` (ceiling and all
side nodes). Which of the two banks is the wall and which the doorway frame
is INFERRED (both are cleared); a node with mask 0 is always drawn, so a
doorframe or pillar modelled that way would stay. A side bit is
`1 << (layer*12 + side offset + position along the side)` (the
`0x00311C67..` cases add the connection point's x or z to the side's base), so
a set bit is a wall segment where no neighbour connects (INFERRED from stock
doorways being open where tiles meet).

### Open air (bounded) and (unbounded) (user request 2026-11-01)

"Split the open air tile flag into two separate flags with different
mechanics: Open air (bounded) [invisible walls keep you from falling off the
edge] and open air (unbounded) [falling off the edge is possible and kills the
player.]"

| mode | mark `+0x26` | saved `+8` (draw and collision) | editor 3D view | play draw |
|---|---|---|---|---|
| No roof | `0x5201` | without `0x02000000` | as saved | as saved |
| Open air (unbounded) | `0x5202` | without `0x02FFFFFF` | as saved | as saved; fall kill height |
| Open air (bounded) | `0x5203` (new) | without `0x02000000` (walls kept) | without `0x02FFFFFF` | side nodes `0x00FFFFFF` left out |

- **Old maps:** `0x5202` keeps its meaning, so every open-air tile saved
  before the split loads and plays as **Open air (unbounded)** (its old
  behaviour, now with the fall kill). Every mode is set from the tile roof
  menu (any tile, tile 8 included). An older build
  ignores mark `0x5203` (unknown mode) and a stock game plays the record's
  `+8`, so a bounded tile shows there as No roof with visible walls.
- **Bounded, draw only (CONFIRMED statically, not played).** Saving the No
  roof mask keeps the wall bits in the per-room table and in `inst+0x30`, so
  the node-mask tests of the geometry queries (`0x00166C77` etc., below) still
  find the walls. The draw tests the same `inst+0x30` per node in
  `sub_0008F5E0` (`0x0008F64A`), and every call of `sub_0008F5E0` is in
  `sub_0008F6B0` itself or in its recursion (`0x0008F853..0x0008F8BC`). The
  hook on `sub_0008F6B0(inst, pass, layer)` clears `0x00FFFFFF` of a bounded
  room's geometry instance for that call and restores it after. A room's
  geometry is the prop `[inst]` with `+0x34` = room (1-based), `+4` = inst and
  model `+0x94 = 0x907 + room` (`sub_00085CC0` `0x00085D6B..0x00085DD0`);
  other props of the room are untouched. The model id is a **16-bit** word
  (`0x000A69E3` stores `word [ebp+0x94]`, `0x000A6F4F` reads it with
  `movzx`; `+0x96` is another word, `0x000A6897`).
- **Playtest v0.4.0-mapmaker.25 (2026-11-01): bounded tiles showed their
  walls, unbounded falls did not kill.** Cause found for the walls
  (CONFIRMED in code): the draw hook compared the whole dword at `+0x94`
  with `0x907 + room`, so with any non-zero `+0x96` it never recognised the
  room geometry and drew everything. It now compares the low 16 bits. For
  the missing death no single static cause was proven; the fix removes the
  fragile parts and logs every step (below): the record walk before the
  build no longer has to end cleanly (it keeps the records it read), the
  per-entry mask comparison only logs, and a second fall rule kills a
  character that stays a floor below the lowest floor for 60 ticks, in case
  a fall outside every room slows or stops before the kill height.
  The .25 log (`logs/mapmaker25-bossmap.err`, ignored) has `1 bounded
  open-air room` and `1 unbounded open-air tile; characters below y -20.0
  (lowest floor 0) die` for every level, so the records, marks and room
  lookup work, and no `fell off the map` line, so no kill was attempted.
  Room geometry is drawn through `sub_000A2900` -> `sub_0008F950` ->
  `sub_0008F6B0` (the probe's node dump sees models `0x908..` there and
  reads `+0x94` as 16 bits too); the other node-mask tests on `inst+0x30`
  (`0x00091C90`, `0x00092606`, `0x000931AC`, `0x00093E27`, `0x00094B3F`,
  `0x000A31D7`, `0x000A5112`, `0x000A532E`, `0x000A557E`, `0x00112A68`,
  `0x00129CB3`, `0x0016BA20`, `0x00243C7B`, `0x0034A122`, ...) are reached
  from the collision module (`sub_0016D380`, `sub_0016CCD0`, `sub_00175510`)
  and from prop ray tests (`sub_000A7A00`, `sub_000A2CD0`), INFERRED not
  draws; so the 16-bit compare is the wall fix. For the death, the next
  test's logs show the fall itself: human players are now also taken from
  their player structs (`[0x007B0C48] + i*0x1584`, entity `+0x14` as
  `sub_000D5EB0` reads it at `0x000D5EB5`, `+4 < 0` empty, count
  `[0x00790950]`), in case they are not in the live list, and every living
  character below the lowest floor's height is logged once a second with
  its y, the change over that second and its health, and the health after
  the kill call.
- **Playtest v0.4.0-mapmaker.26 (2026-11-01): walls fixed, falls still do
  not kill.** Static re-check (CONFIRMED in code, decoder test): the check
  reads the right fields: the human's entity is player struct `+0x14` (the
  stock per-player update reads it the same way at `0x000D5EB5`), y is its
  live position `+0x68` and y is up (scene-61 kill `-100.0 > y` at
  `0x0015F1B3..0x0015F1BF`; fall damage = fall start `actor+0xB70` - landing
  floor `actor+0xBFC`, `0x000D4441..0x000D4449`); and `sub_000BD090` kills a
  human player in MapMaker play: the stock player hazard kill calls it for
  the current player in scene 100 (`0x000D672B..0x000D673E`). What a height
  rule cannot rely on: the character's floor probe `sub_000D9130` asks
  `sub_000C3B10` for the floor in the character's own room only (`+0x34`,
  `0x000D9149`; the room's search honours `inst+0x30`, `0x000C3B6C`) and
  writes `actor+0xBFC` only when it finds one (`0x000D91A2..0x000D91AA`), so
  outside the map the character keeps its last room and floor height. Whether
  the controller then stops the fall at that height or lets y drop is not
  traced (INFERRED either way); a fall that stops never reaches the height
  rules. So a third rule does not use the height: every 6 ticks the
  character's body box (x, z +-0.3, y +0.25..+1.5) is passed to the room
  query `sub_00087310(box, out, 8)` (the camera's room search, tree
  `0x00702E00`, reimplemented by the rooms area); 8 queries in a row with no
  room (about 0.8 s) kill. Inside any tile, open air included, the body is
  inside that tile's room box (INFERRED from the rooms being the tiles'
  volumes; the log shows the count).
- **Which rooms are bounded (CONFIRMED).** The play build `sub_0008A780`
  makes one 0x68-byte entry per piece record, in record order
  (`sub_00083F80`/`sub_00085030` at `0x0008AACE`/`0x0008AAAA`), then calls
  `sub_00085710(entry)` for each in order (`0x0008AC70..0x0008ACAB`, `edi +=
  0x68`). The build hook reads the records (marks, `+8`, floor byte `+6`)
  before the original (a walk that fails part way keeps the records read so
  far); the rooms hook takes entry k, compares its `+0x44` with record k's
  `+8` (`0x0008A920` copy `+0x14` -> `+0x44` at `0x00084F98` /
  `0x000856EB`; a difference is only logged, since the order is fixed by the
  code) and records rooms `+0x28 .. +0x28 + +0x14 - 1` of a bounded piece.
- **Unbounded, falling off kills (CONFIRMED that stock has no void death).**
  The stock kills all need something a void fall lacks: the hazard-floor
  kills need a ground polygon (`sub_000D5EB0` `0x000D6661..0x000D673E`
  players, `sub_0015F0D0` `0x0015F161..` AI), fall damage needs a landing
  (`sub_000D4100` `0x000D44E0..`: drop > 15.0, health -= (drop - 15) * 10),
  the only height kill (y < -100.0, `0x0015F19B..0x0015F1C2`) runs only in
  scene 61, and the remaining height kills belong to story level scripts
  (`sub_001F48A0`, `sub_001E2B80`). That nothing else exists is INFERRED from
  a search of the kill helpers' callers and the floor fields. So a character
  that walked off an unbounded tile at the map edge would fall forever.
  On a map with at least one unbounded tile the tick hook kills every live
  character whose y (entity `+0x68`, y is up; the stock hazard kill compares
  it with the floor height at `0x000D66A3`) is below **5 x lowest floor -
  20** (piece y = record byte `+6` * 5.0 `[0x004760CC]`, `0x0008A87E`,
  `0x0008A8F0`; editor floors -2..2 give a kill height of -30 at most), or
  stays below **5 x lowest floor - 5** (one floor under the map) for **60
  ticks** in a row, or overlaps **no room** for 8 room queries in a row
  (every 6 ticks, above). It
  walks the live list (`0x007A3580`, relocated to `0x00900100` by
  `actors-list.json`, count `[0x0074C37C]`, entity = `[actor]`, entity `+0x7C`
  = actor) and calls the stock kill-now helper `sub_000BD090(entity)` for a
  character with health `+0x1CC` > 0, as the stock loop at
  `0x00067AF1..0x00067B78` does; human players are also read from their
  player structs (above) and checked once. `sub_000BD090` builds a hit with no attacker
  and calls the death `sub_000BABA0`: it is scored as a suicide (INFERRED from
  `0x000BAD24..0x000BB002`), with the normal death and respawn. Humans and
  bots both die. Maps without an unbounded tile, story levels and the editor
  are untouched (the flag is reset at every MapMaker play build).
- Cost: the draw hook runs for every drawn instance; with no bounded room, or
  outside scene 100, it is one counter test and one scene read; otherwise a
  few loads and a 256-entry table. The tick hook is one flag test per frame
  unless the map has an unbounded tile, then a walk of at most 64 characters
  and, per character every 6 ticks, one room query (a tree walk, as the
  camera does every frame). The query box and result live in 56 bytes from
  `xbox_HeapAlloc`, allocated once; without them the room rule is off.
- Log (stderr, `game.err`), so a playtest shows which step fails:
  - every MapMaker play build: `roof: play build: P pieces (walk W), B
    bounded, U unbounded, floors L..H` (walk -1: the records did not end
    cleanly; `roof: map records end unexpectedly after P pieces`);
  - bounded: `roof: bounded piece K: rooms A..B (mask M)` per bounded piece,
    `roof: N bounded open-air rooms (walls hidden, collision kept; K of P
    pieces checked)`, then at the first draw of each bounded room `roof:
    room R drawn bounded: H of N nodes hidden (instance mask M)`. Draws in a
    bounded room that are not its geometry are logged six times (`not the
    room geometry`, with the prop, instance and model id); a mask difference
    as `room build entry K mask ..., its record's +8 ...`;
  - unbounded: `roof: fall check on: characters die below y K, or below y O
    for 60 ticks (lowest floor F)`; at the first tick `roof: fall check
    running: N live characters, H human players, player 0 at y Y health
    H`; for the first 10 seconds `roof: player 0 at x X y Y z Z health H,
    rooms R` once a second (the standing height and the room count inside
    the map; R 0 there would mean the room rule cannot work); `roof:
    character S is outside every room (x y z)`; `roof: character S is below
    the lowest floor (y Y)`; once a second for every living character under
    the lowest floor's height or outside every room `roof: character S off
    the floors: y Y (+D in 1 s) health H, rooms R, T ticks below y O` (a fall
    that stops shows D near 0); and `roof: character S fell off the map
    (outside every room / below the kill height / a floor under the map; x
    y z); killed, health now H` (H not 0: the kill call did not take). S is
    the live-list index, or 64 + the human player index.
  No line `play build` at all means the build hook did not run (limits off,
  or the wrapper is missing); no `fall check running` means the tick hook
  did not run in scene 100.
- Restart from the pause menu (audit after .31, the bosses' restart bug):
  the play build `sub_0008A780` runs only on a fresh load (`sub_001CDE30`),
  not on "START restart" (`sub_001CDD60`), so no `play build` line follows a
  restart. Nothing needs it: the build's rooms, open-room marks, kill heights
  and the open-view groups (`pc_mapmaker_openview.inc`) describe the map's
  geometry, which a restart keeps. The fall check's per-character counters
  carry over but clear on a restarted character's first tick above the floor
  (`out`) or first query inside a room (`noroom`); a dead one's on its death.

### Palette presets (removed 2026-11-07)

The two presets were stock tile 8 with a mark: the palette's last category
got two more tile-8 items while the menu was built (hooks on `sub_00316B50`,
`sub_000792B0`, `sub_00316920`, `sub_00316500`), and their names and numbers
46/47 came from hooks on `sub_0003D7B0` and `sub_0032D9F0` (and the first
branch of the `sub_00151030` hook). Those six hooks are gone; the palette,
tile names and caption are stock again. Why no new tile id was possible
(still true): every tile id indexes fixed 45-entry tables (`0x00783F40`,
`0x00785000`, `0x00789440`, `0x00785080 + (tileset*45 + cat[+0x11])*64`, the
catalogue `0x00529510`, the play builder `sub_0008A780`).

**Old maps (CONFIRMED by the fixture).** A map that used a preset holds tile
8 with record word `+0x26` = `0x5201` (46, No roof) or `0x5202` (47, Open
air, now Open air (unbounded)) and the open `+8`, the same as a tile flagged
from the roof menu. The load hook puts the mark back on the piece, so it
loads as **1. Small Open** with its flag: the grid label reads "1.Small Open
(No Roof)" / "1.Small Open (Open Air, Unbounded)", the right-click menu shows
the flag and can change it, and play is unchanged. No conversion is needed.

### Storage and persistence

- Editor side: a host table keyed by editor piece pointer (pieces are 0x48
  bytes from one pool `[0x00762CC4]`; constructor `sub_00307FD0`, callers
  `0x003021C1` loader and `0x0031658E` palette; the hook forgets a reused
  pointer; the map reset `sub_002FEBF0` clears the table).
- **Placing copies the piece (the v0.4.0-mapmaker.5 bug, CONFIRMED).** The
  pick marks the held piece `editor+0x2F7C`, but placing it
  (`sub_003037A0`) allocates a new piece (`sub_00307EC0`, `0x00303862`),
  copies the 0x48 bytes (`rep movsd`, ecx 0x12, `0x0030387C`), links the copy
  (`sub_00307EF0`) and frees the held list (`sub_00308560`, `0x0030398A`).
  Picking up a placed piece does the same the other way
  (`0x002F9A3E`/`0x002F9BE9`/`0x002F9C8F`). The first build marked only the
  held pointer, so every placed tile was an unmarked tile 8 and the saved
  `+8` was the stock mask: the open tiles played as plain Small Open. Fix:
  each copy is followed by `sub_002F95E0(old, new)` (move: placing,
  `0x0030387E`; pick-up-to-move, `0x002F9B7F`, `0x002F9C36`, `0x002F9CB2`)
  or `sub_00308120(src, dst)` (copy: `0x002F9BB6`, `0x002F9C66`,
  `0x002F9CC7`); both hooks pass the mark to the destination.
  `sub_00308480` (frees one piece; `sub_00308560` frees a list through it)
  forgets the mark, so a pooled pointer starts closed.
- `sub_002FC680` (writer `0x00300CA4`/`0x00300CB5`, editor 3D view
  `0x0032FEBF`) returns the open mask for marked pieces; `sub_002FC470`
  drops `0x02000000`, so the writer stores record `+0x23` = 1 ("exposed",
  `0x00300C2F..0x00300C93`) when nothing is above.
- Saved: after each rebuild (`sub_003006C0`, also for the undo shadow and
  in any scene, as the marks exist only for editor pieces) the
  hook walks the piece records with the editor list (one record per list
  piece, same order, checked by tile id; a mismatch is logged and nothing is
  stored) and sets record word **`+0x26` = 0x5201 (open roof) / 0x5202
  (open air)** and clears the bits in `+8` again (the catalogue callback at
  `0x00300CD7` ORs into `+8`; tile 8 has none). Proof that `+0x26/+0x27` is
  free (decoder, `test_piece_record_word_0x26_unused`): no store with
  displacement 0x26/0x27 in the piece writer `0x00300B74..0x00301174`, no
  read in the loader's piece case `0x0030217A..0x00302428`, none in any
  function that walks piece chunks (`sub_0023F390`, `sub_0023FD10`,
  `sub_00240590`, `sub_00240C50`, `sub_0032A650`, `sub_0032A6D0`,
  `sub_0032A770`, `sub_0008A780`, `sub_00085920`); the compiler zeroes the
  container first (`0x0030070D`); `+0x24` (size word, `0x00300D76`) is
  preserved.
- **Record walk (the v0.4.0-mapmaker.7 bug, CONFIRMED).** `+0x24` is only
  `0x28 + pads*8` (`0x00300D60`); the piece's items (word `+0x1E` of them,
  each `(byte +0x14 * 4 + 0x1B) & ~3` bytes: writer `0x00300D93..0x0030116E`,
  loader `0x00302260..0x003023F8`) follow the pads. The first build stepped
  by `+0x24` alone, so the walk lost its place at the first piece carrying a
  start point or weapon and stored no `+0x26` mark after it. Play was still
  open (the writer's own `sub_002FC680` call is hooked), but returning to the
  editor reloads the map (editor start `sub_00302A60`: reset `0x0030302B`,
  then `sub_00301A90([0x00784014], 2)` at `0x00303076`; the editor heap and
  piece pool are new, so piece pointers do not survive a play) and found no
  marks; the next rebuild (copying a tile, playing again) closed every tile.
  The walk now skips the items too, so the marks survive the round trip.
- Load (`sub_00301A90`: the editor start after a play, Load map
  `0x0032B49F`/`0x0032BB69` and the undo/redo reloads in `sub_00337600`,
  whose snapshots are rebuilds and so carry the marks): each marked record is
  matched to an unmarked editor piece with the same tile, x, y, z and
  rotation; an unmatched mark is logged (`[mapmaker] roof: an open tile in
  the map has no loaded piece`).
- Play: `sub_0008A780` takes record `+8` unchanged (`0x0008A91D`) and calls
  none of the editor mask builders, so the saved open mask is what the
  per-room table (`sub_00085710`) and `inst+0x30` get; no play hook.
- Stock game / switch off: the map is a normal map (tile 8, version 0x12D) and
  plays with the open mask (play only reads `+8`); re-saving it in a stock
  editor or with the switch off recomputes `+8` and drops `+0x26`, so the tile
  becomes a closed tile 8.

### Editor preview

The editor's 3D view draws each placed piece with `sub_002FC680(piece, 0) &
~0x02000000` (`0x0032FEBF..0x0032FEEF`): the stock editor never shows roofs,
so open roof looks the same as a normal tile there, and open air shows its
walls removed. The held piece before placement uses the palette preview mask
(hook above); whether the held piece is redrawn through `sub_002FC680` is
INFERRED.

### Collision, map edge, weather

- Collision: the node-walking geometry queries `sub_00166B90`,
  `sub_00166E70`, `sub_001671E0`, `sub_0016D380` (callers in the
  `0x0016xxxx`/`0x00175510` module) skip nodes with the same test
  (`test [inst+0x30], mask` at `0x00166C77`, `0x00166F5B`, `0x001672CB`,
  `0x0016D47A`, `0x0016D7DB`, CONFIRMED; that they are the collision/ray
  tests is INFERRED from node `+0x58` counts and from stock doorways being
  walkable). Expect hidden ceilings and walls to have no collision: rockets
  and grenades leave through the top, players can walk out of an unbounded
  open-air tile's sides into whatever is next to it. Bounded tiles keep the
  wall bits in the mask, so their walls keep this collision while the draw
  hook hides them (shots hit the invisible walls too).
- Map edge: no boundary follows the mask, and the stock game has no death
  for a fall into the void (see "Open air (bounded) and (unbounded)"). Open
  air (unbounded) adds a kill height below the lowest floor; Open air
  (bounded) keeps the walls' collision, so nobody leaves the tile through a
  side that has no neighbour.
- Weather: rooms without bit 25 are open for `sub_003381D0` (`0x003381E1`,
  via the getter `sub_00083970`) and the camera (`0x000CAB1C`); with
  `+0x23` = 1 as well, rain/snow is expected inside (desired; pads with pad
  flag 0x20 stay dry, `0x00119B71`). Tileset 2 also calls `sub_0010EC20` for
  such rooms (`0x00085DFA..0x00085E26`, role unknown).
- Stacking: not refused. A piece above an open tile is drawn as usual; you
  see its underside through the missing ceiling. The neighbour logic of the
  piece below consults `sub_002FC470` of the open piece (`0x002FD6E6`), so the
  lower piece may open its own top (INFERRED).

### Not done

- Per-map "Open all roofs" checkbox: skipped (the tile roof menu covers the
  request; a map-wide flag would need another info bit and a play-side hook).
- English-only flag names in the grid label.

### Wrappers (area `sky`)

    WRAP sub_002FC680 -> pc_mapmaker_roof_mask
    WRAP sub_002FC470 -> pc_mapmaker_roof_reduced
    WRAP sub_00307FD0 -> pc_mapmaker_roof_piece_new
    WRAP sub_002FEBF0 -> pc_mapmaker_roof_reset
    WRAP sub_003006C0 -> pc_mapmaker_roof_compile
    WRAP sub_00301A90 -> pc_mapmaker_roof_load
    WRAP sub_002F95E0 -> pc_mapmaker_roof_move
    WRAP sub_00308120 -> pc_mapmaker_roof_copy
    WRAP sub_00308480 -> pc_mapmaker_roof_piece_free
    WRAP sub_00151030 -> pc_mapmaker_roof_format
    WRAP sub_0008A780 -> pc_mapmaker_roof_play
    WRAP sub_00085710 -> pc_mapmaker_roof_rooms
    WRAP sub_0008F6B0 -> pc_mapmaker_roof_draw
    WRAP sub_0003C440 -> pc_mapmaker_roof_tick

Removed on 2026-11-07 (the integrator drops their lines and regenerates):
`sub_00316B50 -> pc_mapmaker_roof_menu_init`, `sub_000792B0 ->
pc_mapmaker_roof_menu_add`, `sub_00316920 -> pc_mapmaker_roof_menu_hover`,
`sub_00316500 -> pc_mapmaker_roof_menu_pick`, the roof layer of
`sub_0003D7B0` (`pc_mapmaker_roof_text`; realbosses and weaponset keep
theirs) and `sub_0032D9F0 -> pc_mapmaker_roof_caption` (no other layer:
the wrapper goes). Prototypes and stubs are gone too.

Every remaining hook calls its original exactly once (no
`MAPMAKER_GUEST_RET`). `sub_00151030` (sprintf into `0x00749368`, 204
callers) only swaps the name argument for one return address and tests a
counter first. Chains:
`sub_003006C0` (container, aitemplates, sky compile) and `sub_00301A90`
(editor, aitemplates) and `sub_002FEBF0` (sky reset) already have layers; the
roof hooks only act after the original returns, so any layer order works
(outermost is simplest). Include
`pc_mapmaker_roof.inc` after `pc_mapmaker_skymask.inc`; it uses
`mapmaker_limits`, `MAPMAKER_PTR_OK`/`MAPMAKER_STACK_OK`, `WORD_AT`,
`FLOAT_AT`, `xbox_HeapAlloc`, `g_xbox_mem_offset` and `guest_call`. It calls
one guest function, `sub_000BD090` (declared `extern void
sub_000BD090(void);` in the file; link stub in
`tests/mapmaker/mapmaker_area_stubs.h`).

The four play hooks (bounded/unbounded split) each call their original
exactly once (no `MAPMAKER_GUEST_RET`) and need prototypes beside the other
roof hooks in `pc_display_menu.h` and pass-through stubs in the non-display
branch of `pc_menu_mouse_guest.inc` (not edited here). Chaining:
`sub_0008A780` is already wrapped by the rooms area
(`pc_mapmaker_rooms_compile`); the roof hook only reads the container before
the original and logs after, so any order works (outermost is simplest).
`sub_00085710` is already wrapped by the sky area
(`pc_mapmaker_sky_room_mask`, dev switch); the roof hook reads the entry and
acts after the original, any order works. `sub_0008F6B0` (6 callers:
`0x0008FA0C`, `0x0008FA2E` in `sub_0008F950`, `0x00185C73`, `0x00185C87`,
`0x00185D91`, `0x00185DA5`) and `sub_0003C440` (one caller, `0x0003D3CA`)
are new targets. No guest-code table entry is added.

### What the user tests

1. Build with the wrappers; `TSFP_MAPMAKER_LIMITS=1`.
2. Editor, any tileset: the tile list's last category ends with its stock
   tiles (no 46/47 entries); every caption keeps its stock number.
3. Load a map saved with a 46/47 tile: it is a "1.Small Open" with "(No
   Roof)" / "(Open Air, Unbounded)" in its grid label; right-click it: the
   menu shows that flag. Play: open as before. Move and copy it: it stays
   flagged. Undo/redo once.
4. Flag tiles from the right-click menu (below), put a start point on a tile
   placed before them, save, back to the main menu, load: still flagged (no
   `roof:` warning). Play: look up and around (sky, neighbours' walls over
   the edge), weather on, 2-4 player split screen. Quit to the editor: the
   tiles keep their flags; copy one and play again: still open.
5. Walk through the open sides into neighbouring tiles; at the map edge walk
   out; fire a rocket straight up from inside; let AI path through.
6. Load the map without the switch: it still plays open; re-saving it there
   makes the tiles normal (expected).
7. Story smoke test: one indoor and one outdoor level unchanged.

## Tile roof menu (right click, `TSFP_MAPMAKER_LIMITS=1`)

User request (2026-11-01): "right clicking any tile, instead of selecting it
(only when the tiles option is active in the main mapmaker menu) opens up a
menu to flag the tile either as no roof or open air, probably as options in a
combobox". Files: `src/interface/pc_mapmaker_tileflags.inc` (area `sky`, included
after the roof file), the right-click branch in `src/interface/pc_mapmaker_pointer.inc`
and the hover label in `pc_mapmaker_roof.inc`; tests
`tests/mapmaker/test_mapmaker_tileflags.py` (fixture `mapmaker_tileflags.c`, 35
mutations, decode/XBE checks). Built and played (v0.4.0-mapmaker.18) before
the bounded/unbounded split; the split is not built or played yet.

The flags are the roof marks above. Nothing in them is tile-8 specific: the
mask hook clears the ceiling bit (No roof) or the ceiling and every side node
bit (Open air (unbounded); Open air (bounded) saves the walls and hides them
only from the draw) of whatever piece is marked, the rebuild stores word `+0x26`
for any record, and the load matches tile, position and rotation. A tile
whose model has no separate ceiling or wall nodes (mask 0) keeps those parts:
the flag changes only what the catalogue's connection points describe.

Stock behaviour (CONFIRMED, decode): the editor's frame input
`sub_003063C0` dispatches on the toolbar slot `editor+0x254` (table
`0x00306444`); slot 0, Tiles, is `sub_00303E10` (its only caller,
`0x003063D9`). Back (menu `+0xC4 & 0x20`, what a right click sends) rotates a
held piece (`+0x2F7C`, `sub_00303D00(1)` at `0x003042A8`), otherwise it flips
selection bit 2 of the piece under the pointer (`+0x2F18`, byte `+0x14`,
count `+0x2F9C`, `0x003042E4..0x0030431E`). In Objects mode Back on a
selected object opens its options menu: `sub_002FB230(desc)` (`0x003051DB`;
sets `editor+0x25C |= 0x4000000`, pushes `sub_00079860(root, desc, -1)` at
`0x002FB2A5`; menu `+4` = desc, `0x000798B4`). Slot 0's toolbar row
(`0x0052CD78`) has +8 = -1 and no open/close callbacks, so the close
`sub_002FB3C0` only clears the flag. The pickup options descriptor
`0x0052DECC` is `{sub_00319300 build, sub_003192E0 tick, 0, 0}`.

| piece | what it does |
|---|---|
| pointer right-click release (no drag, editor screen) | after the empty-cell Y case: `pc_mapmaker_tileflags_click()` takes it in Tiles mode with nothing held, a piece under the pointer and no editor menu open (remembers the piece, sends no Back); otherwise Back as before |
| `sub_00303E10` -> `pc_mapmaker_tileflags_tiles` | with a remembered piece still under the pointer: allocates once a heap descriptor `{sub_00319300, sub_003192E0, 0, 0}`, a choice word and the texts, sets the word to the tile's current flag, opens the menu with `sub_002FB230`, and skips that frame's tile input (`MAPMAKER_GUEST_RET`) |
| `sub_00319300` -> `pc_mapmaker_tileflags_build` | for that descriptor only (menu `+4`): the stock frame (`sub_00075B70(0x8400, 0, 50, 0, sub_002FB3C0)`, `+0x218` = 1, `sub_00071B10(1, 0.5, 1, 1)`, `sub_00071C50(0x20000)`), one choice row "Roof" built like the weaponset area's pickup Weapon choice (`sub_00079DE0(text, 1, 0x08000800, 0, 0)`, `sub_0007D4A0`, `sub_00071800` Normal 0 / No roof 1 / Open air (bounded) 2 / Open air (unbounded) 3, `sub_00078590(word)`, `sub_000783D0`), the stock Done row (text 0xB5E, `sub_00079D60` with `sub_002FB3C0`), `sub_00075E70` and the stock position call; the pickup menu itself is the original |
| `sub_003192E0` -> `pc_mapmaker_tileflags_tick` | for that descriptor: when the word changed to row 0..3, the piece (still the same tile id) gets that row's roof mode (rows 0..3 -> modes 0, 1, 3 bounded, 2 unbounded, `tf_modes`, so saved marks keep their values); if it was selected when the menu opened, every selected piece in the editor list does too; then the original (Back closes). The menu opens on the tile's own row |
| `sub_00151030` (roof format hook) | the grid label of any flagged piece, tile 8 included, reads "<number>.<name> (No Roof)" / "(Open Air, Unbounded)" / "(Open Air, Bounded)" (heap copy of the name) |

Selecting tiles: a mouse right click no longer selects a placed tile in
Tiles mode; Escape (Back in the editor) and the gamepad B button still do. A
right click with a piece held still rotates it; on an empty cell it still
clears the selection. The 46/47 palette presets were removed (2026-11-07);
maps that used them load with the same flags. Undo: a flag change is not its own undo step (the next
rebuild stores it).

What the user tests:
1. Tiles mode, nothing held: right-click a placed tile of any shape (not only
   Small Open). A small menu opens with "Roof: Normal" and Done (log
   `tileflags: roof menu opened for tile N`). Pick No roof, Open air
   (bounded) or Open air (unbounded) with left/right or the mouse, then
   Done: the 3D view shows the change (both Open air choices without walls);
   the hover label ends with "(No Roof)" / "(Open Air, Bounded)" / "(Open
   Air, Unbounded)".
2. Select several tiles (Escape or B), right-click one of them: the choice
   applies to all selected tiles (log `N tiles set to ...`).
3. Save, reload, play: the flagged tiles are open in play; back in the editor
   they keep their flags. Set one back to Normal: closed again.
4. Right click with a piece held still rotates it; in Objects mode a right
   click still opens a selected object's options; Escape still selects tiles.
5. Bounded vs unbounded (log `roof: N bounded open-air rooms`, `roof: N
   unbounded open-air tiles; characters below y Y ... die`): build a row of
   tiles at the map edge, one bounded, one unbounded. In play the bounded
   tile shows no walls but you cannot walk (or be pushed by an explosion)
   off its open edge, and shots stop at the invisible wall; between two
   bounded or open tiles you walk through freely. Walk off the unbounded
   tile: you fall and die a moment later (log `character N fell off the
   map`), scored as a suicide, and respawn. Repeat from an upper floor, with
   a bot (knock it off), in 2-player split screen, and in a MapMaker Story
   map. Look for walls that flicker in reflections/shadows on bounded tiles.
   Load a map saved before this change: its open-air tiles are unbounded.

## Tests

- `tests/mapmaker/test_mapmaker_sky.py`: fixture (24 mutations: extra sky without its pak,
  extra row bounds, extra sky kept and level sky restored; scene gates, limits
  gate, restore, level sky set/restored, template, sky range, tileset default,
  stock-bit collision, read before rebuild, load/new-map/editor-change state,
  checkbox caller/binding/refresh, rebuild container, chunk walk) and the stock
  data the design relies on (sky list, cloud fields, level table, tileset
  defaults, call-site bytes), skipped without the local XBE.
- `tests/mapmaker/test_mapmaker_probe.py`: flag 128 window, deduplication, scene gate,
  read-only check, relocated tables, node dump (once per model and table,
  stride, drawn/hidden, per-model and per-run caps, read-only; 7 more mutations).
- `tests/mapmaker/test_mapmaker_roof.py` (fixture `tests/mapmaker/mapmaker_roof.c`): Open
  roof / Open air flags: limits and scene gates, masks, record
  mark and mask, load matching, forget on reset/reuse, record walk bounds,
  items after the pads, play -> editor reload -> copy -> play round trip,
  grid label flags (stock number, held piece, no memory), two-sided draws
  (bit 0x40000 for the call only, stock bit kept, without bounded rooms),
  Open air (bounded): saved walls, editor view,
  mark 0x5203 load, rooms by build order (with the mask cross-check), draw
  hiding and restore; Open air (unbounded): kill height from the lowest
  floor, live/dead/entry checks, scene gate, 64-entry bound, 16-bit model id, partial record walk, hidden-node count, the
  60-tick out-of-map rule, human players from their player structs, the
  no-room rule (8 queries every 6 ticks, body box, reset, no memory);
  plus XBE data (category tables, descriptor, tile 8, tile numbers 1..45,
  both formats, floor height 5.0) and decoder checks (record word +0x26
  unused, hook and label sites, record sizes, editor start load, node-mask
  tests, draw/room/tick/kill sites, the cull-none flag sites).
- `tests/mapmaker/test_mapmaker_skymask.py` (fixture `tests/mapmaker/mapmaker_skymask.c`): the
  room mask switch: off by default, scene 100 only, after the original, the
  relocated table only, build record untouched, 256-entry bound, one summary per
  build (also when a second build starts before a frame), bad values refused,
  exact log text (15 mutations); plus the decoder check that `0x000857A3` is the
  only store into the table and that `rooms.json` relocates every reference
  (skipped without the private XBE).
- `tests/mapmaker/test_mapmaker_openview.py` (fixture `tests/mapmaker/mapmaker_openview.c`):
  rooms seen across open air (see "Seeing across open air"): open marks for
  bounded/unbounded only, gates, areas by portal, closed neighbours, rooms
  beside by box (margin, NaN/empty boxes), two-sided marks, union, depth,
  cap, rebuild per build, bad tables, log text; decoder
  checks of the walk, its entries and the room list.

## Sky-tile test session (user, dev switch + probe)

Needs a build with `WRAP sub_00085710 -> pc_mapmaker_sky_room_mask` and the two
probe wrappers. Use a small map with a stackable and a plain tile, nothing above
them, default sky; note each tile's name and rotation. For each value below,
start one play session with `TSFP_MAPMAKER_PROBE=128` (log path set) and
`TSFP_MAPMAKER_ROOM_MASK_CLEAR=<value>`, stand in each tile, look straight up,
and note sky / ceiling / missing walls:

1. no switch (baseline; the node dump of every tile model);
2. `2000000` (bit 25, "closed top", expected ceiling);
3. `1000000`, then `8000000` (the other vertical bits);
4. only if none removed a ceiling: `FC000000` (bits 26..31), then `FFF` and
   `FFF000` (the two edge banks; expect walls to vanish).

Send per run: `mapmaker-probe.log`, the `[mapmaker] room mask:` lines of
`game.err`, and the notes (with screenshots of looking up). The node dump of run
1 shows which node masks contain which bit, so later runs can be matched to
nodes.

## What to test in play (user)

1. Build with the wrappers above; `TSFP_MAPMAKER_LIMITS=1`.
2. Editor: open Map Settings; the sky row shows a **Clouds** checkbox after the
   stock checkbox. Pick each of the 11 skies once (stock), tick Clouds, save,
   reload the map from the menu, reopen Map Settings: the box is still ticked.
   New map: the box is clear.
3. Play the map: clouds move across the sky for every sky choice, including
   "-1"/tileset default with each tileset; untick, save, play: no clouds.
   Split screen with 2-4 players.
4. Load a clouds map in a session without `TSFP_MAPMAKER_LIMITS=1`: it loads
   with its sky, no clouds, no crash.
5. Story smoke test after playing a clouds map: one outdoor level (for example
   one using sky 5 or 34) looks as before.
6. Probe: run "Room draw capture" (flag 128) and send the log.

Known limits: clouds are not shown in the editor; a cloud map re-saved with
the switch off loses the flag; the checkbox caption is English only and has no
help text.

## Fall death after the .27 playtest

In v0.4.0-mapmaker.27 a player who walked off an unbounded tile dropped to y
-2.5 outside every room, and the game then put them back on the last floor
height before the 8-query rule could fire. The fall check now also kills at the
first room query that finds no room while the character is more than
`MAPMAKER_ROOF_DROP` (1.5) under the lowest floor. The mansion worm (model
0x5EA) is skipped: it starts and burrows under the floor, and .27 killed it at
its spawn (y -4.5), which left its burst on screen.


## Seeing across open air (v0.4.0-mapmaker.28)

User report: from an Open air tile, the tiles beyond were drawn only inside a
sharp-edged box, with the sky around it. Area `openview`,
`src/interface/pc_mapmaker_openview.inc`, `tests/mapmaker/test_mapmaker_openview.py`
(fixture `tests/mapmaker/mapmaker_openview.c`, 27 mutations, decoder checks).
Not built or played.

Cause (CONFIRMED, decode): the per-view portal walk `sub_00089B90` (tail
jumps at `0x001CE688` from `sub_001CE650`, run per view from the tick at
`0x0003C56A`, and `0x001CE8E3`) resets every room (`sub_000878A0`: depth -1,
empty rectangle) and walks from the camera's room with the viewport
(`sub_00087F80`, `0x00089BD9`). Each portal (room record `[0x00708788]` +
room*0xB4, `+4` -> {count, ids}; portal `[[0x0070878C]+id*4]` `+0`/`+4` = its
two rooms) is projected, clipped to the incoming rectangle and merged into
the neighbour's entry (view `+0x3BC` + 12*room, the `+0x1000` shadow in this
fork: int32 depth, int16 x0, y0, x1, y1). `sub_000897D0` lists rooms with a
depth >= 0 and x1 > x0, and every prop of a room is culled and scissored to
that rectangle (see mapmaker-realbosses.md, "Whole height"). Tiles are joined
by one-floor portals that the open-air mask does not remove, so a neighbour
shows only inside the projected shared side, the next one inside the
intersection of two sides, and a diagonal neighbour or one behind a portal
off screen not at all.

Fix: `pc_mapmaker_openview_walk`, after the stock walk, in scene 100 with
`TSFP_MAPMAKER_LIMITS=1` on a map with open-air rooms (the roof's rooms hook
now marks the rooms of bounded and unbounded pieces open). Once per build the
open rooms are grouped into areas joined by portals; an area's touched set is
its rooms plus every room one portal away (a closed tile beside it, so its
outer walls show). Each frame, for each area with a visible room, the union
of its visible rooms' rectangles (the whole viewport when the camera stands
in it, the doorway's rectangle when it is seen from a closed tile) is merged
into every touched room; a room the walk did not reach gets depth (smallest
visible depth of the area) + 1, capped at 0x40, and view `+0xD28` is raised
to it. Depth testing still hides what is behind drawn walls. Unchanged:
rooms behind a closed tile's doorway, an open area joined to the camera's by
no portal (an island across a gap), closed tiles away from open air, other
scenes and story levels. The room id width, the draw buckets (`sub_00086C40`,
`sub_000897D0`) and the shadow layout are not touched.

Cost: one pass over the area's rooms per view per frame (no guest calls); the
areas are rebuilt only when the play build, room count or portal count
changes. The draw cost is the touched rooms of every visible area, each still
culled by its props' projected boxes. Logs: `[mapmaker] openview: N open-air
rooms in A areas; T rooms drawn across them at most` per build (32 builds) and
`W rooms widened across open air (X not reached by the portal walk)` at most 4
new peaks per build.

    WRAP sub_00089B90 -> pc_mapmaker_openview_walk

cdecl, no arguments, plain `ret`; calls the original exactly once, then
acts. Reached only by the two tail `jmp`s (new target, no other layer).
Include after `pc_mapmaker_roof.inc` (it reads `mapmaker_roof_play`); the
`pc_mapmaker_roof_rooms` change needs no new wrapper. Prototype in
`pc_display_menu.h`, stub in the non-display branch of
`pc_menu_mouse_guest.inc` (both added).

Playtest: on a 3x3 field of Open air tiles stand on a corner tile and look
across: every tile, the diagonal ones too, is drawn whole, nothing boxed; turn
around fully; repeat with bounded tiles; stand inside a closed tile and look
out a doorway at the field (only the doorway shows it, as before); put a
closed building in the field (its outer walls show from every side); 2-4
player split screen; frame rate on a large all-open map; the log lines above.

### Walls across open air (2026-11-07)

User report: "when looking at tiles with walls on the other side of
unbounded tiles, you can see the ceiling and floors of some of the tiles but
not the walls of any."

Cause (CONFIRMED in code; that tile walls have no outward faces is INFERRED
from the stock editor never showing a tile from outside):
- Room geometry is drawn single-sided. The room draw `sub_000A2900` reads
  render state 6 (the cull mode: `sub_00021440`/`sub_00021450` cache, values
  0 / 0x900 / 0x901) and switches the stock 0x900 to 0x901 for mirrored
  rooms (record `+0xB2 & 3`, `0x000A2997..0x000A29C9`), restoring it after
  (`0x000A2AF2..0x000A2B03`). A tile's polygons face into its room, so from
  outside its walls are back faces. Its floor (seen from above) and ceiling
  (seen from below) are front faces from any eye height between them: that
  is exactly "floors and ceilings but no walls".
- Tiles beside an open area with no portal into it (a side without a
  connection point) were not in the openview touched set at all, so some
  tiles showed nothing.

Fix:
- `pc_mapmaker_openview.inc`: the touched set also takes every closed room
  whose box (record `+0x10` min xyz, `+0x1C` max xyz; `sub_000873F0` reads it,
  `sub_00087450` tests a point against it) touches an open room's box within
  0.5 (beside, diagonal, above or below; boxes with NaN, absurd or empty
  extents are skipped). Once per build, no guest calls; at most rooms x open
  rooms box tests. Every touched room and every open room is marked
  `mapmaker_roof_play.twosided`.
- `pc_mapmaker_roof_draw` (the existing `sub_0008F6B0` wrapper): for the
  geometry prop of a two-sided room (same checks as the bounded draw:
  `+4` = inst, 16-bit model `0x907 + room`) it sets prop `+0x28` bit
  0x40000 for the call and clears it after (only if it set it). The stock
  `sub_0008F6B0` then sets cull mode 0 for pass 0 and restores the saved
  mode after the nodes (`0x0008F805..0x0008F822`, `0x0008F8C4..0x0008F8E4`;
  decoder test `test_cull_none_flag_sites`). The bit's other two readers
  (`0x000A6045` in `sub_000A5F40`, `0x000D84E6` in `sub_000D84C0`) are prop
  ray/update code, not reached from the draw. Collision, the node masks and
  bounded walls are unchanged.
- Cost: draws of the touched rooms' geometry lose back-face culling (GPU
  overdraw only); the hook stays one counter test per draw on maps without
  open air.

Not changed: rooms that touch no open room and are reached by no portal from
one (behind a closed tile, or across a gap) stay as the stock walk leaves
them; a wall's outside shows the inside texture mirrored and lit as inside.

Logs: the openview build line now ends `(P portals read, N rooms, B beside
by box, T two-sided)`; the first eight two-sided draws log `roof: room R
drawn two-sided (seen across open air)`.

New wrappers: none; no table change.

Playtest: an unbounded field with closed tiles around it, some joined to
the field by a doorway, some with a solid side to it, one diagonal, one a
floor up; stand in the field and look at each from every side: walls,
floor and ceiling all show; walk into a closed tile: its inside looks as
before; repeat from inside a closed tile looking out across the field;
bounded tiles still hide their walls; 2-4 player split screen; frame rate.

### One-walled pieces next to open air (.73 report)

User report (golgotha map): "I can see the walls for corner pieces that are
adjacent to unbounded tiles, but I cannot see the walls for the other
one-walled pieces I placed."

What the .73 log shows (`logs/m73.err`, ignored): 321 pieces, 209 Open air
(bounded), 0 unbounded; every room is in the open-view set and two-sided
(`321 two-sided`), only 8 by box. Pattern per floor: 16 bounded pieces, then
8 closed ones. The 64 bounded pieces that were logged are 3x3 tiles. Their
masks have the three slot bits of exactly one side (0x007, 0x038, 0x1C0,
0xE00: an edge) or of two sides (0x03F, 0x1F8, 0xFC0, 0xE07: a corner),
and the draw hides those 3 or 6 of their 15 nodes. These are the closing
walls on sides with no neighbour, which Open air (bounded) hides by design
(invisible wall, collision kept). They are not a back-face problem: the
open-view set and the two-sided draw already cover those rooms.

What the log cannot show: the tile ids and node masks of the closed pieces
and of the bounded pieces after the first 64, and which drawn rooms keep a
wall node masked out. With no tile ids, it is not yet confirmed whether the
"one-walled pieces" are bounded tiles whose single wall is hidden on
purpose, or closed tiles whose wall node is masked out by their own saved
mask. The catalogue does have 1x1 one-wall tiles (14 and 38: three side
connection points) and 1x1 corners (1 and 37: two). Checked statically and
ruled out:
- Translucent passes: the bucket reader `sub_000865E0` already draws them
  with cull mode 0 (`0x000865EE`).
- The cull-none path: it works in pass 0 for every room geometry prop.
  `sub_0008F5E0` forces 0x901 per node only for props with `+0x28` bit
  0x20000000 (mirrored: scale -1 at `0x000A1602`); whether room props ever
  have it is unknown, so the new log prints the prop flags.

Diagnostics added (no behaviour change):
- `roof: bounded piece K: rooms ... (mask M, tile T at x,z floor f
  rotation r)`: the piece record's tile id and place.
- `roof: room R drawn two-sided (seen across open air): tile ..., open|closed,
  mask M, prop flags F, N of T nodes masked out; node masks ...`: the first
  96 rooms per build, with the first 24 node masks (`+0x5C`). A wall that
  stays invisible shows here as a node whose mask has no bit in M (or as
  flag 0x20000000).
- `openview: area A: closed room R (tile ..., mask M) beside open room S
  (tile ...) by a portal|its box`: the first 96 per build.
- `openview: after 600 views, N of T rooms drawn across open air never drew
  their geometry` and up to 24 `room R never drawn (tile ..., depth now D)`
  lines.

Next playtest: the same map, look at a one-walled piece and a corner piece
from the open area, then send `game.err`. Compare the two pieces' lines.

User clarification: "The one walled pieces are set to closed, but their
walls should be visible beyond the 3x3 open air bounded pieces in front of
them. Currently, the wall is invisible, and you just see the floor of the
piece beyond."

Cause (INFERRED; the .74 logs above confirm or refute it). The open-view
areas were joined only by portals between open rooms. The golgotha log has
3 areas (`209 open-air rooms in 3 areas`): floors of open tiles stacked
above each other, or joined only through closed tiles, have no open-to-open
portal. Each area gives its touched rooms the union of its own visible
rooms' rectangles. An area that does not hold the camera is seen only
through the stock walk's portal rectangles, and those are one floor high.
Every prop of a room is scissored to its rectangle (`sub_000862C0`). So a
closed tile beyond that area kept its floor, which lies inside the portal
rectangle, and lost its wall above it. The two-sided draw does not help
with clipping.

Ruled out (decode of the neighbour mask `sub_002FC680`): coincident-wall
hiding. A side wall bit is set only for a connection point (kind 0/1) whose
neighbour cell (`sub_002F8140` side flag, `sub_002F8780` match) does not
connect (`0x002FD137..0x002FD20D`). Kind 2/3 entries add bits
unconditionally. Mirror tiles (catalogue `+0x12` bit 0) reflect the bits.
Decoration bits come from catalogue `+0x108`. A side without connection
points gets no bit, so a closed tile's solid wall is never removed by an
open neighbour, and the bounded hide only affects bounded rooms. New room
props start with `+0x28` = 0x1161/0x1061 (`0x000A6A7F`), not 0x20000000, so
the mirrored per-node 0x901 is not set on room geometry.

Fix: `mapmaker_openview_build` joins two open rooms into one area when their
boxes touch (same 0.5 margin as the closed-room test), as well as by portal.
The whole open structure then gets one rectangle, the whole viewport when
the camera stands on it, and so does every closed room beside it. Log
`openview: N open-air rooms joined their area by box (no open portal between
them)`. Cost: at most open rooms x rooms box tests once per build; the draw
covers more rooms (still depth-tested and culled per prop). Fixture: the
island room 6, a floor up beside room 3, joins area 1; 0.6 away it does
not. Mutations: `no-box-join`, `box-join-closed-too`.

Playtest (.75): golgotha, from each floor's walkway look at the closed
one-walled pieces beyond the bounded tiles, on the same floor and on floors
above and below: walls show. The build line should read `in 1 area`, with a
`joined their area by box` line.
