# MapMaker objects area: turrets, object lists, pools, teleporters, tags

US Xbox build 45410066. This area raises the capacity behind MapMaker turrets,
teleporters and cover props, and adds guards so that the fixed tables involved
cannot overflow silently. Everything here comes from static analysis of the
disassembly. Nothing has been checked in play yet.

- Background research: `logs/limits/turrets.md` and `logs/limits/items.md`
  §2.2-2.5 (both ignored).
- Code: `config/guest-code-patches/objects.json` (byte rewrites and
  relocations) and `src/pc_mapmaker_objects.inc` (C hooks).
- Tests: `tests/mapmaker_objects.c` and `tests/test_mapmaker_objects.py`.

Tags used below: **CONFIRMED** means the instruction was seen at the given
address; **INFERRED** means it was reasoned, not seen.

## Limits reached

| Limit | Stock | Now | Where | Scenes |
| --- | --- | --- | --- | --- |
| Turrets per category (27/28/29) | 8 | 24 (editor, `TSFP_MAPMAKER_LIMITS=1`) | `mapmaker_categories[]` | editor |
| Turret gun / owner pools | 32 | 80 | `obj-turret-*` | all |
| System Link turret sync | slots 0..31 | slots 0..79 | `obj-turret-sync-slots` | all |
| Object list `0x00716148` (slot 9) | 30 | 128 at `0x008A0000` | `obj-list9-relocate` | all |
| Per-player perception array | 0x3E0 B | 0xA00 B (32 characters + 128 objects) | `obj-perception-*` | all |
| Slot-8 list `0x007161C8` | 50 | 200 at `0x008A0200` | `obj-list8-relocate` | all |
| Slot-0 list `0x007B4FC0` | 112 | 448 at `0x008A0600` | `obj-list0-relocate`, `obj-list0-clear` | all |
| Room-bucket buffer `[0x00715FB4]` | 0x14CD entries | 0x299D | `obj-bucket-*` | all |
| Engine object pool and twin | 550 | 1000 | `pc_mapmaker_objects_pool` / `_twin_pool` | 100, 102 (pools on) |
| Teleporters per colour (cat. 14-18) | 2 | 8 (editor) | `mapmaker_categories[]` + teleporter hook | editor / 100 |
| Logic tag entries `0x0075DAF0` | 250 | 1000 at `0x008A4400` + full-table guard | `obj-tag-entries-*` | all |
| Logic tag links `0x0075D320` | 250 | 1000 at `0x008A2400` | `obj-tag-links-*` | all |
| Logic tag groups `0x0075CE68` | 150 | 150 (unchanged) | - | - |
| Cover props (category 33) | 66 | 132 (editor) | `mapmaker_categories[]` | editor |

Two limits are unchanged:

- **Cover pads.** These stay limited in the editor by `pc_mapmaker_limits.inc`;
  another area owns that work.
- **Logic tag groups (150).** Relocating them fails the relocation check,
  because `sub_00240020` and its alias `sub_00240024` overlap
  (0x00240024..0x00240069). The checker sees every instruction twice
  ("has two relocated operands"). This is a limitation of the checker's scan,
  not of the game. When groups run out, `sub_0023FFA0` and `sub_00240360`
  already refuse cleanly (CONFIRMED: −1 return, caller bails).

## Hooks the integrator must add (`recomp_manual.c`)

Include `pc_mapmaker_objects.inc` after `pc_mapmaker_limits.inc` in
`src/pc_menu_mouse_guest.inc`. For non-display builds, add
`{ original(); }` stubs, as for the limits hooks.

    WRAP sub_0006CE80 -> pc_mapmaker_objects_turret_create
    WRAP sub_00245990 -> pc_mapmaker_objects_turret_init
    WRAP sub_0009E650 -> pc_mapmaker_objects_list_add
    WRAP sub_000A5CE0 -> pc_mapmaker_objects_register
    WRAP sub_000A5C00 -> pc_mapmaker_objects_pool
    WRAP sub_0008DBF0 -> pc_mapmaker_objects_twin_pool
    WRAP sub_0009CD00 -> pc_mapmaker_objects_teleporters
    WRAP sub_002402B0 -> pc_mapmaker_objects_tag

All of these functions are cdecl with a plain `ret` (CONFIRMED).
`sub_0006CE80` also takes `edi`, `ebx` and `esi` in registers; the hook leaves
them untouched.

