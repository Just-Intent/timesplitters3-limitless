"""The pause menu's restart frees heap 0 only; every MapMaker block is Level heap.

"START restart" (sub_0003D440 with [0x006B7AA8] set: 0x0003D4DC sub_001CDAF0,
0x0003D4E1 sub_001CDD60) starts with sub_0003E740(3) (0x001CDD62), which calls
sub_0003E240: that puts heap 0's top [0x006B8388] back to its base
[0x006B8380] and allocates its own heap 0 buffer again. sub_0003DFA0(n, heap)
and sub_0003E0B0(align, n, heap) keep heap 1, the Level heap, at
[0x006B837C] (bottom-up; jump tables 0x0003E094 / 0x0003E1C4), which only the
game start (sub_0003DF00), the full level load (sub_0003E650 at 0x0003D504,
the branch without [0x006B7AA8]), the level exit (sub_0003DF70, only when
[0x006B7AA8] is 0) and the allocation itself write. So a block a MapMaker area
keeps (its own sub_0003DFA0(n, 1) / sub_0003E0B0(.., 1) and the stock inits it
calls) stays this level's across a restart; host blocks (xbox_HeapAlloc) and
the nav block are outside the guest heaps. Restarted bosses set their own
blocks up again in place (realbosses, crowe, worm) instead of allocating:
each new block would be Level heap used up for the rest of the level.
"""
import re
import struct
from pathlib import Path

import pytest

from mapmaker_decoded import decoded, _private

ROOT = Path(__file__).resolve().parents[2]
LEVEL_TOP = 0x006B837C       # heap 1 (Level), bottom-up
HEAP0_TOP = 0x006B8388       # heap 0, top-down from the shared end [0x006B8380]
HEAP_END = 0x006B8380
HEAP_FREE = 0x006B8384
RESTART_FLAG = 0x006B7AA8


def by_address():
    return {i.address: i for i in decoded()}


def imm_target(i):
    imm = [op[1] for op in i.ops if op[0] == 'imm']
    return imm[0] if imm else None


def writes_abs(i, addr):
    """i stores to [addr] (absolute operand first: mov/add/sub/... dst)."""
    if not i.ops or i.mnemonic in ('cmp', 'test', 'push') or i.call or i.branch:
        return False
    op = i.ops[0]
    return op[0] == 'mem' and not op[1] and not op[2] and op[4] == addr


def xbe_reader():
    path = _private() / 'game/default.xbe'
    if not path.is_file():
        pytest.skip('needs the private XBE')
    d = path.read_bytes()
    base = struct.unpack_from('<I', d, 0x104)[0]
    n = struct.unpack_from('<I', d, 0x11C)[0]
    sh = struct.unpack_from('<I', d, 0x120)[0] - base
    secs = [struct.unpack_from('<5I', d, sh + i * 56)[1:] for i in range(n)]

    def rd(va, size):
        for sva, vs, ra, rs in secs:
            if sva <= va < sva + vs:
                off = va - sva
                b = d[ra + off:ra + min(off + size, rs)]
                return b + bytes(size - len(b))
        raise KeyError(hex(va))
    return rd


def table(rd, va, n):
    return list(struct.unpack_from('<%dI' % n, rd(va, 4 * n)))


def test_heap_kinds():
    """Heap 0 allocates down from [0x006B8388]; heap 1 (Level) up from [0x006B837C]."""
    a = by_address()
    rd = xbe_reader()
    plain = table(rd, 0x0003E094, 5)                 # sub_0003DFA0(size, heap)
    aligned = table(rd, 0x0003E1C4, 5)               # sub_0003E0B0(align, size, heap)
    assert ('mem', None, 'eax', 4, 0x0003E094, 4) in a[0x0003DFB8].ops
    assert ('mem', None, 'esi', 4, 0x0003E1C4, 4) in a[0x0003E0BE].ops
    assert plain[0] == 0x0003DFBF and writes_abs(a[0x0003DFEB], HEAP0_TOP)     # heap 0: top -= size + 0x10, 'LOCALMEM'
    assert plain[1] == 0x0003E032 and writes_abs(a[0x0003E055], LEVEL_TOP)     # heap 1: [0x006B837C] += size
    assert aligned[0] == 0x0003E13B and writes_abs(a[0x0003E15F], HEAP0_TOP)
    assert aligned[1] == 0x0003E0C5 and writes_abs(a[0x0003E0E0], LEVEL_TOP)
    assert target_of(a[0x0003E0F6]) == 0x0003DFA0                             # then the plain heap-1 path


