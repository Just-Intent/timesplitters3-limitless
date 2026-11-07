"""A small x86-32 interpreter for decoder checks of short guest code ranges.

Runs the original instructions of the private XBE on a fixture's guest memory
image, so a C port can be compared with the code it replaces
(test_mapmaker_portals.py, test_mapmaker_astar.py). It knows only what those
ranges use: integer ALU, 8/16/32-bit moves, push/pop/call/ret, rep strings,
SSE scalar (float32, rounded as the hardware does) and x87 as the lift has it
(every stack value a double, rounded to float only where stored). Operand
rewrites of the guest-code tables are applied as the lift sees them.
"""
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def private_dir():
    private = ROOT / 'private'
    if not (private / 'game/default.xbe').is_file() and ROOT.parent.name == 'worktrees':
        private = ROOT.parents[2] / 'private'
    return private


class Xbe:
    def __init__(self, data):
        self.d = data
        base = struct.unpack_from('<I', data, 0x104)[0]
        n, at = struct.unpack_from('<II', data, 0x11C)
        self.secs = [struct.unpack_from('<IIII', data, at - base + i * 56 + 4) for i in range(n)]

    def read(self, va, n):
        for v, vs, ra, rs in self.secs:
            if v <= va < v + vs and va - v + n <= rs:
                return self.d[ra + va - v:ra + va - v + n]
        raise KeyError(hex(va))


