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
    'reset-skips': ('    original();\n    memset(ws.ed,0,sizeof ws.ed);\n}', '    original();\n}'),
    # the 20-row scrolling list
    'rows-6': ('    for(v=0;v<WS_SLOTS;v++) guest_call(sub_00079D60', '    for(v=0;v<WS_STOCK;v++) guest_call(sub_00079D60'),
    'no-scroll-region': ('    guest_call(sub_000780E0,2,ws_float(WS_ROW_HEIGHT*(float)WS_VIEW_ROWS),0u);\n', ''),
    'no-region-end': ('    guest_call(sub_000781A0,0);\n', ''),
    'region-height': ('ws_float(WS_ROW_HEIGHT*(float)WS_VIEW_ROWS)', 'ws_float(WS_ROW_HEIGHT*(float)WS_SLOTS)'),
    'row-height': ('#define WS_ROW_HEIGHT 4.6f', '#define WS_ROW_HEIGHT 4.5f'),
    'row-callbacks-swapped': ('ws_float(WS_ROW_HEIGHT),WS_ROW_ACTION,WS_ROW_DRAW);', 'ws_float(WS_ROW_HEIGHT),WS_ROW_DRAW,WS_ROW_ACTION);'),
    'build-ignores-switch': ('    return ws_on() && (WORD_AT(WS_SCENE)==102u || ws_fe_scene());', '    return WORD_AT(WS_SCENE)==102u || ws_fe_scene();'),
    'build-frontend-off': ('    return ws_on() && (WORD_AT(WS_SCENE)==102u || ws_fe_scene());', '    return ws_on() && WORD_AT(WS_SCENE)==102u;'),
    'build-no-ret': ('        ret,menu,WORD_AT(menu+4u),scene,set,ws_pointer_mode());\n    MAPMAKER_GUEST_RET();', '        ret,menu,WORD_AT(menu+4u),scene,set,ws_pointer_mode());'),
    'build-no-focus': ('    guest_call(sub_00077110,2,menu,0xFFFFFFFBu);\n', ''),
    'build-no-finish': ('    guest_call(sub_00075E70,0);\n', ''),
    'build-no-choices': ('    guest_call(sub_002BF830,1,menu);\n', ''),
    'build-hint-args': ('guest_call(sub_00074A00,4,1u,guest_call(sub_0003D7B0,1,0x1A27u),0u,0u);', 'guest_call(sub_00074A00,4,1u,guest_call(sub_0003D7B0,1,0x1A27u),1u,0u);'),
    'build-scene-size': ('ws_float(scene==102?7.5f:5.5f)', 'ws_float(7.5f)'),
    'build-rows-stale': ('    ws.rows=WS_SLOTS;\n', ''),
    'stock-rows-stale': ('        ws.rows=WS_STOCK;\n', ''),
    'fe-stale-map': ('    if(scene!=102) ws_extras(ws_chunk(WORD_AT(WS_CONTAINER)),ws.fe);', '    (void)0;'),
    'get-any-caller': ('    if((ret!=WS_ROW_RET && ret!=WS_PREVIEW_RET) || (s=WORD_AT(g_esp+4u))<WS_STOCK', '    if(((s=WORD_AT(g_esp+4u)),0) || s<WS_STOCK'),
    'get-no-preview': (' && ret!=WS_PREVIEW_RET) || (s=WORD_AT(g_esp+4u))<WS_STOCK', ') || (s=WORD_AT(g_esp+4u))<WS_STOCK'),
    'get-past-end': (' || s>=WS_SLOTS || !ws_on()) { original();return; }\n    if(ws_custom())', ' || s>WS_SLOTS || !ws_on()) { original();return; }\n    if(ws_custom())'),
    'get-preset': ('    if(ws_custom()) g_eax=(uint32_t)(int32_t)ws.ed[s-WS_STOCK];', '    if(WORD_AT(WS_SCENE)==102u) g_eax=(uint32_t)(int32_t)ws.ed[s-WS_STOCK];'),
    'get-ignores-switch': (' || s>=WS_SLOTS || !ws_on()) { original();return; }\n    if(ws_custom())', ' || s>=WS_SLOTS) { original();return; }\n    if(ws_custom())'),
    'frontend-any-set': ('    else if(ws_fe_scene() && ws_fe_map_set()) g_eax', '    else if(ws_fe_scene()) g_eax'),
    'frontend-editor-ids': ('g_eax=(uint32_t)(int32_t)ws.fe[s-WS_STOCK];', 'g_eax=(uint32_t)(int32_t)ws.ed[s-WS_STOCK];'),
    'set-any-caller': ('    if((ret!=WS_PICK_RET && ret!=WS_CLEAR_RET) || !ws_on()', '    if(!ws_on()'),
    'set-no-clear': ('    if((ret!=WS_PICK_RET && ret!=WS_CLEAR_RET) || !ws_on()', '    if(ret!=WS_PICK_RET || !ws_on()'),
    'set-no-advance': ('        guest_call(sub_00074460,3,pane,s+1u,0u);\n', ''),
    'set-advance-from-stock': ('s>=WS_STOCK-1u && s+1u<WS_SLOTS', 's+1u<WS_SLOTS'),
    'set-advance-past-end': ('s>=WS_STOCK-1u && s+1u<WS_SLOTS', 's>=WS_STOCK-1u && s<WS_SLOTS'),
    'set-advance-on-clear': ('    if(ret==WS_PICK_RET && s>=WS_STOCK-1u', '    if(s>=WS_STOCK-1u'),
    'set-advance-value-stale': ('        WORD_AT(pane+0x18Cu)=s+1u;\n', ''),
    'x-not-cleared': ('    if(WORD_AT(WS_SCENE)!=102u && slot>=(int)WS_STOCK) WORD_AT(pane+0xC4u)&=~0x00800000u;\n', ''),
    'x-editor': ('    if(WORD_AT(WS_SCENE)!=102u && slot>=(int)WS_STOCK) WORD_AT(pane+0xC4u)', '    if(slot>=(int)WS_STOCK) WORD_AT(pane+0xC4u)'),
    'x-stock-rows': ('    if(WORD_AT(WS_SCENE)!=102u && slot>=(int)WS_STOCK) WORD_AT(pane+0xC4u)', '    if(WORD_AT(WS_SCENE)!=102u) WORD_AT(pane+0xC4u)'),
    'row-any': ('(v=WORD_AT(item+0x14u))>=WS_STOCK && v<WS_SLOTS) {', '(v=WORD_AT(item+0x14u))>=1u && v<WS_SLOTS) {'),
    'row-editor': ('    if(WORD_AT(WS_SCENE)!=102u && MAPMAKER_PTR_OK(pane,0x200u) && (item=', '    if(MAPMAKER_PTR_OK(pane,0x200u) && (item='),
    'row-no-ret': ('        MAPMAKER_GUEST_RET();\n        return;\n    }\n    original();', '        return;\n    }\n    original();'),
    'label-number': ('"%u",(unsigned)v+1u);', '"%u",(unsigned)v);'),
    'label-any-caller': ('    if(ret!=WS_LABEL_RET) return;\n', ''),
    'label-colour': ('    WORD_AT(entry-0x24u)=WORD_AT(entry-0x18u+4u*(v%WS_STOCK));\n', ''),
    'label-colour-index': ('WORD_AT(entry-0x18u+4u*(v%WS_STOCK))', 'WORD_AT(entry-0x18u+4u*(v%WS_SLOTS))'),
    'label-stock-rows': ('(v=WORD_AT(item+0x14u))<WS_STOCK || v>=WS_SLOTS) return;', '(v=WORD_AT(item+0x14u))>=WS_SLOTS) return;'),
    'label-frame': ('entry=g_esp+0x44u', 'entry=g_esp+0x40u'),
    # the weapon name after a two-digit label (slots 10-20)
    'name-not-moved': ('            WS_F(g_esp+0x2Cu)+=shift;\n', ''),
    'name-fit-wide': ('            WS_F(g_esp+8u)-=shift;\n', ''),
    'name-any-caller': ('WORD_AT(g_esp)==WS_NAME_RET && ws.rows==WS_SLOTS', 'ws.rows==WS_SLOTS'),
    'name-stock-rows': ('WORD_AT(g_esp)==WS_NAME_RET && ws.rows==WS_SLOTS', 'WORD_AT(g_esp)==WS_NAME_RET'),
    'name-from-slot-11': ('#define WS_TWO_DIGITS 9u', '#define WS_TWO_DIGITS 10u'),
    'name-past-end': ('(v=WORD_AT(item+0x14u))>=WS_TWO_DIGITS && v<WS_SLOTS &&', '(v=WORD_AT(item+0x14u))>=WS_TWO_DIGITS &&'),
    'name-one-digit': ('!WS_B(label) || !WS_B(label+1u)) return 0.f;', '!WS_B(label)) return 0.f;'),
    'name-no-fallback': ('return w>0.f && w<=WS_NAME_SHIFT_MAX?w:WS_NAME_SHIFT;', 'return w;'),
    'name-frame': ('item=WORD_AT(g_esp+0x50u);', 'item=WORD_AT(g_esp+0x4Cu);'),
    'name-whole-label': ('WS_B(WS_MEASURE+8u)=WS_B(label);WS_B(WS_MEASURE+9u)=0;',
                         'WS_B(WS_MEASURE+8u)=WS_B(label);WS_B(WS_MEASURE+9u)=WS_B(label+1u);WS_B(WS_MEASURE+10u)=0;'),
    'diag-silent': ('    ws_diag_n++;\n    if(moves)', '    if(moves)'),
    'diag-open-starved': ('    if(moves ? ws_diag_moves>=48u : ws_diag_opens>=256u) return;', '    if(ws_diag_moves>=48u) return;'),
    'diag-no-open': ('        ws_diag(0,"slot pane open:', '        if(0) ws_diag(0,"slot pane open:'),
    'diag-every-row': ('        if(slot>=(int)WS_STOCK || scroll!=ws.last_scroll)\n', '        if(1)\n'),
    'diag-no-moves': ('    ws.last_slot=slot;ws.last_scroll=scroll;\n', ''),
    'held-keeps-slot': ('WORD_AT(e+WS_HELD+WS_NODE_SLOT)=WS_NODE_TAG;', '(void)e;'),
    'frontend-any-mode': ('    return WORD_AT(WS_SCENE)==101u && WORD_AT(WS_GAMETYPE)==9u &&', '    return WORD_AT(WS_SCENE)==101u &&'),
    'frontend-link': (' && !WORD_AT(WS_NET);', ';'),
    'frontend-story': (' && WS_B(WS_TYPEBYTE)!=10u && !WORD_AT(WS_NET)', ' && !WORD_AT(WS_NET)'),
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
        # the diagnostics a user run shows (stderr)
        assert '[mapmaker] weaponset: slot pane build (stock 6 rows: TSFP_MAPMAKER_LIMITS is not 1) ret=0x00078F74 menu=0x00301000 descriptor=0x005236EC' in result.stderr
        assert '[mapmaker] weaponset: slot pane build (20 rows, scrolling) ret=0x00078F74 menu=0x00301000 descriptor=0x005236EC scene=102 set=15' in result.stderr
        assert '[mapmaker] weaponset: slot pane open: pane=0x00301000' in result.stderr
        assert '[mapmaker] weaponset: list: slot 8 highlighted, scroll 0.0' in result.stderr
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
    # the highlighted row's weapon model asks the getter with the row value
    assert ret_of(a, 0x002D410A, 0x2d3b00) == 0x002D410F and ret_of(a, 0x002D40C6, 0x2d3b00) == 0x002D40CB
    # the row draw's filter call after the label (pc_mapmaker_weaponset_label)
    assert ret_of(a, 0x002D3DF1, 0x64e70) == 0x002D3DF6 and a[0x002D3DF1].function == 0x002D3D30
    # the pane system calls a builder from the descriptor (+0 copied to menu +8) at 0x00078F72
    assert a[0x00078F72].mnemonic == 'call' and 8 in disp(a[0x00078F67])


