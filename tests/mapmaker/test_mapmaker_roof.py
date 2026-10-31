"""MapMaker "Open roof" / "Open air" tiles (src/interface/pc_mapmaker_roof.inc): fixture with
mutations, plus the decoder and XBE evidence the design relies on (skipped when
the private XBE is not available)."""
import os
import struct
from pathlib import Path
import subprocess

import pytest

from native_compiler import native_compiler
from test_mapmaker_sky import _xbe

ROOT = Path(__file__).resolve().parents[2]
ROOF = ROOT / 'src/interface/pc_mapmaker_roof.inc'

MUTATIONS = {
    'without-limits': ('    return mapmaker_limits.limits && WORD_AT(0x0079094Cu)==102u;',
                       '    return WORD_AT(0x0079094Cu)==102u;'),
    'any-scene': ('    return mapmaker_limits.limits && WORD_AT(0x0079094Cu)==102u;',
                  '    return mapmaker_limits.limits;'),
    'air-clears-floor': ('#define MAPMAKER_ROOF_AIR 0x02FFFFFFu', '#define MAPMAKER_ROOF_AIR 0x03FFFFFFu'),
    'air-keeps-walls': ('#define MAPMAKER_ROOF_AIR 0x02FFFFFFu', '#define MAPMAKER_ROOF_AIR 0x02000000u'),
    'roof-clears-walls': ('mode==MAPMAKER_ROOF_OPEN_ROOF?MAPMAKER_ROOF_CEILING:0u', 'mode==MAPMAKER_ROOF_OPEN_ROOF?MAPMAKER_ROOF_AIR:0u'),
    'reduced-untouched': ('    g_eax&=~MAPMAKER_ROOF_CEILING;\n}', '}'),
    'table-not-restored': ('    WORD_AT(last+4u)=list;WORD_AT(last+8u)=count;\n', ''),
    'one-entry': ('    WORD_AT(last+4u)=mapmaker_roof.list;WORD_AT(last+8u)=count+2u;',
                  '    WORD_AT(last+4u)=mapmaker_roof.list;WORD_AT(last+8u)=count+1u;'),
    'first-category': ('    last=table+(rows-1u)*12u;', '    last=table;'),
    'always-full-table': ('    rows=WORD_AT(editor+0x200u)?8u:3u;', '    rows=8u;'),
    'any-add-site': ('    if(ret!=MAPMAKER_ROOF_RET_MENU_ADD) return;', '    if(!mapmaker_roof.building) return;'),
    'counts-wrong': ('mapmaker_roof.first=before+count;', 'mapmaker_roof.first=before;'),
    'item-by-id': ('    return item==mapmaker_roof.item[0]?', '    return item?MAPMAKER_ROOF_OPEN_AIR:item==mapmaker_roof.item[0]?'),
    'preview-not-restored': ('        mapmaker_roof_preview_restore();\n        mapmaker_roof.preview_inst=0;', '        mapmaker_roof.preview_inst=0;'),
    'preview-not-open': ('    WORD_AT(inst+0x30u)=mapmaker_roof.preview_applied;\n}', '}'),
    'pick-sees-open-preview': ('WORD_AT(mapmaker_roof.preview_inst+0x30u)=mapmaker_roof.preview_saved;swapped=1;', 'swapped=1;'),
    'pick-not-marked': ('    mapmaker_roof_set(piece,mode);\n    if(mapmaker_roof.placed', '    if(mapmaker_roof.placed'),
    'no-record-mark': ('        WORD_AT(record+0x24u)=(WORD_AT(record+0x24u)&0xFFFFu)|(MAPMAKER_ROOF_MARK|mode)<<16;\n', ''),
    'record-mask-kept': ('        WORD_AT(record+8u)&=~mapmaker_roof_clear(mode);\n', ''),
    'record-size-lost': ('(WORD_AT(record+0x24u)&0xFFFFu)|(MAPMAKER_ROOF_MARK|mode)<<16', '(MAPMAKER_ROOF_MARK|mode)<<16'),
    'no-order-check': ('|| WORD_AT(record)!=WORD_AT(*piece+0x10u)) {', ') {'),
    'load-ignored': ('    if(mapmaker_limits.limits) mapmaker_roof_records(c,mapmaker_roof_match,NULL);', ''),
    'load-any-mode': ("(mode!=MAPMAKER_ROOF_OPEN_ROOF && mode!=MAPMAKER_ROOF_OPEN_AIR)", '(mode==0u)'),
    'load-no-position': ('(int32_t)FLOAT_AT(piece+4u)!=(int8_t)(xyz>>8&0xFFu) ||', ''),
    'reset-keeps': ('    if(mapmaker_roof.keys) mapmaker_roof_forget_all();\n', ''),
    'reused-keeps': ('    if(mapmaker_roof.marked && mapmaker_roof_mode(g_eax)) mapmaker_roof_set(g_eax,0);\n', ''),
    'record-size-unchecked': ('            if(length<MAPMAKER_ROOF_RECORD || length>end-r) return -1;\n', '            if(!length) return -1;\n'),
    'pops-return': ('    original();\n    if(!c) return;', '    original();g_esp+=4u;\n    if(!c) return;'),
    'no-list-silent': ('        if(mapmaker_roof.no_list++<4u)', '        if(0)'),
    # The mark follows the copy the editor makes when placing / picking up.
    'move-not-carried': ('void pc_mapmaker_roof_move(void (*original)(void))\n{\n    mapmaker_roof_carry();',
                         'void pc_mapmaker_roof_move(void (*original)(void))\n{'),
    'copy-not-carried': ('void pc_mapmaker_roof_copy(void (*original)(void))\n{\n    mapmaker_roof_carry();',
                         'void pc_mapmaker_roof_copy(void (*original)(void))\n{'),
    'carry-reversed': ('from=WORD_AT(g_esp+4u);to=WORD_AT(g_esp+8u);', 'to=WORD_AT(g_esp+4u);from=WORD_AT(g_esp+8u);'),
    'free-keeps': ('{ mapmaker_roof_set(piece,0);mapmaker_roof.freed++; }', 'mapmaker_roof.freed++;'),
    'compile-editor-only': ('    mapmaker_limits_read();\n    if(!mapmaker_limits.limits) return;\n    piece=',
                            '    if(!mapmaker_roof_editor()) return;\n    piece='),
    'compile-without-limits': ('    mapmaker_limits_read();\n    if(!mapmaker_limits.limits) return;\n    piece=', '    piece='),
    # Names.
    'text-any-caller': ('    if(ret==MAPMAKER_ROOF_RET_CAPTION) mode=', '    if(1) mode='),
    'text-any-id': (' || WORD_AT(g_esp+4u)!=MAPMAKER_ROOF_NAME_TEXT) { original();return; }', ') { original();return; }'),
    'text-names-swapped': ('mapmaker_roof.labels+(mode-1u)*', 'mapmaker_roof.labels+(2u-mode)*'),
    'text-no-ret': ('    g_eax=mapmaker_roof.labels+(mode-1u)*MAPMAKER_ROOF_LABEL_BYTES;\n    MAPMAKER_GUEST_RET();',
                    '    g_eax=mapmaker_roof.labels+(mode-1u)*MAPMAKER_ROOF_LABEL_BYTES;'),
    'text-in-play': (' || !mapmaker_roof_editor()) { original();return; }\n    g_eax=', ') { original();return; }\n    g_eax='),
    'caption-not-reset': ('    mapmaker_roof.preview_mode=mode;\n', ''),
    'piece-name-ignored': ('    else if(ret==MAPMAKER_ROOF_RET_PIECE_NAME', '    else if(0'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = ROOF.read_text(encoding='utf-8')
        assert old in text, mutation
        (tmp_path / ROOF.name).write_text(text.replace(old, new, 1), encoding='utf-8')
    exe = tmp_path / ('roof.exe' if os.name == 'nt' else 'roof')
    source = ROOT / 'tests/mapmaker/mapmaker_roof.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_roof(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_MAPMAKER')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker roof ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_stock_palette_and_tile_data():
    """Category tables, the tile menu descriptor and the base tile (US XBE)."""
    read = _xbe()
    full = [struct.unpack('<3I', read(0x0052DAC0 + i * 12, 12)) for i in range(8)]
    basic = [struct.unpack('<3I', read(0x0052DB20 + i * 12, 12)) for i in range(3)]
    assert [c for _, _, c in full] == [11, 6, 4, 5, 7, 5, 5, 2]
    assert [c for _, _, c in basic] == [11, 1, 4]
    assert struct.unpack('<2I', read(full[7][1], 8)) == (15, 30)
    ids = [x for _, p, c in full for x in struct.unpack(f'<{c}I', read(p, c * 4))]
    assert sorted(ids) == list(range(45))                    # every tile once
    assert 8 in struct.unpack('<11I', read(full[0][1], 44))
    # Tile menu descriptor: build sub_00316B50, highlight sub_00316920.
    assert struct.unpack('<2I', read(0x0052DDFC, 8)) == (0x00316B50, 0x00316920)
    # Base tile 8: type 9, 1x1x1, connection points 4 sides + one "up" (type 5).
    entry = 0x00529510 + 8 * 0x140
    assert struct.unpack('<I', read(entry, 4))[0] == 9
    assert struct.unpack('<3I', read(entry + 0x14, 12)) == (1, 1, 1)
    count = struct.unpack('<I', read(entry + 0x44, 4))[0]
    types = sorted(read(entry + 0x48 + i * 6, 1)[0] for i in range(count))
    assert types == [1, 2, 3, 4, 5]
    assert struct.unpack('<I', read(entry + 0x0C, 4))[0] == 0  # no mask callback (0x00300CD7)
    assert struct.unpack('<I', read(entry + 0x08, 4))[0] == 0x0A3F  # name text ("Small Open")


def _by_address(insns, lo, hi):
    return [i for i in insns if lo <= i.address < hi]


def test_piece_record_word_0x26_unused():
    """The writer never stores piece record +0x26/+0x27, the editor loader and
    every piece-chunk reader never read them (lifter's decoder)."""
    from mapmaker_decoded import decoded
    insns = decoded()
    readers = {i.function for i in insns if any(v == 0x00C77669 for _, v in i.values())}
    assert readers == {0x0023F390, 0x0023FD10, 0x00240590, 0x00240C50, 0x003006C0, 0x00300800,
                       0x0032A650, 0x0032A6D0, 0x0032A770}, sorted(map(hex, readers))
    play = {0x0008A780, 0x00085920} | (readers - {0x003006C0, 0x00300800})
    writer = _by_address(insns, 0x00300B74, 0x00301174)       # piece records, sub_003006C0
    loader = _by_address(insns, 0x0030217A, 0x00302428)       # piece case of sub_00301A90
    assert writer and loader
    for i in writer + loader + [i for i in insns if i.function in play]:
        for op in i.ops:
            if op[0] == 'mem' and op[1] not in ('esp', 'ebp'):
                assert op[4] not in (0x26, 0x27), hex(i.address)
    # The record size word +0x24 that shares the dword is written at 0x00300D76.
    size = [i for i in writer if i.address == 0x00300D76]
    assert size and size[0].ops[0][4] == 0x24 and size[0].ops[0][5] == 2
    # The compiler zeroes the whole body first.
    assert any(i.address == 0x0030070D and i.mnemonic.startswith('rep stos') for i in insns)


def test_hook_sites():
    """Call sites and constants the hooks rely on."""
    from mapmaker_decoded import decoded
    insns = decoded()
    at = {i.address: i for i in insns}

    def calls(target):
        return {i.address for i in insns if i.call and any(op[0] == 'imm' and op[1] == target for op in i.ops)}
    assert calls(0x00307FD0) == {0x003021C1, 0x0031658E}    # only two piece constructors
    assert calls(0x002FC680) == {0x00300CA4, 0x00300CB5, 0x0032FEBF}
    assert calls(0x00316B50) == set() and calls(0x00316920) == set() and calls(0x00316500) == set()
    assert 'call' in at[0x00316C7F].mnemonic and at[0x00316C7F].ops[0][1] == 0x000792B0
    assert at[0x00316C84].address == 0x00316C84             # return address of the add
    cats = {i.address for i in insns if any(v in (0x0052DAC0, 0x0052DB20) for _, v in i.values())}
    assert cats == {0x00316C1E, 0x00316C26}
    assert at[0x0031659E].text == 'dword ptr [edx + 0x2f7c], eax'          # held piece
    assert at[0x00316B10].text == 'ecx, 0x2000000'                          # preview closed top
    assert at[0x0032FEEF].text == 'dword ptr [eax], 0xfdffffff'             # editor view hides bit 25
    assert at[0x00311C2D].text == 'esi, 0x2000000' and at[0x00311C3C].text == 'esi, 0x1000000'
    assert at[0x0007706A].text == 'dword ptr [esi + 0x18c], eax' and at[0x00077061].text == 'dword ptr [esi + 0x198], edx'
    assert at[0x0008F64A].text == 'dword ptr [ebx + 0x30], eax'             # draw mask test
    # Node-mask tests in the geometry queries (collision follows the mask, INFERRED).
    for a in (0x00166C77, 0x00166F5B, 0x001672CB, 0x0016D47A, 0x0016D7DB):
        assert at[a].mnemonic == 'test' and at[a].ops[0][4] == 0x30, hex(a)


def test_copy_sites():
    """Placing and picking up copy the 0x48-byte piece into a new one
    (sub_00307EC0 + rep movsd of 0x12 dwords), then pass (old, new) to
    sub_002F95E0 (move) or (src, dst) to sub_00308120 (copy); sub_00308480
    frees one piece. This is why a mark keyed by the held piece never reached
    a placed tile."""
    from mapmaker_decoded import decoded
    insns = decoded()
    at = {i.address: i for i in insns}

    def calls(target):
        return {i.address for i in insns if i.call and any(op[0] == 'imm' and op[1] == target for op in i.ops)}
    assert calls(0x00307EC0) == {0x002F9A3E, 0x002F9BE9, 0x002F9C8F, 0x00303862}
    assert calls(0x002F95E0) == {0x002F9B7F, 0x002F9C36, 0x002F9CB2, 0x0030387E}
    assert calls(0x00308120) == {0x002F9BB6, 0x002F9C66, 0x002F9CC7}
    assert calls(0x00308480) == {0x002F95AA, 0x002F9B96, 0x002FB8B0, 0x00308574}
    assert {i.function for i in insns if i.address == 0x0030387E} == {0x003037A0}
    for rep, count in ((0x002F9A69, 0x002F9A62), (0x002F9C14, 0x002F9C0D), (0x002F9CA5, 0x002F9C9C),
                       (0x0030387C, 0x00303872)):
        assert at[rep].mnemonic.startswith('rep movs'), hex(rep)
        assert at[count].text == 'ecx, 0x12', hex(count)
    # Placement: (held, new) pushed as ebx then ebp; the held list is freed after.
    assert at[0x00303871].text == 'ebp' and at[0x0030387B].text == 'ebx'
    assert 'call' in at[0x0030398A].mnemonic and at[0x0030398A].ops[0][1] == 0x00308560
    # Names: tile name text via sub_0003D7B0 for the palette caption and the selected piece.
    for site in (0x0032E5CF, 0x00317C5B):
        assert 'call' in at[site].mnemonic and at[site].ops[0][1] == 0x0003D7B0, hex(site)
    assert at[0x0032E5C8].ops[1][4] == 0x00529518 and at[0x00317C54].ops[1][4] == 0x00529518
    # Play takes record +8 as is and never recomputes the mask.
    assert at[0x0008A91D].text == 'edx, dword ptr [esi + 8]'
    play = [i for i in insns if i.function == 0x0008A780 and i.call]
    targets = {op[1] for i in play for op in i.ops if op[0] == 'imm'}
    assert not targets & {0x002FC680, 0x002FC470, 0x00311C00}


def test_wrappers_documented():
    doc = (ROOT / 'docs/mapmaker/mapmaker-sky.md').read_text(encoding='utf-8')
    text = ROOF.read_text(encoding='utf-8')
    for target, name in (('002FC680', 'mask'), ('002FC470', 'reduced'), ('00307FD0', 'piece_new'),
                         ('002FEBF0', 'reset'), ('003006C0', 'compile'), ('00301A90', 'load'),
                         ('00316B50', 'menu_init'), ('000792B0', 'menu_add'), ('00316920', 'menu_hover'),
                         ('00316500', 'menu_pick'), ('002F95E0', 'move'), ('00308120', 'copy'),
                         ('00308480', 'piece_free'), ('0003D7B0', 'text')):
        assert f'WRAP sub_{target} -> pc_mapmaker_roof_{name}' in doc, name
        assert f'void pc_mapmaker_roof_{name}(void (*original)(void))' in text, name
