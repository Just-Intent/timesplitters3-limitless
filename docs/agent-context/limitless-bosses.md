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

.34 Khallos/bars: the focus chair keeps its stock bar (rider's Story AI name,
khallos text hook); pc_mapmaker_bosses_hud skips a proxied focus and logs
each bar drawn or skipped once per entity and reason (story AI bosses: bar
of ..) plus a map-start count. The chair's jet emitter, made at the train
start, is moved to the chair at creation and each waiting frame
(sub_0010E730); no new WRAP ([area doc](../mapmaker/mapmaker-khallos.md)).

.40 movement: the giant robot's walk goal (WRAP sub_0015D500, return
0x00293D27 only -> `pc_mapmaker_robot_goal`) is limited to its own reachable
pad set: links across a portal lower than its box height are closed, an
unreachable player pad becomes the nearest reachable pad, and with closed
links the goal is steered room by room ([robot doc](../mapmaker/mapmaker-robot.md),
`low_door()`). The split Crowe's arena check adds the floor finder
`sub_000C3C50(point, 1.25, 0.1)` per cell and between cells: no floor or more
than a 1.0 step off is blocked (guest call, no new WRAP;
[Crowe doc](../mapmaker/mapmaker-crowe.md), `ledges()`).
