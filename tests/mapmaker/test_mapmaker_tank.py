"""MapMaker castle tank: the mission 2 tank boss in MapMaker story play.

Compiles src/interface/pc_mapmaker_tank.inc against fake guest memory (with
mutations that must be caught) and, when the private XBE, its disassembly and
the game paks are available, checks the stock facts the hooks rely on: the
tank's resource rows and pak, the object-creation argument layout, the init
and its static state, the fight's start test, the castle arena constants that
are rebased, the gate-area test, the death's park point, the only level
events reachable from the tank's code, and the boss bar's text sites.
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
INC = ROOT / 'src/interface/pc_mapmaker_tank.inc'

MUTATIONS = {
    'always-on': ('    return mapmaker_limits.limits;\n}', '    return 1;\n}'),
    'any-mode': ('return tnk_on() && WORD_AT(TNK_SCENE)==100u && TNK_B(TNK_MODE)==10u; }', 'return tnk_on() && WORD_AT(TNK_SCENE)==100u; }'),
    'any-scene': ('return tnk_on() && WORD_AT(TNK_SCENE)==100u && TNK_B(TNK_MODE)==10u; }', 'return tnk_on() && TNK_B(TNK_MODE)==10u; }'),
    'mine-any-scene': ('obj==tnk_lv.obj && WORD_AT(TNK_SCENE)==100u && tnk_on(); }', 'obj==tnk_lv.obj && tnk_on(); }'),
    'mine-any-object': ('return tnk_lv.obj && obj==tnk_lv.obj && WORD_AT', 'return tnk_lv.obj && WORD_AT'),
    'stale-resmap': ('#define TNK_RESMAP 0x008CA000u', '#define TNK_RESMAP 0x0078B860u'),
    # model requests at story level init
    'request-never': ('    for(k=0;k<3u;k++) guest_call(sub_000A5E40,1,tnk_models[k]);\n', ''),
    'request-tank-only': ('    for(k=0;k<3u;k++) guest_call(sub_000A5E40,1,tnk_models[k]);', '    for(k=0;k<1u;k++) guest_call(sub_000A5E40,1,tnk_models[k]);'),
    'request-before-original': ('    uint32_t k;\n    original();\n    if(!tnk_play() || tnk.optout || tnk_map_has()==0) return;',
                                '    uint32_t k;\n    if(!tnk_play() || tnk.optout || tnk_map_has()==0) { original();return; }\n    for(k=0;k<3u;k++) guest_call(sub_000A5E40,1,tnk_models[k]);\n    original();g_esp-=4u;'),
    'request-any-map': ('    if(!tnk_play() || tnk.optout || tnk_map_has()==0) return;\n    if(!tnk_pak_mounted()) { tnk_log("prop6.pak not mounted at level init',
                        '    if(!tnk_play() || tnk.optout) return;\n    if(!tnk_pak_mounted()) { tnk_log("prop6.pak not mounted at level init'),
    'request-ignores-optout': ('    if(!tnk_play() || tnk.optout || tnk_map_has()==0) return;\n    if(!tnk_pak_mounted()) { tnk_log("prop6.pak not mounted at level init',
                               '    if(!tnk_play() || tnk_map_has()==0) return;\n    if(!tnk_pak_mounted()) { tnk_log("prop6.pak not mounted at level init'),
    'request-any-scene': ('    if(!tnk_play() || tnk.optout || tnk_map_has()==0) return;\n    if(!tnk_pak_mounted()) { tnk_log("prop6.pak not mounted at level init',
                          '    if(!tnk_on() || tnk.optout || tnk_map_has()==0) return;\n    if(!tnk_pak_mounted()) { tnk_log("prop6.pak not mounted at level init'),
    'request-without-pak': ('    if(!tnk_pak_mounted()) { tnk_log("prop6.pak not mounted at level init', '    if(0) { tnk_log("prop6.pak not mounted at level init'),
    'request-without-rows': ('        if(!tnk_resource(tnk_models[k])) { tnk_log("no resource row', '        if(0) { tnk_log("no resource row'),
    'ready-ignored': ('        if(m) {\n            tnk_log(tnk_lv.requested', '        if(0 && m) {\n            tnk_log(tnk_lv.requested'),
    'ready-empty-request': ('    return r && MAPMAKER_PTR_OK(r,0x14u) && WORD_AT(r+TNK_REQ_DATA)!=0;', '    return r && MAPMAKER_PTR_OK(r,0x14u);'),
    'ready-no-lod': ('        if((path=WORD_AT(e+TNK_RES_LOD))!=0 && !tnk_path_ready(path,req)) return tnk_models[k];\n', ''),
    'ready-loaded-ignored': ('        if(WORD_AT(e+TNK_RES_LOADED)) continue;            /* loaded before: no file read */\n', ''),
    'ready-tank-only': ('    for(k=0;k<3u;k++) {\n        *row=e=tnk_resource(tnk_models[k]);', '    for(k=0;k<1u;k++) {\n        *row=e=tnk_resource(tnk_models[k]);'),
    # mount
    'mount-wrong-pak': ('#define TNK_PAK 0x0049DF7Cu', '#define TNK_PAK 0x0049DF98u'),
    'mount-loads-whole': ('guest_call(sub_000622D0,3,TNK_PAK,0u,1u)', 'guest_call(sub_000622D0,3,TNK_PAK,1u,1u)'),
    'mount-any-map': ('if(!tnk_play() || tnk.optout || (has=tnk_map_has())==0) return;', 'if(!tnk_play() || tnk.optout || (has=1)==0) return;'),
    'mount-ignores-optout': ('if(!tnk_play() || tnk.optout || (has=tnk_map_has())==0) return;', 'if(!tnk_play() || (has=tnk_map_has())==0) return;'),
    'mount-twice': ('    if(tnk_pak_mounted()) return;\n    if((mounted', '    if((mounted'),
    'mount-fills-table': ('#define TNK_MOUNT_KEEP 8u', '#define TNK_MOUNT_KEEP 0u'),
    'mount-off-by-one': ('+TNK_MOUNT_KEEP>TNK_MOUNT_MAX) {', '+TNK_MOUNT_KEEP>=TNK_MOUNT_MAX) {'),
    'mount-preset': ('    if(TNK_H(info+TNK_INFO_SET)!=TNK_SET_MAP) return 0;\n', ''),
    'mount-no-unreadable': ('    if(!container || !MAPMAKER_PTR_OK(container,TNK_SCAN) || WORD_AT(container)!=TNK_MAGIC) return -1;',
                            '    if(!container || !MAPMAKER_PTR_OK(container,TNK_SCAN) || WORD_AT(container)!=TNK_MAGIC) return 0;'),
    'mount-skips-extra': ('    for(k=0;extra && k<TNK_B(extra+3u);k++)', '    for(k=0;0 && extra && k<TNK_B(extra+3u);k++)'),
    'mount-ignores-count': ('    if(n>TNK_INFO_SLOTS) n=TNK_INFO_SLOTS;', '    n=TNK_INFO_SLOTS;'),
    'mount-resets-when-idle': ('    if(WORD_AT(TNK_MOUNT_SKIP)) { original();return; }    /* the original does nothing */\n', ''),
    'mount-keeps-old-tank': ('    memset(&tnk_lv,0,sizeof tnk_lv);                       /* a new level: forget the last tank */\n', ''),
    # spawn
    'spawn-any-character': ('TNK_H(a+TNK_ACTOR_CHAR)!=TNK_ID || !MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u)) return;',
                            '!MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u)) return;'),
    'spawn-before-original': ('    a=WORD_AT(g_esp+4u);\n    original();\n', '    a=WORD_AT(g_esp+4u);\n'),
    'spawn-two-tanks': ('    if(tnk_lv.obj) {\n        if(tnk_lv.extras', '    if(0) {\n        if(tnk_lv.extras'),
    'spawn-without-pak': ('    if(!tnk_pak_mounted()) { tnk_log("prop6.pak not mounted: the tank', '    if(0) { tnk_log("prop6.pak not mounted: the tank'),
    'spawn-ignores-init': ('|| WORD_AT(e+TNK_RES_INIT)!=TNK_INIT ||', '||'),
    'spawn-wrong-model': ('obj=guest_call(sub_000A6F90,5,TNK_MODEL,', 'obj=guest_call(sub_000A6F90,5,TNK_WRECK,'),
    'spawn-args-swapped': ('TNK_MODEL,args,args+12u,0u,0u);', 'TNK_MODEL,args+12u,args,0u,0u);'),
    'spawn-no-yaw': ('tnk_setf(args+16u,yaw);', 'tnk_setf(args+16u,0.0f);'),
    'spawn-yaw-as-pitch': ('tnk_setf(args+12u,0.0f);tnk_setf(args+16u,yaw);', 'tnk_setf(args+12u,yaw);tnk_setf(args+16u,0.0f);'),
    'spawn-failed-kept': ('    if(!obj || !MAPMAKER_PTR_OK(obj,0x100u)) {', '    if(!MAPMAKER_PTR_OK(obj,0x100u) && 0) {'),
    'goal-not-rebased': ('    tnk_setf(TNK_GOAL,tnk_getf(TNK_GOAL)+tnk_lv.d[0]);           /* the init\'s first drive target */\n', ''),
    'centre-wrong': ('#define TNK_CX (-12.05f)', '#define TNK_CX (-9.0f)'),
    'centre-z-wrong': ('#define TNK_CZ 1.5f', '#define TNK_CZ (-3.2f)'),
    'offset-sign': ('tnk_lv.d[0]=pos[0]-TNK_CX;', 'tnk_lv.d[0]=TNK_CX-pos[0];'),
    # placeholder removal
    'remove-never': ('        guest_call(sub_0015DDA0,1,ent);\n', ''),
    'remove-every-frame': ('    a=tnk_lv.place;ent=tnk_lv.place_ent;tnk_lv.place=0;', '    a=tnk_lv.place;ent=tnk_lv.place_ent;'),
    'remove-unchecked': ('WORD_AT(a+TNK_ACTOR_RECORD)==tnk_lv.place_rec && WORD_AT(a)==ent &&', 'WORD_AT(a)==ent &&'),
    'remove-focus-kept': ('WORD_AT(player+TNK_FOCUS)==ent) WORD_AT(player+TNK_FOCUS)=0;', 'WORD_AT(player+TNK_FOCUS)==ent) {}'),
    'remove-any-object': ('!MAPMAKER_STACK_OK(8) || !tnk_mine(WORD_AT(g_esp+4u))) { original();return; }\n    a=tnk_lv.place',
                          '!MAPMAKER_STACK_OK(8)) { original();return; }\n    a=tnk_lv.place'),
    # wake
    'wake-stock': ('    g_eax=0;\n    x=tnk_getf', '    original();return;\n    x=tnk_getf'),
    'wake-always': ('        if(dx*dx+dz*dz<=TNK_WAKE*TNK_WAKE) { g_eax=1;break; }', '        { g_eax=1;break; }'),
    'wake-dead-player': ('|| !((h=tnk_getf(ent+TNK_ENT_HEALTH))>0.0f)) continue;', ') continue;'),
    'wake-x-only': ('dx*dx+dz*dz<=TNK_WAKE', 'dx*dx<=TNK_WAKE'),
    'wake-first-player-only': ('    for(k=0;k<n && MAPMAKER_PTR_OK', '    for(k=0;k<n && k<1u && MAPMAKER_PTR_OK'),
    'wake-no-ret': ('    if(g_eax && !tnk_lv.wakes++) tnk_log("castle tank fight started: a player came near",k,tnk_lv.obj);\n    MAPMAKER_GUEST_RET();',
                    '    if(g_eax && !tnk_lv.wakes++) tnk_log("castle tank fight started: a player came near",k,tnk_lv.obj);'),
    'wake-any-scene': ('    if(!tnk_lv.obj || !MAPMAKER_STACK_OK(4) || WORD_AT(TNK_SCENE)!=100u || !tnk_on()) { original();return; }\n    g_eax=0;\n    x=',
                       '    if(!tnk_lv.obj || !MAPMAKER_STACK_OK(4) || !tnk_on()) { original();return; }\n    g_eax=0;\n    x='),
    # area
    'area-stock': ('    g_eax=0;\n    MAPMAKER_GUEST_RET();\n}', '    original();\n}'),
    'area-any-scene': ('    if(!tnk_lv.obj || !MAPMAKER_STACK_OK(4) || WORD_AT(TNK_SCENE)!=100u || !tnk_on()) { original();return; }\n    g_eax=0;\n    MAPMAKER',
                       '    if(!tnk_lv.obj || !MAPMAKER_STACK_OK(4) || !tnk_on()) { original();return; }\n    g_eax=0;\n    MAPMAKER'),
    'area-without-tank': ('    if(!tnk_lv.obj || !MAPMAKER_STACK_OK(4) || WORD_AT(TNK_SCENE)!=100u || !tnk_on()) { original();return; }\n    g_eax=0;\n    MAPMAKER',
                          '    if(!MAPMAKER_STACK_OK(4) || WORD_AT(TNK_SCENE)!=100u || !tnk_on()) { original();return; }\n    g_eax=0;\n    MAPMAKER'),
    # castle frame
    'target-unshifted': ('    tnk_shifted(original,0);\n}', '    original();\n}'),
    'target-double-shift': ('    if(tnk_lv.shifted || !tnk_mine(g_edi))', '    if(!tnk_mine(g_edi))'),
    'target-any-tank': ('    if(tnk_lv.shifted || !tnk_mine(g_edi))', '    if(tnk_lv.shifted || !tnk_lv.obj)'),
    'shift-no-restore': ('        tnk_setf(tank+TNK_ENT_POS+i*4u,tk[i]);\n', ''),
    'shift-restore-add': ('        tnk_setf(tank+TNK_ENT_POS+i*4u,tk[i]);\n', '        tnk_setf(tank+TNK_ENT_POS+i*4u,tnk_getf(tank+TNK_ENT_POS+i*4u)+tnk_lv.d[i]);\n'),
    'shift-target-kept': ('        if(tok) { tp[i]=tnk_getf(t+TNK_ENT_POS+i*4u);tnk_setf(t+TNK_ENT_POS+i*4u,tp[i]-tnk_lv.d[i]); }\n',
                          '        if(tok) { tp[i]=tnk_getf(t+TNK_ENT_POS+i*4u); }\n'),
    'shift-goal-in-world': ('        tnk_setf(TNK_GOAL+i*4u,tnk_getf(TNK_GOAL+i*4u)-tnk_lv.d[i]);\n', ''),
    'shift-goal-not-back': ('        else tnk_setf(TNK_GOAL+i*4u,tnk_getf(TNK_GOAL+i*4u)+tnk_lv.d[i]);\n', '\n'),
    'shift-goal-drift': ('        if(WORD_AT(TNK_GOAL+i*4u)==cas[i]) WORD_AT(TNK_GOAL+i*4u)=old[i];   /* not chosen: exactly as before */\n        else ',
                         '        '),
    'shift-no-y': ('tnk_lv.d[1]=pos[1]-TNK_CY;', 'tnk_lv.d[1]=0.0f;'),
    'shift-flag-kept': ('    tnk_lv.shifted=0;\n', ''),
    'state-unshifted': ('    tnk_shifted(original,1);\n}', '    tnk_shifted(original,0);\n}'),
    'state-every-change': ('|| WORD_AT(TNK_STATE)!=TNK_STATE_LOW || WORD_AT(TNK_PREV)==TNK_STATE_LOW) { original();return; }',
                           '|| WORD_AT(TNK_PREV)==TNK_STATE_LOW) { original();return; }'),
    'state-repeats': ('|| WORD_AT(TNK_STATE)!=TNK_STATE_LOW || WORD_AT(TNK_PREV)==TNK_STATE_LOW) { original();return; }',
                      '|| WORD_AT(TNK_STATE)!=TNK_STATE_LOW) { original();return; }'),
    'guns-one-only': ('        for(k=0;k<TNK_GUN_COUNT;k++)\n', '        for(k=0;k<1u;k++)\n'),
    'guns-wrong-field': ('#define TNK_GUN_AIM 0x10u', '#define TNK_GUN_AIM 0x0u'),
    # wreck, event, text
    'wreck-stock-park': ('    tnk_setf(tank+TNK_ENT_POS+4u,tnk_lv.spawn[1]+TNK_PARK);\n', ''),
    'wreck-before-original': ('    uint32_t tank=g_esi;\n    original();\n    if(!tnk_mine(tank)) return;',
                              '    uint32_t tank=g_esi;\n    if(!tnk_mine(tank)) { original();return; }\n    g_esp-=4u;original();'),
    'wreck-any-object': ('    if(!tnk_mine(tank)) return;\n    tnk_setf', '    if(!tnk_lv.obj) return;\n    tnk_setf'),
    'event-kept': ('    if(tnk_lv.events++<4u) tnk_log("tank level event dropped (MapMaker logic marker untouched)",WORD_AT(g_esp+4u),WORD_AT(g_esp+8u));\n    MAPMAKER_GUEST_RET();',
                   '    if(tnk_lv.events++<4u) tnk_log("tank level event dropped (MapMaker logic marker untouched)",WORD_AT(g_esp+4u),WORD_AT(g_esp+8u));\n    original();'),
    'event-any-caller': ('WORD_AT(g_esp)!=TNK_RET_EVENT || WORD_AT(TNK_SCENE)', 'WORD_AT(TNK_SCENE)'),
    'event-any-scene': ('|| WORD_AT(TNK_SCENE)!=100u || !tnk_on()) { original();return; }\n    if(tnk_lv.events', '|| !tnk_on()) { original();return; }\n    if(tnk_lv.events'),
    'event-wrong-return': ('#define TNK_RET_EVENT 0x00255A94u', '#define TNK_RET_EVENT 0x00255A8Fu'),
    'text-any-caller': ('if(id==TNK_BAR_TEXT && ret==TNK_RET_BAR_TEXT && tnk.label', 'if(id==TNK_BAR_TEXT && tnk.label'),
    'text-any-mode': ('ret==TNK_RET_BAR_TEXT && tnk.label && tnk_play()', 'ret==TNK_RET_BAR_TEXT && tnk.label && tnk_on()'),
    'text-no-timer': ('    else if(id==TNK_TIMER_TEXT && ret==TNK_RET_TIMER_TEXT && tnk.timer && tnk_play()) g_eax=tnk.timer;\n', ''),
    'text-timer-any-caller': ('id==TNK_TIMER_TEXT && ret==TNK_RET_TIMER_TEXT && tnk.timer', 'id==TNK_TIMER_TEXT && tnk.timer'),
    'text-no-ret': ('    else { original();return; }\n    MAPMAKER_GUEST_RET();\n}', '    else { original();return; }\n}'),
    'text-swapped': ('g_eax=tnk.label;\n    else if', 'g_eax=tnk.timer;\n    else if'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('tank.exe' if os.name == 'nt' else 'tank')
    source = ROOT / 'tests/mapmaker/mapmaker_tank.c'
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
def test_mapmaker_tank(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_TANK')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker tank ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_no_guest_code_table_and_no_save_byte():
    """Wrappers only, and the map container is only read."""
    assert not (ROOT / 'config/guest-code-patches/tank.json').exists()
    text = INC.read_text(encoding='utf-8')
    assert not re.findall(r'TNK_[BH]\([^)]*\)\s*=[^=]', text)


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


def calls_to(fn):
    return {i.address for i in decoded() if i.call and target(i) == fn}


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


def f32(rd, va):
    return struct.unpack('<f', rd(va, 4))[0]


def test_strings_init_and_placeholder():
    rd = xbe_reader()
    assert rd(0x0049DF7C, 25) == b'pak/stream/lv4/prop6.pak\0'
    assert struct.unpack('<I', rd(0x004FCA60 + 26 * 4, 4))[0] == 0x002560C0        # init index 26
    # placeholder 0x12: model 0x5AC, no Arcade row; not a real-boss id
    assert struct.unpack('<I', rd(0x0053A3CC + 0x12 * 0x30 + 4, 4))[0] == 0x5AC
    ids = {struct.unpack('<I', rd(0x00521A18 + i * 0x30 + 4, 4))[0] for i in range(150)}
    assert 0x12 not in ids


def test_object_creation_layout():
    """sub_000A6F90(model, pos, rot, a, b) -> sub_000A6EC0(type, model, pos, rot, a, b)."""
    a = by_address()
    assert mem(a[0x000A6F94])[0][4] == 0x0078B860 and mem(a[0x000A6F9C])[0][4] == 0x00791D4C
    assert target(a[0x000A6FC1]) == 0x000A6EC0 and ('imm', 0x18) in a[0x000A6FC6].ops
    # sub_000A6EC0: init = resource +0x2C, called with (object, last arg) unless type 0x100000
    assert mem(a[0x000A6F31])[0][4] == 0x2C and a[0x000A6F3A].mnemonic == 'call'
    assert ('imm', 0x100000) in a[0x000A6F29].ops
    # sub_000A6880: pos -> +0x64/+0x68/+0x6C, rot -> +0xC0/+0x70/+0xC8; a failed model load returns 0
    assert mem(a[0x000A6AA9])[0][4] == 0x64 and mem(a[0x000A6AAF])[0][4] == 0x68 and mem(a[0x000A6AB5])[0][4] == 0x6C
    assert mem(a[0x000A6B03])[0][4] == 0xC0 and mem(a[0x000A6B0C])[0][4] == 0x70 and mem(a[0x000A6B4B])[0][4] == 0xC8
    assert a[0x000A695C].mnemonic == 'xor' and a[0x000A695F].mnemonic == 'ret'
    # the tank's own wreck is made the same way: sub_000A6EC0(1, 0x298, tank+0x64, rot, 0, 0)
    assert ('imm', 0x298) in a[0x00255D41].ops and target(a[0x00255D4E]) == 0x000A6EC0


def test_init_and_static_state():
    rd = xbe_reader()
    a = by_address()
    # sub_002560C0: update callback slot 3, damage callback, static reset with the object
    assert ('imm', 0x00255550) in a[0x002560C5].ops and ('imm', 3) in a[0x002560CA].ops
    assert ('imm', 0x00255FD0) in a[0x002560EC].ops and target(a[0x002560F8]) == 0x00251AA0
    # sub_00251AA0: first drive target (-9, own y, -3.2), gun table pointers
    assert mem(a[0x00251AE7])[0][4] == 0x0047D840 and f32(rd, 0x0047D840) == -9.0
    assert mem(a[0x00251B51])[0][4] == 0x68 and mem(a[0x00251B56])[0][4] == 0x0078B584
    assert f32(rd, 0x0049F0E4) == pytest.approx(-3.2) and mem(a[0x00251B66])[0][4] == 0x0078B588
    assert ('imm', 0x0051B9C8) in a[0x00251AF7].ops and mem(a[0x00251AF7])[0][4] == 0x0078B560
    # resource rows in the scene-100 table come from the game paks (below)


def test_wake_test():
    """sub_00253140: the castle object 'RDPC', then level marker 5; only the update calls it."""
    a = by_address()
    assert ('imm', 0x43504452) in a[0x00253140].ops and target(a[0x00253145]) == 0x000B2350
    assert target(a[0x0025314B]) == 0x000B0F90
    assert ('imm', 5) in a[0x00253199].ops and target(a[0x0025319B]) == 0x00227650
    assert calls_to(0x00253140) == {0x0025557B}
    # players: count [0x00790950], base [0x007B0C48], 0x1584 each, entity +0x14, health +0x1CC
    assert mem(a[0x00253158])[0][4] == 0x00790950 and mem(a[0x00253165])[0][4] == 0x007B0C48
    assert ('imm', 0x1584) in a[0x0025318B].ops and mem(a[0x00253178])[0][4] == 0x1CC


def test_arena_constants_and_gate_areas():
    rd = xbe_reader()
    a = by_address()
    # sub_00254690 default target: player clamped to x -19.1..-5, z -10..13
    assert mem(a[0x0025475C])[0][4] == 0x004760DC and f32(rd, 0x004760DC) == -5.0
    assert mem(a[0x0025478E])[0][4] == 0x0049F114 and f32(rd, 0x0049F114) == pytest.approx(-19.1)
    assert mem(a[0x002547AE])[0][4] == 0x0049F110 and f32(rd, 0x0049F110) == 13.0
    assert mem(a[0x002547BB])[0][4] == 0x0047D6E0 and f32(rd, 0x0047D6E0) == -10.0
    assert (-19.1 + -5.0) / 2 == pytest.approx(-12.05) and (-10.0 + 13.0) / 2 == 1.5
    # it reads the tank through edi and only computes: callees are the gate test and the random number
    callees = {target(i) for i in decoded() if i.function == 0x00254690 and i.call}
    assert callees == {0x00153B40, 0x00251CD0}
    for site in (0x002548F0, 0x00255390):
        assert any(i.function == site and i.call and target(i) == 0x00254690 for i in decoded())
    assert calls_to(0x00254690) == {0x00254A24, 0x002554C7, 0x002559C6}
    assert [a[x].mnemonic for x in (0x002548F9, 0x002548FA)] == ['push', 'mov'] and 'edi, eax' in a[0x002548FA].text
    assert 'edi, eax' in a[0x00255397].text
    # sub_00251CD0: three rectangles 0x0051BB58.. (x/z), leaf, tank callers only
    assert ('imm', 0x0051BB5C) in a[0x00251CD0].ops and ('imm', 0x0051BB8C) in a[0x00251D30].ops
    assert {i.function for i in decoded() if i.call and target(i) == 0x00251CD0} == {0x00253DE0, 0x00254690, 0x00255550}
    rects = struct.unpack('<12f', rd(0x0051BB58, 48))
    assert rects[:4] == pytest.approx((-3.0, -10.0, -15.0, -6.8))
    # sub_00255390, state 7: castle aim points into the five guns (+0x10), then sub_00254690
    assert struct.unpack('<I', rd(0x00255520 + 6 * 4, 4))[0] == 0x002553B7
    assert f32(rd, 0x0049F130) == pytest.approx(-5.3) and f32(rd, 0x0049F094) == -13.0
    assert mem(a[0x00255402])[0][4] == 0x10 and mem(a[0x00255494])[0][4] == 0x140
    assert a[0x0025539E].mnemonic == 'cmp' and mem(a[0x00255390])[0][4] == 0x0078B598
    # the update's absolute height tests (not rebased): y < -1 -> state 7, y <= -2.4 keeps it
    assert mem(a[0x002556B6])[0][4] == 0x004760D0 and f32(rd, 0x004760D0) == -1.0
    assert mem(a[0x0025590D])[0][4] == 0x0049DDE4 and f32(rd, 0x0049DDE4) == pytest.approx(-2.4)


def test_death_park_point():
    rd = xbe_reader()
    a = by_address()
    # sub_00255C20 (esi = tank) parks the tank at (0, 20, 0); only sub_00255E70 calls it
    assert calls_to(0x00255C20) == {0x00255E73}
    assert 'esi, eax' in a[0x00255E71].text
    assert mem(a[0x00255DE2])[0][4] == 0x004782C4 and f32(rd, 0x004782C4) == 20.0
    assert mem(a[0x00255DF2])[0][4] == 0x68 and mem(a[0x00255DF7])[0][4] == 0x6C
    # turret 0x297 through sub_00254D10 (model from 0x0051BB88)
    assert struct.unpack('<I', rd(0x0051BB88, 4))[0] == 0x297
    assert target(a[0x00255DDA]) == 0x00254D10


def test_only_event_site():
    """Every level event reachable from the tank's init (direct calls, pushed and
    tabled function pointers) is the update's 0x16 and the wake test's marker 5."""
    rd = xbe_reader()
    graph, fnset = {}, {i.function for i in decoded()}
    for i in decoded():
        if (i.call or i.mnemonic == 'jmp') and target(i) in fnset:
            graph.setdefault(i.function, set()).add(target(i))
        for kind, v in i.values():
            if kind == 'imm' and v in fnset and v != i.function:
                graph.setdefault(i.function, set()).add(v)
    tabled = {struct.unpack_from('<I', rd(0x0051B920, 0x240), k)[0] for k in range(0, 0x240, 4)} & fnset
    assert {0x00251D90, 0x00251DD0, 0x00251EF0, 0x00253FC0, 0x00252150} <= tabled
    seen, todo = set(), [0x002560C0, *tabled]
    while todo:
        f = todo.pop()
        if f in seen:
            continue
        seen.add(f)
        todo.extend(graph.get(f, ()))
    assert len(seen) > 100
    fire = {i.address for i in decoded() if i.function in seen and target(i) in (0x002274C0, 0x00227650)
            and (i.call or i.mnemonic == 'jmp')}
    assert fire == {0x00255A8F, 0x0025319B}
    a = by_address()
    assert ('imm', 0x16) in a[0x00255A88].ops and ends(a, 0x00255A8F) == 0x00255A94
    # the castle script callback (event via a tail jump) is installed only by the castle level setup
    assert {i.function for i in decoded() if any(k == 'imm' and v == 0x002532D0 for k, v in i.values())} == {0x001E9CC0}
    # no scene test anywhere in the tank's code
    assert not [i for i in decoded() if i.function in seen and 0x00250000 <= i.function < 0x00256200
                and any(v == 0x0079094C for _, v in i.values())]


def test_bar_text_sites():
    a = by_address()
    assert ('imm', 0x281) in a[0x0014CD00].ops and target(a[0x0014CD28]) == 0x00251800
    assert ('imm', 0x165B) in a[0x0014CD38].ops and target(a[0x0014CD3D]) == 0x0003D7B0 and ends(a, 0x0014CD3D) == 0x0014CD42
    assert ('imm', 0x1635) in a[0x0014CD79].ops and target(a[0x0014CD7E]) == 0x0003D7B0 and ends(a, 0x0014CD7E) == 0x0014CD83
    # the update sets the story focus to the tank when the fight starts
    assert mem(a[0x002555AA])[0][4] == 0xFF4


def test_resource_map_relocated():
    """The rooms area moves every access of the model -> row map 0x0078B860 to
    0x008CA000, the builder's and the spawn path's included; the tank reads it there."""
    import json
    patches = json.loads((ROOT / 'config/guest-code-patches/rooms.json').read_text(encoding='utf-8'))['patches']
    moved = {int(p['address'], 16): p for p in patches if '0x78b860' in p.get('reason', '')}
    for site in (0x002427C8, 0x00242A34, 0x000A6F94, 0x000A6EC4, 0x000A688F, 0x000A6F56):
        assert int(moved[site]['value'], 16) == 0x008CA000, hex(site)
    rooms = (ROOT / 'src/interface/pc_mapmaker_rooms.inc').read_text(encoding='utf-8')
    assert '#define MAPMAKER_ROOMS_MODEL_TABLE 0x008CA000u' in rooms
    text = INC.read_text(encoding='utf-8')
    assert '#define TNK_RESMAP 0x008CA000u' in text and '#define TNK_RESMAP_MODELS 0xA00u' in text
    # story scene 100 loads prop_all_xbas (sub_00065250: story and not [0x007DE458] & 0x40 -> 1)
    a = by_address()
    assert target(a[0x00242789]) == 0x00065250 and ('imm', 0x0049F01C) in a[0x002427D8].ops
    assert mem(a[0x00065250])[0][4] == 0x007DE455 and ('imm', 1) in a[0x00065262].ops
    # the builder fills the row's init (+0x2C) from 0x004FCA60[raw +0x24]
    assert mem(a[0x00242A21])[0][4] == 0x004FCA60 and mem(a[0x00242B5E])[0][4] == 0x2C


def test_model_load_path():
    """A model is found only through its load request; the requests of the level
    setup are read by the scene loop right after it; story level init runs after
    the resource table exists."""
    a = by_address()
    # sub_00096410(path, model): request lookup first, its data taken without a null test
    assert target(a[0x00096422]) == 0x000631C0 and mem(a[0x00096437])[0][4] == 0x10
    assert a[0x0009643A].mnemonic == 'jmp' and '0x9651f' in a[0x0009643A].text and mem(a[0x0009651F])[0][4] == 4
    # scene 100 skips the streaming branch; the fallback looks up the bare row path
    assert ('imm', 0x64) in a[0x00096451].ops and '0x964fc' in a[0x00096454].text
    assert target(a[0x000964FF]) == 0x00062C30 and target(a[0x00096515]) == 0x00062F00
    # sub_000A6880 loads the row's path (+0x10) through it
    assert mem(a[0x000A6922])[0][4] == 0x10 and target(a[0x000A6927]) == 0x00096410
    # sub_000A5E40 requests the row path (+0x10) and second path (+0x14); sub_00063A40 builds the .xbr
    assert mem(a[0x000A5EAD])[0][4] == 0x10 and target(a[0x000A5EB7]) == 0x00063A40
    assert mem(a[0x000A5EBC])[0][4] == 0x14 and target(a[0x000A5EC9]) == 0x00063A40
    assert ('imm', 0x00478354) in a[0x00063AAA].ops and mem(a[0x00063ACE])[0][4] == 0xC
    assert target(a[0x00063A4F]) == 0x000631C0
    # sub_0003D440: level setup, then every queued request read
    assert target(a[0x0003D590]) == 0x001CDB80 and target(a[0x0003D5AD]) == 0x00063390
    # sub_001CDB80: pak mounts, object system (resource table), story level init, in that order
    assert target(a[0x001CDB85]) == 0x00062790 and target(a[0x001CDBF3]) == 0x000A5C00
    assert target(a[0x001CDC20]) == 0x0015E7C0 and calls_to(0x0015E7C0) == {0x001CDC20}
    assert mem(a[0x000A5CC0])[0][4] == 0x00715FAC
    assert ('imm', 0x00242BA0) in a[0x001B5271].ops and ('imm', 1) in a[0x001B5276].ops
    assert mem(a[0x0009E2C8])[0][4] == 0x00715FA8
    assert a[0x00242BF8].mnemonic == 'jmp' and target(a[0x00242BF8]) == 0x00242780


def test_mount_site():
    a = by_address()
    # sub_00062790: skipped when [0x006D7C4C] is set (never written by code); chr.pak mounted (path, 0, 1)
    assert mem(a[0x00062790])[0][4] == 0x006D7C4C
    assert not [i for i in decoded() if i.address != 0x00062790 and any(v == 0x006D7C4C for _, v in i.values())]
    assert ('imm', 0x00478790) in a[0x0006298E].ops and target(a[0x00062993]) == 0x000622D0
    assert calls_to(0x00062790) == {0x001CDB85}
    # sub_000622D0 answers -1 when the file does not open (sub_000389B0 is a plain ret)
    assert a[0x000389B0].mnemonic == 'ret'


# ---- game data (skipped without the paks) ------------------------------------

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


def _crc(name):
    return zlib.crc32(name.encode()) & 0xFFFFFFFF


def test_models_and_pak():
    """0x281/0x297/0x298 are scene-100 resource rows; their models are only in prop6.pak."""
    if not (GAME / 'arcade/l_100.pak').exists():
        pytest.skip('game paks not available locally')
    idx = _index(GAME / 'arcade/l_100.pak')
    with open(GAME / 'arcade/l_100.pak', 'rb') as f:
        pos, n = idx[0xD9E2667A]                       # prop/table/prop_all_xbas.raw
        f.seek(pos)
        table = f.read(n)
    start = struct.unpack_from('<I', table, 4)[0] + 8
    props = {}
    for o in range(start, len(table) - 0x27, 0x28):
        mid, typ, _, path, _, _, _, _, flags, init = struct.unpack_from('<iIiiifiiIi', table, o)
        props[mid] = (typ, table[path + 8:table.index(b'\0', path + 8)].decode('latin1'), init)
    assert props[0x281] == (0x8000000, 'castle/props/tankboss', 26)
    assert props[0x297][1] == 'castle/props/tankbossturret' and props[0x298][1] == 'castle/props/tankbosswreck'
    assert props[0x5AC][1] == 'chrs/prisonguardrobot'
    model = 'assets/ts/models/{0}/xbox/output/{1}.xbr'
    paks = {k: _index(GAME / k) for k in ('stream/lv4/prop6.pak', 'chr.pak', 'arcade/l_100.pak', 'gun.pak')}
    for m in (0x281, 0x297, 0x298):
        p = props[m][1]
        assert [k for k, v in paks.items() if _crc(model.format(p, p.split('/')[-1])) in v] == ['stream/lv4/prop6.pak']
    p = props[0x5AC][1]
    assert _crc(model.format(p, p.split('/')[-1])) in paks['chr.pak']
