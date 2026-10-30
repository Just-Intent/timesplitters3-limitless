"""Every 32/30-character constant and live-list / AI-table reference is patched (actors*.json), hooked or reviewed.

docs/mapmaker-actors.md raises characters alive to 64 in wide play. Its
inventory of the 311 character functions was classified by hand; this scan
re-checks it with the lifter's decoder (any instruction length): in every
function touching the live list, the count, the actor pool, its 0xEA0 stride,
the AI table, the pair table or the character mask, each cmp/mov/push/add/sub/
imul/lea immediate 0x1D..0x21 must be patched or reviewed below, and every
reference into the live list or the AI table must be moved.
"""
from mapmaker_decoded import decoded, patched, reviewed, unclassified

LIVE_LIST = range(0x007A357C, 0x007A3600)      # list[-1] .. list[31]; 0x007A3600 is a separate counter
AI_TABLE = range(0x007A2D00, 0x007A2D78)
GLOBALS = {0x0074C37C, 0x007356D8, 0x0074C368, 0x007A2BC8, 0x00739AA0}
COUNTS = {0x1D, 0x1E, 0x1F, 0x20, 0x21}

KEPT_REFS = reviewed("""
002354E8 [0x007A2D24]: group table 0x007A2D80 at index n-1 (stride 0x5C), not the AI table
""")

HOOKED = reviewed("""
000D693A add ecx,0x20 in sub_000D6920: pool slot formula (and 0xE4 / add 0x20 = 32 or 4), replaced in scene 100 by pc_mapmaker_actors_pool_clear
000D6977 add edx,0x20 in sub_000D6960: pool slot formula, replaced in scene 100 by pc_mapmaker_actors_pool
000D6A28 add ecx,0x20 in sub_000D6960: same formula
""")

REVIEWED = reviewed("""
000347EC push 0x20: buffer length argument of sub_003C9540 (string), not characters
0006C30E add edi,0x20: 32-byte record stride in sub_0006C1E0
0006C353 add edi,0x20: 32-byte record stride
000A3CF5 mov esi,0x20: byte offset of a 4 x 8-byte loop (sub 8 per step) in sub_000A3AE0
000C48EF push 0x1E: argument of a draw/text call, not a count
000CF1BC add ebx,0x20: 32-byte record stride in sub_000CF060
0012B44F push 0x21: argument (x87 distance helper), not a count
0014B1EE cmp eax,0x21: switch case id in sub_0014B090
0015E5AE mov ecx,0x1E: clears the 30-dword list 0x007A2880 (sub_001609F0 appends bounded by cmp 0x1E, sub_00160A10 removes; nothing iterates it; not indexed by character id)
0015F61B push 0x20: flag argument of sub_00059E00
0015F6DA push 0x20: flag argument of sub_000174E0
001BD196 cmp [0x0079094C],0x1D: scene id 29
0021BF13 cmp word [edi+0x48],0x20: character type 0x20
0023AE55 push 0x1D: argument (id), not a count
0023B0E4 push 0x21: argument (id), not a count
0023EAA9 push 0x1D: argument (id), not a count
00245CF8 cmp eax,0x1E: pickup type, sub_00245CE0
0025F216 cmp edi,0x20: 32 checkpoints, sub_0025F0C0
00277057 cmp ecx,0x1E: time difference against [0x007DE318] (30 ticks)
0027B783 mov [ebp+0xAA8],0x20: field value
002B0887 add ebx,0x20: 32-byte cover pad stride, loop to [0x007B65C0]
002B0AC4 mov ecx,0x1E: clears the 30-entry effect list 0x0075F670; its adder sub_002B24E0 is bounded (cmp [0x0075F6E8],0x1E), so a 31st entry is dropped, not written (stock limit, not per character); the other adder sub_00357D70 is System Link
002B24E0 cmp [0x0075F6E8],0x1E: the bound of that list (kept)
002B3C31 push 0x20: flag argument of sub_00017570
002B57FE push 0x20: flag argument of sub_00017570
002B5821 push 0x20: flag argument of sub_00017570
0034AC73 cmp edi,0x20: loop over 32 level objects (sub_0006BB90), not characters
00355A96 imul edx,edx,0x1E: System Link record stride (never wide)
00355B3C imul eax,eax,0x1E: System Link record stride (never wide)
00357462 cmp ebp,0x20: System Link sub_00357320 (never wide)
00357490 cmp ebp,0x21: System Link sub_00357320 (never wide)
00358119 cmp ecx,0x1F: System Link sub_003580D0 (never wide)
""")


def touches(i):
    for op in i.ops:
        if op[0] == 'mem':
            if not op[1] and not op[2] and op[4] in GLOBALS:
                return True
            if op[4] in LIVE_LIST or (not op[1] and op[4] in AI_TABLE):
                return True
        if op[0] == 'imm' and (op[1] in GLOBALS or op[1] in LIVE_LIST or op[1] in AI_TABLE or op[1] == 0xEA0):
            return True
    return False


def scan():
    insns = decoded()
    funcs = {i.function for i in insns if touches(i)}
    counts, refs = {}, {}
    for i in insns:
        if i.branch or i.call:
            continue
        if any(v in LIVE_LIST or v in AI_TABLE for k, v in i.values()):
            refs[i.address] = f'{i.mnemonic} {i.text}'
        if i.function not in funcs or i.mnemonic not in ('cmp', 'mov', 'push', 'add', 'sub', 'imul', 'lea'):
            continue
        if i.ops and i.ops[0] == ('reg', 'esp'):
            continue
        if any(k == 'imm' and v in COUNTS for k, v in i.values()):
            counts[i.address] = f'{i.mnemonic} {i.text}'
    return funcs, counts, refs


def test_every_character_constant_is_classified():
    table = patched('actors', 'actors-list')
    funcs, counts, refs = scan()
    assert len(funcs) >= 300                     # the inventory's 311 functions (alias decodes merge)
    assert not unclassified(counts, table, HOOKED, REVIEWED)
    for name, group in (('hooked', HOOKED), ('reviewed', REVIEWED)):
        assert set(group) <= set(counts), (name, sorted(hex(a) for a in set(group) - set(counts)))
        assert not set(group) & table, name


def test_every_live_list_and_ai_table_reference_is_moved():
    table = patched('actors', 'actors-list')
    _, _, refs = scan()
    assert not unclassified(refs, table, KEPT_REFS)
    assert set(KEPT_REFS) <= set(refs) and not set(KEPT_REFS) & table
    assert len(set(refs) & table) == 181 + 8
