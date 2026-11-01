"""MapMaker mothership: the intro's Timesplitter mothership boss in MapMaker story play.

Compiles src/interface/pc_mapmaker_mothership.inc against fake guest memory
(with mutations that must be caught) and, when the private XBE, its
disassembly and the game paks are available, checks the stock facts the hooks
rely on: the ship's init and states, the start action that only sets the
attack state, the firing box and dust constants (read only by the ship), the
shared crash floor, the death path, that no level event is reachable, the
bar's text site, the placeholder id, and the models' pak with its files
identical to every other pak's.
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
INC = ROOT / 'src/interface/pc_mapmaker_mothership.inc'

MUTATIONS = {
    'always-on': ('    return mapmaker_limits.limits;\n}', '    return 1;\n}'),
    'any-mode': ('return msh_on() && WORD_AT(MSH_SCENE)==100u && MSH_B(MSH_MODE)==10u; }', 'return msh_on() && WORD_AT(MSH_SCENE)==100u; }'),
    'any-scene': ('return msh_on() && WORD_AT(MSH_SCENE)==100u && MSH_B(MSH_MODE)==10u; }', 'return msh_on() && MSH_B(MSH_MODE)==10u; }'),
    'mine-any-scene': ('obj==msh_lv.obj && WORD_AT(MSH_SCENE)==100u && msh_on(); }', 'obj==msh_lv.obj && msh_on(); }'),
    'mine-any-object': ('return msh_lv.obj && obj==msh_lv.obj && WORD_AT', 'return msh_lv.obj && WORD_AT'),
    'stale-resmap': ('#define MSH_RESMAP 0x008CA000u', '#define MSH_RESMAP 0x0078B860u'),
    'wrong-id': ('#define MSH_ID 0x75u', '#define MSH_ID 0x74u'),
    # mount
    'mount-wrong-pak': ('#define MSH_PAK "pak/challenge/l_64_ch.pak"', '#define MSH_PAK "pak/story/l_64_ch.pak"'),
    'mount-loads-whole': ('guest_call(sub_000622D0,3,msh.pak,0u,1u)', 'guest_call(sub_000622D0,3,msh.pak,1u,1u)'),
    'mount-any-map': ('if(!msh_play() || msh.optout || (has=msh_map_has())==0) return;', 'if(!msh_play() || msh.optout || (has=1)==0) return;'),
    'mount-ignores-optout': ('if(!msh_play() || msh.optout || (has=msh_map_has())==0) return;', 'if(!msh_play() || (has=msh_map_has())==0) return;'),
    'mount-twice': ('    if(msh_pak_mounted()) return;\n    if((mounted', '    if((mounted'),
    'mount-fills-table': ('#define MSH_MOUNT_KEEP 8u', '#define MSH_MOUNT_KEEP 0u'),
    'mount-skip-ignored': ('    if(WORD_AT(MSH_MOUNT_SKIP)) { original();return; }    /* the original does nothing */\n', ''),
    'mount-preset': ('    if(MSH_H(info+MSH_INFO_SET)!=MSH_SET_MAP) return 0;\n', ''),
    'mount-no-slots-11-20': ('    for(k=0;extra && k<MSH_B(extra+3u);k++)', '    for(k=0;0 && extra && k<MSH_B(extra+3u);k++)'),
    'mount-no-unreadable': ('WORD_AT(container)!=MSH_MAGIC) return -1;', 'WORD_AT(container)!=MSH_MAGIC) return 0;'),
    'mount-keeps-level': ('    memset(&msh_lv,0,sizeof msh_lv);                       /* a new level: forget the last ship */\n', ''),
    # requests
    'request-never': ('    for(k=0;k<2u;k++) guest_call(sub_000A5E40,1,msh_models[k]);\n', ''),
    'request-ship-only': ('    for(k=0;k<2u;k++) guest_call(sub_000A5E40,1,msh_models[k]);', '    for(k=0;k<1u;k++) guest_call(sub_000A5E40,1,msh_models[k]);'),
    'request-any-map': ('    if(!msh_play() || msh.optout || msh_map_has()==0) return;\n    if(!msh_pak_mounted()) { msh_log("l_64_ch.pak not mounted at level init',
                        '    if(!msh_play() || msh.optout) return;\n    if(!msh_pak_mounted()) { msh_log("l_64_ch.pak not mounted at level init'),
    'request-without-pak': ('    if(!msh_pak_mounted()) { msh_log("l_64_ch.pak not mounted at level init', '    if(0) { msh_log("l_64_ch.pak not mounted at level init'),
    'request-without-rows': ('    for(k=0;k<2u;k++) if(!msh_resource(msh_models[k])) { msh_log("no resource row', '    for(k=0;0;k++) if(!msh_resource(msh_models[k])) { msh_log("no resource row'),
    # spawn
    'spawn-before-original': ('    a=WORD_AT(g_esp+4u);\n    original();\n    if(!MAPMAKER_PTR_OK(a,MSH_ACTOR) || MSH_H(a+MSH_ACTOR_CHAR)!=MSH_ID',
                              '    a=WORD_AT(g_esp+4u);\n    if(!MAPMAKER_PTR_OK(a,MSH_ACTOR) || MSH_H(a+MSH_ACTOR_CHAR)!=MSH_ID'),
    'spawn-any-character': (' || MSH_H(a+MSH_ACTOR_CHAR)!=MSH_ID || !MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u)) return;', ' || !MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u)) return;'),
    'spawn-two': ('    if(msh_lv.obj) {\n        if(msh_lv.extras', '    if(0) {\n        if(msh_lv.extras'),
    'spawn-without-pak': ('    if(!msh_pak_mounted()) { msh_log("l_64_ch.pak not mounted: the mothership', '    if(0) { msh_log("l_64_ch.pak not mounted: the mothership'),
    'spawn-any-init': ('WORD_AT(e+MSH_RES_INIT)!=MSH_INIT || ', ''),
    'spawn-unready': ('    if((m=msh_models_unready())!=0) {', '    if((m=msh_models_unready())!=0 && 0) {'),
    'spawn-empty-request': ('    return r && MAPMAKER_PTR_OK(r,0x14u) && WORD_AT(r+MSH_REQ_DATA)!=0;', '    return r && MAPMAKER_PTR_OK(r,0x14u);'),
    'spawn-insane-position': ('if(!msh_sane(pos[i]=msh_getf(ent+MSH_ENT_POS+i*4u))) { msh_log("placeholder position unusable",a,i);return; }',
                              'pos[i]=msh_getf(ent+MSH_ENT_POS+i*4u);'),
    'spawn-on-floor': ('msh_setf(args+4u,pos[1]+MSH_HOVER);', 'msh_setf(args+4u,pos[1]);'),
    'spawn-wrong-yaw': ('msh_setf(args+16u,yaw);', 'msh_setf(args+12u,yaw);msh_setf(args+16u,0.0f);'),
    'spawn-wrong-model': ('obj=guest_call(sub_000A6F90,5,MSH_MODEL,', 'obj=guest_call(sub_000A6F90,5,MSH_WRECK,'),
    'spawn-flies-in': ('    WORD_AT(obj+MSH_STATE)=MSH_ATTACK;                        /* no fly-in: attack where it stands */\n', ''),
    'spawn-hovers-only': ('#define MSH_ATTACK 2u', '#define MSH_ATTACK 1u'),
    'spawn-failed-kept': ('    if(!obj || !MAPMAKER_PTR_OK(obj,0x200u)) { msh_log', '    if(0) { msh_log'),
    # constants
    'box-not-rebased': ('    msh_rebase(pos[0],pos[1],pos[2]);\n', ''),
    'box-axes-swapped': ('    msh_setf(MSH_BOX,z-MSH_RANGE);msh_setf(MSH_BOX+4u,z+MSH_RANGE);\n    msh_setf(MSH_BOX+8u,x-MSH_RANGE);msh_setf(MSH_BOX+12u,x+MSH_RANGE);',
                         '    msh_setf(MSH_BOX,x-MSH_RANGE);msh_setf(MSH_BOX+4u,x+MSH_RANGE);\n    msh_setf(MSH_BOX+8u,z-MSH_RANGE);msh_setf(MSH_BOX+12u,z+MSH_RANGE);'),
    'dust-not-rebased': ('    msh_setf(MSH_DUST,floor);\n', ''),
    'never-restored': ('    memset(&msh_lv,0,sizeof msh_lv);                       /* a new level: forget the last ship */\n    msh_restore();\n',
                       '    memset(&msh_lv,0,sizeof msh_lv);                       /* a new level: forget the last ship */\n'),
    'restore-after-checks': ('    msh_restore();\n    if(!msh_play()', '    if(!msh_play()'),
    'saved-each-time': ('    if(!msh.saved) {\n        for(k=0;k<4u;k++) msh.stock[k]', '    {\n        for(k=0;k<4u;k++) msh.stock[k]'),
    # update
    'placeholder-kept': ('            guest_call(sub_0015DDA0,1,ent);\n            msh_log("mothership placeholder AI removed",a,ent);',
                         '            msh_log("mothership placeholder AI removed",a,ent);'),
    'placeholder-every-frame': ('        a=msh_lv.place;ent=msh_lv.place_ent;msh_lv.place=0;', '        a=msh_lv.place;ent=msh_lv.place_ent;'),
    'placeholder-focus-kept': ('WORD_AT(player+MSH_FOCUS)==ent) WORD_AT(player+MSH_FOCUS)=0;', 'WORD_AT(player+MSH_FOCUS)==ent) {}'),
    'fall-not-shifted': ('    shift=(state==MSH_FALL || state==MSH_CRASH) && msh_sane(msh_lv.floor);', '    shift=0;'),
    'fall-only': ('    shift=(state==MSH_FALL || state==MSH_CRASH) && msh_sane(msh_lv.floor);', '    shift=state==MSH_FALL && msh_sane(msh_lv.floor);'),
    'shift-always': ('    shift=(state==MSH_FALL || state==MSH_CRASH) && msh_sane(msh_lv.floor);', '    shift=msh_sane(msh_lv.floor);'),
    'shift-not-restored': ('    original();\n    msh_setf(obj+MSH_ENT_POS+4u,msh_getf(obj+MSH_ENT_POS+4u)+msh_lv.floor);', '    original();'),
    'shift-up': ('msh_setf(obj+MSH_ENT_POS+4u,msh_getf(obj+MSH_ENT_POS+4u)-msh_lv.floor);\n    original();\n    msh_setf(obj+MSH_ENT_POS+4u,msh_getf(obj+MSH_ENT_POS+4u)+msh_lv.floor);',
                 'msh_setf(obj+MSH_ENT_POS+4u,msh_getf(obj+MSH_ENT_POS+4u)+msh_lv.floor);\n    original();\n    msh_setf(obj+MSH_ENT_POS+4u,msh_getf(obj+MSH_ENT_POS+4u)-msh_lv.floor);'),
    # text
    'text-any-caller': (" || WORD_AT(g_esp)!=MSH_RET_BAR_TEXT || !msh.label ||", ' || !msh.label ||'),
    'text-any-scene': ('       !msh_lv.obj || !msh_play()) { original();return; }\n    g_eax=msh.label;', '       !msh_lv.obj || !msh_on()) { original();return; }\n    g_eax=msh.label;'),
    'text-no-ship': ('       !msh_lv.obj || !msh_play()) { original();return; }\n    g_eax=msh.label;', '       !msh_play()) { original();return; }\n    g_eax=msh.label;'),
    'text-no-ret': ('    g_eax=msh.label;\n    MAPMAKER_GUEST_RET();', '    g_eax=msh.label;'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('mothership.exe' if os.name == 'nt' else 'mothership')
    source = ROOT / 'tests/mapmaker/mapmaker_mothership.c'
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
def test_mapmaker_mothership(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_MOTHERSHIP')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker mothership ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_no_guest_code_table_and_no_save_byte():
    """Wrappers only, and the map container is only read."""
    assert not (ROOT / 'config/guest-code-patches/mothership.json').exists()
    text = INC.read_text(encoding='utf-8')
    assert not re.findall(r'MSH_[BH]\([^)]*\)\s*=[^=]', text)


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


def readers(va):
    """Instructions that read or write the address (any operand)."""
    return {i.address for i in decoded() if any(v == va for _, v in i.values())}


def test_init_case():
    """Init index 20 = sub_0021A710; its 0x539 case installs the update and damage callbacks, health 250, state 0."""
    rd = xbe_reader()
    a = by_address()
    assert struct.unpack('<I', rd(0x004FCA60 + 20 * 4, 4))[0] == 0x0021A710
    # switch on model - 0x511 (0x0021A71F), byte table 0x0021ACC8, dword table 0x0021AC90
    assert ('imm', 0xFFFFFAEF) in a[0x0021A71F].ops
    case = rd(0x0021ACC8 + 0x539 - 0x511, 1)[0]
    assert struct.unpack('<I', rd(0x0021AC90 + 4 * case, 4))[0] == 0x0021AADA
    assert ('imm', 0x00217050) in a[0x0021AB03].ops and ('imm', 3) in a[0x0021AB0D].ops
    assert mem(a[0x0021AB19])[0][4] == 0x1A0                       # state 0 (edi = 0)
    assert ('imm', 0x002193A0) in a[0x0021AB6E].ops and target(a[0x0021AB74]) == 0x0009E720
    assert mem(a[0x0021AB86])[0][4] == 0x0047D80C and f32(rd, 0x0047D80C) == 250.0


def test_states_and_start_action():
    rd = xbe_reader()
    a = by_address()
    # sub_00217050: switch on +0x1A0 (6 states)
    assert mem(a[0x002170CF])[0][4] == 0x1A0 and ('imm', 5) in a[0x00217715].ops
    assert a[0x00217720].mnemonic == 'jmp' and '0x218630' in a[0x00217720].text.lower()
    jt = [struct.unpack('<I', rd(0x00218630 + 4 * k, 4))[0] for k in range(6)]
    assert jt[0] == 0x00217727 and jt[2] == 0x0021791C
    # state 0 flies to (-500, 50, -220)
    assert f32(rd, 0x0049EA9C) == -500.0 and f32(rd, 0x0049EA98) == -220.0
    # logic action sub_00217030: only state 2 on the 'MTHS' object
    assert ('imm', 0x4D544853) in a[0x00217030].ops and target(a[0x00217035]) == 0x0023FF20
    assert mem(a[0x00217041])[0][4] == 0x1A0 and ('imm', 2) in a[0x00217041].ops
    assert a[0x0021704B].mnemonic == 'ret'
    # the damage handler at death: focus cleared, state 3
    assert mem(a[0x00219683])[0][4] == 0xFF4 and mem(a[0x00219692])[0][4] == 0x1A0 and ('imm', 3) in a[0x00219692].ops
    # state 2 focuses every player while health > 0
    assert mem(a[0x00217947])[0][4] == 0xFF4


def test_firing_box_and_dust_constants():
    """Each rebased constant is read only by the ship; their stock values."""
    rd = xbe_reader()
    stock = {0x0049EA84: -45.5, 0x0049EA88: -88.04, 0x0049EA8C: -47.8, 0x0049EA90: -67.0, 0x0049EA94: -56.0}
    sites = {0x0049EA84: 0x00218075, 0x0049EA88: 0x00217994, 0x0049EA8C: 0x00217977,
             0x0049EA90: 0x0021796E, 0x0049EA94: 0x00217953}
    for va, v in stock.items():
        assert f32(rd, va) == pytest.approx(v)
        assert readers(va) == {sites[va]}
    a = by_address()
    # box: x = player +0xA8 between [0x49EA90] and [0x49EA94], z = +0xB0 between [0x49EA88] and [0x49EA8C]
    assert any(op[4] == 0xA8 for op in mem(a[0x0021795B])) and any(op[4] == 0xB0 for op in mem(a[0x0021797F]))
    assert 'xmm' in a[0x00217953].text and a[0x00217963].mnemonic == 'jbe'


def test_crash_floor_shared():
    """The fall ends below 6.6 (0x0049E324): shared with other code, so the ship is moved, not the constant."""
    rd = xbe_reader()
    a = by_address()
    assert f32(rd, 0x0049E324) == pytest.approx(6.6)
    users = {i.function for i in decoded() if any(v == 0x0049E324 for _, v in i.values())}
    assert 0x00217050 in users and len(users) > 1
    assert mem(a[0x00217DCE])[0][4] == 0x0049E324 and mem(a[0x00217DD6])[0][4] == 0x68
    assert mem(a[0x00217E03])[0][4] == 0x1A0 and ('imm', 4) in a[0x00217E03].ops
    # state 5: wreck 0x53A at the ship, then the ship removed
    assert ('imm', 0x53A) in a[0x002185A7].ops and target(a[0x002185B2]) == 0x000A6F90
    assert target(a[0x002185B8]) == 0x000A9070
    # 'ZCTP' focus only when found
    assert ('imm', 0x5054435A) in a[0x0021855F].ops and a[0x0021856E].mnemonic == 'je'


def test_no_level_event():
    """No level event (sub_002274C0) is reachable from the ship's callbacks."""
    graph = {}
    for i in decoded():
        if i.call and target(i) is not None:
            graph.setdefault(i.function, set()).add(target(i))
    seen, todo = set(), [0x00217050, 0x002193A0, 0x00215B50, 0x0021A710]
    while todo:
        f = todo.pop()
        if f in seen:
            continue
        seen.add(f)
        todo.extend(graph.get(f, ()))
    assert 0x002274C0 not in seen and 0x00217050 in seen


