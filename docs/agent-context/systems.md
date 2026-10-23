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
in `assets/textures` (DDS via Git LFS), audited/bundled by `tools/build_textures.py`;
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
| Mouse/menu handling | `src/pc_menu_mouse_guest.inc`, `pc_menu_choices.inc`, `pc_menu_sliders.inc`, `pc_menu_team.inc` | `tests/test_menu_mouse.py`, `test_terminal_mouse.py`, `test_ui_layout.py` |
| Auto-look / camera / cutscene | `src/pc_auto_look_guest.inc`, `pc_camera.h`, `pc_cutscene.h`, `pc_ui_layout.h` | `tests/test_auto_look.py`, `test_cutscene_letterbox.py`, `test_camera_feed.py` |
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

`config/*.default.*` is tracked default configuration; `config/pc-controls.json`
and `config/pc-display.ini` are personal. `private/game` is the single extracted
game-data copy; `private/saves` is personal; `private/test-fixtures/unlocked` is
the canonical local fixture and must remain unchanged. Test runners copy it into
bounded disposable test roots. `private/texture-remaster/pack` is the one active
6-GiB-capped art-production pack; source art and original game assets stay ignored.
Sanitized compiled DDS/JSON replacements are tracked in `assets/textures` and
bundled with game releases; `tools/texture-repository.py` stages them with LFS,
`tools/build_textures.py` audits them. Leave pointers unhydrated for unrelated
source work. Packaging must reject missing LFS content, not silently omit it.

For installed layouts, use the launcher platform adapter instead of assuming
checkout names. Never include private data, generated guest code, credentials,
environment dumps or personal configuration in durable context. Store paths,
symbols, semantics and sanitized measured conclusions instead.

## Prompt text layout read scopes

`pc_prompt_guest.inc` reads through `pc_prompts_read_region`; the bounded
thread-local `pc_text_read_scope.h` cache uses existing OS VM query adapters.
It is valid only during synchronous text layout, with no mapping/protection
changes or event dispatch. Nested scopes retain intervals; the next outer scope
clears them. Scratch writes still use a fresh protection query. Scope wrapping,
label construction/drawing and nested prompt measurement through the shared
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
experiment protocol: `docs/mapmaker-meter-probe.md`. Mac: MAC-060.

## MapMaker editor pointer (prototype)

Off unless `TSFP_MAPMAKER_POINTER=1`. `src/pc_mapmaker_pointer.inc` runs right
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
as the first Back key and not as Start; on the editor screen it stays Start
(pause), since Back there toggles the 3D view. The choice is latched per press
(`pc_mapmaker_escape_back`, `pc_mapmaker_key`; Windows only). Menu
scrollbars (geometry as `sub_00071400`) are driven directly, because A over
the bar did not scroll the Story AI list: arrows step the target
`menu+0x88` (step `+0x98`, repeating while held), the track centres the view
there and the thumb follows the mouse, clamped like `sub_00073560`; the
engine animates the scroll. Prompts show T for the right trigger in the
editor (`pc_controls_mapmaker_editor_active`). On the editor screen right-drag and the window edge pan the view the
way `sub_00309160` + `sub_002F9F70` do (skipped when `editor+0x25C &
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
and WASD is no longer aliased to the D-pad. No guest regeneration. Fixture:
`tests/test_mapmaker_pointer.py`. Mac: MAC-062.
