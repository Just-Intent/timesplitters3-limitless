# MapMaker custom sets: 20 character slots

Status 2026-10-31: character slots implemented in
`src/interface/pc_mapmaker_sets.inc` (area `sets`), fixture-tested
(`tests/mapmaker/test_mapmaker_sets.py`: mutations plus decode and XBE
facts; `tests/mapmaker/test_mapmaker_sets_classified.py`: every slot
constant around the set classified). 2026-11-01: the editor's slot screen
is a 20-row scrolling list instead of two pages ("Editor" below). Not built
or played. No guest-code
table (`config/guest-code-patches/sets.json` does not exist; wrappers only).
Weapon sets have their own area: [mapmaker-weaponset.md](mapmaker-weaponset.md).

User request (2026-10-31): "Add many more slots to the custom mapmaker bot and
weapon sets. I'd like at least twenty. The additional slots can default as
empty or not set."

## Where ten is fixed (stock, CONFIRMED)

The set struct is already 20 members wide: dword count + 20 x 8 bytes (word
id, byte, byte +3, word variant at +6) = 0xA4. Presets 0x0053E41C + n x 0xA4
(count 10, members 10..19 zero); editor custom set editor +0x11860 (the weapon
set follows at +0x11904); play builder 0x0075FB58 and getter 0x00762DC0
(`rep stosd` 0x29 at 0x002C0231 / 0x00312967); the new-map preset copy
`sub_002F9E90` (0x002F9F21..0x002F9F42, whole 0xA4); the play set
`[desc+0x150]+0xE0` inside a 0x190-byte block (0x00240B68). The spawn
lookup `sub_0006BC50` accepts slots 0..19 (`cmp ebx, 0x13` at 0x0006BC55).
Ten is fixed in:

| Place | Site | Change |
| --- | --- | --- |
| Map-info chunk 0xC77668: count byte 10, 10 x 4-byte members at data +0x18, weapon set index at +0x40; chunk size 0x3DC is full | compile 0x00300884, loader 10 unrolled copies 0x00301D74..0x00301F52 | fork-only chunk for members 11-20 |
| Slot screen: 10-row work copy 0x0075FD90 (0x1C each, shared with the Arcade front end), builder `sub_002C19F0` 10 rows, fill `sub_002C0310`/`sub_003141B0`, store `sub_00314210` | 0x002C1C0A, 0x002C0371, 0x002C0555 | paging |
| Story AI character choice: 10 options, slot k named by item 0x38 + k (`sub_00303350`) | 0x0031CD24 | 10 more options |
| Map Settings "set changed" test `sub_00312810` (undo step on close) | 0x0031286A | compares 11-20 too |
| Level load copy `sub_0033EBF0` (ids, count byte 10) and preload `sub_0023F320` (count byte) | 0x00240DCE, 0x0023F472 | members 11-20 added |

Not set slots (classified in the decode test): the 10 Arcade bot slots of
the front end and match start (`sub_002D2C70` 0x002D381C, bot count
0x002C044A), team sub-mode `9 - profile+0xC`, game-mode compares, item
category ids 8/9/0x13/0x14, the actor cap 20 at 0x001640EC. Arcade bots and
human players keep using members 1-10 (`sub_002C0310` fills the 10 bot slots,
`sub_002D2C70` picks `i % count` for at most 4 players).

## Save format

A map whose slots 11-20 are all empty and whose Story AI all use slots 1-10
saves byte-identical to stock. Otherwise `pc_mapmaker_sets_compile` (after the
compiler, which zeroes the whole buffer first) writes, in place of the
terminator at `used - 8`:

- tag 0x00C776F5 (no game code uses it; stock tags end at 0x00C77675), size
  0x30: +0 word version 1, +2 byte first slot 10, +3 byte count 10, +4 ten
  members in the map-info form (word id, byte, sbyte variant), +0x2C zero;
- then the terminator 0x00C7766A with its offset, and info `+0x248` (used)
  += 0x38; the return value follows (the container hook's low-word rule for
  the save compile 0x0032C6FF is repeated).

