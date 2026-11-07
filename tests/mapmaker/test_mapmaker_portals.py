"""MapMaker play build: the three portal passes of sub_0008A780 in C (src/interface/pc_mapmaker_portals.inc).

Fixture tests/mapmaker/mapmaker_portals.c (grid vs every pair, hand-checked maps,
hook gating) with mutations; the guest-code table entries that skip the guest's
loops; and, with the private XBE, the original guest loops 0x0008ADDD..0x0008BA61
run in a small x86 emulator on the fixture's pre image must leave the same
guest memory as the C passes.
"""
import json
import os
from pathlib import Path
import struct
import subprocess

import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
PORTALS = ROOT / 'src/interface/pc_mapmaker_portals.inc'
ROOMS_TABLE = ROOT / 'config/guest-code-patches/rooms.json'
ZERO_WORD = 0x008D2200
SKIPPED = ('0x0008AE03', '0x0008B38E', '0x0008B43E')

MUTATIONS = {
    # pass 1
    'join-keeps-equal-i': ('if(por[i].size>por[j].size) { keep=i;drop=j; }', 'if(por[i].size>=por[j].size) { keep=i;drop=j; }'),
    'join-anti-only': ('if(!(anti>dot) && !(dot>para)) return 0;', 'if(!(anti>dot)) return 0;'),
    'join-no-min-test': ('    if(!(near_>=t)) return 0;\n    d0=j->mx[0]', '    d0=j->mx[0]'),
    'join-far-as-near': ('    return far_>=t;', '    return near_>=t;'),
    'join-paired-i': ('        if(mapmaker_portals_i(ptr[i])>=0 && mapmaker_portals_i(ptr[i]+4u)>=0) continue;\n', ''),
    'join-paired-j': ('            if(j0>=0 && mapmaker_portals_i(ptr[j]+4u)>=0) continue;\n', ''),
    'join-i-slot': ('if(eax>=0) WORD_AT(ptr[i]+4u)=rj; else WORD_AT(ptr[i])=rj;', 'WORD_AT(ptr[i])=rj;'),
    'join-j-own-room': ('rj=j0>=0?(uint32_t)j0:WORD_AT(ptr[j]+4u);', 'rj=WORD_AT(ptr[j]+4u);'),
    'join-no-list': ('            mapmaker_portals_replace(rooms,drop,keep);\n', ''),
    'join-flag-kept': ('            flags[drop]=0;\n', '            flags[keep]=0;\n'),
    'join-skips-last-j': ('for(q=i+1u;q<=(uint32_t)count;q++) cand[nc++]=q;', 'for(q=i+1u;q<(uint32_t)count;q++) cand[nc++]=q;'),
    'grid-one-side': ('for(dx=-1;dx<=1;dx++)', 'for(dx=0;dx<=1;dx++)'),
    'grid-unsorted': ('            qsort(cand,nc,sizeof *cand,mapmaker_portals_cmp_u32);\n', ''),
    'measure-sum-order': ('inv=one/(float)cnt;', 'inv=one/(float)(cnt+1);'),
    # pass 2
    'remove-all-places': ('        if(m==n) continue;\n', '        if(m==n) continue;\n        k--;\n'),
    'remove-no-flag': ('if(rooms>0 && mapmaker_portals_remove(rooms,p)) { flags[p]=0;removed++; }',
                       'if(rooms>0 && mapmaker_portals_remove(rooms,p)) { removed++; }'),
    'remove-paired-too': ('        if(mapmaker_portals_i(ptr[p])>=0 && mapmaker_portals_i(ptr[p]+4u)>=0) continue;\n        if(rooms>0',
                          '        if(rooms>0'),
    # pass 3
    'merge-group-23': ('#define MAPMAKER_PORTALS_GROUP 24', '#define MAPMAKER_PORTALS_GROUP 23'),
    'merge-floors-too': ('if(ny>wall || wall_neg>ny || mapmaker_portals_count16(P+0x14u)!=4) continue;\n            a=mapmaker_portals_i(P);b=mapmaker_portals_i(P+4u);\n            if(a<0 || b<0 || !flags[p] || pos[p]<0) continue;',
                         'if(mapmaker_portals_count16(P+0x14u)!=4) continue;\n            a=mapmaker_portals_i(P);b=mapmaker_portals_i(P+4u);\n            if(a<0 || b<0 || !flags[p] || pos[p]<0) continue;'),
    'merge-ignores-flags': ('for(m=0;m<nm && n<MAPMAKER_PORTALS_GROUP;m++) if(flags[members[m]]) list[n++]=members[m];',
                            'for(m=0;m<nm && n<MAPMAKER_PORTALS_GROUP;m++) list[n++]=members[m];'),
    'merge-no-turn': ('    if(turned) { r0=WORD_AT(f);', '    if(0) { r0=WORD_AT(f);'),
    'merge-no-normalise': ('    if(len>len_min) {', '    if(0) {'),
    'merge-quad-order': ('mapmaker_portals_set_f(f+0x28u,mn1);', 'mapmaker_portals_set_f(f+0x28u,mx1);'),
    'merge-keeps-absorbed': ('        if(mapmaker_portals_remove(rooms,list[m])) flags[list[m]]=0;', '        flags[list[m]]=0;'),
    'merge-order-pairs': ('qsort(keys,nk,sizeof *keys,mapmaker_portals_cmp_pair);', ';'),
    # hook
    'hook-any-caller': ('if(WORD_AT(g_esp)==MAPMAKER_PORTALS_RET) mapmaker_portals_link', 'if(1) mapmaker_portals_link'),
    'hook-frame': ('#define MAPMAKER_PORTALS_FRAME 0x1Cu', '#define MAPMAKER_PORTALS_FRAME 0x18u'),
    'hook-no-check': ('        mapmaker_portals.missed++;\n', ''),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = PORTALS.read_text(encoding='utf-8')
        assert old in text, mutation
        (tmp_path / PORTALS.name).write_text(text.replace(old, new, 1), encoding='utf-8')
    exe = tmp_path / ('portals.exe' if os.name == 'nt' else 'portals')
    source = ROOT / 'tests/mapmaker/mapmaker_portals.c'
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
def test_mapmaker_portals(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=300)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker portals ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def table():
    return json.loads(ROOMS_TABLE.read_text(encoding='utf-8'))['patches']


def test_skip_entries():
    """The three loop counts of sub_0008A780 read the zero word; no other operand does."""
    rows = [p for p in table() if p['id'].startswith('rooms-portals-')]
    assert sorted(p['address'] for p in rows) == sorted(SKIPPED)
    for p in rows:
        assert (p['kind'], p['function'], p['original_hex'], p['operand']) == ('operand', '0x0008A780', '8b0d54977b00', 1)
        assert int(p['value'], 16) == ZERO_WORD
    others = [p for p in table() if p.get('value') and int(p['value'], 16) in range(ZERO_WORD, ZERO_WORD + 0x10)]
    assert others == rows
    text = PORTALS.read_text(encoding='utf-8')
    assert f'#define MAPMAKER_PORTALS_ZERO 0x{ZERO_WORD:08X}u' in text
    # the hook and its declaration/stub, and the compile wrapper's check around the build
    assert 'void pc_mapmaker_rooms_portal_link(void (*original)(void));' in (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    assert 'void pc_mapmaker_rooms_portal_link(void (*original)(void)) { original(); }' in (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    rooms = (ROOT / 'src/interface/pc_mapmaker_rooms.inc').read_text(encoding='utf-8')
    assert 'mapmaker_portals_build_begin();\n    original();\n    mapmaker_portals_build_end();' in rooms


# ---------------------------------------------------------------- emulator --

from guest_emulator import Emu, Xbe, private_dir as _private  # noqa: E402


def _overrides():
    """rooms.json operand rewrites inside the range, except the three skips (the original loops run)."""
    import capstone
    out = {}
    for p in table():
        if p['kind'] != 'operand' or p['function'] != '0x0008A780' or p['id'].startswith('rooms-portals-'):
            continue
        a = int(p['address'], 16)
        if 0x0008ADDD <= a < 0x0008BA61:
            out[a] = (p['operand'], int(p['value'], 16))
    return out


def _guest_vs_c(tmp_path, seed, tiles, mutation='fixed'):
    """(equal, emulated steps): the original loops on the pre image against the C passes."""
    xbe_path = _private() / 'game/default.xbe'
    if not xbe_path.is_file():
        pytest.skip('needs the private XBE')
    pytest.importorskip('capstone')
    pytest.importorskip('numpy')
    xbe = Xbe(xbe_path.read_bytes())
    exe = build(tmp_path, mutation)
    pre, post = tmp_path / 'pre.bin', tmp_path / 'post.bin'
    result = subprocess.run([str(exe), 'dump', str(seed), str(tiles), str(pre), str(post)],
                            capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    mem = bytearray(pre.read_bytes())
    # the fixture's float constants are the XBE's
    for a in (0x47D3A4, 0x475DBC, 0x475D5C, 0x478618, 0x475C78, 0x47D3A0, 0x47D39C, 0x476040, 0x47D398, 0x4765F4):
        assert bytes(mem[a:a + 4]) == xbe.read(a, 4), hex(a)
    # the operand values this emulator applies are the ones the C side assumes (flags at esp+0x1280)
    ov = _overrides()
    assert ov[0x0008ADFA] == (1, 0x1280) and ov[0x0008ADF0] == (1, 0x1000)
    for a in (0x0008AE03, 0x0008B38E, 0x0008B43E):
        assert xbe.read(a, 6) == bytes.fromhex('8b0d54977b00')
    code = [(0x0008ADDD, xbe.read(0x0008ADDD, 0x0008BA61 - 0x0008ADDD + 16))]
    emu = Emu(mem, code, ov)
    emu.r['esp'] = 0x00880000
    emu.run(0x0008ADDD, 0x0008BA61, 50_000_000)
    want = bytearray(post.read_bytes())
    # the loops' scratch locals (esp+0x14..0x9C, the group list at +0x220) and the hook's return
    # word below the frame differ; the tail of sub_0008A780 reads only +0x34/+0x38 of the frame
    # (0x0008BAEA, 0x0008BAF9 after add esp,0x28), which neither side writes
    frame = 0x00880000
    assert mem[frame + 0x34:frame + 0x3C] == want[frame + 0x34:frame + 0x3C]
    for lo, hi in ((frame - 0x40, frame + 0x34), (frame + 0x3C, frame + 0x1280)):
        mem[lo:hi] = want[lo:hi]
    return bytes(mem) == want, emu.steps


# golden seeds 1..3 of the fixture (seed * 2654435761, 2 + seed % 9 tiles) and one more
@pytest.mark.parametrize('seed,tiles', [(2654435761, 3), (1013904226, 4), (3668339987, 5), (42, 3)])
def test_guest_loops_match(tmp_path, seed, tiles):
    equal, steps = _guest_vs_c(tmp_path, seed, tiles)
    assert equal, 'guest loops and C passes differ'
    assert steps > 1000


@pytest.mark.parametrize('mutation', ['join-keeps-equal-i', 'join-i-slot', 'remove-no-flag',
                                      'merge-no-turn', 'merge-quad-order', 'join-no-min-test'])
def test_guest_loops_catch(tmp_path, mutation):
    """The comparison is sharp: a changed C pass no longer matches the guest loops."""
    equal, _ = _guest_vs_c(tmp_path, 17, 4, mutation)
    assert not equal, mutation
