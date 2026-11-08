"""MapMaker logic: action Delay (src/interface/pc_mapmaker_logic.inc).

The C fixture with mutations; the decoder evidence the hooks rely on (the
spare op word +0x154, the runtime copy at +0x194 that nothing reads, the
edit screen's call site and the level start); the shipped sample maps (every
stock Delay word is 0, and the offline logic printout reads them); and the
wiring (prototypes, stubs, include, docs)."""
import os
from pathlib import Path
import re
import struct
import subprocess
import sys

import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'src/interface/pc_mapmaker_logic.inc'
sys.path.insert(0, str(ROOT / 'tools/testing'))
import map_report  # noqa: E402

EXPECTED = '\n'.join([
    'stock map: original only, 6 calls',
    'story delays: 0.5 s at frame 30, 1 s at frame 60, 2 s at 3 ticks a frame at frame 101',
    'other modes: delays ignored',
    'overflow: 256 queued, 44 fired at once',
    'many due: 50 a pass',
    'editor steps: 5 10 20 30 50 100 150 300 600 0 ',
    'editor row: actions only, edit screen only',
    'picker rebuild: delays kept (0 50 20)',
    'mapmaker logic ok',
]) + '\n'

# Each mutation breaks one contract the fixture must detect.
MUTATIONS = {
    'any-mode': ('return WORD_AT(LOGIC_SCENE)==LOGIC_PLAY && LOGIC_B(LOGIC_MODE)==LOGIC_STORY;',
                 'return WORD_AT(LOGIC_SCENE)==LOGIC_PLAY;'),
    'no-restart-reset': ('    logic_play.n=0;logic_play.delays=0;logic_play.overflow=0;\n', '    logic_play.delays=0;logic_play.overflow=0;\n'),
    'ignore-ticks': ('else logic_play.q[j].left-=(int32_t)ticks;', 'else logic_play.q[j].left-=1;'),
    'seconds-not-tenths': ('return (int32_t)((tenths*tps+5u)/10u);', 'return (int32_t)(tenths*tps);'),
    'no-list-restore': ('        for(k=0;k<LOGIC_SLOTS;k++) LOGIC_H(op+LOGIC_RT_ACT_NODE+2u*k)=saved[i][k];\n        LOGIC_H(op+LOGIC_RT_ACT_COUNT)=count_saved[i];\n',
                        '        LOGIC_H(op+LOGIC_RT_ACT_COUNT)=count_saved[i];\n'),
    'no-flag-restore': ('    for(i=0;i<count;i++) LOGIC_B(ops+i*LOGIC_RT_OP+LOGIC_RT_FLAGS)=flags[i];\n', ''),
    'release-all-flags': ('LOGIC_B(f)=(uint8_t)(flags[i]&0xFEu);', ';'),
    'overflow-drops': ('            LOGIC_H(op+LOGIC_RT_ACT_NODE+2u*m)=saved[i][k];m++;\n',
                       '            if(!d) { LOGIC_H(op+LOGIC_RT_ACT_NODE+2u*m)=saved[i][k];m++; }\n'),
    'no-pass-cap': ('for(j=0;j<logic_play.n && n<LOGIC_SLOTS;j++)', 'for(j=0;j<logic_play.n;j++)'),
    'queue-unbounded': ('if(d && logic_play.n<LOGIC_QUEUE) {', 'if(d && logic_play.n<LOGIC_QUEUE+1u) {'),
    'past-count': ('(i=(p-base)/LOGIC_ED_NODE)>=n) return -1;', '(i=(p-base)/LOGIC_ED_NODE)>=LOGIC_ED_NODES_CAP) return -1;'),
    'trigger-row': ('return WORD_AT(p)==LOGIC_KIND_ACTION?(int)i:-1;', 'return (int)i;'),
    'any-screen': ('if(r!=LOGIC_ROWS_AT || !LOGIC_LIMITS_ON()', 'if(!LOGIC_LIMITS_ON()'),
    'no-limits-gate': ('if(r!=LOGIC_ROWS_AT || !LOGIC_LIMITS_ON() ||', 'if(r!=LOGIC_ROWS_AT ||'),
    'first-op-only': ('if(WORD_AT(op+LOGIC_ED_ACT_NODE+4u*k)==node) WORD_AT(op+LOGIC_ED_DELAY+4u*k)=tenths;',
                      'if(WORD_AT(op+LOGIC_ED_ACT_NODE+4u*k)==node) { WORD_AT(op+LOGIC_ED_DELAY+4u*k)=tenths;return; }'),
    'done-closes': ('    MAPMAKER_GUEST_RET();                       /* sub_00328BF0: ret; the screen stays open */\n',
                    '    original();\n'),
    'stale-label': ('    logic_label(e,(uint32_t)node);              /* the open screen shows the new text */\n', ''),
    'step-skip': ('static const uint16_t logic_steps[]={0,5,10,20,30,50,100,150,300,600};',
                  'static const uint16_t logic_steps[]={0,5,20,30,50,100,150,300,600};'),
    'picker-zeroes': ('        WORD_AT(op+LOGIC_ED_DELAY+4u*k)=d;\n', ''),
    'picker-no-other-op': ('d=j<before?delay[j]:logic_ed_delay(e,id,NULL,(int)op_i);', 'd=j<before?delay[j]:0u;'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = SOURCE.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / SOURCE.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('logic.exe' if os.name == 'nt' else 'logic')
    source = ROOT / 'tests/mapmaker/mapmaker_logic.c'
    include = ['/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src')]
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/WX', '/wd4996', *include, str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-D_POSIX_C_SOURCE=200809L',
                *['-I' + i[2:] for i in include], str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_logic(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_MAPMAKER')}
    result = subprocess.run([str(exe)], cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    out = result.stdout.replace('\r\n', '\n')
    if mutation == 'fixed':
        assert result.returncode == 0, out + result.stderr
        assert out == EXPECTED, out
    else:
        assert result.returncode != 0 or out != EXPECTED, f'{mutation} was not detected'


def test_logic_log_lines(tmp_path):
    """The play summary and the editor notes reach stderr (bounded)."""
    exe = build(tmp_path, 'fixed')
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_MAPMAKER')}
    err = subprocess.run([str(exe)], cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120).stderr
    assert '[mapmaker] logic: 3 operations, 5 actions, 3 with a delay (0.5 s..2 s)\n' in err
    assert '; not a Story map: the delays are ignored' in err
    assert 'more than 256 delayed actions waiting' in err and err.count('more than 256') == 1
    assert 'Delay row shown for action node 5' in err
    assert 'action node 11 is in no operation; Delay not stored' in err


# ---- the stock code the hooks rely on (private XBE) ---------------------------

def _decoded():
    from mapmaker_decoded import decoded
    return decoded()


def test_spare_op_word():
    """op +0x154 + 4k: written 0 only by sub_003208F0 (0x00320944) and moved with
    the slot by the delete fix-up (0x00322F6B); play copies it to runtime op
    +0x194 + 2k (0x0033AC14 through the lea at 0x0033ABF1) and no logic code
    reads runtime +0x194."""
    insns = _decoded()
    by = {i.address: i for i in insns}
    assert by[0x00320944].function == 0x003208F0 and by[0x00320944].hex.startswith('c78491540100000000')
    assert by[0x00322F6B].function == 0x00322D40 and '0xc8' in by[0x00322F6B].text
    assert by[0x00322F65].function == 0x00322D40 and '0xcc' in by[0x00322F65].text
    assert by[0x0033ABF1].function == 0x0033AA80 and by[0x0033ABF1].text.endswith('0x194]')
    assert by[0x0033AC14].function == 0x0033AA80 and by[0x0033AC14].mnemonic == 'mov'
    readers = [i for i in insns if 0x00338000 <= i.address < 0x00342000 and any(v == 0x194 for _, v in i.values())]
    assert [i.address for i in readers] == [0x0033ABF1], [hex(i.address) for i in readers]


def test_hook_sites():
    insns = _decoded()
    by = {i.address: i for i in insns}
    edit = by[0x00329B5C]                                  # the edit screen's last spacing before Done
    assert edit.function == 0x00328D20 and edit.call and edit.text == '0x784a0'
    assert 0x00329B5C + len(bytes.fromhex(edit.hex)) == 0x00329B61
    done = [i for i in insns if i.function == 0x00328D20 and ('imm', 0x00328BF0) in i.values()]
    assert {i.address for i in done} == {0x00328D78, 0x00329B6B}
    calls = {(i.function, i.text) for i in insns if i.call}
    assert (0x0033D1B0, '0x33cf80') in calls             # the logic tick fires the actions
    assert (0x00340A60, '0x33aa80') in calls             # level start/restart builds the logic
    assert (0x003282A0, '0x324b60') in calls             # the action picker's rebuild
    assert (0x00324B60, '0x3208f0') in calls
    rets = [i for i in insns if i.function in (0x0033CF80, 0x0033AA80, 0x00328BF0, 0x00324B60) and i.mnemonic == 'ret']
    assert rets and all(not i.text for i in rets)        # plain ret: a replaced call pops 4


# ---- the shipped sample maps (private game data) -------------------------------

def _samples():
    from mapmaker_decoded import _private
    pak = _private() / 'game/pak/arcade/l_102.pak'
    if not pak.is_file():
        pytest.skip('needs the private game data')
    data = pak.read_bytes()
    out = []
    for m in re.finditer(struct.pack('<I', map_report.MAGIC), data):
        body = data[m.start():m.start() + 0x80000]
        out.append(map_report.scan(body, len(body), map_report.Report()))
    return out


def test_sample_maps_have_no_delay():
    maps = _samples()
    assert len(maps) == 8
    slots = 0
    for m in maps:
        for op in m['ops']:
            n = struct.unpack_from('<i', op, 0x88)[0]
            assert 0 <= n <= map_report.OP_SLOTS
            for k in range(n):
                assert struct.unpack_from('<I', op, 0x154 + 4 * k)[0] == 0
                slots += 1
    assert slots == 140


def test_sample_logic_printout():
    lines = {m['name']: map_report.logic_lines(m) for m in _samples()}
    assert lines['A Little Head?'][2] == ('operation 3 "Wave 1": when Wave 1 start timer Reaches 0m 05 seconds -> '
                                          'Spawn New Enemy 1, Spawn New Enemy 2, Spawn New Enemy 3, Spawn New Enemy 4, '
                                          'Change Tile Lights 1, Display Lights')
    assert lines['location'][0].startswith('operation 1 "front lookouts": when Enemy 1 Spots player or Enemy 2 Spots player')
    assert ' and Next Checkpoint Equals 1 -> ' in lines['FINAL: Race'][0]


def test_offline_delay_text_and_summary():
    assert [map_report.delay_text(t) for t in (0, 5, 10, 150, 600, 1200, 25)] == \
        ['none', '0.5 s', '1 s', '15 s', '1 min', '2 min', '2.5 s']
    node = bytearray(0xC0)
    struct.pack_into('<iii', node, 0, 2, 11, 10)
    node[0x20:0x2D] = b'Unlock Door C'
    head = bytearray(0xC0)
    struct.pack_into('<i', head, 0xA4, 0)
    head[0x20:0x26] = b'doors!'
    trig = bytearray(0xC0)
    struct.pack_into('<iii', trig, 0, 1, 0, 3)
    trig[0x20:0x2A] = b'Switch A F'
    op = bytearray(0x220)
    struct.pack_into('<ii', op, 0x50, 1, 2)
    op[0x7C] = 1
    struct.pack_into('<i', op, 0x88, 1)
    struct.pack_into('<i', op, 0x8C, 0)
    struct.pack_into('<I', op, 0x154, 50)
    m = {'nodes': [bytes(node), bytes(head), bytes(trig)], 'ops': [bytes(op)]}
    assert map_report.logic_lines(m) == ['operation 1 "doors!": when NOT Switch A F -> Unlock Door C (after 5 s)']


# ---- wiring --------------------------------------------------------------------

HOOKS = ('setup', 'fire', 'rows', 'done', 'actions')


def test_wired_and_documented():
    header = (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    guest = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    source = SOURCE.read_text(encoding='utf-8')
    guide = (ROOT / 'docs/agent-context/limitless-logic.md').read_text(encoding='utf-8')
    for name in HOOKS:
        assert f'void pc_mapmaker_logic_{name}(void (*original)(void));' in header
        assert f'void pc_mapmaker_logic_{name}(void (*original)(void)) {{ original(); }}' in guest
        assert f'void pc_mapmaker_logic_{name}(void (*original)(void))\n' in source
    assert guest.index('#include "pc_mapmaker_limits.inc"') < guest.index('#include "pc_mapmaker_logic.inc"')
    for line in ('WRAP sub_0033AA80 -> pc_mapmaker_logic_setup', 'WRAP sub_0033CF80 -> pc_mapmaker_logic_fire',
                 'WRAP sub_000784A0 -> pc_mapmaker_logic_rows', 'WRAP sub_00328BF0 -> pc_mapmaker_logic_done',
                 'WRAP sub_00324B60 -> pc_mapmaker_logic_actions'):
        assert line in guide
