# MapMaker Disco variant tileset: texture survey and plan

Step 1 (offline survey) of the first custom MapMaker tileset: a **Disco
variant** that keeps an existing tileset's geometry, pads and connection points
and shows the Disco arcade map's own textures, read at runtime from the
player's game data. No artwork is made or shipped. A map stores its base
tileset (header word `+0xA`, unchanged) plus a variant marker, so a stock game
or a peer without the variant plays it as the base tileset.

Survey only: no game code was changed or run. US XBE 45410066, game data read
in place. CONFIRMED = read in the paks/XBE; INFERRED = deduced (visual roles,
fit). This document holds identifiers, keys and dimensions only; decoded images
stay in the ignored `private/tileset-survey/` folder.

Reproduce (writes PNGs, contact sheets and `survey.json` under `private/`):

```
.venv/Scripts/python.exe tools/textures/tileset-survey.py --decode \
  --preset disco --preset tileset1 ... --preset tileset6
```

## How a model names its textures (CONFIRMED)

Every model file (tile, prop, level) starts with a dword offset to its
texture list: 16-byte rows `{id, id, 0, 0}` with a 24-bit id, ended by
`0xFFFFFFFF` (the sky notes in `mapmaker-sky-music.md` found the same list).
Each id is the pak entry `textures/%06x.xbt` (CRC32 name); the texture pass
loads it from any mounted pak. So tile model -> texture usage is exact, and an
alias can work on **ids** (guest side) or on **payload keys** (host pack).

Tile model texture lists are what the table below counts ("tiles" = how many
of the 29 tile models list the texture). Which faces (floor/wall/ceiling) use
which texture needs the mesh/material-batch layout of the `.xbr`, which is not
decoded; roles below are INFERRED from the images and usage counts.

## Disco (CONFIRMED)

- Scene 56. Level pak `pak/arcade/l_56_ar.pak` holds the monolithic level
  model (entry CRC `0x22294201`, 1,161,140 bytes, **82 textures**) and 8
  Disco props (entries `dda3d816`, `31bd0f3a`, `716e1e12`, `2e1f42fe`,
  `a098452b`, `b27a837f`, `4458886b`, `f7bfa73c`) plus generic props.
- The textures are **not in the level pak**: they are in its memory pak
  `pak/arcademem/l_56_arm.pak` (135 textures). The same payloads are also in
  `storymem/l_56_stm.pak` and `challengemem/l_69_chm.pak` / `l_75_chm.pak`.
- Survey set: textures listed by any model of `l_56_ar.pak`, found in
  `l_56_arm.pak`, minus those also in `l_100.pak` / `l_100m.pak` (generic) =
  **100 textures** (82 level, 18 prop-only), all resolved.
  Formats: 60 DXT1, 21 DXT5, 11 L8, 8 A8. Sizes: 55 square, 35 2:1, 6 4:1,
  4 8:1 (24 at 256x256, 22 at 256x128, 15 at 128x128). Alpha: 71 opaque,
  21 blended, 8 A8 masks. 3..7 mips each.
- Mounting `l_56_arm.pak` beside scene 100's paks is safe by content: it
  shares 162 names with `l_100m.pak`, 27 with `gun.pak`, 19 with `l_102m.pak`,
  all byte-identical; none with `l_100.pak`, `l_102.pak`, `chr.pak` or
  `tileset6.pak`.
- Music: XBE music id `0xBA` = `music/ts3_disco44fx.xbm`, jukebox index 26
  (text `0x1930`). No Disco sky record exists (indoor map).

Tiling hint = wrap-seam difference over mean neighbour difference per axis
(<= 2 counts as repeating); framed or symmetric images can pass it, so it is a
hint, not proof. "level" = listed by the level model.

## Base tileset candidates (CONFIRMED counts)

