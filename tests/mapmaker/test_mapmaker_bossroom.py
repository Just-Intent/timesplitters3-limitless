"""Giant bosses drawn through the camera's room (src/interface/pc_mapmaker_bossroom.inc):
fixture with mutations, wiring, the free extension block it uses, and decoder
evidence for the room prop lists (relink sub_0009E4B0, append sub_0009E430 and
its pool bound, the per-room readers)."""
import os
from pathlib import Path
import re
import subprocess

import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_bossroom.inc'

M = '[mapmaker] bossroom: '
LINE = M + 'boss prop 0x403FF0 (model 0x5E9) drawn through camera room 7: linked to {linked} of its 1 reached listed rooms (2 listed), bucket pool {pool} of 10653 used'
EXPECTED = '\n'.join([
    'phase off',
    LINE.format(linked=0, pool=100),
    'phase add',
    LINE.format(linked=1, pool=100),
    'phase relink',
    'phase skips',
    LINE.format(linked=0, pool=0x2990),
    'phase level',
    'phase bad',
    LINE.format(linked=0, pool=100),
    M + 'boss prop 0x405160 (model 0x24A) drawn through camera room 7: linked to 0 of its 0 reached listed rooms (0 listed), bucket pool 100 of 10653 used',
    M + 'boss prop 0x405448 (model 0x6CB) drawn through camera room 7: linked to 0 of its 1 reached listed rooms (1 listed), bucket pool 100 of 10653 used',
    'phase satellites',
    'mapmaker bossroom ok',
]) + '\n'

