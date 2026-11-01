"""MapMaker custom character set: 20 slots (fork-only chunk, paged slot screen).

Compiles src/interface/pc_mapmaker_sets.inc against fake guest memory (with
mutations that must be caught) and, when the private XBE is available,
checks the stock facts the hooks rely on: every hooked call site and return
address, the 20-member set structs, the 10-member map-info form, the slot
screen descriptors, the Story AI choice loop, the play set block, and that
the fork chunk tag is unused by the game.
"""
import os
from pathlib import Path
import struct
import subprocess
import pytest

from native_compiler import native_compiler
from mapmaker_decoded import decoded, _private

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_sets.inc'

MUTATIONS = {
    'always-chunk': ('    if(!high && !sets_extras_used(e)) return;\n', ''),
    'ignores-story-ai': ('SETS_B(data+SETS_SAI_SLOT)>=SETS_STOCK) *(int *)ctx=1;', 'SETS_B(data+SETS_SAI_SLOT)>=SETS_STOCK) (void)ctx;'),
    'used-stale': ('    WORD_AT(info+0x248u)=used;\n', ''),
    'terminator-offset': ('WORD_AT(nterm+4u)=nterm-c;', 'WORD_AT(nterm+4u)=0;'),
    'no-room-check': ('used+8u+SETS_CHUNK+4u>SETS_SCAN', 'used>SETS_SCAN'),
    'eax-stale': ('        g_eax=used;\n', ''),
    'load-keeps-junk': ('    sets_clear_extras(e);\n    if(!(data=sets_chunk(c))) return;', '    if(!(data=sets_chunk(c))) return;'),
    'load-variant-unsigned': ('SETS_H(m+6u)=(int16_t)(int8_t)SETS_B(d+3u);', 'SETS_H(m+6u)=(int16_t)SETS_B(d+3u);'),
    'changed-ignores-extras': ('    if(g_eax || !(e=sets_editor())) return;\n', '    return;\n'),
    'changed-ignores-variant': ('(id!=-1 && SETS_H(m+6u)!=variant)', '(0 && variant)'),
    'reset-skips': ('    if((e=sets_editor())) sets_clear_extras(e);\n', '    (void)e;\n'),
    'fill-wrong-page': ('    if(ret==SETS_FILL_RET && sets.in_update && sets.page && g_eax) g_eax+=8u*SETS_STOCK;',
                        '    if(ret==SETS_FILL_RET && sets.in_update && sets.page && g_eax) g_eax+=8u*(SETS_STOCK-1u);'),
    'fill-any-time': ('    if(ret==SETS_FILL_RET && sets.in_update && sets.page && g_eax)', '    if(ret==SETS_FILL_RET && sets.page && g_eax)'),
    'store-no-scratch': ('    else if(ret==SETS_STORE_RET && sets.in_store) g_eax=SETS_SCRATCH;\n', ''),
    # the editor's 20-row slot list
    'rows-10': ('    for(v=0;v<SETS_SLOTS;v++) {\n        guest_call(sub_00078320', '    for(v=0;v<SETS_STOCK;v++) {\n        guest_call(sub_00078320'),
    'no-scroll-region': ('    guest_call(sub_000780E0,2,SETS_VIEW,0u);\n', ''),
    'no-region-end': ('    guest_call(sub_000781A0,0);\n    built=', '    built='),
    'region-height': ('#define SETS_VIEW 0x42200000u', '#define SETS_VIEW 0x42A00000u'),
    'no-row-group': ('        guest_call(sub_00078320,3,0u,1u,4u);\n', ''),
    'row-group-open': ('        guest_call(sub_00078380,0);\n', ''),
    'row-callbacks-swapped': ('SETS_ROW_HEIGHT,SETS_ROW_ACTION,SETS_ROW_DRAW);', 'SETS_ROW_HEIGHT,SETS_ROW_DRAW,SETS_ROW_ACTION);'),
    'set-list-value': ('0x004766D8u,0xFFFFFFFEu,0x08030C00u', '0x004766D8u,0xFFFFFFFBu,0x08030C00u'),
    'build-no-choices': ('    guest_call(sub_002C1830,1,menu);\n', ''),
    'build-no-frame': ('    guest_call(sub_00071BE0,3,1u,SETS_F5,SETS_F5);\n', ''),
    'build-size': ('SETS_F5,SETS_F9_2);', 'SETS_F5,SETS_F5);'),
    'build-footer-text': ('guest_call(sub_0003D7B0,1,0x3B2u),1u,0u);', 'guest_call(sub_0003D7B0,1,0xE89u),1u,0u);'),
    'build-ignores-switch': ("    if(scene!=102 || !sets_on() || !MAPMAKER_PTR_OK(menu,0x200u)) {", "    if(scene!=102 || !MAPMAKER_PTR_OK(menu,0x200u)) {"),
    'build-any-scene': ("    if(scene!=102 || !sets_on() || !MAPMAKER_PTR_OK(menu,0x200u)) {", "    if(!sets_on() || !MAPMAKER_PTR_OK(menu,0x200u)) {"),
    'build-no-ret': ('(int)WORD_AT(SETS_INDEX));\n    MAPMAKER_GUEST_RET();', '(int)WORD_AT(SETS_INDEX));'),
    'build-list-off': ('    sl.on=1;\n', ''),
    'build-keeps-shadow': ('    sl.on=0;sl.valid=0;sl.opened=0;', '    sl.on=0;sl.opened=0;'),
    'list-any-scene': ('static int sets_list(void) { return sl.on && WORD_AT(SETS_SCENE)==102u; }', 'static int sets_list(void) { return sl.on; }'),
    'list-row-from-1': ('v>=SETS_STOCK && v<SETS_SLOTS ? (int)(v-SETS_STOCK)', 'v>=1u && v<SETS_SLOTS ? (int)(v%SETS_STOCK)'),
    'list-row-past-end': ('v>=SETS_STOCK && v<SETS_SLOTS ? (int)(v-SETS_STOCK)', 'v>=SETS_STOCK && v<=SETS_SLOTS ? (int)(v-SETS_STOCK)'),
    'borrow-keeps': ('    memcpy(t,w,SETS_ROW);memcpy(w,s,SETS_ROW);memcpy(s,t,SETS_ROW);', '    memcpy(t,w,SETS_ROW);memcpy(w,s,SETS_ROW);(void)t;'),
    'draw-value-kept': ('    WORD_AT(item+0x14u)=(uint32_t)k;sets_borrow((unsigned)k);', '    sets_borrow((unsigned)k);'),
    'draw-value-stale': ('    sets_borrow((unsigned)k);WORD_AT(item+0x14u)=v;', '    sets_borrow((unsigned)k);'),
    'draw-no-return': ('    original();\n    sets_borrow((unsigned)k);WORD_AT(item+0x14u)=v;', '    original();\n    WORD_AT(item+0x14u)=v;'),
    'call-index-kept': ('    WORD_AT(pane+0x184u)=k+2u;\n    if(val==idx-2u)', '    if(val==idx-2u)'),
    'call-index-stale': ('    if(WORD_AT(pane+0x184u)==k+2u) WORD_AT(pane+0x184u)=idx;\n', ''),
    'call-value-kept': ('    if(val==idx-2u) WORD_AT(pane+0x18Cu)=k;\n', ''),
    'call-value-any': ('    if(val==idx-2u) WORD_AT(pane+0x18Cu)=k;\n', '    WORD_AT(pane+0x18Cu)=k;\n'),
    'call-value-stale': ('    if(val==idx-2u && WORD_AT(pane+0x18Cu)==k) WORD_AT(pane+0x18Cu)=val;\n', ''),
    'call-not-kept': ('    sets_borrow(k);\n    if(WORD_AT(pane+0x184u)==k+2u)', '    if(WORD_AT(pane+0x184u)==k+2u)'),
    'clear-not-stored': ('    if(val==idx-2u && WORD_AT(pane+0x18Cu)==k) WORD_AT(pane+0x18Cu)=val;\n    sets_list_keep(k);', '    if(val==idx-2u && WORD_AT(pane+0x18Cu)==k) WORD_AT(pane+0x18Cu)=val;'),
    'keep-preset': ('    if(!sets_custom() || !(e=sets_editor())) return;\n    memcpy(&id,r,4);', '    if(!(e=sets_editor())) return;\n    memcpy(&id,r,4);'),
    'keep-difficulty': ('SETS_B(m+3u)=(uint8_t)diff;', '(void)diff;'),
    'keep-variant': ('SETS_H(m+6u)=(int16_t)variant;\n}', '(void)variant;\n}'),
    'keep-wrong-member': ('    m=SETS_MEMBER(e+SETS_EDITOR_SET,SETS_STOCK+k);\n    SETS_H(m)=(int16_t)id;', '    m=SETS_MEMBER(e+SETS_EDITOR_SET,k);\n    SETS_H(m)=(int16_t)id;'),
    'action-stock': ('    if(!MAPMAKER_PTR_OK(pane,0x200u) || (k=sets_list_row(WORD_AT(pane+0x184u)-2u))<0) { original();return; }\n    sets_list_call(original,pane,(unsigned)k);\n}\n/* WRAP sub_002C1570',
                     '    (void)k;original();\n}\n/* WRAP sub_002C1570'),
    'pane-stock': ('    if((k=sets_list_row((uint32_t)row))<0) { original();return; }\n    sets_list_call(original,pane,(unsigned)k);',
                   '    (void)k;original();'),
    'pick-stock': ('    WORD_AT(slot+0x184u)=(uint32_t)k+2u;WORD_AT(slot+0x18Cu)=SETS_STOCK-1u;', '    original();return;'),
    'pick-stock-advance': ('WORD_AT(slot+0x184u)=(uint32_t)k+2u;WORD_AT(slot+0x18Cu)=SETS_STOCK-1u;', 'WORD_AT(slot+0x184u)=(uint32_t)k+2u;WORD_AT(slot+0x18Cu)=(uint32_t)k;'),
    'pick-not-kept': ('    sets_list_keep((unsigned)k);\n    if(advance', '    if(advance'),
    'pick-index-stale': ('    WORD_AT(slot+0x184u)=idx;WORD_AT(slot+0x18Cu)=val;\n', '    WORD_AT(slot+0x18Cu)=val;\n'),
    'pick-no-advance': ('    if(advance && val+1u<SETS_SLOTS) sets_advance(slot,val+1u);\n}', '}'),
    'pick-advance-past-end': ('    if(advance && val+1u<SETS_SLOTS) sets_advance', '    if(advance && val<SETS_SLOTS) sets_advance'),
    'pick-advance-disabled': ("    advance=MAPMAKER_PTR_OK(item,0x2Cu) && !(SETS_B(item+0x28u)&1u) &&", "    advance=MAPMAKER_PTR_OK(item,0x2Cu) &&"),
    'pick-advance-submode': ("            SETS_B(SETS_SUBMODE)!=0x13u && val==idx-2u;", "            val==idx-2u;"),
    'pick-slot-10-stops': ('        if(advance && val==SETS_STOCK-1u && WORD_AT(slot+0x184u)==idx && WORD_AT(slot+0x18Cu)==val) sets_advance(slot,val+1u);\n', ''),
    'pick-slot-10-twice': ('val==SETS_STOCK-1u && WORD_AT(slot+0x184u)==idx && WORD_AT(slot+0x18Cu)==val)', 'val<SETS_STOCK && WORD_AT(slot+0x184u)==idx)'),
    'screen-no-shadow': ('        if((int32_t)index>=0xC && sets_editor()) {', '        if(0) {'),
    'screen-preset-shadow': ('        if((int32_t)index>=0xC && sets_editor()) {', '        if(sets_editor()) {'),
    'screen-shadow-every-frame': ('    if(!sl.valid || sl.index!=index) {', '    if(1) {'),
    'screen-shadow-sticks': ('    if(!sl.valid || sl.index!=index) {', '    if(!sl.valid) {'),
    'screen-no-force': ('            sets.page=1;sets.in_update=1;WORD_AT(SETS_LOADED)=0xFFFFFFFFu;', '            sets.page=1;sets.in_update=1;'),
    'screen-unshifted': ('            sets.page=1;sets.in_update=1;WORD_AT(SETS_LOADED)=0xFFFFFFFFu;', '            sets.in_update=1;WORD_AT(SETS_LOADED)=0xFFFFFFFFu;'),
    'screen-no-repush': ('            g_esp-=4u;WORD_AT(g_esp)=ret;         /* for the stock run below */\n', ''),
    'screen-work-lost': ('            memcpy(sets_work(0),save,sizeof save);WORD_AT(SETS_LOADED)=loaded;', '            WORD_AT(SETS_LOADED)=loaded;'),
    'screen-loaded-lost': ('            memcpy(sets_work(0),save,sizeof save);WORD_AT(SETS_LOADED)=loaded;', '            memcpy(sets_work(0),save,sizeof save);'),
    'screen-page-sticks': ('            sets.page=0;sets.in_update=0;\n            memcpy(sl.rows', '            sets.in_update=0;\n            memcpy(sl.rows'),
    'screen-skips-original': ('    sets.in_update=1;\n    original();                                   /* pops the return address */\n    sets.in_update=0;\n}',
                              '    sets.in_update=1;\n    g_esp+=4u;\n    sets.in_update=0;\n}'),
    'empty-keeps-ids': ('        memcpy(r,&none,4);memcpy(r+0x10,&none,4);', '        memcpy(r+0x10,&none,4);'),
    'diag-no-open': ('        sets_diag(0,"slot pane open:', '        if(0) sets_diag(0,"slot pane open:'),
    'diag-no-moves': ('    sl.last_row=row;sl.last_scroll=scroll;\n', ''),
    'choice-any-caller': ('WORD_AT(g_esp)!=SETS_CHOICE_RET || !sets_custom()', '0 || !sets_custom()'),
    'choice-ignores-switch': ('    if(!sets_on() && !(MAPMAKER_PTR_OK(value,4)', '    if(0 && !(MAPMAKER_PTR_OK(value,4)'),
    'choice-empty-name': ('    if(SETS_H(m)==-1) return guest_call(sub_0003D7B0,1,0xD7Cu);', '    if(0) return 0;'),
    'name-no-shift': ('    else if(ret==SETS_NAME_SET_RET && sets.name_shift && g_eax) g_eax+=8u*SETS_STOCK;\n', ''),
    'name-shift-sticks': ('    sets.name_shift=0;\n    return text;', '    return text;'),
    'name-wrong-item': ('SETS_SLOT_ITEM+k-SETS_STOCK);', 'SETS_SLOT_ITEM+k);'),
    'icon-skips': ('WORD_AT(g_esp+4u)=item-SETS_STOCK;', '(void)item;'),
    'icon-any-caller': ('        if(ret==SETS_ICON_LIST_RET || ret==SETS_ICON_MAP_RET) {', '        if(ret) {'),
    'icon-one-caller': ('        if(ret==SETS_ICON_LIST_RET || ret==SETS_ICON_MAP_RET) {', '        if(ret==SETS_ICON_LIST_RET) {'),
    'icon-range': ('item<SETS_SLOT_ITEM+SETS_SLOTS)', 'item<=SETS_SLOT_ITEM+SETS_SLOTS)'),
    'play-count': ('    WORD_AT(dst)=SETS_SLOTS;\n', ''),
    'play-empty-ai': ('            SETS_H(SETS_MEMBER(dst,k))=SETS_H(SETS_MEMBER(dst,first));\n', ''),
    'play-any-caller': ('WORD_AT(g_esp)!=SETS_PLAY_RET) { original();return; }', '0) { original();return; }'),
    'preload-empty': ('        if(id!=-1) guest_call(sub_000D7780', '        if(1) guest_call(sub_000D7780'),
    'match-no-refill': ('        guest_call(sub_002C0310,0);\n', ''),
    'match-any-scene': ('    if(sets.page && WORD_AT(SETS_SCENE)==102u && (int32_t)WORD_AT(SETS_INDEX)>=0xC) {',
                        '    if(sets.page && (int32_t)WORD_AT(SETS_INDEX)>=0xC) {'),
    'match-keeps-page': ('        sets.page=0;\n        g_eax=WORD_AT(SETS_INDEX);', '        g_eax=WORD_AT(SETS_INDEX);'),
    'match-eax-lost': ('        g_eax=a;\n', ''),
    'preload-any-caller': ('    if(ret!=SETS_PRELOAD_RET || !(data=', '    if(!(data='),
}


