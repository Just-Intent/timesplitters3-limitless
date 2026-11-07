# MapMaker real story bosses: the giant trophy monster, the giant skeleton and the Crow walker

Status: implemented in `src/interface/pc_mapmaker_realbosses.inc` (area
`realbosses`), fixture-tested (`tests/mapmaker/test_mapmaker_realbosses.py`:
about 150 mutations, plus decode and US XBE data checks of every site below). The
creature was played in v0.4.0-mapmaker.5 (user: working), the giant skeleton
in v0.4.0-mapmaker.13 (user: working; its death fix is in .15); the Crow
walker ([below](#the-crow-walker-story-mission-13)) is not played yet. No guest-code table entry. Switch: `TSFP_MAPMAKER_LIMITS=1`;
`TSFP_MAPMAKER_REAL_BOSS=0` keeps the creature on the normal bot AI (stage 1).

## Restart from the pause menu (fixed after v0.4.0-mapmaker.31)

**Proven in the decode** (`test_restart_resets_only_the_mission_bosses`):
"START restart" runs `sub_001CDAF0` + `sub_001CDD60`, never the level init
`sub_0025F720` (this area's level hook). Its soft restart `sub_0025F6A0`
(`sub_0015E4F0` from 0x001CDD74) resets the skeleton's state with
`sub_00287860` only in scenes 1/0x11 and the walker's with `sub_00297690`
only in 0x51 (the stock inits end in the same calls). In scene 100 the
restarted skeletons and walkers found their instances by spawn record (late
ones too) and fought on with the last run's state and flags: a walker armed
or roaring before never deployed its guns or roared again; a skeleton kept
its floor and fight state. The trophy monster has no state block (class
only, written again at each AI start): not affected.

Fix: new `WRAP sub_001CDD60 -> pc_mapmaker_realbosses_restart`. After the
original, in scene 100, each skeleton and walker instance gets that stock
reset with its own state and statics swapped in (`rbs_skel_in`/`rbs_walk_in`,
the shared `pc_mapmaker_bossfx.inc` holder), and its counters and
once-per-fight flags are cleared (walker `armed`/`roars`/`woken`..., actor
until its AI start; skeleton floor, throws). Blocks, records and statics
copies stay (level memory); the log budget starts afresh. The shared
holder itself needs nothing: it holds no instance between calls. Fixture
case (three skeletons, two walkers, one late each), `restart-*` mutations.
Playtest: restart a map with skeletons and walkers after a fight: each rises
and fights again, walkers deploy their guns again.

**After v0.4.0-mapmaker.40: the restarted bosses fight on the last run's blocks.**
User report (.40): four giant skeletons "not spawning correctly and kind of
flickering in and out" after a story restart; `m40b.err` shows the restart
line `(0x4, 0x0)`, three floor/fire lines and then no throw for about 16000
frames (the run before threw within seconds). CONFIRMED (decode and US XBE
jump table 0x0003E810): the restart's first call `sub_0003E740(3)`
(0x001CDD62) reaches 0x0003E7C7 -> `sub_0003E240`, which sets heap 0's top
`[0x006B8388]` back to its base `[0x006B8380]`. The .40 fix read that as
freeing every `sub_0003DFA0(n, 1)` block and gave every instance new blocks
per restart.

**Corrected 2026-11-06 (CONFIRMED, `test_mapmaker_restart_heap.py`):** heap 0
(top-down `[0x006B8388]`, `sub_0003DFA0` jump table 0x0003E094 entry 0) is
not the Level heap. `sub_0003DFA0(n, 1)` is heap 1, bottom-up
`[0x006B837C]`, written only by `sub_0003DF00` (game start), `sub_0003E650`
(full level load, the branch without `[0x006B7AA8]`), `sub_0003DF70` (only
with `[0x006B7AA8]` 0) and the allocators: none on the restart branch. The
skeleton and walker blocks were never freed; new ones per restart only used
up the Level heap (about 52 KB a skeleton, 190 KB a walker, never returned).
What the .40 symptom needed was the blocks' contents: the .31 restart ran
only the stock resets on the last fight's state and particles.

Fix (`pc_mapmaker_realbosses_restart`, after the original, scene 100): every
instance, late ones included (at most 4 each), keeps its spawn record and
blocks, is cleared otherwise, and sets its blocks up again as at level init
(`rbs_skel_again` / `rbs_walk_again`): the level's statics `rbs_fx_base` with
the instance's block pointers and effect count, every block zeroed as a new
Level block is (state; particles 0x7D00; the walker's two effect blocks,
0x55F0 or 0xF0A0 by the count 0x1F4/0x578: `pc_mapmaker_bossfx.inc`), then
the stock init's non-allocating rest: `sub_00287860` (skeleton);
`sub_002BD480(8, 8, 1.0, state + 0x5C4)` and `sub_00297690` (walker). A holder
left on one of our copies is dropped first (`mapmaker_fx_forget`); the boss
pointers go to the first instance's blocks; an instance whose blocks are not
usable and those after it are dropped (bot behaviour, or a late instance at
its AI start). Late allocation (`rbs_skel_more` / `rbs_walk_more`) still
allocates (`rbs_skel_alloc` / `rbs_walk_alloc`). Log: `level restart: giant
skeleton and Crow walker blocks set up again in place`. Fixture: four
skeletons (the fourth late) and two walkers restarted with their blocks
poisoned: the same addresses, zeroed (a word past a walker effect block
kept), no allocation, the stock rests seen with each instance's blocks,
records found again without a fifth, the fourth fighting on its blocks, a
second restart, an unusable block and effect count, mission 5 and the switch
off; mutations `restart-*`, `again-*`, `fx-effect-size-swapped`,
`alloc-live-statics`. Unplayed.

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

### Height and the throw (fixed after v0.4.0-mapmaker.16)

User report (mapmaker.16): the skeleton hovered one floor above its spawn,
part of its body stretched across the map, and the game froze right after its
feeble throw fired (log: last line `giant skeleton feeble throw fired`, then
no guest frame; both real bosses in slots 11 and 12). CONFIRMED (decode,
`test_skeleton_floor_site`): every frame `sub_0029CEF0` sets the entity's
height +0x68 to 0, the mission 5 courtyard floor (`xorps xmm6` 0x0029D067,
store 0x0029D06B), right before its facing step `sub_002895A0` (call
0x0029D070, return 0x0029D075; the catacombs fight calls it at 0x0029E523).
A MapMaker floor is at another height, so the boss stood at height 0.

| WRAP | what it does |
|---|---|
| `sub_0029FE00` princess | on the boss's first fight frame, notes its spawn height (+0x68), logged once per level (`giant skeleton floor height`) |
| `sub_002895A0` -> `pc_mapmaker_realbosses_face` | return 0x0029D075 only, the boss in scene 100: height = the noted spawn height before the original |
| `sub_00287E20` throw (fire) | a throw whose origin or direction (state +0x413C, six floats) is not finite or lies beyond +-100000 is skipped (phase 2, logged); every throw (first 8 per level) logs its origin, direction and the boss height |

### Throwing at a player a floor up (after v0.4.0-mapmaker.40)

User report (.40): the skeleton could not aim at a player one floor above it.
CONFIRMED (decode): the stock aim (0x00287F02..0x00287FB0, phase 0 once the
throw animation passes 0.45) is the unit vector from the hand (the shot origin
+0x413C) to target point state +0x4164 + i*12 (i = +0x4158), which
`sub_00289290` fills with the player's feet (entity +0x68) plus a horizontal
lead; the action choice `sub_00286F30` and facing `sub_002895A0` use no
height (the picker's height difference `[0x0078B1E0]` is written, never
read). The throw origins in `m40b.err` range from y -10.1 to 5.9 with the boss
at -9.5, so from a low hand the straight line to the feet of a player about 5
units up starts below that floor and meets its edge or underside. INFERRED
(not measured in game): that is the miss; projectile gravity was not traced.

Fix (`rbs_skel_aim`, right after the spoofed aim when the phase became 1):
for a target more than `RBS_SKEL_UP` (2) over the boss's noted floor, the aim
point is raised by `RBS_SKEL_BODY` (1.5, its body), and the upward component
is held at `RBS_SKEL_PITCH` (0.866, about 60 degrees), still a unit vector;
the origin, lead and targets on the boss's floor are the stock ones. Logged
(4 per skeleton per level): `giant skeleton N aims at a target a floor up`.
Fixture: a target 5 up and 10 ahead from a hand below its floor, a steep
overhead target (clamped), the boss's own floor (unchanged), phase kept;
mutations `aim-*`. Unplayed.

### Same-floor throws (the v0.4.0-mapmaker.42 playtest)

User report: "Now they can hit me from one floor above, but they don't hit
me if I stand in place on the same floor." CONFIRMED (log `m42.err`, .41
code): all four noted `floor height -4.49` on their first fight frame with
the player on the 0.5 floor, and every throw logged `aims at a target a
floor up (5.0 over its floor -4.5)`: the noted height is not the placed
floor, so same-floor throws went at the body (1.5 over the feet). The
first-frame height was the map's lowest floor in both logs (-9.49 in
`m40b.err`, floors -2..; -4.49 here, floors -1..). Why it is lower than the
placement is not traced (INFERRED: it sinks between its AI start and the
fight); the body is still held at that noted height (`_face`, unchanged).

Fix: `_ai_start` notes the entity height after the original (`giant
skeleton N placed at height Y (AI start)`). `rbs_skel_ground` (once per
fight at the first aim; reset by the restart): the reference is the finder
floor `sub_000C3C50(point, 1.25, 0.1)` under the boss's x/z, from that
placed height (else the first-frame height; probe point 0.5 over it),
within a step, else
climbing 1.25 at a time up to 11.25 for a floor above it; none: the noted
height. Only a target more than `RBS_SKEL_UP` (now 2.5, half a floor) over
that floor gets the raised aim; the body height (`_face`) is unchanged.
Logged once per fight: `giant skeleton N reference floor F (spawn height
Y, ...)`. Tests: first-frame -9.49 under a 0.5 floor (feet and 2.2 up:
stock; 5 up and 3 ahead: raised, the line passes between its feet and head
from a low hand and through its body from a high one), its own floor under
an upper one, the m42 case (placed 0.51, first frame -4.49 over a -4.5
floor: stock aim at 0.5); mutations `aim-ref-spawn`, `aim-up-2`, `ground-*`,
`pad-not-noted`. Playtest: the skeleton's body may stand on the lower floor
(its noted height); report where it is drawn.

INFERRED: the stretching came from the same mismatch (the body drawn at height
0 while parts follow the spawn floor). The freeze is not explained yet: if it
recurs, wait more than 60 s before closing the game so the hang watchdog
writes a report, and send the throw lines.

### Fire and spawn diagnostics (after v0.4.0-mapmaker.18)

User report (mapmaker.17): the skeleton's fire breath "didn't land, and I
couldn't see its fire, though it did the animation"; the Crow walker did not
spawn. CONFIRMED (decode): the fire is the feeble throw (the only path from
the fight to the damage code `sub_000BABA0` is `sub_00287E20` ->
`sub_000EA9C0` -> `sub_000E0CE0` -> `sub_00160020`); its flames are the particle
block `[0x0078B0D0]`, emitted by `sub_002A6650` (no calls but the math helper
`sub_002A6010`: visual only) and drawn by `sub_0029CE00` from the frame
`sub_001CEBC0` only in scenes 1 and 17 (0x001CEE31/0x001CEE42): scene 1 draws
when state +0x3E88 == 1 with `sub_00019D70(1)` and `sub_002A7F30([[0x007B0CC0]+
0x208], 2.5)`.

| change | what it does |
|---|---|
| `_fx` (`sub_00126560`, ret 0x001CEE8C) | also draws that scene-1 branch for this level's skeleton in scene 100 (logged once: `giant skeleton fire particles drawn`) |
| `_throw` fire log | adds the projectile `sub_000EA9C0` returned (0 = none was made) |
| WRAP `sub_000DAC30` -> `pc_mapmaker_realbosses_create` | log only: each creation of character 0x15/0x28/0x64 in MapMaker play and its object (0 = `sub_000A6F90` failed) |
| `_ai_start` | while the Crow walker's state exists, logs the first 12 AI starts (character, model, record id) |

Walker: the mapmaker.17 log shows lv81.pak mounted and its state allocated,
but no walker AI start at all; model 0x637 `CHR_WALKERCROW` is in the scene 100
table `prop/table/prop_all_xbas.raw` and its model file is in `pak/chr.pak`;
its character table row equals the skeleton's except name text 0. Not
explained yet; the logs above should show whether it is created.

### Walker heartbeat and log budget (after v0.4.0-mapmaker.21)

User report (mapmaker.21): the Crow walker "looks right but isn't responding at all". The log
showed its AI start but nothing after: `rbs_log` had a 24-line budget for the whole session.
The budget is now per level (reset in `_level`), and `_fx` logs a heartbeat every 120 frame
steps, 15 times per level: stage (state +0x98), animation state (entity +0x20), position,
health (state +0x580) and the AI class words (+0x10/+0x14). Log only.

### The walker never woke (fixed after v0.4.0-mapmaker.22)

User report (mapmaker.22): the Crow walker spawns and looks right but never moves or attacks.
The heartbeat showed stage 1 and "anim state 8" unchanged for the whole level.

CONFIRMED (decode, `test_walker_intro_waits_for_level_trigger`):

- "anim state" was the entity's +0x20. The fight's state is the AI's +0x20 (`ebp = [entity+0x7C]`,
  0x002998EF), switched at 0x0029B24F over the 21-entry table 0x0029CB90. The fight applies a
  requested state, AI +0x9D0, first thing every frame (0x002998FA..0x00299922), and the stock state
  changes write it too (0x0029CB19/0x0029CB1C).
