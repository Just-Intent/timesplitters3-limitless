# Limitless MapMaker development roadmap

Status: proposal after phase-1 reconnaissance (2026-10-22). Nothing here is
implemented. Phase 2 must not start until these findings are reviewed.

Goals: (1) usable mouse and keyboard MapMaker controls, (2) remove or
substantially expand the complexity budget, (3) reliable save/load/play of larger
maps, (4) keep original capabilities and existing map compatibility.

## 1. Extension strategy comparison

| Option | What it means | For | Against | Verdict |
| --- | --- | --- | --- | --- |
| **A. Modify the recompiled runtime** (edit lifter output or runtime-wide behaviour) | Change generated C or the lifter to alter game logic | Full access | Generated C must never be hand-edited (root `AGENTS.md`); lifter changes affect every function; not reviewable | Reject for game logic. Use only for genuine runtime bugs. |
| **B. Native hooks in shared source** (existing project pattern) | Wrap specific guest functions in `recomp_manual.c` via a new tracked patch; put policy in testable root `src/` files | Already proven ~116 times (menu mouse, terminal pointer, sliders, mouse-look); shared Windows/Mac; asset-free C fixtures + original-routine tests; reviewable and reversible; original data formats untouched by default | Needs exact addresses and struct layouts from the local XBE; new wrappers require regeneration; guest-memory writes need careful bounds checks | **Recommended** for controls and for the budget. |
| **C. Separate compatible editor** (standalone PC app reading/writing map saves) | Own UI over the map file format, game used only to play | Free UI design; mouse native; no guest-code risk while editing | Requires the full save format (unknown, possibly signed); duplicates the editor's rules (tile connection, logic, lights); cannot remove in-game limits that apply at load/play time (C5/C6 in the capacity file); large scope | Defer. Reconsider only if phase 2 shows the in-game editor UI is unworkable with hooks. A format reader/validator is still useful as a test tool. |

Recommendation: **Option B**, staged. Controls first (they add capability
without changing data), budget second (it changes data and needs proof that
arrays, level building and saves can take more).

## 2. Phases

### Phase 2a: map the editor (read-only, Windows host with local XBE)

Deliverables update `research/*.md` with confirmed addresses.

- E1: meter probe (below).
- Trace the meter fraction to counters/capacities (offline in local generated C).
- Locate cursor state, camera, active-tool state and editor menus (H-IN1…H-IN4).
- Observe the save routine and file layout (H-SV1…H-SV4).
- Record the in-game Controls screen mapping.

Exit criterion: confirmed addresses for meter inputs, per-category counters,
placement checks, cursor position and the map save path.

### Phase 2b: mouse and keyboard controls

1. Context: a shared `mapmaker_context()` (editor scene 102 plus an editor-state check)
   in a new testable `src/pc_mapmaker*.inc`, following `pc_menu_mouse_guest.inc`.
2. Let editor menus use the existing menu-mouse engine if H-IN3 holds.
3. Cursor: release mouse capture in the editor and convert the pointer to
   cursor/grid movement by writing the confirmed cursor state, like the
   terminal pointer (`src/pc_terminal.h`). Wheel for floor/zoom, right-drag for
   camera, keyboard shortcuts for tools; bindings live in the shared controls
   model, not a Mac copy.
4. Tests: C fixture with mutations; original-routine test; bounded Windows run.
   Add a `MAC_PORT_BACKLOG.md` entry; Mac validation stays separate.

### Phase 2c: budget expansion (only after 2a proves C3, C5, C6)

1. Determine the real capacities of the editor arrays, level-building pools and
   save buffer.
2. If arrays are fixed, relocate them to larger host-allocated or guest-heap
   storage via hooks; do not just raise the compare.
3. Introduce a **size-gated** format: maps within original limits save in the
   original format; larger maps use an extension flagged so the original game
   rejects them cleanly. Keep larger maps local-only (no System Link transfer)
   until tested.
4. Test: maximum-original, 1.5x and 2x maps through edit → save → reload → play
   with bots, watching heap, pools and frame time.

### Phase 2d: hardening

Regression sets of saved maps (original-format round trip byte-identical where
nothing changed), Mac adapter validation, documentation and `STATUS.md` entries.

## 3. Smallest safe first experiment (E1: read-only meter probe)

**Status (2026-10-22): implemented on `mapmaker-recon`, awaiting the first
Windows run.** Usage and protocol: `docs/mapmaker-meter-probe.md`. The
implementation differs from the sketch below in one respect: instead of
guessing the painter's internal return address, it also wraps `sub_0030C210`
as a scope marker. That costs one guest regeneration but attributes bars to the
meter without any unverified address.

