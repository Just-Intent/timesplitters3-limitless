# MapMaker limit raise plan (2026-10-23)

Goal (user): raise every MapMaker limit many times over (tiles, items, story
AI, movable objects, keycards, logic). Basis: five static-analysis passes over
the disassembly (reports in the ignored `logs/limits/*.md`; every site there is
listed with function, address and bytes). Everything here is UNVERIFIED in
play until a stage's validation run says otherwise. Tool:
`config/guest-code-patches.json` (verified same-length rewrites and operand
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
