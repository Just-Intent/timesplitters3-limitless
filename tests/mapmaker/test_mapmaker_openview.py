"""MapMaker open-air view (src/interface/pc_mapmaker_openview.inc): rooms seen
across Open air tiles are drawn over the whole opening, not clipped to the
portal rectangles of the open sides. Fixture with mutations, plus decoder
evidence for the portal-walk cause (skipped without the private XBE)."""
import os
from pathlib import Path
import re
import subprocess

import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_openview.inc'
ROOF = ROOT / 'src/interface/pc_mapmaker_roof.inc'

M = '[mapmaker] openview: '
R = '[mapmaker] roof: bounded piece 1: rooms 3..3 (mask 0x01000000, tile 0 at 0,0 floor 0 rotation 0)'
EXPECTED = '\n'.join([
    R,
    'phase roof',
    'phase stock',
    M + '4 open-air rooms in 2 areas; 7 rooms drawn across them at most (8 portals read, 9 rooms, 0 beside by box, 9 two-sided)',
    M + '4 rooms widened across open air (3 not reached by the portal walk), camera room 1',
    'phase inside',
    'phase outside',
    'phase island',
    R,
    M + '3 open-air rooms in 3 areas; 8 rooms drawn across them at most (5 portals read, 9 rooms, 0 beside by box, 9 two-sided)',
    M + '2 rooms widened across open air (2 not reached by the portal walk), camera room 1',
    'phase rebuild',
    *[R, M + '4 open-air rooms in 2 areas; 8 rooms drawn across them at most (8 portals read, 9 rooms, 1 beside by box, 9 two-sided)',
      M + '5 rooms widened across open air (5 not reached by the portal walk), camera room 1'] * 6,
    R,
    M + '4 open-air rooms in 2 areas; 9 rooms drawn across them at most (8 portals read, 9 rooms, 2 beside by box, 9 two-sided)',
    M + '6 rooms widened across open air (6 not reached by the portal walk), camera room 1',
    R,
    M + '4 open-air rooms in 2 areas; 8 rooms drawn across them at most (8 portals read, 9 rooms, 1 beside by box, 9 two-sided)',
    M + '5 rooms widened across open air (5 not reached by the portal walk), camera room 1',
    R,
    M + '4 open-air rooms in 1 area; 8 rooms drawn across them at most (8 portals read, 9 rooms, 1 beside by box, 9 two-sided)',
    M + '1 open-air rooms joined their area by box (no open portal between them)',
    M + '7 rooms widened across open air (7 not reached by the portal walk), camera room 1',
    R,
    M + '4 open-air rooms in 2 areas; 8 rooms drawn across them at most (8 portals read, 9 rooms, 1 beside by box, 9 two-sided)',
    M + '5 rooms widened across open air (5 not reached by the portal walk), camera room 1',
    'phase boxes',
    R,
    M + '4 open-air rooms in 4 areas; 4 rooms drawn across them at most (0 portals read, 9 rooms, 0 beside by box, 9 two-sided)',
    M + '4 open-air rooms in 3 areas; 7 rooms drawn across them at most (5 portals read, 9 rooms, 0 beside by box, 9 two-sided)',
    M + '2 rooms widened across open air (2 not reached by the portal walk), camera room 1',
    M + '4 open-air rooms in 3 areas; 8 rooms drawn across them at most (6 portals read, 9 rooms, 0 beside by box, 9 two-sided)',
    M + '3 rooms widened across open air (3 not reached by the portal walk), camera room 1',
    M + '4 open-air rooms in 2 areas; 7 rooms drawn across them at most (8 portals read, 9 rooms, 0 beside by box, 9 two-sided)',
    M + '4 rooms widened across open air (4 not reached by the portal walk), camera room 1',
    'phase bad',
    'mapmaker openview ok',
]) + '\n'