def test_row_name_follows_label():
    """The slot row draws the label at x + 5 and the weapon name at x + 20 (frame +0x18)."""
    a = by_address()
    # x = (int)(rect.x + 5.0f) at frame +0x14 (0x002D3D53 with the getter's argument pushed)
    assert a[0x002D3D45].mnemonic == 'addss' and 0x004760CC in disp(a[0x002D3D45])
    assert 0x18 in disp(a[0x002D3D53])
    # the label is drawn from frame +0x40 at x + 5 (0x002D3F1E: esp = frame - 0x40, +0x54 = frame +0x14)
    assert 0x54 in disp(a[0x002D3F1E]) and ('imm', 5) in a[0x002D3F40].ops and '0x41e30' in a[0x002D3F4C].text
    assert a[0x002D3F27].mnemonic == 'lea' and 0x48 in disp(a[0x002D3F27])
    # the name: x + 0x14 stored at frame +0x18 (esp = frame - 0x10), width rect +8 - x pushed for the fit
    assert 0x14 in disp(a[0x002D3F5C]) and ('imm', 0x14) in a[0x002D3F6A].ops
    assert a[0x002D3F8A].mnemonic == 'movss' and 0x28 in disp(a[0x002D3F8A])
    assert a[0x002D3F84].mnemonic == 'movss' and 0 in disp(a[0x002D3F84])
    # sub_0003D7B0(name id) at 0x002D3F90 between the store and the fit sub_00040A40(text, width, 0, 1)
    assert ret_of(a, 0x002D3F90, 0x3d7b0) == 0x002D3F95 and a[0x002D3F90].function == 0x002D3D30
    assert '0x40a40' in a[0x002D3F99].text and ('imm', 1) in a[0x002D3F68].ops and ('imm', 0) in a[0x002D3F6D].ops
    # the name is drawn at frame +0x18 (0x002D3FD1: esp = frame - 0xC)
    assert a[0x002D3FD1].mnemonic == 'mov' and 0x24 in disp(a[0x002D3FD1]) and '0x41e30' in a[0x002D3FDC].text
    # the fit routine measures with sub_0003FCF0(&w, &h, text, 0) and compares w with the width
    assert '0x3fcf0' in a[0x00040A54].text and ('imm', 0) in a[0x00040A47].ops


