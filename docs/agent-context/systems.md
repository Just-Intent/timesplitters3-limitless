# Subsystem entry points

<!-- packet:start -->
Root tracked policy: `src/pc_input*`, `pc_controls*`, `pc_bindings.inc` for input;
`pc_display*`, `pc_settings*`, `pc_refresh*` for display/configuration;
`pc_menu_*`, `pc_ui_layout.h`, `pc_cutscene.h`, `pc_camera.h` for menus/camera;
`pc_debug_options.def` for the shared debug fields/keys/labels/capabilities;
`src/graphics/` for common shader/math policy; `src/native/` for host adapters.
Patches connect these files to upstream; the generated guest files are not the
authoritative place to implement portable behavior.

Textures: root `src/texture_pack*`, `texture_mode*`, runtime `src/d3d/` and HLE
texture hooks; offline tooling `tools/texture-*`. Sanitized replacements now live
in `assets/textures` (DDS via Git LFS), audited by `tools/build_textures.py` for optional delivery;
private source art and original assets stay local. Networking: `src/pc_network*`,
`pc_online*`, runtime HLE/kernel patches and `tools/online.py`. Crash capture:
`src/pc_crash*`, `pc_hang_policy.h`, runtime crash hooks and launcher diagnostics.
Audio/movie timing is runtime `src/hle/hle_dsound*`, `hle_xmv*`, not launcher code.
Find the connecting patch by symbol/path using `tools/repo-context.py find`.
Consult `docs/shared-platform.md` for shared versus adapter responsibilities.
<!-- packet:end -->

