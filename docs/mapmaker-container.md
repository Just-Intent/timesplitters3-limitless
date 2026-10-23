# MapMaker map container: 32 KB -> 512 KB

Status 2026-10-23: implemented as a guest-code patch table, C hooks and an
asset-free fixture. Not built, not run in the game. Everything below is from
the US XBE disassembly (`private/pipeline/disasm`) and the runtime source.
CONFIRMED = read at the address given; INFERRED = reasoning, not observed.

Files: `config/guest-code-patches/container.json` (33 same-length rewrites,
`check-tables.py` prints "table verified"), `src/pc_mapmaker_container.inc`
(hooks), `tests/mapmaker_container.c` + `tests/test_mapmaker_container.py`
(fixture, 21 mutations, table/hook consistency).

## Sizes

| | Stock | New |
| --- | --- | --- |
| Buffer N (`[0x00784014]`) | 0x8000 | 0x80000 (512 KB) |
| Largest body B (N - 0x20, the slack holds the 20-byte signature while saving) | 0x7FE0 | 0x7FFE0 |
| Editor limit L (`used + cost < L`, so `used + 4 <= B`) | 0x7FE0 | 0x7FFDC with `TSFP_MAPMAKER_LIMITS=1`, else stock |
| Undo shadow `[0x00765E38]` (Level heap, editor) | 0x8000 | N |
| Undo diff table `[0x00765C2C]` (Level heap, editor), 4 x body | 0x1FF80 | 0x1FFF80 (2 MB) |
| Undo ring `[0x00765C24]`, 2 x buffer | 0x10000 | 0x100000 |
| Save file body tiers | 0x7FE0 | 0x7FE0, 0xFFE0, 0x1FFE0, 0x3FFE0, 0x7FFE0 |

No format change: chunk sizes and the used field (map info data `+0x248`) are
dwords, version 0x12D is kept (CONFIRMED, see `logs/limits/container.md` 2.2).

## Where the buffers live (and why)

- Eternal heap (heap 2 of `sub_0003DFA0`, case 0x0003E063): bump allocator
  with no bound check over 0x9DC00 bytes in all (`sub_0003DF00` 0x0003DF04,
  CONFIRMED). It cannot hold N + 2N = 1.5 MB.
- Level heap (heaps 0/1): `sub_0003E650`, run at each level load, resets it
  and zeroes it (0x0003E6AF..0x0003E6F1, CONFIRMED). The container must keep
  the map from the editor (scene 102) into play (scene 100) and back, so it
  cannot live there. The play-time loaders read `[0x00784014]` directly
  (0x00085981, 0x0008A7BC, 0x0023F3A0, 0x0023FD14, 0x002405AB, 0x00240D47,
  CONFIRMED): there is no separate play copy. System Link receive copies into
  it as well (0x00355048).
- The undo shadow and diff table are allocated by `sub_003366A0` at each
  editor init from the Level heap (CONFIRMED); they stay there (2.5 MB of the
  ~14 MB measured free in the editor, INFERRED from the earlier report).
- The container and the ring come from the runtime heap, `xbox_HeapAlloc`
  (`external/ts3FPrecomp/src/kernel/xbox_memory_layout.c`, the allocator
  behind `MmAllocateSystemMemory`, `kernel_bridge.c`): zeroed, never freed,
  0x00F80000 up to the end of guest RAM. The game's own 47,104,000-byte arena
  (`[0x004C0450]` = 0x02CEC000, allocated at 0x00023CE9 by a four-argument
  physical allocation, CONFIRMED) is in the separate contiguous window, so it
  does not compete. Each buffer is allocated once and reused for the whole
  run; the hooks abort with a message if the allocation fails (every constant
  already assumes the large buffer). Headroom of that heap is INFERRED (not
  measured).
- `[0x00784014]` is written only by `sub_002FE200` and `sub_002FE230`;
  `[0x00765C24]` only by `sub_00336600` (CONFIRMED by a full scan).

## Guest-code patches (`container.json`)

- Whole-body zero/copy, 0x1FF8 -> 0x1FFF8 dwords: 0x00300706 (compiler; every
  compile target is the container or the shadow, CONFIRMED at the nine call
  sites), 0x0032A539, 0x0032B50F, 0x0032BC1A, 0x001CC3E4, 0x002D6E77,
  0x00356BD8, 0x0033774C (container -> shadow copy).
- Undo: 0x0033772D zero to buffer end 0x8000 -> N; 0x00336656 shadow and
  0x0033664F ring resets; 0x003366C0/0x003366D4 shadow allocation and zero;
  0x003366A3 diff table 4B; 0x0033741F (zero B dwords) and 0x00337438 (B/2
  entries) in `sub_00337410`; 0x00337352 ring wrap 2N. The diff writer
  `sub_003367A0` has no table bound; the stock 4 x body ratio is kept.
