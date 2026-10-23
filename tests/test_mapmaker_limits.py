"""Compile MapMaker capacity stage A (AI pools and editor limits) against fake guest memory."""
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]
LIMITS = ROOT / 'src/pc_mapmaker_limits.inc'
PATCH = ROOT / 'patches/zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-mapmaker-meter-stagea.patch'

MUTATIONS = {
    'limits-without-pools': ('mapmaker_limits.limits=mapmaker_limits.pools && l &&', 'mapmaker_limits.limits=l &&'),
    'pools-off-by-default': ("mapmaker_limits.pools=!(p && p[0]=='0' && !p[1]);", 'mapmaker_limits.pools=p!=NULL;'),
    'pools-any-scene': (' || WORD_AT(0x0079094Cu)!=100u) {', ') {'),
    'pools-not-cleared': ('memset((void *)((uintptr_t)p+g_xbox_mem_offset),0,bytes);', '(void)0;'),
    'cover-raised': ('#define MAPMAKER_LIMIT_COVER 1024u', '#define MAPMAKER_LIMIT_COVER 1280u'),
    'cover-pool-stock': ('#define MAPMAKER_POOL_COVER 1536u', '#define MAPMAKER_POOL_COVER 1280u'),
    'no-held-pieces': ('p.pieces+h.pieces>MAPMAKER_LIMIT_PIECES', 'p.pieces>MAPMAKER_LIMIT_PIECES'),
    'sub-raised': ('#define MAPMAKER_LIMIT_SUB 200u', '#define MAPMAKER_LIMIT_SUB 400u'),
    'categories-always': ('    if(!mapmaker_limits.limits) return;\n    mapmaker_categories_apply();',
                          '    mapmaker_categories_apply();\n    if(!mapmaker_limits.limits) return;'),
    'categories-overwrite': (' && WORD_AT(a)==mapmaker_categories[i].stock)', ')'),
    # Turrets: 3 x 24 = 72 of the 80 gun/owner slots (config/guest-code-patches/objects.json).
    'turrets-past-pool': ('    {0x00524E6Cu,8u,24u},', '    {0x00524E6Cu,8u,32u},'),
    'turrets-stock': ('    {0x00524E9Cu,8u,24u},', '    {0x00524E9Cu,8u,8u},'),
    # Teleporters need the sub_0009CD00 wrapper in pc_mapmaker_objects.inc.
    'teleporters-missing': ('    {0x00524D64u,2u,8u},\n', ''),
    'teleporters-past-wrapper': ('    {0x00524D34u,2u,8u},', '    {0x00524D34u,2u,16u},'),
    'cover-props-past-x2': ('{0x00524EFCu,66u,132u}', '{0x00524EFCu,66u,264u}'),
    'flags-clobbered': ('WORD_AT(editor+0x25c)=refuse?(flags|0x400000u):(flags&~0x400000u);',
                        'WORD_AT(editor+0x25c)=refuse?0x400000u:0u;'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = LIMITS.read_text(encoding='utf-8')
        assert old in text
        (tmp_path / LIMITS.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('limits.exe' if os.name == 'nt' else 'limits')
    source = ROOT / 'tests/mapmaker_limits.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_limits(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_MEMORY_SCALE')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker limits ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_stage_a_patch():
    patch = PATCH.read_text(encoding='utf-8')
    for name, hook in (('sub_0009A140', 'pc_display_mapmaker_pools'), ('sub_002FA720', 'pc_display_mapmaker_limits')):
        assert f'extern void {name}_gen(void);' in patch and f'{hook}({name}_gen);' in patch
    assert not any(line.startswith('-') for line in patch.split('@@', 2)[2].splitlines())
    raise_patch = next(ROOT.glob('patches/*-mapmaker-meter-raise.patch'))
    assert raise_patch.name < PATCH.name
    guest = (ROOT / 'src/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    assert '#include "pc_mapmaker_limits.inc"' in guest
    assert guest.index('#include "pc_mapmaker_budget.inc"') < guest.index('#include "pc_mapmaker_limits.inc"')
    for stub in ('pc_display_mapmaker_pools', 'pc_display_mapmaker_limits'):
        assert f'void {stub}(void (*original)(void)) {{ original(); }}' in guest
