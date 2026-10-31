# MapMaker custom weapon set: 20 slots

Status 2026-10-31: implemented in `src/interface/pc_mapmaker_weaponset.inc`
(area `weaponset`), fixture-tested (`tests/mapmaker/test_mapmaker_weaponset.py`:
70 mutations plus decode and XBE facts; `..._weaponset_classified.py`: every
slot constant around the weapon set classified). Not built or played. No
guest-code table (`config/guest-code-patches/weaponset.json` does not exist;
wrappers only).

User request (2026-10-31): "Add many more slots to the custom mapmaker bot and
weapon sets. I'd like at least twenty. The additional slots can default as
empty or not set." Character slots: [mapmaker-sets.md](mapmaker-sets.md).

## How the stock weapon set works (CONFIRMED)

- **Struct.** 0x44 bytes: +0 word, +4 count, +8 up to 15 dword weapon ids
  (`rep movsd 0x11` at 0x0006A76F/0x0006A795). Presets 0x00518F28 + n x 0x44
  (index < 10), profile custom sets, System Link copy 0x00761388 (0x002D39F6).
- **Map info** (chunk 0xC77668, data offsets): +0x40 word set index (>= 10
  custom), +0x42 count byte (compile writes 6, 0x003008D6), +0x44 six word ids
  from editor +0x1190C (the editor set is editor +0x11904, count +0x11908,
  15 ids). Room for 15 words ends at the name string +0x62 (0x00300911);
  the chunk (0x3DC) is full.
- **Editor screen.** Map Settings weapon set = pane 0x005236EC {builder
  `sub_002D4220` (title 0xE86, selector item value -5, six rows of values
  0..5 drawn by `sub_002D3D30`), update `sub_002D40A0`}. A row opens the
  weapon list 0x0052370C; its pick `sub_002BF910` stores `sub_00312AE0(row,
  weapon)` (row = highlighted index - 2) and moves the cursor on while the
  value is below 5 (`sub_00074460`); X clears a row (`sub_00312AE0(row, 0)`,
  0x002D41C2). The row draw asks `sub_002D3B00(row)` (0 for slot >= 6).
- **Play set.** `sub_0006BD30` returns the set every consumer uses. In scene
  100 it rebuilds 0x006F1E28 on every call (`sub_0006A760`: editor test
  `sub_002BE7C0` = the map's ids for index >= 10, else a preset; from the
  front end `sub_002D3A30`, which in MapMaker Arcade takes the setup's choice,
  profile +0x6A4, index 0xF = the map's set), zeroes ids that
  `sub_00064E70([0x007DE456], id)` rejects, and story-type maps append weapon
  0x38 (0x002BE827). Only `sub_0006BD30` touches 0x006F1E28.
- **Slots in play.** All read `set +8 + 4*slot`:
  - pickups: the fill `sub_00240C50` stores catalogue +8 (= slot 0..5 for
    items 0x11..0x16 weapon, 0x17..0x1C ammo) into pickup record +0x10
    (0x00241187, 0x002414C1), then calls `sub_0033E3B0(pickup, item)`;
    `sub_0006C620` (level pickups, inline) and `sub_0006D7A0` (spawn,
    `sub_0006BF10(type, +0x10)`, slot < count, 0 -> none) look the weapon up;
  - vetoes (category table +0x14): `sub_0033E780` weapon (id - 0x11, pickup
    model `[0x004FB194 + w*0x54] > -1`), `sub_0033E7B0` ammo (id - 0x17, ammo
    type via 0x004FA138/0x004F9BAC), both through `sub_0033E6C0(slot)` (map
    data +0x44 for index 15);
  - **Story AI weapon is a slot**, not a weapon id: editor entry +0x2C
    (choice -1, 0..5 named by item 0x11 + k, 0x0031CD78..0x0031CDB2) ->
    chunk word +0x26 (0x0030144D) -> fill `sub_0033E6C0` check and record
    +0x18 (0x0024205A) -> spawn reads `set[slot]` unbounded (0x0015B885,
    0x002B9D51). Chunk word +0x2A is the dropped keycard item, not the weapon;
  - weapon files: `sub_001C8540` loads every id of the set (count);
  - membership `sub_0006BEC0`; AI random weapon `sub_002B4C70` takes
    `set[rand % count]` (0x002B50E8); starting weapon `sub_000CEF30` slot 0.
- No caller copies `sub_0006BD30`'s result as a fixed 0x44 struct (decode
  test), so a longer set is safe there.

