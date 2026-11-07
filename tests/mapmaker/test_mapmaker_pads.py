"""MapMaker AI navigation pads: Layout B handle table and A*/cover bound checks.

Compiles src/interface/pc_mapmaker_pads.inc against fake guest memory (with mutations
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

ROOT = Path(__file__).resolve().parents[2]
PADS = ROOT / 'src/interface/pc_mapmaker_pads.inc'
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
    # Stitcher: cut the table only where the next link would pass the pool.
    'stitch-unguarded': ('        WORD_AT(0x007B677Cu)=i;\n', ''),
    'stitch-counts-inactive': ('        if(!ok) continue;\n', ''),
    'stitch-off-by-one': ('        if(added==room) break;', '        if(added+1u>=room) break;'),
    'stitch-any-scene': ("    if(WORD_AT(0x0079094Cu)!=100u) { original(); return; }\n    mapmaker_limits_read();\n    if(!mapmaker_limits.pools)",
                         "    mapmaker_limits_read();\n    if(!mapmaker_limits.pools)"),
    'stitch-without-pools': ('    if(!mapmaker_limits.pools) { original(); return; }\n    n=', '    n='),
    'stitch-editor-limit': ('room=links<MAPMAKER_POOL_LINKS?MAPMAKER_POOL_LINKS-links:0u;',
                            'room=links<MAPMAKER_LIMIT_LINKS?MAPMAKER_LIMIT_LINKS-links:0u;'),
    'cover-range-short':('#define MAPMAKER_PADS_COVER_RANGE 10240u', '#define MAPMAKER_PADS_COVER_RANGE 9984u'),
    # Regular pad ids past the stock threshold need the pads-idthreshold rewrites (compile-time check).
    'id-threshold-stock': ('#define MAPMAKER_PADS_ID_THRESHOLD 0x2328u', '#define MAPMAKER_PADS_ID_THRESHOLD 0x1F40u'),
    'id-threshold-overlaps-cover': ('#define MAPMAKER_PADS_ID_THRESHOLD 0x2328u', '#define MAPMAKER_PADS_ID_THRESHOLD 0x2329u'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = PADS.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / PADS.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('pads.exe' if os.name == 'nt' else 'pads')
    source = ROOT / 'tests/mapmaker/mapmaker_pads.c'
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
# The 13 "pad id above 8000 is a cover id" compares (cmp reg,0x1f40; jle; call
# sub_0009CCA0; movsx [pad+8]): moved to the last regular id 0x2328.
ID_THRESHOLD_SITES = {0x0023F300, 0x0023FDCF, 0x002411F6, 0x00241300, 0x002413D2, 0x002416BA, 0x00241853,
                      0x00241F64, 0x00242035, 0x00242094, 0x00242107, 0x00339448, 0x00339597}


def c_define(name):
    m = re.search(rf'#define {name} (\d+)u', PADS.read_text(encoding='utf-8'))
    assert m, name
    return int(m.group(1))


def c_hex(name):
    m = re.search(rf'#define {name} 0x([0-9A-F]+)u', PADS.read_text(encoding='utf-8'))
    assert m, name
    return int(m.group(1), 16)


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
    # pad id thresholds: exactly the 13 sites, cmp reg,0x1f40 -> cmp reg,0x2328
    thresholds = [e for e in table['patches'] if e['id'].startswith('pads-idthreshold-')]
    threshold = c_hex('MAPMAKER_PADS_ID_THRESHOLD')
    assert threshold == 0x2329 - 1
    assert {int(e['address'], 16) for e in thresholds} == ID_THRESHOLD_SITES == {
        int(e['address'], 16) for e in thresholds if e['id'] == f"pads-idthreshold-{int(e['address'], 16):08x}"}
    for e in thresholds:
        old, new = bytes.fromhex(e['original_hex']), bytes.fromhex(e['new_hex'])
        assert old[:-4] in (b'\x3d', b'\x81\xf9', b'\x81\xfd', b'\x81\xfe') and old[-4:] == COVER_BASE.to_bytes(4, 'little')
        assert new == old[:-4] + threshold.to_bytes(4, 'little'), e['id']
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
    'threshold-missing': lambda t: _drop(t, '0x00242107'),
    'threshold-into-cover': lambda t: _set_new(t, '0x0023F300', '3d29230000'),
    'threshold-handle-site': lambda t: t['patches'].insert(0,
        {'id': 'pads-idthreshold-0009d5fe', 'kind': 'code', 'function': '0x0009D5E0', 'address': '0x0009D5FE',
         'original_hex': '81f9401f0000', 'new_hex': '81f928230000'}),
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


def test_id_thresholds_against_listing():
    """Each threshold site is the id test before the owner lookup: cmp reg,0x1f40;
    jle within two instructions; then call sub_0009CCA0 and movsx [eax+8]."""
    listing = _listing()
    if listing is None:
        pytest.skip('disassembly listing not available')
    line = re.compile(r'^\s+0x([0-9A-F]{8})\s+([0-9a-f]+)\s+(\S+)\s*(.*?)\s*$')
    rows = []
    with listing.open(encoding='utf-8', errors='replace') as fh:
        for text in fh:
            m = line.match(text)
            if m:
                rows.append((int(m.group(1), 16), m.group(3), m.group(4)))
    index = {a: i for i, (a, _, _) in enumerate(rows)}
    table = json.loads(TABLE.read_text(encoding='utf-8'))
    for e in table['patches']:
        if not e['id'].startswith('pads-idthreshold-'):
            continue
        i = index[int(e['address'], 16)]
        assert rows[i][1] == 'cmp' and rows[i][2].endswith('0x1f40'), e['id']
        after = rows[i + 1:i + 6]
        mnemonics = [m for _, m, _ in after]
        assert 'jle' in mnemonics[:3], e['id']
        j = mnemonics.index('jle')
        assert any(m == 'call' and '0x9cca0' in o for _, m, o in after[j:]), e['id']
        assert any(m == 'movsx' and '+ 8]' in o for _, m, o in rows[i + 1:i + 8]), e['id']


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
