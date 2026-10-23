"""MapMaker AI navigation pads: Layout B handle table and A*/cover bound checks.

Compiles src/pc_mapmaker_pads.inc against fake guest memory (with mutations
that must be caught), and checks config/guest-code-patches/pads.json for
internal consistency and against the C constants. When the disassembly
listing is available (a checkout with private/pipeline), it also checks that
the table covers every instruction using a runtime-handle constant.
"""
import copy
import json
import os
import re
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]
PADS = ROOT / 'src/pc_mapmaker_pads.inc'
TABLE = ROOT / 'config/guest-code-patches/pads.json'

MUTATIONS = {
    'group-ignores-free-list': ('return (int32_t)WORD_AT(free_head)<0 && WORD_AT(counter)>=capacity;',
                                'return WORD_AT(counter)>=capacity;'),
    'group-off-by-one': ('WORD_AT(counter)>=capacity;', 'WORD_AT(counter)>capacity;'),
    'group-stock-capacity': ('#define MAPMAKER_PADS_GROUP_NODES 8192u', '#define MAPMAKER_PADS_GROUP_NODES 1000u'),
    'pad-unguarded': ('    if(mapmaker_pads_pool_full(0x00712044u,0x00715F20u,MAPMAKER_PADS_PAD_NODES)) {',
                      '    if(0) {'),
    'pad-wrong-counter': ('0x00712044u,0x00715F20u', '0x00712044u,0x00711FF8u'),
    'topup-any-scene': ("    if(WORD_AT(0x0079094Cu)!=100u ||\n       !MAPMAKER_STACK_OK(12)",
                        "    if(!MAPMAKER_STACK_OK(12)"),
    'topup-ignores-n': ('add=(int32_t)n>0 && (int32_t)n<16?16u-n:0u;', 'add=16u;'),
    'topup-n-zero-adds': ('add=(int32_t)n>0 && (int32_t)n<16?16u-n:0u;', 'add=(int32_t)n<16?16u-n:0u;'),
    'topup-stock-cap': ('capacity=mapmaker_limits.pools?MAPMAKER_PADS_COVER_RANGE:MAPMAKER_PADS_STOCK_COVER;',
                        'capacity=MAPMAKER_PADS_STOCK_COVER;'),
    'topup-range-cap-without-pools': ('capacity=mapmaker_limits.pools?MAPMAKER_PADS_COVER_RANGE:MAPMAKER_PADS_STOCK_COVER;',
                                      'capacity=MAPMAKER_PADS_COVER_RANGE;'),
    'topup-wraps': ('if(count>capacity || add>capacity-count) {', 'if(add>capacity-count) {'),
    'cover-range-short': ('#define MAPMAKER_PADS_COVER_RANGE 10240u', '#define MAPMAKER_PADS_COVER_RANGE 9984u'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = PADS.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / PADS.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('pads.exe' if os.name == 'nt' else 'pads')
    source = ROOT / 'tests/mapmaker_pads.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    if mutation != 'fixed' and result.returncode != 0:
        return None                      # a size check refused it at compile time
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_pads(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    if exe is None:
        return
    env = {k: v for k, v in os.environ.items() if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker pads ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


# ---- the guest-code table --------------------------------------------------

OLD_RUNTIME, NEW_RUNTIME, COVER_BASE, RUNTIME_SLOTS = 0x209E, 0x4740, 0x1F40, 20
# constant in the original instruction -> constant it must become
REWRITE = {
    OLD_RUNTIME: NEW_RUNTIME,
    OLD_RUNTIME + RUNTIME_SLOTS: NEW_RUNTIME + RUNTIME_SLOTS,
    0x2440: NEW_RUNTIME,                                            # scene-100 cover end
    (-OLD_RUNTIME) & 0xFFFFFFFF: (-NEW_RUNTIME) & 0xFFFFFFFF,
    (-OLD_RUNTIME * 32 + 4) & 0xFFFFFFFF: (-NEW_RUNTIME * 32 + 4) & 0xFFFFFFFF,  # runtime[h].flags
}
EXPECTED_COUNTS = {OLD_RUNTIME: 51, OLD_RUNTIME + RUNTIME_SLOTS: 24, 0x2440: 1,
                   (-OLD_RUNTIME) & 0xFFFFFFFF: 11, (-OLD_RUNTIME * 32 + 4) & 0xFFFFFFFF: 1}
KEPT = {0x002B0363}            # movsx eax,[pool-0x413D8]: does not depend on the handle
EXT_START, EXT_END = 0x008E0000, 0x00900000


def c_define(name):
    m = re.search(rf'#define {name} (\d+)u', PADS.read_text(encoding='utf-8'))
    assert m, name
    return int(m.group(1))


def validate(table):
    layout = [e for e in table['patches'] if e['id'].startswith('pads-layoutb-')]
    counts = {}
    for e in layout:
        assert e['kind'] == 'code'
        old, new = bytes.fromhex(e['original_hex']), bytes.fromhex(e['new_hex'])
        assert len(old) == len(new)
        hits = [v for v in REWRITE if old.count(v.to_bytes(4, 'little')) == 1]
        assert len(hits) == 1, e['id']
        v = hits[0]
        assert new == old.replace(v.to_bytes(4, 'little'), REWRITE[v].to_bytes(4, 'little')), e['id']
        assert e['id'] == f"pads-layoutb-{int(e['address'], 16):08x}"
        assert int(e['address'], 16) not in KEPT
        counts[v] = counts.get(v, 0) + 1
    assert counts == EXPECTED_COUNTS
    assert len({e['address'] for e in layout}) == len(layout) == 88
    # the handle range the C bound checks assume
    assert NEW_RUNTIME - COVER_BASE == c_define('MAPMAKER_PADS_COVER_RANGE')
    # group A* pool relocation and the loop bound that makes it possible
    reloc = [e for e in table['patches'] if e['kind'] == 'relocate']
    assert len(reloc) == 1 and reloc[0]['id'] == 'pads-astar-group-pool'
    r = reloc[0]
    old, size, new, new_size = (int(r[k], 16) for k in ('old', 'size', 'new', 'new_size'))
    assert (old, size) == (0x0070E178, 0x3E80)
    assert EXT_START <= new and new + new_size <= EXT_END
    assert new_size % 16 == 0 and new_size // 16 == c_define('MAPMAKER_PADS_GROUP_NODES') <= 0x7FFF
    keeps = {a for a, how in r['sites'].items() if how == 'keep'}
    assert keeps == {'0x0009B0B9', '0x0009B0C2', '0x0009B576', '0x0009B666'}      # counter 0x00711FF8
    loop = [e for e in table['patches'] if e['id'] == 'pads-astar-loop-bound']
    assert len(loop) == 1 and loop[0]['original_hex'] == '3d40e27000'
    bound = int.from_bytes(bytes.fromhex(loop[0]['new_hex'])[1:], 'little')
    assert 0x0070DD30 < bound < old                                               # same 10 iterations
    return True


def test_table_consistent():
    assert validate(json.loads(TABLE.read_text(encoding='utf-8')))


def _drop(t, address):
    t['patches'] = [e for e in t['patches'] if e.get('address') != address]


def _set_new(t, address, new_hex):
    next(e for e in t['patches'] if e.get('address') == address)['new_hex'] = new_hex


TABLE_MUTATIONS = {
    'missing-site': lambda t: _drop(t, '0x0033EFE1'),
    'wrong-runtime-base': lambda t: _set_new(t, '0x0009D488', '8d8741470000'),
    'runtime-end-not-moved': lambda t: _set_new(t, '0x0009A469', '3d40470000'),
    'kept-site-rewritten': lambda t: t['patches'].append(
        {'id': 'pads-layoutb-002b0363', 'kind': 'code', 'function': '0x002B0320', 'address': '0x002B0363',
         'original_hex': '0fbf8028ecfbff', 'new_hex': '0fbf8018e8f7ff'}),
    'pool-too-big': lambda t: t['patches'][-1].update(new_size='0x40000'),
    'loop-bound-changes-loop': lambda t: _set_new(t, '0x00098540', '3d20dd7000'),
}


@pytest.mark.parametrize('mutation', TABLE_MUTATIONS)
def test_table_mutations(mutation):
    table = json.loads(TABLE.read_text(encoding='utf-8'))
    mutated = copy.deepcopy(table)
    TABLE_MUTATIONS[mutation](mutated)
    with pytest.raises(AssertionError):
        validate(mutated)


def _listing():
    for base in (ROOT, *ROOT.parents):
        p = base / 'private/pipeline/disasm/asm/text.asm'
        if p.is_file():
            return p
    return None


def test_table_complete_against_listing():
    listing = _listing()
    if listing is None:
        pytest.skip('disassembly listing not available')
    want = re.compile(r'(?<![0-9a-fx])(0x209e|0x20b2|0x2440|0xffffdf62|0x413bc|0x413d8)(?![0-9a-f])')
    line = re.compile(r'^\s+0x([0-9A-F]{8})\s+([0-9a-f]+)\s+(\S+)\s*(.*?)\s*$')
    found = {}
    with listing.open(encoding='utf-8', errors='replace') as fh:
        for text in fh:
            m = line.match(text)
            if not m or m.group(3).startswith('j') or m.group(3) == 'call':
                continue
            if want.search(m.group(4)):
                found[int(m.group(1), 16)] = m.group(2)
    table = json.loads(TABLE.read_text(encoding='utf-8'))
    layout = {int(e['address'], 16): e['original_hex'] for e in table['patches']
              if e['id'].startswith('pads-layoutb-')}
    assert set(found) == set(layout) | KEPT
    assert all(found[a] == h for a, h in layout.items())
