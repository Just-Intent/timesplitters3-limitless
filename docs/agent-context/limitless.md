# TimeSplitters 3 Limitless (fork) systems

<!-- packet:start -->
Fork-only systems (TimeSplitters 3 Limitless). MapMaker capacity areas are C
policy in `src/interface/pc_mapmaker_*.inc`, included by
`src/interface/pc_menu_mouse_guest.inc`; their guest wrappers are declared in
`src/interface/pc_display_menu.h` ("capacity area hooks") and supplied by
`patches/graphics/interface/*-mapmaker-meter-wrappers.patch` (with -probe,
-raise, -stagea beside it; basenames keep their z-prefix order after main's
numbered stack). Verified guest-code tables live in `config/guest-code-patches/`
and are applied by the project translator (`tools/compiler/guest_code_patches.py`
via `translation_batch`/`translation_decode`, `--guest-patches` from `tsfp.ps1`
and `tools/build/build-native-mac.py`); any entry that does not verify stops
the lift. Relocated tables live in the guest extension block 0x008A0000/0x80000
reserved by `src/runtime/guest_image.c` (`tsfp_image_set_extension`) through
`src/runtime/guest_extension.h`: Windows via
`patches/platform/*-guest-memory-extension.patch`, Mac in `src/native/main.c`.
Switches are `TSFP_MAPMAKER_*` (default off except pools/mouse controls).
Tests: `tests/mapmaker/`, `tests/compiler/test_guest_code_patches.py`,
`tests/input/test_scope_wheel.py`. Mac follow-up: MAC-131 and later.
<!-- packet:end -->

Fork-specific subsystem notes, kept out of the shared size-capped guides so
upstream merges stay clean. Each part names the shared guide it extends.
MapMaker player/contract docs live in `docs/mapmaker/`; measured results in
`docs/project/STATUS.md`; Mac follow-up in `docs/project/MAC_PORT_BACKLOG.md`
(MAC-131 and later).

## Subsystem map rows (extends [systems.md](systems.md))

| Area | Files | Tests |
| --- | --- | --- |
| Mouse/menu handling | `src/interface/pc_menu_mouse_guest.inc`, `pc_menu_choices.inc`, `pc_menu_sliders.inc`, `pc_menu_team.inc`, `pc_menu_mapmaker_browser.inc`, `pc_menu_briefing.inc` | `tests/input/test_menu_mouse.py`, `test_terminal_mouse.py`, `test_ui_layout.py` |
| Mounted / vehicle gun mouse aim | `src/input/pc_mounted.h` (called from both adapters' `pc_controls_apply_mouse` after the terminal route, before ordinary look) | `tests/gameplay/test_mounted_mouse.py` |
| Mouse-wheel scope zoom | `src/input/pc_scope_wheel.h` (state from both adapters' `pc_controls_apply_mouse`; wheel and key filters in their wheel/key paths), `pc_settings_bound_keys` in `pc_bindings.inc` | `tests/input/test_scope_wheel.py` |
| MapMaker capacity areas | `src/interface/pc_mapmaker_*.inc` (included by `pc_menu_mouse_guest.inc`), `config/guest-code-patches/`, `tools/compiler/guest_code_patches.py`, `src/runtime/guest_extension.h` | `tests/mapmaker/`, `tests/compiler/test_guest_code_patches.py` |
| Guest lift/runtime correctness | Runtime `tools/recomp/`, manual hooks, `tools/build/recomp-generation.py`, verified guest-code patches `tools/compiler/guest_code_patches.py` | `tests/compiler/test_recomp_float_joins.py`, `test_recomp_vehicle_joins.py` |

Issue/Ideas routing (extends [build-test.md](build-test.md)): the issue chooser's
Ideas contact link: the fork's `Just-Intent/timesplitters3-limitless`
Discussions since 2026-10-31.

## Systems (extends [systems.md](systems.md))

### Mounted and vehicle gun mouse aim

The fork's mode-0x400 vehicle-gunner port (Scotland the Brave truck gun,
user-confirmed in game) is superseded by main's shared `src/input/pc_mounted.h`,
which carries the same 0x80/0x400 routing plus the 0x8000 Battle Mech route,
guest-region spans and the native-rate clock; see "Mounted and CCTV gun mouse
aim" below and `tests/gameplay/test_mounted_mouse.py`.

### Mouse-wheel scope zoom

Request: wheel zooms a zoomable scope and must not change weapon while scoped;
iron sights/plain aim keep wheel switching. Guest evidence (confirmed from the
disassembly control flow): `sub_0025E650` (per-player aim/zoom update, called
from `sub_000D5EB0`; `[0x007B0C7C]` player, `[0x007B0CC0]` its camera, which is
also player `+0x08`) toggles aim bit `0x20` in player `+0x0C` from aim mask
`+0x28`. When aim starts it sets bit `0x40` only if `sub_001B7DF0(player+0x55C)`
reports a scope sight (weapon-table `[0x004F9BAC]` entry flags `0x20`, or `0x40`
with weapon mode `+0x38 == 1`); `sub_00259DF0`/the exit path clear it and it
drives camera `+0x21C` bit `0x80`. While aiming, held masks `+0x30/+0x34`
(D-pad up/down in every control scheme, `sub_002586C0`; game bits 4/8 from the
pad table at `0x0047DF00`) move zoom target camera `+0x388` by 60 deg/s,
clamped to `[+0x394, +0x390]` from weapon fields `+0x60/+0x64` (`sub_001B7D00`;
`sub_001B7D70` collapses the range when the scope is unavailable). Zoom is a
held-button effect, so a notch is a short hold. Which weapon each table entry
is (entries 8, 9 and 17 carry flag `0x20` and 0.545x-0.07x ranges) is inferred.

`src/input/pc_scope_wheel.h`: `pc_scope_view` (player `+4 == 0` only, i.e. local
player 1; bits `0x60` and min < max), `pc_scope_wheel_bindings` (a wheel
direction is redirected only when bound to Previous/Next weapon and nothing
else; zoom keys are the first non-wheel keys of actions 10/11), per-notch
`PC_SCOPE_WHEEL_MS` (80 ms, about 5 degrees) holds that add up to 640 ms,
state older than 250 ms ignored, leaving the scope cancels the hold. Both
adapters read `pc_settings_bound_keys` and `pc_scope_view` before their lock in
`pc_controls_apply_mouse`, filter wheel notches (`wheel_input` /
SDL wheel event) and every polled key (`pc_controls_key_down` /
`tsfp_native_key_down`), and reset on menu/capture/focus changes. Controller
input and Q/V are unchanged. `tests/input/test_scope_wheel.py`: fixture plus ten
mutations and adapter/binding structure checks; no in-game run yet.

### MapMaker meter probe

`src/interface/pc_mapmaker_probe.inc` (included by `pc_menu_mouse_guest.inc` before the
slider code) is read-only debug instrumentation. `pc_display_mapmaker_meter`
scopes `sub_0030C210`; `mapmaker_probe_bar()` at the top of
`pc_display_menu_slider_bar` attributes bars drawn inside that scope (and,
with flag 4, other scene-100 bars). Flags come from `TSFP_MAPMAKER_PROBE`
(1 meter, 2 pointer snapshots, 4 other bars, 8 menu layouts and clicks), output
from `TSFP_MAPMAKER_PROBE_LOG` (opened in binary mode) or stderr; records are
deduplicated, rate-limited and capped at 4 MiB. Flag 8 is fed by two read-only
calls in the existing `pc_display_menu_mouse` (`sub_00073750`) wrapper: the
layout after the original runs (before its context checks, so rejected menus
are logged too), and the click after hit testing. No guest regeneration. It must never write guest memory or registers;
`tests/mapmaker/test_mapmaker_probe.py` enforces this with mutations. Usage and the
experiment protocol: `docs/mapmaker/mapmaker-meter-probe.md`. Mac: MAC-131.
Flag 128 (scene 100, 5 s windows every 30 s) logs room draws, their props and
the model draws inside them through two further wrappers,
`pc_mapmaker_probe_room_draw` (`sub_000A2900`) and
`pc_mapmaker_probe_prop_draw` (`sub_0008F950`, every model draw, inert outside
a window); it reads the per-room prop tables at their `rooms.json` locations
(`0x008C7000`/`0x008C7C00`). Once per run per geometry model and node table
it also dumps the model's node tree (`[inst+4]` header, 0xC8-byte nodes
before it; type `+0x48`, child `+0x4B`, sibling `+0x4C`, mask `+0x5C`), capped
at 96 nodes per model, 256 models and 8192 lines. See `docs/mapmaker/mapmaker-sky.md`.

Dev-only sky-tile switch `TSFP_MAPMAKER_ROOM_MASK_CLEAR=<hex>`
(`src/interface/pc_mapmaker_skymask.inc`, included after `pc_mapmaker_sky.inc`): the
`sub_00085710` wrapper `pc_mapmaker_sky_room_mask` clears those bits in the
relocated per-room mask table `0x008C0800` right after the stock store
(`0x000857A3`), scene 100 only, so `inst+0x30` (`0x00085DCA`), the camera
and the weather getter `sub_00083970` agree; editor and saved data are not
touched. `pc_mapmaker_sky_room_mask_flush()` (called from
`pc_display_menu_mouse_mode`) prints one `[mapmaker] room mask: cleared ...`
summary per map build. Independent of `TSFP_MAPMAKER_LIMITS`; the Test Lab
runner strips it with the other `TSFP_*` variables. Fixture
`tests/mapmaker/test_mapmaker_skymask.py`.