## Evidence and design

### Turrets

- **Pool sizes and sync buffer.** The 32-slot pools appear in six places: the
  push sizes and clear dword counts in `sub_000B3BE0`/`sub_000B3A50`, and two
  `push 0x20` node counts (CONFIRMED).
- **System Link sync.** The loop in `sub_00359550` is unrolled four times
  (`cmp ebx,0x20` at 0x003596B3), so N must be a multiple of 4. It writes
  3 bytes per automatic turret, starting at frame offset 9 of a 0x104-byte
  frame: 9 + 3·80 = 249 bytes, which fits. The payload sent to `sub_00353990`
  is at most 245 bytes, under its 0x100-byte copy buffer and its byte-sized
  length (CONFIRMED). The receiver `sub_003596F0` indexes the pool with a byte
  slot (< 80). 80 is the largest value that fits; beyond it, `sub_00359550`
  would have to be reimplemented.
- **No overflow to address 0.** `sub_00245990` pops a gun and an owner without
  checking for null (`sub_00156E10` returns 0 when empty), so stock code would
  write a gun struct at guest address 0. Two hooks prevent this:
  - The level loader `sub_0006D7A0` tolerates a 0 return from `sub_0006CE80`
    (`test eax,eax; je` at 0x0006E821, CONFIRMED). The create hook skips the
    record when either list is empty, and only for that caller (return
    address 0x0006E818).
  - The other callers, `sub_0006D230` and `sub_00180920`, dereference the
    result. For them the init hook pushes a spare block from the reserved
    block onto the empty list: 8 guns at `0x008A9000`, 8 owners at
    `0x008AA000`, reused in turn. This is a last resort and is not reachable
    with stock content.
- **Not AI.** Turrets take no AI slot (call graph CONFIRMED; see turrets.md §3).
- **Known HUD quirk (stock, harmless).** The free list hands out the highest
  slot first, so the HUD loop in `sub_00349E40` scans the wrong slots. A
  larger pool widens the gap but changes nothing else.

### Object lists and perception

- **Slot-9 list.** The list at `0x00716148` has 15 references. They are
  relocated together with the three `0x007161C0` references, which belong to
  a separate scalar and are kept.
- **Perception array.** It is indexed by character id (×0x10) and by
  0x200 + list index (×0x10). Every reader loops up to the list count:
  `sub_000CF8D0`, `sub_000D0730`, `sub_00259930`, `sub_000D01C0` (CONFIRMED).
  The `push [reg+0xF74]` sites in `sub_0038Dxxx` belong to a different struct
  (fields +0x284, +0xEBC; INFERRED to be library code).
- **List guards.** `sub_000A5CE0(obj, slot, flag)` appends to the slot-0, -8
  and -9 lists with no bound (CONFIRMED). The register hook stops at the
  relocated capacity but still writes `obj+0x90` and `obj+0x154+slot*4`.
  `sub_0009E650` is guarded at 128.

### Engine object pool

- **Ceiling of 1024.** An object handle is `slot << 22 | serial`
  (0x000A69BE). It is decoded with `shr 0x16` in about 175 places, which
  makes 1024 the hard ceiling (CONFIRMED). 1000 is used so that no handle can
  ever equal −1 (−1 would need slot 1023 with an all-ones serial).
- **No other table blocks the raise.** The visibility bitset in
  `sub_000A0130` holds 2512 bits. The 214-entry tables are per room. The
  twin-pool index is used only as a time-slice key. `[0x72F5EC]` is indexed
  per level record. All CONFIRMED.
- **Room buckets.** The room-bucket bound at 0x0009E46B drops entries when
  full, but only after raising that room's capacity. This is a latent stock
  bug, so the buffer is doubled in every scene (+21 KB).
- **Story levels are unaffected.** The scene-101 formula
  (`and`/`add` 0x226) is left untouched, so scene 101 still gets 20. The
  hooks reimplement `sub_000A5C00`/`sub_0008DBF0` only in scenes 100 and 102,
  and only with the pools on; all other scenes run the original.
- **Callback dependency.** `sub_000A5C00` ends with an indirect call through
  `[0x00715FAC]`. `sub_001B5260` sets that pointer to `0x00242BA0`
  (CONFIRMED), and the hook calls that function directly. If any other value
  is found, the hook runs the original (550 slots).

