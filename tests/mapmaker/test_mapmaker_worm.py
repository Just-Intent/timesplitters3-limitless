"""MapMaker real story boss: the mansion worm of story mission 5 as a Story AI.

Compiles src/interface/pc_mapmaker_worm.inc against fake guest memory (with
mutations that must be caught) and, when the private XBE and its disassembly
are available, checks the stock facts the hooks rely on: the character data,
the anim pak and its names, the scene descriptor and its level callbacks, the
worm block, the init's mansion steps, the head binding and lookup, the
character update slot, the shifted constants and their readers, the route
pads, the hit callback, the draw and its scene gate, and that no level marker
or tag lookup is reachable from the update.
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
INC = ROOT / 'src/interface/pc_mapmaker_worm.inc'

MUTATIONS = {
    'shared-ids-trophy': ('(int32_t)guest_call(sub_00062260,1,WRM_PAK_TROPHY)!=-1 || ', ''),
    'shared-ids-skeleton': (' || (int32_t)guest_call(sub_00062260,1,WRM_PAK_SKELETON)!=-1)) {', ')) {'),
    'shared-ids-always': ('    if(!pc_mapmaker_area_enabled("worm","pc_mapmaker_worm_register") &&\n', '    if(1 &&\n'),
    'shared-ids-never': ('    if(!pc_mapmaker_area_enabled("worm","pc_mapmaker_worm_register") &&\n', '    if(0 &&\n'),
    'register-off': ('WORD_AT(g_esp+12u)!=WRM_ANIMS_N || !wrm_play()) { original();return; }', 'WORD_AT(g_esp+12u)!=WRM_ANIMS_N || 1) { original();return; }'),
    'register-any-table': ('WORD_AT(g_esp+8u)!=WRM_ANIMS || WORD_AT(g_esp+12u)!=WRM_ANIMS_N', 'WORD_AT(g_esp+12u)!=WRM_ANIMS_N'),
    'register-any-count': (' || WORD_AT(g_esp+12u)!=WRM_ANIMS_N || !wrm_play())', ' || !wrm_play())'),
    'register-any-scene': ('WORD_AT(g_esp+12u)!=WRM_ANIMS_N || !wrm_play()) {', 'WORD_AT(g_esp+12u)!=WRM_ANIMS_N || !wrm_on()) {'),
    'register-no-barney': ('    guest_call(sub_0005B980,3,prefix,WRM_ANIMS+WRM_ANIM_BARNEY*WRM_ANIM,1u);\n', ''),
    'register-wrong-first': ('#define WRM_ANIM_FIRST 21u', '#define WRM_ANIM_FIRST 20u'),
    'register-wrong-count': ('#define WRM_ANIM_COUNT 8u', '#define WRM_ANIM_COUNT 9u'),
    'register-whole-table': ('    WORD_AT(sp+8u)=WRM_ANIMS+WRM_ANIM_FIRST*WRM_ANIM;WORD_AT(sp+12u)=WRM_ANIM_COUNT;\n', ''),
    'register-not-restored': ('    WORD_AT(sp+8u)=WRM_ANIMS;WORD_AT(sp+12u)=WRM_ANIMS_N;', '    (void)0;'),
    'shift-burst-line-missing': ('    {0x0047D78Cu,0x41D80000u,2},', ''),
    'shared-ids-wrong-pak': ('#define WRM_PAK_SKELETON 0x0049D6FCu', '#define WRM_PAK_SKELETON 0x0049D77Cu'),
    'always-on': ('    return mapmaker_limits.limits;\n}', '    return 1;\n}'),
    'any-mode': ('WORD_AT(WRM_SCENE)==100u && WRM_B(WRM_MODE)==10u; }', 'WORD_AT(WRM_SCENE)==100u; }'),
    'any-scene': ('wrm_on() && WORD_AT(WRM_SCENE)==100u && WRM_B(WRM_MODE)==10u; }', 'wrm_on() && WRM_B(WRM_MODE)==10u; }'),
    'set-any-index': ('    if(WRM_H(info+0x14u)!=WRM_SET_MAP) return 0;\n', ''),
    'optout-ignored': ('    if(!wrm_play() || wrm.optout || (has=wrm_set_has())==0) return;\n    WORD_AT(WRM_STATE)=0;',
                       '    if(!wrm_play() || (has=wrm_set_has())==0) return;\n    WORD_AT(WRM_STATE)=0;'),
    'mount-wrong-pak': ('#define WRM_PAK 0x0049D7BCu', '#define WRM_PAK 0x0049DC78u'),
    'mount-full-ignored': ('    if((mounted=WORD_AT(WRM_MOUNTS))+WRM_MOUNT_KEEP>WRM_MOUNT_MAX)', '    if(0)'),
    'mount-when-mounted': ('(has=wrm_set_has())==0 || wrm_pak_mounted()) return;', '(has=wrm_set_has())==0) return;'),
    'level-no-alloc-check': ('    if(!st || !MAPMAKER_PTR_OK(st,WRM_STATE_SIZE) || !scratch', '    if(!scratch'),
    'level-no-scratch-check': ('|| !scratch || !MAPMAKER_PTR_OK(scratch,WRM_SCRATCH)) {', ') {'),
    'scratch-not-zeroed': ('    wrm_zero(scratch,WRM_SCRATCH);\n', ''),
    'scratch-no-object': ('    WORD_AT(scratch+WRM_FAKE_TABLE+WRM_OBJECT*4u)=scratch+WRM_FAKE_OBJECT;\n', ''),
    'init-real-objects': ('    WORD_AT(WRM_OBJECTS)=scratch+WRM_FAKE_TABLE;      /* object', '    (void)0;      /* object'),
    'init-converts-pads': ('    WORD_AT(WRM_ROUTE_DONE)=0;                         /* the mansion', '    (void)0;                         /* the mansion'),
    'init-objects-kept': ('    WORD_AT(WRM_OBJECTS)=objects;WORD_AT(WRM_ROUTE_DONE)=route_done;', '    WORD_AT(WRM_ROUTE_DONE)=route_done;'),
    'init-flag-kept': ('    WORD_AT(WRM_OBJECTS)=objects;WORD_AT(WRM_ROUTE_DONE)=route_done;', '    WORD_AT(WRM_OBJECTS)=objects;'),
    'no-effect': ('    wrm_lv.effect=guest_call(sub_00378400,2,WRM_EFFECT_FN,5u);', '    wrm_lv.effect=0;'),
    'park-none': ('        WORD_AT(w+WRM_W_STATE)=WRM_GONE;\n', ''),
    'park-from-0': ('    for(k=1;k<WRM_WORMS;k++) {                         /* one worm', '    for(k=0;k<WRM_WORMS;k++) {                         /* one worm'),
    'park-emitters-kept': ('        if(WORD_AT(w+WRM_W_EMITTER)) guest_call(sub_0010E2A0,1,WORD_AT(w+WRM_W_EMITTER));\n', ''),
    'enable-at-level': ('    WORD_AT(WRM_ENABLE)=0;                             /* until', '    WORD_AT(WRM_ENABLE)=1;                             /* until'),
    'ai-extra-wins': ('    else if(wrm_lv.record && wrm_lv.record!=rec) {', '    else if(0) {'),
    'ai-any-character': ("    if(WRM_H(a+WRM_ACTOR_CHAR)!=WRM_ID && !(MAPMAKER_PTR_OK(ent,0x100u) && WRM_H(ent+WRM_ENT_MODEL)==WRM_MODEL)) { original();return; }\n", ''),
    'ai-class-changed': ('        wrm_lv.record=rec;wrm_lv.actor=a;', '        wrm_lv.record=rec;wrm_lv.actor=a;WORD_AT(rec)=0;'),
    'head-any-entity': ('    return WORD_AT(a+WRM_ACTOR_RECORD)==wrm_lv.record && WORD_AT(WRM_SCENE)', '    return 1 && WORD_AT(WRM_SCENE)'),
    'head-any-scene': ('wrm_lv.record && WORD_AT(WRM_SCENE)==100u && wrm_on();', 'wrm_lv.record && wrm_on();'),
    'head-stale-state': ('    if(!wrm_lv.record || !wrm_state_ok()) return 0;', '    if(!wrm_lv.record) return 0;'),
    'tick-thinks': ('    if(!wrm.ticks++) wrm_log("mansion worm runs its story update",ent,WORD_AT(wrm_lv.state+WRM_W_STATE));\n    MAPMAKER_GUEST_RET();',
                    '    if(!wrm.ticks++) wrm_log("mansion worm runs its story update",ent,WORD_AT(wrm_lv.state+WRM_W_STATE));\n    original();'),
    'tick-no-update': ('    guest_call(sub_001DAAB0,0);\n', ''),
    'tick-setup-every-time': ('    if(!wrm_lv.ready && !wrm_lv.failed) wrm_setup(ent);', '    if(!wrm_lv.failed) wrm_setup(ent);'),
    'tick-failed-runs': ('    if(wrm_lv.failed) { original();return; }\n    wrm_shift(1,0);', '    wrm_shift(1,0);'),
    'tick-slot-lost': ('!WORD_AT(ent+WRM_SLOT3)) WORD_AT(ent+WRM_SLOT3)=WRM_CHAR_UPDATE;', '!WORD_AT(ent+WRM_SLOT3)) {}'),
    'tick-slot-after-death': ('    if(WORD_AT(WRM_HEADS)==ent && !WORD_AT(ent+WRM_SLOT3))', '    if(!WORD_AT(ent+WRM_SLOT3))'),
    'shift-none': ('    wrm_shift(1,0);\n    guest_call(sub_001DAAB0,0);', '    guest_call(sub_001DAAB0,0);'),
    'shift-not-restored': ('    guest_call(sub_001DAAB0,0);\n    wrm_shift(0,0);', '    guest_call(sub_001DAAB0,0);'),
    'shift-ignores-stock-bits': ('            if(wrm_lv.saved[k]!=wrm_consts[k].bits) continue;\n', ''),
    'shift-ground-x': ('    {0x0049DF08u,0xC0198937u,1},', '    {0x0049DF08u,0xC0198937u,0},'),
    'shift-box-z-as-x': ('    {0x0047D69Cu,0xC0C00000u,2},', '    {0x0047D69Cu,0xC0C00000u,0},'),
    'shift-wake-missing': ('    {0x0049DE3Cu,0xC29E0000u,0},   /* -79.0: wake region x */\n', ''),
    'shift-point-z-as-x': ('    {0x0049DDD4u,0x41873333u,2},', '    {0x0049DDD4u,0x41873333u,0},'),
    'route-not-swapped': ('WORD_AT(WRM_ROUTE+k*4u)=wrm_lv.pad[k]; }', '(void)0; }'),
    'route-not-restored': ('            for(k=0;k<WRM_ROUTE_N;k++) WORD_AT(WRM_ROUTE+k*4u)=wrm_lv.route[k];\n', ''),
    'offset-box-z': ('wrm_lv.dz=sz-WRM_BOX_Z;', 'wrm_lv.dz=sz;'),
    'offset-ground': ('wrm_lv.dy=sy-WRM_GROUND;', 'wrm_lv.dy=sy;'),
    'place-no-trail': ('    for(k=0;k<2u+WRM_W_TRAILS;k++) {', '    for(k=0;k<2u;k++) {'),
    'place-trail-long': ('    for(k=0;k<2u+WRM_W_TRAILS;k++) {', '    for(k=0;k<3u+WRM_W_TRAILS;k++) {'),
    'place-no-enable': ('    WORD_AT(WRM_ENABLE)=1u;\n', ''),
    'setup-nan': ('    if(!wrm_sane(sx) || !wrm_sane(sy) || !wrm_sane(sz)) {', '    if(!wrm_sane(sy)) {'),
    'route-no-box': ('fabsf(dx)>WRM_BOX_HX || fabsf(dz)>WRM_BOX_HZ ||', ''),
    'route-any-floor': ('               fabsf(wrm_getf(p+0x10u)-sy)>WRM_PAD_DY) continue;', '               0) continue;'),
    'route-invalid-pads': ('!(WORD_AT(p+4u)&1u)) continue;', '0) continue;'),
    'route-no-gap': ('ex*ex+ez*ez<WRM_PAD_GAP*WRM_PAD_GAP) break;', '0) break;'),
    'route-farthest': ('if(bd<0.0f || d<bd) { bd=d;best=i; }', 'if(bd<0.0f || d>bd) { bd=d;best=i; }'),
    'route-unsorted': ('        for(j=k;j>0 && ang[j-1]>ang[j];j--) {', '        for(j=k;j>0 && 0;j--) {'),
    'route-refused-kept': ('            if(rejects>=16u) break;\n            reject[rejects++]=best;\n            continue;', '            break;'),
    'route-no-own-pad': ('        wrm_lv.pad[0]=(uint32_t)own;\n        found=1;', '        return 0;'),
    'route-no-repeat': ('    for(k=found;k<WRM_ROUTE_N;k++) wrm_lv.pad[k]=wrm_lv.pad[k%found];\n', ''),
    'tag-any-site': ('|| WORD_AT(g_esp)!=WRM_RET_TAG ||', '||'),
    'tag-any-worm': ('    g_eax=tag==WRM_TAG && wrm_head(ent)?ent:0u;', '    g_eax=wrm_head(ent)?ent:0u;'),
    'tag-before-ready': ('    if(!wrm_lv.ready || !MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=WRM_RET_TAG', '    if(!MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=WRM_RET_TAG'),
    'hit-no-script': ('    WORD_AT(WRM_SCRIPT)=wrm_lv.scratch;WORD_AT(WRM_OBJECTS)', '    WORD_AT(WRM_OBJECTS)'),
    'hit-no-objects': ('WORD_AT(WRM_SCRIPT)=wrm_lv.scratch;WORD_AT(WRM_OBJECTS)=wrm_lv.scratch+WRM_FAKE_TABLE;', 'WORD_AT(WRM_SCRIPT)=wrm_lv.scratch;'),
    'hit-no-effect': ('    WORD_AT(WRM_EFFECT)=wrm_lv.effect;WORD_AT(ent+WRM_ENT_TAG)=WRM_TAG;', '    WORD_AT(ent+WRM_ENT_TAG)=WRM_TAG;'),
    'hit-no-tag': ('    WORD_AT(WRM_EFFECT)=wrm_lv.effect;WORD_AT(ent+WRM_ENT_TAG)=WRM_TAG;', '    WORD_AT(WRM_EFFECT)=wrm_lv.effect;'),
    'hit-not-restored': ('    WORD_AT(WRM_SCRIPT)=script;WORD_AT(WRM_OBJECTS)=objects;', '    WORD_AT(WRM_OBJECTS)=objects;'),
    'hit-tag-kept': ('WORD_AT(WRM_EFFECT)=effect;WORD_AT(ent+WRM_ENT_TAG)=tag;', 'WORD_AT(WRM_EFFECT)=effect;'),
    'hit-counter-kept': ('    WORD_AT(wrm_lv.scratch+WRM_KILLS)=0;\n    WORD_AT(WRM_SCRIPT)=wrm_lv.scratch;', '    WORD_AT(WRM_SCRIPT)=wrm_lv.scratch;'),
    'hit-counter-after': ('    WORD_AT(wrm_lv.scratch+WRM_KILLS)=0;\n    if(WORD_AT(wrm_lv.state+WRM_W_STATE)!=st', '    if(WORD_AT(wrm_lv.state+WRM_W_STATE)!=st'),
    'hit-any-entity': ('       !wrm_head(ent=WORD_AT(info))) { original();return; }', '       !(ent=WORD_AT(info))) { original();return; }'),
    'fx-any-call': ('    int here=wrm_lv.ready && MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==WRM_RET_FX;', '    int here=wrm_lv.ready && MAPMAKER_STACK_OK(4);'),
    'fx-not-ready': ('    int here=wrm_lv.ready && MAPMAKER_STACK_OK(4)', '    int here=wrm_lv.state && MAPMAKER_STACK_OK(4)'),
    'fx-no-draw': ('    guest_call(sub_001DA990,0);\n', ''),
    'fx-no-shift': ('    wrm_shift(1,1);\n', ''),
    'fx-wall-kept': ('            if(wrm_lv.wall==WRM_WALL_BITS) WORD_AT(WRM_WALL)=WRM_FLT_MAX;', '            (void)0;'),
    'fx-wall-not-restored': ('        if(draw) WORD_AT(WRM_WALL)=wrm_lv.wall;', '        if(draw) {}'),
    'fx-draws-props': ('    if(wrm_lv.drawing && MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==WRM_RET_PROPS) { MAPMAKER_GUEST_RET();return; }\n', ''),
    'fx-draws-extra': ('    if(wrm_lv.drawing && MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==WRM_RET_EXTRA) { MAPMAKER_GUEST_RET();return; }\n', ''),
    'props-always-skipped': ('    if(wrm_lv.drawing && MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==WRM_RET_PROPS)', '    if(MAPMAKER_STACK_OK(4))'),
    'script-kept': ('    if(wrm_lv.ready && MAPMAKER_STACK_OK(8) && WORD_AT(g_esp)==WRM_RET_SCRIPT && WORD_AT(WRM_SCENE)==100u) {', '    if(0) {'),
    'script-any-site': ('&& WORD_AT(g_esp)==WRM_RET_SCRIPT && WORD_AT(WRM_SCENE)==100u) {', '&& WORD_AT(WRM_SCENE)==100u) {'),
    'script-any-scene': ('WORD_AT(g_esp)==WRM_RET_SCRIPT && WORD_AT(WRM_SCENE)==100u) {', 'WORD_AT(g_esp)==WRM_RET_SCRIPT) {'),
    'level-not-forgotten': ('    memset(&wrm_lv,0,sizeof wrm_lv);                  /* a new level', '    (void)0;                  /* a new level'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('worm.exe' if os.name == 'nt' else 'worm')
    source = ROOT / 'tests/mapmaker/mapmaker_worm.c'
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
def test_mapmaker_worm(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_REAL_BOSS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker worm ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_no_guest_code_table_and_no_save_byte():
    text = INC.read_text(encoding='utf-8')
    assert 'guest-code-patches' not in text and 'pc_mapmaker_container_write' not in text


# ---- stock facts (need the private XBE and its disassembly) ----------------

def by_address():
    return {i.address: i for i in decoded()}


def mem(i):
    return [o for o in i.ops if o[0] == 'mem']


def ends(a, site):
    return site + len(bytes.fromhex(a[site].hex))


def target(i):
    m = re.search(r'0x([0-9a-f]+)', i.text)
    return int(m.group(1), 16) if m else None


def calls_to(f):
    return {i.address for i in decoded() if i.call and target(i) == f}


def functions_of(f):
    return {i.function for i in decoded() if (i.call or i.mnemonic == 'jmp') and target(i) == f}


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


def word(rd, va):
    return struct.unpack('<I', rd(va, 4))[0]


def test_character_and_anims():
    rd = xbe_reader()
    voice, model, lod = struct.unpack_from('<3I', rd(0x0053A3CC + 0x2C * 0x30, 12), 0)
    assert (voice, model, lod) == (0x005397C8, 0x5EA, 0xFFFFFFFF)
    ids = [word(rd, 0x00521A18 + i * 0x30 + 4) for i in range(150)]
    assert 0x2C not in ids                                           # no Arcade row
    assert rd(0x0049D7BC, 30) == b'pak/stream/lv1/anim/lv1_2.pak\0'
    lv1_2 = anim_table(rd, 0x0048BEC8, 0x1E)
    worm = {k: v for k, v in lv1_2.items() if k.startswith('wrm')}
    assert worm == {'wrmburst': 0x194, 'wrmidle': 0x195, 'wrmflaming': 0x196, 'wrmattack': 0x197,
                    'wrmburrow': 0x198, 'wrmhitreact': 0x199, 'wrmtantrum': 0x19A, 'wrmdeath': 0x19B}
    others = {}
    for va, n in ((0x00484F68, 0x14), (0x0048C738, 0x12), (0x0048C1B0, 0x10), (0x0047E938, 0x29)):   # lv81, lv1_6, lv1_3, lv14
        others.update({v: k for k, v in anim_table(rd, va, n).items()})
    for name, v in lv1_2.items():                                    # a shared id is the same anim
        assert v not in others or others[v] == name
    # the shifted constants and their stock bits
    text = INC.read_text(encoding='utf-8')
    for va, b in ((0x0049DF08, 0xC0198937), (0x0049DF10, 0xC29A6666), (0x0049DE40, 0xC25ECCCD), (0x0049DF0C, 0x421E0000),
                  (0x0047D69C, 0xC0C00000), (0x0049DE3C, 0xC29E0000), (0x0049DE34, 0xC0FCCCCD), (0x0049DE38, 0x42213333),
                  (0x0049DDD8, 0xC2794189), (0x0049DDD4, 0x41873333)):
        assert word(rd, va) == b and f'{{0x{va:08X}u,0x{b:08X}u,' in text
    assert word(rd, 0x0049DE08) == 0xC257999A and 'WRM_WALL_BITS 0xC257999Au' in text
    # the six mansion route pad ids and the "not converted" flag
    assert struct.unpack('<7I', rd(0x004FDC24, 28))[:6] == (0x4FF, 0x514, 0x515, 0x51D, 0x516, 0x500)
    assert word(rd, 0x004FDC58) == 1


def test_anim_registration_and_mount():
    a = by_address()
    assert ('imm', 0x0049D7BC) in a[0x001CAD57].ops and a[0x001CAD57].function == 0x001CA2F0
    assert target(a[0x001CAD5C]) == 0x00062260 and ('imm', 0x0048BEC8) in a[0x001CAD6B].ops
    assert ('imm', 0x1E) in a[0x001CAD69].ops and target(a[0x001CAD75]) == 0x0005B980
    assert target(a[0x0005C501]) == 0x001C8540 and target(a[0x0005C558]) == 0x001CA2F0


def test_scene_descriptor_and_level():
    rd = xbe_reader()
    a = by_address()
    # data/mansion2.raw: init0 (script block), level init, level update
    assert rd(word(rd, 0x004FDFDC), 18) == b'data/mansion2.raw\0'
    assert (word(rd, 0x004FDFC4), word(rd, 0x004FDFC8), word(rd, 0x004FDFD0)) == (0x001D7830, 0x001DDB50, 0x001DDC90)
    assert mem(a[0x001D783D])[0][4] == 0x0075B0C4 and ('imm', 0x50) in a[0x001D7833].ops
    # the worm block: 0x1590 bytes, allocated only for scene 1
    assert ('imm', 0x1590) in a[0x001D1E82].ops and mem(a[0x001D1E91])[0][4] == 0x0075B4B4
    assert calls_to(0x001D1E80) == set() and functions_of(0x001D1E80) == {0x0036C740}
    assert ('imm', 1) in a[0x0036C740].ops
    # level init -> worm init; level update -> worm update; frame draws it in scene 1 only
    assert target(a[0x001DDBBA]) == 0x001D2350 and calls_to(0x001D2350) == {0x001DDBBA}
    assert target(a[0x001DDDC0]) == 0x001DAAB0 and calls_to(0x001DAAB0) == {0x001DDDC0}
    assert calls_to(0x001DA990) == {0x001CEE2C} and ('imm', 1) in a[0x001CEE1D].ops
    assert target(a[0x001CEE87]) == 0x00126560 and ends(a, 0x001CEE87) == 0x001CEE8C


def test_worm_init():
    a = by_address()
    # five worms of 0x450, worm 2 in state 8, the others 1; the trail 8 x 0x60 from +0x3C
    assert ('imm', 0x450) in a[0x001D249A].ops and ('imm', 5) in a[0x001D260E].ops
    assert ('imm', 2) in a[0x001D24B6].ops and ('imm', 8) in a[0x001D2525].ops and ('imm', 1) in a[0x001D24FA].ops
    assert mem(a[0x001D24FA])[0][4] == 0x24 and mem(a[0x001D2525])[0][4] == 0x24
    assert mem(a[0x001D254F])[0][4] == 0x40 and ('imm', 8) in a[0x001D2552].ops and ('imm', 0x60) in a[0x001D25DE].ops
    assert mem(a[0x001D2559])[0][4] in (0xFFFFFFFC, -4)
    assert target(a[0x001D25FF]) == 0x00111AE0 and ('imm', 0x43C) in a[0x001D25F3].ops
    # the enable flag off, the pad conversion once ([0x004FDC58]), object 0x18 without a null check
    assert mem(a[0x001D261E])[0][4] == 0x0075B4BC and mem(a[0x001D2617])[0][4] == 0x004FDC58
    assert ('imm', 0x004FDC24) in a[0x001D2631].ops and target(a[0x001D2639]) == 0x0009CC40
    assert ('imm', 0x18) in a[0x001D2654].ops and target(a[0x001D2656]) == 0x000877B0
    assert mem(a[0x001D265B])[0][4] == 0x28 and target(a[0x001D2679]) == 0x0009E720
    assert mem(a[0x000877B4])[0][4] == 0x007B9784
    assert mem(a[0x0009E728])[0][4] == 0x19C                         # sub_0009E720: the hit callback slot
    # the enable is the level script's: sub_001D5E10 has no direct caller
    assert mem(a[0x001D5E10])[0][4] == 0x0075B4BC and calls_to(0x001D5E10) == set()


def test_head_binding_and_tick_slot():
    a = by_address()
    # sub_001D9AF0: the AI tagged 'WRM1' + worm, slots 3 and 4 off, hit callback, 5 lower, 12.1 health
    assert ('imm', 0x57524D31) in a[0x001D9AF0].ops or mem(a[0x001D9AF0])[0][4] == 0x57524D31
    assert target(a[0x001D9AF8]) == 0x0015BA10 and ends(a, 0x001D9AF8) == 0x001D9AFD
    assert ('imm', 3) in a[0x001D9B08].ops and ('imm', 4) in a[0x001D9B12].ops
    assert ('imm', 0x001D1F30) in a[0x001D9B42].ops and target(a[0x001D9B57]) == 0x0009E720
    assert mem(a[0x001D9B64])[0][4] == 0x0078FAE0
    # sub_000A5CE0: slot n at +0x154 + n*4
    assert mem(a[0x000A5CFB])[0][4] == 0x154
    # the character update in slot 3 of every character
    assert ('imm', 0x000D8E30) in a[0x000D9210].ops and ('imm', 3) in a[0x000D9215].ops and ('imm', 8) in a[0x000D9217].ops
    assert target(a[0x000D9219]) == 0x0009E2D0
    # sub_0015BA10: the AI pool by spawn record +8
    assert mem(a[0x0015BA58])[0][4] == 0xBD8 and mem(a[0x0015BA5E])[0][4] == 8
    # the head lookup is sub_001D9AF0's only sub_0015BA10 call, from the update's loop
    assert calls_to(0x001D9AF0) == {0x001DABFC}


def test_update_constants_route_and_reachability():
    a = by_address()
    readers = {}
    for i in decoded():
        for k, v in i.values():
            if k == 'disp':
                readers.setdefault(v, set()).add(i.function)
    worm = {0x001DAAB0, 0x001DACC5, 0x001DC0B8, 0x001DC0BB, 0x001DC0FC}     # the update and its tail aliases
    for va in (0x0049DF08, 0x0049DF10, 0x0049DF0C):
        assert readers[va] <= worm
    assert readers[0x0049DE40] <= worm | {0x001D5D70}
    for va in (0x0049DE34, 0x0049DE38, 0x0049DE3C):
        assert readers[va] == {0x001D5D70}
    assert readers[0x0049DDD8] <= worm | {0x001D2350, 0x001D9B80} and readers[0x0049DDD4] <= worm | {0x001D2350, 0x001D9B80}
    assert readers[0x0049DE08] == {0x001D2800, 0x001D9B80}
    # the box clamp reads -6.0 (shared) at 0x001DBB6D; the wake test is called from the update
    assert mem(a[0x001DBB6D])[0][4] == 0x0047D69C and target(a[0x001DB108]) == 0x001D5D70
    # the route: only the update reads the six pads
    assert readers[0x004FDC24] <= worm | {0x001D2350}
    # nothing the update reaches reads -6.0, fires a level marker or looks up a tag (direct calls)
    graph = {}
    for i in decoded():
        if (i.call or i.mnemonic == 'jmp') and target(i) is not None:
            graph.setdefault(i.function, set()).add(target(i))

    def reach(stop):
        seen, todo = set(), [0x001DAAB0]
        while todo:
            f = todo.pop()
            if f in seen or f in stop:
                continue
            seen.add(f)
            todo.extend(t for t in graph.get(f, ()) if t not in seen)
        return seen
    seen = reach(set())
    others = readers[0x0047D69C] - worm
    assert others and not others & seen
    others = readers[0x0047D78C] - worm
    assert others and not others & seen
    assert 0x0015BA10 in seen and 0x000877B0 not in seen
    # the two ways to markers and tag lookups: the dropped 'SFCC' trigger and the generic explosion (a read)
    assert ('imm', 0x53464343) in a[0x001DC455].ops and target(a[0x001DC45A]) == 0x00234CD0
    assert ends(a, 0x001DC45A) == 0x001DC45F
    assert {x for x in calls_to(0x00234CD0) if 0x001DAAB0 <= x < 0x001DD400} == {0x001DC45A}
    assert len(calls_to(0x000BB6C0)) > 50
    assert not {0x002274C0, 0x002273C0, 0x002275F0, 0x0023FF20} & reach({0x00234CD0, 0x000BB6C0})
    assert not {0x002274C0, 0x002275F0, 0x0023FF20} & reach({0x00234CD0})


def test_hit_callback():
    a = by_address()
    # the head's tag at +0x60, the kill counter in the script block, the 5-kill branch
    assert mem(a[0x001D1FC1])[0][4] == 0x60
    assert mem(a[0x001D1F9B])[0][4] == 0x0075B0C4 and mem(a[0x001D2013])[0][4] == 0x3C
    assert ('imm', 5) in a[0x001D2019].ops and target(a[0x001D204F]) == 0x002274C0
    assert target(a[0x001D2023]) == 0x000877B0 and target(a[0x001D2116]) == 0x002274C0
    assert mem(a[0x001D207C])[0][4] == 0x0078F590
    # the mansion effect type: sub_00378400(sub_001D6290, 5)
    assert ('imm', 0x001D6290) in a[0x001D64A3].ops and ('imm', 5) in a[0x001D64A1].ops
    assert target(a[0x001D64AD]) == 0x00378400 and mem(a[0x001D64D7])[0][4] == 0x0078F590


def test_draw():
    a = by_address()
    # sub_001DA990: the mansion parts first, then the worms (enable flag), heads placed by sub_001D9B80
    assert target(a[0x001DA993]) == 0x001D9370 and ends(a, 0x001DA993) == 0x001DA998
    assert target(a[0x001DA9A4]) == 0x001D7200 and ends(a, 0x001DA9A4) == 0x001DA9A9
    assert mem(a[0x001DA9BA])[0][4] == 0x0075B4BC
    assert target(a[0x001DAA4B]) == 0x001D9B80 and target(a[0x001DAA3B]) == 0x001D2800
    assert mem(a[0x001DA917])[0][4] == 0x64 and mem(a[0x001DA928])[0][4] == 0x6C
    # the mansion wall clip: camera x against -53.9
    assert mem(a[0x001D294A])[0][4] == 0x0049DE08 and mem(a[0x001D2942])[0][4] == 0x27C



def test_lv1_paks_share_anim_ids():
    """lv1_2 shares two Jo-Beth names (and ids) with lv1_3 (trophy monster) and lv1_6 (skeleton);
    the worm's own entries (21..28 and 5) are unique among every boss pak's registration table."""
    rd = xbe_reader()

    def names(va, n):
        return {anim_table_entry(rd, va, k) for k in range(n)}
    lv1_2 = [anim_table_entry(rd, 0x0048BEC8, k) for k in range(0x1E)]
    trophy, skel = names(0x0048C1B0, 0x10), names(0x0048C738, 0x12)
    assert {n for n in lv1_2 if n in trophy} == {('jblookout', 0x193)}
    assert {n for n in lv1_2 if n in skel} == {('jbgrossend', 0x1AE)} and not trophy & skel
    kept = lv1_2[21:29] + [lv1_2[5]]
    assert [n for n, _ in kept] == ['wrmburst', 'wrmidle', 'wrmflaming', 'wrmattack', 'wrmburrow', 'wrmhitreact',
                                    'wrmtantrum', 'wrmdeath', 'barney_worm']
    others = set()
    for va, n in ((0x00484F68, 0x14), (0x0048C738, 0x12), (0x0048C1B0, 0x10), (0x0047E938, 0x29)):  # lv81, lv1_6, lv1_3, lv14
        others |= names(va, n)
    assert not {i for _, i in kept} & {i for _, i in others} and not {n for n, _ in kept} & {n for n, _ in others}


def anim_table_entry(rd, va, k):
    e = rd(va + k * 0x18, 0x18)
    return rd(struct.unpack_from('<I', e, 0)[0], 40).split(b'\0')[0].decode(), struct.unpack_from('<H', e, 0x10)[0]


def test_registration_sites_and_fixup():
    """sub_0005B980(prefix, table, count) registers entry by entry; sub_00057F30 fixes the anim data up in place;
    the worm's state 2 burst (+0x44C 2, barney_worm 0x149) is behind the z line 27.0 (0x0047D78C)."""
    a = by_address()
    assert ('imm', 0x1E) in a[0x001CAD69].ops and target(a[0x001CAD75]) == 0x0005B980 and ends(a, 0x001CAD75) == 0x001CAD7A
    assert ('imm', 0x0048BEC8) in a[0x001C95F2].ops and target(a[0x001C95FC]) == 0x0005B980
    assert mem(a[0x0005B993])[0][4] == 0x8C and mem(a[0x0005B99B])[0][4] == 0x94 and mem(a[0x0005B987])[0][4] == 0x90
    assert ('imm', 0x18) in a[0x0005B9E9].ops and target(a[0x0005B9C9]) == 0x00057F30
    assert mem(a[0x0005805F])[0][4] == 0x1C and mem(a[0x00058070])[0][4] == 0x28          # offsets become pointers
    # barney_worm on the head in state 9 when +0x44C == 2; +0x44C = the state (2) south of z 27.0
    assert ('imm', 0x149) in a[0x001DBF46].ops and target(a[0x001DBF64]) == 0x0005F460
    assert mem(a[0x001DB887])[0][4] == 0x0047D78C and mem(a[0x001DB88F])[0][4] == 8
    assert mem(a[0x001DB8A4])[0][4] == 0x44C
    readers = {i.function for i in decoded() if any(k == 'disp' and v == 0x0047D78C for k, v in i.values())}
    assert 0x001DACC5 in readers and len(readers - {0x001DAAB0, 0x001DACC5}) == 5   # five readers outside the worm
