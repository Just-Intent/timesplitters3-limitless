# Build and test (MapMaker work)

This summarizes the existing supported procedure and what was reproduced during
reconnaissance. The authoritative guides remain `README.md`, `docs/building.md`,
`docs/macos.md` and `docs/agent-context/build-test.md`.

## 1. Pinned inputs

| Input | Pin | Source |
| --- | --- | --- |
| Game executable | US Xbox `default.xbe`, title `45410066`, SHA-256 `3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc` | User's own disc, extracted to ignored `private/game` |
| Runtime | `nemonicicon/ts3FPrecomp` @ `d90c92988a9f99d0580d2ca3f7efd6ec3a7b7845` | `tsfp.ps1:26` |
| Python | 3.12+; `capstone==5.0.9`, `cmake==4.4.4`, `ninja==1.13.2`, `numpy==2.5.4`, `pefile==2024.8.26`, `pytest==9.1.1`, `Pillow==12.2.0` | `requirements.txt` |
| Windows toolchain | Visual Studio 2022, MSVC x64 | `docs/building.md` |
| Mac toolchain | Xcode CLT, Vulkan SDK from `config/vulkan-sdk.json` | `docs/macos.md` |
| FFmpeg | Verified download | `tools/fetch-video.ps1` |

## 2. Supported build procedure

Windows (from the checkout):

```powershell
powershell -NoProfile -File setup.ps1                       # first time
powershell -NoProfile -File tsfp.ps1 -Action Extract -Iso <path-to-your-iso>
powershell -NoProfile -File tsfp.ps1 -Action Build
# → build/windows/Release/futureperfect_recomp.exe
```

`-Action Build` applies the full patch stack (`tools/apply-patches.py`), checks
the generation fingerprint (`tools/recomp-generation.py`), regenerates guest C
when needed, configures MSVC and builds.

Native Apple Silicon:

```sh
.venv/bin/python tools/build-native-mac.py --setup   # first time
.venv/bin/python tools/build-native-mac.py           # add --generate if stale
# → build/macos/futureperfect_recomp
```

## 3. What was reproduced on 2026-10-22 (Linux recon host)

Host: Ubuntu 24.04 x86-64, GCC 13.3, Python 3.12.3, no sudo, no game data.

| Step | Result |
| --- | --- |
| Clone runtime and check out the pin | OK (`external/ts3FPrecomp`, 15 MB) |
| Create `.venv` with pinned `requirements.txt` | OK (via `uv`; system Python lacks `ensurepip`) |
| `tools/apply-patches.py external/ts3FPrecomp patches` | OK: "Local patches verified; 68 runtime file(s) updated." |
| Guest generation | **Not run**: needs the supported XBE |
| Title build | **Not possible on Linux**: see `docs/architecture.md` §7 |
| Asset-free tests (the CI native list + lightweight checks) | 377 passed, 53 skipped, 19 failed, 5 errors |

The failures and errors are all renderer/RHI/timing fixtures that CI compiles
only on Windows and macOS (for example `tests/texture_toggle.c:19: unknown type
name 'RhiFormat'`, `test_renderer_host`, `test_frame_clock`,
`test_terminal_depth`, `test_ghost_rendering`). They are host-portability gaps
of the test harness on Linux, not evidence of a regression. Skips are Win32/Darwin
adapters or tests that require the local supported XBE.
Input tests relevant to MapMaker (`test_menu_mouse`, `test_menu_text`
non-XBE parts, `test_quit_menu`, `test_ui_layout`) passed; `test_auto_look` and
`test_terminal_mouse` skip off Windows/macOS.

Exact command (from the checkout):

```sh
.venv/bin/python -m pytest <CI native list from .github/workflows/runtime-patches.yml> \
  tests/test_ci_runtime_scope.py tests/test_mac_backlog.py tests/test_repo_context.py \
  tests/test_patch_tracking.py tests/test_patch_updates.py -q
```

## 4. Test routes for MapMaker work

| Check | Asset-free? | Host |
| --- | --- | --- |
| Patch stack applies (`tools/apply-patches.py`) | Yes | Any |
| C fixtures for new guest-memory policy (pattern: `tests/menu_mouse.c` + `tests/test_menu_mouse.py`, with mutation variants) | Yes | Any with gcc/clang/MSVC; guest-memory guard tests need Windows/macOS |
| "Original US routine" tests: lift a real routine from the local XBE and run it against the production wrapper (pattern: `tests/test_crane_overlay.py:21-48`) | No: local XBE, skipped in CI | Windows/macOS |
| Bounded gameplay run with the disposable unlocked profile: `.venv/Scripts/python.exe tools/testing_profile.py --play --seconds 120` (Mac: `tools/test-native-mac.py --unlocked-profile --seconds 120`) | No | Windows / Mac |
| Scripted input: `TSFP_PC_INPUT_REPLAY` (Windows), `RECOMP_FAKE_INPUT`/`RECOMP_INPUT_SEQ` (runtime), `--test-keys` (Mac, synthetic SDL) | No | As noted |

Runtime diagnostics useful for MapMaker reverse engineering (Windows; they rely
on the fault handler, which is a stub on POSIX):

- `RECOMP_WATCH_WRITE=0x…` names the guest instruction that writes an address
  (`runtime:src/kernel/xbox_watchpoint.c`, `docs/technical/memory-watchpoints.md`).
- `RECOMP_DUMP_VA=addr:n` with `RECOMP_DUMP_EVERY`/`_AFTER`; `RECOMP_WATCH_VA`;
  `RECOMP_TRACE_ONLY`, `RECOMP_TRACE_ARGS` (`runtime:src/kernel/recomp_trace.c`).
- `RECOMP_PEEK` prints guest dwords as hex and float.

## 5. Isolation rules (from root `AGENTS.md`)

- Never use `private/saves` or personal `config/pc-*.json/ini` in tests.
  Every game run sets `RECOMP_SAVE_DIR`, `RECOMP_INPUT_CONFIG`,
  `TSFP_DISPLAY_CONFIG` and `RECOMP_DISPLAY_CONFIG` to disposable paths
  (`tools/testing_profile.py:138-143` does this).
- MapMaker save experiments must use a disposable save root and must diff its
  `UserData/45410066/` before and after.
- Recycle only `Partition0..5.img` after a pass; keep logs and captures bounded.
- Do not hand-edit generated C. New guest wrappers go into a new tracked patch,
  ordered after the stack it depends on, and require regeneration.