def target_of(i):
    m = re.search(r'0x([0-9a-f]+)', i.text)
    return int(m.group(1), 16) if m else None


def test_restart_resets_heap_0_only():
    a = by_address()
    assert target_of(a[0x001CDD62]) == 0x0003E740 and ('imm', 3) in a[0x001CDD60].ops
    assert target_of(a[0x0003E7C7]) == 0x0003E240
    body = [i for i in decoded() if i.function == 0x0003E240]
    stored = {i.ops[0][4] for i in body if i.ops and i.ops[0][0] == 'mem' and i.mnemonic in ('mov', 'add') and not i.ops[0][1]}
    assert stored == {HEAP0_TOP, HEAP_FREE}                                  # top = end; free += the heap-0 bytes
    # its own buffer comes from heap 0 again: sub_0003DFA0(2 * size, 0)
    assert target_of(a[0x0003E7D8]) == 0x0003DFA0 and ('imm', 0) in a[0x0003E7D5].ops


def test_level_heap_writers_not_on_the_restart_path():
    a = by_address()
    insns = decoded()
    writers = {i.function for i in insns if writes_abs(i, LEVEL_TOP)}
    assert writers == {0x0003DF00, 0x0003DF70, 0x0003DFA0, 0x0003E0B0, 0x0003E650}
    entries = {0x0003DF00, 0x0003DF70, 0x0003E650}
    refs = {}
    for i in insns:
        t = imm_target(i) if (i.call or i.branch) else None
        if t in entries and i.function != t:
            refs.setdefault(t, set()).add(i.address)
    assert refs == {0x0003DF00: {0x0003C35B}, 0x0003E650: {0x0003D504}, 0x0003DF70: {0x001CE2BE}}
    # sub_0003DF70 writes only when [0x006B7AA8] is 0; the restart branch needs it set
    assert ('mem', None, None, 1, RESTART_FLAG, 4) in a[0x0003DF70].ops and a[0x0003DF7A].mnemonic == 'jne'
    assert a[0x0003D4B1].ops[1][4] == RESTART_FLAG and a[0x0003D4BB].mnemonic == 'je'
    assert target_of(a[0x0003D4BB]) == 0x0003D4F0 and 0x0003D504 >= 0x0003D4F0      # sub_0003E650: the other branch
    assert target_of(a[0x0003D4DC]) == 0x001CDAF0 and target_of(a[0x0003D4E1]) == 0x001CDD60
    # nothing the restart branch calls reaches a writer (direct calls and tail jumps)
    starts = {i.function for i in insns}
    graph = {}
    for i in insns:
        t = imm_target(i) if (i.call or i.branch) else None
        if t in starts and t != i.function:
            graph.setdefault(i.function, set()).add(t)
    roots = {target_of(a[x]) for x in (0x0003D4C3, 0x0003D4C8, 0x0003D4CD, 0x0003D4D2, 0x0003D4D7, 0x0003D4DC, 0x0003D4E1, 0x0003D4E6)}
    seen, todo = set(roots), list(roots)
    while todo:
        for t in graph.get(todo.pop(), ()):
            if t not in seen:
                seen.add(t)
                todo.append(t)
    assert len(seen) > 1000 and not seen & entries


def kind_push(a, call, back):
    """The heap argument: the back-th push before the call (in this function, straight-line)."""
    addr = call
    insns = sorted((i for i in a.values() if i.function == a[call].function), key=lambda i: i.address)
    idx = [i.address for i in insns].index(call)
    n = 0
    for i in reversed(insns[:idx]):
        if i.mnemonic == 'push':
            n += 1
            if n == back:
                return i
        if i.call or i.mnemonic in ('ret', 'jmp'):
            break
    raise AssertionError(hex(addr))


# The stock allocators MapMaker areas call to make boss blocks: every one is heap 1.
STOCK = {
    0x00287E00: [0x00287E07],                        # giant skeleton state 0x4C7C (realbosses)
    0x002A62C0: [0x002A62C7],                        # boss particle block 0x7D00 (realbosses, crowe)
    0x002976F0: [0x0029771C],                        # Crow walker state 0x64DC (realbosses)
    0x0028D6D0: [0x0028D6F7],                        # split Crowe state 0x2E04 (crowe)
    0x00292630: [0x00292634],                        # giant robot state 0x4C (robot)
    0x00378400: [0x00378420],                        # effect type node 0xC (worm)
}


