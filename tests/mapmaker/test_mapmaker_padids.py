"""MapMaker pads past 8000: the original guest code against the C hooks and the tables.

With the private XBE (skipped otherwise), tests/mapmaker/guest_emulator.py runs
the original instructions:

- the regular-pool search of sub_0009CC40 (its cover test moved out of the way)
  must answer every id 0..0x7FFF exactly as pc_mapmaker_pads.inc's
  mapmaker_pads_search does, on pools of the stock id layout and with high ids;
- the pad decoders sub_0009A420, sub_0009A4C0, sub_0009A510, sub_0009CC40,
  sub_0009CCA0, sub_0009A3C0 and sub_0009A330, stock bytes against the bytes
  with every pads.json code patch applied: on a story-sized pool every handle
  and id gives the same pad (handles relabelled 0x1F40 -> 0x4000, 0x209E ->
  0x7800), so maps and story levels decode as before; on the largest MapMaker
  pools (16,384 regular, 14,336 cover) the Layout C handles decode as the
  model says;
- the group matrix builder sub_00275F70 (with a fake route search, the same
  model as tests/mapmaker/mapmaker_pads.c) must hold exactly the entries the
  lazy reader computes, though the fixture changes a link's flags after the
  build.
"""
import json
import os
from pathlib import Path
import struct
import subprocess

import pytest

from guest_emulator import Emu, Xbe, private_dir
from test_mapmaker_pads import build

ROOT = Path(__file__).resolve().parents[2]
TABLE = ROOT / 'config/guest-code-patches/pads.json'
M = 0xFFFFFFFF
RET = 0xDEAD0000

# function ranges (start, end) of the decoded game code
FUNCS = {
    'valid': (0x0009A330, 0x0009A38C), 'cover_lookup': (0x0009A3C0, 0x0009A412),
    'handle_ptr': (0x0009A420, 0x0009A4BC), 'handle_id': (0x0009A4C0, 0x0009A50D),
    'ptr_handle': (0x0009A510, 0x0009A565), 'id_handle': (0x0009CC40, 0x0009CC9A),
    'id_ptr': (0x0009CCA0, 0x0009CCF5), 'group_first': (0x0009C550, 0x0009C569),
    'matrix': (0x00275F70, 0x002760E7),
}


def _xbe():
    path = private_dir() / 'game/default.xbe'
    if not path.is_file():
        pytest.skip('needs the private XBE')
    pytest.importorskip('capstone')
    pytest.importorskip('numpy')
    return Xbe(path.read_bytes())


def _code(xbe, patched, names):
    """[(start, bytes)] of the named functions, with pads.json's code patches when patched."""
    patches = [p for p in json.loads(TABLE.read_text(encoding='utf-8'))['patches'] if p['kind'] == 'code']
    out = []
    for name in names:
        a, b = FUNCS[name]
        blob = bytearray(xbe.read(a, b - a))
        if patched:
            for p in patches:
                at = int(p['address'], 16)
                if a <= at < b:
                    old, new = bytes.fromhex(p['original_hex']), bytes.fromhex(p['new_hex'])
                    assert bytes(blob[at - a:at - a + len(old)]) == old, p['id']
                    blob[at - a:at - a + len(new)] = new
        out.append((a, bytes(blob)))
    return out


def _call(emu, start, *args):
    esp = 0x00300000 - 4 * (len(args) + 1)
    struct.pack_into(f'<{len(args) + 1}I', emu.mem, esp, RET, *[a & M for a in args])
    emu.r['esp'] = esp
    emu.steps = 0
    emu.run(start, RET, 100_000)
    return emu.r['eax']