# Each mutation breaks one contract the fixture must detect.
MUTATIONS = {
    'no-switch': (INC, '    if(!mapmaker_limits.limits) return;\n', ''),
    'any-scene': (INC, ' || WORD_AT(OPENVIEW_SCENE)!=OPENVIEW_PLAY) return;', ') return;'),
    'before-original': [(INC, '    original();\n#ifdef MAPMAKER_CAMERA_VIEW_DIAG\n', '#ifdef MAPMAKER_CAMERA_VIEW_DIAG\n'),
                        (INC, '    if((int32_t)WORD_AT(view+OPENVIEW_DEEPEST)<deepest)', '    original();\n    if((int32_t)WORD_AT(view+OPENVIEW_DEEPEST)<deepest)')],
    'pops-twice': (INC, '    original();\n#ifdef MAPMAKER_CAMERA_VIEW_DIAG\n', '    original();g_esp+=4u;\n#ifdef MAPMAKER_CAMERA_VIEW_DIAG\n'),
    'whole-view-always': (INC, '        if(depth<0) continue;                       /* no room of this area is visible */\n',
                          '        if(depth<0) { x0=0;y0=0;x1=640;y1=480;depth=0; }\n'),
    'unseen-counts': (INC, '            if(d<0 || !(a<c && b<f)) continue;', '            if(d<0) continue;'),
    'no-union': (INC, '                if(a<x0) x0=a;\n', ''),
    'max-depth': (INC, '                if(d<depth) depth=d;', '                if(d>depth) depth=d;'),
    'no-depth-cap': (INC, 'depth=depth+1>(int32_t)OPENVIEW_DEPTH_CAP?(int32_t)OPENVIEW_DEPTH_CAP:depth+1;', 'depth=depth+1;'),
    'same-depth': (INC, 'depth=depth+1>(int32_t)OPENVIEW_DEPTH_CAP?(int32_t)OPENVIEW_DEPTH_CAP:depth+1;',
                   'depth=depth>(int32_t)OPENVIEW_DEPTH_CAP?(int32_t)OPENVIEW_DEPTH_CAP:depth;'),
    'overwrites-depth': (INC, '            if((int32_t)WORD_AT(e)<0) {', '            if(1) {'),
    'replaces-rect': (INC, '            if(x0<OPENVIEW_H(e+4u)) {', '            if(1) {'),
    'deepest-untouched': (INC, '                if(deepest<depth) deepest=depth;', ''),
    'no-closed-neighbours': (INC, '                if(mapmaker_roof_play.open[o-1u] || mapmaker_openview.stamp[o-1u]==mark) continue;',
                             '                continue;'),
    'neighbours-of-neighbours': (INC, '                if(mapmaker_roof_play.open[next[i]-1u] && !mapmaker_openview.stamp[next[i]-1u]) {',
                                 '                if(!mapmaker_openview.stamp[next[i]-1u]) {'),
    'one-group': (INC, '        if(depth<0) continue;                       /* no room of this area is visible */\n',
                  '        if(depth<0) continue;                       /* no room of this area is visible */\n        if(g) break;\n'),
    'closed-once': (INC, ' || mapmaker_openview.stamp[o-1u]==mark) continue;', ') continue;'),
    'never-rebuilt': (INC, '    if(!mapmaker_openview.ready || mapmaker_openview.generation!=mapmaker_roof_play.generation ||',
                      '    if(!mapmaker_openview.ready ||'),
    'no-range-check': (INC, '        if((a=a==r?b:a)<1u || a>n) continue;', '        a=a==r?b:a;if(a>=MAPMAKER_ROOF_ROOMS) continue;'),
    'no-count-check': (INC, '    if(count>OPENVIEW_PORTALS_PER_ROOM || ', '    if('),
    'no-portal-check': (INC, ' || !MAPMAKER_PTR_OK(p=WORD_AT(portals+id*4u),8u)) continue;', ') continue;p=WORD_AT(portals+id*4u);if(!p) continue;'),
    'room-cap': (INC, 'if(n<1u || n>=MAPMAKER_ROOF_ROOMS || ', 'if(n<1u || '),
    'silent': (INC, '    if(mapmaker_openview.build_logs<32u) {', '    if(0) {'),
    'logs-every-frame': (INC, '    if(widened>mapmaker_openview.peak && mapmaker_openview.peak_logs<4u) {', '    if(widened) {'),
    # rooms beside the area by box, and the two-sided marks
    'no-box-join': (INC, '                    mapmaker_openview.stamp[o-1u]=(uint16_t)mark;queue[tail++]=o;mapmaker_openview.joined++;\n',
                    '                    mapmaker_openview.joined+=0u;\n'),
    'box-join-closed-too': (INC, '                if(mapmaker_roof_play.open[o-1u] && !mapmaker_openview.stamp[o-1u] && mapmaker_openview.has_box[o-1u] &&',
                            '                if(!mapmaker_openview.stamp[o-1u] && mapmaker_openview.has_box[o-1u] &&'),
    'no-box-neighbours': (INC, '            if(i==tail) continue;\n', '            continue;\n'),
    'box-no-margin': (INC, '#define OPENVIEW_TOUCH 0.5f', '#define OPENVIEW_TOUCH 0.0f'),
    'box-wide-margin': (INC, '#define OPENVIEW_TOUCH 0.5f', '#define OPENVIEW_TOUCH 1.0f'),
    'box-absurd-accepted': (INC, '        if(!(b[i]>-OPENVIEW_FAR && b[i]<OPENVIEW_FAR)) return 0;   /* NaN too */\n', ''),
    'box-empty-accepted': (INC, '    return b[0]<b[3] && b[1]<b[4] && b[2]<b[5];', '    return b[0]<=b[3] && b[1]<=b[4] && b[2]<=b[5];'),
    'box-x-only': (INC, ' && a[1]-OPENVIEW_TOUCH<=b[4] &&\n           b[1]-OPENVIEW_TOUCH<=a[4] && a[2]-OPENVIEW_TOUCH<=b[5] && b[2]-OPENVIEW_TOUCH<=a[5];', ';'),
    'box-wrong-field': (INC, '#define OPENVIEW_BOX 0x10u', '#define OPENVIEW_BOX 0x14u'),
    'not-two-sided': (INC, '{ mapmaker_roof_play.twosided[r-1u]=1;mapmaker_roof_play.twosided_rooms++; }', '{ mapmaker_roof_play.twosided_rooms++; }'),
    'two-sided-open-only': (INC, '    if(g) for(r=1;r<=n && r<=MAPMAKER_ROOF_ROOMS;r++) {', '    if(g) for(r=1;r<=n && r<=MAPMAKER_ROOF_ROOMS;r++) if(mapmaker_roof_play.open[r-1u]) {'),
    'two-sided-kept': (INC, '    memset(mapmaker_roof_play.twosided,0,sizeof mapmaker_roof_play.twosided);\n    mapmaker_roof_play.twosided_rooms=0;\n    if(g) for(',
                       '    if(g) for('),
    # the roof side
    'roof-bounded-only': (ROOF, '    mapmaker_roof_play.mode[k]=(uint8_t)(mode==MAPMAKER_ROOF_OPEN_BOUNDED || mode==MAPMAKER_ROOF_OPEN_AIR?mode:0u);',
                          '    mapmaker_roof_play.mode[k]=(uint8_t)(mode==MAPMAKER_ROOF_OPEN_BOUNDED?mode:0u);'),
    'roof-not-open': (ROOF, '        mapmaker_roof_play.open[base+j]=1;', ''),
    'roof-hides-unbounded': (ROOF, '        if(mapmaker_roof_play.mode[k]!=MAPMAKER_ROOF_OPEN_BOUNDED) continue;\n', ''),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        pairs = MUTATIONS[mutation]
        texts = {}
        for path, old, new in (pairs if isinstance(pairs, list) else [pairs]):
            text = texts.get(path, path.read_text(encoding='utf-8'))
            assert old in text, mutation
            texts[path] = text.replace(old, new, 1)
        for path, text in texts.items():
            (tmp_path / path.name).write_text(text, encoding='utf-8')
    exe = tmp_path / ('openview.exe' if os.name == 'nt' else 'openview')
    source = ROOT / 'tests/mapmaker/mapmaker_openview.c'
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
def test_mapmaker_openview(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_')}
    result = subprocess.run([str(exe)], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=60)
    output = result.stdout.replace('\r\n', '\n')
    # the per-room diagnostic lines (closed rooms beside an area) are checked apart
    closed = [line for line in output.split('\n') if line.startswith(M + 'area ')]
    output = '\n'.join(line for line in output.split('\n') if not line.startswith(M + 'area '))
    if mutation == 'fixed':
        assert result.returncode == 0, output
        assert output == EXPECTED, output
        assert M + ('area 1: closed room 9 (tile 0 at 0,0 floor 0 rotation 0, mask 0x01000000) beside open room 1 '
                    '(tile 0 at 0,0 floor 0 rotation 0) by a portal') in closed, closed
        assert any(line.endswith('by its box') for line in closed), closed
    else:
        assert result.returncode != 0 or output != EXPECTED, f'{mutation} was not detected'


def test_hook_writes_only_room_entries():
    text = INC.read_text(encoding='utf-8')
    writes = re.findall(r'(?:WORD_AT|OPENVIEW_H)\(([^;]*?)\)\s*=[^=]', text)
    assert writes and all(w.startswith(('e', 'e+', 'view+OPENVIEW_DEEPEST')) for w in writes), writes
    assert 'guest_call' not in text and 'guest_writable' not in text      # per frame: no guest calls
    # the draw buckets and the room-id width belong to the room-cap work: not touched here
    for name in ('0x007042B8', '0x00702DB0', '0xE0C', '0xE08'):
        assert name not in text, name


def test_wrapper_documented_and_wired():
    doc = (ROOT / 'docs/mapmaker/mapmaker-sky.md').read_text(encoding='utf-8')
    assert 'WRAP sub_00089B90 -> pc_mapmaker_openview_walk' in doc
    assert 'void pc_mapmaker_openview_walk(void (*original)(void))' in INC.read_text(encoding='utf-8')
    header = (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    assert 'void pc_mapmaker_openview_walk(void (*original)(void));' in header
    guest = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    assert guest.index('#include "pc_mapmaker_roof.inc"') < guest.index('#include "pc_mapmaker_openview.inc"')
    assert guest.index('#include "pc_mapmaker_limits.inc"') < guest.index('#include "pc_mapmaker_openview.inc"')
    assert 'void pc_mapmaker_openview_walk(void (*original)(void)) { original(); }' in guest


# ---- stock facts (skipped without the private XBE) --------------------------

def by_address():
    from mapmaker_decoded import decoded
    return {i.address: i for i in decoded()}


def target(i):
    m = re.search(r'0x([0-9a-f]+)', i.text)
    return int(m.group(1), 16) if m else None


def disp(i):
    return [op[4] for op in i.ops if op[0] == 'mem']


def test_walk_is_reached_by_tail_jumps_per_view():
    from mapmaker_decoded import decoded
    sites = {i.address for i in decoded() if target(i) == 0x00089B90 and (i.call or i.mnemonic == 'jmp')}
    assert sites == {0x001CE688, 0x001CE8E3}, sites
    a = by_address()
    assert a[0x001CE688].mnemonic == 'jmp' and a[0x001CE8E3].mnemonic == 'jmp'
    assert target(a[0x0003C56A]) == 0x001CE650                 # per view, from the game tick
    assert 0x7B0CC0 in disp(a[0x0003C555])                      # the view loop sets the current view
    # sub_00089B90: reset (sub_000878A0), then the walk from the camera's room with the viewport
    assert target(a[0x00089BBD]) == 0x000878A0 and target(a[0x00089BD9]) == 0x00087F80
    assert a[0x00089BE2].mnemonic == 'ret'


def test_walk_stores_depth_and_rect_per_room():
    a = by_address()
    # sub_00087F80: room entry view +0x3BC + 12*room (the shadow moves it to +0x6000)
    assert 0x3BC in disp(a[0x00087FB7])
    assert a[0x00087FD5].text == 'dword ptr [eax], edx'          # depth (kept the deepest)
    assert [disp(a[x]) for x in (0x00087FFA, 0x0008800D, 0x00088020, 0x00088033)] == [[4], [6], [8], [0xA]]
    # the portal list of a room (+4 of the 0xB4 record: count, ids) and the portal's two rooms
    assert ('imm', 0xB4) in a[0x00087F9B].ops and 0x708788 in disp(a[0x00087FB1])
    assert a[0x00088037].text == 'eax, dword ptr [ecx + 4]'
    assert 0x70878C in disp(a[0x00088075]) and a[0x00088089].text == 'eax, dword ptr [edi]'
    assert a[0x00088093].text == 'ecx, dword ptr [edi + 4]'
    # a neighbour is entered only with its portal's projected rectangle clipped to the incoming one
    assert target(a[0x000883E9]) == 0x00087F80 and target(a[0x00088168]) == 0x00087F80
    # the reset: depth -1 (esi = -1), x1/y1 -1
    assert 0x3BC in disp(a[0x00087916]) and a[0x000878AC].text == 'esi, 0xffffffff'


def test_room_list_needs_depth_and_rect():
    a = by_address()
    # sub_000897D0: buckets rooms by depth 0..0x4F, lists those with x1 > x0
    assert 0xD28 in disp(a[0x00089824]) and a[0x0008982C].text == 'ecx, 0x50'
    assert a[0x00089883].text == 'eax, eax' and a[0x00089887].text == 'eax, 0x50'
    assert a[0x00089919].text == 'dx, cx'