def test_scrolling_list_facts():
    """The 20-row list replays the stock builder and uses the weapon list's scroll region."""
    a = by_address()
    pushed = lambda x: [op[1] for op in a[x].ops if op[0] == 'imm']
    # stock builder sub_002D4220: frame, title 0xE86 (-1, 0x30404), set list 0x004766D8 (-5, 0x08030C00),
    # sub_002BF830(menu), sub_00079710(0), six rows (0, row, 0, 4.6f, sub_002D4040, sub_002D3D30), sizes, footer
    assert pushed(0x002D4222) == [0x002C1450] and pushed(0x002D4229) == [0x42200000] and pushed(0x002D4230) == [0x20010]
    assert '0x75b70' in a[0x002D4235].text
    assert pushed(0x002D4243) == [0x30404] and pushed(0x002D424A) == [0xE86]
    assert pushed(0x002D4263) == [0x08030C00] and pushed(0x002D426A) == [0x004766D8]
    assert '0x2bf830' in a[0x002D4279].text and '0x79710' in a[0x002D4280].text
    assert pushed(0x002D4290) == [0x002D3D30] and pushed(0x002D4295) == [0x002D4040] and pushed(0x002D429A) == [0x40933333]
    assert ('imm', 6) in a[0x002D42AD].ops
    assert pushed(0x002D42B7) == [0x40A00000] and '0x772e0' in a[0x002D42BD].text
    assert pushed(0x002D42D5) == [0x40F00000] and pushed(0x002D42DD + 6) == [0x40B00000] and '0x77320' in a[0x002D42E9].text
    assert [pushed(x) for x in (0x002D42F5, 0x002D4311, 0x002D432D)] == [[0xE88], [0xE89], [0x1A27]]
    assert '0x77110' in a[0x002D4345].text and a[0x002D434F].mnemonic == 'jmp' and '0x75e70' in a[0x002D434F].text
    # the weapon list beside it scrolls: sub_000780E0(67.5f, 0) .. rows .. sub_000781A0()
    assert '0x780e0' in a[0x002BFAF1].text and pushed(0x002BFAEC) == [0x42870000] and '0x781a0' in a[0x002BFB64].text
    # sub_000780E0 sets the builder's region mode 2 and height; items made in it get flag 0x40
    assert ('imm', 2) in a[0x00078115].ops and 0x38 in disp(a[0x00078115]) and 0x3C in disp(a[0x00078126])
    assert 0x38 in disp(a[0x00079145]) and ('imm', 0x40) in a[0x0007914A].ops
    # sub_000781A0 ends it: view +0x6C..+0x70 = its height (mode 2)
    assert 0x3C in disp(a[0x000781D0]) and 0x70 in disp(a[0x000782A1]) and 0x6C in disp(a[0x0007814B])
    # item flag 0x40 puts an item's height into the menu's scrolling content +0x5C
    assert ('imm', 0x40) in a[0x00078432 - 4].ops and 0x5C in disp(a[0x00078443])


