"""AI navigation: sub_0009BCF0 (the nearest enabled pad) in C (src/interface/pc_mapmaker_nearpad.inc).

Fixture tests/mapmaker/mapmaker_nearpad.c (hand-checked cases, recorded
series) with mutations; and, with the private XBE, the original guest code
0x0009BCF0..0x0009BEE9 run in tests/mapmaker/guest_emulator.py must give the
same pad for every query of a series.
"""
import os
from pathlib import Path
import struct
import subprocess

import pytest

from native_compiler import native_compiler
from guest_emulator import Emu, Xbe, private_dir

ROOT = Path(__file__).resolve().parents[2]
NEAR = ROOT / 'src/interface/pc_mapmaker_nearpad.inc'

MUTATIONS = {
    'ties-last': ('        if(best>d && !(d<low', '        if(best>=d && !(d<low'),
    'order-one-way': ('        if(i<unrolled && !(i&3)) {', '        if(0) {'),
    'no-flag': ('        if(!(*(uint8_t *)((uintptr_t)p+4u+g_xbox_mem_offset)&1u)) continue;\n', ''),
    'no-min-pad': ('low=min*min;low=low+mapmaker_near_f(MAPMAKER_NEAR_K_PAD);', 'low=min*min;'),
    'min-ignored': ('if(zero<min || min!=min) {', 'if(0) {'),
    'min-inclusive-skip': ('!(d<low ||', '!(d<=low ||'),
    'no-ret': ('    MAPMAKER_GUEST_RET();                         /* sub_0009BCF0 ends in a plain ret */', ''),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = NEAR.read_text(encoding='utf-8')
        assert old in text, mutation
        (tmp_path / NEAR.name).write_text(text.replace(old, new, 1), encoding='utf-8')
    exe = tmp_path / ('nearpad.exe' if os.name == 'nt' else 'nearpad')
    source = ROOT / 'tests/mapmaker/mapmaker_nearpad.c'
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
def test_mapmaker_nearpad(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=120)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker nearpad ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_declared():
    pads = (ROOT / 'src/interface/pc_mapmaker_pads.inc').read_text(encoding='utf-8')
    assert '#include "pc_mapmaker_nearpad.inc"' in pads
    assert 'void pc_mapmaker_pads_nearest(void (*original)(void));' in (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    assert 'void pc_mapmaker_pads_nearest(void (*original)(void)) { original(); }' in (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')


def _guest_vs_c(tmp_path, seed, pads, mutation='fixed'):
    xbe_path = private_dir() / 'game/default.xbe'
    if not xbe_path.is_file():
        pytest.skip('needs the private XBE')
    pytest.importorskip('capstone')
    pytest.importorskip('numpy')
    xbe = Xbe(xbe_path.read_bytes())
    exe = build(tmp_path, mutation)
    pre = tmp_path / 'pre.bin'
    result = subprocess.run([str(exe), 'dump', str(seed), str(pads), str(pre)], capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    rows = [[int(v) for v in line.split()] for line in result.stdout.splitlines() if line.strip()]
    mem = bytearray(pre.read_bytes())
    for a in (0x475D5C, 0x475D1C):
        assert bytes(mem[a:a + 4]) == xbe.read(a, 4), hex(a)
    emu = Emu(mem, [(0x0009BCF0, xbe.read(0x0009BCF0, 0x0009BEEA - 0x0009BCF0))])
    same = found = 0
    for x, y, z, mn, want in rows:
        struct.pack_into('<3I', mem, 0x00300000, x, y, z)
        esp = 0x00400000 - 0xC
        struct.pack_into('<3I', mem, esp, 0xDEAD0000, 0x00300000, mn)
        emu.r['esp'] = esp
        emu.run(0x0009BCF0, 0xDEAD0000, 100_000_000)
        got = emu.r['eax'] - (1 << 32) if emu.r['eax'] & 0x80000000 else emu.r['eax']
        same += got == want
        found += want >= 0
    return same, len(rows), found


# fixture seeds 7 and 8 (250 and 1003 pads), and small pools around the groups of four
@pytest.mark.parametrize('seed,pads', [(7 * 2654435761 % 2**32, 250), (8 * 2654435761 % 2**32, 1003), (3, 5), (4, 7)])
def test_guest_nearest_matches(tmp_path, seed, pads):
    same, total, found = _guest_vs_c(tmp_path, seed, pads)
    assert same == total, f'{total - same} of {total} queries differ'
    assert found > total // 2


@pytest.mark.parametrize('mutation', ['ties-last', 'order-one-way', 'min-ignored'])
def test_guest_nearest_catch(tmp_path, mutation):
    same, total, _ = _guest_vs_c(tmp_path, 8 * 2654435761 % 2**32, 1003, mutation)
    assert same < total, mutation
