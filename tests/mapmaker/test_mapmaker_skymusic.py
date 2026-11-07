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
    'rows-bind-first': ('    if(ret==SMX_RET_MUSIC_BIND) {\n        for(k=0;', '    original();return;\n    if(ret==SMX_RET_MUSIC_BIND) {\n        for(k=0;'),
    'rows-twice': ('    if(smx.labels) return 1;\n', ''),
    'music-values-overlap': ('#define SMX_MUSIC_FIRST 58 ', '#define SMX_MUSIC_FIRST 57 '),
    'weather-value-stock': ('#define SMX_WEATHER_FIRST 4u', '#define SMX_WEATHER_FIRST 3u'),
    # rebuild
    'compile-music-kept': ('    if(music || m<-1 || m>SMX_MUSIC_STOCK_LAST) smx_set16(info+SMX_INFO_MUSIC,0xFFFFu);\n', ''),
    'compile-bad-music-kept': ('    if(music || m<-1 || m>SMX_MUSIC_STOCK_LAST) smx_set16', '    if(music) smx_set16'),
    'compile-sky-kept': ('    if((sky=smx_sky_row(s))!=0) smx_set16(info+SMX_INFO_SKY,(uint32_t)mapmaker_sky_extras[sky-1u].fallback);',
                         '    sky=smx_sky_row(s);'),
    'compile-weather-kept': ('    if(weather || w<0 || w>(int32_t)SMX_WEATHER_STOCK_LAST) *smx_byte(info+SMX_INFO_WEATHER)=0;\n', ''),
    'compile-stale-bits': ('    flags=WORD_AT(info+SMX_INFO_FLAGS)&~(SMX_MASK|MAPMAKER_VARIANT_FLAG);', '    flags=WORD_AT(info+SMX_INFO_FLAGS);'),
    'compile-bits-without-limits': ('    if(smx_on()) {\n        flags|=', '    {\n        flags|='),
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
    'mount-no-textures-pak': ('    if(!mem || smx_mount(mem)) return 1;', '    (void)mem;return 1;'),
    'mount-textures-optional': ("model's textures would be missing\",mem,0);\n    return 0;", "model's textures would be missing\",mem,0);\n    return 1;"),
    'mount-textures-other-level': ('"pak/arcademem/l_59_arm.pak"', '"pak/arcademem/l_58_arm.pak"'),
    'sand-no-textures-pak': ('smx_mount_pair(smx_weather[k-1u].pak,smx_weather[k-1u].mem)', 'smx_mount(smx_weather[k-1u].pak)'),
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
    'probe-textures-past-end': ('    end=WORD_AT(t+0x0Cu);', '    end=WORD_AT(t+0x0Cu)+1u;'),
    'probe-textures-placeholder': ('        if(id==0x40000000u || slot==0x40000000u) continue;\n', ''),
    'probe-textures-unterminated': ('        if((id=WORD_AT(file+list+k*16u+4u))==0xFFFFFFFFu) break;', '        id=WORD_AT(file+list+k*16u+4u);'),
    # v0.4.0-mapmaker.28: the entry's first word (the loaded-table row) read as the id
    'probe-textures-row-as-id': ('        if((id=WORD_AT(file+list+k*16u+4u))==0xFFFFFFFFu) break;', '        if((id=WORD_AT(file+list+k*16u))==0xFFFFFFFFu) break;'),
    'probe-textures-row-only': ('    for(k=0;k<end;k++) {\n        uint32_t a=rows+k*SMX_TEX_STRIDE+SMX_TEX_ID;', '    for(k=end;k<end;k++) {\n        uint32_t a=rows+k*SMX_TEX_STRIDE+SMX_TEX_ID;'),
    'probe-count-not-reset': ('    smx.tex_total=0;smx.tex_loaded=0;smx.grains=0;\n', ''),
    'probe-textures-never': ('    if(smx.level_logs==1u) smx_sky_textures(data,rec);', '    (void)smx_sky_textures;'),
    # weather
    'sand-without-pak': ('    if(!smx.sand_ready && !(smx.sand_ready=smx_mount_pair(smx_weather[k-1u].pak,smx_weather[k-1u].mem))) { smx_log("sandstorm pak or its textures pak not mounted; no sandstorm",0,0);return; }\n', ''),
    'sand-no-models': ('        if(model!=0xFFFFFFFFu) guest_call(sub_000A5E40,1,model);\n', ''),
    'sand-before-original': ('    uint32_t info,k,model;\n    original();\n', '    uint32_t info,k,model;\n'),
    'sand-list-without-buffers': ('    if(!WORD_AT(SMX_WX_BUF1) || !WORD_AT(SMX_WX_BUF2)) { smx_log(', '    if(0) { smx_log('),
    'sand-list-any-map': ('    if(smx.sand_on!=1 || smx_scene()!=100u || !WORD_AT(SMX_SANDSTORM)) return 0;', '    if(smx_scene()!=100u) return 0;'),
    'sand-start-list-kept': ('    WORD_AT(SMX_WX_LIST)=smx_weather[0].list;\n', ''),
    'sand-level-list-reset': ('        WORD_AT(g_esp+8u)=smx_weather[0].list;\n', ''),
    'sand-level-count-zero': ('        WORD_AT(g_esp+4u)=1u;\n', ''),
    'frame-any-map': ('    if(smx.sand_on!=1 || smx_scene()!=100u || !WORD_AT(SMX_SANDSTORM)) { original();return; }',
                      '    if(smx_scene()!=100u) { original();return; }'),
    'frame-flag-ignored': ('    if(smx.sand_on!=1 || smx_scene()!=100u || !WORD_AT(SMX_SANDSTORM)) { original();return; }',
                           '    if(smx.sand_on!=1 || smx_scene()!=100u) { original();return; }'),
    # MapMaker sandstorm: scene 2's emitter rooms hidden during the stock frame, our grains after
    'sand-rooms-kept': ('        rooms[k]=WORD_AT(a);WORD_AT(a)=0;', '        rooms[k]=WORD_AT(a);'),
    'sand-rooms-lost': ('    for(k=0;k<SMX_SAND_EMITTER_COUNT*SMX_SAND_ROOM_SLOTS;k++)\n        WORD_AT(SMX_SAND_EMITTERS', '    for(k=0;k<0u;k++)\n        WORD_AT(SMX_SAND_EMITTERS'),
    'sand-rooms-first-only': ('#define SMX_SAND_ROOM_SLOTS 4u', '#define SMX_SAND_ROOM_SLOTS 1u'),
    'sand-no-grains': ('    if(type==5u) made=smx_sand_emit();', '    (void)smx_sand_emit;'),
    'sand-grains-any-entry': ('    if(type==5u) made=smx_sand_emit();', '    made=smx_sand_emit();'),
    'sand-grains-indoors': (' || !(FLOAT_AT(SMX_WX_EXPOSED+p*4u)>0.0f)) continue;', ') continue;'),
    'sand-grains-any-player': ('        if(!(flags&0x80000000u) || !(flags&0x400u) || ', '        if('),
    'sand-grains-first-player': ('        if(!MAPMAKER_PTR_OK(pl,SMX_PL_POS+12u)) continue;', '        if(!MAPMAKER_PTR_OK(pl,SMX_PL_POS+12u) || p) continue;'),
    'sand-grains-past-ring': ('            if(g>=SMX_GRAINS) g=0;\n', ''),
    'sand-grains-one-slot': ('            WORD_AT(SMX_WX_RING)=(g+1u)%SMX_GRAINS;\n', ''),
    'sand-grains-unbounded': ('    if(steps>4u*SMX_GRAIN_EVERY) steps=4u*SMX_GRAIN_EVERY;\n', ''),
    'sand-grains-negative-steps': ('|| (int32_t)steps<=0) return 0;', ') return 0;'),
    'sand-grains-at-origin': ('        x=FLOAT_AT(pl+SMX_PL_POS);y=FLOAT_AT(pl+SMX_PL_POS+4u);z=FLOAT_AT(pl+SMX_PL_POS+8u);', '        x=0.0f;y=FLOAT_AT(pl+SMX_PL_POS+4u);z=FLOAT_AT(pl+SMX_PL_POS+8u);'),
    'sand-grains-still': ('            FLOAT_AT(e+0x0Cu)=smx_wind[0]*s+', '            FLOAT_AT(e+0x0Cu)=0.0f*s+'),
    'sand-grains-dead': ('            FLOAT_AT(e+0x20u)=0.0f;', '            FLOAT_AT(e+0x20u)=-1.0f;'),
    'frame-not-reset': ('smx.level_logs=0;smx.frames=0;', 'smx.level_logs=0;'),
    # v0.4.0-mapmaker.31: grains alive but invisible (stock alpha 16/255, haze 2/255, 24 m box, a 1 s life)
    'sand-grains-every-step': ('#define SMX_GRAIN_EVERY 2u', '#define SMX_GRAIN_EVERY 1u'),
    'sand-grains-wide': ('#define SMX_GRAIN_SPREAD 5.0f', '#define SMX_GRAIN_SPREAD 12.0f'),
    'sand-steps-not-reset': ('smx.sand_steps=SMX_GRAIN_EVERY-1u;', 'smx.sand_steps=0;'),
    'sand-steps-no-carry': ('    steps+=smx.sand_steps%SMX_GRAIN_EVERY;\n', ''),
    'fog-not-set': ('    WORD_AT(SMX_LEVEL100+SMX_LEVEL_FOG)=SMX_SAND_FOG;\n', ''),
    'fog-any-map': ('    if(!smx_sand_live()) return;\n    smx.tileset_fog', '    smx.tileset_fog'),
    'fog-before-original': ('    } else smx_level_sky(original);\n    smx_sand_fog();', '    } else { smx_sand_fog();smx_level_sky(original); }\n    if(0) smx_sand_fog();'),
    'fog-other-row': ('#define SMX_SAND_FOG 7u', '#define SMX_SAND_FOG 12u'),
    'sand-live-any-scene': ('return smx.sand_on==1 && smx_scene()==100u && WORD_AT(SMX_SANDSTORM); }', 'return smx.sand_on==1 && WORD_AT(SMX_SANDSTORM); }'),
    'sand-live-no-flag': ('return smx.sand_on==1 && smx_scene()==100u && WORD_AT(SMX_SANDSTORM); }', 'return smx.sand_on==1 && smx_scene()==100u; }'),
    'haze-any-caller': ('WORD_AT(g_esp)==SMX_RET_HAZE && smx_sand_live()', 'smx_sand_live()'),
    'haze-any-map': ('WORD_AT(g_esp)==SMX_RET_HAZE && smx_sand_live() &&', 'WORD_AT(g_esp)==SMX_RET_HAZE &&'),
    'haze-stock': ('        g_eax=(g_eax&0xFFFFFF00u)|SMX_HAZE_ALPHA;\n', ''),
    'haze-wrong-caller': ('#define SMX_RET_HAZE 0x0011B143u', '#define SMX_RET_HAZE 0x0011B13Eu'),
    'grain-colour-any-caller': ('WORD_AT(g_esp)==SMX_RET_GRAIN_COLOUR && ', ''),
    'grain-colour-any-colour': (' && WORD_AT(g_esp+4u)==SMX_GRAIN_COLOUR', ''),
    'grain-colour-any-map': (' && smx_sand_live()) {\n        WORD_AT(g_esp+4u)', ') {\n        WORD_AT(g_esp+4u)'),
    'grain-colour-stock': ('        WORD_AT(g_esp+4u)=(SMX_GRAIN_COLOUR&0xFFFFFF00u)|SMX_GRAIN_ALPHA;\n', ''),
    'grain-colour-wrong-caller': ('#define SMX_RET_GRAIN_COLOUR 0x0011B247u', '#define SMX_RET_GRAIN_COLOUR 0x0011B242u'),
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
    return [(s, f, p, label) for s, f, p, _, label in _sky_rows_mem()]


