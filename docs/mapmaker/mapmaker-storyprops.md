# MapMaker story props: power cell drums and prison guns

Status 2026-11-07: implemented in `src/interface/pc_mapmaker_storyprops.inc`
(area `storyprops`), fixture-tested (`tests/mapmaker/mapmaker_storyprops.c`
+ `tests/mapmaker/test_mapmaker_storyprops.py`: 44 mutations, plus decode,
US XBE data and game-pak checks of every site below). **Not wired, not
built, not played**: the wrappers are the integrator's (list below). Switch:
`TSFP_MAPMAKER_LIMITS=1` (editor and play). No guest-code table, no fork
chunk, no new save size. Story assets wave 1, items 2 and 21
([inventory](mapmaker-story-assets.md)).

CONFIRMED = decode, XBE data or pak index (the test names the site);
INFERRED = reasoning, needs a playtest.

## What the player gets

- **Explosive drum -> Power cell.** A placed explosive drum (Props, item
  0x49) has an options menu (category 26 has none in stock) with **Type:
  Explosive drum / Power cell (big explosion)**. A power cell is story mission
  1's prop 0x53D `farfuture2/powercell`: health 2, a physics body, and at 0
  health explosion type 0x2F (table 0x004DB490 + type*0x18: 10, **12**, 25)
  instead of the drum's 0x0A (10, 6, 25). INFERRED: the second field is the
  radius, so the blast is twice as wide.
- **Fixed gun -> Prison gun.** A placed fixed gun (category 29, items
  0x32..0x34) gets a **Model: MapMaker gun / Prison gun (mission 1)** choice
  after its weapon choice. A prison gun is the intro's manned turret 0xE6
  `level29/props/prisonfixedgun` with the intro's gun type 7; flags 0x10040
  (manned) are what category 29 already writes (0x00241A88). The weapon,
  range and other stock options keep working.
- Any number per map: each is an ordinary prop / turret with its own level
  record (no statics, no one-per-map state; CONFIRMED for the cell's init,
  damage and update `sub_00218740`, which read only the object and record).
- A stock build (or this one without the switch) plays the map with the
  ordinary drum and gun: the variant is one byte the game ignores.

## Why it works (CONFIRMED)

- **Save byte.** Item record byte +0xC: the compiler `sub_00300800` writes
  +0, +4, +8 for 0x32..0x34 (0x003010F6) and zeroes +0..+0xA for category 26
  (0x00300FFA); only 0x35..0x37 write +0xC (0x00301147). The editor loader
  reads +0..+8 for 0x32..0x34 (0x003022D3) and nothing for 0x49; its only
  item +0xC read is 0x35..0x37's (0x003023AC). The fill `sub_00240C50` reads
  +0xC only in the 0x35..0x37 cases (0x00241BFB, 0x00241C0E). Editor node
  +0x30 holds `0x5350 0000 | variant` ('SP'; no stock writer; weaponset uses
  the same field with its 'WS' tag for pickups only). Nodes and records are
  paired by piece and item position on save and load, as the weaponset area.
- **A real level record.** The fill builds a 0x60-byte record for every
  MapMaker prop (+0 logic id + 1 or -1, +8 flags, +0xC model = `sub_002F7790`
  at 0x00241782, +0x10 position) and `sub_0006C740` passes it to the create
  and stores it at obj+0x2D8 (0x0006C7B3). The cell's damage `sub_00218650`
  reads `[[obj+0x2D8]]` and compares it with 'CEL1'..'CEL6' (the intro's
  objective cells, which explode with 0x28 and complete an objective); a
  MapMaker record never matches, so it explodes with 0x2F (0x002186FB).
- **Init by model.** Prop rows come from `prop_all` in scene 100 too: 0x53D's
  row has init 20 = `sub_0021A710`, whose switch on model (byte table
  0x0021ACC8) sends 0x53D to case 0x0021ABC5 (health, damage, update,
  `sub_0016C170` physics). 0xE6 has init 0, type 0x200 (a turret).
- **Order.** `sub_001CDB80` mounts paks (`sub_00062790`) first; the MapMaker
  preload (`sub_001CDC50` -> `sub_0006C920` -> `sub_0023F390`) requests each
  item's model through `sub_0006C090(catalogue, theme)` (0x0023F5D6, esi =
  the item record); the load pass `sub_00063390` (0x0003D5AD) reads them; the
  fill runs later (`sub_0006EC10` from 0x0003D64A). The cell's physics
  resource 0xCFB is added by `sub_00257150` for model 0x53D.
