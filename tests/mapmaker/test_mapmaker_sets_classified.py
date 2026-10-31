"""Decoder classification of every slot-count constant around the character set.

Scans (lifter's decoder) every function that touches a character set: the
slot work copy 0x0075FD90..0x0075FEB8, the play builders' sets 0x0075FB58 /
0x00762DC0, the preset tables 0x0053E41C / 0x0053D2D0, the editor set
(editor +0x11860..+0x11903), or that calls a set getter, the spawn lookup,
the preload or the level-load copy. Every immediate 9, 10, 19 or 20 and every
work-copy end pointer in them is either relied on / handled by
pc_mapmaker_sets.inc or reviewed below (compares against game-mode bytes are
classified automatically). A new unclassified site fails the test.

Also records why weapon sets are not raised: the six weapon slots are bound
to six weapon and six ammo item types.
"""
from mapmaker_decoded import decoded, refs_functions, reviewed, unclassified

SET_GLOBALS = {0x0075FD90, 0x0075FDA0, 0x0075FDA8, 0x0075FEA8, 0x0075FEB8, 0x0075FB58,
               0x00762DC0, 0x0053E41C, 0x0053D2D0, 0x0053D2D4}
SET_CALLEES = ('0x2c0290', '0x312950', '0x33ebc0', '0x6bc50', '0x6bc00', '0x23f320',
               '0x33ebf0', '0x2c0210', '0x2bfef0')
MODE_BYTES = {0x007DE455, 0x0078ADAE, 0x0078AD2C, 0x0078ADAD, 0x0079094C}
VALUES = (9, 10, 0x13, 0x14, 0x0075FEA8, 0x0075FEB8)

HANDLED = reviewed("""
0x0006BC55 spawn lookup sub_0006BC50: slots above 19 become 0 - the 20-slot capacity the area relies on
0x002C0371 sub_002C0310 fill: 10 work-copy rows (end 0x0075FEB8); page 2 reads members 10..19 (pc_mapmaker_sets_set_ptr)
0x002C046C sub_002C0310: clears work-copy rows past the profile bot count (10 rows); page-local
0x002C0555 sub_002C0490 reload: per-row portrait of the 10 rows; the screen hook pages them
0x002C05B9 sub_002C0490: per-row flags of the 10 rows; page-local
0x002C16D8 slot pane update sub_002C1570: the editor shows 10 rows; Down/Up at the ends page (pc_mapmaker_sets_screen)
0x00300884 compile: map-info count byte 10, the chunk is full; members 11-20 go to the fork chunk (pc_mapmaker_sets_compile)
0x0031286A sub_00312810 set-changed test compares members 0..9; pc_mapmaker_sets_changed compares 10..19
0x0031CD24 Story AI character choice: 10 options; pc_mapmaker_sets_sai_choice adds slots 11-20
""")

