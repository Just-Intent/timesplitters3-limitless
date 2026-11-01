"""MapMaker real story boss: the far-future giant robot (story mission 10) as a Story AI.

Compiles src/interface/pc_mapmaker_robot.inc against fake guest memory (with
mutations that must be caught) and, when the private XBE and its disassembly
are available, checks the stock facts the hooks rely on: the character data
of id 0x42, the lv14 anim pak and its names, the class 0x80000 dispatch and
message handler, the scene-14 state allocation, the robot's scene reads,
level event, music change and pad lookups, the scene-14 explosion draw, the
soft-restart reset and the boss bar case.
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
INC = ROOT / 'src/interface/pc_mapmaker_robot.inc'

MUTATIONS = {
    'always-on': ('    return mapmaker_limits.limits;\n}', '    return 1;\n}'),
    'any-mode': ('    return rbt_on() && WORD_AT(RBT_SCENE)==100u && RBT_B(RBT_MODE)==10u;', '    return rbt_on() && WORD_AT(RBT_SCENE)==100u;'),
    'any-scene': ('    return rbt_on() && WORD_AT(RBT_SCENE)==100u && RBT_B(RBT_MODE)==10u;', '    return rbt_on() && RBT_B(RBT_MODE)==10u;'),
    'wrong-pak': ('#define RBT_PAK 0x0049DC78u', '#define RBT_PAK 0x0049DB98u'),
    'wrong-id': ('#define RBT_ID 0x42u ', '#define RBT_ID 0xA8u '),
    'mount-any-map': ('    if(!rbt_play() || rbt.optout || (has=rbt_map_has())==0) return;\n    if(rbt_pak',
                      '    if(!rbt_play() || rbt.optout || ((has=rbt_map_has()),0)) return;\n    if(rbt_pak'),
    'mount-ignores-optout': ('    if(!rbt_play() || rbt.optout || (has=rbt_map_has())==0) return;\n    if(rbt_pak',
                             '    if(!rbt_play() || (has=rbt_map_has())==0) return;\n    if(rbt_pak'),
    'mount-twice': ('    if(rbt_pak_mounted()) { rbt_log("lv14.pak already mounted",0,(uint32_t)has);return; }\n', ''),
    'mount-fills-table': ('#define RBT_MOUNT_KEEP 8u', '#define RBT_MOUNT_KEEP 0u'),
    'mount-before-original': ('    int has;\n    uint32_t mounted;\n    original();\n',
                              '    int has;\n    uint32_t mounted;\n    guest_call(sub_0005A130,1,RBT_PAK);original();\n'),
    'set-preset': ('    if(RBT_H(info+RBT_INFO_SET)!=RBT_SET_MAP) return 0;   /* a preset or profile set */\n', ''),
    'set-skips-slot-1': ('    for(k=0;k<n;k++) if(RBT_H(info', '    for(k=1;k<n;k++) if(RBT_H(info'),
    'set-skips-slots-11-20': ('    for(k=0;extra && k<RBT_B(extra+3u);k++)', '    for(k=0;0 && extra && k<RBT_B(extra+3u);k++)'),
    'set-extra-offset': ('if(RBT_H(extra+4u+k*4u)==RBT_ID) return 1;', 'if(RBT_H(extra+2u+k*4u)==RBT_ID) return 1;'),
    'set-unreadable-skipped': ('    if(!container || !MAPMAKER_PTR_OK(container,RBT_SCAN) || WORD_AT(container)!=RBT_MAGIC) return -1;',
                               '    if(!container || !MAPMAKER_PTR_OK(container,RBT_SCAN) || WORD_AT(container)!=RBT_MAGIC) return 0;'),
    'level-keeps-parts': ('    if(play) WORD_AT(RBT_PARTS)=0;', '    if(0) WORD_AT(RBT_PARTS)=0;'),
    'level-clears-parts-anywhere': ('    if(play) WORD_AT(RBT_PARTS)=0;', '    WORD_AT(RBT_PARTS)=0;'),
    'level-keeps-robot': ('    memset(&rbt_lvl,0,sizeof rbt_lvl);                /* a new level', '    rbt_lvl.state=0;                /* a new level'),
    'level-any-map': ('    if(!play || (has=rbt_map_has())==0) return;\n    WORD_AT(RBT_STATE)=0;', '    if(!play || ((has=rbt_map_has()),0)) return;\n    WORD_AT(RBT_STATE)=0;'),
    'level-stale-state': ('    WORD_AT(RBT_STATE)=0;                              /* stale', '                                  /* stale'),
    'level-second-parts': ('    if(!WORD_AT(RBT_PARTS)) guest_call(sub_002A62C0,0);', '    guest_call(sub_002A62C0,0);'),
    'level-no-parts': ('    if(!WORD_AT(RBT_PARTS)) guest_call(sub_002A62C0,0);', '    if(0) guest_call(sub_002A62C0,0);'),
    'level-unchecked': ('    if(!st || !pt || !MAPMAKER_PTR_OK', '    if(0 && !MAPMAKER_PTR_OK'),
    'class-value': ('#define RBT_CLASS 0x80000u', '#define RBT_CLASS 0x100000u'),
    'class-after-original': ('        WORD_AT(rec)=0;WORD_AT(rec+4u)=RBT_CLASS;\n',
                             '        original();WORD_AT(rec)=0;WORD_AT(rec+4u)=RBT_CLASS;g_esp-=4u;\n'),
    'class-keeps-lo': ('        WORD_AT(rec)=0;WORD_AT(rec+4u)=RBT_CLASS;', '        WORD_AT(rec+4u)=RBT_CLASS;'),
    'class-any-character': ("    if(RBT_H(a+RBT_ACTOR_CHAR)!=RBT_ID && !(MAPMAKER_PTR_OK(ent,0x100u) && RBT_H(ent+RBT_ENT_MODEL)==RBT_MODEL)) { original();return; }\n", ''),
    'class-char-only': ('    if(RBT_H(a+RBT_ACTOR_CHAR)!=RBT_ID && !(MAPMAKER_PTR_OK(ent,0x100u) && RBT_H(ent+RBT_ENT_MODEL)==RBT_MODEL))',
                        '    if(RBT_H(a+RBT_ACTOR_CHAR)!=RBT_ID)'),
    'class-without-pak': ('    else if(!rbt_pak_mounted()) rbt_log', '    else if(0) rbt_log'),
    'class-without-state': ('    else if(!rbt_lvl.state || WORD_AT(RBT_STATE)!=rbt_lvl.state) rbt_log', '    else if(0) rbt_log'),
    'class-stale-state': ('    else if(!rbt_lvl.state || WORD_AT(RBT_STATE)!=rbt_lvl.state) rbt_log', '    else if(!rbt_lvl.state) rbt_log'),
    'class-two-robots': ('    else if(rbt_lvl.record && rbt_lvl.record!=rec) {', '    else if(0) {'),
    'class-ignores-optout': ('    if(rbt.optout) rbt_log("TSFP_MAPMAKER_REAL_BOSS=0', '    if(0) rbt_log("TSFP_MAPMAKER_REAL_BOSS=0'),
    'class-no-actor': ('        rbt_lvl.record=rec;rbt_lvl.actor=a;rbt_lvl.removed=0;', '        rbt_lvl.record=rec;rbt_lvl.removed=0;'),
    'nav-any-caller': ('    if(k==sizeof rbt_nav_rets/sizeof rbt_nav_rets[0] || WORD_AT', '    if(WORD_AT'),
    'nav-missing-site': ('0x00293C64u,0x00293C9Eu,0x00293CD0u};', '0x00293C64u,0x00293C9Eu,0x00293CE5u};'),
    'nav-any-scene': ('    if(k==sizeof rbt_nav_rets/sizeof rbt_nav_rets[0] || WORD_AT(RBT_SCENE)!=100u || !rbt_on())', '    if(k==sizeof rbt_nav_rets/sizeof rbt_nav_rets[0] || !rbt_on())'),
    'nav-found': ('    g_eax=0xFFFFFFFFu;\n    MAPMAKER_GUEST_RET();\n}', '    g_eax=0u;\n    MAPMAKER_GUEST_RET();\n}'),
    'nav-no-ret': ('    g_eax=0xFFFFFFFFu;\n    MAPMAKER_GUEST_RET();\n}', '    g_eax=0xFFFFFFFFu;\n}'),
    'music-kept': ('    if(!rbt_lvl.music++) rbt_log("giant robot death: music change dropped (the map keeps its track)",WORD_AT(g_esp+4u),0);\n    MAPMAKER_GUEST_RET();',
                   '    original();'),
    'music-any-caller': ('|| WORD_AT(g_esp)!=RBT_RET_MUSIC ', ''),
    'music-any-scene': ('WORD_AT(g_esp)!=RBT_RET_MUSIC || WORD_AT(RBT_SCENE)!=100u', 'WORD_AT(g_esp)!=RBT_RET_MUSIC'),
    'event-kept': ('    rbt_remove();\n    MAPMAKER_GUEST_RET();\n}', '    rbt_remove();\n    original();\n}'),
    'event-any-caller': ('|| WORD_AT(g_esp)!=RBT_RET_EVENT ', ''),
    'event-any-scene': ('WORD_AT(g_esp)!=RBT_RET_EVENT || WORD_AT(RBT_SCENE)!=100u', 'WORD_AT(g_esp)!=RBT_RET_EVENT'),
    'event-not-removed': ('    rbt_remove();\n    MAPMAKER_GUEST_RET();', '    MAPMAKER_GUEST_RET();'),
    'remove-twice': ('    rbt_lvl.actor=0;rbt_lvl.removed=1;', '    rbt_lvl.removed=1;'),
    'remove-focus-kept': ('WORD_AT(player+RBT_FOCUS)==ent) WORD_AT(player+RBT_FOCUS)=0;', 'WORD_AT(player+RBT_FOCUS)==ent) {}'),
    'remove-focus-any': ('    if(MAPMAKER_PTR_OK(player,RBT_FOCUS+4u) && WORD_AT(player+RBT_FOCUS)==ent) WORD_AT', '    if(MAPMAKER_PTR_OK(player,RBT_FOCUS+4u)) WORD_AT'),
    'fx-missing': ('    guest_call(sub_002A7F30,2,WORD_AT(cam+RBT_CAMERA_VIEW),RBT_FX_SCALE);\n', ''),
    'fx-no-mode': ('    guest_call(sub_00019D70,1,1u);\n', ''),
    'fx-scale': ('#define RBT_FX_SCALE 0x40200000u', '#define RBT_FX_SCALE 0x3F800000u'),
    'fx-view': ('WORD_AT(cam+RBT_CAMERA_VIEW),RBT_FX_SCALE', 'cam,RBT_FX_SCALE'),
    'fx-any-caller': ('MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==RBT_RET_FX;', 'MAPMAKER_STACK_OK(4);'),
    'fx-alive': (' || WORD_AT(rbt_lvl.state+RBT_DYING)!=1u) return;', ') return;'),
    'fx-after-removal': ('    if(!here || rbt_lvl.removed || !rbt_live()', '    if(!here || !rbt_live()'),
    'fx-split-screen': ('    if(RBT_B(RBT_MODE)==10u && (int32_t)WORD_AT(RBT_PLAYERS)>1) return;', ''),
    'live-stale-state': ('    return rbt_lvl.record && rbt_lvl.state && WORD_AT(RBT_STATE)==rbt_lvl.state && WORD_AT',
                         '    return rbt_lvl.record && rbt_lvl.state && WORD_AT'),
    'live-any-scene': ('WORD_AT(RBT_STATE)==rbt_lvl.state && WORD_AT(RBT_SCENE)==100u && rbt_on();', 'WORD_AT(RBT_STATE)==rbt_lvl.state && rbt_on();'),
    'reset-missing': ('    guest_call(sub_00292550,0);\n', ''),
    'reset-any-scene': ('WORD_AT(RBT_STATE)!=rbt_lvl.state || WORD_AT(RBT_SCENE)!=100u || !rbt_on()) return;\n    guest_call(sub_00292550',
                        'WORD_AT(RBT_STATE)!=rbt_lvl.state || !rbt_on()) return;\n    guest_call(sub_00292550'),
    'reset-stale': ('    if(!rbt_lvl.state || WORD_AT(RBT_STATE)!=rbt_lvl.state || WORD_AT(RBT_SCENE)!=100u || !rbt_on()) return;\n    guest_call(sub_00292550',
                    '    if(!rbt_lvl.state || WORD_AT(RBT_SCENE)!=100u || !rbt_on()) return;\n    guest_call(sub_00292550'),
    'reset-keeps-removed': ('    guest_call(sub_00292550,0);\n    rbt_lvl.removed=0;', '    guest_call(sub_00292550,0);'),
    'hud-own-bar': ('    WORD_AT(player+RBT_FOCUS)=0;                     /* never the stock bar', '    (void)0;                     /* never the stock bar'),
    'hud-keeps-cleared': ('    original();\n    WORD_AT(player+RBT_FOCUS)=focus;\n}', '    original();\n}'),
    'hud-any-robot': ('       WORD_AT(a+RBT_ACTOR_RECORD)!=rbt_lvl.record || !rbt_play())', '       !rbt_play())'),
    'hud-any-model': ('       RBT_H(focus+RBT_ENT_MODEL)!=RBT_MODEL || !MAPMAKER_PTR_OK', '       !MAPMAKER_PTR_OK'),
    'hud-any-scene': ('       WORD_AT(a+RBT_ACTOR_RECORD)!=rbt_lvl.record || !rbt_play())', '       WORD_AT(a+RBT_ACTOR_RECORD)!=rbt_lvl.record || !rbt_on())'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('robot.exe' if os.name == 'nt' else 'robot')
    source = ROOT / 'tests/mapmaker/mapmaker_robot.c'
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
def test_mapmaker_robot(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_REAL_BOSS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker robot ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_no_guest_code_table_and_no_save_byte():
    """Wrappers only; the map container is only read."""
    assert not (ROOT / 'config/guest-code-patches/robot.json').exists()
    text = INC.read_text(encoding='utf-8')
    assert not re.findall(r'RBT_[BH]\([^)]*\)\s*=[^=]', text)


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


def calls(f):
    return {i.address for i in decoded() if i.call and target(i) == f}


def direct_graph():
    graph = {}
    for i in decoded():
        if i.call:
            imm = [op[1] for op in i.ops if op[0] == 'imm']
            if imm:
                graph.setdefault(i.function, set()).add(imm[0])
    return graph


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


HANDLER, MESSAGE = 0x00293730, 0x00294520
CALLBACKS = (0x000BC470, 0x002926B0, 0x002936C0)
ROBOT = (HANDLER, MESSAGE, 0x002926B0, 0x002936C0, 0x00292BF0, 0x00292FA0, 0x00293100, 0x00293660)


def test_robot_data():
    rd = xbe_reader()
    voice, model, lod, flags = struct.unpack_from('<4I', rd(0x0053A3CC + 0x42 * 0x30, 16), 0)
    assert (model, lod) == (0x5F2, 0xFFFFFFFF) and flags & 0x2000000
    assert voice == struct.unpack_from('<I', rd(0x0053A3CC + 0xA8 * 0x30, 4))[0]   # the Arcade giant robot's
    ids = [struct.unpack_from('<I', rd(0x00521A18 + i * 0x30 + 4, 4))[0] for i in range(150)]
    assert 0x42 not in ids and ids.index(0xA8) == 90                  # no Arcade row; the stand-in has one
    assert rd(0x002BE29C + 0x42 - 0x0A, 1)[0] == 1                   # sub_002BE270(0x42) == 0
    assert rd(0x0049DC78, 30) == b'pak/stream/lv14/anim/lv14.pak\0'
    lv14 = anim_table(rd, 0x0047E938, 0x29)
    assert (lv14['giantrob_startthroughdoor'], lv14['giantrob_walkcycle'], lv14['giantrob_death']) == (0x807, 0x808, 0x809)
    others = (anim_table(rd, 0x0048C1B0, 0x10), anim_table(rd, 0x0048C738, 0x12), anim_table(rd, 0x00484F68, 0x14))
    assert not set(lv14.values()) & set().union(*(set(t.values()) for t in others))   # all may be mounted
    # sub_0025F720: only scene 14 reaches the robot case; scene 100 is past the table
    jt = [struct.unpack_from('<I', rd(0x0025F7C0 + 4 * k, 4))[0] for k in range(6)]
    assert [s for s in range(1, 0x52) if jt[rd(0x0025F7D8 + s - 1, 1)[0]] == 0x0025F760] == [14]
    # HUD: model 0x5F2 -> the case at 0x0014D06F (no scene test), label 0x163D
    idx = rd(0x0014D174 + 0x5F2 - 0x5CB, 1)[0]
    assert struct.unpack_from('<I', rd(0x0014D158 + 4 * idx, 4))[0] == 0x0014D06F
    # handler state table: 7 states, 0 = intro, 1 = door entry, 6 = death
    st = struct.unpack('<7I', rd(0x002944FC, 28))
    assert st[0] == 0x00293847 and st[1] == 0x002938B4 and st[6] == 0x00293FB3


def test_robot_registration_and_dispatch():
    a = by_address()
    assert ('imm', 0x0049DC78) in a[0x001CA316].ops and a[0x001CA316].function == 0x001CA2F0
    assert target(a[0x001CA31B]) == 0x00062260 and ('imm', 0x0047E938) in a[0x001CA32A].ops
    assert ('imm', 0x29) in a[0x001CA328].ops and target(a[0x001CA334]) == 0x0005B980
    # class 0x80000 (AI +0x10 == 0): sub_00293730(entity) and the message handler sub_00294520
    assert ('imm', 0x80000) in a[0x0027DDF5].ops and a[0x0027DDF5].function == 0x0027D590
    assert calls(HANDLER) == {0x0027DE18} and a[0x0027DE18].function == 0x0027D590
    assert ('imm', 0x80000) in a[0x0027D402].ops and calls(MESSAGE) == {0x0027D447}
    # the handler reads the entity at [esp+0x88] (0x74 frame, 4 pushes) and the state unchecked
    assert ('imm', 0x74) in a[0x00293730].ops and mem(a[0x00293737])[0][4] == 0x88
    assert mem(a[0x002937DC])[0][4] == 0x0078B1DC and a[0x002937E2].mnemonic == 'mov'
    # state 0: callbacks, health 55, story focus; state 1: 0x807 until its end
    assert ('imm', 0x000BC470) in a[0x00293858].ops and ('imm', 0x002926B0) in a[0x002938EE].ops
    assert ('imm', 0x002936C0) in a[0x00293923].ops
    assert mem(a[0x0029388C])[0][4] == 0x004785CC and mem(a[0x002938A9])[0][4] == 0xFF4
    assert ('imm', 0x807) in a[0x002938B4].ops and target(a[0x002938DE]) == 0x00059DD0


def test_robot_level_state():
    a = by_address()
    assert target(a[0x0025F760]) == 0x0028D6D0 and target(a[0x0025F765]) == 0x00292630
    assert target(a[0x0025F76A]) == 0x002A62C0
    assert calls(0x00292630) == {0x0025F765} and calls(0x0025F720) == {0x0015E818}
    # sub_00292630: [0x0078B1DC] = sub_0003DFA0(0x4C, 1), then the field init sub_00292550
    assert ('imm', 1) in a[0x00292630].ops and ('imm', 0x4C) in a[0x00292632].ops
    assert target(a[0x00292634]) == 0x0003DFA0 and mem(a[0x0029263C])[0][4] == 0x0078B1DC
    assert a[0x00292641].mnemonic == 'jmp' and target(a[0x00292641]) == 0x00292550
    assert ('imm', 0x7D00) in a[0x002A62C2].ops and mem(a[0x002A62CF])[0][4] == 0x0078B0D0
    # sub_00292550 only writes the block (no scene, no call); +0x48 starts at 0
    assert not scene_readers({0x00292550}) and not any(i.call for i in decoded() if i.function == 0x00292550)
    assert mem(a[0x00292622])[0][4] == 0x48
    # no robot function reads the split Crowe's block or the walker/skeleton states
    for g in (0x0075F5E8, 0x0078B1E4, 0x0078B1E8, 0x0078B1EC):
        assert not [i for i in decoded() if i.function in ROBOT and any(op[0] == 'mem' and not op[1] and op[4] == g for op in i.ops)]


def test_robot_scene_events_and_tags():
    a = by_address()
    graph = direct_graph()
    # the fight reads no scene; its only direct callee that does is the level event
    assert not scene_readers(set(ROBOT))
    assert scene_readers(graph[HANDLER]) == {0x002274C0}
    for f in (MESSAGE, *CALLBACKS):
        assert not scene_readers(graph.get(f, set()) | {f})
    seen = reach(graph, [HANDLER, MESSAGE, *CALLBACKS])
    assert len(seen) > 100
    sites = {i.address for i in decoded() if i.call and i.function in seen and target(i) == 0x002274C0}
    assert sites == {0x002944E0} and ends(a, 0x002944E0) == 0x002944E5
    assert ('imm', 2) in a[0x002944DE].ops and ('imm', 0x3F000000) in a[0x002944D9].ops
    assert mem(a[0x002944EE])[0][4] == 0xFF4                        # the focus is cleared after it
    # the music change right before it: sub_000462B0(0) = [0x004D1850] = track
    assert target(a[0x002944D4]) == 0x000462B0 and ends(a, 0x002944D4) == 0x002944D9
    assert mem(a[0x000462B4])[0][4] == 0x004D1850
    # it runs when the death animation has ended (sub_00059DD0 on entity +4), every frame
    assert target(a[0x002944C7]) == 0x00059DD0 and '0x2944f4' in a[0x002944D1].text
    # no tagged-object lookup is reachable; no height is forced; no overhead meter record
    assert not {i.address for i in decoded() if i.call and i.function in seen and target(i) == 0x0023FF20}
    for i in decoded():
        if i.function in ROBOT and i.mnemonic in ('mov', 'movss', 'fstp') and i.ops and i.ops[0][0] == 'mem':
            assert not (i.ops[0][4] == 0x68 and i.ops[0][1] not in ('esp', 'ebp')), hex(i.address)
    assert not {i.function for i in decoded() if i.call and target(i) == 0x0012C800} & set(ROBOT)


def test_robot_death_message():
    a = by_address()
    # message 2: unless already state 6, +0x48 = 1, death anim 0x809, AI +0x9D0 = 6, -1 (no generic death)
    assert mem(a[0x0029454C])[0][4] == 0x20 and ('imm', 6) in a[0x00294550].ops
    assert mem(a[0x00294697])[0][4] == 0x48 and mem(a[0x00294687])[0][4] == 0x0078B1DC
    assert ('imm', 0x809) in a[0x002946F9].ops and target(a[0x002946FF]) == 0x0005F460
    assert mem(a[0x0029470F])[0][4] == 0x9D0
    assert a[0x00294728].mnemonic == 'or' and ('imm', 0xFFFFFFFF) in a[0x00294728].ops


def test_robot_pad_lookups():
    """The walk's hangar pad ids: seven existence tests, two index fetches after a found id."""
    a = by_address()
    sites = {i.address for i in decoded() if i.call and i.function == HANDLER and target(i) == 0x0009CC40}
    tests = {0x00293944, 0x00293BC4, 0x00293C0B, 0x00293C26, 0x00293C5F, 0x00293C99, 0x00293CCB}
    assert sites == tests | {0x00293981, 0x00293CE0}
    assert sorted(ends(a, x) for x in tests) == [0x00293949, 0x00293BC9, 0x00293C10, 0x00293C2B, 0x00293C64, 0x00293C9E, 0x00293CD0]
    for x in tests:                                                  # add esp, 4; test eax, eax; jl/jge
        r = ends(a, x)
        assert a[r].mnemonic == 'add' and a[ends(a, r)].mnemonic == 'test'
        assert a[ends(a, ends(a, r))].mnemonic in ('jl', 'jge')
    # the fetches: 0x0029397C only after the 0x00293944 test found it; 0x00293CE0 from the push blocks
    branches = {i.address: target(i) for i in decoded() if i.function == HANDLER and i.branch}
    assert [s for s, t in branches.items() if t == 0x0029397C] == [0x0029394E] and a[0x0029394E].mnemonic == 'jge'
    assert sorted(s for s, t in branches.items() if t == 0x00293CE0) == [0x00293BF2, 0x00293C1C, 0x00293C37, 0x00293C83, 0x00293CC4]
    assert branches[0x00293BCE] == 0x00293BED and branches[0x00293C69] == 0x00293C7E and branches[0x00293CA3] == 0x00293CBF
    assert a[0x00293C15].mnemonic == 'jl' and a[0x00293C30].mnemonic == 'jl' and a[0x00293CD5].mnemonic == 'jl'
    # sub_0009CC40: a binary search of the pad table [0x00715F2C] (+0 id, stride 0x20)
    assert mem(a[0x0009CC6B])[0][4] == 0x00715F2C and mem(a[0x0009CC5A])[0][4] == 0x007B6664
    # missing id -> the player's own pad (+0x678 of the first player's actor)
    assert mem(a[0x00293C76])[0][4] == 0x678 and mem(a[0x00293C6B])[0][4] == 0x007B0C48


