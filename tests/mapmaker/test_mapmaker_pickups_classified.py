"""Placed pickups and object handles past slot 511, classified over all game code.

v0.4.0-mapmaker.36 playtest (459 pieces, about 500 items): placed health and
armour never appeared, some ammo did. Cause (docs/agent-context/limitless-caps.md
"Placed pickups"): every placed pickup takes a 0xD8-byte record from the
120-entry table [0x00732E60] (count [0x00732E64]); sub_000C1950 frees the new
object and returns 0 once the count reaches 120 (0x000C1DC7 cmp eax,0x78).
objects.json obj-pickups-* raise table, clear and test to 768.

The other suspect, object handles of slots 512..1022 (slot << 22 | serial,
negative as int32), is ruled out here: no handle decode (shr r,0x16 then
imul r,r,0x2E8) is guarded by a signed test of the handle, and every signed
test of a loaded +0x2C field is a non-handle field of another struct.

Decoded with the lifter's decoder over the original XBE (mapmaker_decoded.py).
"""
import json
from pathlib import Path

from mapmaker_decoded import ROOT, decoded, patched, reviewed, unclassified

TABLE = ROOT / 'config/guest-code-patches/objects.json'
PICKUP_GLOBALS = {0x00732E60, 0x00732E64}           # record table pointer, count
PICKUP_FUNCS = {0x000BEEE0, 0x000BEE60, 0x000C1950}  # alloc, clear, add
PICKUP_SIZES = {0x78, 0x6540, 0x1950}                 # 120, 120 x 0xD8, its dwords
SIGNED = {'jl', 'jle', 'jg', 'jge', 'js', 'jns'}

# Readers and writers of the table: each loops to the count or uses one index below it.
REVIEWED_PICKUPS = reviewed("""
000BDB89 mov [0x732e60],0 in sub_000BDB80: drops the pointer (no size)
000BDBA0 mov eax,[0x732e64] in sub_000BDBA0: loop bound, i < count
000BDBB3 mov edx,[0x732e60]: base of that loop
000BDBBF mov edx,[0x732e64]: loop bound re-read
000BDBD0 mov edx,[0x732e64] in sub_000BDBD0: loop bound, i < count
000BDBE7 mov eax,[0x732e60]: base of that loop
000BEE63 mov edi,[0x732e60] in sub_000BEE60: clear base (size patched at 0x000BEE69)
000BEE94 mov [0x732e64],eax: count reset to 0
000BEEEF mov [0x732e60],eax in sub_000BEEE0: stores the allocation (size patched at 0x000BEEE2)
000C0F2D mov ebx,[0x732e64] in sub_000C0F20: loop bound (unrolled by 4, then the rest), i < count
000C0F4C mov eax,[0x732e60]: base of that loop
000C10B6 mov eax,[0x732e60]: base of the remainder loop
000C1125 mov ecx,[0x732e60]: the index found by that loop (< count)
000C1149 mov ecx,[0x732e60]: same index
000C115A mov eax,[0x732e60]: same index
000C116A mov edx,[0x732e60]: same index
000C1179 mov eax,[0x732e60]: same index
000C1DC2 mov eax,[0x732e64] in sub_000C1950: the full test, patched at 0x000C1DC7
000C1DD0 mov ecx,[0x732e60]: the new record, index = count < 768
000C1DE1 inc [0x732e64]: the append
002323BF mov esi,[0x732e64] in sub_00232270: loop bound (dec esi; jne)
002323CE mov edx,[0x732e60]: base of that loop
003384F0 mov ecx,[0x732e64] in sub_003384F0: loop bound; matches go to a caller buffer sized by a first counting call
00338510 mov ecx,[0x732e60]: base of that loop
00338556 mov ecx,[0x732e64]: loop bound re-read
00338570 mov edx,[0x732e64] in sub_00338570: loop bound (dec edx; jne)
0033857D mov ecx,[0x732e60]: base of that loop
00339481 mov edi,[0x732e64] in sub_00339480: loop bound, i < count
003394A2 mov esi,[0x732e60]: base of that loop
00339522 mov ecx,[0x732e60]: the index found by that loop
0034ACB5 mov ecx,[0x732e64] in sub_00349E40: loop bound, i < count
0034ACDB mov edx,[0x732e60]: base of that loop
0034B00F mov edx,[0x732e60]: base re-read
0034B027 mov esi,[0x732e64]: loop bound re-read
""")