RBS = ROOT / 'src/interface/pc_mapmaker_realbosses.inc'
# Whole name chain (mapmaker_sets_names.c: realbosses inside sets, real guest
# stack and return addresses). 'trophy-remap-only' is the v0.4.0-mapmaker.11
# code: slots 11-20 asked sub_002BE600 for the row from the host, a return
# address realbosses does not name (-1), and remapped only id 0x15 to row
# 150, so the Giant Skeleton (0x28, row 151) had no name there;
# 'host-row-lookup' is the code before that (no boss named).
_HOST_ROW = ('    { uint32_t row=guest_call(sub_002BE600,2,(uint32_t)(int32_t)SETS_H(m),(uint32_t)(int32_t)SETS_H(m+6u));\n'
             '%s      text=guest_call(sub_0003D7B0,1,guest_call(sub_002BE300,1,row)); }\n')
NAME_MUTATIONS = {
    'trophy-remap-only': (INC, '    text=guest_call(sub_00303350,1,SETS_SLOT_ITEM+k-SETS_STOCK);\n',
                          _HOST_ROW % '      if(row==0xFFFFFFFFu && SETS_H(m)==0x15) row=150u;\n'),
    'host-row-lookup': (INC, '    text=guest_call(sub_00303350,1,SETS_SLOT_ITEM+k-SETS_STOCK);\n', _HOST_ROW % ''),
    'no-shift': (INC, '    else if(ret==SETS_NAME_SET_RET && sets.name_shift && g_eax) g_eax+=8u*SETS_STOCK;\n', ''),
    'shift-sticks': (INC, '    sets.name_shift=0;\n    return text;', '    return text;'),
    'wrong-item': (INC, 'SETS_SLOT_ITEM+k-SETS_STOCK);', 'SETS_SLOT_ITEM+k);'),
    'shift-any-time': (INC, 'else if(ret==SETS_NAME_SET_RET && sets.name_shift && g_eax)',
                       'else if(ret==SETS_NAME_SET_RET && g_eax)'),
    'rbs-skeleton-unnamed': (RBS, 'row=id==RBS_ID?RBS_ROW:id==RBS_SKEL_ID?RBS_SKEL_ROW:id==RBS_WALK_ID',
                             'row=id==RBS_ID?RBS_ROW:id==RBS_SKEL_ID?RBS_ROW:id==RBS_WALK_ID'),
    'rbs-name-caller-only': (RBS, '(ret==RBS_RET_NAME && WORD_AT(g_esp+12u)==RBS_RET_NAME_OUTER)', '0'),
}


