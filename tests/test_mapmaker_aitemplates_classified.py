"""The guest sites src/pc_mapmaker_aitemplates.inc relies on, checked in the
lifter's decode of the real XBE (skipped without the private files).

- the edit screen sub_0031CB90 calls sub_000784A0 at 0x0031D07E and reads its
  close callback from [esp+0x20] right after it, for the "Done" row;
- the Story AI list opens menu table 0x0052E088, whose build is sub_0031D140,
  which passes sub_0031CAC0 as that close callback;
- before a back callback the menu engine stores menu+0x1E8 in menu+0x18C;
- the Story AI chunk writer (in sub_003006C0) and the editor loader (in
  sub_00301A90) never touch chunk bytes +0x32/+0x33, where the link is saved,
  and the writer stores the loop index at +0x22.
"""
import struct

from mapmaker_decoded import ROOT, _private, decoded


def by_address():
    return {i.address: i for i in decoded()}


def mem(i):
    return [op for op in i.ops if op[0] == 'mem']


def test_edit_screen_rows_site():
    a = by_address()
    call = a[0x0031D07E]
    assert call.call and call.function == 0x0031CB90 and '0x784a0' in call.text
    read = a[0x0031D083]
    assert read.mnemonic == 'mov' and mem(read) and mem(read)[0][1] == 'esp' and mem(read)[0][4] == 0x20
    done = a[0x0031D0A1]
    assert done.call and '0x79d60' in done.text
    assert a[0x0031D08C].mnemonic == 'push' and a[0x0031D08C].text == 'eax'      # the close callback


def test_list_opens_the_edit_screen_with_the_close_callback():
    a = by_address()
    assert a[0x0031BF48].function == 0x0031BF00 and a[0x0031BF48].ops == (('imm', 0x0052E088),)
    assert a[0x0031D146].function == 0x0031D140 and a[0x0031D146].ops == (('imm', 0x0031CAC0),)
    assert a[0x0031D14C].call and '0x31cb90' in a[0x0031D14C].text
    xbe = (_private() / 'game/default.xbe').read_bytes()
    base = struct.unpack_from('<I', xbe, 0x104)[0]
    count, headers = struct.unpack_from('<II', xbe, 0x11C)
    for k in range(count):
        _, va, vsize, raw, _ = struct.unpack_from('<5I', xbe, headers - base + 56 * k)
        if va <= 0x0052E088 < va + vsize:
            assert struct.unpack_from('<I', xbe, raw + 0x0052E088 - va)[0] == 0x0031D140
            break
    else:
        raise AssertionError('0x0052E088 not in a section')


def test_back_sets_the_highlighted_id_to_the_back_id():
    a = by_address()
    load, store = a[0x0007B1D7], a[0x0007B1DE]
    assert mem(load)[0][4] == 0x1E8 and mem(store)[0][4] == 0x18C
    assert a[0x0007B1E4].mnemonic == 'call'                                       # the back callback


def test_sai_chunk_bytes_0x32_unused():
    insns = decoded()
    writer = [i for i in insns if 0x003013E1 <= i.address < 0x003015E5]
    loader = [i for i in insns if 0x00302428 <= i.address < 0x0030267F]
    assert writer and loader
    for i in writer:
        for op in mem(i):
            if {op[1], op[2]} == {'ebx', 'esi'}:
                assert op[4] not in (0x32, 0x33), hex(i.address)
    for i in loader:
        for op in mem(i):
            if op[1] == 'ebx' and not op[2]:
                assert op[4] not in (0x32, 0x33), hex(i.address)
    index = [i for i in writer if i.address == 0x0030143F]
    assert index and mem(index[0])[0][4] == 0x22 and mem(index[0])[0][5] == 2
