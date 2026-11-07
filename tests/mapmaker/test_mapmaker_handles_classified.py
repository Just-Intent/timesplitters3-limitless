"""Engine object handles slot << 21 (config/guest-code-patches/handles.json), classified over all game code.

Stock handles are `slot << 22 | serial` (encode 0x000A69BE, serial
[0x0072F5E0] wrapped at 0x400000 by 0x000A69C4) and every reader decodes
`shr r,0x16; imul r,r,0x2E8`, so the engine object pool ends at 1024 slots
(docs/agent-context/limitless-caps.md "Engine object handles 2047"). The
table moves the slot field down one bit in every scene, so the MapMaker pool
holds 2047. These checks pin what that relies on (mapmaker_decoded.py):

- every decode is rewritten, and nothing else (the table is exactly the scan);
- the remaining `shr/sar r,0x16`, `shl r,0x16` and `imul r,r,0x2E8` sites are
  not handles (reviewed); no code turns an object pointer back into a slot
  by division, so nothing else depends on the field width;
- the serial counter has no other writer than the encode and the pool reset;
- handles never leave the process: the System Link object encoder
  sub_00359400 sends +0x150 player bytes, a character index or the +0x230
  network id, the 21 senders (callers of sub_00353990) never store a
  handle, and the network decodes of handles all read local tables;
- the host-side decoder (src/input/pc_mounted.h) uses the same shift.
"""
import json
import re

from mapmaker_decoded import ROOT, decoded, reviewed

TABLE = ROOT / 'config/guest-code-patches/handles.json'
OBJECTS = ROOT / 'src/interface/pc_mapmaker_objects.inc'
POOL, SERIAL, STRIDE = 0x0072F5E8, 0x0072F5E0, 0x2E8
ENCODE, WRAP = 0x000A69BE, 0x000A69C4
CREATE, RESET = 0x000A6880, 0x000A5AF0
SEND, OBJECT_ENCODER, NETWORK = 0x00353990, 0x00359400, (0x00351000, 0x0035E000)

# shr/sar r,0x16 not followed by imul r,r,0x2E8: not a handle decode.
REVIEWED_SHIFT = reviewed("""
00012C31 sub_00012BE0: sar of a packed signed 10-bit vector component, converted to float
00012C70 sub_00012BE0: same unpack, third component
000592B3 sub_00059210: unpacks a 10-bit vector component (and 0x3FF, fild), not a handle
000906F1 sub_000903D0: sar of a packed signed vector component, converted to float
00090B76 sub_000903D0: same unpack on the second path
00333837 sub_00333780: flag bit 22 of a table word (and 1)
0034EBFA sub_0034EBE0: flag bit 22 of [character+0xBF0]+0x58 (and 1)
0034EC04 sub_0034EBE0: same flag on the zero path
003BC068 MD5-style rotate (shl 0xA / shr 0x16 / or) in library code
003BC0EC same rotate
003BC170 same rotate
003BC1ED same rotate
""")

# shl r,0x16 other than the encode.
REVIEWED_SHL = reviewed("""
001ECC16 sub_001ECB00: colour byte (and 0xFC) shifted into a packed RGBA word, or 0x3F00
003BBA8C MD5-style rotate in library code
003BBB10 same rotate
003BBB94 same rotate
003BBC18 same rotate
""")

# imul r,r,0x2E8 not fed by a decode: slot indices from the count, the +0x88 search or the pool size.
REVIEWED_STRIDE = reviewed("""
0009E3CF sub_0009E3C0: [obj+0x88] id lookup, fast path slot = id (argument, >= 0)
0009E41D sub_0009E3C0: same lookup, index of the matching slot
0009E526 sub_0009E4B0: room bucket rebuild loop from [0x007B975C] to the count
0009EE1D sub_0009EE00: the slot allocator, append at the count
0009EEB1 sub_0009EE00: the slot allocator, append after the free scan
000A2C66 sub_000A2C60: the count times the stride (pool end)
000A5B18 sub_000A5AF0: pool clear, [0x0072F5E4] slots
000A5C1F sub_000A5C00: pool allocation, [0x0072F5E4] slots
003593C3 sub_00359390: System Link lookup by network id +0x230, index of the matching slot
""")


