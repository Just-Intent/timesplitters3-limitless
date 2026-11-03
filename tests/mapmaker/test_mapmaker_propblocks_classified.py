"""Class-0x80 prop blocks, classified over all game code.

v0.4.0-mapmaker.37 playtest (459 pieces, about 500 items, many barrels):
shooting a barrel crashed in sub_000FF200 with esi = 0xFFFF0000, called from
the class-0x80 release sub_000A92F0 (0x000A9321, fourth of the ten effect
handles at block+0x100). Cause (docs/agent-context/limitless-caps.md "Prop
blocks"): the class init sub_000AC940 gives every prop whose template lacks
flag 8 a 0x160-byte block (object +0x7C) from the pointer table [0x0072F728],
indexed by the count [0x007327A8]. sub_000A92A0 sized that table 60 (push
0xF0, rep stosd 0x3C) and nothing bounds the index, so prop 61 on read a
pointer past the table: a foreign non-zero dword became its block (zeroed and
then overwritten by its real owner), a zero one made the init store a fresh
block pointer into the neighbouring allocation. objects.json obj-propblocks-*
raise the table to 2048 entries in every scene.

Decoded with the lifter's decoder over the original XBE (mapmaker_decoded.py).
"""
import json

from mapmaker_decoded import ROOT, decoded, patched, reviewed, unclassified

TABLE = ROOT / 'config/guest-code-patches/objects.json'
POINTERS, COUNT = 0x0072F728, 0x007327A8          # block pointer table, its index
RING_BLOCKS, RING_INDEX, RING_SIZE = 0x0072F730, 0x007327A4, 0x007313EC
RING = range(0x0072F6B0, 0x0072F728, 4)           # flag-8 ring owners, 30 slots
GLOBALS = {POINTERS, COUNT, RING_BLOCKS, RING_INDEX, RING_SIZE}
SETUP = 0x000A92A0                                # allocates the ring, the spare block and the table
SETUP_SIZES = {0xF0, 0x3C, 0x2940, 0x160}
BLOCK = 0x160

REVIEWED = reviewed("""
# sub_000A9090 (level start and restart): ring owners and counts reset
000A91BD mov [0x7313ec],0x1e: ring size 30 (= the 30 owner slots and the 0x2940 B ring)
000A91D3 mov [0x7313ec],0xf: ring size 15 (2 players)
000A91E9 mov [0x7313ec],7: ring size 7 (3-4 players)
000A91F5 mov ecx,[0x7313ec]: ring size read for the clamp to >= 1
000A9204 mov [0x7313ec],eax: clamp to 1
000A9210 mov edi,[0x72f730]: ring blocks base for the clear (0xA50 dwords = 0x2940 B)
000A9221 mov [0x7327a8],eax: block count reset to 0
000A926D mov [0x7327a4],0: ring index reset
# sub_000A92A0: allocations
000A92A3 push 0x2940: ring blocks, 30 x 0x160
000A92AF push 0x160: one spare block at [0x72f6a8]
000A92B4 mov [0x72f730],eax: ring blocks pointer
000A92D4 mov [0x72f728],edi: block pointer table (size and clear patched at 0x000A92C0 / 0x000A92DA)
# sub_000A92F0 (class 0x80 release): ring owner cleared, bounded loop over the 30 slots
000A93C6 mov eax,0x72f6b0: loop start
000A93DD cmp eax,0x72f728: loop end
# sub_000AB610: the loader's cancel (0x0006D1F9 only) gives the last index back
000AB610 dec [0x7327a8]
# sub_000AC940 (class 0x80 init)
000AC97A mov eax,[0x7327a4]: flag 8: ring index (< ring size <= 30)
000AC97F mov ecx,[eax*4+0x72f6b0]: previous owner of that ring slot, released
000AC990 mov eax,[0x7327a4]: ring index re-read
000AC998 mov edx,[0x72f730]: ring blocks base, block = base + index x 0x160
000AC9A6 mov [eax*4+0x72f6b0],esi: new owner
000AC9B1 idiv [0x7313ec]: next index modulo the ring size
000AC9BA mov [0x7327a4],edx: next index
000AC9C2 mov eax,[0x7327a8]: no flag 8: the unbounded index into the pointer table
000AC9C7 mov edx,[0x72f728]: pointer table base (entry read; 0 allocates a block)
000AC9E0 mov ecx,[0x72f728]: same base, the new block's pointer stored there
000AC9E6 mov edx,[0x7327a8]: same index
000ACA04 inc [0x7327a8]: the only increment
""")


