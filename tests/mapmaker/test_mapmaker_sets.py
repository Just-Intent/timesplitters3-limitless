"""MapMaker custom character set: 20 slots (fork-only chunk, paged slot screen).

Compiles src/interface/pc_mapmaker_sets.inc against fake guest memory (with
mutations that must be caught) and, when the private XBE is available,
checks the stock facts the hooks rely on: every hooked call site and return
address, the 20-member set structs, the 10-member map-info form, the slot
screen descriptors, the Story AI choice loop, the play set block, and that
the fork chunk tag is unused by the game.
"""
import os
from pathlib import Path
import struct
import subprocess
import pytest

from native_compiler import native_compiler
from mapmaker_decoded import decoded, _private

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_sets.inc'

MUTATIONS = {
    'always-chunk': ('    if(!high && !sets_extras_used(e)) return;\n', ''),
    'ignores-story-ai': ('SETS_B(data+SETS_SAI_SLOT)>=SETS_STOCK) *(int *)ctx=1;', 'SETS_B(data+SETS_SAI_SLOT)>=SETS_STOCK) (void)ctx;'),
    'used-stale': ('    WORD_AT(info+0x248u)=used;\n', ''),
    'terminator-offset': ('WORD_AT(nterm+4u)=nterm-c;', 'WORD_AT(nterm+4u)=0;'),
    'no-room-check': ('used+8u+SETS_CHUNK+4u>SETS_SCAN', 'used>SETS_SCAN'),
    'eax-stale': ('        g_eax=used;\n', ''),
    'load-keeps-junk': ('    sets_clear_extras(e);\n    if(!(data=sets_chunk(c))) return;', '    if(!(data=sets_chunk(c))) return;'),
    'load-variant-unsigned': ('SETS_H(m+6u)=(int16_t)(int8_t)SETS_B(d+3u);', 'SETS_H(m+6u)=(int16_t)SETS_B(d+3u);'),
    'changed-ignores-extras': ('    if(g_eax || !(e=sets_editor())) return;\n', '    return;\n'),
    'changed-ignores-variant': ('(id!=-1 && SETS_H(m+6u)!=variant)', '(0 && variant)'),
    'reset-skips': ('    if((e=sets_editor())) sets_clear_extras(e);\n', '    (void)e;\n'),
    'flip-loses-pick': ('            guest_call(sub_002BFFD0,0);           /* keep this page\'s last change */\n', ''),
    'fill-wrong-page': ('    if(ret==SETS_FILL_RET && sets.in_update && sets.page && g_eax) g_eax+=8u*SETS_STOCK;',
                        '    if(ret==SETS_FILL_RET && sets.in_update && sets.page && g_eax) g_eax+=8u*(SETS_STOCK-1u);'),
    'fill-any-time': ('    if(ret==SETS_FILL_RET && sets.in_update && sets.page && g_eax)', '    if(ret==SETS_FILL_RET && sets.page && g_eax)'),
    'store-no-scratch': ('    else if(ret==SETS_STORE_RET && sets.in_store) g_eax=SETS_SCRATCH;\n', ''),
    'no-reload': ('            WORD_AT(SETS_LOADED)=0xFFFFFFFFu;     /* the original reloads the work copy */\n', ''),
    'no-cursor': ('            sets_select(pane,sets.page?0u:SETS_STOCK-1u);\n', ''),
    'keeps-direction': ('            WORD_AT(pane+0xC4u)&=~0x000F000Fu;WORD_AT(pane+0xC8u)&=~0xFu;\n', ''),
    'page-sticks': ('    if(!sets.forced && WORD_AT(SETS_LOADED)!=WORD_AT(SETS_INDEX)) sets.page=0;   /* opened or index changed */\n', ''),
    'screen-always-on': ('    if(!sets_custom() || !MAPMAKER_STACK_OK(8) || !sets_on()) { sets.page=0;original();return; }',
                         '    if(!sets_custom() || !MAPMAKER_STACK_OK(8)) { sets.page=0;original();return; }'),
    'screen-skips-original': ('    original();                                   /* pops the return address */\n    sets.in_update=0;',
                              '    g_esp+=4u;\n    sets.in_update=0;'),
    'no-title': ('    if(MAPMAKER_PTR_OK(menu,0x208u)) sets_title(menu);\n', ''),
    'choice-any-caller': ('WORD_AT(g_esp)!=SETS_CHOICE_RET || !sets_custom()', '0 || !sets_custom()'),
    'choice-ignores-switch': ('    if(!sets_on() && !(MAPMAKER_PTR_OK(value,4)', '    if(0 && !(MAPMAKER_PTR_OK(value,4)'),
    'choice-empty-name': ('    if(id==-1) return guest_call(sub_0003D7B0,1,0xD7Cu);', '    if(0) return 0;'),
    'choice-no-realboss-row': ('    if(row==0xFFFFFFFFu && id==(int32_t)SETS_RBS_ID) row=SETS_RBS_ROW;\n', ''),
    'choice-variant-lost': ('row=guest_call(sub_002BE600,2,(uint32_t)id,(uint32_t)(int32_t)SETS_H(m+6u));',
                            'row=guest_call(sub_002BE600,2,(uint32_t)id,0u);'),
    'icon-skips': ('WORD_AT(g_esp+4u)=item-SETS_STOCK;', '(void)item;'),
    'icon-any-caller': ('        if(ret==SETS_ICON_LIST_RET || ret==SETS_ICON_MAP_RET) {', '        if(ret) {'),
    'icon-one-caller': ('        if(ret==SETS_ICON_LIST_RET || ret==SETS_ICON_MAP_RET) {', '        if(ret==SETS_ICON_LIST_RET) {'),
    'icon-range': ('item<SETS_SLOT_ITEM+SETS_SLOTS)', 'item<=SETS_SLOT_ITEM+SETS_SLOTS)'),
    'play-count': ('    WORD_AT(dst)=SETS_SLOTS;\n', ''),
    'play-empty-ai': ('            SETS_H(SETS_MEMBER(dst,k))=SETS_H(SETS_MEMBER(dst,first));\n', ''),
    'play-any-caller': ('WORD_AT(g_esp)!=SETS_PLAY_RET) { original();return; }', '0) { original();return; }'),
    'preload-empty': ('        if(id!=-1) guest_call(sub_000D7780', '        if(1) guest_call(sub_000D7780'),
    'match-no-refill': ('        guest_call(sub_002C0310,0);\n', ''),
    'match-any-scene': ('    if(sets.page && WORD_AT(SETS_SCENE)==102u && (int32_t)WORD_AT(SETS_INDEX)>=0xC) {',
                        '    if(sets.page && (int32_t)WORD_AT(SETS_INDEX)>=0xC) {'),
    'match-keeps-page': ('        sets.page=0;\n        g_eax=WORD_AT(SETS_INDEX);', '        g_eax=WORD_AT(SETS_INDEX);'),
    'match-eax-lost': ('        g_eax=a;\n', ''),
    'preload-any-caller': ('    if(ret!=SETS_PRELOAD_RET || !(data=', '    if(!(data='),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('sets.exe' if os.name == 'nt' else 'sets')
    source = ROOT / 'tests/mapmaker/mapmaker_sets.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/wd4706',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src'),
                str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), '-I' + str(ROOT / 'src'),
                str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_sets(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker sets ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_no_guest_code_table_and_over_stock():
    """Wrappers only; the container's over-stock check knows the fork tag."""
    assert not (ROOT / 'config/guest-code-patches/sets.json').exists()
    container = (ROOT / 'src/interface/pc_mapmaker_container.inc').read_text(encoding='utf-8')
    assert 'if(tag==0x00C776F5u) return 1;' in container
    assert '#define SETS_TAG 0x00C776F5u' in INC.read_text(encoding='utf-8')


# ---- stock facts (skipped without the private XBE) --------------------------

def by_address():
    return {i.address: i for i in decoded()}


def mem(i):
    return [op for op in i.ops if op[0] == 'mem']


def ret_of(a, site, target):
    call = a[site]
    assert call.call and f'0x{target:x}' in call.text, (hex(site), call.text)
    return site + len(bytes.fromhex(call.hex))


def test_hook_return_addresses():
    a = by_address()
    assert ret_of(a, 0x002C0314, 0x2c0290) == 0x002C0319 and a[0x002C0314].function == 0x002C0310
    assert ret_of(a, 0x003141B0, 0x312950) == 0x003141B5
    assert ret_of(a, 0x00314213, 0x312950) == 0x00314218
    assert ret_of(a, 0x0031CD2D, 0x78590) == 0x0031CD32 and a[0x0031CD2D].function == 0x0031CB90
    assert ret_of(a, 0x00240DCE, 0x33ebf0) == 0x00240DD3
    assert ret_of(a, 0x0023F472, 0x23f320) == 0x0023F477
    assert ret_of(a, 0x0032C6FA, 0x3006c0) == 0x0032C6FF
    # the only callers of the wrapped play/editor routines that the hooks act on
    callers = {i.function for i in decoded() if i.call and '0x33ebf0' in i.text}
    assert callers == {0x00240C50}
    callers = {i.address for i in decoded() if i.call and '0x23f320' in i.text}
    assert callers == {0x0023F472}
    assert ret_of(a, 0x00313353, 0x312810) == 0x00313358
    callers = {i.address for i in decoded() if i.call and '0x312810' in i.text}
    assert callers == {0x00313353}
    # sub_00312810 compares the chunk with the undo shadow sub_003365F0 = [0x00765E38], members 0..9
    assert '0x3365f0' in a[0x00312819].text and ('imm', 0xA) in a[0x0031286A].ops
    assert mem(a[0x003365F0])[0][4] == 0x00765E38
    callers = {i.function for i in decoded() if (i.call or i.branch) and '0x314210' in i.text}
    assert callers == {0x002BFFD0}


def test_set_structs_hold_twenty():
    a = by_address()
    # rep stosd 0x29 dwords = 0xA4 = count + 20 x 8 (play builder, non-editor getter)
    for site, buf in ((0x002C0231, 0x0075FB58), (0x00312967, 0x00762DC0)):
        assert ('imm', 0x29) in a[site].ops and ('imm', buf) in a[site + 5].ops
    # the editor set is editor +0x11860, the weapon set follows at +0x11904 = +0xA4
    assert ('imm', 0x11860) in a[0x0031295E].ops and ('imm', 0x11904) in a[0x00312A6E].ops
    # new map: the preset (0xA4 each) is copied whole into the editor set
    assert ('imm', 0xA4) in a[0x002F9F21].ops and ('imm', 0x11860) in a[0x002F9F2D].ops and ('imm', 0x29) in a[0x002F9F3D].ops
    # spawn lookup: slots above 19 become 0
    assert a[0x0006BC55].mnemonic == 'cmp' and ('imm', 0x13) in a[0x0006BC55].ops
    # play set: [desc+0x150] is a 0x190-byte block, the set is at +0xE0 (0xE0 + 0xA4 <= 0x190)
    assert ('imm', 0x190) in a[0x00240B68].ops and mem(a[0x00240B78])[0][4] == 0x150
    assert ('imm', 0xE0) in a[0x00240DC8].ops and mem(a[0x00240DC4])[0][4] == 0x16
    assert ('imm', 0xE0) in a[0x0033EBDA].ops


def test_map_info_holds_ten():
    a = by_address()
    # compile: count byte constant 10 at +0x26 (data +0x16), members word/byte/byte from the editor set
    assert ('imm', 0xA) in a[0x00300884].ops and mem(a[0x00300884])[0][4] == 0x26
    assert mem(a[0x00300896])[0][4] == 0x11864 and mem(a[0x003008A8])[0][4] == 0x1186A and mem(a[0x003008B7])[0][4] == 0x11867
    # the weapon set index follows the 10th member (data +0x40), then names: the chunk is full
    assert mem(a[0x003008D2])[0][4] == 0x50
    assert ('imm', 0x3DC) in a[0x0030072A].ops and ('imm', 0xC77672) in a[0x0030095A].ops
    # editor load: 10 unrolled member copies, the last at +0x118AF
    loads = [i for i in decoded() if i.function == 0x00301A90 and mem(i) and 0x11864 <= mem(i)[0][4] <= 0x118B2 and mem(i)[0][4] % 8 == 4]
    assert len({mem(i)[0][4] for i in loads}) == 10
    # compile terminator and used: tag 0xC7766A at offset used - 8, info +0x248 = used
    assert ('imm', 0xC7766A) in a[0x00301A39].ops and mem(a[0x00301A53])[0][4] == 0x248


def test_slot_screen_and_choice():
    a = by_address()
    # the work copy has 10 rows of 0x1C; the builder adds 10 rows
    assert ('imm', 0x0075FEA8) in a[0x002C0555].ops and ('imm', 0xA) in a[0x002C1C0A].ops
    # sub_002C0490 reloads when the index differs from [0x0078AEAC], ends with the store
    assert mem(a[0x002C04FB])[0][4] == 0x0078AEAC and '0x2c0310' in a[0x002C0522].text
    assert a[0x002C05C8].branch and '0x2bffd0' in a[0x002C05C8].text
    assert ('imm', 0x0052372C) in a[0x002C04AA].ops and '0x74670' in a[0x002C04B0].text
    # the store writes the count of used rows to set +0
    assert a[0x003143B1].mnemonic == 'mov' and mem(a[0x003143B1])[0][4] == 0 and mem(a[0x003143B1])[0][1] == 'eax'
    # titles are pointers at menu +0x204 (sub_00075B70)
    assert mem(a[0x00075BCC])[0][4] == 0x204
    # Story AI character choice: options k = 0..9 from set +4 + 8k, empty text 0xD7C
    assert a[0x0031CD24].mnemonic == 'cmp' and ('imm', 0xA) in a[0x0031CD24].ops
    assert ('imm', 0xD7C) in a[0x0031CD0A].ops and '0x71800' in a[0x0031CD18].text
    # item ids 0x38 + k name slot k (sub_00303350 case 3: id*8 - 0x1BC = +4 + 8*(id-0x38))
    assert mem(a[0x003033A9])[0][4] == (-0x1BC) & 0xFFFFFFFF and mem(a[0x003033B1])[0][4] == (-0x1B6) & 0xFFFFFFFF


def test_story_ai_item_icon_and_name():
    """A Story AI is drawn as item 0x38 + slot; slot names come from sub_002BE600 -> sub_002BE300."""
    a = by_address()
    # sub_0031D3E0 (AI -> item: 9 for type 5, else slot +0x28 plus 0x38), only for the overview marker
    assert ('imm', 9) in a[0x0031D3FD].ops and mem(a[0x0031D403])[0][4] == 0x28
    assert a[0x0031D406].mnemonic == 'add' and ('imm', 0x38) in a[0x0031D406].ops
    callers = {i.address for i in decoded() if i.call and '0x31d3e0' in i.text}
    assert callers == {0x00309ABC}
    assert ret_of(a, 0x00309AC2, 0x315d20) == 0x00309AC7 and a[0x00309AC2].function == 0x00309A80
    # the Story AI list's icon row callback sub_0031D410 inlines it (AI +0x28 = editor +0x2B0)
    assert mem(a[0x0031D43B])[0][4] == 0x2B0 and ('imm', 0x38) in a[0x0031D442].ops
    assert ret_of(a, 0x0031D45B, 0x315d20) == 0x0031D460 and a[0x0031D45B].function == 0x0031D410
    assert ('imm', 0x0031D410) in a[0x0031D532].ops        # pushed as the row draw callback by sub_0031D4C0
    # sub_00315D20: catalogue +0xB4 icon, +0xB8 flag 0x400; its other callers draw catalogue items
    assert mem(a[0x00315D2A])[0][4] == 0x00525010 and mem(a[0x00315D35])[0][4] == 0x0052500C
    callers = {i.address for i in decoded() if (i.call or i.branch) and '0x315d20' in i.text}
    assert callers == {0x00309AC2, 0x0030DA35, 0x0031D45B, 0x00320545, 0x003362AB}
    # sub_002BE650 = sub_002BE300(sub_002BE600(id, variant)); sub_00303350 case 3 calls it at 0x003033BB
    assert '0x2be600' in a[0x002BE65A].text and '0x2be300' in a[0x002BE660].text
    assert ret_of(a, 0x003033BB, 0x2be650) == 0x003033C0
    rd = xbe_reader()
    row = lambda item: rd(0x00524F58 + item * 0xC4, 0xC4)
    slot_icons = {struct.unpack_from('<2I', row(item), 0xB4) for item in range(0x38, 0x42)}
    assert slot_icons == {(0x42, 1)}                     # the ten slot items share one icon
    for item in range(0x42, 0x4C):                       # what slots 11-20 would be drawn as
        assert struct.unpack_from('<2I', row(item), 0xB4) != (0x42, 1)
    assert struct.unpack_from('<I', row(0x42), 0xB4)[0] == 0x45


def test_pause_bots_screen_is_the_arcade_roster():
    """The pause / Arcade setup Bots screen is the slot screen, editing the match's bots."""
    a = by_address()
    # pause > setup (descriptor 0x004E9544 = sub_001A40A0) > sub_0019A000 > sub_002C78F0(menu, 1, ...)
    assert '0x19a000' in a[0x001A40E0].text and '0x2c78f0' in a[0x0019A01B].text
    assert ('imm', 1) in a[0x0019A018].ops
    # its Bots item (text 0x3B8) opens sub_002C7730 = push the slot screen 0x0052375C
    assert ('imm', 0x002C7730) in a[0x002C81CD].ops and ('imm', 0x3B8) in a[0x002C81D8].ops
    assert ('imm', 0x0052375C) in a[0x002C7735].ops
    # the editor's Test map setup (sub_002EFBF0) uses the same builder; its start is sub_002D38C0
    assert '0x2c78f0' in a[0x002EFC22].text and a[0x002EFBE0].branch and '0x2d38c0' in a[0x002EFBE0].text
    assert ret_of(a, 0x002D392E, 0x2d2c70) == 0x002D3933 and ret_of(a, 0x002D5E4D, 0x2d2c70) == 0x002D5E52
    # outside the editor the custom getter: mode 9 (MapMaker Arcade, sub_002C8400) and index 0x11
    # -> the map's set rebuilt from map info (10 members), else the profile's sets
    assert ('imm', 9) in a[0x002BFEFC].ops and ('imm', 0x11) in a[0x002BFF05].ops
    assert '0x312950' in a[0x002BFF1F].text and mem(a[0x002BFF15])[0][4] == (-0x4E4) & 0xFFFFFFFF
    assert ('imm', 9) in a[0x002C8429].ops
    # index 0x11 is offered only in mode 9 (sub_002C1830)
    assert ('imm', 0x11) in a[0x002C18B5].ops and ('imm', 9) in a[0x002C18AC].ops
    # match start: the 10 work-copy rows become bots in 16-byte per-actor arrays
    assert ('imm', 0x0075FDA0) in a[0x002D36EA].ops and ('imm', 0x0075FEB8) in a[0x002D381C].ops
    assert mem(a[0x002D37C3])[0][4] == 0x0078AD6C and mem(a[0x002D37C9])[0][4] == 0x0078AD9C
    assert mem(a[0x002D37B7])[0][4] == 0x0078AD7C and mem(a[0x002D3811])[0][4] == 0x0078AD8C
    rd = xbe_reader()
    assert struct.unpack('<2I', rd(0x004E9544, 8)) == (0x001A40A0, 0x001A4070)
    # actors per match by sub-mode (players + bots), mode row 0x10
    assert struct.unpack('<4i', rd(0x004D239C + 0x10 * 60, 16)) == (14, 14, 16, 16)
    # the hook pages only the editor (scene 102)
    assert "return WORD_AT(SETS_SCENE)==102u && (int32_t)WORD_AT(SETS_INDEX)>=0xC;" in INC.read_text(encoding='utf-8')


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


def test_descriptors_and_presets():
    rd = xbe_reader()
    # {builder, update}: character set screen and its slot pane
    assert struct.unpack('<2I', rd(0x0052375C, 8)) == (0x002C2C20, 0x002C0490)
    assert struct.unpack('<2I', rd(0x0052372C, 8)) == (0x002C19F0, 0x002C1570)
    # presets: count 10, members 10..19 zero (id 0, which is a real character)
    for s in range(12):
        row = rd(0x0053E41C + s * 0xA4, 0xA4)
        assert struct.unpack_from('<I', row, 0)[0] == 10
        assert row[4 + 8 * 10:] == bytes(0xA4 - 4 - 80)


def test_fork_tag_unused():
    tags = {v for i in decoded() for k, v in i.values() if 0x00C77600 <= v < 0x00C77700}
    assert 0x00C776F5 not in tags and max(tags) == 0x00C77675


def test_weapon_slot_item_types():
    """Items 0x11..0x16 are weapon slots 1-6 and 0x17..0x1C ammo slots 1-6 (catalogue +8)."""
    rd = xbe_reader()
    for item in range(0x11, 0x1D):
        row = rd(0x00524F58 + item * 0xC4, 0xC4)
        category, slot = struct.unpack_from('<h', row, 2)[0], struct.unpack_from('<I', row, 8)[0]
        assert (category, slot) == ((8, item - 0x11) if item < 0x17 else (9, item - 0x17))
    # weapon presets: count 6 of the 15 the 0x44 struct holds
    for s in range(10):
        assert struct.unpack('<I', rd(0x00518F28 + s * 0x44 + 4, 4))[0] == 6
