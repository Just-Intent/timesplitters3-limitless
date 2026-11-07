"""Imported music in MapMaker and Arcade (src/interface/pc_mapmaker_custommusic.inc
through the pc_mapmaker_skymusic.inc hooks): fixture with mutations, plus the
stock facts the design relies on, read from the US XBE when it is available
locally. No game data is copied."""
import os
import struct
import subprocess
from pathlib import Path

import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
CMU = ROOT / 'src/interface/pc_mapmaker_custommusic.inc'
SMX = ROOT / 'src/interface/pc_mapmaker_skymusic.inc'
FIXTURE = ROOT / 'tests/mapmaker/mapmaker_custommusic.c'

MUTATIONS = {
    'rows-not-offered': ('    cmu_rows();\n', ''),
    'rows-missing-offered': ('        if(cmu.list.present[k]) { guest_call(sub_00071800,2,cmu_label(k),CMU_VALUE+k);n++; }',
                             '        { guest_call(sub_00071800,2,cmu_label(k),CMU_VALUE+k);n++; }'),
    'rows-stock-values': ('#define CMU_VALUE 0x100u', '#define CMU_VALUE 0x50u'),
    'compile-not-called': ('    if(smx_on()) cmu_compile(container,ret);', '    (void)ret;'),
    'compile-without-limits': ('    if(smx_on()) cmu_compile(container,ret);', '    cmu_compile(container,ret);'),
    'compile-any-value': ('    if(m<CMU_VALUE || m-CMU_VALUE>=cmu.list.count) return;', '    if(m<CMU_VALUE) return;'),
    'compile-used-unchecked': ('    if(term!=c+used-8u || used+8u+CMU_CHUNK+4u>CMU_SCAN)', '    if(used+8u+CMU_CHUNK+4u>CMU_SCAN)'),
    'compile-used-stale': ('    WORD_AT(info+0x248u)=used;\n    if(g_eax) {', '    if(g_eax) {'),
    'compile-return-stale': ('        g_eax=used;\n        if(ret==CMU_SAVE_COMPILE_RET', '        if(ret==CMU_SAVE_COMPILE_RET'),
    'compile-no-terminator': ('    WORD_AT(nterm)=CMU_END;WORD_AT(nterm+4u)=nterm-c;\n', ''),
    'load-not-called': ('    cmu_load(container);\n', ''),
    'load-missing-kept': ('    if((k=pc_music_find(&cmu.list,name))<0) { cmu_log("map\'s imported track is missing; tileset music",name,0);return; }',
                          '    k=pc_music_find(&cmu.list,name);if(k<0) k=0;'),
    'chunk-any-version': ('    if(cmu_byte(data)[0]!=1u || cmu_byte(data)[1]!=0 || !n', '    if(!n'),
    'play-not-called': ('    if(cmu_music(smx_on())) { original();return; }', '    (void)cmu_music;'),
    'play-eax-kept': ('    g_eax=0;\n    return 1;', '    return 1;'),
    'play-over-setup': ('limits && scene==100u && g_eax==0xFFFFFFFFu &&', 'limits && scene==100u &&'),
    'play-any-caller': ('    if(ret!=CMU_RET_MAP && ret!=CMU_RET_ARCADE) return 0;\n', ''),
    'play-music-off': ('    if(guest_call(sub_00045A70,0)) { cmu_log("music is switched off",name,0);return 0; }\n', ''),
    'play-no-stop': ('    pc_music_stop();                         /* a new level music ends an imported track */\n', ''),
    'play-map-without-limits': ('    if(!name && ret==CMU_RET_MAP && limits && scene==100u', '    if(!name && ret==CMU_RET_MAP && scene==100u'),
    'arcade-over-link': ('    arcade=ret==CMU_RET_ARCADE ? WORD_AT(CMU_ARCADE_INDEX)==0u :', '    arcade=ret==CMU_RET_ARCADE ? 1 :'),
    'arcade-in-story': (" && cmu_byte(CMU_MODE)[0]!=0x0Au &&", ' &&'),
    'watch-scene': ('    if(WORD_AT(CMU_SCENE)!=cmu.scene) return 0;\n', ''),
    'watch-pause': ('    *paused=WORD_AT(CMU_PAUSE)!=0;', '    *paused=0;'),
    'watch-slider': ('    *gain=(float)(slider>CMU_SLIDER_MAX?CMU_SLIDER_MAX:slider)/(float)CMU_SLIDER_MAX;', '    *gain=1.0f;'),
    'channel-zero-stops': ('    if(MAPMAKER_PTR_OK(g_esp,8u) && WORD_AT(g_esp+4u)!=0u && pc_music_playing()) {',
                           '    if(MAPMAKER_PTR_OK(g_esp,8u) && pc_music_playing()) {'),
    'channel-ignored': ('        pc_music_stop();\n        cmu_log("jingle channel', '        cmu_log("jingle channel'),
    'arcade-rows-any-caller': ('WORD_AT(g_esp)==CMU_RET_ARCADE_ROWS && g_ebx==0u', 'g_ebx==0u'),
    'arcade-rows-no-loop': ('WORD_AT(g_esp)==CMU_RET_ARCADE_ROWS && g_ebx==0u', 'WORD_AT(g_esp)==CMU_RET_ARCADE_ROWS'),
    'arcade-store-stock-kept': ('        WORD_AT(CMU_ARCADE_SHOWN)=0;WORD_AT(CMU_ARCADE_INDEX)=0;\n', ''),
    'arcade-store-profile-kept': ('        if(MAPMAKER_PTR_OK(profile,CMU_PROFILE_MUSIC+4u)) WORD_AT(profile+CMU_PROFILE_MUSIC)=0;\n', ''),
    'arcade-store-not-saved': ('            if(!pc_music_arcade_save(name)) cmu_log', '            if(0) cmu_log'),
    'arcade-store-never-cleared': ('    } else if(cmu.arcade[0]) {', '    } else if(0) {'),
    'arcade-store-past-offer': ('    if(v>=CMU_ARCADE_FIRST && v-CMU_ARCADE_FIRST<cmu.offered) {', '    if(v>=CMU_ARCADE_FIRST) {'),
    'arcade-show-skipped': ('    WORD_AT(item+CMU_ITEM_VALUE)=CMU_ARCADE_FIRST+p;', '    (void)item;'),
    'arcade-first-row': ('#define CMU_ARCADE_FIRST 58u', '#define CMU_ARCADE_FIRST 57u'),
}