MapMaker Open roof / Open air tiles (`src/interface/pc_mapmaker_roof.inc`, area `sky`,
include after `pc_mapmaker_skymask.inc`; `TSFP_MAPMAKER_LIMITS=1`, editor
only): `pc_mapmaker_roof_menu_init` (`sub_00316B50`) appends tile 8 twice to
the last tile category (tables `0x0052DAC0`/`0x0052DB20`) for the duration of
the build and restores the table; the two items are identified by pointer
(`sub_000792B0` from `0x00316C84`). Picking one (`sub_00316500`) marks the
held editor piece (`+0x2F7C`). Placing/picking up copies the 0x48-byte
piece into a new one (`sub_003037A0`, `sub_00307EC0` + `rep movsd`), so
`sub_002F95E0(old,new)` (move) and `sub_00308120(src,dst)` (copy) pass the
mark on and `sub_00308480` (free) forgets it; without them placed tiles
were plain tile 8 (v0.4.0-mapmaker.5 bug). `sub_0003D7B0` answers tile 8's
name (text 0x0A3F) at `0x0032E5D4` (palette caption) / `0x0031171D` (grid
label: piece `+0x2F18`, else held) / `0x00317C60` (selected piece) with
"Small Open (No Roof)"/"(Open Air)"; `pc_mapmaker_roof_caption`
(`sub_0032D9F0` from `0x0032E5DE`, eax) and `pc_mapmaker_roof_format`
(`sub_00151030` from `0x00311724`, "%d.%s") make the number (catalogue +4,
stock 1..45) 46/47 for those labels. `sub_002FC680`
then drops `0x02000000` (open
roof) or `0x02FFFFFF` (open air, floor bit 24 kept) and `sub_002FC470` drops
`0x02000000`. The rebuild hook (`sub_003006C0`) stores record word `+0x26` =
`0x5201`/`0x5202` and the open `+8` (any scene); records are 0x28 + pads*8
(`+0x24`) + items (`+0x1E`, `(byte +0x14*4+0x1B)&~3` each): skipping only
`+0x24` lost the marks after a start point, so the editor reload after a play
(`sub_00302A60` -> `sub_00301A90` at `0x00303076`, new pointers) closed every
tile (v0.4.0-mapmaker.7 bug); play reads `+8` as is (`0x0008A91D`, no
recompute); the load hook (`sub_00301A90`: play return, Load map, undo)
matches marks back to pieces; `sub_00307FD0` and `sub_002FEBF0` forget marks. The map
stays a stock map (tile 8 + mask), so a stock game plays it open. Notes, WRAP
lines and test steps: `docs/mapmaker/mapmaker-sky.md` "Open-air tiles". Fixture
`tests/mapmaker/test_mapmaker_roof.py`.

### MapMaker level memory budget (opt-in)

`TSFP_MAPMAKER_MEMORY_SCALE=N` (2..8; anything else = off; 8 when
`TSFP_MAPMAKER_LIMITS=1` and no scale is set) divides the
results of `sub_002FA060` (placed pieces) and `sub_002FA0D0` (held piece),
the editor's play-time memory estimate checked against 2,048,000 bytes in
`sub_002FA720` and shown by the meter. Wrappers in
`patches/*-mapmaker-meter-raise.patch` (needs guest regeneration once);
policy `src/interface/pc_mapmaker_budget.inc` changes only `g_eax`. Tests:
`tests/mapmaker/test_mapmaker_budget.py`. Basis and limits:
`research/capacity-constraints.md` §1b.

### MapMaker capacity stage A (pools on by default, limits opt-in)

`src/interface/pc_mapmaker_limits.inc` via `patches/*-mapmaker-meter-stagea.patch`:
`sub_0009A140` is reimplemented in scene 100 with enlarged AI navigation
pools (`TSFP_MAPMAKER_POOLS=0` = original); with `TSFP_MAPMAKER_LIMITS=1`
(ignored while pools are off) `sub_002FA720`'s decision, meter and preview
are recomputed and the category maxima `0x00524BE0` raised; current limits
and the resource table that replaces the meter (`pc_mapmaker_restable.inc`):
[caps](limitless-caps.md). Item node pool: `config/guest-code-patches/`.
Tests: `tests/mapmaker/test_mapmaker_limits.py`. Probe flag 64. Basis:
`research/capacity-constraints.md` §1c.

### MapMaker capacity areas

Each area of the limit raise (research/capacity-raise-plan.md) is a guest-code
table `config/guest-code-patches/<area>.json`, C policy
`src/pc_mapmaker_<area>.inc` (included from `pc_menu_mouse_guest.inc` after
`pc_mapmaker_limits.inc`), a fixture `tests/test_mapmaker_<area>.py` and notes
`docs/mapmaker-<area>.md`. Wrappers for every area are in
`patches/*-mapmaker-meter-wrappers.patch`; declarations sit between the
"capacity area hooks" markers in `src/interface/pc_display_menu.h`, no-op stubs between
the "capacity area stubs" markers in `pc_menu_mouse_guest.inc`. Fixtures that
compile the whole include chain take link stubs from
`tests/mapmaker/mapmaker_area_stubs.h`. Areas: `pads` (cover handle range 10,240,
runtime pad handles 0x4740.., bounded A*/stitch, pad/link/object/memory caps: [caps](limitless-caps.md), group A* pool relocated to
0x008E0000), `container` (map container 512 KB in the runtime heap, tiered
saves stock-identical when small, large maps refused for System Link),
`objects` (2047 `handles.json`, 768 pickups, 4096 props, 2048 bodies, pool guard: [caps](limitless-caps.md)), `editor`, `rooms` (1023, `level.json` frame, word buckets, stride 0x5000, tree: [rooms](limitless-rooms.md)) and `actors` (64 characters / 63 AI
in a local MapMaker story map instead of 20, 72 object slots kept free; live list moved to
0x00900100, AI table to 0x00900200, per-id table 0x00739AA0 to 0x00900300 by
`actors.json` + generated `actors-list.json`; 64-slot actor pool in scene 100;
perception slice 0xC00 shared with `objects.json`; 19 hooks incl. 32-entry
stack-array AI pickers (`sub_0027BA40` in two passes), a side table at
0x00900600 for kill words of victims 32..63, radar blips moved to 0x00902100
(256, bounded), radar loop 64, per-death effect lists at 64 (0x00903400/0x00903200; stock 30 outside wide play), and the patrol table 0x00520C38 read
from a copy at 0x00901600 (ids >= 32: no route); `pc_network_bots_revive`
writes the moved AI table; `TSFP_MAPMAKER_ALIVE=1..64` lowers it; sites, risks:
`docs/mapmaker/mapmaker-actors.md`), and `grid` (editor only: with
`TSFP_MAPMAKER_LIMITS=1` floors -2..13, an 80x80x17 grid ([floors](limitless-floors.md)), the five
grid functions `sub_00307C20/CC0/CF0/DA0/E60` reimplemented, a placement
guard above the top level; `TSFP_MAPMAKER_OVERLAP=1` overlapping tiles and
`TSFP_MAPMAKER_STACK_ITEMS=1` several items per cell (hooks on
`sub_00308260`/`sub_00334770`, and `sub_003348C0` at return 0x003070A7 sets
the per-frame valid bit editor `+0x25C`&4 that the item-under-cursor test at
0x003070B4 otherwise leaves clear = red X; pickups, props and teleporters
(categories 14-18) share with each other and with Story AI points, several
Story AI share a cell: `sub_00334770` also hides other AIs' points from its
refusal 0x0033484F..0x00334893, and `sub_002F8B40` at return 0x0030723F
answers -1 while a Story AI is held, which the valid bit 0x003072B4 and the
drop 0x00305238 require; starts, flags, turrets, movers and markers are never
hidden), both env-only; in play `pc_mapmaker_objects_teleporters` skips the
pad -> pad nav link of a stacked teleporter pair; a
16-byte settings block at 0x00903600 (`src/interface/pc_mapmaker_grid.h`) that
`grid.json` redirects the editor's 40.0/5.0/60.0/(1/60) reads to; save format
unchanged; `docs/mapmaker/mapmaker-grid.md`).
Editor capacity follows `TSFP_MAPMAKER_LIMITS=1`.

