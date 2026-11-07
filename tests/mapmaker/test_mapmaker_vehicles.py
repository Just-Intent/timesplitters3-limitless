"""MapMaker story vehicles (story assets wave 3): the Machine Wars future tank
and the Scotland the Brave truck as variants of the MapMaker vehicle.

Compiles src/interface/pc_mapmaker_vehicles.inc against fake guest memory
(with mutations that must be caught) and, when the private XBE, its
disassembly and the game paks are available, checks the stock facts the hooks
rely on: the unused record byte +0xC of item 0x2B, every hooked call site and
return address, the vehicle record fields the fill writes and the builders
read, the vehicle defs, gun templates, seat anims and pool, the scene-5 gates
left alone, the story levels' vehicle records and the paks.
"""
import os
from pathlib import Path
import struct
import subprocess
import zlib
import pytest

from native_compiler import native_compiler
from mapmaker_decoded import decoded, _private

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_vehicles.inc'

MUTATIONS = {
    'node-any-tag': ('    return (v&0xFFFF0000u)==VH_NODE_TAG && (v&0xFFFFu)>=1u', '    return (v&0xFFFFu)>=1u'),
    'node-out-of-range': ('(v&0xFFFFu)>=1u && (v&0xFFFFu)<=VH_KINDS?(v&0xFFFFu):0u;', '(v&0xFFFFu)>=1u?(v&0xFFFFu):0u;'),
    'rec-out-of-range': ('    return v>=1u && v<=VH_KINDS?v:0u;', '    return v;'),
    'compile-keeps-stale': ('        VH_B(vh_recs[k].at+VH_REC_VAR)=(uint8_t)v;\n', '        if(v) VH_B(vh_recs[k].at+VH_REC_VAR)=(uint8_t)v;\n'),
    'compile-ignores-switch': ('    if(!vh_on() || WORD_AT(VH_SCENE)!=102u) return 0;', '    if(WORD_AT(VH_SCENE)!=102u) return 0;'),
    'compile-any-scene': ('    if(!vh_on() || WORD_AT(VH_SCENE)!=102u) return 0;', '    if(!vh_on()) return 0;'),
    'items-any-id': ('            if((uint32_t)(int32_t)VH_H(p+0xEu)==VH_ITEM) fn(p,piece,ctx);', '            fn(p,piece,ctx);'),
    'nodes-any-id': ('            if(WORD_AT(n)!=VH_ITEM || vh_nn>=VH_MAXN) continue;', '            if(vh_nn>=VH_MAXN) continue;'),
    'pair-ignores-cell': ("k->key[7]=VH_B(n+0xCu);", "k->key[7]=0;"),
    'pair-ignores-piece': ('            memcpy(k->key,pk,4);\n', '            memset(k->key,0,4);\n'),
    'load-keeps-junk': ('    for(k=0;k<vh_nn;k++) WORD_AT(vh_nodes[k].at+VH_NODE_VAR)=VH_NODE_TAG;\n', ''),
    'category-any-caller': ("((ret=WORD_AT(g_esp))!=VH_CAT_RET1 && ret!=VH_CAT_RET2)", "((ret=WORD_AT(g_esp)),0)"),
    'category-any-item': (" || WORD_AT(g_esp+4u)!=VH_ITEM) {\n", ") {\n"),
    'category-edits-stock': ('    g_eax=vh.cat;\n', '    WORD_AT(g_eax+8u)=vh.desc;\n'),
    'category-over-menu': ('!MAPMAKER_PTR_OK(g_eax,24u) || WORD_AT(g_eax+8u) || !(n=WORD_AT(e+VH_SELECTED))',
                           '!MAPMAKER_PTR_OK(g_eax,24u) || !(n=WORD_AT(e+VH_SELECTED))'),
    'build-any-menu': ('|| WORD_AT(menu+4u)!=vh.desc) { original();return; }', ') { original();return; }'),
    'build-runs-original': ('    guest_call(sub_00077320,2,WORD_AT(VH_POS_LIST),y);\n    MAPMAKER_GUEST_RET();', '    guest_call(sub_00077320,2,WORD_AT(VH_POS_LIST),y);\n    original();'),
    'build-three-choices': ('        for(k=0;k<=VH_KINDS;k++) guest_call', '        for(k=0;k<VH_KINDS;k++) guest_call'),
    'build-wrong-bind': ('        guest_call(sub_00078590,1,n+VH_NODE_VAR);', '        guest_call(sub_00078590,1,n+0x2Cu);'),
    'build-keeps-stale': ('        WORD_AT(n+VH_NODE_VAR)=VH_NODE_TAG|vh_node_var(n);      /* a stale value becomes the theme vehicle */\n', ''),
    'mount-whole-pak': ('guest_call(sub_000622D0,3,vh.paks[j],0u,1u)', 'guest_call(sub_000622D0,3,vh.paks[j],1u,1u)'),
    'mount-tank-pak': ('"pak/stream/lv5/prop1.pak"', '"pak/stream/lv5/prop4.pak"'),
    'mount-truck-pak': ('"pak/stream/lv4/prop3.pak"', '"pak/stream/lv4/prop6.pak"'),
    'mount-any-map': ('        if(!vh_pak_wanted(j) || !vh_pak(j) || vh_slot(j)!=-1) continue;', '        if(!vh_pak(j) || vh_slot(j)!=-1) continue;'),
    'mount-twice': ('        if(!vh_pak_wanted(j) || !vh_pak(j) || vh_slot(j)!=-1) continue;', '        if(!vh_pak_wanted(j) || !vh_pak(j)) continue;'),
    'mount-table-full': ('if((mounted=WORD_AT(VH_MOUNTS))+VH_MOUNT_KEEP>VH_MOUNT_MAX)', 'if((mounted=WORD_AT(VH_MOUNTS))>VH_MOUNT_MAX)'),
    'mount-skip-ignored': ('    if(WORD_AT(VH_MOUNT_SKIP)) { original();return; }\n    original();\n    memset(&vh_lv', '    original();\n    memset(&vh_lv'),
    'mount-editor': ('static int vh_play(void) { return vh_on() && WORD_AT(VH_SCENE)==100u; }', 'static int vh_play(void) { return vh_on() && WORD_AT(VH_SCENE)>=100u; }'),
    'mount-stale-level': ('    memset(&vh_lv,0,sizeof vh_lv);\n    if(!vh_play()) return;', '    if(!vh_play()) return;'),
    'wanted-theme-vehicle': ('    for(v=1u;v<=VH_KINDS;v++) if(vh_lv.want[v] && vh_kind[v-1u].pak==j) return 1;', '    for(v=0u;v<=VH_KINDS;v++) if(vh_lv.want[v]) return 1;'),
    'available-no-rows': ('    for(i=0;i<vh_kind[v-1u].nmodels;i++) if(!vh_resource(vh_kind[v-1u].models[i])) return 0;\n', ''),
    'available-no-pak': (' || vh_slot(vh_kind[v-1u].pak)==-1) return 0;', ') return 0;'),
    'ready-vehicle-only': ('    for(i=0;i<vh_kind[v-1u].ready;i++) if(!vh_model_read', '    for(i=0;i<1u;i++) if(!vh_model_read'),
    'ready-unread': ('    return r && WORD_AT(r+0x10u)!=0;', '    return r!=0;'),
    'ready-ignores-loaded': ('    if(WORD_AT(e+4u)) return 1;\n    r=vh_request', '    r=vh_request'),
    'tank-gibs-not-rowed': ('{0xBu,0xCu,0xDu,0x54Du,0x54Eu,0x54Fu},6u,3u,0u', '{0xBu,0xCu,0xDu,0x54Du,0x54Eu,0x54Fu},3u,3u,0u'),
    'tank-wrong-tracks': ('{2u,0xBu,4u,4u,1u,2u,{1,2}', '{2u,0xBu,0u,1u,1u,2u,{1,2}'),
    'tank-no-aa-gun': ('{2u,0xBu,4u,4u,1u,2u,{1,2}', '{2u,0xBu,4u,4u,1u,0xFFFFFFFFu,{1,2}'),
    'tank-wrong-templates': ('{2u,0xBu,4u,4u,1u,2u,{1,2}', '{2u,0xBu,4u,4u,1u,2u,{0,1}'),
    'truck-gun-wrong-template': ('{0u,7u,0u,1u,0u,0xFFFFFFFFu,{0,-1}', '{0u,7u,0u,1u,0u,0xFFFFFFFFu,{1,-1}'),
    'truck-always-gun': ('{0u,7u,0u,1u,0xFFFFFFFFu,0xFFFFFFFFu,{-1,-1}', '{0u,7u,0u,1u,0u,0xFFFFFFFFu,{-1,-1}'),
    'textures-every-time': ('        if(vh_lv.textured[j]) continue;\n', ''),
    'textures-keep-mark': ('    VH_B(r+2u)&=(uint8_t)~0x10u;                       /* registered: no creation-time pass */\n', ''),
    'textures-creation-way': ('    guest_call(sub_00096220,1,path);', '    guest_call(sub_00096220,1,file);'),
    'textures-gibs': ('            for(i=0;i<vh_kind[v-1u].ready;i++) {\n                uint32_t m=', '            for(i=0;i<vh_kind[v-1u].nmodels;i++) {\n                uint32_t m='),
    'textures-outside-play': ('    if(vh_play()) for(j=0;j<VH_PAKS;j++) {', '    for(j=0;j<VH_PAKS;j++) {'),
    'textures-ignore-mark': ('    if(!(VH_B(r+2u)&0x10u)) return;                    /* registered by the load pass */\n', ''),
    'preload-any-caller': ('    if(!MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=VH_PRELOAD_RET) { original();return; }', '    if(!MAPMAKER_STACK_OK(8)) { original();return; }'),
    'preload-any-item': ("    if(!vh_play() || !MAPMAKER_PTR_OK(g_esi,0x18u) || (uint32_t)(int32_t)VH_H(g_esi+0xEu)!=VH_ITEM ||\n       !(v=vh_rec_var(g_esi)) || !vh_lv.want[v]) return;\n    if(!vh_available(v)) {",
                         "    if(!vh_play() || !MAPMAKER_PTR_OK(g_esi,0x18u) ||\n       !(v=vh_rec_var(g_esi)) || !vh_lv.want[v]) return;\n    if(!vh_available(v)) {"),
    'preload-unavailable': ('    if(!vh_available(v)) {\n        if(!vh_lv.refused[v]++)', '    if(0) {\n        if(!vh_lv.refused[v]++)'),
    'def-any-caller': ("((ret=WORD_AT(g_esp))!=VH_DEF_PRELOAD_RET && ret!=VH_DEF_FILL_RET)) { original();return; }",
                       "((ret=WORD_AT(g_esp)),0)) { original();return; }"),
    'def-no-templates': ('            guest_call(sub_000B3B20,1,VH_TEMPLATES+(uint32_t)vh_kind[v-1u].templates[i]*VH_TEMPLATE);\n', '            ;\n'),
    'def-preload-unavailable': ('    if(ret==VH_DEF_PRELOAD_RET) {\n        if(!vh_available(v)) return;', '    if(ret==VH_DEF_PRELOAD_RET) {'),
    'def-fill-unready': ('    if(!vh_ready(v)) {\n        if(vh_lv.refused[v]++<4u)', '    if(!vh_available(v)) {\n        if(vh_lv.refused[v]++<4u)'),
    'def-fill-no-model': ('    WORD_AT(g_ebp+8u)=vh_kind[v-1u].model;\n', ''),
    'def-outside-play': ("    if(!vh_play() || !MAPMAKER_PTR_OK(g_esi,0x18u) || (uint32_t)(int32_t)VH_H(g_esi+0xEu)!=VH_ITEM ||\n       !(v=vh_rec_var(g_esi)) || !vh_lv.want[v]) return;\n    if(ret==",
                         "    if(!MAPMAKER_PTR_OK(g_esi,0x18u) || (uint32_t)(int32_t)VH_H(g_esi+0xEu)!=VH_ITEM ||\n       !(v=vh_rec_var(g_esi)) || !vh_lv.want[v]) return;\n    if(ret=="),
    'create-any-caller': ('    if(!MAPMAKER_STACK_OK(12) || WORD_AT(g_esp)!=VH_CREATE_RET || !vh_play()) { original();return; }',
                          '    if(!MAPMAKER_STACK_OK(12) || !vh_play()) { original();return; }'),
    'create-after-original': ('    } else if(v) vh_log("a story vehicle record changed before its create: left as filled (record, variant)",rec,v);\n    original();',
                              '    } else if(v) vh_log("a story vehicle record changed before its create: left as filled (record, variant)",rec,v);'),
    'create-unchecked-record': (' && WORD_AT(rec+0x24u)==vh_kind[v-1u].def && WORD_AT(rec+8u)==vh_kind[v-1u].model) {', ') {'),
    'create-no-34': ('WORD_AT(rec+0x34u)=vh_kind[v-1u].r34;', ';'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('vehicles.exe' if os.name == 'nt' else 'vehicles')
    source = ROOT / 'tests/mapmaker/mapmaker_vehicles.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/wd4706',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src'),
                str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), '-I' + str(ROOT / 'src'),
                str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_vehicles(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker vehicles ok' in result.stdout
        err = result.stderr
        assert '[mapmaker] vehicles: mounted pak/stream/lv5/prop1.pak for the tanks (table 41; tanks 1, trucks 2)' in err
        assert '[mapmaker] vehicles: mounted pak/stream/lv4/prop3.pak for the trucks' in err
        assert '[mapmaker] vehicles: model 0xB textures: 2 of 2 in pak/stream/lv5/prop1.pak' in err
        assert '[mapmaker] vehicles: model 0xE textures: 1 of 2 in pak/stream/lv4/prop3.pak (first missing 10000F)' in err
        assert '[mapmaker] vehicles: vehicle placed as the Machine Wars future tank' in err
        assert '[mapmaker] vehicles: vehicle placed as the Scotland the Brave truck with its Vickers gun' in err
        assert '[mapmaker] vehicles: created a future tank' in err
        assert '[mapmaker] vehicles: story vehicle models not read this level: it stays the theme vehicle' in err
        assert '[mapmaker] vehicles: saved story vehicles (items, records) (0x3, 0x4)' in err
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_wrappers_declared_no_table():
    """Wrappers only; the variant lives in a stock-unused record byte, no fork chunk or table."""
    assert not (ROOT / 'config/guest-code-patches/vehicles.json').exists()
    src = INC.read_text(encoding='utf-8')
    assert '0x00C776' not in src.replace('0x00C77667u', '').replace('0x00C77668u', '').replace(
        '0x00C77669u', '').replace('0x00C7766Au', '')
    guest = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    assert guest.index('#include "pc_mapmaker_storyprops.inc"') < guest.index('#include "pc_mapmaker_vehicles.inc"')
    header = (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    for hook in ('compile', 'load', 'category', 'build', 'mount', 'textures', 'preload_model', 'def', 'create'):
        assert f'void pc_mapmaker_vehicles_{hook}(void (*original)(void));' in header
        assert f'void pc_mapmaker_vehicles_{hook}(void (*original)(void)) {{ original(); }}' in guest
    # category 35 (the vehicle pool of 4) is not raised by the limits
    limits = (ROOT / 'src/interface/pc_mapmaker_limits.inc').read_text(encoding='utf-8')
    assert '{0x00524F2Cu,' not in limits and '{0x00524F44u,4u,8u}' in limits


# ---- stock facts (skipped without the private XBE) --------------------------

def by_address():
    return {i.address: i for i in decoded()}


def mem(i):
    return [op for op in i.ops if op[0] == 'mem']


def disp(i):
    return [op[4] for op in mem(i)]


def ret_of(a, site, target):
    call = a[site]
    assert call.call and f'0x{target:x}' in call.text, (hex(site), call.text)
    return site + len(bytes.fromhex(call.hex))


def store(i, base, off, value=None):
    """i is `mov dword ptr [base + off], value`."""
    if i.mnemonic != 'mov' or not i.ops or i.ops[0][0] != 'mem' or i.ops[0][1] != base or i.ops[0][4] != off:
        return False
    return value is None or i.ops[1] == ('imm', value & 0xFFFFFFFF)


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


def u32(rd, va):
    return struct.unpack('<I', rd(va, 4))[0]


def cstr(rd, va):
    return rd(va, 64).split(b'\0')[0].decode('latin1')


def test_record_byte_unused_for_the_vehicle():
    """Item 0x2B (category 35) takes the compiler's default branch, which zeroes
    +0..+0xA only; the editor loader and the play fill read +0xC only for 0x35..0x37."""
    a = by_address()
    rd = xbe()
    assert struct.unpack('<h', rd(0x00524F58 + 0x2B * 0xC4 + 2, 2))[0] == 35
    # the special ids and categories of sub_00300800 (0x2B is none of them), then the default
    ids = {op[1] for x in range(0x00300EE8, 0x00300FB6) if x in a for op in a[x].ops if op[0] == 'imm'
           and a[x].mnemonic == 'cmp'}
    assert ids >= {0x35, 0x36, 0x37, 0x32, 0x33, 0x34, 0xA, 0xE, 5, 6, 7, 8, 8, 9, 0x13, 0x14, 0x15, 0x1F, 0x43}
    assert 0x2B not in ids and 35 not in ids
    assert ('imm', 0x43) in a[0x00300FB2].ops and '0x300ffa' in a[0x00300FB5].text
    assert [disp(a[x]) for x in (0x00300FFC, 0x00301001, 0x00301006, 0x0030100A)] == [[0xA], [8], [4], [0]]
    loads = [i.address for i in decoded() if i.function == 0x00301A90 and mem(i) and mem(i)[0][1] == 'esi'
             and mem(i)[0][4] == 0xC and i.ops[0][0] == 'reg']
    assert loads == [0x003021C6, 0x003023AC]
    fills = [i.address for i in decoded() if i.function == 0x00240C50 and mem(i) and mem(i)[0][1] == 'esi'
             and mem(i)[0][4] == 0xC and i.ops[0][0] == 'reg']
    assert sorted(fills) == [0x00241BFB, 0x00241C0E]
    # the category entry has no options menu; the cap is the pool's 4
    assert u32(rd, 0x00524BE0 + 35 * 24 + 8) == 0 and u32(rd, 0x00524BE0 + 35 * 24 + 4) == 4


def test_fill_vehicle_case():
    """sub_00240C50 case 0x2B (0x00241CC4): the 0x40-byte record at ebp gets +8 model
    before the def call sub_0033DDA0(id, theme) (esi = the item); after it +0x24 def,
    +0x28 0, +0x2C 1, +0x30 -1; +0x34 is never written; the def is 3 for model 0x892, else 5."""
    a = by_address()
    rd = xbe()
    table = rd(0x00242568, 0x50)
    case = struct.unpack('<I', rd(0x00242544 + table[0x2B] * 4, 4))[0]
    assert case == 0x00241CC4
    assert store(a[0x00241D16], 'ebp', 8) and '0x2f7790' in a[0x00241D08].text
    assert 0xE in disp(a[0x00241D2E]) and mem(a[0x00241D2E])[0][1] == 'esi'
    assert ret_of(a, 0x00241D34, 0x33dda0) == 0x00241D39
    assert store(a[0x00241D3C], 'ebp', 0x24)
    assert store(a[0x00241D3F], 'ebp', 0x28, 0) and store(a[0x00241D46], 'ebp', 0x2C, 1)
    assert store(a[0x00241D4D], 'ebp', 0x30, -1) and ('imm', 0x40) in a[0x00241D54].ops
    body = [i for x, i in a.items() if 0x00241CC4 <= x < 0x00241D5B]
    assert not any(mem(i) and mem(i)[0][1] == 'ebp' and mem(i)[0][4] == 0x34 for i in body)
    assert ('imm', 0x892) in a[0x0033DDB6].ops and ('imm', 5) in a[0x0033DDBC].ops and ('imm', 3) in a[0x0033DDCE].ops
    assert u32(rd, 0x00524F58 + 0x2B * 0xC4 + 0xC) == 0x894        # theme 0's model: the virtual buggy


def test_preload_vehicle_case():
    """sub_0023F390: the item's model through sub_0006C090 (0x0023F5DB), then for 0x2B
    sub_00180700(sub_0033DDA0(id, theme)) (0x0023F637): its 7 sounds, and for def 2 the
    gib models 0x54D..0x54F. The story preload sub_001807A0 also preloads the gun
    templates +0x30/+0x34 through sub_000B3B20 (model, gun type, sounds); MapMaker's does not."""
    a = by_address()
    rd = xbe()
    idx = rd(0x0023F924, 1)[0]
    assert struct.unpack('<I', rd(0x0023F914 + idx * 4, 4))[0] == 0x0023F62C
    assert ret_of(a, 0x0023F5D6, 0x6c090) == 0x0023F5DB
    assert ret_of(a, 0x0023F632, 0x33dda0) == 0x0023F637 and '0x180700' in a[0x0023F638].text
    assert 0xE in disp(a[0x0023F612]) and mem(a[0x0023F612])[0][1] == 'esi'
    assert ('imm', 2) in a[0x0018076B].ops and [a[x].ops for x in (0x00180772, 0x0018077C)] == [(('imm', 0x54D),), (('imm', 0x54E),)]
    assert '0x45590' in a[0x0018071B].text
    # story preload: model +8, def +0x24, templates +0x30/+0x34
    assert [disp(a[x]) for x in (0x001807A5, 0x001807AE, 0x001807B7, 0x001807D5)] == [[8], [0x24], [0x30], [0x34]]
    assert '0xb3b20' in a[0x001807CD].text and ('imm', 0x004E8300) in a[0x001807C7].ops
    assert 0xC in disp(a[0x000B3B25]) and '0xa5e40' in a[0x000B3B29].text and '0x1b6010' in a[0x000B3B3A].text


def test_create_site():
    """sub_0006D7A0 creates the object from record +8 (sub_000A6F90, 0x0006E934), then
    sub_00182A20(object, record) (0x0006E949). The restart's sub_0006ED70 runs the same
    sub_0006D7A0 (0x0006EDD2) without the fill, so the records keep the fill's values."""
    a = by_address()
    assert ret_of(a, 0x0006E944, 0x182a20) == 0x0006E949
    assert a[0x0006E942].text.split(';')[0].strip() == 'esi' and a[0x0006E943].text.split(';')[0].strip() == 'edi'
    assert 8 in disp(a[0x0006E925]) and '0xa6f90' in a[0x0006E934].text
    assert '0x6d7a0' in a[0x0006EDD2].text and '0x6d7a0' in a[0x0006ED4D].text and '0x240c50' in a[0x0006D80C].text
    # the create stores the record at obj+0x2D8, reads its def +0x24 and calls the def's builder (+0xB0)
    assert 0x2D8 in disp(a[0x00182A35]) and 0x24 in disp(a[0x00182A87]) and 0xB0 in disp(a[0x00182B86])


def test_defs_builders_and_records_fields():
    """Def table 0x004E84A8 + def*0xD8: builders 0 truck sub_00180BA0, 2 tank sub_00181D40,
    3 jeep, 5 buggy. The tank reads record +0x28 (tracks row), +0x30 and +0x34 (gun
    templates); the truck +0x28/+0x2C (wheels) and +0x30 >= 0 (Vickers gunner seat,
    template 0, seat anim 0x83). Templates 0x004E8300 + i*0x8C: models 0xE, 0xC, 0xD."""
    a = by_address()
    rd = xbe()
    builder = lambda k: u32(rd, 0x004E84A8 + k * 0xD8 + 0xB0)
    assert [builder(k) for k in (0, 2, 3, 5)] == [0x00180BA0, 0x00181D40, 0x0017F830, 0x0017FF70]
    # tracked flag: def +0xA8 bit 0 (vehicle block +0x58, 0x00182B51) for the tank only
    assert [u32(rd, 0x004E84A8 + k * 0xD8 + 0xA8) & 1 for k in (0, 2, 3, 5)] == [0, 1, 0, 0]
    assert 0x58 in disp(a[0x00182B51]) and 0xA8 in disp(a[0x00182B4B])
    assert 0x28 in disp(a[0x001823AF]) and ('imm', 0x004E8270) in a[0x001823E0].ops
    assert 0x30 in disp(a[0x00182851]) and '0x180920' in a[0x00182888].text
    assert 0x34 in disp(a[0x00182898]) and '0x180920' in a[0x001828D6].text
    assert 0x30 in disp(a[0x00181181]) and a[0x001811C5].mnemonic == 'xor' and '0x180920' in a[0x0018122F].text
    assert store(a[0x00181217], 'esi', 0xE8, 0x83)
    assert [0x28 in disp(a[x]) or 0x2C in disp(a[x]) for x in (0x0018136C, 0x001813C9)] == [True, True]
    assert ('imm', 0x8C) in a[0x00180937].ops and ('imm', 0x004E8300) in a[0x001809E2].ops
    tmpl = [struct.unpack('<6I', rd(0x004E8300 + i * 0x8C, 24)) for i in range(3)]
    assert [t[3] for t in tmpl] == [0xE, 0xC, 0xD] and [t[5] for t in tmpl] == [0x2A, 0x3B, 0x3C]
    # the gun's object from sub_000B3B80 is used without a null test
    assert '0xa6f90' in a[0x000B3BB5].text and 0x60 in disp(a[0x000B3BBF])
    assert not any(i.mnemonic == 'test' for x, i in a.items() if 0x000B3BBA <= x < 0x000B3BBF)


def test_seat_anims_and_pool():
    """The truck's seats play 0x78..0x83, all in the animcar table (0x0047FDF0, 27
    entries, registered when pak/anim/animcar.pak is mounted; scene 100 mounts it);
    the tank's seats play none. The vehicle pool has 4 blocks."""
    a = by_address()
    rd = xbe()
    seats = {a[x].ops[1][1] for x in range(0x001810F7, 0x00181181) if x in a and a[x].mnemonic == 'mov'
             and mem(a[x]) and mem(a[x])[0][1] == 'esi' and a[x].ops[1][0] == 'imm' and 0x70 <= a[x].ops[0][4] < 0xC4}
    assert seats >= {0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82}
    tank_seats = [a[x] for x in range(0x00182791, 0x00182851) if x in a and mem(a[x]) and mem(a[x])[0][1] == 'esi'
                  and 0x78 <= mem(a[x])[0][4] <= 0x8C]
    assert tank_seats and all(i.ops[1] == ('reg', 'ebx') for i in tank_seats)
    names = {struct.unpack('<I', rd(0x0047FDF0 + k * 0x18 + 0x10, 4))[0]: cstr(rd, u32(rd, 0x0047FDF0 + k * 0x18))
             for k in range(0x1B)}
    assert set(range(0x78, 0x84)) <= set(names) and names[0x79] == 'getintruck' and names[0x83] == 'gunner'
    assert ('imm', 0x1B) in a[0x001CA4A4].ops and ('imm', 0x0047FDF0) in a[0x001CA4A6].ops
    assert ('imm', 0x0047E6B0) in a[0x001CA492].ops and cstr(rd, 0x0047E6B0) == 'pak/anim/animcar.pak'
    assert ('imm', 0x64) in a[0x001C878A].ops and ('imm', 0x0047E6B0) in a[0x001C878F].ops
    assert ('imm', 4) in a[0x0017F340].ops and ('imm', 0x970) in a[0x0017F33A].ops


def test_scene5_gates_left_alone():
    """The death handler (gibs, explosions) and the impact hook run their story parts
    only in scene 5; in scene 100 both do nothing, as for the stock buggies."""
    a = by_address()
    assert 0x0079094C in disp(a[0x0017414A]) and ('imm', 5) in a[0x00174152].ops and '0x1747bf' in a[0x00174167].text
    assert 0x0079094C in disp(a[0x0017BB10]) and ('imm', 5) in a[0x0017BB19].ops and '0x21bc00' in a[0x0017BB2B].text
    assert ('imm', 0x00174120) in a[0x00182C1B].ops          # slot 1: the death handler


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


def _body(pak, crc):
    e = _index(pak)[crc]
    with open(pak, 'rb') as f:
        f.seek(e[0])
        return f.read(e[2] or e[1])


def _crc(s):
    return zlib.crc32(s.encode()) & 0xFFFFFFFF


def _props():
    table = _body(GAME / 'arcade/l_100.pak', 0xD9E2667A)
    start = struct.unpack_from('<I', table, 4)[0] + 8
    out = {}
    for o in range(start, len(table) - 0x27, 0x28):
        mid, typ, _, path = struct.unpack_from('<iIii', table, o)
        out[mid] = (typ, table[path + 8:table.index(b'\0', path + 8)].decode('latin1'))
    return out


def _model(path):
    return _crc('assets/ts/models/{0}/xbox/output/{1}.xbr'.format(path, path.split('/')[-1]))


def _textures(data):
    off, ids = struct.unpack_from('<I', data, 0)[0], []
    while struct.unpack_from('<I', data, off)[0] != 0xFFFFFFFF:
        ids.append(struct.unpack_from('<I', data, off + 4)[0] & 0xFFFFFF)
        off += 16
    return ids


def test_story_vehicle_records():
    """The story levels' records, whose fields the variants copy: l_5_st 'TANK',
    'TNK2', 'TNK3' (model 0xB, def 2, +0x28 4, +0x2C 4, +0x30 1, +0x34 2) and l_4_st
    'TRCK' (model 7, def 0, 0, 1, gunner 0, -1) plus a truck without a gunner (-1)."""
    if not (GAME / 'story/l_5_st.pak').exists():
        pytest.skip('game paks not available locally')
    lvl = _body(GAME / 'story/l_5_st.pak', 0x98C8FB9B)
    for o, tag in ((0x393E8, b'KNAT'), (0x39428, b'2KNT'), (0x39468, b'3KNT')):
        w = struct.unpack_from('<14I', lvl, o)
        assert lvl[o:o + 4] == tag and w[2] == 0xB and w[9:14] == (2, 4, 4, 1, 2)
    lvl = _body(GAME / 'story/l_4_st.pak', 0x1256A5D5)
    w = struct.unpack_from('<14I', lvl, 0x2B270)
    assert lvl[0x2B270:0x2B274] == b'KCRT' and w[2] == 7 and w[9:14] == (0, 0, 1, 0, 0xFFFFFFFF)
    w = struct.unpack_from('<14I', lvl, 0x2B2B0)
    assert w[2] == 7 and w[9:14] == (0, 0, 1, 0xFFFFFFFF, 0xFFFFFFFF)
    src = INC.read_text(encoding='utf-8')
    assert '{2u,0xBu,4u,4u,1u,2u,{1,2}' in src and '{0u,7u,0u,1u,0u,0xFFFFFFFFu,{0,-1}' in src
    assert '{0u,7u,0u,1u,0xFFFFFFFFu,0xFFFFFFFFu,{-1,-1}' in src


def test_models_textures_and_paks():
    """prop_all rows for every model the variants create; each model and all its
    textures are in the mounted stream pak (no memory pak needed), and every file
    those paks share with another pak is byte-identical there. The seat anims are
    all in animcar.pak."""
    if not (GAME / 'arcade/l_100.pak').exists():
        pytest.skip('game paks not available locally')
    props = _props()
    assert props[0xB] == (0x400000, 'rbcar/futuretank') and props[7] == (0x400000, 'rbcar/truck')
    assert props[0xC] == (0x200, 'rbcar/futuretankMAINgunturret') and props[0xD] == (0x200, 'rbcar/futuretankAAgunturret')
    assert props[0xE] == (0x200, 'rbcar/vickerstruck')
    assert [props[m][1] for m in (0x54D, 0x54E, 0x54F)] == ['rbcar/tankgib1', 'rbcar/tankgib2', 'rbcar/tankgib3']
    for pak, ms in (('stream/lv5/prop1.pak', (0xB, 0xC, 0xD, 0x54D, 0x54E, 0x54F)), ('stream/lv4/prop3.pak', (7, 0xE))):
        idx = _index(GAME / pak)
        for m in ms:
            c = _model(props[m][1])
            assert c in idx, (pak, hex(m))
            for t in _textures(_body(GAME / pak, c)):
                assert _crc('textures/%06x.xbt' % t) in idx, (pak, hex(m), hex(t))
        with open(GAME / pak, 'rb') as f:
            for other in sorted(GAME.rglob('*.pak')):
                if other.samefile(GAME / pak):
                    continue
                try:
                    oi = _index(other)
                except (OSError, struct.error):
                    continue
                with open(other, 'rb') as g:
                    for c in set(idx) & set(oi):
                        f.seek(idx[c][0]); g.seek(oi[c][0])
                        assert f.read(idx[c][2] or idx[c][1]) == g.read(oi[c][2] or oi[c][1]), (pak, other.name, hex(c))
    src = INC.read_text(encoding='utf-8')
    assert '"pak/stream/lv5/prop1.pak","pak/stream/lv4/prop3.pak"' in src
    car = _index(GAME / 'anim/animcar.pak')
    rd = xbe()
    for k in range(0x1B):
        name = cstr(rd, u32(rd, 0x0047FDF0 + k * 0x18))
        assert _crc('anim/data/ts3/%s.raw' % name) in car, name
