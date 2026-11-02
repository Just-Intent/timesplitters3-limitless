# MapMaker anim slot budget (crash after v0.4.0-mapmaker.34)

Status: implemented in `src/interface/pc_mapmaker_animslots.inc`. It runs
inside the worm area's existing `WRAP sub_0005B980 -> pc_mapmaker_worm_register`,
so no wrapper changes. Fixture-tested: `tests/mapmaker/mapmaker_animslots.c` +
`tests/mapmaker/test_mapmaker_animslots.py` (18 mutations, plus decode and US
XBE checks). **Not built, not played.** Switch: `TSFP_MAPMAKER_LIMITS=1`.
Only scene 100 (a MapMaker level) is affected; story levels stay stock.

## The crash (CONFIRMED)

User report (.34): every level load crashed when the set held the giant
skeleton, Crow walker, giant robot/Crowe, seated Khallos and mansion worm. The
same map loaded with Khallos left out.

Dump (`logs/m34c/crash-20261102-131721845-*`): ecx 940, edi 0x2D1, stack
arguments prefix 0x0049C690 and table 0x0048F738, esi 0x0048F838. The table
is **grenade.pak's** (`pak/anim/gun/grenade.pak`, 16 entries, registered by
`sub_001CA2F0` at 0x001CB1C3, return 0x001CB1C8). It is not a boss pak.
`sub_0005B980` walks esi = table + 0x10 + k*0x18, so 0x0048F838 is entry 10:
`millsgrenade_reload`, id 0x2D1 = edi. The stack word 0x0048F838 is that
esi, not a second table.

Cause (decode, `test_slot_arrays_and_unchecked_count`):
- `sub_00057F30(path, id, 0)` gives a new id the next slot `[0x006D77F8]`. It
  stores the anim at `[[0x007E3954] + slot*4]` and slot words at
  `0x007DE660 + slot*8`.
- `sub_0005C420` points `[0x007E3954]` at the static array 0x007E2AA4, which
  has 940 entries. The 0x007DE660 array also has 940 (up to the id map
  0x007E03C0). Slots 0..0x22 are reserved. Nothing checks the count.
- Slot 940 writes the anim pointer over `[0x007E3954]` itself, so the next
  read `[[0x007E3954] + 940*4]` is junk and the load faults.
- A boss pak registers its story level's whole cast. Before this fix that
  was 158 entries for the six boss paks (lv1_3 16, lv1_6 18, lv81 20, lv14
  41, lv11 33, lv1_2 30, the worm's cut to 9 since .25). With lv11's 33 the
  load reached slot 940 at grenade.pak, after every boss pak.

A second fixup is **not** the cause (`test_one_fixup_per_anim_file`): flag 4 at
header +8 is tested at 0x00057FEA and set at 0x000581EE. A name registered
again is not fixed up again, and it gets no new slot. The .24 crash (trophy
monster, skeleton, worm) was most likely the same overflow: lv1_2's whole
table was registered then.

## Shared names and ids between boss paks (CONFIRMED, `test_every_overlap_between_boss_paks`)

| paks | name | ids |
|---|---|---|
| lv1_2, lv1_3 | `jblookout` | 0x193, 0x193 |
| lv1_2, lv1_6 | `jbgrossend` | 0x1AE, 0x1AE |
| lv14, lv81 | `r110_run`, `r110_normalwalk`, `r110_stand_idle1` | 0x7DB..0x7DD, 0x6C0/0x6BF/0x6BE |

lv11 (Khallos) shares nothing with the other boss paks. No kept entry (below)
is named or numbered by another boss pak or a `pak/anim/` table. Story-level
paks reuse some names under other ids (for example lv17_4's
`princess_low_idle1`). INFERRED: scene 100 never mounts those paks.

## The fix

`ans_register()` (called first by `pc_mapmaker_worm_register`; 1 = handled
and the original's `ret` done):
- A boss pak's table (address and count) registers only the entries its boss
  plays. Each is a nested `sub_0005B980(prefix, entry, 1)`; that nested call
  comes back through the wrapper and runs the original (`ans.busy`).

  | pak | table | kept entries |
  |---|---|---|
  | lv1_3 (trophy monster) | 0x0048C1B0 x16 | 0..7 `mattackheadbutt`..`mcharge` 0x127..0x12E |
  | lv1_6 (skeleton) | 0x0048C738 x18 | 1..11, 14 `princess_*` 0x19C..0x1A6, 0x1A9 |
  | lv81 (Crow walker) | 0x00484F68 x20 | 5..19 `crowewalker*` 0x6EE..0x6FC |
  | lv14 (robot, Crowe) | 0x0047E938 x41 | 23..25 `giantrob_*` 0x807..0x809; 29..37 `splitcrowe*` 0x80E..0x815, `eggnoise` 0x816 |
  | lv11 (seated Khallos) | 0x00484410 x33 | 32 `khallosidle` 0xBFB |
  | lv1_2 (mansion worm) | 0x0048BEC8 x30 | 5 `barney_worm` 0x149; 21..28 `wrmburst`..`wrmdeath` 0x194..0x19B |

  That is 57 slots instead of 158. `test_boss_anims_are_kept` proves that every
  id from its own pak that a boss's handlers (and the callees only a few
  functions share) use is kept. Crowe's eggs play `eggnoise`
  (`sub_002AA6A0`/`sub_002AB1B0`).
- Each such entry skips an id that is already registered this load. The first
  pak's copy stays, holding the same anim data. It also skips a new id once
  the 940 slots are used, and logs it: that anim is missing, but the load
  does not crash.
- Any other table runs the stock call when it fits in the free slots, and
  goes entry by entry under the same two rules when it does not.

Logs (`[mapmaker] animslots:`, up to 48 lines): `boss anims registered (kept,
left out) from <pak> (k, d)`, `anim ids already registered, kept the first
copy`, `anim slot table full (940): left out of table`.

Chain: wrapper patch `if(pc_mapmaker_area_enabled("worm","pc_mapmaker_worm_register"))
pc_mapmaker_worm_register(sub_0005B980_gen)` -> `ans_register()` -> per entry
`guest_call(sub_0005B980, 3, prefix, entry, 1)` -> wrapper ->
`pc_mapmaker_worm_register` -> `ans_register` returns 0 (busy) -> the
original. Turning the `worm` area's register hook off disables the whole
budget. The worm's mount stopgap then keeps lv1_2 out next to lv1_3/lv1_6.

## Tests

- Fixture: the six boss tables (copies checked against the XBE) plus the
  scene's own tables, with a stock-faithful fake `sub_0005B980`. Covered
  cases:
  - stock overflows (slot 940);
  - scene 100 fits, and each kept id is registered once and valid;
  - left-out ids stay unregistered;
  - `khallosidle` already named by a scene pak keeps that one copy;
  - a scene that cannot fit stops at exactly 940;
  - scene 1 stays stock, and another count stays stock.
- Mutations: the keep runs, the shared-id skip, the full guard, the scene and
  limits tests, the ret, the per-entry count and the prefix.

## Playtest checks

1. The .34 map (skeleton, walker, robot, Khallos, worm) loads. The log shows
   one `boss anims registered` line per mounted boss pak and no `table full`.
2. Each boss animates: the skeleton emerges and attacks, the walker walks,
   turns and dies, the robot comes through and walks, Crowe walks and dies
   (eggs off), Khallos sits idle, the worm bursts, attacks and dies.
3. Add the giant trophy monster too: it should still load (8 more slots).
4. Weapons in the player's hands animate (grenade throw and reload).
