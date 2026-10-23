"""Compile the opt-in MapMaker memory budget against a fake register file on any host."""
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]
BUDGET = ROOT / 'src/pc_mapmaker_budget.inc'
PATCH = ROOT / 'patches/zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-mapmaker-meter-raise.patch'

MUTATIONS = {
    'default-on': ("n>=2 && n<=8)?(unsigned)n:1u;", "n>=2 && n<=8)?(unsigned)n:2u;"),
    'unbounded': ('n>=2 && n<=8)', 'n>=2)'),
    'round-down': ('(((uint64_t)bytes+n-1)/n)', '(((uint64_t)bytes)/n)'),
    'limits-no-default': ('            mapmaker_budget.scale=8u;', '            mapmaker_budget.scale=1u;'),
    'limits-override-explicit': ("if((!v || !*v) && l", "if(l"),
    'limits-without-pools': (" && !(p && p[0]=='0' && !p[1]))", ")"),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = BUDGET.read_text(encoding='utf-8')
        assert old in text
        (tmp_path / BUDGET.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('budget.exe' if os.name == 'nt' else 'budget')
    source = ROOT / 'tests/mapmaker_budget.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_budget(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_MEMORY_SCALE', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_POOLS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker budget ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_wrapper_patch():
    patch = PATCH.read_text(encoding='utf-8')
    # Exactly the two estimate functions of sub_002FA720, additive only, after the probe.
    for name in ('sub_002FA060', 'sub_002FA0D0'):
        assert f'extern void {name}_gen(void);' in patch
        assert f'pc_display_mapmaker_memory({name}_gen);' in patch
    body = patch.split('@@', 2)[2].splitlines()
    assert not any(line.startswith('-') for line in body)
    probe = next(ROOT.glob('patches/*-mapmaker-meter-probe.patch'))
    assert probe.name < PATCH.name
    guest = (ROOT / 'src/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    assert '#include "pc_mapmaker_budget.inc"' in guest
    assert 'void pc_display_mapmaker_memory(void (*original)(void)) { original(); }' in guest
    # The budget never writes guest memory.
    budget = BUDGET.read_text(encoding='utf-8')
    assert 'WORD_AT' not in budget and 'FLOAT_AT' not in budget
