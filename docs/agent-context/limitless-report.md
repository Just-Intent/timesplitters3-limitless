# MapMaker map report (extends [limitless.md](limitless.md))

Owner doc: [mapmaker-report.md](../mapmaker/mapmaker-report.md) (player view,
sample, checks, playtest steps).

Entry points:
- `src/interface/pc_mapmaker_report.inc` (area `bugfix`, included by
  `pc_menu_mouse_guest.inc` after `pc_mapmaker_limits.inc`/`pads.inc` and before
  `pc_mapmaker_validate.inc`). `mapmaker_report_level(container, span, hidden)`
  is called from `pc_mapmaker_validate_level` (existing WRAP `sub_0006D7A0`)
  after `original()` and before the hidden tags are restored, only on the
  scene-100 one-time map pass. No new WRAP, stub or regeneration; the call is
  `#ifdef PC_MAPMAKER_REPORT_INC`, so `mapmaker_validate.c` still builds alone.
- Read-only on guest memory (fixture snapshot check, `writes-guest` mutation,
  regex in `test_report_is_wired_into_level_setup`). Output: stderr lines
  `[mapmaker] map report: ...`, a summary, at most 20 problem lines
  (`MAPMAKER_REPORT_PROBLEMS`) ordered caps, damage, unplaced Story AI, logic,
  navigation, teleporters, overlaps; the last text stays in
  `mapmaker_report.text` for the fixture.
- Timing evidence (static, text.asm): one caller runs the level load
  `sub_001CDE30` at 0x0003D645 (stitcher `sub_0009DEB0` at 0x001CE0CE, group
  matrix via `sub_00158BA0` at 0x001CE14A) and then `sub_0006EC10` ->
  `sub_0006D7A0` at 0x0003D64A, so the pad, link and group tables exist when
  the report runs.
- Data: container layouts as in the file header (piece record length word
  +0x24, x/z/floor bytes +4..+6, roof mark +0x26; Story AI count +0x2C, points
  +0x44 + 4k, boss byte +0x29; logic type +0x80, indices +0x24 + 8k);
  catalogue `0x00524F58 + id*0xC4 + 2`; live maxima `0x00524BE0 + c*24 + 4`;
  pads `[0x00715F2C]`/`[0x007B6664]`, links `[0x00715F30]`/`[0x007B6660]`,
  groups `[0x007B666C]`, cover `[0x007B65C0]`; heap free `[0x006B8384]`, arena
  `[0x004C0450]`; grid = world / 6.0 (`[0x00475D30]`), floor = y / 5.0.
- Connectivity: union-find over regular pads by group (+8) and by links with
  flag 0x10000 (what the group A* crosses), mask and door bit ignored; main
  area = the largest. Pad ids decode as `pc_mapmaker_pads.inc` (regular
  0x3E9+, high 0x5B29+ from 8000, cover 0x2329+ in `[0x00715F34]` -> owner
  word +8, a regular id) and are verified against pad +0. Saved Story AI points
  are cover ids (0x232C..0x2596 in the probe saves).
- Offline: `tools/testing/map_report.py` (container-only checks, no pads).
  Shared constants are checked across languages by
  `test_tool_matches_the_report`; its item-category and stock-maximum tables by
  `test_category_table_matches_the_game` (private XBE, skipped without it).

Tests: `tests/mapmaker/test_mapmaker_report.py` (fixture
`mapmaker_report.c`, mutations, cap/wiring checks) and
`tests/mapmaker/test_map_report_tool.py`; both in
`.github/workflows/runtime-patches.yml`; `tests/input/test_menu_mouse.py`
compiles it in the whole chain with `windows.h` (no identifiers `near`/`far`:
they are macros there). Not built or played; no Mac-specific part (shared
include chain).
