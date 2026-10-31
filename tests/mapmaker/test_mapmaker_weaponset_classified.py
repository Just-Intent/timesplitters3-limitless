"""Decoder classification of every slot-count constant around the weapon set.

Scans (lifter's decoder) every function that touches a weapon set: the play
copies 0x006F1E28 / 0x0075FC78, the System Link copy 0x00761388, the
non-editor map copy 0x00762E68, the editor set (editor +0x11904..+0x11947),
or that calls a set getter / setter / lookup. Every compare or store of 5, 6
or 0xF in them is either handled by pc_mapmaker_weaponset.inc or reviewed
below (compares against game-mode bytes are classified automatically). A new
unclassified site fails the test. Also: no user of sub_0006BD30's result
copies it as a fixed 0x44 struct (the play hook returns a longer one).
"""
from mapmaker_decoded import decoded, refs_functions, reviewed, unclassified

SET_GLOBALS = {0x006F1E28, 0x006F1E2C, 0x006F1E30, 0x0075FC78, 0x0075FC7C, 0x0075FC80,
               0x00761388, 0x00762E68, 0x00762E6C, 0x00762E70}
SET_CALLEES = ('0x6bd30', '0x2d3a30', '0x2d3b00', '0x33e6c0', '0x2be7c0', '0x312a60', '0x312ae0',
               '0x6bf10', '0x2bff90', '0x6bec0', '0x312880')
MODE_BYTES = {0x007DE455, 0x007DE456, 0x0078ADAE, 0x0078AD2C, 0x0078ADAD, 0x0079094C}
VALUES = (5, 6, 0xF)
SKIP = ('push', 'add', 'sub', 'and', 'or', 'shl', 'sar', 'shr', 'test', 'imul', 'lea')

HANDLED = reviewed("""
0x002BF990 sub_002BF910 pick: the cursor moves on below row 6 (value 5); page-local, pc_mapmaker_weaponset_set stores pages 2-4 in slots 7-20
0x002D3BCA sub_002D3B00: slots 6+ answer 0 (the 6-row screen getter); rows on pages 2-4 are answered by pc_mapmaker_weaponset_get
0x002FE84B sub_002FE840 editor model: weapon items 0x11..0x16 are slots 1-6; an item on slot 7+ keeps its item type's model (cosmetic, documented)
0x002FE86C sub_002FE840 editor model: ammo items 0x17..0x1C are slots 1-6 (as above)
0x003128CF sub_00312880 weapon-set-changed test compares the 15 ids; pc_mapmaker_weaponset_changed compares slots 7-20
""")

REVIEWED = reviewed("""
0x0006A785 sub_0006A760 front-end copy: a set whose count is 0 gets 6; the play hook keeps ids 0..5 and the count's extra ids
0x0006BDEF sub_0006BD30 other scenes: a 6-slot set from sub_002D3B00, not the MapMaker map (scene 100 copies the map's set at 0x0006BDD3)
0x0006D948 pickup record +8 = 5 for the type-1 item, not a slot
0x0006DD42 object record (0x6C stride) type 0xF, not a slot
0x0006DEAB object record (0x6C stride) type 0xF, not a slot
0x0006E848 pickup record +8 = 5 for the type-1 item, not a slot
0x001C8881 [0x007DE470] value 0xF (weapon file loader special case), not a slot
0x001C8902 weapon id 5 left out of the weapon file loader's list, not a slot
0x001C8920 weapon id 6 left out of the weapon file loader's list, not a slot
0x00240DBB character set index 0x11 (map info +0x14 - 0xC == 5), the character set's level-load copy
0x00240E18 [0x00790948] switch range 0..5, not a slot
0x002415CD pickup record +8 = 5 for item 0x21 (fill pass), not a slot
0x002419E5 item ids 0x32..0x37 (turrets) switch range
0x00241B43 item ids 0x32..0x37 (turrets) switch range
0x002B4F89 [0x007DE470] value 0xF in the AI weapon choice, not a slot
0x002B504D [0x007DE470] value 0xF in the AI weapon choice, not a slot
0x002D3A67 game type [0x0078AD2C] 6 held in eax
0x002D3B32 game type [0x0078AD2C] 6 held in eax
0x002D415B weapon set index 0xF = the map's set in MapMaker Arcade (not editable there); editor paging needs index >= 10
0x003022ED item ids 5/6 (starts) in the editor loader
0x003022F6 item ids 5/6 (starts) in the editor loader
0x00356519 [0x0077419C] value 6 (network state) in the System Link setup, not a slot
""")


def set_functions(insns):
    fs = refs_functions(insns, SET_GLOBALS)
    for i in insns:
        if i.call and any(c in i.text.split(';')[0] for c in SET_CALLEES):
            fs.add(i.function)
        if any(k == 'disp' and 0x11904 <= v < 0x11948 for k, v in i.values()):
            fs.add(i.function)
    return fs


def test_every_slot_constant_is_classified():
    insns = decoded()
    fs = set_functions(insns)
    found = {}
    for i in insns:
        if i.function not in fs or i.address in found:
            continue
        if not any(v in VALUES for k, v in i.values() if k == 'imm') or i.mnemonic in SKIP:
            continue
        if i.mnemonic == 'cmp' and any(op[0] == 'mem' and not op[1] and not op[2] and op[4] in MODE_BYTES for op in i.ops):
            continue
        found[i.address] = i.text
    assert unclassified(found, HANDLED, REVIEWED) == {}
    stale = sorted(hex(a) for a in set(HANDLED) | set(REVIEWED) if a not in found)
    assert not stale, f'classified sites no longer found: {stale}'
    assert not set(HANDLED) & set(REVIEWED)


def test_no_fixed_copy_of_the_play_set():
    """No caller of sub_0006BD30 copies a 0x44 set (rep movsd 0x11) or holds 0x11 as a length."""
    insns = decoded()
    fs = {i.function for i in insns if i.call and '0x6bd30' in i.text.split(';')[0]} - {0x0006BD30}
    assert fs == {0x0015B830, 0x001B6050, 0x001C8540, 0x00272800, 0x002B0500, 0x002B1B60, 0x002B4C70,
                  0x002B53F0, 0x002B9990, 0x0006BEC0, 0x0006BF10, 0x0006C620, 0x0006C920, 0x000CEF30}
    copies = [hex(i.address) for i in insns if i.function in fs and ('imm', 0x11) in i.ops and i.mnemonic != 'cmp']
    assert not copies
