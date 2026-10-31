"""MapMaker custom weapon set: 20 slots (fork-only chunk, slot byte on pickups).

Compiles src/interface/pc_mapmaker_weaponset.inc against fake guest memory
(with mutations that must be caught) and, when the private XBE is available,
checks the stock facts the hooks rely on: every hooked call site and return
address, the 0x44 set with 15 ids and its 6-id map-info form, the pickup slot
path (catalogue +8 -> pickup +0x10 -> set[slot]), the Story AI weapon slot,
the unused item record byte +0xC and node +0x30, the weapon slot pane and
the pickup options menu, and that the fork chunk tag is unused by the game.
"""
import os
from pathlib import Path
import struct
import subprocess
import pytest

from native_compiler import native_compiler
from mapmaker_decoded import decoded, _private

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_weaponset.inc'

MUTATIONS = {
    'always-chunk': ('    if(!high && !slotted && !ws_extras_used()) return;\n', ''),
    'ignores-story-ai': ('WS_H(data+WS_SAI_WEAPON)>=(int16_t)WS_STOCK) *(int *)ctx=1;', 'WS_H(data+WS_SAI_WEAPON)>=(int16_t)WS_STOCK) (void)ctx;'),
    'ignores-items': ('        if(s) { WS_B(ws_recs[k].at+WS_REC_SLOT)=(uint8_t)s;slotted++; }', '        if(s) WS_B(ws_recs[k].at+WS_REC_SLOT)=(uint8_t)s;'),
    'used-stale': ('    WORD_AT(info+0x248u)=used;\n', ''),
    'terminator-offset': ('WORD_AT(nterm+4u)=nterm-c;', 'WORD_AT(nterm+4u)=0;'),
    'no-room-check': ('used+8u+WS_CHUNK+4u>WS_SCAN', 'used>WS_SCAN'),
    'no-room-keeps-bytes': ('        for(k=0;k<ws_nr;k++) WS_B(ws_recs[k].at+WS_REC_SLOT)=0;\n', ''),
    'eax-stale': ('        g_eax=used;\n', ''),
    'pair-first-only': ('if(!memcmp(ws_nodes[i].key,ws_recs[r].key,sizeof ws_recs[r].key) && !nth--)', 'if(!memcmp(ws_nodes[i].key,ws_recs[r].key,sizeof ws_recs[r].key))'),
    'pair-ignores-piece': ('            memcpy(k->key,pk,4);\n', '            memset(k->key,0,4);\n'),
    'pair-rounds': ('    return (uint8_t)(int32_t)f;', '    return (uint8_t)(int32_t)(f+0.5f);'),
    'untagged-slot': ("    if((v&0xFFFF0000u)!=WS_NODE_TAG || (v&0xFFFFu)<WS_STOCK+1u", "    if((v&0xFFFFu)<WS_STOCK+1u"),
    'any-item': ('#define WS_PICKUP_ID(id) ((id)>=0x11u && (id)<=0x1Cu)', '#define WS_PICKUP_ID(id) ((id)>=0x11u && (id)<=0x23u)'),
    'load-keeps-junk': ('    for(k=0;k<ws_nn;k++) WORD_AT(ws_nodes[k].at+WS_NODE_SLOT)=WS_NODE_TAG;\n', ''),
    'load-keeps-extras': ('    ws_extras(ws_chunk(c),ws.ed);\n    ws_collect_nodes();', '    ws_collect_nodes();'),
    'changed-ignores-extras': ('    if(memcmp(old,ws.ed,sizeof old)) g_eax=1u;\n', ''),
    'reset-skips': ('    memset(ws.ed,0,sizeof ws.ed);\n    ws.page=0;\n', '    ws.page=0;\n'),
    'held-keeps-slot': ('WORD_AT(e+WS_HELD+WS_NODE_SLOT)=WS_NODE_TAG;', '(void)e;'),
    'page-no-reset': ('    ws.page=0;ws.title_ok=0;\n    original();', '    ws.title_ok=0;\n    original();'),
    'no-title': ('        g_eax=WS_TITLE;\n', ''),
    'title-stale': ('            ws_title_suffix();\n        }\n    }\n    original();', '        }\n    }\n    original();'),
    'keeps-direction': ('            WORD_AT(pane+0xC4u)&=~0x000F000Fu;WORD_AT(pane+0xC8u)&=~0xFu;\n', ''),
    'no-cursor': ('            guest_call(sub_00074460,3,pane,to,0u);\n', ''),
    'page-past-end': ('ws.page<3 && (in&0x00020002u)', 'ws.page<4 && (in&0x00020002u)'),
    'screen-always-on': ('    if(!ws_custom() || !MAPMAKER_STACK_OK(8) || !ws_on()) { ws.page=0;original();return; }',
                         '    if(!ws_custom() || !MAPMAKER_STACK_OK(8)) { ws.page=0;original();return; }'),
    'last-page-overlap': ('static const uint8_t ws_page_first[4]={0u,6u,12u,14u};', 'static const uint8_t ws_page_first[4]={0u,6u,12u,18u};'),
    'get-any-caller': ('    if(!ws_paged(ret,WS_ROW_RET)) { original();return; }', '    if(!ws.page) { original();return; }'),
    'set-any-caller': ('    if(!ws_paged(ret,WS_PICK_RET) && !ws_paged(ret,WS_CLEAR_RET)) { original();return; }',
                       '    if(!ws.page) { original();return; }'),
    'set-no-clear': (' && !ws_paged(ret,WS_CLEAR_RET)) { original();return; }', ') { original();return; }'),
    'sai-any-caller': ('WORD_AT(g_esp)!=WS_SAI_RET || WORD_AT(WS_SCENE)!=102u', '0 || WORD_AT(WS_SCENE)!=102u'),
    'sai-ignores-switch': ('    if(!ws_on() && !(MAPMAKER_PTR_OK(value,4) && (int32_t)WORD_AT(value)>=(int32_t)WS_STOCK))', '    if(0 && !(MAPMAKER_PTR_OK(value,4) && (int32_t)WORD_AT(value)>=(int32_t)WS_STOCK))'),
    'text-off-by-one': ('"%u: %.40s",(unsigned)slot+1u,s);', '"%u: %.40s",(unsigned)slot,s);'),
    'options-any-item': ("    if(!MAPMAKER_PTR_OK(n,0x3Cu) || !WS_PICKUP_ID(id=WORD_AT(n))) return;", "    if(!MAPMAKER_PTR_OK(n,0x3Cu) || !(id=WORD_AT(n))) return;"),
    'options-stale-value': ('    WORD_AT(n+WS_NODE_SLOT)=WS_NODE_TAG|cur;                /* a stale value becomes the item\'s own slot */\n', ''),
    'options-no-default': ('    guest_call(sub_00071800,2,guest_call(sub_00303350,1,id),WS_NODE_TAG);\n', ''),
    'options-ignores-switch': ('    if(!ws_on() && !cur) return;\n', ''),
    'options-no-end': ('    guest_call(sub_00078590,1,n+WS_NODE_SLOT);\n    guest_call(sub_000783D0,0);\n', '    guest_call(sub_00078590,1,n+WS_NODE_SLOT);\n'),
    'play-no-filter': ('ws.filtered[k]=id>0 && !guest_call(sub_00064E70,2,(uint32_t)mode,(uint32_t)id)?(uint32_t)id:0u;', 'ws.filtered[k]=id>0?(uint32_t)id:0u;'),
    'play-drops-appended': ('    for(k=WS_STOCK;k<n && m<WS_BIGMAX;k++) WORD_AT(WS_BIGSET+8u+4u*m++)=WORD_AT(WS_PLAYSET+8u+4u*k);   /* appended (0x002BE827) */\n', ''),
    'play-any-set': ('    if(g_eax!=WS_PLAYSET || !ws_play() || !ws_active()) return;', '    if(!ws_play() || !ws_active()) return;'),
    'random-all-slots': ('    if(ret==WS_RANDOM_RET) {', '    if(0) {'),
    'active-any-index': ('    return idx==0xF;\n', '    return idx>=0;\n'),
    'active-ignores-setup': ("    if(WS_B(WS_TYPEBYTE)!=10u && (t==9 || t==0xB || t==0xC || t==0xD)) {", "    if(0 && t) {"),
    'cache-sticks': ('    if(WORD_AT(WS_SCENE)!=100u) { ws.valid=0;return 0; }', '    if(WORD_AT(WS_SCENE)!=100u) return 0;'),
    'slot-filtered': ('    g_eax=(uint32_t)(int32_t)ws.ids[s-(int32_t)WS_STOCK];', '    g_eax=ws.filtered[s-(int32_t)WS_STOCK];'),
    'slot-any': ('    if(s<(int32_t)WS_STOCK || s>=(int32_t)WS_SLOTS || !ws_play()', '    if(s<0 || s>=(int32_t)WS_SLOTS || !ws_play()'),
    'veto-own-slot': ('    if(!s) { original();return; }\n    w=ws_record_weapon(s);\n    g_eax=w && w<0x46u', '    if(1) { original();return; }\n    w=ws_record_weapon(s);\n    g_eax=w && w<0x46u'),
    'veto-model-ignored': ('(int32_t)WORD_AT(0x004FB194u+w*0x54u)>-1;', '1;'),
    'ammo-veto-type': ('    t=MAPMAKER_PTR_OK(a,4)?WORD_AT(a):0u;', '    t=1u;(void)a;'),
    'veto-wrong-category': ("    if(id<first || id>=first+WS_STOCK ||", "    if(id<0x11u || id>0x1Cu ||"),
    'pickup-one-based': ('    if(s) WS_H(p+0x10u)=(int16_t)(s-1u);', '    if(s) WS_H(p+0x10u)=(int16_t)s;'),
    'preload-type': ('id<0x17u?2u:0x40u,w);', '2u,w);'),
    'preload-stale-cache': ('    unsigned n=0;\n    ws.valid=0;\n', '    unsigned n=0;\n'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('weaponset.exe' if os.name == 'nt' else 'weaponset')
    source = ROOT / 'tests/mapmaker/mapmaker_weaponset.c'
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
def test_mapmaker_weaponset(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker weaponset ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_no_guest_code_table_and_over_stock():
    """Wrappers only; the container's over-stock check knows the fork tag."""
    assert not (ROOT / 'config/guest-code-patches/weaponset.json').exists()
    container = (ROOT / 'src/interface/pc_mapmaker_container.inc').read_text(encoding='utf-8')
    assert 'tag==0x00C776F6u' in container
    assert '#define WS_TAG 0x00C776F6u' in INC.read_text(encoding='utf-8')


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


def test_hook_return_addresses():
    a = by_address()
    assert ret_of(a, 0x002B50D0, 0x6bd30) == 0x002B50D5 and a[0x002B50D0].function == 0x002B4C70
    assert ret_of(a, 0x002D3D87, 0x2d3b00) == 0x002D3D8C and a[0x002D3D87].function == 0x002D3D30
    assert ret_of(a, 0x002BF955, 0x312ae0) == 0x002BF95A and a[0x002BF955].function == 0x002BF910
    assert ret_of(a, 0x002D41C2, 0x312ae0) == 0x002D41C7 and a[0x002D41C2].function == 0x002D40A0
    assert callers_of(0x312ae0) == {0x002BF955, 0x002D41C2}
    assert ret_of(a, 0x002D424F, 0x3d7b0) == 0x002D4254 and ('imm', 0xE86) in a[0x002D424A].ops
    assert ret_of(a, 0x0031CDB2, 0x78590) == 0x0031CDB7 and a[0x0031CDAE].mnemonic == 'lea' and 0x2C in disp(a[0x0031CDAE])
    assert ret_of(a, 0x0031939D, 0x783d0) == 0x003193A2 and a[0x0031939D].function == 0x00319300
    assert ret_of(a, 0x0032C6FA, 0x3006c0) == 0x0032C6FF
    # the only callers of the routines the play hooks act on
    assert {a[c].function for c in callers_of(0x33e3b0)} == {0x00240C50}
    assert callers_of(0x23f390) == {0x0006C95E}
    assert {a[c].function for c in callers_of(0x312880)} == {0x00313310}
    assert {a[c].function for c in callers_of(0x2d3b00)} == {0x0006BD30, 0x002D3D30, 0x002D40A0}


def test_play_set_is_rebuilt_per_call():
    """sub_0006BD30 (scene 100) copies a 0x44 set into 0x006F1E28; only it touches the buffer."""
    a = by_address()
    assert ('imm', 0x006F1E28) in a[0x0006BDD9].ops and '0x6a760' in a[0x0006BDDE].text
    assert ('imm', 0x11) in a[0x0006A76F].ops and ('imm', 0x11) in a[0x0006A795].ops      # rep movsd 0x44
    assert ('imm', 0x006F1E28) in a[0x0006BE95].ops
    users = {i.function for i in decoded() for op in i.ops
             if (op[0] == 'imm' and 0x006F1E28 <= op[1] < 0x006F1E6C) or
                (op[0] == 'mem' and not op[1] and 0x006F1E28 <= op[4] < 0x006F1E6C)}
    assert users == {0x0006BD30}
    # the stock filter call per slot (sub_00064E70(mode byte [0x007DE456], id) != 0 -> 0)
    assert '0x64e70' in a[0x0006BE40].text and 0x007DE456 in disp(a[0x0006BE37])
    # the map's custom set: data +0x40 index >= 10, count byte +0x42, words +0x44 (sub_002BE780)
    assert 0x40 in disp(a[0x002BE7DA]) and ('imm', 0xA) in a[0x002BE7E1].ops and 0x42 in disp(a[0x002BE7F2])
    assert a[0x002BE7A0].mnemonic == 'movsx' and 2 in disp(a[0x002BE796])
    # the story-type append of weapon 0x38 after the map's ids
    assert ('imm', 0x38) in a[0x002BE827].ops
    # the compile stores count 6 and 6 ids from editor +0x1190C; data +0x44 + 15*2 reaches the name at +0x62
    assert ('imm', 6) in a[0x003008D6].ops and ('imm', 0x1190C) in a[0x003008DC].ops
    assert 0x72 in disp(a[0x00300911])


def test_slot_users():
    a = by_address()
    # sub_0006BF10(type, slot): slot < count, id = set +8 + 4*slot, 0 -> -1
    assert 4 in disp(a[0x0006BF38]) and 8 in disp(a[0x0006BF5E])
    # the pickup slot word +0x10 is catalogue +8 (0x00241187 -> [esp+0x14]) stored at 0x002414C1
    assert 0x00524F60 in disp(a[0x00241187]) and 0x10 in disp(a[0x002414C1])
    assert '0x33e3b0' in a[0x002414CC].text
    # level pickups: sub_0006C620 inlines the lookup on record +0x10 (0x58 stride)
    assert 0x10 in disp(a[0x0006C65D]) and ('imm', 0x58) in a[0x0006C6EF].ops
    # runtime spawn: sub_0006BF10([rec+4], [rec+0x10])
    assert '0x6bf10' in a[0x0006DA41].text and 0x10 in disp(a[0x0006DA3B])
    # the Story AI weapon is a SLOT: chunk word +0x26 (editor entry +0x2C), checked by sub_0033E6C0
    assert 0x2C in disp(a[0x0030144D]) and 0x26 in disp(a[0x00301451])
    assert 0x26 in disp(a[0x0024205A]) and '0x33e6c0' in a[0x0024205F].text
    # ... and read unbounded as set[slot] at spawn
    assert 0x18 in disp(a[0x0015B882]) or 0x18 in disp(a[0x0015B878])
    assert 8 in disp(a[0x0015B885]) and 8 in disp(a[0x002B9D51])
    # the AI random pick set[rand % count]
    assert a[0x002B50E8].mnemonic == 'div' and 8 in disp(a[0x002B50EB])
    # vetoes: weapon id - 0x11, ammo id - 0x17
    assert ('imm', 0x11) in a[0x0033E788].ops and ('imm', 0x17) in a[0x0033E7B9].ops
    assert 0x004FB194 in disp(a[0x0033E79E]) and 0x004FA138 in disp(a[0x0033E7D4])
    # sub_0033E6C0 / sub_0033E680: map data +0x44 for index 15 (esi == 5 after -10)
    assert ('imm', 5) in a[0x0033E696].ops and ('imm', 0x42) in a[0x0033E693].ops
    # preload of a pickup: sub_000BE860(type, id) -> sub_0006B2D0(desc) via sub_0006BF70
    assert '0x6bf70' in a[0x0023F5AA].text and '0x6b2d0' in a[0x0023F5B7].text


def test_record_byte_and_node_field_unused():
    a = by_address()
    # the compiler's writer sub_00300800: categories 8/9 write +0 / +4 / +0xA only (0x00301012..0x0030102D); +0xC is the turret branch
    assert [disp(a[x]) for x in (0x00301015, 0x0030101C, 0x00301028)] == [[0], [4], [0xA]]
    assert 0xC in disp(a[0x00301147]) and a[0x00301147].function == 0x00300800
    stores_c = [i for i in decoded() if i.function == 0x00300800 and i.mnemonic == 'mov' and mem(i)
                and mem(i)[0][4] == 0xC and mem(i)[0][2] == 'esi' and i.ops[0][0] == 'mem']
    assert {i.address for i in stores_c} <= {0x00301147, 0x00301291, 0x003019FD}
    # the held node +0x30 (editor +0x2F64) is never addressed
    assert not [i for i in decoded() if 0x2F64 in disp(i)]
    # the loader's node template is esp+0x3C; its +0x30 (esp+0x6C) is never written in sub_00301A90
    tmpl = [i for i in decoded() if i.function == 0x00301A90 and i.mnemonic.startswith('mov') and mem(i)
            and i.ops[0][0] == 'mem' and mem(i)[0][1] == 'esp' and mem(i)[0][4] == 0x6C]
    assert not tmpl
    # nodes are copied whole (rep movsd 0xF = 0x3C)
    assert ('imm', 0xF) in a[0x00308170].ops and ('imm', 0xF) in a[0x00308222].ops


def test_screens():
    a = by_address()
    data = (_private() / 'game/default.xbe').read_bytes()
    base = struct.unpack_from('<I', data, 0x104)[0]
    nsec, shdr = struct.unpack_from('<II', data, 0x11C)

    def u32(va):
        for k in range(nsec):
            _, v, vs, raw, rs = struct.unpack_from('<5I', data, shdr - base + k * 0x38)
            if v <= va < v + vs:
                return struct.unpack_from('<I', data, raw + va - v)[0]
        raise KeyError(hex(va))
    # weapon slot pane 0x005236EC = {builder sub_002D4220, update sub_002D40A0}; 6 rows of values 0..5
    assert (u32(0x005236EC), u32(0x005236F0)) == (0x002D4220, 0x002D40A0)
    assert ('imm', 6) in a[0x002D42AD].ops and '0x79d60' in a[0x002D42A4].text
    assert ('imm', 0x002D3D30) in a[0x002D4290].ops
    # the row draw asks sub_002D3B00 for the item's value (+0x14)
    assert 0x14 in disp(a[0x002D3D38])
    # the pick stores slot = highlighted index - 2 and moves on while the value is below 5
    assert 0x184 in disp(a[0x002BF937]) and ('imm', 5) in a[0x002BF990].ops and '0x74460' in a[0x002BF9A0].text
    assert 0x18C in disp(a[0x002D40B9]) and ('imm', 0x800000) in a[0x002D4191].ops
    # pickup options: categories 8, 9, 19, 20, 21 share descriptor 0x0052DECC = {sub_00319300, sub_003192E0}
    for cat in (8, 9, 19, 20, 21):
        assert u32(0x00524BE0 + cat * 24 + 8) == 0x0052DECC
    assert (u32(0x0052DECC), u32(0x0052DED0)) == (0x00319300, 0x003192E0)
    assert 0x2F1C in disp(a[0x00319306])
    # the Story AI weapon choice: options -1 then slots 0..5 named by item 0x11 + k
    assert ('imm', 0x11) in a[0x0031CD92].ops or 0x11 in disp(a[0x0031CD92])
    assert ('imm', 6) in a[0x0031CDA9].ops and ('imm', 0xD76) in a[0x0031CD4F].ops and ('imm', 0xB9) in a[0x0031CD6B].ops
    # weapon names: text id dword at 0x004FB190 + id*0x54 (sub_00303350)
    assert 0x004FB190 in disp(a[0x00303380])
    # held defaults for categories 8/9 set only +0x18/+0x1C (0x00316F11)
    assert 0x2F4C in disp(a[0x00316F11]) and 0x2F50 in disp(a[0x00316F21])


def test_fork_tag_unused():
    hits = [i for i in decoded() if any(v in (0x00C776F6,) for _, v in i.values())]
    assert not hits
