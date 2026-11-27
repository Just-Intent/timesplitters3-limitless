# MapMaker custom character set: every Arcade character

Status: implemented in `src/interface/pc_mapmaker_characters.inc` (area
`characters`), fixture-tested (`tests/mapmaker/test_mapmaker_characters.py`:
17 mutations, plus checks of every site in the decode and of the real Arcade
table rows in the US XBE). Not built or played yet. No guest-code table entry.

User report (2026-10-31): "I don't see any big character available in the
screen where I can edit bots in the custom mapmaker bot set."

## Where the picker list comes from (stock, CONFIRMED)

- Editor **Map Settings** -> character set: `sub_003128F0` opens descriptor
  0x0052375C; its slot screen 0x0052372C (builder `sub_002C19F0`) opens the
  character picker 0x0052374C. The screens edit a 10-slot work copy at
  0x0075FD90 (stride 0x1C), filled from the editor's custom set
  (editor +0x11860, `sub_00312950`) by `sub_002C00F0`/`sub_003141B0` and stored
  back by `sub_002BFFD0`/`sub_00314210`.
- The picker builder `sub_002C1EA0` walks the 150 rows of the Arcade
  character table 0x00521A18 (0x30 per row: +1 sbyte variant, +4 character
  id, +0xC portrait image, +0x14 unlock rule) and adds a row only when
  `sub_001CD980(id, variant)` (call 0x002C1F30, returning to 0x002C1F35)
  returns non-zero. The editor scene (0x66) changes only the item flags
  (0x002C1EAC). The pick callback `sub_002C1C90` stores the row's id and
  variant without another check.
- `sub_001CD980`: the row (`sub_002BE590`), rule type +0x14 must be non-zero,
  then the "all characters" cheat byte (`sub_001CCEB0(4)`, 0x0075AEFC) or the
  rule check `sub_001CD3E0` (jump table 0x001CD530: type 1 always, 2..8 story,
  challenge and league conditions, 0 never). The same routine filters the
  Arcade character select and the profile's own bot sets.

So there is no size, collision, category or memory filter: a character is
missing only because the profile has not unlocked it. Real rows (portrait
names from 0x004D2D50 + image*0x18 + 0xC): giant robot row 90, id 0xA8, rule 2
(story); the four Jacob Crow rows 11..14 (ids 0x1B, 0x49, 0x1F, 0x1E), rule 3;
steam robot, Anya, Cortez variants, many challenge (rule 5) and league (rule 8)
characters. Trophy monster (0xA7), Khallos (0x02) and big monster (0x91) have
rule 1 and are always listed. Every row has a rule 1..8 and an id below 0xB0.

## Change (`TSFP_MAPMAKER_LIMITS=1`)

`pc_mapmaker_characters_eligible` wraps `sub_001CD980`. Only in the editor
(scene 102) and only for the call that returns to 0x002C1F35 (the picker), it
runs the original and, when that says "locked", reports the row available if
the Arcade table holds it with a rule type 1..8. Every other caller and scene
keeps the stock answer (Arcade character select, profile bot sets in the front
end, Story character choice). The list therefore shows all 150 Arcade
characters and variants, in table order, whatever the profile has unlocked.
The first four widened rows are logged
(`[mapmaker] characters: offering a character the profile has not unlocked`).

Asset safety: only Arcade-table characters can be picked, and every one of them
is a character the stock game already loads in Arcade levels and MapMaker maps
(chr.pak); stock MapMaker presets already contain the giant robot (preset 7)
and the trophy monster (preset 8). Story-only creatures (no Arcade row) cannot
be offered.

Load diagnostic: `pc_mapmaker_characters_preload` wraps `sub_000D7780`. At a
MapMaker level start (scene 100) for the set preload calls from `sub_0023F320`
(custom, return 0x0023F33F) and `sub_0023F350` (preset, return 0x0023F37F) it
logs a member that is not an Arcade character, has no model id below 0x9D0
(character table 0x0053A3CC + id*0x30 +4) or whose model has no resource slot
(int16 0x0078B860[model] < 0, entry outside guest memory). It never changes or
skips anything: spawning reads the set separately (`sub_0006BC50`), so skipping
the preload would not avoid a missing model, and every offered character
resolves in stock. A log line here is a bug report, not a recovery.

## Save format and stock loads

Unchanged. The map-info chunk (tag 0x00C77668) stores the set index (+0x14,
>= 0xC = custom), the count (+0x16) and 4-byte members from +0x18 (word id,
byte, sbyte variant at +3). The compiler (0x00300896), the editor loader
(0x00301D5F..) and the play builder `sub_002C0210` copy the values without
an unlock check; `sub_001CD980` has no caller outside the front-end menus.
A stock game loading such a map plays it as a fully unlocked stock profile
(or the "all characters" cheat) would: the same ids are reachable in stock, so
no fork-only form is needed. Opening the set in a stock editor shows the
names; re-picking there only offers its unlocked rows.

Slots 11-20 of the custom set (area `sets`) are stored in a fork-only chunk:
see [mapmaker-sets.md](mapmaker-sets.md).

## Hooks (WRAP lines for the integrator, area `characters`)

```
WRAP sub_001CD980 -> pc_mapmaker_characters_eligible   (new; also run by the Arcade character select, cheap: return-address test first)
WRAP sub_000D7780 -> pc_mapmaker_characters_preload    (new; level load only, log-only)
```

Neither function is wrapped elsewhere (`sub_001CD3E0`, called inside
`sub_001CD980`, is the profile unlock patch's `pc_unlock_requirement`; the
two compose). Include the `.inc` after `pc_mapmaker_limits.inc`.

## User test steps

1. `TSFP_MAPMAKER_LIMITS=1`, a profile with few unlocks. Editor -> Map
   Settings -> characters: custom set -> pick a slot: the list now includes
   the giant robot, the Jacob Crow variants and challenge/league characters.
2. Put the giant robot and Khallos into slots; place a Story AI using each,
   set **Boss: health x4**. Save, reload: the set and the AIs are kept.
3. Play the map in Story: both spawn with their models and the boss bar.
   stderr shows no `set member ...` line.
4. Without the switch: the picker shows only unlocked characters; the saved
   map still loads and plays with the giant robot.
