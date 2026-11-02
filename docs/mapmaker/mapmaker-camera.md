# MapMaker switch cameras

Area `camera` (`src/interface/pc_mapmaker_camera.inc`). Test:
`tests/mapmaker/test_mapmaker_camera.py` (fixture `mapmaker_camera.c`, plus a
decoder check of the original bytes that skips without the private XBE).
Static analysis and fixtures only; not built or played by the agent that
wrote it. Reported in the v0.4.0-mapmaker.31 playtest.

## 1. The camera keeps sweeping while you control it (fixed)

A MapMaker camera is a mounted-gun actor (item types 0x35..0x37). A switch
linked to it (link type 0x18, `sub_0033F0E0`) mounts it with `sub_003391D0` ->
`sub_000B76B0`: player mode 0x80, character extra `+0xC14` = the camera,
camera data `+0xD8` = the character. Mouse aim goes through
`src/input/pc_mounted.h`, the stick through `sub_00258A20`; both write pitch
`+0x1C` and yaw `+0x24`.

Cause (confirmed in the US code): the actor update `sub_00246000` runs its
state machine every tick. State 2 sweeps pitch and yaw by `+0x58`/`+0x5C` per
tick between `+0x34..+0x38` and `+0x3C..+0x40`, negating the speed at a limit;
state 4 eases back to the saved pose; state 3 turns to an AI target. None of
it checks the operator. The only stock way out is data flag `+8` bit 0
(`test cl,1` at `0x002460E5`), and the switch sets that bit on mount only when
flag 0x20 is already set (`0x003391E6`). A camera without 0x20 keeps sweeping
under the player, for the mouse and the controller alike.

Fix: WRAP `sub_00246000` (cdecl, entity at `[esp+4]`, plain ret) ->
`pc_mapmaker_camera_gun_update`, area `camera`, scene 100 only. While the
actor's operator links are reciprocal (data `+0` = entity, `+0xD8` =
character, character extra `+0xC14` = entity, player `+0x14` = character,
mode 0x80) the original runs with bit 0 set, then the bit is cleared again
unless the game had set it. Pose, speeds and limits are untouched, so the
sweep resumes when control ends. Story levels keep the stock behaviour.

Related mouse fix (`pc_mounted.h`): guns with flag 0x1000000 scale aim by
`+0x5C`, which is also the sweep speed and is negative after a bounce; the
helper used to drop all mouse input then. It now uses the magnitude.

## 2. The camera view stays 4:3 on a widescreen display (fixed, shared)

The 3D view was never 4:3: the .34 log has `[PC-DISPLAY] View:` lines for
player mode 0x80 with factor 1.3260 and aspect 1.7680 (16:9), from
`sub_000C77C0` widened by `pc_display_camera`. The 4:3 box is 2D.

Cause (confirmed in the US code, not yet seen in a build): HUD mode 10 of the
table `0x004E6900` (stride 0x1C) pairs the update `sub_0012E100` with the draw
`sub_0014BFF0`. From `0x0014C281`, single view only, the draw frames the view:
0x7F black bars around an inset 3% of the view width (`0x00475CF8`) by 15% of
its height (`0x004765E0`), a green tint `0x1E3C1E28`, a 0x1E tint for camera
guns (`sub_00242EE0`), scanlines every 3 px, corner brackets (texture 0x11B),
labels and a meter. All are `sub_0001EFB0` / `sub_0001F090` / `sub_00041DC0`
draws in guest pixels; AUTO placement fits anything narrower than the canvas
into the centred 4:3 box, so the side bars, tint, scanlines and brackets boxed
the picture at 4:3 and left the world untinted beside it. The update also
raises view effect `+0x5C` (camera guns by 0x50 a tick); `sub_000FE990` draws
it as static tiles (`sub_000F8DC0`, texture 0x336, one tile per texture size),
also inside the box. The camera-gun dimming of the other remote view,
`sub_00137E30` (`0x00137E6A`), could be boxed as a solid rect that inherits a
texture.

Fix (`src/interface/pc_display_ui.inc`, `pc_ui_layout.h`, shared by Windows
and native Mac): WRAP `sub_0014BFF0` -> `pc_display_viewfinder` marks the
frame while the view is wide (HUD scale < 1), single and full. The existing
`sub_0001EFB0` and `sub_0001F090` wrappers then draw each frame primitive
(return address in `0x0014C281..0x0014C962`) STRETCH with
`pc_ui_viewfinder_x`: a span from the left third to the right third keeps each
edge on its side, other pieces fit (scale k) about their third's edge or
centre, so the frame meets the window edges with 4:3 thicknesses and bracket
shapes. Labels (`sub_00041DC0`) use the matching LEFT/CENTRE/RIGHT placement.
The aim reticle before `0x0014C281` and the tail crosshair `sub_00135620`
are untouched. WRAP `sub_000F8DC0` -> `pc_display_view_static` draws the full
view's static STRETCH with the texture record's tile width (+0x26) set to
w*k for the call, which keeps the texel size and covers the window, then puts
the width back and leaves the random state (`0x004E7950`, counter
`0x007497C8`) where the original tiling would have. `0x00137E6F` joins the
full-canvas fade list. 4:3, narrower, split and partial views are unchanged;
`TSFP_CAMERA_WIDESCREEN=0` restores the stock placement. Story CCTV and
mounted guns use the same HUD, so they get the same frame.

Log: `[PC-DISPLAY] Camera view:` (per change, capped) gives the path taken
(viewfinder frame / view static / stock and why), scene, player mode, gun and
its kind, viewport, HUD scale, projection x/y and aspect. Test:
`tests/graphics/test_camera_widescreen.py`.

## 3. A tall stack seen from the bottom floor stops near floor 10 (open)

The log shows every piece built (rooms 255 of 255, nothing dropped, floors
-2..13, no open-air pieces, so the open-air widening is idle). The shared far
plane is 1800 (`0x004DCB0C`) and floor 13 is y 65, so the far clip is not the
limit. In scene 100 the portal walk `sub_00087F80` lets each portal through
twice per walk (the second time with the whole viewport, `0x000881FB`) and
skips a portal flagged for the view (`+0x16` bit `view+0x214`). No depth or
floor cap was found. The `[mapmaker] camera: view ...` line (from the
`sub_00089B90` wrapper, scene 100) prints, per change: view slot, camera
room, visible rooms of the total, the floor range of the visible rooms (room
box y / 5) and the top room, the deepest walk depth, far plane and FOV.
`[mapmaker] camera: actor ...` lines give each used camera's state, flags,
operator, angles, limits and sweep speeds.

## Playtest

Take a camera through its switch: it must stop sweeping, follow only the
mouse/stick, and resume sweeping after release; repeat a second time.
Story CCTV turrets (Breaking and Entering) unchanged. At 16:9 and 21:9 the
camera frame, tint, scanlines and static must reach the window edges, corner
brackets stay square and labels sit by their corners; at 4:3 nothing changes.
Check one ordinary mounted gun too. Send the log with the `[mapmaker] camera:`,
`[PC-DISPLAY] View:` and `[PC-DISPLAY] Camera view:` lines.
`TSFP_MAPMAKER_AREA_OFF=camera` restores the stock update.
