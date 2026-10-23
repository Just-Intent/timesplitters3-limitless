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
    'load-off-by-one': ('++seen[i]>caps[i]', '++seen[i]>caps[i]+1u'),
    'load-sai-stock': ('{MAPMAKER_TAG_SAI,MAPMAKER_EDITOR_SAI_CAP},', '{MAPMAKER_TAG_SAI,50u},'),
    'level-slots': ('#define MAPMAKER_LEVEL_SAI_SLOTS 1024u', '#define MAPMAKER_LEVEL_SAI_SLOTS 1100u'),
    'malformed-hides': ('        if(sz>limit_end-c-8u) break;', '        if(sz>limit_end-c-8u) { ok=1;break; }'),
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
}
SIZE = 0x58390
EXPECTED_SITES = {'sai': 136}   # array references incl. the folded one (tracker + report agree)


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


def test_allocation_matches_layout():
    by_id = {e['id']: e for e in table()}
    assert int(by_id['ed-alloc-size']['value'], 16) == SIZE
    assert by_id['ed-alloc-size']['original_hex'] == '6850190100'          # push 0x11950
    assert int(by_id['ed-alloc-zero']['value'], 16) == SIZE // 4
    assert by_id['ed-alloc-zero']['original_hex'] == 'b952460000'          # mov ecx, 0x4652


def test_array_rewrites_follow_the_layout():
    """Each rewrite moves its value by exactly its array's delta, the old value
    lies in (or a few elements below, for biased indices) the old array, and
    the old value is in the instruction bytes unless the access was folded."""
    counted = dict.fromkeys(LAYOUT, 0)
    for e in table():
        if 'array' not in e:
            continue
        name = e['array']
        old_lo, stride, old_count, new_lo, _ = LAYOUT[name]
        old = int(e['old'], 16) if not e['old'].startswith('-') else -int(e['old'][1:], 16)
        new = int(e['value'], 16)
        delta = new_lo - old_lo
        if e.get('folded'):
            assert old == 0 and new == delta, e['id']
        else:
            sign = -1 if old < 0 else 1
            assert (old + sign * delta) & 0xFFFFFFFF == new, e['id']
            assert old_lo - 4 * stride <= abs(old) < old_lo + stride * old_count, e['id']
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
