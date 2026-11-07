"""Round 3 limit raises, checked against the lifter's decode of the whole game.

- Story AI 511: every reference to the selection index [0x00762F94] (and its
  padding 0x00762F95..97) is rewritten by sai-index.json, the index's two
  helper functions have exactly the callers the widening relies on, and the
  table's new bytes only widen the access (byte -> word/dword).
- Items 500: the editor item node pool's size constants all live in the two
  functions items.json patches, the pool pointer [0x00784008] has no other
  users, and the four entries agree on one node count.
- Pieces 1023 (2026-11-07): the editor piece node pool's four size constants
  (0x400 nodes of 0x48 B) all live in the same two functions, the pool's
  pointer [0x00783FFC] and free count [0x00783FF8] have only the known users,
  and the four items.json entries agree on 2048 nodes.
"""
import json
from pathlib import Path

from mapmaker_decoded import decoded, patched

ROOT = Path(__file__).resolve().parents[2]
INDEX = 0x00762F94


def table(name):
    return json.loads((ROOT / f'config/guest-code-patches/{name}.json').read_text(encoding='utf-8'))['patches']


def absolute(i, addresses):
    return any(op[0] == 'mem' and not op[1] and not op[2] and op[4] in addresses for op in i.ops)


def test_every_index_reference_is_widened():
    refs = {i.address for i in decoded() if absolute(i, {INDEX, INDEX + 1, INDEX + 2, INDEX + 3})}
    assert refs == {0x0031BA80, 0x0031BA94, 0x0031BAC0, 0x0031BB30, 0x0031BF14, 0x0031CAC0, 0x0031CB91}
    assert not {i.address for i in decoded() if absolute(i, {INDEX + 1, INDEX + 2, INDEX + 3})}  # padding
    assert refs <= patched('sai-index')


def test_index_helpers_have_the_known_callers():
    calls = {}
    for i in decoded():
        for op in i.ops:
            if op[0] == 'imm' and op[1] in (0x0031BA80, 0x0031BA90, 0x0031BAA0, 0x0031BF00) and (i.call or i.mnemonic == 'push'):
                calls.setdefault(op[1], set()).add(i.address)
    assert calls[0x0031BA80] == {0x00336E60}                 # result -> sub_0031BAA0
    assert calls[0x0031BA90] == {0x003057E3}                 # push edi = dword [editor+0x2F20]
    assert calls[0x0031BAA0] == {0x00336DFD, 0x00336E66}     # count-1, the getter's eax
    assert calls[0x0031BF00] == {0x0031D773, 0x0031D7D0, 0x0031D945}  # row callbacks of the list


def test_index_rewrites_only_widen():
    widen = {('a0', 'a1'), ('a2', 'a3'), ('8a', '8b'), ('0fb6', '0fb7')}
    entries = table('sai-index')
    assert len(entries) == 11
    for e in entries:
        old, new = e['original_hex'], e['new_hex']
        assert e['kind'] == 'code' and len(old) == len(new)
        prefix = 4 if old.startswith('0f') else 2
        assert (old[:prefix], new[:prefix]) in widen and old[prefix:] == new[prefix:], e['id']
    by_address = {i.address: i for i in decoded()}
    for e in entries:
        assert by_address[int(e['address'], 16)].hex == e['original_hex'], e['id']


def test_item_pool_constants():
    pool, stride = 5300, 0x3C
    by_id = {e['id']: e for e in table('items')}
    le = lambda v: v.to_bytes(4, 'little').hex()
    assert by_id['mm-subentry-pool-bytes']['new_hex'] == '68' + le(pool * stride)
    assert by_id['mm-subentry-pool-loop']['new_hex'] == '3d' + le((pool - 1) * stride)
    assert by_id['mm-subentry-pool-end']['new_hex'] == '8990' + le((pool - 1) * stride + 0x14)
    assert by_id['mm-subentry-pool-count']['new_hex'] == 'c705c02c7600' + le(pool)
    limits = (ROOT / 'src/interface/pc_mapmaker_limits.inc').read_text(encoding='utf-8')
    assert '#define MAPMAKER_LIMIT_SUB 500u' in limits
    # 500 items, a clipboard copy of all of them and the 81 start points, twice
    assert pool >= 2 * (500 + 500 + 81)


def test_item_pool_has_no_other_users():
    pointer = {i.function for i in decoded() if absolute(i, {0x00784008})}
    assert pointer == {0x002FE430, 0x00302A60, 0x00307C20}   # clear, allocate, free-list init
    sizes = {(i.function, v) for i in decoded() if i.function in (0x00302A60, 0x00307C20)
             for kind, v in i.values() if v in (0x7C40, 0x7BFC, 0x7C10, 0x212)}
    sites = {i.address for i in decoded() if i.function in (0x00302A60, 0x00307C20)
             and any(v in (0x7C40, 0x7BFC, 0x7C10, 0x212) for _, v in i.values())}
    item_sites = {int(e['address'], 16) for e in table('items') if e['id'].startswith('mm-subentry-')}
    assert len(sizes) == 4 and sites == item_sites and item_sites <= patched('items')


PIECE_POOL = 2048


def test_piece_pool_constants():
    by_id = {e['id']: e for e in table('items')}
    le = lambda v: v.to_bytes(4, 'little').hex()
    n, stride = PIECE_POOL, 0x48
    assert by_id['mm-piece-pool-bytes']['new_hex'] == '68' + le(n * stride)
    assert by_id['mm-piece-pool-loop']['new_hex'] == '3d' + le((n - 1) * stride)
    assert by_id['mm-piece-pool-last']['new_hex'] == '8990' + le((n - 1) * stride + 0x40)
    assert by_id['mm-piece-pool-count']['new_hex'] == 'c705f83f7800' + le(n)
    # 1023 placed pieces (MAPMAKER_LIMIT_PIECES), a copy of every one of them and the new held piece
    limits = (ROOT / 'src/interface/pc_mapmaker_limits.inc').read_text(encoding='utf-8')
    assert '#define MAPMAKER_LIMIT_PIECES 1023u' in limits and n >= 2 * 1023 + 1


def test_piece_pool_has_no_other_users():
    base = {i.function for i in decoded() if absolute(i, {0x00783FFC})}
    count = {i.address for i in decoded() if absolute(i, {0x00783FF8})}
    assert {0x00302A60, 0x00307C20} <= base
    # set by the init, dec by the two allocators, inc by the free: never compared
    assert count == {0x00307C61, 0x00307EDE, 0x00307FEF, 0x00308540, 0x0030854C}, sorted(hex(a) for a in count)
    by_address = {i.address: i for i in decoded()}
    assert [by_address[a].mnemonic for a in sorted(count)] == ['mov', 'dec', 'dec', 'mov', 'mov']
    stock = (0x12000, 0x11FB8, 0x11FF8, 0x400)
    sites = {i.address for i in decoded() if i.function in (0x00302A60, 0x00307C20)
             and any(v in stock for _, v in i.values())}
    piece_sites = {int(e['address'], 16) for e in table('items') if e['id'].startswith('mm-piece-pool-')}
    assert sites == piece_sites, sorted(hex(a) for a in sites ^ piece_sites)
    by_address = {i.address: i for i in decoded()}
    for e in table('items'):
        if e['id'].startswith('mm-piece-pool-'):
            assert by_address[int(e['address'], 16)].hex == e['original_hex'], e['id']
