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
    'row-disabled': ('guest_call(sub_00071630,0),0u);', 'guest_call(sub_00071630,0),1u);'),
    'wrong-flags': ('#define RBS_PICK_FLAGS 0x20900u', '#define RBS_PICK_FLAGS 0x22900u'),
    'row-after-original': ('        guest_call(sub_00079D60,6,text,RBS_ROW,RBS_PICK_FLAGS,0u,RBS_PICK_CB,0u);\n',
                           '        original();guest_call(sub_00079D60,6,text,RBS_ROW,RBS_PICK_FLAGS,0u,RBS_PICK_CB,0u);g_esp-=4u;\n'),
    'pick-keeps-variant': ('        WORD_AT(s+0x18u)=0xFFFFFFFFu;\n', ''),
    'pick-wrong-id': ('        WORD_AT(s)=RBS_ID;\n', '        WORD_AT(s)=0xA7u;\n'),
    'preview-150': ('WORD_AT(RBS_PREVIEW)=rbs_trophy_row();', 'WORD_AT(RBS_PREVIEW)=RBS_ROW;'),
    'name-any-caller': ('(ret==RBS_RET_NAME && WORD_AT(g_esp+12u)==RBS_RET_NAME_OUTER)', '(ret==RBS_RET_NAME)'),
    'preview-gets-150': ('    else if(ret==RBS_RET_PREVIEW1 || ret==RBS_RET_PREVIEW2) row=rbs_trophy_row();\n',
                         '    else if(ret==RBS_RET_PREVIEW1 || ret==RBS_RET_PREVIEW2) row=RBS_ROW;\n'),
    'row-of-any-id': ('WORD_AT(g_esp+4u)!=RBS_ID || !rbs_editor()) { original();return; }\n    ret=',
                      '0 || !rbs_editor()) { original();return; }\n    ret='),
    'row-of-no-ret': ('    g_eax=row;\n    MAPMAKER_GUEST_RET();\n', '    g_eax=row;\n'),
    'row-text-any': ('WORD_AT(g_esp+4u)!=RBS_ROW || !rbs_editor()', 'WORD_AT(g_esp+4u)>RBS_ROW || !rbs_editor()'),
    'text-no-ret': ('        g_eax=rbs.label?rbs.label:RBS_EMPTY_TEXT;\n        MAPMAKER_GUEST_RET();\n',
                    '        g_eax=rbs.label?rbs.label:RBS_EMPTY_TEXT;\n'),
    'bar-label-anywhere': ('id==RBS_BAR_TEXT && WORD_AT(g_esp)==RBS_RET_BAR_TEXT && rbs.label', 'id==RBS_BAR_TEXT && rbs.label'),
    'bar-label-any-scene': ('rbs.label && rbs_play()) {\n        g_eax=rbs.label;', 'rbs.label && rbs_on()) {\n        g_eax=rbs.label;'),
    'mount-before-original': ('    int has;\n    uint32_t mounted;\n    original();\n    if(!rbs_play() || rbs.optout) return;',
                              '    int has;\n    uint32_t mounted;\n    if(!rbs_play() || rbs.optout) { original();return; }\n    guest_call(sub_0005A130,1,RBS_PAK);original();'),
    'mount-any-map': ('    if((has=rbs_map_has_deerbeast())==0) return;\n', '    has=1;\n'),
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
    'class-ignores-optout': ('    if(rbs.optout) rbs_log("TSFP_MAPMAKER_REAL_BOSS=0', '    if(0) rbs_log("TSFP_MAPMAKER_REAL_BOSS=0'),
    'boss-flag-ignored': ('            return (uint8_t)((f&0xFCu)==0xB0u);', '            return 0;'),
    'boss-wrong-byte': ('#define RBS_SAI_BOSS 0x29u', '#define RBS_SAI_BOSS 0x21u'),
    'boss-without-area': ('pc_mapmaker_area_enabled("bosses","pc_mapmaker_bosses_hud")?', '1?'),
    'hud-keeps-cleared': ('    original();\n    WORD_AT(player+RBS_FOCUS)=focus;\n', '    original();\n'),
    'hud-always-clears': ('    if(k==RBS_REAL_MAX || !rbs.real[k].boss) { original();return; }', '    if(k==RBS_REAL_MAX) { original();return; }'),
    'hud-any-model': ('       RBS_H(focus+RBS_ENT_MODEL)!=RBS_MODEL || !rbs_play()', '       !rbs_play()'),
    'meter-any-caller': ('WORD_AT(g_esp)!=RBS_RET_METER) { original();return; }', '0) { original();return; }'),
    'meter-no-ret': ('    MAPMAKER_GUEST_RET();                            /* cdecl: the caller pops the four args */\n', ''),
    'meter-kept': ('    MAPMAKER_GUEST_RET();                            /* cdecl: the caller pops the four args */\n',
                   '    original();\n'),
    'meter-any-character': ('    if(RBS_H(a+RBS_ACTOR_CHAR)!=RBS_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_MODEL) { original();return; }\n', ''),
    'meter-model-only': ('    if(RBS_H(a+RBS_ACTOR_CHAR)!=RBS_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_MODEL)',
                         '    if(RBS_H(ent+RBS_ENT_MODEL)!=RBS_MODEL)'),
    'meter-any-actor': ('    if(k==RBS_REAL_MAX || !rbs_play()) { original();return; }', '    if(!rbs_play()) { original();return; }'),
    'meter-any-mode': ('    if(k==RBS_REAL_MAX || !rbs_play()) { original();return; }', '    if(k==RBS_REAL_MAX || !rbs_on()) { original();return; }'),
    'meter-wrong-return': ('#define RBS_RET_METER 0x0014D264u', '#define RBS_RET_METER 0x0014D25Fu'),
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