### Teleporters

- **Stack overflow fixed.** In `sub_0023FD10` the 11th teleporter overwrote
  the return address of `sub_0009CD00` (CONFIRMED). The hook replaces the
  whole of `sub_0009CD00` in scene 100. It repeats the container walk of
  `sub_0023FD10` into bounded 1200-entry arrays, more than a 0x7FE0-byte
  container can hold.
- **Same pairing as stock.** Each teleporter is linked to the next one of the
  same colour (`sub_0009A2C0` check, then a `sub_0009A5B0` link with
  ebx → edi).
- **Link-pool guard.** No link is added once the pool is full: 8800 with the
  stage-A pools, otherwise 4400. `sub_0009A5B0` itself has no bound
  (CONFIRMED).
- **Gameplay with more than two per colour.** `sub_000C0540` sends a player to
  the first matching object in pool order. Every teleporter of a colour
  therefore leads to the same destination, the lowest slot; that one leads to
  the second-lowest (CONFIRMED by code). This is memory-safe, but extra
  teleporters do not form new pairs.
- **Container size (affects the map-file area).** The walk stops at
  `MAPMAKER_OBJECTS_CONTAINER_END` (0x7FE0). If the container grows, raise
  this constant and the teleporter cap, in the same way as `sub_0023FD10`
  (0x0023FD20, 0x0023FE31).

### Logic tags

- **What changed.** Tags and links are relocated to 1000 entries each. Three
  init-loop ends are written as end+4 and need `code` patches; the stock links
  end+4 value lies inside the tag table. Links are declared as 0x7CC bytes so
  that their range does not touch the tag table's start; `0x0075DAEC` is never
  referenced (CONFIRMED).
- **Guard.** When all 1000 slots are used, `sub_0023FEC0` returns −1 and
  `sub_002402B0` writes at index −1 (CONFIRMED). The guard instead writes
  `obj+0x60` and the serial `obj+0x230` exactly as the original, and skips
  the table.

### Cover pads and category 33

Only three places in the code add cover pads during play (CONFIRMED by every
writer of `0x007B65C0`):

- the tile builder `sub_0009A7A0`;
- the nav loader `sub_0009CE00`;
- `sub_0023F9D0` (called by `sub_0023FB90` from the fill pass), which tops up
  each team's start points to 16 using cover pads. That top-up is the
  measured +60.

Props in category 33 reference existing pads only (INFERRED from
`sub_00334B20` and the item record), so doubling category 33 to 132 adds no
cover pads.

## Memory

| Scope | Extra memory |
| --- | --- |
| Every scene, story included | turret pools +37,440 B, room buckets +21,312 B, perception +0x620 B per player (Level heap) |
| Scenes 100 and 102 | object pool +334,800 B, twin pool +230,400 B |

The reserved block holds the lists, the tag tables and the spares
(0x008A0000..0x008AA980). Free Level heap was measured at about 11-14 MB in
MapMaker. **Free heap in story levels has not been measured.** The extra
~60 KB there is the main open risk.

## Not done / for other areas

- `pc_mapmaker_limits.inc`'s header comment still says that turrets,
  teleporters and cover props are stock. Only the category table could be
  edited from this area.
- `sub_0006D0B0` has a latent 80-slot stack array (store before the bound,
  0x0006D115/0x0006D118). It is on the non-MapMaker `sub_0006D230` path and
  was not changed.
- The slot-0 and slot-8 lists are filled per object class. Which MapMaker
  classes register in them was not traced; the guards make an overflow
  impossible.
- The docs/agent-context guide for MapMaker capacity should list these hooks
  once integrated.

## Validation

- `tests/test_mapmaker_objects.py` passes: the fixture plus 36 mutations,
  each detected, and a check that the C constants match `objects.json`.
- `tests/test_mapmaker_limits.py` passes with the new category expectations.
- `logs/limits/check-tables.py` prints "table verified" (23 code entries,
  5 relocations) against the real XBE.
- No build and no game run.

**Test in play** with the probe (flags 32/64) on a large map:

- 24 turrets of each type, and System Link if possible: count
  `[0x00732D44]` should reach 72;
- 8 teleporters of one colour;
- 132 cover props and 80 movers: watch `[0x007B4FAC]` and the free heap;
- a story level load to check the extra 60 KB.
