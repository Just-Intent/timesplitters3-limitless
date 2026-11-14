"""Compile the MapMaker map validator (src/interface/pc_mapmaker_validate.inc)
against fake guest memory with mutation cases, check its wiring, and check its
bounds against the supported US executable with the project decoder."""
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import sys

import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'src/interface/pc_mapmaker_validate.inc'
FIXTURE = ROOT / 'tests/storage/mapmaker_validate.c'
PATCH = ROOT / 'patches/storage/2765-mapmaker-map-validation.patch'
XBE = ROOT / 'private/game/default.xbe'
XBE_SHA256 = '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'

MUTATIONS = {
    'tiles-46': ('#define MAPMAKER_VALIDATE_TILES 45u', '#define MAPMAKER_VALIDATE_TILES 46u'),
    'tiles-44': ('#define MAPMAKER_VALIDATE_TILES 45u', '#define MAPMAKER_VALIDATE_TILES 44u'),
    'items-81': ('#define MAPMAKER_VALIDATE_ITEMS 80u', '#define MAPMAKER_VALIDATE_ITEMS 81u'),
    'items-negative': ('if(item<0 || item>=(int32_t)MAPMAKER_VALIDATE_ITEMS)', 'if(item>=(int32_t)MAPMAKER_VALIDATE_ITEMS)'),
    'tileset-8': ('#define MAPMAKER_VALIDATE_TILESETS 7u', '#define MAPMAKER_VALIDATE_TILESETS 8u'),
    'words-18': ('#define MAPMAKER_VALIDATE_HEADER_WORDS 17u', '#define MAPMAKER_VALIDATE_HEADER_WORDS 18u'),
    'words-15': ('#define MAPMAKER_VALIDATE_HEADER_WORDS 17u', '#define MAPMAKER_VALIDATE_HEADER_WORDS 15u'),
    'level-pieces-501': ('#define MAPMAKER_VALIDATE_LEVEL_PIECES 500u', '#define MAPMAKER_VALIDATE_LEVEL_PIECES 501u'),
    'level-pieces-499': ('#define MAPMAKER_VALIDATE_LEVEL_PIECES 500u', '#define MAPMAKER_VALIDATE_LEVEL_PIECES 499u'),
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
    'refusal-keeps-magic': ('    WORD_AT(container)=0;                          /* sub_00301A90', '    (void)0;                          /* sub_00301A90'),
    'refusal-no-loader': ('    WORD_AT(container)=0;                          /* sub_00301A90 @0x00301BAC: magic mismatch */\n    original();',
                          '    WORD_AT(container)=0;                          /* sub_00301A90 @0x00301BAC: magic mismatch */'),
    'editor-loader-twice': ('if(container && !mapmaker_validate_editor_load(container,original)) return;',
                            'if(container) (void)mapmaker_validate_editor_load(container,original);'),
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
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), str(FIXTURE), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), str(FIXTURE), '-o', str(exe)]
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


def test_hooks_are_wired():
    """Both original functions route through the shared menu translation unit."""
    patch = PATCH.read_text(encoding='utf-8')
    for address, hook in (('00301A90', 'pc_mapmaker_validate_editor'), ('0006D7A0', 'pc_mapmaker_validate_level')):
        assert f'+void sub_{address}(void)' in patch and f'+    {hook}(sub_{address}_gen);' in patch
    header = (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    assert 'void pc_mapmaker_validate_editor(void (*original)(void));' in header
    assert 'void pc_mapmaker_validate_level(void (*original)(void));' in header
    menu = (ROOT / 'src/interface/pc_display_menu.c').read_text(encoding='utf-8')
    assert menu.index('static uint32_t guest_call(') < menu.index('#include "pc_mapmaker_validate.inc"')


# ---- the bounds against the supported executable -------------------------
def _image():
    if not XBE.is_file():
        pytest.skip('needs the private US executable at private/game/default.xbe')
    data = XBE.read_bytes()
    if hashlib.sha256(data).hexdigest() != XBE_SHA256:
        pytest.skip('private/game/default.xbe is not the supported US executable')
    sys.path.insert(0, str(ROOT / 'tools/compiler'))
    import translation_layout as layout
    from translation_decode import Disassembler
    layout.configure_from_xbe(str(XBE))
    decoder = Disassembler()

    def insn_at(address):
        offset = layout.va_to_file_offset(address)
        (first, *_) = decoder.disassemble_function(data[offset:offset + 16], address, address + 16)
        return first.mnemonic, first.op_str

    def dword(address):
        return struct.unpack_from('<I', data, layout.va_to_file_offset(address))[0]
    return insn_at, dword


def test_tile_table_has_45_types():
    _, dword = _image()
    rows = [dword(0x00529400 + t * 0x140 + 0x110) for t in range(46)]
    assert rows[:45] == [t + 1 for t in range(45)]      # tile number = type + 1
    assert rows[45] != 46                                # the next row is other data


def test_bounds_match_the_code():
    insn_at, _ = _image()
    expected = {
        0x0024069B: ('cmp', 'eax, 0x4f'),                # item ids: jump table bound (count pass)
        0x002411CF: ('cmp', 'eax, 0x4f'),                # fill pass
        0x00301BCA: ('push', '0x1000'),                  # editor piece buffer: 1024 pointers
        0x00302228: ('mov', 'dword ptr [edx + eax*4], edi'),
        0x00241118: ('mov', 'word ptr [esp + ecx*8 + 0xa0], dx'),   # level piece array ...
        0x00240C50: ('mov', 'eax, 0x1030'),                         # ... in a 0x1030 frame (_chkstk)
        0x00242427: ('add', 'esp, 0x1030'),
        0x00302450: ('movzx', 'edx, byte ptr [edi + 2]'),           # patrol points
        0x0030288D: ('mov', 'eax, 0x4b00'),              # chunk 0x73 buffer
        0x003028E0: ('mov', 'ecx, 0x1f4'),               # chunk 0x74: 500 dwords
        0x00302925: ('mov', 'ecx, 0x2ee'),               # chunk 0x75: 750 dwords
        0x00301FB5: ('mov', 'byte ptr [ebx + 0x241], 0'),  # editor forces the title NUL
        0x00301BAC: ('cmp', 'dword ptr [eax], 0xc77667'),  # the refusal path the validator uses
        0x003029C7: ('push', '0'),
    }
    for address, want in expected.items():
        assert insn_at(address) == want, hex(address)
