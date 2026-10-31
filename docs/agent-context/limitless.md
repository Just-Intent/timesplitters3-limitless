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
`tests/input/test_scope_wheel.py`. Mac follow-up: MAC-126 and later.
<!-- packet:end -->

Fork-specific subsystem notes, kept out of the shared size-capped guides so
upstream merges stay clean. Each part names the shared guide it extends.
MapMaker player/contract docs live in `docs/mapmaker/`; measured results in
`docs/project/STATUS.md`; Mac follow-up in `docs/project/MAC_PORT_BACKLOG.md`
(MAC-126 and later).

## Subsystem map rows (extends [systems.md](systems.md))

| Area | Files | Tests |
| --- | --- | --- |
| Mouse/menu handling | `src/interface/pc_menu_mouse_guest.inc`, `pc_menu_choices.inc`, `pc_menu_sliders.inc`, `pc_menu_team.inc`, `pc_menu_mapmaker_browser.inc`, `pc_menu_briefing.inc` | `tests/input/test_menu_mouse.py`, `test_terminal_mouse.py`, `test_ui_layout.py` |
| Mounted / vehicle gun mouse aim | `src/input/pc_mounted.h` (called from both adapters' `pc_controls_apply_mouse` after the terminal route, before ordinary look) | `tests/gameplay/test_mounted_mouse.py` |
| Mouse-wheel scope zoom | `src/input/pc_scope_wheel.h` (state from both adapters' `pc_controls_apply_mouse`; wheel and key filters in their wheel/key paths), `pc_settings_bound_keys` in `pc_bindings.inc` | `tests/input/test_scope_wheel.py` |
| MapMaker capacity areas | `src/interface/pc_mapmaker_*.inc` (included by `pc_menu_mouse_guest.inc`), `config/guest-code-patches/`, `tools/compiler/guest_code_patches.py`, `src/runtime/guest_extension.h` | `tests/mapmaker/`, `tests/compiler/test_guest_code_patches.py` |
| Guest lift/runtime correctness | Runtime `tools/recomp/`, manual hooks, `tools/build/recomp-generation.py`, verified guest-code patches `tools/compiler/guest_code_patches.py` | `tests/compiler/test_recomp_float_joins.py`, `test_recomp_vehicle_joins.py` |

Issue/Ideas routing (extends [build-test.md](build-test.md)): the issue chooser's
Ideas contact link points to the fork's `JustinDent/timesplitters3-limitless`
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
experiment protocol: `docs/mapmaker/mapmaker-meter-probe.md`. Mac: MAC-126.
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
held editor piece (`+0x2F7C`); `sub_002FC680` then drops `0x02000000` (open
roof) or `0x02FFFFFF` (open air, floor bit 24 kept) and `sub_002FC470` drops
`0x02000000`. The rebuild hook (`sub_003006C0`) stores record word `+0x26` =
`0x5201`/`0x5202` and the open `+8`; the load hook (`sub_00301A90`) matches
marks back to pieces; `sub_00307FD0` and `sub_002FEBF0` forget marks. The map
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
are recomputed with pads 4800, links 8800, groups 600 and 200 items (cover stays
1024; the cover pool is 1536 as a guard), and the category maxima table
`0x00524BE0` is raised (pickups x4, movers/props x4; turrets and teleporters
unchanged). The editor's item node
pool is 2120 through `config/guest-code-patches/` (always on). Tests:
`tests/mapmaker/test_mapmaker_limits.py`. Probe flag 64. Basis:
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
runtime pad handles 0x4740.., bounded A* pools, group A* pool relocated to
0x008E0000), `container` (map container 512 KB in the runtime heap, tiered
saves stock-identical when small, large maps refused for System Link),
`objects`, `editor`, `rooms` (241 pieces) and `actors` (64 characters / 63 AI
in a local MapMaker story map instead of 20 alive; live list moved to
0x00900100, AI table to 0x00900200, per-id table 0x00739AA0 to 0x00900300 by
`actors.json` + generated `actors-list.json`; 64-slot actor pool in scene 100;
perception slice 0xC00 shared with `objects.json`; 19 hooks incl. the 32-entry
stack-array AI pickers (`sub_0027BA40` in two passes), a side table at
0x00900600 for kill words of victims 32..63, radar blips moved to 0x00902100
(256, bounded) with the radar loop at 64, the per-death effect lists 0x0075F670/0x0075F5F8 at 64 (0x00903400/0x00903200, stock 30 outside wide play), and the patrol table 0x00520C38 read
from a copy at 0x00901600 (ids >= 32 have no route); `pc_network_bots_revive`
writes the moved AI table; `TSFP_MAPMAKER_ALIVE=1..64` lowers it; sites and
risks in `docs/mapmaker/mapmaker-actors.md`), and `grid` (editor only: with
`TSFP_MAPMAKER_LIMITS=1` floors -2..4 and an 80x80x8 occupancy grid, the five
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
Campaign story, Arcade Custom, challenges and System Link (modes 11..13) are
unchanged. The gate stays 0 if the hooks do not run, so `story` must not be
switched off by `TSFP_MAPMAKER_AREA_OFF`. Tests:
`tests/mapmaker/test_mapmaker_story_character.py`; notes `docs/mapmaker/mapmaker-story-character.md`.

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
with group logic (or an unconverted old template) means "any member"
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
entries, colour-checked, kept through delete/reset/load/compile. In scene
100 story mode, each HUD frame (wrap of the story boss bar `sub_0014CC80`,
ebx = HUD block) multiplies a Boss AI's entity health +0x1CC/+0x1D8 once
per (re)initialisation and, when no stock bar was drawn (HUD block +8 bit
0), draws the nearest living Boss AI within `TSFP_MAPMAKER_BOSS_RANGE`
(default 30 units) through `sub_00139740` (esi HUD block, edi entity) with
the AI's saved name. The focus field +0xFF4 and the `sub_0014F0B0`
0x0014FAE3 meter (a different, player-drain bar) are untouched. Same file:
read-only `TSFP_MAPMAKER_CREATURE_PROBE=1` (log
`TSFP_MAPMAKER_CREATURE_PROBE_LOG`) listing story-only character model
resource slots per level and every new live character's id, AI class,
record, model and skeleton. Not yet built or played.

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
so one bar per actor. No save byte; the handler cannot reach `sub_002274C0`
(decode call graph). `tests/mapmaker/test_mapmaker_realbosses.py`. Not yet
built or played.

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
presses Y (clears the selection; `editor+0x2F7C`/`+0x2F18`, static analysis). No guest regeneration. Fixture:
`tests/mapmaker/test_mapmaker_pointer.py`. Mac: MAC-128.

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

`*-mapmaker-meter-wrappers.patch` (sorting after `-stagea`) holds the wrappers
of every capacity area (see `docs/agent-context/systems.md`, MapMaker capacity
areas); it is regenerated whole when an area is added and is skipped with the
other MapMaker patches in the historical-upgrade fixture.

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
(`sub_00316B50`, `sub_000792B0`, `sub_00316920`, `sub_00316500`) for the Open roof /
Open air tiles (`src/interface/pc_mapmaker_roof.inc`); compile, load and reset add a further
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
roof / Open air palette tiles: 29 mutations; its XBE data, record word +0x26
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
health scaling, nearest-boss bar, read-only probe; mutations; decode checks of
chunk byte +0x29 and the `sub_0014CC80`/`sub_00139740` bar sites) is too.

`tests/mapmaker/test_mapmaker_grid.py` (fixture `tests/mapmaker/mapmaker_grid.c`) checks that
`grid.json` redirects exactly the decoded editor reads of the grid constants
(skipped without the private XBE) and that the settings block holds the
image constants in stock mode.

Site inventories must come from the lifter's decoder, not from
`private/pipeline/disasm/asm/text.asm`: the listing elides the byte column of
long instructions, which once hid a room id and crashed a story level.
`tests/mapmaker/mapmaker_decoded.py` decodes all game code once per process with
`tools.recomp.disasm`; `tests/test_mapmaker_{pads,container,editor,objects,actors}_classified.py`
(and `test_every_room_constant_is_classified` in the rooms test) require every
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
build and is not sealed for System Link or packaging.

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
previous pin named the moving `latest` asset, which BtbN rebuilds daily, so its
checksum stopped matching; month-end builds are retained. Without
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
