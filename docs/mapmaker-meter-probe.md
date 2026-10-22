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

## Enable it

Use the bounded Test Lab runner. It strips inherited `TSFP_*` variables, so the
probe must be requested on its command line:

```powershell
.venv\Scripts\python.exe tools\testing_profile.py --play --seconds 600 --mapmaker-probe 3
```

| Flags | Records |
| --- | --- |
| `1` | Each meter call: caller return address, scene, `ecx`, six stack words; each bar's fill fraction, rectangle and colours |
| `3` | Also 64-byte snapshots of `ecx` and the first four stack words when they point into guest RAM (candidate budget structures) |
| `7` | Also bars drawn outside the meter painter while scene 100 is active (use if the meter shows nothing) |

The log is `private\test-saves\quick-test\mapmaker-probe.log`. It is truncated
when the game starts, so copy it before the next run. The runner recycles only
the `Partition*.img` files and never removes the log.

Manual use outside the runner (still with isolated paths) is the pair
`TSFP_MAPMAKER_PROBE=<flags>` and `TSFP_MAPMAKER_PROBE_LOG=<file>`; without a
log path, output goes to stderr (`game.err` under the runner).

## Reading the log

```
[mapmaker-probe] v1 flags=3 read-only; meter sub_0030C210, bar sub_000814B0; output …
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