def _sky_rows_mem():
    """(sky, fallback, pak, textures pak or None, label) rows of mapmaker_sky_extras."""
    rows = re.findall(r'\{\s*(\d+),(\d+),"([^"]+)",(?:"([^"]+)"|0),"([^"]+)"\}',
                      _c_table(SKY.read_text(encoding='utf-8'), 'mapmaker_sky_extras[]'))
    assert len(rows) == 17
    return [(int(a), int(b), c, d or None, e) for a, b, c, d, e in rows]


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


def _f32(read, va):
    return struct.unpack('<f', read(va, 4))[0]


def test_stock_sandstorm_draw():
    """v0.4.0-mapmaker.31: what the sandstorm draw makes of a grain and of the
    haze, and the fog row of the story sandstorm levels (US XBE bytes)."""
    read = _xbe()
    # sub_0011B110: haze first (eax = 0x7F5A1E05, 8 layers -> sub_0011A930), return 0x0011B143.
    assert read(0x0011B137, 12) == bytes.fromhex('6a08b8051e5a7fe8edf7ffff')
    # Per live grain (age +0x20 >= 0): t = age/3, colour sub_00154FE0(0x7F643C10, fade),
    # return 0x0011B247; size t + 1.5; sprite sub_0001D5F0 at +0/+4/+8.
    assert read(0x0011B1B6, 6) == bytes.fromhex('f30f10440b20') and _f32(read, 0x00475CAC) == 0.0
    assert read(0x0011B1D7, 8) == bytes.fromhex('f30f590dd8cf4700') and abs(_f32(read, 0x0047CFD8) - 1 / 3) < 1e-6
    assert read(0x0011B23D, 10) == bytes.fromhex('68103c647fe8999d0300')
    assert read(0x0011B2C7, 8) == bytes.fromhex('f30f5805205d4700') and _f32(read, 0x00475D20) == 1.5
    assert read(0x0011B2E4, 5) == bytes.fromhex('e80723f0ff')
    # sub_00154FE0 scales only the low byte (alpha) by the clamped fade.
    assert read(0x00155002, 11) == bytes.fromhex('8b742408' '8bc6' '25ff000000')
    assert read(0x00155028, 6) == bytes.fromhex('81e600ffffff')
    # sub_0011A930: weight = eax low byte; quad alpha = (sum alpha x weight + 1) x 0.125.
    assert read(0x0011A936, 15) == bytes.fromhex('8be825ff000000' '578bf0' '6828030000')
    assert read(0x0011AB72, 16) == bytes.fromhex('d9442410d805785c4700d80d505d4700')
    assert _f32(read, 0x00475D50) == 0.125
    # Grain life: dead past 3 s (-1); the ring is 60 x 0x24.
    assert read(0x0011B0E0, 7) == bytes.fromhex('0f2f05185d4700') and _f32(read, 0x00475D18) == 3.0
    assert read(0x0011B0F9, 6) == bytes.fromhex('81ff70080000')
    # Level table 0x00515FF8 (0x80, scene +8): scenes 2/26/64 use fog row 7 (+0x14), scene 100's
    # entry is 0x00518778; the map start writes the tileset's row there (0x0023F40A).
    level = {_u32(read, 0x00515FF8 + k * 0x80 + 8): 0x00515FF8 + k * 0x80 for k in range(80)}
    assert level[100] == 0x00518778
    assert [_u32(read, level[s] + 0x14) for s in (2, 26, 64)] == [7, 7, 7]
    assert read(0x0023F405, 8) == bytes.fromhex('e866240d00894614')
    # sub_0006A710: fog row = 0x00515778 + row x 0x3C; row 7: colour 0x2F210F, 25..235.5 m.
    assert read(0x0006A747, 11) == bytes.fromhex('8b41146bc03c0578575100')
    fog = 0x00515778 + 7 * 0x3C
    assert _u32(read, fog + 4) == 0x2F210F00 and (_f32(read, fog + 0x14), _f32(read, fog + 0x18)) == (25.0, 235.5)
    # The level start applies it: sub_001CDE30 -> sub_0011D3C0 -> (jmp) sub_0011D240.
    assert read(0x001CDE70, 5) == bytes.fromhex('e84bf5f4ff') and read(0x0011D3DF, 5) == bytes.fromhex('e95cfeffff')


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


