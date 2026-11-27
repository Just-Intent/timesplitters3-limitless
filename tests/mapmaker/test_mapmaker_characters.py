"""MapMaker custom character set: every Arcade character in the editor's picker.

Compiles src/interface/pc_mapmaker_characters.inc against fake guest memory
(with mutations that must be caught) and, when the private XBE is available,
checks the stock facts the hook relies on: the picker builder's call site and
scene test, the eligibility routine's shape, the preload call sites, the real
Arcade table rows (every row has a real unlock rule; the big characters are
there) and the custom-set save/load paths that carry any id without an
unlock check.
"""
import os
from pathlib import Path
import struct
import subprocess
import pytest

from native_compiler import native_compiler
from mapmaker_decoded import decoded, _private

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_characters.inc'

MUTATIONS = {
    'always-on': ('if(!mapmaker_limits.limits || WORD_AT(CHR_SCENE)!=102u', 'if(0 || WORD_AT(CHR_SCENE)!=102u'),
    'any-scene': ('WORD_AT(CHR_SCENE)!=102u || !MAPMAKER_STACK_OK(12)', '0 || !MAPMAKER_STACK_OK(12)'),
    'any-caller': ('(ret=WORD_AT(g_esp))!=CHR_PICKER_RET) {', '((ret=WORD_AT(g_esp)),0)) {'),
    'wrong-ret': ('#define CHR_PICKER_RET 0x002C1F35u', '#define CHR_PICKER_RET 0x002C1F30u'),
    'any-row': ('if(g_eax==0u && chr_offerable(id,variant)) {', 'if(g_eax==0u) {'),
    'rule-zero': ('return rule>=1u && rule<=CHR_RULE_MAX;', 'return rule<=CHR_RULE_MAX;'),
    'any-rule': ('return rule>=1u && rule<=CHR_RULE_MAX;', 'return rule>=1u;'),
    'ignore-variant': ('if(variant<0 || (int32_t)(int8_t)CHR_B(r+1u)==variant) return r;', 'return r;'),
    'short-walk': ('#define CHR_ROWS 150u', '#define CHR_ROWS 149u'),
    'forces-zero': ('        g_eax=1u;\n', '        g_eax=1u;\n    } else if(1) { g_eax=0u;\n'),
    'skips-original': ('    original();                                   /* pops the return address */\n',
                       '    g_esp+=4u;g_eax=0u;                           /* pops the return address */\n'),
    'original-twice': ('    original();                                   /* pops the return address */\n',
                       '    original();original();                        /* pops the return address */\n'),
    'preload-skips': ('                chr_log("set member model has no resource slot",id,(int32_t)model);\n        }\n    }\n    original();',
                      '                { chr_log("set member model has no resource slot",id,(int32_t)model);g_esp+=4u;g_eax=0x77u;return; }\n        }\n    }\n    original();'),
    'preload-silent': ('else if((slot=CHR_H(CHR_SLOTS+model*2u))<0 ||', 'else if((slot=CHR_H(CHR_SLOTS+model*2u))<-1 ||'),
    'preload-any-caller': ('((ret=WORD_AT(g_esp))==CHR_PRELOAD_CUSTOM || ret==CHR_PRELOAD_PRESET)) {', '((ret=WORD_AT(g_esp)),1)) {'),
    'preload-any-scene': ('WORD_AT(CHR_SCENE)==100u && MAPMAKER_STACK_OK(12) &&', 'MAPMAKER_STACK_OK(12) &&'),
    'preload-writes': ('            table=WORD_AT(CHR_RESOURCES);\n', '            table=WORD_AT(CHR_RESOURCES);WORD_AT(CHR_CHARS+id*0x30u)=1u;\n'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('characters.exe' if os.name == 'nt' else 'characters')
    source = ROOT / 'tests/mapmaker/mapmaker_characters.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/wd4706',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src'),
                str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), '-I' + str(ROOT / 'src'),
                str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_characters(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker characters ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_no_guest_code_table():
    """Wrappers only: no code bytes change."""
    assert not (ROOT / 'config/guest-code-patches/characters.json').exists()


# ---- stock facts (skipped without the private XBE) --------------------------

def by_address():
    return {i.address: i for i in decoded()}


def mem(i):
    return [op for op in i.ops if op[0] == 'mem']


def test_picker_builder_site():
    a = by_address()
    call = a[0x002C1F30]
    assert call.call and call.function == 0x002C1EA0 and '0x1cd980' in call.text
    assert 0x002C1F30 + len(bytes.fromhex(call.hex)) == 0x002C1F35       # CHR_PICKER_RET
    # the row walk: variant +1 (movsx byte) and id +4 of 0x00521A18, 0x30 stride, 0x1C20 end
    assert mem(a[0x002C1F21])[0][4] == 0x00521A19 and mem(a[0x002C1F21])[0][5] == 1
    assert mem(a[0x002C1F28])[0][4] == 0x00521A1C
    assert ('imm', 0x30) in a[0x002C1F94].ops and ('imm', 0x1C20) in a[0x002C1F98].ops
    # the only filter before the add: the eligibility result
    assert a[0x002C1F38].mnemonic == 'test' and a[0x002C1F3A].mnemonic == 'je'
    assert '0x2c1f94' in a[0x002C1F3A].text
    # item callback is the pick routine; the editor scene only changes flags
    assert ('imm', 0x002C1C90) in a[0x002C1F3E].ops
    assert ('imm', 0x66) in a[0x002C1EAC].ops and a[0x002C1EAF].mnemonic == 'sete'
    # sub_001CD980 is called from the picker only once in the builder
    calls = [i for i in decoded() if i.call and '0x1cd980' in i.text and i.function == 0x002C1EA0]
    assert [i.address for i in calls] == [0x002C1F30]


def test_picker_reached_from_the_editor():
    a = by_address()
    # sub_003128F0 (editor Map Settings) opens 0x0052375C / 0x0052371C
    assert a[0x0031293D].function == 0x003128F0 and ('imm', 0x0052375C) in a[0x0031293D].ops
    # sub_002C1C90 (pick) stores the row's id/variant; no eligibility call in it
    body = [i for i in decoded() if i.function == 0x002C1C90]
    assert body and not any(i.call and '0x1cd980' in i.text for i in body)
    assert any(mem(i) and mem(i)[0][4] == 0x00521A1C for i in body)


def test_eligibility_shape():
    a = by_address()
    # sub_001CD980: row lookup, rule type != 0, cheat byte 4, then the rule check
    assert '0x2be5e0' in a[0x001CD98B].text
    assert a[0x001CD999].mnemonic == 'cmp' and mem(a[0x001CD999])[0][1] == 'esi'
    assert ('imm', 4) in a[0x001CD99E].ops and '0x1cceb0' in a[0x001CD9A0].text
    assert '0x1cd3e0' in a[0x001CD9B6].text
    assert a[0x001CD411].mnemonic == 'cmp' and ('imm', 8) in a[0x001CD411].ops   # types 0..8


def test_preload_sites():
    a = by_address()
    for site, fn, ret in ((0x0023F33A, 0x0023F320, 0x0023F33F), (0x0023F37A, 0x0023F350, 0x0023F37F)):
        call = a[site]
        assert call.call and call.function == fn and '0xd7780' in call.text
        assert site + len(bytes.fromhex(call.hex)) == ret
    # sub_000D7780 requests the model at char table +4 (0x0053A3D0) through sub_000A5E40
    assert mem(a[0x000D77A3])[0][4] == 0x0053A3D0 and '0xa5e40' in a[0x000D77AA].text
    assert ('imm', 0x9D0) in a[0x000A5E4E].ops and mem(a[0x000A5E5A])[0][4] == 0x0078B860


def test_custom_set_save_paths_carry_any_id():
    a = by_address()
    # editor load (0x00301D5F..) and play builder sub_002C0210: plain copies
    assert mem(a[0x00301D74])[0][4] == 0x18 and mem(a[0x00301D74])[0][5] == 2      # word id
    assert mem(a[0x00301D7F])[0][4] == 0x1B and a[0x00301D7F].mnemonic == 'movsx' # sbyte variant
    assert mem(a[0x00300896])[0][4] == 0x11864 and mem(a[0x00300896])[0][5] == 2   # compile: word id
    builder = [i for i in decoded() if i.function == 0x002C0210]
    assert builder and not any(i.call and i.address != 0x002C021B for i in builder)
    # sub_001CD980 has no caller outside the front-end menus (0x002C....-0x002D....)
    callers = {i.function for i in decoded() if i.call and '0x1cd980' in i.text}
    assert callers and all(0x002C0000 <= f < 0x002E0000 for f in callers), sorted(map(hex, callers))


def xbe_reader():
    path = _private() / 'game/default.xbe'
    if not path.is_file():
        pytest.skip('needs the private XBE')
    d = path.read_bytes()
    base = struct.unpack_from('<I', d, 0x104)[0]
    n = struct.unpack_from('<I', d, 0x11C)[0]
    sh = struct.unpack_from('<I', d, 0x120)[0] - base
    secs = [struct.unpack_from('<5I', d, sh + i * 56)[1:] for i in range(n)]

    def rd(va, size):
        for sva, vs, ra, rs in secs:
            if sva <= va < sva + vs:
                off = va - sva
                b = d[ra + off:ra + min(off + size, rs)]
                return b + bytes(size - len(b))
        raise KeyError(hex(va))
    return rd


def test_real_arcade_rows():
    rd = xbe_reader()

    def cstr(va):
        return rd(va, 64).split(b'\0')[0].decode('latin1')
    names = {}
    for i in range(150):
        row = rd(0x00521A18 + i * 0x30, 0x30)
        cid, rule = struct.unpack_from('<I', row, 4)[0], struct.unpack_from('<I', row, 0x14)[0]
        assert 1 <= rule <= 8, (i, hex(cid), rule)            # all offerable, none never-unlockable
        assert cid < 0xB0, (i, hex(cid))
        model = struct.unpack('<I', rd(0x0053A3CC + cid * 0x30 + 4, 4))[0]
        assert model < 0x9D0, (i, hex(cid), hex(model))
        img = struct.unpack_from('<I', row, 0xC)[0]
        names[cstr(struct.unpack('<I', rd(0x004D2D50 + img * 0x18 + 0xC, 4))[0])] = (i, cid, rule)
    # the big characters the user asked for, with their stock unlock rules
    assert names['chrimage_ff_giantrobot'] == (90, 0xA8, 2)
    assert names['chrimage_man_trophymonster'] == (43, 0xA7, 1)
    assert names['chrimage_train_khallos'] == (31, 0x02, 1)
    assert names['chrimage_bigmonster'] == (109, 0x91, 1)
    for crow in ('chrimage_nemo_crownemo', 'chrimage_train_crowsixties', 'chrimage_man_crowyoung', 'chrimage_nf_oldcrow'):
        assert names[crow][2] == 3
    # rule jump table 0x001CD530: type 0 -> return 0, type 1 -> return 1
    table = struct.unpack('<9I', rd(0x001CD530, 36))
    assert table[0] == 0x001CD528 and table[1] == 0x001CD4DC
    # stock presets already hold the giant robot (7) and trophy monster (8)
    preset = lambda n: {struct.unpack('<H', rd(0x0053E41C + n * 0xA4 + 4 + 8 * k, 2))[0] for k in range(10)}
    assert 0xA8 in preset(7) and 0xA7 in preset(8)