| tileset | pak(s) | textures | used by tile models | tile texture shape |
|---|---|---|---|---|
| Abstract (1) | `tileset1.pak` + tiles `l_102.pak`, tile textures `arcademem/l_102m.pak` | 165 | 87 | all 256x256 DXT1, 7 mips, blueprint/grid placeholder look |
| Lab (2) | `tileset2.pak` | 134 | 64 | mostly 2:1..8:1 atlases (42 without a seamless edge) |
| Egyptian (3) | `tileset3.pak` | 107 | 49 | 43 of 49 have one mip only |
| Military (4) | `tileset4.pak` | 140 | 95 | 37 blended, 46 atlas-like |
| Horror (5) | `tileset5.pak` | 104 | 60 | many detailed atlases (wood, stone, props) |
| Virtual (6) | `tileset6.pak` | 22 | **9** | 256x256, 7 mips, framed panels and glow masks |

Side finding (CONFIRMED in the paks, play effect INFERRED): Abstract's 29 tile
models list 87 textures that exist **only** in `arcademem/l_102m.pak`, not in
`tileset1.pak` or `l_102.pak`. `pc_mapmaker_tilesets.inc` mounts `l_102.pak`
for an Abstract map in play; unless `l_102m.pak` is also resident there, its
tiles would load untextured. Worth a check before relying on Abstract.

### Virtual tile textures (CONFIRMED usage, INFERRED role)

| id | key | format | tiles | props | alpha | role (INFERRED) |
|---|---|---|---|---|---|---|
| e871cc | 71e34d2f1e7c716d | A8R8G8B8 | 29 | 16 | opaque | main dark panel (floor/wall base) |
| 25ae6e | 7129bce0f5a21600 | A8R8G8B8 | 29 | 0 | opaque | cyan glow-framed band (wall panel) |
| 294554 | ab4218bfd50843c2 | A8R8G8B8 | 25 | 0 | opaque | teal framed square (floor/ceiling grid) |
| b2a030 | 63a34ec29bf95d97 | DXT5 | 17 | 1 | blended | vertical light bar (trim glow) |
| 79b187 | 4a7e632af9d9a01a | A8R8G8B8 | 13 | 15 | opaque | light framed panel (ceiling/door frame) |
| 5a89bc | a40f76f0f059094e | A8 | 11 | 0 | mask | glow line mask |
| 666443 | a66e65142c70e232 | A8R8G8B8 | 6 | 7 | opaque | grey trim/pillar |
| d7e9c7 | d785d0028968ecf5 | A8 | 4 | 15 | mask | framed glow mask |
| 939772 | e0911f8a3e80d4ab | A8 | 1 | 4 | mask | glow line mask |

The other 13 Virtual textures (L8 panels, a circuit atlas `6d898c` 512x512,
`40928b`, `bb04f4`, `76cb86`, `20d893`, `a332e9`) are used only by its 26 prop
models.

## Proposed pairing

**Base: Virtual (tileset 6).** Reasons: only 9 textures cover all 29 tile
models (5 opaque surfaces + 4 glow/mask layers), so a 5-row alias table themes
every tile; all are 256x256 with full mips and mostly per-face framed panels,
so square repeating Disco textures fit without UV changes; Virtual's sci-fi
look is the easiest to re-read as a club; the assessment already picked it as
the cheapest proof. Lab/Military/Horror need 49..95 aliases with many atlases
whose UV islands would not match Disco images; Egyptian's tile textures have
no mips; Abstract is the editor's placeholder set (and see the side finding).

| Virtual id | -> Disco id | Disco size / format | fit |
|---|---|---|---|
| e871cc main dark panel | 7dde70 starry dark surface | 256x256 DXT1, opaque | tiling OK, same size |
| 25ae6e glow band | 0d9139 gold retro wallpaper | 256x256 DXT1, opaque | tiling OK in x (band repeats in x); framed glow lost |
| 294554 framed square | 7b0662 red plush | 128x128 DXT1, opaque | tiling OK, half resolution; alt 20e656 starry carpet (DXT5, partial alpha: alpha mismatch) |
| 79b187 light framed panel | 3c2652 dark leather | 256x256 DXT1, opaque | tiling OK in x; also changes 15 Virtual props |
| 666443 grey trim | f05b7c tiger print | 256x256 DXT1, opaque | tiling OK; also changes 7 props |
| b2a030 light bar | keep base | - | no Disco equivalent with matching alpha; 723312 (512x64 DXT5) was checked: aspect + content mismatch |
| 5a89bc, d7e9c7, 939772 masks | keep base | - | A8 glow masks, tinted at draw; Disco masks (13145d, 33a211, 9ef39e) are spot lights |