## Design: a slot byte on the existing pickups (option b)

New item types (option a) would need 28 catalogue entries past the 80-entry
table and every id-indexed table and jump table; rejected. Instead:

- **Item record byte +0xC.** For categories 8/9 the compiler's writer
  `sub_00300800` writes +0, +4 and +0xA only (0x00301012..0x0030102D; +0xC is
  the turret branch 0x00301147) into a zeroed buffer, and neither the loader
  nor play reads +0xC for them: 0 = the item's own slot 1-6 (stock), 7..20 =
  an extra slot.
- **Editor node +0x30.** No stock code writes it (held node editor +0x2F64 is
  never addressed; the loader's template esp+0x6C is never written, so loaded
  nodes carry stack garbage there); nodes are copied whole (`rep movsd 0xF`).
  The slot is kept as 0x57530000 | slot ('WS' tag; anything else = own
  slot). Save and load pair records with nodes by key: the piece's cvttss2si
  x/y/z bytes and byte +0xC, item id, rotation and cell bytes (the writer's
  own fields); identical keys pair in order; an unpaired record keeps its own
  slot (logged).
- **Fork chunk 0x00C776F6**, size 0x20: +0 word version 1, +2 byte first 6,
  +3 byte count 14, +4 14 word weapon ids (0 = empty). Written before the
  terminator only when an extra slot is set, a weapon/ammo pickup or a Story
  AI uses slot 7+; else the map is byte-identical to stock. Room 0x28 bytes;
  refused (item bytes cleared, `[mapmaker] weaponset: no room ...`) only near
  the 512 KB limit. The container's `mapmaker_container_over_stock` treats
  the tag as past stock: such a map saves in the 64 KB tier, so a stock build
  refuses it (error 7) and System Link refuses it like any larger map (the
  host never sends it). Stock would otherwise read ids past the map's 6.

## Editor (`TSFP_MAPMAKER_LIMITS=1`, custom weapon set: index >= 10)

- Load `sub_00301A90`: slots 7-20 from the chunk (empty without it), every
  weapon/ammo node's +0x30 from its record. New map `sub_002FEBF0`: empty.
  Map Settings close `sub_00312880` (weapon-set-changed vs the undo shadow
  `[0x00765E38]`): slots 7-20 count too.
- Weapon set screen: builder `sub_002D4220` resets to page 1; the title text
  (`sub_0003D7B0` returning to 0x002D4254) is a copy with "(1-6 of 20)",
  "(7-12 of 20)", "(13-18 of 20)" or "(15-20 of 20)" that the update rewrites
  (INFERRED: the item keeps the pointer). Update `sub_002D40A0`: Down on row 6
  (value 5) shows the next six slots, Up on row 1 the previous ones; Right /
  Left on any slot row turn the page, wrapping (cursor via `sub_00074460`,
  input bits cleared). On pages 2-4 the row draw (`sub_002D3B00` returning to
  0x002D3D8C) and the highlighted weapon's model (0x002D410F) show slot row +
  start and the pick / X (`sub_00312AE0` returning to 0x002BF95A /
  0x002D41C7) store there. The row digits stay 1-6.

### Every route to the weapon set screen (CONFIRMED, 2026-10-31)

User report (v0.4.0-mapmaker.11): "Only six slots appear for weapon
customization on the pause menu custom mapmaker weapon set screen." The slot
pane 0x005236EC exists only inside screen 0x0052371C = {`sub_002D44D0`,
`sub_002BFC30`} (pushes of 0x0052371C: `sub_002C7710`, `sub_003128F0` only):

- **Editor** (scene 102): Start menu -> Map Settings -> edit weapon set
  (item 0xB08, `sub_003128F0`), and the Test map setup's Weapons item
  (`sub_002EFBF0` -> `sub_002C78F0(menu, 0, ...)`). Edits the editor set
  (editor +0x11904; slots 7-20 in the hook, saved in the fork chunk). This
  is the only place the custom set is defined; it was already paged, but only
  by Down past row 6 with "(1-6)" in the title, so the other 14 slots were
  easy to miss. Statically the paging works: the pane tick calls the update
  (+0xC = descriptor +4, `sub_00078E50` 0x00078F57) at 0x0007AD76 before its
  own Up/Down (0x0007AE2E), so the hook sees Down first. Now the title says
  "of 20" and Right/Left also turn pages (stock moves only choice items with
  them, 0x0007AFB1; slot rows are action items). Not verified in play.