# shr r,0x16 that is not followed by imul r,r,0x2E8: not a handle decode.
REVIEWED_SHR = reviewed("""
000592B3 sub_00059210: unpacks a 10-bit vector component (and 0x3FF, fild), not a handle
00333837 sub_00333780: flag bit 22 of a table word (and 1)
0034EBFA sub_0034EBE0: flag bit 22 of [character+0xBF0]+0x58 (and 1)
0034EC04 sub_0034EBE0: same flag on the zero path
003BC068 MD5-style rotate (shl 0xA / shr 0x16 / or) in library code
003BC0EC same rotate
003BC170 same rotate
003BC1ED same rotate
""")

# mov r,[b+0x2c] then test/cmp r + signed jump: none of these b is an engine object.
REVIEWED_2C = reviewed("""
0003405F sub_00033E70: esi is a debug-text argument struct, +0x2C an optional int printed when >= 0
000B48F2 sub_000B4680: ebp = 0x004D98B8 + i x 0xB4, a weapon definition, not an object
000FEA72 sub_000FE990: esi = 0x0073AE08 + i x 0x1D8, a player stats record (+0x2C..+0x34 counters)
0017A1D9 sub_0017A190: index into the owner's +0x144 table, bounded by its +0x164 count
0017A44E sub_0017A440: same index (+0x144 table, +0x164 count)
0017CFEA sub_0017CF30: same index
0017D07A sub_0017CF30: same index
0017D0AF sub_0017CF30: same index
0017DDD4 sub_0017DA00: same index
0017E2DC sub_0017E010: same index
0017E41C sub_0017E010: same index
0017E48D sub_0017E010: same index
0017EB04 sub_0017E680: same index
0027F2A7 sub_0027F160: esi is a character (+0x9D0, +0xBF0), +0x2C a state value
""")


def _index(insns):
    return {i.address: k for k, i in enumerate(insns)}


def pickup_sites(insns):
    found = {}
    for i in insns:
        if i.branch or i.call:
            continue
        absolute = any(op[0] == 'mem' and not op[1] and not op[2] and op[4] in PICKUP_GLOBALS for op in i.ops)
        sized = i.function in PICKUP_FUNCS and any(k == 'imm' and v in PICKUP_SIZES for k, v in i.values())
        if absolute or sized:
            found[i.address] = f'{i.mnemonic} {i.text}'
    return found


def _reg(op):
    return op[1] if op[0] == 'reg' else None


def handle_decodes(insns):
    """shr r,0x16 sites: (decodes, other) where a decode is followed by imul r,r,0x2E8."""
    decodes, other = [], {}
    for k, i in enumerate(insns):
        if i.mnemonic != 'shr' or len(i.ops) != 2 or i.ops[1] != ('imm', 0x16) or _reg(i.ops[0]) is None:
            continue
        r = _reg(i.ops[0])
        follow = insns[k + 1:k + 4]
        if any(j.function == i.function and j.mnemonic == 'imul' and len(j.ops) == 3 and _reg(j.ops[0]) == r
               and j.ops[2] == ('imm', 0x2E8) for j in follow):
            decodes.append(k)
        else:
            other[i.address] = f'{i.mnemonic} {i.text}'
    return decodes, other


NO_WRITE = {'test', 'cmp', 'push', 'jmp', 'call', 'ret'}


def signed_guards(insns, decodes, window=40):
    """Signed branches on the handle before a decode.

    Walks back from each `shr r,0x16` through the same function, following the
    value: `mov r,s` hands it to s, any other write of r ends it (r held
    something else before). A `test r,r` / `cmp r,0|-1` followed by a signed
    jump while r holds the handle is reported."""
    out = {}
    for k in decodes:
        f = insns[k].function
        names = {_reg(insns[k].ops[0])}
        for j in range(k - 1, max(-1, k - window), -1):
            i = insns[j]
            if i.function != f or not names:
                break
            dst = _reg(i.ops[0]) if i.ops else None
            if dst in names and i.mnemonic not in NO_WRITE and not i.branch:
                names.discard(dst)
                if i.mnemonic == 'mov' and len(i.ops) == 2 and _reg(i.ops[1]):
                    names.add(_reg(i.ops[1]))
                continue
            if i.mnemonic in ('test', 'cmp') and dst in names and j + 1 < len(insns) and insns[j + 1].mnemonic in SIGNED:
                src = i.ops[1]
                if src == i.ops[0] or src in (('imm', 0), ('imm', 0xFFFFFFFF)):
                    out[i.address] = f'{i.mnemonic} {i.text}; {insns[j + 1].mnemonic} -> decode at 0x{insns[k].address:08X}'
    return out


