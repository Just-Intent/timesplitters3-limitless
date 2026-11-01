# MapMaker real story bosses: the giant trophy monster and the giant skeleton

Status: implemented in `src/interface/pc_mapmaker_realbosses.inc` (area
`realbosses`), fixture-tested (`tests/mapmaker/test_mapmaker_realbosses.py`:
95 mutations, plus decode and US XBE data checks of every site below). The
creature was played in v0.4.0-mapmaker.5 (user: working); the floating-meter
fix, the health display choice, "Boss Off = no health" and the giant
skeleton ([below](#the-giant-skeleton-mansion-boss)) are not built or played yet. No guest-code table entry. Switch: `TSFP_MAPMAKER_LIMITS=1`;
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
| `sub_001C8540` -> `pc_mapmaker_realbosses_anims` | after the original (`sub_0005C420` calls it at 0x0005C501, then registers every mounted anim pak's names with `sub_001CA2F0` at 0x0005C558): when the map's custom set (map-info chunk 0x00C77668: +0x14 == 0x11, members +0x18 = slots 1-10; slots 11-20 in the sets chunk 0x00C776F5, [below](#which-bosses-the-set-holds-fixed-after-v040-mapmaker12)) holds id 0x15, or the container cannot be read, and lv1_3.pak is not mounted (`sub_00062260`), mount it with `sub_0005A130` (= `sub_000622D0(path,1,1)`, the stock per-level anim pak call) |
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

- No Boss flag (Boss Off): no health shown at all (user, 2026-10-31: "The
  giant trophy monster also now has boss health visible even if boss is set
  to off for its AI. This shouldn't be the case."). The hud hook clears the
  focus for the stock call for every noted real boss, so its own bar is
  never drawn, and the meter stays hidden.
- Boss flag on (Story AI chunk +0x29 = 0xB0 | level, area `bosses`): the
  realbosses HUD hook clears the focus for the stock call only (restored
  after), so the stock bar is not drawn and `pc_mapmaker_bosses_hud` draws the
  AI's Boss bar (its name, stacking, health x2..x8 and the health levels).
  One bar per actor. The Boss health multiplier applies in both cases (it
  scales entity +0x1CC/+0x1D8 in the bosses hook).
- Boss flag with **Health display: Sub-boss bar** (+0x29 bit 2): the focus
  is cleared as for the Boss bar, the bosses area draws no bottom bar for it,
  and the overhead meter below is kept: the meter instead of any bar.
- Several deerbeasts: give each the Boss flag for its own bar.
- With the `bosses` area switched off there is no Boss row: as Boss Off, no
  health is shown.

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
chunk +0x29): no Boss flag = no bar, meter hidden; Boss bar = the
bosses area's bar, meter hidden; Sub-boss bar = meter shown, no bottom bar.
The meter's height comes from the model's own skeleton node, not a fixed
human offset ([bosses: Health display](mapmaker-bosses.md#health-display)).

## The giant skeleton (mansion boss)

User request (2026-10-31): the giant skeleton of mission 5 that rises out of
the ground, as a Story AI. CONFIRMED (decode and US XBE data,
`test_skeleton_*`):

| | value |
|---|---|
| character id | 0x28 (model 0x5E9 `CHR_MANSION_MANSIONBOSS`, no LOD; no Arcade row) |
| picker | synthetic row 151 "Giant Skeleton", text 0x52420028; preview stand-in the Arcade skeleton cowboy (id 0xA6, row 144) |
| behaviour | class 0x20000: `sub_0027D590` 0x0027DD54 -> `sub_0029FE00(entity)` (call 0x0027DDD3), which tail-jumps to the mansion variant `sub_0029CEF0` in scene 1 and to the catacombs variant `sub_0029E0A0` elsewhere |
| animations | `pak/stream/lv1/anim/lv1_6.pak` (0x0049D6FC, registered at 0x001CAE61, table 0x0048C738, 18 names `princess_start` 0x19C .. `princess_eyeoverwrite` 0x1A9; no id shared with lv1_3) |
| per-level state | `sub_00287E00`: `[0x0078B1EC]` = level heap 0x4C7C, `sub_002A62C0`: `[0x0078B0D0]` = 0x7D00 (800 x 0x28 particles), then `sub_00287860` initialises the fields; stock only from `sub_0025F720` in scenes 1 and 17 (`sub_0015E7C0` 0x0015E818, story mode only) |

### Which bosses the set holds (fixed after v0.4.0-mapmaker.12)

User log (mapmaker.12): the editor logged `slot set to the giant trophy
monster (0x0, 0x15)`, `... giant skeleton (0x0, 0x28)`, `... trophy monster
(0x1, 0x15)`; in play only `mounted lv1_3.pak`, no lv1_6 mount and no
`giant skeleton state allocated`, then `lv1_6.pak not mounted: giant skeleton
with bot behaviour`. The characters area's preload log named 0x15 but not
0x28, so 0x28 was not among the map-info members.

Cause (CONFIRMED by the code and the fixture): the slot number in the pick
log is the work-copy row (0..9); on the slot screen's page 2 those rows are
slots 11-20 (`pc_mapmaker_sets.inc`). Slots 11-20 are saved in the sets
area's own chunk 0x00C776F5, which the play copy (`sub_0033EBF0` hook) and
the preload use, but the realbosses set check read only the map-info chunk
(slots 1-10). A boss in slots 11-20 spawned with the bot class and without
its pak or state; the other boss in slots 1-10 worked.

Fix: `rbs_set_scan` reads both chunks (the sets chunk with the header checks
of `sets_take`: word 1, first 10, count 1..10, members word id at +4 + 4k)
and is shared by the anim pak mount and the level init, so both bosses get
their pak, the skeleton its state, whatever their slots (1-20) or order. It
also requires set index 0x11, the one stock play takes the map's members for
(`sub_00240C50` 0x00240DAD..0x00240DBE, preload `sub_0023F390`
0x0023F462..0x0023F46D; other indices >= 0xC are profile sets); the old test
`>= 0xC` read stale members for those. Both hooks log what they found:
`anim pak set check found: giant trophy monster slot 2, giant skeleton slot
11` (or `none 0`, `not the map's own set`, `map container unreadable`). The
fixture plays the user's set (skeleton slot 1, trophy slot 2), reversed,
slots 9/10, 11/12, 14/20, mixed 1/11 and 1/12, single bosses, a profile
index and bad sets-chunk headers (`test_custom_set_sites` checks the stock
sites).

No arena coordinates: the fight reads its own entity position, bone matrices
and the players; the particle shape `sub_002A62F0(3, 1.5, 0, ..)` is local to
a bone. The emergence is the animation `princess_start` (state 1 waits for
it), so the skeleton rises at its Story AI's spawn point. INFERRED: the
animation starts below the floor, as in the courtyard; on a MapMaker tile it
should rise through the floor (unplayed).

| WRAP | what it does (scene 100 story, switch on) |
|---|---|
| `sub_001C8540` (anims) | also mounts lv1_6.pak (>= 2 of 30 slots free) when the custom set (slots 1-20, index 0x11) holds 0x28; both paks when both bosses are in the set |
| `sub_0025F720` -> `pc_mapmaker_realbosses_level` | after the original: forget the last level; when the set holds 0x28 (or the container is unreadable), clear the two pointers and call the stock `sub_00287E00` (52.6 KB, freed with the level heap like the stock blocks); a failed allocation is logged and the skeleton keeps the bot AI |
| `sub_0015B830` (ai_start) | char 0x28 / model 0x5E9: class 0/0x20000 when lv1_6 is mounted and this level's state is in place; only the first spawn record per level (extras logged, keep the bot AI and look wrong: human animations on a non-human rig) |
| `sub_0029FE00` -> `pc_mapmaker_realbosses_princess` | that boss in scene 100: call `sub_0029CEF0` directly (it pops the caller's return address, exactly the lifted tail jump) |
| `sub_0028D0A0` -> `pc_mapmaker_realbosses_message` | its message handler: stock `sub_00286D30` (mansion) only in scene 1, else `sub_0028C560` (catacombs), which answers -1 to messages 1/2 and does nothing; for the boss in scene 100: the mansion handler (direct call = tail jump). See [the death](#the-death-fixed-after-v040-mapmaker13) |
| `sub_002875F0` -> `pc_mapmaker_realbosses_risen` | state 1 "entrance done": stock is `sub_00059DB0(entity, 0x19C)` only in scene 1 (0x23B in 17, else 0: stuck rising); scene 1's answer for the boss |
| `sub_00287E20` -> `pc_mapmaker_realbosses_throw` | the feeble throw (action type 6, 0x005208B8[a+0x9DC]; phase +0x4160 0 aim / 1 fire / 2 done) only runs in scene 1, else every frame resets it. Aim: the original with the scene word 1 (its only scene read is its first instruction; its callees read none, decode). Fire: the stock `sub_000EA9C0(0x34, s, s+0xC, 0.3, entity, a, 2, 1, entity+0x34, 0xE, 0xC, 0.1)` (s = state+0x413C), `sub_0010EB90([s+0x18], 0)`, phase 2, in the real scene. Done: return, as scene 1 |
| `sub_002274C0` -> `pc_mapmaker_realbosses_event` | the death's level event 3 (call 0x0029E063, ret 0x0029E068) would fire the map's own logic marker 3: dropped and logged. It is the only level event the fight and its callbacks (`sub_000BC470`, `sub_00286430`, `sub_00289690`, `sub_00288BD0`) can reach |
| `sub_0014CC80` (hud) | the HUD case 0x5E9 draws a bar only in scene 1 (0x0014CF05; else a catacombs meter when entity +0x1A0 == 1): the focus is cleared for the stock call as for the deerbeast |
| `sub_0013A400` (meter) | the fight makes a +0x268 record (0x0029D101): hidden unless Sub-boss bar |

### The death (fixed after v0.4.0-mapmaker.13)

User report (mapmaker.13): the skeleton with the Boss bar lost all its
health but did not die; the log had the mount, state, class and throw lines
but never `level event dropped` (its death path). Cause (CONFIRMED, decode,
`test_skeleton_death_message_sites`):

- Its health is entity +0x1CC, the same value as the bar: the damage
  callback `sub_00289690` (registered when it has risen, 0x0029D4BF)
  subtracts the scaled hit and clamps at 0. The fight itself never tests it.
- At 0 the stock damage code (`sub_000BABA0` 0x000BB059) calls the AI death
  `sub_0015DB20`, which first sends message 2 through `sub_0015C310` ->
  class handler `sub_0027CEF0` -> `sub_0028D0A0(entity, 2, ..)` (0x0027D3A7);
  an answer of -1 skips the generic death (0x0015DB43).
- `sub_0028D0A0` sends scene 1 to the mansion handler `sub_00286D30`:
  message 2 sets AI +0x9D0 = 6 and plays the death animation 0x1A6
  (0x00286D64..0x00286D80), and returns -1; state 6 (0x0029E021) waits for
  the animation, clears the story focus, fires level event 3 and removes the
  AI with `sub_0015DDA0` (0x0029E069: message 5, alive counters, groups).
  Every other scene goes to the catacombs handler `sub_0028C560`, which
  returns -1 for messages 1 and 2 doing nothing (0x0028C615). So in scene
  100 the death was swallowed: no state 6, no death, no removal.

Fix: WRAP `sub_0028D0A0` -> `pc_mapmaker_realbosses_message`: for the boss
(as `_princess`: this level's record and state, scene 100) call the mansion
handler directly, for every message, as in scene 1 (message 1, the hit while
it kneels in state 5, takes 10 health with its effect; message 5 does
nothing). The death then plays, event 3 is still dropped by `_event` (map
logic untouched), and the AI is removed the stock way. Logged once:
`giant skeleton killed: the mansion death`. The Boss multiplier scales the
same +0x1CC, so the bar and the death agree. INFERRED: MapMaker logic that
counts this AI as killed sees the stock removal; the generic kill
statistics (`sub_00238E00`, after the -1 test) are skipped, as in mission 5.
The giant trophy monster is not affected: its handler `sub_00281620` has no
scene test, acts on message 1 only and returns 0 for 2, so it takes the
generic death (played in mapmaker.5).

Health: the fight sets 40/40 at its start (0x0029D0E5); the Boss row's
multiplier is re-applied by the bosses area when the maximum changes. Boss
Off: no health shown; Boss bar: the bosses bar; Sub-boss bar: the meter.

Not done in scene 100: the dust particles (`sub_0029CE00`, drawn by the frame
`sub_001CEBC0` only in scenes 1 and 17, 0x001CEE31/0x001CEE42). Risks: the
spoofed scene during aiming, the fight's untraced reliance on mansion script
state, the throw target point (state +0x4164, filled by `sub_00159150`),
collision on tiles.

## Whole height on screen

User report (v0.4.0-mapmaker.11): the giant skeleton is visible only up to
about two floors. Area `bigdraw`, `src/interface/pc_mapmaker_bigdraw.inc`,
`tests/mapmaker/test_mapmaker_bigdraw.py` (fixture, 20 mutations, decode
checks). Not built or played.

Cause (CONFIRMED, decode): every prop is drawn with a screen scissor that is
the union of the portal rectangles of the rooms its box touches.

- `sub_0009FBE0` lists up to 10 rooms (+0x30 count, +0x38 ids) overlapping
  the box +0x1A8 +- +0x1C0 (`sub_000873C0` at 0x0009FCF0); a character's box
  is a cube of its model radius (`sub_0009EAB0` -> `sub_0008E5F0`).
- The portal walk `sub_00087F80` gives the camera's room the whole viewport
  `[0x007E71C8]` (call 0x00089BD9) and every other room the rectangle of the
  portals it is seen through (view +0x3BC + 12*room).
- `sub_000862C0(prop)` (only caller 0x000A246B in `sub_000A2430`, ret
  0x000A2470, per prop of a room about to be drawn) stores the union of the
  visible listed rooms' rectangles at +0x118 x0, +0x11C y0, +0x120 x1, +0x124
  y1. `sub_000A2430` culls the prop when its projected box misses it;
  `sub_000A2740` makes it the scissor (`sub_0001A010`, 0x000A285B) before the
  model draw (0x000A28D4).

A MapMaker tile is one room one floor high, with portals (doorways, open
sides) one floor high, and an open top has no room above it. A boss in
another tile than the camera is therefore cut at the projected top of the
portal it is seen through: roughly two floors at its distance. Mission 5
fights it inside the courtyard room the player stands in, whose rectangle is
the whole viewport, so stock never clips it. Not the cause: frustum or fog
culling (the box test uses the same rectangle), node masks (tile rooms only),
bones.

| WRAP | what it does |
|---|---|
| `sub_000862C0` -> `pc_mapmaker_bigdraw_rect` | after the original, for a character prop (+0x20 == 8) with model 0x5E9 or 0x5BE, return 0x000A2470, scene 100, `TSFP_MAPMAKER_LIMITS=1`: when the stock rectangle is not empty, replace it with the whole viewport (what the camera's own room gets); logged once. An empty rectangle (no listed room visible) stays, so the stock cull applies (logged once) |

Depth testing still hides the boss behind every drawn wall: from inside a
closed tile only its doorways show it, and a closed tile's ceiling still
covers it. Where stock shows only sky (over an open top) the boss is now
drawn. INFERRED risk: a part inside a tile that is not drawn at all (no
portal path to it) shows where stock showed sky; the box is still the
model's radius, so a boss whose drawn height exceeds it can be culled when
only its head is on screen. The giant trophy monster gets the same fix (it
is clipped the same way once taller than the portal it is seen through).
Other characters, objects, scenes and story levels: stock.

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
WRAP sub_0025F720 -> pc_mapmaker_realbosses_level          (new; story level init, after the original)
WRAP sub_0029FE00 -> pc_mapmaker_realbosses_princess       (new; class 0x20000 dispatch, per AI frame)
WRAP sub_0028D0A0 -> pc_mapmaker_realbosses_message        (new; class 0x20000 messages, incl. its death)
WRAP sub_002875F0 -> pc_mapmaker_realbosses_risen          (new; per skeleton frame in state 1)
WRAP sub_00287E20 -> pc_mapmaker_realbosses_throw          (new; per skeleton frame)
WRAP sub_002274C0 -> pc_mapmaker_realbosses_event          (new; level events, return-address test first)
WRAP sub_000862C0 -> pc_mapmaker_bigdraw_rect             (new, area bigdraw; prop draw rectangle, per frame per prop of a drawn room)
```

Include the `.inc` after `pc_mapmaker_bosses.inc` / `pc_mapmaker_characters.inc`
(it uses `pc_mapmaker_area_enabled`, declared in the file and defined later in
`pc_menu_mouse_guest.inc`). Link stubs for `sub_0005A130`, `sub_00062260`,
`sub_00071630`, `sub_00071C70`, `sub_00287E00`, `sub_0029CEF0`, `sub_00286D30`, `sub_00059DB0`,
`sub_000EA9C0` and `sub_0010EB90` are in `tests/mapmaker/mapmaker_area_stubs.h`;
the off-build no-op hooks are in `pc_menu_mouse_guest.inc`, prototypes in `pc_display_menu.h`.
`pc_mapmaker_bigdraw.inc` (area `bigdraw`) is included right after it (needs
only `pc_mapmaker_limits.inc`; its stub and prototype are in place, no link
stubs). No extension memory.

## User test steps

1. `TSFP_MAPMAKER_LIMITS=1`. Editor -> Map Settings -> characters: custom set
   -> pick a slot: the last row is **Giant Trophy Monster**. Pick it; the slot
   shows that name; leave and reopen the set: still there.
2. Place a Story AI using that slot (its character row shows the name).
   Save, reload: kept. No crash while highlighting the row or the slot
   (the preview shows the Arcade trophy monster).
3. Play the map in Story: stderr shows `mounted lv1_3.pak ...` and `giant
   trophy monster gets the story monster AI`. It is the large mission 5
   model, roars, charges and head-butts and dies with its death
   animation. With Boss Off no health bar or meter is shown. Logic objectives in
   the map still behave.
   No health meter floats above its back (stderr: `overhead health meter
   hidden on a real story boss`, once).
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
7. Giant skeleton: the picker's row after it is **Giant Skeleton** (preview:
   the skeleton cowboy). One Story AI with it, play in Story: stderr shows
   `mounted lv1_6.pak`, `giant skeleton state allocated`, `giant skeleton
   gets the mansion boss AI`. It rises out of the floor at its spawn point
   (or note if it floats/clips), then stamps, swipes, bites and throws
   (stderr `feeble throw fired` once); dies with its animation; stderr
   `level event dropped`, and map logic markers (e.g. objective 3) do not
   change. Boss Off: no health; Boss x4: one bar with the name; Sub-boss
   bar: the meter. A second Giant Skeleton AI: `only one giant skeleton per
   map` and it fights as a bot. Mission 5 and 6 still play stock.
   Both bosses in one set: put the Giant Skeleton and the Giant Trophy
   Monster in any two slots (also on the slot screen's page 11-20, and in
   either order), one Story AI each, play in Story: stderr shows `set check
   found: giant trophy monster slot N, giant skeleton slot M` with the slots
   you used, `mounted lv1_6.pak`, `mounted lv1_3.pak`, `giant skeleton state
   allocated` and both "gets the ... AI" lines.
   Death: give the skeleton the Boss bar (any multiplier) and empty it: it
   plays its death animation and disappears; stderr `giant skeleton killed:
   the mansion death`, then `level event dropped`; a logic trigger on that
   AI being killed fires, map logic marker 3 does not change. The trophy
   monster still dies as before.
8. Whole height: put the Giant Skeleton in a tile next to an open area and
   watch it rise from a neighbouring tile and from further away: the whole
   body shows above the tile walls (stderr `real boss drawn over the whole
   view`, once). From inside a closed tile it shows only through doorways,
   not through walls or the ceiling. Same for the Giant Trophy Monster.
   Without `TSFP_MAPMAKER_LIMITS=1` it is clipped as before.
