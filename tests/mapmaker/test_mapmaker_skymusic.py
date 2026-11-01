"""Extra MapMaker skies, weather and music (src/interface/pc_mapmaker_skymusic.inc and the
extra-sky table in src/interface/pc_mapmaker_sky.inc): fixture with mutations, plus the
stock facts the design relies on, read from the US XBE, the decoder and the game
pak indexes when they are available locally (names and CRCs only)."""
import os
import re
import struct
import subprocess
import zlib
from pathlib import Path

import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
SKY = ROOT / 'src/interface/pc_mapmaker_sky.inc'
SMX = ROOT / 'src/interface/pc_mapmaker_skymusic.inc'
DOC = ROOT / 'docs/mapmaker/mapmaker-sky-music.md'

MUTATIONS = {
    # editor rows
    'rows-any-caller': ('if((ret!=SMX_RET_MUSIC_BIND && ret!=SMX_RET_SKY_BIND && ret!=SMX_RET_WEATHER_BIND) ||\n       !smx_on()',
                        'if(!smx_on()'),
    'rows-without-limits': ('       !smx_on() || smx_scene()!=102u || !smx_label_storage())', '       smx_scene()!=102u || !smx_label_storage())'),
    'rows-any-scene': ('       !smx_on() || smx_scene()!=102u || !smx_label_storage())', '       !smx_on() || !smx_label_storage())'),
    'rows-no-bind': ('MAPMAKER_SKY_EXTRA_COUNT);\n    original();\n}', 'MAPMAKER_SKY_EXTRA_COUNT);\n}'),
    'rows-bind-first': ('    if(ret==SMX_RET_MUSIC_BIND)\n        for(k=0;', '    original();return;\n    if(ret==SMX_RET_MUSIC_BIND)\n        for(k=0;'),
    'rows-twice': ('    if(smx.labels) return 1;\n', ''),
    'music-values-overlap': ('#define SMX_MUSIC_FIRST 58 ', '#define SMX_MUSIC_FIRST 57 '),
    'weather-value-stock': ('#define SMX_WEATHER_FIRST 4u', '#define SMX_WEATHER_FIRST 3u'),
    # rebuild
    'compile-music-kept': ('    if(music || m<-1 || m>SMX_MUSIC_STOCK_LAST) smx_set16(info+SMX_INFO_MUSIC,0xFFFFu);\n', ''),
    'compile-bad-music-kept': ('    if(music || m<-1 || m>SMX_MUSIC_STOCK_LAST) smx_set16', '    if(music) smx_set16'),
    'compile-sky-kept': ('    if((sky=smx_sky_row(s))!=0) smx_set16(info+SMX_INFO_SKY,(uint32_t)mapmaker_sky_extras[sky-1u].fallback);',
                         '    sky=smx_sky_row(s);'),
    'compile-weather-kept': ('    if(weather || w<0 || w>(int32_t)SMX_WEATHER_STOCK_LAST) *smx_byte(info+SMX_INFO_WEATHER)=0;\n', ''),
    'compile-stale-bits': ('    flags=WORD_AT(info+SMX_INFO_FLAGS)&~SMX_MASK;', '    flags=WORD_AT(info+SMX_INFO_FLAGS);'),
    'compile-bits-without-limits': ('    if(smx_on())\n        flags|=', '    flags|='),
    'compile-global-container': ('    original();\n    ed=WORD_AT(SMX_EDITOR);\n    if(!MAPMAKER_PTR_OK(ed,SMX_ED_WEATHER+4u) || !(info=mapmaker_sky_info(container))) return;\n    m=',
                                 '    original();\n    ed=WORD_AT(SMX_EDITOR);\n    if(!MAPMAKER_PTR_OK(ed,SMX_ED_WEATHER+4u) || !(info=mapmaker_sky_info(WORD_AT(SMX_CONTAINER)))) return;\n    m='),
    'music-bits-clouds': ('#define SMX_MUSIC_MASK 0x1F000000u', '#define SMX_MUSIC_MASK 0x1F040000u'),
    'sky-bits-stock': ('#define MAPMAKER_SKY_EXTRA_SHIFT 19u', '#define MAPMAKER_SKY_EXTRA_SHIFT 17u'),
    # load
    'load-ignored': ('    if((k=smx_bits(flags,SMX_MUSIC_MASK,SMX_MUSIC_SHIFT,SMX_MUSIC_COUNT))!=0) WORD_AT(ed+SMX_ED_MUSIC)', '    if(0) WORD_AT(ed+SMX_ED_MUSIC)'),
    'load-any-scene': ('    if(!smx_on() || smx_scene()!=102u) return;\n    ed=', '    if(!smx_on()) return;\n    ed='),
    'load-unchecked-row': ('    return k<=count?k:0u;', '    return k;'),
    # mounts
    'mount-no-reset': ('    smx.sky_row=0;smx.sand_ready=0;smx.sand_on=0;smx.level_logs=0;smx.frames=0;\n', ''),
    'mount-at-level-start': ('    if(pending==100u && smx_on()) smx_log(', '    if(smx_play_info()) mapmaker_sky.extra_ready=smx_mount(mapmaker_sky_extras[1].pak);\n    if(pending==100u && smx_on()) smx_log('),
    'mount-no-prepare-hook': ('mapmaker_sky.extra_ready=0;mapmaker_sky.extra_prepare=smx_prepare_sky;', 'mapmaker_sky.extra_ready=0;mapmaker_sky.extra_prepare=0;'),
    'mount-skip-ignored': ('    if(WORD_AT(SMX_MOUNT_SKIP)) { original();return; }   /* the original does nothing */\n', ''),
    'mount-whole-file': ('guest_call(sub_000622D0,3,path,0u,1u)', 'guest_call(sub_000622D0,3,path,1u,1u)'),
    'mount-no-budget': ('    if((count=WORD_AT(SMX_MOUNTS))+SMX_MOUNT_KEEP>SMX_MOUNT_MAX) {', '    if((count=WORD_AT(SMX_MOUNTS))>SMX_MOUNT_MAX) {'),
    'mount-twice': ('    if((slot=(int32_t)guest_call(sub_00062260,1,path))!=-1) { smx_logs("pak already mounted",pak,(uint32_t)slot);return 1; }\n', ''),
    'mount-failure-ready': ('    if((slot=(int32_t)guest_call(sub_000622D0,3,path,0u,1u))<0) { smx_logs("pak did not mount",pak,count);return 0; }',
                            '    slot=(int32_t)guest_call(sub_000622D0,3,path,0u,1u);'),
    'prepare-stale-row': ('    if(smx.sky_row!=k) {', '    if(!smx.sky_row) {'),
    'prepare-any-scene': ('    if(!k || smx_scene()!=100u || !smx_on()) return 0;', '    if(!k || !smx_on()) return 0;'),
    'prepare-ignores-failure': ('    }\n    return mapmaker_sky.extra_ready?k:0u;\n}', '    }\n    return k;\n}'),
    # map start
    'level-without-pak': ('    if(!info || !(k=smx_prepare_sky(info))) {', '    if(!info || !(k=smx_bits(WORD_AT(info+SMX_INFO_FLAGS),MAPMAKER_SKY_EXTRA_MASK,MAPMAKER_SKY_EXTRA_SHIFT,MAPMAKER_SKY_EXTRA_COUNT))) {'),
    'level-no-restore': ('    original();\n    smx_set16(info+SMX_INFO_SKY,(uint32_t)old);', '    original();'),
    'level-no-row-check': ('    if(!smx_model_ok(model)) {\n        smx_log("map start: extra sky model', '    if(0) {\n        smx_log("map start: extra sky model'),
    'sky-index-without-prepare': ('    if(mapmaker_sky.extra_prepare) return mapmaker_sky.extra_prepare(info);\n', ''),
    # probe
    'probe-every-view': ('       smx.level_logs>=2u) return;', '       0) return;'),
    'probe-before-original': ('    uint32_t view=MAPMAKER_PTR_OK(g_esp,8u)?WORD_AT(g_esp+4u):0u,info,rec,model,row,e=0,req=0,data=0,prop;\n    original();\n',
                              '    uint32_t view=MAPMAKER_PTR_OK(g_esp,8u)?WORD_AT(g_esp+4u):0u,info,rec,model,row,e=0,req=0,data=0,prop;\n    original();original();\n'),
    # weather
    'sand-without-pak': ('    if(!smx.sand_ready && !(smx.sand_ready=smx_mount(smx_weather[k-1u].pak))) { smx_log("sandstorm pak not mounted; no sandstorm",0,0);return; }\n', ''),
    'sand-no-models': ('        if(model!=0xFFFFFFFFu) guest_call(sub_000A5E40,1,model);\n', ''),
    'sand-before-original': ('    uint32_t info,k,model;\n    original();\n', '    uint32_t info,k,model;\n'),
    'sand-list-without-buffers': ('    if(!WORD_AT(SMX_WX_BUF1) || !WORD_AT(SMX_WX_BUF2)) { smx_log(', '    if(0) { smx_log('),
    'sand-list-any-map': ('    if(smx.sand_on!=1 || smx_scene()!=100u || !WORD_AT(SMX_SANDSTORM)) return 0;', '    if(smx_scene()!=100u) return 0;'),
    'sand-start-list-kept': ('    WORD_AT(SMX_WX_LIST)=smx_weather[0].list;\n', ''),
    'sand-level-list-reset': ('        WORD_AT(g_esp+8u)=smx_weather[0].list;\n', ''),
    'sand-level-count-zero': ('        WORD_AT(g_esp+4u)=1u;\n', ''),
    'frame-any-map': ('    if(smx.sand_on!=1 || smx_scene()!=100u) return;\n    smx.frames++;', '    smx.frames++;'),
    'frame-not-reset': ('smx.level_logs=0;smx.frames=0;', 'smx.level_logs=0;'),
    # music
    'music-any-caller': ('    if(MAPMAKER_PTR_OK(g_esp,4u) && WORD_AT(g_esp)==SMX_RET_MUSIC_PLAY && g_eax==0xFFFFFFFFu &&',
                         '    if(MAPMAKER_PTR_OK(g_esp,4u) && g_eax==0xFFFFFFFFu &&'),
    'music-over-setup': ('WORD_AT(g_esp)==SMX_RET_MUSIC_PLAY && g_eax==0xFFFFFFFFu &&', 'WORD_AT(g_esp)==SMX_RET_MUSIC_PLAY &&'),
    'music-without-limits': ('    if(smx_scene()!=100u || !smx_on()) return 0;', '    if(smx_scene()!=100u) return 0;'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        hits = 0
        for src in (SMX, SKY):
            text = src.read_text(encoding='utf-8')
            if old in text:
                hits += 1
                (tmp_path / src.name).write_text(text.replace(old, new, 1), encoding='utf-8')
        assert hits == 1, mutation
    exe = tmp_path / ('skymusic.exe' if os.name == 'nt' else 'skymusic')
    source = ROOT / 'tests/mapmaker/mapmaker_skymusic.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), '-I' + str(ROOT / 'src'), str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_skymusic(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_MAPMAKER')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker skymusic ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


# ---- stock facts --------------------------------------------------------------

def _private():
    private = ROOT / 'private'
    if not (private / 'game/default.xbe').is_file() and len(ROOT.parents) > 2:
        private = ROOT.parents[2] / 'private'
    return private


def _xbe():
    path = _private() / 'game/default.xbe'
    if not path.exists():
        pytest.skip('US XBE not available locally')
    data = path.read_bytes()
    base = struct.unpack_from('<I', data, 0x104)[0]
    count, headers = struct.unpack_from('<II', data, 0x11C)
    sections = [struct.unpack_from('<IIIII', data, headers - base + i * 0x38) for i in range(count)]

    def read(va, n):
        for _, start, vsize, raw, rsize in sections:
            if start <= va < start + vsize:
                off = va - start
                chunk = data[raw + off:raw + min(off + n, rsize)]
                return chunk + bytes(n - len(chunk))
        raise KeyError(hex(va))
    return read


def _u32(read, va):
    return struct.unpack('<I', read(va, 4))[0]


def _cstr(read, va):
    return read(va, 96).split(b'\0')[0].decode('latin1')


def _c_table(text, name):
    body = text[text.index(name):]
    return body[:body.index('};')]


def _sky_rows():
    """(sky, fallback, pak, label) rows of mapmaker_sky_extras."""
    rows = re.findall(r'\{\s*(\d+),(\d+),"([^"]+)","([^"]+)"\}', _c_table(SKY.read_text(encoding='utf-8'), 'mapmaker_sky_extras[]'))
    return [(int(a), int(b), c, d) for a, b, c, d in rows]


def _music_rows():
    return [int(v, 16) for v in re.findall(r'\{0x([0-9A-F]+)u,"', _c_table(SMX.read_text(encoding='utf-8'), 'smx_music[]'))]


def test_tables_fit_their_bits():
    skies, music = _sky_rows(), _music_rows()
    assert len(skies) == 17 and len(music) == 28
    assert len(skies) <= 31 and len(music) <= 31            # 5-bit fields, 0 = none
    assert len({s for s, *_ in skies}) == 17 and len(set(music)) == 28
    for sky, fallback, pak, _ in skies:
        assert not 58 <= sky <= 67 and sky != 0 and 58 <= fallback <= 67
        assert len(pak) < 32                                  # SMX_PATH_MAX
    # Bits 19..30: free of the stock bits and of Clouds (0x40000).
    text = SMX.read_text(encoding='utf-8')
    masks = [0x00F80000, 0x1F000000, 0x60000000]
    assert '0x00F80000u' in SKY.read_text(encoding='utf-8') and '0x1F000000u' in text and '0x60000000u' in text
    used = 0x1 | 0x7FFE | 0x8000 | 0x10000 | 0x20000 | 0x40000
    for m in masks:
        assert not m & used
    assert not masks[0] & masks[1] and not masks[1] & masks[2] and not masks[0] & masks[2]


def test_stock_music_data():
    """The jukebox list, the music file table and what the list leaves out."""
    read = _xbe()
    files = {i: _cstr(read, _u32(read, 0x00535530 + i * 4)) for i in range(1, 0xCA)}
    assert _u32(read, 0x00535530 + 0xCA * 4) == 0                       # 0xC9 is the last id
    jukebox = [_u32(read, 0x00537210 + k * 8) for k in range(58)]
    assert jukebox[0] == 0xFFFFFFFF and jukebox[57] == 0xFFFFFFFE       # none, random
    offered = {files[i] for i in jukebox[1:57]}
    extra = _music_rows()
    for i in extra:
        assert files[i] not in offered, hex(i)
        assert not (0x4B <= i <= 0xAE and (i - 0x4B) % 5), hex(i)     # base language entry
    # Every file the list leaves out is in the extra table (cutscenes by their base id).
    left = {files[i] for i in range(1, 0xCA)
            if not (0x4B <= i <= 0xAE and (i - 0x4B) % 5)} - offered
    assert left == {files[i] for i in extra}
    # The editor loop offers indexes 1..0x39 (sub_0035DE60 always 1).
    assert read(0x0035DE60, 6) == bytes.fromhex('b801000000c3')
    assert read(0x00313751, 3) == bytes.fromhex('83fe3a')


def test_stock_sky_weather_data():
    read = _xbe()
    # Sky list of the editor and the records the extras use (model at +4).
    assert struct.unpack('<22i', read(0x0052D150, 88))[0::2] == (-1, *range(58, 68))
    for sky, *_ in _sky_rows():
        assert struct.unpack('<i', read(0x005149A8 + sky * 0x34 + 4, 4))[0] > 0
    # Weather list of the editor: 4 values 0..3; the map-start table has 4 real rows.
    assert [read(0x0052D1A8 + k * 8, 1)[0] for k in range(4)] == [0, 1, 2, 3]
    assert struct.unpack('<8I', read(0x005196DC, 32)) == (0xFFFFFFFF, 0, 0x139, 0x00119920, 0x139, 0x00119920, 0x18, 0x00119940)
    # Sandstorm: models 0x527..0x52A, scene 2's one-entry list of type 5, the default list type 0.
    assert [_u32(read, 0x004E62F8 + k * 0x44) for k in range(4)] == [0x527, 0x528, 0x529, 0x52A]
    assert _u32(read, 0x00507898) == 5 and _u32(read, 0x004E63F8) == 0
    assert _u32(read, 0x00508390 + 0x94) == 0x00507898 and _u32(read, 0x00508390 + 0x98) == 1
    assert _u32(read, 0x00516078 + 0x24) == 0x00508390                  # level entry of scene 2
    # Call sites recognised by return address.
    assert read(0x00313763, 5) == bytes.fromhex('e8284ed6ff')            # music bind -> 0x00313768
    assert read(0x0031383E, 5) == bytes.fromhex('e84d4dd6ff')            # sky bind -> 0x00313843
    assert read(0x00313901, 5) == bytes.fromhex('e88a4cd6ff')            # weather bind -> 0x00313906
    assert read(0x00045FB7, 5) == bytes.fromhex('e824feffff')            # music start -> 0x00045FBC
    assert read(0x0006298E, 5) == bytes.fromhex('6890874700')            # gun.pak index-only mount


def test_decoded_sites():
    """Who reads the three editor fields, the single callers of the hooked level
    functions, and the type-5 tests (lifter's decoder)."""
    from mapmaker_decoded import decoded
    insns = decoded()

    def calls(target):
        return {i.address for i in insns if i.call and any(op[0] == 'imm' and op[1] == target for op in i.ops)}
    assert calls(0x0023F390) == {0x0006C95E}
    assert calls(0x00119960) == {0x001CDCEB}
    assert calls(0x00062790) == {0x001CDB85}
    assert 0x00045FB7 in calls(0x00045DE0)
    # Order of a MapMaker level load: mounts, cloud setup, then (sub_001CDC50) weather load,
    # the map start (tail of the scene-100 case) and only then the load pass.
    assert calls(0x0011C9F0) == {0x001CDBB7}
    assert calls(0x001CDB80) == {0x0003D590} and calls(0x001CDC50) == {0x0003D5A3}
    assert calls(0x00063390) >= {0x0003D5AD}
    # The level setup after the weather start resets the weather list (the
    # second sandstorm bug): sub_001CDE30 then sub_0006EC10 -> sub_0006D7A0 ->
    # sub_00119AB0 (one caller); the views (sky props) are built in sub_001CDE30.
    assert calls(0x001CDE30) == {0x0003D645} and calls(0x0006EC10) == {0x0003D64A}
    assert calls(0x0006D7A0) == {0x0006ED4D, 0x0006EDD2} and calls(0x00119AB0) == {0x0006E87A}
    assert calls(0x0011B430) == {0x001CE046, 0x001CE0BF} and calls(0x0011D2C0) == {0x000CC961}
    assert calls(0x0011BBC0) == {0x001CE55F, 0x001CE596}
    assert calls(0x000CCA10) >= {0x001CDF9A}
    at0 = {i.address: i for i in insns}
    # sub_00062790 mounts for the pending scene byte while [0x007DE398] has 0x800000.
    assert at0[0x000627A0].mnemonic == 'test' and at0[0x000627B4].text == 'edi, byte ptr [0x7de394]'
    # Integer accesses with displacement 0x210/0x220/0x224 in the editor code: the
    # defaults, the rebuild, the loader, the menu close, and an unrelated object.
    fields = {i.address for i in insns if 0x002F8000 <= i.address < 0x00340000 and not i.mnemonic.startswith('movss')
              and any(op[0] == 'mem' and op[4] in (0x210, 0x220, 0x224) and op[1] not in ('esp', 'ebp') for op in i.ops)}
    assert fields == {0x002FE2B8, 0x002FE2F0, 0x002FE307,          # sub_002FE2B0 defaults
                      0x00300784, 0x00300795, 0x003007A5,          # rebuild
                      0x00301C5D, 0x00301C6D, 0x00301C7C,          # loader
                      0x003133A1, 0x003133B9, 0x003133C3,          # Map Settings close
                      0x003154C0, 0x003154FA, 0x003155D6}, sorted(map(hex, fields))  # [0x00783B90] object
    at = {i.address: i for i in insns}
    assert at[0x0011BC4F].mnemonic == 'cmp' and at[0x0011BCB5].mnemonic == 'cmp'


GAME = _private() / 'game/pak'


def _index(path):
    with open(path, 'rb') as f:
        head = f.read(12)
        offset, length = struct.unpack_from('<II', head, 4)
        f.seek(offset)
        table = f.read(length)
    return {c: (pos, n) for c, pos, n, _ in struct.iter_unpack('<4I', table)}


def _crc(name):
    return zlib.crc32(name.encode()) & 0xFFFFFFFF


def _props(path):
    idx = _index(path)
    pos, n = idx[0xD9E2667A]
    with open(path, 'rb') as f:
        f.seek(pos)
        table = f.read(n)
    start = struct.unpack_from('<I', table, 4)[0] + 8
    props = {}
    for o in range(start, len(table) - 0x27, 0x28):
        mid, _, _, name, *_ = struct.unpack_from('<iiiiifiiii', table, o)
        props[mid] = table[name + 8:table.index(b'\0', name + 8)].decode('latin1')
    return props


def _model(name):
    leaf = name.split('/')[-1]
    return _crc(f'assets/ts/models/{name}/xbox/output/{leaf}.xbr')


STOCK_PLAY = ['arcade/l_100.pak', 'gun.pak', 'chr.pak', 'musicts.pak'] + [f'stream/lv100/tileset/tileset{t}.pak' for t in range(1, 7)]


def test_pak_assets():
    """Each extra sky's model is in its pak and in no pak MapMaker play mounts; the
    sandstorm models are in l_64_ch.pak; every extra track is in musicts.pak; an
    extra pak shares only byte-identical entries with the stock play paks
    (skipped without game data)."""
    if not (GAME / 'arcade/l_100.pak').exists():
        pytest.skip('game paks not available locally')
    read = _xbe()
    props = _props(GAME / 'arcade/l_100.pak')
    stock = {p: _index(GAME / p) for p in STOCK_PLAY}
    extra = {}
    for sky, _, pak, _ in _sky_rows():
        rel = pak[len('pak/'):]
        extra.setdefault(rel, _index(GAME / rel))
        model = struct.unpack('<i', read(0x005149A8 + sky * 0x34 + 4, 4))[0]
        crc = _model(props[model])
        assert crc in extra[rel], (sky, props[model])
        assert not any(crc in v for v in stock.values()), (sky, props[model])
    sand = extra.setdefault('challenge/l_64_ch.pak', _index(GAME / 'challenge/l_64_ch.pak'))
    for m in range(0x527, 0x52B):
        assert props[m] == f'intro/intro_sandstorm_{m - 0x526}' and _model(props[m]) in sand
    music = stock['musicts.pak']
    for i in _music_rows():
        assert _crc(_cstr(read, _u32(read, 0x00535530 + i * 4))) in music, hex(i)
    for rel, idx in extra.items():
        with open(GAME / rel, 'rb') as a:
            for name, sidx in stock.items():
                with open(GAME / name, 'rb') as b:
                    for crc in set(idx) & set(sidx):
                        (pa, na), (pb, nb) = idx[crc], sidx[crc]
                        a.seek(pa)
                        b.seek(pb)
                        assert na == nb and a.read(na) == b.read(nb), (rel, name, hex(crc))


def test_wrappers_documented():
    doc = DOC.read_text(encoding='utf-8')
    text = SMX.read_text(encoding='utf-8')
    for target, name in (('00078590', 'rows'), ('003006C0', 'compile'), ('00301A90', 'load'), ('00062790', 'mount'),
                         ('0023F390', 'level'), ('00119960', 'weather_load'), ('0011B310', 'weather_start'), ('00119AB0', 'weather_list'), ('0011D2C0', 'sky_built'), ('0011BBC0', 'weather_frame'),
                         ('00045DE0', 'music')):
        assert f'WRAP sub_{target} -> pc_mapmaker_skymusic_{name}' in doc, name
        assert f'void pc_mapmaker_skymusic_{name}(void (*original)(void))' in text, name
