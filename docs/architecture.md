# Architecture (MapMaker reconnaissance view)

Status: reconnaissance, 2026-10-22, branch `mapmaker-recon` from main `141f440`.
Upstream runtime `nemonicicon/ts3FPrecomp` pinned at
`d90c92988a9f99d0580d2ca3f7efd6ec3a7b7845` (`tsfp.ps1:26`). Paths written
`runtime:` are relative to the ignored checkout `external/ts3FPrecomp`; `HEAD:`
means pristine upstream, `WT:` means after the wrapper's patch stack is applied.
For day-to-day navigation also read `docs/agent-context/README.md`.

This document describes the existing system. It does not describe planned
MapMaker changes; see `docs/development-roadmap.md`.

## 1. Projects and licenses

| Layer | Project | License / attribution |
| --- | --- | --- |
| Wrapper (this repo) | `TheDoffman/timesplitters-future-perfect-pc`: launcher, PC/Mac host code in `src/`, ~110 runtime patches in `patches/`, tools, tests | `LICENSE`, `NOTICE`, `LICENSES/` |
| Static-recompilation runtime | `nemonicicon/ts3FPrecomp` (cloned by `tsfp.ps1 -Action Setup`, `tools/build-native-mac.py`) | GPL-3.0 (`runtime:LICENSE`), plus `LICENSE.upstream-MIT`, `NOTICE`, `CONTRIBUTORS.md` |
| Runtime submodules | SDL3, XbSymbolDatabase, others under `runtime:third_party/` | Their own licenses; not checked out unless Setup runs |
| Game | Original Xbox US `default.xbe`, title `45410066`, SHA-256 `3cfd001a…816bc` | Proprietary; user-supplied, stays in ignored `private/game` |

Generated guest C is derived from the proprietary XBE and is never committed
(`.gitignore`: `/external/`, `/private/`, `*.xbe`).

## 2. Recompilation pipeline (confirmed)

`runtime:scripts/recompile.py:16-19, 68-158` runs four stages over one input,
`default.xbe`:

1. `tools.xbe_parser`: sections, imports, certificate (title ID).
2. `tools.disasm`: Capstone recursive descent plus linear sweep.
3. `tools.func_id`: library/CRT identification; `tools.xdk_symbols` adds XDK
   names using XbSymbolDatabase.
4. `tools.recomp`: lifts every function to C.

The wrapper invokes it as `recompile.py --work-dir private/pipeline --project
titles/futureperfect --trace-all-entries` (`tsfp.ps1:141-144`,
`tools/build-native-mac.py:110-112`). Output goes to
`runtime:titles/futureperfect/src/recomp/gen/`: `recomp_funcs.h`,
`recomp_NNNN.c`, `recomp_dispatch.c` (address → function table), `recomp_hle.c`
(`HEAD:tools/recomp/translator.py:1846-1851`).

Guest model (`runtime:docs/pipeline/04-lifting.md`):

- Every guest function becomes `void sub_XXXXXXXX(void)` named by its Xbox
  virtual address. Registers are thread-local globals (`g_eax`, `g_esp`, …);
  arguments live on the simulated guest stack.
- Guest memory access is `MEM32(a)` = host pointer `a + g_xbox_mem_offset`. The
  guest address space is one host file mapping (section 4).
- Indirect calls go through the dispatch table (`RECOMP_ICALL`).

