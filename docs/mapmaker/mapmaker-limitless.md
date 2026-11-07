# MapMaker Limitless: player guide

[Back to the README](../README.md)

TimeSplitters 3 Limitless is a fork of the
[original PC port](https://github.com/TheDoffman/timesplitters-future-perfect-pc).
This guide covers what the fork adds to MapMaker, how to switch it on, and
what has and has not been tested. Technical notes for each area are in the
`docs/mapmaker-*.md` files; measured test results are in [STATUS.md](../project/STATUS.md).

> [!WARNING]
> This is experimental work. The maintainer tests compile-only Windows builds
> on a separate Windows 11 PC. There is no published fork release yet, and
> nothing here has been run on a Mac. Keep backups of maps you care about.

## Getting started

**With the launcher:** open **Settings → MapMaker**, tick the switches you want
(**Raised MapMaker limits** = `LIMITS`, **Overlapping tiles** = `OVERLAP`,
**Stacked items** = `STACK_ITEMS`; **Characters alive at once** = `ALIVE`),
save, then press Play and open **MapMaker**. Everything is off (64 characters)
by default. The launcher ignores these environment variables from your shell
and passes only its saved choices. `MEMORY_SCALE` and the developer switches
are not in the launcher.

**From source:**

1. Build the game from source with the [build guide](../development/building.md). You need
   your own US Xbox copy of the game (title ID `45410066`).
2. Open PowerShell in the checkout and set the switches you want:

   ```powershell
   $env:TSFP_MAPMAKER_LIMITS = '1'
   $env:TSFP_MAPMAKER_OVERLAP = '1'      # optional
   $env:TSFP_MAPMAKER_STACK_ITEMS = '1'  # optional
   .\tsfp.ps1 -Action Run
   ```

   The variables only last for that PowerShell window. Close it, or set a
   variable to an empty string, to return to the original behaviour.
3. Open **MapMaker** from the main menu.

## Switches

| Switch | Default | What it does | Tested |
| --- | --- | --- | --- |
| `TSFP_MAPMAKER_LIMITS=1` | Off | The main switch: the raised editor limits below, sixteen floors and the 80x80 grid, any unlocked character in MapMaker Story, Story AI duplicates and linked copies, the Clouds checkbox and the open-sky tiles. | See each feature below |
| `TSFP_MAPMAKER_OVERLAP=1` | Off | Tiles can be placed over other tiles. | Checked in game (2026-10-31) |
| `TSFP_MAPMAKER_STACK_ITEMS=1` | Off | Several items can share one grid cell. | Checked in game (2026-10-31) |
| `TSFP_MAPMAKER_MEMORY_SCALE=2..8` | 8 with `LIMITS` | How far the editor's memory estimate is relaxed. Lower it if large maps run short of memory. | Used in the capacity play test |
| `TSFP_MAPMAKER_ALIVE=1..64` | 64 | Most characters alive at once in a local MapMaker Story map. 32 or 24 can help a slow PC; 20 matches the original game. | Automated tests only |
| `TSFP_MAPMAKER_POINTER=0` | Mouse on | Turns the editor mouse controls off. | Editor mouse checked in game |
| `TSFP_MAPMAKER_POOLS=0` | Pools on | Restores the original AI navigation pools in MapMaker play. This also turns `LIMITS` off. Troubleshooting only. | - |
| `TSFP_MAPMAKER_ROOM_MASK_CLEAR=<hex>` | Off | Developer test for open-air (sky) tiles. Not a player feature. | Work in progress |
| `TSFP_MAPMAKER_TABLE=0` | Table on with `LIMITS` | Shows the original memory meter instead of the [resource table](mapmaker-restable.md). | Automated tests only |
| `TSFP_MAPMAKER_PROBE=<flags>` | Off | Read-only diagnostic log; see the [probe guide](mapmaker-meter-probe.md). Flag 32 also shows how much memory is left for a larger arena. | Developer use |
| `TSFP_MAPMAKER_ARENA_MB=0..16` | 0 (off) | Gives the game's heap that many more MB in every scene (also story levels), taken from the same 64 MB block as textures. Measure with probe flag 32 first; see [memory](mapmaker-memory.md). | Not tested in game |
| `TSFP_MAPMAKER_BOSS_RANGE=<units>` | 30 | How close you must be for a Boss Story AI's health bar to show (1..100000 game units). | Automated tests only |
| `TSFP_MAPMAKER_REAL_BOSS=0` | Story behaviour on | With `LIMITS`: the giant trophy monster and the giant skeleton fight like normal bots instead of with its story behaviour. See [real bosses](mapmaker-realbosses.md). | Automated tests only |
| `TSFP_MAPMAKER_CREATURE_PROBE=1` | Off | Read-only log of the characters each level spawns and whether story-only creature models can load; write it to a file with `TSFP_MAPMAKER_CREATURE_PROBE_LOG=<file>`. See [bosses](mapmaker-bosses.md). | Developer use |

## Raised limits (`TSFP_MAPMAKER_LIMITS=1`)

| Limit | Original | Limitless |
| --- | --- | --- |
| Tiles (pieces) | 200 | 1023 (see below) |
| Items | 50 | 500 |
| Keycards | 2 per colour | 16 per colour |
| Pickups (weapons, ammo, health, armour, power-ups) | original per-type maxima | x4 |
| Movable objects, props and markers | 20 each | 160 each |
| Turrets | 8 per type | 24 per type |
| Teleporters | 2 per colour | 8 per colour |
| Story AI | 50 | 511 |
| Characters alive at once (MapMaker Story, local play) | 32, with about 20 AI | 64 (63 AI with one player) |
| Bots per MapMaker Arcade match (local play) | 10 (16 characters) | 20 (24 characters) |
| Logic operations / nodes / records | 30 / 130 / 100 | 120 / 520 / 400 |
| Free-standing objects | 40 / 30 / 100 | 160 / 120 / 400 |
| Pads / pad links / pad groups | 2400 / 4400 / 300 | 7680 / 32256 / 1024 |
| Engine objects (tiles + items + 128 kept for characters and explosions) | 550 | 1023 |
| Map file | 32 KB | 512 KB |
| Memory budget meter | 1x | 8x |

With `LIMITS` the editor's memory meter becomes a [resource table](mapmaker-restable.md):
one row per limit above ("Tiles: 123/1023"), amber near the limit and red at
it. *Automated tests only.*

**Tiles past 511 (2026-11-07, not yet tested in game).** The tile limit is
1023, but every tile is also one engine object, and the game has room for
1023 of those in all: tiles + items + 128 must fit. So a map can hold about
895 tiles with no items, 600 tiles with 295 items, or 511 tiles with 384
items; the Objects row of the resource table shows how close you are.
Heavy tiles (many pads and links) stop earlier on Pads (about 549) or Links
(about 521), and typical maps near 680 tiles on Pad groups. Saving, loading
and every edit get slower as the tile count grows (the editor recomputes
the whole map each time). Old maps load as before.

**Tested (STATUS.md, 2026-10-30):** editor and play on maps with 236 tiles,
200 items and more than 102 Story AI, up to 64 characters in MapMaker Story,
team deathmatch and the first story mission. No crashes; at least 5.56 MB of
memory stayed free. Not tested: System Link, splitscreen, Mac, other story
levels.

**Round 3 (2026-10-31, not yet tested in game):** items 500, keycards 16 per
colour, movers/props/markers 160, Story AI 511. Maps that hold more of
anything than the original game allows are now always saved in a form the
original game refuses to load (instead of crashing it). Every heap overrun
is logged once as `[mapmaker] heap full: ...`.

The extra play-time capacity (more characters alive, larger object pools)
is always present in MapMaker play; the switch decides how much the editor
lets you place.

## New editor features

### Sixteen floors and an 80x80 grid (`LIMITS`)

The floor menu has sixteen floors (-2 to 13) instead of five, and the grid is
80 x 80 cells instead of 40 x 40. The 3D view can also move up to the top
floor. Old maps open where they were and keep their floors. Pieces placed
beyond the original grid or floors still play, but the original editor cannot
select them. *Seven floors and the 80x80 grid were checked in game
(2026-10-31); sixteen floors are not tested in game yet.*

### Overlapping tiles (`OVERLAP`)

Tiles can be placed on top of each other. Expect visual seams, gaps in the
sky, one-way walls and AI that cannot path between overlapping pieces; the
result depends on placement order. Removing a tile restores the cells of the
tiles underneath it. *Checked in game (2026-10-31).*

### Stacked items (`STACK_ITEMS`)

Weapons, ammo, keycards, health, armour, power-ups, special pickups and props
can share one cell. With an item held, the place button always drops it; with
empty hands it picks up the oldest item in the cell first. Teleporters, start
points and flags never stack. Stacked items spawn on the same spot in play.
*Checked in game (2026-10-31).*

### Any unlocked character in MapMaker Story (`LIMITS`)

When a MapMaker map is played in Story, the character screen offers every
character your profile has unlocked (the Arcade roster) instead of Cortez only.
The choice is remembered in your profile, as in Arcade. Campaign story, Arcade,
challenges and System Link are unchanged. *Checked in game: every unlocked character is offered (2026-10-31). HUD portraits
and end screens with unusual characters are unchecked.*

### Story AI: Duplicate and linked copies (`LIMITS`)

Open a Story AI from the Story AI list. New rows appear above **Done**:

- **Duplicate Story AI** makes an independent copy with the same settings and
  path.
- **Add linked copy** makes this AI a group leader and adds a numbered copy
  ("Guard 2", "Guard 3", ...). Linked AIs share character, team, weapon,
  behaviour, spawn settings and type; name, colour, path, facing and patrol
  mode stay separate.
- **Logic means: this AI / whole group** (leader only) chooses whether logic on
  the leader applies to the leader alone or to any member of the group.
- **Unlink copy** / **Unlink (next copy leads)** remove an AI from its group.
  Deleting or unlinking a leader makes its first copy the new leader.

Maps with groups load in the original game as ordinary, separate Story AI.
*Checked in game (2026-10-31).*

### Story AI bosses (`LIMITS`)

The Story AI edit screen also has a **Boss** row. Each click cycles **Off,
On, health x2, x4, x8**. In MapMaker Story play a Boss AI has that much more
health, and the story boss health bar, labelled with the AI's name, appears
when you are within about 30 units of a living boss
(`TSFP_MAPMAKER_BOSS_RANGE` changes the distance). With several bosses in
range, up to four bars stack upward (nearest at the bottom; two in split
screen); a story boss bar of the game itself keeps the bottom place. Any character in the map's
character set can be a boss (for example the giant robot or Khallos); it keeps
its normal MapMaker AI behaviour, not the story boss fight. While Boss is not
Off the screen also has **Health display: Boss bar** (the bar at the bottom
of the screen) or, after a click, **Health display: Sub-boss bar** (a small
health meter floating above that enemy instead, not limited to the bar distance).
After switching Boss from Off, reopen the AI to see that row. **Duplicate
Story AI** and **Add linked copy** keep the Boss and Health display settings
(change them per AI afterwards). Maps with bosses
load in the original game with ordinary AI. Story-only creatures (the mission 5
ghosts and worms) are not deployable yet. *Automated tests only.*
Details: [bosses](mapmaker-bosses.md).

### Stronger Story AI health (`LIMITS`)

The Story AI **Health** choice (Weakling, Low, Normal, High, Tough Guy) has
four more settings after Tough Guy: **Brute** (2x Tough Guy's health),
**Juggernaut** (4x), **Titan** (8x) and **Colossus** (16x). They combine with
the Boss setting: a Colossus boss at health x8 has 128 times Tough Guy's
health. Without the switch, or in the original game, such an AI loads as
Tough Guy. *Automated tests only.*

### Every character in the custom character set (`LIMITS`)

In **Map Settings**, the custom character set's picker lists all 150 Arcade
characters and variants, including the giant robot, the Jacob Crow variants
and challenge or league characters your profile has not unlocked. Only
characters that Arcade itself can use are offered; story-only creatures are
not. Maps using them load and play in the original game too. *Automated tests
only.* Details: [characters](mapmaker-characters.md).

### 20 character slots (`LIMITS`)

The custom character set has 20 slots instead of 10. In **Map Settings**,
open the custom set: the title shows **(1-10)**; press Down on the last row
to see slots **(11-20)** and Up on the first row to go back. New slots start
empty. A Story AI's character choice lists all 20 slots. Players still use
slots 1-10; Arcade bots can use 11-20 (see 20 bots below). A map that uses slots 11-20 needs this fork:
the original game and System Link refuse it; other maps are saved as before.
An AI set to an empty slot plays as the first filled slot. More different
characters use more memory. *Automated tests only.* Details:
[sets](mapmaker-sets.md).

### 20 weapon slots (`LIMITS`)

The custom weapon set has 20 slots instead of 6. In **Map Settings**, open
the custom weapon set (the editor's Start menu; this is where the set is
defined): the slot list now has 20 rows, numbered 1-20, in a list that
scrolls like the editor's other long lists: move the pointer or press Down
past row 6, turn the mouse wheel, or drag the scrollbar. New slots start
empty. Pick a weapon for any row as before (X clears it). Save the map
afterwards. The front end's Arcade setup shows the MapMaker set's 20 rows
the same way, read only; the in-game pause menu has no weapon set screen (as in the original). A weapon or ammo pickup's
options gain a **Weapon** choice: its own slot (as before) or slots 7-20. A
Story AI's weapon choice lists all 20 slots. Bots pick from every filled
slot. A map that uses slots 7-20 needs this fork: the original game and
System Link refuse it; other maps are saved as before. More different weapons
use more memory. The editor shows a pickup on slots 7-20 with its own
slot's model. *Automated tests only.* Details:
[weapon set](mapmaker-weaponset.md).

With the switch the editor's weapon list (pick a weapon for a slot) also
offers three weapons the game hides: **Time Grenade**, **TNT** and the
**Temporal Uplink**. They work in slots, pickups and Story AI weapons; the
time grenade explodes like a grenade (no slow motion was found in the game's
code). The front end's profile weapon sets are unchanged. *Automated tests
only.*

The **Bots** screen of an Arcade match setup (front end and the in-game pause
menu's setup) has its own page 2 when you play a MapMaker map: see the next
section. In the editor the Test map setup's Bots screen edits the map's set
and pages like Map Settings; the test's bots are slots 1-10 plus any used
slots 11-20.

### 20 bots in MapMaker Arcade (`LIMITS`)

A MapMaker Arcade match (and the editor's Test map) can have **20 bots and
24 characters** (4 players + 20 bots) instead of 10 and 16. On the Bots
screen press Down on the last row to see bots **(11-20)**; Up on the first
row goes back. With the map's own character set, rows 11-20 start as the
map's slots 11-20; otherwise they start empty and are kept until you quit
the game (they are not saved in your profile). Ordinary Arcade maps and
System Link keep 10 bots. *Automated tests only.* Details:
[matchbots](mapmaker-matchbots.md).

### The giant trophy monster from story mission 5 (`LIMITS`)

The custom character set's picker also ends with **Giant Trophy Monster**: the
large creature from the mansion mission, not the Arcade trophy monster. Put it
in a Story AI; in MapMaker Story play it uses its story behaviour (roars,
charges and head-butts). With **Boss** Off no health is shown for it; give it
the **Boss** row for a single bar with the AI's name and more health; with
**Health display: Sub-boss bar** it shows the meter on its back instead of a
bottom bar. `TSFP_MAPMAKER_REAL_BOSS=0`
keeps the big model but makes it fight like a normal bot. The original game
plays such a map with the bot behaviour and shows an empty name in the editor.
*Automated tests only.* Details: [real bosses](mapmaker-realbosses.md).

### The giant skeleton from story mission 5 (`LIMITS`)

The picker's next row is **Giant Skeleton**: the mansion boss that rises out
of the ground. Put it in one Story AI; in MapMaker Story play it rises where
that AI spawns, then stamps, swipes, bites and throws at you, and dies with its
own animation. Its death does not trigger your map's logic. Only one per map
gets the boss behaviour (others fight like bots and look wrong). Boss row and
Health display work as for the trophy monster (Off = no health shown). The
dust clouds of the original fight are not shown. *Not yet built or played.*
Details: [real bosses](mapmaker-realbosses.md).

### Prometheus robots from story mission 9 (`LIMITS`)

The custom character set's picker ends with two more rows: **Prometheus
SK/8** (the Machine Wars rocket robot) and **Prometheus SD/7** (the guard
robot). Put them in Story AIs; any number per map. They use the game's own
robot behaviour and animations; the preview shows their Arcade look-alikes.
The original game plays such a map with the same robots but shows an empty
name in the editor. *Automated tests only.* Details:
[story assets](mapmaker-story-assets.md) item 18.

### Story props: power cells and prison guns (`LIMITS`)

Select a placed **explosive drum** and open its options (as for a pickup):
**Type: Power cell (big explosion)** turns it into story mission 1's blue
power cell, whose blast is about twice as wide. A placed **fixed gun** has a
new **Model** choice after its weapon: **Prison gun (mission 1)** is the
intro's big manned turret. Place as many as you like; the choice is saved
with the map and kept by .tsmap export. In the editor they still look like a
drum and a MapMaker gun. The original game (or this one without the switch)
plays them as plain drums and guns. *Automated tests only.* Details:
[story props](mapmaker-storyprops.md).

### Clouds per map (`LIMITS`)

**Map Settings** gains a **Clouds** checkbox beside the sky choice. Clouds are
shown only in play, not in the editor. A map saved with clouds loads normally
without the switch, just without clouds. *Automated tests only.*

### Open-sky tiles (`LIMITS`)

The last tile category of every tileset ends with two extra 1x1 tiles,
numbered after the stock 45: **46. Small Open (No Roof)** (the Small Open
tile without its ceiling, so you see the sky) and **47. Small Open (Open
Air)** (floor only: no ceiling and no walls). The name shows under the
palette preview, in the label over a placed tile ("46.Small Open (No Roof)",
like the stock "1.Small Open") and when the placed tile is selected. The
editor never draws ceilings, so No Roof looks like Small Open there; Open Air
shows its walls gone. Placed, moved and copied tiles keep their type, also
after playing the map and returning to the editor, and a saved map reloads
them. In play the open tiles show the sky
and let weather in. Hidden walls and ceilings have no collision: rockets leave
through the top, and you can walk out of an Open Air tile's sides, so keep it
away from the map edge. The original game plays the map open too, but
re-saving it there turns them back into Small Open. *Automated tests only.*
`TSFP_MAPMAKER_ROOM_MASK_CLEAR` stays a developer test switch. Details:
[open sky](mapmaker-sky.md).

### Sixth tileset: Abstract (`LIMITS`)

**Map Settings → Tileset** gains a sixth choice, **Abstract**, after Virtual.
It is the plain look the original editor uses for its piece previews; the
developers never offered it, but every tile, pad and item model for it is in
the game files. It shares Lab's music and most of Lab's props (with its own
keycards). Playing an Abstract map loads the editor's game file as well,
which needs no switch; if that is not possible the map plays as Lab and the
log says so. Abstract maps save in the fork's format, so the original game
and System Link refuse them. Its tile textures (in another game file) now load
in play too. *Automated tests only.* Details:
[tilesets](mapmaker-tilesets.md).

### Disco tileset (`LIMITS`)

**Map Settings → Tileset** also offers **Disco**, right after Virtual: Virtual's
rooms dressed in the Disco arcade level's own textures (starry floor, gold
wallpaper, red plush, leather, tiger print), read from your game files. The
editor reloads the tiles when you switch. With Music and Sky on *Recommended*
a Disco map plays the Disco track under a dark sky. It is saved as a Virtual
map with a Disco mark, so the original game, older builds and System Link
peers without this build simply see Virtual. If the Disco game file is missing it shows plain
Virtual and the log says so. *Automated tests only.* Details:
[Disco variant](mapmaker-disco-variant.md).

## Mouse controls

These are on without any switch:

- **Editor:** the pointer follows the mouse; click menu rows, drag scrollbars,
  use the wheel to scroll lists or zoom the view, right-drag or push the window
  edge to pan, right-click empty ground to clear the selection, and
  right-click or Escape to close an open list (the lighting list too). A/D
  switch toolbar categories while a menu is open. Checked in game;
  `TSFP_MAPMAKER_POINTER=0` turns it off.
- **Map browser:** click a map in **MapMaker → Load map** to highlight it,
  click it again to load it.
- **Character screen:** after previewing a character, a left click anywhere
  other than another portrait confirms it.
- **Mounted and vehicle guns:** the mouse aims seat and mounted guns, such as
  the truck gun in *Scotland the Brave*, within the gun's own limits.

Planned, not yet merged: mouse-wheel zoom for weapon scopes.

## Bug fixes (always on)

- System Link: malformed map fragments and turret or death-effect messages that
  point outside their tables are dropped instead of corrupting memory.
- A search in level setup that could overwrite its own return address now
  stops one entry earlier.
- Map files are checked for out-of-range data before loading. The editor
  refuses a damaged map the way the original does; level setup skips the
  damaged parts.
- Arcade bots no longer read a goal from outside their table.
- With `LIMITS`, the Story HUD shows icons for every live turret.

Details: [mapmaker-bugfix.md](mapmaker-bugfix.md).

## Compatibility and known limits

- Maps within the original limits are saved in the original format and work in
  other builds. Larger maps need this fork and are refused for System Link,
  which can only send original-size maps. Maps using the Abstract tileset
  always need this fork.
- More than 32 characters and more than 30 AI only happen in a MapMaker map
  played locally in Story. Arcade, System Link and campaign levels keep the
  original numbers.
- Tiles stay capped at 255 even on the 80x80 grid.
- Very large maps can be slow on weaker PCs; lower `TSFP_MAPMAKER_ALIVE` or
  `TSFP_MAPMAKER_MEMORY_SCALE` if needed.
- Untested: System Link, splitscreen, Mac, story levels after the first, and
  save/load of maps larger than 64 KB.
- `LIMITS`, `OVERLAP`, `STACK_ITEMS` and `ALIVE` are in the launcher's
  **Settings → MapMaker** tab (Windows checked by compiling only; Mac not run).
  They are not in the in-game settings. Other switches are environment
  variables for source runs only.
