# MapMaker investigation (phase 1: reconnaissance)

Date: 2026-10-22. Branch `mapmaker-recon` from main `141f440`; runtime
`d90c929` with the full patch stack. No game data, generated guest C or game
build was available on the reconnaissance host, so **no MapMaker routine has
been disassembled yet**. Everything about MapMaker internals below is either an
address already proven by earlier project work, or a labelled hypothesis.

Evidence labels:

- **CONFIRMED**: proven in this repository's code, tests or recorded runtime
  evidence (cited).
- **COMMUNITY**: public player reports or guides (cited URL); not verified on
  this build.
- **HYPOTHESIS**: our inference; includes the check that would settle it.

## 1. Known MapMaker anchors in the US Xbox build

| Fact | Evidence | Label |
| --- | --- | --- |
| MapMaker's Arcade table entry uses **scene 100** (most likely playing a map; unverified). The current scene is the int32 at `0x0079094C`; scenes 0..100 are active play, >100 is frontend. | `src/pc_discord_guest.h:49-53, 128`; `src/pc_menu_mouse_guest.inc:51-53` | CONFIRMED (Discord presence verified live, `STATUS.md` ~11430-11460) |
| The MapMaker **editor** runs as **scene 102**. | First Windows meter-probe run (2026-10-22): all 634 meter draws reported `scene=102` | CONFIRMED (one run) |
| MapMaker is a row in the 17-row Arcade map table at `0x004D2390` (stride 0x3C, scene id at +0, name id at −8, network capacity at +0xC+col*4). | `src/pc_discord_guest.h:119-133` | CONFIRMED |
| `sub_0030C210` draws the noninteractive **MapMaker capacity meter** using the shared bar primitive `sub_000814B0`. | `STATUS.md:2552-2556` (US audit of the four original callers of `sub_000814B0`); `src/pc_menu_sliders.inc:55` | CONFIRMED |
| `sub_000814B0` takes 9 stack arguments: fill fraction (float), x, y, w, h, colours. It is already wrapped by `pc_display_menu_slider_bar`, which acts only for return address `0x002EA3E0` (sensitivity). | `patches/*-menu-setting-sliders.patch:17-23`; `src/pc_menu_sliders.inc:58-69`; `src/pc_display_menu.c:547`; `tests/fov_settings.c:98` | CONFIRMED |
| MapMaker tileset textures live in archives whose path contains `/tileset/`. | `tools/texture-inventory.py:45-46`; `STATUS.md:8397` | CONFIRMED |
| MapMaker map naming goes through the shared on-screen keyboard (`0x0007DB20` character rows), so physical typing already reaches it. | `docs/agent-context/systems.md` (#136); `MAC_PORT_BACKLOG.md:3085` | CONFIRMED for the shared keyboard; MapMaker name dialog specifically pending validation |
| MapMaker controls and mouse navigation have **never been validated** on PC. | `STATUS.md:6862`; `MAC_PORT_BACKLOG.md:3125` | CONFIRMED (as a gap) |

Not found anywhere in the repository or upstream: editor state structures,
cursor/grid position, piece arrays, budget counters, the MapMaker save routine,
or any MapMaker symbol name.

## 2. Public description of the editor (COMMUNITY)

From the Future Perfect MapMaker guide
(<https://timesplitters-world.de/tsfp/mapmaker/TSFP_MapMaker_Swatt.pdf>) and the
fan wiki (<https://timesplitters.fandom.com/wiki/TimeSplitters_Future_Perfect_Mapmaker>,
<https://timesplitters.fandom.com/wiki/Mapmaker_Glitches>):

- Editing categories: Tile, Item, Light (FIXED/FLICKER/SWITCH/PULSE), Story AI,
  Logic (Triggers, Actions, Objectives, Awards; up to five Actions per
  operation), Map Settings (theme, light colour, weapon/bot sets, music, sky,
  disabled modes, briefing).
- Five tilesets (Egyptian, Horror, Military Bunker, Metal Lab, Virtual), 30+
  tiles, stackable and special tiles, up to four vehicles.
- Tools: MARK (multi-select), grab/move, rotate, copy, Undo/Redo, Center Map,
  beginner/advanced mode, grid intensity, in-game Controls readout.
- Grid reportedly has 40 rows; normal tile connection on floors 1-6; a known
  glitch can push tiles to a 7th floor, suggesting the stored vertical extent is
  larger than the UI allows.
- Xbox allows 10 bots in a MapMaker map versus 7 on PS2/GameCube; Xbox map names
  ~27 characters.
- Maps could be shared over Xbox Live (map rating/download; service ended 2010).

No public source documents the Xbox button mapping, the save format, or any
decompiled MapMaker symbol. The in-game Controls screen is the cheapest
authoritative source for the mapping.

## 3. Input path inside MapMaker

CONFIRMED by code:

- **Superseded 2026-10-22:** the editor is scene 102, so `menu_mouse_context()`
  is true there and the PC layer is in frontend menu mode while editing (pointer
  shown; only generic menu rows clickable). The original analysis below
  applies to scene 100 only.
- `menu_mouse_context()` is false for scene 100 unless the player-1 pause flag
  (`*0x00761C48` bit 0) is set (`src/pc_menu_mouse_guest.inc:49-54`). So while
  editing, the PC layer is in **gameplay mode**: the mouse is captured, left
  button = Fire binding (right trigger), right button = right-thumb click,
  wheel = D-pad left/right (`config/pc-controls.default.json`).
- Mouse movement only changes camera yaw/pitch of the player at `*0x007B0C7C`,
  applied inside `sub_000D2B80` (`patches/pc-controls.patch:229`). It never
  produces right-stick values.
- Keyboard bindings produce a normal Xbox pad, so the editor is already usable
  as "keyboard as a controller".

HYPOTHESES (each needs one bounded Windows run with the disposable unlocked profile):

| # | Hypothesis | Discriminating check |
| --- | --- | --- |
| H-IN1 | The editor cursor is driven by the left/right sticks read through `XInputGetState`; mouse movement currently has no effect in the editor. | Enter MapMaker; move mouse only; observe cursor/camera. `RECOMP_INPUT_LOG` shows pad reads continue. |
| H-IN2 | The editor does not call `sub_000D2B80` (player movement) or has no valid `*0x007B0C7C` player, so mouse-look is inert. | Log one line from the existing `sub_000D2B80` wrapper path when scene==100 (debug build only), or check with `RECOMP_TRACE_ONLY`. |
| H-IN3 | Editor pop-up menus (item lists, logic editor, pause) use the generic menu engine (`sub_00073750`/`sub_0007A7B0`) and would get mouse support if the context test admitted them. | With the pause flag set inside MapMaker, verify the menu-mouse hit test already works; then trace whether editor lists go through `sub_00073750`. |
| H-IN4 | The editor cursor/grid position is a small integer/float struct reachable from a MapMaker global; writing it directly (like the terminal pointer) is feasible. | Find writers of the cursor value with `RECOMP_WATCH_WRITE` once a candidate address is found by memory diffing while moving the cursor one cell. |

## 4. Capacity meter

See `research/capacity-constraints.md`. The single solid lead is the
`sub_0030C210 → sub_000814B0` call: the fill fraction argument must be computed
from the budget counters.

## 5. Serialization

CONFIRMED: saves are plain host files under `RECOMP_SAVE_DIR/UserData/45410066/`
written by the game's own lifted XAPI code; there is no FATX or quota layer
(`docs/architecture.md` §6).

HYPOTHESES:

| # | Hypothesis | Check |
| --- | --- | --- |
| H-SV1 | Each saved map is its own XAPI save directory beside the profile directory, not part of `tsftrpft`. | Disposable save root: snapshot `UserData/45410066/`, save one map, diff. |
| H-SV2 | A map file is a fixed-size image of the editor's in-memory arrays, so the stored capacity equals the budget limits. | Save an empty map and a full map; compare sizes. Community attachments of ~4 KiB to ~100 KiB are inconclusive (unknown platform). |
| H-SV3 | Map files carry an HMAC with the runtime's zero key like profiles, so an external editor must re-sign. | Inspect the save tail after H-SV1; compare with `tools/testing_profile.py` signing. |
| H-SV4 | The save routine is reachable from guest file-create calls on `U:` during "Save Map". | `RECOMP_TRACE_ONLY`/file logging around `NtCreateFile` during a save; take the guest caller chain. |

## 6. Reverse-engineering plan for phase 2 (needs Windows host + local XBE)

1. Offline: in the local generated C (never committed or transmitted), find
   every caller of `sub_0030C210` and the code computing the float passed as
   `sub_000814B0`'s first argument. Record the globals and struct offsets it
   reads.
2. Offline: lift `sub_0030C210` and its callers with the existing
   original-routine harness pattern (`tests/test_crane_overlay.py:21-48`).
3. Runtime, read-only: log the meter fraction and candidate counters in
   MapMaker while placing tiles/items (see roadmap experiment E1).
4. Runtime: `RECOMP_WATCH_WRITE` on the counters to find the placement and
   limit-check routines.
5. Repeat 1-4 for the cursor (H-IN4) and the save routine (H-SV4).

Record each confirmed address here with the command or test that proved it.
