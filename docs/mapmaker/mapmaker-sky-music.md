# MapMaker: the game's other skies, weather and music

User request (2026-11-01): "Add any skies or weather to mapmaker present in the
game but not available in mapmaker mode" and "Add any music present in the game
but not available in mapmaker mode."

Static analysis of the US XBE (title 45410066) and the game pak indexes (names,
CRCs and sizes only; no game data is copied). CONFIRMED = read at the address,
in the lifter's decoder or in a pak index; INFERRED = deduced. **Nothing here
has been built or played**, except the v0.4.0-mapmaker.25 playtest under Play.

Files: `src/interface/pc_mapmaker_skymusic.inc` (new, area `sky`), the extra-sky
table and the extra-sky cloud rule in `src/interface/pc_mapmaker_sky.inc`;
tests `tests/mapmaker/test_mapmaker_skymusic.py` (fixture
`tests/mapmaker/mapmaker_skymusic.c`, 77 mutations, XBE / decoder / pak and texture-pak
checks) and the extended `tests/mapmaker/test_mapmaker_sky.py`.

## What MapMaker offers (stock)

Map Settings (`sub_003133E0`) builds each choice as a list: rows with
`sub_00071800(text, value)`, then `sub_00078590(&editor+field)` binds the row
(CONFIRMED):

| row | editor field | rows (values) | bind call / return | saved as (info chunk 0xC77668 data) |
|---|---|---|---|---|
| music, text 0xABB | `+0x210` | -1 (0x426, tileset), 0 (0x1917), 1..57 (`sub_002BE6F0`: text of jukebox entry) | `0x00313763` / `0x00313768` | `+0x0E` word (`0x00300784`) |
| sky, text 0xABE | `+0x220` | 11 entries of `0x0052D150`: -1, records 58..67 | `0x0031383E` / `0x00313843` | `+0x10` word (`0x00300795`) |
| weather, text 0xAC0 | `+0x224` | 4 entries of `0x0052D1A8` {byte value, text}: 0..3 (0xAC1..0xAC4) | `0x00313901` / `0x00313906` | `+0x12` byte (`0x003007A5`) |

The loop that offers music indexes 1..0x39 asks `sub_0035DE60(i)`, which always
returns 1 (`0x0035DE60`). The only readers of the three editor fields (decoder,
integer accesses with these displacements in the editor code) are the defaults
`sub_002FE2B0`, the rebuild, the loader `sub_00301A90` (`0x00301C5D..0x00301C7C`)
and the Map Settings close callback `sub_00313310`; `0x003154C0`/`0x003154FA`/
`0x003155D6` belong to another object (`[0x00783B90]`). Nothing draws or plays
them in the editor, so an editor field may hold a value a stock game would not
understand as long as the rebuild never saves it.

## Inventory

### Music (CONFIRMED)

- File table `0x00535530`: music ids 1..0xC9 (id 0xCA is null), each a
  `music/*.xbm` name; every one is in `pak/musicts.pak`, which every level
  mounts index-only (`sub_00045EB0`, `0x00045EC3`: `sub_000622D0(path, 0, 1)`).
  Ids 0x4B..0xAE are 20 cutscene tracks x 5 languages; `sub_00045D80` adds the
  language offset (`sub_00350C60`) itself, so a base id plays the right one.
- Jukebox list `0x00537210`, 58 x {music id, text}: 0 = none (-1), 1..56 =
  tracks, 57 = random (-2). A map stores the **index** (info `+0x0E`); scene 100
  converts it (`sub_00045EB0`, `0x00045F7A..0x00045FB7`): -1 -> tileset music
  (`0x0052CF40 + t*0x58` word), 0 -> silence, else `table[index].id`. The game
  setup's choice overrides the map's only when its entry is not -1
  (`sub_00045E60`, `0x00045E93`), so a -1 at `0x00045FB7` always comes from the
  map. An index >= 58 would read past the table (`0x005373E0` holds pointers):
  a stock game must never see one.
- The list offers 56 distinct files. The other 28 (each id below is the only or
  the base entry for its file; intro.xbm also fills unused ids 3, 10..22, ...):

| id | file | row label |
|---|---|---|
| 0x18 | ts3_menu32fx | Front End Menu |
| 0x01 | intro | Intro |
| 0x2B | ts3_gameover_ph44_fx | Game Over |
| 0x33 | ts3_mission_complete44 | Mission Complete |
| 0xC1 | ts3_challenge_win32 | Challenge Win |
| 0xC2 | ts3_farfuture1_challenge_44fx | Challenge: Far Future |
| 0x49 | ts3_ff2_boss_tune1_44fx | Boss: Far Future |
| 0x4A | ts3_ff2_boss_tune_faster_44fx | Boss: Far Future (Faster) |
| 0x4B..0xAA step 5 | cs01_prologue .. cs20anyalab (20 cutscene tracks) | Cutscene: ... |

Already offered and so not added: duplicates 0x17 (= 0xBC tileset guitar) and
0xBF (= 0xB8 Siberia). The jingles (Game Over, Mission Complete, Challenge
Win) are short; whether the music player loops them is not verified.

### Skies (CONFIRMED)

Sky table `0x005149A8`, 68 records of 0x34 bytes (model at `+4`, follow flags
`+8`, cloud colours `+0x10/+0x14`). Level table `0x00515FF8` (`+0x10` sky) for
the scenes using each; pak search = CRC of
`assets/ts/models/<name>/xbox/output/<leaf>.xbr` in every pak under
`private/game/pak`. MapMaker play mounts only `tileset<N>.pak`, `l_100.pak`,
`gun.pak`, `chr.pak` (`sub_00062790`; plus `musicts.pak`): a sky whose model is
elsewhere cannot be drawn there.

