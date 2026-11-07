"""MapMaker real bosses drawn at full height (src/interface/pc_mapmaker_bigdraw.inc):
fixture with mutations, plus decoder evidence for the room-scissor cause
(prop draw rectangle = union of its visible rooms' portal rectangles; the
camera's room alone gets the whole viewport)."""
import os
from pathlib import Path
import re
import subprocess

import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_bigdraw.inc'

M = '[mapmaker] bigdraw: '
EXPECTED = '\n'.join([
    'phase off',
    M + "real boss drawn over the whole view, not clipped to its room's portals (prop 0x400000, model 0x5E9)",
    'phase play',
    'phase scenes',
    M + 'real boss in no visible room: left to the stock cull (prop 0x400000, model 0x5E9)',
    'phase unseen',
    'phase bad',
    'mapmaker bigdraw ok',
]) + '\n'

# Each mutation breaks one contract the fixture must detect.
MUTATIONS = {
    'any-model': ('if((model!=BIGDRAW_SKELETON && model!=BIGDRAW_TROPHY && model!=BIGDRAW_WALKER &&\n        model!=BIGDRAW_ROBOT && model!=BIGDRAW_CROWE) || ', 'if('),
    'pit-satellites-left-out': ('if(model==BIGDRAW_PIT_BOTTLE || model==BIGDRAW_PIT_RACK || model==BIGDRAW_PIT_BANDAGE || model==BIGDRAW_RAGBUG) {}\n    else ', ''),
    'ragbug-left-out': (' || model==BIGDRAW_RAGBUG) {}', ') {}'),
    'walker-left-out': (' && model!=BIGDRAW_WALKER &&', ' &&'),
    'robot-left-out': ('\n        model!=BIGDRAW_ROBOT && model!=BIGDRAW_CROWE) || ', '\n        model!=BIGDRAW_CROWE) || '),
    'crowe-left-out': ('model!=BIGDRAW_ROBOT && model!=BIGDRAW_CROWE) || ', 'model!=BIGDRAW_ROBOT) || '),
    'skeleton-only': ('model!=BIGDRAW_SKELETON && model!=BIGDRAW_TROPHY', 'model!=BIGDRAW_SKELETON'),
    'trophy-only': ('model!=BIGDRAW_SKELETON && model!=BIGDRAW_TROPHY', 'model!=BIGDRAW_TROPHY'),
    'any-type': (' || WORD_AT(prop+BIGDRAW_TYPE)!=BIGDRAW_CHARACTER) return;', ') return;'),
    'no-switch': ('if(!mapmaker_limits.limits || ', 'if('),
    'any-scene': (' || WORD_AT(BIGDRAW_SCENE)!=BIGDRAW_PLAY) return;', ') return;'),
    'any-caller': (' && WORD_AT(g_esp)==BIGDRAW_RET) prop=', ') prop='),
    'reads-after-pop': [('if(MAPMAKER_STACK_OK(8u) && WORD_AT(g_esp)==BIGDRAW_RET) prop=WORD_AT(g_esp+4u);\n    original();',
                         'original();\n    if(MAPMAKER_STACK_OK(8u) && WORD_AT(g_esp-4u)==BIGDRAW_RET) prop=WORD_AT(g_esp+4u);')],
    'before-original': [('    original();\n    if(!prop', '    if(!prop'),
                        ('    if(!mapmaker_bigdraw.widened++)', '    original();\n    if(!mapmaker_bigdraw.widened++)')],
    'pops-return': ('    original();\n    if(!prop', '    original();g_esp+=4u;\n    if(!prop'),
    'widens-empty': ('if(!(FLOAT_AT(prop+BIGDRAW_RECT)<', 'if(0 && !(FLOAT_AT(prop+BIGDRAW_RECT)<'),
    'y-from-x': ('=(float)(int32_t)WORD_AT(vp+0x14u);', '=(float)(int32_t)WORD_AT(vp+4u);'),
    'x1-from-y': ('=(float)(int32_t)WORD_AT(vp+4u);', '=(float)(int32_t)WORD_AT(vp+0x14u);'),
    'height-only': ('    FLOAT_AT(prop+BIGDRAW_RECT)=(float)(int32_t)WORD_AT(vp);\n', ''),
    'y1-off-by-one': ('(float)(int32_t)WORD_AT(vp+0x18u)', '(float)(int32_t)WORD_AT(vp+0x18u)+1.0f'),
    'no-viewport-check': ('if(!MAPMAKER_PTR_OK(vp,0x1Cu)) return;', 'if(!vp) return;'),
    'no-stack-check': ('if(MAPMAKER_STACK_OK(8u) && ', 'if('),
    'silent': ('if(!mapmaker_bigdraw.widened++) mapmaker_bigdraw_log', 'if(!mapmaker_bigdraw.widened++ && 0) mapmaker_bigdraw_log'),
    'silent-unseen': ('if(!mapmaker_bigdraw.unseen++) mapmaker_bigdraw_log', 'if(!mapmaker_bigdraw.unseen++ && 0) mapmaker_bigdraw_log'),
    'logs-every-frame': ('if(!mapmaker_bigdraw.widened++) mapmaker_bigdraw_log', 'mapmaker_bigdraw.widened++;mapmaker_bigdraw_log'),
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
    exe = tmp_path / ('bigdraw.exe' if os.name == 'nt' else 'bigdraw')
    source = ROOT / 'tests/mapmaker/mapmaker_bigdraw.c'
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
def test_mapmaker_bigdraw(tmp_path, mutation):
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


def test_hook_only_writes_the_rectangle():
    text = INC.read_text(encoding='utf-8')
    writes = re.findall(r'(?:WORD_AT|FLOAT_AT|BIGDRAW_H)\(([^;]*?)\)\s*=[^=]', text)
    assert writes and all(w.startswith('prop+BIGDRAW_RECT') for w in writes), writes
    assert not (ROOT / 'config/guest-code-patches/bigdraw.json').exists()
    assert 'guest_writable' not in text and 'guest_call' not in text     # hot: no system calls


# ---- stock facts (skipped without the private XBE) --------------------------

def by_address():
    from mapmaker_decoded import decoded
    return {i.address: i for i in decoded()}


def target(i):
    m = re.search(r'0x([0-9a-f]+)', i.text)
    return int(m.group(1), 16) if m else None


def ends(a, site):
    return site + len(bytes.fromhex(a[site].hex))


def disp(i):
    return [op[4] for op in i.ops if op[0] == 'mem']


def test_draw_rectangle_has_one_caller():
    from mapmaker_decoded import decoded
    calls = {(i.address, i.function) for i in decoded() if i.call and target(i) == 0x000862C0}
    assert calls == {(0x000A246B, 0x000A2430)}, calls
    a = by_address()
    assert ends(a, 0x000A246B) == 0x000A2470                     # BIGDRAW_RET
    assert a[0x000862C0].text == 'eax, dword ptr [esp + 4]'     # cdecl prop
    assert a[0x0008642C].mnemonic == 'ret' and a[0x00086478].mnemonic == 'ret'
    # sub_000A2430 is run per prop of a room about to be drawn: sub_000A79B0 (room
    # list walk) from sub_00086480 just before that room's draw sub_000A2900.
    assert target(a[0x000A79E6]) == 0x000A2430 and a[0x000A79E6].function == 0x000A79B0
    assert target(a[0x00086579]) == 0x000A79B0 and target(a[0x0008658F]) == 0x000A2900
    assert a[0x00086579].function == a[0x0008658F].function == 0x00086480


def test_rectangle_is_the_union_of_visible_room_rects():
    a = by_address()
    # union over the prop's listed rooms (+0x30 count, +0x38 ids) of the view's room rects (+0x3BC + 12*room)
    assert a[0x000862E3].text == 'cl, byte ptr [eax + 0x30]' and a[0x00086319].text == 'edx, [eax + 0x38]'
    assert 0x3BC in disp(a[0x0008632B]) and a[0x00086332].text == 'dword ptr [ecx], 0'   # negative flag = not visible
    assert [disp(a[x]) for x in (0x00086337, 0x00086354, 0x00086371, 0x0008638A)] == [[4], [6], [8], [0xA]]
    # empty start values, then the clamp to / the outside-a-room copy of the viewport
    stores = [(x, disp(a[x])) for x in (0x0008643F, 0x00086452, 0x00086465)]
    assert all(0x007E71C8 in d for _, d in stores), stores
    pairs = [(disp(a[r])[0], disp(a[w])[0]) for r, w in ((0x00086433, 0x00086437), (0x00086445, 0x0008644A),
                                                         (0x00086458, 0x0008645D), (0x0008646B, 0x00086470))]
    assert pairs == [(0, 0x118), (0x14, 0x11C), (4, 0x120), (0x18, 0x124)], pairs    # x0, y0, x1, y1


def test_camera_room_gets_the_viewport():
    a = by_address()
    # sub_00089B90: the portal walk starts at the camera's room (view +0x2A0, else +0x2A4)
    # with the whole viewport x0 [vp], x1 [vp+4], y0 [vp+0x14], y1 [vp+0x18].
    assert a[0x00089BD9].call and target(a[0x00089BD9]) == 0x00087F80 and a[0x00089BD9].function == 0x00089B90
    assert 0x2A0 in disp(a[0x00089B96]) and 0x2A4 in disp(a[0x00089BA0])
    assert 0x007E71C8 in disp(a[0x00089BC2])
    assert [disp(a[x]) for x in (0x00089BC7, 0x00089BCA, 0x00089BCE, 0x00089BD2)] == [[0x18], [0x14], [4], [0]]


def test_rectangle_is_the_scissor_and_the_cull():
    a = by_address()
    # sub_000A2740 (the room's prop draw): scissor sub_0001A010(x0, y0, x1, y1) from +0x118.. before the model draw
    assert [disp(a[x])[0] for x in (0x000A2836, 0x000A283E, 0x000A2846, 0x000A2850)] == [0x124, 0x120, 0x11C, 0x118]
    assert target(a[0x000A285B]) == 0x0001A010 and target(a[0x000A28D4]) == 0x0008F950
    assert a[0x000A285B].function == a[0x000A28D4].function == 0x000A2740
    assert a[0x000A286A].text == 'eax, 0x20' and a[0x000A2878].text == 'eax, 8'    # +0x20 type, 8 = character
    assert a[0x000A276C].text.startswith('word ptr [esi + 0x94]')                  # word +0x94 model
    # sub_000A2430: the projected box is tested against the same rectangle
    for site, d in ((0x000A25C4, 0x118), (0x000A25DF, 0x120), (0x000A25FA, 0x11C), (0x000A260E, 0x124)):
        assert d in disp(a[site]), hex(site)
    # sub_0001A010 sets the scissor (an inverted rectangle becomes the whole target)
    assert a[0x0001A02F].text == 'esi, ecx' and a[0x0001A033].text == 'esi, esi'


def test_room_list_comes_from_the_box():
    a = by_address()
    # sub_0009FBE0: up to 10 rooms touching +0x64 + 0x1A8 -+ 0x1C0 (x; y, z alike)
    assert a[0x0009FCF0].call and target(a[0x0009FCF0]) == 0x000873C0 and a[0x0009FCF0].function == 0x0009FBE0
    assert ('imm', 0xA) in a[0x0009FCC2].ops
    assert 0x1A8 in disp(a[0x0009FC44]) and 0x1C0 in disp(a[0x0009FC4C])
    assert a[0x0009FCFE].text == 'byte ptr [esi + 0x30], al'
    # a character's box: a cube of the model radius (sub_0008E5F0) at sub_0009EAB0
    assert target(a[0x0009EACE]) == 0x0008E5F0
    assert [disp(a[x]) for x in (0x0009EB33, 0x0009EB3B, 0x0009EB43)] == [[0x1C0], [0x1C4], [0x1C8]]


def test_wrapper_documented_and_wired():
    doc = (ROOT / 'docs/mapmaker/mapmaker-realbosses.md').read_text(encoding='utf-8')
    assert 'WRAP sub_000862C0 -> pc_mapmaker_bigdraw_rect' in doc
    assert 'void pc_mapmaker_bigdraw_rect(void (*original)(void))' in INC.read_text(encoding='utf-8')
    header = (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    assert 'void pc_mapmaker_bigdraw_rect(void (*original)(void));' in header
    guest = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    assert guest.index('#include "pc_mapmaker_limits.inc"') < guest.index('#include "pc_mapmaker_bigdraw.inc"')
    assert 'void pc_mapmaker_bigdraw_rect(void (*original)(void)) { original(); }' in guest
