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
    'drop-rule-gone': ('            if(y<floor_y-MAPMAKER_ROOF_DROP) { mapmaker_roof_kill(slot,ent,"dropping outside every room");return; }\n', ''),
    'drop-any-height': ('            if(y<floor_y-MAPMAKER_ROOF_DROP) {', '            if(y<floor_y+1.0f) {'),
    'worm-killed': ('==MAPMAKER_ROOF_WORM_MODEL) return;\n', '==0xFFFFu) return;\n'),
    'without-limits': ('    return mapmaker_limits.limits && WORD_AT(0x0079094Cu)==102u;',
                       '    return WORD_AT(0x0079094Cu)==102u;'),
    'any-scene': ('    return mapmaker_limits.limits && WORD_AT(0x0079094Cu)==102u;',
                  '    return mapmaker_limits.limits;'),
    'air-clears-floor': ('#define MAPMAKER_ROOF_AIR 0x02FFFFFFu', '#define MAPMAKER_ROOF_AIR 0x03FFFFFFu'),
    'air-keeps-walls': ('#define MAPMAKER_ROOF_AIR 0x02FFFFFFu', '#define MAPMAKER_ROOF_AIR 0x02000000u'),
    'roof-clears-walls': ('mode==MAPMAKER_ROOF_OPEN_ROOF || mode==MAPMAKER_ROOF_OPEN_BOUNDED?MAPMAKER_ROOF_CEILING:0u',
                          'mode==MAPMAKER_ROOF_OPEN_ROOF?MAPMAKER_ROOF_AIR:mode==MAPMAKER_ROOF_OPEN_BOUNDED?MAPMAKER_ROOF_CEILING:0u'),
    'reduced-untouched': ('    g_eax&=~MAPMAKER_ROOF_CEILING;\n}', '}'),
    'no-record-mark': ('        WORD_AT(record+0x24u)=(WORD_AT(record+0x24u)&0xFFFFu)|(MAPMAKER_ROOF_MARK|mode)<<16;\n', ''),
    'record-mask-kept': ('        WORD_AT(record+8u)&=~mapmaker_roof_clear(mode);\n', ''),
    'record-size-lost': ('(WORD_AT(record+0x24u)&0xFFFFu)|(MAPMAKER_ROOF_MARK|mode)<<16', '(MAPMAKER_ROOF_MARK|mode)<<16'),
    'no-order-check': ('|| WORD_AT(record)!=WORD_AT(*piece+0x10u)) {', ') {'),
    'load-ignored': ('    if(mapmaker_limits.limits) mapmaker_roof_records(c,mapmaker_roof_match,NULL);', ''),
    'load-no-position': ('(int32_t)FLOAT_AT(piece+4u)!=(int8_t)(xyz>>8&0xFFu) ||', ''),
    'reset-keeps': ('    if(mapmaker_roof.keys) mapmaker_roof_forget_all();\n', ''),
    'reused-keeps': ('    if(mapmaker_roof.marked && mapmaker_roof_mode(g_eax)) mapmaker_roof_set(g_eax,0);\n', ''),
    'record-size-unchecked': ('            if(length<MAPMAKER_ROOF_RECORD || length>end-r) return -1;\n', '            if(!length) return -1;\n'),
    'pops-return': ('    original();\n    if(!c) return;', '    original();g_esp+=4u;\n    if(!c) return;'),
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
    # Grid label: a flagged tile's name gets its flag (the 46/47 presets are gone).
    'hover-no-held': ('    if(!(piece=WORD_AT(editor+0x2F18u))) piece=WORD_AT(editor+0x2F7Cu);\n', '    piece=WORD_AT(editor+0x2F18u);\n'),
    'label-in-play': ('    return piece && mapmaker_roof_editor()?mapmaker_roof_mode(piece):0u;', '    return piece?mapmaker_roof_mode(piece):0u;'),
    'label-wrong-tail': ('    tail=mapmaker_roof_tails[mode-1u];', '    tail=mapmaker_roof_tails[mode%3u];'),
    'load-unknown-mode': ('    if((mark&0xFF00u)!=MAPMAKER_ROOF_MARK || !mapmaker_roof_known(mode)) return 1;',
                          '    if((mark&0xFF00u)!=MAPMAKER_ROOF_MARK || !mode) return 1;'),
    'format-any-caller': ('WORD_AT(g_esp)==MAPMAKER_ROOF_RET_HOVER_FORMAT &&', ''),
    'format-any-format': ('       WORD_AT(g_esp+4u)==MAPMAKER_ROOF_HOVER_FORMAT && ', '       '),
    'format-pops-twice': ('        mapmaker_roof_suffix(mode);\n    original();',
                          '        mapmaker_roof_suffix(mode);\n    original();g_esp+=4u;'),
    # Records carry the piece's items after the pads (play -> editor round trip).
    'items-not-skipped': ('for(at=r+length,k=0;k<items;k++) {', 'for(at=r+length,k=items;k<items;k++) {'),
    'item-size-fixed': ('bytes=((WORD_AT(at+0x14u)&0xFFu)*4u+0x1Bu)&~3u;', 'bytes=MAPMAKER_ROOF_ITEM_MIN;'),
    'item-count-wrong-word': ('items=WORD_AT(r+0x1Cu)>>16', 'items=WORD_AT(r+0x1Cu)&0xFFFFu'),
    # Open air (bounded): walls saved (collision), hidden from the editor view and the play draw.
    'bounded-saves-no-walls': ('mode==MAPMAKER_ROOF_OPEN_ROOF || mode==MAPMAKER_ROOF_OPEN_BOUNDED?MAPMAKER_ROOF_CEILING:0u',
                               'mode==MAPMAKER_ROOF_OPEN_ROOF?MAPMAKER_ROOF_CEILING:mode?MAPMAKER_ROOF_AIR:0u'),
    'bounded-view-walls': ('    return mode==MAPMAKER_ROOF_OPEN_BOUNDED?MAPMAKER_ROOF_AIR:mapmaker_roof_clear(mode);',
                           '    return mapmaker_roof_clear(mode);'),
    'view-any-caller': ('g_eax&=~(ret==MAPMAKER_ROOF_RET_VIEW?', 'g_eax&=~(ret?'),
    'load-no-bounded': ('mode<=MAPMAKER_ROOF_OPEN_BOUNDED; }', 'mode<=MAPMAKER_ROOF_OPEN_AIR; }'),
    'bounded-no-suffix': ('" (Open Air, Unbounded)"," (Open Air, Bounded)"}', '" (Open Air, Unbounded)",""}'),
    'play-no-records': ('        read=mapmaker_roof_records(c,mapmaker_roof_play_record,NULL);\n', ''),
    'partial-walk-dropped': ('    if(mapmaker_roof_play.pieces) {\n        mapmaker_roof_play.building=', '    if(read>=0) {\n        mapmaker_roof_play.building='),
    'mismatch-stops': ('                WORD_AT(build+0x44u),mapmaker_roof_play.mask[k]);\n', '                WORD_AT(build+0x44u),mapmaker_roof_play.mask[k]);\n    if(WORD_AT(build+0x44u)!=mapmaker_roof_play.mask[k]) return;\n'),
    'draw-model-dword': ('(WORD_AT(prop+0x94u)&0xFFFFu)!=MAPMAKER_ROOF_ROOM_MODEL+room', 'WORD_AT(prop+0x94u)!=MAPMAKER_ROOF_ROOM_MODEL+room'),
    'count-wrong-stride': ('m=WORD_AT(first+i*MAPMAKER_ROOF_NODE+0x5Cu);', 'm=WORD_AT(first+i*0xC0u+0x5Cu);'),
    'count-hidden-anyway': ('if(m && (m&mask) && !(m&mask&~hide)) (*hidden)++;', 'if(m && !(m&mask&~hide)) (*hidden)++;'),
    'out-rule-off': ('    else if(mapmaker_roof_play.out[slot]>=MAPMAKER_ROOF_OUT_FRAMES) mapmaker_roof_kill(', '    else if(0) mapmaker_roof_kill('),
    'room-rule-off': ('            if(++mapmaker_roof_play.noroom[slot]>=MAPMAKER_ROOF_NOROOM_QUERIES) {', '            if(0) {'),
    'room-rule-no-reset': ('        } else if(rooms>0) mapmaker_roof_play.noroom[slot]=0;', '        }'),
    'room-rule-fast': ('#define MAPMAKER_ROOF_NOROOM_QUERIES 8u', '#define MAPMAKER_ROOF_NOROOM_QUERIES 4u'),
    'room-query-every-tick': ('    if((mapmaker_roof_play.ticks+slot)%MAPMAKER_ROOF_QUERY_EVERY==0u) {', '    if(1) {'),
    'room-no-memory-kills': ('    if(!q) return -1;', '    if(!q) return 0;'),
    'room-box-at-feet': ('FLOAT_AT(q+4u)=y+MAPMAKER_ROOF_BODY_LOW;', 'FLOAT_AT(q+4u)=y;'),
    'room-box-flat': ('FLOAT_AT(q+0x10u)=y+MAPMAKER_ROOF_BODY_HIGH;', 'FLOAT_AT(q+0x10u)=y+MAPMAKER_ROOF_BODY_LOW;'),
    'humans-skipped': ('    for(i=0;i<h;i++) if(human[i]) mapmaker_roof_fall_check(', '    for(i=0;i<0u;i++) if(human[i]) mapmaker_roof_fall_check('),
    'inactive-player': ('        if((int32_t)WORD_AT(pl+4u)<0 || !MAPMAKER_PTR_OK(', '        if(!MAPMAKER_PTR_OK('),
    'player-entity-offset': ('ent=WORD_AT(pl+0x14u),0x1D0u)', 'ent=WORD_AT(pl+0x10u),0x1D0u)'),
    'out-rule-at-floor': ('#define MAPMAKER_ROOF_OUT_DEPTH 5.0f', '#define MAPMAKER_ROOF_OUT_DEPTH 0.0f'),
    'out-not-reset': ('    if(!(y<mapmaker_roof_play.out_y)) { mapmaker_roof_play.out[slot]=0;mapmaker_roof_play.below[slot]=0;return; }',
                      '    if(!(y<mapmaker_roof_play.out_y)) { mapmaker_roof_play.below[slot]=0;return; }'),
    'rooms-wrong-base': ('mapmaker_roof_play.hide[base+j]=MAPMAKER_ROOF_SIDES;', 'mapmaker_roof_play.hide[j]=MAPMAKER_ROOF_SIDES;'),
    'rooms-one-room': ('    for(j=0;(int32_t)count>0 && j<count;j++) {', '    for(j=0;(int32_t)count>0 && j<1u;j++) {'),
    'rooms-not-reset': ('    memset(mapmaker_roof_play.hide,0,sizeof mapmaker_roof_play.hide);\n', ''),
    'draw-any-scene': (' || WORD_AT(0x0079094Cu)!=MAPMAKER_ROOF_SCENE_PLAY ||\n       !MAPMAKER_STACK_OK(8u) ||', ' ||\n       !MAPMAKER_STACK_OK(8u) ||'),
    'draw-any-prop': ('    if(WORD_AT(prop+4u)!=inst || (WORD_AT(prop+0x94u)&0xFFFFu)!=MAPMAKER_ROOF_ROOM_MODEL+room) {',
                      '    if(WORD_AT(prop+4u)!=inst) {'),
    'draw-room-off-by-one': ('hide=mapmaker_roof_play.hide[room-1u];', 'hide=mapmaker_roof_play.hide[room];'),
    'twosided-room-off-by-one': ('two=mapmaker_roof_play.twosided[room-1u];', 'two=mapmaker_roof_play.twosided[room];'),
    'draw-not-restored': ('    if(hide && WORD_AT(inst+0x30u)==(mask&~hide)) WORD_AT(inst+0x30u)=mask;\n', ''),
    'draw-not-hidden': ('    if(hide) WORD_AT(inst+0x30u)=mask&~hide;\n', ''),
    # Rooms seen across open air: geometry drawn without back-face culling.
    'twosided-off': ('    if(two) WORD_AT(prop+0x28u)=flags|MAPMAKER_ROOF_TWO_SIDED;\n', ''),
    'twosided-not-restored': ('    if(two) WORD_AT(prop+0x28u)&=~MAPMAKER_ROOF_TWO_SIDED;\n', ''),
    'twosided-clears-stock': ('    if(two && (flags&MAPMAKER_ROOF_TWO_SIDED)) two=0;', ''),
    'twosided-wrong-bit': ('#define MAPMAKER_ROOF_TWO_SIDED 0x00040000u', '#define MAPMAKER_ROOF_TWO_SIDED 0x00020000u'),
    'twosided-needs-bounded': ('    if(!(mapmaker_roof_play.rooms || mapmaker_roof_play.twosided_rooms) ||', '    if(!mapmaker_roof_play.rooms ||'),
    'twosided-not-reset': ('    memset(mapmaker_roof_play.twosided,0,sizeof mapmaker_roof_play.twosided);\n    mapmaker_roof_play.twosided_rooms=0;\n', ''),
    'twosided-any-prop': ('    if(WORD_AT(prop+4u)!=inst || (WORD_AT(prop+0x94u)&0xFFFFu)!=MAPMAKER_ROOF_ROOM_MODEL+room) {',
                          '    if(WORD_AT(prop+4u)!=inst || (hide && (WORD_AT(prop+0x94u)&0xFFFFu)!=MAPMAKER_ROOF_ROOM_MODEL+room)) {'),
    # Open air (unbounded): kill height below the lowest floor.
    'kill-any-map': ('        mapmaker_roof_play.kill=mapmaker_roof_play.unbounded!=0;', '        mapmaker_roof_play.kill=1;'),
    'kill-bounded-too': ('    if(mode==MAPMAKER_ROOF_OPEN_AIR) mapmaker_roof_play.unbounded++;', '    if(mode) mapmaker_roof_play.unbounded++;'),
    'kill-any-scene': ('    if(!mapmaker_roof_play.kill || WORD_AT(0x0079094Cu)!=MAPMAKER_ROOF_SCENE_PLAY) return;',
                       '    if(!mapmaker_roof_play.kill) return;'),
    'kill-the-dead': ('    if(!(health>0.0f)) { mapmaker_roof_play.out[slot]=', '    if(0) { mapmaker_roof_play.out[slot]='),
    'kill-no-entry-check': (' || WORD_AT(ent+0x7Cu)!=actor) continue;', ') continue;'),
    'kill-floor-ignored': ('mapmaker_roof_play.kill_y=(float)mapmaker_roof_play.low*MAPMAKER_ROOF_FLOOR_HEIGHT-MAPMAKER_ROOF_KILL_DEPTH;',
                           'mapmaker_roof_play.kill_y=-MAPMAKER_ROOF_KILL_DEPTH;'),
    'kill-no-margin': ('#define MAPMAKER_ROOF_KILL_DEPTH 20.0f', '#define MAPMAKER_ROOF_KILL_DEPTH 0.0f'),
    'kill-floor-unsigned': ('    int floor=(int8_t)(WORD_AT(record+4u)>>16&0xFFu);', '    int floor=(int)(WORD_AT(record+4u)>>16&0xFFu);'),
    'kill-unbounded-count': ('    if(n>MAPMAKER_ROOF_ACTORS_MAX) n=MAPMAKER_ROOF_ACTORS_MAX;\n', '    if(n>MAPMAKER_ROOF_ACTORS_MAX) n=2u;\n'),
    'tick-pops-twice': ('    int log_now;\n    original();', '    int log_now;\n    original();g_esp+=4u;'),
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


