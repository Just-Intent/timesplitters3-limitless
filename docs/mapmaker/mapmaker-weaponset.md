# MapMaker custom weapon set: 20 slots

Status 2026-10-31: implemented in `src/interface/pc_mapmaker_weaponset.inc`
(area `weaponset`), fixture-tested (`tests/mapmaker/test_mapmaker_weaponset.py`:
mutations plus decode and XBE facts; `..._weaponset_classified.py`: every
slot constant around the weapon set classified). 2026-11-01: weapon names
on rows 10-20 move right past the two-digit number ("Row name" below). Not
built or played. No
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
- Weapon set screen (2026-10-31, replaces the hidden paging): a scrolling
  list of 20 slot rows. See "Why paging was invisible" below.
  - Builder `sub_002D4220` (the pane system calls it at 0x00078F72 from
    descriptor +0, return 0x00078F74): with the switch in scene 102 (any set)
    or the front end's MapMaker Arcade setup, the hook replays the stock calls
    (`sub_00075B70(0x20010, 0, 40.0f, 0, 0x002C1450)`, title 0xE86 value -1
    flags 0x30404, set list 0x004766D8 value -5 flags 0x08030C00,
    `sub_002BF830(menu)`, `sub_00079710(0)`, sizes 5.0f / 7.5f (5.5f outside
    the editor), footer 0xE88 / 0xE89 / 0x1A27, `sub_00077110(menu, -5)`,
    `sub_00075E70`) but adds rows 0..19 (`sub_00079D60(0, row, 0, 4.6f,
    sub_002D4040, sub_002D3D30)`) inside `sub_000780E0(27.6f, 0)` ..
    `sub_000781A0()`, the region the weapon list builder `sub_002BFA70` uses
    (0x002BFAF1 / 0x002BFB64): mode 2 (0x00078115), view = height
    (+0x6C..+0x70), each item made in it gets flag 0x40 (0x0007914A) and adds
    its height to the scrolling content +0x5C (0x00078443). 27.6 = six rows
    of 4.6 (INFERRED unit: the rows' height argument, item +0x74), so the
    pane keeps its stock size. Row index is still value + 2 (the regions add
    no items), which the pick and X clear rely on. Otherwise: the original.
  - The stock cursor, the editor pointer (`sub_000777E0`), the scrollbar and
    the wheel (`mapmaker_pointer_wheel`, three steps per notch, needs content
    > view and style flag 0x40; INFERRED: the pane shares the weapon list's
    current style) then scroll it; nothing list-specific in the mouse code.
  - Rows 7-20: the row draw and model preview ask `sub_002D3B00(6..19)`
    (returns 0x002D3D8C / 0x002D410F): custom set -> slots 7-20, front end
    with the map's set -> the map's (read-only), else the original (0). Pick /
    X store via `sub_00312AE0(slot, weapon)` (0x002BF95A / 0x002D41C7) into
    slots 7-20; after a pick on rows 6-19 the cursor moves on (stock stops at
    value 5, 0x002BF990; slot pane in ebx, 0x002BF935).
  - Row label: the draw writes '1' + value into the consumed rect argument
    slot (entry + 0xC, 0x002D3DE8; drawn from esp+0x48 at 0x002D3F27) and
    reads a 6-entry colour table `[esp + value*4 + 0x20]` (0x002D3D8C). The
    hook on `sub_00064E70` (only its call at return 0x002D3DF6, after both)
    rewrites the label to "7".."20" and the stored colour (entry - 0x24) to
    entry `value % 6`; the read past the table took the return address.
  - Row name (2026-11-01; user request: "Offset weapon slot weapon name text
    slightly for slots 11-20, as the name is currently overlapping the number
    on each slot"). CONFIRMED (decode test `test_row_name_follows_label`): the
    draw computes x = (int)(rect.x + 5.0f) (0x002D3D45), draws the label at x
    + 5 (0x002D3F40) and the weapon name at x + 20: it stores x + 0x14 as a
    float at frame +0x18 (0x002D3F6A / 0x002D3F8A), asks `sub_0003D7B0(name
    id)` (call 0x002D3F90, return 0x002D3F95), shrinks the font to fit the
    width rect +8 - name x (`sub_00040A40(text, width, 0, 1)`, which measures
    with `sub_0003FCF0(&w, &h, text, 0)`) and draws the name from frame +0x18
    (0x002D3FD1). A one-digit label fits the 15 units; "10".."20" ran into the
    name. `pc_mapmaker_weaponset_title` (the existing `sub_0003D7B0` WRAP,
    return 0x002D3F95 only, 20-row pane only) measures the label's first digit
    with `sub_0003FCF0` and moves the name right by that width, narrowing the
    fit width by the same amount, for values 9..19 (slots 10-20: "10" has two
    digits as well). A failed or implausible measure (<= 0 or > 20) uses 6.0
    (INFERRED digit width). Not seen in game; the gap after the second digit
    is then the stock one-digit gap only if both digits are equally wide.
  - Outside the editor, rows 7-20 are read-only: the row action
    `sub_002D4040` (opens the weapon list; the front end's pick writes the
    profile set at row index - 2, 0x002BF982, six slots) does nothing there,
    and the update clears X (0x800000; profile write 0x002D41D5).
- Diagnostics (stderr, `[mapmaker] weaponset: ...`, always on, own budget):
  every build `slot pane build (20 rows, scrolling) ret=0x00078F74
  menu=... descriptor=0x005236EC scene=102 set=15 pointer=1` (or `slot pane
  build (stock 6 rows: <reason>) ...`), the first update `slot pane open:
  pane=... items=22 content=... view=a..b scroll=... slot=... pointer=...`
  and moves `list: slot N highlighted, scroll S of MAX` (rows 7-20 or a scroll
  change; at most 48). No such line on opening the screen means the hooks did
  not run (check `TSFP_MAPMAKER_AREA_OFF` and the wrappers patch).

### Why paging was invisible (CONFIRMED statically, 2026-10-31)

User report (v0.4.0-mapmaker.11/.12, switch on): "I can't page through
anything. I still only see six slots. I should be able to scroll down."
The user's log (`20261031-174829-424.err`) has no weaponset line (that area
logged only errors then). The paging waited for Down on row 6 / Right in the
pane update, but:

- the editor makes its menus pointer menus: `sub_00071010` (called at
  0x00302FE9) sets `[0x007BA270 + player*0x1E0]`; the tick `sub_0007A7B0`
  then selects the row under the pointer in `sub_000777E0` and jumps over the
  stock Up/Down handling (0x0007ADEC -> 0x0007AFF6), so the stick and mouse
  move the pointer, not a cursor that could pass row 6;
- the PC mouse (wheel steps 0x10001/0x20002, the hovered row) is applied by
  `pc_display_menu_mouse` on `sub_00073750` at 0x0007ADC5, after the update
  call at 0x0007AD76;
- six rows fit their view, so the scrollbar and the editor wheel
  (`mapmaker_pointer_wheel`) had nothing to scroll.

### Every route to the weapon set screen (CONFIRMED, 2026-10-31)

User report (v0.4.0-mapmaker.11): "Only six slots appear for weapon
customization on the pause menu custom mapmaker weapon set screen." The slot
pane 0x005236EC exists only inside screen 0x0052371C = {`sub_002D44D0`,
`sub_002BFC30`} (pushes of 0x0052371C: `sub_002C7710`, `sub_003128F0` only):

- **Editor** (scene 102): Start menu -> Map Settings -> edit weapon set
  (item 0xB08, `sub_003128F0`), and the Test map setup's Weapons item
  (`sub_002EFBF0` -> `sub_002C78F0(menu, 0, ...)`). Edits the editor set
  (editor +0x11904; slots 7-20 in the hook, saved in the fork chunk). This
  is the only place the custom set is defined. The pane tick calls the update
  (+0xC = descriptor +4, `sub_00078E50` 0x00078F57) at 0x0007AD76; the earlier
  paging there never showed (above); now the pane is a 20-row list. Not
  verified in play.
- **Front end** (scene 101) setup Weapons item (`sub_002C7710`, offered only
  when scene > 100 and the builder's second argument is 0): the set list
  `sub_002BF830` (value -5, choice +0xC8 = profile +0x6A4) includes the map's
  set 0xF in MapMaker Arcade (mode 9), shown by `sub_002D3B00` ->
  `sub_002BFF90` -> `sub_00312A60` (the map info's 6 ids) and read-only in
  stock (0x002D415B, action `sub_002D4040` 0x002D406F). With the switch the pane has
  20 rows there too: with the map's set rows 7-20 show its slots 7-20
  read-only; with a profile set they are empty and inert.
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
0x00906100..0x0090610F the row label measure (w, h, one-character text),
0x00906110..0x0090613F unused (the retired paged title), 0x00906140..0x0090640F choice texts
(15 x 0x30); 0x00906000..0x009064FF reserved for this area.

## Hooks (WRAP lines for the integrator, area `weaponset`)

```
WRAP sub_003006C0 -> pc_mapmaker_weaponset_compile      (chained: OUTSIDE pc_mapmaker_container_compile; order vs sets/aitemplates/bosses/sky/roof free)
WRAP sub_00301A90 -> pc_mapmaker_weaponset_load         (chained: editor, aitemplates, bosses, roof, sets; order free)
WRAP sub_002FEBF0 -> pc_mapmaker_weaponset_reset        (chained: bosses, sky, roof, sets; order free)
WRAP sub_00312880 -> pc_mapmaker_weaponset_changed      (new)
WRAP sub_00316D80 -> pc_mapmaker_weaponset_held         (new)
WRAP sub_002D4220 -> pc_mapmaker_weaponset_build        (new; replaces the builder for the 20-row list)
WRAP sub_002D40A0 -> pc_mapmaker_weaponset_screen       (new; diagnostics, front-end X guard)
WRAP sub_002D3B00 -> pc_mapmaker_weaponset_get          (new; return-address test: row draw and model 0x002D410F)
WRAP sub_00312AE0 -> pc_mapmaker_weaponset_set          (new; return-address test; reads ebx)
WRAP sub_002D4040 -> pc_mapmaker_weaponset_row          (new; slot row action)
WRAP sub_00064E70 -> pc_mapmaker_weaponset_label        (new; return-address test 0x002D3DF6, per drawn row)
WRAP sub_0003D7B0 -> pc_mapmaker_weaponset_title        (chained, outermost as now; return-address test 0x002D3F95: weapon name after a two-digit label; keep it)
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
Stubs for whole-chain fixtures (in tests/mapmaker/mapmaker_area_stubs.h):
`sub_00074A00`, `sub_00077110`, `sub_000772E0`, `sub_00077320`, `sub_000780E0`,
`sub_000781A0`, `sub_002BF830`, `sub_00075B70`, `sub_00075E70`, `sub_00079710`,
`sub_00064E70`, `sub_00069640`,
`sub_000BE860`, `sub_0006B2D0`, `sub_00074460`, `sub_00079DE0`,
`sub_0007D4A0`, `sub_000716D0`, `sub_000783D0`, `sub_00303350` (plus those
already stubbed).

## User test steps

1. `TSFP_MAPMAKER_LIMITS=1`. Editor -> Start -> Map Settings -> weapon set,
   choose the custom set. The slot list shows six rows with a scrollbar;
   stderr has `[mapmaker] weaponset: slot pane build (20 rows, scrolling)`
   and `slot pane open: ... items=22`. Scroll with the wheel, the scrollbar
   or by moving the pointer/Down past row 6: rows 7-20 (labels 7..20,
   empty). Pick weapons into a few of them (A, choose; X clears). Slots 1-6
   are unchanged. On rows 10-20 the weapon name starts a little further
   right than on rows 1-9 and no longer covers the two-digit number; long
   names still end inside the row. Save the map. If no `slot pane build` line appears, send
   the log: the hooks did not run.
1b. Front end -> MapMaker -> load the map -> Arcade -> setup -> Weapons ->
   choose the MapMaker set: 20 rows, 7-20 show the saved slots (read-only,
   as stock's 1-6; A/X do nothing there). Profile sets: rows 7-20 empty.
   The in-play pause has no weapon set screen (stock).
2. Place a weapon pickup, open its options: a "Weapon" choice lists the
   item's own slot and "7: ..." .. "20: ...". Pick one. Do the same for an
   ammo pickup. Place a Story AI: its weapon choice lists 20 slots.
3. Save, reload: the slots, the pickups' choices and the AI's weapon are kept.
4. Test map / play (Arcade, map's weapon set): the pickups spawn the chosen
   weapons, the AI carries its weapon, bots use weapons from all used slots;
   stderr has no `[mapmaker] weaponset:` or `heap full` line.
5. A map that uses none of this saves as before (stock size). A map using it:
   a stock build refuses it; System Link refuses it.
6. Without the switch: six rows (`slot pane build (stock 6 rows: ...)`), no extra choices; the map from step 3 still
   loads and plays with its 20 slots.