Round 3 (2026-10-31, `docs/mapmaker/mapmaker-memory.md`, not built or played):
area `memory` (`src/interface/pc_mapmaker_memory.inc`, included first among the
areas): a read-only guard on the game heaps `sub_0003DFA0`/`sub_0003E0B0` that
logs `[mapmaker] heap full: ...` once per overrun event (8 per run), and the
off-by-default `TSFP_MAPMAKER_ARENA_MB=0..16` that raises the arena dword
`[0x004C0450]` before `sub_00023930` reads it (trial block from the
contiguous window first; stock kept if it fails). Probe flag 32 now adds the
host window/runtime-heap peaks from the allocation observer
(`pc_capture_allocation_usage`, `src/diagnostics/pc_breadcrumbs.h`, fed in
`pc_crash.c` and `src/native/crash_posix.c`). Raises: items 500
(`MAPMAKER_LIMIT_SUB`, node pool 5300 in `items.json`), keycards 16 per colour,
movers/props/markers 160 (category table), Story AI 511: `editor.json` moves
the Story AI array to the end of the editor object (+0x58390, object
0x74C00), `sai-index.json` widens the selection index `[0x00762F94]` to a
word (11 same-length rewrites, no wrapper), the editor menu item pool is
6144. Saves: `mapmaker_container_over_stock` forces the 64 KB tier for any
map past a stock count (chunks, or the editor's counts in scene 102), so
stock builds refuse it. Decoder checks:
`tests/mapmaker/test_mapmaker_limits_classified.py`; fixtures
`tests/mapmaker/test_mapmaker_memory.py`.

risks in `docs/mapmaker/mapmaker-actors.md`).
Editor capacity follows `TSFP_MAPMAKER_LIMITS=1`. `sky` (no table; not yet
wired) adds a per-map cloud layer with `TSFP_MAPMAKER_LIMITS=1`: a "Clouds"
checkbox after the stock sky row of Map Settings (the 11-sky choice itself is
stock, editor `+0x220` -> info `+0x10`), stored as info flag 0x40000, applied
in scene 100 by copying sky record 5's cloud fields into the map's MapMaker
sky record (58..67) around `sub_0011C9F0`. Four hooks (`sub_0011C9F0`,
`sub_00078560`, `sub_002FEBF0`, `sub_003006C0` chained after the container
hook); details and test steps in `docs/mapmaker/mapmaker-sky.md`.

### MapMaker map browser mouse

Main menu > MapMaker > Load map is an ordinary menu (descriptor `0x0052EA88`,
construct `sub_0032BEA0`, tick `sub_0032BDD0`) whose type-7 rows carry the
index of the enumerated file list `[0x00778D38]` (count `+8`, 0x6C-byte
entries at `+0xC`). Its tick loads the highlighted row when the active menu's
`+0xC4` has Confirm (0x10), but `sub_0007A7B0` runs the tick (0x0007AD76)
before the row hook `sub_00073750` (0x0007ADC5), so the shared row click was
too late. `src/interface/pc_menu_mapmaker_browser.inc`, called from
`pc_display_menu_mouse_input` after the footer test (scene 101 only), makes a
fresh left click on a row highlight it (`sub_00076EF0`) and a click on the
highlighted row Confirm (raw pad A plus `+0xC4`/`+0xC8` bit 0x10, as the
footer). Rows flagged `+0x28 & 5` or with an id past the count are ignored;
hover and the wheel keep the shared row behaviour. No patch or regeneration.
Fixture: `map_browser_tests` in `tests/input/menu_mouse.c` (mutation
`late-map-confirm`).

### Mission and event briefing click

The Story briefing (descriptor `0x005238C0`, construct `sub_002CAC30`, menu
Confirm `+0x1BC = sub_002BEAA0` -> start screen) and the Arcade League /
Challenge event briefing (`0x00523870`, construct `sub_002D5F40`, tick
`sub_002C9A80`, Confirm `sub_002D5F10` -> character carousel) hold only
display items (item flag 4), so the shared row click hit nothing. With no
highlighted row `sub_0007A7B0` jumps from 0x0007B0C7 straight to the `+0x1BC`
call (0x0007B179); neither tick consumes Confirm (`sub_002C9A80` only reads
`+0xC4 & 0x400000`, the online leaderboard footer). `src/interface/pc_menu_briefing.inc`,
called from `pc_display_menu_mouse_input` when the map browser did not take
the click (after tabs, profile prompts and footer prompts), makes any fresh
left click on these two pages (scene 101, descriptor and callback both
matched) Confirm as raw Select 0x80 plus `+0xC4`/`+0xC8` bit 0x10. Footer
Back/leaderboard, right-click Back and the wheel text scroll are unchanged.
The lists before them (categories `0x00523840` and events `0x00523860`,
flags 0x2A00; Story missions `0x005238E0` and difficulty `0x005238D0`, flags
0) are ordinary selectable type-7 rows the shared row click already handles
(static check of the generated constructors; locked rows `+0x28 & 1` skip).
No patch or regeneration. Fixture: `briefing_tests` in
`tests/input/menu_mouse.c` (mutation `late-briefing-confirm`). Untested in game.

### Character selection mouse

`src/interface/pc_menu_characters.inc` captures the drawn portrait quads of the
carousel (`0x005237E0` child of container `0x005237F0`). The wheel and a
click on another visible portrait preview it (`sub_002C4310`). The previewed
portrait is replaced by the 3D model, so any other fresh left click (on it,
empty space or footer Select) confirms the previewed character as raw Select
(0x80); footer Back or right-click is Back (0x120). Fixture: `character_tests`
in `tests/input/menu_mouse.c` (mutation `footer-only-character-confirm`).

### MapMaker Story character choice

`story-character.json` makes the four MapMaker-Story Cortez branches
(`cmp byte [0x0078ADAE],2` after a mode-9 test: roster `sub_002C6C80`
0x002C6D1F, preview `sub_002C6680` 0x002C672E, confirm `sub_002D4D00`
0x002D5414, config `sub_002D2C70` 0x002D359D) read the gate byte 0x00903500.
`src/interface/pc_mapmaker_story_character.inc` wraps those four functions (area
`story`) and sets the gate on entry: the sub-mode byte (stock, Cortez only)
unless `TSFP_MAPMAKER_LIMITS=1`, then 0, so MapMaker Story uses the Arcade
roster of player 0's unlocked characters, starts on the profile's last
choice, stores the choice and writes it (and the variant) to the config for
each local player. Play code already takes the character from the config.
Campaign story, Arcade Custom, challenges, System Link (modes 11..13):
unchanged. The gate stays 0 if the hooks do not run, so `story` must not be
switched off by `TSFP_MAPMAKER_AREA_OFF`. Tests:
`tests/mapmaker/test_mapmaker_story_character.py`; notes `docs/mapmaker/mapmaker-story-character.md`.
Story assets: `docs/mapmaker/mapmaker-story-assets.md`.

Story AI groups (`src/interface/pc_mapmaker_aitemplates.inc`, `docs/mapmaker/mapmaker-aitemplates.md`,
`TSFP_MAPMAKER_LIMITS=1`): the Story AI edit screen (`sub_0031CB90`, list path
close `sub_0031CAC0`) gains "Duplicate Story AI" / "Add linked copy" / "Unlink"
rows and, on a group leader, a "Logic means: this AI / whole group" toggle
(added at the `sub_000784A0` call 0x0031D07E, told apart by menu+0x18C). The
first linked copy makes the AI the leader (no extra entry) and copies are named
"<leader> N". Links live in editor entry +0x50 (low word leader+1, bit 16 group
logic) and Story AI chunk word +0x32 (0x4000 format marker, 0x8000 group logic;
stock-zero, stock games load members as independent AIs). Unlink/delete of a
leader promotes its first copy. Old template-format maps (plain link to an
"All X" entry) are converted at editor load. In scene 100 logic on a leader
with group logic (or an unconverted old template) means "all members"
(`sub_0033E560`, `sub_00163780`, `sub_0033E620`, `sub_00339600`). A separate
logic-picker entry was rejected: Story AI ids are R + index + 1 and the object
ids start at R + count (0x0031DA9B). Compile/load/level-start hooks chain
outside the container/editor ones. New linked copies and duplicates start
unplaced (`ait_unplace`: +0x58 = 0, +0x64..+0xE3 zero) so the list shows the
"!" and they spawn where placed; with `TSFP_MAPMAKER_STACK_ITEMS=1` an
empty-handed pick-up on a shared cell takes the highest-index AI first
(`pc_mapmaker_stack_sai_point`, old maps with stacked copies). The editor's
menu item pool (`aitemplates-pool.json`, operand 0x00070DC3 in `sub_00070DB0`,
scene 102 branch only) is 6144 items instead of 920; WRAP `sub_000715E0` ->
`pc_mapmaker_aitemplates_menu_item` logs once when it runs out in scene 102.
Not yet built or played.