def test_tile_numbers_and_formats():
    """The number in front of a tile name is catalogue +4: 1..45 in palette
    order (tile 8 "Small Open" is 1), so the open entries are 46 and 47. The
    palette caption draws "%d. " (with a space), the grid label "%d.%s"."""
    read = _xbe()
    numbers = [struct.unpack('<I', read(0x00529510 + t * 0x140 + 4, 4))[0] for t in range(45)]
    assert sorted(numbers) == list(range(1, 46))
    full = [struct.unpack('<3I', read(0x0052DAC0 + i * 12, 12)) for i in range(8)]
    order = [x for _, ptr, c in full for x in struct.unpack(f'<{c}I', read(ptr, c * 4))]
    assert [numbers[t] for t in order] == list(range(1, 46))   # numbered in list order
    assert numbers[8] == 1
    assert read(0x0049F7C0, 6) == b'%d.%s\0'
    assert read(0x0049F838, 5) == b'%d. \0'


def test_label_sites():
    """Grid label (sub_00311xxx): name of piece editor +0x2F18, else the held
    piece +0x2F7C, through sub_0003D7B0 at 0x00311718, then
    sub_00151030("%d.%s", byte catalogue +4, name). Palette caption:
    sub_0032D9F0(eax = catalogue +4 or -1, ecx = name) at 0x0032E5DE, which
    prints "%d. " (0x0032DA29). Records: +0x24 = 0x28 + pads*8, items after."""
    from mapmaker_decoded import decoded
    insns = decoded()
    at = {i.address: i for i in insns}
    assert at[0x0031165C].text == 'eax, dword ptr [ecx + 0x2f18]'
    assert at[0x003116C0].text == 'eax, dword ptr [ecx + 0x2f7c]'
    assert at[0x0031170B].text == 'edx, dword ptr [ecx + 0x2f7c]' and at[0x003116B8].text == 'edx, dword ptr [ecx + 0x2f18]'
    assert at[0x00311714].text == 'ecx, dword ptr [eax + 8]'
    assert 'call' in at[0x00311718].mnemonic and at[0x00311718].ops[0][1] == 0x0003D7B0
    assert at[0x0031171D].text == 'eax' and at[0x0031171E].text == 'esi'
    assert at[0x0031171F].ops[0] == ('imm', 0x0049F7C0)
    assert 'call' in at[0x00311724].mnemonic and at[0x00311724].ops[0][1] == 0x00151030
    assert at[0x00311698].ops[1][4] == 0x00529514 and at[0x003116EB].ops[1][4] == 0x00529514
    assert at[0x0032E5AB].text == 'esi, byte ptr [esi + 4]' and at[0x0032E5B1].text == 'esi, 0xffffffff'
    assert at[0x0032E5DA].text == 'eax, esi' and at[0x0032E5D7].text == 'ecx, eax'
    assert 'call' in at[0x0032E5DE].mnemonic and at[0x0032E5DE].ops[0][1] == 0x0032D9F0
    assert at[0x0032DA29].ops[0] == ('imm', 0x0049F838)
    assert {i.address for i in insns if i.call and any(op == ('imm', 0x0032D9F0) for op in i.ops)} == \
        {0x0032E5DE, 0x0032E714, 0x0032E734}
    # Record sizes: writer and loader.
    assert at[0x00300D60].text == 'eax, [ecx*8 + 0x28]'
    assert at[0x0030115C].text == 'eax, [edx*4 + 0x1b]' and at[0x00301163].text == 'eax, 0xfffffffc'
    assert at[0x0030224D].text == 'esi, [esi + edi*8 + 0x28]'
    assert at[0x003023EA].text == 'eax, [edx*4 + 0x1b]'
    # Editor start: reset, then load of the kept map with flags 2.
    assert 'call' in at[0x0030302B].mnemonic and at[0x0030302B].ops[0][1] == 0x002FEBF0
    assert 'call' in at[0x00303076].mnemonic and at[0x00303076].ops[0][1] == 0x00301A90
    assert at[0x00303066].text == 'eax, dword ptr [0x784014]'
    # Play hand-over: rebuild of the kept map (mode 3), then the play level.
    assert at[0x00312DFC].text == 'eax, dword ptr [0x784014]'
    assert 'call' in at[0x00312E05].mnemonic and at[0x00312E05].ops[0][1] == 0x003006C0


