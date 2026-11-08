"""MapMaker debug panel (Shift+F9): shared model, store and wiring.

The fixture runs the pure rate/format code on every host and, on Windows,
the store in src/diagnostics/pc_debug.c (refresh cadence, option gating,
toggle, one rate-limited stderr snapshot). Mutations must fail it."""

import os
from pathlib import Path
import subprocess
import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
PANEL = ROOT / 'src/diagnostics/pc_debug_panel.h'
STORE = ROOT / 'src/diagnostics/pc_debug.c'

PANEL_MUTATIONS = {
    'restart-window': ('            frames = b->swap_frames;   /* the window restarted in between */\n', '            frames = 0;\n'),
    'present-in-work': ('out->work_ms = rest > present ? (double)(rest - present) * ms : 0.0;', 'out->work_ms = (double)rest * ms;'),
    'backwards-draws': ('        b->draws_vb >= a->draws_vb && ', '        '),
    'zero-cap-shown': ('    if (cap) snprintf(out, bytes, "%u/%u", value, cap);', '    if (1) snprintf(out, bytes, "%u/%u", value, cap);'),
    'queries-unordered': ('mb->at_ms > ma->at_ms && ', ''),
    'snapshot-title': ('    for (i = 1; i < count && used < bytes; i++)', '    for (i = 0; i < count && used < bytes; i++)'),
}
STORE_MUTATIONS = {
    'refresh-every-frame': ('    if(now-panel_refreshed>=PC_DEBUG_PANEL_REFRESH_MS) panel_refresh_locked(now);\n    ReleaseSRWLockExclusive(&debug_lock);\n}\n\nint pc_debug_mapmaker_due',
                            '    panel_refresh_locked(now);\n    ReleaseSRWLockExclusive(&debug_lock);\n}\n\nint pc_debug_mapmaker_due'),
    'log-every-toggle': ('(!panel_logged || now-panel_logged>=PC_DEBUG_PANEL_LOG_MS)', '1'),
    'panel-ignores-option': ('    if(options.show_mapmaker_panel) {\n        /* Hosts', '    if(1) {\n        /* Hosts'),
    'publish-no-cadence': ('    due=now-panel_published>=PC_DEBUG_PANEL_REFRESH_MS;', '    due=1;'),
}


def build(tmp_path, store, panel_text=None, store_text=None):
    compiler, env = native_compiler(tmp_path)
    if panel_text is not None:
        (tmp_path / 'pc_debug_panel.h').write_text(panel_text, encoding='utf-8')
    if store_text is not None:
        (tmp_path / 'pc_debug.c').write_text(store_text, encoding='utf-8')
    (tmp_path / 'pc_network_build.h').write_text('#define TSFP_NETWORK_COMPATIBILITY_HEX "0123456789abcdef"\n', encoding='utf-8')
    (tmp_path / 'recomp_config.h').write_text('const char *recomp_config_lookup(const char *env, const char *key);\n', encoding='utf-8')
    exe = tmp_path / ('panel.exe' if os.name == 'nt' else 'panel')
    source = ROOT / 'tests/diagnostics/debug_panel.c'
    if os.name == 'nt':
        # The fixture's own folder is searched first; put the (mutated) copies there.
        local = tmp_path / 'debug_panel.c'
        local.write_text(source.read_text(encoding='utf-8'), encoding='utf-8')
        args = [compiler, '/nologo', '/std:c11', '/W3', '/WX', '/D_CRT_SECURE_NO_WARNINGS', '/I' + str(tmp_path),
                '/I' + str(ROOT / 'src/diagnostics'), str(local), '/Fe:' + str(exe)]
        if store:
            if store_text is None:
                (tmp_path / 'pc_debug.c').write_text(STORE.read_text(encoding='utf-8'), encoding='utf-8')
            args += ['/DTEST_STORE', 'shell32.lib', 'user32.lib']
    else:
        local = tmp_path / 'debug_panel.c'
        local.write_text(source.read_text(encoding='utf-8'), encoding='utf-8')
        args = [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-I' + str(tmp_path),
                '-I' + str(ROOT / 'src/diagnostics'), str(local), '-o', str(exe), '-lm']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return subprocess.run([str(exe)], cwd=tmp_path, capture_output=True, text=True, timeout=30)