- Its first frame (state 0, stage 0) does the setup, sets state 1 and stage 1 (0x0029B597). Stage
  stays 1 for the whole normal fight. Stage 2 is the low-health phase.
- State 1 (0x0029B5A6) is the intro. It waits for its animation to end (`sub_00059DD0`), then for
  mission 13's level trigger `SPC2` (-> `crowewalker180turnright` 0x6FB) or `SPC1` (->
  `crowewalker180turnleft` 0x6FC), both then state 2. The triggers are looked up by `sub_00234190`
  (level logic items `[0x0078CD90]`, count `[0x0075CCA8]`, by tag +0xAC) and tested by
  `sub_00234270` (+0x34). With neither it returns (0x0029B609 -> 0x0029CB83). No other code uses
  these tags.
- The steering, walking and firing above the switch (`sub_00298270`, `sub_00297750`, `sub_00298070`,
  `sub_00296B50`, `sub_00296120`) run only in states above 2 (0x0029A9B2). Those helpers read only
  the walker state. They use no tag, pad or level object.
- State 2 (0x0029B62E) waits for the animation, plays `crowewalkerstunscript` 0x6F7 and then sets
  state 4 (0x0029CAF5), the fight.

A MapMaker map has no `SPC1`/`SPC2` item, so the walker stayed in state 1 forever.