def signed_2c_tests(insns):
    out = {}
    for k, i in enumerate(insns[:-3]):
        if i.mnemonic != 'mov' or len(i.ops) != 2 or _reg(i.ops[0]) is None:
            continue
        m = i.ops[1]
        if m[0] != 'mem' or m[1] in (None, 'esp') or m[2] or m[4] != 0x2C or m[5] != 4:
            continue
        r = _reg(i.ops[0])
        for d in (1, 2):
            t, j = insns[k + d], insns[k + d + 1]
            if (t.mnemonic in ('test', 'cmp') and _reg(t.ops[0]) == r and j.mnemonic in SIGNED
                    and (t.ops[1] == t.ops[0] or t.ops[1] in (('imm', 0), ('imm', 0xFFFFFFFF)))):
                out[i.address] = f'{i.mnemonic} {i.text}; {t.mnemonic} {t.text}; {j.mnemonic}'
                break
    return out


def test_every_pickup_table_site_is_classified():
    insns = decoded()
    table = patched('objects')
    found = pickup_sites(insns)
    assert not unclassified(found, table, REVIEWED_PICKUPS)
    assert set(REVIEWED_PICKUPS) <= set(found) and not set(REVIEWED_PICKUPS) & table
    for a in (0x000BEEE2, 0x000BEE69, 0x000C1DC7):        # alloc size, clear dwords, full test
        assert a in found and a in table, hex(a)


def test_pickup_full_test_semantics():
    """`cmp ah,3; jge` refuses exactly from 768 (the count only grows by one), as `cmp eax,0x78` did from 120."""
    entries = {e['id']: e for e in json.loads(TABLE.read_text(encoding='utf-8'))['patches']}
    new = bytes.fromhex(entries['obj-pickups-cap']['new_hex'])
    assert new[:2] == b'\x80\xfc'                          # cmp ah, imm8
    full = new[2] << 8

    def refused(count):                                    # jge after an 8-bit signed cmp of ah
        ah = (count >> 8) & 0xFF
        return (ah - 256 if ah > 127 else ah) >= new[2]

    assert [c for c in range(0, 0x7FFF) if refused(c)][0] == full == 768
    assert not any(refused(c) for c in range(full))
    alloc = int.from_bytes(bytes.fromhex(entries['obj-pickups-alloc']['new_hex'])[1:], 'little')
    clear = int.from_bytes(bytes.fromhex(entries['obj-pickups-clear']['new_hex'])[1:], 'little')
    assert alloc == full * 0xD8 and clear * 4 == alloc


def test_no_signed_handle_test_before_a_decode():
    """Handles of slots 512..1022 are negative as int32; nothing that decodes a handle tests its sign."""
    insns = decoded()
    decodes, other = handle_decodes(insns)
    assert len(decodes) >= 150                             # about 175 shr 0x16 sites in the listing
    assert not unclassified(other, REVIEWED_SHR)
    assert not signed_guards(insns, decodes)
    assert signed_2c_tests(insns).keys() == REVIEWED_2C.keys()


def test_guards_detect_a_signed_handle_test():
    """Mutation: a `test ecx,ecx; jl` before `mov eax,ecx; shr eax,0x16; imul` is reported."""
    insns = decoded()
    idx = _index(insns)
    k = idx[0x00012F0C]                                    # sub_00012F00: test ecx,ecx; je; mov eax,ecx; shr eax,0x16
    assert insns[k].mnemonic == 'shr' and insns[k - 3].mnemonic == 'test' and insns[k - 2].mnemonic == 'je'
    je = insns[k - 2]
    mutated = list(insns)
    mutated[k - 2] = type(je)(je.address, je.function, 'jl', je.text, je.hex, True, False, je.ops)
    decodes, _ = handle_decodes(mutated)
    assert 0x00012F06 in signed_guards(mutated, decodes)


def test_classification_detects_a_missing_patch():
    """Mutation: without obj-pickups-cap the stock 120 test is unclassified."""
    insns = decoded()
    table = patched('objects') - {0x000C1DC7}
    assert '0x000C1DC7' in unclassified(pickup_sites(insns), table, REVIEWED_PICKUPS)
