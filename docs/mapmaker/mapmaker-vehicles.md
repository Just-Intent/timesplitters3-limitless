# MapMaker story vehicles: the Machine Wars tank and the Scotland the Brave truck

Status 2026-11-07: implemented in `src/interface/pc_mapmaker_vehicles.inc`
(area `vehicles`), fixture-tested (`tests/mapmaker/mapmaker_vehicles.c` +
`tests/mapmaker/test_mapmaker_vehicles.py`: 73 mutations, plus decode, US XBE
data and game-pak checks of every site below). Played in v0.4.0-mapmaker.61
(user): all three sank into the floor; both trucks rose onto the floor once
entered and drove well, the Vickers gun works; the tank sank further and
could not be entered. Fixed after .61 (below, no new wrapper); not played
since. Switch:
`TSFP_MAPMAKER_LIMITS=1` (editor and play). No guest-code table, no fork
chunk, no new save size. Story assets wave 3, items 19 and 20
([inventory](mapmaker-story-assets.md)). The castle boss tank (prop 0x281,
[tank](mapmaker-tank.md)) is unrelated.

CONFIRMED = decode, XBE data or pak index (the test names the site);
INFERRED = reasoning, needs a playtest.

## What the player gets

The MapMaker vehicle (Objects, category 35, catalogue item 0x2B) gets an
options menu (category 35 has none in stock) with **Vehicle**:

| choice | what you drive | guns |
|---|---|---|
| Theme vehicle | the stock buggy or jeep of the map's theme | none |
| Future tank (Machine Wars) | `rbcar/futuretank` (model 0xB), tracked | main gun (gun type 0x3B, model 0xC) and AA gun (0x3C, model 0xD) |
| Castle truck + Vickers gun | `rbcar/truck` (model 7) | a gunner seat on the Vickers gun (0x2A, model 0xE) |
| Castle truck | the same truck | none |

- These are the story vehicles' own defs, builders and gun templates; only
  where they stand comes from the map. A stock build (or this one without
  the switch) plays the map with the theme vehicle: the choice is one byte
  the game ignores.
- **Up to 4 vehicles per map in all** (any mix), the stock vehicle pool. The
  limits switch used to raise category 35 to 8; that is removed (see "Pool").
- Players get in and out of the tank as of a buggy (the story's no-player
  flag is cleared).
- The tank, like the stock buggies, cannot be destroyed in a MapMaker map
  (see "Not changed").

## Why it works (CONFIRMED)

- **Save byte.** Item 0x2B takes the compiler's default branch (0x00300FFA:
  +0..+0xA zeroed); only 0x35..0x37 write +0xC. The editor loader reads +0xC
  only at 0x003021C6 (piece colour) and 0x003023AC (0x35..0x37), the fill
  only at 0x00241BFB/0x00241C0E (0x35..0x37). Byte +0xC = 0 theme vehicle,
  1 tank, 2 truck with gun, 3 truck. Editor node +0x30 holds `0x5648 0000 |
  variant` ('VH'; storyprops uses 'SP' for drums and fixed guns, weaponset
  'WS' for pickups). Nodes and records pair by piece and item position on
  save and load, as storyprops. `.tsmap` export and System Link send the
  container bytes, so the choice travels with the map.
- **The fill.** `sub_00240C50` case 0x2B (0x00241CC4) writes a 0x40-byte
  vehicle record at ebp: +0 -1, +4 flags, +8 model (`sub_002F7790`,
  0x00241D16), +0xC position, +0x18 rotation, then +0x24 def =
  `sub_0033DDA0(id, theme)` (0x00241D39; 3 for model 0x892, else 5), +0x28 0,
  +0x2C 1, +0x30 -1 (0x00241D3F..0x00241D4D); +0x34 is never written.
- **Create.** `sub_0006D7A0` makes the object from +8 (`sub_000A6F90`,
  0x0006E934), then `sub_00182A20(object, record)` (0x0006E949), which stores
  the record at +0x2D8, takes a block from the pool and calls the def's
  builder (def + 0xB0): def 0 truck `sub_00180BA0`, 2 tank `sub_00181D40`,
  3 jeep, 5 buggy. Def table `0x004E84A8 + def*0xD8`.