REVIEWED = reviewed("""
0x00158C64 game type byte [0x007DE456] value 0x13, not a slot count
0x001640EC live actor count [0x0074C37C] against 20 (actor cap), not a set slot
0x001641A2 live actor count [0x0074C37C] against 20 (actor cap), not a set slot
0x001B533F game mode value written to [0x007EB355]
0x001B53CB game mode value 10 (sub_0006A500 record), not a slot
0x001B53D6 game mode value written to [0x007EB355]
0x001B549A game mode value 10, not a slot
0x001B550A game mode value written to [0x007EB355]
0x001B55DC game mode value 10 passed on, not a slot
0x001B5610 game mode value 10 passed on, not a slot
0x0023F45D [0x007DE455] game type 10 (MapMaker) held in cl
0x0023F5EE item category word +2 values 8/9/0x13/0x14 (piece preload), not slots
0x0023F5F4 item category word +2 values 8/9/0x13/0x14 (piece preload), not slots
0x0023F5FA item category word +2 values 8/9/0x13/0x14 (piece preload), not slots
0x002412AC character set index for other game types (desc+8, read by sub_0006BC00), not a slot count
0x00241494 [0x007DE455] game type 10 held in al
0x00241551 [0x007DE455] game type 10 held in cl
0x00242400 counting loop over desc+0x34 records (min(10, +4)) with no side effect, not the set
0x002C0009 front-end store path of sub_002BFFD0 (the editor returned earlier through sub_00314210)
0x002C0129 front-end fill path of sub_002C00F0 (the editor jumped to sub_003141B0)
0x002C0184 game mode 9 compare in ebx
0x002C02A4 game mode [0x0078AD2C] values 9/10 in eax
0x002C02A9 game mode [0x0078AD2C] values 9/10 in eax
0x002C044A front-end profile bot count against the 10 Arcade bot slots
0x002C17B1 team sub-mode 0x13: 9 - profile +0xC rows (front end)
0x002C1D8E pick moves the slot cursor to the next row below row 10; page-local, no change needed
0x002C1DA7 team sub-mode 0x13: 9 - profile +0xC rows (front end)
0x002C2CC1 team sub-mode 0x13: 9 - profile +0xC rows (front end)
0x002C2D16 sub_002C2CA0 front-end walk of the 10 Arcade bot slots
0x002C2F06 sub_002C2F00 front-end team assignment from the last bot slot down
0x002C2F91 team sub-mode 0x13: 9 - profile +0xC rows (front end)
0x002D27DA start of the next table 0x0075FEA8 (sub_002D2780), not the work-copy end
0x002D2841 start of the next table 0x0075FEA8 (sub_002D2780), not the work-copy end
0x002D28B1 start of the next table 0x0075FEA8 (sub_002D2780), not the work-copy end
0x002D2CEA sub_002D2C70 switch range over the game mode
0x002D2DC1 sub-mode value 10 written to [0x0078ADAD]
0x002D2E6F sub-mode value 10 written to [0x0078ADAD]
0x002D3305 sub-mode [0x0078ADAD] value 9 in al
0x002D3373 sub-mode value 9 in bl
0x002D339A sub-mode value 10 in bl
0x002D3447 sub-mode value 9 in bl
0x002D357F game mode 9 kept in ebp for the compare at 0x002D363D
0x002D3641 game mode [0x0078AD2C] value 10 in eax
0x002D370F team sub-mode 0x13: 9 - profile +0xC rows (front end)
0x002D381C match start: the 10 work-copy rows are the match's bots (front end, pause setup, editor Test map; 16 actors 0x0078AD6C..); the editor's page 2 is refilled first (pc_mapmaker_sets_match_start); rows 11-20 become more bots after the loop (pc_mapmaker_matchbots_match_start)
0x00300F1E item id ranges (0x33/0x34/10/14) in the compile's piece walk
0x00300F79 item category word +2 values 8/9/0x13/0x14 (compile), not slots
0x00300F83 item category word +2 values 8/9/0x13/0x14 (compile), not slots
0x00300F8D item category word +2 values 8/9/0x13/0x14 (compile), not slots
0x00301811 logic record type 10 (editor +0xDD84 records), not the set
0x003022C9 item id ranges (0x33/0x34/10/14) in the editor loader
0x00302313 item category word +2 values 8/9/0x13/0x14 (editor loader), not slots
0x00302319 item category word +2 values 8/9/0x13/0x14 (editor loader), not slots
0x0030231F item category word +2 values 8/9/0x13/0x14 (editor loader), not slots
0x003027C0 logic record type 10 (editor loader), not the set
0x00313D48 test play from the editor: game type 10 in the start record
0x00313DA2 test play from the editor: game type 10 in the start record
""")


def set_functions(insns):
    fs = refs_functions(insns, SET_GLOBALS)
    for i in insns:
        if i.call and any(c in i.text for c in SET_CALLEES):
            fs.add(i.function)
        if any(k == 'disp' and 0x11860 <= v < 0x11904 for k, v in i.values()):
            fs.add(i.function)
    return fs


def test_every_slot_constant_is_classified():
    insns = decoded()
    fs = set_functions(insns)
    found = {}
    for i in insns:
        if i.function not in fs or i.address in found:
            continue
        imms = [v for k, v in i.values() if k == 'imm']
        hit = [v for v in imms if v in VALUES]
        if not hit:
            continue
        if i.mnemonic in ('push', 'add', 'sub', 'and', 'or', 'shl', 'sar', 'shr', 'test') and 0x0075FEA8 not in hit:
            continue
        if i.mnemonic == 'cmp' and any(op[0] == 'mem' and not op[1] and not op[2] and op[4] in MODE_BYTES for op in i.ops):
            continue
        found[i.address] = i.text
    assert unclassified(found, HANDLED, REVIEWED) == {}
    stale = sorted(hex(a) for a in set(HANDLED) | set(REVIEWED) if a not in found)
    assert not stale, f'classified sites no longer found: {stale}'
    assert not set(HANDLED) & set(REVIEWED)


def test_weapon_slots_are_item_types():
    """Weapon sets keep 6 slots: each slot is an item type (see docs/mapmaker/mapmaker-sets.md)."""
    a = {i.address: i for i in decoded()}
    # sub_002FE840 / sub_00303350: item ids 0x11..0x16 weapon slot id-0x11, 0x17..0x1C ammo slot id-0x17
    assert ('imm', 6) in a[0x002FE84B].ops and a[0x002FE845].mnemonic == 'lea'
    assert ('imm', 6) in a[0x002FE86C].ops
    assert ('imm', 0x30) in a[0x00303358].ops                        # item ids 0x11..0x41 by table
    # the weapon set struct is 0x44 = name + count + 15 ids (rep movsd 0x11), map info stores 6
    assert ('imm', 0x11) in a[0x0006A76F].ops and ('imm', 6) in a[0x0006A785].ops
    assert ('imm', 6) in a[0x003008D6].ops and ('imm', 0x1190C) in a[0x003008DC].ops
    # the editor's weapon check walks 6 slots (+8 .. +0x20)
    assert ('imm', 8) in a[0x003004FF].ops and ('imm', 0x20) in a[0x0030053A].ops
    # System Link copies a whole 0x44 set
    assert ('imm', 0x00761388) in a[0x002D39F6].ops and ('imm', 0x11) in a[0x002D39FB].ops
