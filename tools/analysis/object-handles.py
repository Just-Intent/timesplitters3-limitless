"""Write config/guest-code-patches/handles.json: engine object handles slot << 21.

Usage (from the project root, after a Generate so the disassembly exists):

    .venv/Scripts/python.exe tools/analysis/object-handles.py [--root <main checkout>] [--check]

An engine object handle is `slot << 22 | serial` in the stock game (encode
0x000A69BE `shl ecx,0x16`, serial [0x0072F5E0] wrapped by `test eax,0xFFC00000`
at 0x000A69C4), decoded as `shr r,0x16` then `imul r,r,0x2E8` (the pool
stride) in every reader. That limits the pool to 1024 slots. The table moves
the boundary one bit down (`slot << 21 | serial`, serial 1..0x1FFFFF), so the
pool can hold 2047 slots (slot 2047 could form -1). Every decode found by the
lifter's own decoder is rewritten (one byte each: 0x16 -> 0x15), plus the
encode and the serial wrap. The same scan is the contract of
tests/mapmaker/test_mapmaker_handles_classified.py, so a decode the table
misses fails there. --check compares instead of writing.
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GAME_CODE_END = 0x003D3380            # .text of the game; D3D, XGRPH, XONLINE, ... follow
TABLE = ROOT / 'config/guest-code-patches/handles.json'
ENCODE = [
    ('handle-encode-shift', 0x000A6880, 0x000A69BE, 'c1e116', 'c1e115',
     'handle = [obj+0x88] << 21 | serial (was << 22): 11 slot bits, pool up to 2047 slots'),
    ('handle-serial-wrap', 0x000A6880, 0x000A69C4, 'a90000c0ff', 'a90000e0ff',
     'serial [0x0072F5E0] resets to 1 at 0x200000 (test 0xFFE00000; was 0x400000): serial 1..0x1FFFFF'),
]


def decode_sites(root):
    """(address, function, bytes hex) of every handle decode `shr r,0x16; imul r,r,0x2E8`."""
    sys.path.insert(0, str(ROOT / 'tools/compiler'))
    import translation_layout as config
    from translation_decode import Disassembler
    from guest_code_patches import _decode as decode
    xbe = root / 'private/game/default.xbe'
    config.configure_from_xbe(str(xbe))
    data = xbe.read_bytes()
    dis = Disassembler()
    functions = json.loads((root / 'private/pipeline/disasm/functions.json').read_text(encoding='utf-8'))
    found = {}
    for f in sorted((f for f in functions if 'end' in f), key=lambda f: int(f['start'], 16)):
        start, end = int(f['start'], 16), int(f['end'], 16)
        if start >= GAME_CODE_END or not config.is_code_address(start):
            continue
        g = dict(f, _addr=start, end=end)
        insns = list(decode(dis, data, g))
        for k, i in enumerate(insns):
            ops = i.operands
            if i.mnemonic != 'shr' or len(ops) != 2 or ops[0].type != 'reg' or ops[1].type != 'imm' or ops[1].imm != 0x16:
                continue
            r = ops[0].reg
            if any(j.mnemonic == 'imul' and len(j.operands) == 3 and j.operands[0].type == 'reg'
                   and j.operands[0].reg == r and j.operands[2].type == 'imm' and j.operands[2].imm == 0x2E8
                   for j in insns[k + 1:k + 4]):
                found.setdefault(i.address, (start, i.bytes_hex, r))   # lowest containing function
    return found


def build(root):
    patches = []
    for eid, func, addr, old, new, reason in ENCODE:
        patches.append({'id': eid, 'kind': 'code', 'function': f'0x{func:08X}', 'address': f'0x{addr:08X}',
                        'original_hex': old, 'new_hex': new, 'reason': reason})
    for addr, (func, hexs, reg) in sorted(decode_sites(root).items()):
        assert hexs.endswith('16') and len(hexs) == 6, (hex(addr), hexs)
        patches.append({'id': f'handle-decode-{addr:08X}', 'kind': 'code', 'function': f'0x{func:08X}',
                        'address': f'0x{addr:08X}', 'original_hex': hexs, 'new_hex': hexs[:-2] + '15',
                        'reason': f'handle decode shr {reg},0x16 -> 0x15 (slot = handle >> 21)'})
    return {'patches': patches}


def render(table):
    lines = ['{', '  "patches": [']
    rows = [json.dumps(p, separators=(', ', ': ')) for p in table['patches']]
    lines += ['    ' + r + (',' if k + 1 < len(rows) else '') for k, r in enumerate(rows)]
    lines += ['  ]', '}', '']
    return '\n'.join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--root', default=str(ROOT), help='checkout holding private/ (e.g. the main one from a worktree)')
    ap.add_argument('--check', action='store_true', help='compare with the tracked table instead of writing it')
    args = ap.parse_args()
    root = Path(args.root)
    if not (root / 'private/game/default.xbe').is_file() and ROOT.parent.name == 'worktrees':
        root = ROOT.parents[2]
    table = build(root)
    text = render(table)
    if args.check:
        same = TABLE.is_file() and TABLE.read_text(encoding='utf-8') == text
        print('handles.json is current' if same else 'handles.json differs from the decode scan')
        return 0 if same else 1
    TABLE.write_text(text, encoding='utf-8', newline='\n')
    print(f'{TABLE.relative_to(ROOT)}: {len(table["patches"])} entries')
    return 0


if __name__ == '__main__':
    sys.exit(main())
