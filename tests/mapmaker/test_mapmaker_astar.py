"""AI group route search sub_0009B540 in C (src/interface/pc_mapmaker_astar.inc).

Fixture tests/mapmaker/mapmaker_astar.c (counts vs every list walked, recorded
results, a hand-built chain, the pool bound) with mutations; and, with the
private XBE, the original guest code 0x0009B090..0x0009B629 run in
tests/mapmaker/guest_emulator.py on the fixture's pre image (pads.json's node
pool relocation applied) must give the same return values and guest memory
as the C search, for a series of 60 searches per graph.
"""
import json
import os
from pathlib import Path
import struct
import subprocess

import pytest

from native_compiler import native_compiler
from guest_emulator import Emu, Xbe, private_dir, relocations

ROOT = Path(__file__).resolve().parents[2]
ASTAR = ROOT / 'src/interface/pc_mapmaker_astar.inc'
PADS_TABLE = ROOT / 'config/guest-code-patches/pads.json'

MUTATIONS = {
    'insert-before-equal': ('if(next<0 || !mapmaker_astar_node_ok(next) || mapmaker_astar_f(mapmaker_astar_node(next)+0xCu)>f) break;',
                            'if(next<0 || !mapmaker_astar_node_ok(next) || mapmaker_astar_f(mapmaker_astar_node(next)+0xCu)>=f) break;'),
    'insert-head-equal': ('mapmaker_astar_f(mapmaker_astar_node(head)+0xCu)>f) {', 'mapmaker_astar_f(mapmaker_astar_node(head)+0xCu)>=f) {'),
    'open-replace-equal': ('if(!(mapmaker_astar_f(mapmaker_astar_node(found)+0xCu)>f)) continue;',
                           'if(mapmaker_astar_f(mapmaker_astar_node(found)+0xCu)<f) continue;'),
    'closed-any-pad': ('if(!(mapmaker_astar_f(fn+0xCu)>f) || mapmaker_astar_w(fn+4u)!=to_pad) continue;',
                       'if(!(mapmaker_astar_f(fn+0xCu)>f)) continue;'),
    'closed-never-reopened': ('                mapmaker_astar_remove(MAPMAKER_ASTAR_CLOSED,found,1);\n            }',
                              '                continue;\n            }'),
    'no-free-list': ('    mapmaker_astar_set_w(mapmaker_astar_node(node),(int32_t)WORD_AT(MAPMAKER_ASTAR_FREE));\n    WORD_AT(MAPMAKER_ASTAR_FREE)=(uint32_t)node;\n', ''),
    'g-float-early': ('        s=(double)c+s;\n        s=s+(double)gpar;', '        s=(double)(float)((double)c+s);\n        s=s+(double)gpar;'),
    'h-no-factor': ('        h=h*(double)mapmaker_astar_f(L+0xCu);\n', ''),
    'cost-one-factor': ('    s=s*(double)mapmaker_astar_f(link+0xCu);\n    s=s*(double)mapmaker_astar_f(link+0xCu);',
                        '    s=s*(double)mapmaker_astar_f(link+0xCu);'),
    'cost-slow-ignored': ('    if(flags&0x40u) r=r*mapmaker_astar_f(MAPMAKER_ASTAR_K_SLOW);\n', ''),
    'cost-blocked-ignored': ('    if(flags&0x20000u) return mapmaker_astar_f(MAPMAKER_ASTAR_K_BLOCKED);\n', ''),
    'doors-ignored': ('            if((flags&2u) && !WORD_AT(MAPMAKER_ASTAR_DOORS)) continue;\n', ''),
    'mask-ignored': ('if(!(flags&0x10000u) || (flags&WORD_AT(MAPMAKER_ASTAR_MASK))) continue;', 'if(!(flags&0x10000u)) continue;'),
    'near-pad-swapped': ('if(mapmaker_astar_group(to_pad)==group) { near_pad=to_pad;to_pad=(int32_t)WORD_AT(L+8u); }',
                         'if(mapmaker_astar_group(to_pad)==group) { near_pad=(int32_t)WORD_AT(L+8u); }'),
    'route-skips-goal': ('        if(guest_writable(route,2u)) mapmaker_astar_set_w(route,goal);\n        count=1;max--;', '        count=0;'),
    'route-ignores-max': ('while(esi>=0 && max>0 && mapmaker_astar_node_ok(esi)) {', 'while(esi>=0 && mapmaker_astar_node_ok(esi)) {'),
    'goal-f-kept': ('            WORD_AT(MAPMAKER_ASTAR_GOAL_F)=WORD_AT(n+0xCu);\n', ''),
    'pop-no-closed': ('        WORD_AT(MAPMAKER_ASTAR_CLOSED)=(uint32_t)node;\n', ''),
    'count-skip-wrong': ('if(!mapmaker_astar.walk_all && (link!=(int16_t)link || !*mapmaker_astar_link_count(link,closed))) {',
                         'if(!mapmaker_astar.walk_all && (link!=(int16_t)link || *mapmaker_astar_link_count(link,closed)<2)) {'),
    'goal-count-wrong': ('if(!mapmaker_astar.walk_all && !*mapmaker_astar_group_count(group))', 'if(!mapmaker_astar.walk_all && *mapmaker_astar_group_count(group)<2)'),
    'no-pool-bound': ('    if(node<0 && WORD_AT(MAPMAKER_ASTAR_COUNTER)>=MAPMAKER_ASTAR_NODES) {', '    if(0) {'),
    'wrapper-no-ret': ('    MAPMAKER_GUEST_RET();                       /* sub_0009B540 ends in a plain ret */', ''),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = ASTAR.read_text(encoding='utf-8')
        assert old in text, mutation
        (tmp_path / ASTAR.name).write_text(text.replace(old, new, 1), encoding='utf-8')
    exe = tmp_path / ('astar.exe' if os.name == 'nt' else 'astar')
    source = ROOT / 'tests/mapmaker/mapmaker_astar.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), str(source), '-o', str(exe), '-lm']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_astar(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=300)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker astar ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_constants_match_tables():
    """The pool, its size and the wrapper hooks are the ones the tables and the build use."""
    text = ASTAR.read_text(encoding='utf-8')
    pool = next(p for p in json.loads(PADS_TABLE.read_text(encoding='utf-8'))['patches'] if p['id'] == 'pads-astar-group-pool')
    assert f'#define MAPMAKER_ASTAR_POOL 0x{int(pool["new"], 16):08X}u' in text
    assert f'#define MAPMAKER_ASTAR_NODES {int(pool["new_size"], 16) // 16}u' in text
    pads = (ROOT / 'src/interface/pc_mapmaker_pads.inc').read_text(encoding='utf-8')
    assert '#define MAPMAKER_PADS_GROUP_NODES 8192u' in pads and '#include "pc_mapmaker_astar.inc"' in pads
    assert 'void pc_mapmaker_pads_group_route(void (*original)(void));' in (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    assert 'void pc_mapmaker_pads_group_route(void (*original)(void)) { original(); }' in (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')


def _guest_vs_c(tmp_path, seed, side, mutation='fixed'):
    xbe_path = private_dir() / 'game/default.xbe'
    if not xbe_path.is_file():
        pytest.skip('needs the private XBE')
    pytest.importorskip('capstone')
    pytest.importorskip('numpy')
    xbe = Xbe(xbe_path.read_bytes())
    exe = build(tmp_path, mutation)
    pre, post = tmp_path / 'pre.bin', tmp_path / 'post.bin'
    result = subprocess.run([str(exe), 'dump', str(seed), str(side), str(pre), str(post)],
                            capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    searches = [tuple(int(v) for v in line.split()) for line in result.stdout.splitlines() if line.strip()]
    mem = bytearray(pre.read_bytes())
    for a in (0x47D408, 0x475D18):
        assert bytes(mem[a:a + 4]) == xbe.read(a, 4), hex(a)
    code = [(0x0009B090, xbe.read(0x0009B090, 0x0009B630 - 0x0009B090))]
    emu = Emu(mem, code, relocations=relocations(PADS_TABLE))
    got = []
    for k, (start, target, route, mx, _) in enumerate(searches):
        struct.pack_into('<I', mem, 0x00715F50, k & 1)
        esp = 0x00300000 - 0x14
        struct.pack_into('<5I', mem, esp, 0xDEAD0000, start, target, route, mx)
        emu.r['esp'] = esp
        emu.st = []
        emu.run(0x0009B540, 0xDEAD0000, 20_000_000)
        got.append(emu.r['eax'])
    want = bytearray(post.read_bytes())
    stack = (0x00300000 - 0x200, 0x00300000)            # both sides' call frames
    mem[stack[0]:stack[1]] = want[stack[0]:stack[1]]
    return got == [s[4] for s in searches] and bytes(mem) == want, sum(s[4] for s in searches)


# fixture seeds 1..3 (seed * 2654435761, 3 + seed % 8 groups a side) and one more
@pytest.mark.parametrize('seed,side', [(2654435761, 4), (1013904226, 5), (3668339987, 6), (77, 7)])
def test_guest_search_matches(tmp_path, seed, side):
    equal, routed = _guest_vs_c(tmp_path, seed, side)
    assert equal, 'guest search and C search differ'
    assert routed > 0


@pytest.mark.parametrize('mutation', ['h-no-factor', 'g-float-early', 'closed-any-pad', 'route-skips-goal'])
def test_guest_search_catch(tmp_path, mutation):
    equal, _ = _guest_vs_c(tmp_path, 1013904226, 5, mutation)
    assert not equal, mutation
