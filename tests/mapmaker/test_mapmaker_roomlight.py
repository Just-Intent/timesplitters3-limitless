"""MapMaker play: sub_00115F50 (a room light's colour spread to the room's objects) in C.

Fixture tests/mapmaker/mapmaker_roomlight.c (hand-built kinds, recorded series)
with mutations; and, with the private XBE, the original guest code
0x00115F50..0x0011612B run in tests/mapmaker/guest_emulator.py (rooms.json's
held-id operands applied, the three callees replaced by the fixture's fakes)
must make the same guest calls in the same order and leave the same memory.
"""
import json
import os
from pathlib import Path
import struct
import subprocess

import pytest

from native_compiler import native_compiler
from guest_emulator import Emu, Xbe, private_dir

ROOT = Path(__file__).resolve().parents[2]
LIGHT = ROOT / 'src/interface/pc_mapmaker_roomlight.inc'
ROOMS_TABLE = ROOT / 'config/guest-code-patches/rooms.json'

MUTATIONS = {
    'type2-coloured': ('if(at==room && type!=2) {', 'if(at==room) {'),
    'no-model-check': ('    if(MAPMAKER_PTR_OK(model,12u) && !WORD_AT(model+8u)) guest_call', '    if(MAPMAKER_PTR_OK(model,12u)) guest_call'),
    'held-ids-stock': ('#define MAPMAKER_ROOMLIGHT_HELD_FIRST 0x401u', '#define MAPMAKER_ROOMLIGHT_HELD_FIRST 0xCAu'),
    'held-any-room': ('if(!body || !MAPMAKER_PTR_OK(body,0x38u) || WORD_AT(body+0x34u)!=room) continue;',
                      'if(!body || !MAPMAKER_PTR_OK(body,0x38u)) continue;'),
    'held-two': ('static const uint32_t held[3]={0x1D8u,0x5B0u,0xE84u};', 'static const uint32_t held[3]={0x1D8u,0x5B0u,0x5B0u};'),
    'flag-any-room': ('        if(WORD_AT(MAPMAKER_ROOMLIGHT_SELF_ONLY) && at!=room) continue;\n', ''),
    'blue-from-arg': ('uint32_t room,red,green,blue=g_ebx,i;', 'uint32_t room,red,green,blue=0,i;'),
    'one-arg': ('#define MAPMAKER_ROOMLIGHT_ONE 0x3F800000u', '#define MAPMAKER_ROOMLIGHT_ONE 0x3F000000u'),
    'count-once': ('for(i=0;(int32_t)i<(int32_t)WORD_AT(MAPMAKER_ROOMLIGHT_COUNT);i++) {',
                   'for(i=0;(int32_t)i<(int32_t)WORD_AT(MAPMAKER_ROOMLIGHT_COUNT)-1;i++) {'),
    'no-ret': ('    MAPMAKER_GUEST_RET();                        /* sub_00115F50 ends in a plain ret */', ''),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = LIGHT.read_text(encoding='utf-8')
        assert old in text, mutation
        (tmp_path / LIGHT.name).write_text(text.replace(old, new, 1), encoding='utf-8')
    exe = tmp_path / ('roomlight.exe' if os.name == 'nt' else 'roomlight')
    source = ROOT / 'tests/mapmaker/mapmaker_roomlight.c'
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
def test_mapmaker_roomlight(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=120)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker roomlight ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def table():
    return json.loads(ROOMS_TABLE.read_text(encoding='utf-8'))['patches']


def _overrides():
    out = {}
    for p in table():
        if p['kind'] == 'operand' and p['function'] == '0x00115F50':
            out[int(p['address'], 16)] = (p['operand'], int(p['value'], 16))
    return out


def test_held_ids_match_table():
    """The held room ids in C are the ones rooms.json gives the original."""
    ov = _overrides()
    text = LIGHT.read_text(encoding='utf-8')
    assert ov[0x00115FB3] == (1, 0x401) and ov[0x00115FBE] == (1, 0x405) and ov[0x00115FC9] == (1, 0xFFFFFBFF)
    assert '#define MAPMAKER_ROOMLIGHT_HELD_FIRST 0x401u' in text and '#define MAPMAKER_ROOMLIGHT_HELD_END 0x405u' in text
    rooms = (ROOT / 'src/interface/pc_mapmaker_rooms.inc').read_text(encoding='utf-8')
    assert '#include "pc_mapmaker_roomlight.inc"' in rooms
    assert 'void pc_mapmaker_rooms_light_spread(void (*original)(void));' in (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    assert 'void pc_mapmaker_rooms_light_spread(void (*original)(void)) { original(); }' in (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')


def _fakes(log):
    """The fixture's three callee fakes, on the emulator's stack and memory."""
    def args(emu, n):
        return [struct.unpack_from('<I', emu.mem, emu.r['esp'] + 4 + 4 * k)[0] for k in range(n)]

    def colour(emu):
        a = args(emu, 5)
        m = struct.unpack_from('<I', emu.mem, a[0] + 4)[0]
        if m:
            v = struct.unpack_from('<I', emu.mem, m + 8)[0]
            struct.pack_into('<I', emu.mem, m + 8, (v + 1) & 1)
        struct.pack_into('<I', emu.mem, a[0] + 0x10, a[2] ^ a[3] ^ a[4] ^ a[1])
        log.append((1, *a))
        emu.r['eax'] = 0

    def skip(emu):
        a = args(emu, 2)
        v = struct.unpack_from('<I', emu.mem, a[0] + 0x14)[0]
        struct.pack_into('<I', emu.mem, a[0] + 0x14, (v + a[1]) & 0xFFFFFFFF)
        log.append((2, a[0], a[1], 0, 0, 0))
        emu.r['eax'] = 0

    def player(emu):
        a = args(emu, 1)
        log.append((3, a[0], 0, 0, 0, 0))
        emu.r['eax'] = (a[0] * 3 + 1) & 3

    return {0x00094250: colour, 0x00094290: skip, 0x00356800: player}


def _guest_vs_c(tmp_path, seed, objects, mutation='fixed'):
    xbe_path = private_dir() / 'game/default.xbe'
    if not xbe_path.is_file():
        pytest.skip('needs the private XBE')
    pytest.importorskip('capstone')
    pytest.importorskip('numpy')
    xbe = Xbe(xbe_path.read_bytes())
    exe = build(tmp_path, mutation)
    pre, post = tmp_path / 'pre.bin', tmp_path / 'post.bin'
    result = subprocess.run([str(exe), 'dump', str(seed), str(objects), str(pre), str(post)],
                            capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    want_calls = [tuple(int(v) for v in line.split()) for line in result.stdout.splitlines() if line.strip()]
    mem = bytearray(pre.read_bytes())
    code = [(0x00115F50, xbe.read(0x00115F50, 0x00116130 - 0x00115F50))]
    emu = Emu(mem, code, _overrides())
    log = []
    stubs = _fakes(log)
    for k in range(16):                                   # the fixture's series()
        struct.pack_into('<I', mem, 0x004D1984, 1 if k < 8 else 0)
        room = 1 + k % 8 if k % 8 < 6 else 0x401 + k % 8
        esp = 0x00500000 - 0x10
        struct.pack_into('<4I', mem, esp, 0xDEAD0000, room, 0x10 + k, 0x20 + k)
        emu.r['esp'] = esp
        emu.r['ebx'] = 0x30 + k
        emu.run(0x00115F50, 0xDEAD0000, 5_000_000, stubs)
    want = bytearray(post.read_bytes())
    lo, hi = 0x00500000 - 0x100, 0x00500000                  # both sides' call frames
    mem[lo:hi] = want[lo:hi]
    return log == want_calls and bytes(mem) == want, len(log)


@pytest.mark.parametrize('seed,objects', [(2654435761, 77), (1013904226, 114), (3668339987, 151), (5, 300)])
def test_guest_spread_matches(tmp_path, seed, objects):
    equal, n = _guest_vs_c(tmp_path, seed, objects)
    assert equal, 'guest walk and C walk differ'
    assert n > 10


@pytest.mark.parametrize('mutation', ['held-ids-stock', 'blue-from-arg', 'type2-coloured', 'held-any-room'])
def test_guest_spread_catch(tmp_path, mutation):
    equal, _ = _guest_vs_c(tmp_path, 5, 300, mutation)
    assert not equal, mutation