| records | what | status |
|---|---|---|
| 58..67 | MapMaker skies (gun.pak) | stock list |
| 34, 53, 54 | mansion, zeppelin, new Mexico: same models as 59, 65, 66 | already offered |
| 0, 24, 26 | model -1 (no sky) | "sky off" is the stock Map Settings path |
| 5 (also 1, 23, 25, 33) | skycoldnight, with clouds; scenes 20/57/62 | **added** (record 5) |
| 35 | skycastle_xbox; scenes 4/30/32/51/54/59/80 | **added** |
| 39 | skytrainXB; 11/33/34 | **added** |
| 41 | skyvirtua_xbox; 38 | **added** |
| 42 | skyprison_xbox; 29 | **added** |
| 43 | skyTS3intro_xbox; 2/24/26/44/64 | **added** |
| 44 | skynemo; 9/81 | **added** |
| 45 | skyveniceXB; 46/63 | **added** |
| 46 | skystreetwar; 5/40 | **added** |
| 47 | skynam_xbox; 48/73 | **added** |
| 48 | skytemple_xbox; 49/50/71 | **added** |
| 49 | skyspaceportXB; 15/72 | **added** |
| 50 | skyTS3depot_xbox; 18/45/76 | **added** |
| 51 | skysniper; 53/68/77 | **added** |
| 52 | skyTS3nf1_xbox; 6/19/23/66 | **added** |
| 56 | skychinese_xbox; 25/70 | **added** |
| 57 | skyhorrorXB; 39/60/74 | **added** |
| 2-4, 6-22, 27-32, 37, 38, 55 | TS2-era names (gangster, pinkdawn, mars, circus, ...) | **left out**: no pak has the model |
| 36 (scene 31), 40 (scene 28) | skysunset, skycorpbuilding | **left out**: no pak has a model under that name (INFERRED: those levels draw no sky) |

Row labels are English and come from the model names ("Story:" / "Arcade:"
by the level that uses it); "Story: Time to Split" is scene 2 (STATUS.md
calls scene 2 Time to Split), "Story: NF1" is `skyTS3nf1`, whose level name
was not looked up.

### Weather (CONFIRMED unless marked)

- Map start `sub_0023F390` (`0x0023F41B..0x0023F44B`) indexes the 8-byte table
  `0x005196DC` with info `+0x12`: {ambient sound, setup function}: 0 {-1, -},
  1 {0x139, `sub_00119920` rain: `[0x0073D97C]` = 1, resource group 0x80000},
  2 {0x139, `sub_00119920`}, 3 {0x18, `sub_00119940` snow: `[0x0073D994]`,
  0x100000}; rows 4 and 5 are {-1, 0}. Values 1 and 2 differ in the frame
  code: type 2 adds lightning (`sub_00119B20` -> `sub_00118D90`, INFERRED from
  the name-less call).
- Story levels set weather from their level data (`sub_00119960`, list at
  `+0x94`, count `+0x98`, entries 0x28 bytes): type 1/2 rain, 3 snow, **5
  sandstorm** (`[0x0073D9AC]`; models 0x527..0x52A
  `intro/intro_sandstorm_1..4`, table `0x004E62F8`, stride 0x44). Story use:
  type 2 in scenes 1, 8, 35, 37; type 3 in 5, 53, 68, 77; type 5 in 2, 26, 64.
  The sandstorm needs its buffers (`sub_0011B430`, `0x0011B45B..0x0011B493`),
  its props (`sub_0011B310` -> `sub_001189C0`) and a current weather entry of
  type 5 (`0x0011BC4F`, `0x0011BCB5`); in a MapMaker map the list is the type-0
  default `0x004E63F8` (`sub_0023F390` zeroes the scene-100 level data
  `0x0078CC00`; `sub_0011B310` resets the list).
- Its models are in `challenge/l_64_ch.pak`, `story/l_2_st.pak`,
  `story/l_26_st.pak` (not in any pak MapMaker play mounts).
- **Added:** Sandstorm. Nothing else is missing: rain, rain with lightning and
  snow are the stock rows.
- **Left out:** fog. Each level picks a fog entry (level `+0x14`); MapMaker
  takes the tileset's (`sub_00311870`). It is not a sky or weather choice and
  would need more storage than the 13 free flag bits (see Save format).
- Clouds: the per-map Clouds checkbox already exists (`docs/mapmaker/mapmaker-sky.md`).

## Load order of a MapMaker level (CONFIRMED)