Per-title inputs: `runtime:config/seeds/45410066.json` (22 runtime-observed
function starts) and `runtime:config/extra_symbols/45410066.json` (XMV symbols
XbSymbolDatabase misses; the wrapper's stack modifies it).

## 3. Hooking mechanisms (confirmed)

| Mechanism | How | Cost |
| --- | --- | --- |
| Replace/wrap by address | Define `sub_X` in `recomp_manual.c`; declare `extern void sub_X_gen(void)` to keep the lifted body callable (`HEAD:tools/recomp/manual_scan.py:17-23`). The patched file has 116 such wrappers. | A **new** wrapper changes generated output → regenerate (needs XBE). Editing an existing wrapper's body → rebuild only. |
| HLE by XDK name | `HLE_EXPORT(Name)` / `HLE_ORIGINAL` / `HLE_CALL_ORIGINAL` (`HEAD:src/hle/hle.h:1-64`) | Same regeneration rule for new names |
| Runtime manual lookup | `recomp_lookup_manual(va)` (`HEAD:recomp_manual.c:190-201`) | Empty upstream |

Wrapper convention: patches add a thin wrapper in `recomp_manual.c` that calls a
shared host function in root `src/` (for example `pc_display_menu_mouse`,
`pc_controls_apply_mouse`). Guest-memory policy lives in testable `.inc`/`.h`
files, compiled into asset-free C fixtures under `tests/`.

## 4. Game runtime and memory (confirmed)

- Xbox kernel, threads and memory: `runtime:src/kernel/`. NV2A/GPU:
  `src/nv2a/`. D3D8 frontend + RHI backends (D3D11, Vulkan, legacy GL):
  `src/d3d/`. HLE audio/input/movies: `src/hle/`.
- Guest RAM is **64 MiB**. `XBOX_TOTAL_RAM` is 64 MiB and `XBOX_DEVKIT_RAM`
  128 MiB (`runtime:src/kernel/xbox_memory_layout.h:47-48`); neither
  `xbox_SetTotalRam` nor `xbox_SetMapSize` is called anywhere in upstream or the
  wrapper (only their definitions at `xbox_memory_layout.c:116,122`).
- Future Perfect's image extends to about `0x0089CD60`, so the runtime shifts the
  8 MiB stack and the heap upward (`g_xbox_low_shift`; header comments
  `xbox_memory_layout.h:420-434`). The heap is a bump/first-fit allocator backing
  `NtAllocateVirtualMemory` and `MmAllocateSystemMemory`.
- Contiguous (GPU) allocations use a **separate** 64 MiB mapping at
  `0x80000000` ("Deliberately NOT a view of the 64 MB RAM mapping",
  `xbox_memory_layout.c:2548`). This gives more GPU-side headroom than real
  hardware had.
- The header warns that raising total RAM broke another title during CRT init;
  enlarging only the map (`xbox_SetMapSize`) is the documented safer lever
  (`xbox_memory_layout.h:57-76`). Neither is tested for Future Perfect.

## 5. Input subsystem (confirmed)

Guest side: HLE replaces `XInputGetState` and the device enumeration functions
(`runtime:src/hle/hle_input.c:155-324`). The guest receives a standard 22-byte
`XINPUT_STATE`: digital `wButtons`, eight analog buttons, four thumbstick axes.
There is no keyboard or mouse device visible to the game.

Host side:

```
Windows WndProc ─ pc_controls_message (src/pc_controls.c:333-424)
   keys / mouse buttons → 256-entry key array (pc_input_key)
   WM_INPUT raw mouse   → pc_input_motion
   wheel                → queued F23/F24 presses (src/pc_input.c:43-65)
Mac SDL events ─ tsfp_native_input_event (src/native/input_sdl.c:184-293)

XInputGetState → recomp_input_host_sample_port (input_host.c:276)
   → recomp_bindings_sample (input_bindings.c:~839, + src/pc_bindings.inc)
   → per-control binding (config/pc-controls.default.json) → XINPUT_STATE
```

The wrapper adds three non-pad paths that write guest state directly:

1. **Mouse-look**: `pc-controls.patch` wraps `sub_000D2B80` and calls
   `pc_controls_apply_mouse(MEM32(0x007B0C7C) player, MEM32(0x007B0CC0) fov, …)`
   (`patches/pc-controls.patch:229`). Yaw/pitch are floats at player+0x100/+0x104
   (`src/pc_camera.h:14`). Mouse movement is not converted to stick values.
2. **Menu mouse**: `src/pc_menu_mouse_guest.inc` hit-tests the generic menu
   engine's items and injects highlight/confirm/back edges. Context test
   `menu_mouse_context()` (lines 49-54): scene `0x0079094C` > 100 (frontend) or
   pause flag bit 0 at `*0x00761C48`.
3. **Terminal pointer**: `src/pc_terminal.h` moves an in-world terminal cursor
   when the actor state at player+0x114 is `0x2000`.

## 6. Save handling (confirmed)

- No host implementation of XAPI saves: the game's own lifted XAPI code runs
  over the kernel's `Nt*` file calls. Files are plain host files; there is no
  FATX layer (`runtime:src/kernel/kernel_path.c`, `kernel_file.c`).
- `patches/save-routing.patch` maps `Partition1\UDATA` → `<save>/UserData` and
  `Partition1\TDATA` → `<save>/TitleData`. `<save>` is `RECOMP_SAVE_DIR`.
- Profile layout: `UserData/45410066/<12-hex>/tsftrpft` + `SaveMeta.xbx`, body
  0x1F54 bytes plus HMAC-SHA1 with a zero key (`tools/testing_profile.py:16-83`).
  Signing keys are zero in this runtime (`kernel_bridge.c:261-271`), so saves
  from a console or xemu would not validate.
- `Partition0..5.img` are sparse placeholders, not save storage.
- Free-space reporting comes from the host disk; there is no quota
  (`kernel_file.c:582-596`, `:1123`).

Where MapMaker maps are stored is **not yet observed**; see
`research/mapmaker-investigation.md`.

## 7. Platforms

| Host | Title build | Notes |
| --- | --- | --- |
| Windows x64 | Supported (`tsfp.ps1`, MSVC) | D3D11 RHI; all PC input features |
| macOS Apple Silicon | Supported (`tools/build-native-mac.py`) | Vulkan (MoltenVK) or legacy GL; SDL input adapter |
| Linux | **Not supported** | Patched title CMake stops with `FATAL_ERROR` (`WT:titles/futureperfect/CMakeLists.txt:97`); upstream `main.c` is Win32-only; POSIX fault handler is a stub; `input_bindings.c:834` "no keyboard source off Windows yet". Runtime libraries have POSIX branches. |

MapMaker work must be shared-source first (root `AGENTS.md`), with Windows as
the primary validation host and a `MAC_PORT_BACKLOG.md` entry for Mac.
