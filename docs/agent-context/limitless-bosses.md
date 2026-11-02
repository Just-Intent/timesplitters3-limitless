# MapMaker boss, sky and open-air notes

Split out of [limitless](limitless.md) to keep that guide under its size cap.
Each area's full wrapper list and history is in its `docs/mapmaker/mapmaker-<area>.md`.

More boss areas (2026-11-01): `khallos` (id 0x33 chair prop 0x376; .28 WRAP sub_00045670),
`worm` (0x2C), `mothership` (0x75 prop 0x539); rows 156-158 in `rbs_extra`; wrapper
lists in each mapmaker-<area>.md. Sky area adds `pc_mapmaker_skymusic.inc` (extra
sky/sand/music, +mem paks, MM sand grains; mapmaker-sky-music.md) and roof play/rooms/draw/tick hooks
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

.31 Crowes fired but never walked: the egg's blocked cells steer state 5
(sub_002A9E60); the skipped egg now keeps them 120 calls (WRAP sub_002A9D00
-> `_crowe_cells`); a stalled state-9 walk lands after 240; per-Crowe trace. Pause restart
(sub_001CDD60, no level init): WRAP -> `_crowe_restart` forgets the blocks.
