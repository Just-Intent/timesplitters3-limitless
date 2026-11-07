# MapMaker boss, sky and open-air notes

Split out of [limitless](limitless.md) to keep that guide under its size cap.
Each area's full wrapper list and history is in its `docs/mapmaker/mapmaker-<area>.md`.

More boss areas (2026-11-01): `khallos` (id 0x33 chair prop 0x376; .28 WRAP sub_00045670),
`worm` (0x2C), `mothership` (0x75 prop 0x539); rows 156-158 in `rbs_extra`; wrapper
lists in each mapmaker-<area>.md. Sky area adds `pc_mapmaker_skymusic.inc` (extra
sky/sand/music, +mem paks, MM sand grains, .32 grain/haze alpha + fog row 7; mapmaker-sky-music.md) and roof play/rooms/draw/tick hooks
for bounded vs unbounded open air (mapmaker-sky.md). Bot set pane = 20-row scroll
list (mapmaker-sets.md).
Multi-boss merge (.26): 28 WRAPs (walker/skeleton/robot/crowe/tank/mothership/khallos statics
swaps, worm anim register, sky weather); sub_000DAC30 chain: actors, realbosses,
tank, khallos (log only).
.27: sets_spawn_masks (sub_00240C50): slots 11-20 get slot 1's mask;
worm block at AI start; late walker/skel; fall drop. .28: Crowe eggs
off; `pc_mapmaker_bossfx.inc` statics.

.29-.30: tank/mothership multi-instance and named bars (`bos_bar_extra_add`),
Khallos seated rider and bars (`bos_bar_proxy`, WRAP sub_0014CC80, sub_00045670),
worm removal (WRAP sub_001D5E20), open-air view (`pc_mapmaker_openview.inc`,
WRAP sub_00089B90), sandstorm grains outside scene 2 (weather frame hook),
Crowe egg skip (WRAP sub_002AB1B0) and the shared boss statics holder
(`pc_mapmaker_bossfx.inc`).

.31 Khallos: riders also ask for class 8 (AI +0xAA8/+0xAAC, which
`sub_0015C700` applies before each think), khallosidle replayed in state 1
(`sub_0005F460`), and WRAP sub_001CDD60 (level restart, no pak mount)
resets the chairs ([area doc](../mapmaker/mapmaker-khallos.md)).

Anim slot budget (after .34): the stock anim slot table has 940 static slots
with an unchecked count (`sub_00057F30`); boss paks registered their whole
story cast and slot 940 crashed the load (grenade.pak entry 10). In scene 100
`pc_mapmaker_animslots.inc`, run first by the existing `pc_mapmaker_worm_register`
(WRAP sub_0005B980, no new wrapper), registers only each boss's own anims,
skips ids already registered, never passes 940
([area doc](../mapmaker/mapmaker-animslots.md); test_mapmaker_animslots.py).

Restart audit (after .31): "START restart" runs `sub_001CDAF0` + `sub_001CDD60`,
never `sub_001CDB80` (pak mount, `sub_0015E7C0`/`sub_0025F720` level init),
and skips the map start `sub_00240C50` (`[0x006F32EC]`). New WRAP sub_001CDD60
-> `pc_mapmaker_{tank,mothership,worm,realbosses,bosses}_restart` beside
Khallos' (each its own state; realbosses runs the stock skeleton/walker
resets after the original). Robot (`sub_0025F6A0` reset), skymusic, roof
and openview need none; see each area doc's restart section.

Restart heap (2026-11-06, after .40; CONFIRMED, `test_mapmaker_restart_heap.py`):
the restart's `sub_0003E740(3)` -> `sub_0003E240` resets only heap 0 (top-down
`[0x006B8388]`, jump tables 0x0003E094/0x0003E1C4 entry 0). The Level heap
`sub_0003DFA0(n, 1)` is heap 1 (`[0x006B837C]`), written only by
`sub_0003DF00`/`sub_0003DF70`/`sub_0003E650`/the allocators, none reachable
from the restart branch. So no area's guest block goes stale: every area call
asks for heap 1, host blocks are `xbox_HeapAlloc`. The .40 skeleton fix and the
Crowe restart allocated new blocks per restart (Level heap used up, about
52-190 KB a boss); both now set their own blocks up again in place (zeroed,
stock non-allocating init rest: `rbs_skel_again`/`rbs_walk_again`/`crw_again`,
block sizes in `pc_mapmaker_bossfx.inc`), as the worm already did.

