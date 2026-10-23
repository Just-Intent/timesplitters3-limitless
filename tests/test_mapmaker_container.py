"""Compile the MapMaker map container hooks against fake guest memory, and
check the container patch table against the hooks' sizes."""
import json
import os
import re
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]
INC = ROOT / 'src/pc_mapmaker_container.inc'
TABLE = ROOT / 'config/guest-code-patches/container.json'

MUTATIONS = {
    'load-stock-only': ('    for(i=0;i<MAPMAKER_CONTAINER_TIER_COUNT;i++) {\n        /* System Link',
                        '    for(i=0;i<1;i++) {\n        /* System Link'),
    'load-buffer-not-cleared': ('    mapmaker_container_zero(buf,MAPMAKER_CONTAINER_N);\n    r=',
                                '    r='),
    'load-system-link-large': ('        if(i && guest_call(sub_0035E960,0)) return 7;\n', ''),
    'load-word-32-bit': ('MMC_HALF(entry+0x66)=(uint16_t)(WORD_AT(info+0x248)+4u);',
                         'WORD_AT(entry+0x66)=WORD_AT(info+0x248)+4u;'),
    'save-crc-not-counted': ('if(used+4u<=mapmaker_container_tiers[i])', 'if(used<=mapmaker_container_tiers[i])'),
    'save-always-largest': ('    unsigned i;\n    if(used && used<=MAPMAKER_CONTAINER_B-4u)',
                            '    unsigned i;\n    if(used) return MAPMAKER_CONTAINER_B;\n    if(used && used<=MAPMAKER_CONTAINER_B-4u)'),
    'save-any-caller': ('    if(WORD_AT(g_esp)!=ret) return;\n', ''),
    'crc-store-clobbers': ('            g_eax=WORD_AT(buf+len);\n', ''),
    'crc-16-bit': ('            WORD_AT(g_esp+8)=used;\n            original();\n            WORD_AT(g_esp+8)=len;',
                   '            original();'),
    'check-16-bit': ('        if(used>0xFFFFu-4u) {', '        if(0) {'),
    'check-unbounded': ('if(used>MAPMAKER_CONTAINER_B-4u || !guest_writable(buf,used+4u)) { g_eax=0; return; }', ''),
    'compile-zero-low-half': ('    if(r==ret && g_eax && !(g_eax&0xFFFFu)) g_eax|=4u;', '    (void)r;(void)ret;'),
    'find-not-gated': (' ||\n       !mapmaker_container.buffer || !mapmaker_container_raised()) return;',
                       ' ||\n       !mapmaker_container.buffer) return;'),
    'find-unbiased': ('    g_eax=scratch-0x248u;\n', ''),
    'find-off-by-limit': ('-(MAPMAKER_CONTAINER_L-MAPMAKER_CONTAINER_STOCK);', '-(MAPMAKER_CONTAINER_B-MAPMAKER_CONTAINER_STOCK);'),
    'meter-not-gated': ('    if(mapmaker_container_raised() && WORD_AT(0x0049F7ACu)==0x38002008u)',
                        '    if(WORD_AT(0x0049F7ACu)==0x38002008u)'),
    'send-large-map': ('            g_eax=0;\n            return;\n        }\n    }\n    original();\n}',
                       '        }\n    }\n    original();\n}'),
    'ring-stock-ratio': ('#define MAPMAKER_CONTAINER_RING (2u*MAPMAKER_CONTAINER_N)',
                         '#define MAPMAKER_CONTAINER_RING (MAPMAKER_CONTAINER_N)'),
    'buffer-reallocated': ('    if(!mapmaker_container.buffer)\n        mapmaker_container.buffer=',
                           '    mapmaker_container.buffer='),
    'ring-not-cleared': ('    mapmaker_container_zero(mapmaker_container.ring,MAPMAKER_CONTAINER_RING);\n', ''),
    'editor-init-skips-buffer': ('    if(!cur) {\n        mapmaker_container_install();', '    if(0) {\n        mapmaker_container_install();'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('container.exe' if os.name == 'nt' else 'container')
    source = ROOT / 'tests/mapmaker_container.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_container(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_MEMORY_SCALE')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker container ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def _defines():
    text = INC.read_text(encoding='utf-8')
    n = int(re.search(r'#define MAPMAKER_CONTAINER_N (0x[0-9A-F]+)u', text).group(1), 16)
    b = int(re.search(r'#define MAPMAKER_CONTAINER_B (0x[0-9A-F]+)u', text).group(1), 16)
    return n, b


def test_table_matches_the_hooks():
    n, b = _defines()
    assert b == n - 0x20                    # stock slack (signature) kept
    patches = json.loads(TABLE.read_text(encoding='utf-8'))['patches']
    assert set(json.loads(TABLE.read_text(encoding='utf-8'))) == {'patches'}
    sizes = {'walk': b, 'body': b // 4}
    seen = {}
    for p in patches:
        assert p['kind'] == 'code' and p['id'].startswith('mm-container-') and p['reason']
        new = bytes.fromhex(p['new_hex'])
        value = int.from_bytes(new[-4:], 'little')
        seen[int(p['address'], 16)] = value
        if p['id'].startswith('mm-container-walk-'):
            assert value == sizes['walk']
        elif p['id'].startswith('mm-container-body-'):
            assert value == sizes['body']
    assert len(seen) == len(patches) == 33
    expected = {
        0x0033772D: n, 0x00336656: n // 4, 0x0033664F: 2 * n // 4, 0x003366C0: n,
        0x003366D4: n // 4, 0x003366A3: 4 * b, 0x0033741F: b, 0x00337438: b // 2,
        0x00337352: 2 * n, 0x001907A0: n}
    for address, value in expected.items():
        assert seen[address] == value, hex(address)
    # Sites that stay stock: placement checks (gated by the find hook), the
    # save/load sizes (tiers chosen by the hooks), the free-space estimates
    # and System Link (one stock message).
    for address in (0x00300696, 0x003006AB, 0x0030366D, 0x00303936, 0x00304442,
                    0x00190567, 0x001905C9, 0x001909CA, 0x00190A12, 0x00190A39,
                    0x002FE203, 0x002FE23C, 0x00336603, 0x0032A3EC, 0x0032B5BB,
                    0x00191F74, 0x0035644C, 0x003563AB, 0x0035504E, 0x003526FA):
        assert address not in seen, hex(address)


def test_documented_wrappers_exist():
    doc = (ROOT / 'docs/mapmaker-container.md').read_text(encoding='utf-8')
    text = INC.read_text(encoding='utf-8')
    wraps = re.findall(r'WRAP (sub_[0-9A-F]{8}) -> (pc_mapmaker_container_\w+)', doc)
    assert len(wraps) == 13
    for _, hook in wraps:
        assert f'void {hook}(void (*original)(void))' in text