MEM_DIR = {'arcade': 'arcademem', 'challenge': 'challengemem', 'cutscene': 'cutscenemem', 'story': 'storymem'}


def _texture_ids(data):
    """Texture ids a model file names (+0: 16-byte entries, id first, -1 ends)."""
    ids, o = [], struct.unpack_from('<I', data, 0)[0]
    while (tid := struct.unpack_from('<I', data, o)[0]) != 0xFFFFFFFF:
        if tid != 0x40000000:
            ids.append(tid)
        o += 16
    return ids


def _entry(path, crc):
    pos, n = _index(path)[crc]
    with open(path, 'rb') as f:
        f.seek(pos)
        return f.read(n)


def test_texture_paks_named():
    """Each textures pak is the memory pak of the row's own level; the sandstorm's too."""
    for _, _, pak, mem, _ in _sky_rows_mem():
        if mem:
            d, f = pak[len('pak/'):].split('/')
            assert mem == f'pak/{MEM_DIR[d]}/{f[:-4]}m.pak' and len(mem) < 32, (pak, mem)
    assert '{"Sandstorm","pak/challenge/l_64_ch.pak","pak/challengemem/l_64_chm.pak",SMX_SAND_LIST}' in SMX.read_text(encoding='utf-8')


def test_sky_texture_paks():
    """v0.4.0-mapmaker.27 (skies not drawn or only in part): every texture an extra
    sky or sandstorm model names that no stock play pak holds is in the row's
    textures pak, and only there among the mounted paks; rows without one need
    none; a textures pak shares only byte-identical entries with the stock play
    paks (skipped without game data)."""
    if not (GAME / 'arcade/l_100.pak').exists():
        pytest.skip('game paks not available locally')
    read = _xbe()
    props = _props(GAME / 'arcade/l_100.pak')
    stock = {p: _index(GAME / p) for p in STOCK_PLAY + ['arcademem/l_100m.pak']}
    rows = [(struct.unpack('<i', read(0x005149A8 + sky * 0x34 + 4, 4))[0:1], pak, mem)
            for sky, _, pak, mem, _ in _sky_rows_mem()]
    rows.append((tuple(range(0x527, 0x52B)), 'pak/challenge/l_64_ch.pak', 'pak/challengemem/l_64_chm.pak'))
    for models, pak, mem in rows:
        level = _index(GAME / pak[len('pak/'):])
        textures = _index(GAME / mem[len('pak/'):]) if mem else {}
        need = []
        for model in models:
            ids = _texture_ids(_entry(GAME / pak[len('pak/'):], _model(props[model])))
            assert ids, props[model]
            need += [i for i in ids if not any(_crc(f'textures/{i:06x}.xbt') in v for v in stock.values())]
        assert bool(need) == bool(mem), (pak, mem, [hex(i) for i in need])
        for i in need:
            crc = _crc(f'textures/{i:06x}.xbt')
            assert crc in textures and crc not in level, (pak, hex(i))
        if mem:
            with open(GAME / mem[len('pak/'):], 'rb') as a:
                for name, sidx in stock.items():
                    with open(GAME / name, 'rb') as b:
                        for crc in set(textures) & set(sidx):
                            (pa, na), (pb, nb) = textures[crc], sidx[crc]
                            a.seek(pa)
                            b.seek(pb)
                            assert na == nb and a.read(na) == b.read(nb), (mem, name, hex(crc))