def test_stock_boss_blocks_are_level_heap():
    a = by_address()
    for f, sites in STOCK.items():
        calls = {i.address for i in decoded() if i.function == f and i.call and target_of(i) in (0x0003DFA0, 0x0003E0B0)}
        assert calls == set(sites), hex(f)
        for c in sites:
            assert ('imm', 1) in kind_push(a, c, 2).ops, hex(c)
    # sub_002A8300 (walker / Crowe effect blocks): two branches, each push 1 first
    assert ('imm', 1) in a[0x002A8306].ops and ('imm', 1) in a[0x002A831E].ops and ('imm', 1) in a[0x002A834C].ops
    assert {target_of(a[x]) for x in (0x002A8319, 0x002A832A, 0x002A8347, 0x002A8358)} == {0x0003DFA0}
    # the worm blocks: sub_0003E0B0(0x20, 0x1590, 1)
    assert target_of(a[0x001D1E89]) == 0x0003E0B0 and ('imm', 1) in a[0x001D1E80].ops


ALLOC = re.compile(r'guest_call\(\s*(sub_0003DFA0|sub_0003E0B0)\s*,\s*(\d)\s*,([^;]*)\);?')


def test_area_allocations_are_level_heap():
    """Every guest heap call in a MapMaker area asks for heap 1 (the last argument)."""
    found = 0
    for path in sorted((ROOT / 'src/interface').glob('pc_mapmaker_*.inc')):
        text = path.read_text(encoding='utf-8')
        for m in ALLOC.finditer(text):
            found += 1
            args = m.group(3).rstrip(')').split(',')
            assert args[-1].strip().rstrip(')') in ('1u', '1'), f'{path.name}: {m.group(0)}'
    assert found >= 18                               # actors, grid, limits, objects (12), rooms, worm


def test_scan_catches_a_heap_0_call():
    """The source scan's own check: a heap-0 call is refused."""
    bad = 'p=guest_call(sub_0003DFA0,2,bytes,0u);'
    m = ALLOC.search(bad)
    assert m and m.group(3).split(',')[-1].strip().rstrip(')') not in ('1u', '1')


def test_in_place_set_up_matches_the_stock_inits():
    """What rbs_skel_again / rbs_walk_again / crw_again run after zeroing the
    blocks: the stock inits' non-allocating rest, in the same order."""
    a = by_address()
    # skeleton: state, particles (sub_002A62C0), then sub_00287860
    assert target_of(a[0x00287E14]) == 0x002A62C0 and target_of(a[0x00287E19]) == 0x00287860
    # walker: effect blocks (sub_002A8300), particles, state, grid (8, 8, 1.0, state + 0x5C4), sub_00297690
    assert target_of(a[0x00297708]) == 0x002A8300 and target_of(a[0x00297710]) == 0x002A62C0
    assert ('imm', 0x5C4) in a[0x00297726].ops and a[0x0029772B].mnemonic == 'push'
    assert ('imm', 0x3F800000) in a[0x0029772C].ops and ('imm', 8) in a[0x00297731].ops and ('imm', 8) in a[0x00297733].ops
    assert target_of(a[0x00297735]) == 0x002BD480 and ('imm', 0x18) in a[0x0029773A].ops
    assert target_of(a[0x0029773D]) == 0x00297690
    # sub_002A8300's sizes: count 0x1F4 -> 0x55F0, 0x578 -> 0xF0A0 (pc_mapmaker_bossfx.inc)
    assert ('imm', 0x55F0) in a[0x002A830A].ops and ('imm', 0x1F4) in a[0x002A830F].ops
    assert ('imm', 0xF0A0) in a[0x002A8338].ops and ('imm', 0x578) in a[0x002A833D].ops
    assert all(a[x].ops[0][4] == 0x0078B08C for x in (0x002A830F, 0x002A833D))
    # the window words they write: [0x0078B08C], [0x0078B098], [0x0078B090]; sub_002A62C0 [0x0078B0D0]
    stores = {i.ops[0][4] for i in decoded() if i.function in (0x002A8300, 0x002A62C0)
              and i.ops and i.ops[0][0] == 'mem' and writes_abs(i, i.ops[0][4])}
    assert stores == {0x0078B08C, 0x0078B098, 0x0078B090, 0x0078B0D0}
    # split Crowe: effect blocks, state, tail sub_0028D0C0 (crw_alloc then makes the particle block)
    assert target_of(a[0x0028D6E8]) == 0x002A8300 and target_of(a[0x0028D704]) == 0x0028D0C0
    assert not any(i.call and target_of(i) in (0x0003DFA0, 0x0003E0B0) for i in decoded()
                   if i.function in (0x00287860, 0x00297690, 0x002BD480, 0x0028D0C0))
