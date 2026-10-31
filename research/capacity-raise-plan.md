# MapMaker limit raise plan (2026-10-23)

Goal (user): raise every MapMaker limit many times over (tiles, items, story
AI, movable objects, keycards, logic). Basis: five static-analysis passes over
the disassembly (reports in the ignored `logs/limits/*.md`; every site there is
listed with function, address and bytes). Everything here is UNVERIFIED in
play until a stage's validation run says otherwise. Tool:
`config/guest-code-patches/` (verified same-length rewrites and operand
overrides at lift time) plus function wrappers.

## What really limits each thing

| Editor limit | Stock | What it protects at play time | Hard ceiling as built | Main work |
| --- | --- | --- | --- | --- |
| Pieces | 200 | 200 visibility rooms, engine-wide (room tables, draw buckets of byte room ids, view struct per-room rects, 0x9D0 model table, prop pool slots) | 200; ~242 with byte ids; beyond needs widened ids | ~150 rewrites + ~15 relocations for 242; wrappers of draw/sort/light code beyond 255 |
| Cover pads | 1024 | Cover pool and handles 0x1F40..0x243F; runtime pads overlap at 0x209E (stock bug) | 1280 now; ~23,766 (int16 ids) after moving runtime handles | "Layout B": 89 rewrites, larger pool |
| Pads / links | 2400 / 4400 | Pools (raised in stage A) | Regular pads 7000-8000 (ids, 13 id compares); links 32767 (int16 A* indices) | Regular 4800 and links 8800 already fit |
| Pad groups | 300 | N x N matrix; two 1000-node .bss A* pools with no bound | ~500-1000 safe | Bound/relocate the A* pools before 600 is trusted |
| Items | 50 | Heap by count, but: 250-entry logic tag table (writes index -1 when full), 550-slot object pool, teleporters 10 stack slots | Tag table 250 / object pool 550 | Wrapper constant, sub-entry pool 530 -> 4x/8x (4 sites), tag-table guard |
| Keycards / movers / props | 2 per colour / 20 | Category max table (.data `0x00524BE0`) | Object pool 550 | Data writes when the editor starts |
| Free objects | 40 / 30 / 100 | Editor arrays (170) inside the editor object; loader has no bound | 170 | Relocate with the editor object |
| Story AI | 50 | Editor array 50 x 0xE4; unchecked 50-slot stack arrays in `sub_00240590` and `sub_00323220`; byte UI index | 255 (byte index), then 32767 | Grow editor object (wrap `sub_002FE360`) + 137 offset rewrites + 2 reimplemented functions; only ~20 alive at once regardless |
| Logic ops / nodes / records | 30 / 130 / 100 | Editor arrays; play-time 45-slot logic table (`[0x765E44]`, no bound); 8-slot stack array | Container size | Same editor-object growth (390 rewrites) + 45-slot table relocation |
| Map file | 32,736 B | Save/load, undo, System Link (bytes counts, 47,800 B max) | 64 KB (16-bit `used` word sites) | 45 rewrites for 64 KB; 32-bit word sites + container outside the Eternal heap for 256 KB+ |

## Status

- Stage 1 implemented 2026-10-23: items 200 (opt-in
  `TSFP_MAPMAKER_LIMITS=1`), editor item node pool 2120 (guest-code patch),
  category maxima x4 for pickups (weapons, ammo, keycards 8 per colour,
  health, armour, power-ups, special), movers/props 80 (x4) and categories
  35/36 8 (x2). Not raised: turrets (8 each: two 32-slot gun/owner pools with
  an unchecked allocator and a 30-entry unbounded object list, see
  `logs/limits/turrets.md`; build 08809be allowed 16 each and is unsafe with
  more than 32 turrets), teleporters, cover props (category 33),
  free-standing objects (editor arrays of 170), flags, start points.
- CONFIRMED in play (2026-10-23, Windows, compile-only build 08809be, map
  `bigmap3`): 110 tiles, 67 items incl. 8 red keycards and 30+ movers; team
  deathmatch with no visible problems; saved and reloaded with every item.
  Probe: cover 990 in the editor, 1050 in play (the same +60 as bigmap2 with
  142 tiles and 50 items, so items do not add cover pads; the per-item cover
  charge of the first build was removed), engine objects 203/550, logic tags
  0, minimum free 11.56 MB.

