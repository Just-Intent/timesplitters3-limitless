"""MapMaker editor on widescreen displays: grid, tiles and pointer share one 2D fit."""
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]
POLICY = ROOT / 'src/pc_mapmaker_widescreen.inc'
UI = ROOT / 'src/pc_display_ui.inc'

# Each mutation breaks one contract the fixture must detect.
MUTATIONS = {
    'any-scene': (POLICY, '(int32_t)WORD_AT(0x0079094Cu)==MAPMAKER_WIDESCREEN_SCENE && ', ''),
    'also-4x3': (POLICY, ' && pc_display_wide_now();', ';'),
    'cannot-disable': (POLICY, "mapmaker_widescreen.enabled=!(v && v[0]=='0' && !v[1]);", 'mapmaker_widescreen.enabled=1;'),
    'stretch-field': (POLICY, '#define MAPMAKER_WIDESCREEN_CENTRE 2', '#define MAPMAKER_WIDESCREEN_CENTRE 1'),
    'lines-unscoped': (POLICY, '{ mapmaker_widescreen_scope(original,0x0030A550u); }', '{ original(); }'),
    'view-unscoped': (POLICY, '{ mapmaker_widescreen_scope(original,0x00310A20u); }', '{ original(); }'),
    'no-restore': (UI, '    original();\n    ui_place(old,old_tag);\n}\n\nvoid pc_display_camera_feed', '    original();\n}\n\nvoid pc_display_camera_feed'),
    'backdrop-squeezed': (UI, '    case 0x0030A580u: /* MapMaker editor backdrop', '    case 0x0030A581u: /* MapMaker editor backdrop'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        path, old, new = MUTATIONS[mutation]
        text = path.read_text(encoding='utf-8')
        assert old in text
        (tmp_path / path.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('widescreen.exe' if os.name == 'nt' else 'widescreen')
    source = ROOT / 'tests/mapmaker_widescreen.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/wd4996', '/I' + str(tmp_path), '/I' + str(ROOT / 'src'),
                str(source), '/Fe:' + str(exe), '/Fo:' + str(tmp_path / 'widescreen.obj')]
    else:
        args = [compiler, '-std=c11', '-O2', '-D_POSIX_C_SOURCE=200809L', '-I' + str(tmp_path),
                '-I' + str(ROOT / 'src'), str(source), '-o', str(exe), '-lm']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_widescreen(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = dict(os.environ)
    env.pop('TSFP_MAPMAKER_WIDESCREEN', None)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30, env=env)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker widescreen ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_widescreen_wiring():
    """The hooks are the grid view's two US entry points; the backdrop caller is exact."""
    text = POLICY.read_text(encoding='utf-8')
    assert 'void pc_mapmaker_grid_view(void (*original)(void))' in text
    assert 'void pc_mapmaker_grid_lines(void (*original)(void))' in text
    assert 'guest_writable' not in text.split('#include <stdlib.h>', 1)[1]  # per-frame path
    ui = UI.read_text(encoding='utf-8')
    assert 'void pc_display_ui_scope(void (*original)(void),int placement,uint32_t tag)' in ui
    header = (ROOT / 'src/pc_display_menu.h').read_text(encoding='utf-8')
    assert 'void pc_display_ui_scope(void (*original)(void),int placement,uint32_t tag);' in header