# ---- the regular-pool search -------------------------------------------
@pytest.mark.parametrize('count', [0, 1, 2, 3, 7, 2400, 8000, 8001, 9000, 16384])
def test_search_matches_guest(tmp_path, count):
    xbe = _xbe()
    exe = build(tmp_path, 'fixed')
    result = subprocess.run([str(exe), 'search', str(count)], capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stderr
    want = [int(v) for v in result.stdout.split()]
    assert len(want) == 0x8000
    mem = bytearray(0x800000)
    reg = 0x00400000
    struct.pack_into('<II', mem, 0x00715F2C, reg, 0)
    struct.pack_into('<I', mem, 0x007B6664, count)
    for i in range(count):
        struct.pack_into('<I', mem, reg + i * 32, 0x3E9 + i if i < 8000 else 0x5B29 + i - 8000)
    # the fixture's pools() also fills the cover pool; the search does not read it
    emu = Emu(mem, _code(xbe, True, ['id_handle', 'cover_lookup']),
              overrides={0x0009CC45: (1, 0x7FFFFFFF)})   # every id takes the regular search
    ids = sorted(set(range(0, 0x8000, 29)) | {0x3E9, 0x3EA, 0x2328, 0x2329, 0x5B28, 0x5B29, 0x5B2A, 0x7BE8, 0x7BE9,
                                              0x3E9 + count - 1, 0x3E9 + count, 0x5B29 + count - 8001})
    got = {i: _call(emu, 0x0009CC40, i) for i in ids if 0 <= i < 0x8000}
    assert {i: (v if v < 0x80000000 else v - (1 << 32)) for i, v in got.items()} == {i: want[i] for i in got}


# ---- stock against Layout C ----------------------------------------------
def _image(regular, cover, runtime_used, scene):
    mem = bytearray(0x800000)
    reg, cov, run = 0x00400000, 0x00480000, 0x00500000
    struct.pack_into('<III', mem, 0x00715F2C, reg, 0, cov)
    struct.pack_into('<I', mem, 0x00715F38, run)
    struct.pack_into('<I', mem, 0x007B6664, regular)
    struct.pack_into('<I', mem, 0x007B65C0, cover)
    struct.pack_into('<I', mem, 0x0079094C, scene)
    for i in range(regular):
        struct.pack_into('<III', mem, reg + i * 32, 0x3E9 + i if i < 8000 else 0x5B29 + i - 8000, 1, i // 9)
    for j in range(cover):
        struct.pack_into('<III', mem, cov + j * 32, 0x2329 + j, 0x801, 0x3E9 + j % max(regular, 1))
    for k in range(20):
        struct.pack_into('<III', mem, run + k * 32, 0xFFFFFFFE, 0x400001 if k < runtime_used else 0x400000, 0x3E9)
    return mem, reg, cov, run


NAMES = ['valid', 'cover_lookup', 'handle_ptr', 'handle_id', 'ptr_handle', 'id_handle', 'id_ptr']


def _decode_all(emu, handles, ids, pointers):
    out = {}
    for h in handles:
        out[('ptr', h)] = _call(emu, 0x0009A420, h)
        out[('id', h)] = _call(emu, 0x0009A4C0, h)
    for i in ids:
        out[('handle', i)] = _call(emu, 0x0009CC40, i)
        out[('pad', i)] = _call(emu, 0x0009CCA0, i)
    for p in pointers:
        out[('back', p)] = _call(emu, 0x0009A510, p)
        out[('valid', p)] = _call(emu, 0x0009A330, p)
    return out


@pytest.mark.parametrize('scene', [53, 100])
def test_layout_c_decodes_like_stock(scene):
    """A story-sized map (2400 regular, 350 cover, runtime pads in use): every pad
    decodes to the same pad, id and handle under the stock bytes and Layout C."""
    xbe = _xbe()
    regular, cover = 2400, 350
    mem, reg, cov, run = _image(regular, cover, 5, scene)
    stock = Emu(bytearray(mem), _code(xbe, False, NAMES))
    layout = Emu(bytearray(mem), _code(xbe, True, NAMES))

    def relabel(h):
        if 0x1F40 <= h < 0x209E:
            return h - 0x1F40 + 0x4000
        if 0x209E <= h < 0x20B2:
            return h - 0x209E + 0x7800
        return h
    # Runtime handles only outside scene 100: there the stock sub_0009A420 decoded
    # 0x1F40..0x243F as cover first (the overlap Layout B removed), so stock is wrong.
    runtime = [0x209E + k for k in (0, 4, 5, 19)] if scene != 100 else []
    handles = [0, 1, 1234, regular - 1, regular, 0x1F3F] + [0x1F40 + j for j in (0, 1, 200, cover - 1)] + \
              runtime + [0xFFFFFFFF]
    ids = [0, 0x3E8, 0x3E9, 0x3E9 + 700, 0x3E9 + regular - 1, 0x3E9 + regular, 0x2328, 0x2329, 0x2329 + cover - 1,
           0x2329 + cover, 0xFFFFFFFE]
    pointers = [0, reg, reg + 32 * 700, reg + 32 * (regular - 1), cov, cov + 32 * (cover - 1), run, run + 32 * 4,
                run + 32 * 7]
    a = _decode_all(stock, handles, ids, pointers)
    b = _decode_all(layout, [relabel(h) for h in handles], ids, pointers)
    for key, v in a.items():
        kind, x = key
        if kind in ('ptr', 'id'):
            assert b[(kind, relabel(x))] == v, key
        elif kind in ('handle', 'back'):
            assert b[key] == (v if v == M else relabel(v)), key
        else:
            assert b[key] == v, key


def test_layout_c_largest_pools():
    """16,384 regular (ids 0x3E9.. then 0x5B29..), 14,336 cover, 20 runtime pads:
    handles decode as the model says under the patched bytes (no hooks)."""
    xbe = _xbe()
    regular, cover = 16384, 14336
    mem, reg, cov, run = _image(regular, cover, 20, 100)
    emu = Emu(mem, _code(xbe, True, NAMES))
    for i in (0, 7999, 8000, 12345, 16383):
        assert _call(emu, 0x0009A420, i) == reg + 32 * i
        assert _call(emu, 0x0009A4C0, i) == (0x3E9 + i if i < 8000 else 0x5B29 + i - 8000)
        assert _call(emu, 0x0009A510, reg + 32 * i) == i
    for j in (0, 1279, 1280, 10239, 10240, 14335):
        assert _call(emu, 0x0009A420, 0x4000 + j) == cov + 32 * j
        assert _call(emu, 0x0009A4C0, 0x4000 + j) == 0x2329 + j
        assert _call(emu, 0x0009A510, cov + 32 * j) == 0x4000 + j
        assert _call(emu, 0x0009CC40, 0x2329 + j) == 0x4000 + j
        assert _call(emu, 0x0009CCA0, 0x2329 + j) == cov + 32 * j
    for k in (0, 19):
        assert _call(emu, 0x0009A420, 0x7800 + k) == run + 32 * k
        assert _call(emu, 0x0009A4C0, 0x7800 + k) == 0xFFFFFFFE
        assert _call(emu, 0x0009A510, run + 32 * k) == 0x7800 + k
    # low regular ids through the original; a high regular pad is valid (pads-regid-0009a37e)
    assert _call(emu, 0x0009CC40, 0x3E9 + 7999) == 7999
    assert _call(emu, 0x0009A330, reg + 32 * 16383) == 1
    struct.pack_into('<I', mem, reg + 32 * 16383, 0x7BE9)
    assert _call(emu, 0x0009A330, reg + 32 * 16383) == 0


# ---- the group distance matrix ---------------------------------------------
def _fake_route(emu):
    np = emu.np
    esp = emu.r['esp']
    route = emu.r32(esp + 8)
    a, b = emu.r32(route), emu.r32(route + 0xC)
    f = emu.r32(emu.r32(0x00715F30))
    if (a + b) % 7 == 3:
        emu.r['eax'] = 0xFFFFFFFE
        return
    emu.wf(0x00715F44, np.float32((a * 7 + b * 3) % 97) * np.float32(0.25))
    emu.wf(0x00715F48, np.float32((f >> 16) & 0xFF) + np.float32(0.125) * np.float32(b % 5))
    emu.w32(0x00715EDC, b)
    emu.w32(0x0071204C, 0x1234)
    emu.r['eax'] = 1


def _matrix(tmp_path, groups, mutation='fixed'):
    """(entries of the original builder, entries the C reader computed)."""
    xbe = _xbe()
    exe = build(tmp_path, mutation)
    image = tmp_path / 'image.bin'
    result = subprocess.run([str(exe), 'matrix', str(groups), str(image)], capture_output=True, text=True, timeout=120)
    assert result.returncode == 0 or mutation != 'fixed', result.stdout + result.stderr
    lazy = [int(v, 16) for v in result.stdout.split()]
    mem = bytearray(image.read_bytes())
    heap = [0x006E0000]

    def alloc(emu):
        size = emu.r32(emu.r['esp'] + 4)
        emu.r['eax'] = heap[0]
        heap[0] += (size + 15) & ~15
    emu = Emu(mem, _code(xbe, False, ['matrix', 'group_first']))
    esp = 0x00300000 - 4
    struct.pack_into('<I', mem, esp, RET)
    emu.r['esp'] = esp
    emu.run(0x00275F70, RET, 5_000_000, stubs={0x0003DFA0: alloc, 0x001596F0: _fake_route})
    matrix = emu.r32(0x0078B2D8)
    built = [emu.r32(emu.r32(matrix + 4 * i) + 4 * j) for i in range(groups) for j in range(groups)]
    return built, lazy


@pytest.mark.parametrize('groups', [9, 40])
def test_matrix_builder_matches_lazy(tmp_path, groups):
    built, lazy = _matrix(tmp_path, groups)
    assert len(lazy) == groups * groups
    assert built == lazy
    assert any(v == struct.unpack('<I', struct.pack('<f', 1.0e9))[0] for v in built)   # some pairs have no route


@pytest.mark.parametrize('mutation', ['matrix-swapped', 'matrix-one-cost', 'matrix-live-flags'])
def test_matrix_catch(tmp_path, mutation):
    built, lazy = _matrix(tmp_path, 9, mutation)
    assert built != lazy, mutation


def test_search_catch(tmp_path):
    """The emulator comparison catches a search that steps differently."""
    xbe = _xbe()
    exe = build(tmp_path, 'search-off-by-one')
    result = subprocess.run([str(exe), 'search', '9000'], capture_output=True, text=True, timeout=120)
    want = [int(v) for v in result.stdout.split()]
    mem = bytearray(0x800000)
    struct.pack_into('<II', mem, 0x00715F2C, 0x00400000, 0)
    struct.pack_into('<I', mem, 0x007B6664, 9000)
    for i in range(9000):
        struct.pack_into('<I', mem, 0x00400000 + i * 32, 0x3E9 + i if i < 8000 else 0x5B29 + i - 8000)
    emu = Emu(mem, _code(xbe, True, ['id_handle', 'cover_lookup']), overrides={0x0009CC45: (1, 0x7FFFFFFF)})
    ids = list(range(0x3E9, 0x3E9 + 300)) + list(range(0x5B29, 0x5B29 + 300)) + [0x2400, 0x7000]
    got = [_call(emu, 0x0009CC40, i) for i in ids]
    assert [v if v < 0x80000000 else v - (1 << 32) for v in got] != [want[i] for i in ids]