def test_bounded_and_fall_sites():
    """Open air (bounded) and (unbounded) in play (decoder + XBE):
    - every draw of sub_0008F5E0 (the node-mask draw) is inside sub_0008F6B0;
    - sub_00085710 is called once per build entry, in order (edi += 0x68);
      the entry's +0x44 comes from the record copy's +0x14 (record +8);
    - room geometry props: model 0x907 + room, +0x34 = room;
    - the play y of a piece is record byte +6 * 5.0;
    - sub_0003C440 is the once-per-frame tick; sub_000BD090(entity) is the
      stock kill-now helper (no attacker -> sub_000BABA0) and the stock loop
      at 0x00067AF1 walks the live list with it."""
    from mapmaker_decoded import decoded
    insns = decoded()
    at = {i.address: i for i in insns}

    def calls(target):
        return {i.address for i in insns if i.call and any(op[0] == 'imm' and op[1] == target for op in i.ops)}
    draws = calls(0x0008F5E0)
    assert draws and {at[a].function for a in draws} <= {0x0008F5E0, 0x0008F6B0}
    assert {a for a in draws if at[a].function == 0x0008F6B0} == {
        0x0008F853, 0x0008F85D, 0x0008F868, 0x0008F881, 0x0008F88D, 0x0008F898, 0x0008F8BC}
    assert calls(0x00085710) == {0x0008AC71}
    assert at[0x0008ACA6].text == 'edi, 0x68' and at[0x0008AC70].text == 'edi'
    assert at[0x000857A3].text == 'dword ptr [edx*4 + 0x703460], eax' and at[0x0008579E].text == 'eax, dword ptr [esi + 0x44]'
    assert at[0x00084F98].text == 'dword ptr [ebp + 0x44], edx' and at[0x00084F91].text == 'edx, dword ptr [ebx + 0x14]'
    assert at[0x000856EB].text == 'dword ptr [ebx + 0x44], edx' and at[0x000856E4].text == 'edx, dword ptr [ecx + 0x14]'
    assert at[0x0008A920].text == 'dword ptr [esp + 0xb4], edx'          # copy +0x14 = record +8
    assert at[0x00085D6B].text == 'eax, [ebp + 0x907]' and at[0x00085D9F].text == 'dword ptr [eax + 0x34], ebp'
    assert at[0x00085DD0].text == 'dword ptr [ecx + 0x30], edx'
    # The prop's model id +0x94 is a 16-bit word next to another word +0x96.
    assert at[0x000A69E3].text == 'word ptr [ebp + 0x94], di' and at[0x000A6897].text == 'word ptr [ebp + 0x96], ax'
    assert at[0x000A6F4F].text == 'ecx, word ptr [esi + 0x94]'
    # Node table walked by the draw: header [inst+4] = count, nodes 0xC8 bytes before it.
    assert at[0x0008F5E6].text == 'esi, dword ptr [ebx + 4]' and at[0x0008F5EB].text == 'eax, eax, 0xc8'
    assert at[0x0008F637].text == 'eax, dword ptr [esi + edi + 0x5c]'
    assert at[0x0008A87E].text == 'ebx, byte ptr [esi + 6]'
    assert at[0x0008A8F0].ops[1][4] == 0x004760CC
    assert calls(0x0003C440) == {0x0003D3CA}
    assert at[0x0003D3C5].text == 'dword ptr [0x6b7af0], eax'          # frame counter, then the tick
    assert 'call' in at[0x000BD0E2].mnemonic and at[0x000BD0E2].ops[0][1] == 0x000BABA0
    assert at[0x000BD0D2].text == 'dword ptr [esp + 0xc], 0'           # no attacker
    assert at[0x00067AF1].ops[1][4] == 0x007A3580 and at[0x00067B58].ops[0][1] == 0x000BD090
    assert at[0x00067AFE].text == 'xmm0, dword ptr [eax + 0x1cc]'
    # The player hazard kill uses the same helper in MapMaker play (scene 100
    # hook first), on the entity of the current player struct (+0x14).
    assert at[0x000D672B].text == 'dword ptr [0x79094c], 0x64'
    assert at[0x000D673E].ops[0][1] == 0x000BD090 and at[0x000D673D].text == 'ebp'
    assert at[0x000D5EB5].text == 'ecx, dword ptr [eax + 0x14]' and at[0x000D5EBC].text == 'esi, dword ptr [ecx + 0x7c]'
    # y is up: the scene-61 kill is -100.0 > y, fall damage is start - landing height.
    assert at[0x0015F1B3].ops[1][4] == 0x0047D6D8 and at[0x0015F1BB].text == 'xmm0, dword ptr [esi + 0x68]'
    assert at[0x000D4449].text == 'xmm0, dword ptr [edi + 0xbfc]' and at[0x000D4441].text == 'xmm0, dword ptr [edi + 0xb70]'
    # The floor probe searches only the character's room (+0x34) and keeps
    # the last floor when it finds none; the room's search honours inst+0x30.
    assert at[0x000D9149].text == 'ecx, dword ptr [esi + 0x34]' and at[0x000D9184].ops[0][1] == 0x000C3B10
    assert at[0x000D91AA].text == 'dword ptr [edi + 0xbfc], xmm0' and 'jnp' in at[0x000D91A2].mnemonic
    assert at[0x000C3B46].text == 'esi, dword ptr [0x7b975c]' and at[0x000C3B6C].text == 'edx, dword ptr [ecx + 0x30]'
    read = _xbe()
    assert struct.unpack('<f', read(0x004760CC, 4))[0] == 5.0
    assert struct.unpack('<f', read(0x0047D6D8, 4))[0] == -100.0