- **The story records** (paks, `test_story_vehicle_records`): l_5_st
  'TANK'/'TNK2'/'TNK3' model 0xB, def 2, +0x28 4, +0x2C 4, +0x30 1, +0x34 2;
  l_4_st 'TRCK' model 7, def 0, 0, 1, +0x30 0, +0x34 -1, and a second truck
  with +0x30 -1. The tank builder reads +0x28 (tracks row 4 of 0x004E8270:
  16, 0.2, 0.2, 0.95, ...; MapMaker's 0 is the buggy's) and the gun
  templates +0x30/+0x34 (`sub_00180920`); the truck reads +0x28/+0x2C
  (wheels) and adds the Vickers gunner seat when +0x30 >= 0 (template 0
  always, seat anim 0x83 'gunner', 0x00181181..0x0018122F).
- **Guns.** Templates `0x004E8300 + i*0x8C`: 0 Vickers (model 0xE, gun type
  0x2A), 1 tank main gun (0xC, 0x3B), 2 tank AA gun (0xD, 0x3C). They become
  ordinary turret objects (`sub_000B3B80`), so they share the 80 gun slots:
  72 placed turrets + 4 tanks x 2 = 80 still fits.
- **Seats and anims.** The tank's seats play no character anim (all ids 0,
  as in the story). The truck's 0x78..0x83 (`drivetruck`, `getintruck`, ...,
  `gunner`) are all in `pak/anim/animcar.pak` (table 0x0047FDF0, 27 entries,
  registered when that pak is mounted, 0x001CA492), which scene 100 mounts for
  every map (0x001C878A) and the stock buggy already uses (0x7F..0x8A). This
  closes the inventory's open point "truck seat anim pak".
- **Driving.** Entering, driving, the gunner seat and exiting are the generic
  vehicle code. The tank is tracked (def +0xA8 bit 0 -> block +0x58), so the
  PC tracked-driver adapter `src/input/pc_tank.h` and the vehicle gun mouse
  aim (`src/input/pc_mounted.h`) apply as in Machine Wars.
- **No story-only state.** Within the vehicle code (0x00164000..0x00183000)
  the scene is read only by the generic physics (also used by the stock
  buggies in scene 100) and two scene-5 gates (below). The record is read
  back only by the reset/save code `sub_0017F420`..`sub_0017F700` (+0xC
  position, +0x18 rotation); nothing reads +0x38/+0x3C.

## Play, step by step

1. **mount** (`sub_00062790`, every level load): after the original, only for
   a map with tanks `pak/stream/lv5/prop1.pak` (38 files: models 0xB, 0xC,
   0xD, gibs 0x54D..0x54F and all their textures), only for trucks
   `pak/stream/lv4/prop3.pak` (27 files: 7, 0xE and their textures). Kept
   open (`sub_000622D0(path, 0, 1)`, index only). Every file they share with
   another pak (60 and 63) is byte-identical there, so they cannot shadow a
   MapMaker file. No memory pak is needed: unlike the storyprops models,
   these textures are in the same pak.
2. **preload model** (`sub_0006C090` at 0x0023F5DB, esi = the item): the
   variant's model instead of the theme buggy, when its pak is mounted and
   every model it creates has a scene-100 resource row (`sub_000A5E40`
   indexes the row without a check).
3. **def at the preload** (`sub_0033DDA0` at 0x0023F637): the variant def goes
   to `sub_00180700` (its 7 sounds; def 2 also requests the gibs), and the gun
   templates are preloaded the story way, `sub_000B3B20(template)` (model,
   gun type `sub_001B6010`, sounds), as `sub_001807A0` does in story levels.
4. **textures** (`sub_00063900`, before the texture queue loads): each
   vehicle and gun model the load pass deferred (request +2 & 0x10, the
   storyprops finding) is registered the load pass's way,
   `sub_00096220(path)`; the gibs are not (never drawn here).