Defaults for the variant: music jukebox index 26 (Disco, id 0xBA); sky 59
Dark (indoor club; Virtual's own is 60 Abstract); lighting: Virtual's record
(ambient 0.35, no directional light) until a darker ambient is tested.
Previews of rows 1-5 (plus the 20e656 alternative) were reviewed offline only.
Further candidates for a later pass: 61c959 wood boards (256x128, needs 2:1
UV), 693875 bamboo, 207ed4 snakeskin, 330ff1 black/gold panel, c16827 gold
moulding (2:1).

## Open risks

- **Vertex colour / tint**: Virtual's L8/framed textures look designed for
  tinted, glowing vertex colours; Disco textures may come out cyan-tinted or
  too bright. Needs a capture once implemented; a variant lighting override
  may be needed.
- **Face roles unverified**: which texture is floor/wall/ceiling is inferred;
  decoding the tile `.xbr` material batches (or one capture per tile) settles it.
- **Alpha/material state**: if a material's blend state follows the texture
  format, swapping opaque A8R8G8B8 for DXT5 would change it; the pairing keeps
  opaque -> opaque for that reason.
- **Prop sharing**: 79b187, 666443 and e871cc are also on Virtual props
  (doors, windows); aliasing per tileset re-themes them too (acceptable, but
  check door frames).
- **Memory/mounts**: mounting `l_56_arm.pak` takes one more mount slot (the
  Abstract WRAP refuses below 8 free); only the aliased textures (~5 x 44 KB
  DXT1) are read.
- **Texture pack interaction**: aliased Disco payloads carry their own keys,
  so issue #74 replacements of those Disco keys (if any) also apply; the
  Virtual keys' replacements stop showing in the variant.
- **Local-only result**: the variant needs the player's own `l_56_arm.pak`;
  if it is missing the map must fall back to plain Virtual (logged).

## Implementation plan

1. **Variant marker** in the map header: choose a verified-free bit/field
   (decoder-checked like the roof's `+0x26`, info-flag pattern as the sky
   extras use); values 0 = none, 1 = Disco. Stock games ignore it; base stays
   Virtual (5) in `+0xA`. Validator: variant only with its base tileset.
2. **Menu row**: append "Disco" after the tileset rows in the Map Settings
   builder `sub_003133E0` (same hook style as the Abstract row /
   Clouds checkbox); selecting it sets `+0xA` = 5 and the marker.
3. **Textures pak**: in scenes 100/102, when the marker is set, mount
   `arcademem/l_56_arm.pak` index-only with the existing `smx_mount` path
   (`pc_mapmaker_skymusic.inc`, `smx_mount_pair`), before the tileset's
   models are read. This replaces the assessment's "host pak reader" for the
   guest route; a host pak reader is only needed for the host-pack route below.
4. **Alias at load**: after a tile/prop model of tileset 6 is read and before
   its textures are registered (`sub_00096220` / queue `sub_000637E0`), rewrite
   the listed ids per the alias table (both words of the row). Rewrite the
   model's list, not the queue name, so material lookups by id stay consistent
   (INFERRED; verify that materials resolve through the same list).
   Alternative host route: in `src/texture_pack.c`, a scene-scoped
   variant lookup that substitutes the Disco payload by key, read by a small
   host pak reader (both host adapters).
5. **Defaults**: variant sky/music/lighting through the existing extras
   (`pc_mapmaker_skymusic.inc` music path, table overrides) when the map's own
   choice is -1 (Recommended).
6. **Cache invalidation**: switching variant in the editor (scene 102) must
   reload the tileset models (same path as a tileset change) so the rewritten
   lists apply; for the host route, reset `pack_checked`/`pack_key` on affected
   texture objects. Verify texture lifetime across tileset switches.
7. **Fallback/logging**: if the mount or any aliased id is missing, keep the
   stock ids (plain Virtual) and log once.
8. **Tests**: fixture for marker encode/decode and validator; pak audit test
   (Disco ids present in `l_56_arm.pak`, byte-identical overlaps); capture of
   one Disco map in play and in the editor on both hosts.

## Disco texture list (CONFIRMED identifiers; hints INFERRED)

| id | key | format | size | mips | alpha | tiling hint | used by | hint |
|---|---|---|---|---|---|---|---|---|
| 007997 | ee02bbd0e39e49ca | DXT1 | 256x64 | 5 | opaque | repeat x+y | level | brass/wood rail strip |
| 0a7bc8 | b01356aec69e302b | DXT1 | 256x128 | 6 | opaque | atlas/decal? | level | gold relief (atlas) |
| 0d9139 | 7c69eb8d8d8d89a0 | DXT1 | 256x256 | 7 | opaque | repeat x | level | gold retro wallpaper |
| 0e328e | c3b855ef007cbeab | DXT1 | 32x32 | 4 | opaque | repeat x+y | props | tiny dark red swatch |
| 10f2af | c1ef5e81470157ac | DXT1 | 256x256 | 7 | opaque | atlas/decal? | props | first-aid sign/panel (prop) |
| 112d79 | 818219b801bda44b | L8 | 64x64 | 5 | opaque | repeat x+y | level | dark gradient (L8) |
| 114635 | 0d715f254aa53fd5 | A8 | 64x64 | 5 | A8 mask | repeat x+y | props | checker/grid mask (prop) |
| 13145d | d05a4c4b9643a7ec | A8 | 64x64 | 5 | A8 mask | repeat x+y | level | soft grid mask |
| 14c565 | 0b545902cab06c04 | DXT1 | 128x128 | 6 | opaque | repeat y | level | red panel with white rounded inset |
| 1759e0 | 812524e802e5e8ac | DXT1 | 256x128 | 6 | opaque | atlas/decal? | level | gold relief (atlas) |
| 179934 | 17872906a47dc150 | DXT1 | 128x64 | 5 | opaque | atlas/decal? | level | gold relief piece |
| 19977a | 3d34b10044859995 | L8 | 256x128 | 6 | opaque | repeat x+y | level | dark screen/vent (L8) |
| 1abf87 | 74f893a0981d4f6e | DXT5 | 256x256 | 7 | blended | repeat x | level | reed/bamboo screen (alpha) |
| 1cd9e1 | 62399eb6fcf2a527 | DXT1 | 128x128 | 6 | opaque | atlas/decal? | level | red/white ring segment (atlas) |
| 1fb064 | 14462f2e28d94446 | DXT1 | 256x128 | 6 | opaque | atlas/decal? | level | gold relief + pink (atlas) |
| 207ed4 | b58c05ac69e27eec | DXT1 | 256x256 | 7 | opaque | repeat x | level | snakeskin |
| 20e656 | 22637e86d82a321e | DXT5 | 256x256 | 7 | blended | repeat x+y | level + props | starry dark carpet (partial alpha) |
| 2a0beb | ef96552f6a9a8d93 | L8 | 64x64 | 5 | opaque | repeat x+y | props | dark ring glow (L8, prop) |
| 330ff1 | b1dfe39389e94b89 | DXT1 | 256x256 | 7 | opaque | repeat x | level | black panel with gold trim |
| 33641a | 512bbe69a75e5951 | DXT1 | 256x256 | 7 | opaque | atlas/decal? | props | drum skin (prop) |
| 33a211 | 3f6a4e3fbdceaaf4 | A8 | 64x64 | 5 | A8 mask | repeat x+y | level + props | round glow mask |
| 38e5f3 | 44a3b07c1649793e | DXT5 | 128x32 | 4 | blended | repeat x+y | level | light bulb strip (alpha) |
| 39e150 | 3b7dd454c610a86d | DXT5 | 256x128 | 6 | blended | repeat y | level | metal truss (alpha) |
| 3c2652 | aa087d178ee0b104 | DXT1 | 256x256 | 7 | opaque | repeat x | level | dark leather |
| 42aecb | e9bf4dcc27e7ba0a | L8 | 64x64 | 5 | opaque | repeat x+y | level | round light (L8) |
| 4482ce | 43e3ba328bf66163 | DXT1 | 256x64 | 5 | opaque | repeat x | level | wood/brass trim strip |
| 45169c | c19f057fa5723047 | DXT1 | 128x128 | 6 | opaque | repeat x+y | level | spiral (dark) |
| 488fdc | aee6654ae938210a | DXT1 | 512x512 | 7 | opaque | atlas/decal? | props | mixed atlas (prop) |
| 4a9632 | 674dda3c8a728032 | DXT1 | 128x128 | 6 | opaque | atlas/decal? | props | mirror-ball highlight (prop) |
| 4b449f | 7d8c6a9d8e8aab65 | A8 | 256x256 | 7 | A8 mask | repeat x+y | level | large soft mask |
| 507339 | 4e160efeae12a8c7 | DXT5 | 256x256 | 7 | blended | atlas/decal? | level | dark panel with ring (alpha) |
| 50b30d | e078d1c0e904651a | DXT1 | 256x128 | 6 | opaque | atlas/decal? | level | gold relief (atlas) |
| 565027 | 6bba7a0f1b7f887a | A8 | 64x64 | 5 | A8 mask | repeat x+y | level + props | square glow mask |
| 5689af | d97df67ad39be541 | DXT5 | 256x256 | 7 | blended | repeat y | props | cymbal (prop, alpha) |
| 59ff3f | c8eda72da87de440 | DXT1 | 512x512 | 7 | opaque | atlas/decal? | props | mixed atlas (prop) |
| 5f9260 | 9d551ee64ece7612 | DXT1 | 128x128 | 6 | opaque | repeat x+y | level | red/white stripes |
| 61c959 | 39304566dec324c0 | DXT1 | 256x128 | 6 | opaque | repeat y | level + props | wood boards |
| 62dcd9 | bab9cda2836a9124 | DXT1 | 256x128 | 6 | opaque | atlas/decal? | level | gold relief (atlas) |
| 65d922 | e61b214bd9743ef6 | A8 | 256x128 | 6 | A8 mask | repeat x+y | props | sign lettering mask (prop) |
| 679921 | 6b78c21e2ac67677 | DXT1 | 512x256 | 7 | opaque | repeat x+y | level | neon figure outline (sign) |
| 680d21 | 0aa682b857587662 | DXT1 | 128x64 | 5 | opaque | repeat x | props | grey ring trim (prop) |
| 693875 | 8503875a344ef9c7 | DXT1 | 256x256 | 7 | opaque | repeat x | level | bamboo panel |
| 6cea14 | 62a333a724a54b4d | DXT1 | 512x256 | 7 | opaque | repeat x+y | level | neon figure outline (sign) |
| 6db149 | 20086deb70e6198b | DXT1 | 256x256 | 7 | opaque | repeat x+y | level + props | turntable/record deck |
| 6e1045 | 5ba787a78405944d | DXT1 | 512x256 | 7 | opaque | repeat x | level + props | guitar/poster strip |
| 70dabe | 99d228558e9945ee | DXT1 | 128x32 | 4 | opaque | repeat x | props | small lit sign (prop) |
| 71c2e9 | 01536bc19f1a0f51 | DXT1 | 512x512 | 7 | opaque | atlas/decal? | props | mixed atlas (prop) |
| 723312 | 1433e7d80d106da2 | DXT5 | 512x64 | 5 | blended | repeat y | level | decorative strip (alpha) |
| 76bd65 | 27e67a1b8c1b5c8a | DXT5 | 64x64 | 5 | blended | repeat x+y | level | white glow spot (alpha) |
| 76d5c6 | 12897aee0a1ac805 | L8 | 128x128 | 6 | opaque | repeat x+y | level | dark leather (L8) |
| 77054c | edd5fc5940e7a428 | DXT1 | 128x128 | 6 | opaque | atlas/decal? | level | poster/album art |
| 790833 | ae7468f3160c1df7 | DXT1 | 256x32 | 4 | opaque | repeat x | level | dark strip with dots |
| 7b0662 | 4577c7a6e0aa6399 | DXT1 | 128x128 | 6 | opaque | repeat x+y | level + props | red plush |
| 7dde70 | 2645e057259471f7 | DXT1 | 256x256 | 7 | opaque | repeat x+y | level | starry dark surface |
| 7f03a5 | 1e6eb94305097f61 | DXT1 | 256x128 | 6 | opaque | atlas/decal? | level | dark wood grain |
| 7fbb06 | 0941f67d77cceee7 | DXT1 | 256x256 | 7 | opaque | atlas/decal? | props | worn sign board (prop) |
| 80b8a8 | f1771d3aeb391675 | L8 | 16x16 | 3 | opaque | repeat x+y | level | white 16 px (L8) |
| 8287f5 | f3ec1593578336c4 | DXT5 | 256x128 | 6 | blended | repeat x | level | gold bulb fringe (alpha) |
| 82aeb2 | 509e4ede8fa88dc0 | DXT5 | 128x128 | 6 | blended | repeat x+y | props | vinyl record (alpha, prop) |
| 833712 | c0c1e2e4d248ada1 | DXT1 | 32x32 | 4 | opaque | repeat x+y | level | small dark disc |
| 894ded | 6aa40e72fb76bc81 | DXT5 | 512x256 | 7 | blended | repeat x+y | level | outline decal (alpha) |
| 8ace7c | 3b1b584bc17b01cc | A8 | 256x256 | 7 | A8 mask | repeat x+y | level | large soft mask |
| 8cec12 | 76f3e87f8e61751d | DXT1 | 128x64 | 5 | opaque | repeat y | props | orange ashtray/dish (prop) |
| 8f2c01 | a68644235e20d089 | DXT5 | 256x256 | 7 | blended | repeat x+y | level | yellow disc light (alpha) |
| 9464f0 | 6272549f3aa8c096 | DXT1 | 256x128 | 6 | opaque | atlas/decal? | props | gold/wood frame (prop) |
| 94e298 | 7cdc0d3d171a499c | DXT5 | 128x128 | 6 | blended | repeat x+y | level | starburst light (alpha) |
| 9530e0 | 53ca4eecc1aa3e76 | DXT1 | 128x64 | 5 | opaque | atlas/decal? | level | gold door relief |
| 9bceb4 | 568637dc6cbbb336 | DXT1 | 128x128 | 6 | opaque | repeat x+y | level | four light spots |
| 9e038e | ffbfb05d91e668a1 | DXT1 | 128x128 | 6 | opaque | atlas/decal? | level | red/white ring segment (atlas) |
| 9e446d | 9d79484d732dd192 | DXT1 | 256x128 | 6 | opaque | atlas/decal? | level | gold relief (atlas) |
| 9ef39e | e8055a25f9f46adb | A8 | 64x64 | 5 | A8 mask | repeat x+y | level + props | small glow mask |
| a2044d | 37d9054c8f140da3 | DXT1 | 256x128 | 6 | opaque | repeat x | level | gold mesh/grille |
| a3004c | e2311b8de424310e | DXT1 | 256x32 | 4 | opaque | repeat x+y | level | purple light strip |
| a34045 | 4c245837d403350b | DXT1 | 128x128 | 6 | opaque | atlas/decal? | props | red light (prop) |
| a4e881 | 3b37e4e179df03ff | DXT1 | 512x256 | 7 | opaque | repeat x | level | warm brown panel |
| a59295 | d437d76b03372483 | DXT5 | 256x256 | 7 | blended | repeat x+y | level | dark quarters decal (alpha) |
| a8e2f2 | 4a771e9180d9e045 | DXT1 | 256x128 | 6 | opaque | atlas/decal? | level | hair/fur gold (atlas) |
| ab0361 | 58a4d81efd854fcc | DXT5 | 64x64 | 5 | blended | repeat x+y | level | small dark decal (alpha) |
| ab5e8b | 045ea6715c8675c6 | DXT5 | 512x256 | 7 | blended | repeat x | level | coral/aquarium strip (alpha) |
| b07469 | 1d944eab54238d3d | DXT1 | 256x128 | 6 | opaque | repeat y | level | shelf/bar back |
| b2b50e | 540a9cf140785f09 | DXT1 | 512x64 | 5 | opaque | repeat y | level | dark-brass gradient strip |
| bb3b75 | 7c90ffd11ca587ff | DXT1 | 256x256 | 7 | opaque | repeat x+y | level | dark glitter |
| c16827 | ad80abcfaca2c99c | DXT1 | 256x128 | 6 | opaque | repeat x | level | gold moulding trim |
| cc22fa | c9854b6346ca24b9 | DXT1 | 256x128 | 6 | opaque | atlas/decal? | level | gold face relief (atlas) |
| cfdc71 | 17721e0189ee606d | DXT5 | 256x128 | 6 | blended | repeat x+y | level | curve decal (alpha) |
| d0a1b2 | befb6e15185f9aa0 | DXT1 | 128x128 | 6 | opaque | repeat x+y | level | red/white stripes |
| d16b5d | 1c1a72a5d440cad9 | DXT5 | 256x256 | 7 | blended | repeat x+y | level | neon sign (alpha) |
| d9e209 | 0788a49f3a871691 | L8 | 128x128 | 6 | opaque | repeat x+y | level | star field (L8) |
| e43b61 | 30a5327756b977b7 | DXT1 | 256x128 | 6 | opaque | repeat x | level | wood trim |
| e54949 | bada43422f9ffad7 | L8 | 256x256 | 7 | opaque | repeat x+y | level | star outlines (L8) |
| e719b5 | b0c13a2f5b497449 | L8 | 128x64 | 5 | opaque | repeat x | level | white bars (L8) |
| e79dc1 | df0c647d664466f5 | L8 | 64x16 | 3 | opaque | repeat x+y | level | tiny dark strip (L8) |
| ead13b | 7b0cef9b9dfcbc0c | L8 | 256x256 | 7 | opaque | repeat x+y | level | dark radial (L8) |
| ec439f | 05eae31d9d20858c | DXT1 | 256x128 | 6 | opaque | atlas/decal? | level | gold cloth (atlas) |
| ec7cef | b128119b74603487 | DXT1 | 128x32 | 4 | opaque | repeat y | level | cream light strip |
| f03d16 | 495b7c144b6aa9b7 | DXT5 | 512x256 | 7 | blended | repeat x+y | level | silhouette decal (alpha) |
| f05b7c | 4af650ac771e4d6b | DXT1 | 256x256 | 7 | opaque | repeat x+y | level + props | tiger print |
| f16543 | 65e7d20dcb995ebf | DXT5 | 256x256 | 7 | blended | repeat x | level + props | lettering decal (alpha) |
| f6a1ee | b7ef76b69dfe1793 | DXT5 | 512x256 | 7 | blended | repeat x+y | level | silhouette decal (alpha) |
| ff92ee | c3a128a0ac381f80 | DXT1 | 256x128 | 6 | opaque | repeat x | level + props | amp/console front |