| change | what it does |
|---|---|
| `_fx` wake-up (`rbs_walk_wake`) | at the frame step (ret 0x001CEE8C) in scene 100: when this level's walker (record and entity still match) has finished its first frame (state +0x98 == 1), waits in fight state 1 and has no request pending (AI +0x9D0 < 0), request state 2 (AI +0x9D0 = 2). The fight applies it next frame: roar, then fight. Logged once: `Crow walker woken: no SPC1/SPC2 trigger in a MapMaker map; roar, then fight` |
| heartbeat | now logs the fight state (AI +0x20), target (AI +4), aim error (state +0xE4) and the entity's +0x20 under its own name |

The 180° turns are skipped: they assume the mission arena's layout, and the fight's own steering
(state 4 -> turn states 8/9) turns the walker toward the player. No new wrapper is needed.
INFERRED, needs a playtest: that state 4 onward works in a MapMaker map (the remaining risks
below still apply). The next log should show the fight state leaving 1.

### The roar never started (fixed after v0.4.0-mapmaker.23)

User report (mapmaker.23): "Crow walker still isn't orienting toward me or attacking." The
heartbeat showed the wake-up working: fight state 2, the player as target, the aim following
the player. Then fight state 2 for the rest of the level, and the walker never turned, moved
or fired. (Entity +0x20 = 8 is the entity's type, "character" (0x0005F514), not a state.)

CONFIRMED (decode, `test_walker_roar_needs_an_animation`):

- State 2 (0x0029B62E) first asks `sub_00059DD0(entity +4)` whether the current animation has
  finished, and returns while it hasn't (0x0029B63C). Only then does it play
  `crowewalkerstunscript` 0x6F7 (`sub_0005F460`, 0x0029B664). State 4 follows once 0x6F7 is
  the current animation (`sub_00059D20`, 0x0029B648).
- `sub_00059DD0` reads the animation controller's finished flag: +0x17C when a blend target
  +0x12C is set, else +0x124. A new controller (`sub_00097C00`) starts with no animation
  (+0xD4 = 0), +0x124 = 0 and +0x12C = 0. Only playing an animation changes that
  (`sub_0005BC20`: +0xD4 = id, +0x124 = 0, set when it ends).
- The walker gets its story class before its first frame (`_ai_start`), and its fight plays no
  animation before state 2. So in MapMaker its controller never had an animation, was never
  "finished", and state 2 waited forever. In mission 13 something else animates it first
  (INFERRED: the level script).
- With no current animation, the play call accepts any animation (`sub_001C8C20`:
  0x001C8CA0 -> 0x001C8D09 -> 0x001C8D5D). The walker's animation flags match the split
  Crowe's, whose fight plays in MapMaker.

| change | what it does |
|---|---|
| `_fx` wake-up, roar | in fight state 1 or 2 after the first frame (state +0x98 == 1), once per level, when the controller (entity +4) has never had an animation (+0xD4 and +0x12C both 0): the stock call state 2 makes, `sub_0005F460(entity, 0x6F7, 0, 1.0)`. When the roar ends, state 2 finds it finished and current and enters the fight. Logged once: `Crow walker had never been animated: roar started ...`, or `... roar refused by sub_0005F460 ...` if the call returns 0 |
| heartbeat | a second line per beat: the current animation (+0xD4), blend target (+0x12C), the finished flag `sub_00059DD0` reads, and the state request (AI +0x9D0) |

No new wrapper. The area now calls `sub_0005F460`; its link stub is added to
`tests/mapmaker/mapmaker_area_stubs.h`. INFERRED, needs a playtest: the next log should show
animation 0x6F7, then the fight state reaching 4 and beyond (turns 8/9, walking, firing). If
the roar is refused, the log names it.

### Stage 1 is a standoff (fixed after v0.4.0-mapmaker.25)

User report (mapmaker.25): "Crow walker still won't turn to face me." Static analysis
(CONFIRMED by decode, `test_walker_guns_gate_the_fight`; the log for this build was not
available yet):

- **How it turns:** state 4 (0x0029B696) sends it to state 8 when the aim error |+0xE4|
  exceeds 60. State 8 plays `crowewalkerturnleft`/`right` 0x6F4/0x6F5 while it is still above 40 and
  enters state 9 (else back to state 4). When that animation ends, state 9 sets +0x590 to ±1. That turns the yaw
  (entity +0x70, copied to +0xC4) by 59.5 degrees (0x0029CA42..0x0029CABF). The yaw changes
  only through these finished turn animations.
- **Why it never turned:** state 4 runs its walking and turning code only while both guns are
  active (state +0x4CC and +0x52C == 1). Otherwise it goes to state 0x13
  (0x0029BC6F -> 0x0029BE76, edi = 0x13 from 0x0029B1EC). State 0x13 (0x0029C933) only waits
  for both guns.
  - The level init never sets the guns (`sub_002976F0`, `sub_00297690`, `sub_002971D0`,
    `sub_00296000`, `sub_00296BD0`).
  - Only `sub_00294B90` does (ecx = 1 at 0x00294C1C), called once, by state 0x12 when an
    animation ends (0x0029CAF0). The damage code clears a destroyed gun.
  - State 0x12 is requested by the damage code (`sub_00298570`, from the damage callback
    `sub_00298BA0`): when stage-1 health +0x580 falls to +0x584 (60 -> 40, or 50 -> 35 at the
    easiest setting), with the stun `crowewalkerstun` 0x6F6 and stage 2 (0x00298AB5..0x00298B01).
  - Stage-1 hits count only in the walker's back: `sub_00295150` needs the hit direction
    against its facing below -0.75 (0x0047DE84).
- So stage 1 is a standoff in state 0x13: the walker stands without turning until shot in the
  back to the stun. Mission 13 arranges this with its arena and the 180 turns (INFERRED). In a
  MapMaker map the walker faces the player and never leaves stage 1.
- The animations are not the problem: lv81's ids 0x6B9..0x6FC collide with none of lv1_2,
  lv1_3, lv1_6, lv14 or the 44 global `pak/anim` tables (checked against every registration
  table). They are registered whenever lv81.pak is mounted (0x001CA92C).

| change | what it does |
|---|---|
| `_fx` wake-up, guns | once per level: when the walker stands in state 0x13 in stage 1 with both guns off and no request pending, request state 0x12 (AI +0x9D0). When the current animation ends, the stock `sub_00294B90` deploys the guns and state 4 runs the fight (turns 8/9, walking, firing). Back hits and the stun keep working. Logged once: `Crow walker guns deployed: the stage-1 standoff ... skipped` |
| fight-state log | every change of the fight state, 40 per level: `Crow walker fight state A -> B: stage, animation, yaw, aim, turn, guns, health (stun at), request` |
| heartbeat | the animation line also shows the yaw (+0x70), turn (+0x590), guns and stun threshold |

INFERRED, needs a playtest: walking and gun fire on MapMaker tiles; destroyed guns send it back
to state 0x13 until the next stun, as in stock.

### The roar never started either: the bind pose (fixed after v0.4.0-mapmaker.25)

The mapmaker.25 log (built before the guns change) showed the wake-up, then
`animation 0xFE (blend 0x0, finished 0)` and fight state 2 in every beat. The mapmaker.23 roar
start ran only for an empty controller. CONFIRMED (decode, `test_walker_roar_needs_an_animation`):

- Character creation `sub_000DAC30` gives a creature its skeleton's bind pose as its first
  "animation", with `sub_000966E0` (0x000DB0BB..0x000DB1CF): 0xFE for the walker skeleton
  (0x000DB1C9), 0xA3 for the mansion boss, 0xA8 for the split Crowe, 0xF0 for the worm.
- `sub_000966E0` sets +0xD4 = id and clears "finished" (+0x124). A pose never finishes, so state
  2 still waited.
- The skeleton and the split Crowe fights play their first animation over their poses, and they
  work in game.

| change | what it does |
|---|---|
| `_fx` wake-up, roar | the roar now starts (once per level) whenever neither the current animation nor the blend target is one of the walker's own (lv81 0x6EE..0x6FC), so over the bind pose too. Logged once: `Crow walker had none of its own animations (bind pose): roar started ...` with the animation it replaced |

The next log should show animation 0x6F7, then the fight state passing 4 -> 0x13 -> 0x12 -> 4
and then 8/9 turns (the fight-state lines from the guns change).

### Several per map (after v0.4.0-mapmaker.25)

User request (2026-11-01): "Same one-per-map limit applies presently to the giant skeleton. All of
these one-per-map limits need fixed." The giant trophy monster already allowed several (no
per-level state: every one gets class 0x80). The giant skeleton and the Crow walker now do too.