def build(tmp_path, mutation, source_name='mapmaker_sets.c', edit=None):
    compiler, env = native_compiler(tmp_path)
    if edit is None and mutation != 'fixed':
        edit = (INC, *MUTATIONS[mutation])
    if edit is not None:
        path, old, new = edit
        text = path.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / path.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / (Path(source_name).stem + ('.exe' if os.name == 'nt' else ''))
    source = ROOT / 'tests/mapmaker' / source_name
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
def test_mapmaker_sets(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker sets ok' in result.stdout
        # the diagnostics a user run shows (stderr)
        assert '[mapmaker] sets: slot pane build (stock 10 rows: TSFP_MAPMAKER_LIMITS is not 1) ret=0x00078F74 menu=0x00301000' in result.stderr
        assert '[mapmaker] sets: slot pane build (20 rows, scrolling) ret=0x00078F74 menu=0x00301000 set=17' in result.stderr
        assert '[mapmaker] sets: slot pane open: pane=0x00301000 items=' in result.stderr
        assert '[mapmaker] sets: list: slot 13 highlighted, scroll 0.0' in result.stderr
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


@pytest.mark.parametrize('mutation', ['fixed', *NAME_MUTATIONS])
def test_mapmaker_sets_names(tmp_path, mutation):
    """Slots 11-20 are named exactly as slots 1-10, realboss rows included."""
    exe = build(tmp_path, mutation, 'mapmaker_sets_names.c', NAME_MUTATIONS.get(mutation))
    env = {k: v for k, v in os.environ.items() if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker sets names ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_no_guest_code_table_and_over_stock():
    """Wrappers only; the container's over-stock check knows the fork tag."""
    assert not (ROOT / 'config/guest-code-patches/sets.json').exists()
    container = (ROOT / 'src/interface/pc_mapmaker_container.inc').read_text(encoding='utf-8')
    assert 'if(tag==0x00C776F5u ||' in container
    assert '#define SETS_TAG 0x00C776F5u' in INC.read_text(encoding='utf-8')


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


def test_hook_return_addresses():
    a = by_address()
    assert ret_of(a, 0x002C0314, 0x2c0290) == 0x002C0319 and a[0x002C0314].function == 0x002C0310
    assert ret_of(a, 0x003141B0, 0x312950) == 0x003141B5
    assert ret_of(a, 0x00314213, 0x312950) == 0x00314218
    assert ret_of(a, 0x0031CD2D, 0x78590) == 0x0031CD32 and a[0x0031CD2D].function == 0x0031CB90
    assert ret_of(a, 0x00240DCE, 0x33ebf0) == 0x00240DD3
    assert ret_of(a, 0x0023F472, 0x23f320) == 0x0023F477
    assert ret_of(a, 0x0032C6FA, 0x3006c0) == 0x0032C6FF
    # the only callers of the wrapped play/editor routines that the hooks act on
    callers = {i.function for i in decoded() if i.call and '0x33ebf0' in i.text}
    assert callers == {0x00240C50}
    callers = {i.address for i in decoded() if i.call and '0x23f320' in i.text}
    assert callers == {0x0023F472}
    assert ret_of(a, 0x00313353, 0x312810) == 0x00313358
    callers = {i.address for i in decoded() if i.call and '0x312810' in i.text}
    assert callers == {0x00313353}
    # sub_00312810 compares the chunk with the undo shadow sub_003365F0 = [0x00765E38], members 0..9
    assert '0x3365f0' in a[0x00312819].text and ('imm', 0xA) in a[0x0031286A].ops
    assert mem(a[0x003365F0])[0][4] == 0x00765E38
    callers = {i.function for i in decoded() if (i.call or i.branch) and '0x314210' in i.text}
    assert callers == {0x002BFFD0}


def test_slot_list_facts():
    """The editor's 20-row list replays the stock builder; rows 11-20 borrow a work-copy row."""
    a = by_address()
    pushed = lambda x: [op[1] for op in a[x].ops if op[0] == 'imm']
    # stock builder sub_002C19F0: frame (0x10, 0, 45.0f, 0, 0x002C1450), sub_00071BE0(1, 5.0f, 5.0f)
    assert [pushed(x) for x in (0x002C19F3, 0x002C19F8, 0x002C19FA, 0x002C19FF, 0x002C1A01)] == \
        [[0x002C1450], [0], [0x42340000], [0], [0x10]] and '0x75b70' in a[0x002C1A03].text
    assert [pushed(x) for x in (0x002C1A08, 0x002C1A0D, 0x002C1A12)] == [[0x40A00000], [0x40A00000], [1]]
    assert '0x71be0' in a[0x002C1A14].text
    # title 0xE8B (-1, 0x04032404), set list 0x004766D8 (-2, 0x08030C00), sub_002C1830(menu), sub_00079710(0)
    assert pushed(0x002C1A22) == [0x04032404] and pushed(0x002C1A27) == [-1 & 0xFFFFFFFF] and pushed(0x002C1A29) == [0xE8B]
    assert pushed(0x002C1A42) == [0x08030C00] and pushed(0x002C1A47) == [-2 & 0xFFFFFFFF] and pushed(0x002C1A49) == [0x004766D8]
    assert '0x2c1830' in a[0x002C1A58].text and 0x40 in disp(a[0x002C1A53]) and '0x79710' in a[0x002C1A5F].text
    # ten row groups: sub_00078320(0, 1, 4); editor: sub_000792B0(0, row, 0, 40.0f, 4.0f, sub_002C1490, sub_002C0660)
    assert [pushed(x) for x in (0x002C1A70, 0x002C1A72, 0x002C1A74)] == [[4], [1], [0]] and '0x78320' in a[0x002C1A76].text
    assert ('imm', 0x66) in a[0x002C1A83].ops and a[0x002C1A86].mnemonic == 'jne'
    assert [pushed(x) for x in (0x002C1A88, 0x002C1A8D, 0x002C1A92, 0x002C1A97, 0x002C1A9C, 0x002C1A9F)] == \
        [[0x002C0660], [0x002C1490], [0x40800000], [0x42200000], [0], [0]]
    assert a[0x002C1A9E].mnemonic == 'push' and a[0x002C1A9E].ops == (('reg', 'esi'),)       # the row value
    assert a[0x002C1A67].mnemonic == 'xor' and 'esi' in a[0x002C1A67].text                    # from 0
    assert '0x792b0' in a[0x002C1AA1].text and a[0x002C1AA9].mnemonic == 'jmp' and '0x2c1c04' in a[0x002C1AA9].text
    assert '0x78380' in a[0x002C1C04].text and ('imm', 0xA) in a[0x002C1C0A].ops
    # sizes and footer: sub_00077140(menu), sub_00077360(menu, 5.0f, 9.2f), 0xE88 / 0x3B2 / 0x1A27
    assert '0x77140' in a[0x002C1C1A].text and pushed(0x002C1C24) == [0x41133333] and pushed(0x002C1C29) == [0x40A00000]
    assert '0x77360' in a[0x002C1C2F].text
    assert [pushed(x) for x in (0x002C1C3B, 0x002C1C57, 0x002C1C73)] == [[0xE88], [0x3B2], [0x1A27]]
    assert [pushed(x) for x in (0x002C1C49, 0x002C1C65, 0x002C1C81)] == [[0], [3], [1]]
    assert a[0x002C1C8E].mnemonic == 'ret'
    # a group end counts one row (+0x180); group items made in a region get flag 0x40 too
    assert 0x180 in disp(a[0x000783C8]) and a[0x000783C8].mnemonic == 'inc'
    assert 0x14 in disp(a[0x000791A3]) and ('imm', 0x40) in a[0x0007914A].ops
    # a laid-out row with +0xB0 set and flag 0x40 adds its height to the content +0x5C
    assert 0xB0 in disp(a[0x000753A7]) and 0x5C in disp(a[0x000753CD]) and 0x5C in disp(a[0x000753D2])
    assert ('imm', 1) in a[0x00078392].ops and 0xB0 in disp(a[0x00078392])     # sub_00078380 marks the row's last item
    # the draw reads the row value at item +0x14; the editor names rows below 10 only; highlight = item pointer
    assert 0x34 in disp(a[0x002C0663]) and 0x14 in disp(a[0x002C0667])
    assert ('imm', 0xA) in a[0x002C08AF].ops and '0x71a60' in a[0x002C0A2B].text and 0x48 in disp(a[0x002C0A30])
    # the row action and the pane update: row = +0x184 - 2, the editor stops at 10 rows
    assert 0x184 in disp(a[0x002C1499]) and ('imm', 2) in a[0x002C149F].ops and ('imm', 0xA) in a[0x002C14B3].ops
    assert 0x184 in disp(a[0x002C157E]) and ('imm', 2) in a[0x002C1584].ops and ('imm', 0xA) in a[0x002C16D8].ops
    assert ('imm', 0x800000) in a[0x002C16FD].ops and 0x0075FD90 in disp(a[0x002C1718])
    assert 0x0075FDA8 in disp(a[0x002C17F2]) and 0x0075FD90 in disp(a[0x002C17F8]) and '0x2be600' in a[0x002C1800].text
    # the pick: slot pane (sub_00074670(parent, 0x0052372C)) +0x184 - 2, moves on while +0x18C < 9
    assert pushed(0x002C1CB4) == [0x0052372C] and 0x184 in disp(a[0x002C1CC7]) and ('imm', 2) in a[0x002C1CD9].ops
    assert 0x0075FD90 in disp(a[0x002C1D2B]) and 0x0075FDA4 in disp(a[0x002C1D37]) and 0x0075FDA8 in disp(a[0x002C1D50])
    assert 0x18C in disp(a[0x002C1D88]) and ('imm', 9) in a[0x002C1D8E].ops and '0x74460' in a[0x002C1DC5].text
    assert 0x28 in disp(a[0x002C1C9F]) and a[0x002C1CA9].mnemonic == 'jne'
    # the picker list scrolls in a region; its descriptor's update is the highlight
    assert '0x780e0' in a[0x002C1F15].text and '0x781a0' in a[0x002C1FA0].text
    # the update's reload portraits return to 0x002C054C (realbosses names its rows there)
    assert ret_of(a, 0x002C0547, 0x2be600) == 0x002C054C


def test_slot_list_descriptors():
    rd = xbe_reader()
    # slot pane {builder, update}, picker {builder, update = highlight}
    assert struct.unpack('<2I', rd(0x0052372C, 8)) == (0x002C19F0, 0x002C1570)
    assert struct.unpack('<2I', rd(0x0052374C, 8)) == (0x002C1EA0, 0x002C1E10)


def test_set_structs_hold_twenty():
    a = by_address()
    # rep stosd 0x29 dwords = 0xA4 = count + 20 x 8 (play builder, non-editor getter)
    for site, buf in ((0x002C0231, 0x0075FB58), (0x00312967, 0x00762DC0)):
        assert ('imm', 0x29) in a[site].ops and ('imm', buf) in a[site + 5].ops
    # the editor set is editor +0x11860, the weapon set follows at +0x11904 = +0xA4
    assert ('imm', 0x11860) in a[0x0031295E].ops and ('imm', 0x11904) in a[0x00312A6E].ops
    # new map: the preset (0xA4 each) is copied whole into the editor set
    assert ('imm', 0xA4) in a[0x002F9F21].ops and ('imm', 0x11860) in a[0x002F9F2D].ops and ('imm', 0x29) in a[0x002F9F3D].ops
    # spawn lookup: slots above 19 become 0
    assert a[0x0006BC55].mnemonic == 'cmp' and ('imm', 0x13) in a[0x0006BC55].ops
    # play set: [desc+0x150] is a 0x190-byte block, the set is at +0xE0 (0xE0 + 0xA4 <= 0x190)
    assert ('imm', 0x190) in a[0x00240B68].ops and mem(a[0x00240B78])[0][4] == 0x150
    assert ('imm', 0xE0) in a[0x00240DC8].ops and mem(a[0x00240DC4])[0][4] == 0x16
    assert ('imm', 0xE0) in a[0x0033EBDA].ops


def test_map_info_holds_ten():
    a = by_address()
    # compile: count byte constant 10 at +0x26 (data +0x16), members word/byte/byte from the editor set
    assert ('imm', 0xA) in a[0x00300884].ops and mem(a[0x00300884])[0][4] == 0x26
    assert mem(a[0x00300896])[0][4] == 0x11864 and mem(a[0x003008A8])[0][4] == 0x1186A and mem(a[0x003008B7])[0][4] == 0x11867
    # the weapon set index follows the 10th member (data +0x40), then names: the chunk is full
    assert mem(a[0x003008D2])[0][4] == 0x50
    assert ('imm', 0x3DC) in a[0x0030072A].ops and ('imm', 0xC77672) in a[0x0030095A].ops
    # editor load: 10 unrolled member copies, the last at +0x118AF
    loads = [i for i in decoded() if i.function == 0x00301A90 and mem(i) and 0x11864 <= mem(i)[0][4] <= 0x118B2 and mem(i)[0][4] % 8 == 4]
    assert len({mem(i)[0][4] for i in loads}) == 10
    # compile terminator and used: tag 0xC7766A at offset used - 8, info +0x248 = used
    assert ('imm', 0xC7766A) in a[0x00301A39].ops and mem(a[0x00301A53])[0][4] == 0x248


def test_slot_screen_and_choice():
    a = by_address()
    # the work copy has 10 rows of 0x1C; the builder adds 10 rows
    assert ('imm', 0x0075FEA8) in a[0x002C0555].ops and ('imm', 0xA) in a[0x002C1C0A].ops
    # sub_002C0490 reloads when the index differs from [0x0078AEAC], ends with the store
    assert mem(a[0x002C04FB])[0][4] == 0x0078AEAC and '0x2c0310' in a[0x002C0522].text
    assert a[0x002C05C8].branch and '0x2bffd0' in a[0x002C05C8].text
    assert ('imm', 0x0052372C) in a[0x002C04AA].ops and '0x74670' in a[0x002C04B0].text
    # the store writes the count of used rows to set +0
    assert a[0x003143B1].mnemonic == 'mov' and mem(a[0x003143B1])[0][4] == 0 and mem(a[0x003143B1])[0][1] == 'eax'
    # titles are pointers at menu +0x204 (sub_00075B70)
    assert mem(a[0x00075BCC])[0][4] == 0x204
    # Story AI character choice: options k = 0..9 from set +4 + 8k, empty text 0xD7C
    assert a[0x0031CD24].mnemonic == 'cmp' and ('imm', 0xA) in a[0x0031CD24].ops
    assert ('imm', 0xD7C) in a[0x0031CD0A].ops and '0x71800' in a[0x0031CD18].text
    # item ids 0x38 + k name slot k (sub_00303350 case 3: id*8 - 0x1BC = +4 + 8*(id-0x38))
    assert mem(a[0x003033A9])[0][4] == (-0x1BC) & 0xFFFFFFFF and mem(a[0x003033B1])[0][4] == (-0x1B6) & 0xFFFFFFFF


def test_story_ai_item_icon_and_name():
    """A Story AI is drawn as item 0x38 + slot; slot names come from sub_002BE600 -> sub_002BE300."""
    a = by_address()
    # sub_0031D3E0 (AI -> item: 9 for type 5, else slot +0x28 plus 0x38), only for the overview marker
    assert ('imm', 9) in a[0x0031D3FD].ops and mem(a[0x0031D403])[0][4] == 0x28
    assert a[0x0031D406].mnemonic == 'add' and ('imm', 0x38) in a[0x0031D406].ops
    callers = {i.address for i in decoded() if i.call and '0x31d3e0' in i.text}
    assert callers == {0x00309ABC}
    assert ret_of(a, 0x00309AC2, 0x315d20) == 0x00309AC7 and a[0x00309AC2].function == 0x00309A80
    # the Story AI list's icon row callback sub_0031D410 inlines it (AI +0x28 = editor +0x2B0)
    assert mem(a[0x0031D43B])[0][4] == 0x2B0 and ('imm', 0x38) in a[0x0031D442].ops
    assert ret_of(a, 0x0031D45B, 0x315d20) == 0x0031D460 and a[0x0031D45B].function == 0x0031D410
    assert ('imm', 0x0031D410) in a[0x0031D532].ops        # pushed as the row draw callback by sub_0031D4C0
    # sub_00315D20: catalogue +0xB4 icon, +0xB8 flag 0x400; its other callers draw catalogue items
    assert mem(a[0x00315D2A])[0][4] == 0x00525010 and mem(a[0x00315D35])[0][4] == 0x0052500C
    callers = {i.address for i in decoded() if (i.call or i.branch) and '0x315d20' in i.text}
    assert callers == {0x00309AC2, 0x0030DA35, 0x0031D45B, 0x00320545, 0x003362AB}
    # sub_002BE650 = sub_002BE300(sub_002BE600(id, variant)); sub_00303350 case 3 calls it at 0x003033BB
    assert '0x2be600' in a[0x002BE65A].text and '0x2be300' in a[0x002BE660].text
    assert ret_of(a, 0x003033BB, 0x2be650) == 0x003033C0
    # the name reads the set from sub_002C0290(sub_003129D0()) (return 0x003033A9, wrapped by
    # pc_mapmaker_sets_set_ptr), member item - 0x38; the loop calls it for item 0x38 + k
    assert ret_of(a, 0x003033A4, 0x2c0290) == 0x003033A9 and a[0x003033A4].function == 0x00303350
    assert '0x3129d0' in a[0x0030339E].text and mem(a[0x003129D0])[0][4] == 0x00784024
    assert ret_of(a, 0x0031CD03, 0x303350) == 0x0031CD08 and mem(a[0x0031CCFF])[0][4] == 0x38
    rd = xbe_reader()
    row = lambda item: rd(0x00524F58 + item * 0xC4, 0xC4)
    slot_icons = {struct.unpack_from('<2I', row(item), 0xB4) for item in range(0x38, 0x42)}
    assert slot_icons == {(0x42, 1)}                     # the ten slot items share one icon
    for item in range(0x42, 0x4C):                       # what slots 11-20 would be drawn as
        assert struct.unpack_from('<2I', row(item), 0xB4) != (0x42, 1)
    assert struct.unpack_from('<I', row(0x42), 0xB4)[0] == 0x45


def test_pause_bots_screen_is_the_arcade_roster():
    """The pause / Arcade setup Bots screen is the slot screen, editing the match's bots."""
    a = by_address()
    # pause > setup (descriptor 0x004E9544 = sub_001A40A0) > sub_0019A000 > sub_002C78F0(menu, 1, ...)
    assert '0x19a000' in a[0x001A40E0].text and '0x2c78f0' in a[0x0019A01B].text
    assert ('imm', 1) in a[0x0019A018].ops
    # its Bots item (text 0x3B8) opens sub_002C7730 = push the slot screen 0x0052375C
    assert ('imm', 0x002C7730) in a[0x002C81CD].ops and ('imm', 0x3B8) in a[0x002C81D8].ops
    assert ('imm', 0x0052375C) in a[0x002C7735].ops
    # the editor's Test map setup (sub_002EFBF0) uses the same builder; its start is sub_002D38C0
    assert '0x2c78f0' in a[0x002EFC22].text and a[0x002EFBE0].branch and '0x2d38c0' in a[0x002EFBE0].text
    assert ret_of(a, 0x002D392E, 0x2d2c70) == 0x002D3933 and ret_of(a, 0x002D5E4D, 0x2d2c70) == 0x002D5E52
    # outside the editor the custom getter: mode 9 (MapMaker Arcade, sub_002C8400) and index 0x11
    # -> the map's set rebuilt from map info (10 members), else the profile's sets
    assert ('imm', 9) in a[0x002BFEFC].ops and ('imm', 0x11) in a[0x002BFF05].ops
    assert '0x312950' in a[0x002BFF1F].text and mem(a[0x002BFF15])[0][4] == (-0x4E4) & 0xFFFFFFFF
    assert ('imm', 9) in a[0x002C8429].ops
    # index 0x11 is offered only in mode 9 (sub_002C1830)
    assert ('imm', 0x11) in a[0x002C18B5].ops and ('imm', 9) in a[0x002C18AC].ops
    # match start: the 10 work-copy rows become bots in 16-byte per-actor arrays
    assert ('imm', 0x0075FDA0) in a[0x002D36EA].ops and ('imm', 0x0075FEB8) in a[0x002D381C].ops
    assert mem(a[0x002D37C3])[0][4] == 0x0078AD6C and mem(a[0x002D37C9])[0][4] == 0x0078AD9C
    assert mem(a[0x002D37B7])[0][4] == 0x0078AD7C and mem(a[0x002D3811])[0][4] == 0x0078AD8C
    rd = xbe_reader()
    assert struct.unpack('<2I', rd(0x004E9544, 8)) == (0x001A40A0, 0x001A4070)
    # actors per match by sub-mode (players + bots), mode row 0x10
    assert struct.unpack('<4i', rd(0x004D239C + 0x10 * 60, 16)) == (14, 14, 16, 16)
    # the hook pages only the editor (scene 102)
    assert "return WORD_AT(SETS_SCENE)==102u && (int32_t)WORD_AT(SETS_INDEX)>=0xC;" in INC.read_text(encoding='utf-8')


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


def test_descriptors_and_presets():
    rd = xbe_reader()
    # {builder, update}: character set screen and its slot pane
    assert struct.unpack('<2I', rd(0x0052375C, 8)) == (0x002C2C20, 0x002C0490)
    assert struct.unpack('<2I', rd(0x0052372C, 8)) == (0x002C19F0, 0x002C1570)
    # presets: count 10, members 10..19 zero (id 0, which is a real character)
    for s in range(12):
        row = rd(0x0053E41C + s * 0xA4, 0xA4)
        assert struct.unpack_from('<I', row, 0)[0] == 10
        assert row[4 + 8 * 10:] == bytes(0xA4 - 4 - 80)


def test_fork_tag_unused():
    tags = {v for i in decoded() for k, v in i.values() if 0x00C77600 <= v < 0x00C77700}
    assert 0x00C776F5 not in tags and max(tags) == 0x00C77675


def test_weapon_slot_item_types():
    """Items 0x11..0x16 are weapon slots 1-6 and 0x17..0x1C ammo slots 1-6 (catalogue +8)."""
    rd = xbe_reader()
    for item in range(0x11, 0x1D):
        row = rd(0x00524F58 + item * 0xC4, 0xC4)
        category, slot = struct.unpack_from('<h', row, 2)[0], struct.unpack_from('<I', row, 8)[0]
        assert (category, slot) == ((8, item - 0x11) if item < 0x17 else (9, item - 0x17))
    # weapon presets: count 6 of the 15 the 0x44 struct holds
    for s in range(10):
        assert struct.unpack('<I', rd(0x00518F28 + s * 0x44 + 4, 4))[0] == 6