Every chunk walker skips an unknown tag by its size (`sub_0032A5B0` and the
compare chains at 0x0008599A, 0x0008A807, 0x0023F3CE, 0x0023FD3F,
0x002405D8, 0x00240D6B, 0x0032A715; editor loader switch default 0x00301C08).
`mapmaker_container_over_stock` (`pc_mapmaker_container.inc`, one line) treats
the tag as past stock, so such a map saves in the 64 KB tier: a stock build
refuses it (error 7) instead of reading an id-0 character for slots 11-20,
and System Link refuses it in a session like any larger map (the host never
sends it). Old maps load unchanged. Container room: 0x38 bytes, refused and
logged (`[mapmaker] sets: no room ...`) only within 0x3C bytes of the 512 KB
limit.

## Editor

- Load (`sub_00301A90`): members 10..19 from the chunk, else empty (id -1,
  variant -1). New map (`sub_002FEBF0`): the preset copy leaves id 0 there,
  so they are cleared to empty.
- Slot screen (2026-11-01, replaces the paging; `TSFP_MAPMAKER_LIMITS=1`,
  scene 102, any set): a 20-row scrolling list built like the weapon set's
  ([mapmaker-weaponset.md](mapmaker-weaponset.md), "Editor"). User request:
  "weapon set and bot set customization screen scrolling work differently ...
  use the same scrolling behavior and add a scroll bar for bot set
  customization". Screen 0x0052375C = {`sub_002C2C20` builder,
  `sub_002C0490` update}; its slot pane 0x0052372C = {`sub_002C19F0`,
  `sub_002C1570`}; the picker 0x0052374C = {`sub_002C1EA0`, `sub_002C1E10`}.
  CONFIRMED stock (decode test `test_slot_list_facts`):
  - The editor branch of the pane builder adds frame `sub_00075B70(0x10, 0,
    45.0f, 0, 0x002C1450)`, `sub_00071BE0(1, 5.0f, 5.0f)`, title 0xE8B
    (value -1, flags 0x04032404), set list 0x004766D8 (value -2, flags
    0x08030C00), `sub_002C1830(menu)`, `sub_00079710(0)`, ten row groups
    (`sub_00078320(0, 1, 4)`, one item `sub_000792B0(0, row, 0, 40.0f, 4.0f,
    sub_002C1490, sub_002C0660)`, `sub_00078380()`), `sub_00077140(menu)`,
    `sub_00077360(menu, 5.0f, 9.2f)`, footer 0xE88 / 0x3B2 / 0x1A27.
  - The row callbacks index the 10-row work copy 0x0075FD90 (0x1C a row) by
    row: the draw by item +0x14 (0x002C0667; the editor names rows below 10
    only, 0x002C08AF; the highlight compares the item pointer, 0x002C0A2B),
    the row action and the pane update by pane `+0x184 - 2` (0x002C1499,
    0x002C157E; the editor stops at 10, 0x002C14B3 / 0x002C16D8; the update
    shows hints, clears the row on X 0x002C1718 and sets the preview
    `[0x0078AEA8]` 0x002C17F2), the picker's pick by the slot pane's `+0x184
    - 2` (0x002C1CC7..0x002C1D56; it moves the slot highlight on while
    `+0x18C < 9`, 0x002C1D8E). The work copy is followed by other data
    (0x0075FEA8..), so it cannot grow in place; more than 100 references
    read it.
  - Builder hook `pc_mapmaker_sets_build` (`sub_002C19F0`, editor, switch
    on): the same calls with twenty row groups inside `sub_000780E0(40.0f,
    0)` .. `sub_000781A0()` (ten rows of 4.0, INFERRED unit as the weapon
    pane's; the pane keeps its stock size). A group end counts one row
    (`+0x180`, 0x000783C8), items made in the region get flag 0x40
    (0x0007914A) and a row's last item (`+0xB0`, set by `sub_00078380`) adds
    its height to the content `+0x5C` (0x000753CD). No stock menu puts
    groups inside a region (INFERRED to work: the layout code has no
    group-specific branch for it). Row index stays value + 2.
  - Rows 11-20 (values 10..19) are ten host-side shadow rows. Each stock
    callback runs for row 10 + k with shadow row k lent to work-copy row k
    and the row index shifted (item +0x14 = k, or pane `+0x184` = k + 2 and
    `+0x18C` = k), then everything is put back and the row is stored to
    member 10 + k (word id, byte difficulty +3, word variant +6, as
    `sub_00314210`). Hooks: draw `sub_002C0660`, row action `sub_002C1490`,
    pane update `sub_002C1570`, pick `sub_002C1C90` (chained outside
    realbosses' pick, so its boss-row fix-up sees the lent row; during the
    pick `+0x18C` is 9 so the stock move stays off, and the hook moves the
    highlight to the next slot itself; slot 10 also moves on to 11).
  - Loading the shadow (`pc_mapmaker_sets_screen` on `sub_002C0490`, when the
    pane was built or the set index changed): the update runs once with the
    fill getters on members 10..19 (`sub_002C0290` returning to 0x002C0319,
    `sub_00312950` returning to 0x003141B5, the tail store to the scratch set)
    and `[0x0078AEAC]` = -1, so the stock reload, with the portraits it asks
    `sub_002BE600` for at return 0x002C054C (realbosses' boss rows), fills
    the work copy with slots 11-20; those rows become the shadow, the work
    copy and `[0x0078AEAC]` are put back, the return address is pushed again
    and the original runs as stock for slots 1-10. Presets (index < 0xC) get
    empty rows 11-20 and nothing is stored there (their members 10..19 are id
    0, a real character).
  - Difficulty default: a pick into an empty row copies the difficulty of the
    nearest used row above it in the work copy (0x002C1CFA); for slots 11-20
    that search sees slots 1-10, not 11..k. Team/difficulty are not shown in
    the editor (one column) and the Test map takes rows 1-10's for slots
    11-20 (matchbots).
  - The title is the stock one (no "(1-10)" / "(11-20)").
  - Diagnostics (stderr, own budget): every build `[mapmaker] sets: slot pane
    build (20 rows, scrolling) ret=... menu=... set=N` (or `(stock 10 rows:
    ...)`), the first pane update `slot pane open: pane=... items=22
    content=... view=a..b scroll=... slot=...` and moves `list: slot N
    highlighted, scroll S of MAX` (rows 11-20 or a scroll change; at most
    48). INFERRED: the pane gets the same current style as the weapon pane
    (`sub_00075B70` takes `sub_000802F0()`), which the editor wheel
    (`mapmaker_pointer_wheel`, style flag 0x40) needs.
  - Not built or played.
- Story AI character (`sub_00078590` returning to 0x0031CD32): ten more
  options, value 10..19, named like the stock ones (`sub_002BE650(id,
  variant)` through `sub_0003D7B0`, empty text 0xD7C). Without the switch they
  are added only for an AI that already uses slot 11-20. The Story AI chunk
  stores the slot as byte +0x28.
- Story AI icons (user report 2026-10-31, build .8/.9: slots 11-20 showed
  no name and a time crystal icon). CONFIRMED: a Story AI is drawn as
  catalogue item 0x38 + slot (9 for type 5): `sub_0031D3E0` (0x0031D406,
  only caller the editor overview's AI marker `sub_00309A80`, icon call
  0x00309AC2) and the Story AI list's icon row callback `sub_0031D410`
  (pushed by `sub_0031D4C0` at 0x0031D532; 0x0031D442, icon call
  0x0031D45B). `sub_00315D20(item)` draws catalogue +0xB4 (flag +0xB8 &
  0x400). Items 0x38..0x41 all have icon 0x42 / flags 1; slots 11-20 gave
  items 0x42..0x4B, other items (0x42: icon 0x45, the time crystal the user
  saw). `pc_mapmaker_sets_icon` (`sub_00315D20` returning to 0x0031D460 or
  0x00309AC7 only) draws items 0x42..0x4B there as item - 10, the same icon
  as slots 1-10; the palette and other callers keep the real items.
- Slot names (CONFIRMED path, disassembly + whole-chain fixture). Slots
  1-10: the loop 0x0031CCF8..0x0031CD27 calls `sub_00303350(0x38 + k)`
  (0x0031CD03, ret 0x0031CD08; empty member: `sub_0003D7B0(0xD7C)`), whose
  case 3 (0x0030339E) reads member item - 0x38 of `sub_002C0290(
  sub_003129D0())` (call 0x003033A4, ret 0x003033A9; scene 102 custom
  index -> `sub_002BFEF0` -> `sub_00312950` = editor +0x11860) and calls
  `sub_002BE650` (0x003033BB, ret 0x003033C0) = `sub_002BE300(
  sub_002BE600(id, variant))` (rets 0x002BE65F / 0x002BE665), then jumps to
  `sub_0003D7B0`. realbosses names ids 0x15 / 0x28 (rows 150 / 151 ->
  synthetic text -> "Giant Trophy Monster" / "Giant Skeleton") only for
  `sub_002BE600` returning to 0x002BE65F with 0x003033C0 above it; the slot
  screen uses its reload return 0x002C054C. The choice shows the option
  whose value is the current one (`sub_00078590` -> `sub_00075540`), so the
  list and the selected value share these labels.
  Slots 11-20 were named from the host (`guest_call` return address), so
  `sub_002BE600` returned -1 for both bosses; v0.4.0-mapmaker.11 remapped
  only id 0x15 to row 150, so a Giant Skeleton there had no name (user
  report). Now `sets_label` runs the stock path itself:
  `sub_00303350(0x38 + k - 10)` with `sets.name_shift` set, and
  `pc_mapmaker_sets_set_ptr` (existing `sub_002C0290` WRAP) adds 10 members
  for return 0x003033A9 while it is set; every name hook (current and future
  realboss ids) applies as for slots 1-10. Without the realbosses area both
  halves show stock's empty text. `tests/mapmaker/mapmaker_sets_names.c`
  chains the real hooks (realbosses inside sets) on a real guest stack;
  its mutations include the mapmaker.11 code. Not built or played. The
  portrait in the list row (`sub_0031BF60`) already reads the 20-member set.
- Map Settings close (`sub_00312810`): a change to slots 11-20 against the
  undo shadow `[0x00765E38]` also counts, so it gets its undo step.

## Play

- `sub_0033EBF0` (level load, from 0x00240DCE): members 10..19 from the chunk
  and count 20. The count's users skip id -1 (`sub_0025F830` 0x0025F86D,
  `sub_0015C560` 0x0015C61D); `sub_00158BC0`'s unchecked loop only runs
  outside scene 100. A Story AI whose slot is empty (any slot, maps with the
  chunk only) gets the first used slot, logged (`[mapmaker] sets: story AI
  uses an empty character slot ...`); stock would spawn id -1 (INFERRED
  unsafe: character table row -1).
- `sub_0023F320` (custom-set preload, from 0x0023F477): also preloads the
  used members 11-20 (`sub_000D7780(id, id)`).
- Memory: each extra distinct character costs its model, textures and
  animation data in the Level heap (not measured). The memory area's heap
  guard logs `[mapmaker] heap full: ...` on an overrun; with many distinct
  big characters, watch it and the probe's free-memory figure.

## Pause and Arcade Bots screen (2026-10-31; paged by the matchbots area)

User report: 20 slots in the editor, but the pause menu's screen for the
custom MapMaker bot set shows 10. CONFIRMED path: pause -> game setup
(descriptor 0x004E9544 = {`sub_001A40A0`, `sub_001A4070`}, pushed at
0x001AFFF7) -> `sub_0019A000` (0x001A40E0) -> setup builder
`sub_002C78F0(menu, 1, sub_00196DE0)` (0x0019A01B; the front end calls it at
0x002D5EDF, the editor's Test map setup `sub_002EFBF0` at 0x002EFC22) -> Bots
item (text 0x3B8, action `sub_002C7730` at 0x002C81CD, shown unless mode
0xB/0xC/0xD) -> the same slot screen 0x0052375C. Outside scene 102 it
differs:

- The set index is the pane's item -2 (`sub_002C0490` 0x002C04D4), stored in
  the profile settings `sub_00069640()+0x6A8`. Index 0x11 (the map's set) is
  offered only in game mode 9 (`sub_002C1830` 0x002C18AC/0x002C18B5); mode 9
  is MapMaker Arcade (`sub_002C8400` 0x002C8429, after reading the map info).
- Fill: `sub_002BFEF0` returns, for mode 9 and index 0x11, `sub_00312950`'s
  non-editor path = 0x00762DC0 rebuilt from the map-info chunk (10 members),
  else profile set `settings + index*0xA4 - 0x4E4` (0x002BFF15).
- Store (`sub_002BFFD0` 0x002BFFEC..): member ids and the per-row team /
  difficulty arrays (`settings + 0x6B0/0x6D8/0x700/0x728 + index*0xA0`, 10
  per index) in the profile settings, never the map. So stock pause edits are
  profile/session state; for index 0x11 in mode 9 the getter ignores even
  that and the screen reloads from the map on reopening (`sub_002C2C20` sets
  `[0x0078AEAC]` = 0).
- Apply: the rows ARE the match's bots. `sub_002D2C70` (front end 0x002D5E4D,
  `sub_002D38C0` 0x002D392E for setup confirms including the editor's Test
  map) loops the 10 rows (0x002D36EA..0x002D381C) into per-actor bytes after
  the players: characters 0x0078AD6C, teams 0x0078AD7C, difficulty
  0x0078AD8C, variants 0x0078AD9C - 16 bytes each, 16 actors per match (the
  0xA0-byte match block 0x0078AD58). The actors table 0x004D239C allows 14 or
  16 actors (mode row 0x10: 14, 14, 16, 16).

So paging this screen to 20 needed more than the set: at most 16 players +
bots, at most 10 rows in the work copy shared with the front end. That is
now the matchbots area ([mapmaker-matchbots.md](mapmaker-matchbots.md)):
with `TSFP_MAPMAKER_LIMITS=1` this screen pages in MapMaker Arcade (rows
11-20 kept by its hooks, the map set's slots 11-20 as their default) and
the match gets up to 20 bots / 24 characters; System Link stays at 10.

What changed: the editor's Test map setup Bots screen (scene 102) is the same
20-row list as Map Settings, and its rows become the test's bots. The list
always leaves rows 1-10 in the work copy; should the getters still be
shifted at a match start, `pc_mapmaker_sets_match_start` refills rows 1-10
first (`sub_002C0310` with the index in eax, getters unshifted), so the
test's bots stay slots 1-10 as in stock. The front end and in-play pause
Bots screens (scenes 100/101) keep the matchbots paging: their rows have
team and difficulty columns with their own callbacks, and are the match's
roster.

## Extension memory

0x00903700..0x0090373F title text (matchbots' pages), 0x00903740..0x009037E3
store scratch for members 10..19; 0x009037E4..0x009037FF reserved for this
area. The list's shadow rows (10 x 0x1C) are host memory.

## Hooks (WRAP lines for the integrator, area `sets`)

```
WRAP sub_003006C0 -> pc_mapmaker_sets_compile      (chained: OUTSIDE pc_mapmaker_container_compile; order vs aitemplates/bosses/sky/roof free)
WRAP sub_00301A90 -> pc_mapmaker_sets_load         (chained: editor, aitemplates, bosses, roof; order free)
WRAP sub_002FEBF0 -> pc_mapmaker_sets_reset        (chained: bosses, sky, roof; order free)
WRAP sub_00312810 -> pc_mapmaker_sets_changed      (new)
WRAP sub_002C0490 -> pc_mapmaker_sets_screen       (new)
WRAP sub_002C0290 -> pc_mapmaker_sets_set_ptr      (new; per frame, return-address test)
WRAP sub_00312950 -> pc_mapmaker_sets_editor_set   (new; per frame, return-address test)
WRAP sub_00314210 -> pc_mapmaker_sets_store        (new)
WRAP sub_00078590 -> pc_mapmaker_sets_sai_choice   (chained: bosses, weaponset; every choice item, return-address test)
WRAP sub_00315D20 -> pc_mapmaker_sets_icon         (new; per drawn item icon, return-address test 0x0031D460/0x00309AC7)
WRAP sub_0033EBF0 -> pc_mapmaker_sets_play_set     (new; level load)
WRAP sub_0023F320 -> pc_mapmaker_sets_preload      (new; level load; argument in ebx, untouched)
WRAP sub_002D2C70 -> pc_mapmaker_sets_match_start  (chained: story's pc_mapmaker_story_char_config; order free)
WRAP sub_002C19F0 -> pc_mapmaker_sets_build        (new, 2026-11-01; slot pane builder, replaces it for the 20-row list)
WRAP sub_002C0660 -> pc_mapmaker_sets_row_draw     (new, 2026-11-01; per drawn slot row)
WRAP sub_002C1490 -> pc_mapmaker_sets_row_action   (new, 2026-11-01; slot row action)
WRAP sub_002C1570 -> pc_mapmaker_sets_pane         (new, 2026-11-01; slot pane update, per frame)
WRAP sub_002C1C90 -> pc_mapmaker_sets_pick         (2026-11-01; chained: OUTSIDE realbosses' pc_mapmaker_realbosses_pick)
```

`pc_mapmaker_sets_screen` (existing WRAP) now calls its original twice when
it loads the shadow rows: it pushes the return address again between the
calls, so the layers below it (the generated original) run twice that frame.
`pc_mapmaker_sets_pick` must wrap `pc_mapmaker_realbosses_pick`, not the
other way round, or the boss rows picked into slots 11-20 keep row 150+ as
their id. The builder replaces the original (`MAPMAKER_GUEST_RET`). New
`guest_call` targets: `sub_00071BE0`, `sub_002C1830`, `sub_00078320`,
`sub_000792B0`, `sub_00078380`, `sub_00077140`, `sub_00077360` (stubs in
`tests/mapmaker/mapmaker_area_stubs.h`), plus the weapon builder's
`sub_00075B70`, `sub_00079D60`, `sub_00079710`, `sub_000780E0`,
`sub_000781A0`, `sub_00074A00` and `sub_00074460`. The `pc_menu_mouse_guest.inc`
pass-through list and `pc_display_menu.h` need the five new hook names.

All cdecl, plain `ret`; every hook calls its original once. Include the
`.inc` after `pc_mapmaker_limits.inc` (it uses `mapmaker_limits_read`).
`sub_000D7780` (preload) is the characters area's log-only wrapper; the extra
preloads return elsewhere and are not logged by it.

## Weapon sets

Raised to 20 slots by the `weaponset` area without new item types (a slot
byte on the weapon/ammo pickup records, fork chunk 0x00C776F6):
[mapmaker-weaponset.md](mapmaker-weaponset.md).

## User test steps

1. `TSFP_MAPMAKER_LIMITS=1`. Editor -> Map Settings -> character set: custom.
   The slot list shows ten rows with a scrollbar, like the weapon set's;
   stderr has `[mapmaker] sets: slot pane build (20 rows, scrolling)` and
   `slot pane open: ... items=22`. Scroll with the wheel, the scrollbar or by
   moving the pointer/Down past row 10: rows 11-20 (empty). Pick characters
   into a few of them (A, choose; after a pick the cursor moves to the next
   row; X clears). Slots 1-10 are unchanged. A preset set: rows 11-20 stay
   empty and inert. If no `slot pane build` line appears, send the log.
2. Place a Story AI, open it: the character choice lists 20 slots; pick one
   from 11-20 (named like 1-10; the Giant Trophy Monster by that name). Back
   in the Story AI list and the editor overview, its icon is the Story AI
   icon of slots 1-10, not a time crystal. Save, reload the map: the slots
   and the AI's choice are kept.
3. Play the map (Story): the AI spawns with its character; stderr has no
   `[mapmaker] sets:` or `heap full` line. Point an AI at an empty slot: it
   spawns as the first used slot and the log says so.
4. A map with slots 11-20 empty and no AI on them: saves as before (stock
   size). A map using them: a stock build refuses to load it; System Link
   refuses it.
5. Without the switch: ten rows (`slot pane build (stock 10 rows: ...)`), 10
   choices; the map from step 2 still loads and plays with its 20 slots.
6. Editor -> Test map setup -> Bots: the same 20-row list. Scroll down, back
   out and start the test (Arcade type): rows 1-10 are the first ten bots,
   slots 11-20 more bots (matchbots area). Front end / in-game pause -> setup
   -> Bots on a MapMaker map: still two pages (matchbots).
