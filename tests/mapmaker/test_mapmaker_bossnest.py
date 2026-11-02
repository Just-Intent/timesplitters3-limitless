"""MapMaker split Crowes and Crow walkers in one map (v0.4.0-mapmaker.28 crash).

Compiles src/interface/pc_mapmaker_realbosses.inc and pc_mapmaker_crowe.inc
in one translation unit, as the build does, with the boss statics they share
(pc_mapmaker_bossfx.inc), against fake guest memory. Checks that the Crowe's
egg drop is skipped with esp balanced, and that every nesting of the two
areas' entries (a walker damage callback during a Crowe think with that
Crowe's hit inside it, and the reverse) sees its own state and statics,
keeps esp balanced on every path and puts every global back. Mutations of
any of the three files must be caught.
"""
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / 'src/interface'
CROWE, RBS, FX = 'pc_mapmaker_crowe.inc', 'pc_mapmaker_realbosses.inc', 'pc_mapmaker_bossfx.inc'

MUTATIONS = {
    # the crash: the egg drop makes four null props that are released on landing
    'egg-drop-kept': (CROWE, 'crw.eggs);\n    MAPMAKER_GUEST_RET();', 'crw.eggs);\n    original();'),
    'egg-no-ret': (CROWE, 'crw.eggs);\n    MAPMAKER_GUEST_RET();', 'crw.eggs);'),
    'egg-any-caller': (CROWE, 'WORD_AT(g_esp)!=CRW_RET_EGG || ', ''),
    # the overlapping statics: "the same instance again" skipped the swap
    'crowe-same-skips-swap': (CROWE, '    p.fx=mapmaker_fx_enter(k<0?0:&crw_inst[k].fx);\n    if(p.k!=k) {',
                              '    p.fx=mapmaker_fx_holder;\n    if(p.k==k) return p;\n    p.fx=mapmaker_fx_enter(k<0?0:&crw_inst[k].fx);\n    {'),
    'crowe-leave-no-fx': (CROWE, '    mapmaker_fx_leave(p.fx);\n    if(crw_cur!=p.k) crw_set(p.k);', '    if(crw_cur!=p.k) crw_set(p.k);'),
    'crowe-no-state-swap': (CROWE, '    WORD_AT(CRW_STATE)=k<0?crw_out:crw_inst[k].state;\n', ''),
    'walk-same-skips-fx': (RBS, '    s->fx=mapmaker_fx_enter(fx);\n    s->swapped=0;\n    if(same) return;\n',
                           '    s->fx=mapmaker_fx_holder;\n    s->swapped=0;\n    if(same) return;\n    s->fx=mapmaker_fx_enter(fx);\n'),
    'walk-out-no-fx': (RBS, '    if(s->swapped) WORD_AT(global)=s->ptr;\n    mapmaker_fx_leave(s->fx);\n',
                       '    if(s->swapped) WORD_AT(global)=s->ptr;\n'),
    'walk-ptr-not-restored': (RBS, '    if(s->swapped) WORD_AT(global)=s->ptr;\n', ''),
    'walk-damage-unswapped': (RBS, '    rbs_walk_call(original,WORD_AT(r));', '    original();'),
    'fx-park-lost': (FX, '    memcpy(prev?prev->b:mapmaker_fx_outside.b,live,MAPMAKER_FX_SIZE);\n', ''),
    'fx-load-lost': (FX, '    memcpy(live,c?c->b:mapmaker_fx_outside.b,MAPMAKER_FX_SIZE);\n', ''),
    'fx-holder-kept': (FX, '    mapmaker_fx_holder=c;\n', ''),
    'fx-same-swaps': (FX, '    if(prev==c) return prev;\n', '    if(prev==c && 0) return prev;\n'),
}
# 'fx-same-swaps' is equivalent (parking then loading the same copy changes nothing): kept out
EQUIVALENT = {'fx-same-swaps'}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        name, old, new = MUTATIONS[mutation]
        for n in (CROWE, RBS, FX):                    # all three next to each other: the copies are the ones found
            text = (SRC / n).read_text(encoding='utf-8')
            if n == name:
                assert text.count(old) == 1, mutation
                text = text.replace(old, new)
            (tmp_path / n).write_text(text, encoding='utf-8')
    exe = tmp_path / ('bossnest.exe' if os.name == 'nt' else 'bossnest')
    source = ROOT / 'tests/mapmaker/mapmaker_bossnest.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/wd4706', '/wd4505',
                '/I' + str(tmp_path), '/I' + str(SRC), '/I' + str(ROOT / 'src'),
                str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Wno-unused-function', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(SRC), '-I' + str(ROOT / 'src'),
                str(source), '-o', str(exe), '-lm']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *(m for m in MUTATIONS if m not in EQUIVALENT)])
def test_mapmaker_bossnest(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_REAL_BOSS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker bossnest ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_one_window_for_both_areas():
    """Both areas include the shared holder; neither copies the window on its own any more."""
    crowe, rbs = (SRC / CROWE).read_text(encoding='utf-8'), (SRC / RBS).read_text(encoding='utf-8')
    assert '#include "pc_mapmaker_bossfx.inc"' in crowe and '#include "pc_mapmaker_bossfx.inc"' in rbs
    assert 'crw_win[' not in crowe and 'memcpy(s->fx' not in rbs
    menu = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    assert menu.index('#include "pc_mapmaker_realbosses.inc"') < menu.index('#include "pc_mapmaker_crowe.inc"')