def test_bar_text_site():
    a = by_address()
    assert ('imm', 0x164B) in a[0x0014CDDD].ops and a[0x0014CDE2].mnemonic == 'jmp' and target(a[0x0014CDE2]) == 0x0014D09A
    assert target(a[0x0014D09A]) == 0x0003D7B0 and ends(a, 0x0014D09A) == 0x0014D09F


def test_placeholder_and_pak_lookup():
    rd = xbe_reader()
    a = by_address()
    # 0x75: model 0x65A CHR_FARFUTURE_GUARDROBOT, no Arcade row
    assert struct.unpack('<I', rd(0x0053A3CC + 0x75 * 0x30 + 4, 4))[0] == 0x65A
    ids = {struct.unpack('<I', rd(0x00521A18 + i * 0x30 + 4, 4))[0] for i in range(150)}
    assert 0x75 not in ids
    # pak paths are formatted ("pak/%s%s/l_%d%s.pak"), so the area keeps its own string
    assert rd(0x0047874C, 20) == b'pak/%s%s/l_%d%s.pak\0'
    # lookups try the last two paks that answered first (0x000623C7/0x000623CD), then the table in order
    assert mem(a[0x000623C7])[0][4] == 0x004D19F8 and mem(a[0x000623CD])[0][4] == 0x004D19F4


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
    return {c: (pos, n, cm) for c, pos, n, cm in struct.iter_unpack('<4I', table)}


