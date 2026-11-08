"""AI navigation: sub_0006A8B0 (a random pad in a ring) in C (src/interface/pc_mapmaker_randpad.inc).

Fixture tests/mapmaker/mapmaker_randpad.c (hand-checked cases, recorded
series of results, generator states and argument-slot draws) with
mutations; and, with the private XBE, the original guest code
0x0006A8B0..0x0006A9EB with the generator 0x00153B40 run in
tests/mapmaker/guest_emulator.py must leave the same pad, draw and generator
state for every query of a series.
"""
import os
from pathlib import Path
import struct
import subprocess

import pytest

from native_compiler import native_compiler
from guest_emulator import Emu, Xbe, private_dir

ROOT = Path(__file__).resolve().parents[2]
RAND = ROOT / 'src/interface/pc_mapmaker_randpad.inc'

MUTATIONS = {
    'nan-rejected': ('        if(d>outer) ok=0;', '        if(!(d<=outer)) ok=0;'),
    'inner-inclusive': ('        else if(inner>d) ok=0;', '        else if(inner>=d) ok=0;'),
    'no-scene': ('        if(scene==100u &&', '        if(0 &&'),
    'no-flag': ('        if(*(uint8_t *)((uintptr_t)p+4u+g_xbox_mem_offset)&1u) break;', '        break;'),
    'index-modulo': ('idx=(uint32_t)(((unsigned long long)r*(unsigned long long)(uint32_t)count)>>32);', 'idx=r%(uint32_t)count;'),
    'no-slot': ('        WORD_AT(slot)=r;\n', ''),
    'no-counter': ('    WORD_AT(MAPMAKER_RANDPAD_RNG_CALLS)+=1u;\n', ''),
    'tries-all': ('t=(float)count;t=t*half;tries=(int32_t)t;', 'tries=count;'),
    'no-ret': ('    MAPMAKER_GUEST_RET();                         /* sub_0006A8B0 ends in a plain ret */', ''),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = RAND.read_text(encoding='utf-8')
        assert old in text, mutation
        (tmp_path / RAND.name).write_text(text.replace(old, new, 1), encoding='utf-8')
    exe = tmp_path / ('randpad.exe' if os.name == 'nt' else 'randpad')
    source = ROOT / 'tests/mapmaker/mapmaker_randpad.c'
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
def test_mapmaker_randpad(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=120)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker randpad ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_declared():
    pads = (ROOT / 'src/interface/pc_mapmaker_pads.inc').read_text(encoding='utf-8')
    assert '#include "pc_mapmaker_randpad.inc"' in pads
    assert 'void pc_mapmaker_pads_random(void (*original)(void));' in (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    assert 'void pc_mapmaker_pads_random(void (*original)(void)) { original(); }' in (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')


def _allmul(emu):
    """__allmul (0x003C9ED0): edx:eax = a * b (64-bit), stdcall: pops its 16 argument bytes."""
    esp = emu.r['esp']
    a = emu.r32(esp + 4) | emu.r32(esp + 8) << 32
    b = emu.r32(esp + 12) | emu.r32(esp + 16) << 32
    r = (a * b) & (2**64 - 1)
    emu.r['eax'], emu.r['edx'] = r & 0xFFFFFFFF, r >> 32
    ret = emu.r32(esp)
    emu.r['esp'] = esp + 16
    emu.w32(emu.r['esp'], ret)


def _ftol(emu):
    """__ftol (0x003C848C): pops st(0), truncates toward zero into edx:eax."""
    v = int(emu.st.pop())
    v &= 2**64 - 1
    emu.r['eax'], emu.r['edx'] = v & 0xFFFFFFFF, v >> 32


def _guest_vs_c(tmp_path, seed, pads, mutation='fixed'):
    xbe_path = private_dir() / 'game/default.xbe'
    if not xbe_path.is_file():
        pytest.skip('needs the private XBE')
    pytest.importorskip('capstone')
    pytest.importorskip('numpy')
    xbe = Xbe(xbe_path.read_bytes())
    assert xbe.read(0x475CCC, 4) == struct.pack('<f', 2.0**32)
    assert xbe.read(0x4785D0, 4) == struct.pack('<f', 2.0**-32)
    exe = build(tmp_path, mutation)
    pre = tmp_path / 'pre.bin'
    result = subprocess.run([str(exe), 'dump', str(seed), str(pads), str(pre)], capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    rows = [[int(v) for v in line.split()] for line in result.stdout.splitlines() if line.strip()]
    mem = bytearray(pre.read_bytes())
    assert bytes(mem[0x475CD4:0x475CD8]) == xbe.read(0x475CD4, 4)
    for a in (0x475CCC, 0x4785D0):                      # the x87 index constants (not read by the C)
        mem[a:a + 4] = xbe.read(a, 4)
    emu = Emu(mem, [(0x0006A8B0, xbe.read(0x0006A8B0, 0x0006A9EC - 0x0006A8B0)),
                    (0x00153B40, xbe.read(0x00153B40, 0x00153B7A - 0x00153B40))])
    stubs = {0x003C9ED0: _allmul, 0x003C848C: _ftol}
    same = found = 0
    for x, y, z, ring, want, slot, lo, hi, calls in rows:
        struct.pack_into('<3I', mem, 0x00300000, x, y, z)
        esp = 0x00400000 - 0xC
        struct.pack_into('<3I', mem, esp, 0xDEAD0000, ring, 0x00300000)
        emu.r['esp'] = esp
        emu.run(0x0006A8B0, 0xDEAD0000, 200_000_000, stubs)
        got = (emu.r['eax'], emu.r32(esp + 4), emu.r32(0x4E7950), emu.r32(0x4E7954), emu.r32(0x7497C8))
        same += got == (want, slot, lo, hi, calls)
        found += want != 0
    return same, len(rows), found


# fixture seeds 7 and 8 (250 and 1003 pads; scene 100 and another scene), and small pools
@pytest.mark.parametrize('seed,pads', [(7 * 2654435761 % 2**32, 250), (8 * 2654435761 % 2**32, 1003), (3, 3), (4, 41)])
def test_guest_random_matches(tmp_path, seed, pads):
    same, total, found = _guest_vs_c(tmp_path, seed, pads)
    assert same == total, f'{total - same} of {total} queries differ'
    assert found < total and (pads < 100 or found > 0)


@pytest.mark.parametrize('mutation', ['nan-rejected', 'no-scene', 'index-modulo'])
def test_guest_random_catch(tmp_path, mutation):
    same, total, _ = _guest_vs_c(tmp_path, 7 * 2654435761 % 2**32, 250, mutation)
    assert same < total, mutation