- **Front end** (scene 101) setup Weapons item (`sub_002C7710`, offered only
  when scene > 100 and the builder's second argument is 0): the set list
  `sub_002BF830` (value -5, choice +0xC8 = profile +0x6A4) includes the map's
  set 0xF in MapMaker Arcade (mode 9), shown by `sub_002D3B00` ->
  `sub_002BFF90` -> `sub_00312A60` (the map info's 6 ids) and read-only in
  stock (0x002D415B, action `sub_002D4040` 0x002D406F). With the switch, its
  pages 2-4 show the map's slots 7-20 read-only (the X clear, which writes
  the profile at 0x002D41D5, is suppressed there); profile sets stay stock.
- **In-play pause** (scene 100): pause -> setup `sub_0019A000` calls
  `sub_002C78F0(menu, 1, sub_00196DE0)` (0x0019A018) and the builder skips the
  Weapons item when that argument is set (0x002C808A `jne 0x002C81A8`); the
  item's action is also absent at scene <= 100 (0x002C809E). No weapon set
  can be viewed or changed there in stock or now; the match's set is fixed at
  load (play set 0x00906000 from the saved map).
- Story AI weapon choice (`sub_00078590` returning to 0x0031CDB7): 14 more
  options, values 6..19, "7: <weapon>" .. "20: <weapon>". Without the switch
  only for an AI that already uses slot 7+.
- Pickup options (`sub_00319300`, shared by categories 8, 9, 19-21; after the
  respawn time item, `sub_000783D0` returning to 0x003193A2): weapon/ammo
  pickups get a "Weapon" choice (text 0xD76, help 0xB9, built like the Story
  AI weapon choice) bound to node +0x30: the item's own name (its slot 1-6)
  or "7: <weapon>" .. "20: <weapon>". New pickups start on their own slot
  (held defaults `sub_00316D80`). INFERRED: the menu sizes to its items.
- Not changed: the editor model of a pickup on slot 7+ is its item type's
  (`sub_002FE840`, cosmetic); a slot change in the pickup options is saved
  with the map but may not get its own undo step.

## Play (scene 100; works without the switch)

- Cache: the map's slots 7-20 from `[0x00784014]`, read once per level
  (dropped outside scene 100 and at the MapMaker preload `sub_0023F390`).
- Active only when the map's custom set is the source (editor test: index >=
  10; front end: index 0xF, in MapMaker Arcade the setup's choice); presets
  and profile sets stay stock, and pickups on slot 7+ are then skipped.
- `sub_0006BD30`: when it returns 0x006F1E28, a copy at 0x00906000 with ids
  0..5 stock, 6..19 the map's slots 7-20 (filtered by `sub_00064E70` as
  stock, cached per mode), then any appended ids (0x38), count 20+. For the
  AI random pick (return 0x002B50D5) only the used ids (0x00906080): bots
  choose among all used slots 1-20 instead of hitting empty slots.
- `sub_0033E6C0(slot)`: slots 6..19 answer the map's raw id (Story AI fill,
  vetoes). Vetoes `sub_0033E780`/`sub_0033E7B0`: a record with byte +0xC in
  7..20 uses that slot's weapon with the original checks. `sub_0033E3B0`:
  pickup +0x10 = slot - 1. Preload `sub_0023F390`: also `sub_000BE860(2 /
  0x40, weapon)` -> `sub_0006B2D0` for pickups on slots 7+.
- Memory: every distinct weapon in the set has its files loaded
  (`sub_001C8540`) and each used pickup its model; 20 distinct weapons cost
  more Level heap (not measured). Watch the memory area's `heap full` log and
  the probe's free-memory figure.

## Extension memory

0x00906000..0x0090607F play set, 0x00906080..0x009060FF AI pick set,
0x00906100..0x0090613F title, 0x00906140..0x0090640F choice texts
(15 x 0x30); 0x00906000..0x009064FF reserved for this area.