def test_debug_panel_model(tmp_path):
    result = build(tmp_path, store=False)
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.parametrize('mutation', sorted(PANEL_MUTATIONS))
def test_debug_panel_model_mutations(tmp_path, mutation):
    old, new = PANEL_MUTATIONS[mutation]
    text = PANEL.read_text(encoding='utf-8')
    assert text.count(old) == 1, mutation
    result = build(tmp_path, store=False, panel_text=text.replace(old, new))
    assert result.returncode != 0, mutation


@pytest.mark.skipif(os.name != 'nt', reason='the store test links the Win32 build of pc_debug.c')
def test_debug_panel_store(tmp_path):
    result = build(tmp_path, store=True)
    assert result.returncode == 0, result.stdout + result.stderr
    snapshots = [l for l in result.stderr.splitlines() if l.startswith('[PC-DEBUG] MapMaker panel:')]
    assert len(snapshots) == 1, result.stderr      # two toggles within a second: one line
    assert snapshots[0].endswith('(panel shown)') and 'characters 40/64' in snapshots[0]


@pytest.mark.skipif(os.name != 'nt', reason='the store test links the Win32 build of pc_debug.c')
@pytest.mark.parametrize('mutation', sorted(STORE_MUTATIONS))
def test_debug_panel_store_mutations(tmp_path, mutation):
    old, new = STORE_MUTATIONS[mutation]
    text = STORE.read_text(encoding='utf-8').replace('\r\n', '\n')
    assert text.count(old) == 1, mutation
    result = build(tmp_path, store=True, store_text=text.replace(old, new))
    lines = [l for l in result.stderr.splitlines() if l.startswith('[PC-DEBUG] MapMaker panel:')]
    assert result.returncode != 0 or len(lines) != 1, mutation


def test_debug_panel_wiring():
    read = lambda p: (ROOT / p).read_text(encoding='utf-8')
    # A persisted shared option, default off, in every settings dialog.
    assert 'PC_DEBUG_OPTION(MAPMAKER, show_mapmaker_panel, "Show MapMaker debug panel (Shift+F9 toggles during play)", 0)' \
        in read('src/diagnostics/pc_debug_options.def')
    # Shift+F9 on both hosts; plain F9 keeps the frame counter.
    patch = read('patches/diagnostics/2640-mapmaker-debug-panel.patch')
    assert '+        if (GetAsyncKeyState(VK_SHIFT) & 0x8000)\n+            pc_debug_toggle_mapmaker();\n+        else\n+            pc_debug_toggle_fps();' in patch
    assert '+        pc_debug_render(&render);' in patch and '+unsigned long hle_d3d8_texture_binds(void) { return g_bound_count; }' in patch
    sdl = read('src/native/input_sdl.c')
    assert 'if(e->key.mod&SDL_KMOD_SHIFT)pc_debug_toggle_mapmaker();else pc_debug_toggle_fps();' in sdl
    assert 'debug_now.show_mapmaker_panel==!debug_before.show_mapmaker_panel' in sdl   # native self-test
    # Drawn by the shared sidebar canvas while the sidebar is closed.
    sidebar = read('src/interface/pc_developer_sidebar.inc')
    assert 'unsigned panel=developer_panel_lines(width,height,out,capacity);' in sidebar
    assert 'pc_debug_panel_text(text,PC_DEBUG_PANEL_LINES)' in sidebar
    # The game-thread reading runs from the developer guest frame and is gated.
    assert 'pc_mapmaker_panel_guest_tick();' in read('src/interface/pc_developer_guest.inc')
    assert '#include "pc_mapmaker_panel.inc"' in read('src/interface/pc_menu_mouse_guest.inc')
    guest = read('src/interface/pc_mapmaker_panel.inc')
    assert 'if(!pc_debug_mapmaker_due()) return;' in guest
    doc = read('docs/mapmaker/mapmaker-limitless.md')
    for word in ('## Debug panel (Shift+F9)', 'Show MapMaker debug panel', '[PC-DEBUG] MapMaker panel:'):
        assert word in doc, word
