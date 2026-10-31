"""Every pad-handle constant in the game's code is patched (pads.json) or reviewed.

Layout B (docs/mapmaker/mapmaker-pads.md) moves the runtime pad handles 0x209E..0x20B1 to
0x4740..0x4753 and keeps the cover base 0x1F40. pads.json was first checked
against the text listing, which drops instructions whose byte column is elided;
this scan decodes all game code with the lifter's decoder instead, any length.
"""
from mapmaker_decoded import decoded, neg, patched, reviewed, unclassified

# Constants of the handle ranges: runtime base/end (0x209E/0x20B2, and the
# inclusive forms), the stock scene-100 cover end (0x2440/0x243F), the cover base
# 0x1F40, in every sign.
HANDLE = neg({0x209D, 0x209E, 0x20B1, 0x20B2, 0x243F, 0x2440, 0x1F40})


def folded(v):
    """A negative displacement folding a handle base into a pool index: -(base*stride +- field)."""
    if v < 0x80000000:
        return False
    d = 0x100000000 - v
    return any(abs(d - base * s) <= 0x40 for base in (0x209E, 0x20B2, 0x2440, 0x1F40) for s in range(2, 65))


# The site Layout B keeps on purpose (docs/mapmaker/mapmaker-pads.md "The one site kept").
KEPT = reviewed("""
002B0363 movsx eax,[eax-0x413D8]: eax was just reloaded from [0x715F38], so it is a fixed stray read before the runtime pool, not runtime[h].owner; rewriting would only move it
""")

# Cover handle base 0x1F40 and the pad-id threshold 0x1F40: Layout B keeps both
# (cover = 0x1F40 + index, ids > 8000 are cover ids). These decode/encode cover
# handles exactly as before.
COVER_BASE = reviewed("""
0009A40B add eax,0x1F40: cover index -> handle (sub_0009A3C0)
0009A43D cmp eax,0x1F40: handle >= cover base (sub_0009A420 decoder)
0009A454 cmp eax,0x1F40: handle >= cover base (sub_0009A420)
0009A49B lea esi,[eax-0x1F40]: cover handle -> index (sub_0009A420)
0009A4E0 cmp eax,0x1F40: handle >= cover base (sub_0009A4C0 decoder)
0009A4F4 add eax,-0x1F40: cover handle -> index (sub_0009A4C0)
0009A553 add eax,0x1F40: cover index -> handle (sub_0009A510)
0009CCE7 add eax,-0x1F40: cover handle -> index (sub_0009CCA0)
0009D26E add eax,-0x1F40: cover handle -> index (nav loader sub_0009CE00)
0009D37E add eax,-0x1F40: cover handle -> index (sub_0009CE00)
0009D474 add eax,-0x1F40: cover handle -> index (sub_0009D3B0)
0009D5FE cmp ecx,0x1F40: pad id threshold (sub_0009D5E0)
0009D622 cmp edx,0x1F40: pad id threshold (sub_0009D5E0)
00159712 cmp eax,0x1F40: handle >= cover base (sub_001596F0)
0015975B cmp eax,0x1F40: handle >= cover base (sub_001596F0)
0015976F add eax,-0x1F40: cover handle -> index (sub_001596F0)
0015D51A cmp edi,0x1F40: pad id threshold (sub_0015D500)
0015E4B5 cmp edx,0x1F40: pad id threshold (sub_0015E450)
00160BD4 cmp eax,0x1F40: handle >= cover base (sub_00160BD0)
001FCED5 cmp eax,0x1F40: handle >= cover base (sub_001FCE90)
001FCEE3 add eax,-0x1F40: cover handle -> index (sub_001FCE90)
002329DF cmp eax,0x1F40: handle >= cover base (sub_00232270)
002329F3 add eax,-0x1F40: cover handle -> index (sub_00232270)
0023F300 cmp eax,0x1F40: pad id threshold (sub_0023F300)
0023FDCF cmp ecx,0x1F40: pad id threshold (teleporter walk sub_0023FD10)
002411F6 cmp ebp,0x1F40: pad id threshold (map fill sub_00240C50)
00241300 cmp ebp,0x1F40: pad id threshold (sub_00240C50)
002413D2 cmp ebp,0x1F40: pad id threshold (sub_00240C50)
002416BA cmp ebp,0x1F40: pad id threshold (sub_00240C50)
00241853 cmp ebp,0x1F40: pad id threshold (sub_00240C50)
00241F64 cmp esi,0x1F40: pad id threshold (sub_00240C50)
00242035 cmp eax,0x1F40: pad id threshold (sub_00240C50)
00242094 cmp esi,0x1F40: pad id threshold (sub_00240C50)
00242107 cmp esi,0x1F40: pad id threshold (sub_00240C50)
00274411 cmp eax,0x1F40: handle >= cover base (sub_00274400)
0027446A cmp eax,0x1F40: handle >= cover base (sub_00274450)
0027C3BB cmp eax,0x1F40: handle >= cover base (sub_0027C3A0)
002B5A03 cmp eax,0x1F40: handle >= cover base (sub_002B55D0)
002B5A22 movsx edx,[eax+ecx-0x3E7F8]: folded cover[h-0x1F40] (x32, +8), cover base kept
002B5A86 movsx eax,[ecx+edx-0x3E7F8]: folded cover[h-0x1F40] (x32, +8), cover base kept
00339448 cmp eax,0x1F40: handle >= cover base (sub_00339410)
003394B2 cmp ecx,0x1F40: handle >= cover base (sub_00339480)
003394C2 add ecx,-0x1F40: cover handle -> index (sub_00339480)
00339597 cmp eax,0x1F40: handle >= cover base (sub_00339560)
0033B7DC cmp [esp+edx*4+0x14],0x1F40: handle >= cover base (sub_0033B750)
0033B80B sub edx,0x1F40: cover handle -> index (sub_0033B750)
0033B95D cmp eax,0x1F40: handle >= cover base (sub_0033B940)
0033B971 add eax,-0x1F40: cover handle -> index (sub_0033B940)
0033BBD7 cmp edi,0x1F40: handle >= cover base (sub_0033B940)
0033BBED add edi,-0x1F40: cover handle -> index (sub_0033B940)
0033BC73 cmp eax,0x1F40: handle >= cover base (sub_0033BC50)
0033BC81 add eax,-0x1F40: cover handle -> index (sub_0033BC50)
0033BE04 cmp eax,0x1F40: handle >= cover base (sub_0033BDC0)
0033BE18 add eax,-0x1F40: cover handle -> index (sub_0033BDC0)
0033BEBE cmp eax,0x1F40: handle >= cover base (sub_0033BDC0)
0033BED2 add eax,-0x1F40: cover handle -> index (sub_0033BDC0)
0033EFB5 cmp eax,0x1F40: handle >= cover base (sub_0033EF90)
0033EFC9 add eax,-0x1F40: cover handle -> index (sub_0033EF90)
003BC6EC cmp eax,0x1F40: handle >= cover base (sub_003BC6D0)
""")