Why it is smallest: `sub_000814B0` is **already wrapped** on Windows and Mac
(`patches/*-menu-setting-sliders.patch` → `pc_display_menu_slider_bar` in
`src/pc_menu_sliders.inc:58-69`). That wrapper already reads the caller's return
address (`WORD_AT(g_esp)`) and the bar arguments. A log-only branch for the
MapMaker caller needs **no new guest wrapper and no regeneration**, only a
rebuild, and changes no game state.

Steps:

1. Offline (local generated C, never committed): find the return address(es)
   inside `sub_0030C210` that follow its call to `sub_000814B0`.
2. Add a debug-only, environment-gated branch (for example
   `TSFP_MAPMAKER_METER_LOG=1`) in `pc_display_menu_slider_bar`: when the return
   address matches, log the fraction and geometry at most once per change to a
   bounded file under the disposable test root. Default off; no behaviour change.
3. Asset-free test: extend the existing slider fixture so the MapMaker return
   address logs and nothing else changes (mutation: logging must never alter
   arguments or call order).
4. Bounded Windows run with the disposable unlocked profile
   (`tools/testing_profile.py --play --seconds 120`): open MapMaker, place one of
   each category, then several of one type versus several different types.
5. Record fractions per action in `research/capacity-constraints.md`.

Expected outcomes: per-category costs and whether variety costs more, which
directly separates hypotheses C1/C8 and points at the counters for the next
step. If the meter is not drawn through `sub_000814B0` on the editing screen,
that is itself a finding and costs one run.

Rollback: delete the branch; no save, setting or format is touched.

## 4. Open issues

Tracked separately; do not merge their diagnosis.

### MM-1: MapMaker probe fixture fails on Windows (fixed; passes on Windows 2026-10-22)

- **Observed (2026-10-22, Windows 11, MSVC):** `tests/test_mapmaker_probe.py`
  failed the `repeats=11\n` check (`tests/mapmaker_probe.c:148`) and the
  `n<=MAPMAKER_PROBE_CAP+128` check (`:189`). Linux/gcc passes.
- **Likely cause (not yet confirmed):** `src/pc_mapmaker_probe.inc:95` opens the log
  with `fopen(path,"w")`. On Windows that is text mode, so each `\n` is
  written as `\r\n`. The fixture reads the file in binary (`"rb"`), so
  `"repeats=11\n"` is not found. The cap counts bytes before the newline
  translation, so the file exceeds the 4 MiB cap by about one byte per line.
  This also affects real Windows logs, which use CRLF and run slightly past
  the cap.
- **Proposed fix (not applied):** open the log in binary mode (`"wb"`), then rerun
  the fixture on Windows. Until then, treat the fixture as failing on Windows.
- **Fix (2026-10-22):** the log is now opened with `"wb"`, and the fixture
  asserts it contains no `\r`. Linux passes; rerun `tests\test_mapmaker_probe.py`
  on Windows to close this.
- **Independent of** the black-screen startup report (MM-2).

### MM-2: black screen after direct launch of the Windows build (resolved by launch settings)

- **2026-10-22 update:** with `RECOMP_TRACE_BUDGET=0`, output redirected to
  files and the other isolated settings, the game reaches its menus and
  gameplay. No source change was needed.

- **Observed:** a directly launched `futureperfect_recomp.exe` (with
  `RECOMP_GAME_DIR` set) shows the PC Settings menu and the version text but
  never reaches the main menu. The process stays responsive, and the console
  fills with `[TRACE]` lines.
- **Leading explanation (unverified):** this build is lifted with
  `--trace-all-entries`. Without `RECOMP_TRACE_BUDGET=0`, each guest function
  entry writes and flushes a line to the attached console, up to 400,000
  lines. The official `tsfp.ps1 Run` sets the budget to 0 and sends output to
  files for exactly this reason.
- **Next step:** relaunch through the bounded Test Lab runner, or directly with
  the budget at 0 and output redirected to files, before changing any source.

### MM-3: no movement or firing in earlier direct launches (cause unconfirmed)

- **Observed:** in earlier launches started some other way, mouse look and
  wheel weapon switching worked, but W/A/S/D movement, firing and the
  MapMaker cursor did not. Starting the game with the full isolated settings
  (including `RECOMP_INPUT_CONFIG` pointing at a copy of
  `tests/fixtures/pc-controls.json`) fixed all three.
- **Ruled out:** a profile-name typing-mode latch. Creating the profile
  differently did not change the behaviour.
- **Candidate (unverified):** without `RECOMP_INPUT_CONFIG`, the runtime
  loads `%APPDATA%\xboxrecomp\input_bindings.json`, then an
  `input_bindings.json` in the current folder (`input_bindings.c:627-661`).
  A stale file in either place would replace the project's bindings.