- Chunk walks `base + 0x7FE0` -> `base + B`: 0x00085986, 0x00085A6C,
  0x0008A7C7, 0x0008AB2F, 0x0023F3AE, 0x0023F764, 0x0023FD20, 0x0023FE31,
  0x002405B5, 0x002407FB, 0x00240D4F, 0x002423C4, 0x0032A5CA (chunk finder),
  0x0032A6FC, 0x00337E1A (`sub_00337D80`, no terminator check; reached through
  the callback table at 0x0052EBA4, INFERRED to get the container).
- Save list flag 0x001907A0: `seta` on body > 0x8000 sets entry `+0x68`,
  which changes how `sub_0032BEA0` lists the slot in modes `[0x00765A80] &
  0xC` (0x0032C18A, 0x0032C23C). The compare becomes > 0x80000 so larger-tier
  map files are listed like stock maps (meaning of the flag INFERRED).
- Left at stock on purpose: the five placement compares (0x00300696,
  0x003006AB, 0x0030366D, 0x00303936, 0x00304442; gated by a hook below), the
  load and save sizes in `sub_00190530`/`sub_00190900` (tiers chosen by
  hooks), the allocation sites in `sub_002FE200`/`sub_002FE230`/`sub_00336600`
  (replaced), the free-space estimates (0x0032A3EC, 0x0032B5BB, 0x00191F74:
  `sub_00025BE0(0x8000)` block counts; on PC `U:` is a host folder and free
  space comes from the host disk, `kernel_file.c` NtQueryVolumeInformationFile,
  so they cannot refuse a save; the shown block count stays the stock one),
  System Link (one 0x8000 message, see below) and `sub_0019D510` (Xbox Live
  packing, dead on PC).

Why hooks for the 16-bit fields: the verifier refuses any rewrite that
changes an instruction's length or mnemonic, and every 16-bit site
(`mov cx, word`, `movzx`, `add word`, `66 A3`) needs one. So the word at
0x00765AEE (entry `+0x66`, used + 4) stays 16-bit and the code that depends
on it is corrected around it.

## Hooks (integrator: add each to `recomp_manual.c` as the stage A wrappers)

WRAP sub_002FE200 -> pc_mapmaker_container_frontend_init
WRAP sub_002FE230 -> pc_mapmaker_container_editor_init
WRAP sub_00336600 -> pc_mapmaker_container_undo_ring
WRAP sub_00190530 -> pc_mapmaker_container_load
WRAP sub_00026740 -> pc_mapmaker_container_save_async
WRAP sub_00025710 -> pc_mapmaker_container_save_sign
WRAP sub_000263E0 -> pc_mapmaker_container_save_sync
WRAP sub_001504F0 -> pc_mapmaker_container_crc
WRAP sub_00150560 -> pc_mapmaker_container_crc_check
WRAP sub_003006C0 -> pc_mapmaker_container_compile
WRAP sub_002FE0C0 -> pc_mapmaker_container_compiled_size
WRAP sub_0032A5B0 -> pc_mapmaker_container_find
WRAP sub_00353FE0 -> pc_mapmaker_container_net_send

All are cdecl (plain `ret`). `#include "pc_mapmaker_container.inc"` after
`pc_mapmaker_limits.inc` in `src/pc_menu_mouse_guest.inc` (it uses
`mapmaker_limits_read`/`mapmaker_limits.limits`). It declares
`extern uint32_t xbox_HeapAlloc(uint32_t, uint32_t)` (runtime symbol, linked
into the game on Windows and Mac). The wrappers keyed by return address leave
every other caller on the original.

- `frontend_init` (replaces `sub_002FE200`, called by `sub_002D6E10` when
  `[0x00784014]` is 0): allocate once, set `[0x00784014]`, zero, then
  `sub_00336600` (the original tail-calls it).
- `editor_init` (`sub_002FE230`, from `sub_001CDC50`): sets the meter constant
  when the limits are on; if `[0x00784014]` is 0 installs the buffer and ring
  first; then the original (it allocates only when the pointer is 0).
- `undo_ring` (replaces `sub_00336600`): ring 2N, zeroed, `[0x00765C28]`,
  `[0x00765C30]`, `[0x00765C20]` cleared, as the original.
- `load` (replaces `sub_00190530`, only caller `sub_0032BA90` with entry
  0x00765A88 and `[0x00784014]`, 0x0032BBEA): the same calls and results
  (`sub_000827B0(1)`, flags 2/0xE from `sub_000827F0`, bit 4 cleared outside
  a network session, the second read with `| 0x14`, `[0x00762CBC]`), plus:
  zero the whole buffer first (a smaller map must not keep a larger one's
  tail), read the stock body first and each larger tier only when the reader
  `sub_00025280` returns 7 (file larger than body + signatures, its only
  source of 7: 0x0002536A; other failures come from `sub_00024900`), and
  store the 16-bit `used + 4` as stock (entry `+0x68` untouched). A file that
  reads but has no map info returns 7 (the original would dereference 0).
- `save_async`/`save_sign`/`save_sync`: in `sub_00190900` only (return
  addresses 0x001909E3, 0x00190A1F, 0x00190A54), replace the body 0x7FE0 by
  the smallest tier holding `used + 4` (CRC included), read from the buffer
  being saved. A map that fits saves exactly the stock body: same bytes,
  same signature. The writer truncates (sync path `CreateFile` OPEN_ALWAYS
  then `sub_0037CDCB` = SetEndOfFile via FileEndOfFileInformation, which the
  runtime implements, `kernel_file.c`), so a smaller save over a larger file
  leaves no tail. No other size limit was found in `sub_000263E0`,
  `sub_00026740`, `sub_00025710` (no length compare, CONFIRMED by scan).
