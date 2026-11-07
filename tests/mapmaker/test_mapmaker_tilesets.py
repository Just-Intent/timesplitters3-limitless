"""MapMaker hidden Abstract tileset (src/interface/pc_mapmaker_tilesets.inc): fixture with
mutations, the XBE and decoder facts the hooks rely on, and the asset audit of the
local game paks (skipped without the private XBE / game data)."""
import os
import struct
import zlib
from pathlib import Path
import subprocess

import pytest

from native_compiler import native_compiler
from test_mapmaker_sky import _xbe

ROOT = Path(__file__).resolve().parents[2]
TILESETS = ROOT / 'src/interface/pc_mapmaker_tilesets.inc'
CONTAINER = ROOT / 'src/interface/pc_mapmaker_container.inc'
DOC = ROOT / 'docs/mapmaker/mapmaker-tilesets.md'

MUTATIONS = {
    'menu-any-caller': (' || WORD_AT(g_esp)!=TS_RET_MENU_ROW) { original();return; }', ') { original();return; }'),
    'menu-every-row': ('    if(value!=TS_LAST_STOCK_ROW || !ts_on()', '    if(!ts_on()'),
    'menu-without-limits': (' || !ts_on() || WORD_AT(TS_SCENE)!=102u) return;', ' || WORD_AT(TS_SCENE)!=102u) return;'),
    'menu-any-scene': (' || !ts_on() || WORD_AT(TS_SCENE)!=102u) return;', ' || !ts_on()) return;'),
    'menu-wrong-value': ('guest_call(sub_00071800,2,text,TS_ABSTRACT);', 'guest_call(sub_00071800,2,text,TS_FALLBACK);'),
    'menu-wrong-text': ('WORD_AT(TS_TABLE+TS_ABSTRACT*TS_STRIDE));', 'WORD_AT(TS_TABLE+TS_FALLBACK*TS_STRIDE));'),
    'menu-row-before-stock': ('    value=WORD_AT(g_esp+8u);\n    original();\n', '    value=WORD_AT(g_esp+8u);\n'),
    'menu-no-image': ('    ts_image();\n    g_eax=eax;\n', '    g_eax=eax;\n'),
    'image-overwrites': ('    if(WORD_AT(a)==0u) WORD_AT(a)=TS_LAB_IMAGE;', '    WORD_AT(a)=TS_LAB_IMAGE;'),
    'image-wrong': ('#define TS_LAB_IMAGE 0x17Du', '#define TS_LAB_IMAGE 0x17Cu'),
    'play-any-caller': (' || WORD_AT(g_esp)!=TS_RET_PAK_PATH || ', ' || '),
    'play-any-tileset': (' || WORD_AT(g_esp+4u)!=TS_ABSTRACT) {', ') {'),
    'play-gated': ('    ts_image();\n    if(ts_mount_editor_pak()) {', '    ts_image();\n    if(ts_on() && ts_mount_editor_pak()) {'),
    'play-not-whole': ('ts_mount_pak(ts_editor_pak,TS_PAK_LEN,1u)', 'ts_mount_pak(ts_editor_pak,TS_PAK_LEN,0u)'),
    'play-wrong-heap': ('guest_call(sub_000622D0,3,s,whole,1u)', 'guest_call(sub_000622D0,3,s,whole,2u)'),
    'textures-whole': ('ts_mount_pak(ts_editor_textures,TS_TEX_PAK_LEN,0u)', 'ts_mount_pak(ts_editor_textures,TS_TEX_PAK_LEN,1u)'),
    'textures-not-mounted': ('    if(ts_mount_pak(ts_editor_textures,TS_TEX_PAK_LEN,0u)) {', '    if(0) {'),
    'textures-first': ('    if(!ts_mount_pak(ts_editor_pak,TS_PAK_LEN,1u)) return 0;\n    if(ts_mount_pak(ts_editor_textures,TS_TEX_PAK_LEN,0u)) {',
                       '    if(ts_mount_pak(ts_editor_textures,TS_TEX_PAK_LEN,0u)) ts_state.textures++;\n    if(!ts_mount_pak(ts_editor_pak,TS_PAK_LEN,1u)) return 0;\n    if(1) {'),
    'textures-failure-fatal': ('    } else ts_log("Abstract map without l_102m.pak; its tiles have no textures",WORD_AT(TS_MOUNTS),0);\n    return 1;',
                               '    } else return 0;\n    return 1;'),
    'textures-wrong-pak': ('="pak/arcademem/l_102m.pak";', '="pak/arcademem/l_100m.pak";'),
    'disco-row-missing': ('        guest_call(sub_00071800,2,ts_state.disco_text,TS_DISCO_ROW);\n', ''),
    'disco-row-value': ('#define TS_DISCO_ROW 6u', '#define TS_DISCO_ROW 5u'),
    'disco-row-after-abstract': ('    if(ts_disco_text()) {', '    if(0) {'),
    'disco-label-each-time': ('    if(ts_state.disco_text) return ts_state.disco_text;\n', ''),
    'play-path-in-frame': ('    s=(esp-64u)&~3u;', '    s=esp;'),
    'play-calls-over-path': ('    g_esp=s;                                             /* calls run below the string */\n', ''),
    'play-esp-not-restored': ('    g_esp=esp;\n    return ok;', '    return ok;'),
    'play-remounts': ('    if((int32_t)guest_call(sub_00062260,1,s)!=-1) ok=1;  /* already mounted */\n    else if',
                      '    if'),
    'play-table-full-ignored': ('#define TS_MOUNT_KEEP 8u', '#define TS_MOUNT_KEEP 0u'),
    'play-mount-failure-ignored': ('guest_call(sub_000622D0,3,s,whole,1u)>=0) ok=1;', 'guest_call(sub_000622D0,3,s,whole,1u)>=-1) ok=1;'),
    'fallback-arg-kept': ('        WORD_AT(g_esp+4u)=TS_FALLBACK;\n', ''),
    'fallback-header-kept': ('            WORD_AT(info+0x08u)=(WORD_AT(info+0x08u)&0xFFFFu)|(TS_FALLBACK<<16);\n', ''),
    'fallback-header-low-half': ('(WORD_AT(info+0x08u)&0xFFFFu)|(TS_FALLBACK<<16)', '(TS_FALLBACK<<16)'),
    'fallback-current-kept': ('        if(WORD_AT(TS_CURRENT)==TS_ABSTRACT) WORD_AT(TS_CURRENT)=TS_FALLBACK;\n', ''),
    'fallback-not-lab': ('#define TS_FALLBACK 1u', '#define TS_FALLBACK 2u'),
    'pak-wrong': ('="pak/arcade/l_102.pak";', '="pak/arcade/l_100.pak";'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = TILESETS.read_text(encoding='utf-8')
        assert old in text, mutation
        (tmp_path / TILESETS.name).write_text(text.replace(old, new, 1), encoding='utf-8')
    exe = tmp_path / ('tilesets.exe' if os.name == 'nt' else 'tilesets')
    source = ROOT / 'tests/mapmaker/mapmaker_tilesets.c'
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
def test_mapmaker_tilesets(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_MAPMAKER')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker tilesets ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_save_tier_rule():
    """A tileset-0 map is past the stock limits (fork 64 KB tier); covered by the
    container fixture (tests/mapmaker/test_mapmaker_container.py, over-abstract-*)."""
    text = CONTAINER.read_text(encoding='utf-8')
    assert "if(tag==0x00C77668u && size>=0x0Cu && (WORD_AT(p+8u+8u)>>16)==0u) return 1;" in text
    fixture = (ROOT / 'tests/mapmaker/mapmaker_container.c').read_text(encoding='utf-8')
    assert 'tileset 0, Abstract: fork tier' in fixture


def test_stock_tileset_data():
    """The table, menu, loader and editor facts the hooks rely on (US XBE)."""
    read = _xbe()
    u32 = lambda va: struct.unpack('<I', read(va, 4))[0]
    rec = lambda t, off: 0x0052CF38 + t * 0x58 + off
    # Record 0 is Abstract: text 0xAAF, tile models 1980.., default sky 60, no image, Lab music.
    assert [u32(rec(t, 0)) for t in range(6)] == [0xAAF + t for t in range(6)]
    assert [u32(rec(t, 4)) for t in range(6)] == [0x7BC + 29 * t for t in range(6)]
    assert read(rec(0, 0xA), 1)[0] == 60
    assert [u32(rec(t, 0x54)) for t in range(6)] == [0, 0x17D, 0x17B, 0x17E, 0x17C, 0x17F]
    assert u32(rec(0, 0x50)) == 0x1A0 and u32(rec(1, 0x50)) == 0x19F
    # Map Settings: the row loop skips record 0 and adds rows with sub_00071800(text, index).
    assert read(0x00313508, 8) == bytes.fromhex('33ffbe38cf5200' + '90')
    assert read(0x00313510, 0x2C) == bytes.fromhex(
        '81fe90cf5200' '7420' '81fee8cf5200' '7418' '81fe40d05200' '7410' '81fe98d05200' '7408'
        '81fef0d05200' '7515' '8b16' '57' '52')
    assert read(0x00313545, 5) == bytes.fromhex('e8b6e2d5ff')          # call sub_00071800 -> 0x0031354A
    assert read(0x00313551, 8) == bytes.fromhex('81fe48d152007cb7')    # loop bound 0x0052D148
    # Level load: tileset pak path from header +0xA, mounted whole on the level heap.
    assert read(0x000628E8, 0x14) == bytes.fromhex('0fb7400a' '50' 'e87e582d00' '6a01' '6a01' '50' 'e8d4f9ffff')
    assert read(0x0049FA94, 38) == b'pak/stream/lv100/tileset/tileset1.pak\0'
    assert read(0x000620F9, 3) == bytes.fromhex('83f81e')                # 30 mount slots
    # Editor: index 0 is a real case (palette props from model 0x7BC, preview models).
    assert u32(0x00311F98) == 0x00311EF4 and read(0x00311EF4, 6) == bytes.fromhex('8d8ebc070000')
    assert read(0x0033811E, 3) == bytes.fromhex('83fb01') and read(0x00338123, 2) == bytes.fromhex('7d14')
    assert read(0x00338134, 3) == bytes.fromhex('83fe1d')               # sub_00338110: 29 preview models of index 0
    # Play builder: tile model names and the tileset-1 pad fallback.
    assert read(0x0047D37C, 20) == b'tileset%d/tile%d_%d\0'
    assert read(0x00085AF4, 0x12) == bytes.fromhex('7f12' '57' '6a01' '6864d34700' 'e82db50c00' '83c40c')


def test_decoded_sites():
    """Each hook recognises its only caller (decoder, not the text listing)."""
    from mapmaker_decoded import decoded
    insns = decoded()

    def calls(target):
        return {i.address for i in insns if i.call and any(op[0] == 'imm' and op[1] == target for op in i.ops)}
    assert calls(0x00338170) == {0x000628ED}
    assert 0x00313545 in calls(0x00071800)
    readers = {i.function for i in insns if any(v in (0x0052CF38, 0x0052CF90) for _, v in i.values())}
    assert readers == {0x00311800, 0x00311820, 0x003133E0}, sorted(map(hex, readers))


GAME = ROOT / 'private/game/pak'
if not GAME.exists() and len(ROOT.parents) > 2:
    GAME = ROOT.parents[2] / 'private/game/pak'


def _index(path):
    with open(path, 'rb') as f:
        head = f.read(12)
        offset, length = struct.unpack_from('<II', head, 4)
        f.seek(offset)
        table = f.read(length)
    return {c: (pos, n) for c, pos, n, _ in struct.iter_unpack('<4I', table)}


def _crc(name):
    return zlib.crc32(name.encode()) & 0xFFFFFFFF


def test_abstract_assets():
    """Abstract's tiles and pads are in l_102.pak only; its item models are in tileset1.pak;
    l_100/l_102 share no name with different content (skipped without game data)."""
    if not (GAME / 'arcade/l_102.pak').exists():
        pytest.skip('game paks not available locally')
    read = _xbe()
    names = {'l_100': 'arcade/l_100.pak', 'l_102': 'arcade/l_102.pak',
             **{f't{t}': f'stream/lv100/tileset/tileset{t}.pak' for t in range(1, 7)}}
    idx = {k: _index(GAME / v) for k, v in names.items()}
    model = 'assets/ts/models/{0}/xbox/output/{1}.xbr'
    for t in range(1, 7):
        tiles = {k for k, v in idx.items() for m in range(1, 30)
                 if _crc(model.format(f'tileset{t}/tile{t}_{m}', f'tile{t}_{m}')) in v}
        pads = {k: sum(_crc(f'pad/data/tile{t}_{p}.raw') in v for p in range(1, 36)) for k, v in idx.items()}
        assert tiles == ({'l_102'} if t == 1 else {f't{t}'}), (t, tiles)
        assert pads['l_102'] == 35 and pads['l_100'] == (0 if t == 1 else 35), (t, pads)
    # Item column 0 models (prop table in l_100/l_102) are in tileset1.pak when streamed.
    with open(GAME / names['l_100'], 'rb') as f:
        pos, n = idx['l_100'][0xD9E2667A]
        f.seek(pos)
        table = f.read(n)
    start = struct.unpack_from('<I', table, 4)[0] + 8
    props = {}
    for o in range(start, len(table) - 0x27, 0x28):
        mid, _, _, name, _, _, _, _, flags, _ = struct.unpack_from('<iiiiifiiii', table, o)
        props[mid] = (table[name + 8:table.index(b'\0', name + 8)].decode('latin1'), flags)
    streamed = 0
    for item in range(80):
        base = 0x00524F60 + item * 0xC4
        flag = struct.unpack('<I', read(base + 0xB0, 4))[0]
        mid = struct.unpack('<i', read(base + 4, 4))[0]
        if not flag & 4 or mid < 0:
            continue
        name, flags = props[mid]
        if flags & 0x2000000:
            streamed += 1
            leaf = name.split('/')[-1]
            assert _crc(model.format(name, leaf)) in idx['t1'], name
    assert streamed == 24          # 26 per-tileset items; the two armours are resident (l_102)
    with open(GAME / names['l_100'], 'rb') as a, open(GAME / names['l_102'], 'rb') as b:
        for crc in set(idx['l_100']) & set(idx['l_102']):
            (pa, na), (pb, nb) = idx['l_100'][crc], idx['l_102'][crc]
            a.seek(pa)
            b.seek(pb)
            assert na == nb and a.read(na) == b.read(nb), hex(crc)


def test_wrappers_documented():
    doc = DOC.read_text(encoding='utf-8')
    for line in ('WRAP sub_00071800 -> pc_mapmaker_tilesets_menu_row',
                 'WRAP sub_00338170 -> pc_mapmaker_tilesets_play_pak'):
        assert line in doc, line
    text = TILESETS.read_text(encoding='utf-8')
    for name in ('pc_mapmaker_tilesets_menu_row', 'pc_mapmaker_tilesets_play_pak'):
        assert f'void {name}(void (*original)(void))' in text