def test_why_paging_was_invisible():
    """Editor menus are pointer menus: their tick skips the stock Up/Down; the mouse comes later."""
    a = by_address()
    # the editor turns pointer menus on (sub_00071010: [0x007BA270 + player*0x1E0] = 1)
    assert '0x71010' in a[0x00302FE9].text and 0x007BA270 in disp(a[0x0007101B])
    # the tick: pointer menus go to sub_000777E0 and jump past the Up/Down block at 0x0007AE2E
    assert 0x007BA270 in disp(a[0x0007ADEC]) and '0x777e0' in a[0x0007AE09].text
    assert a[0x0007AE0E].mnemonic == 'jmp' and '0x7aff6' in a[0x0007AE0E].text
    # the pane update runs at 0x0007AD76, the mouse hook's sub_00073750 later at 0x0007ADC5
    assert a[0x0007AD76].mnemonic == 'call' and '0x73750' in a[0x0007ADC5].text
    # the row draw: colour table [esp + value*4 + 0x20] (6 entries), label '1' + value
    assert 0x20 in disp(a[0x002D3D8C]) and ('imm', 0x31) in a[0x002D3DE0].ops and 0x4C in disp(a[0x002D3DE8])
    assert 0x14 in disp(a[0x002D3D9C]) and 0x48 in disp(a[0x002D3F27])
    # the pick: slot pane in ebx, cursor on only below value 5
    assert a[0x002BF935].mnemonic == 'mov' and ('imm', 5) in a[0x002BF990].ops


