"""MapMaker story creatures (story assets wave 2): one generic deployer, one
row per creature in src/interface/pc_mapmaker_creatures.def.

Compiles src/interface/pc_mapmaker_creatures.inc against fake guest memory
(tests/mapmaker/mapmaker_creatures.c, with mutations that must be caught) and,
when the private XBE, its decode and the story paks are available, proves the
rows: the class reaches its handler, the handler reads no scene-only state,
the anim ids it plays are the kept registration entries (and survive a
restart), the story level spawns that character with that class, and the
carrier ids are free.
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
INC = ROOT / 'src/interface/pc_mapmaker_creatures.inc'
DEF = ROOT / 'src/interface/pc_mapmaker_creatures.def'
FIXTURE = ROOT / 'tests/mapmaker/mapmaker_creatures.c'

MUTATIONS = {
    'any-scene': ('WORD_AT(CRT_SCENE)==100u && CRT_B(CRT_MODE)==10u', 'CRT_B(CRT_MODE)==10u'),
    'any-mode': ('WORD_AT(CRT_SCENE)==100u && CRT_B(CRT_MODE)==10u', 'WORD_AT(CRT_SCENE)==100u'),
    'limits-ignored': ('    return mapmaker_limits.limits;\n}', '    return 1;\n}'),
    'optout-ignored': ("        crt.optout=v && v[0]=='0' && !v[1];", '        crt.optout=0;(void)v;'),
    'preset-set': ('    if(CRT_H(info+0x14u)!=CRT_SET_MAP) return 0;\n', ''),
    'no-slots-11-20': ('    for(k=0;extra && k<CRT_B(extra+3u);k++) ids[CRT_INFO_SLOTS+k]=CRT_H(extra+4u+k*4u);\n', ''),
    'mount-unreadable': ('    if(crt.optout || crt_set(ids)!=1) return;\n    for(k=0;k<CRT_KINDS;k++) {\n        const CrtKind *r=&crt_kinds[k];\n        if(!crt_has(ids,r->id)) continue;',
                         '    if(crt.optout) return;\n    if(crt_set(ids)<0) for(k=0;k<CRT_SLOTS;k++) ids[k]=(uint16_t)crt_kinds[k%CRT_KINDS].id;\n    for(k=0;k<CRT_KINDS;k++) {\n        const CrtKind *r=&crt_kinds[k];\n        if(!crt_has(ids,r->id)) continue;'),
    'mount-any-member': ('        if(!crt_has(ids,r->id)) continue;\n        if(crt_mounted', '        if(crt_mounted'),
    'mount-twice': ('        if(crt_mounted(r->pak)) crt_log("anim pak already mounted",k,r->pak,0);\n        else if((used',
                    '        if((used'),
    'mount-table-full': ('(used=WORD_AT(CRT_MOUNTS))+CRT_MOUNT_KEEP>CRT_MOUNT_MAX', '(used=WORD_AT(CRT_MOUNTS))>CRT_MOUNT_MAX'),
    'request-member-look': (' || crt_has(ids,r->look)) continue;', ') continue;'),
    'request-no-lod': ('        for(i=0;i<2u;i++) if((m=crt_model(r->look,i))<CRT_MODEL_MAX)', '        for(i=0;i<1u;i++) if((m=crt_model(r->look,i))<CRT_MODEL_MAX)'),
    'create-not-ready': ('    if(!crt_look_ready(crt_kinds[k].look)) {', '    if(0) {'),
    'create-unread': ('    return r && MAPMAKER_PTR_OK(r,0x14u) && WORD_AT(r+CRT_REQ_DATA)!=0;', '    return r!=0;'),
    'create-not-restored': ('    for(i=0;i<2u;i++) WORD_AT(c+i*4u)=keep[i];\n', ''),
    'create-own-look': ('crt_kinds[k].look==CRT_NO_LOOK ||\n       !crt_play()', '0 ||\n       !crt_play()'),
    'start-unregistered': ('    else if((ok=crt_anims(k,&of))<of) {', '    else if((ok=crt_anims(k,&of))<1u) {'),
    'start-unmounted': ('    else if(!crt_mounted(crt_kinds[k].pak)) {', '    else if(0) {'),
    'start-no-hi': ('WORD_AT(rec)=crt_kinds[k].lo;WORD_AT(rec+4u)=crt_kinds[k].hi;', 'WORD_AT(rec)=crt_kinds[k].lo;'),
    'start-after-original': ('            fflush(stderr);\n        }\n    }\n    original();\n}', '            fflush(stderr);\n        }\n    }\n}'),
    'start-any-char': ('(k=crt_kind_of(CRT_H(a+CRT_ACTOR_CHAR)))>=CRT_KINDS', '(k=0)>=CRT_KINDS'),
}
DEF_MUTATIONS = {
    'ghost-class': ('"Catacomb Ghost",0x50000000u,0x00000000u', '"Catacomb Ghost",0x40000000u,0x00000000u'),
    'securibot-class': ('"SecuriDroid XP",0x10000000u,0x00004000u', '"SecuriDroid XP",0x11000000u,0x00004000u'),
    'mutant-class': ('"Mutant (Story)",0x10000000u,0x00008000u', '"Mutant (Story)",0x10000000u,0x00004000u'),
    'mutant-look': ('MAPMAKER_CREATURE(163u,0x30u,0x59u,', 'MAPMAKER_CREATURE(163u,0x30u,0xFFu,'),
    'ghost-pak': ('0x0049D968u,"lv17"', '0x0049DA84u,"lv17"'),
    'sk8-bot-class': ('"Prometheus SK/8",0x10000004u', '"Prometheus SK/8",0x11000000u'),
    'sd7-bot-class': ('"Prometheus SD/7",0x10000004u', '"Prometheus SD/7",0x01000000u'),
    'sk8-row-moved': ('MAPMAKER_CREATURE(159u,0x62u', 'MAPMAKER_CREATURE(166u,0x62u'),
    'animrb-partial': ('"Prometheus SK/8",0x10000004u,0x00000000u,0x0047E65Cu,"animrb",0x0048F240u,0x15u,0u,21u,0)',
                       '"Prometheus SK/8",0x10000004u,0x00000000u,0x0047E65Cu,"animrb",0x0048F240u,0x15u,1u,10u,0)'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    # both copied: a quoted include looks next to its includer first
    inc, table = INC.read_text(encoding='utf-8'), DEF.read_text(encoding='utf-8')
    if mutation in MUTATIONS:
        old, new = MUTATIONS[mutation]
        assert inc.count(old) == 1, mutation
        inc = inc.replace(old, new)
    elif mutation in DEF_MUTATIONS:
        old, new = DEF_MUTATIONS[mutation]
        assert table.count(old) == 1, mutation
        table = table.replace(old, new)
    (tmp_path / INC.name).write_text(inc, encoding='utf-8')
    (tmp_path / DEF.name).write_text(table, encoding='utf-8')
    exe = tmp_path / ('creatures.exe' if os.name == 'nt' else 'creatures')
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/wd4706', '/wd4505',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src'),
                str(FIXTURE), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Wno-unused-function', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), '-I' + str(ROOT / 'src'),
                str(FIXTURE), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS, *DEF_MUTATIONS])
def test_mapmaker_creatures(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_CREATURES')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert result.stdout.strip() == 'ok'
        err = result.stderr
        assert '[mapmaker] creatures: Catacomb Ghost: mounted lv17.pak (anim entries 30..33 kept' in err
        assert '[mapmaker] creatures: SecuriDroid XP: mounted lv6.pak (anim entries 3..13 kept' in err
        assert '[mapmaker] creatures: Mutant (Story): mounted lv22.pak (anim entries 41..46 kept' in err
        assert ('[mapmaker] creatures: Catacomb Ghost (character 0x2F) gets its story AI, class 0x0:0x50000000, '
                '4 of 4 anims from lv17.pak') in err
        assert '[mapmaker] creatures: SecuriDroid XP (character 0x4D) gets its story AI, class 0x4000:0x10000000, 11 of 11' in err
        assert '[mapmaker] creatures: Mutant (Story) (character 0x30) gets its story AI, class 0x8000:0x10000000, 6 of 6' in err
        assert 'Catacomb Ghost: anims not registered (resolved, kept): keeps the bot AI (0x0, 0x4)' in err
        assert "Mutant (Story): look's models not loaded: made with its own (0x30, 0x59)" in err
        assert "Mutant (Story): made with the look's models (character, object) (0x59, 0x777000)" in err
        assert "Catacomb Ghost: made with the look's models (character, object) (0x17, 0x777000)" in err
        assert "Mutant (Story): requested the look's models (0x613, 0x614)" in err
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


# ---- source contracts ---------------------------------------------------

def rows():
    out = []
    for m in re.finditer(r'^MAPMAKER_CREATURE\((.*)\)\s*$', DEF.read_text(encoding='utf-8'), re.M):
        f = [x.strip() for x in re.findall(r'"[^"]*"|[^,]+', m.group(1))]
        assert len(f) == 14, f
        num = lambda x: int(x.rstrip('u'), 0)
        out.append(dict(row=num(f[0]), id=num(f[1]), look=num(f[2]), preview=num(f[3]), label=f[4].strip('"'),
                        lo=num(f[5]), hi=num(f[6]), pak=num(f[7]), name=f[8].strip('"'), table=num(f[9]),
                        count=num(f[10]), first=num(f[11]), n=num(f[12]), level=f[13]))
    return out


def test_rows_and_wiring():
    r = rows()
    assert [x['label'] for x in r] == ['Prometheus SK/8', 'Prometheus SD/7', 'Catacomb Ghost', 'SecuriDroid XP',
                                       'Mutant (Story)', 'Pit Skeleton', 'Ragbug']
    assert all(len(x['label']) <= 15 for x in r)                     # rbs_extra label[16]
    assert [x['row'] for x in r] == [159, 160, 161, 162, 163, 164, 165]   # 159/160 kept from wave 1, after realbosses' 150..158
    assert len({x['id'] for x in r}) == len(r) and all(x['level'] == '0' for x in r)
    rb = (ROOT / 'src/interface/pc_mapmaker_realbosses.inc').read_text(encoding='utf-8')
    ans = (ROOT / 'src/interface/pc_mapmaker_animslots.inc').read_text(encoding='utf-8')
    for text in (rb, ans, INC.read_text(encoding='utf-8')):
        assert '#include "pc_mapmaker_creatures.def"' in text
    assert '{row,id,0x52420000u|(id),preview,label},' in rb
    assert 'typedef char rbs_extra_labels_fit[RBS_EXTRA_N<=RBS_EXTRA_MAX?1:-1];' in rb
    assert 'extra_label[RBS_EXTRA_MAX]' in rb and 'extra_label[6]' not in rb   # .27-.wave1 overflow (8 rows, 6 labels)
    for m in re.finditer(r'\{(\d+)u,0x([0-9A-F]+)u,0x52420', rb):         # existing synthetic rows and ids
        assert int(m.group(1)) not in {x['row'] for x in r} and int(m.group(2), 16) not in {x['id'] for x in r}
    assert {0x15, 0x28, 0x64}.isdisjoint({x['id'] for x in r})
    assert '{((first)==0u && (n)==(count))?0u:(table),count,{{first,n},{0,0},{0,0}},pak_name},' in ans
    guest = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    header = (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    assert '#include "pc_mapmaker_creatures.inc"' in guest
    for hook in ('anims', 'request', 'create', 'ai_start'):
        assert f'void pc_mapmaker_creatures_{hook}(void (*original)(void));' in header
        assert f'void pc_mapmaker_creatures_{hook}(void (*original)(void)) {{ original(); }}' in guest
    assert not (ROOT / 'config/guest-code-patches/creatures.json').exists()


# ---- stock facts (skipped without the private XBE) -------------------------

def xbe():
    path = _private() / 'game/default.xbe'
    if not path.is_file():
        pytest.skip('needs the private XBE')
    d = path.read_bytes()
    base = struct.unpack_from('<I', d, 0x104)[0]
    n, sh = struct.unpack_from('<II', d, 0x11C)
    secs = [struct.unpack_from('<5I', d, sh - base + i * 0x38)[1:] for i in range(n)]

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
    return [op[1] for op in i.ops if op[0] == 'imm']


def by_address():
    return {i.address: i for i in decoded()}


def entry(rd, table, k):
    e = rd(table + k * 0x18, 0x18)
    return cstr(rd, struct.unpack_from('<I', e, 0)[0]), struct.unpack_from('<H', e, 0x10)[0], struct.unpack_from('<I', e, 0xC)[0]


HANDLERS = {   # row label -> (dispatch call site in sub_0027D590 / sub_0027F8E0, handler, message handler or None)
    'Prometheus SK/8': (0x0027FAA0, 0x00283730, 0x00283F80),
    'Prometheus SD/7': (0x0027FAA0, 0x00283730, 0x00283F80),
    'Catacomb Ghost': (0x0027D958, 0x002BA300, 0x002BA100),
    'SecuriDroid XP': (0x0027DD06, 0x0026B7A0, 0x0026B5E0),
    'Mutant (Story)': (0x0027DDB2, 0x00264A10, None),
}
ANIMRB = ['botshock', 'bigbotsattackidle', 'bigbotsfirestill', 'bigbotsadvtostand', 'bigbotsadvfire', 'bigbotsstandtoadv',
          'bigbotdeath', 'bigbotdeath2', 'bigbotrocket', 'bigbotrocketadv', 'bigbotrocketadvfire', 'fastbotsadv',
          'fastbotsadvfire', 'fastbotsadvtostand', 'fastbotsattackidle', 'fastbotsawake', 'fastbotsfirestill',
          'fastbotsstandtoadv', 'fastbotsstrafelft', 'fastbotsstraferht', 'fastbotshitreact']
KEPT = {   # label -> anim names of the kept entries
    'Prometheus SK/8': ANIMRB,
    'Prometheus SD/7': ANIMRB,
    'Catacomb Ghost': ['ghostidle', 'ghosttrap', 'ghostattack', 'ghostdeath'],
    'SecuriDroid XP': ['minimovefor', 'minimoveforslow', 'minitostillslow', 'minifrmstillslow', 'minirotlft', 'minirotrht',
                       'mini180', 'minifire', 'ministill', 'minifirefor', 'minideath'],
    'Mutant (Story)': ['mutantwalk', 'mutantelecattack', 'mutantleap', 'zomgrow', 'mutantswipe', 'mutanthitreact'],
    'Pit Skeleton': ['princess_cat_cs', 'princess_low_idle1', 'princess_raise_up', 'princess_pit_fireattack',
                     'princess_pit_jobetheat', 'princess_pit_roar', 'princess_pit_death', 'princess_attack_feeble_throw',
                     'ragbugflight', 'ragbugattack', 'ragbugleftdive', 'ragbugrightdive'],
    'Ragbug': ['ragbugflight', 'ragbugattack', 'ragbugleftdive', 'ragbugrightdive'],
}
# The pit fight's rows run with per-level state and the lv17_4 anims the restart unmaps: both are handled by
# pc_mapmaker_pitboss.inc and proven by test_mapmaker_pitboss.py; the stateless checks below cover the others.
PIT_SHARED = {'princess_low_idle1', 'princess_raise_up', 'princess_attack_feeble_throw'}
PIT_ROWS = {'Pit Skeleton': (0x0027DDD3, 0x0029FE00), 'Ragbug': (0x0027DEDE, 0x0028B610)}


def stateless():
    return [x for x in rows() if x['label'] in HANDLERS]


def test_registration_tables_and_paks():
    """Each row's pak path, its sub_001CA2F0 table/count, the kept entries' names and ids; their slot words lack
    the 0x4000 flag the restart unmaps (sub_0005C2F0), so a restart keeps them mapped."""
    from test_mapmaker_animslots import registrations
    rd, regs = registrations()
    by_pak = {p: (t, n) for _, p, t, n in regs}
    for x in rows():
        path = cstr(rd, x['pak'])
        if x['name'] == 'animrb':
            assert path == 'pak/anim/animrb.pak' and x['first'] == 0 and x['n'] == x['count'], path   # every level's pak, whole
        else:
            assert path == 'pak/stream/%s/anim/%s.pak' % (re.match(r'lv\d+', x['name']).group(0), x['name']), path
        assert by_pak[path] == (x['table'], x['count']), path
        kept = [entry(rd, x['table'], x['first'] + i) for i in range(x['n'])]
        assert [k[0] for k in kept] == KEPT[x['label']]
        assert x['label'] in PIT_ROWS or all(not (k[2] & 0x4000) for k in kept), x['label']
    ghost = [entry(rd, 0x00488850, k)[1] for k in range(30, 34)]
    assert ghost == [0x249, 0x24A, 0x24B, 0x24C]
    assert [entry(rd, 0x00482610, k)[1] for k in (3, 13)] == [0xA22, 0xA2C]
    assert [entry(rd, 0x00480C60, k)[1] for k in (41, 46)] == [0x76B, 0x770]


def test_kept_anims_unique_in_scene_100():
    """No kept id or name is registered by a pak/anim/ table (what every scene 100 level mounts) or by another row's
    or boss pak, so the anim slot budget's first-copy rule never gives a creature another pak's anim."""
    from test_mapmaker_animslots import registrations, table, BOSS_PAKS
    rd, regs = registrations()
    mine = {(x['table'], x['count']): x for x in rows()}
    for x in rows():
        kept = [entry(rd, x['table'], x['first'] + i)[:2] for i in range(x['n'])]
        for _, p, t, c in regs:
            if (t, c) == (x['table'], x['count']):
                continue
            if p.startswith('pak/anim/') or p in BOSS_PAKS or (t, c) in mine:
                for nm, i in table(rd, t, c):
                    if x['label'] == 'Pit Skeleton' and p == 'pak/stream/lv1/anim/lv1_6.pak':
                        # the mansion pak names three of the pit anims too, under its own ids (known, documented
                        # in mapmaker-pitboss.md): only the ids must differ
                        assert all(i != k[1] for k in kept) and (nm not in [k[0] for k in kept] or nm in PIT_SHARED), nm
                        continue
                    assert all(nm != k[0] and i != k[1] for k in kept), (x['label'], p, nm)


def test_classes_reach_their_handlers():
    """sub_0027D590 compares the whole 64-bit class (AI +0x10 lo, +0x14 hi); sub_0015F8C0 strips the 0x10000000
    'active' bit first (0x0015FA3A..0x0015FA49); story mode copies the record at 0x0015B9A3..0x0015B9AB; MapMaker's
    own class is 0x11000000/0 (0x002421A7/0x002421AD)."""
    a = by_address()
    assert a[0x0027D594].text.endswith('[edi + 0x14]') and a[0x0027D59A].text.endswith('[edi + 0x10]')
    assert 0x10000000 in imms(a[0x0015FA3A]) and 0xEFFFFFFF in imms(a[0x0015FA43])
    assert a[0x0015B9A3].ops[1][4] == 0 and a[0x0015B9A5].ops[0][4] == 0x10 and a[0x0015B9AB].ops[0][4] == 0x14
    assert 0x11000000 in imms(a[0x002421A7]) and 0 in imms(a[0x002421AD])
    for x in rows():
        site, handler = PIT_ROWS[x['label']] if x['label'] in PIT_ROWS else HANDLERS[x['label']][:2]
        if x['hi'] == 0 and x['lo'] & 0x1FFF:              # the small classes: sub_0027F8E0 (see the robot test)
            assert a[site].call and target(a[site]) == handler and a[site].function == 0x0027F8E0
            continue
        assert a[site].call and target(a[site]) == handler and a[site].function == 0x0027D590
        value = x['hi'] or x['lo'] & ~0x10000000           # one bit set in one half, the other half 0
        assert bin(value).count('1') == 1 and (x['lo'] & ~0x10000000 == 0 or x['hi'] == 0)
        assert any(i.mnemonic == 'cmp' and value in imms(i) for i in decoded() if i.function == 0x0027D590)
    # the compares that lead to the three calls (hi in ecx, lo in eax)
    assert 'ecx, 0x4000' in a[0x0027DABF].text and a[0x0027DAC5].text.endswith('0x27dcef')
    assert 'ecx, 0x8000' in a[0x0027DD64].text
    assert 'eax, 0x40000000' in a[0x0027D825].text and a[0x0027D82E].text.endswith('0x27d947')


def reach(roots, max_callers=4):
    calls, callers, body = defaultdict(set), defaultdict(set), defaultdict(list)
    for i in decoded():
        body[i.function].append(i)
        if (i.call or i.mnemonic == 'jmp') and not i.frame():
            t = target(i)
            if t is not None:
                calls[i.function].add(t)
                callers[t].add(i.function)
    seen, todo = set(roots), list(roots)
    while todo:
        f = todo.pop()
        for t in calls[f]:
            if t in body and t not in seen and len(callers[t]) <= max_callers:
                seen.add(t)
                todo.append(t)
    return seen, body


SCENE_STATE = [   # per-scene state sub_0025F720 allocates or level init fills, and the boss blocks
    (0x0078B080, 0x0078B200),   # boss particle/effect statics and boss pointers
    (0x0078B2E0, 0x0078B300),   # alarm table (scene 6/4)
    (0x00772B50, 0x00772B54),   # rail-bot pool (scene 22)
    (0x0076C270, 0x0076C274),   # wall-panel terminal pool (scenes 6/17/22)
]


def test_handlers_read_no_scene_state():
    """From each handler (and its message handler), through callees with at most four callers: the only scene test
    is sub_00228B40 (scene 100: MapMaker's own kill marker, sub_0033E560); no boss block, alarm, rail-bot or wall-panel
    pool is touched."""
    for label, (_, handler, message) in HANDLERS.items():
        seen, body = reach([handler] + ([message] if message else []))
        for f in seen:
            for i in body[f]:
                for op in i.ops:
                    if op[0] != 'mem' or op[1] is not None or op[2] is not None:
                        continue
                    if op[4] == 0x0079094C:
                        assert f == 0x00228B40, (label, hex(f), hex(i.address))
                    assert not any(lo <= op[4] < hi for lo, hi in SCENE_STATE), (label, hex(f), hex(i.address))
    a = by_address()
    assert 0x64 in imms(a[0x00228B40]) and target(a[0x00228B49]) == 0x0033E560


def test_handlers_play_only_kept_anims():
    """Every id of the row's pak table that its handlers use is a kept entry (and the ghost/securibot/mutant play
    their own: 0x249..0x24B, 0xA22..0xA2B, 0x76B..0x76F)."""
    from test_mapmaker_animslots import registrations, table
    rd, _ = registrations()
    for x in stateless():
        _, handler, message = HANDLERS[x['label']]
        seen, body = reach([handler] + ([message] if message else []))
        ids = {i: k for k, (_, i) in enumerate(table(rd, x['table'], x['count']))}
        used = {ids[v] for f in seen for i in body[f] for v in imms(i) if v in ids}
        keep = set(range(x['first'], x['first'] + x['n']))
        assert used and used <= keep, (x['label'], sorted(used - keep))


def test_carriers_and_looks():
    """The ids the rows store have no Arcade row (0x00521A18, 150 x 0x30, +4 id), are in no story set (0x0053D2D0,
    27 x 0xA4) except the securibot's own scene 6 set, are not used by another port, and no game code tests them as
    a character (+0x48) except the securibot's collision radius (0x000A3BB9/0x000A4434). The looks' models are the
    zombie's and the mutant's (character table +4/+8); sub_000DAC30 reads +4 and +8."""
    rd = xbe()
    arcade = {struct.unpack_from('<I', rd(0x00521A18 + i * 0x30 + 4, 4))[0] for i in range(150)}
    sets = defaultdict(set)
    for s in range(27):
        for k in range(20):
            cid = struct.unpack_from('<h', rd(0x0053D2D0 + s * 0xA4 + 4 + k * 8, 2))[0]
            if cid >= 0:
                sets[cid].add(s)
    model = lambda cid, o: struct.unpack_from('<i', rd(0x0053A3CC + cid * 0x30 + o, 4))[0]
    r = {x['label']: x for x in rows()}
    for x in rows():
        assert x['id'] not in arcade, x['label']
        assert x['preview'] in arcade, x['label']
    assert not sets[0x2F] and not sets[0x30] and sets[0x4D] == {3} and sets[0x62] == {10} and sets[0x36] == {7}
    assert (model(0x2F, 4), model(0x2F, 8)) == (0x619, -1) and (model(0x30, 4), model(0x30, 8)) == (0x619, -1)
    assert (model(0x17, 4), model(0x17, 8)) == (0x5C8, 0x5C9) and (model(0x59, 4), model(0x59, 8)) == (0x613, 0x614)
    assert model(0x4D, 4) == 0x607 and r['SecuriDroid XP']['look'] == 0xFF
    assert r['Catacomb Ghost']['look'] == 0x17 and r['Mutant (Story)']['look'] == 0x59
    used = set()
    for p in (ROOT / 'src/interface').glob('pc_mapmaker_*.inc'):
        used |= {int(m, 16) for m in re.findall(r'#define \w+_ID 0x([0-9A-F]+)u', p.read_text(encoding='utf-8'))}
    assert used.isdisjoint({0x2F, 0x30}) and 0x4D not in used
    for i in decoded():
        if i.mnemonic == 'cmp' and len(i.ops) == 2 and i.ops[0][0] == 'mem' and i.ops[0][4] == 0x48 and i.ops[1][0] == 'imm':
            assert i.ops[1][1] not in (0x2F, 0x30), hex(i.address)
            if i.ops[1][1] == 0x4D:
                assert i.function in (0x000A3AE0, 0x000A4330), hex(i.address)
    a = by_address()
    body = [i for i in decoded() if i.function == 0x000DAC30]
    disps = {op[4] for i in body for op in i.ops if op[0] == 'mem'}
    assert 0x0053A3D0 in disps and 0x0053A3D4 in disps


def story_records(pak, tags):
    """Spawn records (0x98 B: +0 class lo, +4 hi, +8 tag, +0x16 set slot) of a story level pak, by tag."""
    raw = pak.read_bytes()
    off, ln = struct.unpack_from('<II', raw, 4)
    out = {}
    for _, p, n, _ in struct.iter_unpack('<4I', raw[off:off + ln]):
        d = raw[p:p + n]
        for t in tags:
            pat = t.encode()[::-1]
            i = d.find(pat)
            while i >= 0:
                if i >= 8 and (i - 8) % 4 == 0:
                    lo, hi = struct.unpack_from('<II', d, i - 8)
                    slot = struct.unpack_from('<h', d, i - 8 + 0x16)[0]
                    if lo >> 28 in (1, 5) and 0 <= slot < 20:
                        out.setdefault(t, set()).add((lo, hi, slot))
                i = d.find(pat, i + 1)
    return out


def test_story_levels_spawn_these_classes():
    """The story levels spawn exactly these: catacombs (scene 17, set 5) GHT1..GHT7 are zombies with 0:0x50000000;
    mission 7 (scene 6, set 3) RS01/RS02/CR01 are the securibot 0x4D with 0x4000:0x10000000; mission 8 (scene 22,
    set 4) MT06..MT19 are the mutant 0x59 with 0x8000:0x10000000."""
    rd = xbe()
    story = _private() / 'game/pak/story'
    if not (story / 'l_17_st.pak').is_file():
        pytest.skip('needs the story paks')
    member = lambda s, k: struct.unpack_from('<h', rd(0x0053D2D0 + s * 0xA4 + 4 + k * 8, 2))[0]
    r = {x['label']: x for x in rows()}
    ghosts = story_records(story / 'l_17_st.pak', ['GHT%d' % k for k in range(1, 8)])
    assert len(ghosts) == 7
    for recs in ghosts.values():
        for lo, hi, slot in recs:
            assert (lo, hi) == (r['Catacomb Ghost']['lo'], r['Catacomb Ghost']['hi'])
            assert member(5, slot) in (0x17, 0x18, 0x23, 0x2A)
    bots = story_records(story / 'l_6_st.pak', ['RS01', 'RS02', 'CR01'])
    assert len(bots) == 3
    for recs in bots.values():
        assert recs == {(r['SecuriDroid XP']['lo'], r['SecuriDroid XP']['hi'], 2)} and member(3, 2) == 0x4D
    mutants = story_records(story / 'l_22_st.pak', ['MT%02d' % k for k in range(6, 20)])
    assert len(mutants) >= 10
    for recs in mutants.values():
        for lo, hi, slot in recs:
            assert (lo, hi) == (r['Mutant (Story)']['lo'], r['Mutant (Story)']['hi']) and member(4, slot) == 0x59


# ---- the Prometheus robots (v0.4.0-mapmaker.58 report: SK/8 loops its rocket pose, fires nothing) ----

def test_robot_class_4_path():
    """Story mode: an AI class with lo & 0x1FFF goes to sub_0027F8E0 (sub_0027FE30, 0x0027FE56), whose jump table
    (bytes 0x0027FE04, targets 0x0027FDE8) sends class 4 to 0x0027F9C4; a robot (sub_000D6DD0 at 0x0027FA37) with a
    target runs sub_00283730 (0x0027FAA0); in scene 100 (0x0027FA43) one with no built-in weapon (+0x94), no +0x2BC
    and no usable weapon (sub_0027D510) the unarmed attack sub_0027E4E0 (0x0027FA86). Class 4 messages:
    sub_0027CEF0 sends a robot to sub_00283F80 (0x0027D05C)."""
    rd = xbe()
    a = by_address()
    assert 0x1FFF in imms(a[0x0027FE56]) and target(a[0x0027FE68]) == 0x0027F8E0
    bt, tb = rd(0x0027FE04, 0x20), struct.unpack('<7I', rd(0x0027FDE8, 28))
    assert tb[bt[4 - 1]] == 0x0027F9C4
    assert target(a[0x0027FA37]) == 0x000D6DD0 and target(a[0x0027FAA0]) == 0x00283730
    assert 0x64 in imms(a[0x0027FA43]) and a[0x0027FA4C].ops[1][4] == 0x94 and a[0x0027FA56].ops[1][4] == 0x2BC
    assert target(a[0x0027FA61]) == 0x0027D510 and target(a[0x0027FA86]) == 0x0027E4E0
    assert target(a[0x0027D040]) == 0x000D6DD0 and target(a[0x0027D05C]) == 0x00283F80
    for rid in (0x62, 0x36):
        assert rd(0x000D6E00 + rid - 0x12, 1)[0] == 0                      # in the robot list


def test_bot_class_never_reaches_the_robot_code():
    """The root cause: MapMaker's class 0x01000000 runs sub_0026EFC0 (0x0027D8B6); nothing it reaches through
    callees with at most six callers calls the robot list or the robot handler, so the robot never fires its
    built-in weapon."""
    a = by_address()
    assert 0x1000000 in imms(a[0x0027D854]) and target(a[0x0027D8B6]) == 0x0026EFC0
    seen, body = reach([0x0026EFC0], max_callers=6)
    for f in seen:
        for i in body[f]:
            if i.call:
                assert target(i) not in (0x000D6DD0, 0x00283730, 0x0027E4E0, 0x0027F8E0), (hex(f), hex(i.address))


def test_rocket_robot_built_in_weapon():
    """The AI start gives 0x62 the built-in weapon 0x43 (+0x94, 0x0015B927, its ammo from weapon 0x43's record)
    when the spawn record has no weapon list (+0x18 -1, 0x0015B87C); MapMaker writes -1 there (0x00242155)."""
    a = by_address()
    assert a[0x0015B878].ops[1][4] == 0x18
    assert 0x62 in imms(a[0x0015B921]) and 0x43 in imms(a[0x0015B927]) and a[0x0015B927].ops[0][4] == 0x94
    assert a[0x00242155].ops[0][4] == 0x18 and a[0x00242155].function == 0x00240C50


def test_animrb_mounted_in_every_mapmaker_level():
    """animrb.pak ("pak/anim/animrb.pak", 0x0047E65C) is registered whole by sub_001CA2F0 (0x001CB133..0x001CB151)
    whenever it is mounted; the robot handler reads only animrb's anim ids (table 0x00520840)."""
    from test_mapmaker_animslots import registrations
    rd, regs = registrations()
    assert cstr(rd, 0x0047E65C) == 'pak/anim/animrb.pak'
    assert ('pak/anim/animrb.pak', 0x0048F240, 0x15) in [(p, t, n) for _, p, t, n in regs]
    a = by_address()
    assert 0x0047E65C in imms(a[0x001CB133])
    ids = {struct.unpack_from('<I', rd(0x00520840 + k * 4, 4))[0] for k in range(16)} - {0xFFFFFFFF}
    animrb = {struct.unpack_from('<H', rd(0x0048F240 + k * 0x18 + 0x10, 2))[0] for k in range(0x15)}
    assert ids and ids <= animrb


def test_story_robots_spawn_class_4():
    """Mission 9 (scene 5, set 10) spawns 0x62 and mission 10 (scene 14, set 7) 0x36 with 0:0x10000004 and no
    weapon (+0x14 -1), e.g. SB06/PR01/GB04 and DW03/TC03."""
    rd = xbe()
    story = _private() / 'game/pak/story'
    if not (story / 'l_5_st.pak').is_file():
        pytest.skip('needs the story paks')
    member = lambda s, k: struct.unpack_from('<h', rd(0x0053D2D0 + s * 0xA4 + 4 + k * 8, 2))[0]
    for pak, setno, rid, tags in (('l_5_st.pak', 10, 0x62, ['SB06', 'PR01', 'GB04']), ('l_14_st.pak', 7, 0x36, ['DW03', 'TC03'])):
        recs = story_records(story / pak, tags)
        assert len(recs) == len(tags), pak
        for r in recs.values():
            assert all((lo, hi) == (0x10000004, 0) and member(setno, slot) == rid for lo, hi, slot in r), (pak, r)