Story AI bosses (area `bosses`, `src/interface/pc_mapmaker_bosses.inc`,
`docs/mapmaker/mapmaker-bosses.md`, `TSFP_MAPMAKER_LIMITS=1`): a "Boss:
Off/On/health x2/x4/x8" row on the same edit screen (same `sub_000784A0`
site; the click cycles in place and keeps the screen open). Saved in Story
AI chunk byte +0x29 (0xB0 | level-1; decode-proven unused by the writer,
loader and every chunk walker); in the editor a C table indexed like the
entries, colour-checked, kept through delete/reset/load/compile; the close
hook (outside aitemplates) gives a Duplicate/linked copy (one entry added,
words +0x28..+0x4C equal) the source's level/display, then per AI. In scene
100 story mode, each HUD frame (wrap of the story boss bar `sub_0014CC80`,
ebx = HUD block) multiplies a Boss AI's entity health +0x1CC/+0x1D8 once
per (re)initialisation and draws up to 4 living Boss AIs (2 in split
screen, nearest first) within `TSFP_MAPMAKER_BOSS_RANGE` (default 30 units)
through `sub_00139740` (esi HUD block, edi entity) with the AI's saved name,
stacked upward by raising the int HUD margin `[0x007A614C]` 57 x
`[0x004E68F8]` per slot around each call (restored after); a stock bar (HUD
block +8 bit 0) keeps the bottom slot; the bar smoothing +0x171C/+0x1724/
+0x172C is swapped per boss, each copy's fade +0x172C lowered by HUD clock
+0x2C time x [0x00475D10] as `sub_0014B090` does for the live block (else the
bars never drained). Stronger health: WRAP `sub_00078590` ->
`pc_mapmaker_bosses_health_rows` appends Brute/Juggernaut/Titan/Colossus
(entry +0x4C values 5..8 = x2..x16 Tough Guy) to the stock Health row (text
0xC0D, values 0..4, bind at 0x0031CFD2); compile saves chunk +0x30 = 4 and
byte +0x21 = 0xA0 | 1..4 (decode-proven unused), load restores, play
multiplies health by 2^extra x the Boss factor (stock 0x0052E060 has 5
entries; Tough Guy = 20.0 in 0x004DACF4). The focus field +0xFF4 and the `sub_0014F0B0`
0x0014FAE3 meter (a different, player-drain bar) are untouched. Same file:
read-only `TSFP_MAPMAKER_CREATURE_PROBE=1` (log
`TSFP_MAPMAKER_CREATURE_PROBE_LOG`) listing story-only character model
resource slots per level and every new live character's id, AI class,
record, model and skeleton. Health display (2026-10-31): a second row
"Health display: Boss bar / Sub-boss bar" (id 0x424F5302, built only while
Boss is not Off; same close-callback mechanics) sets +0x29 bit 2 (0xB4 |
level-1; mask 0xF8). Sub-boss AIs get no bottom bar; WRAP `sub_0014D1F0` ->
`pc_mapmaker_bosses_meters` draws `sub_0013A400(actor,0,0x7F,0x6E)` before
the original for each living Sub-boss AI whose entity has no +0x268 record
(one with a record is the stock loop's). Records are not created:
`sub_0012C800` is a 17-slot bump pool reset only at 0x0014853D and +0x268
has ~100 readers. The meter sits 0.4..0.6 units above model node
[[[entity+4]+0x8C]+0x18] (+0x34 of its matrix), so it already follows the
model's skeleton; no extra scaling.

Custom character set (area `characters`, `src/interface/pc_mapmaker_characters.inc`,
`docs/mapmaker/mapmaker-characters.md`, `TSFP_MAPMAKER_LIMITS=1`): the Map
Settings picker `sub_002C1EA0` (descriptor 0x0052374C) adds an Arcade-table
row (0x00521A18, 150 x 0x30) only if `sub_001CD980` says unlocked; there is no
size or category filter. WRAP `sub_001CD980` answers 1 for rows with rule type
1..8 only for return 0x002C1F35 in scene 102 (other callers stock). WRAP
`sub_000D7780` logs (never skips) set members without an Arcade row or model
resource slot at the scene 100 set preload (returns 0x0023F33F/0x0023F37F).
No save change (chunk 0xC77668 +0x18 word id, +0x1B variant). Not yet built
or played.

Real story boss: giant trophy monster (area `realbosses`,
`src/interface/pc_mapmaker_realbosses.inc`, `docs/mapmaker/mapmaker-realbosses.md`,
`TSFP_MAPMAKER_LIMITS=1`, opt-out `TSFP_MAPMAKER_REAL_BOSS=0`). Editor: the
picker gets synthetic row id 150 (list end `sub_000781A0` ret 0x002C1FA5);
WRAPs of `sub_002C1C90` (row 150 -> slot id 0x15, variant -1), `sub_002C1E10`
and `sub_002BE600` (preview gets the Arcade trophy row; slot reload ret
0x002C054C and edit-screen name ret 0x002BE65F/0x003033C0 get 150),
`sub_002BE300` (150 -> text 0x52420015) and `sub_0003D7B0` (that id, and 0xDCC
at ret 0x0014CED5 in scene 100 -> "Giant Trophy Monster"); `sub_000A5E40` logs
editor requests of model 0x5BE. Play (scene 100 story): `sub_001C8540` mounts
`pak/stream/lv1/anim/lv1_3.pak` (0x0049D77C, `sub_0005A130`, >= 2 of 30 slots
free) when the map's custom set holds 0x15, before `sub_001CA2F0` registers
anim names; `sub_0015B830` sets spawn record class 0/0x80 (`sub_00280FA0`)
for a+0x48 == 0x15 / model 0x5BE when the pak is mounted; `sub_0014CC80`
(INSIDE bosses) hides the stock bar when that AI has the Boss flag (+0x29),
so one bar per actor. Floating meter: stock
`sub_0014D1F0` draws `sub_0013A400` (only caller 0x0014D25F, ret 0x0014D264)
over every AI-list actor whose entity has a +0x268 damage record, in scene
100 story only (0x0014D21C/0x0014D225; mission 5 is scene 1); the deerbeast
handler makes that record (0x002811A3), MapMaker AIs never do. WRAP
`sub_0013A400` -> `pc_mapmaker_realbosses_meter` skips it for noted real
bosses (char 0x15 / model 0x5BE) unless their Story AI chose the Sub-boss
bar (then the stock meter shows and the stock bar is hidden too). No save byte; the handler cannot reach
`sub_002274C0` (decode call graph). `tests/mapmaker/test_mapmaker_realbosses.py`.
Only the creature itself played (v0.4.0-mapmaker.5); the meter fix is not built.
Boss Off = no health: `pc_mapmaker_realbosses_hud` clears
the focus for the stock call for every noted real boss (Boss flag or not);
Boss bar = bosses' bar, Sub-boss = meter, Off = nothing.
Giant skeleton (mansion boss, id 0x28, model 0x5E9, picker row 151 "Giant
Skeleton", preview Arcade id 0xA6): `sub_001C8540` also mounts lv1_6.pak
(0x0049D6FC); WRAP `sub_0025F720` -> `pc_mapmaker_realbosses_level` calls the
stock `sub_00287E00` ([0x0078B1EC] 0x4C7C + [0x0078B0D0] 0x7D00, level heap;
stock only scenes 1/17) after clearing both pointers; ai_start gives one spawn
record per level class 0/0x20000; WRAPs `sub_0029FE00` -> `_princess` (direct
`sub_0029CEF0` call = tail jump), `sub_002875F0` -> `_risen` (scene-1 answer
`sub_00059DB0(ent, 0x19C)`), `sub_00287E20` -> `_throw` (action type 6: aim
with scene word 1 around the original, fire = stock `sub_000EA9C0` args +
`sub_0010EB90`, phase 2), `sub_002274C0` -> `_event` (drops ret 0x0029E068,
event 3). Dust particles (`sub_0029CE00`, scenes 1/17 only) not drawn. Not
built or played. Set gate (mapmaker.12 log: skeleton in slot 11+ not seen,
no lv1_6 mount/state): `rbs_set_scan` reads slots 1-10 (map-info +0x18) and
11-20 (sets chunk 0x00C776F5), only for set index 0x11 (stock play sites
0x00240DBB/0x0023F46A); anims and level log "set check found".
Death (mapmaker.13: bar empty, no death): message 2 from `sub_0015DB20`
goes through `sub_0028D0A0`, mansion `sub_00286D30` (state 6, anim 0x1A6)
only in scene 1, else catacombs `sub_0028C560` swallows it (-1). WRAP
`sub_0028D0A0` -> `pc_mapmaker_realbosses_message` calls the mansion one
for the boss in scene 100; event 3 stays dropped, `sub_0015DDA0` removes it.
Whole height (area `bigdraw`, `src/interface/pc_mapmaker_bigdraw.inc`, user:
skeleton cut at ~2 floors in mapmaker.11): a prop's scissor (+0x118..+0x124,
`sub_000862C0`, only caller 0x000A246B ret 0x000A2470) is the union of the
portal rects (view +0x3BC) of the visible rooms its box touches
(`sub_0009FBE0`, <= 10); only the camera's room gets the whole viewport
`[0x007E71C8]` (0x00089BD9). Tiles/portals are one floor high, so a boss in
another tile is cut at the portal top; mission 5's arena is the camera's room.
WRAP `sub_000862C0` -> `pc_mapmaker_bigdraw_rect`: character, model
0x5E9/0x5BE, scene 100, switch on, non-empty rect -> whole viewport (depth
still occludes). `tests/mapmaker/test_mapmaker_bigdraw.py`. Not built or played.
Crow walker (id 0x64, model 0x637, picker row 152 "Crow Walker", preview
Arcade 0xA8; docs section in mapmaker-realbosses.md): anims lv81.pak
(0x0049D9A8); level WRAP calls stock `sub_002976F0` ([0x0078B1E8] 0x64DC)
before the skeleton's; ai_start class 0/0x100000 (`sub_002998E0`, one per
map); death needs no hook (`sub_0029CBF0`, any scene). New WRAPs:
`sub_0023FF20` -> `pc_mapmaker_realbosses_tag` (zeroed 0x2E8 stand-in for
the unchecked 'WELL' lookup, ret 0x0029B331) and `sub_00126560` ->
`pc_mapmaker_realbosses_fx` (beams `sub_0029CCB0` after ret 0x001CEE8C,
stock scene 81 only). `_event` drops events at rets 0x0029AEBE/0x0029B120
and at 0x0029CB80 removes it (`sub_0015DDA0`). bigdraw adds 0x637. Tests
`test_walker_*`, 34 walk mutations. Unplayed.
Tile roof menu (user, 2026-11-01): `src/interface/pc_mapmaker_tileflags.inc`
(area `sky`, after roof). Right click (pointer.inc, after the empty-cell Y
case) on a placed tile in Tiles mode (`editor+0x254` 0) with nothing held ->
`pc_mapmaker_tileflags_click`; WRAP `sub_00303E10` opens a heap descriptor
`{sub_00319300, sub_003192E0}` via `sub_002FB230`; WRAPs `sub_00319300`
(build: one "Roof" choice Normal/No roof/Open air + stock Done) and
`sub_003192E0` (tick: roof mark on the tile, or all selected tiles) act only
for that descriptor (menu +4). Roof format hook suffixes any flagged tile's
grid label. Escape/B still select. `tests/mapmaker/test_mapmaker_tileflags.py`.
Not built or played.
Skeleton height (mapmaker.16 hover): fight stores +0x68 = 0 at 0x0029D06B;
WRAP `sub_002895A0` -> `pc_mapmaker_realbosses_face` (ret 0x0029D075)
restores the spawn height noted by `_princess`; `_throw` skips a non-finite
aim and logs throws. Freeze after a throw: unexplained.
mapmaker.40: _restart reallocates (restart frees level heap); 
bs_skel_aim aims a floor up.
Robot doors (.45): stacked one-floor portals combine; `rbt_hold` puts back knee-level moves through low ones (mapmaker-robot.md).
Ported bosses in their own areas (merged 2026-11-01): `robot` (`pc_mapmaker_robot.inc`, id 0x42, docs/mapmaker/mapmaker-robot.md),
`crowe` (`pc_mapmaker_crowe.inc`, id 0x46, walk stops at ledges: mapmaker-crowe.md), `tank`
(`pc_mapmaker_tank.inc`: guard robot id 0x12 placeholder -> castle tank prop
0x281 from prop6.pak, mapmaker-tank.md). Wrappers, restart: each doc. ORDER puts them
after realbosses, before bosses (HUD inside `bosses_hud`; robot level outer of
realbosses level); included after realbosses.inc. Picker rows 153/154/155 are a
`rbs_extra` table in realbosses.inc (159/160 Prometheus); `storyprops`: drum/gun (mem-pak textures, lift env); bigdraw adds 0x5F2/0x5E8. Tank WRAPs
`sub_0015E7C0` -> `_tank_request` (level-init models), `sub_000A2740` -> `_draw`
(log), `sub_00228B50` -> `_mine` (.55 TNT); shown at create. .22: tank yard/target search bounded;
walker intro skips mission 13's SPC1/SPC2 trigger in `_fx` (see area docs).
Boss areas/skies/open air/set slots/pak table/restart heap: [boss notes](limitless-bosses.md); [.tsmap](map-transfer.md). Anim slots
(940, .34 crash): `pc_mapmaker_animslots.inc` in worm_register, mapmaker-animslots.md. MM sandstorm
draw (.32: grain/haze WRAPs sub_00154FE0/sub_0011A930, fog row 7): mapmaker-sky-music.md; music gain/ticks: custom-music.md.
Pak table test: `tests/mapmaker/test_mapmaker_paktable.py`.
Skeleton fire (mapmaker.17): `_fx` also draws the scene-1 particle branch
of `sub_0029CE00` (+0x3E88 == 1, `sub_002A7F30`). Walker missing: log-only
WRAP `sub_000DAC30` -> `pc_mapmaker_realbosses_create`, AI-start log; per-level
log budget and a walker heartbeat in `_fx` (mapmaker.21).

