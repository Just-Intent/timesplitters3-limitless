# MapMaker Story: choose any unlocked character

With `TSFP_MAPMAKER_LIMITS=1`, a MapMaker map played in Story offers every
character the profile has unlocked on the character select screen (the Arcade
roster) instead of one Cortez portrait per player. The chosen character and
variant are used in play by each local player and remembered in the profile
as Arcade does. Without the switch the game behaves as stock.

## Stock behaviour (US 45410066)

Editor Play (`sub_002C8400`) sets the mode `[0x0078AD2C] = 9` and opens the
game-type menu; its callback `sub_002D6190` writes the sub-mode
`[0x0078ADAE]` (2 = Story) and, for Story, opens the character select
container. Four branches then test `mode == 9 && sub-mode == 2`:

| id | function | site | stock effect |
|---|---|---|---|
| story-char-roster | `sub_002C6C80` roster builder | 0x002C6D1F | one fixed entry, id 0xC, per player |
| story-char-preview | `sub_002C6680` carousel init | 0x002C672E | cursor on Cortez |
| story-char-confirm | `sub_002D4D00` carousel confirm | 0x002D5414 | choice not stored in profile +0x60/+0x64 or player record 0x777344 |
| story-char-config | `sub_002D2C70` config builder | 0x002D359D | character 0xC for every local player, variant -1 |

The play code (scene 100) reads the character and variant from the active
game config (`+0x14`/`+0x44`) in every mode (`sub_000D2FD0`, `sub_000CF230`,
`sub_000D50E0`), so nothing in play needs changing.

## Change

`config/guest-code-patches/story-character.json` rewrites the four
`cmp byte [0x0078ADAE],2` (`80 3D AE AD 78 00 02`) to compare the gate byte
0x00903500 instead (same length). `src/pc_mapmaker_story_character.inc`
wraps the four functions and sets the gate before each runs: the sub-mode byte
when the switch is off, 0 when it is on. With 0 every site takes the Arcade
path: the roster of characters `sub_001CD980` reports unlocked for player 0's
profile, the cursor on the profile's last choice, the choice stored in the
profile and the config filled from `sub_00069620(i)+0x60/+0x64`.

Within `sub_002D2C70` the sub-mode is rewritten only from `sub_002D2C10`,
which returns the same byte in mode 9, so the gate set on entry is still
current at the config site. The other three functions do not write it.

Every site tests mode 9 first: campaign story levels (their own branch of
`sub_002D2C70`), Arcade Custom (mode 5), challenges/league (6/7) and System
Link MapMaker (11..13) are unchanged. Other `cmp [0x0078ADAE],2` sites
(0x002D31D8, 0x002D326C flags; 0x002D64E3 Back navigation) are left alone;
`tests/test_mapmaker_story_character.py` keeps that inventory complete.

The gate is 0 until a hook sets it, so the hooks must always run: the area
`story` ignores `TSFP_MAPMAKER_AREA_OFF` like `container` and `actors`.

Not changed: in story mode `sub_000D76D0` loads a full voice set only for the
characters the story table names for the scene (`sub_00064DD0`) and half a
set for others; stock Cortez in a MapMaker map is in the same position, so
the chosen character still has voices. Hook `sub_00064DD0` only if testing
shows missing lines.

## Save compatibility

No map format change. The profile gets the same character/variant fields
Arcade writes, always an unlocked character, so stock builds read it as usual.

## Test

See the test steps in the commit/PR. Unverified in play: HUD/pause portraits,
end-of-map screens and story interactions with small characters.