### MM-4: MapMaker category toolbar ignores mouse clicks (open)

- **Observed:** with the full launch settings, the pointer appears in
  MapMaker and some elements accept clicks. The category icons (tile, item,
  light, AI, logic, settings) and their left/right arrows do not, even when the
  pointer is placed exactly on them.
- **Likely reason (inference from code):** the shared mouse layer clicks only
  generic menu rows of item types 4, 7 and 8 with stored rectangles
  (`src/pc_menu_mouse_guest.inc:481-483`). Custom widgets such as the
  character, team and choice-arrow carousels each needed their own handler,
  and this toolbar appears to be another one. Its item type, rectangles and
  selection routine are unknown.
- **Next step (implemented, awaiting a run):** probe flag `8` logs each menu
  laid out while scene 100 is active and every left click's pointer position
  and hit item. See the toolbar steps in `docs/mapmaker-meter-probe.md`.
- **2026-10-22 run:** the meter log (flags 11) recorded 634 meter draws, all
  with `scene=102`, and no menu or click records: flag 8 was gated on scene
  100, so the capture never ran. The gate now accepts scene 102 (editor) and
  100; rerun the toolbar steps.
- **Consequence:** scene 102 is above 100, so `menu_mouse_context()` already
  treats the editor as a frontend menu (pointer shown, generic rows of types
  4/7/8 clickable). Toolbar and grid clicks fail because they are not such
  rows, not because the editor is in gameplay mode.

- **2026-10-22 capture (second run):** toolbar, floor and grid clicks all hit
  nothing; the editor root menu has no items. The editor HUD hover-tests the
  game's own per-player pointer (`0x007BA260 + player*0x1E0 + 0x20/+0x24`,
  moved by `sub_00074C50`). See `research/mapmaker-investigation.md` §3a.
  Proposed fix: drive that pointer from the mouse in scene 102 (covers MM-5).
- **Prototype (2026-10-22, off by default):** `TSFP_MAPMAKER_POINTER=1` places
  the game pointer at the mouse on the editor screen and holds A while the left
  button is held. Fixture passes on Windows/MSVC; in-game run pending. See
  `docs/agent-context/systems.md` (MapMaker editor mouse controls).
- **First in-game run (2026-10-23):** toolbar icons and arrows respond to
  clicks; clicking the grid at the top level opens the highlighted category
  (A). Pause-menu and category-list rows were hit and highlighted but did not
  activate: the editor reads A itself and ignores the menu-engine confirm (row
  callbacks there are highlight callbacks, e.g. `0x00316500`). Row clicks in
  scene 102 now hold A for that press instead; rerun pending. Floor buttons
  are not hover-tested (no `sub_000711F0` caller found for them); they likely
  need a click-to-button mapping once the floor input and state are known.
- **Second run (2026-10-23):** row clicks still did nothing, and the mode
  dialog (beginner/advanced) only accepted a click when the game's own cursor
  was over the row. Editor menus (flag `0x8000` in `+0x40`) select the row under
  the game pointer, so the pointer now follows the mouse in every scene-102
  menu; only the editor screen owns held A.
- **Third run (2026-10-23):** toolbar, dialogs, pause menu and lists respond.
  Added (prototype, untested in game): Escape as Back in the editor; scrollbar
  arrows/track (A on the bar) and thumb drag (engine drag speed at
  `player+0x34`); right-drag and window-edge panning. The game already maps
  the pointer to the grid cell (`editor+0x2F2E/+0x2F30`), so no screen-to-grid
  code is needed. Logic-menu rows still fail; the probe click record now also
  logs the menu descriptor, the game pointer and the engine selection to find
  out why. Mouse-wheel zoom (held-button bits 0x8000/0x4000) is not done.
- **Fourth run (2026-10-23):** logic rows still failed. The new click fields
  showed the active menu was the logic list's description panel (descriptor
  `0x0052E9D8`, one type-8 item), a mode-2 child of the list (`0x0052E9F8`);
  the clicks were on the parent's rows. Fixed through the parent (prototype).
  Right-click rotate was lost by the pan change and is restored for clicks
  without a drag. T raises the floor (the right trigger's key is the left
  mouse button); A/D/W/S no longer act as the D-pad in the editor. Story AI
  list scrolling fails; the click record now also logs the menu's scrollbar
  fields. Zoom: the editor's Controls readout should name the zoom buttons.
