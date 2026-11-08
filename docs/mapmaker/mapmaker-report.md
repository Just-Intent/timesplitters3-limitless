# MapMaker map report

[Back to the MapMaker guide](mapmaker-limitless.md)

Every time a MapMaker map starts (Play from the editor, or a saved map started
from the front end), the game writes one short **map report** to its log
(`game.err` beside the game, or the console when run from source). It counts
what the map holds against its limits and names problems that have caused bugs
in playtests, each with the grid position to look at in the editor. An
offline checker prints the same kind of report for a saved or exported map
without starting the game.

Nothing is shown on screen: the report is log only. A message at Play would
need a new editor hook and a new screen; the log is free and already collected
with bug reports. The report never changes the map or the game.

## Sample

```
[mapmaker] map report: "Fort Island" (story, raised limits), 312 tiles, 140 items, 12 Story AI (3 bosses), 18 logic entries
[mapmaker] map report: navigation: 2950 pads, 410 cover pads, 4100 links, 455 pad groups; 2 connected areas, the main one 2934 pads
[mapmaker] map report: level memory: 9216 KB free of a 46000 KB arena
[mapmaker] map report: open tiles: 0 no roof, 4 open air (unbounded: falling off the map kills), 0 open air (bounded)
[mapmaker] map report: near limit: turrets (type 1) 23/24 (95%)
[mapmaker] map report: 5 problems ("possible" = a heuristic check):
[mapmaker] map report:   problem: Story AI "Guard 2" has no position (shown with ! in the editor list): it is not placed
[mapmaker] map report:   problem: logic entry 5 (link 2) names tile #313, which is not on the map (deleted?)
[mapmaker] map report:   possible problem: Story AI "Sniper" starts at grid 50,4 floor 1 in an area of 16 pads not linked to the main area
[mapmaker] map report:   possible problem: 16 pads around grid 48,4 floor 1 are not linked to the main area (AI cannot walk there or back)
[mapmaker] map report:   problem: teleporter colour 2 has a single teleporter: it needs a partner of the same colour
[mapmaker] map report: end
```

Grid positions are editor cells (`x,z`) and the floor number, as the editor
shows them. "Tile #n" counts placed tiles in the order they were placed; "tile
n" is the tile's number in the tile list. At most 20 problem lines are shown,
most serious first; the rest are counted (`... and 7 more not listed`).

## What is checked

**Counts against caps.** Over a cap is a problem; above 90% is listed as
"near limit" (caps below 10, such as 8 teleporters of a colour, only when
passed). With **Raised MapMaker limits** (`LIMITS`) the raised caps apply,
otherwise the original ones.

| Count | Cap (`LIMITS` / original) | Source |
| --- | --- | --- |
| Tiles | 1023 / 200 | tile records in the map |
| Items (catalogue ids 10 and up) | 500 / 50 | items on tiles |
| Engine objects (estimate) | 2047 | tiles + items + 128 reserved |
| Story AI | 511 / 50 | Story AI records |
| Logic entries | 400 / 100 | logic records |
| Pads, cover pads, links, pad groups | 16384, 14336, 32512, 2047 | the game's navigation tables after loading |
| Each item category (weapons, ammo, keycards, teleporters per colour, turrets per type, movers, props, vehicles, ...) | the game's live per-category maximum | items on tiles |

Bosses are counted, never flagged: any number of copies of every boss type is
allowed.

**Problems found from the data the game uses (reliable):**

- parts of the map file that the [validator](mapmaker-bugfix.md) skipped as
  damaged, or tile records that end early;
