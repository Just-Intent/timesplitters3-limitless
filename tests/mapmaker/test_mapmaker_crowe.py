"""MapMaker real story boss: the split Crowe of story mission 10 as a Story AI.

Compiles src/interface/pc_mapmaker_crowe.inc against fake guest memory (with
mutations that must be caught) and, when the private XBE, its disassembly and
the game paks are available, checks the stock facts the hooks rely on: the
character data, the anim pak and its names, the class 0x40000 dispatch and
message handler, the scene 14 state blocks, the arena grid and its floor
constants, the intro wait, the turret prop and its pak, the level marker, the
frame draw site, the bar and meter sites, and that no other level event or tag
lookup is reachable from the fight.
"""
import os
from pathlib import Path
import re
import struct
import subprocess
import zlib
import pytest

from native_compiler import native_compiler
from mapmaker_decoded import decoded, _private

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_crowe.inc'

MUTATIONS = {
    'always-on': ('    return mapmaker_limits.limits;\n}', '    return 1;\n}'),
    'any-mode': ('WORD_AT(CRW_SCENE)==100u && CRW_B(CRW_MODE)==10u; }', 'WORD_AT(CRW_SCENE)==100u; }'),
    'any-scene': ('crw_on() && WORD_AT(CRW_SCENE)==100u && CRW_B(CRW_MODE)==10u; }', 'crw_on() && CRW_B(CRW_MODE)==10u; }'),
    'set-any-index': ('    if(CRW_H(info+0x14u)!=CRW_SET_MAP) return 0;\n', ''),
    'set-skips-extra': ('    for(k=0;extra && k<CRW_B(extra+3u);k++)', '    for(k=0;0 && extra && k<CRW_B(extra+3u);k++)'),
    'set-any-id': ('if(CRW_H(info+0x18u+k*4u)==CRW_ID) return 1;', 'if(CRW_H(info+0x18u+k*4u)!=0xFFFFu) return 1;'),
    'set-extra-offset': ('if(CRW_H(extra+4u+k*4u)==CRW_ID) return 1;', 'if(CRW_H(extra+2u+k*4u)==CRW_ID) return 1;'),
    'set-unreadable-none': ('WORD_AT(container)!=CRW_MAGIC) return -1;\n    c=container+8u;end=container+CRW_SCAN;\n    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {\n        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u),d=c+8u;',
                            'WORD_AT(container)!=CRW_MAGIC) return 0;\n    c=container+8u;end=container+CRW_SCAN;\n    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {\n        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u),d=c+8u;'),
    'wrong-pak': ('#define CRW_PAK 0x0049DC78u', '#define CRW_PAK 0x0049DC58u'),
    'mount-twice': ('(has=crw_set_has())==0 || crw_pak_mounted()) return;', '(has=crw_set_has())==0) return;'),
    'mount-fills-table': ('#define CRW_MOUNT_KEEP 8u', '#define CRW_MOUNT_KEEP 0u'),
    'mount-ignores-optout': ('    if(!crw_play() || crw.optout || (has=crw_set_has())==0 || crw_pak_mounted()) return;',
                             '    if(!crw_play() || (has=crw_set_has())==0 || crw_pak_mounted()) return;'),
    'level-any-map': ('    if(!crw_play() || crw.optout || (has=crw_set_has())==0) return;\n    WORD_AT(CRW_STATE)=0;',
                      '    if(!crw_play() || crw.optout || (has=1)==0) return;\n    WORD_AT(CRW_STATE)=0;'),
    'level-ignores-optout': ('    if(!crw_play() || crw.optout || (has=crw_set_has())==0) return;\n    WORD_AT(CRW_STATE)=0;',
                             '    if(!crw_play() || (has=crw_set_has())==0) return;\n    WORD_AT(CRW_STATE)=0;'),
    'level-keeps': ('    crw_reset();                                       /* a new level', '    (void)0;                                       /* a new level'),
    'level-stale-state': ('    WORD_AT(CRW_STATE)=0;                              /* a stale pointer', '    /* a stale pointer'),
    'level-no-particles': ('    guest_call(sub_002A62C0,0);\n', ''),
    'level-particles-lost': ('    for(j=0;j<CRW_NWIN;j++) WORD_AT(crw_win[j])=live[j];\n', ''),
    'level-no-alloc': ('    guest_call(sub_0028D6D0,0);\n', ''),
    'class-value': ('#define CRW_CLASS 0x40000u', '#define CRW_CLASS 0x80000u'),
    'class-after-original': ('        WORD_AT(rec)=0;WORD_AT(rec+4u)=CRW_CLASS;\n',
                             '        original();WORD_AT(rec)=0;WORD_AT(rec+4u)=CRW_CLASS;g_esp-=4u;\n'),
    'class-without-pak': ('    else if(!crw_pak_mounted()) crw_log("lv14', '    else if(0) crw_log("lv14'),
    'class-without-state': ('    else if(!crw_n) crw_log(', '    else if(0) crw_log('),
    'class-two': ('    else if(k==crw_n && crw_n>=CRW_MAX) {', '    else if(k==crw_n && crw_n>=1u) {'),
    'class-ignores-optout': ('    if(crw.optout) crw_log("TSFP_MAPMAKER_REAL_BOSS=0', '    if(0) crw_log("TSFP_MAPMAKER_REAL_BOSS=0'),
    'class-any-character': ('    if(CRW_H(a+CRW_ACTOR_CHAR)!=CRW_ID && !(MAPMAKER_PTR_OK(ent,0x100u) && CRW_H(ent+CRW_ENT_MODEL)==CRW_MODEL)) { original();return; }\n', ''),
    'boss-flag-ignored': ('            if((f&0xF8u)!=0xB0u) return 0;', '            return 0;'),
    'sub-is-bar': ('return (uint8_t)((f&4u)?CRW_BOSS_SUB:CRW_BOSS_BAR);', 'return (uint8_t)CRW_BOSS_BAR;'),
    'boss-without-area': ('crw_inst[k].boss=pc_mapmaker_area_enabled("bosses","pc_mapmaker_bosses_hud")?', 'crw_inst[k].boss=1?'),
    'boss-any-scene': ('    return k>=0 && WORD_AT(CRW_SCENE)==100u && crw_on()?k:-1;', '    return k>=0 && crw_on()?k:-1;'),
    'boss-any-entity': ('    for(k=0;k<crw_n;k++) if(crw_inst[k].record && crw_inst[k].record==rec && crw_inst[k].state) return (int)k;', '    for(k=0;k<crw_n;k++) if(crw_inst[k].state) return (int)k;'),
    'relocate-none': ('&& !crw_lv.relocated) crw_relocate(ent);', '&& !crw_lv.relocated) {}'),
    'relocate-twice': ('    if(WORD_AT(a+CRW_AI_STATE)==0u && !crw_lv.relocated) crw_relocate(ent);', '    if(WORD_AT(a+CRW_AI_STATE)==0u) crw_relocate(ent);'),
    'relocate-any-cell': ('            if(WORD_AT(c+0x20u)!=0) continue;              /* type 0', '            /* type 0'),
    'relocate-farthest': ('if(bd<0.0f || d<bd) { bd=d;best=c;row=', 'if(bd<0.0f || d>bd) { bd=d;best=c;row='),
    'relocate-size-moved': ('fx[3]={0u,8u,0x10u}', 'fx[3]={0u,8u,0x18u}'),
    'relocate-z-as-x': ('fz[3]={4u,0xCu,0x14u}', 'fz[3]={4u,0xCu,0x10u}'),
    'start-nearest-centre': ('    if(!(best=crw_start_cell(g,oz,&row,&col))) {', '    if(1) {'),
    'start-any-type': ('            t=WORD_AT(cell+0x20u)==0;d=', '            t=1;d='),
    'start-farthest': ('(t==bt && d<bd)) { best=cell;', '(t==bt && d>bd)) { best=cell;'),
    'start-x-wrong': ('#define CRW_START_X 310.0f', '#define CRW_START_X 300.0f'),
    'place-none': ('    if(placing) crw_place(ent);\n', ''),
    'place-only-x': ('crw_setf(ent+CRW_ENT_X,crw_lv.sx);crw_setf(ent+CRW_ENT_X+8u,crw_lv.sz);', 'crw_setf(ent+CRW_ENT_X,crw_lv.sx);'),
    'place-keeps-index': ('    WORD_AT(st+CRW_START_ROW)=crw_lv.row;WORD_AT(st+CRW_START_COL)=crw_lv.col;\n', ''),
    'place-flag-kept': ('    crw_lv.place=0;\n    if(WORD_AT(st+CRW_START_ROW)', '    if(WORD_AT(st+CRW_START_ROW)'),
    'cell-row-by-x': ('        if(!(z>crw_getf(cell+4u) && crw_getf(cell+0xCu)>z)) continue;', '        if(!(x>crw_getf(cell) && crw_getf(cell+8u)>x)) continue;'),
    'relocate-skips-edges': ('    for(k=0;k<CRW_CELLS+1u+CRW_EDGES;k++) {', '    for(k=0;k<CRW_CELLS+1u;k++) {'),
    'relocate-skips-outer': ('k==CRW_CELLS?o:st+CRW_EDGE', 'k==CRW_CELLS?st+CRW_EDGE+0x400u:st+CRW_EDGE'),
    'relocate-unused-edges': ('        if(!WORD_AT(c) && !WORD_AT(c+8u)) continue;        /* an unused edge entry */\n', ''),
    'relocate-nan': ('    if(!crw_sane(sx) || !crw_sane(sy) || !crw_sane(sz) || !crw_sane(ox) || !crw_sane(oz)) {',
                     '    if(!crw_sane(sy) || !crw_sane(ox) || !crw_sane(oz)) {'),
    'floor-wrong': ('#define CRW_FLOOR 86.0f', '#define CRW_FLOOR 0.0f'),
    'shift-none': ('    crw_shift(1);\n    original();\n    crw_shift(0);', '    original();\n    crw_shift(0);'),
    'shift-not-restored': ('    crw_shift(1);\n    original();\n    crw_shift(0);', '    crw_shift(1);\n    original();'),
    'shift-ignores-stock-bits': ('            if(crw_lv.saved[k]!=crw_consts[k].bits) continue;\n', ''),
    'shift-death-z-as-x': ('    {0x0049F3C8u,0x42A1E666u,2},', '    {0x0049F3C8u,0x42A1E666u,0},'),
    'shift-missing-hazard': ('    {0x0049F4CCu,0x42AC68F6u,1},', '    {0x0049F4CCu,0x42AC68F6u,7},'),
    'shift-wrong-bits': ('    {0x0049F3C4u,0x42AC0000u,1},', '    {0x0049F3C4u,0x42AD0000u,1},'),
    'near-not-restored': ('    WORD_AT(crw_consts[0].addr)=crw_lv.saved[0];\n    original();', '    original();'),
    'near-not-reshifted': ('    original();\n    WORD_AT(crw_consts[0].addr)=v;\n}', '    original();\n    WORD_AT(crw_consts[0].addr)=crw_lv.saved[0];\n}'),
    'sparks-no-shift': ('    crw_setf(slot,crw_getf(slot)+crw_lv.dy);\n', ''),
    'sparks-slot-left': ('    WORD_AT(slot)=v;                                   /* the caller', '    (void)v;                                   /* the caller'),
    'intro-kept': ('        WORD_AT(a+0x9D4u)=1u;WORD_AT(a+CRW_AI_STATE)=2u;', '        WORD_AT(a+0x9D4u)=1u;'),
    'intro-next-stale': ('WORD_AT(a+CRW_AI_STATE)=2u;WORD_AT(a+0x9D0u)=2u;', 'WORD_AT(a+CRW_AI_STATE)=2u;'),
    'multi-no-swap': ('    prev=crw_enter(k);\n    crw_fight(ent,original);', '    prev=crw_cur;crw_i=&crw_inst[k];\n    crw_fight(ent,original);'),
    'multi-not-left': ('    crw_fight(ent,original);\n    crw_leave(prev);', '    crw_fight(ent,original);'),
    'multi-nested-lost': ('    int prev=crw_cur;\n    if(prev==k) return prev;\n    crw_store(prev);', '    int prev=crw_cur;\n    if(prev==k) return prev;'),
    'multi-leave-no-store': ('    if(crw_cur==prev) return;\n    crw_store(crw_cur);', '    if(crw_cur==prev) return;'),
    'multi-window-short': ('0x0078B180u,0x0078B184u,0x0078B188u,\n};', '0x0078B180u,0x0078B184u,\n};'),
    'multi-no-state-window': ('    CRW_STATE,\n    0x0078B080u,', '    0x0078B080u,'),
    'multi-alloc-keeps-globals': ('    for(j=0;j<CRW_NWIN;j++) WORD_AT(crw_win[j])=live[j];\n    if(ok) crw_log(', '    if(ok) crw_log('),
    'multi-alloc-not-captured': ('        for(j=0;j<CRW_NWIN;j++) crw_inst[k].win[j]=WORD_AT(crw_win[j]);\n', ''),
    'multi-no-extra-alloc': ('    } else if(k==crw_n && !crw_alloc(k)) {', '    } else if(k==crw_n) {'),
    'multi-reuse-slot-lost': ('    if(k==crw_n) for(k=0;k<crw_n && crw_inst[k].record;k++) {}   /* a slot allocated but unused */\n', ''),
    'multi-count-kept': ('        if(k==crw_n) crw_n++;\n', ''),
    'multi-cap-high': ('#define CRW_MAX 4u', '#define CRW_MAX 5u'),
    'multi-draw-unswapped': ('        prev=crw_enter((int)k);\n        guest_call(sub_0028D710,0);', '        prev=crw_cur;\n        guest_call(sub_0028D710,0);'),
    'message-unswapped': ('    crw_entry(WORD_AT(g_esp+4u),original);\n}\n/* WRAP sub_0028DA10', '    original();\n}\n/* WRAP sub_0028DA10'),
    'think-unswapped': ('    crw_entry(WORD_AT(g_esp+4u),original);\n}\n/* WRAP sub_0028F730', '    original();\n}\n/* WRAP sub_0028F730'),
    'hit-unswapped': ('    crw_entry(WORD_AT(info),original);', '    original();'),
    'hit-attacker': ('    crw_entry(WORD_AT(info),original);', '    crw_entry(WORD_AT(info+4u),original);'),
    'entry-not-left': ('    prev=crw_enter(k);\n    original();\n    crw_leave(prev);', '    prev=crw_enter(k);\n    original();'),
    'remove-missing': ('        guest_call(sub_0015DDA0,1,ent);\n', ''),
    'remove-twice': ('        crw_lv.removed=1;\n', ''),
    'remove-at-once': ('        if(WORD_AT(CRW_LIVE) && crw_lv.fading<CRW_FADE_MAX) return;\n', ''),
    'remove-never': ('        if(WORD_AT(CRW_LIVE) && crw_lv.fading<CRW_FADE_MAX) return;', '        if(1) return;'),
    'remove-no-cap': ('        if(WORD_AT(CRW_LIVE) && crw_lv.fading<CRW_FADE_MAX) return;', '        if(WORD_AT(CRW_LIVE)) return;'),
    'death-smokes-on': ('        WORD_AT(crw_lv.state+CRW_PARTS_ON)=0;\n', ''),
    'death-parts-lost': ('        WORD_AT(crw_lv.state+CRW_PARTS_ON)=parts;\n', ''),
    'death-no-original': ('        crw_shift(1);\n        original();\n        crw_shift(0);\n        WORD_AT(crw_lv.state+CRW_PARTS_ON)=parts;', '        MAPMAKER_GUEST_RET();\n        WORD_AT(crw_lv.state+CRW_PARTS_ON)=parts;'),
    'death-unshifted': ('        crw_shift(1);\n        original();\n        crw_shift(0);\n        WORD_AT(crw_lv.state+CRW_PARTS_ON)=parts;', '        original();\n        WORD_AT(crw_lv.state+CRW_PARTS_ON)=parts;'),
    'fx-after-removal': ('        if(!crw_inst[k].state || !crw_inst[k].record || crw_inst[k].removed) continue;', '        if(!crw_inst[k].state || !crw_inst[k].record) continue;'),
    'remove-focus-kept': ('WORD_AT(player+CRW_FOCUS)==ent) WORD_AT(player+CRW_FOCUS)=0;', 'WORD_AT(player+CRW_FOCUS)==ent) {}'),
    'part-original': ('        guest_call(sub_000EFBD0,8,', '        original();g_esp-=4u;guest_call(sub_000EFBD0,8,'),
    'part-position': ('crw_lv.state+CRW_SHOT+part*CRW_PART_STRIDE', 'crw_lv.state+CRW_SHOT'),
    'part-effect': ('guest_call(sub_000EFBD0,8,0x20u,0x17u,', 'guest_call(sub_000EFBD0,8,0x22u,0x17u,'),
    'part-bound': ('    if(part<CRW_PART_MAX)\n', '    if(part<8u)\n'),
    'part-any-caller': ('WORD_AT(g_esp)!=CRW_RET_PART || !crw_state_ok()', '!crw_state_ok()'),
    'part-any-scene': (' || !crw_state_ok() ||\n       WORD_AT(CRW_SCENE)!=100u || !crw_on()) { original();return; }', ' || !crw_state_ok() ||\n       !crw_on()) { original();return; }'),
    'event-kept': ('(MapMaker logic marker untouched)",WORD_AT(g_esp+4u),0);\n    MAPMAKER_GUEST_RET();', '(MapMaker logic marker untouched)",WORD_AT(g_esp+4u),0);\n    original();'),
    'event-any-caller': ('WORD_AT(g_esp)!=CRW_RET_EVENT || WORD_AT(CRW_SCENE)!=100u', 'WORD_AT(CRW_SCENE)!=100u'),
    'event-any-scene': ('WORD_AT(g_esp)!=CRW_RET_EVENT || WORD_AT(CRW_SCENE)!=100u || !crw_on()', 'WORD_AT(g_esp)!=CRW_RET_EVENT || !crw_on()'),
    'event-without-boss': ('    if(!crw_lv.record || !MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=CRW_RET_EVENT', '    if(!MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=CRW_RET_EVENT'),
    'fx-missing': ('    guest_call(sub_0028D710,0);\n', ''),
    'fx-any-caller': ('MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==CRW_RET_FX,prev;', 'MAPMAKER_STACK_OK(4),prev;'),
    'fx-stale': ('    if(!here || WORD_AT(CRW_SCENE)!=100u || !crw_on()) return;\n    for(k=0;k<crw_n;k++) {', '    if(!here) return;\n    for(k=0;k<crw_n;k++) {'),
    'hud-keeps-cleared': ('    original();\n    WORD_AT(player+CRW_FOCUS)=focus;\n', '    original();\n'),
    'hud-any-entity': ('       CRW_H(focus+CRW_ENT_MODEL)!=CRW_MODEL || crw_find(focus)<0 || !crw_play()) { original();return; }', '       CRW_H(focus+CRW_ENT_MODEL)!=CRW_MODEL || !crw_play()) { original();return; }'),
    'hud-stock-bar': ('    WORD_AT(player+CRW_FOCUS)=0;\n    original();', '    original();'),
    'meter-shown': ('    if(!crw.meters++) crw_log("overhead health meter hidden on the split Crowe",a,0);\n    MAPMAKER_GUEST_RET();',
                    '    if(!crw.meters++) crw_log("overhead health meter hidden on the split Crowe",a,0);\n    original();'),
    'meter-sub-hidden': ('    if(k==(int)crw_n || crw_inst[k].boss==CRW_BOSS_SUB || !crw_play())', '    if(k==(int)crw_n || !crw_play())'),
    'meter-any-caller': ('WORD_AT(g_esp)!=CRW_RET_METER) { original();return; }', '0) { original();return; }'),
    'meter-any-actor': ('(!crw_inst[k].record || crw_inst[k].record!=WORD_AT(a+CRW_ACTOR_RECORD));k++) {}', '0;k++) {}'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('crowe.exe' if os.name == 'nt' else 'crowe')
    source = ROOT / 'tests/mapmaker/mapmaker_crowe.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/wd4706', '/wd4505',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src'),
                str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Wno-unused-function', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), '-I' + str(ROOT / 'src'),
                str(source), '-o', str(exe), '-lm']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_crowe(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_REAL_BOSS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker crowe ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_no_guest_code_table_and_no_save_byte():
    """Wrappers only; no Story AI chunk byte of its own (+0x29 belongs to bosses)."""
    assert not (ROOT / 'config/guest-code-patches/crowe.json').exists()
    text = INC.read_text(encoding='utf-8')
    assert not re.findall(r'CRW_B\([^)]*\)\s*=[^=]', text)


# ---- stock facts (skipped without the private XBE) --------------------------

def by_address():
    return {i.address: i for i in decoded()}


def mem(i):
    return [op for op in i.ops if op[0] == 'mem']


def ends(a, site):
    return site + len(bytes.fromhex(a[site].hex))


def target(i):
    m = re.search(r'0x([0-9a-f]+)', i.text)
    return int(m.group(1), 16) if m else None


def calls_to(f):
    return {i.address for i in decoded() if i.call and target(i) == f}


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


def anim_table(rd, va, n):
    out = {}
    for k in range(n):
        e = rd(va + k * 0x18, 0x18)
        name = rd(struct.unpack_from('<I', e, 0)[0], 40).split(b'\0')[0].decode()
        out[name] = struct.unpack_from('<H', e, 0x10)[0]
    return out


def test_character_and_anims():
    rd = xbe_reader()
    voice, model, lod = struct.unpack_from('<3I', rd(0x0053A3CC + 0x46 * 0x30, 12), 0)
    assert (voice, model, lod) == (0x00539948, 0x5E8, 0xFFFFFFFF)
    ids = [struct.unpack_from('<I', rd(0x00521A18 + i * 0x30 + 4, 4))[0] for i in range(150)]
    assert 0x46 not in ids and ids.index(0x1E) == 14                 # no Arcade row; Old Crow, the stand-in
    # sub_002BE270(0x46) == 0: byte table 0x002BE29C (ids 0x0A..0x63) -> jump table 0x002BE294 entry 1
    assert rd(0x002BE29C + 0x46 - 0x0A, 1)[0] == 1
    assert struct.unpack('<2I', rd(0x002BE294, 8)) == (0x002BE28A, 0x002BE290)
    assert rd(0x0049DC78, 30) == b'pak/stream/lv14/anim/lv14.pak\0'
    lv14 = anim_table(rd, 0x0047E938, 0x29)
    crowe = {k: v for k, v in lv14.items() if k.startswith('splitcrowe')}
    assert crowe == {'splitcrowewalk': 0x80E, 'splitcrowestandtowalk': 0x80F, 'splitcrowewalktostand': 0x810,
                     'splitcrowetantrum': 0x811, 'splitcrowe90turn': 0x812, 'splitcrowefirestill': 0x813,
                     'splitcrowe90turnback': 0x814, 'splitcrowedeath': 0x815}
    others = set()
    for va, n in ((0x00484F68, 0x14), (0x0048C738, 0x12), (0x0048C1B0, 0x10)):   # lv81, lv1_6, lv1_3
        others |= set(anim_table(rd, va, n).values())
    assert not set(lv14.values()) & others                           # all may be mounted together
    # sub_0025F720: scene 14 reaches the 0x0025F760 case
    jt = [struct.unpack_from('<I', rd(0x0025F7C0 + 4 * k, 4))[0] for k in range(6)]
    assert jt[rd(0x0025F7D8 + 13, 1)[0]] == 0x0025F760
    # the fight's state jump table (0x002923DC, states 0..0x11)
    states = [struct.unpack_from('<I', rd(0x002923DC + 4 * k, 4))[0] for k in range(18)]
    assert states[0] == 0x00290D9F and states[1] == 0x00290FDF and states[2] == 0x0029100C
    assert states[0x11] == 0x00292211 and states[0x0C] == states[0x10] == 0x002923CE
    # the shifted constants and their stock bits
    bits = {0x0049F3C4: 0x42AC0000, 0x0049F4CC: 0x42AC68F6, 0x0049F4D0: 0x42B08000, 0x0049F4D4: 0x42B0CCCD,
            0x0049F3CC: 0x439B3EB8, 0x0049F3C8: 0x42A1E666, 0x0049F3D8: 0x439B0000}
    text = INC.read_text(encoding='utf-8')
    for va, b in bits.items():
        assert struct.unpack('<I', rd(va, 4))[0] == b
        assert f'{{0x{va:08X}u,0x{b:08X}u,' in text


def test_anim_registration_and_mount():
    a = by_address()
    assert ('imm', 0x0049DC78) in a[0x001CA316].ops and a[0x001CA316].function == 0x001CA2F0
    assert target(a[0x001CA31B]) == 0x00062260 and ('imm', 0x0047E938) in a[0x001CA32A].ops
    assert ('imm', 0x29) in a[0x001CA328].ops and target(a[0x001CA334]) == 0x0005B980
    # sub_0005C420 mounts (sub_001C8540) before it registers (sub_001CA2F0)
    assert target(a[0x0005C501]) == 0x001C8540 and target(a[0x0005C558]) == 0x001CA2F0


def test_class_and_messages():
    a = by_address()
    # dispatch: +0x10 == 0 and +0x14 == 0x40000 -> sub_0028FE30 (its only caller)
    assert ('imm', 0x40000) in a[0x0027DDE5].ops and a[0x0027DDEB].mnemonic == 'je' and '0x27de22' in a[0x0027DDEB].text
    assert target(a[0x0027DE39]) == 0x0028FE30 and calls_to(0x0028FE30) == {0x0027DE39}
    assert target(a[0x0027DE18]) == 0x00293730                       # 0x80000: the giant robot
    # messages: class 0x40000 -> sub_00292430; message 2 = death anim 0x815, state 0x11, -1
    assert ('imm', 0x40000) in a[0x0027D36B].ops and target(a[0x0027D3E9]) == 0x00292430
    assert ('imm', 0x815) in a[0x0029250B].ops and ('imm', 0x11) in a[0x00292521].ops
    assert a[0x0029253D].mnemonic == 'or' and ('imm', 0xFFFFFFFF) in a[0x0029253D].ops


def test_state_blocks():
    a = by_address()
    # sub_0025F720 scene 14: sub_0028D6D0, the robot's sub_00292630, the particles sub_002A62C0
    assert [target(a[x]) for x in (0x0025F760, 0x0025F765, 0x0025F76A)] == [0x0028D6D0, 0x00292630, 0x002A62C0]
    assert calls_to(0x0028D6D0) == {0x0025F760}
    # sub_0028D6D0: effect blocks, 0x2E04 bytes of level heap at [0x0075F5E8], tail to the init
    assert target(a[0x0028D6E8]) == 0x002A8300
    assert ('imm', 0x2E04) in a[0x0028D6F2].ops and target(a[0x0028D6F7]) == 0x0003DFA0
    assert mem(a[0x0028D6FF])[0][4] == 0x0075F5E8
    assert a[0x0028D704].mnemonic == 'jmp' and target(a[0x0028D704]) == 0x0028D0C0
    # sub_002A62C0 allocates the particle block [0x0078B0D0]; sub_002A8300 writes [0x0078B090]/[0x0078B098]
    assert ('imm', 0x7D00) in a[0x002A62C2].ops and mem(a[0x002A62CF])[0][4] == 0x0078B0D0
    writes = {mem(i)[0][4] for i in decoded() if i.function == 0x002A8300 and i.mnemonic == 'mov' and mem(i)}
    assert {0x0078B090, 0x0078B098} <= writes
    # [0x0075F5E8] is read only by the Crowe functions (not by the giant robot sub_00293730)
    users = {i.function for i in decoded() if any(k == 'disp' and v == 0x0075F5E8 for k, v in i.values())}
    assert users and all(0x0028D0C0 <= f <= 0x00292430 for f in users), sorted(map(hex, users))
    assert 0x00293730 not in users


def test_arena_grid_and_floor():
    a = by_address()
    rd = xbe_reader()
    f = lambda va: struct.unpack('<f', rd(va, 4))[0]
    # sub_0028D0C0: sub_002A93D0(state+0x970, +0xEE8, +0xF10, +0xF60, +0x1190, +0x1198, 283.45, 61.2)
    assert ('imm', 0x4274CCCD) in a[0x0028D644].ops and ('imm', 0x438DB99A) in a[0x0028D656].ops
    disps = [mem(a[x])[0][4] for x in (0x0028D65B, 0x0028D662, 0x0028D669, 0x0028D670, 0x0028D677)]
    assert disps == [0x1198, 0x1190, 0xF60, 0xF10, 0xEE8]
    assert ('imm', 0x970) in a[0x0028D67E].ops and target(a[0x0028D684]) == 0x002A93D0
    assert calls_to(0x002A93D0) == {0x0028D684}                      # the grid is built once per level
    # state 0: the boss goes to its cell's centre (+0x10 x, +0x14 z of the 0x28-byte cell)
    assert mem(a[0x00290E81])[0][4] == 0x980 and mem(a[0x00290EA5])[0][4] == 0x984
    assert mem(a[0x00290EAF])[0][4] == 0x6C
    # the death point (310.49, 86, 80.95): x, y and z constants, written to +0x64/+0x68/+0x6C
    assert mem(a[0x002922FB])[0][4] == 0x0049F3C4 and mem(a[0x0029230B])[0][4] == 0x68
    assert mem(a[0x00292303])[0][4] == 0x0049F3CC and mem(a[0x00292310])[0][4] == 0x0049F3C8
    assert mem(a[0x0029231D])[0][4] == 0x6C
    assert (f(0x0049F3C4), round(f(0x0049F3CC), 2), round(f(0x0049F3C8), 2)) == (86.0, 310.49, 80.95)
    # the hazards: spawned at y 86 (sub_002AB1B0, from the fight only), exploding below 86.205
    assert calls_to(0x002AB1B0) == {0x00291E7F} and calls_to(0x002AA6A0) == {0x0028F587}
    assert mem(a[0x002AB39A])[0][4] == 0x0049F3C4 and mem(a[0x002AB3A5])[0][4] == 0x118
    assert mem(a[0x002AADE9])[0][4] == 0x0049F4CC and target(a[0x002AAE1A]) == 0x000EFBD0
    assert calls_to(0x002AD3D0) == {0x0028EB8C, 0x0028EC85}
    # the spark floor: push 86.4 for sub_002A85C0
    assert ('imm', 0x42ACCCCD) in a[0x00290BF8].ops and target(a[0x00290BFD]) == 0x002A85C0
    assert ends(a, 0x00290BFD) == 0x00290C02 and calls_to(0x002A85C0) == {0x00290BFD}
    # sub_0028DD50 reads 86.0 as a distance threshold (restored inside)
    assert mem(a[0x0028DD83])[0][4] == 0x0049F3C4 and a[0x0028DD8F].mnemonic == 'comiss'
    assert target(a[0x00290BE2]) == 0x0028DD50 and ends(a, 0x00290BE2) == 0x00290BE7
    # every reader of the shifted constants (the Crow walker's two reads of 86.0 are outside the fight)
    readers = {}
    for i in decoded():
        for k, v in i.values():
            if k == 'disp' and v in (0x0049F3C4, 0x0049F4CC, 0x0049F4D0, 0x0049F4D4, 0x0049F3CC, 0x0049F3C8, 0x0049F3D8):
                readers.setdefault(v, set()).add(0x0028FE30 if i.function == 0x0029002C else i.function)   # its tail alias
    assert readers[0x0049F3C4] == {0x0028DD50, 0x0028FE30, 0x002998E0, 0x002AA6A0, 0x002AB1B0, 0x002AD3D0}
    for va in (0x0049F4CC, 0x0049F4D0, 0x0049F4D4):
        assert readers[va] == {0x002AA6A0}
    for va in (0x0049F3CC, 0x0049F3C8, 0x0049F3D8):
        assert readers[va] == {0x0028FE30}



def test_start_cell():
    """Stock state 0 puts the boss in the 310.0 column (the hook's start cell)."""
    a = by_address()
    rd = xbe_reader()
    assert struct.unpack('<f', rd(0x0049F3D8, 4))[0] == 310.0
    # ebp = entity +0x64 (x); state 0 writes 310.0 there before the lookup
    m = mem(a[0x00290022])[0]
    assert a[0x00290022].mnemonic == 'lea' and m[1] == 'esi' and m[4] == 0x64
    assert mem(a[0x00290DC2])[0][4] == 0x0049F3D8
    m = mem(a[0x00290DD3])[0]
    assert m[1] == 'ebp' and not m[4]
    # sub_002A9D00(state+4, ..., x, own z): the second point's cell to +4+0x1194 = +0x1198
    assert ('imm', 4) in a[0x00290E5E].ops and target(a[0x00290E62]) == 0x002A9D00
    assert mem(a[0x00290E4C])[0][1] == 'ebp' and mem(a[0x00290E49])[0][4] == 0x6C
    assert mem(a[0x002A9D19])[0][4] == 0x96C and mem(a[0x002A9D2D])[0][4] == 0x1194
    assert target(a[0x002A9D37]) == 0x002A9330
    # the boss's cell index: +0x1198 * 7 + +0x119C
    assert mem(a[0x00290E6D])[0][4] == 0x1198 and mem(a[0x00290E73])[0][4] == 0x119C
    assert ('imm', 7) in a[0x00290E79].ops
    # sub_002A9330: rows by z of each row's first cell (+4, +0xC; 7 cells a row), then x (+0, +8)
    assert mem(a[0x002A934F])[0][4] == 0xC and ('imm', 0x118) in a[0x002A93A0].ops
    assert mem(a[0x002A9356])[0][4] == 0xFFFFFFF8 and mem(a[0x002A935C])[0][4] in (0, None)
    assert mem(a[0x002A9383])[0][4] == 8 and ('imm', 7) in a[0x002A9393].ops
    assert a[0x002A93AB].mnemonic == 'cmp' and ('imm', 1) in a[0x002A93AB].ops   # written only when found
    # sub_002A93D0 (the build) starts both cell indices at -1
    assert ('imm', 0xFFFFFFFF) in a[0x002A93FD].ops
    assert mem(a[0x002A9400])[0][4] in (0, None) and mem(a[0x002A9409])[0][4] in (0, None)

def test_intro_wait():
    a = by_address()
    # state 1: waits for player 0's scripted mode 0x100 (the intro cutscene), then state 2
    assert mem(a[0x00290FDF])[0][4] == 0x007B0C48
    assert mem(a[0x00290FE5])[0][4] == 0x114 and ('imm', 0x100) in a[0x00290FE5].ops
    # state 2: a player out of the scripted mode starts the fight (state 4)
    assert mem(a[0x002911B5])[0][4] == 0x114 and ('imm', 4) in a[0x002911E2].ops
    # 0x100 is the mode sub_000D4B00 sets
    assert ('imm', 0x100) in a[0x000D4B69].ops and mem(a[0x000D4B69])[0][4] == 0x114


def test_turret_prop_site():
    a = by_address()
    assert target(a[0x0028ECEC]) == 0x0028E770 and ends(a, 0x0028ECEC) == 0x0028ECF1
    assert calls_to(0x0028E770) == {0x0028ECEC}
    # the explosion sub_000EFBD0(0x20, 0x17, state + (part+0x7A)*0x5C + 0x24, 0, 0, 0, 0, 1)
    assert mem(a[0x0028E792])[0][4] == 0x7A and ('imm', 0x5C) in a[0x0028E795].ops
    assert mem(a[0x0028E7B7])[0][4] == 0x24 and target(a[0x0028E802]) == 0x000EFBD0
    assert ('imm', 0x20) in a[0x0028E7F4].ops and ('imm', 0x17) in a[0x0028E7EE].ops
    assert ('imm', 1) in a[0x0028E798].ops
    assert 0x7A * 0x5C + 0x24 == 0x2BFC
    # the prop: model 0x4BE + part (sub_000A6F90), stored at +0x2C30 + part*0x5C and never read
    assert mem(a[0x0028E815])[0][4] == 0x4BE and target(a[0x0028E81C]) == 0x000A6F90
    readers = [i for i in decoded() if any(k == 'disp' and v in (0x2C30, 0x2C8C, 0x2CE8) for k, v in i.values())]
    assert all(i.function in (0x0028D0C0, 0x0028E770) and i.mnemonic == 'mov' and i.ops[0][0] == 'mem'
               for i in readers), [hex(i.address) for i in readers]


GAME = ROOT / 'private/game/pak'
if not GAME.exists() and len(ROOT.parents) > 2:
    GAME = ROOT.parents[2] / 'private/game/pak'


def _index(path):
    with open(path, 'rb') as f:
        head = f.read(12)
        offset, length = struct.unpack_from('<II', head, 4)
        f.seek(offset)
        table = f.read(length)
    return {c: (pos, n) for c, pos, n, _ in struct.iter_unpack('<4I', table)}


def test_models_in_paks():
    """The boss model is in chr.pak; the turret props only in the mission 10 level pak (skipped without data)."""
    if not (GAME / 'arcade/l_100.pak').exists():
        pytest.skip('game paks not available locally')
    idx = _index(GAME / 'arcade/l_100.pak')
    with open(GAME / 'arcade/l_100.pak', 'rb') as f:
        pos, n = idx[0xD9E2667A]                                     # prop/table/prop_all_xbas.raw
        f.seek(pos)
        table = f.read(n)
    start = struct.unpack_from('<I', table, 4)[0] + 8
    props = {}
    for o in range(start, len(table) - 0x27, 0x28):
        mid, kind, name, path, _, _, group, _, flags, _ = struct.unpack_from('<iiiiifiiii', table, o)
        props[mid] = (table[path + 8:table.index(b'\0', path + 8)].decode('latin1'), kind, group, flags)
    crc = lambda p: zlib.crc32(f'assets/ts/models/{p}/xbox/output/{p.split("/")[-1]}.xbr'.encode()) & 0xFFFFFFFF
    path, kind, group, flags = props[0x5E8]
    assert (path, kind, group) == ('chrs/splittercrow', 8, 0x4D) and flags & 0x2000000
    assert crc(path) in _index(GAME / 'chr.pak')
    story = _index(GAME / 'story/l_14_st.pak')
    mounted = [_index(GAME / p) for p in ('chr.pak', 'gun.pak', 'arcade/l_100.pak')]
    for m in (0x4BE, 0x4BF, 0x4C0):
        path = props[m][0]
        assert 'splittercrow' in path and crc(path) in story
        assert not any(crc(path) in i for i in mounted), path


def test_level_marker_and_reachability():
    a = by_address()
    # the death sets level marker 0 (sub_002275F0: marker +0xB = 5), call 0x002923A3
    assert target(a[0x002923A3]) == 0x002275F0 and ends(a, 0x002923A3) == 0x002923A8
    assert mem(a[0x002275F4])[0][4] == 0x0078CECC and ('imm', 5) in a[0x0022760C].ops
    assert calls_to(0x002275F0) == {0x002923A3, 0x0023658D}
    # then state 0x12, which no case handles (ja past 0x11)
    assert ('imm', 0x12) in a[0x0029237B].ops and ('imm', 0x11) in a[0x00290D8F].ops
    # no level event sub_002274C0 and no tag lookup sub_0023FF20 below the fight, its messages and draw
    graph = {}
    for i in decoded():
        if i.call or (i.branch and i.mnemonic == 'jmp'):
            t = target(i)
            if t is not None and t != i.function:
                graph.setdefault(i.function, set()).add(t)
    seen, todo = set(), [0x0028FE30, 0x00292430, 0x0028D710, 0x0028DA10, 0x0028F730, 0x0028E6F0, 0x0028E700]
    while todo:
        f = todo.pop()
        if f in seen:
            continue
        seen.add(f)
        todo.extend(graph.get(f, ()))
    assert len(seen) > 500 and 0x002275F0 in seen
    assert 0x002274C0 not in seen and 0x0023FF20 not in seen
    # the Crowe's own functions read no scene number
    scene = {i.function for i in decoded() if any(k == 'disp' and v == 0x0079094C for k, v in i.values())}
    own = {f for f in seen if 0x0028D0C0 <= f <= 0x00292630}
    assert own and not own & scene


def test_draw_hud_and_meter_sites():
    a = by_address()
    # the frame draws the Crowe (sub_0028D710) only in scene 14; sub_00126560 at 0x001CEE87 runs every frame
    assert ('imm', 0xE) in a[0x001CEE49].ops and target(a[0x001CEE53]) == 0x0028D710
    assert calls_to(0x0028D710) == {0x001CEE53}
    assert target(a[0x001CEE87]) == 0x00126560 and ends(a, 0x001CEE87) == 0x001CEE8C
    assert calls_to(0x00126560) == {0x001CEE87}
    assert mem(a[0x0028D710])[0][4] == 0x0075F5E8 and a[0x0028D717].mnemonic == 'je'
    # the fight sets the story focus and makes the +0x268 record of the overhead meter
    assert mem(a[0x00290F61])[0][4] == 0xFF4 and target(a[0x00290F67]) == 0x0012C800
    assert mem(a[0x00290F6E])[0][4] == 0x268
    # the meter's only caller
    assert calls_to(0x0013A400) == {0x0014D25F} and ends(a, 0x0014D25F) == 0x0014D264


def test_death_effects():
    """After the death the fight call still updates the particles; smoke follows the parts mask."""
    a = by_address()
    # every fight call, before the state switch (0x00290D98): the particle update with the emit mask
    assert target(a[0x00290D3E]) == 0x002A7D60 and a[0x00290D3E].address < 0x00290D98
    assert mem(a[0x00290C0F])[0][4] == 0x2E00 and a[0x00290C17].mnemonic == 'je'   # mask built only while +0x2E00
    assert ('imm', 0x42C80000) in a[0x00290D17].ops
    # sub_002A7D60 recounts the live particles in [0x0078B0CC] and ages them (+0x18)
    assert mem(a[0x002A7D8A])[0][4] == 0x0078B0CC and mem(a[0x002A7E95])[0][4] == 0x0078B0CC
    assert mem(a[0x002A7DA6])[0][4] == 0x18
    # +0x2E00: 0x2F at the fight start, 0 only in state 0 (0x00290E43) and the init; the death leaves it
    writes = {i.address for i in decoded() if i.mnemonic == 'mov' and mem(i) and mem(i)[0][4] == 0x2E00
              and i.ops[0][0] == 'mem' and 0x0028D0C0 <= i.address < 0x00292430}
    assert writes == {0x0028D2AA, 0x00290E43, 0x002911D0, 0x002913EB}
    assert ('imm', 0x2F) in a[0x002911D0].ops and ('imm', 0x2F) in a[0x002913EB].ops
    # the draw: hazards, parts, sparks and the shared particles, all from sub_0028D710
    assert target(a[0x0028D9D0]) == 0x002A8FC0 and target(a[0x0028DA03]) == 0x002A7F30


def test_multi_instance_globals():
    """Every split Crowe global: [0x0075F5E8] is read only below the hooked entries (fight, message handler,
    think and hit callbacks, the draw and the allocation), and the effect/particle globals its code reaches
    are exactly the swapped window."""
    a = by_address()
    graph, users, fx = {}, set(), {}
    for i in decoded():
        if i.call or (i.branch and i.mnemonic == 'jmp'):
            t = target(i)
            if t is not None:
                graph.setdefault(i.function, set()).add(t)
        for k, v in i.values():
            if k == 'disp' and v == 0x0075F5E8:
                users.add(i.function)
            if k == 'disp' and 0x0078B080 <= v < 0x0078B1C0:
                fx.setdefault(v, set()).add(i.function)

    def reach(roots):
        seen, todo = set(), list(roots)
        while todo:
            f = todo.pop()
            if f not in seen:
                seen.add(f)
                todo.extend(graph.get(f, ()))
        return seen
    entries = {0x0028FE30, 0x00292430, 0x0028DA10, 0x0028F730, 0x0028D710, 0x0028D6D0, 0x002A62C0}
    seen = reach(entries)
    assert users and users <= seen | {0x0029002C}
    window = {v for v, fs in fx.items() if fs & seen}
    text = INC.read_text(encoding='utf-8')
    for v in window:
        assert f'0x{v:08X}u' in text, hex(v)
    assert len(window) == 28                                       # + [0x0075F5E8] = 29 words
    # the entries the swap needs: message handler, think (slot 2) and hit (+0x19C) callbacks
    assert target(a[0x0027D3E9]) == 0x00292430
    assert ('imm', 0x0028DA10) in a[0x00290EF7].ops and ('imm', 2) in a[0x00290EFC].ops
    assert ('imm', 0x0028F730) in a[0x00290EEC].ops and target(a[0x00290EF2]) == 0x0009E720
    # [0x0078B1E4] is a scratch value of one fight call (written, then read at 0x00291B61)
    assert mem(a[0x00291AF6])[0][4] == 0x0078B1E4 and mem(a[0x00291B61])[0][4] == 0x0078B1E4