## Hooks (WRAP lines for the integrator, area `weaponset`)

```
WRAP sub_003006C0 -> pc_mapmaker_weaponset_compile      (chained: OUTSIDE pc_mapmaker_container_compile; order vs sets/aitemplates/bosses/sky/roof free)
WRAP sub_00301A90 -> pc_mapmaker_weaponset_load         (chained: editor, aitemplates, bosses, roof, sets; order free)
WRAP sub_002FEBF0 -> pc_mapmaker_weaponset_reset        (chained: bosses, sky, roof, sets; order free)
WRAP sub_00312880 -> pc_mapmaker_weaponset_changed      (new)
WRAP sub_00316D80 -> pc_mapmaker_weaponset_held         (new)
WRAP sub_002D4220 -> pc_mapmaker_weaponset_build        (new)
WRAP sub_002D40A0 -> pc_mapmaker_weaponset_screen       (new; also the front-end view, calls sub_00071940)
WRAP sub_002D3B00 -> pc_mapmaker_weaponset_get          (new; return-address test: row draw and model 0x002D410F)
WRAP sub_00312AE0 -> pc_mapmaker_weaponset_set          (new; return-address test)
WRAP sub_0003D7B0 -> pc_mapmaker_weaponset_title        (chained: sky's roof_text, realbosses_text; order free; return-address test, hot)
WRAP sub_00078590 -> pc_mapmaker_weaponset_sai_choice   (chained: bosses, sets; order free; return-address test)
WRAP sub_000783D0 -> pc_mapmaker_weaponset_options      (new; return-address test)
WRAP sub_0006BD30 -> pc_mapmaker_weaponset_play_set     (new; hot, per call)
WRAP sub_0033E6C0 -> pc_mapmaker_weaponset_slot         (new)
WRAP sub_0033E780 -> pc_mapmaker_weaponset_weapon_veto  (new)
WRAP sub_0033E7B0 -> pc_mapmaker_weaponset_ammo_veto    (new)
WRAP sub_0033E3B0 -> pc_mapmaker_weaponset_pickup       (new; level load)
WRAP sub_0023F390 -> pc_mapmaker_weaponset_preload      (new; level load, no arguments)
```

All cdecl, plain `ret`; a hook that answers itself pops the return address
(`MAPMAKER_GUEST_RET`). Include the `.inc` after `pc_mapmaker_limits.inc`.
Stubs for whole-chain fixtures: `sub_00064E70`, `sub_00069640`,
`sub_000BE860`, `sub_0006B2D0`, `sub_00074460`, `sub_00079DE0`,
`sub_0007D4A0`, `sub_000716D0`, `sub_000783D0`, `sub_00303350` (plus those
already stubbed).

## User test steps

1. `TSFP_MAPMAKER_LIMITS=1`. Editor -> Map Settings -> weapon set: custom.
   The title reads "(1-6 of 20)". On the last row press Down (or Right on any
   row): "(7-12 of 20)", empty rows; pick weapons (and on "(13-18 of 20)",
   "(15-20 of 20)"). Up on the first row or Left goes back; slots 1-6 are
   unchanged. Save the map.
1b. Front end -> MapMaker -> load the map -> Arcade -> setup -> Weapons ->
   choose the MapMaker set: Right shows "(7-12 of 20)" with the saved slots
   (read-only, as stock's 1-6). Profile sets: 6 rows, stock. The in-play
   pause has no weapon set screen (stock).
2. Place a weapon pickup, open its options: a "Weapon" choice lists the
   item's own slot and "7: ..." .. "20: ...". Pick one. Do the same for an
   ammo pickup. Place a Story AI: its weapon choice lists 20 slots.
3. Save, reload: the slots, the pickups' choices and the AI's weapon are kept.
4. Test map / play (Arcade, map's weapon set): the pickups spawn the chosen
   weapons, the AI carries its weapon, bots use weapons from all used slots;
   stderr has no `[mapmaker] weaponset:` or `heap full` line.
5. A map that uses none of this saves as before (stock size). A map using it:
   a stock build refuses it; System Link refuses it.
6. Without the switch: no paging or extra choices; the map from step 3 still
   loads and plays with its 20 slots.