| Area | Start here (tracked root unless noted) | Useful checks/docs |
| --- | --- | --- |
| Bindings, input prompts | `src/pc_controls.c`, `pc_input.c`, `pc_bindings.inc`, `pc_prompts.c` | `tests/pc_controls.c`, `tests/pc_prompts.c`, `docs/windows-settings.md` |
| Mouse/menu handling | `src/pc_menu_mouse_guest.inc`, `pc_menu_choices.inc`, `pc_menu_sliders.inc`, `pc_menu_team.inc`, `pc_menu_mapmaker_browser.inc` | `tests/test_menu_mouse.py`, `test_terminal_mouse.py`, `test_ui_layout.py` |
| Auto-look / camera / cutscene | `src/pc_auto_look_guest.inc`, `pc_camera.h`, `pc_cutscene.h`, `pc_ui_layout.h` | `tests/test_auto_look.py`, `test_cutscene_letterbox.py`, `test_camera_feed.py` |
| Mounted / vehicle gun mouse aim | `src/pc_mounted.h` (called from both adapters' `pc_controls_apply_mouse` after the terminal route, before ordinary look) | `tests/test_mounted_mouse.py` |
| Mouse-wheel scope zoom | `src/pc_scope_wheel.h` (state from both adapters' `pc_controls_apply_mouse`; wheel and key filters in their wheel/key paths), `pc_settings_bound_keys` in `pc_bindings.inc` | `tests/test_scope_wheel.py` |
| Display model and native UI | `src/pc_display.c`, `pc_display_*.inc`, `pc_settings.c`, `src/native/` | `tests/test_pc_display_modes.py`, `test_debug_settings_model.py` |
| GPU clear / screen copies / sampling | `src/pc_clear_rhi.inc`, `src/graphics/`, runtime `src/d3d/` | `tests/test_rhi_menu_sampling.py`, `tests/rhi/`, `docs/shared-platform.md` |
| Texture loading and formats | `src/texture_pack.c`, `texture_pack_format.h`, `texture_pack_io.h`, `texture_mode.c` | `tests/test_texture_runtime.py`, `tests/texture_pack_tools.py` |
| Texture production and audit | `tools/texture-worklist.py`, `tools/texture-hybrid-plan.py` | Root AGENTS texture rules, `docs/textures.md` |
| System Link / online / bots | `src/pc_network.c`, `pc_network_guest.inc`, `pc_network_bots*`, `pc_online.c`, `tools/online.py` | `tests/test_network_guest_dispatch.py`, `test_online.py`, `test_network_identity.py` |
| Discord presence | `src/pc_discord*` plus runtime integration patches | `tests/test_discord_presence.py`, `test_discord_macos.py` |
| Saves and profiles | Runtime save-routing patch; `tools/testing_profile.py`, `tools/test-native-mac.py` | `docs/testing-profile.md`, `tests/test_testing_profile.py` |
| Crash/hang diagnostics | `src/pc_crash.c`, `pc_hang_policy.h`, `tools/crash-symbols.py` | `tests/test_crash_capture.py`, `test_native_crash.py`, `test_crash_symbols.py` |
| Guest lift/runtime correctness | Runtime `tools/recomp/`, manual hooks, `tools/recomp-generation.py` | `tests/test_recomp_float_joins.py`, `test_recomp_vehicle_joins.py` |
| MapMaker (probe and off-by-default editor pointer prototype) | `src/pc_mapmaker_pointer.inc`, `pc_mapmaker_pointer.h`, `research/mapmaker-investigation.md`, `research/capacity-constraints.md`; known anchors: editor scene 102 (map play likely 100), meter `sub_0030C210` → `sub_000814B0` | `docs/development-roadmap.md` |
| Audio/movie startup | Runtime `src/hle/` sound and XMV files | `tests/test_audio_timing.py`, `test_xmv_stop.py` |

Names in this table are entry points, not a complete call graph. Read the
applicable adapter plus one relevant test. Tests may encode historical migration
or compatibility behavior, so confirm current defaults in root instructions and
effective source. Preserve unknown/unsupported saved settings across hosts.

Ghost Gun living-actor reveal alignment (#181/#241) is handled by
`pc_display_weapon_effect` in `src/pc_display_ui.inc`. The integer rectangle
returning to US caller `0x001232A9` is the final filtered screen-copy composite
over the actor's projected bounds. In a full widescreen view it uses full-view
placement, with deferred draws flushed before restoring the surrounding UI
placement. Guest coordinates, UVs/tint and the five preceding blur passes stay
unchanged. The separate humanoid ghost composite retains its existing guarded
call site. Original 4:3 and split-view paths keep their existing behavior.
`tests/test_ghost_actor_overlay.py` covers placement, deferred batches, ABI,
both platform wrappers and a local supported-US-routine check; see build/test
for test prerequisites. Windows scene confirmation is recorded in `STATUS.md`;
native Mac visual validation remains pending under MAC-036.

## State and data boundaries

Online bot presentation (#222) uses `pc_network_bot_motion.h` from the
`pc_network_bots_guest_frame` hook. The transport still consumes each validated
host snapshot once. The guest caches it and updates the displayed body, spatial
bounds and matching animation frame times every frame over a 50 ms span, retargeting
from the current display pose when arrivals vary. Discrete vitals, scores, shots,
absence and spawn transitions retain host values; no guest AI or extrapolation
is enabled. A gap over 250 ms snaps to the next received host state. Match,
host, actor-pool and inactive-state changes clear presentation history. The
Windows QPC adapter supplies monotonic seconds independently of guest game time.
Human controller replication and the host's three-tick send interval are unchanged.
Native Mac fallback remains original until its network adapter exists (MAC-052).
Only clip-block `+8` (model `DC`/`134`) is interpolated within the same clip/cycle;
the other animation channels and crossfade controls stay atomic. US `5CEF0`
can promote the secondary clip during zero-advance evaluation, so mixing fade
duration/elapsed with another snapshot's clip identity can select an invalid clip.

Camera and perspective corrections use `pc_display_memory.h` to retain one
OS-validated writable region only after their original guest routine returns.
The stack-local snapshot expires when the correction returns; no guest calls,
allocations or protection changes may occur while it is reused. Bounds,
overflow, read-only and guard/no-access rejection remain enforced. Other
display/menu validation still uses its existing checks. This avoids repeating
the VM query for camera, output and source matrices within the same region;
it does not cache matrices, reduce updates or change projection arithmetic.
Decal corrections validate the camera and its source matrix before the output
because the original callers build their temporary output on the guest stack.
Separate camera/stack regions therefore need two queries, not three.

The native SDL input self-test ends its synthetic terminal context before
testing independent mouse capture/fire behavior. The fractional-terminal case
otherwise leaves `camera_input.terminal_active` set after its mock guest is freed,
suppressing the next case's Fire button. This is test isolation inside
`tsfp_native_input_test`; ordinary frame/input behavior is unchanged.

`config/*.default.*` is tracked default configuration; `config/pc-controls.json`
and `config/pc-display.ini` are personal. `private/game` is the single extracted
game-data copy; `private/saves` is personal; `private/test-fixtures/unlocked` is
the canonical local fixture and must remain unchanged. Test runners copy it into
bounded disposable test roots. `private/texture-remaster/pack` is the one active
6-GiB-capped art-production pack; source art and original game assets stay ignored.
Sanitized compiled DDS/JSON replacements are tracked in `assets/textures` and
published as optional packs; `tools/texture-repository.py` stages them with LFS,
`tools/build_textures.py` audits them. Leave pointers unhydrated for unrelated
source work. Optional pack publication must reject missing LFS content. Ordinary
game packages omit textures; existing installed packs and runtime fallback remain.

For installed layouts, use the launcher platform adapter instead of assuming
checkout names. Never include private data, generated guest code, credentials,
environment dumps or personal configuration in durable context. Store paths,
symbols, semantics and sanitized measured conclusions instead.

## Prompt text layout read scopes

`pc_prompt_guest.inc` reads through `pc_prompts_read_region`; the bounded
thread-local `pc_text_read_scope.h` cache uses existing OS VM query adapters.
It is valid only during synchronous text layout, with no mapping/protection
changes or event dispatch. Nested scopes retain intervals; the next outer scope
clears them. Scratch writes still use a fresh protection query.
The cached interval covers the full OS-validated region, so descending guest
stack reads and earlier labels in the same mapping reuse one query. It never
extends beyond that region or survives the outer layout scope.
The US outlined-label wrapper (`sub_00041E30`) retains this same scope over
its four outline draws and foreground pass, then expires it at return.
Scope wrapping, label construction/drawing and nested prompt measurement use the shared
hooks in `pc_tutorial_ui.inc`, `pc_display_ui.inc` and `pc_prompt_guest.inc`.
`pc_display_menu.c` includes `pc_prompts.h` before the .inc callers to avoid
implicit declarations. Do not extend caches across menu ticks or frames.
Tests cover region boundaries/expiry and actual prompt-hook scratch ABI/fallbacks.
Issue #105 also requires the runtime late-frame pacing patch; caching alone
measured 30 FPS. See STATUS.md for measured acceptance and Mac limits.

## Physical keyboard text entry (#136)

`src/pc_text_input.h` defines the shared bounded per-dialog text queue and edit
commands. Windows `pc_controls.c` supplies WM_CHAR/WM_UNICHAR; native Mac
`input_sdl.c` supplies SDL text events and starts/stops text input on its main
thread. Both adapters suppress keyboard menu bindings while a text owner is active
and discard queued text on focus loss or owner changes.

`src/pc_menu_text.inc`, included by `pc_menu_mouse_guest.inc`, recognizes live
character-row callbacks (`0x0007DB20`) rather than one dialog descriptor. Create
Profile embeds this keyboard in descriptor `0x004E8D54`; testing only standalone
`0x004D8D78` misses it. The original guest insertion/edit/Done routines retain
selection replacement, limits and save behavior. Escape queues the owning menu's
Back action, preserving embedded-menu callbacks. Numeric descriptor `0x004D94D4`
is excluded. Supported input follows the original Latin-1 tables; paste and
arbitrary Unicode are outside this change. See MAC-059 and the #136 STATUS entry
for physical Windows confirmation and pending Mac/dialog-matrix validation.

## Automatic display size

`pc_display_current()` is the selected/persisted mode: `0,0` retains console
resolution, `1,1` means Auto, and other valid dimensions are manual. New missing
settings default to Auto; existing settings are preserved. Render/camera/UI code
must use `pc_display_effective()` or the shape/scale helpers, never interpret
the Auto sentinel as physical pixels. The shared catalog appends Auto without
renumbering existing ratio groups. Guest Display settings and Cocoa expose it.

On Windows Auto reads the fullscreen window's nearest monitor desktop mode,
or its non-minimized client pixels in windowed mode. The SDL main thread
publishes drawable pixels via `pc_display_host_size`; the render thread reads
an atomic snapshot. Frame completion checks at most four times per second;
GPU allocation failure retains the last effective size and retries later.
Unsupported/empty sizes retain the last valid output (guest output before a window
exists); current supported limits remain landscape 640x480 through 7680x4320,
no wider than 4:1. Manual selection disables this tracking. Desktop resolution
and refresh rate are never changed.

## Mounted and vehicle gun mouse aim

Ordinary mouse look writes player yaw/pitch (`+0x100/+0x104`) from the
`sub_000D2B80` wrapper, before `sub_000D5C70` dispatches the player mode
(`+0x114`). Mode `0x400` (vehicle occupant) goes to `sub_000D2470`
(`0x000D5DA8`), which rebuilds player yaw from the controller axes for gunner
seats (`0x000D25E8`, `0x000D2774`, `0x000D2A20`) and aims the seat gun from
that packet, so mouse look was discarded there (Scotland the Brave truck gun).
Mode `0x80` (scripted mount `sub_000B76B0`) aims from the gun data angles.
`src/pc_mounted.h` (port of main's `src/input/pc_mounted.h`, #182/#405/#412)
routes mouse counts to the bound gun's angles in those two modes: vehicle seat
lookup through the character `+0xC08` handle, entity table `0x0072F5E8`,
seat records (`+0x70`, stride `0x34`, types 3/4 gunners, tracked type 0 driver
camera), gun data `+0x1C/+0x24` with authored limits `+0x34..+0x40`, operator
`+0xD8` checks. Mode `0x40` (`sub_000D1650`, gun follows the player view) and
on-foot look stay on the ordinary path. This branch has no native-rate clock,
so callers pass `native_timing=0` (whole-tick `0x004E7944` gate). Controller
aim is untouched (original code). `tests/test_mounted_mouse.py`: fixture plus
six mutations and an adapter ordering check; not a live turret test.

## Mouse-wheel scope zoom

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

`src/pc_scope_wheel.h`: `pc_scope_view` (player `+4 == 0` only, i.e. local
player 1; bits `0x60` and min < max), `pc_scope_wheel_bindings` (a wheel
direction is redirected only when bound to Previous/Next weapon and nothing
else; zoom keys are the first non-wheel keys of actions 10/11), per-notch
`PC_SCOPE_WHEEL_MS` (80 ms, about 5 degrees) holds that add up to 640 ms,
state older than 250 ms ignored, leaving the scope cancels the hold. Both
adapters read `pc_settings_bound_keys` and `pc_scope_view` before their lock in
`pc_controls_apply_mouse`, filter wheel notches (`wheel_input` /
SDL wheel event) and every polled key (`pc_controls_key_down` /
`tsfp_native_key_down`), and reset on menu/capture/focus changes. Controller
input and Q/V are unchanged. `tests/test_scope_wheel.py`: fixture plus ten
mutations and adapter/binding structure checks; no in-game run yet.

## Crane viewport edge clipping (#221)

`pc_ui_vignette_clip` in `pc_ui_layout.h` converts the crane/remote-view
vignette's full 0,0..639,479 clip to host-exclusive 640,480 bounds only for
STRETCH tag `0x00137010`. Shared RHI `d3d8_GetScissor` and legacy GL
`native_apply_scissor` apply this policy before output scaling. Stored guest
clips, other tags and partial rectangles remain unchanged. The title helper
scope supplies the tag only for complete single-player non-4:3 views.

## MapMaker meter probe

`src/pc_mapmaker_probe.inc` (included by `pc_menu_mouse_guest.inc` before the
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
`tests/test_mapmaker_probe.py` enforces this with mutations. Usage and the
experiment protocol: `docs/mapmaker-meter-probe.md`. Mac: MAC-063.
Flag 128 (scene 100, 5 s windows every 30 s) logs room draws, their props and
the model draws inside them through two further wrappers,
`pc_mapmaker_probe_room_draw` (`sub_000A2900`) and
`pc_mapmaker_probe_prop_draw` (`sub_0008F950`, every model draw, inert outside
a window); it reads the per-room prop tables at their `rooms.json` locations
(`0x008C7000`/`0x008C7C00`). Once per run per geometry model and node table
it also dumps the model's node tree (`[inst+4]` header, 0xC8-byte nodes
before it; type `+0x48`, child `+0x4B`, sibling `+0x4C`, mask `+0x5C`), capped
at 96 nodes per model, 256 models and 8192 lines. See `docs/mapmaker-sky.md`.

Dev-only sky-tile switch `TSFP_MAPMAKER_ROOM_MASK_CLEAR=<hex>`
(`src/pc_mapmaker_skymask.inc`, included after `pc_mapmaker_sky.inc`): the
`sub_00085710` wrapper `pc_mapmaker_sky_room_mask` clears those bits in the
relocated per-room mask table `0x008C0800` right after the stock store
(`0x000857A3`), scene 100 only, so `inst+0x30` (`0x00085DCA`), the camera
and the weather getter `sub_00083970` agree; editor and saved data are not
touched. `pc_mapmaker_sky_room_mask_flush()` (called from
`pc_display_menu_mouse_mode`) prints one `[mapmaker] room mask: cleared ...`
summary per map build. Independent of `TSFP_MAPMAKER_LIMITS`; the Test Lab
runner strips it with the other `TSFP_*` variables. Fixture
`tests/test_mapmaker_skymask.py`.

MapMaker Open roof / Open air tiles (`src/pc_mapmaker_roof.inc`, area `sky`,
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
lines and test steps: `docs/mapmaker-sky.md` "Open-air tiles". Fixture
`tests/test_mapmaker_roof.py`.

## MapMaker level memory budget (opt-in)

`TSFP_MAPMAKER_MEMORY_SCALE=N` (2..8; anything else = off; 8 when
`TSFP_MAPMAKER_LIMITS=1` and no scale is set) divides the
results of `sub_002FA060` (placed pieces) and `sub_002FA0D0` (held piece),
the editor's play-time memory estimate checked against 2,048,000 bytes in
`sub_002FA720` and shown by the meter. Wrappers in
`patches/*-mapmaker-meter-raise.patch` (needs guest regeneration once);
policy `src/pc_mapmaker_budget.inc` changes only `g_eax`. Tests:
`tests/test_mapmaker_budget.py`. Basis and limits:
`research/capacity-constraints.md` §1b.

## MapMaker capacity stage A (pools on by default, limits opt-in)

`src/pc_mapmaker_limits.inc` via `patches/*-mapmaker-meter-stagea.patch`:
`sub_0009A140` is reimplemented in scene 100 with enlarged AI navigation
pools (`TSFP_MAPMAKER_POOLS=0` = original); with `TSFP_MAPMAKER_LIMITS=1`
(ignored while pools are off) `sub_002FA720`'s decision, meter and preview
are recomputed with pads 4800, links 8800, groups 600 and 200 items (cover stays
1024; the cover pool is 1536 as a guard), and the category maxima table
`0x00524BE0` is raised (pickups x4, movers/props x4; turrets and teleporters
unchanged). The editor's item node
pool is 2120 through `config/guest-code-patches/` (always on). Tests:
`tests/test_mapmaker_limits.py`. Probe flag 64. Basis:
`research/capacity-constraints.md` §1c.

## MapMaker capacity areas

Each area of the limit raise (research/capacity-raise-plan.md) is a guest-code
table `config/guest-code-patches/<area>.json`, C policy
`src/pc_mapmaker_<area>.inc` (included from `pc_menu_mouse_guest.inc` after
`pc_mapmaker_limits.inc`), a fixture `tests/test_mapmaker_<area>.py` and notes
`docs/mapmaker-<area>.md`. Wrappers for every area are in
`patches/*-mapmaker-meter-wrappers.patch`; declarations sit between the
"capacity area hooks" markers in `src/pc_display_menu.h`, no-op stubs between
the "capacity area stubs" markers in `pc_menu_mouse_guest.inc`. Fixtures that
compile the whole include chain take link stubs from
`tests/mapmaker_area_stubs.h`. Areas: `pads` (cover handle range 10,240,
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
risks in `docs/mapmaker-actors.md`), and `grid` (editor only: with
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
16-byte settings block at 0x00903600 (`src/pc_mapmaker_grid.h`) that
`grid.json` redirects the editor's 40.0/5.0/60.0/(1/60) reads to; save format
unchanged; `docs/mapmaker-grid.md`).
Editor capacity follows `TSFP_MAPMAKER_LIMITS=1`.

MapMaker editor widescreen (`src/pc_mapmaker_widescreen.inc`, include after
the area files): the editor view (scene 102) is 2D guest-pixel drawing, not a
camera (`sub_003111F0` reaches neither `sub_000C77C0` nor `sub_00151CF0`), so
`pc_display_camera`'s factor never applies to it. AUTO 2D placement stretched
the grid's one flat full-width line list (`sub_0030A550` via `sub_0001B090`)
while tiles/pointer stayed in the 4:3 box. `pc_mapmaker_grid_view`
(`sub_00310A20`) and `pc_mapmaker_grid_lines` (`sub_0030A550`) run them at
CENTRE through `pc_display_ui_scope` (`pc_display_ui.inc`) on non-4:3 displays
in scene 102 only; backdrop caller `0x0030A580` stretches via
`pc_display_transition_overlay`. `TSFP_MAPMAKER_WIDESCREEN=0` = old. Edge
panning clamps both directions. Tests `tests/test_mapmaker_widescreen.py`,
`test_mapmaker_pointer.py`, `test_ui_layout.py`; `docs/mapmaker-widescreen.md`.

risks in `docs/mapmaker-actors.md`).
Editor capacity follows `TSFP_MAPMAKER_LIMITS=1`. `sky` (no table; not yet
wired) adds a per-map cloud layer with `TSFP_MAPMAKER_LIMITS=1`: a "Clouds"
checkbox after the stock sky row of Map Settings (the 11-sky choice itself is
stock, editor `+0x220` -> info `+0x10`), stored as info flag 0x40000, applied
in scene 100 by copying sky record 5's cloud fields into the map's MapMaker
sky record (58..67) around `sub_0011C9F0`. Four hooks (`sub_0011C9F0`,
`sub_00078560`, `sub_002FEBF0`, `sub_003006C0` chained after the container
hook); details and test steps in `docs/mapmaker-sky.md`.

## MapMaker map browser mouse

Main menu > MapMaker > Load map is an ordinary menu (descriptor `0x0052EA88`,
construct `sub_0032BEA0`, tick `sub_0032BDD0`) whose type-7 rows carry the
index of the enumerated file list `[0x00778D38]` (count `+8`, 0x6C-byte
entries at `+0xC`). Its tick loads the highlighted row when the active menu's
`+0xC4` has Confirm (0x10), but `sub_0007A7B0` runs the tick (0x0007AD76)
before the row hook `sub_00073750` (0x0007ADC5), so the shared row click was
too late. `src/pc_menu_mapmaker_browser.inc`, called from
`pc_display_menu_mouse_input` after the footer test (scene 101 only), makes a
fresh left click on a row highlight it (`sub_00076EF0`) and a click on the
highlighted row Confirm (raw pad A plus `+0xC4`/`+0xC8` bit 0x10, as the
footer). Rows flagged `+0x28 & 5` or with an id past the count are ignored;
hover and the wheel keep the shared row behaviour. No patch or regeneration.
Fixture: `map_browser_tests` in `tests/menu_mouse.c` (mutation
`late-map-confirm`).

## Character selection mouse

`src/pc_menu_characters.inc` captures the drawn portrait quads of the
carousel (`0x005237E0` child of container `0x005237F0`). The wheel and a
click on another visible portrait preview it (`sub_002C4310`). The previewed
portrait is replaced by the 3D model, so any other fresh left click (on it,
empty space or footer Select) confirms the previewed character as raw Select
(0x80); footer Back or right-click is Back (0x120). Fixture: `character_tests`
in `tests/menu_mouse.c` (mutation `footer-only-character-confirm`).

## MapMaker Story character choice

`story-character.json` makes the four MapMaker-Story Cortez branches
(`cmp byte [0x0078ADAE],2` after a mode-9 test: roster `sub_002C6C80`
0x002C6D1F, preview `sub_002C6680` 0x002C672E, confirm `sub_002D4D00`
0x002D5414, config `sub_002D2C70` 0x002D359D) read the gate byte 0x00903500.
`src/pc_mapmaker_story_character.inc` wraps those four functions (area
`story`) and sets the gate on entry: the sub-mode byte (stock, Cortez only)
unless `TSFP_MAPMAKER_LIMITS=1`, then 0, so MapMaker Story uses the Arcade
roster of player 0's unlocked characters, starts on the profile's last
choice, stores the choice and writes it (and the variant) to the config for
each local player. Play code already takes the character from the config.
Campaign story, Arcade Custom, challenges and System Link (modes 11..13) are
unchanged. The gate stays 0 if the hooks do not run, so `story` must not be
switched off by `TSFP_MAPMAKER_AREA_OFF`. Tests:
`tests/test_mapmaker_story_character.py`; notes `docs/mapmaker-story-character.md`.

Story AI groups (`src/pc_mapmaker_aitemplates.inc`, `docs/mapmaker-aitemplates.md`,
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
outside the container/editor ones. Not yet built or played.

## MapMaker and System Link bug fixes

Area `bugfix` (`src/pc_mapmaker_bugfix.inc`, `src/pc_mapmaker_validate.inc`,
`config/guest-code-patches/bugfix.json`; table and sites in
`docs/mapmaker-bugfix.md`). Always on unless noted: System Link fragment
bounds (`sub_00352DF0`), the `sub_0006D0B0` return-address overwrite
(0x0006D115, at most 79 matches), turret-sync slot/player bounds
(`sub_003596F0`), network death-effect bounds (`sub_00357D70`), the arcade
mode 1 bot goal read (`sub_002B0320`), and a bounds-only map validator: the
editor loader refuses a bad map the stock way, and level setup
(`sub_0006D7A0`) hides bad chunks for that call. With
`TSFP_MAPMAKER_LIMITS=1` in scene 100 story mode the HUD turret icons scan
the whole gun pool (`sub_00349E40`/`sub_00349970`). Tests:
`tests/test_mapmaker_bugfix.py`, `tests/test_mapmaker_validate.py`.

## MapMaker editor mouse controls

On by default since 2026-10-23 (milestone confirmed in game);
`TSFP_MAPMAKER_POINTER=0` turns them off and restores the original editor
input. `src/pc_mapmaker_pointer.inc` runs right
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
`tests/test_mapmaker_pointer.py`. Mac: MAC-065.

## Mounted and CCTV gun mouse aim (#182)

`src/pc_mounted.h` handles guest player mode `0x80` before ordinary player
camera updates in both `pc_controls.c` and `native/input_sdl.c`. The US guest
`sub_0025D520`/`sub_00258A20` aim an AGUN through player entity data `+0xC14`;
ordinary player yaw/pitch do not turn it. The shared helper validates its AGUN
tag and reverse owner link, uses local input port zero (not guest player ID),
and updates gun pitch/yaw with authored limits, sensitivity, inversion, FOV and
optional gun speed. Mouse displacement is consumed once without frame-time
multiplication. Invalid ownership, pause, focus loss and stale motion consume
input without falling through to hidden player angles. Terminal mode `0x2000`
and its left-click mapping retain `pc_terminal_mouse` behavior. Controller
input remains in the guest routines. The separate vehicle-seat mode `0x8000`
is not routed by this helper. `tests/test_mounted_mouse.py` runs the guarded C
fixture on Windows/macOS; physical Mac validation is tracked under MAC-001.
The mounted helper receives the adapter's explicit native-timing selection.
Native mode accepts finite positive game-float time at `4E794C`; Original60
retains its whole-time gate at `4E7944`. Both consume/rebase mouse batches with
`pc_input_consume_motion`, preventing fresh packets from expiring against a
previous batch timestamp. Paused/invalid native time consumes without aiming.
The helper still handles only AGUN mode `0x80`, not vehicle-seat mode `0x8000`.
Host input self-tests route synthetic motion through `pc_controls_apply_mouse`
in Windows and SDL, with zero-whole positive time, paused stale whole time,
live return to Original60 and return to on-foot controls. Windows execution
passes; native SDL self-test execution remains a host validation requirement.

## Unified Video Options and native presets (PR #285)

`pc_display_menu.c` stages resolution (including Auto), aspect, vertical FOV,
texture style, FPS and subtitles behind one dirty-state Apply. Auto uses the
`1,1` saved sentinel and is labeled Auto, never displayed as a pixel size.
Controls follows Video Options; conditional frontend Cheats is last. Audio
retains its frontend/pause-specific alignment and common highlights are rounded.
`pc_frame_rate_settings.h` persists the beta presets; live requests enter
`pc_frame_rate_live.inc` and commit at the guest clock boundary.

The merge retains main's nested text-read scope and fresh scratch-write checks
rather than the older per-hook stack-query optimization. The real-protection
fixture in `tests/pc_prompts.c` retains boundary/ABI checks; query reduction is
measured inside the cache by `tests/text_read_scope.c`, because reads now execute
in a separately compiled translation unit. Mounted mouse aim remains ahead of
ordinary camera input in both adapters; fractional terminal handling is retained.
Native high-refresh validation is MAC-060, distinct from crane MAC-058 and
text input MAC-059.

## Issue #74 menu texture additions

The branch pack now contains 2,046 accepted replacements. The same keyed DDS
files serve Windows and the common native Mac renderer; loader contracts are
unchanged. Menu coverage is 574/750 keys, with 164 original character icons, eight
intentional functional originals and four unresolved replacements.
`config/texture-originals.json` records excluded icon keys; the worklist rejects
installed replacements for those keys and hybrid planning retains the original
lane. Character model materials remain in scope. Source resampling and original-detail
recovery are distinguished from newly generated art in `docs/main-menu-textures.md`.
Offline acceptance does not certify mapped scene appearance or Mac GPU behavior.

The original-icon policy also includes historical public DDS/JSON hashes.
`build_textures.ensure_pack` rejects reintroduced icon keys and emits these
identities in `BUILD.json` under `textures.retired` for both game packagers.
The shared launcher retires only matching installed pairs transactionally;
custom artwork stays protected. See the launcher guide for recovery behavior.

## Native movement boost resource

The US walking routine `sub_00258F50` gates a movement boost on mode byte
`0x7DE456 == 0x11`, selected player `0x7DE498`, input bit `0x800`, and positive
entity+0x1D0 resource. The fixed subtraction at `0x259060` is now scoped through
`pc_native_boost_drain` in `pc_native_numeric.h`. Native frames split one active
original-update charge by `max(0, min(1, raw_delta, game_delta))`: slow motion
retains the original zero-whole-tick gate's reduced consumption; fast/catch-up
updates still consume at most one charge. Original mode and other sites retain
exact subtraction. No constant table, boost speed, threshold or stored clock
is changed. The complete US routine fixture uses a controlled flat collision
floor; it does not establish real-map geometry, input acceleration or network
transport parity. The game rule-name table at 0x4D1F90 + index*0x20, decoded
through the existing Discord string-table path, identifies index 17 as Escort.
A bounded frontend lookup confirms the name; live boost activation remains
unverified.

Escort recovery takes a separate route: active-match `sub_00068830` calls
`sub_000681C0`, then `sub_000654E0`. Its original game-clock delay is more than
five seconds; eligible match updates include zero-whole-game-tick frames.
`pc_native_boost_recover` therefore scales only addition site `0x6552D` by
`max(0,min(1,raw_delta))`. The original dispatcher gates pause; its selected-actor
write and mirror, threshold overshoot and health-dependent clamp are preserved.
The full three-routine fixture controls external services and validates those
boundaries separately from the walking drain's active-game-update budget.

## Experimental water coefficient precision

`pc_surface_coefficients` in `pc_surface_bias.h` preserves the current guest
uploader's clamped/truncated byte anchors, then scales acceleration/displacement
and exponentiates damping without repacking fractional values. The existing
bias+float+fractional opt-in gates enable it. `pc_native_surface_guest.h` copies
original parameters into the HLE state before temporary guest scaling; whole
steps retain original uploads. The HLE path copies bank 0 stages 3/5/7 and bank
1 stage 6 into ordered commands only for shader 0x4BBF10, caching duplicates and
clearing on scope exit. Defaults and original/catch-up behavior stay unchanged.
The constant-uploader fixture documents the current lifter's truncation; this
change does not claim original Xbox MXCSR rounding fidelity or full wave parity.

The opt-in precision policy now selects full-float format 0x4B for registered
feedback intermediates. Final byte textures, guest headers, rate gating and
normal-play defaults are unchanged. Common-RHI promotion/demotion preserves
live GPU contents and capture bytes; the transaction still flushes before copy,
rebinds only on success and retains the old resource on failure. This removes
measured half-float accumulation error but does not prove original wave parity.

Optional game packages use `build_textures.optional_identity` to retain only the
small `textures.retired` migration, without any DDS/JSON payload or LFS access.
This preserves the original-icon fix using the existing shared launcher consumer;
ordinary missing texture files never cause removal of an installed pack.

## Direct keyboard inventory slots

`pc_weapon_slots.def` appends host-only actions after the 24 Xbox controls;
`pc_weapon_slots.h` owns press edges and expiring requests. `pc_bindings.inc`
loads/saves them through the normal binding editor and migrates only missing
slots. Stock White=1 and Black=2 move to free Z/X keys; existing custom and
explicitly unbound actions remain intact. No migration writes until Save.
Both Win32 and SDL gate requests on focus, mouse capture, menus and text input.

The additive keyboard-weapon-slots patch wraps US `sub_001BF140`, the original
player weapon-input routine. `pc_weapon_slots_guest.inc` runs on that guest
thread after ordinary input, using the ordered table at `0x4FC888`, the original
variant filter `sub_001BAA70`, usability `sub_001BA410` and selection
`sub_001BA130`, then refreshes the weapon HUD with `sub_00142820`. It deduplicates
weapon groups, checks local input slot zero rather than network player ID, and
never grants inventory. Empty slots do nothing. Settings expose exactly ten bindable
slots, defaulting to 1–9/0. The separate 70-entry internal table bound covers
weapon IDs and variants; it does not add settings rows.

## Themed live keyboard and mouse editor

`pc_keyboard_menu.inc` is included by `pc_display_menu.c`, adding Keyboard and
Mouse to both main Settings and paused Options. It uses the original menu
builders, callbacks, highlights and footer navigation. The controller-style
bindings pages draw a keyboard/mouse diagram with primary/alternate columns;
`pc_keyboard_menu_model.h` partitions all 34 actions across short pages.
`PC_BINDING_SLOTS` is two in the shared model and native dialogs; old keyboard
profiles use their first two sources without modifying disk until saved.
Controller profiles keep their existing source capacity.

`pc_binding_capture.h` handles opening-input release, capture, final-input
release, Escape cancellation, focus loss and timeout. `pc_bindings.inc` owns
its lock/state; Windows and SDL supply raw physical keys and wheel pulses, latching press
events so quick taps are retained between polling ticks.
While capturing, controller polling and menu pointer dispatch are suppressed,
so E/F/Y and mouse clicks bind without activating menu commands. Assignments
use the existing atomic settings save/conflict API. Mouse sensitivity previews
live, debounces writes, flushes on exit, and restores previous values on error.

Capture starts once in the new page update. Native descriptor +12 is a per-frame
callback, not a destructor; installing capture cleanup there cancels it every
frame. The callback fixture in `tests/fov_settings.c` exercises both native
per-frame callbacks and requires a selectable capture instruction row.

## Host storage choices in Create Profile

`pc_storage_menu.inc` injects mounted host drive rows at native storage-button
site 001944C3, directly in Select Storage Device. Rows sort by drive letter on Windows and case-insensitive volume name on Mac;
Rows show only volume names and drive letters, with no game/save role suffix.
Native menu sizing is widened before its
title at 0019440B. Labels are ASCII-safe for the guest font. Windows lists mounted
drive letters; Mac lists the startup volume and /Volumes mounts. Neither lists
unmounted partitions. The native 00192770 update and 00191470 icon adapter keep
host indices away from the console device table. Selection delegates to original
00192E60 with console index zero, preserving the save/overwrite state machine.
After a drive change, synchronous 00024EC0/00190F70 enumeration rebuilds the native
profile list (74F9FC) before duplicate-name validation against the new location.

`pc_storage.inc` supplies actual volume metadata, cached five seconds for original
storage-text replacements. `pc_storage_routes.inc` validates a writable save
folder, prepares UserData/45410066 for the native free-space query, and atomically
saves save-location.txt beside RECOMP_INPUT_CONFIG. Changes immediately redirect
future UDATA opens through the registered kernel adapter; failed persistence
rolls that route back. Existing handles, caches and partition devices stay put.
Startup also honors the preference; profiles are neither moved nor deleted.
An unavailable configured location fails explicitly instead of silently
creating a new profile elsewhere. TSFP_SAVE_LOCATION_LOCK protects isolated test
sessions from changing real drive preferences. Windows resolves junctions and
volume identities; drive metadata retains executable-volume identity internally.
The Mac metadata adapter includes unistd.h before calling getattrlist; sys/attr.h
provides attribute structures but not that function declaration. The production
pc_prompts.c compile contract checks this include order in Mac CI.
