"""Every pad-id site the high ids (regular pads 8000.., ids 0x5B29..0x7BE8) pass is handled.

Decoded with the lifter's decoder (tests/mapmaker/mapmaker_decoded.py). Each
cover-id constant 0x2329 and each regular-id base 0x3E9 in the pad and editor
code is either untouched by high ids or covered by a hook in
src/interface/pc_mapmaker_pads.inc; the hooks' return-address lists and
caller sets are the ones the code has.
"""
import re
from pathlib import Path

from mapmaker_decoded import decoded, reviewed

ROOT = Path(__file__).resolve().parents[2]
PADS = (ROOT / 'src/interface/pc_mapmaker_pads.inc').read_text(encoding='utf-8')

COVER_ID = reviewed("""
0006C3C1 cmp eax,0x2329 (sub_0006C1E0): >= takes sub_0009A3C0, which answers a high id its regular handle (cover lookup hook)
0009A34E cmp ecx,0x2329 (sub_0009A330): validity of a pad with the cover flag; regular pads never carry it
0009A37E cmp ecx,0x2329 (sub_0009A330): regular id upper bound, pads.json pads-regid-0009a37e -> 0x7BE9
0009AC14 add eax,0x2329 (sub_0009A7A0): cover id assignment, unchanged
0009CC45 cmp edi,0x2329 (sub_0009CC40): >= takes sub_0009A3C0 (cover lookup hook answers high ids)
0009CCAB cmp eax,0x2329 (sub_0009CCA0): hooked, high ids never reach it
0009CCCC cmp eax,0x2329 (sub_0009CCA0): hooked, high ids never reach it
0009D231 cmp eax,0x2329 (sub_0009CE00): story nav loader (scene != 100), no high ids
0009D253 cmp eax,0x2329 (sub_0009CE00): story nav loader
0009D341 cmp eax,0x2329 (sub_0009CE00): story nav loader
0009D363 cmp eax,0x2329 (sub_0009CE00): story nav loader
0009D437 cmp eax,0x2329 (sub_0009D3B0): owner decode, fixed after the call by the runtime pad hook
0009D459 cmp eax,0x2329 (sub_0009D3B0): owner decode, fixed after the call by the runtime pad hook
0023FA20 cmp edx,0x2329 (sub_0023F9D0): seed filter, high seeds aliased by the top-up hook
0023FAC9 add ebx,0x2329 (sub_0023F9D0): cover id assignment, unchanged
00240C76 mov [esp+0x5c],0x2329 (sub_00240C50): running first cover id of the per-piece array (cover ids unchanged)
0033230F lea eax,[ebx+ecx+0x2329] (sub_00331E50): the editor's cover id, unchanged
00332359 cmp esi,0x2329 (sub_00332330): the editor loader's decode of saved Story AI points, which are cover ids
003323B0 sub esi,0x2329 (sub_00332330): same
""")

# Regular-id base 0x3E9 in functions that use the pad pools or make editor pad ids.
REGULAR_ID = reviewed("""
0009A376 cmp ecx,0x3E9 (sub_0009A330): lower bound, unchanged
0009A8BE add eax,0x3E9 (sub_0009A7A0): regular id = 0x3E9 + index, renumbered from 8000 by the tile hook
0009AC4E sub ebp,0x3E9 (sub_0009A7A0): owner index of this tile's cover pads, read before the tile hook renumbers
0009CCA4 cmp eax,0x3E9 (sub_0009CCA0): hooked
0009D22A cmp eax,0x3E9 (sub_0009CE00): story nav loader
0009D33A cmp eax,0x3E9 (sub_0009CE00): story nav loader
0009D430 cmp eax,0x3E9 (sub_0009D3B0): owner decode lower bound
0009DCB9 sub ecx,0x3E9 (sub_0009DC70): stitch id -> index; the stitch hook turns high ids into 0x3E9 + index first
0009DCC4 sub esi,0x3E9 (sub_0009DC70): same
00240C6E mov [esp+0x64],0x3E9 (sub_00240C50): running first regular id per piece, stored in the per-piece array only (never read back)
002FA1B3 push 0x3E9 (sub_002FA150): local id for sub_00332490 inside the linear scope
002FA1CD sub edi,0x3E9 (sub_002FA150): base index inside the linear scope
002FA3F0 lea edx,[ebx+0x3E9] (sub_002FA150): local ids of the piece's own pads
00332420 sub esi,0x3E9 (sub_00332330): the editor decode's regular branch (saved points are cover ids)
0033254C lea eax,[eax+ebp+0x3E9] (sub_00332490): the editor's regular id, mapped by the editor id hook
00332DB9 push 0x3E9 (sub_00332D70): local id argument of sub_00332490
00333A36 cmp word [ecx+2],0x3E9 (sub_00333780): unsigned validity, high ids pass
00333E67 cmp ax,0x3E9 (sub_00333BA0): unsigned validity
00334070 cmp word [esi+2],0x3E9 (sub_00333BA0): unsigned validity
0033420F cmp word [edi+2],0x3E9 (sub_00333BA0): unsigned validity
003346E4 cmp word [eax],0x3E9 (sub_003346E0): unsigned validity
003346EB cmp word [eax+2],0x3E9 (sub_003346E0): unsigned validity
00334BC9 push 0x3E9 (sub_00334B20): local id argument, inside the item pads scope
00334D16 cmp word [esi],0x3E9 (sub_00334C70): unsigned validity
00334D1D cmp word [esi+2],0x3E9 (sub_00334C70): unsigned validity
0033B182 sub eax,0x3E9 (sub_0033B100): owner id -> index after sub_0009CCA0; the pointer hook answers 0x3E9 + index
""")

