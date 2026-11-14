"""Compile the MapMaker Story HUD turret icons (src/interface/pc_mapmaker_turret_icons_guest.inc)
against fake guest memory with mutation cases, and check the hooked sites
against the supported US executable with the project decoder."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys

import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'src/interface/pc_mapmaker_turret_icons_guest.inc'
FIXTURE = ROOT / 'tests/graphics/mapmaker_turret_icons.c'
PATCH = ROOT / 'patches/graphics/interface/2295-mapmaker-story-turret-icons.patch'
XBE = ROOT / 'private/game/default.xbe'
XBE_SHA256 = '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'

MUTATIONS = {
    'any-scene': ('PC_TURRET_ICONS_U32(PC_TURRET_ICONS_SCENE)!=100u ||', ''),
    'any-mode': ('       PC_TURRET_ICONS_B(PC_TURRET_ICONS_MODE)!=10u || !pc_turret_icons_enabled()',
                 '       !pc_turret_icons_enabled()'),
    'no-switch': (' || !pc_turret_icons_enabled()) { original();return; }', ') { original();return; }'),
    'switch-inverted': ("pc_turret_icons_switch=!(value && value[0]=='0' && !value[1]);",
                        "pc_turret_icons_switch=(value && value[0]=='0' && !value[1]);"),
    'no-restore': ('    PC_TURRET_ICONS_U32(PC_TURRET_ICONS_COUNT)=live+(after-slots); /* the call changed it by after-slots */\n', ''),
    'no-delta': ('PC_TURRET_ICONS_U32(PC_TURRET_ICONS_COUNT)=live+(after-slots);',
                 'PC_TURRET_ICONS_U32(PC_TURRET_ICONS_COUNT)=live+0*after;'),
    'no-free-filter': ('       pc_turret_icons_state.free_slot[k]) {', '       0) {'),
    'any-caller': (' ||\n       PC_TURRET_ICONS_U32(g_esp)!=PC_TURRET_ICONS_CALL) { original();return; }', ') { original();return; }'),
    'no-skip-ret': ('        pc_turret_icons_state.skipped++;\n        PC_TURRET_ICONS_RET();',
                    '        pc_turret_icons_state.skipped++;'),
    'no-pool-bound': (' || slots>PC_TURRET_ICONS_MAX', ''),
    'fixed-pool': ('slots=PC_TURRET_ICONS_U32(PC_TURRET_ICONS_FREE+0xCu);', 'slots=80u;'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = SOURCE.read_text(encoding='utf-8')
        assert old in text, mutation
        (tmp_path / SOURCE.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('icons.exe' if os.name == 'nt' else 'icons')
    strict = mutation == 'fixed'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/D_CRT_SECURE_NO_WARNINGS', *(['/WX'] if strict else []),
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), str(FIXTURE), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L', *(['-Werror'] if strict else []),
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), str(FIXTURE), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_turret_icons(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if k != 'TSFP_MAPMAKER_TURRET_ICONS'}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker turret icons ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_hooks_are_wired():
    text = PATCH.read_text(encoding='utf-8')
    assert '+#include "pc_mapmaker_turret_icons_guest.inc"' in text
    assert '+void sub_00349E40(void)\n+{ pc_turret_icons_hud(sub_00349E40_gen); }' in text
    assert '+void sub_00349970(void)\n+{ pc_turret_icons_turret(sub_00349970_gen); }' in text


# ---- against the supported executable -------------------------------------
def _insn_at():
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
    return insn_at


def test_hooked_sites_are_as_documented():
    insn_at = _insn_at()
    expected = {
        0x0034A75E: ('mov', 'eax, dword ptr [0x732d44]'),   # the HUD loop's live-count reads
        0x0034A796: ('mov', 'eax, dword ptr [0x732d44]'),
        0x0034A78E: ('call', '0x349970'),                   # icon call; returns to 0x0034A793
        0x0034A79C: ('add', 'edi, 0x1dc'),                  # next slot
        0x00349E17: ('ret', ''),                            # sub_00349970 ends in a plain ret
        0x000B3A90: ('push', '0x20'),                       # stock gun pool: 32 nodes
    }
    for address, want in expected.items():
        assert insn_at(address) == want, hex(address)
    assert 0x0034A78E + 5 == 0x0034A793
