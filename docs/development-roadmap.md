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

1. Context: a shared `mapmaker_context()` (scene 100 plus an editor-state check)
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

### MM-1: MapMaker probe fixture fails on Windows (fix applied; Windows recheck pending)

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

### MM-5: MapMaker cursor is keyboard/stick only (expected; phase 2b)

- Moving the editor cursor with W/A/S/D works, but is slow and awkward.
  Pointer-driven cursor movement needs the editor's cursor/grid state address
  (H-IN4); this is the main phase 2b controls work.