- **Fifth run (2026-10-23):** rotate and the logic list's left rows work. The
  logic panel's right-hand entries are drawn inside one non-selectable type-8
  item and hit-tested by the game's own pointer code, so a click there now
  presses A. Escape-as-Back had made the pause menu unreachable (Start opens
  it from the editor screen; Back there is the 3D view): Escape is now Back
  only while a menu is open. Scroll-press logging (probe flag 8) records the
  computed scrollbar geometry for the Story AI investigation.
- **Sixth run (2026-10-23):** the whole logic menu works. The Story AI bar
  geometry matched the clicks (bar x 309.4-330.6, y 77.3-436.8) and A was
  pressed there, but the engine did not scroll; the PC side now sets the
  scroll target itself. The floor-up hint showed the left mouse button (the
  right trigger's key); in the editor it now shows T. R also raises the floor.
  Zoom uses held bits 0x8000/0x4000; by the menu table's pairing (0x400/0x800
  are the triggers) these are likely White/Black (keys 1/2), unconfirmed.
- **Seventh run (2026-10-23):** keys 1/2 zoom (confirmed); T and its hint
  work; tile/item lists scroll by arrows, drag and wheel; Story AI and logic
  submenus by arrows and drag only (their wheel moved the row selection,
  which they ignore). The wheel now scrolls editor scrollbars directly and
  zooms on the editor screen. Toolbar clicks while a menu is open close the
  menu (Back) and then press A. Wheel zoom direction and the toolbar strip
  bounds (x 370-620, y 35-70, from recorded clicks) need in-game checks.
- **Eighth run (2026-10-23):** wheel zoom and wheel scrolling work. A toolbar
  click with a list open opened the 3D view: a second Back reached the editor
  screen while the list was still closing. Back now goes only to a menu that
  is fully open (state 3) with no open child. Edge panning needed the mouse
  at the very edge: the zone is now 24 UI pixels with up to 0.25 cells/frame.
- **Ninth run (2026-10-23):** edge and right-drag panning are good. Toolbar
  switching still opened the 3D view: editor lists close on B, not Back
  (Back is the 3D view), so the switch and Escape-in-menus now use B. The
  movement keys no longer move the pointer; A/D step the toolbar. A right
  click on an empty cell with nothing held clears the selection (Y).
- **Tenth run (2026-10-23):** Escape, A/D toolbar stepping, right-click
  deselect and rotate all work. The toolbar switch closed the list and then
  reopened the same one: A arrived before the toolbar took the hover of the
  clicked icon. On the editor screen the switch now moves the pointer one
  pixel off and back onto the icon and waits 120 ms before A.
- **Eleventh run (2026-10-23):** the clicked icon now gets the highlight but
  its list did not open, and clicking the open list's own icon closed it.
  The switch now waits 300 ms on the editor screen and presses A for 120 ms,
  and uses the toolbar's own icon hit test (`sub_00335890`: rectangles at
  `0x00765BA8`, 5 icons in advanced / 3 in beginner mode via `editor+0x200`)
  to leave the list open when the click is on the current category
  (`editor+0x254`).
- **Twelfth run (2026-10-23):** still no reliable click switch. Decision:
  with a list open, toolbar clicks do nothing and A/D switch categories; the
  switch code (generated B/A presses, icon hit test) was removed.
- **Milestone (2026-10-23):** the user confirmed the MapMaker UI fully usable
  with mouse and keyboard (commit `404e113`). The controls are now on by
  default; `TSFP_MAPMAKER_POINTER=0` turns them off. MM-4 and MM-5 are done.

## 5. Capacity (phase 2c) status, 2026-10-23

See `research/capacity-constraints.md` §1b. The meter combines a 32 KiB
map-container byte budget and seven per-category budgets; in practice a
category budget fills first. Next: a read-only probe (flag 16) logging the
container bytes (`+0x248`), `editor+0x270/+0x274` and each category's count
per placement, to name the binding category and its play-time pool.
Implemented (probe flag 16, use 17 or 27). First run: the binding budget
is the level memory estimate (`sub_002FA060`, 2,048,000 bytes), not the
container (13788 bytes at the limit). Next: find the play-time pool behind
it, then test a raised estimate in memory up to the 200-piece cap, with the
original save format (no format change needed below about 280 tiles).
Static analysis: the 2 MB is a designer budget over the shared ~43 MB Level
heap; overflow corrupts silently. Probe flag 32 logs heap state and the
minimum free during a match; next run: a full MapMaker map versus a large
arcade map.

### MM-5: MapMaker cursor is keyboard/stick only (expected; phase 2b)

- Moving the editor cursor with W/A/S/D works, but is slow and awkward.
  Pointer-driven cursor movement needs the editor's cursor/grid state address
  (H-IN4); this is the main phase 2b controls work.

