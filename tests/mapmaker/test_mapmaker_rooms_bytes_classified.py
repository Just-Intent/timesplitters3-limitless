"""Stage 1 of rooms past 241: every byte-width room-id use in the game code, classified.

Room ids are dwords almost everywhere (prop +0x34/+0x38, particle +0xF0, view
+0x2A0/+0x2A4, ...). What caps them at 255 is the few places that keep a room
in a byte, compare one as a byte or size a table by the byte range. This test
decodes all game code with the lifter's decoder (mapmaker_decoded) and pins:

1. every byte access at the displacements of room fields (+0x34..+0x3B,
   +0xB0..+0xB3, +0xF0..+0xF3, +0x99, +0x2A0..+0x2A7) engine-wide, each assigned
   to its record; only the nine light+0x99 sites hold a room;
2. the draw-bucket tables (0x007042B8 rows, 0x00702A40, byte counts
   0x00702DB0/0x00702D60): all users, the four byte writers and their source
   (the real-room loop counter, never a special id);
3. no 8-bit instruction uses a room constant (0xC8..0xD6) as a room;
4. the closed flow of byte room ids: seeded at the bucket and light byte
   loads and followed through registers, stack slots and call arguments, it
   reaches exactly the listed byte stores, compares and room-indexed tables;
5. the light room source: a prop's room or [0x0073D95C] (= room count, set
   from [0x007B975C]), so the light byte holds 0..R or a held/view special id;
6. the per-room flag dword[0xD6] at 0x007B5180 (one writer, 0x000A79C8),
   relocated by rooms.json (stage 2).

docs/mapmaker/mapmaker-rooms.md ("Byte-width room ids: classification") has
the table and the stage list built on it. Skips without the private XBE.
"""
from collections import defaultdict
import json
from pathlib import Path

import pytest

from mapmaker_decoded import decoded

ROOT = Path(__file__).resolve().parents[2]
W8 = {'al', 'ah', 'bl', 'bh', 'cl', 'ch', 'dl', 'dh'}
W16 = {'ax', 'bx', 'cx', 'dx', 'si', 'di', 'bp'}
FULL = {s: f for f, subs in {
    'eax': ('eax', 'ax', 'al', 'ah'), 'ebx': ('ebx', 'bx', 'bl', 'bh'), 'ecx': ('ecx', 'cx', 'cl', 'ch'),
    'edx': ('edx', 'dx', 'dl', 'dh'), 'esi': ('esi', 'si'), 'edi': ('edi', 'di'), 'ebp': ('ebp', 'bp'),
    'esp': ('esp', 'sp')}.items() for s in subs}

LIGHT = {0x001122DE, 0x001123D5, 0x0011241C, 0x001124B9, 0x001124FC, 0x0011408E, 0x00114390, 0x00114829, 0x00114851}
NET = 'System Link code (sub_00350320..sub_00359AA0 and sub_00068830): session/player records [0x774028], 0x774180+n*0x30C, flag bytes'
FLAG = 'bit flags of a non-room record (tested/set with masks, or loaded and masked)'
EDIT = 'MapMaker editor records/state (0x2FA000..0x33A000: serialised item bytes, [0x762F98]/[0x7844A8] flags)'
LIB = 'code past 0x003B0000: C++ objects with vtables (0x004B60C0, ...) and runtime library'