def test_texture_load_sites():
    """The texture path the probe and the fix rely on (XBE and decoder)."""
    read = _xbe()
    assert _cstr(read, 0x0047D3C8) == 'textures/%06x.xbt' and _cstr(read, 0x00475D90) == 'host0:textures/%06x.xbt'
    # Load pass: a read model's textures registered at once when its block +0x48 is 0x10.
    assert read(0x00063543, 5) == bytes.fromhex('83ff107411') and read(0x0006355D, 5) == bytes.fromhex('e8be2c0300')
    # sub_00018B90: loaded table [0x004B85C8], rows of 0x34 with the id at +0x1C, end at +0xC.
    assert read(0x00018BAB, 6) == bytes.fromhex('8b0dc8854b00') and read(0x00018BB4, 3) == bytes.fromhex('8b510c')
    assert read(0x00018BCF, 7) == bytes.fromhex('6bf6348d4c0e1c')
    assert read(0x00018C83, 4) == bytes.fromhex('895c111c')
    # Story levels mount their memory pak whole: sub_00062610(scene, 1) -> sub_000622D0(path, 1, 1).
    assert read(0x0006289B, 4) == bytes.fromhex('6a0157e8') and read(0x000628AC, 10) == bytes.fromhex('6a016a0156e81afaffff')
    from mapmaker_decoded import decoded
    insns = decoded()
    texture_pass = {i.address for i in insns if i.call and any(op[0] == 'imm' and op[1] == 0x00063900 for op in i.ops)}
    assert 0x0003D5E5 in texture_pass and 0x0003D5AD < 0x0003D5E5 < 0x0003D645