CONFIRMED (decode, `test_several_per_map_entries`):

- Their fights keep everything in a per-level block (`[0x0078B1EC]` 0x4C7C, `[0x0078B1E8]`
  0x64DC). The particle and effect code they call keeps its state in statics
  0x0078B080..0x0078B1DB: the particle block pointer `[0x0078B0D0]`, emitter shape, counts,
  texture, and the walker's effect blocks `[0x0078B090]`/`[0x0078B098]`. The boss pointers follow
  at 0x0078B1DC (robot), 0x0078B1E4 (split Crowe), 0x0078B1E8 and 0x0078B1EC.
- Every function that reads `[0x0078B1E8]` is reached from the walker's fight `sub_002998E0`, its
  message handler `sub_0029CBF0`, its damage callback `sub_00298BA0`, its object slots 2/3
  `sub_00299140`/`sub_00298F60` (all installed by its first frame) or the beam draw `sub_0029CCB0`.
  The exceptions are the level init `sub_002976F0`/`sub_00297690`/`sub_00296BD0`.
- Every function that reads `[0x0078B1EC]` is reached from the mansion fight `sub_0029CEF0`, its
  slot-2 callback `sub_00286430` or its particle-type callback `sub_00288BD0`. The exceptions are
  the catacombs fight and its callbacks (never run for a MapMaker skeleton), the init
  `sub_00287E00`/`sub_00287860` and the scene 1/17 frame draw `sub_0029CE00`.
