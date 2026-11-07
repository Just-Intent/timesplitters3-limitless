"""MapMaker AI navigation pads: Layout C handle table, ids past 8000, lazy group
distances and the A*/cover bound checks.

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
    'cover-range-short':('#define MAPMAKER_PADS_COVER_RANGE 14336u', '#define MAPMAKER_PADS_COVER_RANGE 14080u'),
    # Ids past 8000 (pc_mapmaker_pads.inc "pad ids past 8000").
    'id-high-overlaps-cover': ('#define MAPMAKER_PADS_HIGH_ID (MAPMAKER_PADS_COVER_ID+MAPMAKER_PADS_COVER_RANGE) /* 0x5B29 */',
                               '#define MAPMAKER_PADS_HIGH_ID (MAPMAKER_PADS_COVER_ID+MAPMAKER_PADS_COVER_RANGE-1u)'),
    'id-low-pads-moved': ('#define MAPMAKER_PADS_LOW_PADS 8000u', '#define MAPMAKER_PADS_LOW_PADS 7999u'),
    'search-off-by-one': ('        mid=lo+((hi-lo)>>1);', '        mid=lo+((hi-lo+1)>>1);'),
    'search-no-minus-one': ('    if(mid==-1) return -1;\n', ''),
    'lookup-low-ids-too': ("    if(id<(int32_t)MAPMAKER_PADS_HIGH_ID || id>=(int32_t)MAPMAKER_PADS_HIGH_END) { original(); return; }\n    mapmaker_pads.high_lookups++;\n    g_eax=(uint32_t)mapmaker_pads_search(id);",
                           "    if(id<(int32_t)MAPMAKER_PADS_REGULAR_ID) { original(); return; }\n    mapmaker_pads.high_lookups++;\n    g_eax=(uint32_t)mapmaker_pads_search(id);"),
    'pointer-no-scratch': ('        if(mapmaker_pads_owner_ret(ret)) p=mapmaker_pads_scratch(id);\n', ''),
    'pointer-scratch-everyone': ('        if(mapmaker_pads_owner_ret(ret)) p=mapmaker_pads_scratch(id);', '        p=mapmaker_pads_scratch(id);'),
    'pointer-owner-index-kept': ('            g_eax=mapmaker_pads_scratch((int32_t)(MAPMAKER_PADS_REGULAR_ID+(uint32_t)g));', '            (void)g;'),
    'pointer-alias-ignored': ('        id=(int32_t)mapmaker_pads.alias_id[arg&0x7FFFFFFFu];', '        (void)0;'),
    'runtime-no-fix': ('    WORD_AT(pad+0x18u)=h<0?0u:WORD_AT(WORD_AT(0x00715F2Cu)+(uint32_t)h*32u+0x18u);', '    (void)h;'),
    'tile-no-cover-owner': ('            WORD_AT(p)=(w&0xFFFF0000u)|mapmaker_pads_id((uint32_t)owner-MAPMAKER_PADS_REGULAR_ID);', '            (void)w;'),
    'tile-cover-word-clobbered': ('            WORD_AT(p)=(w&0xFFFF0000u)|mapmaker_pads_id((uint32_t)owner-MAPMAKER_PADS_REGULAR_ID);',
                                  '            WORD_AT(p)=mapmaker_pads_id((uint32_t)owner-MAPMAKER_PADS_REGULAR_ID);'),
    'tile-renumbers-low': ('    for(i=rb>MAPMAKER_PADS_LOW_PADS?rb:MAPMAKER_PADS_LOW_PADS;i<ra && i<MAPMAKER_POOL_PADS;i++)',
                           '    for(i=rb;i<ra && i<MAPMAKER_POOL_PADS;i++)'),
    'editor-groups-mapped': ('    mapmaker_pads.linear++;\n    original();\n    mapmaker_pads.linear--;\n}\n/* WRAP sub_00334B20',
                             '    original();\n}\n/* WRAP sub_00334B20'),
    'item-pads-not-mapped': ('            WORD_AT(out+k*4u)=mapmaker_pads_id(v-MAPMAKER_PADS_REGULAR_ID);', '            (void)v;'),
    'item-pads-nested-mapped': ('    if(mapmaker_pads.linear>0) return;\n    n=g_eax&0xFFu;', '    n=g_eax&0xFFu;'),
    'topup-no-alias': ('            WORD_AT(r+4u)=0x80000000u|aliased;\n', ''),
    'topup-not-restored': ('    for(k=0;k<aliased;k++) WORD_AT(rec[k])=mapmaker_pads.alias_id[k];\n', ''),
    'stitch-high-kept': ('                w=(w&~(0xFFFFu<<(16*k)))|((MAPMAKER_PADS_REGULAR_ID+(uint32_t)g)<<(16*k));', '                (void)g;'),
    # Group distances.
    'matrix-always-dense': ('    return groups>MAPMAKER_PADS_MATRIX_DENSE;', '    return 0;'),
    'matrix-lazy-at-1024': ('    return groups>MAPMAKER_PADS_MATRIX_DENSE;', '    return groups>=MAPMAKER_PADS_MATRIX_DENSE;'),
    'matrix-live-flags': ('if(now) for(k=0;k<n;k++) { now[k]=WORD_AT(pool+k*20u);WORD_AT(pool+k*20u)=mapmaker_matrix.flags[k]; }',
                          'if(now) for(k=0;k<n;k++) { now[k]=WORD_AT(pool+k*20u); }'),
    'matrix-flags-not-restored': ('    if(now) { for(k=0;k<n;k++) WORD_AT(pool+k*20u)=now[k]; free(now); }', '    free(now);'),
    'matrix-costs-not-restored': ('    WORD_AT(MAPMAKER_PADS_COST_A)=keep_a;WORD_AT(MAPMAKER_PADS_COST_B)=keep_b;', '    (void)keep_a;(void)keep_b;'),
    'matrix-one-cost': ('v=b+a; }', 'v=b; (void)a; }'),
    'matrix-no-diagonal': ('    if(i==j) return 0.0f;                      /* the builder\'s diagonal */\n', ''),
    'matrix-no-cache': ('            if(!mapmaker_matrix.known[i][j]) {', '            if(1) {'),
    'matrix-swapped': ('    guest_call(sub_0009C550,2,i,out);\n    WORD_AT(r)=WORD_AT(WORD_AT(out));\n    guest_call(sub_0009C550,2,j,out);',
                       '    guest_call(sub_0009C550,2,j,out);\n    WORD_AT(r)=WORD_AT(WORD_AT(out));\n    guest_call(sub_0009C550,2,i,out);'),
    'matrix-char-passed': ('    result=(int32_t)guest_call(sub_001596F0,3,0u,r,0u);', '    result=(int32_t)guest_call(sub_001596F0,3,1u,r,0u);'),
    'matrix-negative-read': ('    if(i>=n || j>=n || i<0 || j<0) v=FLOAT_AT(MAPMAKER_PADS_NO_ENTRY);', '    if(i>=n || j>=n) v=FLOAT_AT(MAPMAKER_PADS_NO_ENTRY);'),
    'matrix-stale-level': ("       WORD_AT(0x00715F30u)!=mapmaker_matrix.link_pool || WORD_AT(MAPMAKER_PADS_GROUP_COUNT)!=mapmaker_matrix.groups) {",
                           "       WORD_AT(0x00715F30u)!=mapmaker_matrix.link_pool) {"),
    'matrix-no-fp-push': ('    MAPMAKER_FP_PUSH(v);\n    MAPMAKER_GUEST_RET();\n}', '    (void)v;\n    MAPMAKER_GUEST_RET();\n}'),
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

OLD_RUNTIME, NEW_RUNTIME, COVER_BASE, NEW_COVER_BASE, RUNTIME_SLOTS = 0x209E, 0x7800, 0x1F40, 0x4000, 20
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
# Cover handle base 0x1F40 -> 0x4000 (pads-coverbase-*): compares, adds, the
# -0x1F40 conversions and the two folded cover[h - base].owner reads.
COVER_REWRITE = {
    COVER_BASE: NEW_COVER_BASE,
    (-COVER_BASE) & 0xFFFFFFFF: (-NEW_COVER_BASE) & 0xFFFFFFFF,
    (-COVER_BASE * 32 + 8) & 0xFFFFFFFF: (-NEW_COVER_BASE * 32 + 8) & 0xFFFFFFFF,
}
COVER_COUNTS = {COVER_BASE: 27, (-COVER_BASE) & 0xFFFFFFFF: 16, (-COVER_BASE * 32 + 8) & 0xFFFFFFFF: 2}
KEPT = {0x002B0363}            # movsx eax,[pool-0x413D8]: does not depend on the handle
NOT_PAD_HANDLE = {0x003BC6EC}  # cmp eax,0x1f40 on a big-endian network port (sub_003BC6D0)
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
    layout = [e for e in table['patches'] if e['id'].startswith('pads-layoutc-')]
    counts = {}
    for e in layout:
        assert e['kind'] == 'code'
        old, new = bytes.fromhex(e['original_hex']), bytes.fromhex(e['new_hex'])
        assert len(old) == len(new)
        hits = [v for v in REWRITE if old.count(v.to_bytes(4, 'little')) == 1]
        assert len(hits) == 1, e['id']
        v = hits[0]
        assert new == old.replace(v.to_bytes(4, 'little'), REWRITE[v].to_bytes(4, 'little')), e['id']
        assert e['id'] == f"pads-layoutc-{int(e['address'], 16):08x}"
        assert int(e['address'], 16) not in KEPT
        counts[v] = counts.get(v, 0) + 1
    assert counts == EXPECTED_COUNTS
    assert len({e['address'] for e in layout}) == len(layout) == 88
    cover = [e for e in table['patches'] if e['id'].startswith('pads-coverbase-')]
    counts = {}
    for e in cover:
        assert e['kind'] == 'code' and e['id'] == f"pads-coverbase-{int(e['address'], 16):08x}"
        old, new = bytes.fromhex(e['original_hex']), bytes.fromhex(e['new_hex'])
        hits = [v for v in COVER_REWRITE if old.count(v.to_bytes(4, 'little')) == 1]
        assert len(hits) == 1, e['id']
        v = hits[0]
        assert new == old.replace(v.to_bytes(4, 'little'), COVER_REWRITE[v].to_bytes(4, 'little')), e['id']
        assert int(e['address'], 16) not in KEPT | ID_THRESHOLD_SITES | NOT_PAD_HANDLE
        counts[v] = counts.get(v, 0) + 1
    assert counts == COVER_COUNTS and len({e['address'] for e in cover}) == 45
    # the regular id validity bound of sub_0009A330
    regid = [e for e in table['patches'] if e['id'].startswith('pads-regid-')]
    assert [(e['address'], e['original_hex'], e['new_hex']) for e in regid] == [('0x0009A37E', '81f929230000', '81f9e97b0000')]
    assert c_hex('MAPMAKER_PADS_HIGH_END') == 0x7BE9
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
    # the handle ranges the C code assumes
    assert NEW_RUNTIME - NEW_COVER_BASE == c_define('MAPMAKER_PADS_COVER_RANGE')
    assert c_hex('MAPMAKER_PADS_COVER_BASE') == NEW_COVER_BASE and c_hex('MAPMAKER_PADS_RUNTIME_BASE') == NEW_RUNTIME
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
    'runtime-end-not-moved': lambda t: _set_new(t, '0x0009A469', '3d00780000'),
    'layout-b-runtime': lambda t: _set_new(t, '0x0009D488', '8d8740470000'),
    'cover-base-missing': lambda t: _drop(t, '0x0033EFC9'),
    'cover-base-kept': lambda t: _set_new(t, '0x0009A40B', '05401f0000'),
    'cover-fold-kept': lambda t: _set_new(t, '0x002B5A22', '0fbf94080818fcff'),
    'network-port-rewritten': lambda t: t['patches'].insert(0,
        {'id': 'pads-coverbase-003bc6ec', 'kind': 'code', 'function': '0x003BC6D0', 'address': '0x003BC6EC',
         'original_hex': '3d401f0000', 'new_hex': '3d00400000'}),
    'regid-missing': lambda t: _drop(t, '0x0009A37E'),
    'regid-short': lambda t: _set_new(t, '0x0009A37E', '81f9295b0000'),
    'kept-site-rewritten': lambda t: t['patches'].append(
        {'id': 'pads-layoutc-002b0363', 'kind': 'code', 'function': '0x002B0320', 'address': '0x002B0363',
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
              if e['id'].startswith('pads-layoutc-')}
    assert set(found) == set(layout) | KEPT
    assert all(found[a] == h for a, h in layout.items())