def test_wrappers_documented():
    doc = (ROOT / 'docs/mapmaker/mapmaker-sky.md').read_text(encoding='utf-8')
    text = ROOF.read_text(encoding='utf-8')
    for target, name in (('002FC680', 'mask'), ('002FC470', 'reduced'), ('00307FD0', 'piece_new'),
                         ('002FEBF0', 'reset'), ('003006C0', 'compile'), ('00301A90', 'load'),
                         ('002F95E0', 'move'), ('00308120', 'copy'),
                         ('00308480', 'piece_free'), ('00151030', 'format'),
                         ('0008A780', 'play'), ('00085710', 'rooms'),
                         ('0008F6B0', 'draw'), ('0003C440', 'tick')):
        assert f'WRAP sub_{target} -> pc_mapmaker_roof_{name}' in doc, name
        assert f'void pc_mapmaker_roof_{name}(void (*original)(void))' in text, name
    # The tile-8 palette presets 46/47 were removed (user, 2026-11-07): no
    # palette, text or caption hook is left anywhere.
    header = (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    guest = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    for name in ('menu_init', 'menu_add', 'menu_hover', 'menu_pick', 'text', 'caption'):
        for place in (doc, text, header, guest):
            assert f'pc_mapmaker_roof_{name}(' not in place and f'pc_mapmaker_roof_{name}\n' not in place, name


def test_cull_none_flag_sites():
    """sub_0008F6B0 draws pass 0 with render state 6 (cull mode) 0 when the
    prop has +0x28 bit 0x40000 (or its model +0x28 bit 0x20000), and puts the
    saved mode back after; the room draw sub_000A2900 culls 0x900, or 0x901
    for mirrored rooms; the bit's only other readers are not draw code."""
    from mapmaker_decoded import decoded
    insns = decoded()
    at = {i.address: i for i in insns}
    assert at[0x0008F805].text == 'ebp, ebp' and at[0x0008F809].text == 'edx, dword ptr [ebx + 0x24]'
    assert at[0x0008F815].text == 'dword ptr [ebx + 0x28], 0x40000'
    assert at[0x0008F81E].ops[0] == ('imm', 0) and at[0x0008F820].ops[0] == ('imm', 6)
    assert at[0x0008F822].ops[0][1] == 0x00021450
    assert at[0x0008F8D4].text == 'dword ptr [ebx + 0x28], 0x40000' and at[0x0008F8E4].ops[0][1] == 0x00021450
    assert at[0x0008F6B9].text == 'ebx, dword ptr [esi]'                     # ebx = [inst] = the prop
    assert ('imm', 0x901) in at[0x000A29C2].ops and at[0x000A29C9].ops[0][1] == 0x00021450
    assert ('imm', 0x900) in at[0x000A29B7].ops and at[0x000A29AF].ops[0][1] == 0x00021440
    tests = {i.address for i in insns if i.mnemonic == 'test' and any(op[0] == 'mem' and op[4] == 0x28 for op in i.ops)
             and ('imm', 0x40000) in i.ops}
    assert tests == {0x0008F815, 0x0008F8D4, 0x000A6045, 0x000D84E6}, sorted(map(hex, tests))
