# MapMaker boss, sky and open-air notes

Split out of [limitless](limitless.md) to keep that guide under its size cap.
Each area's full wrapper list and history is in its `docs/mapmaker/mapmaker-<area>.md`.

More boss areas (2026-11-01): `khallos` (id 0x33 chair prop 0x376; .28 WRAP sub_00045670),
`worm` (0x2C), `mothership` (0x75 prop 0x539); rows 156-158 in `rbs_extra`
(159/160 Prometheus SK/8 0x62, SD/7 0x36: creatures.def rows, SK/8 class 4 (.58), fire/sight log (.66/.73), SD/7 bot AI (.73)); wrapper
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

Story props (wave 1, area `storyprops`, `pc_mapmaker_storyprops.inc`, after
weaponset): drum 0x49 -> power cell 0x53D and fixed guns 0x32..0x34 -> prison
gun 0xE6 (gun type 7) chosen in item options (drum: heap descriptor via a
`sub_002F7770` copy of category 26 with +8 set; gun: `sub_000783D0` at
0x00318A8C), saved in item record byte +0xC (stock-unused for these ids) and
node +0x30 ('SP'), applied by the play fill (`sub_002F7790`, `sub_0033DD80`,
preload `sub_0006C090`), paks l_64_ch / l_26_st + memory paks l_64_chm / l_26_stm (textures only
there; .55 fix: WRAP sub_00063900 registers via sub_00096220; .57 floated: cell at drum height, `TSFP_MAPMAKER_POWERCELL_LIFT`). 10 WRAPs:
mapmaker-storyprops.md. `test_mapmaker_storyprops.py`. Unplayed.

Story vehicles (wave 3, area `vehicles`, `pc_mapmaker_vehicles.inc`, after
storyprops): MapMaker vehicle 0x2B options (heap descriptor on a category 35
copy, node +0x30 'VH', record byte +0xC) choose the Machine Wars tank (def 2,
model 0xB, gun templates 1/2) or the castle truck (def 0, model 7, Vickers
template 0 or none). WRAP `sub_0033DDA0` sets the def at the preload
(0x0023F637, plus `sub_000B3B20` per template) and def+model at the fill
(0x00241D39, only when every created model was read: `sub_000B3B80` has no
null test); WRAP `sub_00182A20` (0x0006E949, also the restart) writes the
story record fields +0x28..+0x34. Stream paks lv5/prop1, lv4/prop3 mounted
index-only (textures inside). Category 35 is no longer raised (4-block pool
`sub_001806B0`, unchecked pop); restable row Vehicles. 9 WRAPs:
mapmaker-vehicles.md. `test_mapmaker_vehicles.py`. .61: all sank (record y
at the pad, models below their origin: truck -0.935, tank -2.365; the fill
hook now lifts by that + 0.01, `TSFP_MAPMAKER_TANK_LIFT`/`_TRUCK_LIFT`) and
the tank refused players (block +0x58 bit 0x10 from def +0xA8, tested at
0x0017A27E/D1E8/DAFB/ECD6; the create hook clears it). .63 fixes: reticle
`tank_half` (HUD 0x125, group 0x200 = scene 5 only) requested via
`sub_000810C0` with `l_5_st` mounted index-only; gun data +0x34/+0x38
pitch -10..20 / -45..15; chase camera via a def copy (+0x50) whose +0xAC
indexes a heap preset (10, 3; `TSFP_MAPMAKER_TANK_CAMERA`). Unplayed.

Tank TNT (.55, CONFIRMED, `test_tnt_on_the_stunned_tank`): a sticky explosive
going off (`sub_000F0430`) calls `sub_00228B50` (story mode, 0x000F049A), which
runs the castle's `sub_001E4030` -> `sub_00256040(tank, explosive)` only in
scene 4 ('KNAT' tag): state 9, explosive +0x158 on the tank, part +0x15C in
15..17, health -10.1. New WRAP sub_00228B50 -> `pc_mapmaker_tank_mine` makes
that call for our tanks with their statics (mapmaker-tank.md). Unplayed.

Story creatures (wave 2, area `creatures`, `pc_mapmaker_creatures.inc`, after
storyprops): one deployer, one X-macro row per creature in
`pc_mapmaker_creatures.def` (picker row, carrier id, look, class lo/hi, anim pak
+ keep run, level hook), also read by realbosses (`rbs_extra` rows 161-163) and
animslots (`ans_paks`). Rows: Catacomb Ghost 0x2F (zombie look, 0:0x50000000,
lv17), SecuriDroid XP 0x4D (0x4000:0x10000000, lv6), Mutant (Story) 0x30 (0x59
look, 0x8000:0x10000000, lv22). 4 WRAPs (sub_001C8540, sub_0015E7C0,
sub_000DAC30, sub_0015B830): mapmaker-creatures.md. `test_mapmaker_creatures.py`
(decode: dispatcher, no scene state, kept anims, story spawn records). Unplayed.
Cleaning bot deferred (scripted class 0x80000). Realbosses' `extra_label` now
holds RBS_EXTRA_MAX 16 (it held 6 for 8 rows).

Pit skeleton (wave 3, area `pitboss`, `pc_mapmaker_pitboss.inc`, after
creatures): creature rows 164 "Pit Skeleton" (carrier 0x31, look 0x28,
0x20000:0x10000000) and 165 "Ragbug" (0xB0, 0x200000:0x10000000), lv17_4
entries 16..27. The mission 6 fight `sub_0029E0A0` runs per instance (<= 4)
with its own skeleton block and statics, the 34 catacombs arena constants
moved to the spawn for the call, l_17_st/l_17_stm kept open for props
0x246/0x249/0x24A/0x24B, 'NMME' answered (NEW WRAP sub_00234270), rack timer
held (no hostage), rim pads 0x4B5..0x4C1 copied; ragbugs join the nearest
skeleton's flock (NEW WRAP sub_0028B610), removed without one; realbosses
skips char 0x31. 11 NEW WRAPs + 11 chains: mapmaker-pitboss.md.
`test_mapmaker_pitboss.py`. Unplayed.
After .62 (no bottle, ragbugs one at a time): bossroom/bigdraw also draw the
pit satellites (0x24A/0x246/0x24B/0x6CB) high over their tile; the flock is
launched together at state 6 (`sub_0028A0A0` x2); prop/flock log lines.
After .66 (bottle at the feet, shots did nothing): the boss's pose is computed
before each fight frame (`sub_000A1B80`, view bit cleared: bones were stale
off screen) and roomless props take the boss's rooms; hit, trace and ragbug
phase lines (mapmaker-pitboss.md).
After .68 (camera broken): that pose call is gone (it posed the view slot 0
camera); the bottle is held 11 up, 2.5 ahead of the boss; the hull hits
count a quarter; the rim pads live in the pad pool's tail (dives need index
< 0x1F40, sub_0009A420).
After .73: the collision hull 0x249 is hidden as the story's slot 4 does
(`sub_000942B0(instance, 8)`, the draw skips +0x1F6 & 8); no early ragbug
release (story state-10 launches; bugs dive near the rim ring, explode when shot).