def _table():
    return {e['id']: e for e in json.loads(TABLE.read_text(encoding='utf-8'))['patches']}


def _reg(op):
    return op[1] if op[0] == 'reg' else None


def scan(insns):
    """(decodes {address: insn}, other shifts {address: text}, stride uses {address: text})."""
    decodes, other, stride = {}, {}, {}
    for k, i in enumerate(insns):
        if i.mnemonic in ('shr', 'sar') and len(i.ops) == 2 and i.ops[1] == ('imm', 0x16) and _reg(i.ops[0]):
            r = _reg(i.ops[0])
            if i.mnemonic == 'shr' and any(j.function == i.function and j.mnemonic == 'imul' and len(j.ops) == 3
                                           and _reg(j.ops[0]) == r and j.ops[2] == ('imm', STRIDE)
                                           for j in insns[k + 1:k + 4]):
                decodes.setdefault(i.address, i)
            else:
                other[i.address] = f'{i.mnemonic} {i.text}'
        if i.mnemonic == 'imul' and len(i.ops) == 3 and i.ops[2] == ('imm', STRIDE):
            if not any(p.mnemonic == 'shr' and p.ops[1:] == (('imm', 0x16),) for p in insns[max(0, k - 4):k]):
                stride[i.address] = f'{i.mnemonic} {i.text}'
    return decodes, other, stride


def missing(table, decodes):
    """Decodes the table does not rewrite, and table decode entries that are no decode."""
    rows = {int(e['address'], 16): e for k, e in table.items() if k.startswith('handle-decode-')}
    return sorted(set(decodes) - set(rows)), sorted(set(rows) - set(decodes))


def test_every_decode_is_rewritten_and_nothing_else():
    insns = decoded()
    decodes, _, _ = scan(insns)
    table = _table()
    assert len(decodes) == 167
    assert missing(table, decodes) == ([], [])
    for k, e in table.items():
        if k.startswith('handle-decode-'):
            i = decodes[int(e['address'], 16)]
            assert e['original_hex'] == i.hex and e['new_hex'] == i.hex[:-2] + '15', k
            assert int(e['function'], 16) <= i.address
    assert set(table) - {k for k in table if k.startswith('handle-decode-')} == {'handle-encode-shift', 'handle-serial-wrap'}


def test_missing_decode_is_reported():
    """Mutation: a table without one decode (or with an extra row) fails the classification."""
    insns = decoded()
    decodes, _, _ = scan(insns)
    table = _table()
    gone = 'handle-decode-0017A0B6' if 'handle-decode-0017A0B6' in table else sorted(k for k in table if k.startswith('handle-decode-'))[5]
    lost = dict(table)
    del lost[gone]
    assert missing(lost, decodes)[0] == [int(table[gone]['address'], 16)]
    extra = dict(table, **{'handle-decode-00012C31': dict(table[gone], address='0x00012C31')})
    assert missing(extra, decodes)[1] == [0x00012C31]


def test_other_shift_and_stride_sites_are_reviewed():
    insns = decoded()
    _, other, stride = scan(insns)
    assert set(other) == set(REVIEWED_SHIFT), sorted(set(other) ^ set(REVIEWED_SHIFT))
    assert set(stride) == set(REVIEWED_STRIDE), sorted(set(stride) ^ set(REVIEWED_STRIDE))
    shl = {i.address for i in insns if i.mnemonic == 'shl' and i.ops[1:] == (('imm', 0x16),)}
    assert shl == set(REVIEWED_SHL) | {ENCODE}


def test_no_pointer_to_slot_division():
    """No reciprocal of the 0x2E8 stride (or of 93 = 0x2E8 / 8) appears, so no object
    pointer is turned back into a slot that a narrower field could hold."""
    insns = decoded()
    near = []
    for i in insns:
        for kind, v in i.values():
            if kind == 'imm' and v >= 0x1000000:
                for d in (744, 93, 186, 372):
                    if any(abs(v - (1 << s) / d) <= 2 for s in range(32, 45)):
                        near.append(i.address)
    assert near == []