### MapMaker and System Link bug fixes

Area `bugfix` (`src/interface/pc_mapmaker_bugfix.inc`, `src/interface/pc_mapmaker_validate.inc`,
`config/guest-code-patches/bugfix.json`; table and sites in
`docs/mapmaker/mapmaker-bugfix.md`). Always on unless noted: System Link fragment
bounds (`sub_00352DF0`), the `sub_0006D0B0` return-address overwrite
(0x0006D115, at most 79 matches), turret-sync slot/player bounds
(`sub_003596F0`), network death-effect bounds (`sub_00357D70`), the arcade
mode 1 bot goal read (`sub_002B0320`), and a bounds-only map validator: the
editor loader refuses a bad map the stock way, and level setup
(`sub_0006D7A0`) hides bad chunks for that call. With
`TSFP_MAPMAKER_LIMITS=1` in scene 100 story mode the HUD turret icons scan
the whole gun pool (`sub_00349E40`/`sub_00349970`). Tests:
`tests/mapmaker/test_mapmaker_bugfix.py`, `tests/mapmaker/test_mapmaker_validate.py`.

### MapMaker editor mouse controls

On by default since 2026-10-23 (milestone confirmed in game);
`TSFP_MAPMAKER_POINTER=0` turns them off and restores the original editor
input. `src/interface/pc_mapmaker_pointer.inc` runs right
after the original `sub_00073870` inside `pc_display_menu_mouse_input`. When
scene 102 dispatches a menu with no rows and no open child (the editor screen)
and the host cursor moved or a button was pressed, it places the game's
per-player pointer (`0x007BA260 + player*0x1E0`, player `0x007BA9E0`) at the
mouse with the arithmetic of `sub_00071140`/`sub_00071050` (offsets
`+0x18/+0x1C`, position `+0x20/+0x24`, clamp `+0x48..+0x54`, bit 2 of `+0x00`).
It reports ownership through `pc_controls_mapmaker_pointer`; the host then
reports a held left button as held A via `pc_controls_terminal_click`
(`pc_mapmaker_click` in `pc_mapmaker_pointer.h` blocks a button already held
when ownership changes). Editor menus (pause menu, category lists) are normal
rows that the shared hook hit-tests and highlights, but the editor acts on A
itself, so in scene 102 a row click calls `mapmaker_pointer_row_click()`, which
holds A for that press (`pc_controls_mapmaker_row_press`) instead of the
menu-engine confirm. Also in scene 102: while a menu is open, Escape reports
as the first B key (editor menus close on B; Back toggles the 3D view) and not
as Start; on the editor screen it stays Start (pause). The choice is latched per press
(`pc_mapmaker_escape_back`, `pc_mapmaker_key`; Windows only). Menu
scrollbars (geometry as `sub_00071400`) are driven directly, because A over
the bar did not scroll the Story AI list: arrows step the target
`menu+0x88` (step `+0x98`, repeating while held), the track centres the view
there and the thumb follows the mouse, clamped like `sub_00073560`; the
engine animates the scroll. The wheel scrolls those menus the same way
(three steps per notch) and, on the editor screen, zooms by briefly holding
White (up) or Black (down) (`pc_mapmaker_zoom_*`). While a menu is open,
toolbar clicks do nothing and A/D switch categories; a click switch made of
generated B and A presses was tried and removed (it never opened the clicked
category reliably). Prompts show T for the right trigger in the
editor (`pc_controls_mapmaker_editor_active`). On the editor screen right-drag and the window edge pan the view the
way `sub_00309160` + `sub_002F9F70` do (centre clamp from the grid settings
block: 40, or 80 with the limits) (skipped when `editor+0x25C &
0x0C044088`), and the right button no longer sends the menu Back action
there (a right click without a drag sends it on release, which the editor
uses to rotate). The logic list keeps a passive description panel open as
its child (`sub_00078E50(..., 0x0052E9D8, 2)` in `sub_003278D0`), so a click
inside the panel's own entry list (one non-selectable type-8 item drawn by
`sub_00327520`, whose rows `sub_00323C30` tests against the game pointer and
activates on A) presses A; otherwise a click that misses the panel tries the
parent's rows
(`mapmaker_pointer_parent_press`: highlight with `sub_00076EF0`, then A). In
the editor T acts as the first "Fire" key (right trigger, floor up;
`pc_mapmaker_trigger_key`) because that key is usually the left mouse button,
and the movement keys (W/A/S/D by default) no longer move the pointer;
strafe left/right step the toolbar as the D-pad (`pc_mapmaker_move_key`,
`pc_mapmaker_dpad_alias`). A right click over an empty cell with no piece held
presses Y (clears the selection; `editor+0x2F7C`/`+0x2F18`, static analysis). In
an open list a right click is Back (`+0xC4 |= 0x20`), set by the shared row hook
(`sub_00073750`, 0x0007ADC5); the dispatcher acts on it through the list's Back
callback `+0x1C0` (`sub_002FB090`, 0x0007B1A9). The lighting list (descriptor
`0x0052DFF0`, toolbar table `0x0052CD50` slot 2) and its colour sub-lists
(`0x0052E000`/`E020`/`E030`/`E040`) have no `+0x1C0` and no pop flag (`+0x40 &
0x2000`): their tick `sub_0031AF80` (0x0007AD76, before the row hook; `+0xC4` is
cleared at 0x0007AA12) reads Back itself (0x0031B172, closes via `sub_0031A500`),
so the late Back was lost. `mapmaker_pointer_tick_back` sets it in
`pc_display_menu_mouse_input` right after the input read instead, as B does, and
clears the right-click action for the row hook. Every other MapMaker list descriptor
in `0x0052C000..0x0052F000` relies on `+0x1C0` (scan of ticks testing `+0xC4 & 0x20`;
tile/item/Story AI/logic/settings observed with `+0x1C0` set). No guest regeneration. Fixture:
`tests/mapmaker/test_mapmaker_pointer.py`. Mac: MAC-133. Next:
`mapmaker_table_input` (panel toggle, eats its A), SAI pts: [caps](limitless-caps.md).

