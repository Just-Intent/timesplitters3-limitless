"""Compile the MapMaker meter probe against fake guest memory on any host."""
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / 'src/pc_mapmaker_probe.inc'

# Each mutation breaks one contract the fixture must detect.
MUTATIONS = {
    'writes-guest': ('mapmaker_probe.bars=0;', 'mapmaker_probe.bars=0;WORD_AT(g_esp+4u)^=1u;'),
    'unscoped': ('    mapmaker_probe.depth++;\n', '\n'),
    'default-on': ('mapmaker_probe.flags=(text && end!=text && !*end)?(unsigned)(value&MAPMAKER_PROBE_ALL):0;',
                   'mapmaker_probe.flags=(text && end!=text && !*end)?(unsigned)(value&MAPMAKER_PROBE_ALL):1;'),
    'menus-any-scene': ("if(!(mapmaker_probe_flags()&MAPMAKER_PROBE_MENUS) || !mapmaker_probe_active())return;",
                        "if(!(mapmaker_probe_flags()&MAPMAKER_PROBE_MENUS))return;"),
    'editor-scene-missing': ('return scene==MAPMAKER_PROBE_SCENE_EDITOR || scene==MAPMAKER_PROBE_SCENE_PLAY;',
                             'return scene==MAPMAKER_PROBE_SCENE_PLAY;'),
    'budgets-without-flag': ('        if(mapmaker_probe_flags()&MAPMAKER_PROBE_BUDGETS) mapmaker_probe_budgets(k);', '        mapmaker_probe_budgets(k);'),
    'container-any-version': ("|| WORD_AT(c+4)!=0x12Du) return 0;", ') return 0;'),
    'uncapped': ('if(mapmaker_probe.written+length>MAPMAKER_PROBE_CAP) {', 'if(0) {'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = PROBE.read_text(encoding='utf-8')
        assert old in text
        (tmp_path / PROBE.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('probe.exe' if os.name == 'nt' else 'probe')
    source = ROOT / 'tests/mapmaker_probe.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-o', str(exe)]
    # Quoted includes resolve beside the fixture first; the mutated copy is
    # found through the leading -I directory.
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_probe(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_MAPMAKER_PROBE')}
    result = subprocess.run([str(exe), str(tmp_path / 'probe.log')], env=env,
                            capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker probe ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_meter_wrapper_patch():
    patch = next(ROOT.glob('patches/*-mapmaker-meter-probe.patch')).read_text(encoding='utf-8')
    sliders = next(ROOT.glob('patches/*-menu-setting-sliders.patch'))
    assert 'extern void sub_0030C210_gen(void);' in patch
    assert 'pc_display_mapmaker_meter(sub_0030C210_gen);' in patch
    body = patch.split('@@', 2)[2].splitlines()
    assert sum(line.startswith('+') for line in body) == 12
    assert not any(line.startswith('-') for line in body)  # additive only
    # Its context is the bar wrapper added by the slider patch, so it must sort later.
    assert sliders.name < next(ROOT.glob('patches/*-mapmaker-meter-probe.patch')).name
    sliders_src = (ROOT / 'src/pc_menu_sliders.inc').read_text(encoding='utf-8')
    assert 'mapmaker_probe_bar();' in sliders_src.split('void pc_display_menu_slider_bar', 1)[1].split('original();', 1)[0]
    guest = (ROOT / 'src/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    assert 'void pc_display_mapmaker_meter(void (*original)(void)) { original(); }' in guest
    # Menu capture uses the existing sub_00073750 wrapper: the caller is read
    # before the original returns, the layout after, and clicks after hit testing.
    hook = guest.split('void pc_display_menu_mouse(void (*original)(void))', 1)[1]
    assert hook.index('uint32_t caller=WORD_AT(g_esp);') < hook.index('original();') < \
        hook.index('mapmaker_probe_menu(menu,caller);') < hook.index('if(!menu_mouse_context()')
    assert hook.index('choice_direction=menu_choice_hit(') < hook.index('mapmaker_probe_click(menu,x,y,hit,actions);')
    assert 'fopen(path,"wb")' in PROBE.read_text(encoding='utf-8')