MUTATIONS = {
    'no-switch': ('    return mapmaker_limits.limits;', '    return 1;'),
    'any-scene': ('    if(bsr.off || WORD_AT(BSR_SCENE)!=BSR_PLAY) return 0;', '    if(bsr.off) return 0;'),
    'no-opt-out': ('bsr.off=v && v[0]==\'0\' && !v[1];', 'bsr.off=0;(void)v;'),
    'any-model': ('if(!bsr_boss_model(model) || ', 'if('),
    'skeleton-only': ('return m==0x5E9u || m==0x5BEu ||', 'return m==0x5E9u || 0 ||'),
    'any-type': (' || WORD_AT(p+0x20u)!=8u) continue;', ') continue;'),
    'rooms-too': ('for(pass=0;pass<2u;pass++) for(i=n;i<end;i++) {', 'for(pass=0;pass<2u;pass++) for(i=0;i<end;i++) {'),
    'unreached-too': ('        if(!visible && !(pass && bosses && !listed)) continue;\n', ''),
    'no-satellites': ('    for(pass=0;pass<2u;pass++) for(i=n;i<end;i++) {', '    for(pass=0;pass<1u;pass++) for(i=n;i<end;i++) {'),
    'satellites-alone': [('        if(!visible && !(pass && bosses && !listed)) continue;', '        if(!visible && !(pass && !listed)) continue;'),
                         ('(listed==0 && (!pass || !bosses))', '(listed==0 && !pass)')],
    'satellite-any-model': ('        if(pass) { if(!bsr_satellite(model)) continue; }', '        if(pass) { (void)bsr_satellite; }'),
    'global-camera': ('        if(cam<1u || cam>n || cam==BSR_GLOBAL) { bsr.no_room++;continue; }\n', ''),
    'duplicates': ('        if(bsr_in_list(cam,p)) continue;\n', ''),
    'in-place': ('            WORD_AT(BSR_LISTS+cam*4u)=buf;list=buf;own=1;', '            buf=list;own=1;'),
    'no-copy': ('            if(count) memmove(', '            if(0) memmove('),
    'no-cap-check': (' || WORD_AT(BSR_CAPS+cam*4u)>BSR_SLOT_ENTRIES', ''),
    'one-buffer': ('    buf=BSR_BUFFER+slot*BSR_SLOT_BYTES;', '    buf=BSR_BUFFER;(void)slot;'),
    'count-kept': ('        WORD_AT(BSR_COUNTS+cam*4u)=++count;', '        ++count;'),
    'no-level-reset': ('    if(WORD_AT(BSR_POOL_PTR)!=bsr.pool) {', '    if(0) {'),
    'silent': ('    if(bsr.last_unlinked[k]==unlinked || !bsr_line()) return;', '    return;'),
    'logs-every-add': ('    if(bsr.last_unlinked[k]==unlinked || !bsr_line()) return;', '    if(!bsr_line()) return;'),
    'no-view-check': ('    if(!MAPMAKER_PTR_OK(view,BSR_RECTS+(BSR_ROOMS_MAX+1u)*12u)) return;\n', ''),
    'no-table-check': (' || !MAPMAKER_PTR_OK(table,end*BSR_PROP)) return;', ') return;'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        pairs = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        for old, new in (pairs if isinstance(pairs, list) else [pairs]):
            assert old in text, mutation
            text = text.replace(old, new, 1)
        (tmp_path / INC.name).write_text(text, encoding='utf-8')
    exe = tmp_path / ('bossroom.exe' if os.name == 'nt' else 'bossroom')
    source = ROOT / 'tests/mapmaker/mapmaker_bossroom.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/wd4505',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src'),
                str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Wno-unused-function', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), '-I' + str(ROOT / 'src'),
                str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_bossroom(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_')}
    result = subprocess.run([str(exe)], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=60)
    output = result.stdout.replace('\r\n', '\n')
    if mutation == 'fixed':
        assert result.returncode == 0, output
        assert output == EXPECTED, output
    else:
        assert result.returncode != 0 or output != EXPECTED, f'{mutation} was not detected'


def test_writes_only_the_camera_room_and_its_buffer():
    text = INC.read_text(encoding='utf-8')
    writes = re.findall(r'WORD_AT\(([^;]*?)\)\s*=[^=]', text)
    assert sorted(set(writes)) == ['BSR_COUNTS+cam*4u', 'BSR_LISTS+cam*4u', 'buf+count*4u'], writes
    assert 'guest_call' not in text


def test_wired_and_in_free_extension_memory():
    rooms = (ROOT / 'src/interface/pc_mapmaker_rooms.inc').read_text(encoding='utf-8')
    buckets = rooms[rooms.index('void pc_mapmaker_rooms_buckets'):]
    call = buckets.index('mapmaker_bossroom_view(view);')
    assert buckets.rfind('#ifdef MAPMAKER_BOSSROOM', 0, call) > 0
    assert call < buckets.index('guest_call(sub_000A79B0,1,MAPMAKER_ROOMS_ID_GLOBAL);')   # before any room is drawn
    guest = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    assert guest.index('#include "pc_mapmaker_limits.inc"') < guest.index('#include "pc_mapmaker_bossroom.inc"') \
        < guest.index('#include "pc_mapmaker_rooms.inc"')
    from test_mapmaker_rooms import ROOMS_BLOCKS
    assert ROOMS_BLOCKS['boss camera lists'] == (0x008D4000, 0x8000, [])
    assert '#define BSR_BUFFER 0x008D4000u' in INC.read_text(encoding='utf-8')
    doc = (ROOT / 'docs/mapmaker/mapmaker-realbosses.md').read_text(encoding='utf-8')
    assert 'Drawn through the camera' in doc and 'pc_mapmaker_bossroom.inc' in doc


# ---- stock facts (skipped without the private XBE) --------------------------

def by_address():
    from mapmaker_decoded import decoded
    return {i.address: i for i in decoded()}


def target(i):
    m = re.search(r'0x([0-9a-f]+)', i.text)
    return int(m.group(1), 16) if m else None


def test_room_prop_lists_are_rebuilt_per_frame_before_the_views():
    a = by_address()
    # the frame (sub_001CE2E0): relink, then the views' room lists
    assert target(a[0x001CE368]) == 0x0009E4B0 and target(a[0x001CE36D]) == 0x000897D0
    assert target(a[0x001CE38B]) == 0x0009E4B0
    # sub_0009E4B0: props from [0x007B975C] (the rooms' own are skipped) to [0x007B4FAC], each listed room appended
    assert 0x007B975C in [op[4] for op in a[0x0009E4ED].ops if op[0] == 'mem']
    assert target(a[0x0009E574]) == 0x0009E430 and a[0x0009E570].text == 'edx, dword ptr [ebp]'
    # sub_0009E430: a full room grows its capacity, then the pool bound drops the prop (raised cap kept)
    assert a[0x0009E46B].text == 'eax, 0x14cd'          # stock; objects.json obj-bucket-bound makes it 0x299D
    assert ('mem', None, 'edx', 4, 0x007B5BA0, 4) in a[0x0009E462].ops     # stock; rooms.json p3 -> 0x008CF0C0


def test_rooms_reach_props_only_through_their_lists():
    a = by_address()
    # sub_00086480 (reader): sub_000A79B0(room) then the draw; sub_000A79B0 walks [0x008CD040 + 4*room]
    assert target(a[0x00086579]) == 0x000A79B0 and a[0x00086579].function == 0x00086480
    assert target(a[0x000A79E6]) == 0x000A2430
