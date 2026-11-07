"""Giant skeleton draw log (src/interface/pc_mapmaker_skelvis.inc, log only):
fixture with mutations, wiring of its hooks, and decoder evidence for the
addresses it keys on (the room list query chain, the frustum test's only
caller and its cull)."""
import os
from pathlib import Path
import re
import subprocess

import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_skelvis.inc'
EXPECTED_FILE = ROOT / 'tests/mapmaker/mapmaker_skelvis.expected'

# Each mutation breaks one contract the fixture must detect.
MUTATIONS = {
    'any-model': ('*(uint16_t *)((uintptr_t)(p+SKV_MODEL_WORD)+g_xbox_mem_offset)==SKV_MODEL &&', '1 &&'),
    'any-type': (' &&\n           WORD_AT(p+SKV_TYPE)==SKV_CHARACTER;', ';'),
    'no-switch': ('    return mapmaker_limits.limits;', '    return 1;'),
    'any-scene': ('    if(WORD_AT(SKV_SCENE)!=SKV_PLAY) return 0;\n', ''),
    'no-truncation': ('    trunc=touched>max;', '    trunc=0;'),
    'truncation-at-cap': ('    trunc=touched>max;', '    trunc=touched>=max;'),
    'dropped-unseen': ('if((w&1u) && (w>>1)==cam) { where=2;break; }', 'if(0) { where=2;break; }'),
    'list-every-build': ('    if(key==s->list_key) return;\n', ''),
    'frustum-any-caller': (' || WORD_AT(g_esp)!=SKV_RET_FRUSTUM) { original();return; }', ') { original();return; }'),
    'frustum-zero-culls': ('if((int32_t)g_eax<0) s->behind++;', 'if((int32_t)g_eax<=0) s->behind++;'),
    'frustum-stays-armed': ('    uint32_t p=skv.cur;\n    SkvSkel *s;\n    skv.cur=0;', '    uint32_t p=skv.cur;\n    SkvSkel *s;'),
    'frustum-reads-before-original': [('    original();\n    skv.frustum_calls++;', '    skv.frustum_calls++;'),
                                      ('    if((int32_t)g_eax<0) s->behind++; else s->front++;\n',
                                       '    if((int32_t)g_eax<0) s->behind++; else s->front++;\n    original();\n')],
    'empty-rect-missed': ('    if(!(skv_f(p+0x118u)<skv_f(p+0x120u) && skv_f(p+0x11Cu)<skv_f(p+0x124u))) s->empty++;\n', ''),
    'planes-missed': ('    if(WORD_AT(p+SKV_FLAGS)&SKV_NO_ROOM) return SKV_PLANES;\n', ''),
    'unreached-as-unseen': ('return *visible?SKV_UNREACHED:SKV_UNSEEN;', 'return SKV_UNSEEN;'),
    'no-list-missed': ('    if(n<=0) return SKV_NOLIST;\n', ''),
    'other-views-judged': ('    skv.in_pass=slot==skv.slot;', '    skv.in_pass=1;'),
    'no-budget': ('    if(skv.lines>=SKV_LINES || (unseen && skv.unseen_lines>=SKV_UNSEEN_LINES)) return 0;', '    (void)unseen;'),
    'no-unseen-budget': (' || (unseen && skv.unseen_lines>=SKV_UNSEEN_LINES)) return 0;', ') return 0;'),
    'every-transition-unbudgeted': ('    if(skv_line(st==SKV_UNSEEN)) {', '    if(1) {'),
    'summary-always': ('    if(!skv.n || key==skv.sum_key || skv.summaries>=SKV_SUMMARIES) return;', '    if(!skv.n || skv.summaries>=SKV_SUMMARIES) return;'),
    'no-summary': ('        if(!(skv.passes%SKV_SUMMARY_PASSES)) skv_summary(0);\n', ''),
    'judges-freed-prop': ('    if(!skv_is_skel(p)) return;                    /* freed or reused: no counts */\n', ''),
    'no-reset-on-leave': ('        if(skv.n || skv.passes) mapmaker_skelvis_reset("left MapMaker play");\n', ''),
    'no-reset-on-rebuild': ('    if(skv.passes && rooms!=skv.rooms) mapmaker_skelvis_reset("rooms rebuilt");\n', ''),
    'counters-kept': ('        s->rects=s->empty=s->front=s->behind=s->draws=0;s->frustum=0;\n', ''),
    'writes-guest': ('    s->builds++;\n', '    s->builds++;WORD_AT(p+SKV_HOME)+=0u*touched+1u;\n'),
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
    exe = tmp_path / ('skelvis.exe' if os.name == 'nt' else 'skelvis')
    source = ROOT / 'tests/mapmaker/mapmaker_skelvis.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/wd4505',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src'),
                str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Wno-unused-function', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), '-I' + str(ROOT / 'src'),
                str(source), '-o', str(exe), '-lm']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


