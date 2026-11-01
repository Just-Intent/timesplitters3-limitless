# MapMaker tilesets: the hidden Abstract set

Contract for `src/interface/pc_mapmaker_tilesets.inc`, the save rule in
`src/interface/pc_mapmaker_container.inc` and their tests
(`tests/mapmaker/test_mapmaker_tilesets.py` with fixture
`tests/mapmaker/mapmaker_tilesets.c`; the save tier is in
`tests/mapmaker/test_mapmaker_container.py`). US XBE 45410066 only.
CONFIRMED = read at the address, in the decoder or in the game paks;
INFERRED = deduced.

## What ships

The tileset table `0x0052CF38` has six 0x58-byte records; the map header
(chunk 0x00C77668) stores the index in word +0xA, paks and model folders use
index + 1.

| idx | name (text) | tile models (+0x04) | sky (+0xA) | music (+0x50) | image (+0x54) | Map Settings |
|---|---|---|---|---|---|---|
| 0 | Abstract (0xAAF) | 1980..2008 `PRETILE1..29` | 60 Abstract | 0x1A0 `tileset_lab_high` | 0 (none) | hidden |
| 1 | Lab | 2009.. `LABTILE*` | 65 | 0x19F lab | 0x17D lab | yes |
| 2 | Egyptian | 2038.. | 61 | 0x19D egypt | 0x17B egypt | yes |
| 3 | Military | 2067.. | 66 | 0x1A1 military | 0x17E military | yes |
| 4 | Horror | 2096.. | 59 | 0x19E horror | 0x17C horror | yes |
| 5 | Virtual | 2125.. | 60 | 0x1A2 vr | 0x17F vr | yes |

(Model names and flags from the prop table, pak entry CRC 0xD9E2667A, identical
in `l_100.pak` and `l_102.pak`; image/music names from the resource table
`0x004D2D64 + id*0x18`.)

Abstract's assets, all present (CONFIRMED, `test_abstract_assets`):

- **Tile models** `tileset1/tile1_1..29` (29 of 29): only in the editor pak
  `pak/arcade/l_102.pak`, flags 0x200 (resident). The other tilesets' models
  are flagged 0x2000000 (streamed) and live in `tileset<N>.pak`.
- **Pads** `pad/data/tile1_1..35.raw` (35 of 35): only in `l_102.pak`; the
  play pak `l_100.pak` has the pads of tilesets 2..6 only.
- **Textures and props**: `tileset1.pak` (102 entries, 78 textures). All 24
  streamed per-tileset item references of item column 0 (`0x00524F64 +
  (id*7 + 0)*0x1C`, 26 items with flag 4; the two armours are resident in
  every tileset) are in it: its own keycards (`mapmaker/pre*key`) plus Lab
  props (machines, crates, bookshelf, barrels, ceiling gun, doors, windows),
  Horror guns and camera, a Military ramp and the Virtual buggy.
- **Lighting, music, sky**: record 0 has the same ambient (0.35) and
  zero directional light as Lab, the music cue 0xBC, Lab's music track and
  sky 60; only the image is missing (0).
- `l_100.pak` and `l_102.pak` share 218 entries, all byte-identical, so
  mounting both cannot change what a stock name resolves to.

## Why it is hidden

INFERRED from the evidence: Abstract is the editor's own **preview** set.
"PRE" names its models and keys; the editor always preloads its 29 tile models
(`sub_00338110` from the editor setup `sub_0032CAB0`, loop bound
`cmp ebx, 1` at 0x0033811E), and its palette builder handles index 0 like any
other (`sub_00311E90`, jump table 0x00311F98, case 0 -> model 0x7BC). It was
never packaged for play: its geometry and pads exist only in the editor pak, it
has no own image and reuses Lab's music and most of Lab's props. The Map
Settings builder `sub_003133E0` skips it with five pointer compares
(0x00313510..0x00313536). Nothing else excludes it; the editor, loader
(`sub_00301A90`, no range check) and play builder are table driven.

## Editor (scene 102)

Stock behaviour once selected (CONFIRMED paths): the row value binds editor
+0x204; `sub_002FC0A0` copies it to `[0x00784020]` and sets reload flags 0x12;
`sub_002FC200` streams `tileset1.pak` (`sub_00338180(0, 0)`, stream set
`0x0052EC88` entry 0, `TIL1`). Tile models and pads are resident from
`l_102.pak`; streamed item models are looked up in the last streamed pak
(`sub_00096410`, 0x000964AB) and all of column 0's are in `tileset1.pak`.
There is no second tileset picker: New map sets 3 (`sub_002FEBF0`,
`sub_00302A20`).

