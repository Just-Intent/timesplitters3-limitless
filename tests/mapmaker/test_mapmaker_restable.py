"""Compile the MapMaker editor resource table against fake guest memory."""
import os
from pathlib import Path
import re
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
TABLE = ROOT / 'src/interface/pc_mapmaker_restable.inc'
LIMITS = ROOT / 'src/interface/pc_mapmaker_limits.inc'
EDITOR = ROOT / 'src/interface/pc_mapmaker_editor.inc'
OBJECTS = ROOT / 'src/interface/pc_mapmaker_objects.inc'
GUEST = ROOT / 'src/interface/pc_menu_mouse_guest.inc'

# (file, old, new): each breaks one contract the fixture must detect.
MUTATIONS = {
    # The rows show the numbers of the editor's own checks.
    'tiles-held': (LIMITS, 'mapmaker_limits_view.pieces=p.pieces;', 'mapmaker_limits_view.pieces=p.pieces+h.pieces;'),
    'items-no-free': (LIMITS, 'mapmaker_limits_view.items=p.sub+extra;', 'mapmaker_limits_view.items=p.sub;'),
    'objects-no-reserve': (LIMITS, 'mapmaker_limits_view.objects=mapmaker_objects_needed(p.pieces,p.sub+extra);',
                           'mapmaker_limits_view.objects=p.pieces+p.sub+extra;'),
    'links-no-stitch': (LIMITS, 'mapmaker_limits_view.links=mapmaker_link_cost(&p);', 'mapmaker_limits_view.links=p.cost[2];'),
    'memory-total': (LIMITS, 'mapmaker_limits_view.memory=placed_mem;', 'mapmaker_limits_view.memory=total_mem;'),
    'over-not-kept': (LIMITS, 'mapmaker_limits_view.groups=groups;mapmaker_limits_view.over=over;',
                      'mapmaker_limits_view.groups=groups;mapmaker_limits_view.over=0;'),
    'view-not-cleared': (LIMITS, '    mapmaker_limits_view.valid=0;                 /* until this check completes */\n', ''),
    'view-any-editor': (TABLE, 'mapmaker_limits_view.editor!=editor || ', ''),
    'tiles-stock-cap': (TABLE, 'MAPMAKER_TABLE_ROW("Tiles",v->pieces,MAPMAKER_LIMIT_PIECES,',
                        'MAPMAKER_TABLE_ROW("Tiles",v->pieces,200u,'),
    'pads-wrong-bit': (TABLE, 'MAPMAKER_TABLE_ROW("Cover pads",v->cover,MAPMAKER_LIMIT_COVER,MAPMAKER_CAP_COVER);',
                       'MAPMAKER_TABLE_ROW("Cover pads",v->cover,MAPMAKER_LIMIT_COVER,MAPMAKER_CAP_PADS);'),
    'memory-bytes': (TABLE, 'MAPMAKER_TABLE_ROW("Memory",memory_kb,', 'MAPMAKER_TABLE_ROW("Memory",v->memory,'),
    'memory-rounded-down': (TABLE, '((uint64_t)v->memory+MAPMAKER_TABLE_KB-1u)/MAPMAKER_TABLE_KB', '((uint64_t)v->memory)/MAPMAKER_TABLE_KB'),
    'pickups-no-keycards': (TABLE, '{8u,9u,10u,11u,12u,13u,19u,20u,21u,31u}', '{8u,9u,19u,20u,21u,31u}'),
    'pickups-editor-cap': (TABLE, 'MAPMAKER_TABLE_ROW("Pickups",pickups,MAPMAKER_OBJECTS_PICKUPS,0u);',
                           'MAPMAKER_TABLE_ROW("Pickups",pickups,MAPMAKER_LIMIT_SUB,0u);'),
    'category-wrong-half': (TABLE, 'cat=(int32_t)(int16_t)(WORD_AT(row)>>16);', 'cat=(int32_t)(int16_t)(WORD_AT(row)&0xFFFFu);'),
    'category-unbounded-id': (TABLE, 'if(id>=MAPMAKER_TABLE_CATALOGUE_N || !MAPMAKER_PTR_OK(row,4)) continue;',
                              'if(!MAPMAKER_PTR_OK(row,4)) continue;'),
    'category-stock-max': (TABLE, 'uint32_t a=MAPMAKER_TABLE_CATEGORIES+i*24u+4u;', 'uint32_t a=MAPMAKER_TABLE_CATEGORIES+i*24u;'),
    'group-not-full': (TABLE, '        if(c->count[cats[i]]>=c->max[cats[i]]) full=1;\n', ''),
    'turrets-two-types': (TABLE, 'rows[n++]=mapmaker_table_group("Turrets",c,mapmaker_table_turrets,3);',
                          'rows[n++]=mapmaker_table_group("Turrets",c,mapmaker_table_turrets,2);'),
    'story-ai-stock': (TABLE, 'MAPMAKER_TABLE_ROW("Story AI",story_ai,MAPMAKER_EDITOR_SAI_LIMIT,0u);',
                       'MAPMAKER_TABLE_ROW("Story AI",story_ai,MAPMAKER_EDITOR_SAI_STOCK,0u);'),
    'logic-wrong-count': (TABLE, 'WORD_AT(editor+0x280u),WORD_AT(editor+0x41E0u),rows);', 'WORD_AT(editor+0x280u),WORD_AT(editor+0x41D8u),rows);'),
    # Colours near and at the cap.
    'no-near': (TABLE, 'return (uint64_t)used*10u>=(uint64_t)max*9u?MAPMAKER_TABLE_STATE_NEAR:MAPMAKER_TABLE_STATE_OK;',
                'return MAPMAKER_TABLE_STATE_OK;'),
    'full-only-over': (TABLE, 'if(refused || used>=max) return MAPMAKER_TABLE_STATE_FULL;', 'if(refused || used>max) return MAPMAKER_TABLE_STATE_FULL;'),
    'refusal-ignored': (TABLE, 'rows[n].state=mapmaker_table_state((u),(m),(v->over&(bit))!=0);', 'rows[n].state=mapmaker_table_state((u),(m),0);'),
    'no-blink': (TABLE, '            MAPMAKER_TABLE_BLINK:MAPMAKER_TABLE_FULL;', '            MAPMAKER_TABLE_FULL:MAPMAKER_TABLE_FULL;'),
    # Drawing: replaces the meter, in its font and place, and leaves the game's state.
    'no-ret': (TABLE, '    restore_registers(&r);\n    MAPMAKER_GUEST_RET();', '    restore_registers(&r);'),
    'also-stock': (TABLE, '    save_registers(&r);\n    align=', '    original();\n    save_registers(&r);\n    align='),
    'font-not-restored': (TABLE, '    guest_call(sub_0003FA10,1,font);\n', ''),
    'align-not-restored': (TABLE, '    WORD_AT(MAPMAKER_TABLE_ALIGN_WORD)=align;\n', ''),
    'wrong-font': (TABLE, '#define MAPMAKER_TABLE_FONT 0x12u', '#define MAPMAKER_TABLE_FONT 0x13u'),
    'tight-rows': (TABLE, '#define MAPMAKER_TABLE_LINE 12.0f', '#define MAPMAKER_TABLE_LINE 8.0f'),
    'no-bottom-clamp': (TABLE, '    if(y+h>MAPMAKER_TABLE_CANVAS_H-MAPMAKER_TABLE_MARGIN) y=MAPMAKER_TABLE_CANVAS_H-MAPMAKER_TABLE_MARGIN-h;\n', ''),
    'no-right-clamp': (TABLE, '    if(x+w>MAPMAKER_TABLE_CANVAS_W-MAPMAKER_TABLE_MARGIN) x=MAPMAKER_TABLE_CANVAS_W-MAPMAKER_TABLE_MARGIN-w;\n', ''),
    'nan-position': (TABLE, '    if(!isfinite(x)) x=MAPMAKER_TABLE_MARGIN;\n', ''),
    # The stock meter where the table does not apply.
    'without-limits': (TABLE, '!mapmaker_limits.limits || ', ''),
    'before-check': (TABLE, '!mapmaker_limits_view.valid ||', '0 ||'),
    'probe-ignored': (TABLE, '(mapmaker_probe_flags()&MAPMAKER_PROBE_METER) || ', ''),
    'switch-ignored': (TABLE, "mapmaker_table.enabled=!(v && v[0]=='0' && !v[1]);", 'mapmaker_table.enabled=1;'),
    'no-buffer-check': (TABLE, '    if(!mapmaker_table.text) { mapmaker_probe_meter(original);return; }\n', ''),
}


