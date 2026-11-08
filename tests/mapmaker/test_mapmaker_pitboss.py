"""MapMaker pit skeleton (story assets wave 3): the mission 6 pit fight and its ragbugs.

Compiles src/interface/pc_mapmaker_pitboss.inc against fake guest memory
(tests/mapmaker/mapmaker_pitboss.c, with mutations that must be caught) and,
when the private XBE, its decode and the story paks are available, proves the
sites it relies on: the fight's states, return addresses, arena constants and
their readers, the props and their paks, the rim pads, the callbacks, the
records the catacombs level spawns and the anims the restart unmaps.
"""
import os
from pathlib import Path
import re
import struct
import subprocess
import zlib
from collections import defaultdict

import pytest

from native_compiler import native_compiler
from mapmaker_decoded import decoded, _private

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_pitboss.inc'
FIXTURE = ROOT / 'tests/mapmaker/mapmaker_pitboss.c'

MUTATIONS = {
    'any-scene': ("WORD_AT(PIT_SCENE)==100u && PIT_B(PIT_MODE)==10u; }", 'PIT_B(PIT_MODE)==10u; }'),
    'limits-ignored': ('static int pit_on(void) { mapmaker_limits_read(); return mapmaker_limits.limits; }',
                       'static int pit_on(void) { mapmaker_limits_read(); return 1; }'),
    'princess-any-scene': ("PIT_ID) || WORD_AT(PIT_SCENE)!=100u || !pit_on()) { original();return; }",
                           "PIT_ID) || !pit_on()) { original();return; }"),
    'preset-set': ('    if(PIT_H(info+0x14u)!=PIT_SET_MAP) return 0;\n', ''),
    'mount-not-kept-open': ('guest_call(sub_000622D0,3,p,0u,1u)', 'guest_call(sub_000622D0,3,p,1u,1u)'),
    'mount-table-full': ('(used=WORD_AT(PIT_MOUNTS))+PIT_MOUNT_KEEP>PIT_MOUNT_MAX', '(used=WORD_AT(PIT_MOUNTS))>PIT_MOUNT_MAX'),
    'mount-skip-ignored': ('    if(WORD_AT(PIT_MOUNT_SKIP)) { original();return; }\n', ''),
    'textures-unmarked': ('|| !(PIT_B(r+2u)&0x10u)) continue;', ') continue;'),
    'textures-mark-kept': ('            PIT_B(r+2u)&=(uint8_t)~0x10u;\n', ''),
    'level-statics-kept': ('    memcpy((void *)((uintptr_t)MAPMAKER_FX+g_xbox_mem_offset),cur,MAPMAKER_FX_SIZE);\n    WORD_AT(PIT_STATE)=home;\n    mapmaker_fx_leave(prev);\n    return ok;',
                           '    WORD_AT(PIT_STATE)=home;\n    mapmaker_fx_leave(prev);\n    return ok;'),
    'no-home': ('    if(pit_lv.n && !WORD_AT(PIT_STATE)) WORD_AT(PIT_STATE)=pit_lv.k[0].state;', ''),
    'start-any-class': ('    if(id!=PIT_ID || WORD_AT(rec+4u)!=PIT_CLASS) return;', '    if(id!=PIT_ID) return;'),
    'no-boss-flag': ('pit_boss_flag(WORD_AT(rec+8u)-WORD_AT(PIT_SAI_BASE)-1u):0u;', '0u:0u;'),
    'rebase-sign-ignored': ('pit_lv.base[i]+(float)pit_rebase[i].sign*d', 'pit_lv.base[i]+d'),
    'rebase-not-restored': ('    pit_back(saved);\n    pit_out(&s);', '    pit_out(&s);'),
    'rebase-axis': ('{0x0049F498u,0,1},{0x0049F494u,1,1},{0x0049F37Cu,2,1},', '{0x0049F498u,0,1},{0x0049F494u,1,1},{0x0049F37Cu,0,1},'),
    'state-not-swapped': ('    WORD_AT(PIT_STATE)=k->state;\n    pit_lv.cur=k;', '    pit_lv.cur=k;'),
    'statics-not-swapped': ('    s->fx=mapmaker_fx_enter(&k->fx);', '    s->fx=mapmaker_fx_enter(0);'),
    'timer-runs': ('if(st<PIT_DEATH) pit_setf(k->state+PIT_ST_TIMER,0.0f);', '(void)st;'),
    'pads-on-the-heap': ('    p=base+idx*0x20u;', '    p=base+(idx+0x1F40u)*0x20u;idx+=0x1F40u;'),
    'pads-over-the-map': ('    if(WORD_AT(PIT_PAD_COUNT)>idx || ', '    if('),
    'pads-not-moved': ('pit_setf(r+0xCu,pit_pad_xyz[i][0]+k->dx);', 'pit_setf(r+0xCu,pit_pad_xyz[i][0]);'),
    'not-ready-runs': ("if(!k->ready && !(k->ready=pit_models_ready(&missing))) { pit_remove(", "if(0) { pit_remove("),
    'no-block-runs': ('    if(!(k=pit_of(ent))) { pit_remove(ent,"pit skeleton without a block this level: removed");MAPMAKER_GUEST_RET();return; }',
                      '    if(!(k=pit_of(ent))) { original();return; }'),
    'message-no-swap': ('    pit_in(k,&s);\n    sub_0028C560();\n    pit_out(&s);', '    (void)s;(void)k;\n    sub_0028C560();'),
    'callback-owner-ignored': ('    k=pit_owner(obj);\n', '    k=0;(void)obj;\n'),
    'callback-damage-record': ('o=r && MAPMAKER_PTR_OK(r,0x2Cu)?WORD_AT(r):0u;', 'o=r;'),
    'logic-any-site': ('((ret=WORD_AT(g_esp))!=PIT_RET_NMME0 && ret!=PIT_RET_NMME1)', '((ret=WORD_AT(g_esp))==0u)'),
    'logic-always-1': ('g_eax=ret==PIT_RET_NMME0?0u:', 'g_eax=1u||ret==PIT_RET_NMME0?1u:'),
    'logic-outside-fight': ('    if(!k || !MAPMAKER_STACK_OK(8) || ((ret', '    if(!MAPMAKER_STACK_OK(8) || ((ret'),
    'event-any-site': ('WORD_AT(g_esp)!=PIT_RET_EVENT', 'WORD_AT(g_esp)==0u'),
    'failure-kept': ("    if(pit_lv.fails++<4u) pit_log(\"pit skeleton hostage failure dropped\",WORD_AT(g_esp+4u),0);\n    MAPMAKER_GUEST_RET();",
                     "    original();"),
    'bug-no-rebase': ('    pit_move(&pit_lv.k[b->owner],saved);\n    original();\n    pit_back(saved);', '    (void)saved;\n    original();'),
    'bug-any-owner': ('        if(b<0 || d<best) { b=(int)k;best=d; }', '        if(b<0) { b=(int)k;best=d; }'),
    'flock-refilled': ('        } else if(!WORD_AT(s) && b->filled) b->ent=0;', '        } else if(!WORD_AT(s) && b->filled) WORD_AT(s)=b->ent;'),
    'flock-flag-kept': ('            WORD_AT(b->ent+0x28u)&=0xFFFFFFFEu;\n', ''),
    'hull-shown': ('        pit_hull(k);\n', ''),
    'hull-hidden-every-frame': ('|| (PIT_B(inst+0x1F6u)&8u)) return;', ') return;'),
    'flock-noted-early': ('    if(waiting) return;\n', ''),
    'mouth-not-held': ('        pit_mouth(k,ent,st);\n', ''),
    'mouth-no-ahead': ('    x=pit_getf(ent+PIT_ENT_POS)+PIT_MOUTH_AHEAD*pit_sin(yaw);', '    x=pit_getf(ent+PIT_ENT_POS);'),
    'mouth-nan-tilt-kept': ('    if(!pit_finite(pit_getf(b+0xC0u))) pit_setf(b+0xC0u,0.0f);\n', ''),
    'hull-no-callback': ('WORD_AT(h+0x19Cu)!=PIT_BOTTLE_CB) WORD_AT(h+0x19Cu)=PIT_BOTTLE_CB;', '0) WORD_AT(h+0x19Cu)=PIT_BOTTLE_CB;'),
    'hull-full-share': ('before-(before-after)*PIT_BODY_SHARE', 'after'),
    'rooms-not-given': ('        pit_props_rooms(k,ent);\n', ''),
    'rooms-no-room-id': ('        WORD_AT(o+0x34u)=WORD_AT(ent+0x38u);\n', ''),
    'restart-props-kept': ('            if(o) { guest_call(sub_000A9070,1,o);WORD_AT(pit_lv.k[k].state+f[i])=0u; }', '            (void)o;'),
    'restart-anims-lost': ('        PIT_H(PIT_ANIM_MAP+id*2u)=(uint16_t)slot;\n', ''),
    'restart-any-anim': ('                   !(WORD_AT(PIT_ANIM_WORDS+slot*8u+4u)&PIT_ANIM_DROPPED)) continue;', '                   0) continue;'),
    'restart-no-again': ('        if(!pit_again(p)) { memset(p,0,sizeof *p);break; }', '        if(0) break;'),
    'hud-bar-kept': ('    WORD_AT(player+PIT_FOCUS)=0;\n    original();', '    original();'),
    'meter-sub-hidden': ('|| k->boss==PIT_BOSS_SUB ||', '||'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    inc = INC.read_text(encoding='utf-8')
    if mutation in MUTATIONS:
        old, new = MUTATIONS[mutation]
        assert inc.count(old) == 1, mutation
        inc = inc.replace(old, new)
    (tmp_path / INC.name).write_text(inc, encoding='utf-8')
    exe = tmp_path / ('pitboss.exe' if os.name == 'nt' else 'pitboss')
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


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_pitboss(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert result.stdout.strip() == 'ok'
        err = result.stderr
        assert '[mapmaker] pitboss: mounted pak/story/l_17_st.pak for the pit skeleton\'s props' in err
        assert '[mapmaker] pitboss: mounted pak/storymem/l_17_stm.pak for the pit skeleton\'s props' in err
        assert 'pitboss: pit skeleton blocks allocated (first, count) (0x700000, 0x2)' in err
        assert 'pit skeleton 1 placed at (4.00, 2.00, -6.00): the catacombs arena moved by (15.80, 102.28, -62.29)' in err
        assert "pitboss: ragbug joins pit skeleton 2's flock (slot 0)" in err
        assert 'pit skeleton killed: its level event dropped' in err
        assert 'level restart: pit skeleton blocks set up again, props deleted, lv17_4 anims mapped again (blocks, anims) (0x2, 0xB)' in err
        assert 'ragbug without a pit skeleton this level' in err
        assert '[mapmaker] pitboss: prop model 0x249: not loaded, file read, textures left to its creation' in err
        assert '[mapmaker] pitboss: prop model 0x24B: not loaded, file read, textures registered by the load pass' in err
        assert 'pit skeleton 1: gas bottle 0x24A made (object 0x452000, model 0x24A, 0 listed rooms, first 0) at (12.12, 0.00, 0.00)' in err
        assert "pit skeleton 1: flock complete, 2 ragbugs parked; they rise at the fight's launches (state 10)" in err
        assert 'collision hull 0x249 hidden as the story' in err
        assert 'released together' not in err
        assert 'pose computed' not in err
        assert 'pit skeleton 1: gas bottle held at the mouth (4.00, 13.00, -3.50)' in err
        assert 'pit skeleton 1: gas bottle hit through the body (type 0x21' in err
        assert 'pit skeleton 1: gas bottle hit (type 0x21, kind 0x0, amount 1.00): health 8.00 -> 5.33' in err
        assert 'pit skeleton 1: ragbug 0 phase 0 at (3.00, 2.00, -5.00), boss state 1' in err
        assert 'pit skeleton 1 trace: state 1, boss (4.00, 2.00, -6.00) rooms 0, bottle 0x0, health 0.00, ragbug phases -1 -1 -1 -1 -1 -1' in err
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_wiring():
    """The area is included after the creatures area, every hook is declared and stubbed, and the realbosses
    skeleton claim leaves the carrier 0x31 (model 0x5E9) to this area."""
    guest = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    header = (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    hooks = re.findall(r'^void (pc_mapmaker_pitboss_\w+)\(void \(\*original\)\(void\)\)', INC.read_text(encoding='utf-8'), re.M)
    assert len(hooks) == 22
    for h in hooks:
        assert 'void %s(void (*original)(void));' % h in header, h
        assert 'void %s(void (*original)(void)) { original(); }' % h in guest, h
    assert guest.index('#include "pc_mapmaker_creatures.inc"') < guest.index('#include "pc_mapmaker_pitboss.inc"')
    rb = (ROOT / 'src/interface/pc_mapmaker_realbosses.inc').read_text(encoding='utf-8')
    assert '#define RBS_PIT_ID 0x31u' in rb
    assert 'RBS_H(a+RBS_ACTOR_CHAR)!=RBS_PIT_ID && MAPMAKER_PTR_OK(ent,0x100u)' in rb
    d = (ROOT / 'src/interface/pc_mapmaker_creatures.def').read_text(encoding='utf-8')
    assert 'MAPMAKER_CREATURE(164u,0x31u,0x28u,0xA6u,"Pit Skeleton",0x10000000u,0x00020000u,0x0049D59Cu,"lv17_4",0x0048E298u,0x26u,16u,12u,0)' in d
    assert 'MAPMAKER_CREATURE(165u,0xB0u,0xFFu,0x4Eu,"Ragbug",0x10000000u,0x00200000u,0x0049D59Cu,"lv17_4",0x0048E298u,0x26u,24u,4u,0)' in d


# ---- decode, XBE data, story paks -----------------------------------------

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


def target(i):
    m = re.search(r'0x([0-9a-f]+)', i.text)
    return int(m.group(1), 16) if m else None


def imms(i):
    return [op[1] for op in i.ops if op[0] == 'imm']


def by_address():
    return {i.address: i for i in decoded()}


def inc_table():
    s = INC.read_text(encoding='utf-8')
    body = s[s.index('pit_rebase[]={'):s.index('#define PIT_REBASE_N')]
    return [(int(a, 16), int(x), int(sg)) for a, x, sg in re.findall(r'\{0x([0-9A-F]+)u,(\d),(-?1)\}', body)]


def calls(i, t):
    return i.call and target(i) == t


def test_fight_states_and_sites():
    """sub_0029FE00 sends every scene but 1 to sub_0029E0A0; its 17 states (table 0x0029FDBC); the return
    addresses the area tests; the NMME item, the hostage tag and the props it creates."""
    a = by_address()
    rd = xbe()
    table = [struct.unpack_from('<I', rd(0x0029FDBC + 4 * k, 4))[0] for k in range(17)]
    assert table[0] == 0x0029E8BC and table[14] == 0x0029FB23 and table[16] == 0x0029FC55
    assert calls(a[0x0029EDA0], 0x00234270) and a[0x0029EDA0].address + 5 == 0x0029EDA5
    assert calls(a[0x0029EECC], 0x00234270) and a[0x0029EECC].address + 5 == 0x0029EED1
    assert calls(a[0x0029FC33], 0x002274C0) and 2 in imms(a[0x0029FC2B])
    assert calls(a[0x0029FD10], 0x00212160) and calls(a[0x0029ED82], 0x00234190)
    assert 0x4E4D4D45 in imms(a[0x0029ED75]) and 0x50524A42 in imms(a[0x0029E702])
    body = [i for i in decoded() if i.function == 0x0029E0A0]
    body.sort(key=lambda i: i.address)
    made = []                                            # the last model id pushed before each creation
    for k, c in enumerate(body):
        if calls(c, 0x000A6F90):
            ids = [imms(q)[0] for q in body[max(0, k - 12):k] if q.mnemonic == 'push' and imms(q) and 0x240 <= imms(q)[0] < 0x250]
            made.append(ids[-1])
    assert sorted(set(made)) == [0x246, 0x249, 0x24A, 0x24B]
    # the 13 rim pads looked up by sub_0009CC40 and kept at +0x4BF0..+0x4C20
    pads = [imms(a[x])[-1] for x in range(0x0029E919, 0x0029E999 + 1) if x in a and a[x].mnemonic == 'mov' and imms(a[x])]
    assert pads == list(range(0x4B5, 0x4C2))
    assert 0x4BF0 in imms(a[0x0029E901]) and 0x4C24 in imms(a[0x0029E9C1]) and calls(a[0x0029E9AD], 0x0009CC40)
    # the rack timer and its stages; the hostage-eating request (state 12) only from sub_0028B780 stage 4
    assert 0x423C in [op[4] for op in a[0x0029E566].ops if op[0] == 'mem']
    assert a[0x0028BA03].text.endswith('ebx') and 0xC in imms(a[0x0028B9A1])
    # the flock launch the area calls twice at the fight's start: state 10's call, the boss entity as its only
    # argument ([esp + 0x20] after 4 pushes and 0xC), up to 3 parked slots (+0x24 == 0) per call, +0x24 = 1
    assert calls(a[0x0029F647], 0x0028A0A0) and a[0x0029F646].text.endswith('esi')
    assert 'esp + 0x20' in a[0x0028A0CF].text and 3 in imms(a[0x0028A0B7]) and 0x4A54 in [op[4] for op in a[0x0028A0C0].ops if op[0] == 'mem']
    assert 0x4A54 in [op[4] for op in a[0x0028A23D].ops if op[0] == 'mem'] and a[0x0028A23D].text.endswith('ebp')
    # the pose: sub_0008F6B0 (the prop draw) computes it with sub_000A1B80(prop) only when the prop's word +0x14
    # lacks the view slot's bit (0x0008F6F7..0x0008F700); sub_000A1B80 sets that bit (0x000A1BC5); stock
    # sub_0018F8E0 calls it before reading bones (0x0018F90B); the damage code calls the object's +0x19C (0x000BB638)
    assert calls(a[0x0008F700], 0x000A1B80) and 0x14 in [op[4] for op in a[0x0008F6F7].ops if op[0] == 'mem']
    assert 0x214 in [op[4] for op in a[0x0008F6EA].ops if op[0] == 'mem'] and a[0x000A1BC5].mnemonic == 'mov' and 0x14 in [op[4] for op in a[0x000A1BC5].ops if op[0] == 'mem']
    assert calls(a[0x0018F90B], 0x000A1B80) and 0x19C in [op[4] for op in a[0x000BB62D].ops if op[0] == 'mem']
    assert a[0x000BB638].call and 0x19C in [op[4] for op in a[0x0009E728].ops if op[0] == 'mem']
    # the dives' targets: a rim pad index from +0x4BF0 through sub_0009A420 (0x0028B08F/0x0028B097), which reads
    # [0x00715F2C] + index*0x20 below 0x1F40 but sends 0x1F40..0x243F (scene 100) to the cover pool, and keeps a pad
    # only when sub_0009A330 accepts it (+4 bit 0, id 0x3E9..0x2328): the rim pads go in the pool's tail
    assert 0x4BF0 in [op[4] for op in a[0x0028B08F].ops if op[0] == 'mem'] and calls(a[0x0028B097], 0x0009A420)
    assert 0x1F40 in imms(a[0x0009A43D]) and 0x2440 in imms(a[0x0009A45B]) and 0x64 in imms(a[0x0009A44B])
    assert 0x00715F2C in [op[4] for op in a[0x0009A47D].ops if op[0] == 'mem'] and calls(a[0x0009A48B], 0x0009A330)
    assert 0x3E9 in imms(a[0x0009A376]) and 0x2329 in imms(a[0x0009A37E])
    # the hull is hidden in the story by its slot-4 callback: sub_0028C540 -> sub_000942B0(instance, 8) sets
    # instance +0x1F6 |= 8, and the prop draw sub_000A2740 skips such a prop (0x000A2829/0x000A28C7)
    assert calls(a[0x0028C54A], 0x000942B0) and 8 in imms(a[0x0028C547])
    assert 0x1F6 in [op[4] for op in a[0x000942B9].ops if op[0] == 'mem']
    for x in (0x000A2829, 0x000A28C7):
        assert a[x].function == 0x000A2740 and 8 in imms(a[x]) and 0x1F6 in [op[4] for op in a[x].ops if op[0] == 'mem']
    # the ragbugs: phase 4 patrols the rim pads and turns to 5 (dive) only for a player within 5 (squared, x/z) of the
    # bug (0x0028B1D1); a bug explodes (6) when its health is 1.0, which its damage callback sets (sub_0028B5F0)
    assert 0x004760CC in [op[4] for op in a[0x0028B1D1].ops if op[0] == 'mem'] and 5 in imms(a[0x0028B236])
    assert 6 in imms(a[0x0028AF55]) and 0x00475C78 in [op[4] for op in a[0x0028AF31].ops if op[0] == 'mem']
    # the boss's own spawn records: class 0x20000 and 0x200000 reach these handlers
    assert calls(a[0x0027DDD3], 0x0029FE00) and calls(a[0x0027DEDE], 0x0028B610)
    gen = [i for i in decoded() if i.function == 0x0029FE00]
    assert any(i.mnemonic == 'jmp' and target(i) == 0x0029E0A0 for i in gen)


def test_arena_constants():
    """Every rebased constant has its documented stock value, is read by pit code, and no function reachable from
    the pit entries reads it except pit code; the boss point is the one state 0 pins the boss to."""
    rd = xbe()
    f = lambda va: struct.unpack('<f', rd(va, 4))[0]
    rows = inc_table()
    assert len(rows) == 34 and len({r[0] for r in rows}) == 34
    assert [round(f(x), 3) for x in (0x0049F498, 0x0049F494, 0x0049F37C)] == [-11.8, -100.279, 56.295]
    assert round(f(0x0049F380), 3) == 11.8 and round(f(0x0049F378), 3) == 100.279   # subtracted: sign -1
    signed = {r[0]: r[2] for r in rows}
    assert signed[0x0049F380] == -1 and signed[0x0049F378] == -1 and sum(1 for r in rows if r[2] < 0) == 2
    centre = (f(0x0049F498), f(0x0049F494), f(0x0049F37C))
    for va, axis, sign in rows:                          # every point is within 30 of the pit, on its axis
        v = sign * f(va)
        assert abs(v - centre[axis]) < 30.0, hex(va)
    readers = defaultdict(set)
    for i in decoded():
        for op in i.ops:
            if op[0] == 'mem' and op[1] is None and op[2] is None and op[4] in signed:
                readers[op[4]].add(i.function)
    pit = {0x0029E0A0, 0x0028A0A0, 0x0028AAA0, 0x0028B610, 0x0028B730, 0x0028B780, 0x00288540}
    seen = reach([0x0029E0A0, 0x0028B610])
    for va in signed:
        assert readers[va] & pit, hex(va)
        assert not (readers[va] - pit) & seen, (hex(va), [hex(x) for x in readers[va] - pit])


def reach(roots):
    calls_, body = defaultdict(set), set()
    for i in decoded():
        body.add(i.function)
        if (i.call or i.mnemonic == 'jmp') and not i.frame():
            t = target(i)
            if t is not None:
                calls_[i.function].add(t)
    seen, todo = set(roots), list(roots)
    while todo:
        f = todo.pop()
        for t in calls_[f]:
            if t in body and t not in seen:
                seen.add(t)
                todo.append(t)
    return seen


def test_block_readers_are_routed():
    """Every function that reads the skeleton block pointer [0x0078B1EC] is a wrapped entry, a callee of the two
    fights, or the init/frame-draw; the pit callbacks are referenced only by the pit fight."""
    wrapped = {0x0029E0A0, 0x0028C560, 0x00289720, 0x00289E20, 0x0028C1C0, 0x0028C390, 0x0028C4A0, 0x0028C2F0,
               0x00289080, 0x00288E20}
    mansion = {0x0029CEF0, 0x00286430, 0x00288BD0}
    other = {0x00287E00, 0x00287860, 0x0029CE00}
    inner = reach(sorted(wrapped | mansion))
    for i in decoded():
        for op in i.ops:
            if op[0] == 'mem' and op[1] is None and op[2] is None and op[4] == 0x0078B1EC:
                assert i.function in wrapped | mansion | other | inner, hex(i.function)
    refs = defaultdict(set)
    for i in decoded():
        for v in imms(i):
            if v in (0x00289720, 0x00289E20, 0x0028C1C0, 0x0028C390, 0x0028C4A0, 0x0028C2F0, 0x00289080, 0x00288E20):
                refs[v].add(i.function)
    assert len(refs) == 8 and all(f == {0x0029E0A0} for f in refs.values())
    # the bottle's damage record holds the hit object at +0 (as sub_0028B5F0's: [record] + 0x1CC)
    a = by_address()
    assert a[0x0028B5F4].text.endswith('dword ptr [eax]') and 0x21 in imms(a[0x0028C2F4])


GAME = _private() / 'game/pak'


def _index(path):
    with open(path, 'rb') as f:
        head = f.read(12)
        offset, length = struct.unpack_from('<II', head, 4)
        f.seek(offset)
        table = f.read(length)
    return {c: (pos, n, cm) for c, pos, n, cm in struct.iter_unpack('<4I', table)}


def test_story_records_pads_and_props():
    """l_17_st: 'PRIN' is the skeleton 0x28 with 0x20000:0x10000000 and 'RAG0'..'RAG5' the ragbug 0xB0 with
    0x200000:0x10000000; the pad file holds the 13 rim pads the area copies; the four props' models are in
    l_17_st (not in chr.pak or l_100) and their textures in l_17_stm; every entry the two paks share with another
    pak is byte-identical there."""
    st, stm = GAME / 'story/l_17_st.pak', GAME / 'storymem/l_17_stm.pak'
    if not st.is_file():
        pytest.skip('needs the story paks')
    rd = xbe()
    raw = st.read_bytes()
    idx = _index(st)
    member = lambda k: struct.unpack_from('<h', rd(0x0053D2D0 + 5 * 0xA4 + 4 + k * 8, 2))[0]
    found = defaultdict(set)
    for c, (pos, n, cm) in idx.items():
        d = raw[pos:pos + n]
        for tag in [b'PRIN'] + [b'RAG%d' % k for k in range(6)]:
            i = d.find(tag[::-1])
            while i >= 0:
                if i >= 8 and (i - 8) % 4 == 0:
                    lo, hi = struct.unpack_from('<II', d, i - 8)
                    slot = struct.unpack_from('<h', d, i - 8 + 0x16)[0]
                    if lo >> 28 == 1 and 0 <= slot < 20:
                        found[tag].add((lo, hi, member(slot)))
                i = d.find(tag[::-1], i + 1)
    assert found[b'PRIN'] == {(0x10000000, 0x20000, 0x28)}
    for k in range(6):
        assert found[b'RAG%d' % k] == {(0x10000000, 0x200000, 0xB0)}
    s = INC.read_text(encoding='utf-8')
    pads = [tuple(float(v) for v in m) for m in re.findall(r'\{(-?[\d.]+)f,(-?[\d.]+)f,(-?[\d.]+)f,(-?[\d.]+)f\}', s)]
    assert len(pads) == 13
    pos, n, _ = idx[0x671173A1]
    d = raw[pos:pos + n]
    at = d.find(struct.pack('<III', 0x4B5, 1, 0x1A))
    assert at > 0
    for k in range(13):
        pid, used, room, x, y, z, yaw = struct.unpack_from('<3I4f', d, at + 0x20 * k)
        assert (pid, used, room) == (0x4B5 + k, 1, 0x1A)
        assert all(abs(p - q) < 0.001 for p, q in zip((x, y, z, yaw), pads[k]))
    # props: prop table rows in l_100 and their files
    l100 = GAME / 'arcade/l_100.pak'
    li = _index(l100)
    with open(l100, 'rb') as f:
        p, n, _ = li[0xD9E2667A]
        f.seek(p)
        tb = f.read(n)
    start = struct.unpack_from('<I', tb, 4)[0] + 8
    props = {}
    for o in range(start, len(tb) - 0x27, 0x28):
        mid, typ, _, path, *_ = struct.unpack_from('<iIiiifiiIi', tb, o)
        props[mid] = (typ, tb[path + 8:tb.index(b'\0', path + 8)].decode('latin1'))
    want = {0x246: 'catacombs/torturerackNoRope', 0x249: 'catacombs/mansionboss_collision',
            0x24A: 'catacombs/gas_bottle', 0x24B: 'catacombs/eyebandage'}
    chr_, mem = _index(GAME / 'chr.pak'), _index(stm)
    tex = lambda i: zlib.crc32(('textures/%06x.xbt' % i).encode()) & 0xFFFFFFFF
    for m, path in want.items():
        assert props[m] == (1, path)
        crc = zlib.crc32('assets/ts/models/{0}/xbox/output/{1}.xbr'.format(path, path.split('/')[-1]).encode()) & 0xFFFFFFFF
        assert crc in idx and crc not in chr_ and crc not in li
        p, n, cm = idx[crc]
        body = raw[p:p + n]
        off, ids = struct.unpack_from('<I', body, 0)[0], []
        while struct.unpack_from('<I', body, off)[0] != 0xFFFFFFFF:
            ids.append(struct.unpack_from('<I', body, off + 4)[0] & 0xFFFFFF)
            off += 16
        assert ids and all(tex(i) in mem for i in ids), hex(m)
    for pak, pi in ((st, idx), (stm, mem)):
        with open(pak, 'rb') as f:
            for other in sorted(GAME.rglob('*.pak')):
                if other.samefile(pak):
                    continue
                try:
                    oi = _index(other)
                except (OSError, struct.error):
                    continue
                common = set(pi) & set(oi)
                if not common:
                    continue
                with open(other, 'rb') as g:
                    for c in common:
                        f.seek(pi[c][0]); g.seek(oi[c][0])
                        assert f.read(pi[c][2] or pi[c][1]) == g.read(oi[c][2] or oi[c][1]), (pak.name, other.name, hex(c))


def test_lv17_4_anims_and_carrier():
    """lv17_4 (table 0x0048E298, 0x26 entries, path 0x0049D59C) entries 16..27 are 0x23B..0x246 and carry the
    restart's 0x4000 flag; the pit code plays exactly those; carrier 0x31 is a soldiermodel_talk copy (model 0x619)
    with no Arcade row and no story set; no game code tests a character for 0x31 or 0xB0."""
    from test_mapmaker_animslots import registrations, table
    rd, regs = registrations()
    by = {p: (t, n) for _, p, t, n in regs}
    assert by['pak/stream/lv17/anim/lv17_4.pak'] == (0x0048E298, 0x26)
    assert rd(0x0049D59C, 32).split(b'\0')[0] == b'pak/stream/lv17/anim/lv17_4.pak'
    names = table(rd, 0x0048E298, 0x26)
    assert [i for _, i in names[16:28]] == list(range(0x23B, 0x247))
    assert names[16][0] == 'princess_cat_cs' and names[27][0] == 'ragbugrightdive'
    for k in range(16, 28):
        assert struct.unpack_from('<I', rd(0x0048E298 + k * 0x18 + 0xC, 4))[0] & 0x4000
    used, seen = set(), reach([0x0029E0A0, 0x0028B610])
    for i in decoded():
        if i.function in seen and i.mnemonic in ('push', 'mov'):
            used |= {v for v in imms(i) if 0x23B <= v <= 0x246}
    assert used == set(range(0x23B, 0x247))
    arcade = {struct.unpack_from('<I', rd(0x00521A18 + i * 0x30 + 4, 4))[0] for i in range(150)}
    model = lambda cid: struct.unpack_from('<i', rd(0x0053A3CC + cid * 0x30 + 4, 4))[0]
    assert 0x31 not in arcade and 0xB0 not in arcade and 0xA6 in arcade and 0x4E in arcade
    assert model(0x31) == 0x619 and model(0x28) == 0x5E9 and model(0xB0) == 0x6CB
    for s in range(27):
        for k in range(20):
            cid = struct.unpack_from('<h', rd(0x0053D2D0 + s * 0xA4 + 4 + k * 8, 2))[0]
            assert cid != 0x31 and (cid != 0xB0 or s == 5)
    for i in decoded():
        if i.mnemonic == 'cmp' and len(i.ops) == 2 and i.ops[0][0] == 'mem' and i.ops[0][4] == 0x48 and i.ops[1][0] == 'imm':
            assert i.ops[1][1] not in (0x31, 0xB0), hex(i.address)
