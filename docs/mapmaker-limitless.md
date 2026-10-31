# MapMaker Limitless: player guide

[Back to the README](../README.md)

TimeSplitters 3 Limitless is a fork of the
[original PC port](https://github.com/TheDoffman/timesplitters-future-perfect-pc).
This guide covers what the fork adds to MapMaker, how to switch it on, and
what has and has not been tested. Technical notes for each area are in the
`docs/mapmaker-*.md` files; measured test results are in [STATUS.md](../STATUS.md).

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

1. Build the game from source with the [build guide](building.md). You need
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
| `TSFP_MAPMAKER_LIMITS=1` | Off | The main switch: the raised editor limits below, seven floors and the 80x80 grid, any unlocked character in MapMaker Story, Story AI duplicates and linked copies, and the Clouds checkbox. | See each feature below |
| `TSFP_MAPMAKER_OVERLAP=1` | Off | Tiles can be placed over other tiles. | Checked in game (2026-10-31) |
| `TSFP_MAPMAKER_STACK_ITEMS=1` | Off | Several items can share one grid cell. | Checked in game (2026-10-31) |
| `TSFP_MAPMAKER_MEMORY_SCALE=2..8` | 8 with `LIMITS` | How far the editor's memory estimate is relaxed. Lower it if large maps run short of memory. | Used in the capacity play test |
| `TSFP_MAPMAKER_ALIVE=1..64` | 64 | Most characters alive at once in a local MapMaker Story map. 32 or 24 can help a slow PC; 20 matches the original game. | Automated tests only |
| `TSFP_MAPMAKER_POINTER=0` | Mouse on | Turns the editor mouse controls off. | Editor mouse checked in game |
| `TSFP_MAPMAKER_POOLS=0` | Pools on | Restores the original AI navigation pools in MapMaker play. This also turns `LIMITS` off. Troubleshooting only. | - |
| `TSFP_MAPMAKER_ROOM_MASK_CLEAR=<hex>` | Off | Developer test for open-air (sky) tiles. Not a player feature. | Work in progress |
| `TSFP_MAPMAKER_PROBE=<flags>` | Off | Read-only diagnostic log; see the [probe guide](mapmaker-meter-probe.md). | Developer use |

## Raised limits (`TSFP_MAPMAKER_LIMITS=1`)

| Limit | Original | Limitless |
| --- | --- | --- |
| Tiles (pieces) | 200 | 241 |
| Items | 50 | 200 |
| Keycards | 2 per colour | 8 per colour |
| Pickups (weapons, ammo, health, armour, power-ups) | original per-type maxima | x4 |
| Movable objects and props | 20 | 80 |
| Turrets | 8 per type | 24 per type |
| Teleporters | 2 per colour | 8 per colour |
| Story AI | 50 | 255 |
| Characters alive at once (MapMaker Story, local play) | 32, with about 20 AI | 64 (63 AI with one player) |
| Logic operations / nodes / records | 30 / 130 / 100 | 120 / 520 / 400 |
| Free-standing objects | 40 / 30 / 100 | 160 / 120 / 400 |
| Pads / pad links / pad groups | 2400 / 4400 / 300 | 4800 / 8800 / 600 |
| Map file | 32 KB | 512 KB |
| Memory budget meter | 1x | 8x |

**Tested (STATUS.md, 2026-10-30):** editor and play on maps with 236 tiles,
200 items and more than 102 Story AI, up to 64 characters in MapMaker Story,
team deathmatch and the first story mission. No crashes; at least 5.56 MB of
memory stayed free. Not tested: System Link, splitscreen, Mac, other story
levels.

The extra play-time capacity (more characters alive, larger object pools)
is always present in MapMaker play; the switch decides how much the editor
lets you place.

## New editor features

### Seven floors and an 80x80 grid (`LIMITS`)

The floor menu has seven floors instead of five, and the grid is 80 x 80 cells
instead of 40 x 40. Old maps open where they were. Pieces placed beyond the
original grid still play, but the original editor cannot select them.
*Checked in game (2026-10-31).*

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

### Clouds per map (`LIMITS`)

**Map Settings** gains a **Clouds** checkbox beside the sky choice. Clouds are
shown only in play, not in the editor. A map saved with clouds loads normally
without the switch, just without clouds. *Automated tests only.*

### Open-air tiles (in progress)

Making indoor tiles open to the sky is being investigated with the developer
switch `TSFP_MAPMAKER_ROOM_MASK_CLEAR`. It is not a finished feature.

## Mouse controls

These are on without any switch:

- **Editor:** the pointer follows the mouse; click menu rows, drag scrollbars,
  use the wheel to scroll lists or zoom the view, right-drag or push the window
  edge to pan, right-click empty ground to clear the selection, and Escape to
  close menus. A/D switch toolbar categories while a menu is open. Checked in
  game; `TSFP_MAPMAKER_POINTER=0` turns it off.
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
  which can only send original-size maps.
- More than 32 characters and more than 30 AI only happen in a MapMaker map
  played locally in Story. Arcade, System Link and campaign levels keep the
  original numbers.
- Tiles stay capped at 241 even on the 80x80 grid.
- Very large maps can be slow on weaker PCs; lower `TSFP_MAPMAKER_ALIVE` or
  `TSFP_MAPMAKER_MEMORY_SCALE` if needed.
- Untested: System Link, splitscreen, Mac, story levels after the first, and
  save/load of maps larger than 64 KB.
- `LIMITS`, `OVERLAP`, `STACK_ITEMS` and `ALIVE` are in the launcher's
  **Settings → MapMaker** tab (Windows checked by compiling only; Mac not run).
  They are not in the in-game settings. Other switches are environment
  variables for source runs only.
