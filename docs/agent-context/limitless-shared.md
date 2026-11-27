# MapMaker extensions: notes that extend shared guides

Moved out of the size-capped [launcher](launcher.md), [systems](systems.md) and
[build-test](build-test.md) guides. Main MapMaker guide: [limitless](limitless.md).

## MapMaker settings tab

The shared `SettingsPage` adds a **MapMaker** tab (after Discord) with `MapMakerLimits`, `MapMakerOverlap`, `MapMakerStackItems`
checkboxes and a `MapMakerAlive` choice (64 default, 48, 32, 24, 20; any saved
1-64 value is kept). `GameSettings` reads/writes strict booleans and an integer
under `launcher.mapmaker_*` in pc-controls.json through the existing
transactional Save (backup, outside-change refusal, unknown-key preservation);
invalid types/ranges refuse loading. `StartInfo` still strips inherited
`TSFP_*`/`RECOMP_*`, then applies `MapMakerEnvironment()`: `LIMITS`, `OVERLAP`,
`STACK_ITEMS` = `1` only when on and `ALIVE` only when not 64. The normal Play,
crash/hang-test (`LaunchGame`) and Mac `NativeWindow` paths all use StartInfo.
`MEMORY_SCALE`, `POINTER`, `POOLS`, `ROOM_MASK_CLEAR` and `PROBE` remain
developer environment-only (source runs). Packaged `Launch.ps1`
(`tools/playable-launch.ps1`) applies the same saved keys. Tests:
`tests/launcher/launcher_settings.cs` (defaults, round-trip, unknown keys, inherited strip,
invalid values, UI save), `NativeTests.cs` (Mac routing),
`tests/launcher/test_launcher_startup.py::test_script_mapmaker_switches`. Mac compile and
runtime of this tab are pending validation (`MAC_PORT_BACKLOG.md`).

## Release repository and test-only local source

`ReleaseClient.Repository` is `TheDoffman/timesplitters-future-perfect-pc`: game and
launcher release checks, self-update, news, known issues, texture manifest,
Support/issue/Ideas links and `ValidateDownloadUrl` all follow it.
`ReleaseClient.UpstreamRepository` (`TheDoffman/timesplitters-future-perfect-pc`)
is credit only. Python tools share `tools/project_repository.py`; PowerShell
reads `config/project-repository.json`; the standalone-installed
`tools/ci/compatibility-worker.py` keeps a matching inline constant.
`config/build/launcher-artwork.json` still pins the original project's immutable
launcher-v1.2.18 artwork asset (hash-verified; the fork has no copy).

`LocalReleaseSource` (in `ReleaseClient.cs`, both hosts) is a hidden TEST-ONLY
`HttpMessageHandler`. Only when `TSFP_LAUNCHER_RELEASE_SOURCE=<folder>` is set,
`new ReleaseClient()` answers this repository's GitHub API/download URLs from
`<folder>/releases.json` and `<folder>/<tag>/<asset>` (byte ranges supported);
every other URL gets 404 with no network request. Selection, URL validation,
digest/`.sha256` checks, resumable transfer, extraction, install and repair are
unchanged. `SetStatus`/Mac `Status` prefix `[TEST ONLY: local release source …]`
and the version labels say `TEST-ONLY LOCAL RELEASE SOURCE`.
`tools/release/make-local-release.py` builds the folder via
`tools/release/package-release.py --local-preview`, which keeps the compile-only build
record in BUILD.json `preview` (validated false) instead of a network seal.
Tests: `tests/launcher/launcher_downloads.cs` (local source group),
`tests/launcher/package_launcher.py`, `tests/launcher/test_make_local_release.py`.

Story branch preview preparation: `0.4.0-story-coop.1` uses channel ID
`story-coop` and display name `Story Co-op Preview`. It follows the existing
clean/pushed branch publisher, optional-texture policy and immutable preview tags.
This version preparation is not publication or native Mac Online acceptance.