def run(exe):
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_')}
    result = subprocess.run([str(exe)], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=120)
    return result.returncode, result.stdout.replace('\r\n', '\n')


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_skelvis(tmp_path, mutation):
    code, output = run(build(tmp_path, mutation))
    expected = EXPECTED_FILE.read_text(encoding='utf-8')
    if mutation == 'fixed':
        assert code == 0, output
        assert output == expected, output
    else:
        assert code != 0 or output != expected, f'{mutation} was not detected'


def test_log_lines_are_the_documented_ones():
    expected = EXPECTED_FILE.read_text(encoding='utf-8')
    for line in ('skeleton 1: tracked (prop 0x400000): radius 6.50',
                 'skeleton 1: room list 10 of 14 touched rooms (cap 10) TRUNCATED, home room 3, camera room 7 DROPPED by the cap',
                 'skeleton 1: hidden (frustum culled at 0x000A24AA)',
                 'skeleton 1: drawn again after 2 passes hidden (frustum culled at 0x000A24AA)',
                 'hidden (none of its listed rooms visible)', 'hidden (view-plane culled at 0x000A2515)',
                 'hidden (no rooms in its list (0x000A2455))', 'summary after 600 passes: skeleton 1 drawn',
                 '60 lines this level, quiet now', 'left MapMaker play: forgetting 1 skeleton'):
        assert line in expected, line
    doc = (ROOT / 'docs/mapmaker/mapmaker-realbosses.md').read_text(encoding='utf-8')
    for fmt in ('skelvis: skeleton N: tracked', 'skelvis: skeleton N: room list', 'skelvis: skeleton N: hidden (',
                'skelvis: skeleton N: drawn again', 'skelvis: summary after'):
        assert fmt in doc, fmt


def test_log_only():
    text = INC.read_text(encoding='utf-8')
    assert not re.findall(r'(?:WORD_AT|FLOAT_AT|SKV_B|SKV_H)\([^;]*?\)\s*=[^=]', text)
    assert 'guest_call' not in text and 'guest_writable' not in text and 'g_eax=' not in text.replace(' ', '')