- `sub_00288BD0` is registered once for all skeletons (`sub_00378400` keeps one entry per
  callback) and is called with no entity. The skeleton message handler and its damage callback
  `sub_00289690` read neither pointer nor the statics.

| change | what it does |
|---|---|
| level | one instance per Story AI placed with the boss (Story AI chunk +0x28 slot whose set member is 0x28 / 0x64; at least 1, at most 4), each made by the stock `sub_00287E00` / `sub_002976F0` from the statics as they were. Each keeps its own copy of the statics, and the statics are put back after. The boss pointers are left on the first instance ("home") |
| swap | around every entry above (`_princess` for the mansion fight, the new wrappers, the fire and beam draws in `_fx`), the instance's pointer and statics go in. After the call the statics are kept in the instance, and pointer and statics are put back exactly. An inner entry of the instance already in keeps the pointer. The statics always go through the one holder shared with the split Crowe area (see below). A robot, split Crowe or other instance never sees this one's blocks |
| ai_start | each skeleton / walker takes a free instance (the same one again for a restarting AI). One more than the map prepared keeps the bot class (`no giant skeleton left this level ...`) |
| walker | heartbeat, fight-state log, wake-up, roar and guns per walker (logs say `Crow walker N`). One WELL stand-in per walker. The end of a walker's death removes that walker |
| skeleton | floor height, throw and fire per skeleton |

