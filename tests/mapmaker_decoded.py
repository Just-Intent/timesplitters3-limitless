"""Every instruction of the game's code, decoded with the lifter's own decoder.

The text listing (private/pipeline/disasm/asm/text.asm) elides the byte column of
long instructions (an 11-byte `mov [esp+eax*4+0x180],0xD5` is shown as
`c7848480010000d50000..`), so an inventory parsed from it silently drops them;
that once missed a room id and crashed a story level. The classification tests
(tests/test_mapmaker_*_classified.py) scan this decode instead.

decoded() -> list of Insn, cached per process; it skips the calling test when
the private XBE, its function list or external/ts3FPrecomp is missing.
"""
from dataclasses import dataclass
from functools import lru_cache
import json
from pathlib import Path
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]
GAME_CODE_END = 0x003D3380            # .text of the game; D3D, XGRPH, XONLINE, ... follow


@dataclass(frozen=True)
class Insn:
    address: int
    function: int
    mnemonic: str
    text: str
    hex: str
    branch: bool
    call: bool
    ops: tuple        # ('imm', v) | ('mem', base, index, scale, disp & 0xFFFFFFFF, size) | ('reg', name)

    def values(self):
        """(kind, value) for every immediate and memory displacement (32-bit)."""
        for op in self.ops:
            if op[0] == 'imm':
                yield 'imm', op[1]
            elif op[0] == 'mem':
                yield 'disp', op[4]

    def frame(self):
        """True when a memory operand is esp/ebp based (stack frame)."""
        return any(op[0] == 'mem' and op[1] in ('esp', 'ebp') for op in self.ops)


def _private():
    private = ROOT / 'private'
    if not (private / 'game/default.xbe').is_file() and ROOT.parent.name == 'worktrees':
        private = ROOT.parents[2] / 'private'
    return private


@lru_cache(maxsize=1)
def _decode():
    private = _private()
    xbe = private / 'game/default.xbe'
    functions = private / 'pipeline/disasm/functions.json'
    recomp = private.parent / 'external/ts3FPrecomp'
    if not (xbe.is_file() and functions.is_file() and (recomp / 'tools/recomp/disasm.py').is_file()):
        return None
    sys.path.insert(0, str(recomp))
    from tools.recomp import config
    from tools.recomp.disasm import Disassembler, _decode as decode
    config.configure_from_xbe(str(xbe))
    data = xbe.read_bytes()
    dis = Disassembler()
    out = []
    for f in json.loads(functions.read_text(encoding='utf-8')):
        if 'end' not in f:
            continue
        f['_addr'], f['end'] = int(f['start'], 16), int(f['end'], 16)
        if f['_addr'] >= GAME_CODE_END or not config.is_code_address(f['_addr']):
            continue
        for i in decode(dis, data, f):
            ops = []
            for op in i.operands:
                if op.type == 'imm':
                    ops.append(('imm', op.imm & 0xFFFFFFFF))
                elif op.type == 'mem':
                    ops.append(('mem', op.mem_base, op.mem_index, op.mem_scale, op.mem_disp & 0xFFFFFFFF, op.mem_size))
                else:
                    ops.append(('reg', op.reg))
            out.append(Insn(i.address, f['_addr'], i.mnemonic, i.op_str, i.bytes_hex,
                            bool(i.is_branch), bool(i.is_call), tuple(ops)))
    return out


def decoded():
    insns = _decode()
    if insns is None:
        pytest.skip('needs the private XBE, its disassembly and external/ts3FPrecomp')
    return insns


def patched(*areas):
    """Addresses rewritten by the named guest-code tables (operand, code, relocate sites)."""
    out = set()
    for area in areas:
        table = json.loads((ROOT / f'config/guest-code-patches/{area}.json').read_text(encoding='utf-8'))
        for e in table['patches']:
            if 'address' in e:
                out.add(int(e['address'], 16))
            for a, how in (e.get('sites') or {}).items():
                if how != 'keep':
                    out.add(int(a, 16))
    return out


def reviewed(text):
    """Parse 'ADDRESS reason...' lines (blank lines and # comments ignored)."""
    out = {}
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        addr, _, reason = line.partition(' ')
        assert reason.strip(), f'{addr}: every reviewed site needs a reason'
        out[int(addr, 16)] = reason.strip()
    return out


def neg(values):
    values = set(values)
    return values | {(-v) & 0xFFFFFFFF for v in values}


def refs_functions(insns, globals_):
    """Functions with an absolute memory operand or immediate equal to one of globals_."""
    out = set()
    for i in insns:
        for op in i.ops:
            if (op[0] == 'mem' and not op[1] and not op[2] and op[4] in globals_) or (op[0] == 'imm' and op[1] in globals_):
                out.add(i.function)
    return out


def unclassified(found, *classified):
    left = {a: s for a, s in found.items() if not any(a in c for c in classified)}
    return {f'0x{a:08X}': s for a, s in sorted(left.items())}