def test_sandstorm_rooms_and_texture_rows():
    """v0.4.0-mapmaker.28 (sandstorm armed and drawn every frame, nothing seen;
    sky probe counted 0 loaded): the sandstorm's emitters name scene 2's rooms,
    which the grain and gust code compare with the player's; grains are drawn
    with global texture 0x303 in flagged views; the texture pass writes the
    loaded-table row into the model's entry +0 (XBE)."""
    read = _xbe()
    rooms = [_u32(read, 0x004E62E8 + e * 0x44 + s * 4) for e in range(4) for s in range(4)]
    assert rooms == [2, 3, 7, 0, 6, 26, 27, 0, 5, 4, 0, 0, 19, 0, 0, 0]
    assert struct.unpack('<3f', read(0x004E62E8 + 0x44 + 0x18, 12)) == pytest.approx((0.53, 0.0, -0.85))
    text = SMX.read_text(encoding='utf-8')
    assert 'smx_wind[3]={0.53f,0.0f,-0.85f}' in text and '#define SMX_SAND_EMITTERS 0x004E62E8u' in text
    # grains: emitter table, player's room list word, compare; gust: player's room +0x2A0
    assert read(0x0011AD53, 5) == bytes.fromhex('bee8624e00') and read(0x0011AD69, 5) == bytes.fromhex('0fbf3b3bf8')
    assert read(0x0011A65B, 5) == bytes.fromhex('bae8624e00') and read(0x0011A66B, 6) == bytes.fromhex('3987a0020000')
    assert read(0x0011ADC2, 7) == bytes.fromhex('0faf1544794e00')                 # rate x [0x004E7944]
    # the stock player test the weather and our grains use
    assert read(0x0011A68F, 6) == bytes.fromhex('8b871c020000') and read(0x0011A69D, 3) == bytes.fromhex('f6c404')
    # emitter props created at the origin from +0x10 into +0x34
    assert read(0x001189CB, 5) == bytes.fromhex('be1c634e00') and read(0x00118A0C, 1) == b'\xe8'
    # draw: per-view flag, grain texture 0x303; haze texture 0x328; global texture handles
    assert read(0x0011B11B, 7) == bytes.fromhex('8b048d2cc87a00') and read(0x0011B194, 5) == bytes.fromhex('6803030000')
    assert read(0x0011A940, 5) == bytes.fromhex('6828030000')
    assert read(0x00080F20, 15) == bytes.fromhex('8b4424048d04408b04c5642d4d00c3')
    # texture pass: sub_000637E0 keeps the entry at +0x10/+0x14, sub_00063900 passes them,
    # sub_00018B90 writes -1 and then the row through its third argument
    assert read(0x00063816, 3) == bytes.fromhex('894e10') and read(0x0006383A, 3) == bytes.fromhex('894e14')
    assert read(0x00063929, 6) == bytes.fromhex('8b46148b4e10')
    assert read(0x00018BA4, 7) == bytes.fromhex('c74500ffffffff') and read(0x00018C91, 3) == bytes.fromhex('894500')
    assert read(0x000962CA, 3) == bytes.fromhex('565650')                         # (id, entry, entry)