def sites(insns):
    found = {}
    for i in insns:
        hit = False
        for op in i.ops:
            if op[0] == 'mem' and not op[1] and not op[2] and op[4] in GLOBALS:
                hit = True
            elif op[0] == 'mem' and not op[1] and op[4] in RING:
                hit = True
            elif op[0] == 'imm' and (op[1] in GLOBALS or op[1] in RING):
                hit = True
        if i.function == SETUP and any(k == 'imm' and v in SETUP_SIZES for k, v in i.values()):
            hit = True
        if hit:
            found[i.address] = f'{i.mnemonic} {i.text}'
    return found


def _entries():
    return {e['id']: e for e in json.loads(TABLE.read_text(encoding='utf-8'))['patches']}


EXPECTED_WRITERS = {0x000A9221, 0x000AB610, 0x000ACA04}   # reset, loader cancel, init


def count_writers(insns):
    return {i.address for i in insns if i.ops and i.ops[0][0] == 'mem' and not i.ops[0][1]
            and not i.ops[0][2] and i.ops[0][4] == COUNT and i.mnemonic in ('mov', 'inc', 'dec', 'add', 'sub')}


def _ring_owner_stores(insns):
    """Writes into the 30 ring owner slots that are not sub_000A9090's reset."""
    return [i for i in insns if i.mnemonic == 'mov' and i.ops and i.ops[0][0] == 'mem'
            and not i.ops[0][1] and i.ops[0][4] in RING and i.function != 0x000A9090]


def test_every_prop_block_site_is_classified():
    insns = decoded()
    found = sites(insns)
    reset = {i.address for i in insns if i.function == 0x000A9090 and i.mnemonic == 'mov'
             and i.ops[0][0] == 'mem' and i.ops[0][4] in RING}
    assert len(reset) == 30                                 # every ring owner cleared at level start
    table = patched('objects')
    assert not unclassified(found, table, REVIEWED, reset)
    assert set(REVIEWED) <= set(found) and not set(REVIEWED) & table
    for a in (0x000A92C0, 0x000A92DA):                      # table bytes, clear dwords
        assert a in found and a in table, hex(a)


def test_index_only_moves_at_init_and_loader_cancel():
    """[0x007327A8] counts props given a table block; releases never lower it."""
    insns = decoded()
    assert count_writers(insns) == EXPECTED_WRITERS
    cancel = [i.address for i in insns if i.call and ('imm', 0x000AB610) in i.ops]
    assert cancel == [0x0006D1F9]


def test_ring_stays_inside_its_30_blocks():
    insns = decoded()
    sizes = [i.ops[1][1] for i in insns if i.mnemonic == 'mov' and i.ops[0][0] == 'mem'
             and not i.ops[0][1] and i.ops[0][4] == RING_SIZE and i.ops[1][0] == 'imm']
    assert sizes and max(sizes) == len(RING) == 30
    assert ('imm', 30 * BLOCK) in next(i for i in insns if i.address == 0x000A92A3).ops
    assert [i.address for i in _ring_owner_stores(insns)] == [0x000AC9A6]


def test_table_holds_every_pool_slot_twice():
    e = _entries()
    alloc = int.from_bytes(bytes.fromhex(e['obj-propblocks-alloc']['new_hex'])[1:], 'little')
    clear = int.from_bytes(bytes.fromhex(e['obj-propblocks-clear']['new_hex'])[1:], 'little')
    assert e['obj-propblocks-alloc']['original_hex'] == '68f0000000'      # stock 60 pointers
    assert alloc == clear * 4 and clear >= 2 * 1023


def test_classification_detects_a_missing_patch():
    """Mutation: without obj-propblocks-alloc the stock 60-entry size is unclassified."""
    insns = decoded()
    table = patched('objects') - {0x000A92C0}
    assert '0x000A92C0' in unclassified(sites(insns), table, REVIEWED)


def test_classification_detects_a_second_count_writer():
    """Mutation: a store to the count elsewhere is caught by the writer check."""
    insns = decoded()
    k = next(n for n, i in enumerate(insns) if i.address == 0x000AC9C2)   # mov eax,[0x7327a8]
    i = insns[k]
    mutated = list(insns)
    mutated[k] = type(i)(i.address, i.function, 'mov', i.text, i.hex, False, False, (i.ops[1], i.ops[0]))
    assert count_writers(mutated) == EXPECTED_WRITERS | {0x000AC9C2}