- CONFIRMED in play (2026-10-30, Windows 11 test PC without Smart App Control
  enforcement, compile-only build 97f4ad5a, maps bigmap6 / bigmap4): MapMaker
  editor and play with 236 pieces, 200 items, 102+ Story AI, up to 64
  characters in MapMaker story play, team deathmatch, and the first story
  mission's exploding barrels (the 06aed2de crash, a missed 0xD5 room id at
  0x000BBA05, is fixed). Probe: 462 objects of 1000, 46 live turrets, cover
  pads 2184, 236 rooms of 241, minimum free 5.56 MB, no guard drops, no
  unresolved jumps. Frame rate fine after the tester fixed the PC's power
  settings. Not yet tested: System Link, splitscreen, native Mac.

- Round 3 implemented 2026-10-31 (not built or played;
  `docs/mapmaker/mapmaker-memory.md`): heap overrun guard and the probe's
  host headroom fields (step A1/A2), `TSFP_MAPMAKER_ARENA_MB` prepared off by
  default (A3, needs the probe measurement), items 500 (node pool 5300),
  keycards 16 per colour, movers/props/markers 160, Story AI 511 (array at
  the end of the editor object, word selection index, menu pool 6144), and
  maps past any stock count saved in the 64 KB tier. Not done: moving pools
  out of the Level heap, a larger extension block, rooms 512, Story AI 1023
  (menu pool and editor heap unmeasured).

- Character set slots 10 -> 20 implemented 2026-10-31 (not built or
  played; `docs/mapmaker/mapmaker-sets.md`): fork-only chunk for slots
  11-20, paged slot screen, 20 Story AI character choices, play copy and
  preload.
- Weapon set slots 6 -> 20 implemented 2026-10-31 (not built or played;
  `docs/mapmaker/mapmaker-weaponset.md`): fork-only chunk for slots 7-20, a
  slot byte (+0xC) on weapon/ammo pickup records instead of new item types,
  paged weapon set screen, 20 Story AI weapon choices, 20+ id play set.

- Arcade bots per MapMaker match 10 -> 20 (24 characters) implemented
  2026-10-31 (not built or played; `docs/mapmaker/mapmaker-matchbots.md`):
  live per-actor arrays read through a 64-entry mirror (26 operands), rank
  table relocated to 64, pair table humans + 20 outside story, Bots screen
  page 2 and match-start extras for MapMaker Arcade / Test map; the 26-entry
  arcade stats table sets the 24 limit. Ordinary Arcade and System Link
  stay stock.

## Stages (in order)

1. **Item and object quick wins.** Keycards, movers, props and other category
   maxima x4 (data writes in scene 102), total items 50 -> 200 (wrapper
   constant, sub-entry pool 530 -> 2120), guard the 250-entry tag table.
   Teleporters stay at 10 (stack arrays) until `sub_0009CD00` is reimplemented.
   Container limits content here first.
2. **Map file 32 KB -> 256 KB.** Container, CRC, save and load paths 32-bit;
   container and undo ring outside the Eternal heap; maps that fit stay
   byte-identical to stock. Large maps are refused for System Link (local play
   only) until a multi-part transfer exists.
3. **Cover pads (Layout B).** Runtime pad handles moved above a 10,240-entry
   cover range (also fixes the stock overlap), cover pool and editor limit
   raised; bound the group A* pools. Tiles are then capped by rooms (200).
4. **Story AI and logic.** Grow the editor object, rewrite every array offset
   (527 sites, cross-checked against generated C), reimplement the two 50-slot
   stack users, widen the byte AI index, relocate the 45-slot logic table.
   Targets: story AI 255, logic x4-x8 (container permitting).
5. **Rooms 200 -> ~242**, then (if wanted) widened room ids for 1024.

Each stage: guest-patch entries + wrappers + fixture tests, a compile-only
build, then a user play test with the probe (pools, heap, and new counters).