- a Story AI with no position (the `!` in the editor's list): it is not placed;
- a Story AI point whose pad id is no pad of the loaded map (points usually
  sit on cover pads; a cover pad counts as the pad it belongs to);
- a logic entry that names a tile past the placed ones (the editor saves that
  when the tile it pointed at was deleted);
- a teleporter colour with a single teleporter;
- two tiles at the same grid cell and floor (an overlap) when `OVERLAP` is off;
  with `OVERLAP` on only their number is given;
- the level memory heap running out while the level loaded.

**Possible problems (heuristic):**

- pad areas not linked to the main area. The main area is the largest
  connected set of pads; two pads are connected when they share a pad group or
  a link the AI route search may cross. Doors and one-way links count as open
  in both directions, so a real problem can hide behind a one-way door, and a
  small separate area can be harmless (a ledge nobody needs to reach). The three
  largest separate areas are listed, the rest summed;
- a Story AI whose first point is in such an area, or whose later points
  cannot be reached from its first;
- a teleporter colour with an odd number of teleporters;
- less than 1 MB of level memory left after loading.

**Not checked yet:** items or start points on no pad, outside every room, under
a floor or floating (item records do not say which pad they use, and nothing in
the game answers that cheaply at load); open-air settings beyond the count
above.

## Checking a map without the game

```powershell
.venv\Scripts\python.exe tools\testing\map_report.py "Backups\Maps\Fort Island.tsmap"
.venv\Scripts\python.exe tools\testing\map_report.py private\saves      # every map in a save folder
.venv\Scripts\python.exe tools\testing\map_report.py --logic "Backups\Maps\Fort Island.tsmap"
```

A map with logic gets a `logic:` line (operations, actions, how many have a
[Delay](mapmaker-limitless.md#logic-delay-on-actions-limits)). `--logic` also
prints every operation in plain words, for example `operation 3 "Wave 1":
when Wave 1 start timer Reaches 0m 05 seconds -> Spawn New Enemy 1, ...`
(triggers joined by "and"/"or", "NOT" in front of an inverted trigger, and
"(after 5 s)" on a delayed action).

It accepts a `.tsmap` export (from the launcher's Saves page), an
`anicemap.mkr` save file, one map's save directory, or a whole save folder. It
never writes anything. Exit status: 0 no problems, 1 problems, 2 a file could
not be read. It checks the counts, the map file's own bounds (what the game
would refuse at load) and the reliable problems above except the pad ones; the
navigation part needs the game's tile templates and is in-game only. It also
says when a map is past the original game's limits and so needs this build with
**Raised MapMaker limits**.

## Implementation

- In game: `src/interface/pc_mapmaker_report.inc` (area `bugfix`), called by
  `pc_mapmaker_validate_level` (the existing WRAP of `sub_0006D7A0`, level
  setup) after the original and before the validator restores the chunks it
  hid. No new guest wrapper and no regeneration. The navigation tables are
  built by then: the level load `sub_001CDE30` (called at `0x0003D645`) runs the
  stitcher (`0x001CE0CE`) and the group matrix (`0x001CE14A`) before
  `sub_0006D7A0` (`0x0003D64A`). Record layouts and addresses are listed in the
  file's header.
- Offline: `tools/testing/map_report.py`. Its caps, category names and item
  table are checked against the C source and (when present) the private XBE by
  `tests/mapmaker/test_mapmaker_report.py` and
  `tests/mapmaker/test_map_report_tool.py`.
- Tests: `tests/mapmaker/mapmaker_report.c` (fake guest memory: container,
  catalogue, pad/link/group tables; checks the report text and that guest
  memory is unchanged) with 35 mutations; the tool test builds `.mkr` and
  `.tsmap` files in the launcher's format.

Not built or run in game yet.

## Playtest

1. Open a map with Story AI in the editor and press Play. Quit the level and
   open `game.err`: one block of `[mapmaker] map report:` lines from the first
   line (map name and counts) to `end`.
2. Add a Story AI without placing it (the list shows `!`), press Play: the
   report names it ("has no position").
3. Delete a tile that a logic entry points at, press Play: "logic entry ... names
   tile #...".
4. Place one teleporter of a colour only: "has a single teleporter".
5. Build a room with no doorway to the rest of the map, put a Story AI in it,
   press Play: "pads around grid x,z floor f are not linked to the main area"
   and "Story AI ... starts at grid ...". Check that the grid position matches
   the room in the editor.
6. A map with several bosses of one type: they are counted, not listed as
   problems.
7. Export the map in the launcher and run the offline checker on the `.tsmap`:
   the same counts and the non-navigation problems.
