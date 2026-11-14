"""Compile the System Link receive bounds (src/network/pc_net_packet_guard_guest.inc)
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
SOURCE = ROOT / 'src/network/pc_net_packet_guard_guest.inc'
FIXTURE = ROOT / 'tests/network/net_packet_guard.c'
PATCH = ROOT / 'patches/network/2475-system-link-packet-bounds.patch'
XBE = ROOT / 'private/game/default.xbe'
XBE_SHA256 = '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'

MUTATIONS = {
    'frag-no-len': ('len>PC_NET_GUARD_FRAG_SLOT || ', ''),
    'frag-no-index': (' || index*PC_NET_GUARD_FRAG_STRIDE+len>PC_NET_GUARD_FRAG_BUFFER', ''),
    'frag-slot-ef': ('#define PC_NET_GUARD_FRAG_SLOT 0xF0u', '#define PC_NET_GUARD_FRAG_SLOT 0xEFu'),
    'frag-stride': ('#define PC_NET_GUARD_FRAG_STRIDE 0xEFu', '#define PC_NET_GUARD_FRAG_STRIDE 0xF0u'),
    'frag-buffer-ge': ('index*PC_NET_GUARD_FRAG_STRIDE+len>PC_NET_GUARD_FRAG_BUFFER',
                       'index*PC_NET_GUARD_FRAG_STRIDE+len>=PC_NET_GUARD_FRAG_BUFFER'),
    'frag-no-ret': ('        g_eax=0;                                   /* as the original\'s "no free slot" exit */\n        PC_NET_GUARD_RET();',
                    '        g_eax=0;                                   /* as the original\'s "no free slot" exit */'),
    'turret-slot-gt': ('if(slot>=slots ||', 'if(slot>slots ||'),
    'turret-no-player': (' || (player!=0xFFu && player>=humans)', ''),
    'turret-ff-rejected': ('(player!=0xFFu && player>=humans)', '(player>=humans)'),
    'turret-first-only': ('    for(i=0;i<n;i++) {\n        uint32_t slot=', '    for(i=0;i<n && i<1u;i++) {\n        uint32_t slot='),
    'turret-fixed-pool': ('slots=PC_NET_GUARD_U32(PC_NET_GUARD_GUN_TOTAL);', 'slots=32u;'),
    'death-cap-31': ('#define PC_NET_GUARD_EFFECTS_MAX 30u', '#define PC_NET_GUARD_EFFECTS_MAX 31u'),
    'death-no-full': ('    if((int32_t)PC_NET_GUARD_U32(pl+4u)<0 &&', '    if(0 &&'),
    'death-no-player': ('    if(player>=PC_NET_GUARD_U32(PC_NET_GUARD_HUMANS)) {', '    if(0) {'),
    'death-any-path': ('if((int32_t)PC_NET_GUARD_U32(pl+4u)<0 &&', 'if(1 &&'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    include = tmp_path / 'network'
    include.mkdir()
    text = SOURCE.read_text(encoding='utf-8')
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        assert old in text, mutation
        text = text.replace(old, new)
    (include / SOURCE.name).write_text(text, encoding='utf-8')
    exe = tmp_path / ('guard.exe' if os.name == 'nt' else 'guard')
    strict = mutation == 'fixed'      # a mutation may leave a constant condition behind
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', *(['/WX'] if strict else []),
                '/I' + str(tmp_path), str(FIXTURE), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', *(['-Werror'] if strict else []),
                '-I' + str(tmp_path), str(FIXTURE), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_net_packet_guard(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'net packet guard ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_wrappers_route_each_handler_to_its_guard():
    text = PATCH.read_text(encoding='utf-8')
    assert '+#include "pc_net_packet_guard_guest.inc"' in text
    for address, guard in (('00352DF0', 'pc_net_guard_fragment'), ('003596F0', 'pc_net_guard_turret_sync'),
                           ('00357D70', 'pc_net_guard_death')):
        assert f'+void sub_{address}(void)\n+{{ {guard}(sub_{address}_gen); }}' in text


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
        # fragment receiver: length into the 0x100-byte slot, index into the 0x8000 buffer
        0x00352E33: ('mov', 'ax, word ptr [esi + 4]'),
        0x00352E3E: ('mov', 'cl, byte ptr [esi + 2]'),
        0x00352E5B: ('lea', 'edi, [ebx + 0x10]'),
        0x00352F7A: ('imul', 'edi, edi, 0xef'),
        0x00352FE8: ('ret', ''),
        # turret sync: unbounded slot and player indices
        0x00359740: ('imul', 'eax, eax, 0x1dc'),
        0x0035974B: ('mov', 'dword ptr [eax + edx + 0x10], ecx'),
        0x00359755: ('cmp', 'ecx, 0xff'),
        0x00359763: ('imul', 'ecx, ecx, 0x1584'),
        0x00359794: ('ret', ''),
        0x000B3A90: ('push', '0x20'),                    # stock gun pool: 32 nodes
        # death effect: unbounded add, while the local adder stops at 30
        0x00357D74: ('movzx', 'eax, byte ptr [ecx]'),
        0x00357DB3: ('call', '0x2b23d0'),
        0x00357DC5: ('ret', ''),
        0x002B24B1: ('mov', 'ecx, dword ptr [0x75f6e8]'),
        0x002B24E0: ('cmp', 'dword ptr [0x75f6e8], 0x1e'),
    }
    for address, want in expected.items():
        assert insn_at(address) == want, hex(address)
