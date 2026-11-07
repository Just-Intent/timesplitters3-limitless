"""MapMaker sky tile dev switch (src/interface/pc_mapmaker_skymask.inc): fixture with
mutations, plus the decoder evidence that the hooked store is the only writer
of the per-room mask table."""
import os
from pathlib import Path
import subprocess

import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
SKYMASK = ROOT / 'src/interface/pc_mapmaker_skymask.inc'

M = '[mapmaker] room mask: '
EXPECTED = '\n'.join([
    'phase off',
    M + 'dev switch TSFP_MAPMAKER_ROOM_MASK_CLEAR=0x02000000 (MapMaker play only)',
    'phase built',
    M + 'cleared 0x02000000 on 2 rooms (build 1: 3 rooms in 2 pieces)',
    M + 'room 1 0x020001C7 -> 0x000001C7',
    M + 'room 2 0x020001C7 -> 0x000001C7',
    'phase flushed',
    M + 'cleared 0x02000000 on 2 rooms (build 2: 2 rooms in 1 pieces)',
    M + 'room 1 0x020001C7 -> 0x000001C7',
    M + 'room 2 0x020001C7 -> 0x000001C7',
    M + 'cleared 0x02000000 on 1 rooms (build 3: 1 rooms in 1 pieces)',
    M + 'room 1 0x0A000000 -> 0x08000000',
    'phase rebuild',
    M + 'dev switch TSFP_MAPMAKER_ROOM_MASK_CLEAR=0x09000000 (MapMaker play only)',
    M + 'cleared 0x09000000 on 2 rooms (build 1: 2 rooms in 1 pieces)',
    M + 'room 1023 0xFFFFFFFF -> 0xF6FFFFFF',
    M + 'room 1024 0xFFFFFFFF -> 0xF6FFFFFF',
    M + '2 rooms past the 1024-entry table left unchanged',
    'phase bound',
    M + 'dev switch TSFP_MAPMAKER_ROOM_MASK_CLEAR=0x02000000 (MapMaker play only)',
    M + 'room build in scene 102 ignored (play is scene 100)',
    'phase editor',
    M + 'TSFP_MAPMAKER_ROOM_MASK_CLEAR=zz is not a 32-bit hex mask; ignored',
    M + 'dev switch TSFP_MAPMAKER_ROOM_MASK_CLEAR=0x02000000 (MapMaker play only)',
    'phase bad',
    'mapmaker skymask ok',
]) + '\n'

