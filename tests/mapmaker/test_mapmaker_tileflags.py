"""MapMaker right-click tile roof menu (src/interface/pc_mapmaker_tileflags.inc) and the
flagged-tile hover label (pc_mapmaker_roof.inc): fixture with mutations, plus the
decoder and XBE evidence the design relies on (skipped without the private XBE)."""
import os
from pathlib import Path
import re
import struct
import subprocess

import pytest

from native_compiler import native_compiler
from mapmaker_decoded import decoded

ROOT = Path(__file__).resolve().parents[2]
TILEFLAGS = ROOT / 'src/interface/pc_mapmaker_tileflags.inc'
ROOF = ROOT / 'src/interface/pc_mapmaker_roof.inc'

MUTATIONS = {
    'any-mode': (TILEFLAGS, "if(!e || WORD_AT(e+TF_SLOT)!=0u || WORD_AT(e+TF_HELD)", "if(!e || WORD_AT(e+TF_HELD)"),
    'while-holding': (TILEFLAGS, "if(!e || WORD_AT(e+TF_SLOT)!=0u || WORD_AT(e+TF_HELD) ||", "if(!e || WORD_AT(e+TF_SLOT)!=0u ||"),
    'over-menu': (TILEFLAGS, "WORD_AT(e+TF_HELD) || (WORD_AT(e+TF_FLAGS)&TF_MENU_OPEN)) return 0;", "WORD_AT(e+TF_HELD)) return 0;"),
    'empty-cell': (TILEFLAGS, "    if(!(piece=WORD_AT(e+TF_UNDER)) || !MAPMAKER_PTR_OK(piece,0x48u)) return 0;\n",
                   "    piece=WORD_AT(e+TF_UNDER);\n"),
    'any-scene': (TILEFLAGS, "if(!mapmaker_limits.limits || WORD_AT(0x0079094Cu)!=102u) return 0;", "if(!mapmaker_limits.limits) return 0;"),
    'without-limits': (TILEFLAGS, "if(!mapmaker_limits.limits || WORD_AT(0x0079094Cu)!=102u) return 0;", "if(WORD_AT(0x0079094Cu)!=102u) return 0;"),
    'no-request': (TILEFLAGS, "    tf.request=piece;\n    return 1;", "    return 1;"),
    'request-kept': (TILEFLAGS, "    tf.request=0;\n    if(!(e=tf_editor())", "    if(!(e=tf_editor())"),
    'moved-piece': (TILEFLAGS, "WORD_AT(e+TF_UNDER)!=piece || WORD_AT(e+TF_HELD) ||\n", "WORD_AT(e+TF_HELD) ||\n"),
    'open-and-input': (TILEFLAGS, "    MAPMAKER_GUEST_RET();                               /* no tile input this frame */", "    original();"),
    'no-open': (TILEFLAGS, "    guest_call(sub_002FB230,1,tf.desc);\n", ""),
    'wrong-build': (TILEFLAGS, "#define TF_BUILD 0x00319300u", "#define TF_BUILD 0x003192E0u"),
    'start-normal': (TILEFLAGS, "    WORD_AT(tf.value)=tf.applied;\n", "    WORD_AT(tf.value)=0;\n"),
    'build-any-menu': (TILEFLAGS, "!MAPMAKER_PTR_OK(menu,0x220u) || WORD_AT(menu+4u)!=tf.desc) { original();return; }", "!MAPMAKER_PTR_OK(menu,0x220u)) { original();return; }"),
    'build-stock-too': (TILEFLAGS, "    guest_call(sub_00077320,2,WORD_AT(TF_POS_LIST),y);\n    MAPMAKER_GUEST_RET();", "    guest_call(sub_00077320,2,WORD_AT(TF_POS_LIST),y);\n    original();"),
    'no-frame-flag': (TILEFLAGS, "    WORD_AT(menu+0x218u)=1u;\n", ""),
    'two-options': (TILEFLAGS, "for(k=0;k<3u;k++)", "for(k=0;k<2u;k++)"),
    'unbound': (TILEFLAGS, "    guest_call(sub_00078590,1,tf.value);\n", "    guest_call(sub_00078590,1,0u);\n"),
    'no-done': (TILEFLAGS, "    guest_call(sub_00079D60,6,guest_call(sub_0003D7B0,1,TF_DONE_TEXT)", "    (void)(0,guest_call(sub_0003D7B0,1,TF_DONE_TEXT)"),
    'position-sign': (TILEFLAGS, "f=FLOAT_AT(TF_POS_BASE)-FLOAT_AT(menu+0x68u);", "f=FLOAT_AT(menu+0x68u)-FLOAT_AT(TF_POS_BASE);"),
    'tick-any-menu': (TILEFLAGS, "MAPMAKER_PTR_OK(menu,8u) && WORD_AT(menu+4u)==tf.desc && tf.piece", "MAPMAKER_PTR_OK(menu,8u) && tf.piece"),
    'tick-any-value': (TILEFLAGS, "(v=WORD_AT(tf.value))<=2u &&", "(v=WORD_AT(tf.value))<=7u &&"),
    'tick-reused-piece': (TILEFLAGS, " && WORD_AT(tf.piece+0x10u)==tf.tile) {", ") {"),
    'tick-no-original': (TILEFLAGS, "set to %s\\n\",n,n==1u?\"\":\"s\",tf_texts[v+1u]);\n    }\n    original();", "set to %s\\n\",n,n==1u?\"\":\"s\",tf_texts[v+1u]);\n    }\n    g_esp+=4u;"),
    'no-group': (TILEFLAGS, "    tf.group=(WORD_AT(piece+0x14u)&TF_SELECTED)!=0;\n", "    tf.group=0;\n"),
    'group-all': (TILEFLAGS, "if(p!=tf.piece && (WORD_AT(p+0x14u)&TF_SELECTED))", "if(p!=tf.piece)"),
    'not-applied': (TILEFLAGS, "        tf_mark(tf.piece,v);n=1;\n", "        n=1;\n"),
    'label-missing': (ROOF, "        mapmaker_roof_suffix(mode);\n", "        (void)mode;\n"),
    'label-any-caller': (ROOF, "    else if(mapmaker_roof.marked && MAPMAKER_STACK_OK(16u) && WORD_AT(g_esp)==MAPMAKER_ROOF_RET_HOVER_FORMAT &&",
                         "    else if(mapmaker_roof.marked && MAPMAKER_STACK_OK(16u) &&"),
    'label-unflagged': (ROOF, "    return piece && mapmaker_roof_editor()?mapmaker_roof_mode(piece):0u;", "    return piece && mapmaker_roof_editor()?2u:0u;"),
    'label-wrong-tail': (ROOF, '{" (No Roof)"," (Open Air)"}', '{" (Open Air)"," (No Roof)"}'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        path, old, new = MUTATIONS[mutation]
        text = path.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / path.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('tileflags.exe' if os.name == 'nt' else 'tileflags')
    source = ROOT / 'tests/mapmaker/mapmaker_tileflags.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src'),
                str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Wno-unused-function', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), '-I' + str(ROOT / 'src'),
                str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_tileflags(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_MAPMAKER')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker tileflags ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


# ---- stock facts (skipped without the private XBE) --------------------------

def by_address():
    return {i.address: i for i in decoded()}


def target(i):
    m = re.search(r'0x([0-9a-f]+)', i.text)
    return int(m.group(1), 16) if m else None


def mem(i):
    return [op for op in i.ops if op[0] == 'mem']


def test_tiles_mode_back_selects():
    """Tiles mode (slot 0) Back: rotate the held piece, else flip the selection bit."""
    a = by_address()
    calls = lambda f: {i.address for i in decoded() if i.call and target(i) == f}
    assert calls(0x00303E10) == {0x003063D9} and a[0x003063D9].function == 0x003063C0
    assert mem(a[0x003063C6])[0][4] == 0x254 and '0x306444' in a[0x003063D1].text
    assert ('imm', 0x20) in a[0x003042D0].ops and mem(a[0x003042DA])[0][4] == 0x2F7C
    assert mem(a[0x003042E4])[0][4] == 0x2F18 and ('imm', 2) in a[0x003042F3].ops
    assert mem(a[0x003042F7])[0][4] == 0x14 and target(a[0x003042A8]) == 0x00303D00


def test_options_menu_route():
    """Objects mode opens a node's options with sub_002FB230(desc); the pickup
    options descriptor is {sub_00319300, sub_003192E0}; the build's frame and
    Done row; menu +4 is the descriptor."""
    a = by_address()
    assert target(a[0x003051DB]) == 0x002FB230
    assert target(a[0x002FB2A5]) == 0x00079860 and ('imm', 0x4000000) in a[0x002FB25B].ops
    assert mem(a[0x000798B4])[0][4] == 4
    from test_mapmaker_realbosses import xbe_reader
    rd = xbe_reader()
    assert struct.unpack('<4I', rd(0x0052DECC, 16)) == (0x00319300, 0x003192E0, 0, 0)
    slot0 = struct.unpack('<6I', rd(0x0052CD78, 24))
    assert slot0[2] == 0xFFFFFFFF and slot0[3] == slot0[5] == 0      # no reopen, no open/close callbacks
    assert ('imm', 0x8400) in a[0x0031931B].ops and target(a[0x00319323]) == 0x00075B70
    assert ('imm', 0x002FB3C0) in a[0x0031930D].ops and mem(a[0x00319337])[0][4] == 0x218
    assert target(a[0x00319341]) == 0x00071B10 and ('imm', 0x20000) in a[0x00319346].ops
    assert ('imm', 0xB5E) in a[0x003193F8].ops and ('imm', 0x002FB3C0) in a[0x003193EA].ops
    assert target(a[0x00319406]) == 0x00079D60 and target(a[0x0031940B]) == 0x00075E70
    assert mem(a[0x00319410])[0][4] == 0x0049F7E8 and mem(a[0x00319418])[0][4] == 0x68
    assert mem(a[0x0031941D])[0][4] == 0x007BB1E8 and target(a[0x0031942B]) == 0x00077320