def test_encode_and_serial():
    insns = decoded()
    create = {i.address: i for i in insns if i.function == CREATE}
    enc, wrap = create[ENCODE], create[WRAP]
    assert enc.mnemonic == 'shl' and enc.ops[1] == ('imm', 0x16)
    assert wrap.mnemonic == 'test' and wrap.ops[1] == ('imm', 0xFFC00000)
    table = _table()
    assert table['handle-encode-shift']['original_hex'] == enc.hex
    assert table['handle-serial-wrap']['original_hex'] == wrap.hex
    # [0x0072F5E0] is written by the encode (inc / reset to 1) and the pool reset only.
    writers = {i.address for i in insns if i.ops and i.ops[0][0] == 'mem' and not i.ops[0][1] and not i.ops[0][2]
               and i.ops[0][4] == SERIAL and i.mnemonic in ('mov', 'inc', 'dec', 'add', 'sub', 'or', 'and')}
    assert writers == {0x000A69CC, 0x000A69D3, 0x000A5B62}
    assert {i.function for i in insns if i.address in writers} == {CREATE, RESET}
    # The C side agrees: the pool stays below 2 ** (32 - 21) so no slot forms -1.
    text = OBJECTS.read_text(encoding='utf-8')
    assert '#define MAPMAKER_OBJECTS_HANDLE_SHIFT 21u' in text
    assert '#define MAPMAKER_OBJECTS_POOL 2047u' in text


def test_handles_never_cross_the_wire():
    insns = decoded()
    by = {}
    for i in insns:
        by.setdefault(i.function, []).append(i)
    # The object encoder writes a tag byte then the player byte (+0x150), a character
    # index or the network id (+0x230): never the handle (+0x2C).
    enc = by[OBJECT_ENCODER]
    reads = {op[4] for i in enc for op in i.ops if op[0] == "mem" and op[1] == "esi"}
    assert reads == {0x20, 0x150, 0x230}
    senders = sorted({i.function for i in insns if i.call and ('imm', SEND) in i.ops})
    assert len(senders) == 21                       # 22 call sites (sub_0035A480 sends twice)
    # No sender loads a handle field, decodes one or touches the pool, except
    # sub_0035AA70, which turns its handle argument into a pointer for sub_00359400.
    touch = {}
    for f in senders:
        for i in by[f]:
            if any(op[0] == 'mem' and op[4] == 0x2C and op[1] not in ('esp', 'ebp') for op in i.ops) \
                    or (i.mnemonic == 'shr' and i.ops[1:] == (('imm', 0x16),)) \
                    or any(op[0] == 'mem' and not op[1] and op[4] == POOL for op in i.ops):
                touch.setdefault(f, []).append(i.address)
    assert touch == {0x0035AA70: [0x0035AA90, 0x0035AA98, 0x0035AAA1]}
    assert any(i.call and ('imm', OBJECT_ENCODER) in i.ops for i in by[0x0035AA70])
    # Every handle decode in the network code reads a local table or argument,
    # then validates it against the local pool (+0x2C), like any other reader.
    decodes, _, _ = scan(insns)
    net = sorted(a for a in decodes if NETWORK[0] <= a < NETWORK[1])
    assert net == [0x0035160A, 0x00351FF0, 0x003598B2, 0x0035AA98, 0x0035B905]


def test_host_decoder_uses_the_same_shift():
    text = (ROOT / 'src/input/pc_mounted.h').read_text(encoding='utf-8')
    assert '(handle>>21)*0x2e8' in text and '>>22' not in text
    for path in (ROOT / 'src').rglob('*'):
        if path.suffix in ('.c', '.h', '.inc') and path.is_file():
            src = path.read_text(encoding='utf-8', errors='replace')
            assert not re.search(r'>> *22\)? *\* *0x2[eE]8', src), path