5. **def at the fill** (0x00241D39, esi = the item, ebp = the record): the
   variant def and model, only when the vehicle and gun models were read
   (resource +4, or the request's file +0x10). `sub_000B3B80` uses its turret
   object without a null test (0x000B3BBF), so a missing gun model would
   crash; the vehicle then stays the theme vehicle and is logged.
6. **create** (`sub_00182A20` at 0x0006E949): a record filled in step 5 (and
   still that def and model) gets +0x28, +0x2C, +0x30, +0x34 before the
   builder reads them (the fill wrote 0, 1, -1 after the def call).

**Restart** ("START restart"): `sub_0006ED70` (0x0003D4E6) runs the same
`sub_0006D7A0` without the fill and without the mount, so the records keep
their def and model, this level's list survives, and step 6 sets the fields
again. Nothing is allocated by this area in play. INFERRED: the stock pool
reset makes the 4 blocks free again, as for the buggies.

## Fixed after .61: spawn height and tank entry

Log (m61: `20261107-141636-131.err`): tank, truck with gunner seat and truck
created, every texture loaded (7/7, 5/5, 3/3, 14/14, 10/10).

**Sunk (CONFIRMED, `test_spawn_heights`).** The fill puts the record y at
the vehicle pad (`sub_0033E2B0`: pad +0xC..+0x14). The stock buggies and
the jeep stand on their origin: the lowest of their position rows ({x, y,
z, 0xFFFFFFE0}) is y 0. The truck's wheels reach y -0.935 and the tank's
tracks y -2.365 below their origin, which is how deep they sank (the story
levels place them by their records). A parked vehicle's body does not
settle; entering wakes it, which is why the trucks rose. The fill hook now
adds that depth + 0.01 (the props' floor gap) to the record y (+0x10):
`tank spawn height: record y +2.375 (model base y -2.365)`, `truck spawn
height: record y +0.945 ...` (once per level each). For a playtest
`TSFP_MAPMAKER_TANK_LIFT=<m>` / `TSFP_MAPMAKER_TRUCK_LIFT=<m>` (-1..4, read
once) replace the value. The restart re-creates from the raised record.

**Tank not enterable (CONFIRMED, `test_tank_no_player_flag`).** The create
copies def +0xA8 into the vehicle block +0x58 (0x00182B51). Def 2 has 0x17;
bit 0x10 there means "no player": the enterable test `sub_0017A260`
(0x0017A27E) used by the nearest-vehicle scan `sub_0017C6A0`, the use prompt
`sub_0017D1D0` (0x0017D1E8), the exit (0x0017DAFB) and the vehicle tick
(0x0017ECD6) all refuse it; they are the only tests of that bit in the
vehicle code. Machine Wars seats the player by script. The truck (4) and
the buggies (0x24) lack it. The create hook now clears bit 0x10 of a
MapMaker tank's block after the builder (bit 0, tracked, kept): `tank open
to players: block flags 0x17 -> 0x7 (... tracked bit 1; seats 2)`, once per
level. Its seats have no anims; the enter path skips anim id 0
(0x0017C985), so the player is placed without one, as the story does.
INFERRED: driving with the tracked adapter, the driver's main gun and the
gunner seat's AA gun then work as in Machine Wars.

## Pool (fixed here)

The vehicle pool is allocated once per level: `sub_001806B0` (0x000A5C95)
takes 0x25C0 bytes = 4 blocks of 0x970 and 4 free pointers from the Level
heap, and `sub_00182A20` pops one per vehicle **without a bound check**
(`[0x0074C670]` free count). The limits area raised category 35 from 4 to 8
in capacity stage 1, so a 5th vehicle took the word before the free list as
its block. `pc_mapmaker_limits.inc` no longer raises category 35 (fixture
check in `mapmaker_limits.c`); category 36 (item 0x48, a prop) stays raised.

## Resource table

The editor panel ([resource table](mapmaker-restable.md)) has a 16th row,
**Vehicles** (category 35, used/4), counting stock and story vehicles
alike. The Memory row stays the stock estimate (40 KB for the first item of
an id): a tank set reads about 1.39 MB once per map (models 177 KB, textures
1.21 MB), a truck set about 0.93 MB (models 273 KB, textures 658 KB), against
the 11-14 MB Level heap free measured in MapMaker maps. The heap guard of the
memory area (`[mapmaker] heap full: ...`) logs an overrun.

## Not changed

- **Wreck.** The death handler `sub_00174120` (handler slot 1, 0x00182C1B,
  called when health reaches 0) makes 7 gibs and the explosions only in
  scene 5 (0x00174152). In scene 100 it returns at once, as for the stock
  buggies: a tank or truck is not destroyed. Enabling it would mean running
  the scene-5 body outside scene 5, whose callees read the scene too
  (`sub_000A6880` -> `sub_00096410` 0x0009644C); left for a later round.
- **Impact hook** `sub_0017BB10` calls the Machine Wars script
  `sub_0021BC00` (crush prop 0x557) only in scene 5 (0x0017BB19).
- **Editor model.** In the editor the item keeps the theme buggy model
  (cosmetic), as the storyprops variants keep theirs.
- **Story levels** are untouched: every play hook acts in scene 100 with the
  switch on, at its own return address.

## Hooks (WRAP lines for the integrator, area `vehicles`)

```
WRAP sub_003006C0 -> pc_mapmaker_vehicles_compile        (chained: OUTSIDE pc_mapmaker_container_compile; order vs storyprops/weaponset/sets/... free)
WRAP sub_00301A90 -> pc_mapmaker_vehicles_load           (chained: editor, aitemplates, bosses, roof, sets, weaponset, storyprops; order free)
WRAP sub_002F7770 -> pc_mapmaker_vehicles_category       (chained with pc_mapmaker_storyprops_category; return-address test 0x003051B5 / 0x003051D7, item 0x2B only)
WRAP sub_00319300 -> pc_mapmaker_vehicles_build          (chained with tileflags/storyprops build; each acts only for its own heap descriptor, menu +4)
WRAP sub_00062790 -> pc_mapmaker_vehicles_mount          (chained with the mothership/tank/storyprops mounts; order free: each mounts after the original and skips a mounted pak)
WRAP sub_00063900 -> pc_mapmaker_vehicles_textures       (chained with pc_mapmaker_storyprops_textures; before the original; order free)
WRAP sub_0006C090 -> pc_mapmaker_vehicles_preload_model  (chained with pc_mapmaker_storyprops_preload_model; same return 0x0023F5DB, each acts only for its own item ids)
WRAP sub_0033DDA0 -> pc_mapmaker_vehicles_def            (new; return-address tests 0x0023F637 / 0x00241D39)
WRAP sub_00182A20 -> pc_mapmaker_vehicles_create         (new; return-address test 0x0006E949; before the original)
```

All cdecl, plain `ret`; the build hook answers itself for its descriptor
(`MAPMAKER_GUEST_RET`). Included after `pc_mapmaker_storyprops.inc`. The guest
functions it calls are stubbed in `tests/mapmaker/mapmaker_area_stubs.h`
(`sub_000B3B20` added).

Log lines (`[mapmaker] vehicles: ...`, 32 per run plus one per mount and per
model): `saved story vehicles (items, records)`, `mounted
pak/stream/lv5/prop1.pak for the tanks (table N; tanks T, trucks K)`, the same
for prop3.pak and the trucks, `model 0xB textures: 7 of 7 in
pak/stream/lv5/prop1.pak`, `registered a vehicle model's textures before the
texture queue loads`, `vehicle placed as the Machine Wars future tank`,
`vehicle placed as the Scotland the Brave truck with its Vickers gun`,
`created a future tank (record, guns 2)`, `created a truck (...)`, and every
fallback (`... it stays the theme vehicle`, `pak mount table full`, `did not
mount`, `vehicle model or pak not available`).

## Unverified (INFERRED)

- Everything in play: entering, driving and leaving the tank and the truck
  in a MapMaker room, both tank guns and the Vickers gunner seat, the
  tracked-driver adapter outside Machine Wars, the raised spawn height
  (the fill's
  position, then physics).
- Resource rows for 0xB, 0xC, 0xD, 7, 0xE, 0x54D..0x54F in scene 100's table
  (prop_all has them; the hooks fall back to the theme vehicle and log when
  one is missing).
- Whether the textures of these models are deferred (the log says which).
- Bots and vehicles: no AI driver was found for either vehicle.
- Restart with a tank or truck.

## User test steps

1. `TSFP_MAPMAKER_LIMITS=1`. Editor -> Objects -> the vehicle: place four.
   Select one and open its options (the Objects-mode options button, as for a
   pickup): **Vehicle** -> Future tank (Machine Wars). Another -> Castle truck
   + Vickers gun, another -> Castle truck; leave one as the theme vehicle.
   A fifth vehicle is refused (the Vehicles row of the resource panel shows
   4/4).
2. Save, reload the map: the choices are kept (open the options again).
   Export the map (.tsmap) and import it: kept.
3. Test map: stderr has `mounted pak/stream/lv5/prop1.pak for the tanks`,
   `mounted pak/stream/lv4/prop3.pak for the trucks`, the `model 0x..
   textures: N of N` lines, `vehicle placed as ...` and `created a future
   tank` / `created a truck`, `tank spawn height: record y +2.375`, `truck
   spawn height: record y +0.945` and `tank open to players: block flags
   0x17 -> 0x7`. All three stand on the floor before anyone gets in. If one
   still sinks or floats, rerun with `TSFP_MAPMAKER_TANK_LIFT=2.6` (or
   `TSFP_MAPMAKER_TRUCK_LIFT=1.1`, ...) and report the value that rests.
4. Drive the tank: get in, drive (tracks: the stick turns as in Machine
   Wars), fire the main gun, switch to or let a second player take the AA
   gun; get out. Drive the truck; take its gunner seat and fire the Vickers.
   Both are textured.
5. START -> restart: the vehicles are back with their guns; stderr repeats
   `created a ...`.
6. Without the switch the same map plays four theme vehicles.
