# MapMaker Story HUD turret icons

In Story mode ([7DE455] == 10) the original HUD 349E40 walks gun pool slots
0..[732D44]-1, the live gun count (reads at 34A75E and 34A796), and draws each
turret's icon with 349970(hud, block) (call at 34A78E, return 34A793). The gun
free list (156DB0) links block i to block i-1 with the last block as head, so
allocation hands out the highest slots first. Live turrets therefore sit above
the scanned range and their icons never appear in MapMaker Story maps. Stock
Story levels are unaffected by the hook.

`graphics/interface/2295-mapmaker-story-turret-icons.patch` wraps both routines
in `recomp_manual.c` through `src/interface/pc_mapmaker_turret_icons_guest.inc`:

- `pc_turret_icons_hud` (349E40): only in scene 100 Story mode, it shows the
  HUD the pool size (free-list total [732D38], 32 in the stock game) as the
  count for that call, then restores the live count plus any change the call
  made. A free list that does not walk like the pool's (loop, stray node, pool
  above 256) falls back to the original.
- `pc_turret_icons_turret` (349970): during that HUD call and only from the
  loop's call site, blocks on the free list are skipped (`ret` without
  drawing); freed blocks keep stale flags.

The fix is on by default in MapMaker play; `TSFP_MAPMAKER_TURRET_ICONS=0`
(read once) keeps the original icon-less HUD. That default is a maintainer
choice: the icons were never visible before, so players may notice new HUD
markers in MapMaker Story maps.

`tests/graphics/test_mapmaker_turret_icons.py` compiles the hooks against a fake
HUD loop and a stock 32-slot pool (default/switch, scene/mode scope, caller
check, count restoration, malformed free lists, mutation cases) and, with the
supported private US executable, decodes every cited instruction and the stock
pool size push at B3A90. In-game appearance remains a manual check.
