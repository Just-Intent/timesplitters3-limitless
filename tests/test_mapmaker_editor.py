"""Compile the MapMaker editor capacity policy (src/pc_mapmaker_editor.inc)
against fake guest memory, with mutation cases, and check the guest-code
table config/guest-code-patches/editor.json against the documented layout.

The table itself is verified against the real XBE by the lift (every entry
must apply, see external/ts3FPrecomp/tools/recomp/disasm.py); these checks
cover what the lift cannot know: that each rewrite moves its value by
exactly its array's delta and that the layout is consistent with the C side."""
import json
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]
EDITOR = ROOT / 'src/pc_mapmaker_editor.inc'
TABLE = ROOT / 'config/guest-code-patches/editor.json'

MUTATIONS = {
    'sai-limit-always': ('limit=mapmaker_limits.limits?MAPMAKER_EDITOR_SAI_LIMIT:MAPMAKER_EDITOR_SAI_STOCK;',
                         'limit=MAPMAKER_EDITOR_SAI_LIMIT;'),
    'sai-limit-stock': ('#define MAPMAKER_EDITOR_SAI_LIMIT 255u', '#define MAPMAKER_EDITOR_SAI_LIMIT 50u'),
    'sai-no-room-check': ('if((int32_t)count<(int32_t)limit && room) {', 'if((int32_t)count<(int32_t)limit) {'),
    'sai-message-stock': ('guest_call(sub_003C85D4,3,editor,guest_call(sub_0003D7B0,1,0xBB0u),limit);',
                          'guest_call(sub_003C85D4,3,editor,guest_call(sub_0003D7B0,1,0xBB0u),50u);'),
    'sai-colour-kept': ('if(WORD_AT(entry+0x60u)>=50u) WORD_AT(entry+0x60u)=(count-1u)%50u;', '(void)entry;'),
    'load-no-restore': ('    original();\n    mapmaker_unhide(h,n);', '    original();'),
    'load-off-by-one': ('++seen[i]>caps[i].capacity', '++seen[i]>caps[i].capacity+1u'),
    'load-sai-stock': ('{MAPMAKER_TAG_SAI,MAPMAKER_EDITOR_SAI_CAP,0,0},', '{MAPMAKER_TAG_SAI,50u,0,0},'),
    'level-slots': ('#define MAPMAKER_LEVEL_SAI_SLOTS 1024u', '#define MAPMAKER_LEVEL_SAI_SLOTS 1100u'),
    'malformed-hides': ('        if(sz>limit_end-c-8u) break;', '        if(sz>limit_end-c-8u) { ok=1;break; }'),
    'load-kind1-any': ('    {MAPMAKER_TAG_OP,MAPMAKER_KIND1_OPS,1,1},', '    {MAPMAKER_TAG_OP,MAPMAKER_KIND1_OPS,0,0},'),
    'load-double-count': ('for(i=0;i<ncaps && !hide;i++)', 'for(i=0;i<ncaps;i++)'),
    'load-nodes-stock': ('{MAPMAKER_TAG_NODE,MAPMAKER_EDITOR_NODES_CAP,0,0},', '{MAPMAKER_TAG_NODE,130u,0,0},'),
    'ops-always-raised': ('return mapmaker_limits.limits?MAPMAKER_EDITOR_OPS_CAP:MAPMAKER_OPS_STOCK;',
                          'return MAPMAKER_EDITOR_OPS_CAP;'),
    'objectives-stock': ('#define MAPMAKER_OBJECTIVES_LIMIT 40u', '#define MAPMAKER_OBJECTIVES_LIMIT 10u'),
    'objective-no-ops-check': ('       WORD_AT(editor+0x41E0u)>=MAPMAKER_EDITOR_OPS_CAP ||\n       WORD_AT(editor+0x81A4u)+MAPMAKER_LOGIC_HEADROOM>MAPMAKER_EDITOR_NODES_CAP ||\n       WORD_AT(editor+0xDD10u)+MAPMAKER_LOGIC_HEADROOM>MAPMAKER_EDITOR_RECORDS_CAP) {\n        g_eax=editor;',
                               '       0) {\n        g_eax=editor;'),
    'kinds-always': ('if(mapmaker_limits.limits && guest_writable(0x0052E8E8u,4)', 'if(guest_writable(0x0052E8E8u,4)'),
    'node-types-stock': ('    if(mapmaker_limits.limits) cap*=4u;', ''),
    'node-old-array': ('n+=WORD_AT(editor+MAPMAKER_EDITOR_NODES+i*0xB4u)==type;', 'n+=WORD_AT(editor+0x81A8u+i*0xB4u)==type;'),
    'room-any-caller': ('    if(i==sizeof mapmaker_logic_room_callers/sizeof mapmaker_logic_room_callers[0]) return;\n', ''),
    'room-no-headroom': ('#define MAPMAKER_LOGIC_HEADROOM 8u', '#define MAPMAKER_LOGIC_HEADROOM 0u'),
    'slot-no-guard': ('    if(count<MAPMAKER_LOGIC_SLOTS) { original();return; }', '    original();return;'),
    'free-shim-always': ('if(type>2u || !mapmaker_limits.limits ||', 'if(type>2u ||'),
    'free-shim-no-restore': ('    MAPMAKER_PUT16(count_addr,count+(word-shown));', ''),
    'free-move-old-array': ('uint32_t entry=g_edx+8u+MAPMAKER_FREE_DELTA+g_eax*12u,type;', 'uint32_t entry=g_edx+8u+g_eax*12u,type;'),
    'free-cap-x2': ('count>=4u*mapmaker_free_stock[type]', 'count>=2u*mapmaker_free_stock[type]'),
    'load-free-stock': ('{MAPMAKER_TAG_FREE,MAPMAKER_EDITOR_FREE_CAP,0,0},', '{MAPMAKER_TAG_FREE,170u,0,0},'),
    'replacing-hook-no-ret': ('static void mapmaker_ret(uint32_t n) { g_esp+=4u+n; }', 'static void mapmaker_ret(uint32_t n) { (void)n; }'),
    'replacing-hook-ret-8': ('static void mapmaker_ret(uint32_t n) { g_esp+=4u+n; }', 'static void mapmaker_ret(uint32_t n) { g_esp+=8u+n; }'),
    'room-ret-after-original': ('ret=MAPMAKER_STACK_OK(4)?WORD_AT(g_esp):0u;  /* before the original pops it */', 'ret=0u;'),
    'slot-no-dedup': ('        if((int32_t)WORD_AT(obj+0x13Cu+i*0x8Cu)==record) { original();return; } /* already has a slot */', ''),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = EDITOR.read_text(encoding='utf-8')
        assert old in text, mutation
        (tmp_path / EDITOR.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('editor.exe' if os.name == 'nt' else 'editor')
    source = ROOT / 'tests/mapmaker_editor.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_editor(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_MEMORY_SCALE')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker editor ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


# ---- guest-code table ------------------------------------------------------
# name: (old offset, stride, old count, new offset, new count)
LAYOUT = {
    'sai': (0x288, 0xE4, 50, 0x11950, 255),
    'ops': (0x41E4, 0x220, 30, 0x1FC70, 120),
    'nodes': (0x81A8, 0xB4, 130, 0x2FB70, 520),
    'recs': (0xDD14, 0x8C, 100, 0x46910, 400),
    'free': (0x3070, 12, 170, 0x543D0, 680),
    'clip': (0x3870, 12, 170, 0x563B0, 680),
}
CLIP_COUNTS = (0x3868, 0x5048)   # clipboard counts move so set+8+delta works for both sets
SIZE = 0x58390
# array references incl. folded ones; see docs/mapmaker-editor.md for the cross-checks
EXPECTED_SITES = {'sai': 136, 'ops': 96, 'nodes': 154, 'recs': 137, 'free': 95, 'clip': 5, 'clip-counts': 19}


def table():
    return json.loads(TABLE.read_text(encoding='utf-8'))['patches']


def le32(v):
    return (v & 0xFFFFFFFF).to_bytes(4, 'little').hex()


def test_layout_fits_and_does_not_overlap():
    spans = sorted((new, new + stride * count) for _, stride, _, new, count in LAYOUT.values())
    assert spans[0][0] >= 0x11950
    for (a, b), (c, d) in zip(spans, spans[1:]):
        assert b <= c
    assert spans[-1][1] <= SIZE and SIZE % 16 == 0
    text = EDITOR.read_text(encoding='utf-8')
    assert f'#define MAPMAKER_EDITOR_SIZE 0x{SIZE:X}u' in text
    assert f'#define MAPMAKER_EDITOR_SAI 0x{LAYOUT["sai"][3]:X}u' in text
    assert f'#define MAPMAKER_EDITOR_SAI_CAP {LAYOUT["sai"][4]}u' in text
    for name, macro in (('ops', 'OPS'), ('nodes', 'NODES'), ('recs', 'RECORDS')):
        assert f'#define MAPMAKER_EDITOR_{macro} 0x{LAYOUT[name][3]:X}u' in text
        assert f'#define MAPMAKER_EDITOR_{macro}_CAP {LAYOUT[name][4]}u' in text
    # the chunk writers over-read nodes by 12 and records by 4 bytes (reads only; the loaders copy 0xB4 / 0x8C)
    assert LAYOUT['nodes'][3] + 0xB4 * LAYOUT['nodes'][4] + 12 <= SIZE
    assert LAYOUT['recs'][3] + 0x8C * LAYOUT['recs'][4] + 4 <= SIZE


def test_allocation_matches_layout():
    by_id = {e['id']: e for e in table()}
    assert int(by_id['ed-alloc-size']['value'], 16) == SIZE
    assert by_id['ed-alloc-size']['original_hex'] == '6850190100'          # push 0x11950
    assert int(by_id['ed-alloc-zero']['value'], 16) == SIZE // 4
    assert by_id['ed-alloc-zero']['original_hex'] == 'b952460000'          # mov ecx, 0x4652


def delta(name):
    if name == 'clip-counts':
        return CLIP_COUNTS[1] - CLIP_COUNTS[0]
    return LAYOUT[name][3] - LAYOUT[name][0]


def test_free_sets_share_one_delta():
    # set functions reach entries as set + 8 + 12*i for both sets
    assert LAYOUT['free'][3] - 0x3070 == LAYOUT['clip'][3] - (CLIP_COUNTS[1] + 8)
    old_ops = (LAYOUT['ops'][0], LAYOUT['ops'][0] + LAYOUT['ops'][1] * LAYOUT['ops'][2])
    assert old_ops[0] <= CLIP_COUNTS[1] and CLIP_COUNTS[1] + 8 <= old_ops[1]   # vacated
    text = EDITOR.read_text(encoding='utf-8')
    assert f'#define MAPMAKER_EDITOR_FREE 0x{LAYOUT["free"][3]:X}u' in text
    assert f'#define MAPMAKER_EDITOR_CLIP 0x{LAYOUT["clip"][3]:X}u' in text
    assert f'#define MAPMAKER_EDITOR_CLIP_COUNTS 0x{CLIP_COUNTS[1]:X}u' in text
    rel = [e for e in table() if e.get('set_relative')]
    assert len(rel) == 4 and all(int(e['value'], 16) - int(e['old'], 16) == delta('free') for e in rel)


def test_array_rewrites_follow_the_layout():
    """Each rewrite moves its value by exactly its array's delta, the old value
    lies in (or a few elements below, for biased indices) the old array, and
    the old value is in the instruction bytes unless the access was folded."""
    counted = dict.fromkeys(EXPECTED_SITES, 0)
    for e in table():
        if 'array' not in e:
            continue
        name = e['array']
        old = int(e['old'], 16) if not e['old'].startswith('-') else -int(e['old'][1:], 16)
        new = int(e['value'], 16)
        d = delta(name)
        if name == 'clip-counts':
            lo, hi, stride = CLIP_COUNTS[0], CLIP_COUNTS[0] + 8, 1
        else:
            old_lo, stride, old_count, _, _ = LAYOUT[name]
            lo, hi = old_lo - 4 * stride, old_lo + stride * old_count
        if e.get('folded'):
            assert old == 0 and new == d, e['id']
        elif e.get('folded_scale'):
            assert (new - old) * e['folded_scale'] == d, e['id']
        elif e.get('set_relative'):
            assert new - old == d and old in (0, 8), e['id']
        else:
            sign = -1 if old < 0 else 1
            assert (old + sign * d) & 0xFFFFFFFF == new, e['id']
            assert lo <= abs(old) < hi, e['id']
            assert le32(old) in e['original_hex'], e['id']
        assert e['kind'] == 'operand'
        counted[name] += 1
    assert counted == EXPECTED_SITES
    ids = [e['id'] for e in table()]
    addresses = [e['address'] for e in table()]
    assert len(ids) == len(set(ids)) and len(addresses) == len(set(addresses))


def test_sai_sizes_and_stack_frames():
    by_id = {e['id']: e for e in table()}
    sizes = [e for e in table() if e['reason'].startswith('sai clear size')]
    assert len(sizes) == 2 and all(int(e['value'], 16) == 255 * 0xE4 // 4 for e in sizes)
    # sub_00240590: 50 slots at frame+0x1C; 1024 slots after the change
    assert int(by_id['ed-load-frame']['value'], 16) == 0x1C + 4 * 1024
    assert int(by_id['ed-load-arg']['value'], 16) == 0xF0 + 4 * (1024 - 50)
    assert all(int(by_id[f'ed-load-frame-{a}']['value'], 16) == 0x1C + 4 * 1024
               for a in ('00240b9b', '00240bb9', '00240bcd'))
    text = EDITOR.read_text(encoding='utf-8')
    assert '#define MAPMAKER_LEVEL_SAI_SLOTS 1024u' in text
    # sub_00323220: slots at frame+4 must hold every editor SAI
    assert (int(by_id['ed-sai-frame']['value'], 16) - 4) // 4 >= 255
    assert by_id['ed-sai-frame']['value'] == by_id['ed-sai-frame-end']['value']


def test_logic_sizes_and_frames():
    by_id = {e['id']: e for e in table()}
    for name, dwords in (('ops', 120 * 0x220 // 4), ('nodes', 520 * 0xB4 // 4), ('recs', 400 * 0x8C // 4)):
        sizes = [e for e in table() if e['reason'].startswith(f'{name} clear size')]
        assert len(sizes) == 1 and int(sizes[0]['value'], 16) == dwords
    slots = int(by_id['ed-match-frame']['value'], 16) // 4
    assert slots >= LAYOUT['ops'][4]                       # one slot per op at most
    assert all(by_id[f'ed-match-frame-{a}']['value'] == by_id['ed-match-frame']['value']
               for a in ('002fbe5c', '002fbe68'))
    assert int(by_id['ed-objective-limit']['value'], 16) == 40
    assert '#define MAPMAKER_OBJECTIVES_LIMIT 40u' in EDITOR.read_text(encoding='utf-8')
    folded = [e for e in table() if e.get('folded') and e['array'] == 'ops']
    assert len(folded) == 1 and folded[0]['address'] == '0x00326024'
