"""MapMaker anim slot budget: the anim names a MapMaker level registers.

Compiles src/interface/pc_mapmaker_animslots.inc against fake guest memory
(tests/mapmaker/mapmaker_animslots.c, with mutations that must be caught) and,
when the private XBE and its disassembly are available, proves the stock
facts: the 940-slot arrays and their unchecked count, the one-fixup flag, the
v0.4.0-mapmaker.34 crash entry, the boss paks' registration tables (the
fixture's copies), every name/id they share, and that each boss's handlers
play only the anims the budget keeps.
"""
import os
from pathlib import Path
import re
import struct
import subprocess
from collections import defaultdict

import pytest

from native_compiler import native_compiler
from mapmaker_decoded import decoded, _private

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_animslots.inc'
FIXTURE = ROOT / 'tests/mapmaker/mapmaker_animslots.c'

MUTATIONS = {
    'keep-everything': ('    if(!p) return 1;\n', '    return 1;\n'),
    'keep-nothing': ('    if(!p) return 1;\n', '    if(!p) return 1;\n    return 0;\n'),
    'khallos-wrong-entry': ('{{32,1},{0,0},{0,0}},"lv11"', '{{31,1},{0,0},{0,0}},"lv11"'),
    'worm-no-barney': ('{{5,1},{21,8},{0,0}},"lv1_2"', '{{21,8},{0,0},{0,0}},"lv1_2"'),
    'crowe-no-eggnoise': ('{{23,3},{29,9},{0,0}},"lv14"', '{{23,3},{29,8},{0,0}},"lv14"'),
    'skeleton-no-eye': ('{{1,11},{14,1},{0,0}},"lv1_6"', '{{1,11},{0,0},{0,0}},"lv1_6"'),
    'third-run-ignored': ('for(r=0;r<3u;r++)', 'for(r=0;r<1u;r++)'),
    'any-count': ('ans_paks[i].table==table && ans_paks[i].count==count', 'ans_paks[i].table==table'),
    'shared-registered': ('        if(id<ANS_IDS && ANS_H(ANS_MAP+id*2u)!=ANS_FREE) { shared++;continue; }   /* another pak\'s copy */\n', ''),
    'no-full-guard': ('        if(WORD_AT(ANS_USED)>=ANS_SLOTS) { full++;continue; }\n', ''),
    'guard-one-late': ('WORD_AT(ANS_USED)>=ANS_SLOTS) { full++', 'WORD_AT(ANS_USED)>ANS_SLOTS) { full++'),
    'overflow-stock': ('    if(!p && used<=ANS_SLOTS && count<=ANS_SLOTS-used) return 0;', '    if(!p) return 0;'),
    'boss-stock-when-fits': ('    if(!p && used<=ANS_SLOTS', '    if(used<=ANS_SLOTS'),
    'any-scene': (' || WORD_AT(ANS_SCENE)!=100u) return 0;', ') return 0;'),
    'limits-ignored': ('    if(!mapmaker_limits.limits || WORD_AT(ANS_SCENE)', '    if(WORD_AT(ANS_SCENE)'),
    'no-ret': ('    MAPMAKER_GUEST_RET();\n    return 1;', '    return 1;'),
    'count-all-at-once': ('        guest_call(sub_0005B980,3,prefix,e,1u);', '        guest_call(sub_0005B980,3,prefix,e,count-k);'),
    'wrong-prefix': ('guest_call(sub_0005B980,3,prefix,e,1u)', 'guest_call(sub_0005B980,3,table,e,1u)'),
    'creature-rows-missing': ('#include "pc_mapmaker_creatures.def"\n', '\n'),
    'creature-whole-table-row': ('{((first)==0u && (n)==(count))?0u:(table),count,', '{(table),count,'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('animslots.exe' if os.name == 'nt' else 'animslots')
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/wd4706', '/wd4505',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src'),
                str(FIXTURE), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Wno-unused-function', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), '-I' + str(ROOT / 'src'),
                str(FIXTURE), '-o', str(exe), '-lm']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_animslots(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert result.stdout.strip() == 'ok'
        assert 'boss anims registered (kept, left out) from lv11 (0x0, 0x20)' in result.stderr   # khallosidle: shared
        assert 'anim slot table full (940)' in result.stderr
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_wired_through_the_worm_register_hook():
    worm = (ROOT / 'src/interface/pc_mapmaker_worm.inc').read_text(encoding='utf-8')
    assert '#include "pc_mapmaker_animslots.inc"' in worm
    body = worm[worm.index('void pc_mapmaker_worm_register(void (*original)(void))'):]
    assert body.split('}')[0].split('{', 1)[1].split() == ['if(ans_register())', 'return;', 'original();']


# ---- stock facts (need the private XBE and its disassembly) ----------------

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


def cstr(rd, va):
    return rd(va, 64).split(b'\0')[0].decode('latin1')


def target(i):
    m = re.search(r'0x([0-9a-f]+)', i.text)
    return int(m.group(1), 16) if m else None


def imms(i):
    return [v for k, v in i.values() if k == 'imm']


def registrations():
    """sub_001CA2F0: (call site, pak path, table, count) for every `if mounted(pak) sub_0005B980(prefix, table, count)`."""
    rd = xbe_reader()
    ins = sorted((i for i in decoded() if i.function == 0x001CA2F0), key=lambda i: i.address)
    out, pushes, pak = [], [], None
    for i in ins:
        if i.mnemonic == 'push' and imms(i):
            pushes.append(imms(i)[0])
        elif i.call and target(i) == 0x00062260:
            pak, pushes = cstr(rd, pushes[-1]), []
        elif i.call and target(i) == 0x0005B980:
            assert pushes[-1] == 0x0049C690
            out.append((i.address, pak, pushes[-2], pushes[-3]))
            pushes = []
    return rd, out


def table(rd, va, n):
    return [(cstr(rd, struct.unpack_from('<I', rd(va + k * 0x18, 0x18), 0)[0]),
             struct.unpack_from('<H', rd(va + k * 0x18, 0x18), 0x10)[0]) for k in range(n)]


BOSS_PAKS = {   # pak -> (table, count, the entries the budget keeps)
    'pak/stream/lv1/anim/lv1_3.pak': (0x0048C1B0, 0x10, set(range(0, 8))),
    'pak/stream/lv1/anim/lv1_6.pak': (0x0048C738, 0x12, set(range(1, 12)) | {14}),
    'pak/stream/lv81/anim/lv81.pak': (0x00484F68, 0x14, set(range(5, 20))),
    'pak/stream/lv14/anim/lv14.pak': (0x0047E938, 0x29, set(range(23, 26)) | set(range(29, 38))),
    'pak/stream/lv11/anim/lv11.pak': (0x00484410, 0x21, {32}),
    'pak/stream/lv1/anim/lv1_2.pak': (0x0048BEC8, 0x1E, {5} | set(range(21, 29))),
}


def test_inc_keeps_the_documented_entries():
    text = INC.read_text(encoding='utf-8')
    for pak, (va, n, keep) in BOSS_PAKS.items():
        name = pak.rsplit('/', 1)[1][:-4]
        m = re.search(r'\{0x%08Xu,0x%Xu,\{([^"]*)\},"%s"\}' % (va, n, name), text)
        assert m, pak
        runs = [tuple(int(x) for x in r) for r in re.findall(r'\{(\d+),(\d+)\}', m.group(1))]
        assert {k for f, c in runs for k in range(f, f + c)} == keep, pak


def test_slot_arrays_and_unchecked_count():
    """940 slots: [0x007E3954] = 0x007E2AA4 (940 pointers up to 0x007E3954 itself), 0x007DE660 (940 x 8 up to the
    id map 0x007E03C0); slots 0..0x22 reserved; sub_00057F30 takes [0x006D77F8] for a new id with no bound."""
    a = {i.address: i for i in decoded()}
    assert imms(a[0x0005C43E]) == [0x007E2AA4] and a[0x0005C43E].ops[0][4] == 0x007E3954
    assert 0x3AD in imms(a[0x0005C47E]) and 0x007E2AA0 in imms(a[0x0005C483])     # count word + 940 pointers
    assert 0x758 in imms(a[0x0005C48C]) and 0x007DE660 in imms(a[0x0005C491])     # 940 * 8 bytes
    assert 0x601 in imms(a[0x0005C46E]) and 0x007E03C0 in imms(a[0x0005C473])     # id map: 0xC03 halfwords of -1
    assert 0x23 in imms(a[0x0005C4C9]) and 0x23 in imms(a[0x0005C4CE])
    assert 0x007E2AA4 + 940 * 4 == 0x007E3954 and 0x007DE660 + 940 * 8 == 0x007E03C0
    assert any(o[0] == 'mem' and o[4] == 0x006D77F8 for o in a[0x00057FC1].ops)    # a new id: the next slot
    body = [i for i in decoded() if i.function == 0x00057F30]
    assert not any(0x3AC in imms(i) or 940 in imms(i) for i in body)              # no bound on the slot
    assert a[0x00057FE4].mnemonic == 'mov' and 'edx + ecx*4' in a[0x00057FE4].text   # [[0x007E3954] + slot*4] = anim


def test_one_fixup_per_anim_file():
    """Flag 4 at the anim header +8: tested at 0x00057FEA (set: no fixup, no new slot), set at 0x000581EE; the
    count only grows when the slot was the next one (0x000581EC)."""
    a = {i.address: i for i in decoded()}
    assert 4 in imms(a[0x00057FEA]) and a[0x00057FF7].mnemonic == 'je'
    assert 4 in imms(a[0x000581E6]) and a[0x000581F4].ops[0][4] == 0x006D77F8
    assert a[0x000581EC].mnemonic == 'cmp' and a[0x000581F1].mnemonic == 'jne'


def test_crash_entry_of_mapmaker34():
    """The .34 dump: ecx 940 (the slot), edi 0x2D1, esi 0x0048F838 = the id of entry 10 (millsgrenade_reload) of
    grenade.pak's table 0x0048F738 (sub_0005B980 walks esi = table + 0x10 + k*0x18), registered at 0x001CB1C3 after
    every boss pak."""
    rd, regs = registrations()
    site = {r[0]: r for r in regs}
    assert site[0x001CB1C3][1:] == ('pak/anim/gun/grenade.pak', 0x0048F738, 0x10)
    assert table(rd, 0x0048F738, 0x10)[10] == ('millsgrenade_reload', 0x2D1) and 0x0048F738 + 0x10 + 10 * 0x18 == 0x0048F838
    order = [r[1] for r in regs]
    assert all(order.index(p) < order.index('pak/anim/gun/grenade.pak') for p in BOSS_PAKS)


def test_boss_tables_and_fixture_copies():
    rd, regs = registrations()
    by_pak = {p: (t, n) for _, p, t, n in regs}
    fixture = FIXTURE.read_text(encoding='utf-8')
    for pak, (va, n, _) in BOSS_PAKS.items():
        assert by_pak[pak] == (va, n), pak
        name = pak.rsplit('/', 1)[1][:-4]
        body = fixture[fixture.index('static const struct ent tab_%s[]={' % name):]
        body = body[:body.index('};')]
        copy = [(nm, int(i, 16)) for nm, i in re.findall(r'\{"([^"]+)",0x([0-9A-F]+)\}', body)]
        assert copy == table(rd, va, n), pak


def test_every_overlap_between_boss_paks():
    """Every name or id two boss paks share (lv11, the seated Khallos' pak, shares none: its .34 crash was the slot
    count) and that no kept entry is named or numbered by another boss pak or by a pak/anim/ table (the paks a
    scene 100 level mounts besides the boss paks; story level paks such as lv17_4, which also names
    princess_low_idle1 under another id, are never mounted there)."""
    rd, regs = registrations()
    tabs = {p: table(rd, va, n) for p, (va, n, _) in BOSS_PAKS.items()}
    short = {p: p.rsplit('/', 1)[1][:-4] for p in tabs}
    found = set()
    for p in tabs:
        for q in tabs:
            if short[p] < short[q]:
                for np_, ip in tabs[p]:
                    for nq, iq in tabs[q]:
                        if np_ == nq or ip == iq:
                            found.add((short[p], np_, ip, short[q], nq, iq))
    assert found == {
        ('lv1_2', 'jblookout', 0x193, 'lv1_3', 'jblookout', 0x193),
        ('lv1_2', 'jbgrossend', 0x1AE, 'lv1_6', 'jbgrossend', 0x1AE),
        ('lv14', 'r110_run', 0x7DB, 'lv81', 'r110_run', 0x6C0),
        ('lv14', 'r110_normalwalk', 0x7DC, 'lv81', 'r110_normalwalk', 0x6BF),
        ('lv14', 'r110_stand_idle1', 0x7DD, 'lv81', 'r110_stand_idle1', 0x6BE),
    }
    for p, (va, n, keep) in BOSS_PAKS.items():
        for k in keep:
            nm, i = tabs[p][k]
            for _, q, t, c in regs:
                if q != p and (q in BOSS_PAKS or q.startswith('pak/anim/')):
                    assert all(e[0] != nm and e[1] != i for e in table(rd, t, c)), (p, nm, q)
            assert all(not (e[0] == nm or e[1] == i) for j, e in enumerate(tabs[p]) if j != k)


ROOTS = {   # the bosses' handlers (area docs): the AI class update, message handler, init
    'pak/stream/lv1/anim/lv1_3.pak': [0x00280FA0],
    'pak/stream/lv1/anim/lv1_6.pak': [0x0029FE00],
    'pak/stream/lv81/anim/lv81.pak': [0x002998E0, 0x0029CBF0],
    'pak/stream/lv14/anim/lv14.pak': [0x00293730, 0x00294520, 0x0028FE30, 0x00292430],
    'pak/stream/lv11/anim/lv11.pak': [0x00263260],
    'pak/stream/lv1/anim/lv1_2.pak': [0x001DAAB0, 0x001DA990, 0x001D1F30, 0x001D2350],
}
NOT_ANIMS = {0x001DAC65}   # push 0x155 (an sub_000468A0 argument, not poolscientistidle)


def test_boss_anims_are_kept():
    """Every id of a boss pak's table that the boss's code (its handlers and the callees only a few functions
    share) uses is a kept entry: nothing the boss plays is left out."""
    rd, _ = registrations()
    calls, callers, uses = defaultdict(set), defaultdict(set), defaultdict(set)
    for i in decoded():
        if (i.call or i.mnemonic == 'jmp') and not i.frame():
            t = target(i)
            if t is not None:
                calls[i.function].add(t)
                callers[t].add(i.function)
        if i.address not in NOT_ANIMS:
            for v in imms(i):
                uses[i.function].add(v)
    for pak, roots in ROOTS.items():
        va, n, keep = BOSS_PAKS[pak]
        ids = {i: k for k, (_, i) in enumerate(table(rd, va, n))}
        seen, todo = set(roots), list(roots)
        while todo:
            f = todo.pop()
            for t in calls[f]:
                if t not in seen and len(callers[t]) <= 4:
                    seen.add(t)
                    todo.append(t)
        used = {ids[v] for f in seen for v in uses[f] if v in ids}
        assert used and used <= keep, (pak, sorted(used - keep))