- **Paks.** 0x53D is in `challenge/l_64_ch.pak` (mounted as the mothership
  port does); 0xE6 only in `story/l_2_st.pak` and `story/l_26_st.pak`
  (9,303,120 bytes, 325 files). Both are mounted kept open (`sub_000622D0(path,
  0, 1)`: only the index stays in memory). Every file l_26_st shares with
  another pak (15,592 shared entries) is byte-identical there, so it cannot
  shadow MapMaker's files (test `test_models_and_paks`).
- **Textures.** As for the mothership: scene 100 skips texture registration
  when a level-pak model is created (0x00096556), so a variant model whose
  request carries the deferred mark (+2 & 0x10) gets `sub_00096330(file, 1)`
  once per level before its first use.

## Hooks (WRAP lines for the integrator, area `storyprops`)

```
WRAP sub_003006C0 -> pc_mapmaker_storyprops_compile        (chained: OUTSIDE pc_mapmaker_container_compile; order vs weaponset/sets/aitemplates/bosses/sky/roof free)
WRAP sub_00301A90 -> pc_mapmaker_storyprops_load           (chained: editor, aitemplates, bosses, roof, sets, weaponset; order free)
WRAP sub_002F7770 -> pc_mapmaker_storyprops_category       (new; return-address test 0x003051B5 / 0x003051D7)
WRAP sub_00319300 -> pc_mapmaker_storyprops_build          (chained with pc_mapmaker_tileflags_build; each acts only for its own heap descriptor, menu +4)
WRAP sub_000783D0 -> pc_mapmaker_storyprops_turret_options (chained with pc_mapmaker_weaponset_options; return-address tests 0x00318A8C / 0x003193A2 differ)
WRAP sub_00062790 -> pc_mapmaker_storyprops_mount          (chained with the mothership/tank mounts; order free: each mounts after the original and skips a mounted pak)
WRAP sub_0006C090 -> pc_mapmaker_storyprops_preload_model  (new; return-address test 0x0023F5DB)
WRAP sub_002F7790 -> pc_mapmaker_storyprops_model          (new; return-address tests 0x00241787 / 0x002419C4)
WRAP sub_0033DD80 -> pc_mapmaker_storyprops_gun_type       (new; return-address test 0x00241AA6)
```

All cdecl, plain `ret`; the build hook answers itself for its descriptor
(`MAPMAKER_GUEST_RET`). Include the `.inc` after `pc_mapmaker_weaponset.inc`.
The guest functions it calls are already stubbed in
`tests/mapmaker/mapmaker_area_stubs.h` (tileflags, mothership).

Log lines (`[mapmaker] storyprops: ...`, 32 per run): `saved story prop
variants`, `mounted l_64_ch.pak for the power cells`, `mounted l_26_st.pak
for the prison guns`, `registered the variant model's textures`, `drum
placed as the mission 1 power cell`, `fixed gun placed as the mission 1
prison gun`, and every fallback (`... stays a drum`, `... stays a MapMaker
gun`, `pak mount table full`, `model not read this level`).

## Unverified (INFERRED)

- The cell's physics body, its random push when hit by damage type 0x1A and
  its explosion in a MapMaker room; the prison gun's mount points, aim limits
  (the stock options' range, not the intro's -30..60 / -83..83) and gun type
  7's effects on a player-mounted MapMaker turret.
- Resource rows for 0x53D / 0xE6 in scene 100's table (the hooks fall back
  to the stock model and log when a row is missing).
- In the editor the item keeps the drum / MapMaker gun model (cosmetic).
- Memory: l_26_st's index (325 entries) and the two models (34,412 B and
  88,716 B) per level that uses them.

## User test steps

1. `TSFP_MAPMAKER_LIMITS=1`. Editor -> Objects -> Props: place two explosive
   drums. Select one and open its options (the Objects-mode options button,
   as for a pickup): **Type** -> Power cell (big explosion). Done.
2. Objects -> fixed gun (category 29): place one, open its options: after the
   weapon choice, **Model** -> Prison gun (mission 1).
3. Save, reload the map: the choices are kept (open the options again).
   Export the map (.tsmap) and import it: kept.
4. Test map: stderr has `mounted l_64_ch.pak for the power cells`, `mounted
   l_26_st.pak for the prison guns`, `drum placed as the mission 1 power
   cell`, `fixed gun placed as the mission 1 prison gun`. The cell looks like
   the intro's blue power cell; shoot it: a bigger blast than the drum's. The
   prison gun looks like the intro's turret; mount and fire it.
5. Several cells and prison guns in one map all work. Without the switch the
   same map plays plain drums and MapMaker guns.
