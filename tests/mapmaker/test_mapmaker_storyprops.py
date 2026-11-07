"""MapMaker story props (story assets wave 1): the mission 1 power cell as an
explosive drum variant and the prison fixed gun as a fixed gun variant.

Compiles src/interface/pc_mapmaker_storyprops.inc against fake guest memory
(with mutations that must be caught) and, when the private XBE, its
disassembly and the game paks are available, checks the stock facts the hooks
rely on: the unused record byte +0xC for these items, the node field +0x30,
every hooked call site and return address, the fill's prop/turret cases, the
power cell's init/damage/explosion, the prison gun's rows and the paks.
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
INC = ROOT / 'src/interface/pc_mapmaker_storyprops.inc'

MUTATIONS = {
    'compile-any-node': ('    return (v&0xFFFF0000u)==SP_NODE_TAG && (v&0xFFFFu)==1u?1u:0u;', '    return (v&0xFFFFu)==1u?1u:0u;'),
    'compile-keeps-stale': ('        SP_B(sp_recs[k].at+SP_REC_VAR)=(uint8_t)v;\n', '        if(v) SP_B(sp_recs[k].at+SP_REC_VAR)=(uint8_t)v;\n'),
    'compile-ignores-switch': ('    if(!sp_on() || WORD_AT(SP_SCENE)!=102u) return 0;', '    if(WORD_AT(SP_SCENE)!=102u) return 0;'),
    'kind-any-item': ('    if(id>=SP_GUN_FIRST && id<=SP_GUN_LAST) return 1;', '    if(id>=SP_GUN_FIRST && id<=0x37u) return 1;'),
    'kind-pickups': ('    if(id==SP_DRUM) return 0;', '    if(id==SP_DRUM || id==0x11u) return 0;'),
    'pair-ignores-piece': ('            memcpy(k->key,pk,4);\n', '            memset(k->key,0,4);\n'),
    'pair-ignores-cell': ("k->key[7]=SP_B(n+0xCu);", "k->key[7]=0;"),
    'pair-rec-ignores-cell': ("k->key[7]=SP_B(item+0x15u);", "k->key[7]=SP_B(item+0x16u);"),
    'load-keeps-junk': ('    for(k=0;k<sp_nn;k++) WORD_AT(sp_nodes[k].at+SP_NODE_VAR)=SP_NODE_TAG;\n', ''),
    'load-any-byte': ('        if(SP_B(sp_recs[k].at+SP_REC_VAR)!=1u) continue;', '        if(!SP_B(sp_recs[k].at+SP_REC_VAR)) continue;'),
    'category-any-caller': ("((ret=WORD_AT(g_esp))!=SP_CAT_RET1 && ret!=SP_CAT_RET2)", "((ret=WORD_AT(g_esp)),0)"),
    'category-any-item': (" || WORD_AT(g_esp+4u)!=SP_DRUM) {\n", ") {\n"),
    'category-edits-stock': ('    g_eax=sp.cat;\n', '    WORD_AT(g_eax+8u)=sp.desc;\n'),
    'category-no-copy': ('    memcpy((void *)((uintptr_t)sp.cat+g_xbox_mem_offset),(const void *)((uintptr_t)g_eax+g_xbox_mem_offset),24u);\n', ''),
    'build-any-menu': ('|| WORD_AT(menu+4u)!=sp.desc) { original();return; }\n    e=sp_editor()', ') { original();return; }\n    e=sp_editor()'),
    'build-runs-original': ('    guest_call(sub_00077320,2,WORD_AT(SP_POS_LIST),y);\n    MAPMAKER_GUEST_RET();', '    guest_call(sub_00077320,2,WORD_AT(SP_POS_LIST),y);\n    original();'),
    'build-no-done': ('    guest_call(sub_00079D60,6,guest_call(sub_0003D7B0,1,SP_DONE_TEXT),0xFFFFFFFFu,0u,0x40000000u,SP_CLOSE,0u);\n', ''),
    'build-wrong-bind': ('        guest_call(sub_00078590,1,n+SP_NODE_VAR);\n        guest_call(sub_000783D0,0);\n    } else', '        guest_call(sub_00078590,1,n+0x2Cu);\n        guest_call(sub_000783D0,0);\n    } else'),
    'build-keeps-stale': ('        WORD_AT(n+SP_NODE_VAR)=SP_NODE_TAG|sp_node_var(n);       /* a stale value becomes the drum */\n', ''),
    'turret-any-caller': ('WORD_AT(g_esp)!=SP_TURRET_OPT_RET || !(e=sp_editor())', '!(e=sp_editor())'),
    'turret-any-item': ('sp_kind_of(WORD_AT(n))!=1 || !sp_alloc()) return;\n    WORD_AT(n+SP_NODE_VAR)', 'sp_kind_of(WORD_AT(n))<0 || !sp_alloc()) return;\n    WORD_AT(n+SP_NODE_VAR)'),
    'turret-no-end': ('    guest_call(sub_00078590,1,n+SP_NODE_VAR);\n    guest_call(sub_000783D0,0);\n}', '    guest_call(sub_00078590,1,n+SP_NODE_VAR);\n}'),
    'mount-whole-pak': ('guest_call(sub_000622D0,3,sp.paks[j],0u,1u)', 'guest_call(sub_000622D0,3,sp.paks[j],1u,1u)'),
    'mount-no-texture-pak': ('    for(j=0;j<2u*SP_KINDS;j++) {', '    for(j=0;j<SP_KINDS;j++) {'),
    'mount-cell-texture-pak': ('"pak/challengemem/l_64_chm.pak"', '"pak/challenge/l_64_chm.pak"'),
    'mount-gun-texture-pak': ('"pak/storymem/l_26_stm.pak"', '"pak/storymem/l_2_stm.pak"'),
    'mount-textures-without-model': ('        if(j>=SP_KINDS && !sp_mounted(k)) continue;     /* no model, no textures */\n', ''),
    'mount-wrong-pak': ('"pak/story/l_26_st.pak"', '"pak/story/l_2_st.pak"'),
    'mount-cell-pak': ('"pak/challenge/l_64_ch.pak"', '"pak/story/l_64_ch.pak"'),
    'mount-any-map': ('        if(!sp_lv.want[k] || !sp_pak(j) || sp_slot(j)!=-1) continue;', '        if(!sp_pak(j) || sp_slot(j)!=-1) continue;'),
    'mount-twice': ('        if(!sp_lv.want[k] || !sp_pak(j) || sp_slot(j)!=-1) continue;', '        if(!sp_lv.want[k] || !sp_pak(j)) continue;'),
    'mount-table-full': ('if((mounted=WORD_AT(SP_MOUNTS))+SP_MOUNT_KEEP>SP_MOUNT_MAX)', 'if((mounted=WORD_AT(SP_MOUNTS))>SP_MOUNT_MAX)'),
    'mount-skip-ignored': ('    if(WORD_AT(SP_MOUNT_SKIP)) { original();return; }\n    original();', '    original();'),
    'mount-editor': ('static int sp_play(void) { return sp_on() && WORD_AT(SP_SCENE)==100u; }', 'static int sp_play(void) { return sp_on() && WORD_AT(SP_SCENE)>=100u; }'),
    'mount-stale-level': ('    memset(&sp_lv,0,sizeof sp_lv);\n    if(!sp_play()) return;', '    if(!sp_play()) return;'),
    'preload-any-caller': ('    if(!MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=SP_PRELOAD_RET) { original();return; }', '    if(!MAPMAKER_STACK_OK(8)) { original();return; }'),
    'preload-stock-items': ('       SP_B(g_esi+SP_REC_VAR)!=1u || !sp_lv.want[k]) return;', '       !sp_lv.want[k]) return;'),
    'preload-no-row': ('    if(!sp_mounted((unsigned)k) || !sp_resource(sp_kind[k].model)) {', '    if(!sp_mounted((unsigned)k)) {'),
    'model-any-caller': ("((ret=WORD_AT(g_esp))!=SP_PROP_RET && ret!=SP_GUN_RET)) { original();return; }\n    original();\n    if(ret==SP_GUN_RET)",
                         "((ret=WORD_AT(g_esp)),0)) { original();return; }\n    original();\n    if(ret==SP_GUN_RET)"),
    'model-wrong-case': ('       (ret==SP_PROP_RET)!=(k==0) || SP_B(g_esi+SP_REC_VAR)!=1u) return;', '       SP_B(g_esi+SP_REC_VAR)!=1u) return;'),
    'model-unread': ('    return r && WORD_AT(r+0x10u)!=0;', '    return r!=0;'),
    'textures-every-time': ('    if(sp_lv.textured[k] || !sp_ready(k)) return;', '    if(!sp_ready(k)) return;'),
    'textures-unread': ('    if(sp_lv.textured[k] || !sp_ready(k)) return;', '    if(sp_lv.textured[k]) return;'),
    'textures-ignore-mark': ("    if(!(SP_B(r+2u)&0x10u)) {", "    if(0) {"),
    'textures-keep-mark': ('    SP_B(r+2u)&=(uint8_t)~0x10u;                       /* registered: no creation-time pass */\n', ''),
    'textures-creation-way': ('    guest_call(sub_00096220,1,path);', '    guest_call(sub_00096220,1,file);'),
    'textures-outside-play': ('    if(sp_play()) for(k=0;k<SP_KINDS;k++) if(sp_lv.want[k]) sp_register(k);', '    for(k=0;k<SP_KINDS;k++) if(sp_lv.want[k]) sp_register(k);'),
    'lift-none': ('#define SP_CELL_LIFT 0.737f', '#define SP_CELL_LIFT 0.0f'),
    'lift-guns': ('    else if(MAPMAKER_PTR_OK(g_ebp,0x60u)) SP_F(g_ebp+SP_REC_Y)+=SP_CELL_LIFT;', '    if(MAPMAKER_PTR_OK(g_ebp,0x60u)) SP_F(g_ebp+SP_REC_Y)+=SP_CELL_LIFT;'),
    'lift-wrong-field': ('#define SP_REC_Y 0x14u', '#define SP_REC_Y 0x10u'),
    'guntype-any-item': ('    if(sp_lv.gun_item && sp_lv.gun_item==g_esi && sp_play()) g_eax=SP_PRISON_GUN_TYPE;', '    if(sp_lv.gun_item && sp_play()) g_eax=SP_PRISON_GUN_TYPE;'),
    'guntype-sticks': ('    if(sp_lv.gun_item && sp_lv.gun_item==g_esi && sp_play()) g_eax=SP_PRISON_GUN_TYPE;\n    sp_lv.gun_item=0;', '    if(sp_lv.gun_item && sp_lv.gun_item==g_esi && sp_play()) g_eax=SP_PRISON_GUN_TYPE;'),
    'guntype-any-caller': ('    if(!MAPMAKER_STACK_OK(12) || WORD_AT(g_esp)!=SP_GUNTYPE_RET) { original();return; }', '    if(!MAPMAKER_STACK_OK(12)) { original();return; }'),
    'guntype-value': ('#define SP_PRISON_GUN_TYPE 7u', '#define SP_PRISON_GUN_TYPE 8u'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('storyprops.exe' if os.name == 'nt' else 'storyprops')
    source = ROOT / 'tests/mapmaker/mapmaker_storyprops.c'
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
def test_mapmaker_storyprops(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker storyprops ok' in result.stdout
        assert '[mapmaker] storyprops: mounted pak/challenge/l_64_ch.pak for the power cells' in result.stderr
        assert '[mapmaker] storyprops: mounted pak/storymem/l_26_stm.pak for the prison guns' in result.stderr
        assert '[mapmaker] storyprops: power cell 0x53D textures: 2 of 3 in pak/challengemem/l_64_chm.pak (missing 123456' in result.stderr
        assert '[mapmaker] storyprops: prison gun 0xE6 textures: 1 of 1 in pak/storymem/l_26_stm.pak' in result.stderr
        assert '[mapmaker] storyprops: drum placed as the mission 1 power cell' in result.stderr
        assert '[mapmaker] storyprops: fixed gun placed as the mission 1 prison gun' in result.stderr
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_no_guest_code_table_no_chunk():
    """Wrappers only; the variant lives in a stock-unused record byte, no fork chunk."""
    assert not (ROOT / 'config/guest-code-patches/storyprops.json').exists()
    src = INC.read_text(encoding='utf-8')
    assert '0x00C776' not in src.replace('0x00C77667u', '').replace('0x00C77668u', '').replace(
        '0x00C77669u', '').replace('0x00C7766Au', '')
    guest = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    assert '#include "pc_mapmaker_storyprops.inc"' in guest
    header = (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    for hook in ('compile', 'load', 'category', 'build', 'turret_options', 'mount', 'textures', 'preload_model', 'model', 'gun_type'):
        assert f'void pc_mapmaker_storyprops_{hook}(void (*original)(void));' in header
        assert f'void pc_mapmaker_storyprops_{hook}(void (*original)(void)) {{ original(); }}' in guest


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


def callers_of(target):
    return {i.address for i in decoded() if i.call and f'0x{target:x}' in i.text.split(';')[0]}


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


def test_record_byte_unused_for_drum_and_fixed_guns():
    """The compiler writes +0..+8 for 0x32..0x34 and zeroes +0..+0xA for cat. 26; +0xC
    is only the 0x35..0x37 branch. The editor loader and the play fill read +0xC
    only for 0x35..0x37."""
    a = by_address()
    # writer sub_00300800: 0x32..0x34 -> 0x003010F6 (+0, +4, +8); other categories -> 0x00300FFA (+0xA, +8, +4, +0)
    assert ('imm', 0x32) in a[0x00300F03].ops and '0x3010f6' in a[0x00300F06].text
    assert [disp(a[x]) for x in (0x003010F9, 0x003010FF, 0x00301107)] == [[0], [4], [8]]
    assert [disp(a[x]) for x in (0x00300FFC, 0x00301001, 0x00301006, 0x0030100A)] == [[0xA], [8], [4], [0]]
    assert 0xC in disp(a[0x00301147]) and ('imm', 0x35) in a[0x00300EE8].ops
    stores_c = [i for i in decoded() if i.function == 0x00300800 and i.mnemonic == 'mov' and mem(i)
                and i.ops[0][0] == 'mem' and mem(i)[0][4] == 0xC and mem(i)[0][2] == 'esi']
    assert {i.address for i in stores_c} <= {0x00301147, 0x00301291, 0x003019FD}
    # loader sub_00301A90: 0x32..0x34 read +0, +4, +8 (0x003022D3); +0xC only at 0x003023AC (0x35..0x37)
    assert ('imm', 0x32) in a[0x003022BA].ops and [disp(a[x]) for x in (0x003022D3, 0x003022D5, 0x003022DC)] == [[0], [4], [8]]
    reads_c = [i.address for i in decoded() if i.function == 0x00301A90 and mem(i) and mem(i)[0][1] == 'esi' and mem(i)[0][4] == 0xC
               and i.ops[0][0] == 'reg']
    assert reads_c == [0x003021C6, 0x003023AC]           # the piece header's colour (sub_003194E0), 0x35..0x37
    assert '0x3194e0' in a[0x003021DE].text and ('imm', 0x7F7F7F00) in a[0x003021CB].ops
    # play fill sub_00240C50: [esi+0xC] only in the 0x35..0x37 mode cases
    fill_c = [i.address for i in decoded() if i.function == 0x00240C50 and mem(i) and mem(i)[0][1] == 'esi' and mem(i)[0][4] == 0xC
              and i.ops[0][0] == 'reg']                  # reads (0x0024201E stores a Story AI record's +0xC)
    assert sorted(fill_c) == [0x00241BFB, 0x00241C0E]
    assert set(struct.unpack('<6I', xbe()(0x00242650, 24))) == {0x00241C25, 0x00241B53}


def test_fill_sites():
    """sub_00240C50: items 0x48..0x4C / 0x2C..0x31 -> prop case, 0x32..0x37 -> turret case;
    the model sub_002F7790(id) with esi = the item record; the cat. 29 gun type."""
    a = by_address()
    rd = xbe()
    table = rd(0x00242568, 0x50)
    case = lambda i: struct.unpack('<I', rd(0x00242544 + table[i] * 4, 4))[0]
    assert case(0x49) == 0x002416DE and case(0x32) == case(0x34) == 0x00241962
    assert ret_of(a, 0x00241782, 0x2f7790) == 0x00241787 and ('reg', 'ecx') in a[0x00241781].ops
    assert 0xE in disp(a[0x0024177D]) and mem(a[0x0024177D])[0][1] == 'esi'
    assert 0xC in disp(a[0x00241795])                                     # record +0xC = model
    assert ret_of(a, 0x002419BF, 0x2f7790) == 0x002419C4 and 0xE in disp(a[0x002419BA])
    assert 0xC in disp(a[0x002419C4])
    # cat. 29 (0x32..0x34 -> 0x00241A6E): flags |= 0x10000 on 4|0x40 base, gun type = sub_0033DD80(theme, id)
    assert struct.unpack('<3I', rd(0x00242638, 12)) == (0x00241A6E,) * 3
    assert ('imm', 0x10000) in a[0x00241A88].ops and ('imm', 0x35) in a[0x002419CC].ops
    assert ret_of(a, 0x00241AA1, 0x33dd80) == 0x00241AA6 and 0xE in disp(a[0x00241A95])
    assert 0x1C in disp(a[0x00241AA9]) and '0x242c00' in a[0x00241AB6].text
    # the record +0x88 comes from the gun template of that type (after the hook)
    assert 0x94 in disp(a[0x00241AC2])
    # props: records of stride 0x60 with +0 = logic id + 1 or -1, created by sub_0006C740 (obj+0x2D8 = record)
    assert ('imm', 0x60) in a[0x002417CF].ops and 0x2D8 in disp(a[0x0006C7B3])
    assert {a[c].function for c in callers_of(0x6c740)} >= {0x0006D7A0}


def test_preload_and_mount_order():
    """sub_001CDB80 mounts (sub_00062790) first; the MapMaker preload (sub_001CDC50 ->
    sub_0006C920 -> sub_0023F390) requests models through sub_0006C090 (esi = item);
    the load pass sub_00063390 reads them; the fill runs after (sub_0006EC10)."""
    a = by_address()
    assert '0x62790' in a[0x001CDB85].text
    assert [a[x].text.split(';')[0].split()[-1] for x in (0x0003D590, 0x0003D5A3, 0x0003D5AD, 0x0003D64A)] == \
        ['0x1cdb80', '0x1cdc50', '0x63390', '0x6ec10']
    assert ret_of(a, 0x0023F5D6, 0x6c090) == 0x0023F5DB and '0xa5e40' in a[0x0023F5DC].text
    assert 0xE in disp(a[0x0023F5C1]) and mem(a[0x0023F5C1])[0][1] == 'esi'
    assert '0x240c50' in a[0x0006D80C].text and '0x6d7a0' in a[0x0006ED4D].text
    # the power cell's physics preload resource 0xCFB is added for model 0x53D
    assert ('imm', 0xCFB) in a[0x00257268].ops


def test_options_sites():
    """Objects mode: sub_002F7770(id) -> category entry; +8 = options descriptor
    opened with sub_002FB230; category 26 has none; the fixed gun options builder."""
    a = by_address()
    rd = xbe()
    assert ret_of(a, 0x003051B0, 0x2f7770) == 0x003051B5 and 8 in disp(a[0x003051B5])
    assert ret_of(a, 0x003051D2, 0x2f7770) == 0x003051D7 and 8 in disp(a[0x003051D7]) and '0x2fb230' in a[0x003051DB].text
    assert 0x2F1C in disp(a[0x003051A3])
    cat = lambda c, o: struct.unpack('<I', rd(0x00524BE0 + c * 24 + o, 4))[0]
    assert cat(26, 8) == 0 and cat(29, 8) == 0x0052DE7C and cat(8, 8) == 0x0052DECC
    assert struct.unpack('<2I', rd(0x0052DE7C, 8)) == (0x003188C0, 0x003192E0)
    assert struct.unpack('<2I', rd(0x0052DECC, 8)) == (0x00319300, 0x003192E0)
    assert struct.unpack('<h', rd(0x00524F58 + 0x49 * 0xC4 + 2, 2))[0] == 26
    for i in (0x32, 0x33, 0x34):
        assert struct.unpack('<h', rd(0x00524F58 + i * 0xC4 + 2, 2))[0] == 29
    # sub_003188C0: the selected node at editor +0x2F1C; the weapon choice ends at 0x00318A87
    assert 0x2F1C in disp(a[0x003188C6]) and '0x78590' in a[0x00318A82].text
    assert ret_of(a, 0x00318A87, 0x783d0) == 0x00318A8C


def test_power_cell_and_prison_gun_data():
    """Init 20 case 12 for 0x53D (health 2.0, damage sub_00218650), explosion 0x2F
    (10, 12, 25) unless the record tag is CEL1..CEL6; prison gun type 0x200, init 0."""
    a = by_address()
    rd = xbe()
    f = lambda va: struct.unpack('<f', rd(va, 4))[0]
    assert struct.unpack('<5f', rd(0x004DB490 + 0x2F * 0x18 + 8, 12) + bytes(8))[:3] == (10.0, 12.0, 25.0)
    assert struct.unpack('<3f', rd(0x004DB490 + 0x0A * 0x18 + 8, 12)) == (10.0, 6.0, 25.0)
    assert ('imm', 0x2F) in a[0x002186FB].ops and '0xefbd0' in a[0x002186FD].text
    assert 0x2D8 in disp(a[0x002186AE]) and ('imm', 0x43454C31) in a[0x002186B9].ops
    assert ('imm', 0x00218650) in a[0x0021AC4D].ops and f(0x00475D10) == 2.0
    # init 20 switch on model - 0x511 (byte table 0x0021ACC8): 0x53D -> 0x0021ABC5
    idx = rd(0x0021ACC8 + 0x53D - 0x511, 1)[0]
    assert struct.unpack('<I', rd(0x0021AC90 + idx * 4, 4))[0] == 0x0021ABC5


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


def test_models_and_paks():
    """prop_all rows: 0x53D powercell (type 1, init 20), 0xE6 prisonfixedgun (type 0x200,
    init 0); the models are in l_64_ch / l_26_st, not in chr.pak or l_100.pak, and every
    file l_26_st shares with another pak is byte-identical there."""
    if not (GAME / 'arcade/l_100.pak').exists():
        pytest.skip('game paks not available locally')
    idx = _index(GAME / 'arcade/l_100.pak')
    with open(GAME / 'arcade/l_100.pak', 'rb') as f:
        pos, n, _ = idx[0xD9E2667A]
        f.seek(pos)
        table = f.read(n)
    start = struct.unpack_from('<I', table, 4)[0] + 8
    props = {}
    for o in range(start, len(table) - 0x27, 0x28):
        mid, typ, _, path, _, _, _, _, flags, init = struct.unpack_from('<iIiiifiiIi', table, o)
        props[mid] = (typ, table[path + 8:table.index(b'\0', path + 8)].decode('latin1'), init)
    assert props[0x53D] == (1, 'farfuture2/powercell', 20)
    assert props[0xE6] == (0x200, 'level29/props/prisonfixedgun', 0)
    assert props[0x8B1] == (0x80, 'mapmaker/labdrumexplosive', 38)
    model = 'assets/ts/models/{0}/xbox/output/{1}.xbr'
    crc = lambda m: zlib.crc32(model.format(props[m][1], props[m][1].split('/')[-1]).encode()) & 0xFFFFFFFF
    cell, gun = _index(GAME / 'challenge/l_64_ch.pak'), _index(GAME / 'story/l_26_st.pak')
    chr_ = _index(GAME / 'chr.pak')
    assert crc(0x53D) in cell and crc(0xE6) in gun
    for m in (0x53D, 0xE6):
        assert crc(m) not in chr_ and crc(m) not in idx
    ch = GAME / 'story/l_26_st.pak'
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
            common = set(gun) & set(oi)
            with open(other, 'rb') as g:
                for c in common:
                    assert body(f, gun[c]) == body(g, oi[c]), (other.name, hex(c))


def test_texture_queue_site():
    """The texture queue's load sub_00063900 runs once, after the load pass and before
    the fill; sub_00096220 (the load pass's registration) queues "textures/%06x.xbt"
    through sub_000637E0; scene 100 skips the creation-time pass (0x00096556); the
    fill's prop record pointer is ebp when sub_002F7790 is called (y at +0x14)."""
    a = by_address()
    assert callers_of(0x63900) == {0x0003D5E5}
    order = [0x0003D5AD, 0x0003D5E5, 0x0003D64A]
    assert [a[x].text.split(';')[0].split()[-1] for x in order] == ['0x63390', '0x63900', '0x6ec10']
    assert '0x96220' in a[0x0006355D].text and ('imm', 0x10) in a[0x00063548].ops    # deferred mark +2 |= 0x10
    assert '0x637e0' in a[0x000962D9].text and '0x631c0' in a[0x00096227].text
    assert 0x14 in disp(a[0x0024176B]) and mem(a[0x0024176B])[0][1] == 'ebp'
    assert 0xC in disp(a[0x00241795]) and mem(a[0x00241795])[0][1] == 'ebp'


def _model(path):
    pk = path.split('/')
    name = 'assets/ts/models/{0}/xbox/output/{1}.xbr'.format(path, pk[-1])
    return zlib.crc32(name.encode()) & 0xFFFFFFFF


def _body(pak, crc):
    e = _index(pak)[crc]
    with open(pak, 'rb') as f:
        f.seek(e[0])
        return f.read(e[2] or e[1])


def test_variant_textures_in_memory_paks():
    """Every texture the power cell / prison gun model lists is in its memory pak
    (and in no model pak or MapMaker pak), and every file those memory paks share
    with another pak is byte-identical there."""
    if not (GAME / 'story/l_26_st.pak').exists():
        pytest.skip('game paks not available locally')
    for model, mpak, tpak in (('farfuture2/powercell', 'challenge/l_64_ch.pak', 'challengemem/l_64_chm.pak'),
                              ('level29/props/prisonfixedgun', 'story/l_26_st.pak', 'storymem/l_26_stm.pak')):
        d = _body(GAME / mpak, _model(model))
        off, ids = struct.unpack_from('<I', d, 0)[0], []
        while struct.unpack_from('<I', d, off)[0] != 0xFFFFFFFF:
            ids.append(struct.unpack_from('<I', d, off + 4)[0] & 0xFFFFFF)
            off += 16
        tex = _index(GAME / tpak)
        crc = lambda i: zlib.crc32(('textures/%06x.xbt' % i).encode()) & 0xFFFFFFFF
        assert ids and all(crc(i) in tex for i in ids), model
        for other in ('arcade/l_100.pak', 'arcademem/l_100m.pak', mpak):
            if (GAME / other).exists():
                oi = _index(GAME / other)
                assert not any(crc(i) in oi for i in ids), (model, other)
    for tpak in ('challengemem/l_64_chm.pak', 'storymem/l_26_stm.pak'):
        ti = _index(GAME / tpak)
        with open(GAME / tpak, 'rb') as f:
            for other in sorted(GAME.rglob('*.pak')):
                if other.samefile(GAME / tpak):
                    continue
                try:
                    oi = _index(other)
                except (OSError, struct.error):
                    continue
                common = set(ti) & set(oi)
                if not common:
                    continue
                with open(other, 'rb') as g:
                    for c in common:
                        f.seek(ti[c][0]); g.seek(oi[c][0])
                        assert f.read(ti[c][2] or ti[c][1]) == g.read(oi[c][2] or oi[c][1]), (tpak, other.name, hex(c))


def test_power_cell_height():
    """0x53D's vertices span y 0..1.471 but its bounds block (after the texture list:
    count, centre x/y/z, half extents) is centred at y -0.001 with half height 0.736,
    so it is drawn centred on its origin (the intro's upright and lying cells share
    one height); the drum's vertices start at y 0 and it has no such block."""
    if not (GAME / 'challenge/l_64_ch.pak').exists():
        pytest.skip('game paks not available locally')
    d = _body(GAME / 'challenge/l_64_ch.pak', _model('farfuture2/powercell'))
    off = struct.unpack_from('<I', d, 0)[0]
    while struct.unpack_from('<I', d, off)[0] != 0xFFFFFFFF:
        off += 16
    count, cx, cy, cz, hx, hy, hz = struct.unpack_from('<I6f', d, off + 16)
    assert count == 1 and abs(cx) < 1e-3 and abs(cz) < 1e-3 and abs(hx - 0.546) < 0.01
    lift = hy - cy
    src = INC.read_text(encoding='utf-8')
    want = float(src.split('#define SP_CELL_LIFT ')[1].split('f')[0])
    assert abs(lift - want) < 0.002, lift
    # the vertex rows {x, y, z, packed} of the cell's body: y from 0 to 2 * half height
    ys = [struct.unpack_from('<f', d, 0x3804 + 16 * i + 4)[0] for i in range(352)]
    assert abs(min(ys)) < 1e-3 and abs(max(ys) - 2 * hy) < 0.01
