"""Compile the MapMaker editor pointer prototype against fake guest memory on any host."""
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]
ADAPTER = ROOT / 'src/pc_mapmaker_pointer.inc'
POLICY = ROOT / 'src/pc_mapmaker_pointer.h'

# Each mutation breaks one contract the fixture must detect.
MUTATIONS = {
    'default-on': (ADAPTER, "mapmaker_pointer.enabled=v && v[0]=='1' && !v[1];", 'mapmaker_pointer.enabled=1;'),
    'any-scene': (ADAPTER, '(int32_t)WORD_AT(0x0079094Cu)==MAPMAKER_POINTER_SCENE &&', ''),
    'child-menu': (ADAPTER, ' && !WORD_AT(menu+0x170);', ';'),
    'still-mouse-overrides': (ADAPTER, 'if(moved || pressed) mapmaker_pointer_place(x,y);', 'mapmaker_pointer_place(x,y);'),
    'no-max-clamp': (ADAPTER, 'if(px>FLOAT_AT(b+0x50)) {', 'if(0) {'),
    'wrong-player': (ADAPTER, 'b=MAPMAKER_POINTER_BLOCK+player*MAPMAKER_POINTER_STRIDE;', 'b=MAPMAKER_POINTER_BLOCK;'),
    'held-click-leaks': (POLICY, 'if (active != s->active) s->blocked = left_down;', ''),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        path, old, new = MUTATIONS[mutation]
        text = path.read_text(encoding='utf-8')
        assert old in text
        (tmp_path / path.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('pointer.exe' if os.name == 'nt' else 'pointer')
    source = ROOT / 'tests/mapmaker_pointer.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-o', str(exe), '-lm']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_pointer(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if k != 'TSFP_MAPMAKER_POINTER'}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker pointer ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_pointer_wiring():
    guest = (ROOT / 'src/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    assert '#include "pc_mapmaker_pointer.inc"' in guest
    # After the original stick update, before any early return of the menu hook.
    hook = guest.split('void pc_display_menu_mouse_input(void (*original)(void))', 1)[1]
    assert hook.index('original();') < hook.index('mapmaker_pointer_input(menu);') < hook.index('return;')
    for host in ('src/pc_controls.c', 'src/native/input_sdl.c'):
        text = (ROOT / host).read_text(encoding='utf-8')
        assert 'void pc_controls_mapmaker_pointer(int active)' in text
        click = text.split('int pc_controls_terminal_click(void)', 1)[1].split('return down;', 1)[0]
        assert 'pc_mapmaker_click(&mapmaker_click,' in click