Not routed (INFERRED, visual only): `sub_00288BD0` always sees the first skeleton's state. Its
particles for the other skeletons may be placed from the first one's values. Memory: about 51 KB per
skeleton and 59 KB per walker of the level heap. The new wrappers are under
[Integrator notes](#integrator-notes).

**One holder for the statics (after the v0.4.0-mapmaker.28 crash report)**. The split Crowe's
code uses 28 words of the same window (`docs/mapmaker/mapmaker-crowe.md`, "Several split
Crowes"). Up to .28 each area copied its part of the window on its own, and an inner entry
of "the instance already in" swapped nothing. So a walker's damage callback inside a split
Crowe's hit inside that walker's fight ran on the Crowe's particle and effect blocks (and the
reverse). Now `pc_mapmaker_bossfx.inc` (included by both areas) keeps one holder: entering an
instance parks the live window in the current holder's copy (another area's boss, another
instance, or the outside copy) and loads the instance's; leaving enters the previous holder
again. Every nesting order sees each instance's latest values and puts everything back. The
level init and late allocations still start from the outside statics (a level start first
drops a holder that is one of its own instances). Tests: the fixture's nested foreign-holder
case and `tests/mapmaker/test_mapmaker_bossnest.py`, which builds both areas in one
translation unit as the game does.