def test_weapon_screen_routes():
    """Every way to the slot pane 0x005236EC, and why the in-play pause has none."""
    a = by_address()
    pushes = lambda v: {i.address for i in decoded() if i.mnemonic == 'push' and ('imm', v) in i.ops}
    # screen 0x0052371C (builder sub_002D44D0 adds the pane) is pushed by the setup's
    # Weapons action sub_002C7710 and the editor's Map Settings action sub_003128F0
    assert {a[x].function for x in pushes(0x0052371C)} == {0x002C7710, 0x003128F0}
    # (the weapon list's pick sub_002BF910 and update sub_002BF9C0 return focus to it)
    assert {a[x].function for x in pushes(0x005236EC)} == {0x002BF910, 0x002BF9C0, 0x002D44D0}
    # the setup builder sub_002C78F0 skips the Weapons item (text 0x3B7) when its
    # second argument (ebx, esp+0x2C with 4 pushes pending) is set; the pause passes 1
    assert 0x2C in disp(a[0x002C79A1]) and a[0x002C808C].mnemonic == 'jne' and ('imm', 0x3B7) in a[0x002C80B6].ops
    assert ('imm', 1) in a[0x0019A018].ops and '0x2c78f0' in a[0x0019A01B].text
    # and offers its action only outside scene 100
    assert ('imm', 0x64) in a[0x002C8097].ops and a[0x002C809E].mnemonic == 'jg' and ('imm', 0x002C7710) in a[0x002C80AB].ops
    # front end, mode 9, list value 0xF (the map's set): rows are read-only in stock
    assert ('imm', 0xF) in a[0x002D415B].ops and ('imm', 0xF) in a[0x002D406F].ops
    # the pane tick runs the pane's update (+0xC, from descriptor +4) before its Up/Down
    assert a[0x0007AD76].mnemonic == 'call' and 0xC in disp(a[0x0007AD50])
    assert 0xC4 in disp(a[0x0007AE2E]) and ('imm', 0x20000) in a[0x0007AE7D].ops
    # Left/Right move only choice items (+0xB4 == 1)
    assert 0xB4 in disp(a[0x0007AFB1]) and ('imm', 0x40000) in a[0x0007AFC6].ops and ('imm', 0x80000) in a[0x0007AFDB].ops


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