# Byte accesses at room-field displacements, engine-wide (no index register, base not esp).
FIELD_BYTES = {
    'ROOM light+0x99 (light records: 0xB8 stride [0x0073D950], 0xC0 stride [0x0073D960])': LIGHT,
    'record filled by sub_00040FA0: +0x34 = word*16, +0x38 a small byte (0x2C), not a prop':
        {0x0004129B, 0x000413E7, 0x00041698, 0x000418A4, 0x00041BF0},
    NET: {0x00068D6F, 0x0035322E, 0x00353296, 0x0035329F, 0x003532A6, 0x003541CA, 0x003541D8, 0x003543A7,
          0x00354420, 0x00354D0B, 0x00354D1D, 0x00354F34, 0x00354F3C, 0x003550F4, 0x0035589A, 0x003558A3,
          0x003558AD, 0x00355A27, 0x00355A34, 0x00355A3A, 0x00355F8D, 0x00356408, 0x00356B5C, 0x00356C29,
          0x00357522, 0x00357679, 0x00357683, 0x00359B58, 0x0035BDAE, 0x0035E0C5},
    'System Link settings copy 0x78AD58 (+0x99 = byte [0x541CD8]); not a light (rooms.md listed it as one)':
        {0x003564F9},
    'room record [0x708788] (0xB4 stride) +0xB2 flag bits: indexed by room, holds none':
        {0x00083541, 0x000892EA, 0x0008930E},
    'colour bytes +0x35..+0x37 converted with fild': {0x00111122, 0x00111146, 0x00111164},
    'static table 0x7A18A0 (stride 0x68) flag bytes': {0x001845D9, 0x001845DC},
    FLAG: {0x00188F61, 0x00189B4D, 0x0018BE47, 0x001B62B6, 0x001BD0EA, 0x001BF50F, 0x001BF60D, 0x001BF708,
           0x001BF728, 0x001BF752, 0x001C3054, 0x001C3092, 0x001C3B68, 0x00234ECB, 0x002375A4, 0x00237645,
           0x002379E3, 0x002381A4, 0x00268A81, 0x00278980, 0x0027943B, 0x0027AB49, 0x0027AD76, 0x0027AF12,
           0x0036D0C7},
    'static records 0x74FE7C, bit mask test': {0x001A22C1, 0x001A22DF},
    'level number word +0x3A (cmp word 0x2E, then +0x4D as a character)': {0x001B4AD9, 0x001B4AE7},
    'static 0x4FA128 +0xF0 flag mask 0xC': {0x001B6B7A},
    '+0xB0 flag byte of the sub_00236070 record (bit tests, test al,al)':
        {0x0023638F, 0x00236397, 0x002363B8, 0x002363D3},
    EDIT: {0x002FB98E, 0x002FB9A5, 0x0030114B, 0x0030145D, 0x00301ED6, 0x00301EE8, 0x00301F07, 0x00301F1E,
           0x003141E6, 0x0031431D, 0x00314323, 0x0031E2FD, 0x0031E7FD, 0x0031F6EA, 0x0031FF53, 0x0031FF84,
           0x003207F1, 0x003208CD, 0x003208D8, 0x003208E2, 0x00320AD6, 0x00320BAD, 0x00320BCD, 0x00320D53,
           0x00324A90, 0x00324AA8, 0x0032514E, 0x00338AAC},
    LIB: {0x003B20F0, 0x003B2110, 0x003B2130, 0x003B2150, 0x003B229C, 0x003B23CB, 0x003B241B, 0x003B2A30,
          0x003B3D71, 0x003B4597, 0x003B49E4, 0x003B49F2, 0x003BE974, 0x003BE9E6, 0x003C44A7},
}
FIELD_DISPS = set(range(0x34, 0x3C)) | set(range(0xB0, 0xB4)) | set(range(0xF0, 0xF4)) | {0x99} | set(range(0x2A0, 0x2A8))

BUCKET_FUNCS = {0x00086C40, 0x000897D0, 0x00086480, 0x000865E0}
BUCKET_WRITERS = {0x00086CD4: 0x00702A40, 0x00086CF6: 0x007042B8, 0x00086D80: 0x007042B8, 0x000898A3: 0x007042B8}
BUCKET_READERS = {0x000864F4, 0x00086644, 0x000898F0, 0x00086D77}

# 8-bit instructions with an immediate byte 0xC8..0xD6: none is a room (all also in NOT_ROOM of test_mapmaker_rooms).
IMM8 = {
    0x0009116F: 'and cl,0xC9: bit mask of a flag byte (sub_000903D0)',
    0x00091209: 'and cl,0xD2: same flag byte',
    0x0022CCF8: 'data decoded as code inside sub_0022CB70 (pointer table)',
    0x0022CE20: 'data decoded as code inside sub_0022CD30',
    0x002C6955: 'mov cl,0xCE: fill of four bytes at 0x78A254 (front-end record)',
    **{0x003D0C25 + 4 * k: '0xCC stack fill pattern (runtime library)' for k in range(10)},
}

