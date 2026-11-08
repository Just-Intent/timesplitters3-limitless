"""MapMaker rooms: the room tree walk of sub_00087310 in C (src/interface/pc_mapmaker_roomquery.inc).

Fixture tests/mapmaker/mapmaker_roomquery.c (hand-checked cases, recorded
series) with mutations; and, with the private XBE, the original guest walk
(sub_00157440, sub_001571C0, the callback sub_00087260 and sub_00047560)
run in tests/mapmaker/guest_emulator.py must leave the same room list for
every query of a series.
"""
import os
from pathlib import Path
import struct
import subprocess

import pytest

from native_compiler import native_compiler
from guest_emulator import Emu, Xbe, private_dir

ROOT = Path(__file__).resolve().parents[2]
WALK = ROOT / 'src/interface/pc_mapmaker_roomquery.inc'

MUTATIONS = {
    'overlap-inclusive-fail': ('if(amin[k]>mapmaker_rooms_f(bmax+k*4u) ||', 'if(amin[k]>=mapmaker_rooms_f(bmax+k*4u) ||'),
    'no-dedupe': ('        if(*flag&0x10u) continue;\n', ''),
    'right-first': ('        if(split>=qmin[axis]) {\n            memcpy(box,nmax,sizeof box);box[axis]=split;\n'
                    '            mapmaker_rooms_walk(tree,qmin,qmax,node*2u,nmin,box,list,depth+1u);\n        }\n',
                    '        if(0) {\n        }\n'),
    'split-from-max': ('split=split+nmin[axis];', 'split=nmax[axis]-split;'),
    'left-strict': ('if(split>=qmin[axis])', 'if(split>qmin[axis])'),
    'no-root-test': ('    if(mapmaker_rooms_overlap(qmin,qmax,tree+8u,tree+0x14u))\n', ''),
    'no-in-box-bit': ('(uint16_t)(room*2u|(uint32_t)mapmaker_rooms_overlap(qmin,qmax,rec+0x10u,rec+0x1Cu))', '(uint16_t)(room*2u)'),
    'nan-no-overlap': ('if(amin[k]>mapmaker_rooms_f(bmax+k*4u) || mapmaker_rooms_f(bmin+k*4u)>amax[k]) return 0;',
                       'if(!(amin[k]<=mapmaker_rooms_f(bmax+k*4u)) || !(mapmaker_rooms_f(bmin+k*4u)<=amax[k])) return 0;'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = WALK.read_text(encoding='utf-8')
        assert old in text, mutation
        (tmp_path / WALK.name).write_text(text.replace(old, new, 1), encoding='utf-8')
    exe = tmp_path / ('roomquery.exe' if os.name == 'nt' else 'roomquery')
    source = ROOT / 'tests/mapmaker/mapmaker_roomquery.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_roomquery(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=120)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker roomquery ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_wired():
    rooms = (ROOT / 'src/interface/pc_mapmaker_rooms.inc').read_text(encoding='utf-8')
    assert '#include "pc_mapmaker_roomquery.inc"' in rooms
    assert 'if(mapmaker_rooms_query_mode()) mapmaker_rooms_tree_query(0x00702E00u,box,box+0xCu,buffer);' in rooms
    assert 'else guest_call(sub_00157440,5,0x00702E00u,box,box+0xCu,0x00087260u,buffer);' in rooms


TREE, BOX, LIST, RECORDS = 0x00100000, 0x00300000, 0x00301000, 0x00200000


def _guest_vs_c(tmp_path, seed, rooms, mutation='fixed'):
    xbe_path = private_dir() / 'game/default.xbe'
    if not xbe_path.is_file():
        pytest.skip('needs the private XBE')
    pytest.importorskip('capstone')
    pytest.importorskip('numpy')
    xbe = Xbe(xbe_path.read_bytes())
    exe = build(tmp_path, mutation)
    pre = tmp_path / 'pre.bin'
    result = subprocess.run([str(exe), 'dump', str(seed), str(rooms), str(pre)], capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    rows = [[int(v) for v in line.split()] for line in result.stdout.splitlines() if line.strip()]
    mem = bytearray(pre.read_bytes())
    assert bytes(mem[0x47DD70:0x47DD74]) == xbe.read(0x47DD70, 4)
    emu = Emu(mem, [(0x00157440, xbe.read(0x00157440, 0x00157488 - 0x00157440)),
                    (0x001571C0, xbe.read(0x001571C0, 0x0015731C - 0x001571C0)),
                    (0x00087260, xbe.read(0x00087260, 0x000872E9 - 0x00087260)),
                    (0x00047560, xbe.read(0x00047560, 0x000475BA - 0x00047560))])
    same = listed = 0
    for row in rows:
        box, n, want = row[:6], row[6], row[7:]
        struct.pack_into('<6I', mem, BOX, *box)
        struct.pack_into('<H', mem, LIST, 0)
        esp = 0x00400000 - 0x18
        struct.pack_into('<6I', mem, esp, 0xDEAD0000, TREE, BOX, BOX + 0xC, 0x00087260, LIST)
        emu.r['esp'] = esp
        emu.run(0x00157440, 0xDEAD0000, 50_000_000)
        got_n = struct.unpack_from('<H', mem, LIST)[0]
        got = list(struct.unpack_from(f'<{got_n}H', mem, LIST + 2)) if got_n else []
        for w in got:                                   # as pc_mapmaker_rooms_query clears them
            mem[RECORDS + (w >> 1) * 0xB4 + 0xB2] &= 0xEF
        same += got_n == n and got == want
        listed += n
    return same, len(rows), listed


@pytest.mark.parametrize('seed,rooms', [(7 * 2654435761 % 2**32, 400), (8 * 2654435761 % 2**32, 1023), (3, 5), (4, 40)])
def test_guest_walk_matches(tmp_path, seed, rooms):
    same, total, listed = _guest_vs_c(tmp_path, seed, rooms)
    assert same == total, f'{total - same} of {total} queries differ'
    assert listed > total


@pytest.mark.parametrize('mutation', ['right-first', 'no-in-box-bit', 'split-from-max'])
def test_guest_walk_catch(tmp_path, mutation):
    same, total, _ = _guest_vs_c(tmp_path, 8 * 2654435761 % 2**32, 1023, mutation)
    assert same < total, mutation