class Emu:
    REG = ('eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi')
    LOW8 = {'al': 'eax', 'cl': 'ecx', 'dl': 'edx', 'bl': 'ebx'}
    HIGH8 = {'ah': 'eax', 'ch': 'ecx', 'dh': 'edx', 'bh': 'ebx'}
    LOW16 = {'ax': 'eax', 'cx': 'ecx', 'dx': 'edx', 'bx': 'ebx', 'si': 'esi', 'di': 'edi', 'bp': 'ebp', 'sp': 'esp'}

    def __init__(self, mem, code, overrides=None, relocations=()):
        """code: [(va, bytes)]; overrides: {address: (operand, value)};
        relocations: [(old, size, new, {address: 'offset'|'end'|'keep'})] (guest-code 'relocate' rows)."""
        import capstone
        import numpy
        self.np = numpy
        self.mem = mem
        self.r = dict.fromkeys(self.REG, 0)
        self.x = [numpy.zeros(4, dtype=numpy.float32) for _ in range(8)]
        self.st = []                       # x87 stack, st(0) last
        self.zf = self.sf = self.cf = self.of = self.pf = False
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        md.detail = True
        self.code = {}
        for start, blob in code:
            for i in md.disasm(blob, start):
                self.code[i.address] = i
        self.overrides = dict(overrides or {})
        self.relocations = list(relocations)
        self.steps = 0

    # relocated tables: a displacement or immediate inside the old table moves with it
    def reloc(self, address, v):
        for old, size, new, new_size, sites in self.relocations:
            how = sites.get(address)
            if how == 'offset' and old <= v < old + size:
                return (v - old + new) & 0xFFFFFFFF
            if how == 'end' and v == old + size:
                return new + new_size
        return v

    # memory
    def r32(self, a): return struct.unpack_from('<I', self.mem, a)[0]
    def w32(self, a, v): struct.pack_into('<I', self.mem, a, v & 0xFFFFFFFF)
    def rf(self, a): return self.np.frombuffer(bytes(self.mem[a:a + 4]), dtype=self.np.float32)[0]
    def wf(self, a, f): self.mem[a:a + 4] = self.np.float32(f).tobytes()

    def addr(self, i, k):
        m = i.operands[k].mem
        disp = m.disp & 0xFFFFFFFF
        ov = self.overrides.get(i.address)
        if ov and ov[0] == k:
            disp = ov[1]
        else:
            disp = self.reloc(i.address, disp)
        a = disp
        if m.base:
            a += self.r[i.reg_name(m.base)]
        if m.index:
            a += self.r[i.reg_name(m.index)] * m.scale
        return a & 0xFFFFFFFF

    def reg_get(self, name):
        if name in self.HIGH8:
            return self.r[self.HIGH8[name]] >> 8 & 0xFF
        if name in self.LOW8:
            return self.r[self.LOW8[name]] & 0xFF
        if name in self.LOW16:
            return self.r[self.LOW16[name]] & 0xFFFF
        return self.r[name]

    def reg_put(self, name, v):
        if name in self.HIGH8:
            f = self.HIGH8[name]
            self.r[f] = (self.r[f] & 0xFFFF00FF) | (v & 0xFF) << 8
        elif name in self.LOW8:
            f = self.LOW8[name]
            self.r[f] = (self.r[f] & 0xFFFFFF00) | (v & 0xFF)
        elif name in self.LOW16:
            f = self.LOW16[name]
            self.r[f] = (self.r[f] & 0xFFFF0000) | (v & 0xFFFF)
        else:
            self.r[name] = v & 0xFFFFFFFF

    def val(self, i, k):
        import capstone.x86 as x
        op = i.operands[k]
        if op.type == x.X86_OP_IMM:
            ov = self.overrides.get(i.address)
            return ov[1] if ov and ov[0] == k else self.reloc(i.address, op.imm & 0xFFFFFFFF)
        if op.type == x.X86_OP_REG:
            return self.reg_get(i.reg_name(op.reg))
        a = self.addr(i, k)
        if op.size == 1:
            return self.mem[a]
        if op.size == 2:
            return struct.unpack_from('<H', self.mem, a)[0]
        return self.r32(a)

    def put(self, i, k, v):
        import capstone.x86 as x
        op = i.operands[k]
        if op.type == x.X86_OP_REG:
            self.reg_put(i.reg_name(op.reg), v)
            return
        a = self.addr(i, k)
        if op.size == 1:
            self.mem[a] = v & 0xFF
        elif op.size == 2:
            struct.pack_into('<H', self.mem, a, v & 0xFFFF)
        else:
            self.w32(a, v)

    def width(self, i):
        return i.operands[0].size if i.operands else 4

    def flags_sub(self, a, b, r, w=4):
        top = 1 << (8 * w - 1)
        mask = (1 << (8 * w)) - 1
        self.zf = r & mask == 0
        self.sf = bool(r & top)
        self.cf = (a & mask) < (b & mask)
        self.of = bool(((a ^ b) & (a ^ r)) & top)

    def flags_logic(self, r, w=4):
        mask = (1 << (8 * w)) - 1
        self.zf = r & mask == 0
        self.sf = bool(r & (1 << (8 * w - 1)))
        self.cf = self.of = False

    def xmm_src(self, i, k):
        import capstone.x86 as x
        op = i.operands[k]
        if op.type == x.X86_OP_REG:
            return self.x[int(i.reg_name(op.reg)[3:])][0]
        return self.rf(self.addr(i, k))

    def xreg(self, i, k):
        return int(i.reg_name(i.operands[k].reg)[3:])

    def sti(self, i, k):
        return int(i.reg_name(i.operands[k].reg)[3])          # 'st(3)' -> 3

    def push(self, v):
        self.r['esp'] = (self.r['esp'] - 4) & 0xFFFFFFFF
        self.w32(self.r['esp'], v)

    def pop(self):
        v = self.r32(self.r['esp'])
        self.r['esp'] = (self.r['esp'] + 4) & 0xFFFFFFFF
        return v

    def taken(self, m):
        return {'je': self.zf, 'jne': not self.zf, 'jl': self.sf != self.of, 'jge': self.sf == self.of,
                'jle': self.zf or self.sf != self.of, 'jg': not self.zf and self.sf == self.of,
                'ja': not self.cf and not self.zf, 'jbe': self.cf or self.zf, 'jb': self.cf, 'jae': not self.cf,
                'jp': self.pf, 'jnp': not self.pf, 'js': self.sf, 'jns': not self.sf}[m]

    def fmem(self, i, k):
        return float(self.rf(self.addr(i, k)))                # fld dword: float to double, exact

    def run(self, eip, end, limit, stubs=None):
        """stubs: {address: fn(emu)} run in place of a callee (fn reads its stack, sets eax; the
        stub then returns like a cdecl ret)."""
        import capstone.x86 as x
        np = self.np
        stubs = stubs or {}
        while eip != end:
            self.steps += 1
            if self.steps > limit:
                raise RuntimeError('step limit')
            if eip in stubs:
                stubs[eip](self)
                eip = self.pop()
                continue
            i = self.code[eip]
            m = i.mnemonic
            nxt = eip + i.size
            w = self.width(i)
            if m.startswith('j'):
                if m == 'jmp' or self.taken(m):
                    nxt = i.operands[0].imm
            elif m == 'call':
                self.push(nxt)
                nxt = i.operands[0].imm
            elif m == 'ret':
                nxt = self.pop()
                if i.operands:
                    self.r['esp'] += i.operands[0].imm
            elif m == 'push':
                self.push(self.val(i, 0))
            elif m == 'pop':
                self.put(i, 0, self.pop())
            elif m in ('mov', 'movsx', 'lea'):
                if m == 'lea':
                    v = self.addr(i, 1)
                elif m == 'movsx':
                    v = self.val(i, 1)
                    v = v - 0x10000 if i.operands[1].size == 2 and v & 0x8000 else v - 0x100 if i.operands[1].size == 1 and v & 0x80 else v
                else:
                    v = self.val(i, 1)
                self.put(i, 0, v)
            elif m in ('add', 'sub', 'cmp'):
                a, b = self.val(i, 0), self.val(i, 1) & ((1 << (8 * w)) - 1)
                if m == 'add':
                    r = a + b
                    mask = (1 << (8 * w)) - 1
                    self.zf = r & mask == 0
                    self.sf = bool(r & (1 << (8 * w - 1)))
                    self.cf = r > mask
                    self.of = bool(~(a ^ b) & (a ^ r) & (1 << (8 * w - 1)))
                else:
                    r = a - b
                    self.flags_sub(a, b, r, w)
                if m != 'cmp':
                    self.put(i, 0, r)
            elif m in ('inc', 'dec'):
                a = self.val(i, 0)
                r = a + (1 if m == 'inc' else -1)
                cf = self.cf
                self.flags_sub(a, 0xFFFFFFFF if m == 'inc' else 1, r, w)
                self.cf = cf
                self.of = (a == 0x7FFFFFFF) if m == 'inc' else (a == 0x80000000)
                self.put(i, 0, r)
            elif m in ('xor', 'and', 'or', 'test'):
                a, b = self.val(i, 0), self.val(i, 1)
                r = a ^ b if m == 'xor' else a & b if m in ('and', 'test') else a | b
                self.flags_logic(r, w)
                if m != 'test':
                    self.put(i, 0, r)
            elif m == 'sar':
                a, n = self.val(i, 0), self.val(i, 1) & 31
                sa = a - (1 << 32) if a & 0x80000000 else a
                r = (sa >> n) & 0xFFFFFFFF
                self.flags_logic(r)
                self.put(i, 0, r)
            elif m == 'neg':
                a = self.val(i, 0)
                r = (-a) & 0xFFFFFFFF
                self.flags_sub(0, a, r, w)
                self.cf = a != 0
                self.put(i, 0, r)
            elif m == 'sbb':
                a, b = self.val(i, 0), self.val(i, 1)
                r = (a - b - int(self.cf)) & 0xFFFFFFFF
                self.flags_sub(a, (b + int(self.cf)) & 0xFFFFFFFF, r, w)
                self.put(i, 0, r)
            elif m == 'movzx':
                self.put(i, 0, self.val(i, 1))
            elif m == 'imul' and len(i.operands) == 2:
                a, b = self.val(i, 0), self.val(i, 1)
                sa = a - (1 << 32) if a & 0x80000000 else a
                sb = b - (1 << 32) if b & 0x80000000 else b
                self.put(i, 0, sa * sb)
            elif m == 'imul':
                a, b = self.val(i, 1), self.val(i, 2)
                sa = a - (1 << 32) if a & 0x80000000 else a
                sb = b - (1 << 32) if b & 0x80000000 else b
                self.put(i, 0, sa * sb)
            elif m in ('shl', 'shr'):
                a, n = self.val(i, 0), self.val(i, 1) & 31
                r = (a << n) & 0xFFFFFFFF if m == 'shl' else a >> n
                self.flags_logic(r)
                self.put(i, 0, r)
            elif m.startswith('rep '):
                kind = m.split()[1]
                step = 4 if kind.endswith('d') else 1
                n = self.r['ecx']
                if kind.startswith('stos'):
                    v = self.r['eax']
                    for k in range(n):
                        a = self.r['edi'] + k * step
                        if step == 4:
                            self.w32(a, v)
                        else:
                            self.mem[a] = v & 0xFF
                else:
                    for k in range(n):
                        s, d = self.r['esi'] + k * step, self.r['edi'] + k * step
                        self.mem[d:d + step] = self.mem[s:s + step]
                    self.r['esi'] += n * step
                self.r['edi'] += n * step
                self.r['ecx'] = 0
            elif m in ('sete', 'setne'):
                self.put(i, 0, int(self.zf if m == 'sete' else not self.zf))
            elif m in ('nop', 'int3'):
                if m == 'int3':
                    raise RuntimeError(f'{eip:#x} int3')
            elif m == 'xorps':
                self.x[self.xreg(i, 0)] = np.zeros(4, dtype=np.float32)
            elif m == 'movaps':
                self.x[self.xreg(i, 0)] = self.x[self.xreg(i, 1)].copy()
            elif m == 'movss':
                if i.operands[0].type == x.X86_OP_REG:
                    k = self.xreg(i, 0)
                    if i.operands[1].type == x.X86_OP_REG:
                        self.x[k][0] = self.x[self.xreg(i, 1)][0]
                    else:
                        self.x[k] = np.zeros(4, dtype=np.float32)
                        self.x[k][0] = self.rf(self.addr(i, 1))
                else:
                    self.wf(self.addr(i, 0), self.x[self.xreg(i, 1)][0])
            elif m in ('addss', 'subss', 'mulss', 'divss'):
                k = self.xreg(i, 0)
                a, b = self.x[k][0], self.xmm_src(i, 1)
                with np.errstate(all='ignore'):
                    r = a + b if m == 'addss' else a - b if m == 'subss' else a * b if m == 'mulss' else a / b
                self.x[k][0] = np.float32(r)
            elif m == 'rsqrtss':
                k = self.xreg(i, 0)
                with np.errstate(all='ignore'):
                    self.x[k][0] = np.float32(1.0) / np.sqrt(np.float32(self.xmm_src(i, 1)))   # the lift: 1/sqrtf
            elif m == 'cvtsi2ss':
                v = self.val(i, 1)
                self.x[self.xreg(i, 0)][0] = np.float32(v - (1 << 32) if v & 0x80000000 else v)
            elif m == 'comiss':
                a, b = self.x[self.xreg(i, 0)][0], self.xmm_src(i, 1)
                if np.isnan(a) or np.isnan(b):
                    self.zf = self.pf = self.cf = True
                else:
                    self.zf, self.pf, self.cf = bool(a == b), False, bool(a < b)
                self.of = self.sf = False
            # x87 as lifted: doubles
            elif m == 'fld':
                if i.operands[0].type == x.X86_OP_REG:
                    self.st.append(self.st[-1 - self.sti(i, 0)])
                else:
                    self.st.append(self.fmem(i, 0))
            elif m in ('fadd', 'fsub', 'fmul'):
                if i.operands[0].type == x.X86_OP_MEM:
                    b = self.fmem(i, 0)
                else:
                    b = self.st[-1 - self.sti(i, len(i.operands) - 1)]
                a = self.st[-1]
                self.st[-1] = a + b if m == 'fadd' else a - b if m == 'fsub' else a * b
            elif m == 'faddp':
                k = self.sti(i, 0) if i.operands else 1
                self.st[-1 - k] = self.st[-1 - k] + self.st[-1]
                self.st.pop()
            elif m == 'fsqrt':
                a = self.st[-1]
                self.st[-1] = math.sqrt(a) if a >= 0 else float('nan')
            elif m == 'fxch':
                k = self.sti(i, len(i.operands) - 1) if i.operands else 1
                self.st[-1], self.st[-1 - k] = self.st[-1 - k], self.st[-1]
            elif m == 'fstp':
                if i.operands[0].type == x.X86_OP_REG:
                    k = self.sti(i, 0)
                    self.st[-1 - k] = self.st[-1]
                else:
                    self.wf(self.addr(i, 0), self.st[-1])          # (float)double, round to nearest
                self.st.pop()
            else:
                raise NotImplementedError(f'{eip:#x} {m} {i.op_str}')
            eip = nxt


def relocations(table_path):
    """The 'relocate' rows of a guest-code table as Emu relocations."""
    import json
    out = []
    for p in json.loads(Path(table_path).read_text(encoding='utf-8'))['patches']:
        if p['kind'] == 'relocate':
            out.append((int(p['old'], 16), int(p['size'], 16), int(p['new'], 16), int(p['new_size'], 16),
                        {int(a, 16): how for a, how in p['sites'].items()}))
    return out