The crash itself was the split Crowe's egg drop (null props released on landing, each skipped
wild call leaving esp 8 bytes high): `docs/mapmaker/mapmaker-crowe.md`, "Eggs". The
realbosses hooks were audited for esp on every path (each runs its original once or does
one `ret`; `princess` and `message` call the mansion variant directly, which pops the
caller's return address like the lifted tail jump) and need no change.

## The Crow walker (story mission 13)

User request (2026-11-01): the Crow walker and tank bosses next. The walker
is a character, so it follows the giant skeleton's recipe; the tank is a
level prop (research `logs/limits/features/real-bosses.md` section 4) and is
not done yet. CONFIRMED (decode and US XBE data, `test_walker_*`):

| | value |
|---|---|
| character id | 0x64 (character table: voice set 0x00539948, model 0x637 `chrs/walkercrow`, no LOD; not story-only flagged); no Arcade row |
| picker | synthetic row 152 "Crow Walker", text 0x52420064; preview stand-in the Arcade giant robot (id 0xA8, row 90) |
| behaviour | class 0x100000: `sub_0027D590` 0x0027DD2A -> `sub_002998E0(entity)` (call 0x0027DE5A, its only caller); no scene test in the fight, its only direct callee that reads the scene is the level event `sub_002274C0` |
| death | message handler `sub_0029CBF0` (0x0027D426): message 2 plays `crowewalkerdeath` 0x6F9 and sets anim state 0x14 in every scene; no message hook needed |
| animations | `pak/stream/lv81/anim/lv81.pak` (string 0x0049D9A8, 94 KB, registered at 0x001CA92C from table 0x00484F68: `crowewalker*` 0x6EE..0x6FC plus five scene props); no id shared with lv1_3 or lv1_6 |
| per-level state | `sub_002976F0` (stock from `sub_0025F720` only in scene 81): `sub_002A8300` (effect blocks `[0x0078B090]`/`[0x0078B098]`), `sub_002A62C0` (the particle block `[0x0078B0D0]` the skeleton also uses), `[0x0078B1E8]` = `sub_0003DFA0(0x64DC, 1)`, then `sub_00297690` sets every field |

The fight steers from the entity's own position and the players
(`[0x007B0C48]`); no arena coordinate was found in its state or code.

| WRAP | what it does (scene 100 story, `TSFP_MAPMAKER_LIMITS=1`) |
|---|---|
| picker_end, pick, highlight, row_of, row_text, text | row 152 / id 0x64 as rows 150/151 |
| `sub_001C8540` anims | also mounts lv81.pak when the map's set (slots 1-20) holds 0x64 |
| `sub_0025F720` level | before the skeleton's: clears `[0x0078B1E8]` and calls the stock `sub_002976F0` (about 59 KB of level heap with its effect blocks) |
| `sub_0015B830` ai_start | one walker per map gets record class 0/0x100000 when lv81.pak is mounted and this level's state exists; notes it for the Boss bar, meter and HUD as the others |
| `sub_0023FF20` -> `pc_mapmaker_realbosses_tag` | the fight's first frame looks up the level object tagged `WELL` (0x57454C4C, call 0x0029B32C, ret 0x0029B331) and writes its +0x28 without a null test (0x0029B349; again at the end of its death, 0x0029CB3A). A MapMaker map has none: that call gets a zeroed 0x2E8-byte stand-in object (heap, reused, zeroed per lookup). A real object is returned unchanged |
| `sub_002274C0` event | level event 1 after its intro (ret 0x0029AEBE, 0x0029B120) and event 2 when the death animation ends (ret 0x0029CB80) are the map's own logic markers: dropped. At event 2 the walker is removed with `sub_0015DDA0` (the skeleton's stock death call) and a story focus on it is cleared; mission 13 runs its ending instead |
| `sub_00126560` -> `pc_mapmaker_realbosses_fx` | the frame `sub_001CEBC0` draws the walker's beams (`sub_0029CCB0`) only in scene 81 (0x001CEE8C), right after `sub_00126560` (call 0x001CEE87, its only caller): after the original, with this level's walker state, called in scene 100 too. First it wakes a walker still waiting for mission 13's `SPC1`/`SPC2` trigger (see "The walker never woke") |
| hud, meter, bigdraw | model 0x637 like 0x5E9/0x5BE: Boss Off = no health, Boss bar = the bosses bar, Sub-boss = the meter; drawn over the whole view |