### MapMaker editor widescreen

`src/interface/pc_mapmaker_widescreen.inc` (included after the grid area): the editor view
(scene 102) is 2D guest-pixel drawing, not a camera (`sub_003111F0` reaches neither
`sub_000C77C0` nor `sub_00151CF0`), so `pc_display_camera`'s factor never applies to it.
AUTO 2D placement stretched the grid's single full-width line list (`sub_0030A550` via
`sub_0001B090`) while tiles and the pointer stayed in the 4:3 box. `pc_mapmaker_grid_view`
(`sub_00310A20`) and `pc_mapmaker_grid_lines` (`sub_0030A550`) run them at CENTRE through
`pc_display_ui_scope` (`src/interface/pc_display_ui.inc`) on non-4:3 displays in scene 102
only; backdrop caller `0x0030A580` stretches via `pc_display_transition_overlay`.
`TSFP_MAPMAKER_WIDESCREEN=0` restores the old placement. Edge panning clamps both directions.
Notes: `docs/mapmaker/mapmaker-widescreen.md`.

## Runtime patches (extends [runtime.md](runtime.md))

### MapMaker meter probe wrapper

`*-mapmaker-meter-probe.patch` adds a pass-through `sub_0030C210` wrapper
(MapMaker capacity meter painter) beside the existing `sub_000814B0` bar wrapper
from `*-menu-setting-sliders.patch`, on whose context it depends; it therefore
sorts after it and is skipped with it in the historical-upgrade fixture in
`tests/repository/test_patch_updates.py`. A new wrapper changes the generation fingerprint,
so the first build regenerates guest C. Behavior is unchanged unless
`TSFP_MAPMAKER_PROBE` is set; see `docs/mapmaker/mapmaker-meter-probe.md`.

### MapMaker capacity wrappers

`*-mapmaker-meter-raise.patch` wraps `sub_002FA060`/`sub_002FA0D0` (memory
estimate, `TSFP_MAPMAKER_MEMORY_SCALE`); `*-mapmaker-meter-stagea.patch`,
sorting after it, wraps `sub_0009A140` (AI navigation pool allocator,
reimplemented with larger pools in scene 100 unless `TSFP_MAPMAKER_POOLS=0`)
and `sub_002FA720` (editor limits, recomputed after the original only with
`TSFP_MAPMAKER_LIMITS=1`). Both are skipped in the historical-upgrade fixture
and force one guest regeneration. Policy: `src/interface/pc_mapmaker_budget.inc`,
`src/interface/pc_mapmaker_limits.inc`; see `docs/agent-context/systems.md`.

`*-mapmaker-meter-wrappers.patch` (after `-stagea`) holds every area's wrappers;
regenerated whole, skipped with the other MapMaker patches in the
historical-upgrade fixture. Upstream-owned WRAPs: [runtime](runtime.md#upstream-defined-mapmaker-wrappers).

### Verified guest-code patches

`tools/compiler/guest_code_patches.py` is the lift's guest-patch checker (formerly the
runtime patch `*-guest-code-patches.patch`, ported into the project translator when
main replaced the runtime lifter; `translation_batch` applies the table,
`translation_decode.Disassembler` applies operand overrides, `translation_cli`
and `compile_pipeline` take `--guest-patches`):
`config/guest-code-patches/` (project-tracked) lists `code` entries (same-length
byte rewrites of the in-memory XBE copy the lifter decodes) and `operand` entries
(a decoded immediate or displacement replaced, so an imm8 compare can take any
32-bit value). The lift stops unless the XBE SHA-256, each entry's original bytes,
its function, whole-instruction coverage and the function's instruction boundaries,
mnemonics and branch targets all check out; entries in hand-written or HLE-replaced
functions are refused. The XBE on disk is never written, and the disassembly stage
is unaffected. Both builds pass the table (`tsfp.ps1` Generate and Build's lift,
`tools/build/build-native-mac.py`), and `tools/build/recomp-generation.py` hashes it and
`guest_code_patches.py`, so an edit regenerates guest C. The table is a folder: `00-header.json` holds the title,
XBE hash and the reserved extension block; every other file is one area's
`patches` list (merged by name order, ids unique). `relocate` entries move a
zero-initialised table into the extension block: every immediate or
displacement in lifted code inside `[old, old+size]` must be listed as an
`offset`, `end` or `keep` site and initialised data words in range as
`data_ok`, or the lift stops. `tools/analysis/guest-refs.py START SIZE [--data]` prints
that scan. Data-section (initial value) entries are not implemented; C
policy writes guest data where needed.

### MapMaker character set slots (area `sets`)