def test_texture_entries_hold_the_id_twice():
    """The probe reads the id from an entry's second word: in every extra sky and
    sandstorm model file both words are the id (skipped without game data)."""
    if not (GAME / 'arcade/l_100.pak').exists():
        pytest.skip('game paks not available locally')
    read = _xbe()
    props = _props(GAME / 'arcade/l_100.pak')
    rows = [(struct.unpack('<i', read(0x005149A8 + sky * 0x34 + 4, 4))[0], pak) for sky, _, pak, _ in _sky_rows()]
    rows += [(m, 'pak/challenge/l_64_ch.pak') for m in range(0x527, 0x52B)]
    for model, pak in rows:
        data = _entry(GAME / pak[len('pak/'):], _model(props[model]))
        o = struct.unpack_from('<I', data, 0)[0]
        while True:
            first, second = struct.unpack_from('<II', data, o)
            assert first == second, (props[model], hex(first), hex(second))
            if first == 0xFFFFFFFF:
                break
            o += 16


def test_wrappers_documented():
    doc = DOC.read_text(encoding='utf-8')
    text = SMX.read_text(encoding='utf-8')
    for target, name in (('00078590', 'rows'), ('003006C0', 'compile'), ('00301A90', 'load'), ('00062790', 'mount'),
                         ('0023F390', 'level'), ('00119960', 'weather_load'), ('0011B310', 'weather_start'), ('00119AB0', 'weather_list'), ('0011D2C0', 'sky_built'), ('0011BBC0', 'weather_frame'),
                         ('00045DE0', 'music')):
        assert f'WRAP sub_{target} -> pc_mapmaker_skymusic_{name}' in doc, name
        assert f'void pc_mapmaker_skymusic_{name}(void (*original)(void))' in text, name
