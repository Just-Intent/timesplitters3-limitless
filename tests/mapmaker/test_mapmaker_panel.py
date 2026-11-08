"""MapMaker debug panel: the game-thread reading (src/interface/pc_mapmaker_panel.inc).

Fixture tests/mapmaker/mapmaker_panel.c: read-only, gated to 250 ms by
pc_debug_mapmaker_due, Limitless caps only in scene 100 with the pools, the
wide AI table only in local MapMaker Story, pointer guards. Mutations must
fail it. The store, format and hotkeys: tests/diagnostics/test_debug_panel.py.
"""
import os
from pathlib import Path
import subprocess

import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
PANEL = ROOT / 'src/interface/pc_mapmaker_panel.inc'

MUTATIONS = {
    'not-gated': ('    if(!pc_debug_mapmaker_due()) return;\n', ''),
    'caps-without-pools': ('if(m->mapmaker && mapmaker_limits.pools) {', 'if(m->mapmaker) {'),
    'wide-ai-in-session': ('m->mapmaker && !WORD_AT(MAPMAKER_PANEL_SESSION) &&', 'm->mapmaker &&'),
    'wide-ai-any-mode': ('*(uint8_t *)((uintptr_t)MAPMAKER_PANEL_MODE+g_xbox_mem_offset)==10u ?', '1 ?'),
    'objects-ignore-freed': ('m->objects=count-freed;', 'm->objects=count;'),
    'freed-unchecked': ('if(cap && freed<=count) {', 'if(cap) {'),
    'visible-counts-culled': ('if((int32_t)WORD_AT(e)>=0 && x0<x1) visible++;', 'if(x0<x1) visible++;'),
    'scan-any-scene': ('    if(m->mapmaker && n>=1u && n<=MAPMAKER_PANEL_ROOM_CAP &&', '    if(n>=1u && n<=MAPMAKER_PANEL_ROOM_CAP &&'),
    'player-unchecked': ('MAPMAKER_PANEL_GUEST_OK(ent=WORD_AT(player+0x14u),0x70u)', '(ent=WORD_AT(player+0x14u))!=0u'),
    'writes-guest': ('    m->heap_known=1;\n', '    m->heap_known=1;WORD_AT(MAPMAKER_PANEL_LEVEL_FREE)^=1u;\n'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = PANEL.read_text(encoding='utf-8').replace('\r\n', '\n')
        assert text.count(old) == 1, mutation
        (tmp_path / PANEL.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('panel.exe' if os.name == 'nt' else 'panel')
    source = ROOT / 'tests/mapmaker/mapmaker_panel.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/W4', '/WX', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src/diagnostics'),
                str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), '-I' + str(ROOT / 'src/diagnostics'),
                str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_panel(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
    else:
        assert result.returncode != 0, mutation
