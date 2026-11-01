"""MapMaker per-map cloud layer (src/interface/pc_mapmaker_sky.inc): fixture with mutations,
plus the stock facts the design relies on, read from the decoded US XBE when it
is available locally."""
import os
import struct
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
SKY = ROOT / 'src/interface/pc_mapmaker_sky.inc'
PROBE = ROOT / 'src/interface/pc_mapmaker_probe.inc'

MUTATIONS = {
    'play-any-scene': ('    if(mapmaker_sky_scene()!=100u) { original();return; }\n    mapmaker_sky_restore();',
                       '    mapmaker_sky_restore();'),
    'play-without-limits': ('info=mapmaker_sky_on()?mapmaker_sky_info(WORD_AT(0x00784014u)):0u;',
                            'info=mapmaker_sky_info(WORD_AT(0x00784014u));'),
    'no-restore': ('    mapmaker_sky_restore();\n    info=', '    info='),
    'level-sky-kept': ('    original();\n    WORD_AT(MAPMAKER_SKY_LEVEL+0x10u)=old;\n}', '    original();\n}'),
    'level-sky-not-set': ('    old=WORD_AT(MAPMAKER_SKY_LEVEL+0x10u);\n    WORD_AT(MAPMAKER_SKY_LEVEL+0x10u)=(uint32_t)sky;\n    original();\n    WORD_AT(MAPMAKER_SKY_LEVEL+0x10u)=old;\n}',
                          '    old=WORD_AT(MAPMAKER_SKY_LEVEL+0x10u);\n    original();\n    WORD_AT(MAPMAKER_SKY_LEVEL+0x10u)=old;\n}'),
    'wrong-template': ('#define MAPMAKER_SKY_TEMPLATE 5', '#define MAPMAKER_SKY_TEMPLATE 4'),
    'any-sky-patched': ('    if(sky<MAPMAKER_SKY_FIRST || sky>MAPMAKER_SKY_LAST) {', '    if(sky<0) {'),
    'tileset-default-ignored': ('    if(sky>=0) return sky;\n    if(tileset>=6u) return -1;', '    return sky;\n    if(tileset>=6u) return -1;'),
    'flag-is-stock-bit': ('#define MAPMAKER_SKY_FLAG_CLOUDS 0x00040000u', '#define MAPMAKER_SKY_FLAG_CLOUDS 0x00020000u'),
    'read-after-rebuild': ('    clouds=mapmaker_sky_clouds_now(container);\n    original();',
                           '    original();\n    clouds=mapmaker_sky_clouds_now(container);'),
    'loader-reset-ignored': ('mapmaker_sky.live=ret!=MAPMAKER_SKY_RET_LOADER_RESET;', 'mapmaker_sky.live=1;'),
    'new-map-keeps-clouds': ('mapmaker_sky.live=ret!=MAPMAKER_SKY_RET_LOADER_RESET;', 'mapmaker_sky.live=0;'),
    'editor-change-ignored': ('if(!mapmaker_sky.live || mapmaker_sky.editor!=WORD_AT(0x007844A8u))', 'if(!mapmaker_sky.live)'),
    'checkbox-any-caller': ('if(ret!=MAPMAKER_SKY_RET_SETTINGS_BIND || !mapmaker_sky_on()', 'if(!mapmaker_sky_on()'),
    'checkbox-without-limits': ('ret!=MAPMAKER_SKY_RET_SETTINGS_BIND || !mapmaker_sky_on() || ', 'ret!=MAPMAKER_SKY_RET_SETTINGS_BIND || '),
    'checkbox-not-bound': ('    guest_call(sub_00078560,1,cell);\n', ''),
    'checkbox-stale': ('    WORD_AT(cell)=(uint32_t)mapmaker_sky_clouds_now(WORD_AT(0x00784014u));\n', ''),
    'compile-any-scene': ("if(!mapmaker_sky_on() || mapmaker_sky_scene()!=102u || !MAPMAKER_PTR_OK(g_esp,8u))",
                          "if(!mapmaker_sky_on() || !MAPMAKER_PTR_OK(g_esp,8u))"),
    'compile-global-container': ('    clouds=mapmaker_sky_clouds_now(container);', '    clouds=mapmaker_sky_clouds_now(WORD_AT(0x00784014u));'),
    'extra-sky-ignored': ('    if(info && mapmaker_sky_extra_of(info)) {', '    if(0) {'),
    'extra-level-kept': ('        WORD_AT(MAPMAKER_SKY_LEVEL+0x10u)=old;\n        return;', '        return;'),
    'extra-without-pak': ('    return mapmaker_sky.extra_ready?k:0u;\n}', '    return k;\n}'),
    'extra-any-row': ('    if(!k || k>MAPMAKER_SKY_EXTRA_COUNT) return 0;', '    if(!k) return 0;'),
    'info-first-chunk-only': ('if(tag==0x00C7766Au || size>MAPMAKER_SKY_CONTAINER_MAX) return 0;',
                              'if(tag!=0x00C77668u) return 0;'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = SKY.read_text(encoding='utf-8')
        assert old in text, mutation
        (tmp_path / SKY.name).write_text(text.replace(old, new, 1), encoding='utf-8')
    exe = tmp_path / ('sky.exe' if os.name == 'nt' else 'sky')
    source = ROOT / 'tests/mapmaker/mapmaker_sky.c'
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
def test_mapmaker_sky(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_MAPMAKER')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker sky ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def _xbe():
    path = ROOT / 'private/game/default.xbe'
    if not path.exists():
        path = ROOT.parents[2] / 'private/game/default.xbe' if len(ROOT.parents) > 2 else path
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


def test_stock_sky_data():
    """The data the hooks assume, read from the US XBE (skipped without it)."""
    read = _xbe()
    # The editor's sky list: 11 x {sky index, text id}, -1 then 58..67.
    entries = struct.unpack('<22i', read(0x0052D150, 88))
    assert entries[0::2] == (-1, *range(58, 68))
    # MapMaker records 58..67 have no cloud colours; record 5 (story) has.
    for sky in range(58, 68):
        assert struct.unpack('<II', read(0x005149A8 + sky * 0x34 + 0x10, 8)) == (0, 0)
    assert struct.unpack('<5I', read(0x005149A8 + 5 * 0x34 + 0x10, 20)) == (0x5A5A6420, 0, 0x43480000, 0x42480000, 0)
    # No level table entry uses a MapMaker sky; scene 100's entry is 0x00518778.
    scenes = {}
    for i in range(200):
        entry = read(0x00515FF8 + i * 0x80, 0x20)
        if struct.unpack_from('<H', entry, 4)[0] == 0:
            break
        scene, sky = struct.unpack_from('<i', entry, 8)[0], struct.unpack_from('<i', entry, 0x10)[0]
        scenes[scene] = 0x00515FF8 + i * 0x80
        assert not 58 <= sky <= 67, hex(0x00515FF8 + i * 0x80)
    assert scenes[100] == 0x00518778
    # Tileset default skies are MapMaker skies.
    for t in range(6):
        assert 58 <= read(0x0052CF40 + t * 0x58 + 2, 1)[0] <= 67
    # The call sites the hooks recognise by return address.
    assert read(0x0031387C, 12) == bytes.fromhex('81c11c02000051e8d84cd6ff')   # &editor+0x21C; call sub_00078560
    assert read(0x00301B4A, 5) == bytes.fromhex('e8a1d0ffff')                  # loader: call sub_002FEBF0
    assert read(0x00313838, 5) == bytes.fromhex('0520020000')                  # sky row bound to editor+0x220


def test_wrappers_documented():
    doc = (ROOT / 'docs/mapmaker/mapmaker-sky.md').read_text(encoding='utf-8')
    for line in ('WRAP sub_0011C9F0 -> pc_mapmaker_sky_clouds', 'WRAP sub_00078560 -> pc_mapmaker_sky_settings',
                 'WRAP sub_002FEBF0 -> pc_mapmaker_sky_reset', 'WRAP sub_003006C0 -> pc_mapmaker_sky_compile',
                 'WRAP sub_000A2900 -> pc_mapmaker_probe_room_draw', 'WRAP sub_0008F950 -> pc_mapmaker_probe_prop_draw'):
        assert line in doc, line
    for name in ('pc_mapmaker_sky_clouds', 'pc_mapmaker_sky_settings', 'pc_mapmaker_sky_reset', 'pc_mapmaker_sky_compile'):
        assert f'void {name}(void (*original)(void))' in SKY.read_text(encoding='utf-8')
    for name in ('pc_mapmaker_probe_room_draw', 'pc_mapmaker_probe_prop_draw'):
        assert f'void {name}(void (*original)(void))' in PROBE.read_text(encoding='utf-8')
