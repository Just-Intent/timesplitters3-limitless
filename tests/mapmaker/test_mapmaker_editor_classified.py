"""Every access to the editor object's moved arrays is patched (editor.json) or reviewed.

editor.json moves the Story AI, logic op/node/record and free-object arrays of
the editor object ([0x007844A8], docs/mapmaker/mapmaker-editor.md "Layout") behind
+0x11950. Its sites came from an interprocedural tracker; this is an
independent, simpler check over the lifter's decode (any instruction length):
in every function that loads [0x007844A8], follow the pointer through registers
in instruction order (copies, lea/add/sub of constants; a call clobbers
eax/ecx/edx, any other write kills the register) and flag every memory operand,
lea or add/sub whose offset from the editor base lands in an old array.
Pointers formed by a patched instruction already point into the new array, so
tracking stops there.
"""
from mapmaker_decoded import decoded, patched, reviewed, unclassified

EDITOR = 0x007844A8
OLD = [('sai', 0x288, 0x2D30), ('free', 0x3070, 0x3868), ('clipboard counts', 0x3868, 0x3870),
       ('clipboard', 0x3870, 0x4068), ('ops', 0x41E4, 0x8028), ('nodes', 0x81A8, 0xDCD0),
       ('records', 0xDD14, 0x11384)]
REG32 = {'eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp'}
PART = {'al': 'eax', 'ah': 'eax', 'ax': 'eax', 'bl': 'ebx', 'bh': 'ebx', 'bx': 'ebx', 'cl': 'ecx', 'ch': 'ecx',
        'cx': 'ecx', 'dl': 'edx', 'dh': 'edx', 'dx': 'edx', 'si': 'esi', 'di': 'edi', 'bp': 'ebp'}

# Not an array element (docs/mapmaker/mapmaker-editor.md, "Kept").
NOT_MOVED = reviewed("""
00316569 [edx+ecx*4+0x4064]: the dword array at +0x4068 indexed with index-1, not the clipboard (kept)
00316AD0 [eax+edx*4+0x4064]: dword array +0x4068, index-1
0032E077 [eax+edx*4+0x4064]: dword array +0x4068, index-1
0033067D [edx+ecx*4+0x4064]: dword array +0x4068, index-1
00330B14 [edx+eax*4+0x4064]: dword array +0x4068, index-1
""")


def region(offset):
    for name, lo, hi in OLD:
        if lo <= offset < hi:
            return name
    return None


def candidate_sites():
    table = patched('editor')
    by_function = {}
    for i in decoded():
        by_function.setdefault(i.function, []).append(i)
    found = {}
    for insns in by_function.values():
        if not any(op[0] == 'mem' and not op[1] and not op[2] and op[4] == EDITOR for i in insns for op in i.ops):
            continue
        tracked, retire = {}, None             # register -> offset from the editor base
        for i in sorted(insns, key=lambda i: i.address):
            if retire:
                tracked.pop(retire, None)
            ops = i.ops
            dst = PART.get(ops[0][1], ops[0][1]) if ops and ops[0][0] == 'reg' else None
            retire = dst if i.address in table else None
            for op in ops:
                if op[0] != 'mem' or (op[1] in tracked) == (op[2] in tracked) or not op[4]:
                    continue
                base = tracked[op[1]] if op[1] in tracked else tracked[op[2]]
                if region((base + op[4]) & 0xFFFFFFFF):
                    found[i.address] = f'{i.mnemonic} {i.text}'
            if i.call:
                for r in ('eax', 'ecx', 'edx'):
                    tracked.pop(r, None)
                continue
            if i.branch:
                continue
            m = i.mnemonic
            if m == 'mov' and dst in REG32 and len(ops) == 2:
                src = ops[1]
                if src[0] == 'mem' and not src[1] and not src[2] and src[4] == EDITOR:
                    tracked[dst] = 0
                elif src[0] == 'reg' and src[1] in tracked:
                    tracked[dst] = tracked[src[1]]
                else:
                    tracked.pop(dst, None)
                continue
            if m == 'lea' and dst in REG32:
                src = ops[1]
                if src[1] in tracked and not src[2]:
                    tracked[dst] = (tracked[src[1]] + src[4]) & 0xFFFFFFFF
                elif src[2] in tracked and not src[1] and src[3] == 1:
                    tracked[dst] = (tracked[src[2]] + src[4]) & 0xFFFFFFFF
                else:
                    tracked.pop(dst, None)
                    continue
                if region(tracked[dst]):
                    found[i.address] = f'{i.mnemonic} {i.text}'
                continue
            if m in ('add', 'sub') and dst in tracked and ops[1][0] == 'imm':
                delta = ops[1][1] if m == 'add' else (-ops[1][1]) & 0xFFFFFFFF
                tracked[dst] = (tracked[dst] + delta) & 0xFFFFFFFF
                if region(tracked[dst]):
                    found[i.address] = f'{i.mnemonic} {i.text}'
                continue
            if dst in REG32:
                tracked.pop(dst, None)
            if m == 'xchg' and len(ops) == 2 and ops[1][0] == 'reg':
                tracked.pop(PART.get(ops[1][1], ops[1][1]), None)
            if m in ('mul', 'div', 'idiv', 'cdq'):
                tracked.pop('eax', None)
                tracked.pop('edx', None)
            if m.startswith('rep') or m in ('movsd', 'stosd', 'movsb', 'stosb', 'movsw', 'stosw'):
                for r in ('ecx', 'esi', 'edi'):
                    tracked.pop(r, None)
    return found


def test_every_moved_editor_array_access_is_classified():
    table = patched('editor')
    found = candidate_sites()
    assert not unclassified(found, table, NOT_MOVED)
    assert set(NOT_MOVED) <= set(found) and not set(NOT_MOVED) & table
    # the tracker is not vacuous: it re-finds most of the table on its own
    assert len(set(found) & table) >= 550, len(set(found) & table)
