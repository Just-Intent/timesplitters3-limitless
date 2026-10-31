"""MapMaker real story bosses: the giant trophy monster (deerbeast) as a Story AI.

Compiles src/interface/pc_mapmaker_realbosses.inc against fake guest memory
(with mutations that must be caught) and, when the private XBE and its
disassembly are available, checks the stock facts the hooks rely on: the
picker's list end, row callback, highlight and row lookups; the synthetic row
id being past every stock bound; the character data of id 0x15; the anim pak
string, mount routine, mount cap and registration order; the AI class copy
and the 0x80 dispatch; the boss bar label site; and that the monster handler
cannot fire a level event (static call graph).
"""
import os
from pathlib import Path
import re
import struct
import subprocess
import pytest

from native_compiler import native_compiler
from mapmaker_decoded import decoded, _private

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_realbosses.inc'

MUTATIONS = {
    'always-on': ('    return mapmaker_limits.limits;\n}', '    return 1;\n}'),
    'any-list': ('WORD_AT(g_esp)==RBS_PICKER_END && rbs_editor()', '1 && rbs_editor()'),
    'row-in-play': ('static int rbs_editor(void) { return rbs_on() && WORD_AT(RBS_SCENE)==102u; }',
                    'static int rbs_editor(void) { return rbs_on() && WORD_AT(RBS_SCENE)>=100u; }'),
    'row-disabled': ('(sub_00071630,0),0u);   /* enabled, as the stock rows */', '(sub_00071630,0),1u);   /* enabled, as the stock rows */'),
    'wrong-flags': ('#define RBS_PICK_FLAGS 0x20900u', '#define RBS_PICK_FLAGS 0x22900u'),
    'row-after-original': ('        guest_call(sub_00079D60,6,text,RBS_ROW,RBS_PICK_FLAGS,0u,RBS_PICK_CB,0u);\n',
                           '        original();guest_call(sub_00079D60,6,text,RBS_ROW,RBS_PICK_FLAGS,0u,RBS_PICK_CB,0u);g_esp-=4u;\n'),
    'pick-keeps-variant': ('        WORD_AT(s+0x18u)=0xFFFFFFFFu;\n', ''),
    'pick-wrong-id': ('if(WORD_AT(s+0x14u)==RBS_ROW) id=RBS_ID;', 'if(WORD_AT(s+0x14u)==RBS_ROW) id=0xA7u;'),
    'preview-150': ('    if(rbs_editor()) WORD_AT(RBS_PREVIEW)=rbs_preview_row(WORD_AT(RBS_PREVIEW));', '    if(0) WORD_AT(RBS_PREVIEW)=0;'),
    'name-any-caller': ('(ret==RBS_RET_NAME && WORD_AT(g_esp+12u)==RBS_RET_NAME_OUTER)', '(ret==RBS_RET_NAME)'),
    'preview-gets-150': ('    else if(ret==RBS_RET_PREVIEW1 || ret==RBS_RET_PREVIEW2) row=rbs_preview_row(row);\n',
                         '    else if(ret==RBS_RET_PREVIEW1 || ret==RBS_RET_PREVIEW2) {}\n'),
    'row-of-any-id': ('((id=WORD_AT(g_esp+4u))!=RBS_ID && id!=RBS_SKEL_ID)', '((id=WORD_AT(g_esp+4u)),0)'),
    'row-of-no-ret': ('    g_eax=row;\n    MAPMAKER_GUEST_RET();\n', '    g_eax=row;\n'),
    'row-text-any': ('((row=WORD_AT(g_esp+4u))!=RBS_ROW && row!=RBS_SKEL_ROW)', '((row=WORD_AT(g_esp+4u))>RBS_SKEL_ROW)'),
    'text-no-ret': ('        g_eax=rbs.label?rbs.label:RBS_EMPTY_TEXT;\n        MAPMAKER_GUEST_RET();\n',
                    '        g_eax=rbs.label?rbs.label:RBS_EMPTY_TEXT;\n'),
    'bar-label-anywhere': ('id==RBS_BAR_TEXT && WORD_AT(g_esp)==RBS_RET_BAR_TEXT && rbs.label', 'id==RBS_BAR_TEXT && rbs.label'),
    'bar-label-any-scene': ('rbs.label && rbs_play()) {\n        g_eax=rbs.label;', 'rbs.label && rbs_on()) {\n        g_eax=rbs.label;'),
    'mount-before-original': ('    int has;\n    uint32_t mounted;\n    original();\n    if(!rbs_play() || rbs.optout) return;',
                              '    int has;\n    uint32_t mounted;\n    if(!rbs_play() || rbs.optout) { original();return; }\n    guest_call(sub_0005A130,1,RBS_PAK);original();'),
    'mount-any-map': ('    if((has=rbs_map_has(RBS_ID))==0) return;\n', '    has=1;\n'),
    'mount-preset': ('            if(RBS_H(d+RBS_INFO_SET)<0xCu) return 0;          /* a preset set */\n', ''),
    'mount-twice': ('    if(rbs_pak_mounted()) return;\n    if((mounted', '    if((mounted'),
    'mount-fills-table': ('#define RBS_MOUNT_KEEP 2u', '#define RBS_MOUNT_KEEP 0u'),
    'mount-ignores-optout': ('    if(!rbs_play() || rbs.optout) return;\n    if((has', '    if(!rbs_play()) return;\n    if((has'),
    'wrong-pak': ('#define RBS_PAK 0x0049D77Cu', '#define RBS_PAK 0x0047E624u'),
    'class-after-original': ('        WORD_AT(rec)=RBS_CLASS_LO;WORD_AT(rec+4u)=RBS_CLASS_HI;\n',
                             '        original();WORD_AT(rec)=RBS_CLASS_LO;WORD_AT(rec+4u)=RBS_CLASS_HI;g_esp-=4u;\n'),
    'class-swapped': ('#define RBS_CLASS_LO 0u ', '#define RBS_CLASS_LO 0x80u '),
    'class-without-anims': ('    else if(!rbs_pak_mounted()) rbs_log', '    else if(0) rbs_log'),
    'class-any-character': ('    if(RBS_H(a+RBS_ACTOR_CHAR)!=RBS_ID && !(MAPMAKER_PTR_OK(ent,0x100u) && RBS_H(ent+RBS_ENT_MODEL)==RBS_MODEL)) {',
                            '    if(0) {'),
    'class-any-mode': ('    return rbs_on() && WORD_AT(RBS_SCENE)==100u && RBS_B(RBS_MODE)==10u;', '    return rbs_on() && WORD_AT(RBS_SCENE)==100u;'),
    'class-ignores-optout': ('    if(rbs.optout) rbs_log("TSFP_MAPMAKER_REAL_BOSS=0: giant trophy', '    if(0) rbs_log("TSFP_MAPMAKER_REAL_BOSS=0: giant trophy'),
    'boss-flag-ignored': ('            if((f&0xF8u)!=0xB0u) return 0;', '            return 0;'),
    'boss-flag-loose': ('            if((f&0xF8u)!=0xB0u) return 0;', '            if((f&0xF0u)!=0xB0u) return 0;'),
    'sub-is-bar': ('return (uint8_t)((f&4u)?RBS_BOSS_SUB:RBS_BOSS_BAR);', 'return (uint8_t)RBS_BOSS_BAR;'),
    'sub-wrong-bit': ('return (uint8_t)((f&4u)?RBS_BOSS_SUB:RBS_BOSS_BAR);', 'return (uint8_t)((f&1u)?RBS_BOSS_SUB:RBS_BOSS_BAR);'),
    'boss-wrong-byte': ('#define RBS_SAI_BOSS 0x29u', '#define RBS_SAI_BOSS 0x21u'),
    'boss-without-area': ('pc_mapmaker_area_enabled("bosses","pc_mapmaker_bosses_hud")?rbs_boss_flag(id-WORD_AT(RBS_SAI_BASE)-1u):0u);\n        rbs_log("giant trophy',
                          '1?rbs_boss_flag(id-WORD_AT(RBS_SAI_BASE)-1u):0u);\n        rbs_log("giant trophy'),
    'hud-keeps-cleared': ('    original();\n    WORD_AT(player+RBS_FOCUS)=focus;\n', '    original();\n'),
    'hud-boss-off-stock': ('    WORD_AT(player+RBS_FOCUS)=0;\n    original();\n    WORD_AT(player+RBS_FOCUS)=focus;\n}',
                           '    if(!rbs.real[k].boss) { original();return; }\n    WORD_AT(player+RBS_FOCUS)=0;\n    original();\n    WORD_AT(player+RBS_FOCUS)=focus;\n}'),
    'hud-any-actor': ('    if(k==RBS_REAL_MAX) { original();return; }\n    /* never', '    /* never'),
    'hud-any-model': ('       (RBS_H(focus+RBS_ENT_MODEL)!=RBS_MODEL && RBS_H(focus+RBS_ENT_MODEL)!=RBS_SKEL_MODEL) || !rbs_play()', '       !rbs_play()'),
    'meter-any-caller': ('WORD_AT(g_esp)!=RBS_RET_METER) { original();return; }', '0) { original();return; }'),
    'meter-no-ret': ('    MAPMAKER_GUEST_RET();                            /* cdecl: the caller pops the four args */\n', ''),
    'meter-kept': ('    MAPMAKER_GUEST_RET();                            /* cdecl: the caller pops the four args */\n',
                   '    original();\n'),
    'meter-any-character': ('    if(RBS_H(a+RBS_ACTOR_CHAR)!=RBS_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_MODEL &&\n       RBS_H(a+RBS_ACTOR_CHAR)!=RBS_SKEL_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_SKEL_MODEL) { original();return; }\n', ''),
    'meter-model-only': ('RBS_H(a+RBS_ACTOR_CHAR)!=RBS_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_MODEL &&\n       RBS_H(a+RBS_ACTOR_CHAR)!=RBS_SKEL_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_SKEL_MODEL)',
                         'RBS_H(ent+RBS_ENT_MODEL)!=RBS_MODEL &&\n       RBS_H(ent+RBS_ENT_MODEL)!=RBS_SKEL_MODEL)'),
    'meter-any-actor': ('    if(k==RBS_REAL_MAX || rbs.real[k].boss==RBS_BOSS_SUB || !rbs_play())', '    if(rbs.real[k].boss==RBS_BOSS_SUB || !rbs_play())'),
    'meter-any-mode': ('rbs.real[k].boss==RBS_BOSS_SUB || !rbs_play()) { original();return; }', 'rbs.real[k].boss==RBS_BOSS_SUB || !rbs_on()) { original();return; }'),
    'meter-sub-hidden': (' || rbs.real[k].boss==RBS_BOSS_SUB || !rbs_play())', ' || !rbs_play())'),
    'meter-bar-shown': ('rbs.real[k].boss==RBS_BOSS_SUB || !rbs_play())', 'rbs.real[k].boss || !rbs_play())'),
    'meter-wrong-return': ('#define RBS_RET_METER 0x0014D264u', '#define RBS_RET_METER 0x0014D25Fu'),
    # the giant skeleton (id 0x28)
    'skel-row-missing': ('        if((text=rbs_skel_label())!=0) {', '        if(0) {'),
    'skel-pick-id': ('else if(WORD_AT(s+0x14u)==RBS_SKEL_ROW) id=RBS_SKEL_ID;', 'else if(WORD_AT(s+0x14u)==RBS_SKEL_ROW) id=RBS_ID;'),
    'skel-preview': ('if(row==RBS_SKEL_ROW) return rbs_arcade_row(RBS_SKEL_STANDIN);', 'if(row==RBS_SKEL_ROW) return RBS_SKEL_ROW;'),
    'skel-wrong-pak': ('#define RBS_SKEL_PAK 0x0049D6FCu', '#define RBS_SKEL_PAK 0x0049D77Cu'),
    'skel-mount-any-map': ('    if((has=rbs_map_has(RBS_SKEL_ID))!=0) {', '    if((has=1)!=0) {'),
    'skel-level-any-map': ('(has=rbs_map_has(RBS_SKEL_ID))==0) return;\n    WORD_AT', '(has=1)==0) return;\n    WORD_AT'),
    'skel-level-keeps': ('    memset(&rbs_skel,0,sizeof rbs_skel);              /* a new level: nothing of the last one */\n', ''),
    'skel-stale-pointer': ('    WORD_AT(RBS_SKEL_STATE)=0;WORD_AT(RBS_SKEL_PARTS)=0;', '   '),
    'skel-level-any-mode': ('    if(!rbs_play() || rbs.optout || (has=rbs_map_has(RBS_SKEL_ID))==0) return;',
                            '    if(!rbs_on() || rbs.optout || (has=rbs_map_has(RBS_SKEL_ID))==0) return;'),
    'skel-class': ('#define RBS_SKEL_CLASS 0x20000u', '#define RBS_SKEL_CLASS 0x80u'),
    'skel-two-bosses': ('        else if(rbs_skel.record && rbs_skel.record!=rec) {', '        else if(0) {'),
    'skel-without-state': ('        else if(!rbs_skel.state || WORD_AT(RBS_SKEL_STATE)!=rbs_skel.state)\n', '        else if(0)\n'),
    'skel-without-pak': ('else if(!rbs_skel_pak_mounted()) rbs_log("lv1_6', 'else if(0) rbs_log("lv1_6'),
    'skel-ignores-optout': ('    if(rbs.optout) rbs_log("TSFP_MAPMAKER_REAL_BOSS=0: giant skeleton', '    if(0) rbs_log("TSFP_MAPMAKER_REAL_BOSS=0: giant skeleton'),
    'skel-princess-any': ('if(!rbs_skel.record || !MAPMAKER_STACK_OK(8) || !rbs_skel_boss(WORD_AT(g_esp+4u))) { original();return; }\n    sub_0029CEF0();',
                          'if(!rbs_skel.record || !MAPMAKER_STACK_OK(8)) { original();return; }\n    sub_0029CEF0();'),
    'skel-princess-original': ('    sub_0029CEF0();\n}', '    original();\n}'),
    'skel-boss-any-scene': ('WORD_AT(a+RBS_ACTOR_RECORD)==rbs_skel.record && WORD_AT(RBS_SCENE)==100u && rbs_on();',
                            'WORD_AT(a+RBS_ACTOR_RECORD)==rbs_skel.record && rbs_on();'),
    'skel-boss-stale-state': ('    if(!rbs_skel.record || !rbs_skel.state || WORD_AT(RBS_SKEL_STATE)!=rbs_skel.state) return 0;',
                              '    if(!rbs_skel.record) return 0;'),
    'skel-risen-anim': ('#define RBS_SKEL_START 0x19Cu', '#define RBS_SKEL_START 0x23Bu'),
    'skel-risen-no-ret': ('    g_eax=guest_call(sub_00059DB0,2,ent,RBS_SKEL_START);\n    MAPMAKER_GUEST_RET();', '    g_eax=guest_call(sub_00059DB0,2,ent,RBS_SKEL_START);'),
    'skel-throw-any-action': ('    if(act>=8u || WORD_AT(RBS_SKEL_ACTIONS+act*4u)!=RBS_SKEL_THROW) { original();return; }', '    if(act>=8u) { original();return; }'),
    'skel-throw-past-table': ('    if(act>=8u || WORD_AT(RBS_SKEL_ACTIONS', '    if(WORD_AT(RBS_SKEL_ACTIONS'),
    'skel-aim-no-spoof': ('        WORD_AT(RBS_SCENE)=1u;\n', ''),
    'skel-aim-no-restore': ('        WORD_AT(RBS_SCENE)=100u;\n', ''),
    'skel-fire-spoofed': ('    if(phase==0) {', '    if(phase<=1) {'),
    'skel-fire-args': ('0x34u,shot,shot+0xCu', '0x34u,shot+0xCu,shot'),
    'skel-fire-keeps-phase': ('        WORD_AT(st+RBS_SKEL_PHASE)=2u;\n', ''),
    'skel-fire-effect': ('guest_call(sub_0010EB90,2,WORD_AT(shot+0x18u),0u);', 'guest_call(sub_0010EB90,2,WORD_AT(shot+0x14u),0u);'),
    'skel-done-original': ('    MAPMAKER_GUEST_RET();                              /* done: scene 1 returns (0x00287FD7) */', '    original();'),
    'skel-event-any': ('WORD_AT(g_esp)!=RBS_RET_EVENT || WORD_AT(RBS_SCENE)', 'WORD_AT(RBS_SCENE)'),
    'skel-event-any-scene': ('|| WORD_AT(RBS_SCENE)!=100u || !rbs_on()) { original();return; }\n    if(rbs_skel.events', '|| !rbs_on()) { original();return; }\n    if(rbs_skel.events'),
    'skel-event-no-ret': ('WORD_AT(g_esp+8u));\n    MAPMAKER_GUEST_RET();\n}', 'WORD_AT(g_esp+8u));\n}'),
    'skel-meter-shown': (' &&\n       RBS_H(a+RBS_ACTOR_CHAR)!=RBS_SKEL_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_SKEL_MODEL)', ')'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('realbosses.exe' if os.name == 'nt' else 'realbosses')
    source = ROOT / 'tests/mapmaker/mapmaker_realbosses.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/wd4706', '/wd4505',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src'),
                str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Wno-unused-function', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), '-I' + str(ROOT / 'src'),
                str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_realbosses(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_REAL_BOSS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker realbosses ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_no_guest_code_table_and_no_save_byte():
    """Wrappers only, and no Story AI chunk byte of its own (+0x21 and +0x29 belong to bosses)."""
    assert not (ROOT / 'config/guest-code-patches/realbosses.json').exists()
    text = INC.read_text(encoding='utf-8')
    writes = re.findall(r'RBS_B\([^)]*\)\s*=[^=]', text)
    assert not writes, writes


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


def test_picker_sites():
    a = by_address()
    # sub_002C1EA0: item add sub_00079D60(text, row, flags, 0, 0x002C1C90, 0), then enable, row walk to 150
    assert ('imm', 0x002C1C90) in a[0x002C1F3E].ops
    assert ('imm', 0x20900) in a[0x002C1EC6].ops and ('imm', 0x2000) in a[0x002C1EC0].ops
    assert target(a[0x002C1F86]) == 0x00071630 and target(a[0x002C1F8C]) == 0x00071C70
    assert ('imm', 0x1C20) in a[0x002C1F98].ops                       # 150 rows of 0x30
    # the list end: call sub_000781A0 at 0x002C1FA0, returning to 0x002C1FA5, after the loop
    assert a[0x002C1FA0].call and target(a[0x002C1FA0]) == 0x000781A0 and ends(a, 0x002C1FA0) == 0x002C1FA5
    assert a[0x002C1FA0].function == 0x002C1EA0
    # sub_00071630 returns the last item; sub_00071C70(item, disabled) sets item +0x28 bit 0
    assert mem(a[0x00071630])[0][4] == 0x007BB1D8 and a[0x00071635].mnemonic == 'ret'
    assert mem(a[0x00071C74])[0][4] == 0x28 and ('imm', 0xFFFFFFFE) in a[0x00071C7B].ops
    # pick callback: menu +0x18C row; slot +0 id / +0x14 row / +0x18 variant; disabled rows refused
    assert mem(a[0x002C1CBF])[0][4] == 0x18C and mem(a[0x002C1CD3])[0][4] == 0x00521A1C
    assert mem(a[0x002C1D2B])[0][4] == 0x0075FD90 and mem(a[0x002C1D37])[0][4] == 0x0075FDA4
    assert mem(a[0x002C1D50])[0][4] == 0x0075FDA8
    assert mem(a[0x002C1C9F])[0][4] == 0x28
    # highlight: [0x0078AEA8] = highlighted row; the preview reads it as an Arcade row
    assert mem(a[0x002C1E25])[0][4] == 0x18C and mem(a[0x002C1E2B])[0][4] == 0x0078AEA8
    assert mem(a[0x002C2BA0])[0][4] == 0x0078AEA8 and mem(a[0x002C2BAB])[0][4] == 0x00521A1C


def test_row_lookup_sites():
    a = by_address()
    # sub_002BE300 returns 0 for rows outside 0..149 (so 150 is free); sub_002BE600 returns -1 when not found
    assert ('imm', 0x96) in a[0x002BE30C].ops and a[0x002BE311].mnemonic == 'jae'
    assert '0x2be3d0' in a[0x002BE311].text and a[0x002BE3D0].mnemonic == 'xor'
    assert ('imm', 0xFFFFFFFF) in a[0x002BE63A].ops
    for site, ret, fn in ((0x002BE65A, 0x002BE65F, 0x002BE650), (0x002C0547, 0x002C054C, 0x002C0490),
                          (0x002C1800, 0x002C1805, 0x002C1570), (0x002C180F, 0x002C1814, 0x002C1570)):
        assert a[site].call and target(a[site]) == 0x002BE600 and ends(a, site) == ret and a[site].function == fn
    # sub_002BE650: no push between entry and the call, so its own return is at esp+0xC
    assert [a[x].mnemonic for x in (0x002BE650, 0x002BE654, 0x002BE658, 0x002BE659)] == ['mov', 'mov', 'push', 'push']
    assert target(a[0x003033BB]) == 0x002BE650 and ends(a, 0x003033BB) == 0x003033C0
    assert a[0x003033BB].function == 0x00303350
    # the reload stores the row at slot +0x14; the slot draw names the slot by that row
    assert mem(a[0x002C054C])[0][4] == 0x14
    assert mem(a[0x002C08FB])[0][4] == 0x0075FDA4 and target(a[0x002C0902]) == 0x002BE300
    # sub_0003D7B0(0) is the empty string 0x004766D8 (stock name of a row-less id)
    assert ('imm', 0x004766D8) in a[0x0003D7D1].ops
    # the preview lookups keep [0x0078AEA8] unless the row is found
    assert mem(a[0x002C1817])[0][4] == 0x0078AEA8


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


def test_character_data():
    rd = xbe_reader()
    entry = rd(0x0053A3CC + 0x15 * 0x30, 0x30)
    voice, model, lod = struct.unpack_from('<3I', entry, 0)
    assert (model, lod) == (0x5BE, 0x5BF)
    assert voice == struct.unpack_from('<I', rd(0x0053A3CC + 0xA7 * 0x30, 4))[0]   # trophy monster's voices
    ids = {struct.unpack_from('<I', rd(0x00521A18 + i * 0x30 + 4, 4))[0] for i in range(150)}
    assert 0x15 not in ids and 0xA7 in ids                          # no Arcade row; the stand-in has one
    # sub_002BE270(0x15) == 0: byte table 0x002BE29C (ids 0x0A..0x63) -> jump table 0x002BE294 entry 1
    assert rd(0x002BE29C + 0x15 - 0x0A, 1)[0] == 1
    assert struct.unpack('<2I', rd(0x002BE294, 8)) == (0x002BE28A, 0x002BE290)
    assert rd(0x0049D77C, 30) == b'pak/stream/lv1/anim/lv1_3.pak\0'


def test_mount_and_registration():
    a = by_address()
    # sub_0005C420: sub_001C8540 (mount) at 0x0005C501, then sub_001CA2F0 (register names) at 0x0005C558
    assert target(a[0x0005C501]) == 0x001C8540 and target(a[0x0005C558]) == 0x001CA2F0
    callers = {i.address for i in decoded() if i.call and target(i) == 0x001C8540}
    assert callers == {0x0005C501}
    # sub_001CA2F0 registers lv1_3's names when it is mounted (sub_00062260 != -1)
    assert ('imm', 0x0049D77C) in a[0x001CADA3].ops and target(a[0x001CADA8]) == 0x00062260
    assert a[0x001CADA3].function == 0x001CA2F0 and target(a[0x001CADBC + 5]) == 0x0005B980
    # sub_0005A130(path) = sub_000622D0(path, 1, 1) -> 1 on success; the table holds 30 paks
    assert [op for op in a[0x0005A134].ops] == [('imm', 1)] and target(a[0x0005A139]) == 0x000622D0
    assert mem(a[0x000620E0])[0][4] == 0x006D7880 and ('imm', 0x1E) in a[0x000620F9].ops
    # the bulk unmount clears the whole table (sub_00062720, from the scene loop sub_0003D440)
    assert mem(a[0x0006277E])[0][4] == 0x006D7880
    assert any(i.call and target(i) == 0x00062720 and i.function == 0x0003D440 for i in decoded())
    # scene 0x64 never gets lv1 by the stock list: sub_001C8540 builds lv%d paths only from that scene list
    assert ('imm', 0x0047E624) in a[0x001C897B].ops


def test_class_copy_and_dispatch():
    a = by_address()
    # sub_0015B830: story mode only, record +0/+4 -> AI +0x10/+0x14; record at AI +0xBF0
    assert mem(a[0x0015B841])[0][4] == 0xBF0
    assert mem(a[0x0015B99A])[0][4] == 0x007DE455 and ('imm', 10) in a[0x0015B99A].ops
    assert mem(a[0x0015B9A5])[0][4] == 0x10 and mem(a[0x0015B9AB])[0][4] == 0x14
    assert {i.function for i in decoded() if i.call and target(i) == 0x0015B830} == {0x0015F8C0}
    # MapMaker's own class (0x11000000/0) written by sub_00240C50
    assert ('imm', 0x11000000) in a[0x002421A7].ops and a[0x002421A7].function == 0x00240C50
    # dispatcher: AI +0x14 == 0x80 (with +0x10 == 0) -> sub_00280FA0
    assert mem(a[0x0027D594])[0][4] == 0x14 and mem(a[0x0027D59A])[0][4] == 0x10
    assert ('imm', 0x80) in a[0x0027DB94].ops and target(a[0x0027DBE2]) == 0x00280FA0
    # the handler indexes the character table by AI +0x48 and sets the story focus
    assert mem(a[0x002810F6])[0][4] == 0x48 and mem(a[0x00281110])[0][4] == 0x0053A3EC
    assert mem(a[0x002811B5])[0][4] == 0x007B0C7C


def test_boss_bar_label_site():
    a = by_address()
    assert ('imm', 0xDCC) in a[0x0014CECB].ops and target(a[0x0014CED0]) == 0x0003D7B0
    assert ends(a, 0x0014CED0) == 0x0014CED5 and a[0x0014CED0].function == 0x0014CC80
    assert mem(a[0x0014CEDE])[0][4] == 0xFF4
    # the model flag test: skipped in scene 0x64, applied in the editor 0x66
    assert ('imm', 0x64) in a[0x000A5E80].ops and '0xa5ea2' in a[0x000A5E83].text
    assert ('imm', 0x66) in a[0x000A5E93].ops and ('imm', 0x2000000) in a[0x000A5E9B].ops


def test_monster_cannot_fire_level_events():
    """sub_00280FA0 and its damage callback never reach sub_002274C0 (direct calls)."""
    graph = {}
    for i in decoded():
        if i.call:
            t = target(i)
            if t is not None:
                graph.setdefault(i.function, set()).add(t)
    a = by_address()
    assert ('imm', 0x000BC4D0) in a[0x0028115B].ops                 # damage callback
    seen, todo = set(), [0x00280FA0, 0x000BC4D0]
    while todo:
        f = todo.pop()
        if f in seen:
            continue
        seen.add(f)
        todo.extend(graph.get(f, ()))
    assert len(seen) > 100                                          # the walk really ran
    assert 0x002274C0 not in seen
    # control: a boss that does fire events reaches it (the train jetpack, 0x001EEC6D)
    assert 0x002274C0 in graph.get(0x001EEBD0, set())


def test_overhead_meter_sites():
    """The floating meter: MapMaker story play only, actors with a +0x268 record, one caller."""
    a = by_address()
    # sub_0014D1F0: scene 100 and story mode, else the actor loop is skipped (story levels, mission 5)
    assert mem(a[0x0014D21C])[0][4] == 0x0079094C and ('imm', 0x64) in a[0x0014D21C].ops
    assert a[0x0014D223].mnemonic == 'jne' and '0x14d277' in a[0x0014D223].text
    assert mem(a[0x0014D225])[0][4] == 0x007DE455 and ('imm', 0xA) in a[0x0014D225].ops
    assert a[0x0014D22C].mnemonic == 'jne' and '0x14d277' in a[0x0014D22C].text
    # loop: actors 0x007A3580[[0x00790950] .. [0x0074C37C]); entity +0x268 record, not the player's entity
    assert mem(a[0x0014D22E])[0][4] == 0x00790950 and mem(a[0x0014D234])[0][4] == 0x0074C37C
    assert mem(a[0x0014D240])[0][4] == 0x007A3580 and mem(a[0x0014D249])[0][4] == 0x268
    assert mem(a[0x0014D253])[0][4] == 0x14
    assert ('imm', 0x6E) in a[0x0014D258].ops and ('imm', 0x7F) in a[0x0014D25A].ops
    assert a[0x0014D25F].call and target(a[0x0014D25F]) == 0x0013A400 and ends(a, 0x0014D25F) == 0x0014D264
    assert a[0x0014D25F].function == 0x0014D1F0 and ('imm', 0x10) in a[0x0014D26A].ops   # cdecl
    callers = {i.address for i in decoded() if i.call and target(i) == 0x0013A400}
    assert callers == {0x0014D25F}
    assert {i.function for i in decoded() if i.call and target(i) == 0x0014D1F0} == {0x001CEBC0}
    # sub_0013A400 reads the entity from the actor, health +0x1CC, ends in a plain ret
    assert a[0x0013A41E].mnemonic == 'mov' and mem(a[0x0013AAA5])[0][4] == 0x1CC
    assert target(a[0x0013AAEF]) == 0x0001AC50 and a[0x0013AB00].mnemonic == 'ret' and not a[0x0013AB00].ops
    # the list holds entity +0x7C (the actor pool entry)
    assert mem(a[0x0015BCA4])[0][4] == 0x7C and mem(a[0x0015BCAC])[0][4] == 0x007A3580
    # the deerbeast handler creates the record; MapMaker's own AI setup never does
    assert target(a[0x002811A3]) == 0x0012C800 and mem(a[0x002811AA])[0][4] == 0x268
    makers = {i.function for i in decoded() if i.call and target(i) == 0x0012C800}
    assert 0x00280FA0 in makers and 0x000D2FD0 in makers
    assert not makers & {0x00240C50, 0x0015B830, 0x0015F8C0}


# ---- the giant skeleton (mansion boss, id 0x28) -----------------------------

def direct_graph():
    """Direct calls only (an immediate target); indirect calls listed apart."""
    graph, indirect = {}, {}
    for i in decoded():
        if i.call:
            imm = [op[1] for op in i.ops if op[0] == 'imm']
            if imm:
                graph.setdefault(i.function, set()).add(imm[0])
            else:
                indirect.setdefault(i.function, []).append(i.address)
    return graph, indirect


def reach(graph, roots):
    seen, todo = set(), list(roots)
    while todo:
        f = todo.pop()
        if f not in seen:
            seen.add(f)
            todo.extend(graph.get(f, ()))
    return seen


def scene_readers(functions):
    return {i.function for i in decoded() if i.function in functions
            and any(op[0] == 'mem' and not op[1] and not op[2] and op[4] == 0x0079094C for op in i.ops)}


def test_skeleton_data():
    rd = xbe_reader()
    voice, model, lod = struct.unpack_from('<3I', rd(0x0053A3CC + 0x28 * 0x30, 12), 0)
    assert (model, lod) == (0x5E9, 0xFFFFFFFF)
    ids = [struct.unpack_from('<I', rd(0x00521A18 + i * 0x30 + 4, 4))[0] for i in range(150)]
    assert 0x28 not in ids and ids.index(0xA6) == 144                # no Arcade row; the stand-in has one
    assert rd(0x002BE29C + 0x28 - 0x0A, 1)[0] == 1                   # sub_002BE270(0x28) == 0, as 0x15
    assert rd(0x0049D6FC, 30) == b'pak/stream/lv1/anim/lv1_6.pak\0'
    assert struct.unpack('<8I', rd(0x005208B8, 32)) == (8, 0, 6, 3, 7, 4, 2, 5)   # action types; 6 = throw

    def table(va, n):
        out = {}
        for k in range(n):
            e = rd(va + k * 0x18, 0x18)
            name = rd(struct.unpack_from('<I', e, 0)[0], 40).split(b'\0')[0].decode()
            out[name] = struct.unpack_from('<H', e, 0x10)[0]
        return out
    lv1_6, lv1_3 = table(0x0048C738, 0x12), table(0x0048C1B0, 0x10)
    assert lv1_6['princess_start'] == 0x19C and lv1_6['princess_raise_react_end'] == 0x1A6
    assert not set(lv1_6.values()) & set(lv1_3.values())             # both may be mounted
    # sub_0025F720: scenes 1 and 17 reach the sub_00287E00 case; scene 100 is past the table (ja)
    jt = [struct.unpack_from('<I', rd(0x0025F7C0 + 4 * k, 4))[0] for k in range(6)]
    assert jt[rd(0x0025F7D8 + 0, 1)[0]] == 0x0025F7AD and jt[rd(0x0025F7D8 + 16, 1)[0]] == 0x0025F7AD
    # HUD: model 0x5E9 -> the case at 0x0014CF05 (scene 1 test)
    idx = rd(0x0014D174 + 0x5E9 - 0x5CB, 1)[0]
    assert struct.unpack_from('<I', rd(0x0014D158 + 4 * idx, 4))[0] == 0x0014CF05


def test_skeleton_state_and_dispatch():
    a = by_address()
    # registration of lv1_6's names when mounted
    assert ('imm', 0x0049D6FC) in a[0x001CAE61].ops and target(a[0x001CAE66]) == 0x00062260
    assert a[0x001CAE61].function == 0x001CA2F0 and ('imm', 0x0048C738) in a[0x001CAE75].ops
    assert target(a[0x001CAE7F]) == 0x0005B980
    # class 0x20000 -> sub_0029FE00(entity); scene 1 -> sub_0029CEF0, else sub_0029E0A0
    assert ('imm', 0x20000) in a[0x0027DD54].ops and a[0x0027DD54].function == 0x0027D590
    assert a[0x0027DDD3].call and target(a[0x0027DDD3]) == 0x0029FE00 and ends(a, 0x0027DDD3) == 0x0027DDD8
    assert mem(a[0x0029FE00])[0][4] == 0x0079094C and ('imm', 1) in a[0x0029FE00].ops
    assert a[0x0029FE09].mnemonic == 'jmp' and target(a[0x0029FE09]) == 0x0029CEF0
    assert a[0x0029FE0E].mnemonic == 'jmp' and target(a[0x0029FE0E]) == 0x0029E0A0
    assert {i.address for i in decoded() if i.call and target(i) == 0x0029FE00} == {0x0027DDD3}
    # the handler takes the entity at [esp+4] (0x80 after its 0x78 frame and one push)
    assert a[0x0029CEF0].mnemonic == 'sub' and ('imm', 0x78) in a[0x0029CEF0].ops
    assert mem(a[0x0029CEF4])[0][4] == 0x80 and mem(a[0x0029CEFD])[0][4] == 0x7C
    # level init: sub_0015E7C0 calls sub_0025F720 in story mode only; it is the only caller
    assert {i.address for i in decoded() if i.call and target(i) == 0x0025F720} == {0x0015E818}
    assert mem(a[0x0015E80F])[0][4] == 0x007DE455 and ('imm', 0xA) in a[0x0015E80F].ops
    assert target(a[0x0025F7AD]) == 0x00287E00 and mem(a[0x0025F72F])[0][4] == 0x0079094C
    assert ('imm', 0x50) in a[0x0025F735].ops and a[0x0025F738].mnemonic == 'ja'
    # sub_00287E00: level-heap blocks 0x4C7C -> [0x0078B1EC], 0x7D00 -> [0x0078B0D0], then the init
    assert ('imm', 1) in a[0x00287E00].ops and ('imm', 0x4C7C) in a[0x00287E02].ops
    assert target(a[0x00287E07]) == 0x0003DFA0 and mem(a[0x00287E0F])[0][4] == 0x0078B1EC
    assert target(a[0x00287E14]) == 0x002A62C0 and target(a[0x00287E19]) == 0x00287860
    assert ('imm', 0x7D00) in a[0x002A62C2].ops and mem(a[0x002A62CF])[0][4] == 0x0078B0D0
    assert {i.address for i in decoded() if i.call and target(i) == 0x00287E00} == {0x0025F7AD}
    # sub_00287860 only writes fields of the block (reads no scene, calls only the RNG and rounding)
    assert scene_readers({0x00287860}) == set()


def test_skeleton_scene_sites():
    a = by_address()
    graph, indirect = direct_graph()
    # direct callees of the fight that read the scene: exactly the three hooked/handled ones
    assert scene_readers(graph[0x0029CEF0]) == {0x002274C0, 0x002875F0, 0x00287E20}
    # sub_002875F0: scene 1 -> sub_00059DB0(entity, 0x19C); 17 -> 0x23B; else 0
    assert mem(a[0x002875F0])[0][4] == 0x0079094C
    assert ('imm', 0x19C) in a[0x00287617].ops and target(a[0x0028761D]) == 0x00059DB0
    assert ('imm', 0x23B) in a[0x00287604].ops and a[0x002875FD].mnemonic == 'xor'
    assert target(a[0x0029D49F]) == 0x002875F0
    # sub_00287E20: one scene read, its first instruction; the throw phases
    reads = [i.address for i in decoded() if i.function == 0x00287E20
             and any(op[0] == 'mem' and op[4] == 0x0079094C for op in i.ops)]
    assert reads == [0x00287E20]
    assert mem(a[0x00287E34])[0][4] == 0x1C and mem(a[0x00287E38])[0][4] == 0x7C      # entity, actor
    assert mem(a[0x00287E43])[0][4] == 0x9DC and mem(a[0x00287E49])[0][4] == 0x005208B8
    assert ('imm', 6) in a[0x00287E49].ops
    assert mem(a[0x00287E3B])[0][4] == 0x413C and mem(a[0x00287EA0])[0][4] == 0x4160
    assert ('imm', 1) in a[0x00287FD4].ops and '0x287e7b' in a[0x00287FD7].text
    pushes = [a[x] for x in (0x00287FE0, 0x00287FE5, 0x00287FE7, 0x00287FEF, 0x00287FF9)]
    assert [p.ops[0] for p in pushes] == [('imm', 0x3DCCCCCD), ('imm', 0xC), ('imm', 0xE), ('imm', 0x3E99999A), ('imm', 0x34)]
    assert mem(a[0x00287FDD])[0][4] == 0x34 and mem(a[0x00287FF4])[0][4] == 0xC
    assert target(a[0x00287FFB]) == 0x000EA9C0 and ('imm', 0x38) in a[0x00288011].ops
    assert mem(a[0x00288000])[0][4] == 0x18 and target(a[0x00288006]) == 0x0010EB90
    assert mem(a[0x00288016])[0][4] == 0x4160 and ('imm', 2) in a[0x00288016].ops
    # the aim path's callees read no scene; their one indirect call (a tree walk,
    # sub_001571C0 0x00157204) calls the callback sub_00087310 passes (0x00087324)
    aim = reach(graph, [0x00059E70, 0x0010E730, 0x0010EB90])
    assert {x for f in aim for x in indirect.get(f, ())} == {0x00157204} and 0x00087310 in aim
    assert ('imm', 0x00087260) in a[0x00087324].ops and target(a[0x0008733A]) == 0x00157440
    aim = reach(graph, aim | {0x00087260})
    assert not scene_readers(aim) and {x for f in aim for x in indirect.get(f, ())} == {0x00157204}
    assert target(a[0x0029D050]) == 0x00287E20 and a[0x0029D050].function == 0x0029CEF0
    # the action choice sub_00286F30 plays the throw (0x19F) and resets the phase to 0
    assert ('imm', 0x19F) in a[0x0028753C].ops and mem(a[0x00287542])[0][4] == 0x4160


def test_skeleton_events_and_bar():
    a = by_address()
    graph, indirect = direct_graph()
    # callbacks the fight installs: damage sub_000BC470, slot 2 sub_00286430, sub_00289690, emitter sub_00288BD0
    assert ('imm', 0x000BC470) in a[0x0029D41E].ops and ('imm', 0x00286430) in a[0x0029D43B].ops
    assert ('imm', 0x00289690) in a[0x0029D4BF].ops and ('imm', 0x00288BD0) in a[0x0029D3D0].ops
    seen = reach(graph, [0x0029CEF0, 0x000BC470, 0x00286430, 0x00289690, 0x00288BD0])
    assert len(seen) > 100
    sites = {i.address for i in decoded() if i.call and i.function in seen and target(i) == 0x002274C0}
    assert sites == {0x0029E063}                                     # the death, event 3
    assert ('imm', 3) in a[0x0029E057].ops and ends(a, 0x0029E063) == 0x0029E068
    assert mem(a[0x0029E059])[0][4] == 0xFF4                         # the focus is cleared first
    # the fight's own focus and damage record (state 0)
    assert mem(a[0x0029D0FB])[0][4] == 0xFF4 and target(a[0x0029D101]) == 0x0012C800
    assert mem(a[0x0029D108])[0][4] == 0x268 and mem(a[0x0029D0E5])[0][4] == 0x1CC
    # HUD case 0x5E9: scene 1 -> label 0x163C, the bar sub_00139740; else entity +0x1A0 == 1 meter
    assert mem(a[0x0014CF05])[0][4] == 0x0079094C and ('imm', 0x163C) in a[0x0014CF14].ops
    assert '0x14d07b' in a[0x0014CF19].text and target(a[0x0014D07B]) == 0x0003D7B0
    assert target(a[0x0014D0B1]) == 0x00139740 and mem(a[0x0014CF1E])[0][4] == 0x1A0
    assert target(a[0x0014CC9D]) == 0x002E4B80 and mem(a[0x0014D0D5])[0][4] == 8
    # the dust particles are drawn by the frame only in scenes 1 and 17
    assert {i.address for i in decoded() if i.call and target(i) == 0x0029CE00} == {0x001CEE31, 0x001CEE42}
    assert ('imm', 1) in a[0x001CEE1D].ops and ('imm', 0x11) in a[0x001CEE38].ops
