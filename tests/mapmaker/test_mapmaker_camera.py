"""MapMaker security cameras taken over through a switch
(src/interface/pc_mapmaker_camera.inc): the stock sweep stops while a player
holds the camera and resumes afterwards; view diagnostics name the floors a
view reaches. Fixture with mutations, plus decoder evidence for the cause
(skipped without the private XBE)."""
import hashlib
import os
from pathlib import Path
import subprocess

import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_camera.inc'
OPENVIEW = ROOT / 'src/interface/pc_mapmaker_openview.inc'
XBE = ROOT / 'private/game/default.xbe'

C = '[mapmaker] camera: '
GUN = 'actor 0x00100000 type 7 state 2 flags 0x{flags} operator 0x{op}: sweep {what}'
EXPECTED_PREFIXES = [
    C + GUN.format(flags='00400004', op='00102000', what='suspended while controlled'),
    C + GUN.format(flags='00400004', op='00000000', what='running'),
    'phase sweep',
    C + GUN.format(flags='00400005', op='00102000', what='stock (already held by the stock flag)'),
    'phase stock',
    C + GUN.format(flags='00400004', op='00000000', what='running'),
    C + GUN.format(flags='00400004', op='00102000', what='suspended while controlled'),
    'phase links',
    C + 'view 0 (player on a mounted gun/camera): camera room 1, 3 of 4 rooms visible, floors -2..9 (top room 3), deepest walk 2, far 1800, fov 55.0',
    C + 'view 0 (player on a mounted gun/camera): camera room 1, 4 of 4 rooms visible, floors -2..13 (top room 4), deepest walk 2, far 1800, fov 55.0',
    'phase view',
    'mapmaker camera ok',
]

MUTATIONS = {
    'any-scene': "if(WORD_AT(MAPMAKER_CAMERA_SCENE)==MAPMAKER_CAMERA_PLAY && MAPMAKER_PTR_OK(entity,0x80u) &&",
    'no-freeze': "            WORD_AT(data+8u)|=MAPMAKER_CAMERA_FROZEN;\n",
    'no-restore': "    if(set) WORD_AT(data+8u)&=~MAPMAKER_CAMERA_FROZEN;\n",
    'clears-stock-bit': "    if(set) WORD_AT(data+8u)&=~MAPMAKER_CAMERA_FROZEN;\n",
    'no-reciprocal': "WORD_AT(extra+0xC14u)!=entity) return 0;",
    'any-mode': "return WORD_AT(player+0x114u)==0x80u ? op : 0;",
    'no-backlink': "WORD_AT(data)==entity) {",
}
REPLACE = {
    'any-scene': "if(MAPMAKER_PTR_OK(entity,0x80u) &&",
    'no-freeze': "",
    'no-restore': "",
    'clears-stock-bit': "    WORD_AT(data+8u)&=~MAPMAKER_CAMERA_FROZEN;\n",
    'no-reciprocal': "0) return 0;",
    'any-mode': "return op;",
    'no-backlink': "1) {",
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    text = INC.read_text(encoding='utf-8')
    if mutation != 'fixed':
        old = MUTATIONS[mutation]
        assert text.count(old) == 1, mutation
        text = text.replace(old, REPLACE[mutation])
    (tmp_path / INC.name).write_text(text, encoding='utf-8')
    exe = tmp_path / ('camera.exe' if os.name == 'nt' else 'camera')
    source = ROOT / 'tests/mapmaker/mapmaker_camera.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/wd4996', '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'),
                str(source), '/Fe:' + str(exe), '/Fo:' + str(tmp_path / 'camera.obj')]
    else:
        args = [compiler, '-std=c11', '-O2', '-D_POSIX_C_SOURCE=200809L', '-I' + str(tmp_path),
                '-I' + str(ROOT / 'src/interface'), str(source), '-o', str(exe), '-lm']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_camera(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    result = subprocess.run([str(exe)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=30)
    lines = [l for l in result.stdout.splitlines() if l.startswith(C) or l.startswith('phase') or l.endswith(' ok')]
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout
        assert len(lines) == len(EXPECTED_PREFIXES), '\n'.join(lines)
        for line, want in zip(lines, EXPECTED_PREFIXES):
            assert line.startswith(want), (line, want)
    else:
        assert result.returncode != 0 or len(lines) != len(EXPECTED_PREFIXES) or \
            any(not l.startswith(w) for l, w in zip(lines, EXPECTED_PREFIXES)), f'{mutation} was not detected'


def test_camera_wiring():
    """The hook, its stub and prototype exist; the portal walk calls the view diagnostic."""
    text = INC.read_text(encoding='utf-8')
    assert 'void pc_mapmaker_camera_gun_update(void (*original)(void))' in text
    assert 'WRAP sub_00246000' in text
    guest = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    assert guest.index('#include "pc_mapmaker_camera.inc"') < guest.index('#include "pc_mapmaker_openview.inc"')
    assert 'void pc_mapmaker_camera_gun_update(void (*original)(void)) { original(); }' in guest
    header = (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    assert 'void pc_mapmaker_camera_gun_update(void (*original)(void));' in header
    openview = OPENVIEW.read_text(encoding='utf-8')
    walk = openview.split('void pc_mapmaker_openview_walk(void (*original)(void))', 1)[1]
    assert walk.index('original();') < walk.index('mapmaker_camera_view_diag();') < walk.index('if(!mapmaker_roof_play.open_rooms')


def _text(image, va, n):
    """Bytes at a virtual address of the XBE image (section table)."""
    import struct
    base = struct.unpack_from('<I', image, 0x104)[0]
    count = struct.unpack_from('<I', image, 0x11C)[0]
    headers = struct.unpack_from('<I', image, 0x120)[0] - base
    for i in range(count):
        _, vaddr, vsize, raw, rsize = struct.unpack_from('<IIIII', image, headers + i * 56)
        if vaddr <= va < vaddr + vsize:
            return image[raw + va - vaddr: raw + va - vaddr + n]
    raise AssertionError(hex(va))


def test_original_sweep_and_switch_flag():
    """US 45410066: sub_00246000 skips its state machine on data+8 bit 0 and
    sweeps state 2 without checking +0xD8; the switch mount sets bit 0 only
    when flag 0x20 is set."""
    if not XBE.is_file():
        pytest.skip('Local supported US XBE required; never included in CI')
    image = XBE.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    # 0x002460E2 mov ecx,[esi+8]; test cl,1; jne (skip)
    assert _text(image, 0x002460E2, 6) == bytes.fromhex('8b4e08f6c101')
    # state 2 sweep: yaw += [esi+0x5C] * [0x4E794C] (0x002461C0..0x002461D2)
    assert _text(image, 0x002461C0, 5) == bytes.fromhex('f30f10465c')
    # 0x003391E6 mov ecx,[eax+8]; test cl,0x20; je; and ecx,~0x20; or ecx,1
    assert _text(image, 0x003391E6, 14) == bytes.fromhex('8b4808f6c1207409' '83e1df83c901')