Restart anims (after .43, CONFIRMED decode + US XBE, `test_restart_drops_the_mansion_anims`):
the skeletons "glitching out after restarting" (no throw after any restart in
m40b/m42/m43b.err) were not their blocks. The restart's `sub_0005FC60`
(0x001CDD6A) -> `sub_0005C2F0` unmaps every loaded anim whose slot word +4
has 0x4000 (pointer 0, id map 0xFFFF; next slot back to the mark
`[0x006D77FC]`) and nothing registers them again (`sub_001CA2F0` only on a
full load). All kept lv1_6/lv1_3 anims have it (lv81/lv14 not), so the
restarted skeleton never played princess_start and stayed in its entrance.
`pc_mapmaker_realbosses_restart` notes this map's ids (set + pak mounted)
before the original and maps them to the same slot/data after it (data is
in the Level heap pak buffer) when the slot is empty and below the next
slot. The worm's lv1_2 anims carry the flag too (worm area, not fixed).

Skeleton same-floor throws (.44, "can't hit me on floor one still"): in
`m43b`/`m44.err` the floor (-4.49/-4.50) was right and every aim was at a
player on 0.5; the shot origin is a model bone point (0x0029CFA4..0x0029D050)
about 10.7 over its feet, 5.7 over that player, past the ceiling of the
player's one-floor room (INFERRED: blocked there). `rbs_skel_low` (inside
`rbs_skel_aim`, no new WRAP) starts a shot at most 4 over the target's feet
(not for a target under its floor), then the stock/raised aim. Fixture m44
case, mutations `hand-*`. Unplayed.

.34 Khallos/bars: the focus chair keeps its stock bar (rider's Story AI name,
khallos text hook); pc_mapmaker_bosses_hud skips a proxied focus and logs
each bar drawn or skipped once per entity and reason (story AI bosses: bar
of ..) plus a map-start count. The chair's jet emitter, made at the train
start, is moved to the chair at creation and each waiting frame
(sub_0010E730); no new WRAP ([area doc](../mapmaker/mapmaker-khallos.md)).

Pak mount table (mapmaker.20: 29 of 30 used -> stretched bosses, robot/crowe
anims unmounted): `config/guest-code-patches/paktable.json` moves 0x006D7900
(30 x 0x1C) to 0x00907000 (64), caps 0x00062049/0x000620F9 -> 0x40, clear
0x0006275B -> 0x1C0 dwords; areas use MOUNT_MAX 64 / KEEP 8.

Crowe walk (.43, "not moving at all"): no static ledge blocking now. The
.41-.43 floor probe blocked cells over a lower floor (m43b.err: 15-20 of 35),
so state 5's picker (`sub_002A9E60`: 5 x 5 round the player, in line, 2+
away) found nothing. Cells in no room stay blocked unless that starves the
picker (`crw_pickable`); in state 9 `crw_ahead` probes 2 ahead (room query,
floor finder) and ends the walk (`crw_stop`) at no room or a floor 2+ below
the reference. Tests: `ledges()` in `mapmaker_crowe.c`, `ahead-*`/`starve-*`.

Skeleton vanishing log (after .45, "disappear up close / mid-animation"; LOG
ONLY): `pc_mapmaker_skelvis.inc` (included after limits; callers test
`MAPMAKER_SKELVIS`) reads the 10-room list build (rooms query under
0x0009FCF5: touched vs kept, camera room view +0x2A0 in/dropped), the bigdraw
rect (reached, empty), NEW WRAP `sub_0008C020` -> `pc_mapmaker_skelvis_frustum`
(only caller 0x000A249E; < 0 culls at 0x000A24AA), the tank draw hook
(`sub_000A2740`) and, before openview's walk, ends the main view's pass.
Per-pass reason, state changes only, 60 lines a level + summaries every 600
passes; grep `[mapmaker] skelvis:` (formats in mapmaker-realbosses.md
"Skeleton visibility log"). `test_mapmaker_skelvis.py` (fixture +
`mapmaker_skelvis.expected`, mutations, decode). Unplayed.

Boss camera room (.46 m46.err: all hidden passes "not reached", drawn only
while the camera room was in the boss list): `pc_mapmaker_bossroom.inc`, first
in `pc_mapmaker_rooms_buckets` (no new WRAP), appends each giant boss with a
reached listed room to a copy of the camera room's prop list (`0x008D4000` +
slot*0x1000). Lists: `sub_0009E4B0` relinks per frame via `sub_0009E430`
(pool bound 0x0009E46B drops a prop, raised cap kept). Which drop hit the
bosses is INFERRED; the `bossroom:` line (linked K, pool use) decides.
`test_mapmaker_bossroom.py`. Unplayed.