- `crc` (`sub_001504F0` at 0x0032C71F): the save state (`sub_0032C410`,
  0x0032C702..0x0032C731) truncates the compiled size to 16 bits, CRCs that
  many bytes and stores the CRC at `buffer + low16`. For `used > 0xFFFF` the
  hook CRCs `used` bytes, stores the CRC at `buffer + used` and returns the
  word already at `buffer + low16`, so the caller's store is a no-op.
- `crc_check` (`sub_00150560` at 0x0032B425, the load check in
  `sub_0032B400`): for `used + 4 > 0xFFFF` checks `used` bytes against the
  word at `buffer + used`; `used > B - 4` fails. Smaller maps: the original.
- `compile`/`compiled_size` (`sub_003006C0` at 0x0032C6FA, `sub_002FE0C0` at
  0x0032C6ED): the save fails when the 16-bit size is 0 (`test ax, ax` at
  0x0032C702), i.e. for a map of exactly k x 64 KB; such a result gets a
  non-zero low half (4) and `crc` works from the real size. The compiler
  returns `ebx` = map info `+0x248` (0x00301A53, 0x00301A84, CONFIRMED).
- `find` (`sub_0032A5B0`) only at the four placement checks (return
  addresses 0x00300685 `sub_00300670`, 0x003035D2 `sub_00303460`, 0x00303927
  `sub_003037A0`, 0x00304432 `sub_00304330`), only with
  `TSFP_MAPMAKER_LIMITS=1`: each reads `[info + 0x248]`, adds a cost and
  compares signed with 0x7FE0; the hook returns a view whose used is lower by
  L - 0x7FE0 (a scratch dword after the buffer), so the stock compare tests
  `used + cost < L`. With the limits off the editor is exactly stock. The
  no-container branches (cost alone) keep the stock 0x7FE0.
- `net_send` (`sub_00353FE0` from `sub_003563C0` 0x00356455 and
  `sub_003563A0` 0x003563B4, type 0x30, the container, 0x8000 bytes): a map
  with `used + 4 > 0x7FE0` is not sent (logged once). This is a backstop;
  the refusal itself is at load.

Meter: `sub_0030C180` multiplies by the float at 0x0049F7AC (1/32736); its
only readers are 0x0030C191 and 0x0030C1BA and no data word points at it
(CONFIRMED by a full scan of code and of the XBE). With the limits on it is
set to 1/L (`mapmaker_container_editor_apply`, called by `editor_init`).
`sub_0030C210` draws max(that fraction, editor `+0x270`), so the meter shows
the container term without a new term in `pc_display_mapmaker_limits`.
Nothing has to be called from the limits wrapper; calling
`mapmaker_container_editor_apply()` there as well is harmless.

## System Link

The host sends the container as one type-0x30 message of 0x8000 bytes
(0x0035644C, 0x003563AB); the fragment header has a byte count and index
(max 255 x 239 bytes) and the receiver pool holds 200 fragments, so nothing
larger than the stock size can travel (`logs/limits/container.md` 1.8). No
multi-part transfer was added. In a network session (`sub_0035E960`,
`[0x00774024] == 1`, the same test the stock load uses for its flags) the
load refuses any file that needs a larger tier with error 7, which
`sub_0032BA90` already shows through `sub_0032B540` (the same result the
stock game gives for such a file). `sub_00356BB0` (15 network
session callers) zeroes the container when `[0x0076B158]` is 0 and then sets
it (0x00356BB9..0x00356BE3, CONFIRMED), so a session normally starts without
the editor's map (INFERRED from the callers' names, not traced). Stock-size maps keep the stock
wire format and stay compatible with other builds. Older builds refuse
larger-tier files with error 7 too.

## Compatibility

- Maps that fit (used + 4 <= 0x7FE0) save byte-identical to stock: the first
  0x7FE0 bytes of the container, CRC position and signature are unchanged.
- With the limits off a map whose `used` is 0x7FDD..0x7FDF (possible with the
  stock limit) now saves in the 64 KB tier with a correct CRC; the stock game
  saved a body that lost its CRC.
- Old maps load unchanged (stock body read first).

## Open risks / to test in play

- Runtime heap headroom for 1.5 MB (INFERRED large; not measured).
- Level heap in the editor: shadow + diff table 2.5 MB (stock 160 KB).
- Save/load round trip of a map past 64 KB, then past 256 KB; reload in the
  editor; play it (scene 100); undo across a large edit.
- System Link host with a large map: expect the load error; with a small map,
  normal play with an older build as peer.
- Meter and "map full" with `TSFP_MAPMAKER_LIMITS=1`; without it the editor
  must stop at the stock size.
- The other editor limits (200 pieces, memory estimate, cover pads) still
  bind long before 512 KB.