# Each mutation breaks one contract the fixture must detect.
MUTATIONS = {
    'default-on': ('strtoul(text,&end,16):0ul;', 'strtoul(text,&end,16):0x02000000ul;'),
    'any-scene': ('if(scene==MAPMAKER_SKYMASK_SCENE_PLAY) build=', 'if(1) build='),
    'stock-table': ('#define MAPMAKER_SKYMASK_TABLE 0x008C1000u', '#define MAPMAKER_SKYMASK_TABLE 0x00703460u'),
    'no-bound': ('if(room>=MAPMAKER_SKYMASK_ROOMS) {', 'if(0) {'),
    'writes-build': ('WORD_AT(address)=old&~clear;', 'WORD_AT(address)=old&~clear;WORD_AT(build+0x44u)&=~clear;'),
    'zeroes-mask': ('WORD_AT(address)=old&~clear;', 'WORD_AT(address)=0u;'),
    'before-original': [('    original();\n    if(!build', '    if(!build'),
                        ('        mapmaker_skymask.changed++;\n    }\n}', '        mapmaker_skymask.changed++;\n    }\n    original();\n}')],
    'pops-return': ('    original();\n    if(!build', '    original();g_esp+=4u;\n    if(!build'),
    'no-summary': ('fprintf(stderr,"[mapmaker] room mask: cleared', 'if(0)fprintf(stderr,"[mapmaker] room mask: cleared'),
    'no-room-list': ('if(shown<MAPMAKER_SKYMASK_DETAILS)', 'if(0)'),
    'no-rebuild-split': ('if(mapmaker_skymask.hit[base+j]) { pc_mapmaker_sky_room_mask_flush();break; }', ';'),
    'bad-hex-accepted': ('(end==text || *end || value>0xFFFFFFFFul)', '(value>0xFFFFFFFFul)'),
    'ignored-silent': ('else if(!mapmaker_skymask.ignored++)\n', 'else if(0)\n'),
    'no-beyond-note': ('    if(mapmaker_skymask.beyond)\n', '    if(0)\n'),
    'idle-summary': ('    if(!mapmaker_skymask.pending) return;\n', ''),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        pairs = MUTATIONS[mutation]
        text = SKYMASK.read_text(encoding='utf-8')
        for old, new in (pairs if isinstance(pairs, list) else [pairs]):
            assert old in text, mutation
            text = text.replace(old, new)
        (tmp_path / SKYMASK.name).write_text(text, encoding='utf-8')
    exe = tmp_path / ('skymask.exe' if os.name == 'nt' else 'skymask')
    source = ROOT / 'tests/mapmaker/mapmaker_skymask.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/WX', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_skymask(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_')}
    result = subprocess.run([str(exe)], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=60)
    output = result.stdout.replace('\r\n', '\n')
    if mutation == 'fixed':
        assert result.returncode == 0, output
        assert output == EXPECTED, output
    else:
        assert result.returncode != 0 or output != EXPECTED, f'{mutation} was not detected'


def test_room_mask_table_has_one_writer():
    """The hook relies on sub_00085710 being the only store into the per-room
    mask table (stock 0x00703460, relocated to 0x008C1000) and on rooms.json
    relocating every reference. Decoded with the lifter's decoder."""
    import json
    from mapmaker_decoded import decoded
    insns = decoded()
    refs = [i for i in insns if any(v & 0xFFFFFFFF in range(0x00703460, 0x00703460 + 0x31C) for _, v in i.values())]
    sites = {i.address: (i.function, i.mnemonic, i.text) for i in refs}
    # 0x0070377C (+0x31C, the 45-entry table after it; 0x000859F0/0x00085A00) is not part of the range.
    assert set(sites) == {0x00083974, 0x000857A3, 0x00085DCA, 0x0008A79B}, sites
    assert sites[0x000857A3][0] == 0x00085710 and sites[0x000857A3][1] == 'mov' and sites[0x000857A3][2].startswith('dword ptr [')
    assert sites[0x00085DCA][0] == 0x00085CC0 and sites[0x00083974][0] == 0x00083970
    assert sites[0x0008A79B][0] == 0x0008A780
    rooms = json.loads((ROOT / 'config/guest-code-patches/rooms.json').read_text(encoding='utf-8'))
    entries = rooms['patches']
    reloc = next(e for e in entries if e.get('id') == 'rooms-reloc-t3-0x703460')
    assert int(reloc['new'], 16) == 0x008C1000 and int(reloc['new_size'], 16) >= 1024 * 4   # 1024 rooms; the switch covers them all
    for site in (0x00083974, 0x000857A3, 0x00085DCA, 0x0008A79B):
        assert reloc['sites'][f'0x{site:08X}'] == 'offset'


def test_wrapper_documented_and_wired():
    doc = (ROOT / 'docs/mapmaker/mapmaker-sky.md').read_text(encoding='utf-8')
    assert 'WRAP sub_00085710 -> pc_mapmaker_sky_room_mask' in doc
    assert 'void pc_mapmaker_sky_room_mask(void (*original)(void))' in SKYMASK.read_text(encoding='utf-8')
    header = (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    assert 'void pc_mapmaker_sky_room_mask(void (*original)(void));' in header
    guest = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    assert guest.index('#include "pc_mapmaker_limits.inc"') < guest.index('#include "pc_mapmaker_skymask.inc"')
    tick = guest.split('void pc_display_menu_mouse_mode(void)\n', 1)[1].replace('\r', '').split('\n}', 1)[0]
    assert 'pc_mapmaker_sky_room_mask_flush();' in tick
    assert 'void pc_mapmaker_sky_room_mask(void (*original)(void)) { original(); }' in guest
