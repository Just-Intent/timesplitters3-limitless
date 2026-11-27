"""Compile the bug fixes next to MapMaker (src/interface/pc_mapmaker_bugfix.inc) against
fake guest memory with mutation cases; check the hooked sites and the
sub_0006D0B0 bound (config/guest-code-patches/bugfix.json) with the lifter's
decoder."""
import json
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'src/interface/pc_mapmaker_bugfix.inc'
TABLE = ROOT / 'config/guest-code-patches/bugfix.json'

MUTATIONS = {
    'frag-no-len': ('len>MAPMAKER_BUGFIX_FRAG_SLOT || ', ''),
    'frag-no-index': (' || index*MAPMAKER_BUGFIX_FRAG_STRIDE+len>MAPMAKER_BUGFIX_FRAG_BUFFER', ''),
    'frag-slot-ef': ('#define MAPMAKER_BUGFIX_FRAG_SLOT 0xF0u', '#define MAPMAKER_BUGFIX_FRAG_SLOT 0xEFu'),
    'frag-stride': ('#define MAPMAKER_BUGFIX_FRAG_STRIDE 0xEFu', '#define MAPMAKER_BUGFIX_FRAG_STRIDE 0xF0u'),
    'frag-buffer-ge': ('index*MAPMAKER_BUGFIX_FRAG_STRIDE+len>MAPMAKER_BUGFIX_FRAG_BUFFER', 'index*MAPMAKER_BUGFIX_FRAG_STRIDE+len>=MAPMAKER_BUGFIX_FRAG_BUFFER'),
    'frag-no-ret': ('        g_eax=0;                                   /* as the original\'s "no free slot" exit */\n        MAPMAKER_GUEST_RET();',
                    '        g_eax=0;                                   /* as the original\'s "no free slot" exit */'),
    'turret-slot-gt': ('if(slot>=slots ||', 'if(slot>slots ||'),
    'turret-no-player': (' || (player!=0xFFu && player>=humans)', ''),
    'turret-ff-rejected': ('(player!=0xFFu && player>=humans)', '(player>=humans)'),
    'turret-first-only': ('    for(i=0;i<n;i++) {\n        uint32_t slot=', '    for(i=0;i<n && i<1u;i++) {\n        uint32_t slot='),
    'turret-no-clamp': ('return total>MAPMAKER_BUGFIX_GUN_MAX?MAPMAKER_BUGFIX_GUN_MAX:total;', 'return total;'),
    'death-always-wide': ('cap=mapmaker_bugfix_wide()?MAPMAKER_BUGFIX_EFFECTS_WIDE:MAPMAKER_BUGFIX_EFFECTS_STOCK;', 'cap=MAPMAKER_BUGFIX_EFFECTS_WIDE;'),
    'death-always-stock': ('cap=mapmaker_bugfix_wide()?MAPMAKER_BUGFIX_EFFECTS_WIDE:MAPMAKER_BUGFIX_EFFECTS_STOCK;', 'cap=MAPMAKER_BUGFIX_EFFECTS_STOCK;'),
    'death-session-ignored': ('MAPMAKER_BUGFIX_B(MAPMAKER_BUGFIX_MODE)==10u &&\n           !WORD_AT(MAPMAKER_BUGFIX_SESSION);', 'MAPMAKER_BUGFIX_B(MAPMAKER_BUGFIX_MODE)==10u;'),
    'death-no-player': ('    if(player>=WORD_AT(MAPMAKER_BUGFIX_HUMANS)) {', '    if(0) {'),
    'death-any-path': ('if((int32_t)WORD_AT(pl+4u)<0) {', 'if(1) {'),
    'goal-stock-base': ('#define MAPMAKER_BUGFIX_RUNTIME_BASE 0x7800u', '#define MAPMAKER_BUGFIX_RUNTIME_BASE 0x209Eu'),
    'goal-flags-field': ('(h-MAPMAKER_BUGFIX_RUNTIME_BASE)*32u+8u)', '(h-MAPMAKER_BUGFIX_RUNTIME_BASE)*32u+4u)'),
    'goal-zero-extend': ('g_eax=(uint32_t)(int32_t)(int16_t)(WORD_AT(', 'g_eax=(uint32_t)(WORD_AT('),
    'goal-any-mode': ('if(MAPMAKER_BUGFIX_B(MAPMAKER_BUGFIX_MODE)!=1u || !MAPMAKER_STACK_OK(8))', 'if(!MAPMAKER_STACK_OK(8))'),
    'goal-not-carrying': (' || !WORD_AT(actor+0x80u)', ''),
    'goal-20-pads': ('#define MAPMAKER_BUGFIX_RUNTIME_PADS 20u', '#define MAPMAKER_BUGFIX_RUNTIME_PADS 21u'),
    'goal-no-ret': ('    MAPMAKER_GUEST_RET();                          /* sub_002B0320: ret (0x002B036D) */', ''),
    'hud-always': ('if(!mapmaker_limits.limits || WORD_AT(MAPMAKER_BUGFIX_SCENE)!=100u ||', 'if(WORD_AT(MAPMAKER_BUGFIX_SCENE)!=100u ||'),
    'hud-story-levels': ('if(!mapmaker_limits.limits || WORD_AT(MAPMAKER_BUGFIX_SCENE)!=100u ||', 'if(!mapmaker_limits.limits ||'),
    'hud-no-restore': ('    WORD_AT(MAPMAKER_BUGFIX_GUN_COUNT)=live+(after-slots); /* the call changed it by after-slots */\n', ''),
    'hud-no-delta': ('WORD_AT(MAPMAKER_BUGFIX_GUN_COUNT)=live+(after-slots);', 'WORD_AT(MAPMAKER_BUGFIX_GUN_COUNT)=live+0*after;'),
    'hud-no-free-filter': ('       mapmaker_bugfix_hud_state.free_slot[k]) {', '       0) {'),
    'hud-any-caller': (' ||\n       WORD_AT(g_esp)!=MAPMAKER_BUGFIX_HUD_CALL) { original();return; }', ') { original();return; }'),
    # (a free-list cycle is caught by the step bound as well as the duplicate test)
    'hud-turret-no-ret': ('        mapmaker_bugfix_hud_state.skipped++;\n        MAPMAKER_GUEST_RET();', '        mapmaker_bugfix_hud_state.skipped++;'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = SOURCE.read_text(encoding='utf-8')
        assert old in text, mutation
        (tmp_path / SOURCE.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('bugfix.exe' if os.name == 'nt' else 'bugfix')
    source = ROOT / 'tests/mapmaker/mapmaker_bugfix.c'
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
def test_mapmaker_bugfix(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker bugfix ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


# ---- against the decoded game code ----------------------------------------
def _by_address():
    from mapmaker_decoded import decoded
    return {i.address: i for i in decoded()}


def _is(by, a, mnemonic, text):
    assert by[a].mnemonic == mnemonic and by[a].text == text, (hex(a), by[a].mnemonic, by[a].text)


def test_match_array_bound_keeps_the_return_address():
    """sub_0006D0B0: every store [esp+ebp*4+0x14] stays below the return address."""
    by = _by_address()
    _is(by, 0x0006D0B0, 'sub', 'esp, 0x144')
    pushes = [a for a in (0x0006D0BB, 0x0006D0BC, 0x0006D0C2, 0x0006D0C3) if by[a].mnemonic == 'push']
    assert len(pushes) == 4
    _is(by, 0x0006D115, 'cmp', 'ebp, 0x50')
    _is(by, 0x0006D118, 'mov', 'dword ptr [esp + ebp*4 + 0x14], edi')
    _is(by, 0x0006D11C, 'jge', '0x6d11f')
    ret_at = 0x144 + 4 * len(pushes)                    # [esp+0x154]
    (entry,) = json.loads(TABLE.read_text(encoding='utf-8'))['patches']
    assert entry['address'] == '0x0006D115' and by[0x0006D115].hex == entry['original_hex']
    bound = bytes.fromhex(entry['new_hex'])[2]
    assert entry['new_hex'][:4] == '83fd'
    # the store happens before the compare, so ebp == bound is stored (repeatedly)
    assert 0x14 + 4 * bound + 4 <= ret_at                # new: last slot ends at the return address
    assert 0x14 + 4 * 0x50 + 4 > ret_at                  # stock: the 81st store hit it


def test_hooked_sites_are_as_documented():
    by = _by_address()
    # 1. fragment receiver
    _is(by, 0x00352E33, 'mov', 'ax, word ptr [esi + 4]')
    _is(by, 0x00352E3E, 'mov', 'cl, byte ptr [esi + 2]')
    _is(by, 0x00352E5B, 'lea', 'edi, [ebx + 0x10]')
    _is(by, 0x00352F7A, 'imul', 'edi, edi, 0xef')
    _is(by, 0x00352EA9, 'mov', 'byte ptr [esp + ebp + 0x14], cl')
    _is(by, 0x00352FE8, 'ret', '')
    # 3. turret sync
    _is(by, 0x00359740, 'imul', 'eax, eax, 0x1dc')
    _is(by, 0x0035974B, 'mov', 'dword ptr [eax + edx + 0x10], ecx')
    _is(by, 0x00359755, 'cmp', 'ecx, 0xff')
    _is(by, 0x00359763, 'imul', 'ecx, ecx, 0x1584')
    _is(by, 0x00359794, 'ret', '')
    # 5. death effect: unbounded add
    _is(by, 0x00357D74, 'movzx', 'eax, byte ptr [ecx]')
    _is(by, 0x00357DB3, 'call', '0x2b23d0')
    _is(by, 0x00357DC5, 'ret', '')
    _is(by, 0x002B24B1, 'mov', 'ecx, dword ptr [0x75f6e8]')
    # 6. HUD loop and its icon call
    _is(by, 0x0034A75E, 'mov', 'eax, dword ptr [0x732d44]')
    _is(by, 0x0034A796, 'mov', 'eax, dword ptr [0x732d44]')
    _is(by, 0x0034A78E, 'call', '0x349970')
    assert 0x0034A793 == 0x0034A78E + 5
    _is(by, 0x0034A79C, 'add', 'edi, 0x1dc')
    _is(by, 0x00349E17, 'ret', '')
    _is(by, 0x000B3A90, 'push', '0x20')                  # stock node count (objects.json -> 0x50)
    # 7. stray read
    _is(by, 0x002B035E, 'mov', 'eax, dword ptr [0x715f38]')
    _is(by, 0x002B0363, 'movsx', 'eax, word ptr [eax - 0x413d8]')
    _is(by, 0x002B036D, 'ret', '')


def test_runtime_base_matches_pads_table():
    pads = json.loads((ROOT / 'config/guest-code-patches/pads.json').read_text(encoding='utf-8'))['patches']
    entry = next(e for e in pads if e['address'] == '0x002B0348')
    assert entry['new_hex'] == '3d00780000'              # cmp eax,0x7800
    text = SOURCE.read_text(encoding='utf-8')
    assert '#define MAPMAKER_BUGFIX_RUNTIME_BASE 0x7800u' in text
