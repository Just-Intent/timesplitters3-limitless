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


# ---- links: stitches counted from the catalogue (2026-11-07) ---------------
def _xbe_bytes():
    import sys
    import pytest
    from mapmaker_decoded import _private
    xbe = _private() / 'game/default.xbe'
    recomp = _private().parent / 'external/ts3FPrecomp'
    if not xbe.is_file() or not (recomp / 'tools/recomp/config.py').is_file():
        pytest.skip('needs the private XBE and external/ts3FPrecomp')
    sys.path.insert(0, str(recomp))
    from tools.recomp import config
    config.configure_from_xbe(str(xbe))
    data = xbe.read_bytes()

    def read(va, n=4):
        o = config.va_to_file_offset(va)
        return int.from_bytes(data[o:o + n], 'little')
    return read


def test_stitch_pipeline_contract():
    by = {i.address: i for i in decoded()}

    def insn(a, mnemonic, text):
        assert by[a].mnemonic == mnemonic and by[a].text == text, (hex(a), by[a].mnemonic, by[a].text)

    # sub_0009A7A0 drops a stitch pair whose reverse is already in the table
    insn(0x0009AEB4, 'mov', 'bp, word ptr [edx]')
    insn(0x0009AEB7, 'cmp', 'bp, word ptr [eax]')
    insn(0x0009AEBC, 'mov', 'bp, word ptr [edx + 2]')
    insn(0x0009AEC0, 'cmp', 'bp, word ptr [eax - 2]')
    # sub_0009DC70 appends one link per active entry, unbounded, then the teleporter links
    insn(0x0009DCEE, 'mov', 'dword ptr [0x7b6660], edx')
    insn(0x0009DD4E, 'call', '0x9cd00')
    # the stitcher's only caller (wrapped by pc_mapmaker_pads_stitch)
    callers = {i.address for i in decoded() if i.call and ('imm', 0x0009DC70) in i.ops}
    assert callers == {0x0009DEDD}, sorted(hex(a) for a in callers)
    # the catalogue connection count: 0x00529554 + tile' * 0x140 = row(tile) + 0x14
    insn(0x00311C0B, 'mov', 'eax, dword ptr [ebp + 0x529554]')


def test_catalogue_side_slots():
    rd = _xbe_bytes()
    slots = {}
    for n in range(1, 46):
        cat, row = 0x00529510 + (n - 1) * 0x140, 0x00529400 + n * 0x140
        assert rd(cat) == n and cat + 0x30 == row
        count = rd(row + 0x14)
        assert count == rd(cat + 0x44) and count <= 40
        entries = [(rd(row + 0x18 + k * 6, 1), rd(row + 0x18 + k * 6 + 4, 1)) for k in range(count)]
        slots[n] = sum(s for t, s in entries if 1 <= t <= 4)
        assert slots[n] <= 32                        # MAPMAKER_LINK_SLOTS_MAX: the level record's 32 entries
    assert (rd(0x00529400 + 17 * 0x140), rd(0x00529408 + 17 * 0x140), slots[17]) == (9, 29, 12)
    assert (rd(0x00529408 + 14 * 0x140), slots[14]) == (46, 24)
    assert (rd(0x00529408 + 42 * 0x140), slots[42]) == (46, 24)
    assert max(slots.values()) == 24
    # half of any tile's side slots stays within the stock table of 16 stitch entries per piece
    assert all((s + 1) // 2 <= 16 for s in slots.values())