Preview packaging follow-up: `Core.AtomicReplace` is shared by metadata,
`GameSessions` records and `GameSettings` writes. The original atomic replacement
runs on every host. Windows retries only IO errors32/33/1175, while both paths
still exist, for at most20 waits of50ms. Other errors, missing paths and
non-Windows failures propagate immediately. No destination delete/copy fallback
is used; existing backup arguments and session WriteThrough flushing remain.
Microsoft documents1175 as retaining both original names;1176/1177 can move or
remove names and are deliberately not retried:
[ReplaceFile](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew).
`launcher_updates.cs` covers real locked-file recovery with backup retention,
synthetic1175 recovery, bounded permanent failure and immediate1176 rejection.
Existing download/session/settings suites exercise the shared callers.
Generated preview notes now direct users to Preview, then the named Testing
track, matching the shared launcher UI. `test_publish_release.py` checks this.

## MapMaker debug panel

`show_mapmaker_panel` ("Show MapMaker debug panel", default off, persisted by
the shared `pc_debug_options.def` model, so every settings dialog lists it).
Shift+F9 toggles it in memory (Windows: the HLE overlay frame's F9 edge with
`GetAsyncKeyState(VK_SHIFT)`, patch 2640; SDL: `e->key.mod&SDL_KMOD_SHIFT` in
`input_sdl.c`, covered by its self-test); plain F9 stays the FPS counter.
`src/diagnostics/pc_debug_panel.h` holds value-only readings, per-frame
rates and the line format; `pc_debug.c` stores them under `debug_lock`:
`pc_debug_render` (render thread, one HLE reading per frame: cumulative UP/
indexed-UP/VB/indexed-VB draws, `hle_d3d8_texture_binds`, and the 5 s swap
timing window, whose restart is handled), `pc_debug_mapmaker_due/publish`
(game thread, `pc_mapmaker_panel.inc`), refresh at most every 250 ms, and
`pc_debug_panel_text` (presentation thread, 0 lines while off).
`developer_panel_lines` in `pc_developer_sidebar.inc` draws them as a box plus
mono labels when the sidebar is closed (re-fitted only when text changes), on
RHI and legacy GL (GL lacks the HLE reading: frame split/draws show "--").
Frame split: work = rest - present, present, swap, wait (gate). Each toggle
logs one `[PC-DEBUG] MapMaker panel:` line, at most once a second, not under
`TSFP_DEBUG_QUIET=1`. Tests: `tests/diagnostics/test_debug_panel.py` (model on
all hosts, Windows store, 10 mutations, wiring), `developer_tools.c` panel
lines. Guest reading: [limitless](limitless.md#mapmaker-debug-panel-shiftf9).

## Tests

MapMaker debug panel tests (runtime-patches CI): `tests/diagnostics/test_debug_panel.py` (model all hosts; Windows compiles `pc_debug.c` with stubs for the store/toggle log; mutations) and `tests/mapmaker/test_mapmaker_panel.py` (guest reading). Tail gains2640. Controller profile: [continuation](build-test-continuation.md#controller-profile-guards).

MM restart/skelvis/aim/meter/bossroom+piteye:-bosses;Disco/props/car+view/tank/creature+pose/portals+rays/roof(no seal)/astar/*pad/room*/pit/report(raised):test_mapmaker_<x>;tiles:limits,pad*;tools/testing/map_report.py:test_map_report_tool;logic Delay+map_report --logic:test_mapmaker_logic.

## Subsystem row

Moved from the [systems](systems.md) subsystem table:

| Limitless fork (MapMaker+Disco, scope wheel; `pc_display_menu.h`/`pc_menu_mouse_guest.inc` stubs; rooms 1023+tree+portal passes/light spread/ray step/query/roof two-sided+open view (room diag, box areas, all 2-sided)/route A*/nearest+random pad/pads 16K+lazy groups/objs 2047/links/props/body/bossfloor/walk/bossdraw/boss meter+breath/AIobj:limitless-*; story props (mem-pak textures, lift)/vehicles (lift, reticle)/weapons/robots/tank TNT/creatures+robots(sight+pose gate)/pit(+satellites, eye, sound): limitless-bosses; level-start map report+raised caps: limitless-report; action Delay (logic area, 5 hooks): limitless-logic) | [limitless.md](limitless.md) | `tests/mapmaker/`, `tests/input/test_scope_wheel.py` |

## Relocated actor tables in network code

`config/guest-code-patches/actors-list.json` moves the live character list 0x007A3580 to 0x00900100 and the AI table 0x007A2D00 to 0x00900200 for every guest reference, in every mode. Host code that indexes them uses `PC_GUEST_ACTOR_LIST` / `PC_GUEST_AI_TABLE` (guarded defaults in `src/network/pc_network_bot_lifecycle.inc`, `pc_arcade_join_guest.inc`, `pc_story_npc.inc`; `pc_network_bots_revive` in `pc_display_menu.c`). Fixtures that model the stock layout (`tests/network/bot_lifecycle.c`, `arcade_join_guest.c`, `story_lobby.c`) define the stock addresses first. `tests/mapmaker/test_mapmaker_actors.py::test_host_code_follows_relocated_actor_tables` keeps the defaults equal to the relocation and rejects stock-address indexing in those files. The roster count 0x0074C37C and its 32-entry cap in network sessions are unchanged.

Music slider fixtures: `pc_display_menu_audio_tick` (fork) wraps the stock audio tick and asks `sub_00074720`/`sub_00071A60` for the focused item, and the Music bar draws position/`MENU_MUSIC_TICKS` (24), so `tests/graphics/fov_settings.c` answers "nothing focused" and expects 8/24 for a stock value of 8.

## Other systems pointers

Battle Mech mouse: [routes, guards, logs](mounted-mouse.md#battle-mech-mouse-425). [Developer Tools integration](developer-tools-integration.md).

Systems map: MM chain/memory: [limitless](limitless.md). Boss/doors/holds/aim/anim/restart heap/view/sand: same

Crash capture: with the helper armed, Windows stderr is buffered (`pc_crash_buffer_log`, patch 2630): a 100 ms flusher thread, and the crash filter has it flush before the capture; `TSFP_LOG_UNBUFFERED=1` restores unbuffered writes.

Display and HUD fork sections: [limitless](limitless.md).

Fork versions of shared [systems](systems.md) rows:

| Bindings, input prompts | `src/input/pc_controls.c`, `pc_input.c`, `pc_bindings.inc`, `pc_prompts.c` (MM table key: limitless-caps) | `tests/input/pc_controls.c`, `tests/input/pc_prompts.c`, `docs/players/windows-settings.md` |
| Mouse/menu handling | `src/interface/pc_menu_mouse_guest.inc`, `pc_menu_choices.inc`, `pc_menu_sliders.inc`, `pc_menu_team.inc`, `pc_menu_briefing.inc` (briefings: fresh left click off footer = Confirm; MM lighting right-click: limitless) | `tests/input/test_menu_mouse.py`, `test_terminal_mouse.py`, `test_ui_layout.py` |
| Auto-look / camera / cutscene | `src/gameplay/pc_auto_look_guest.inc`, `pc_camera.h`, `pc_cutscene.h`, `pc_ui_layout.h` | `tests/gameplay/test_auto_look.py`, `test_cutscene_letterbox.py`, `test_camera_feed.py`, `test_camera_widescreen.py` |
Audio: `src/hle/hle_dsound*`, `hle_xmv*`; [beta](../development/positional-audio.md), [own, trim, ticks](custom-music.md).

Fork lines moved from [build-test](build-test.md):

[Save-folder checks](save-locations.md): storage, picker ownership, copy/restore/UI tests; both builds use SaveLocations.cs, [MapTransfer](map-transfer.md) (1023).

[Jingle/imported music, gain, slider ticks](custom-music.md).

MM caps/props/obj:limitless-caps; tests: [limitless-shared](limitless-shared.md#tests)

Mech cockpit/input+log: test_mounted_mouse.py [scope](mounted-mouse.md#battle-mech-mouse-425)

MM boss/hold/aim/anim/sand/rooms:limitless

Mac-network subset: no `-meter-story-chain`.
