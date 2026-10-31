"""Every turret-pool, perception and object-pool constant is patched (objects.json), hooked or reviewed.

docs/mapmaker/mapmaker-objects.md raises the 32-slot turret pools to 80, the per-player
perception slice, and (by hook, scenes 100/102) the 550-object engine pool.
Decoded with the lifter's decoder over all game code (any instruction length).
"""
from mapmaker_decoded import decoded, neg, patched, refs_functions, reviewed, unclassified

# 32 x 0x1DC gun / 32 x 0x130 owner bytes, their dword clears, and folded forms
TURRET_SIZES = neg({0x3B80, 0x2600, 0xEE0, 0x980, 0x770 * 8, 0x5DC0})
# gun / owner pools [0x00732D3C]/[0x00732D40], free lists 0x00732D2C/0x00732D1C, count [0x00732D44]
TURRET_GLOBALS = {0x00732D3C, 0x00732D40, 0x00732D2C, 0x00732D1C, 0x00732D44, 0x00732D30, 0x00732D20}
TURRET_FUNCS = {0x00359550, 0x003596F0}          # System Link sync send / receive
SLOT_COUNTS = {0x1E, 0x1F, 0x20, 0x21}
PERCEPTION = neg({0x3E0})
OBJECT_POOL = neg({0x225, 0x226, 0x227})

REVIEWED = reviewed("""
00014351 movss [edi+0x980]: float field of another struct, not a turret size
00105110 movss [edi+0x980]: float field of another struct
00105531 mulss [ebx+0x980]: float field of another struct
00290E81 mov eax,[ecx+edx*8+0x980]: array at +0x980 of another struct
002F60D6 push 0x980: argument of an editor menu call, not a pool size
0034AC73 cmp edi,0x20 in sub_00349E40: loop over 32 level objects via sub_0006BB90(i, ...), not turret slots (the turret HUD loop at 0x0034A75E is bounded by the count [0x00732D44])
002953EC cmp ecx,0x3E0 in sub_00295310: (n << 5) bound of another buffer, not perception
0029A1A2 add edx,0x3E0: field +0x3E0 of [0x0078B1E8], not perception
0029A1BD add edx,0x3E0: same field
0008DC07 add eax,0x226: twin pool 550 (sub_0008DBF0), replaced in scenes 100/102 by pc_mapmaker_objects_twin_pool; scene 101 formula kept on purpose
002136A0 cmp eax,0x225 in sub_00213580: a type/id word read through sub_00059CF0 (dispatches to sub_003734E0), not a pool size
002136B0 cmp [esi],0x227: same id word (dispatches to sub_00373500)
000A5C15 add eax,0x226: object pool 550 (sub_000A5C00), replaced in scenes 100/102 by pc_mapmaker_objects_pool; scene 101 formula kept on purpose
""")


def candidate_sites():
    insns = decoded()
    turret_funcs = refs_functions(insns, TURRET_GLOBALS) | TURRET_FUNCS
    found = {}
    for i in insns:
        if i.branch or i.call:
            continue
        vals = list(i.values())
        hit = any(v in TURRET_SIZES for k, v in vals)
        hit |= any(k == 'imm' and v in PERCEPTION | OBJECT_POOL for k, v in vals)
        hit |= any(k == 'disp' and v in OBJECT_POOL and not i.frame() for k, v in vals)
        if (i.function in turret_funcs and i.mnemonic in ('cmp', 'mov', 'push', 'imul', 'lea')
                and any(k == 'imm' and v in SLOT_COUNTS for k, v in vals)):
            hit = True
        if hit:
            found[i.address] = f'{i.mnemonic} {i.text}'
    return found


def test_every_objects_constant_is_classified():
    table = patched('objects')
    found = candidate_sites()
    assert not unclassified(found, table, REVIEWED)
    assert set(REVIEWED) <= set(found) and not set(REVIEWED) & table
    # the six pool sizes, the sync bound and the five perception sizes are all among the rewrites
    for a in (0x000B3BE2, 0x000B3BEE, 0x000B3A63, 0x000B3A70, 0x000B3A90, 0x000B3AA8, 0x003596B3,
              0x000D301C, 0x000D3084, 0x000D39DE, 0x000D3B60, 0x000CF33C):
        assert a in found and a in table, hex(a)