`src/interface/pc_mapmaker_sets.inc` (not built or played): the custom
character set has 20 slots with `TSFP_MAPMAKER_LIMITS=1` (the 0xA4 set
structs and `sub_0006BC50` already hold 20). Members 11-20 travel in a
fork-only chunk 0x00C776F5 before the terminator (written only when used or a
Story AI uses slot >= 10; `mapmaker_container_over_stock` then forces the
64 KB tier, so stock builds and System Link refuse the map). Thirteen wrappers:
compile/load/reset (chained), set-changed `sub_00312810`, slot screen paging
(`sub_002C0490`, `sub_002C0290`, `sub_00312950`, `sub_00314210`: Down on row
10 / Up on row 11, title "(1-10)"/"(11-20)" via menu `+0x204`), Story AI
choice `sub_00078590` (0x0031CD32; slot k >= 10 named by stock
`sub_00303350(0x38 + k - 10)` with `sub_002C0290` ret 0x003033A9 shifted 10
members, so realbosses' 0x003033C0 name rows 150/151 apply; the old host
lookup gave the Giant Skeleton no name; `mapmaker_sets_names.c` whole-chain
fixture), Story AI icon `sub_00315D20` (returns
0x0031D460 list icon row, 0x00309AC7 overview marker: a Story AI is item
0x38 + slot, so slots 11-20 drew items 0x42..0x4B, e.g. the time crystal;
drawn as item - 10), play copy `sub_0033EBF0` (count 20,
empty AI slot -> first used), preload `sub_0023F320` and match start
`sub_002D2C70` (chained with story; refills work-copy rows 1-10 when the
editor screen closed on page 2, since the rows are the match's bots). The
pause / front-end Arcade Bots screen (`sub_002C78F0` item 0x3B8 ->
`sub_002C7730` -> 0x0052375C, scenes 100/101, map set = index 0x11 in mode 9
only) is not paged by this area: rows are bots, 16 actors per match
(0x0078AD6C..), stock stores its edits in the profile settings, never the
map; its page 2 is the `matchbots` area below. Extension memory
0x00903700..0x009037FF. Contract and WRAP lines: `docs/mapmaker/mapmaker-sets.md`; tests
`tests/mapmaker/test_mapmaker_sets.py`, `..._sets_classified.py`.

### MapMaker weapon set slots (area `weaponset`)

`src/interface/pc_mapmaker_weaponset.inc` (not built or played): 20 weapon
slots with `TSFP_MAPMAKER_LIMITS=1`, no new item types. The play set
(`sub_0006BD30`, scene 100 rebuilds 0x006F1E28: 0x44 struct, map count 6)
is read by slot (pickup record +0x10 = catalogue +8, Story AI chunk word
+0x26 is a SLOT, `sub_001C8540` loads, AI random pick); none copies 0x44, so the hook returns a 20+ id copy at 0x00906000 when the
map's custom set is the source. A weapon/ammo pickup's slot 7-20 is record
byte +0xC (unwritten by `sub_00300800` for categories 8/9) and editor node
+0x30 ('WS' tag; no stock writer), paired by piece/item key on save/load.
Slots 7-20 travel in fork chunk 0x00C776F6 (only when used; over_stock ->
64 KB tier, stock and System Link refuse). Editor: weapon set pane
0x005236EC builder replayed with 20 rows in a scroll region (`sub_000780E0`
27.6f .. `sub_000781A0`); paging was invisible (pointer menus skip Up/Down);
the front end shows the MapMaker set (list value 0xF, mode 9) read-only; the in-play pause setup has no Weapons
item (`sub_002C78F0` arg 2 = 1, 0x002C808A). Story AI weapon choice (0x0031CDB7) and
pickup options `sub_00319300` (0x003193A2) get slots 7-20. Twenty wrappers;
extension 0x00906000..0x009064FF. Contract, WRAP lines:
`docs/mapmaker/mapmaker-weaponset.md`; tests `tests/mapmaker/test_mapmaker_weaponset.py`,
`..._weaponset_classified.py`.

### MapMaker Arcade bots per match (area `matchbots`)

`src/interface/pc_mapmaker_matchbots.inc` + `config/guest-code-patches/matchbots.json`
(not built or played): with `TSFP_MAPMAKER_LIMITS=1` a local MapMaker Arcade
match (mode 9, Test map mode 5 on scene byte 0x64, editor Test map) gets up to
20 bots / 24 characters (the 26-record arcade stats table `[0x0075CCC0]` and
the 26-row awards frame of `sub_0023A040` set 24). The table, applied in every
scene: the 26 indexed references to the live block's per-actor arrays
(0x007DE414/24/34/44, 16 each) read a 64-entry mirror at 0x00904000
(chars, teams, difficulty, variants x 0x40); the rank table 0x007DE4C0 (16)
is relocated to 0x00904100 (64; team scores 0x007DE500 kept, the System Link
copy end `0x00064CAB` an operand to 0x00904140); the non-story pair table is
`humans + 20` (`0x0015E0F5`, `0x0015E76C`, `0x0015E7DF`). Six wrappers:
`sub_000670C0` (pending -> live copy; refreshes the mirror: 0..15 from the
block, 16+ from the side arrays of the match start that added them; must be
wrapped wherever the table is applied), `sub_002EF9F0` (live -> setup: mirror
0..15 back), `sub_002D2C70` (chained: rows 11-20 after the stock roster,
stock team/difficulty rules), Bots screen `sub_002C0490` (chained with sets;
Down on row 10 swaps the work copy 0x0075FD90 with ten host-memory rows),
builder `sub_002C2C20` and store `sub_002BFFD0` (skipped on page 2). System
Link, story type, monkey sub-mode 0x13 and ordinary Arcade maps stay stock.
Contract and WRAP lines: `docs/mapmaker/mapmaker-matchbots.md`; tests
`tests/mapmaker/test_mapmaker_matchbots.py`, `..._matchbots_classified.py`.

### MapMaker Abstract and Disco tilesets (area `tilesets`)

`src/interface/pc_mapmaker_tilesets.inc` re-enables hidden index 0 (table
`0x0052CF38`; tiles/pads only in `pak/arcade/l_102.pak`, tile textures only in
`arcademem/l_102m.pak`). WRAP `sub_00071800` (return 0x0031354A, value 5,
`LIMITS`, scene 102) appends rows "Disco" (6) and "Abstract" (0). WRAP
`sub_00338170` (caller 0x000628ED) mounts `l_102.pak` whole, then `l_102m.pak`
index-only, before an Abstract map's tileset pak, ungated; without `l_102.pak`
it plays as Lab. Tileset 0 saves in the 64 KB tier. Disco
(`pc_mapmaker_disco.inc`, skymusic TU): header stays Virtual 5 plus info flag
bit 31 (decoder-proven free), editor +0x204 = 6; WRAPs `sub_0008E3E0` (alias
5 Virtual ids in fresh texture lists), `sub_002FC0A0` (row to base, reload on
a switch), `sub_00338180` (editor stream reload); map start mounts
`arcademem/l_56_arm.pak`. Docs `mapmaker-tilesets.md`, `mapmaker-disco-variant.md`
(`docs/mapmaker/`); tests `test_mapmaker_tilesets.py`, `test_mapmaker_disco.py`.

### Guest memory extension block

The project image loader owns the block: `tsfp_image_set_extension(bytes)` in
`src/runtime/guest_image.c` (before `xbox_MemoryLayoutInit`) makes inspection
place a zeroed block right after the image (end rounded up to 64 KB) and count it
in `image_end`/`image_hi`, so `tsfp_image_bootstrap` moves the runtime's low
memory (kernel data, stack, heap) up past it (the heap shrinks by its size) and
host writers treat it as image; `tsfp_image_extension()` reports where the last
load put it. `src/runtime/guest_extension.h` holds Future Perfect's 0x80000
bytes at 0x008A0000 (`TSFP_GUEST_EXTENSION_VA/SIZE`, matching
`00-header.json`). Both boots reserve it and stop with a message if the block
lands elsewhere, because relocated tables in the generated code point into it:
Windows through `patches/platform/*-guest-memory-extension.patch` (runtime
`titles/futureperfect/src/main.c`), native Mac in `src/native/main.c`. Before
main's independent image loader this was a runtime patch to
`src/kernel/xbox_memory_layout.c`. Operand entries may also redirect a
read to a C-written value there: `grid.json` points eight MapMaker editor
constant reads at the 16-byte settings block 0x00903600
(`src/interface/pc_mapmaker_grid.h`), written by the grid hooks before the editor runs.
With `TSFP_MAPMAKER_STACK_ITEMS=1` the grid hook on `sub_003348C0` (return
0x003070A7) also sets placement-valid bit 4 of editor `+0x25C` when an allowed
pickup is held over a cell that already has an item, so the stock drop path runs.
The sky area also wraps the editor neighbour-mask builders (`sub_002FC680`,
`sub_002FC470`), piece creation (`sub_00307FD0`) and the tile palette menu
(`sub_00316B50`, `sub_000792B0`, `sub_00316920`, `sub_00316500`), piece
move/copy/free (`sub_002F95E0`, `sub_00308120`, `sub_00308480`), text
(`sub_0003D7B0`, chained with realbosses) and tile numbers (`sub_00151030`,
`sub_0032D9F0`) for the Open roof / Open air tiles (`src/interface/pc_mapmaker_roof.inc`); compile, load and reset add a further
layer to their existing chains. Contract: `docs/mapmaker/mapmaker-sky.md`.
Its hook on `sub_002F8B40` (return 0x0030723F, Story AI mode) answers -1 while
a Story AI is held, the value `[editor+0x2F20]` already has for "no point under
the cursor", so a Story AI can be dropped on another one's cell; with empty
hands it answers the highest-index AI with a point in the cell (newest first).
Tests: `tests/compiler/test_guest_code_patches.py`, `tests/compiler/test_recomp_generation.py`,
`tests/compiler/test_guest_image.py` (image loader contract).

### Wrappers shared with upstream patches

Where an upstream patch already defines a guest wrapper (today `sub_000A5CE0`, wrapped by
`patches/gameplay/timing/0460-native-wind-history.patch`), the generated MapMaker chain is
exported as `sub_<addr>_mapmaker` (generator `CHAIN_ONTO` set) and the upstream policy
function calls it instead of `original` (`src/gameplay/pc_native_wind_guest.inc`): upstream
outermost, MapMaker inside, original innermost.

### Hook and table notes (moved from runtime.md)

`0460-native-wind-history` wraps `sub_000A5CE0`; its policy calls the MapMaker chain.
`aitemplates-pool.json` raises the editor menu-item pool (0x00070DC3, 0x398 to 0xC00).
The characters area wraps `sub_001CD980` (unlock test, MapMaker picker return only) and `sub_000D7780` (log-only preload).
The memory area wraps the Level-heap allocators `sub_0003DFA0`/`sub_0003E0B0` (overrun log) and arena setup `sub_00023930` (`TSFP_MAPMAKER_ARENA_MB`).
Bosses also wraps `sub_00078590` (Story AI health rows, return 0x0031CFD7) for the levels above Tough Guy.
The sets area (20-slot custom character set) wraps the set screen, store, play copy and preload and adds layers to compile/load/reset and `sub_00078590`.
Realbosses (giant trophy monster) wraps the picker, text, model request, anim mount `sub_001C8540` and AI start `sub_0015B830`; its HUD layer sits inside bosses.
Realbosses also wraps `sub_0013A400` (Story overhead meter; skipped only for the real trophy monster at return 0x0014D264).
Sets also wraps `sub_002D2C70` (match start; editor Test map bots from slots 1-10), chained with the story character hook.
Sky also wraps piece move/copy/free (`sub_002F95E0`, `sub_00308120`, `sub_00308480`) to carry open-roof marks, and text `sub_0003D7B0` (+realbosses).
Bosses also wraps `sub_0014D1F0` (HUD per viewport) to draw Sub-boss overhead meters via `sub_0013A400` before the stock loop.
`matchbots.json` moves the 26 live per-actor reads to a 64-entry mirror at 0x00904000 and the rank table to 0x00904100 (every scene); the matchbots area fills them.
The weaponset area (20 slots; list shows hidden 0x29/0x2F/0x38: `sub_001CD9D0`) wraps the play set `sub_0006BD30`, pickup slot/veto/spawn and weapon set screen functions, and chains compile/load/reset, `sub_0003D7B0` and `sub_00078590`; roof wraps the label format `sub_00151030` and caption `sub_0032D9F0`.
Realbosses adds the giant skeleton: story level init `sub_0025F720`, `sub_0029FE00` variant, `sub_002875F0` emerge, `sub_00287E20` throw and level events `sub_002274C0` (death event dropped).
bigdraw `sub_000862C0`: bosses on the whole view; skelvis logs; bossroom: bosses in camera room list. openview: walk `sub_00089B90`: open-air areas unclipped (mapmaker-sky.md).
camera wraps actor update `sub_00246000`: a switch-held camera (mode 0x80, D8/C14 reciprocal) runs with stock skip bit data+8 bit 0 (0x002460E5), so the sweep stops; the walk logs what each view reaches. Test `test_mapmaker_camera.py`; mapmaker-camera.md (4:3 frame fixed in shared pc_display_ui.inc, test_camera_widescreen.py; ~10-floor view open).

## Build and test (extends [build-test.md](build-test.md))

### MapMaker probe runs and tests

`tools/testing/testing_profile.py --play --mapmaker-probe FLAGS` (Windows) and
`tools/testing/test-native-mac.py --mapmaker-probe FLAGS` (Mac) share
`mapmaker_probe_environment`, which writes `mapmaker-probe.log` inside the
disposable test root. `prepare_run` still strips inherited `TSFP_*`, so a
personal environment cannot enable it. Asset-free coverage:
`tests/mapmaker/test_mapmaker_probe.py` (registered in the native CI list; fixed case,
four mutations and patch shape) and `tests/repository/test_testing_profile.py`.
Fixture success is not evidence of in-game meter behaviour.
Probe flags are 1..255 (64 = AI pool counts, 128 = room prop draws, both in
scene 100). `tests/mapmaker/test_mapmaker_sky.py` (MapMaker clouds) also checks the
stock sky data against the local XBE and skips that check without it.
`tests/mapmaker/test_mapmaker_skymask.py` (fixture `tests/mapmaker/mapmaker_skymask.c`, dev
switch `TSFP_MAPMAKER_ROOM_MASK_CLEAR`) compares the exact stderr text and
runs 15 mutations; its decoder check of the per-room mask table's references
skips without the private XBE. Both are in the native CI list, as is
`tests/mapmaker/test_mapmaker_roof.py` (fixture `tests/mapmaker/mapmaker_roof.c`, the Open
roof / Open air palette tiles: 55 mutations including place -> save ->
play +8 -> reload, play -> editor -> copy -> play with items and labels
46/47; its XBE data, copy-site, record word +0x26, label-site
and hook-site decoder checks skip without the private XBE). The MapMaker
pointer, memory budget and stage A limits fixtures
(`tests/mapmaker/test_mapmaker_pointer.py`, `tests/mapmaker/test_mapmaker_budget.py`,
`tests/mapmaker/test_mapmaker_limits.py`) are in the same CI list, as is
`tests/gameplay/test_mounted_mouse.py` (mounted/vehicle gun mouse aim; compiles
`tests/gameplay/mounted_mouse.c` with `src/input/pc_input.c`, six header mutations must fail)
and `tests/input/test_scope_wheel.py` (mouse-wheel scope zoom; compiles
`tests/input/scope_wheel.c` against `src/input/pc_scope_wheel.h`, ten mutations must fail). Fixtures that
include `pc_display_menu.c` or `pc_menu_mouse_guest.inc` (`menu_mouse.c`,
`fov_settings.c`) need a `sub_0003DFA0` stub for the stage A allocator.
`tests/input/test_menu_mouse.py` also covers the MapMaker map browser click
(`map_browser_tests`; mutation `late-map-confirm` removes the input-hook click)
and the briefing click (`briefing_tests`; mutation `late-briefing-confirm`).
`tests/mapmaker/test_mapmaker_grid.py` covers stacked items with the per-frame valid
bit hook `pc_mapmaker_stack_valid` (`sub_003348C0`, return 0x003070A7) and
the always-run `sub_00308260` original (`stack-valid-*`, `stack-find-*`
mutations), and Story AI / teleporter stacking (`test_stack_sai`: a fake
`sub_00334770` with both refusals over the relocated Story AI array at
editor+0x11950, a fake `sub_002F8B40`; `sai-*`, `stack-hides-any-item`,
`stack-no-teleporter*` mutations). `tests/mapmaker/test_mapmaker_objects.py` covers the
skipped self link of a stacked teleporter pair (`tele-self-*`).

### Guest-code patch table

`config/guest-code-patches/` (a folder of JSON tables) is applied by every lift (Windows Generate and
Build's regeneration, Mac `--generate`); its hash is part of the generation
fingerprint, so editing it forces regeneration on the next build. An entry that
fails verification stops the build with the entry id. See
`docs/agent-context/runtime.md` (Verified guest-code patches).

MapMaker capacity area fixtures (`tests/mapmaker/test_mapmaker_pads.py`,
`tests/mapmaker/test_mapmaker_container.py`, and later areas) are registered in the
native CI list; `menu_mouse.c` and `fov_settings.c` include
`tests/mapmaker/mapmaker_area_stubs.h` for the game functions those areas call. The
`test_menu_mouse.py` mutation `footer-only-character-confirm` checks that a
character-screen click off the footer confirms the previewed character.
`tests/mapmaker/test_mapmaker_actors.py` also checks that every generated operand entry
in `actors-list.json` keeps its offset in the moved table, and that the
perception slice in `objects.json` matches the actors and objects constants;
its enemy-picker case compares the two-pass `sub_0027BA40` hook against a
model of the original's 32-entry array and argmin on random lists.
`tests/mapmaker/test_mapmaker_story_character.py` (also in the native CI list) checks
the gate wrappers with mutations, the table bytes, and (with the private XBE)
that every `cmp byte [0x0078ADAE],2` in game code is patched or reviewed.

`tests/mapmaker/test_mapmaker_aitemplates.py` (Story AI duplicate, leader groups,
numbered names, group-logic toggle, promotion, old-format migration, unplaced
copies/duplicates, menu-pool diagnostic, mutations) and `tests/mapmaker/test_mapmaker_aitemplates_classified.py` (decode checks of
the edit-screen row site, menu table 0x0052E088 and the unused chunk bytes
+0x32/+0x33) are in the same CI list.
`tests/mapmaker/test_mapmaker_bosses.py` (Boss row, editor table, chunk +0x29,
health scaling, stacked boss bars, stronger health options, read-only probe;
mutations; decode checks of chunk bytes +0x29/+0x21, the Health row and
tables, the `sub_0014CC80`/`sub_00139740` bar sites and the `sub_0013A400`
meter contract) is too.

`tests/mapmaker/test_mapmaker_grid.py` (fixture `tests/mapmaker/mapmaker_grid.c`) checks that
`grid.json` redirects exactly the decoded editor reads of the grid constants
(skipped without the private XBE) and that the settings block holds the
image constants in stock mode.

Site inventories must come from the lifter's decoder, not from
`private/pipeline/disasm/asm/text.asm`: the listing elides the byte column of
long instructions, which once hid a room id and crashed a story level.
`tests/mapmaker/mapmaker_decoded.py` decodes all game code once per process with
`tools.recomp.disasm`; `tests/test_mapmaker_{pads,container,editor,objects,actors,rooms_bytes}_classified.py`
(and the rooms test's constant check) require every
candidate constant/displacement of their area to be patched or listed with a
reason. They skip without the private XBE, so CI only imports them.

### Compile-only Windows builds

`tsfp.ps1 -Action Build -CompileOnly` exists for hosts whose application control
blocks unsigned local executables. Everything up to and including the shared
`cmake --build` target list is unchanged; the post-build branch only reports
the skipped `ghost_targets.py`/`d3d8_replay.exe`, `tsfp_*_test.exe` runs and
the network seal, writing `build/windows/compile-only-build.json`. The full
branch removes that record after its tests pass. Adding a new executed build
test requires listing it in the compile-only skipped list;
`tests/repository/test_compile_only_build.py` fails otherwise (static checks on every
host, PowerShell checks on Windows). A compile-only build is never a passing
build nor sealed for System Link or packaging.

### Local test release folder

`tools/release/package-release.py` accepts `--build-root <checkout>` (build/windows and
external runtime from another checkout, e.g. from a source-only worktree) and
the hidden `--local-preview` used by `tools/release/make-local-release.py`. Local preview
skips the network seal, checks the executable hash against
`build/windows/compile-only-build.json`, builds the launcher without private
artwork/test run, and records BUILD.json `preview` (validated false) plus a
README warning. It is never a published release. Tests:
`tests/launcher/package_launcher.py`, `tests/launcher/test_make_local_release.py`.

### FFmpeg pin (2026-10-31)

`tools/build/fetch-video.ps1` downloads the BtbN month-end build
`autobuild-2026-09-30-13-08/ffmpeg-n8.1.3-9-g29e619e767-win64-lgpl-shared-8.1.zip`
(SHA-256 `3e47bda1…6811a`, also recorded by `tools/release/package-release.py`). The
previous pin named BtbN's daily-rebuilt `latest` asset; month-end builds are retained. Without
`third_party/ffmpeg/include` the runtime builds without `XMV_HAVE_FFMPEG`
(no XMV movie playback), so fetch FFmpeg before a release build.

A `--local-preview` package of a compile-only (unsealed) build gets its
`Support/Online/network-build.json` stamp written into the staged copy only
(`local_preview: true`); `build/windows/generated` stays unsealed and
BUILD.json keeps `network_sealed: false`.

Launcher MapMaker switches (2026-10-31): `tests/launcher/launcher_settings.cs` (run by
`tools/build/build-launcher.ps1 -Test --settings`) and the Mac `NativeTests.cs` cover the
saved `launcher.mapmaker_*` keys and StartInfo export; the portable
`tools/playable-launch.ps1` mirror is covered by
`tests/launcher/test_launcher_startup.py::test_script_mapmaker_switches`, which compiles
and runs a synthetic child exe (may be blocked by local antivirus).

`tests/mapmaker/test_mapmaker_widescreen.py` (editor 2D placement on non-4:3 displays;
compiles `tests/mapmaker/mapmaker_widescreen.c` on the `tests/graphics/ui_layout.c` batch
harness, 8 mutations including `pc_display_ui.inc`); `tests/input/menu_mouse.c` stubs
`pc_display_wide_now`/`pc_display_ui_scope` because it does not compile the display layer.