PAD_GLOBALS = {0x00715F2C, 0x00715F34, 0x00715F38, 0x007B6664, 0x007B65C0, 0x0078400C}


def _hooked_functions():
    return {0x0009A3C0, 0x0009CCA0, 0x0009D3B0, 0x0009A7A0, 0x00332490, 0x002FA150, 0x00334B20,
            0x0009DC70, 0x0023F9D0, 0x00275F70, 0x00275F40}


def test_cover_id_sites():
    found = {i.address: i for i in decoded()
             if not (i.branch or i.call) and any(v in (0x2329, 0x2328 + 1) for k, v in i.values())}
    assert set(found) == set(COVER_ID), sorted(hex(a) for a in set(found) ^ set(COVER_ID))


def test_regular_id_sites():
    insns = decoded()
    pad_functions = {i.function for i in insns if any(op[0] == 'mem' and not op[1] and not op[2] and op[4] in PAD_GLOBALS
                                                       for op in i.ops)}
    pad_functions |= {i.function for i in insns if i.call and any(op == ('imm', 0x00332490) for op in i.ops)}
    sites = {i.address for i in insns if not (i.branch or i.call) and any(v in (0x3E9, 0xFFFFFC17) for k, v in i.values())}
    found = {a for a in sites if next(i for i in insns if i.address == a).function in pad_functions}
    # every 0x3E9 of a pad or editor-id function is reviewed; every reviewed site is real
    assert found <= set(REGULAR_ID), sorted(hex(a) for a in found - set(REGULAR_ID))
    assert set(REGULAR_ID) <= sites, sorted(hex(a) for a in set(REGULAR_ID) - sites)


def _by_address():
    return {i.address: i for i in decoded()}


def test_owner_return_addresses():
    """The 13 threshold callers and sub_0033B100: call sub_0009CCA0, then movsx reg,[eax+8]."""
    by = _by_address()
    rets = [int(v, 16) for v in re.findall(r'0x([0-9A-F]{8})u', PADS.split('mapmaker_pads_owner_rets[13]={')[1].split('};')[0])]
    assert len(rets) == 13
    for r in rets + [0x0033B179]:
        call = by[r - 5]
        assert call.call and ('imm', 0x0009CCA0) in call.ops, hex(r)
        nxt = by[r]
        assert nxt.mnemonic == 'movsx' and nxt.text.endswith('word ptr [eax + 8]'), hex(r)
    assert by[0x0033B182].mnemonic == 'sub' and by[0x0033B182].text == 'eax, 0x3e9'
    # each threshold return follows one of the 13 id compares (pads-idthreshold-*)
    sites = [0x0023F300, 0x0023FDCF, 0x002411F6, 0x00241300, 0x002413D2, 0x002416BA, 0x00241853,
             0x00241F64, 0x00242035, 0x00242094, 0x00242107, 0x00339448, 0x00339597]
    for s, r in zip(sites, rets):
        assert 0 < r - s < 0x20, (hex(s), hex(r))


def test_hook_callers():
    insns = decoded()

    def callers(target):
        return {i.address for i in insns if i.call and ('imm', target) in i.ops}
    assert callers(0x0009A3C0) == {0x0006C3C9, 0x0009CC4E, 0x0009CCD4, 0x0009D25B, 0x0009D36B, 0x0009D461}
    assert callers(0x0009D3B0) == {0x000C2B8B}
    assert callers(0x00334B20) == {0x002FA323, 0x00300E0F}
    assert callers(0x002FA150) == {0x0030117E}
    assert callers(0x0009A7A0) == {0x00084FA9, 0x00085700}
    # sub_00332490's callers store the id they get (one word or the nested call) except the two scoped ones
    calls = callers(0x00332490)
    assert len(calls) == 14
    scoped = {0x002FA1B8, 0x00334BE0}
    by = {i.address: n for n, i in enumerate(insns)}
    for c in calls - scoped:
        after = insns[by[c] + 1:by[c] + 8]
        assert any(i.mnemonic == 'mov' and 'word ptr' in i.text and i.text.endswith(', ax') for i in after) or \
            any(i.call and ('imm', 0x00334C70) in i.ops for i in after), hex(c)


def test_tile_loader_sites():
    by = _by_address()
    assert by[0x0009A8BE].text == 'eax, 0x3e9' and by[0x0009A8C6].text == 'dword ptr [esi + ecx], eax'
    assert by[0x0009AC24].text == 'dx, word ptr [0x7b6664]' and by[0x0009AC2E].text == 'word ptr [esi + 8], dx'
    assert by[0x0009AC4E].text == 'ebp, 0x3e9'
    assert by[0x0009DCB9].text == 'ecx, 0x3e9' and by[0x0009DCC4].text == 'esi, 0x3e9'
    assert by[0x0023FA1C].text == 'edx, dword ptr [edx + ebx + 4]' and by[0x0023FA20].text == 'edx, 0x2329'
    # sub_00275F40, the matrix reader: fld matrix[i][j] or the default
    assert by[0x00275F63].mnemonic == 'fld' and by[0x00275F67].text == 'dword ptr [0x4760d0]'
    assert by[0x0027605B].text == 'dword ptr [0x715f44], xmm0' and by[0x00276099].text == 'xmm0, dword ptr [0x715f44]'