def defines():
    """The editor/objects limits the table reads, from their own sources."""
    out = []
    for path, name in ((EDITOR, 'MAPMAKER_EDITOR_SAI_LIMIT'), (EDITOR, 'MAPMAKER_EDITOR_SAI_STOCK'),
                       (EDITOR, 'MAPMAKER_EDITOR_OPS_CAP'), (OBJECTS, 'MAPMAKER_OBJECTS_PICKUPS')):
        m = re.search(rf'^#define {name} (\d+)u', path.read_text(encoding='utf-8'), re.M)
        assert m, name
        out.append(f'{name}={m.group(1)}u')
    return out


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        path, old, new = MUTATIONS[mutation]
        text = path.read_text(encoding='utf-8')
        assert old in text, mutation
        (tmp_path / path.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('restable.exe' if os.name == 'nt' else 'restable')
    source = ROOT / 'tests/mapmaker/mapmaker_restable.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/wd4505',
                *('/D' + d for d in defines()),
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Wno-unused-function', '-D_POSIX_C_SOURCE=200809L',
                *('-D' + d for d in defines()),
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), str(source), '-o', str(exe), '-lm']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_restable(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_MAPMAKER')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker restable ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_restable_wiring():
    """The table owns the existing meter wrapper; no new guest wrapper."""
    guest = GUEST.read_text(encoding='utf-8')
    probe = (ROOT / 'src/interface/pc_mapmaker_probe.inc').read_text(encoding='utf-8')
    table = TABLE.read_text(encoding='utf-8')
    # MAPMAKER_RESTABLE before the probe; the table after limits, editor and objects.
    assert guest.index('#define MAPMAKER_RESTABLE 1') < guest.index('#include "pc_mapmaker_probe.inc"')
    for before in ('pc_mapmaker_limits.inc', 'pc_mapmaker_editor.inc', 'pc_mapmaker_objects.inc'):
        assert guest.index(f'#include "{before}"') < guest.index('#include "pc_mapmaker_restable.inc"')
    assert guest.index('#include "pc_mapmaker_restable.inc"') < guest.index('/* MapMaker capacity area includes (end) */')
    # One definition of the hook whichever way the probe is included.
    assert '#ifndef MAPMAKER_RESTABLE\nvoid pc_display_mapmaker_meter(void (*original)(void)) { mapmaker_probe_meter(original); }\n#endif' \
        in probe.replace('\r\n', '\n')
    assert 'void pc_display_mapmaker_meter(void (*original)(void))\n{' in table
    assert 'void pc_display_mapmaker_meter(void (*original)(void)) { original(); }' in guest  # #else stub
    # The probe patch still supplies the wrapper; nothing new to generate.
    patch = next(ROOT.glob('patches/**/*-mapmaker-meter-probe.patch')).read_text(encoding='utf-8')
    assert 'pc_display_mapmaker_meter(sub_0030C210_gen);' in patch
    wrappers = next(ROOT.glob('patches/**/*-mapmaker-meter-wrappers.patch')).read_text(encoding='utf-8')
    assert 'sub_0030C210' not in wrappers
    # Whole-chain fixtures link the two text functions.
    stubs = (ROOT / 'tests/mapmaker/mapmaker_area_stubs.h').read_text(encoding='utf-8')
    assert 'void sub_00041E30(void) {}' in stubs and 'void sub_0003FA10(void) {}' in stubs
    # The documented contract.
    doc = (ROOT / 'docs/mapmaker/mapmaker-restable.md').read_text(encoding='utf-8')
    for word in ('TSFP_MAPMAKER_TABLE', 'sub_0030C210', 'Engine objects', 'Pickups', 'Teleporters'):
        assert word in doc
