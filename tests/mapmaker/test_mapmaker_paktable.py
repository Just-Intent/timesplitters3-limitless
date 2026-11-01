"""Pak mount table relocation (config/guest-code-patches/paktable.json): the 30-slot
table at 0x006D7900 (0x1C per entry, count [0x006D7880]) moves to 64 slots at
0x00907000 in the guest extension. Every instruction that addresses the old
table must be rewritten, both caps raised and the clear's length grown;
decode facts are skipped without the private disassembly."""
import json
from pathlib import Path
import re

from mapmaker_decoded import decoded

ROOT = Path(__file__).resolve().parents[2]
TABLE = ROOT / 'config/guest-code-patches/paktable.json'
OLD, NEW, STRIDE = 0x006D7900, 0x00907000, 0x1C
OLD_END = OLD + 30 * STRIDE


def patches():
    return json.loads(TABLE.read_text(encoding='utf-8'))['patches']


def test_new_table_is_free_extension_memory():
    """64 x 0x1C fits below the next claimed block and inside the extension."""
    header = json.loads((ROOT / 'config/guest-code-patches/00-header.json').read_text(encoding='utf-8'))['extension']
    va, size = int(header['va'], 16), int(header['size'], 16)
    assert va <= NEW and NEW + 64 * STRIDE <= va + size
    used = set()
    for path in (ROOT / 'src/interface').glob('*.inc'):
        text = '\n'.join(l for l in path.read_text(encoding='utf-8', errors='replace').splitlines() if 'paktable' not in l)
        used |= {int(v, 16) for v in re.findall(r'0x(0090[0-9A-Fa-f]{4})', text)}
    assert not [v for v in used if NEW <= v < NEW + 64 * STRIDE]


def test_every_table_reference_is_relocated():
    want = {}
    for i in decoded():
        for op in i.ops:
            v = op[4] if op[0] == 'mem' else op[1] if op[0] == 'imm' else None
            if v is not None and OLD <= v < OLD_END:
                want[i.address] = v
    assert len(want) == 23
    got = {int(p['address'], 16): int(p['value'], 16) for p in patches() if p['kind'] == 'operand'}
    assert set(got) == set(want)
    for a, v in want.items():
        assert got[a] == NEW + v - OLD


def test_caps_and_clear():
    by = {int(p['address'], 16): p for p in patches() if p['kind'] == 'code'}
    assert by[0x00062049]['new_hex'] == '83f940' and by[0x000620F9]['new_hex'] == '83f840'
    assert by[0x0006275B]['new_hex'] == 'b9c0010000' and 64 * STRIDE // 4 == 0x1C0
    a = {i.address: i for i in decoded()}
    assert ('imm', 0x1E) in a[0x00062049].ops and ('imm', 0x1E) in a[0x000620F9].ops
    assert ('imm', 0xD2) in a[0x0006275B].ops and 30 * STRIDE // 4 == 0xD2
    # no other bound on the slot count: every other reader loops to [0x006D7880]
    caps = [i.address for i in decoded() if 0x00061FD0 <= i.address < 0x00062B00
            and i.mnemonic == 'cmp' and ('imm', 0x1E) in i.ops]
    assert sorted(caps) == [0x00062049, 0x000620F9]
