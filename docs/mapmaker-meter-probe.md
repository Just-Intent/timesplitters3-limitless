# MapMaker meter probe (experiment E1)

A read-only debug log of the MapMaker capacity meter. It records what the
original meter painter `sub_0030C210` draws through the bar primitive
`sub_000814B0`, so we can measure how placements change the meter before any
limit is touched. It is off by default and never writes game memory, saves or
settings.

Source: `src/pc_mapmaker_probe.inc`; wrapper patch
`patches/*-mapmaker-meter-probe.patch`; fixture `tests/mapmaker_probe.c`.
Background: `research/capacity-constraints.md`.

## Build (Windows)

The patch adds a new guest wrapper, so the first build after pulling it
regenerates the guest C. `tsfp.ps1 -Action Build` detects this automatically
from the generator fingerprint; expect a longer build once.

```powershell
powershell -NoProfile -File tsfp.ps1 -Action Build
```

If Smart App Control blocks the build's test programs, add `-CompileOnly`
(see `docs/building.md`). That executable is untested, and Smart App Control
may block it as well; run the experiment on an unrestricted Windows test machine.

## Enable it

Use the bounded Test Lab runner. It strips inherited `TSFP_*` variables, so the
probe must be requested on its command line:

```powershell
.venv\Scripts\python.exe tools\testing_profile.py --play --seconds 600 --mapmaker-probe 3
```

Flags add together (for example `11` = 1 + 2 + 8):

| Flag | Records |
| --- | --- |
| `1` | Each meter call: caller return address, scene, `ecx`, six stack words; each bar's fill fraction, rectangle and colours |
| `2` | 64-byte snapshots of `ecx` and the first four stack words when they point into guest RAM (candidate budget structures); use with `1` |
| `4` | Bars drawn outside the meter painter while scene 100 is active (use if the meter shows nothing) |
| `8` | While MapMaker runs: each menu the game lays out (`sub_00073750`): its caller, descriptor, state, rectangle and every item's type, flags, callback, rectangle and 64 raw bytes, plus each left click's pointer position in game-UI coordinates and the item it hit (`hit=0` means none). Use this for the toolbar click investigation (MM-4) |

The log is `private\test-saves\quick-test\mapmaker-probe.log`. It is truncated
when the game starts, so copy it before the next run. The runner recycles only
the `Partition*.img` files and never removes the log.

### Direct launch (complete block)

Paste into a **new** PowerShell window. It uses an isolated save folder,
turns generated-function tracing off (otherwise startup can stall; MM-2),
waits for you to quit, then lists the probe log:

```powershell
$root = 'C:\Dev\timesplitters-limitless'
$run  = "$root\private\test-saves\mapmaker-probe"
New-Item -ItemType Directory -Force "$run\saves" | Out-Null
if (!(Test-Path "$run\controls.json")) { Copy-Item "$root\tests\fixtures\pc-controls.json" "$run\controls.json" }
if (!(Test-Path "$run\display.ini")) { Set-Content "$run\display.ini" 'TSFP_DISPLAY_V1 0 0' }
if (!(Test-Path "$run\runtime.ini")) { Set-Content "$run\runtime.ini" '# Isolated test runtime' }
$env:RECOMP_GAME_DIR = "$root\private\game";  $env:RECOMP_SAVE_DIR = "$run\saves"
$env:RECOMP_TRACE_BUDGET = '0';                $env:RECOMP_INPUT_CONFIG = "$run\controls.json"
$env:TSFP_DISPLAY_CONFIG = "$run\display.ini"; $env:RECOMP_DISPLAY_CONFIG = "$run\runtime.ini"
$env:TSFP_MAPMAKER_PROBE = '11';               $env:TSFP_MAPMAKER_PROBE_LOG = "$run\mapmaker-probe.log"
$exe = "$root\build\windows\Release\futureperfect_recomp.exe"
Start-Process $exe -WorkingDirectory (Split-Path $exe) -Wait `
  -RedirectStandardOutput "$run\game.out" -RedirectStandardError "$run\game.err"
Get-Item "$run\mapmaker-probe.log" | Select-Object FullName, Length, LastWriteTime
```

The save folder persists between runs, so the profile created on the first
run is reused; the log is overwritten each run.

Manual use outside the runner (still with isolated paths) is the pair
`TSFP_MAPMAKER_PROBE=<flags>` and `TSFP_MAPMAKER_PROBE_LOG=<file>`; without a
log path, output goes to stderr (`game.err` under the runner).

## Reading the log

```
[mapmaker-probe] v2 flags=3 read-only; meter sub_0030C210, bar sub_000814B0; output …
[mapmaker-probe] t=123456 meter call=812 repeats=37
 caller=0x0030xxxx scene=100 ecx=0x…… args=0x…… 0x…… …
  dump ecx@0x……: 16 dwords
  bar[0] ret=0x0030C2xx fraction=0.734000 (0x3F3BE76D) rect=x,y,w,h colours=…
  bars=1
```

- A record is written only when its content changes, at most four per second.
  `repeats` is how many meter draws since the previous record were identical
  or rate-limited.
- `fraction` is exactly what the game passed to the bar (float and raw bits).
- `caller` identifies who draws the meter; `bar[n] ret` is the call site inside
  the painter. Record both in `research/capacity-constraints.md`.
- The file stops at 4 MiB with a "log cap reached" line.

## Repeatable experiment protocol

Each run: one action sequence, one log. Wait about one second between steps so
each state gets its own record. Write the step list with timestamps in a
notebook so log records can be matched to actions.

1. Start the runner with `--mapmaker-probe 3`, open MapMaker, create a new map
   with default settings. Note the starting fraction.
2. **Tiles, same type:** place 1, 2, 5 and 10 copies of one tile.
3. Undo back to empty; confirm the fraction returns to the start value.
4. **Tiles, different types:** place 5 different tiles; compare with step 2.
5. Repeat steps 2-4 for **items**, then **lights**, **Story AI**, **logic**
   (one trigger, then one action), and **player starts**.
6. **Limit:** fill one category until placement is refused; note the fraction
   and the step at which it stops. Note whether refusal happens before 1.0.
7. Save the map once in a fresh session and note the snapshot values (later
   save-format work, H-SV1).

Exit without saving unless the step asks for it. Keep all tests in the
disposable Test Lab copy; never use `private\saves`.

### Toolbar click capture (flag 8, MM-4)

In the same session, after the meter steps:

1. With the editor open and the pointer visible, click once on each toolbar
   icon (tile, item, light, AI, logic, settings) and on the left and right
   arrows, about one second apart, in that order. Note the order.
2. Click once on something that *does* respond, for comparison.
3. Open and close one category's item list, so its layout is captured too.

The log then shows which menus exist on the editing screen, whether the icons
are items of that menu (and of which type), and where each click landed. If a
click line shows `hit=0x00000000` while a `menu-layout` record lists an item
whose rectangle contains that pointer position, the icons are items the PC
layer does not yet treat as clickable. If no record contains the icons, they
are drawn outside the menu engine and need a different hook.

The log does not contain personal data, but it does contain guest memory
values; share it with the project only, and do not commit it.

## Recording results

For each run, add to `research/capacity-constraints.md`: build commit, flags,
step list, the fraction after each step, `caller` / `bar ret` addresses, and any
snapshot words that changed in step with placements (candidate counters).
Copy only these values; do not commit raw logs or guest memory dumps.

## Mac

The wrapper and probe are shared and also built for native Mac
(`tools/test-native-mac.py --unlocked-profile --mapmaker-probe 3`, log in
`private/test-saves/native-mac/`). Mac compilation and runs are pending; see
MAC-060.