Risks (INFERRED, need a playtest): movement on MapMaker tiles and pads
(the walker is large and may not fit through tile doorways), the jumps
(`crowewalkerjumpleft/right`) against tile walls, collision radius, what the
stand-in `WELL` would have shown (the fight only toggles its flag 0x40 and
moves its +0x68), beam visuals in scene 100, a walker and a skeleton in the
same map sharing the particle block `[0x0078B0D0]` (both valid; the walker's
emitter shape `sub_002A62F0` is set first, the skeleton's after). Not built
or played.

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
WRAP sub_001CDD60 -> pc_mapmaker_realbosses_restart        (after .31; level restart, after the original; layer with khallos/tank/... restart, any order)
WRAP sub_0029FE00 -> pc_mapmaker_realbosses_princess       (new; class 0x20000 dispatch, per AI frame)
WRAP sub_0028D0A0 -> pc_mapmaker_realbosses_message        (new; class 0x20000 messages, incl. its death)
WRAP sub_002875F0 -> pc_mapmaker_realbosses_risen          (new; per skeleton frame in state 1)
WRAP sub_00287E20 -> pc_mapmaker_realbosses_throw          (new; per skeleton frame)
WRAP sub_002274C0 -> pc_mapmaker_realbosses_event          (new; level events, return-address test first)
WRAP sub_000862C0 -> pc_mapmaker_bigdraw_rect             (new, area bigdraw; prop draw rectangle, per frame per prop of a drawn room)
```

Added for several giant skeletons / Crow walkers per map (after v0.4.0-mapmaker.25):

```
WRAP sub_00286430 -> pc_mapmaker_realbosses_skel_slot      (new; the mansion fight's slot-2 callback, per skeleton frame)
WRAP sub_002998E0 -> pc_mapmaker_realbosses_walk_fight     (new; class 0x100000 dispatch, per walker frame)
WRAP sub_0029CBF0 -> pc_mapmaker_realbosses_walk_message   (new; the walker's message handler)
WRAP sub_00298BA0 -> pc_mapmaker_realbosses_walk_damage    (new; the walker's damage callback, cdecl damage record)
WRAP sub_00299140 -> pc_mapmaker_realbosses_walk_slot      (new; walker slot 2)
WRAP sub_00298F60 -> pc_mapmaker_realbosses_walk_slot      (new; walker slot 3: the same hook for both)
```

Each swaps the boss's instance in around the original and runs the original
unchanged for anything else (no instance for the entity, another scene, the
switch off). No new guest calls, so no new link stubs. Order with other
wrappers of these functions: free (none exist).

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
9. Crow walker: in a custom set slot pick "Crow Walker" (after Giant
   Skeleton; preview shows the giant robot), give one Story AI that slot and
   play in Story. stderr: `set check found: ... Crow walker slot N`,
   `mounted lv81.pak for the Crow walker`, `Crow walker state allocated`,
   `Crow walker gets its story AI (class 0x100000)`, then `no WELL object in
   a MapMaker map; stand-in used`, `Crow walker effects drawn` and `Crow
   walker woken: no SPC1/SPC2 trigger ...`; the heartbeat's fight state should
   leave 1 (roar, then 4 and beyond). Check it
   walks, turns, jumps and fires its beams at you; with the Boss bar, empty
   it: death animation, then it disappears (`Crow walker defeated:
   removed`, `Crow walker level event dropped`), logic markers unchanged.
   A second Crow walker AI fights as a bot (`only one Crow walker per map`).
   Mission 13 still plays stock.

## Late instances at the AI start (v0.4.0-mapmaker.27)

The level init's count (`rbs_count`) found one skeleton and one walker in the
.26 test where three of each were placed, so the extras played as bots.
`rbs_walk_more`/`rbs_skel_more` now allocate one more instance at its AI start
when none is free (up to `RBS_INST_MAX`), from the statics saved at the level
init (`rbs_fx_base`), putting the live statics and the home boss pointers back.
The split Crowe area already allocated late (four Crowes played in .26). Why
the count misses placed AIs is still open: stock spawn records keep the slot as
word +0x24 of the Story AI data, while `RBS_SAI_SLOT` reads byte +0x28.

Open: the walker took no damage in any .26 run (health stayed 60). Its damage
callback `sub_00298BA0` -> `sub_00298570` scores hits by distance to its parts
(state +0x188.. with +0x19C == 1) and explosion type 0x17; the stage-1 fight
only takes hits on particular parts.