# The byte room-id flow (check 4).
SEEDS = BUCKET_READERS | {0x001123D5, 0x0011241C, 0x001124FC, 0x00114829, 0x00114851}
SEED_ARGS = {(0x00114290, 3)}    # light creation: room argument (see check 5)
FLOW = {
    'byte-store': {0x00086D80, 0x0011408E, 0x00114390},          # bucket copy, light room x2
    'word-store': {0x00089925},                                    # view sorted list +0xE0C (words)
    'cmp8': {0x001123DB},                                          # light: room == 0 (unlinked)
    'byte-load': SEEDS | {0x00114377},
}
# Tables indexed (or addressed) by a byte room id, by base: (element bytes, stock entries, where they live now).
FLOW_TABLES = {
    0x007B54E0: (4, 0xD6, 'per-id prop count, rooms.json -> 0x008C7000 (0x20D)'),
    0x007B5F00: (4, 0xD6, 'per-id prop list, rooms.json -> 0x008C7CC0 (0x20D)'),
    0x007B5180: (4, 0xD6, 'per-id drawn flag, rooms.json -> 0x008C8500 (0x210, check 6)'),
    0x0073C350: (4, 3 * 0xD5, 'light heads [3][0xD5], rooms.json -> 0x008D0000 [3][0x20C]'),
    0x0073D910: (4, 14, 'light round robin room/16, rooms.json -> 0x008CB700 (64)'),
    0x007AC880: (12, 0xD5, 'room light colour float[0xD5][3], rooms.json -> 0x008D18A0 [0x20C][3]'),
}


def unique():
    seen = {}
    for i in decoded():
        seen.setdefault(i.address, i)
    return seen


def by_function():
    out = defaultdict(list)
    for i in decoded():
        out[i.function].append(i)
    return out


def test_room_field_byte_accesses_are_classified():
    found = {}
    for a, i in unique().items():
        if i.branch:
            continue
        for op in i.ops:
            if op[0] == 'mem' and op[5] == 1 and op[4] in FIELD_DISPS and op[1] and op[1] != 'esp' and not op[2]:
                found[a] = f'{i.mnemonic} {i.text}'
    reviewed = set().union(*FIELD_BYTES.values())
    assert sum(len(v) for v in FIELD_BYTES.values()) == len(reviewed), 'a site is in two groups'
    new = {f'0x{a:08X}': s for a, s in sorted(found.items()) if a not in reviewed}
    gone = sorted(f'0x{a:08X}' for a in reviewed - set(found))
    assert not new and not gone, (new, gone)
    assert len(found) == 130
    # the room ones are exactly the light byte; prop/particle/view rooms are dwords
    for a in LIGHT:
        assert any(op[0] == 'mem' and op[4] == 0x99 for op in unique()[a].ops)


def test_room_fields_are_dwords():
    """The stores of the special ids 0xD5 / 0xCE into room fields are all dword stores."""
    widths = defaultdict(set)
    for i in unique().values():
        if i.mnemonic != 'mov' or len(i.ops) != 2 or i.ops[0][0] != 'mem' or i.ops[1] != ('imm', 0xD5) and i.ops[1] != ('imm', 0xCE):
            continue
        op = i.ops[0]
        if op[1] and op[1] != 'esp' and not op[2] and op[4] in (0x34, 0x38, 0xF0, 0x48, 0x2A0, 0x2A4):
            widths[op[4]].add(op[5])
    assert widths and all(w == {4} for w in widths.values()), dict(widths)


def test_draw_buckets():
    users = defaultdict(set)
    for a, i in unique().items():
        for kind, v in i.values():
            if 0x00702A40 <= v < 0x00702E00 or 0x007042B8 <= v < 0x00705000:
                users[i.function].add(a)
    assert set(users) == BUCKET_FUNCS, sorted(f'{f:08X}' for f in users)
    stores, loads = {}, set()
    for f in BUCKET_FUNCS:
        for i in by_function()[f]:
            op = i.ops[0] if i.ops else None
            if i.mnemonic == 'mov' and op and op[0] == 'mem' and op[5] == 1 and op[2] and i.ops[1][0] == 'reg':
                stores[i.address] = (op[4], i.ops[1][1])
            if i.mnemonic in ('mov', 'movzx', 'movsx') and len(i.ops) == 2 and i.ops[1][0] == 'mem' \
                    and i.ops[1][5] == 1 and i.ops[1][4] < 0x10000:
                loads.add(i.address)       # a byte load through a computed row pointer, not a count
    rows = {a: d for a, (d, r) in stores.items() if d in (0x00702A40, 0x007042B8)}
    assert rows == BUCKET_WRITERS
    # writers store dl = the room loop counter (edx = 1..[0x007B975C]); the copy stores bl from 0x00702A40
    assert {a: r for a, (d, r) in stores.items() if a in BUCKET_WRITERS} == {
        0x00086CD4: 'dl', 0x00086CF6: 'dl', 0x00086D80: 'bl', 0x000898A3: 'dl'}
    for f, start in ((0x00086C40, 0x00086C72), (0x000897D0, 0x00089869)):
        i = next(i for i in by_function()[f] if i.address == start)
        assert i.text == 'edx, 1'
        assert any(x.text.endswith('[0x7b975c]') for x in by_function()[f])
    assert loads == BUCKET_READERS
    # readers zero-extend (movzx) or copy the byte unchanged; none sign-extends a room (0xFF stays 255)
    assert {unique()[a].mnemonic for a in BUCKET_READERS} == {'movzx', 'mov'}
    # the global room is drawn by push 0xD5 (rooms.json: 0x10C), never stored in a bucket
    assert any(i.address == 0x00086DDF and i.ops == (('imm', 0xD5),) for i in by_function()[0x00086C40])


