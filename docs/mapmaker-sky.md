# MapMaker open sky: per-map sky and clouds, sky-tile probe

Feature (c) of `logs/limits/features/sky.md` (ignored, main checkout), plus the
probe for feature (d). Static analysis of the US XBE (title 45410066);
CONFIRMED = read at the given address, INFERRED = deduced. Nothing here has
been built or played yet.

Files: `src/pc_mapmaker_sky.inc` (C policy, wrappers), `tests/mapmaker_sky.c`
+ `tests/test_mapmaker_sky.py` (fixture with mutations, stock-data checks
against the local XBE), probe flag 128 in `src/pc_mapmaker_probe.inc`
(`tests/mapmaker_probe.c`, `docs/mapmaker-meter-probe.md`), and the dev-only
room mask switch in `src/pc_mapmaker_skymask.inc` (`tests/mapmaker_skymask.c`
+ `tests/test_mapmaker_skymask.py`). No guest-code table entries, no extension
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

Story safety: no entry of the 87-entry level table `0x00515FF8` uses skies
58..67 (checked by `tests/test_mapmaker_sky.py` when the XBE is present), the
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
`0x00313888`); link stubs for these are in `tests/mapmaker_area_stubs.h`.
The two probe hooks are in `pc_mapmaker_probe.inc` and are inert unless
`TSFP_MAPMAKER_PROBE` has flag 128; `sub_0008F950` runs for every model draw,
so its hook only tests a counter unless a capture window is open.
`sub_00085710` is not wrapped by any other area. `pc_mapmaker_sky_room_mask`
is in `src/pc_mapmaker_skymask.inc` (included after `pc_mapmaker_sky.inc`;
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
  A decoder scan of the stock range (`tools/guest-refs.py 0x703460 0x31C`,
  repeated by `tests/test_mapmaker_skymask.py`) finds four references: the only
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
in `docs/mapmaker-meter-probe.md`.

## Tests

- `tests/test_mapmaker_sky.py`: fixture (20 mutations: scene gates, limits
  gate, restore, level sky set/restored, template, sky range, tileset default,
  stock-bit collision, read before rebuild, load/new-map/editor-change state,
  checkbox caller/binding/refresh, rebuild container, chunk walk) and the stock
  data the design relies on (sky list, cloud fields, level table, tileset
  defaults, call-site bytes), skipped without the local XBE.
- `tests/test_mapmaker_probe.py`: flag 128 window, deduplication, scene gate,
  read-only check, relocated tables, node dump (once per model and table,
  stride, drawn/hidden, per-model and per-run caps, read-only; 7 more mutations).
- `tests/test_mapmaker_skymask.py` (fixture `tests/mapmaker_skymask.c`): the
  room mask switch: off by default, scene 100 only, after the original, the
  relocated table only, build record untouched, 256-entry bound, one summary per
  build (also when a second build starts before a frame), bad values refused,
  exact log text (15 mutations); plus the decoder check that `0x000857A3` is the
  only store into the table and that `rooms.json` relocates every reference
  (skipped without the private XBE).

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