Hook: **`pc_mapmaker_tilesets_menu_row`** (WRAP `sub_00071800`, add a list row,
cdecl text, value). Only for return 0x0031354A (the tileset loop of
`sub_003133E0`), value 5 (Virtual, the last stock row),
`TSFP_MAPMAKER_LIMITS=1` and scene 102: after the original it adds
`sub_00071800(sub_0003D7B0(0xAAF), 0)`, so **Abstract** is the sixth row.
Every other caller (bosses, sets, weaponset, sky list 0x003135D4, ...) runs
the original only.

## Play (scene 100)

Stock: `sub_00062790` mounts `sub_000622D0(sub_00338170(header +0xA), 1, 1)`
(call 0x000628ED) and then `l_100.pak`. For tileset 0 that mounts
`tileset1.pak`, and the play builder `sub_00085920` would then request
`tileset1/tile1_M` models and `pad/data/tile1_P.raw` pads that are in no
mounted pak (its tileset-1 pad fallback at 0x00085AF4 finds nothing either):
missing room geometry and pads, so the stock game cannot play such a map.

Hook: **`pc_mapmaker_tilesets_play_pak`** (WRAP `sub_00338170`, its only caller
is 0x000628ED, checked by the decoder). For return 0x000628F2 and argument 0,
before the original:

- mount `pak/arcade/l_102.pak` exactly like the tileset paks:
  `sub_000622D0(path, 1, 1)` (read whole, level heap). The path is written
  below the guest stack and the calls run below it; `sub_00062260` first, so it
  is never mounted twice. The stock teardown `sub_00062720` unmounts it with
  the level's other paks. Cost: 4.2 MB of level heap; with `tileset1.pak`
  (2.2 MB) that is 6.4 MB, less than Lab (7.3 MB) or Military (9.4 MB) alone.
- **not gated** on `TSFP_MAPMAKER_LIMITS`: stock maps never have tileset 0, and
  an Abstract map must not be played without it.
- if the mount is impossible (fewer than 8 of the 30 mount slots,
  `0x000620F9`, would remain, or the file does not open) the map plays as
  **Lab**, the tileset whose props and music Abstract shares: the getter's
  argument, header word +0xA and `[0x00784020]` become 1, logged once as
  `[mapmaker] tilesets: Abstract map without l_102.pak; playing it as Lab`.
- both hooks give record 0 Lab's image (+0x54 = 0x17D) when it is 0, so the
  map info panel (`sub_002C8560` -> `sub_002C8450`) shows an image.

## Save format

A map with tileset 0 is a valid table index for a stock game: its editor
would even load and edit it, but its play would miss the tiles and pads. So a
header with tileset 0 counts as past the stock limits in
`mapmaker_container_over_stock` (`mapmaker_container_over_chunks`, chunk
0x00C77668, word +0xA == 0) and saves in the fork's 64 KB tier: a stock game
refuses it (file larger than its body) instead of loading it, and System Link
sessions load only the stock tier, so it is never sent to a peer. This build
loads it with or without the switch; without the switch the Map Settings list
has no row for it (choose another tileset to leave it).

## WRAP lines

```
WRAP sub_00071800 -> pc_mapmaker_tilesets_menu_row     (new; area 'tilesets')
WRAP sub_00338170 -> pc_mapmaker_tilesets_play_pak     (new; area 'tilesets')
```

Neither function is wrapped by another area or an upstream patch (bosses, sets
and weaponset only call `sub_00071800` through `guest_call`, which reaches the
wrapper with another return address). The integrator adds the include after
`pc_mapmaker_weaponset.inc` in `pc_menu_mouse_guest.inc`, the declarations in
`pc_display_menu.h` and regenerates the wrappers patch.

## Tests

`tests/mapmaker/test_mapmaker_tilesets.py`: fixture (gate, scene, caller and
row checks; mount path, arguments and stack placement; no remount; fallback
for a full table, a failed mount and a missing container; esp and return
address) with 27 mutations; XBE facts (table fields, menu loop bytes, loader
call, mount slots, editor case 0, builder pad fallback); decoder (the getter's
only caller, the table's readers); the pak audit above (skipped without
`private/game`). Container fixture: tileset 0 saves in the 64 KB tier, Lab in
the stock one (mutations `over-abstract-ignored`, `over-abstract-any-tileset`).

## Not verified

Not built or run: the editor look of Abstract, play of an Abstract map (tiles,
collision, pads, spawns, items, music), the info panel image and the fallback.
Splitscreen, System Link and Mac not tried.