def test_robot_explosion_draw():
    a = by_address()
    # the frame draws the robot's particles only in scene 14 (sub_00292650, its only caller)
    assert calls(0x00292650) == {0x001CEE58} and ('imm', 0xE) in a[0x001CEE49].ops
    assert ('imm', 0x4D454348) in a[0x00292650].ops and target(a[0x00292655]) == 0x0023FF20   # 'MECH', null-checked
    assert a[0x0029265F].mnemonic == 'je' and mem(a[0x0029266A])[0][4] == 0x48
    assert mem(a[0x00292676])[0][4] == 0x007DE455 and mem(a[0x0029267F])[0][4] == 0x00790950
    assert target(a[0x00292688]) == 0x00019D70 and mem(a[0x00292692])[0][4] == 0x208
    assert ('imm', 0x40200000) in a[0x00292698].ops and target(a[0x0029269E]) == 0x002A7F30
    assert ('imm', 0xC) in a[0x002926A3].ops                       # cdecl: 1 + 2 args
    assert calls(0x00126560) == {0x001CEE87} and ends(a, 0x001CEE87) == 0x001CEE8C
    # sub_002A7F30 only draws: its stores are to its own frame
    for i in decoded():
        if i.function == 0x002A7F30 and i.mnemonic in ('mov', 'movss', 'fstp') and i.ops and i.ops[0][0] == 'mem':
            assert i.ops[0][1] == 'esp', hex(i.address)


def test_robot_soft_restart_reset():
    a = by_address()
    # sub_0015E4F0 (story mode) tail-jumps to sub_0025F6A0, which resets the robot only in scene 14
    assert a[0x0015E7B1].mnemonic == 'jmp' and target(a[0x0015E7B1]) == 0x0025F6A0
    assert ('imm', 0xE) in a[0x0025F6C8].ops and target(a[0x0025F6D2]) == 0x00292550
    assert calls(0x00292550) == {0x0025F6D2}
    # the restart path of the scene loop (sub_001CDD60) runs after a level init, apart from it
    assert target(a[0x001CDD74]) == 0x0015E4F0 and calls(0x0015E4F0) == {0x001CDD74}
    assert calls(0x001CDD60) == {0x0003D4E1} and calls(0x001CDB80) == {0x0003D590}
    assert a[0x0003D4BB].mnemonic == 'je' and '0x3d4f0' in a[0x0003D4BB].text


def test_robot_bar_label_site():
    a = by_address()
    assert ('imm', 0x163D) in a[0x0014D076].ops and target(a[0x0014D07B]) == 0x0003D7B0
    assert mem(a[0x0014D086])[0][4] == 0xFF4 and target(a[0x0014D0B1]) == 0x00139740