def wav(path, frames=2205):
    data = struct.pack('<hh', 500, -500) * frames
    fmt = struct.pack('<HHIIHH', 1, 2, 22050, 22050 * 4, 4, 16)
    body = b'WAVE' + b'fmt ' + struct.pack('<I', len(fmt)) + fmt + b'data' + struct.pack('<I', len(data)) + data
    path.write_bytes(b'RIFF' + struct.pack('<I', len(body)) + body)


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    # Both files are copied: a quoted include is found beside its includer first.
    old, new = MUTATIONS.get(mutation, (None, None))
    hits = 0
    for src in (CMU, SMX):
        text = src.read_text(encoding='utf-8')
        if old and old in text:
            hits += text.count(old)
            text = text.replace(old, new, 1)
        (tmp_path / src.name).write_text(text, encoding='utf-8')
    assert mutation == 'fixed' or hits == 1, mutation
    exe = tmp_path / ('custommusic.exe' if os.name == 'nt' else 'custommusic')
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/I' + str(tmp_path),
                '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src'), str(FIXTURE), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L', '-D_DEFAULT_SOURCE',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), '-I' + str(ROOT / 'src'), str(FIXTURE),
                '-o', str(exe), '-lm']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_custommusic(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    music, settings = tmp_path / 'music', tmp_path / 'settings'
    music.mkdir()
    settings.mkdir()
    wav(music / 'Alpha.wav')
    wav(music / 'beta_song.wav')
    (music / 'readme.txt').write_text('ignored')
    env = {k: v for k, v in os.environ.items() if not k.startswith(('TSFP_', 'RECOMP_'))}
    result = subprocess.run([str(exe), str(music), str(settings)], env=env, cwd=tmp_path, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker custommusic ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_chunk_tag_is_free():
    """The chunk tag is new, and the container keeps such a map in the stock tier."""
    tag = '0x00C776F7'
    hits = [p for p in (ROOT / 'src').rglob('*') if p.is_file() and tag in p.read_text(encoding='utf-8', errors='ignore')]
    assert [p.name for p in hits] == ['pc_mapmaker_custommusic.inc']
    container = (ROOT / 'src/interface/pc_mapmaker_container.inc').read_text(encoding='utf-8')
    assert '0x00C776F7' not in container


# ---- stock facts (US XBE) -----------------------------------------------------

def _xbe():
    private = ROOT / 'private'
    if not (private / 'game/default.xbe').is_file() and len(ROOT.parents) > 2:
        private = ROOT.parents[2] / 'private'
    path = private / 'game/default.xbe'
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
                return data[raw + off:raw + off + n]
        raise KeyError(hex(va))
    return read


def test_stock_music_code():
    """The call sites, return addresses and fields the hooks rely on."""
    read = _xbe()
    call = lambda site, target: read(site, 5) == b'\xe8' + struct.pack('<i', target - site - 5)
    assert call(0x00045FB7, 0x00045DE0) and call(0x00045F46, 0x00045DE0)      # returns 0x45FBC / 0x45F4B
    assert call(0x002C8042, 0x00071BE0) and call(0x002C8029, 0x000717D0)      # Arcade rows, then 0x2C8047
    assert read(0x002C7FCB, 4) == bytes.fromhex('85db756d')                   # rows only when ebx is 0
    assert read(0x002C801A, 5) == bytes.fromhex('be14725300')                 # jukebox texts 0x00537214
    assert read(0x002C8034, 6) == bytes.fromhex('81fee4735300')               # ... 58 rows
    assert read(0x002C749C, 0x16) == bytes.fromhex('8b80c8000000a3d4137600a3d0137600898714010000')
    assert read(0x002C7232, 0xC) == bytes.fromhex('8b0dd41376008988c8000000')
    assert read(0x002C7223, 2) == b'\x6a\x0e' and read(0x002C748D, 2) == b'\x6a\x0e'
    assert read(0x00028A22, 6) == bytes.fromhex('893534914b00')               # sub_000289C0: [0x4B9134] = channel
    assert read(0x00029EAA, 7) == bytes.fromhex('c7462801000000')             # pause: +0x28 = 1
    assert read(0x00028964, 5) == bytes.fromhex('b998bd6400')                 # player 0x0064BD98
    assert read(0x00045BE8, 8) == bytes.fromhex('f30f2a05bc365200')           # slider [0x005236BC]
    assert struct.unpack('<f', read(0x00475DC4, 4))[0] == 0.0625              # / 16
    assert struct.unpack('<i', read(0x005236BC, 4))[0] == 11                  # default
    assert read(0x00045E7F, 7) == bytes.fromhex('803d55e47d000a')             # mode byte 10 = no setup choice
    assert read(0x00045E73, 5) == bytes.fromhex('a104147600')
    jukebox = [struct.unpack('<I', read(0x00537210 + 8 * k, 4))[0] for k in range(58)]
    assert jukebox[0] == 0xFFFFFFFF                                           # row 0: the level's music