def _crc(name):
    return zlib.crc32(name.encode()) & 0xFFFFFFFF


def test_models_and_pak():
    """0x539/0x53A rows in prop_all; models not in chr.pak but in l_64_ch.pak, whose shared files are identical."""
    if not (GAME / 'arcade/l_100.pak').exists():
        pytest.skip('game paks not available locally')
    idx = _index(GAME / 'arcade/l_100.pak')
    with open(GAME / 'arcade/l_100.pak', 'rb') as f:
        pos, n, _ = idx[0xD9E2667A]                    # prop/table/prop_all_xbas.raw
        f.seek(pos)
        table = f.read(n)
    start = struct.unpack_from('<I', table, 4)[0] + 8
    props = {}
    for o in range(start, len(table) - 0x27, 0x28):
        mid, typ, _, path, _, _, _, _, flags, init = struct.unpack_from('<iIiiifiiIi', table, o)
        props[mid] = (typ, table[path + 8:table.index(b'\0', path + 8)].decode('latin1'), init)
    assert props[0x539] == (1, 'intro/intro_mothership', 20)
    assert props[0x53A][1] == 'intro/intro_mscrash'
    assert props[0x65A][1] == 'chrs/guardrobot'
    model = 'assets/ts/models/{0}/xbox/output/{1}.xbr'
    ch = GAME / 'challenge/l_64_ch.pak'
    chi = _index(ch)
    chr_ = _index(GAME / 'chr.pak')
    for m in (0x539, 0x53A):
        p = props[m][1]
        c = _crc(model.format(p, p.split('/')[-1]))
        assert c in chi and c not in chr_ and c not in idx
    p = props[0x65A][1]
    assert _crc(model.format(p, p.split('/')[-1])) in chr_
    # every file l_64_ch.pak shares with another pak is byte-identical there
    with open(ch, 'rb') as f:
        def body(fh, e):
            fh.seek(e[0])
            return fh.read(e[2] or e[1])
        for other in sorted(GAME.rglob('*.pak')):
            if other.samefile(ch):
                continue
            try:
                oi = _index(other)
            except (OSError, struct.error):
                continue
            common = set(chi) & set(oi)
            if not common:
                continue
            with open(other, 'rb') as g:
                for c in common:
                    assert body(f, chi[c]) == body(g, oi[c]), (other.name, hex(c))