def test_no_byte_instruction_uses_a_room_constant():
    found = {}
    for a, i in unique().items():
        if i.branch or i.call or len(i.ops) < 2:
            continue
        d, s = i.ops[0], i.ops[-1]
        is8 = (d[0] == 'reg' and d[1] in W8) or (d[0] == 'mem' and d[5] == 1)
        if is8 and s[0] == 'imm' and 0xC8 <= (s[1] & 0xFF) <= 0xD6 and (s[1] < 0x100 or s[1] >= 0xFFFFFF00):
            found[a] = f'{i.mnemonic} {i.text}'
    assert set(found) == set(IMM8), {f'0x{a:08X}': s for a, s in sorted(found.items()) if a not in IMM8}
    import test_mapmaker_rooms as rooms
    assert set(IMM8) <= rooms.NOT_ROOM


def _flow():
    """Linear per-function taint of byte room ids through registers, stack slots and call arguments."""
    funcs = by_function()
    args = set(SEED_ARGS)
    while True:
        sinks, new = defaultdict(dict), set()
        for f, insns in funcs.items():
            _flow_function(f, insns, args, sinks, new)
        if new <= args:
            return sinks, args
        args |= new


def _flow_function(f, insns, args, sinks, new):
    t = {r: 'reg' for g, r in args if g == f and isinstance(r, str)}
    slot, delta, targets, dead = {}, 0, {}, False

    def src_taint(o):
        if o[0] == 'reg':
            return t.get(FULL.get(o[1]))
        if o[0] == 'mem' and o[1] == 'esp' and o[2] is None and o[4] < 0x80000000:
            off = delta + o[4]
            if off in slot:
                return slot[off]
            if off > 0 and off % 4 == 0 and (f, off // 4 - 1) in args:
                return f'arg{off // 4 - 1}'
        return None

    for i in insns:
        if dead and i.address in targets:
            delta = targets[i.address]
        dead = False
        m, ops = i.mnemonic, i.ops
        dst = ops[0] if ops else None
        src = ops[1] if len(ops) > 1 else None
        for o in ops:
            if o[0] == 'mem' and o[1] != 'esp' and ((o[2] and FULL.get(o[2]) in t) or (o[1] and FULL.get(o[1]) in t and (o[2] or o[4] >= 0x10000))):
                sinks['index'][i.address] = (f, o[4], o[3])
        if i.branch and not i.call:
            for o in ops:
                if o[0] == 'imm':
                    targets.setdefault(o[1], delta)
            dead = m == 'jmp'
            continue
        if i.call:
            tgt = next((o[1] for o in ops if o[0] == 'imm'), None)
            if tgt is not None:
                new.update((tgt, k) for k in range(8) if slot.get(delta + 4 * k))
                new.update((tgt, r) for r in ('eax', 'ecx', 'edx') if r in t)
            for r in ('eax', 'ecx', 'edx'):
                t.pop(r, None)
            continue
        if m == 'ret':
            dead = True
            continue
        if m == 'push':
            delta -= 4
            v = src_taint(dst)
            slot[delta] = v
            continue
        if m == 'pop':
            if dst[0] == 'reg' and dst[1] in FULL:
                if slot.get(delta):
                    t[FULL[dst[1]]] = slot[delta]
                else:
                    t.pop(FULL[dst[1]], None)
            delta += 4
            continue
        if dst and dst[0] == 'reg' and dst[1] == 'esp':
            if m in ('sub', 'add') and src[0] == 'imm':
                delta += src[1] if m == 'add' else -src[1]
            continue
        if m in ('mov', 'movzx', 'movsx') and src:
            v = 'seed' if i.address in SEEDS else src_taint(src)
            if dst[0] == 'mem':
                if dst[1] == 'esp' and dst[2] is None and dst[4] < 0x80000000:
                    slot[delta + dst[4]] = v if dst[5] == 4 else None
                elif v and src[0] == 'reg':
                    sinks[{1: 'byte', 2: 'word', 4: 'dword'}[dst[5]] + '-store'][i.address] = f
                continue
            d = FULL.get(dst[1])
            if v and src[0] == 'mem' and src[5] == 1:
                sinks['byte-load'][i.address] = f
            if v:
                t[d] = v
            else:
                t.pop(d, None)
            continue
        if m == 'cmp':
            if dst[0] == 'reg' and dst[1] in W8 and FULL[dst[1]] in t:
                sinks['cmp8'][i.address] = f
            continue
        if m == 'test' or not dst or dst[0] != 'reg':
            if dst and dst[0] == 'mem' and dst[1] == 'esp' and dst[2] is None and dst[4] < 0x80000000:
                slot.pop(delta + dst[4], None)
            continue
        d = FULL.get(dst[1])
        tainted_src = src is not None and src[0] == 'reg' and FULL.get(src[1]) in t
        if m in ('add', 'sub', 'inc', 'dec', 'shr', 'sar', 'shl', 'or', 'and') and (d in t or tainted_src):
            t[d] = 'derived'
        elif m == 'imul' and (d in t or tainted_src or (len(ops) == 3 and FULL.get(src[1]) in t)):
            t[d] = 'derived'
        elif m == 'lea' and src[0] == 'mem' and (FULL.get(src[1]) in t or FULL.get(src[2]) in t):
            t[d] = 'derived'
        else:
            t.pop(d, None)
            if m == 'xchg' and src[0] == 'reg':
                t.pop(FULL.get(src[1]), None)


def test_byte_room_flow_is_closed():
    sinks, args = _flow()
    for kind, expected in FLOW.items():
        assert set(sinks[kind]) == expected, (kind, sorted(f'0x{a:08X}' for a in set(sinks[kind]) ^ expected))
    tables, stray = set(), set()
    for f, base, scale in sinks['index'].values():
        if 0x00400000 <= base < 0x00900000:
            hit = [b for b, (size, n, _) in FLOW_TABLES.items() if b <= base < b + size * n]
            (tables.update(hit) if hit else stray.add(base))
    assert not stray and tables == set(FLOW_TABLES), (sorted(map(hex, stray)), sorted(map(hex, set(FLOW_TABLES) - tables)))
    # the other indexed uses are relative: view rects (+0x3BC.. of [0x007B0CC0]) and room records (+0xB2)
    relative = {base for f, base, scale in sinks['index'].values() if base < 0x10000}
    assert relative <= {0, 0x1C, 0xB2, 0xF0, 0x3C4}, sorted(map(hex, relative))
    # the room arguments it reaches: the room-prop draws, the light functions and the room-flag writer
    room_args = {(g, k) for g, k in args if g in (0x000A2900, 0x000A79B0, 0x00112EA0, 0x00114030, 0x00114290, 0x001164C0)}
    assert {(0x000A2900, 0), (0x000A79B0, 0), (0x00114290, 3), (0x00114030, 'eax')} <= room_args
    # nothing reached stores a room as a byte outside the buckets and the light record
    assert not sinks['byte-store'].keys() - FLOW['byte-store']


def test_light_room_source():
    """sub_00114290's room is the prop's +0x34, or [0x0073D95C] for the global room / multi-room props."""
    funcs = by_function()
    callers = {i.function for i in unique().values() if i.call and ('imm', 0x00114290) in i.ops}
    assert callers == {0x00115070, 0x001154E0}
    for f, load, cmp in ((0x00115070, 0x00115074, 0x0011507D), (0x001154E0, 0x00115795, 0x00115798)):
        insns = {i.address: i for i in funcs[f]}
        assert insns[load].ops[1][4] == 0x34 and insns[cmp].ops[1] == ('imm', 0xD5)
        assert any(op[0] == 'mem' and op[4] == 0x0073D95C for i in funcs[f] for op in i.ops)
    writers = {i.address for i in unique().values() if i.mnemonic == 'mov' and i.ops[0][0] == 'mem'
               and i.ops[0][4] == 0x0073D95C and not i.ops[0][1]}
    assert writers == {0x001121F5}
    prev = next(i for i in funcs[0x001121E0] if i.address == 0x001121F0)
    assert prev.ops[1][4] == 0x007B975C          # [0x0073D95C] = room count R
    # held / view rooms (0xCA..0xD4 stock) are not substituted: such a light keeps a special id in its byte


def _flag_table_problems(rooms, objects):
    """Why the stage-2 relocation of the per-id drawn flag table 0x007B5180 is wrong ([] when right)."""
    flag = [e for e in rooms if e.get('kind') == 'relocate' and int(e['old'], 16) <= 0x007B5180 < int(e['old'], 16) + int(e['size'], 16)]
    if len(flag) != 1:
        return ['0x007B5180 is not relocated exactly once']
    out = []
    if flag[0]['sites'] != {'0x000A79C8': 'offset'}:
        out.append('the only reference 0x000A79C8 must be an offset site')
    old, size, new, new_size = (int(flag[0][k], 16) for k in ('old', 'size', 'new', 'new_size'))
    # declared 4 bytes short: entry 0xD5 is the old end, so the list count 0x007B54D8 is outside the range
    if old != 0x007B5180 or size != 0xD5 * 4:
        out.append('declared range must be entries 0..0xD5 (0x354 bytes)')
    if new_size < 0x210 * 4:
        out.append('new table holds fewer than 0x210 ids')
    if not (0x008C0000 <= new and new + new_size <= 0x008CD000):
        out.append('new table is outside the rooms tables (0x008C0000..0x008CD000)')
    list0 = next(e for e in objects if e['id'] == 'obj-list0-relocate')
    if int(list0['old'], 16) + int(list0['size'], 16) >= 0x007B5180 or '0x000A79C8' in list0['sites']:
        out.append('objects.json slot-0 list still claims 0x007B5180')
    return out


FLAG_MUTATIONS = {
    'not-relocated': lambda r, o: [e for e in r if e['id'] != 'rooms-reloc-f-0x7b5180'],
    'too-small': lambda r, o: [dict(e, new_size='0x3FC') if e['id'] == 'rooms-reloc-f-0x7b5180' else e for e in r],
    'counter-in-range': lambda r, o: [dict(e, size='0x358') if e['id'] == 'rooms-reloc-f-0x7b5180' else e for e in r],
    'outside-rooms': lambda r, o: [dict(e, new='0x008D4000') if e['id'] == 'rooms-reloc-f-0x7b5180' else e for e in r],
}


def _tables():
    load = lambda name: json.loads((ROOT / 'config/guest-code-patches' / name).read_text(encoding='utf-8'))['patches']
    return load('rooms.json'), load('objects.json')


def test_room_flag_table_7b5180_is_relocated():
    """Stage 2: the per-id drawn flag dword[0xD6] (one reference, 0x000A79C8) moves with room for 0x110 ids."""
    refs = {a for a, i in unique().items() for k, v in i.values() if 0x007B5180 <= v < 0x007B54D8}
    assert refs == {0x000A79C8}                   # mov dword [room*4+0x7B5180], 1 in sub_000A79B0
    rooms, objects = _tables()
    assert not _flag_table_problems(rooms, objects)
    # entry 0xD6 of the stock table was [0x007B54D8], a count only incremented/reset (no reader uses it as a
    # bound); the 241 layout wrote ids 0xD6..0xFE there and into the old 0x007B54E0 tables. Now nothing is.
    counter = {a: i.mnemonic for a, i in unique().items() if any(v == 0x007B54D8 for k, v in i.values())}
    assert counter == {0x0009FBAD: 'inc', 0x000A016E: 'mov', 0x000A0206: 'mov', 0x000A0228: 'mov',
                       0x000A0393: 'mov', 0x000A03B5: 'mov', 0x000A5BAB: 'mov'}


@pytest.mark.parametrize('mutation', sorted(FLAG_MUTATIONS))
def test_room_flag_table_mutations_fail(mutation):
    rooms, objects = _tables()
    assert _flag_table_problems(FLAG_MUTATIONS[mutation](rooms, objects), objects), mutation


def test_room_flag_table_list0_mutation_fails():
    rooms, objects = _tables()
    kept = [dict(e, size='0x1C0', sites=dict(e['sites'], **{'0x000A79C8': 'keep'})) if e['id'] == 'obj-list0-relocate' else e
            for e in objects]
    assert _flag_table_problems(rooms, kept)


def _define(name):
    for line in (ROOT / 'src/interface/pc_mapmaker_rooms.inc').read_text(encoding='utf-8').splitlines():
        if line.startswith(f'#define {name} '):
            return int(line.split()[2].rstrip('u'), 0)
    raise AssertionError(name)


def test_light_side_table_matches_the_code():
    """Stage 3: what pc_mapmaker_rooms.inc's light hooks assume about sub_001121E0..sub_00114290."""
    funcs, insns = by_function(), unique()
    calls = defaultdict(set)
    for i in insns.values():
        if i.call or i.branch:
            for op in i.ops:
                if op[0] == 'imm' and op[1] in (0x001121E0, 0x001123C0, 0x001124D0, 0x00114030, 0x00114290):
                    calls[op[1]].add(i.address)
    # sub_00114030 is linked only from sub_00114290 (the pending full id is meant for that call)
    assert calls[0x00114030] == {0x00114859}
    assert calls[0x00114290] == {0x00115403, 0x0011588C}
    assert calls[0x001123C0] == {0x001129A1, 0x001129E0, 0x001142E2, 0x00115593, 0x00115642}
    assert calls[0x001124D0] == {0x00112D71} and calls[0x001121E0] == {0x00112396, 0x001CDDFB}
    # the pool clear writes the room byte through +0x80 + 0x19
    assert insns[0x001122AF].ops[0][4] == 0x19 and insns[0x00112299].ops[1] == ('imm', 0x80)
    # pool / record / group globals the side table indexes by
    texts = {a: insns[a].text for a in (0x00112294, 0x00112233, 0x001122BA, 0x001142B8, 0x001142AF)}
    assert '[0x73d950]' in texts[0x00112294] and '[0x4e62dc]' in texts[0x00112233] and '[0x73d960]' in texts[0x001122BA]
    assert '[0x73d954]' in texts[0x001142B8] and '0x73d910' in texts[0x001142AF]
    assert (_define('MAPMAKER_ROOMS_LIGHT_POOL'), _define('MAPMAKER_ROOMS_LIGHT_COUNT'), _define('MAPMAKER_ROOMS_LIGHT_ONE'),
            _define('MAPMAKER_ROOMS_LIGHT_PER')) == (0x0073D950, 0x004E62DC, 0x0073D960, 0x0073D954)
    assert insns[0x001122B2].ops[1] == ('imm', _define('MAPMAKER_ROOMS_LIGHT_STRIDE'))
    # creation: the room argument (esp+0x78 = entry [esp+0x10]) picks group room/16, record rr[g] + g*per
    assert insns[0x001142A2].ops[1][4] == 0x78 and insns[0x001142AC].text == 'eax, 4'
    assert insns[0x00114377].ops[1][4] == 0x78 and insns[0x00114390].ops[1] == ('reg', 'dl')
    # the pool sizes the level code picks (500/200/265/400) fit the side table
    counts = {i.ops[1][1] for i in insns.values() if 0x00112300 <= i.address < 0x0011233A and i.mnemonic == 'mov'
              and i.ops[0] == ('reg', 'eax') and i.ops[1][0] == 'imm'}
    assert counts == {0x1F4, 0xC8, 0x109, 0x190} and max(counts) <= _define('MAPMAKER_ROOMS_LIGHT_SLOTS')
    # heads, row width and round robin as rooms.json moves them
    rooms = {e['id']: e for e in _tables()[0]}
    assert int(rooms['rooms-reloc-l1-head']['new'], 16) == _define('MAPMAKER_ROOMS_LIGHT_HEADS')
    assert int(rooms['rooms-reloc-lb-0x73d910']['new'], 16) == _define('MAPMAKER_ROOMS_LIGHT_GROUPS')
    assert int(rooms['rooms-reloc-lb-0x73d910']['new_size'], 16) == 4 * _define('MAPMAKER_ROOMS_LIGHT_GROUP_COUNT')
    rows = [e for e in rooms.values() if e['kind'] == 'operand' and 'light head row width' in e['reason']]
    assert sorted(int(e['address'], 16) for e in rows) == [0x001123EA, 0x00112415, 0x00112EBD, 0x0011403C]
    assert {int(e['value'], 16) for e in rows} == {_define('MAPMAKER_ROOMS_LIGHT_ROW')}
