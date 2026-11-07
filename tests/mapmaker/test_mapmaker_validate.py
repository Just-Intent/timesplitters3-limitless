"""Compile the MapMaker map validator (src/interface/pc_mapmaker_validate.inc) against
fake guest memory with mutation cases, and check its bounds against the real
code (decoded with the lifter's decoder) and data (the private XBE)."""
import os
from pathlib import Path
import struct
import subprocess
import sys
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'src/interface/pc_mapmaker_validate.inc'

MUTATIONS = {
    'tiles-46': ('#define MAPMAKER_VALIDATE_TILES 45u', '#define MAPMAKER_VALIDATE_TILES 46u'),
    'tiles-44': ('#define MAPMAKER_VALIDATE_TILES 45u', '#define MAPMAKER_VALIDATE_TILES 44u'),
    'items-81': ('#define MAPMAKER_VALIDATE_ITEMS 80u', '#define MAPMAKER_VALIDATE_ITEMS 81u'),
    'items-negative': ('if(item<0 || item>=(int32_t)MAPMAKER_VALIDATE_ITEMS)', 'if(item>=(int32_t)MAPMAKER_VALIDATE_ITEMS)'),
    'tileset-8': ('#define MAPMAKER_VALIDATE_TILESETS 7u', '#define MAPMAKER_VALIDATE_TILESETS 8u'),
    'variant-any-base': ('#define MAPMAKER_VALIDATE_VARIANT_BASE 5u', '#define MAPMAKER_VALIDATE_VARIANT_BASE 3u'),
    'variant-not-cleared': ('            WORD_AT(d+MAPMAKER_VALIDATE_FLAGS)&=~MAPMAKER_VALIDATE_VARIANT;\n', ''),
    'variant-other-bits': ('WORD_AT(d+MAPMAKER_VALIDATE_FLAGS)&=~MAPMAKER_VALIDATE_VARIANT;', 'WORD_AT(d+MAPMAKER_VALIDATE_FLAGS)=0;'),
    'variant-cleared-in-play': ('        if(editor && sz>=MAPMAKER_VALIDATE_FLAGS+4u', '        if(sz>=MAPMAKER_VALIDATE_FLAGS+4u'),
    'words-18': ('#define MAPMAKER_VALIDATE_HEADER_WORDS 17u', '#define MAPMAKER_VALIDATE_HEADER_WORDS 18u'),
    'words-15': ('#define MAPMAKER_VALIDATE_HEADER_WORDS 17u', '#define MAPMAKER_VALIDATE_HEADER_WORDS 15u'),
    'editor-pieces-1025': ('#define MAPMAKER_VALIDATE_EDITOR_PIECES 1024u', '#define MAPMAKER_VALIDATE_EDITOR_PIECES 1025u'),
    'level-pieces-512': ('#define MAPMAKER_VALIDATE_LEVEL_PIECES 1024u', '#define MAPMAKER_VALIDATE_LEVEL_PIECES 512u'),
    'level-pieces-stock-frame': ('#define MAPMAKER_VALIDATE_LEVEL_PIECES 1024u', '#define MAPMAKER_VALIDATE_LEVEL_PIECES 500u'),
    'level-pieces-1025': ('#define MAPMAKER_VALIDATE_LEVEL_PIECES 1024u', '#define MAPMAKER_VALIDATE_LEVEL_PIECES 1025u'),
    'no-pool-total': ('if(v->mode==MAPMAKER_VALIDATE_EDITOR && v->total>MAPMAKER_VALIDATE_EDITOR_PIECES)', 'if(0)'),
    'pieces-no-fit': ('if(p>end || end-p<0x28u)', 'if(p>end)'),
    'pads-ignored': ('p+=0x28u+pads*8u;', 'p+=0x28u;'),
    'items-no-fit': ('if(size>end-p) { v->value=(uint32_t)i;return "items past the piece chunk"; }', ''),
    'patrol-9': ('#define MAPMAKER_VALIDATE_PATROL 8u', '#define MAPMAKER_VALIDATE_PATROL 9u'),
    'patrol-7': ('#define MAPMAKER_VALIDATE_PATROL 8u', '#define MAPMAKER_VALIDATE_PATROL 7u'),
    'name-unchecked': ('if(editor && !mapmaker_validate_nul(d,MAPMAKER_VALIDATE_SAI_NAME))', 'if(0)'),
    'name-short': ('#define MAPMAKER_VALIDATE_SAI_NAME 0x24u', '#define MAPMAKER_VALIDATE_SAI_NAME 0x20u'),
    'links3-14': ('#define MAPMAKER_VALIDATE_LINKS3 13u', '#define MAPMAKER_VALIDATE_LINKS3 14u'),
    'linksA-13': ('#define MAPMAKER_VALIDATE_LINKSA 10u', '#define MAPMAKER_VALIDATE_LINKSA 13u'),
    'idx-unchecked': ('if(idx<lo || idx>hi)', 'if(0)'),
    'idx-by-pieces': ('hi=(int32_t)MAPMAKER_VALIDATE_BUFFER-1; }', 'hi=5; }'),
    'idxA-zero-based': ('lo=1;hi=(int32_t)MAPMAKER_VALIDATE_BUFFER; }', 'lo=0;hi=(int32_t)MAPMAKER_VALIDATE_BUFFER-1; }'),
    'tag73': ('#define MAPMAKER_VALIDATE_73 0x4B00u', '#define MAPMAKER_VALIDATE_73 0x4B01u'),
    'tag74': ('#define MAPMAKER_VALIDATE_74 2000u', '#define MAPMAKER_VALIDATE_74 2001u'),
    'tag75': ('#define MAPMAKER_VALIDATE_75 3000u', '#define MAPMAKER_VALIDATE_75 2999u'),
    'tag74-at-level': ('case 0x74: if(editor && sz>', 'case 0x74: if(sz>'),
    'walk-sizes-trusted': ('if(!mapmaker_validate_in(v,c,8u) || !mapmaker_validate_in(v,c+8u,WORD_AT(c+4u))) {',
                           'if(!mapmaker_validate_in(v,c,8u)) {'),
    # (the step bound is unreachable: 512 KB hold at most 0x10000 empty chunks; a walk without an end tag leaves the container)
    'refusal-keeps-magic': ('    WORD_AT(container)=0;                          /* sub_00301A90', '    (void)0;                          /* sub_00301A90'),
    'refusal-no-loader': ('    WORD_AT(container)=0;                          /* sub_00301A90 @0x00301BAC: magic mismatch */\n    original();',
                          '    WORD_AT(container)=0;                          /* sub_00301A90 @0x00301BAC: magic mismatch */'),
    'level-no-restore': ('    while(n--) if(WORD_AT(edits[n].address)==edits[n].value) WORD_AT(edits[n].address)=edits[n].old;\n', ''),
    'level-pieces-kept': ('            if(tag==MAPMAKER_VALIDATE_TAG(0x68)) v->hide_pieces=1;\n', ''),
    'level-no-cut': ('                MAPMAKER_VALIDATE_EDIT(c,MAPMAKER_VALIDATE_END);\n', ''),
    'level-any-scene': ("WORD_AT(0x0079094Cu)!=100u ||", ''),
    'level-every-pass': ('WORD_AT(0x006F32ECu) ||', ''),
    'title-unchecked': ('if(!editor && !mapmaker_validate_nul(d+0x162u,MAPMAKER_VALIDATE_TITLE))', 'if(0)'),
    'title-at-editor': ('if(!editor && !mapmaker_validate_nul(d+0x162u', 'if(!mapmaker_validate_nul(d+0x162u'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = SOURCE.read_text(encoding='utf-8')
        assert old in text, mutation
        (tmp_path / SOURCE.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('validate.exe' if os.name == 'nt' else 'validate')
    source = ROOT / 'tests/mapmaker/mapmaker_validate.c'
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
def test_mapmaker_validate(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=120)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker validate ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_editor_load_calls_the_validator_first():
    """The editor area's loader hook refuses before hiding chunks; the include chain declares it."""
    editor = (ROOT / 'src/interface/pc_mapmaker_editor.inc').read_text(encoding='utf-8')
    body = editor[editor.index('void pc_mapmaker_editor_load('):]
    body = body[:body.index('\n}\n')]
    assert body.index('pc_mapmaker_validate_editor_load(container,original)') < body.index('mapmaker_hide_run(')
    guest = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    assert '#include "pc_mapmaker_validate.inc"' in guest


# ---- the bounds against the game ------------------------------------------
def _xbe():
    from mapmaker_decoded import _private
    xbe = _private() / 'game/default.xbe'
    recomp = _private().parent / 'external/ts3FPrecomp'
    if not xbe.is_file() or not (recomp / 'tools/recomp/config.py').is_file():
        pytest.skip('needs the private XBE and external/ts3FPrecomp')
    sys.path.insert(0, str(recomp))
    from tools.recomp import config
    config.configure_from_xbe(str(xbe))
    data = xbe.read_bytes()
    return lambda va: struct.unpack_from('<I', data, config.va_to_file_offset(va))[0]


def test_tile_table_has_45_types():
    dw = _xbe()
    rows = [dw(0x00529400 + t * 0x140 + 0x110) for t in range(46)]
    assert rows[:45] == [t + 1 for t in range(45)]      # tile number = type + 1
    assert rows[45] != 46                                # the next row is other data


def test_bounds_match_the_code():
    from mapmaker_decoded import decoded
    by = {i.address: i for i in decoded()}

    def insn(a, mnemonic, text):
        assert by[a].mnemonic == mnemonic and by[a].text == text, (hex(a), by[a].mnemonic, by[a].text)

    insn(0x0024069B, 'cmp', 'eax, 0x4f')                 # item ids: jump table bound (count pass)
    insn(0x002411CF, 'cmp', 'eax, 0x4f')                 # fill pass
    insn(0x00301BCA, 'push', '0x1000')                   # editor piece buffer: 1024 pointers
    insn(0x00302228, 'mov', 'dword ptr [edx + eax*4], edi')
    insn(0x00241118, 'mov', 'word ptr [esp + ecx*8 + 0xa0], dx')   # level piece array ...
    insn(0x00242427, 'add', 'esp, 0x1030')                          # ... in a 0x1030 frame
    insn(0x00302450, 'movzx', 'edx, byte ptr [edi + 2]')            # patrol points
    insn(0x0030288D, 'mov', 'eax, 0x4b00')               # chunk 0x73 buffer
    insn(0x003028E0, 'mov', 'ecx, 0x1f4')                # chunk 0x74: 500 dwords
    insn(0x00302925, 'mov', 'ecx, 0x2ee')                # chunk 0x75: 750 dwords
    insn(0x00301FB5, 'mov', 'byte ptr [ebx + 0x241], 0') # editor forces the title NUL
    insn(0x00301BAC, 'cmp', 'dword ptr [eax], 0xc77667') # the refusal path the validator uses
    insn(0x003029C7, 'push', '0')


# ---- the level fill pass frame (config/guest-code-patches/level.json) ----
LEVEL = ROOT / 'config/guest-code-patches/level.json'
LEVEL_FRAME_STOCK = 0x1030
LEVEL_ARRAY = 0x90                    # [esp+i*8+0xA0] with 4 registers pushed: frame + 0x90


def _define(path, name):
    for line in path.read_text(encoding='utf-8').splitlines():
        if line.startswith(f'#define {name} '):
            return int(line.split()[2].rstrip('u'), 0)
    raise AssertionError(name)


def test_level_frame_holds_the_piece_cap():
    import json
    patches = json.loads(LEVEL.read_text(encoding='utf-8'))['patches']
    frame = int(next(p for p in patches if p['id'] == 'level-frame')['value'], 16)
    grow = frame - LEVEL_FRAME_STOCK
    assert grow > 0 and grow % 0x10 == 0
    # the validator's level bound is what the grown frame holds (stock: 500), and covers the editor's cap
    level = _define(SOURCE, 'MAPMAKER_VALIDATE_LEVEL_PIECES')
    assert level == (frame - LEVEL_ARRAY) // 8 and (LEVEL_FRAME_STOCK - LEVEL_ARRAY) // 8 == 500
    assert _define(ROOT / 'src/interface/pc_mapmaker_limits.inc', 'MAPMAKER_LIMIT_PIECES') <= level
    for p in patches:
        assert p['kind'] == 'operand' and p['function'] == '0x00240C50' and p['operand'] == 1, p['id']
        old = int.from_bytes(bytes.fromhex(p['original_hex'])[-4:], 'little')
        assert int(p['value'], 16) == old + grow, p['id']
    assert len(patches) == 10


def test_level_frame_sites_match_the_code():
    """Every frame-size and argument displacement of sub_00240C50 is in level.json."""
    import json
    from mapmaker_decoded import decoded
    patches = {int(p['address'], 16): p for p in json.loads(LEVEL.read_text(encoding='utf-8'))['patches']}
    sites = {}
    for i in decoded():
        if i.function != 0x00240C50:
            continue
        for kind, value in i.values():
            if LEVEL_FRAME_STOCK <= value < LEVEL_FRAME_STOCK + 0x100 and (kind == 'imm' or i.frame()):
                sites[i.address] = i
    assert set(sites) == set(patches), sorted(hex(a) for a in set(sites) ^ set(patches))
    for a, i in sites.items():
        assert i.hex == patches[a]['original_hex'], hex(a)
    assert sites[0x00240C50].mnemonic == 'mov' and sites[0x00242427].mnemonic == 'add'
