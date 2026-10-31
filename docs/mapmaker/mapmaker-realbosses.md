# MapMaker real story bosses: the giant trophy monster

Status: implemented in `src/interface/pc_mapmaker_realbosses.inc` (area
`realbosses`), fixture-tested (`tests/mapmaker/test_mapmaker_realbosses.py`:
49 mutations, plus decode and US XBE data checks of every site below). The
creature was played in v0.4.0-mapmaker.5 (user: working); the floating-meter
fix and the health display choice below are not built or played yet. No guest-code table entry. Switch: `TSFP_MAPMAKER_LIMITS=1`;
`TSFP_MAPMAKER_REAL_BOSS=0` keeps the creature on the normal bot AI (stage 1).

User report (2026-10-31): the trophy monster from story mission 5 is much
bigger than the Arcade trophy monster and was not selectable. It is a
separate character (research: `logs/limits/features/real-bosses.md`, main
checkout, ignored):

| | value (CONFIRMED) |
|---|---|
| character id | 0x15 (no Arcade-table row) |
| character table 0x0053A3CC + 0x15*0x30 | voice set 0x00538B08 (the Arcade trophy monster's), model 0x5BE `CHR_MANSION_DEERBEAST`, LOD 0x5BF |
| story AI class | AI +0x14 = 0x80, +0x10 = 0: `sub_0027D590` 0x0027DB94 -> `sub_00280FA0` (0x0027DBE2) |
| animations | `mattackheadbutt` .. `mcharge`, named only by `pak/stream/lv1/anim/lv1_3.pak` (string 0x0049D77C; `sub_001CA2F0` registers them at 0x001CADA3 when the pak is mounted) |
| model in MapMaker play | `pak/chr.pak` (mounted in scene 100); `sub_000A5E40` skips the story-only flag 0x2000000 in scene 100 (0x000A5E83) and honours it in the editor (0x66, 0x000A5E93) |

## Stage 1: selectable in the custom character set (editor, scene 102)

The picker (`sub_002C1EA0`, descriptor 0x0052374C) adds Arcade-table rows
(0x00521A18, 150 x 0x30) with `sub_00079D60(text, row, flags, 0, 0x002C1C90, 0)`
and enables each (`sub_00071C70(sub_00071630(), 0)`), then closes the list
with `sub_000781A0` (call 0x002C1FA0). Item ids are row indices; the pick
callback `sub_002C1C90` stores the row's id/variant into the 10-slot work copy
0x0075FD90 (+0 id, +0x14 row, +0x18 variant).

| WRAP | what it does (only scene 102, switch on) |
|---|---|
| `sub_000781A0` -> `pc_mapmaker_realbosses_picker_end` | for the call returning to 0x002C1FA5: adds row id 150 ("Giant Trophy Monster", flags 0x20900 as the stock rows in 0x66, enabled), then the original |
| `sub_002C1C90` -> `pc_mapmaker_realbosses_pick` | after the original: any slot holding row 150 gets id 0x15, variant -1 (the original read past the table) |
| `sub_002C1E10` -> `pc_mapmaker_realbosses_highlight` | after the original: preview row [0x0078AEA8] = 150 becomes the Arcade trophy monster row (`sub_002C2B40` reads that row's id) |
| `sub_002BE600` -> `pc_mapmaker_realbosses_row_of` | id 0x15 only: row 150 for the slot reload (`sub_002C0490`, ret 0x002C054C) and the Story AI edit screen name (`sub_002BE650` ret 0x002BE65F called from `sub_00303350` ret 0x003033C0); the trophy monster row for the slot preview (`sub_002C1570`, ret 0x002C1805/0x002C1814); every other caller (e.g. `sub_000DAC30` in play) stock -1 |
| `sub_002BE300` -> `pc_mapmaker_realbosses_row_text` | row 150 -> synthetic text id 0x52420015 (stock returns 0 for rows >= 150, 0x002BE30C) |
| `sub_0003D7B0` -> `pc_mapmaker_realbosses_text` | text 0x52420015 -> the label (a Latin-1 string from `xbox_HeapAlloc`); also the boss bar label, below. Runs on every text lookup: one compare, then the original |
| `sub_000A5E40` -> `pc_mapmaker_realbosses_model_request` | log only: a request for model 0x5BE/0x5BF in the editor (none expected; the stock call skips the flagged model there) |

`sub_002BE270(0x15)` is 0 (byte table 0x002BE29C), so the stock slot reload
drops this id's variant; the pick stores -1 to match.

## Stage 2: the monster behaviour (play, scene 100, story mode)

| WRAP | what it does |
|---|---|
| `sub_001C8540` -> `pc_mapmaker_realbosses_anims` | after the original (`sub_0005C420` calls it at 0x0005C501, then registers every mounted anim pak's names with `sub_001CA2F0` at 0x0005C558): when the map's custom set (map-info chunk 0x00C77668: +0x14 >= 0xC, members +0x18) holds id 0x15, or the container cannot be read, and lv1_3.pak is not mounted (`sub_00062260`), mount it with `sub_0005A130` (= `sub_000622D0(path,1,1)`, the stock per-level anim pak call) |
| `sub_0015B830` -> `pc_mapmaker_realbosses_ai_start` | before the original (which copies spawn record +0/+4 to AI +0x10/+0x14 in story mode, 0x0015B9A3): pool entry a with character a+0x48 == 0x15 (or entity [a]+0x94 == 0x5BE), and lv1_3.pak mounted: record +0 = 0, +4 = 0x80 |
| `sub_0014CC80` -> `pc_mapmaker_realbosses_hud` | chain INSIDE `pc_mapmaker_bosses_hud`: see the boss bar |

Pak mount: 141 KB read whole into the level heap, one slot of the 30-slot
table ([0x006D7880], cap 0x1E at 0x000620F9); refused (logged, bot AI) when
fewer than two slots are free. Unmounted with every other pak by the stock
`sub_00062720` (from the scene loop `sub_0003D440`), like the per-level anim
paks it imitates. The anim ids (0x127..0x193) do not collide with the global
anim paks registered in scene 100 (research). When `sub_0005C420` skips the
mount step because anims are already loaded ([0x006D781C] = 1), nothing is
mounted and the AI keeps the bot behaviour ("lv1_3.pak not mounted" log).

Level events: `sub_00280FA0` and its damage callback `sub_000BC4D0` cannot
reach `sub_002274C0` (static call graph of the decode, checked by the test;
the train jetpack `sub_001EEBD0` is the positive control). So the monster
cannot fire the map's logic markers, and no event guard is installed. Later
bosses (mansion boss, tank, jetpack) need one (research section 1.5).

## Boss bar and the Boss flag

`sub_00280FA0` sets the story focus [[0x007B0C7C]+0xFF4] (0x002811BE);
`sub_0014CC80` draws its bar for model 0x5BE with label text 0xDCC (call
0x0014CED0, returning to 0x0014CED5). 0xDCC is a level-local text id
(>= 0xD87), so in scene 100 the text hook returns "Giant Trophy Monster" for
that one call instead.

- No Boss flag: one bar, the stock one (the creature's own health).
- Boss flag on (Story AI chunk +0x29 = 0xB0 | level, area `bosses`): the
  realbosses HUD hook clears the focus for the stock call only (restored
  after), so the stock bar is not drawn and `pc_mapmaker_bosses_hud` draws the
  AI's Boss bar (its name, stacking, health x2..x8 and the health levels).
  One bar per actor. The Boss health multiplier applies in both cases (it
  scales entity +0x1CC/+0x1D8 in the bosses hook).
- Boss flag with **Health display: Sub-boss bar** (+0x29 bit 2): the focus
  is cleared as for the Boss bar, the bosses area draws no bottom bar for it,
  and the overhead meter below is kept: the meter instead of any bar.
- Several deerbeasts: the stock focus holds one at a time; give the others the
  Boss flag for their own bars.
- With the `bosses` area switched off the stock bar always stays.

## The floating health meter (fixed after v0.4.0-mapmaker.5)

User report: the creature worked, but a health meter floated above its back,
unlike the boss bars. It is a stock MapMaker story feature, not a boss bar
(CONFIRMED by decode, `test_overhead_meter_sites`):

- `sub_0014D1F0` (HUD, called from `sub_001CEBC0` at 0x001CEF19) runs its
  actor loop only when the scene is 100 (0x0014D21C) and the mode byte
  [0x007DE455] is story (0x0014D225). For every AI-list actor
  (0x007A3580, filled with entity +0x7C by `sub_0015BCA0`) from index
  [0x00790950] to [0x0074C37C] whose entity has a damage-stage record at
  +0x268 and is not the player's own entity ([[0x007B0C7C]+0x14]) it calls
  `sub_0013A400(actor, 0, 0x7F, 0x6E)` at 0x0014D25F (its only caller,
  cdecl). That function only draws: a camera-facing quad meter coloured by
  entity +0x1CC through `sub_0001AC50`, plain `ret`.
- The record comes from `sub_0012C800` (17-slot pool 0x00745938). Players
  (`sub_000D2FD0`), story scripts and the boss handlers create one; MapMaker's
  own AI setup (`sub_00240C50`, `sub_0015F8C0`/`sub_0015B830`) never does, so
  ordinary Story AIs (with or without the Boss flag) show no such meter. The
  deerbeast handler creates one in its intro state (0x002811A3, stored at
  0x002811AA), so in MapMaker the creature qualified. The meter sits at a
  human height above the origin, i.e. on its back.
- Mission 5 is scene 1, so the stock loop never runs there.

| WRAP | what it does |
|---|---|
| `sub_0013A400` -> `pc_mapmaker_realbosses_meter` | return 0x0014D264 only: for an actor whose record ai_start noted (the real boss) and whose character is 0x15 or entity model 0x5BE, in scene 100 story, skip the draw (`MAPMAKER_GUEST_RET`, logged once) unless its Story AI chose the Sub-boss bar; everything else the original |

The +0x268 record stays: `sub_0014CC80` and the damage flash `sub_0014D3A0`
read it. With `TSFP_MAPMAKER_REAL_BOSS=0` (bot class) the creature never gets
a record (INFERRED from the caller list), so there is no meter to hide.
Narrowest gate by design: other actors that legitimately carry a record keep
their meter. Per real boss (ai_start notes 0 / Boss bar / Sub-boss bar from
chunk +0x29): no Boss flag = stock bottom bar, meter hidden; Boss bar = the
bosses area's bar, meter hidden; Sub-boss bar = meter shown, no bottom bar.
The meter's height comes from the model's own skeleton node, not a fixed
human offset ([bosses: Health display](mapmaker-bosses.md#health-display)).

## Save format and stock games

Unchanged: a stock custom-set member (word id 0x15, sbyte variant -1). No
Story AI chunk byte (+0x21 is the health levels, +0x29 the Boss flag). A
stock game or older fork build: the slot screen and the Story AI edit screen
show an empty name (`sub_002BE600` -> -1, `sub_002BE300(-1)` -> 0 -> the empty
string 0x004766D8), the preview stays on the previous row, and play spawns id
0x15 with the MapMaker bot class (`sub_000D6D00` covers 0x16..0xA3 only) on
the human rig, i.e. stage 1. Mission 5's own set holds the same id. The
characters area's preload logger reports the member as "not an Arcade
character" (log only). In play its name text (from `sub_002BE650` in
`sub_000DAC30`) stays empty, as in the stock mission.

## Integrator notes

```
WRAP sub_000781A0 -> pc_mapmaker_realbosses_picker_end     (new; menu list end, return-address test first)
WRAP sub_002C1C90 -> pc_mapmaker_realbosses_pick           (new; picker row callback, called through the descriptor)
WRAP sub_002C1E10 -> pc_mapmaker_realbosses_highlight      (new; picker highlight callback)
WRAP sub_002BE600 -> pc_mapmaker_realbosses_row_of         (new)
WRAP sub_002BE300 -> pc_mapmaker_realbosses_row_text       (new)
WRAP sub_0003D7B0 -> pc_mapmaker_realbosses_text           (new; hot: every text lookup)
WRAP sub_000A5E40 -> pc_mapmaker_realbosses_model_request  (new; log only)
WRAP sub_001C8540 -> pc_mapmaker_realbosses_anims          (new; level load)
WRAP sub_0015B830 -> pc_mapmaker_realbosses_ai_start       (new; AI start)
WRAP sub_0014CC80 -> pc_mapmaker_realbosses_hud            (already wrapped by bosses: chain INSIDE pc_mapmaker_bosses_hud)
WRAP sub_0013A400 -> pc_mapmaker_realbosses_meter          (new; overhead meter, per frame per recorded actor)
```

Include the `.inc` after `pc_mapmaker_bosses.inc` / `pc_mapmaker_characters.inc`
(it uses `pc_mapmaker_area_enabled`, declared in the file and defined later in
`pc_menu_mouse_guest.inc`). Link stubs for `sub_0005A130`, `sub_00062260`,
`sub_00071630` and `sub_00071C70` are in `tests/mapmaker/mapmaker_area_stubs.h`.
No extension memory.

## User test steps

1. `TSFP_MAPMAKER_LIMITS=1`. Editor -> Map Settings -> characters: custom set
   -> pick a slot: the last row is **Giant Trophy Monster**. Pick it; the slot
   shows that name; leave and reopen the set: still there.
2. Place a Story AI using that slot (its character row shows the name).
   Save, reload: kept. No crash while highlighting the row or the slot
   (the preview shows the Arcade trophy monster).
3. Play the map in Story: stderr shows `mounted lv1_3.pak ...` and `giant
   trophy monster gets the story monster AI`. It is the large mission 5
   model, roars, charges and head-butts, has its own bar labelled "Giant
   Trophy Monster", and dies with its death animation. Logic objectives in
   the map still behave.
   No health meter floats above its back (stderr: `overhead health meter
   hidden on the giant trophy monster`, once).
4. Set its **Boss** row to x4: a single bar with the AI's name and four times
   the health. Two deerbeasts with Boss: two bars. Reopen it and set
   **Health display: Sub-boss bar**: in play no bottom bar, the meter on its
   back is shown and drains as it is hurt.
5. `TSFP_MAPMAKER_REAL_BOSS=0`: the same model walks and shoots like a bot
   (human animations). Without `TSFP_MAPMAKER_LIMITS` the row is absent; the
   saved map still plays (bot behaviour).
6. Unknown and worth watching: movement on MapMaker pads, collision size,
   whether the creature expects the mansion script, the text of bar/name in
   other languages (the label is English only).