# 0x1F40 that is not a pad handle at all.
NOT_HANDLE = reviewed("""
001064D2 movss xmm0,[esi+0x1F40]: float field of another struct (sub_001064B0)
0010652E subss xmm0,[esi+0x1F40]: same field
001065A2 mov [esi+0x1F40],edx: same field
00127B10 lea ebp,[ebp+ecx*8+0x1F40]: array at +0x1F40 of another struct (stride 8, sub_00127830)
00194F95 lea ecx,[edi+ebx*8+0x1F40]: array at +0x1F40 of another struct (stride 8, sub_00194DB0)
00388D57 mov [ebp-0x10],0x1F40: library code (0x0038xxxx), a local of 8000
00389486 mov word [esi+0x2C],0x1F40: library code, word field 8000
""")


def candidate_sites():
    found = {}
    for i in decoded():
        if i.branch or i.call:
            continue
        if any(v in HANDLE for k, v in i.values()) or any(k == 'disp' and folded(v) for k, v in i.values()):
            found[i.address] = f'{i.mnemonic} {i.text}'
    return found


def test_every_pad_handle_constant_is_classified():
    table = patched('pads')
    found = candidate_sites()
    assert not unclassified(found, table, KEPT, COVER_BASE, NOT_HANDLE)
    # the reviewed lists describe real sites and never hide a patched one
    for name, group in (('kept', KEPT), ('cover base', COVER_BASE), ('not a handle', NOT_HANDLE)):
        assert set(group) <= set(found), (name, sorted(hex(a) for a in set(group) - set(found)))
        assert not set(group) & table, name
    # every runtime-range constant (not the kept cover base) is rewritten
    moved = {a for a, s in found.items() if a not in KEPT and a not in COVER_BASE and a not in NOT_HANDLE}
    assert moved <= table
    assert len(moved) == 88
    assert 0x002B0456 in moved                     # folded runtime[h - base].flags
