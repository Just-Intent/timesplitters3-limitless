"""List every reference in the game's code to a guest address range.

Usage (from the project root, after a Generate so the disassembly exists):

    .venv/Scripts/python.exe tools/guest-refs.py 0x00716148 0x78 [--data]

Prints one line per instruction whose immediate or memory displacement lies
in [start, start + size] (the end included, since loops compare against it):
address, function, value, offset from start, and the instruction. With
--data it also lists initialised data words in that range. This is the
same scan the lift's `relocate` entries are checked against
(config/guest-code-patches/), so its output is the `sites` list to
review and classify (offset / end / keep).
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('start', type=lambda s: int(s, 0))
    ap.add_argument('size', type=lambda s: int(s, 0))
    ap.add_argument('--data', action='store_true', help='also list data words in range')
    ap.add_argument('--root', default=str(ROOT),
                    help='checkout holding private/ and external/ (e.g. the main one from a worktree)')
    ap.add_argument('--xbe')
    ap.add_argument('--functions')
    args = ap.parse_args()
    root = Path(args.root)
    args.xbe = args.xbe or str(root / 'private/game/default.xbe')
    args.functions = args.functions or str(root / 'private/pipeline/disasm/functions.json')
    sys.path.insert(0, str(root / 'external/ts3FPrecomp'))

    from tools.recomp import config
    from tools.recomp.disasm import Disassembler, _decode
    config.configure_from_xbe(args.xbe)
    data = Path(args.xbe).read_bytes()
    funcs = []
    for f in json.loads(Path(args.functions).read_text(encoding='utf-8')):
        f['_addr'] = int(f['start'], 16)
        if 'end' in f:
            f['end'] = int(f['end'], 16)
            funcs.append(f)
    lo, hi = args.start, args.start + args.size
    dis = Disassembler()
    count = 0
    for f in funcs:
        if not config.is_code_address(f['_addr']):
            continue
        for insn in _decode(dis, data, f):
            if insn.is_branch or insn.is_call:
                continue
            for op in insn.operands:
                value = op.imm if op.type == 'imm' else (
                    op.mem_disp & 0xFFFFFFFF if op.type == 'mem' and isinstance(op.mem_disp, int)
                    and op.mem_disp > 0 else None)
                if value is not None and lo <= value <= hi:
                    tag = 'END' if value == hi else f'+0x{value - lo:X}'
                    print(f'0x{insn.address:08X}  sub_{f["_addr"]:08X}  0x{value:08X} {tag:>8}  '
                          f'{insn.bytes_hex:<20} {insn.mnemonic} {insn.op_str}')
                    count += 1
                    break
    if args.data:
        for s in config._SECTIONS:
            if s.is_code:
                continue
            for va in range(s.va, s.va + min(s.raw_size, s.va_size) - 3, 4):
                off = s.raw_addr + (va - s.va)
                value = int.from_bytes(data[off:off + 4], 'little')
                if lo <= value <= hi:
                    print(f'data 0x{va:08X} ({s.name}) = 0x{value:08X}')
    print(f'{count} code references', file=sys.stderr)


if __name__ == '__main__':
    main()
