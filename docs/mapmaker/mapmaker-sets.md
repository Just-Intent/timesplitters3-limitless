# MapMaker custom sets: 20 character slots (weapon sets stay at 6)

Status 2026-10-31: character slots implemented in
`src/interface/pc_mapmaker_sets.inc` (area `sets`), fixture-tested
(`tests/mapmaker/test_mapmaker_sets.py`: 40 mutations plus decode and XBE
facts; `tests/mapmaker/test_mapmaker_sets_classified.py`: every slot
constant around the set classified). Not built or played. No guest-code
table (`config/guest-code-patches/sets.json` does not exist; wrappers only).
Weapon sets are not raised (see the end).

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
- Slot screen (`TSFP_MAPMAKER_LIMITS=1`, custom set index >= 0xC, scene
  102). Screen 0x0052375C = {`sub_002C2C20` builder, `sub_002C0490` update};
  its slot pane 0x0052372C. The update reloads the work copy when the set
  index differs from `[0x0078AEAC]` and stores it every frame
  (`sub_002BFFD0` -> `sub_00314210`). `pc_mapmaker_sets_screen` reads the
  pane's highlighted row (`+0x18C`) and input (`+0xC4`): Down on row 10 shows
  slots 11-20 (cursor on the first row), Up on that row returns (cursor on row
  10). On a flip it stores the current page, clears the direction bits, moves
  the highlight (`sub_00071970`, `sub_00076EF0`) and sets `[0x0078AEAC]` to -1
  so the original reloads. While page 2 is shown, the fill's set pointer
  (`sub_002C0290` returning to 0x002C0319, `sub_00312950` returning to
  0x003141B5) moves by 10 members and the store (`sub_00312950` returning to
  0x00314218) writes a scratch set, copied to members 10..19; the count
  (used rows of 1-10) keeps its stock meaning. The title pointer (menu
  `+0x204`, stored by `sub_00075B70`) is pointed at a copy with "(1-10)" or
  "(11-20)". Opening the screen or changing the index returns to page 1.
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
- Slot names: the choice used `sub_002BE650` from the host, so the realbosses
  area (row 150 only for `sub_002BE600` returning to 0x002BE65F from
  0x003033C0) never named the Giant Trophy Monster (id 0x15) in slots
  11-20. The label is now `sub_0003D7B0(sub_002BE300(row))` with row =
  `sub_002BE600(id, variant)`, -1 for id 0x15 -> 150 (the realbosses text;
  stock `sub_002BE300(150)` = 0 = stock's -1). Ordinary ids: same text as
  before. Not built or played: whether a generic bot's name was also
  missing in-game is not reproduced here (the choice text path is the stock
  one); the portrait in the list row (`sub_0031BF60`) already reads the
  20-member editor set.
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

What changed: the editor's Test map setup Bots screen (scene 102) is paged by
the existing hooks, and its rows become the test's bots. A match start after
the screen closed on "(11-20)" refills rows 1-10 first
(`pc_mapmaker_sets_match_start`: `sub_002C0310` with the index in eax,
getters unshifted), so the test's bots stay slots 1-10 as in stock.

## Extension memory

0x00903700..0x0090373F title text, 0x00903740..0x009037E3 page-2 store
scratch; 0x009037E4..0x009037FF reserved for this area.

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
```

All cdecl, plain `ret`; every hook calls its original once. Include the
`.inc` after `pc_mapmaker_limits.inc` (it uses `mapmaker_limits_read`).
`sub_000D7780` (preload) is the characters area's log-only wrapper; the extra
preloads return elsewhere and are not logged by it.

## Weapon sets: not raised (CONFIRMED why)

A weapon set is 0x44 bytes: name, count, 15 ids (`rep movsd` 0x11 at
0x0006A76F; System Link copy to 0x00761388 at 0x002D39F6). Map info stores 6
(0x003008D6, editor +0x1190C). But a weapon slot is an item type: catalogue
0x00524F58 items 0x11..0x16 are weapon slots 1-6 and 0x17..0x1C ammo slots
1-6 (row +8 = slot; `sub_002FE840` `id - 0x11 < 6`, `id - 0x17 < 6`;
`sub_00303350` table 0x003033FC), and the editor's weapon check walks six
slots (`sub_003004E0`, +8..+0x20). Slots 7-20 would need 28 new item types
past the 80-entry catalogue (editor palette, every `id < 0x50` check, item
records, the category maxima, the stock-max save check) and a larger set
struct everywhere it is copied. That is a separate feature, not a slot raise.

## User test steps

1. `TSFP_MAPMAKER_LIMITS=1`. Editor -> Map Settings -> character set: custom.
   The title reads "(1-10)". Move to the last row and press Down: the title
   reads "(11-20)" and the rows are empty. Pick characters into a few of them;
   press Up on the first row: slots 1-10 are unchanged.
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
5. Without the switch: no paging, 10 choices; the map from step 2 still loads
   and plays with its 20 slots.
6. Editor -> Test map setup -> Bots: pages like Map Settings. Leave it on
   "(11-20)", back out and start the test (Arcade type): rows 1-10 are the
   first ten bots, slots 11-20 more bots (matchbots area). Front end /
   in-game pause -> setup -> Bots on a MapMaker map: paged too (matchbots).