`0x0003D590` `sub_001CDB80`: `sub_00062790` (pak mounts, `0x001CDB85`) ...
`sub_0011C9F0` (cloud setup, `0x001CDBB7`); `0x0003D5A3` `sub_001CDC50`
(scene 100 is the default case `0x001CDCD2`): `sub_00119960` (weather load,
`0x001CDCEB`), `sub_00085920` (tiles), ..., tail `sub_0006C920` ->
`sub_0023F390` (map start: level sky, model request, weather); `0x0003D5AD`
`sub_00063390` (load pass: reads every requested file); `0x0003D645`
`sub_001CDE30` -> `sub_0011B430` -> `sub_0011B310` (weather start). A restart
runs `sub_001CDD60`, which calls `sub_0011B310` directly (`0x001CDDB0`).
Restart audit after .31 (the bosses' restart bug, [Khallos](mapmaker-khallos.md)):
this area needs no restart hook. The mount (`sub_00062790`) and map start
(`sub_0023F390`) do not run on a restart, but what they decided stays true:
the paks stay mounted, the sandstorm flag and fog row 7 stay in the level
entry (the restart applies the row again, `sub_0011D240`), and the weather
start / list hooks run again on the restart path.

## Design (`TSFP_MAPMAKER_LIMITS=1`)

### Editor (scene 102)

- **rows** (`sub_00078590`, bind returns `0x00313768` / `0x00313843` /
  `0x00313906`): before the stock bind, `sub_00071800(label, value)` adds 28
  music rows (values 58..85), 17 sky rows (value = the sky record: 5, 35, 39,
  41..52, 56, 57) and Sandstorm (value 4). Labels: one `xbox_HeapAlloc` block
  (about 700 bytes, once per session). The rows come from the shared option
  pool (`sub_00071750`, 1500 nodes `0x006F73F0..0x006FE920`); Map Settings had
  about 100 rows.
- **compile** (`sub_003006C0`, after the original): reads the three editor
  fields; an extra value becomes the stock fallback in the rebuilt info
  (music -1; the sky's MapMaker twin, column `fallback` of
  `mapmaker_sky_extras`; weather 0) and its row number goes into the info
  flags. Any other out-of-range music/weather value is also replaced by -1 / 0.
  The fallback is written with the switch off too; the bits only with it on.
- **load** (`sub_00301A90`, after the original, scene 102): the bits turn the
  editor fields back into the extra values (unknown row numbers are ignored).

### Save format

Info flags dword (data `+0x244`). Stock bits: 0x1, 0x2..0x4000, 0x8000,
0x10000, 0x20000; Clouds 0x40000 (audit in `mapmaker-sky.md`). New:

| bits | mask | value |
|---|---|---|
| 19..23 | `0x00F80000` | extra sky row 1..17 (`mapmaker_sky_extras`) |
| 24..28 | `0x1F000000` | extra music row 1..28 (`smx_music`) |
| 29..30 | `0x60000000` | extra weather row 1 (Sandstorm) |
| 31 | `0x80000000` | free |

Maps keep version 0x12D, size and tier; nothing else changes. **A stock or
older build** reads only the fallbacks: the map loads and plays with the
twin MapMaker sky, the tileset's music and no weather; its editor shows those
choices, and re-saving there drops the bits (its rebuild rewrites the
dword). System Link peers without this build see the same fallbacks.

### Play (scene 100)

**Playtest v0.4.0-mapmaker.31** (`logs/m31.err`, ignored): "Sandstorm still
isn't visible", with `grains alive 60 of 60`, view 0 flagged, player 1
outdoors 1.00 and both texture handles set. The stock draw path (static,
XBE; bytes checked in `test_stock_sandstorm_draw`):

- **Draw (CONFIRMED).** `sub_0011BC60` -> `sub_0011B110` (type 5, view
  flag `[0x007AC82C+4v]`): first the haze `sub_0011A930(eax 0x7F5A1E05, 8)`
  (`0x0011B13E`), then for each grain whose age `+0x20` >= 0 one sprite
  `sub_0001D5F0(x, y, z, colour, size, angle, 0)` with t = age/3 (life 3 s,
  `0x0011B0E0`), size t + 1.5, angle (t + 1) x `+0x18`, colour
  `sub_00154FE0(0x7F643C10, fade)` (`0x0011B23D`), fade
  (sin((1-(1-t)^2) 2pi - pi/2) + 1)/2 (peak near 0.9 s). No room, zone or
  frustum test: position and age are the only grain fields it reads (spin
  `+0x1C` and velocity are the update's). Our grains fill every field as the
  stock emitter does (y is up; box around the player, not behind or below).
- **Cause: nearly transparent (CONFIRMED).** `sub_00154FE0` scales only the
  colour's low byte (alpha) by the clamped fade (`0x00155008`, `0x00155028`):
  a grain is at most 16 of 255 opaque. The haze weight is eax's low byte (5):
  each quad blends four layers with alpha ((sum of layer alpha) x 5 + 1) x
  0.125 (`0x0011AB4C..0x0011AB82`), at most 2 of 255. .31 also spread its
  grains over 24 x 24 m and made 60 a second in the ring of 60, so each was
  overwritten after 1 s.
- **Story look (CONFIRMED data, INFERRED effect).** Scenes 2/26/64 use fog
  row 7 (level `+0x14`; table `0x00515778`, 0x3C a row, `sub_0006A710`):
  colour 0x2F210F (x2 at `sub_0011CF00`), 25..235.5 m. MapMaker's map start
  writes the tileset's row (12..15, `sub_00311870`, `0x0023F40A`); the level
  start applies the row (`sub_001CDE30` -> `sub_0011D3C0` -> `sub_0011D240`
  -> `sub_0011CF00` -> `sub_00022A20`). Their light block (`+0x2C`, 15
  floats: warm 0.72, 0.51, 0.185) is not copied (MapMaker tilesets mostly
  have no directional light; left out).
- **Scene-2 props (SUSPECTED main stock visual, not changed).** The four
  `intro_sandstorm_N` models are sand sheets placed for scene 2's terrain:
  their file headers hold centre/extent floats such as (-1.5, 4.2, 24.9) r
  32.8 and (-41.3, -6.0, -222.4) r 58.7 (INFERRED meaning). Only a matching
  emitter scrolls them (`sub_00153550(mat, phase, 0, 0)` -> node 0,
  `0x0011AF5E`), sets instance alpha `+0x1FA` = gust x 127 and flag
  `+0x1F6` 0x20 (`0x0011AF9D..0x0011AFAB`). Moving them to the player needs
  the per-room prop lists (`sub_000A79B0`, a prop is drawn only when listed
  in a drawn room, `sub_0009FBE0` lists it from its box at `+0x64`): not
  attempted blind. The probe logs where they are and their alpha.
- **Fix (.32):** `smx_sand_live()` (armed by us, scene 100, flag set) gates
  all three. haze (WRAP `sub_0011A930`, return `0x0011B143` only): weight 5
  -> 0x50 (quad alpha up to 40). grain_colour (WRAP `sub_00154FE0`, return
  `0x0011B247` and colour 0x7F643C10 only): alpha 0x10 -> 0x60. level (after
  the map start): fog row 7 in scene 100's level entry `+0x14`. Grains: one
  per two update steps (30 a second, a 2 s life in the ring: fade 0.12 when
  replaced), at most 4 a frame, in a 10 x 10 m box 4 m upwind, y -0.7..2.3 m.
  Snow (`0x0011B95C`, `0x0011BBA2`), menus, scene 2/26/64 and maps without
  the sandstorm keep the stock values.

**Playtest v0.4.0-mapmaker.28** (`logs/m28-sand.err`, ignored): the extra
skies render; "Sandstorm does not appear to be working". The log has the
paks mounted, the flag and models, scene 2's type-5 list kept and every
weather frame on it, so the stock sandstorm code ran. Causes (static, XBE):

- **Sandstorm tied to scene 2's rooms (CONFIRMED).** The four emitters at
  `0x004E62E8` (0x44 bytes: room ids `+0..+0xC`, 0 = none; model `+0x10`;
  rate `+0x14`; wind `+0x18..+0x20`; gust `+0x30`; prop `+0x34`) hold rooms
  {2,3,7}, {6,26,27} (wind 0.53, 0, -0.85), {5,4} (1, 0, 0), {19}
  (-0.56, 0, 0.83). `sub_0011ACF0` spawns grains only for a room of the
  player's room list (`+0xE0A` count, `+0xE0C` words) that one of them names
  (`0x0011AD69`), at a random point of that room's floor zones
  (`sub_001186F0` -> `sub_00118670`, `sub_00087840(room, 1)`: the level
  object table `[0x007B9784]`), and only there animates and fades in the
  emitter's prop (`0x0011AF5E..0x0011AFAB`); `sub_0011A600` matches the
  player's room `+0x2A0` for the gust (`0x0011A66B`). The props are
  created at the origin (`sub_001189C0`, `0x00118A0C`), i.e. scene 2's own
  sand sheets (INFERRED from the names `intro/intro_sandstorm_N`). MapMaker
  rooms are piece numbers 1..N, so no grain was made (or, in a map with
  those piece numbers, scene 2's props would fade in at the origin).
- Checked and not a cause: the draw `sub_0011B110` needs only the view's
  flag `[0x007AC82C+4v]`, which `sub_0011A600` sets for every player the
  weather runs for (`+0x21C` sign bit and 0x400, as rain, `0x0011A68F`);
  grain and haze textures are global ids 0x303 / 0x328 (`sub_00080F20`,
  handles at `0x004D2D64 + id*0x18`), each used by other effects too; the
  haze alpha is `[0x007AC844+4p]` (1 outdoors by the rain test
  `sub_00119B50`, which has a MapMaker branch) times max(0.65, gust). The
  ring is 60 grains of 0x24 (`[0x007AC874]`, index `[0x007AC878]`): pos,
  velocity `+0xC`, angle `+0x18`, spin `+0x1C`, age `+0x20` (-1 dead, dies
  past 3 s).
- **Fix** (`pc_mapmaker_skymusic_weather_frame`, the existing `sub_0011BBC0`
  wrapper, sandstorm maps only): the 16 emitter room words read 0 while the
  stock frame runs and are put back right after (story levels see them
  unchanged); then, while the entry has type 5, one grain per update step
  (`[0x004E7944]`, at most 4) for each player the weather runs for who is
  outdoors, in a 24 m box around the player, 6 m upwind, with the stock
  emitter's velocity rule (wind x 3..6 m/s + jitter, 0..1.2 m/s up), angle,
  spin and age 0, into the stock ring; the stock code ages, moves and draws
  them. Host random numbers: the guest generator `[0x007497CC]` is not
  advanced. Wind: emitter 1's. Grains show only outdoors, like rain.
- **Sky texture probe (CONFIRMED wrong).** It read the first word of each
  16-byte texture entry as the id. In memory the texture pass has replaced
  that word by the loaded-table row (`sub_00018B90` writes -1, then the row,
  through its third argument, `0x00018BA4`/`0x00018C91`; `sub_00063900` passes
  the entry, `sub_000637E0` queued it): .28 logged rows 0x692..0x698, while
  the files' ids are 24-bit (Spaceport: 0x855357, 0x876E22). The counts 2,
  4, 6, 7 were each sky's own (Spaceport, Chinese, Sniper, Vietnam), so the
  "growing list" was row numbers, not a list kept across levels. Fix: id =
  second word (both words are equal in all 21 files), row = first; loaded
  when that row holds the id, else the id anywhere in the table; -1 rows no
  longer end the list; the count is reset at each level.

**Playtest v0.4.0-mapmaker.27:** "Most of the skies are still not rendering
at all. Some have elements that are rendering in part." Static cause
(CONFIRMED in the XBE and the pak indexes, not yet played):

- A sky model's textures are separate files `textures/<id>.xbt` (id = the
  24-bit word at the start of each 16-byte entry of the list at model file
  `+[file+0]`). The load pass `sub_00063390` reads the model and, since every
  sky file's texture block `+0x48` is 0x10 (`0x00063543`, all 27 skies
  checked), registers its textures at once (`sub_00096220`, `0x0006355D`):
  nothing is deferred to the prop's creation, so the mothership's fix
  (`sub_00096330(file, 1)`, f75895d9) does not apply. Each texture is
  queued (`sub_000637E0`, name `host0:textures/%06x.xbt`) and the texture
  pass `sub_00063900` (`0x0003D5E5`, before the views at `0x0003D645`) loads
  it from any mounted pak (`sub_00018B90`: size, data, then the id into the
  loaded table `[0x004B85C8]`, rows of 0x34, id `+0x1C`). A texture in no
  mounted pak is skipped (size 0), and its material has no texture.
- **No level pak holds any sky texture.** They are in the level's memory
  pak (`arcademem/l_20_arm.pak`, ...), which a story level mounts whole
  (`sub_00062610(scene, 1)` -> `sub_000622D0(path, 1, 1)`, `0x000628B1`;
  scene 100 gets `l_100m.pak` there). MapMaker play mounted only the level
  pak: 14 skies lacked textures, 9 all of them (not drawn) and 5 only some
  (Train, Time to Split, Vietnam, Spaceport, NF1 also use textures the stock
  play paks share: "in part"); Virtua, Prison and Sniper lack none. The sandstorm's four models each need one
  texture of `challengemem/l_64_chm.pak` too (the .25 "flag set, nothing
  shown").
- Checked and not a cause: sky records hold one model (`+4`) and no second
  model or layer (`0x005149A8`, 0x34 bytes, dumped for all 17); the room
  mask switch (`pc_mapmaker_skymask.inc`) is a dev switch, off by default;
  no scene-gated sky draw code was found (not searched exhaustively).
- **Fix:** each row names its textures pak (`mem` in `mapmaker_sky_extras`,
  0 for the three without), the sandstorm too; `smx_mount_pair` mounts the
  model's pak, then the textures pak, index-only, and the extra sky or the
  sandstorm is used only when both are mounted (else the stock choice,
  logged). The probe lists, once per level, each of the model's textures
  missing from the loaded table and a loaded/total count.

**Playtest v0.4.0-mapmaker.25 (mapmaker-recon 54129718):** the new music
played; the extra skies and the sandstorm did not show. Static re-check:

The .25 log (`logs/mapmaker25-bossmap.err`, ignored) has, per level, two
`mounted an extra sky or weather pak` lines and `sandstorm on (1319, 4)`: in
that test the paks did mount in the old place and the sandstorm was armed, so
the mount-timing change below is a hardening, not the cause there. The
sandstorm was then undone by the level setup's list reset (second item). The
sky hook logged nothing in .25 (it had no success line), so whether the
model was requested and read is what the new lines show. A further suspect
for the sky: story sky models are small (`skycoldnight` 1.8 KB, `skycastle_xbox`
11.7 KB) and their textures are separate pak entries; if a level's textures
are loaded from a level-wide list rather than through the model, an extra
sky would be built but untextured or invisible (INFERRED, not traced). The
weather per-frame path has no scene gate (`sub_001CE420` -> `sub_0011BBC0`
every frame; `sub_001CEBC0` -> `sub_0011BC60` draw); the sandstorm runs only
while the current list entry has type 5.

- **Mounts in the wrong place (sky and sandstorm, the likely cause).** The
  first build mounted both paks in the `sub_00062790` hook, the only hook that
  reads the map before `sub_001CDC50`, and only when `[0x0079094C]` was 100.
  But `sub_00062790` mounts for a *pending* scene: it takes the scene byte
  `[0x007DE394]` instead of `[0x0079094C]` while `[0x007DE398]` has 0x800000
  (`0x000627A0..0x000627B4`; `sub_00067280`, called just before at
  `0x0003D58B`, sets that flag and copies the next game state). A level
  started that way, or with the container not yet holding the new map,
  mounted nothing and every later step skipped silently. Music reads the map
  much later (`0x00045FB7`) and worked. **Fix:** `sub_00062790` now only starts
  a new level (resets the per-level state, logs); the paks are mounted by the
  first step that needs them, where the scene is 100 and the map is read: the
  cloud setup (`sub_0011C9F0`, through `mapmaker_sky.extra_prepare`), the map
  start (`sub_0023F390`) for the sky, the weather load (`sub_00119960`) for
  the sandstorm. All three run before the load pass (`0x0003D5AD`).
- **Sandstorm list reset (sandstorm, CONFIRMED).** After the weather start
  (`sub_001CDE30` -> `sub_0011B430` -> `sub_0011B310`, `0x0003D645`) the level
  setup `sub_0006EC10` (`0x0003D64A`) -> `sub_0006D7A0` -> `sub_00119AB0(count,
  list)` (its only caller, `0x0006E87A`) passes scene 100's empty list
  (`0x0078CC00`, zeroed by `sub_0023F390`), which selects the type-0 default
  `0x004E63F8`: the type-5 entry set at the weather start was undone before
  the first frame. **Fix:** `pc_mapmaker_skymusic_weather_list` gives that
  call count 1 and scene 2's list when the sandstorm is armed and its buffers
  exist. The restart path (`sub_0006ED70` at `0x0003D4E6`) uses the same call.
- Checked and unchanged: the sky prop is built per view by `sub_0011D2C0`
  (`sub_000CC920` <- `sub_000CCA10`, `0x001CDF9A` in `sub_001CDE30`, after the
  load pass) from the level entry's sky (`sub_0006A6C0`: `[0x00518788]` ->
  record -> model `+4`), which `sub_0023F390` sets (`0x0023F48F`) and nothing
  writes later; `sub_000A5E40` requests in scene 100 regardless of the
  streamed flag (`0x000A5E80`) through the relocated map `0x008CA000`
  (`rooms.json`, `0x000A5E5A`); scene 100's resource table comes from the full
  prop table (`sub_00242780` scene-100 branch). The tail jump
  `sub_0011B430` -> `sub_0011B310` has no arguments.
- **sky_built** (`sub_0011D2C0`, probe, after the original, at most two lines
  per level): logs the level's sky record, its model, the view's prop and the
  prop's model (`+0x94`), the model's resource row, its loaded flag (`+4`) and
  its request's data (`sub_000631C0(row path) +0x10`), so the next playtest
  shows whether the model file was read.

Steps now:

- **mount** (`sub_00062790`, after the original): a new level; nothing is
  mounted here.
- Mounting (first of the steps below that needs it): for an extra sky its
  model pak and then its textures pak (table below), for the sandstorm
  `pak/challenge/l_64_ch.pak` and `pak/challengemem/l_64_chm.pak`; used only
  when both are mounted. Index only, like `chr.pak` (`sub_000622D0(path, 0,
  1)`: the file stays open and entries are read on demand; textures are read
  the same way, `sub_00062CA0`/`sub_00062C30`/`sub_00062D20`), at most four
  (Time to Split + Sandstorm share both), skipped when already mounted
  (`sub_00062260`), tried once per level and row, refused when fewer than 8 of
  the 64 slots (`paktable.json`) would stay free. The stock bulk unmount
  (`sub_00062720`) removes them with the level. The path strings live in one
  heap block (32 bytes per pak, room for 36), kept for the session.
- **cloud setup** (`pc_mapmaker_sky_clouds`, existing hook): with an extra
  sky (its pak mounted now) the level's sky is that record while
  `sub_0011C9F0` runs, so its own cloud layer (record 5) is used; story
  records are never patched. The Clouds checkbox only patches MapMaker
  records 58..67, as before.
- **level** (`sub_0023F390`): while the original runs, info `+0x10` holds the
  extra record, so the stock code sets the level sky and requests its model
  (`0x0023F4BA`) before the load pass; the word is put back after. Not done
  (fallback sky stays, logged) when the pak did not mount or the resource map
  has no row for the model.
- **weather_load** (`sub_00119960`, after): sandstorm map: mount its pak,
  `[0x0073D9AC]` = 1 and the four models requested (`sub_000A5E40`), as
  `0x00119A47..0x00119A65`; refused if a model has no resource row.
- **weather_start** (`sub_0011B310`, after) and **weather_list**
  (`sub_00119AB0`, before): when we armed the sandstorm and its buffers exist
  (`[0x007AC824]`, `[0x007AC874]`), the weather list is scene 2's
  `0x00507898` (one entry, type 5): `[0x007AC804]`, count `[0x007AC800]` = 1,
  index `[0x007AC808]` = 0.
- **music** (`sub_00045DE0`, return `0x00045FBC`, eax = -1): eax becomes the
  extra track's id.

| sky | model pak | textures pak (textures not in stock paks) | fallback |
|---|---|---|---|
| 5 Cold Night | arcade/l_20_ar.pak | arcademem/l_20_arm.pak (6 of 6) | 64 |
| 35 Castle | arcade/l_59_ar.pak | arcademem/l_59_arm.pak (2 of 2) | 58 |
| 39 Train | cutscene/l_33_cs.pak | cutscenemem/l_33_csm.pak (2 of 3) | 63 |
| 41 Virtua | arcade/l_38_ar.pak | none (its 1 is in gun.pak's MapMaker twin) | 60 |
| 42 Prison | arcade/l_29_ar.pak | none (6 of 6 stock) | 67 |
| 43 Time to Split | challenge/l_64_ch.pak | challengemem/l_64_chm.pak (1 of 5) | 61 |
| 44 Nemo | story/l_81_st.pak | storymem/l_81_stm.pak (1 of 1) | 61 |
| 45 Venice | arcade/l_46_ar.pak | arcademem/l_46_arm.pak (6 of 6) | 59 |
| 46 Street War | cutscene/l_40_cs.pak | cutscenemem/l_40_csm.pak (30 of 30) | 61 |
| 47 Vietnam | arcade/l_48_ar.pak | arcademem/l_48_arm.pak (1 of 7) | 62 |
| 48 Temple | arcade/l_49_ar.pak | arcademem/l_49_arm.pak (10 of 10) | 62 |
| 49 Spaceport | arcade/l_15_ar.pak | arcademem/l_15_arm.pak (1 of 2) | 60 |
| 50 Depot | challenge/l_76_ch.pak | challengemem/l_76_chm.pak (2 of 2) | 63 |
| 51 Sniper | arcade/l_53_ar.pak | none (6 of 6 stock) | 64 |
| 52 NF1 | challenge/l_66_ch.pak | challengemem/l_66_chm.pak (1 of 2) | 61 |
| 56 Chinese | arcade/l_25_ar.pak | arcademem/l_25_arm.pak (4 of 4) | 66 |
| 57 Horror | arcade/l_39_ar.pak | arcademem/l_39_arm.pak (4 of 4) | 59 |
| Sandstorm | challenge/l_64_ch.pak | challengemem/l_64_chm.pak (1 per model) | - |

Pak choice: arcade, then challenge, cutscene, story; the textures pak is that
level's own memory pak. Every extra pak shares with the stock play paks
(now including `arcademem/l_100m.pak`) only byte-identical entries
(CONFIRMED, `test_pak_assets`, `test_sky_texture_paks`), which matters
because the pak lookup tries the last pak that answered first
(`sub_00062B70`, `[0x004D19F4]`/`[0x004D19F8]`), so a shared name with
different content could otherwise change a stock asset. The old INFERRED
"a level pak holds the sky model's textures" was wrong: no level pak holds
any sky texture (`.27` cause below).

## Wrappers for the integrator (area `sky`)

```
WRAP sub_00078590 -> pc_mapmaker_skymusic_rows           (chained: bosses, sets, weaponset; return-address test, order free)
WRAP sub_003006C0 -> pc_mapmaker_skymusic_compile        (chained: container, aitemplates, sky compile, roof; acts after the original, any order)
WRAP sub_00301A90 -> pc_mapmaker_skymusic_load           (chained: editor, aitemplates, roof; acts after the original, any order)
WRAP sub_00062790 -> pc_mapmaker_skymusic_mount          (chained: tank, mothership, khallos; acts after the original, any order)
WRAP sub_0023F390 -> pc_mapmaker_skymusic_level          (chained: weaponset preload; order free)
WRAP sub_00119960 -> pc_mapmaker_skymusic_weather_load   (new in .25)
WRAP sub_0011B310 -> pc_mapmaker_skymusic_weather_start  (new in .25; sub_0011B430 reaches it by a tail jump, sub_001CDD60 by a call)
WRAP sub_00045DE0 -> pc_mapmaker_skymusic_music          (new in .25; register argument eax, must see the guest g_eax at entry)
WRAP sub_00119AB0 -> pc_mapmaker_skymusic_weather_list   (NEW after .25; cdecl count, list; one caller 0x0006E87A; edits its two stack arguments before the original)
WRAP sub_0011D2C0 -> pc_mapmaker_skymusic_sky_built      (NEW after .25; cdecl view; one caller 0x000CC961; probe after the original)
WRAP sub_0011BBC0 -> pc_mapmaker_skymusic_weather_frame  (NEW after .25; weather frame update, no args, every frame; callers 0x001CE55F/0x001CE596; .28 fix: sandstorm maps only, hides the emitter rooms during the original, then adds grains; one compare when no sandstorm)
WRAP sub_0011A930 -> pc_mapmaker_skymusic_haze           (NEW after .31; register argument eax = colour | weight, must see the guest g_eax at entry; return-address test 0x0011B143; edits eax before the original)
WRAP sub_00154FE0 -> pc_mapmaker_skymusic_grain_colour   (NEW after .31; cdecl colour, fade; many callers (menus too), return-address test 0x0011B247 first; edits its first stack argument before the original)
```

All call their original exactly once (no `MAPMAKER_GUEST_RET`). Guest
functions called: `sub_00071800` (list row; the tilesets hook on it acts only
on its own return address), `sub_00062260` (is a pak mounted), `sub_000622D0`
(mount), `sub_000A5E40` (request a model), `sub_000631C0` (find a model's
request; probe only); all have link stubs in
`tests/mapmaker/mapmaker_area_stubs.h`. Host: `xbox_HeapAlloc` (labels,
paths). Include `pc_mapmaker_skymusic.inc` after `pc_mapmaker_sky.inc` (it uses
`mapmaker_sky_extras`, `mapmaker_sky_info`, the `mapmaker_sky` state, and sets
`mapmaker_sky.extra_prepare`). For the two new wrappers the integrator also
adds prototypes in `pc_display_menu.h`, pass-through stubs in the non-display
branch of `pc_menu_mouse_guest.inc` and the generated wrapper patch; none of
those files, nor `docs/agent-context`, was edited here.

Log lines (stderr, `[mapmaker] skymusic: `, at most 96 a session), in level
order:

- `new MapMaker level (scene now, mounted paks) (S, N)` at the pak mounts
  when the level being loaded is scene 100 (S is `[0x0079094C]` at that time);
- `map has extra sky (row, record) (K, R)`, then `mounted pak (slot): pak/... (N)`,
  `pak already mounted: ...`, `pak mount table too full; not mounted: ...` or
  `pak did not mount: ...` for the model pak and then the textures pak, and
  `textures pak not mounted; the model's textures would be missing: pak/...`
  when only the second failed (the stock sky or no sandstorm is kept);
- `sandstorm on, models requested (first, count) (1319, 4)` or
  `sandstorm pak or its textures pak not mounted; no sandstorm` / `sandstorm model has no resource row ...`;
- `map start: level sky record, model requested (R, M)` or
  `map start: extra sky pak not mounted; MapMaker sky kept` /
  `map start: extra sky model has no resource row; MapMaker sky kept`;
- `music: extra track (row, id)`;
- `sky built: record R model 0x.. prop 0x.. prop model 0x.. row N loaded L request 0x.. data 0x..`
  (one line per view, two at most); after the first, `sky texture not loaded:
  textures/xxxxxx.xbt (row N, record R; in no mounted pak?)` for each missing
  texture (eight at most) and `sky textures loaded (loaded, total) (L, T)`, or
  `sky textures not checked: model file not read (record, file)`;
- `weather start: sandstorm list set ...`, `level setup: sandstorm list kept (count, list) (1, 5273752)`,
  or `sandstorm buffers missing; not drawn (a, b)`.
- `map start: extra sky row K resolved to record R model 0x.. (resource row N), map's stock sky S, level sky before B`;
- `weather frame F: list 0x.. index I type T sandstorm 1 buffers ..` on the
  1st, 60th and 600th frame of a sandstorm level (type 5 draws), each
  followed by `sandstorm drawn: grains alive A of 60 (this frame N, level L),
  views flagged a b c d, player 1 outdoors X, textures 0x303 H 0x328 H,
  scene-2 props ...`, then `sandstorm grain G: pos x y z velocity .. angle
  .. spin .. age A; player 1 at x y z` (the newest grain), `sandstorm haze:
  view 0 layer 0 offset .. depth D alpha H; weight 80 (stock 5), grain
  alpha 0x60 (stock 0x10), boosted draws haze N grains M, fog row 7
  (tileset's T)` and four `scene-2 prop K: 0x.. at x y z, rooms R, instance
  0x.. alpha A flags 0x..` (or `none`);
- `map start: sandstorm fog row (row, tileset's row) (7, T)`.

Reading the next log: no `map has extra sky` line = the map's row bits are
not seen in play; `mounted` but no `map start` line = the map start did not
run the hook; `map start` with record R but `sky built: record` other than R
= something rewrote the level sky; `sky built` with `data 0x00000000` and
`loaded 0` = the model file was not read from the pak (pak path or contents);
`sky textures loaded` below the total = a missing textures pak or texture
(the line before names it); all loaded but no visible sky = drawing (view,
mask, draw distance), not assets. Sandstorm: `grains alive 0` with `player 1
outdoors 0.00` = the player stood indoors (the rain test); grains alive but
`views flagged 0` = the stock player test failed; a texture handle of
0xFFFFFFFF or 0 = the global grain/haze texture is not loaded; `boosted
draws haze 0 grains 0` = the two new wrappers are not wired (or the stock
draw did not run); a grain more than 10 m from `player 1 at` or with age
past 3 = placement or the update; haze `alpha 0.00` while outdoors = the
haze layers (`sub_00117D60`) are faded by the view vectors; `fog row` not 7
= the map start did not run the hook.

## What the user tests

1. Build with the wrappers; `TSFP_MAPMAKER_LIMITS=1`.
2. Editor, Map Settings: the music list ends with Front End Menu ...
   Cutscene: Anya's Lab; the sky list with Story: Cold Night ... Arcade:
   Horror; the weather list with Sandstorm (log: `rows offered`).
3. Pick Story: Castle, Cutscene: Prologue, Sandstorm; save; load the map
   again: the three rows still show them. Play: the castle sky (look up in an
   Open air tile or at the map edge), the cutscene track, sand blowing past
   outdoors (none inside, like rain; the `sandstorm drawn` lines). Send
   every `skymusic:` line. Try Story: Time to Split with Sandstorm (shared
   paks), Story: Cold Night, Arcade: Temple, Story: Street War (no texture
   in stock paks before), Arcade: Vietnam (was in part), Arcade: Prison (no
   textures pak: control), split screen 2 players. The `sky textures
   loaded (L, T)` line now names 24-bit ids and T is that sky's own count.
4. Quit back to the editor: still the extra choices; play again (second level
   load: `pak already mounted`).
5. A session without the switch (or an older build): the same map loads and
   plays with the MapMaker twin sky, tileset music and no weather; no crash.
6. Story smoke test afterwards: scene 2 (sandstorm, intro sky) and one level
   using a sky from the table look as before (the emitter rooms are hidden
   only during a MapMaker sandstorm frame; the haze, grain alpha and fog
   changes apply only to scene 100).
7. Sandstorm (.32): outdoors, sand puffs drift past within a few metres and a
   sandy haze scrolls across the view; distant tiles take a brown fog;
   indoors no puffs (the haze fades with the rain test). A snow map and the
   menus look as before. Send the `sandstorm grain`, `sandstorm haze` and
   `scene-2 prop` lines.

## Not verified / risks

- Sandstorm (.32, not played): the alpha values (grains 0x60, haze weight
  0x50) are chosen, not taken from a stock level; too strong or too faint is
  a constant change. The story sandstorm's sheets (the scene-2 props) are
  still not shown in MapMaker, so it looks like puffs, haze and fog rather
  than drifting sand curtains. Fog row 7 is fainter at mid range than some
  tilesets' own rows (12: 27.9..90 m), so the fog change is mostly colour.

- Played (.25, .27): music works; skies not or partly drawn, the textures
  pak fix (.27 cause above) is not played yet. Unknowns: that index-only
  textures load as the whole-mounted memory pak's do in story (same lookup,
  INFERRED; the texture probe tells); scene-gated sky extras in story level
  code (none found, not searched exhaustively); the sandstorm's look over MapMaker tiles
  (it follows the camera, INFERRED) and its ambient sound (none is started:
  the type-5 entry's sound fields were not decoded); whether the jingles loop.
- Mounting moved from the pak-mount step into the load (`sub_001CDB80` /
  `sub_001CDC50`); the index goes on the level heap like the stock mounts
  (INFERRED harmless: the tank's pak is mounted at the end of the mount step
  the same way).
- Mount budget: each extra pak takes one of the 64 slots (index memory only,
  some KB); a textures pak holds 187..412 entries.
- English labels only, no help text.
