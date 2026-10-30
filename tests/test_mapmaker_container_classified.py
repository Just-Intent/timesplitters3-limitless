"""Every map-container size constant in the game's code is patched (container.json), hooked or reviewed.

The container grows from 0x8000 to 0x80000 bytes (docs/mapmaker-container.md).
Sizes unique to it (0x7FE0 body, 0x1FF8 dwords, 0x1FF80 diff table, ...) are
scanned in all game code; the common powers of two (0x8000, 0x2000, 0x10000,
0x4000) only in functions that touch the container, its undo buffers or the
save entry, where they are most likely sizes. Decoded with the lifter's decoder
(any instruction length), not from the text listing.
"""
from mapmaker_decoded import decoded, neg, patched, refs_functions, reviewed, unclassified

RARE = neg({0x7FE0, 0x7FDC, 0x7FDD, 0x7FDE, 0x7FDF, 0x1FF8, 0x1FF80, 0x3FF0, 0x7FC0, 0x1FF0})
COMMON = {0x8000, 0x2000, 0x10000, 0x4000, 0x7FFF, 0x20000, 0xFFFF8000, 0xFFFFE000, 0xFFFF0000, 0x7FE4, 0x8020}
# container [0x00784014], undo shadow/diff/ring and their state, the save entry 0x00765A88 (+0x66 at 0x00765AEE)
POINTERS = {0x00784014, 0x00765E38, 0x00765C2C, 0x00765C24, 0x00765AEE, 0x00765C28, 0x00765C30, 0x00765A88}

# Functions a container hook replaces or whose size arguments a hook rewrites
# (docs/mapmaker-container.md "Hooks").
HOOKED = reviewed("""
00190567 sub_00190530 load: replaced by pc_mapmaker_container_load (tiered body read)
001905C9 sub_00190530 load: replaced by pc_mapmaker_container_load
001909CA sub_00190900 save: body+signature size argument rewritten by pc_mapmaker_container_save_async (ret 0x001909E3)
00190A12 sub_00190900 save: size argument rewritten by pc_mapmaker_container_save_sign (ret 0x00190A1F)
00190A39 sub_00190900 save: body argument rewritten by pc_mapmaker_container_save_sync (ret 0x00190A54)
002FE203 sub_002FE200 front-end allocation: replaced by pc_mapmaker_container_frontend_init
002FE218 sub_002FE200 zero of the new buffer: replaced by pc_mapmaker_container_frontend_init
002FE23C sub_002FE230 editor allocation: pc_mapmaker_container_editor_init installs the buffer first (original allocates only when 0)
002FE250 sub_002FE230 zero of the new buffer: as above, not reached
00336603 sub_00336600 undo ring allocation: replaced by pc_mapmaker_container_undo_ring
00336617 sub_00336600 undo ring zero: replaced by pc_mapmaker_container_undo_ring
003563AB System Link send of the container (0x8000, one message): pc_mapmaker_container_net_send refuses larger maps
0035644C System Link send of the container: pc_mapmaker_container_net_send
""")

# Kept at stock on purpose, or not a container size.
KEPT = reviewed("""
000C6256 add ecx,0x1FF8: rooms view array (sub_000C61D0, 6 x 0xFFC + 8), not the container
000C62A6 add ecx,0x3FF0: rooms view array (sub_000C61D0), not the container
0018ED49 and edx,0x7FC0: bit mask
001907BD cmp [esi+0x14C],0x7FE0: save list tags a body smaller than stock (text 0x199B); every tier is >= 0x7FE0, so stock is correct
0019D600 sub_0019D510 Xbox Live packing (0x2000 dwords): dead on PC
0019D614 sub_0019D510 Xbox Live packing: dead on PC
001C91E1 push 0x1FF8: argument pair (0x1FF8, 0x6E) stored by sub_00058C50 into a struct (+0x9C/+0xA0), not a size
001C9272 push 0x1FF8: same (sub_001C91C0)
001C9286 push 0x1FF8: same
001C9318 push 0x1FF8: same
001C932C push 0x1FF8: same
001C9394 push 0x1FF8: same
001C93A8 push 0x1FF8: same
001C93BC push 0x1FF8: same
0023F42F test [ebp+0x244],0x20000: map info flag
00240EFA or [eax+0x20],0x4000: flag
00240F07 or [eax+0x20],0x8000: flag
002413F6 or ebp,0x4000: flag
00241834 and eax,0x8000: flag
0024183D and ecx,0x20000: flag
0024184B and ecx,0x10000: flag
00241A88 or eax,0x10000: flag
002421F2 or ecx,0x10000: flag
002C8991 push 0x8000: text draw flags for sub_00075B70
002D047B push 0x8000: text draw flags for sub_00075B70
002FDAD4 and eax,0x2000: flag
002FDAE0 and eax,0x4000: flag
00300696 placement compare used + cost < 0x7FE0: kept, pc_mapmaker_container_find shifts used when TSFP_MAPMAKER_LIMITS=1
003006AB placement compare (no-container branch, cost alone): kept
00300846 or [ebx+0x254],0x8000: map info flag
00300860 or [ebx+0x254],0x10000: map info flag
0030087A or [ebx+0x254],0x20000: map info flag
0030366D placement compare: kept, gated by pc_mapmaker_container_find
00303936 placement compare: kept, gated by pc_mapmaker_container_find
003039E4 or ecx,0x4000: flag
00304442 placement compare: kept, gated by pc_mapmaker_container_find
003068A2 mov esi,0x4000: editor flag mask tested against [edi+0x25C]
00312D6E or [eax+0x238],0x8000: editor flag
0035504E System Link receive copies one 0x8000 message into the container; larger maps are never sent (net_send) and the session start zeroes the container (0x00356BD8, patched)
""")


def candidate_sites():
    insns = decoded()
    funcs = refs_functions(insns, POINTERS)
    table = patched('container')
    funcs |= {i.function for i in insns if i.address in table}
    found = {}
    for i in insns:
        if i.branch or i.call:
            continue
        vals = [v for k, v in i.values()]
        if any(v in RARE for v in vals) or (i.function in funcs and any(v in COMMON for v in vals)):
            found[i.address] = f'{i.mnemonic} {i.text}'
    return found


def test_every_container_constant_is_classified():
    table = patched('container')
    found = candidate_sites()
    assert not unclassified(found, table, HOOKED, KEPT)
    for name, group in (('hooked', HOOKED), ('kept', KEPT)):
        assert set(group) <= set(found), (name, sorted(hex(a) for a in set(group) - set(found)))
        assert not set(group) & table, name
    # every whole-body zero/copy and chunk-walk bound is rewritten
    assert {a for a, s in found.items() if s.endswith(', 0x1ff8') and s.startswith('mov ecx')} <= table
    walks = {a for a, s in found.items() if s.endswith('+ 0x7fe0]') or s.endswith(', 0x7fe0') and s.startswith('add')}
    assert walks - set(HOOKED) <= table