def test_hooks_wired():
    sites = {
        'src/interface/pc_mapmaker_rooms.inc': 'if(list_call) mapmaker_skelvis_list(out,max,touched,found,buffer,count);',
        'src/interface/pc_mapmaker_openview.inc': 'mapmaker_skelvis_pass();',
        'src/interface/pc_mapmaker_tank.inc': 'if(MAPMAKER_STACK_OK(8)) mapmaker_skelvis_drawn(WORD_AT(g_esp+4u));',
        'src/interface/pc_mapmaker_bigdraw.inc': 'mapmaker_skelvis_rect(prop);',
        'src/interface/pc_mapmaker_realbosses.inc': 'mapmaker_skelvis_reset("level restart");',
    }
    for path, call in sites.items():
        text = (ROOT / path).read_text(encoding='utf-8')
        assert call in text, path
        before = text[:text.index(call)]
        assert before.rstrip().splitlines()[-1].strip() == '#ifdef MAPMAKER_SKELVIS' or \
            before.rfind('#ifdef MAPMAKER_SKELVIS') > before.rfind('#endif'), path
    openview = (ROOT / 'src/interface/pc_mapmaker_openview.inc').read_text(encoding='utf-8')
    walk = openview[openview.index('void pc_mapmaker_openview_walk'):]
    assert walk.index('mapmaker_skelvis_pass();') < walk.index('    original();')     # before the walk resets the rects
    bigdraw = (ROOT / 'src/interface/pc_mapmaker_bigdraw.inc').read_text(encoding='utf-8')
    assert bigdraw.index('mapmaker_skelvis_rect(prop);') < bigdraw.index('FLOAT_AT(prop+BIGDRAW_RECT)=')  # stock rectangle
    rooms = (ROOT / 'src/interface/pc_mapmaker_rooms.inc').read_text(encoding='utf-8')
    query = rooms[rooms.index('void pc_mapmaker_rooms_query'):rooms.index('MAPMAKER_GUEST_RET();')]
    assert query.index('list_call=') < query.index('guest_call(sub_00157440')   # stack read before the walk
    assert 'touched+=word&1u;' in query
    guest = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    assert guest.index('#include "pc_mapmaker_limits.inc"') < guest.index('#include "pc_mapmaker_skelvis.inc"') \
        < guest.index('#include "pc_mapmaker_rooms.inc"')
    assert 'void pc_mapmaker_skelvis_frustum(void (*original)(void)) { original(); }' in guest
    header = (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    assert 'void pc_mapmaker_skelvis_frustum(void (*original)(void));' in header
    doc = (ROOT / 'docs/mapmaker/mapmaker-realbosses.md').read_text(encoding='utf-8')
    assert 'WRAP sub_0008C020 -> pc_mapmaker_skelvis_frustum' in doc


# ---- stock facts (skipped without the private XBE) --------------------------

def by_address():
    from mapmaker_decoded import decoded
    return {i.address: i for i in decoded()}


def target(i):
    m = re.search(r'0x([0-9a-f]+)', i.text)
    return int(m.group(1), 16) if m else None


def ends(a, site):
    return site + len(bytes.fromhex(a[site].hex))


def test_frustum_test_has_one_caller_and_culls_below_zero():
    from mapmaker_decoded import decoded
    calls = {(i.address, i.function) for i in decoded() if i.call and target(i) == 0x0008C020}
    assert calls == {(0x000A249E, 0x000A2430)}, calls
    a = by_address()
    assert ends(a, 0x000A249E) == 0x000A24A3                       # SKV_RET_FRUSTUM
    assert a[0x000A24A6].text == 'eax, eax' and a[0x000A24A8].mnemonic == 'jge'   # >= 0 goes on
    assert a[0x000A24AA].text == 'dword ptr [esi + 0x28], 0x800000'
    assert a[0x000A2515].mnemonic == 'mov' and a[0x000A2518].text == 'eax, 0x802000'


def test_room_list_query_chain():
    a = by_address()
    assert target(a[0x0009FCF0]) == 0x000873C0 and ends(a, 0x0009FCF0) == 0x0009FCF5        # SKV_RET_LIST
    assert target(a[0x000873DB]) == 0x00087310 and ends(a, 0x000873DB) == 0x000873E0        # SKV_RET_QUERY
    # three argument pushes, so the hook sees [esp] 0x000873E0, +4 box, +8 out, +0xC max, +0x10 0x0009FCF5
    assert [a[x].text for x in (0x000873D8, 0x000873D9, 0x000873DA)] == ['eax', 'ecx', 'edx']
    assert a[0x000873E3].mnemonic == 'ret' and a[0x000873D4].text == 'edx, dword ptr [esp + 8]'
    assert ('imm', 0xA) in a[0x0009FCC2].ops                                                # SKV_CAP
    assert a[0x0009FCDF].text == 'ebx, [esi + 0x38]' and a[0x0009FCE2].text == 'ebx'           # out = prop + 0x38
