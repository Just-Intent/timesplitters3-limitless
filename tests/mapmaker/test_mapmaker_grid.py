"""Compile the MapMaker grid policy (src/interface/pc_mapmaker_grid.inc: seven floors,
80x80 grid, overlapping tiles, stacked items) against fake guest memory, with
mutation cases, and check config/guest-code-patches/grid.json against the
settings block the C side writes.

The table itself is verified against the real XBE by the lift (and by
logs/limits/check-tables.py); the decode checks below cover what it cannot
know: that every editor read of the grid constants this feature changes is
redirected, and only those."""
import json
import os
from pathlib import Path
import re
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
GRID = ROOT / 'src/interface/pc_mapmaker_grid.inc'
HEADER = ROOT / 'src/interface/pc_mapmaker_grid.h'
TABLE = ROOT / 'config/guest-code-patches/grid.json'

MUTATIONS = {
    'wide-always': ('mapmaker_grid.wide=mapmaker_limits.limits;', 'mapmaker_grid.wide=1;'),
    'overlap-default-on': ('mapmaker_grid.overlap=mapmaker_grid_switch("TSFP_MAPMAKER_OVERLAP");', 'mapmaker_grid.overlap=1;'),
    'stack-default-on': ('mapmaker_grid.stack=mapmaker_grid_switch("TSFP_MAPMAKER_STACK_ITEMS");', 'mapmaker_grid.stack=1;'),
    'levels-stock': ('#define MAPMAKER_GRID_WIDE_L 8 ', '#define MAPMAKER_GRID_WIDE_L 6 '),
    'width-stock': ('#define MAPMAKER_GRID_WIDE_W 80', '#define MAPMAKER_GRID_WIDE_W 40'),
    'flat-bound-only': ('if(x<0 || x>=w || y<0 || y>=w ||', 'if(x<0 || y<0 ||'),
    'top-floor-5': ('#define MAPMAKER_GRID_TOP_FLOOR 4', '#define MAPMAKER_GRID_TOP_FLOOR 5'),
    'floor-any-caller': ('WORD_AT(g_esp)==MAPMAKER_GRID_RET_FLOOR &&', ''),
    'floor-any-text': ('       WORD_AT(g_esp+4u)==0x1576u) {', '       1) {'),
    'floor-down-too': ('if(f>=2 && f<MAPMAKER_GRID_TOP_FLOOR) {', 'if(f<MAPMAKER_GRID_TOP_FLOOR) {'),
    'floor-no-below': ('                WORD_AT(editor+0x268u)=(uint32_t)f;\n', ''),
    'floor-refusal-sound': ('                WORD_AT(g_esp+4u)=0x157Du;', '                (void)0;'),
    'bar-hidden-wide': ('if(!mapmaker_grid.wide && (int32_t)g_ebx>2', 'if((int32_t)g_ebx>2'),
    'bar-hides-floor-2': ('(int32_t)g_ebx>2 && MAPMAKER_STACK_OK(0x20)', '(int32_t)g_ebx>1 && MAPMAKER_STACK_OK(0x20)'),
    'bar-any-caller': ('bar=MAPMAKER_STACK_OK(0x20) && WORD_AT(g_esp)==MAPMAKER_GRID_RET_BAR;', 'bar=MAPMAKER_STACK_OK(0x20);'),
    'guard-off-by-one': ('+2>=(int64_t)mapmaker_grid_l()', '+2>(int64_t)mapmaker_grid_l()'),
    'guard-bottom-only': ('(int64_t)z+h-1+2>=', '(int64_t)z+2>='),
    'guard-first-piece': ('            g_eax=0;\n            return;\n        }\n    }', '            g_eax=0;\n        }\n        return;\n    }'),
    'guard-any-scene': ('if(!g_eax || !(mapmaker_grid.wide || mapmaker_grid.overlap) || !mapmaker_grid_editor()) return;',
                        'if(!g_eax || !(mapmaker_grid.wide || mapmaker_grid.overlap)) return;'),
    'guard-off-in-overlap': ('if(!g_eax || !(mapmaker_grid.wide || mapmaker_grid.overlap) ||', 'if(!g_eax || !mapmaker_grid.wide ||'),
    'reregister-whole-piece': ('    if(x<r->box.x0 || x>r->box.x1 || y<r->box.y0 || y>r->box.y1 || level<r->box.l0 || level>r->box.l1) return;\n', ''),
    'reregister-removed-too': ('        if(p!=removed) mapmaker_grid_cells(p,mapmaker_grid_restore,&r);', '        mapmaker_grid_cells(p,mapmaker_grid_restore,&r);'),
    'reregister-stock-off': ('if(piece && (mapmaker_grid.wide || mapmaker_grid.overlap)) mapmaker_grid_reregister(piece);',
                             'if(piece && mapmaker_grid.wide) mapmaker_grid_reregister(piece);'),
    'reregister-first-wins': ('    WORD_AT(r->grid+i*4u)=piece;r->cells++;', '    if(!WORD_AT(r->grid+i*4u)) WORD_AT(r->grid+i*4u)=piece;r->cells++;'),
    'lookup-z-bias': ('                              z==INT32_MIN?-1:z+2);', '                              z==INT32_MIN?-1:z+3);'),
    'lookup-no-ret': ('        if(i!=UINT32_MAX) g_eax=WORD_AT(g+i*4u);\n    }\n    MAPMAKER_GUEST_RET();', '        if(i!=UINT32_MAX) g_eax=WORD_AT(g+i*4u);\n    }'),
    'insert-no-ret': ('mapmaker_grid_cells(WORD_AT(g_esp+4u),mapmaker_grid_put,&g);\n    MAPMAKER_GUEST_RET();', 'mapmaker_grid_cells(WORD_AT(g_esp+4u),mapmaker_grid_put,&g);'),
    'alloc-stock-size': ('    bytes=mapmaker_grid_bytes();\n', '    bytes=0x9600u;\n'),
    'alloc-not-cleared': ('if(g && guest_writable(g,bytes)) memset(', 'if(0) memset('),
    'reset-partial': ('memset((void *)((uintptr_t)g+g_xbox_mem_offset),0,mapmaker_grid_bytes());', 'memset((void *)((uintptr_t)g+g_xbox_mem_offset),0,0x9600u);'),
    'config-extent-40': ('w?(float)MAPMAKER_GRID_WIDE_W:', 'w?40.f:'),
    'config-floors-5': ('w?7.f:FLOAT_AT(0x004760CCu)', 'w?5.f:FLOAT_AT(0x004760CCu)'),
    'config-zoom-inv-stock': ('w?.01f:FLOAT_AT(0x00478394u)', 'w?1.f/60.f:FLOAT_AT(0x00478394u)'),
    'config-stock-literal': ('w?.01f:FLOAT_AT(0x00478394u)', 'w?.01f:1.f/61.f'),
    'probe-any-caller': ('    if(ret!=MAPMAKER_GRID_RET_PROBE || !g_eax) return;', '    if(!g_eax) return;'),
    'probe-any-scene': ('    if(!mapmaker_grid.overlap || !mapmaker_grid_editor()) return;', '    if(!mapmaker_grid.overlap) return;'),
    'stack-no-teleporters': ('        1,1,1,1,1,             /* 14-18 teleporters', '        1,1,1,1,0,             /* 14-18 teleporters'),
    'stack-no-teleporter-0': ('        1,1,1,1,1,             /* 14-18 teleporters', '        0,1,1,1,1,             /* 14-18 teleporters'),
    'stack-starts': ('        0,0,0,0, 0,0,0,0,      /* 0-3 starts', '        1,0,0,0, 0,0,0,0,      /* 0-3 starts'),
    'stack-cover-denied': ('        1,0,1,0,1,1};', '        1,0,0,0,1,1};'),
    'stack-pickup-caller': ('(ret!=MAPMAKER_STACK_RET_COUNT && ret!=MAPMAKER_STACK_RET_REPLACE) ||', ''),
    'stack-find-replaces': ('item added to an occupied cell\\n");\n    g_eax=0;\n}', 'item added to an occupied cell\\n");\n}'),
    'stack-find-double-ret': ('    original();\n    if(!g_eax || !mapmaker_grid.stack', '    original();MAPMAKER_GUEST_RET();\n    if(!g_eax || !mapmaker_grid.stack'),
    'stack-find-logs-empty': ('    if(!g_eax || !mapmaker_grid.stack ||', '    if(!mapmaker_grid.stack ||'),
    'stack-valid-off': ('    WORD_AT(editor+0x25Cu)|=4u;', '    (void)editor;'),
    'stack-valid-any-caller': ('    if(ret!=MAPMAKER_STACK_RET_VALID || !(g_eax&0xFFu)) return;', '    if(!(g_eax&0xFFu)) return;'),
    'stack-valid-refused-cell': ('    if(ret!=MAPMAKER_STACK_RET_VALID || !(g_eax&0xFFu)) return;', '    if(ret!=MAPMAKER_STACK_RET_VALID) return;'),
    'stack-valid-any-held': ('    if(!MAPMAKER_PTR_OK(editor,0x2F38) || !WORD_AT(editor+0x2F1Cu) ||\n       !mapmaker_stack_allowed((int32_t)WORD_AT(editor+0x2F34u))) return;',
                             '    if(!MAPMAKER_PTR_OK(editor,0x2F38) || !WORD_AT(editor+0x2F1Cu)) return;'),
    'stack-valid-no-item': ('    if(!MAPMAKER_PTR_OK(editor,0x2F38) || !WORD_AT(editor+0x2F1Cu) ||', '    if(!MAPMAKER_PTR_OK(editor,0x2F38) ||'),
    'stack-valid-any-scene': ('    if(!mapmaker_grid.stack || !mapmaker_grid_editor()) return;\n    editor=', '    if(!mapmaker_grid.stack) return;\n    editor='),
    'stack-valid-default-on': ('    if(!mapmaker_grid.stack || !mapmaker_grid_editor()) return;\n    editor=', '    if(!mapmaker_grid_editor()) return;\n    editor='),
    'stack-valid-clobbers-result': ('    WORD_AT(editor+0x25Cu)|=4u;', '    WORD_AT(editor+0x25Cu)|=4u;g_eax=0;'),
    'stack-no-restore': ('    for(i=0;i<n;i++) MAPMAKER_GRID_U16(hidden[i])=x;', ''),
    'stack-cell-any-held': ('(held=mapmaker_stack_held(editor))==MAPMAKER_STACK_HELD_NONE) { original();return; }',
                            '(held=mapmaker_stack_held(editor))==-3) { original();return; }'),
    'stack-hides-any-item': (' &&\n               mapmaker_stack_allowed((int32_t)WORD_AT(item))) {', ') {'),
    'stack-find-replaces-start': ('    if(MAPMAKER_PTR_OK(g_eax,4) && !mapmaker_stack_allowed((int32_t)WORD_AT(g_eax))) return;  /* a start etc. is replaced as stock */\n', ''),
    'sai-own-points': ('            if((int32_t)s==held) continue;\n', ''),
    'sai-points-not-hidden': ('                    MAPMAKER_GRID_U16(pt+4u)=MAPMAKER_STACK_HIDE;\n', ''),
    'sai-points-uncapped': ('            if(points>MAPMAKER_STACK_SAI_POINTS) points=MAPMAKER_STACK_SAI_POINTS;\n', ''),
    'sai-array-stock-offset': ('#define MAPMAKER_STACK_SAI 0x58390u', '#define MAPMAKER_STACK_SAI 0x288u'),
    'sai-array-round2-offset': ('#define MAPMAKER_STACK_SAI 0x58390u', '#define MAPMAKER_STACK_SAI 0x11950u'),
    'sai-not-held': ('    return sai>=0?sai:MAPMAKER_STACK_HELD_NONE;', '    return MAPMAKER_STACK_HELD_NONE;'),
    'sai-item-first': ('    if((int32_t)WORD_AT(editor+0x2F34u)>=0)\n', '    if(0)\n'),
    'sai-point-any-caller': ('ret!=MAPMAKER_STACK_RET_SAI_POINT || ', ''),
    'sai-point-empty-hands': ('mapmaker_stack_held(editor)<0) {', 'mapmaker_stack_held(editor)<-2) {'),
    'sai-newest-off': ('                    g_eax=(s<<16)|k;\n', ''),
    'sai-newest-negative-count': ('points=(int32_t)points<0?0u:points>', 'points=points>'),
    'sai-newest-item-held': ('        if(!args || (int32_t)WORD_AT(editor+0x2F34u)>=0) return;', '        if(!args) return;'),
    'sai-newest-past-count': ('        count=WORD_AT(editor+0x280u);\n', '        count=4u;\n'),
    'sai-newest-below-first': ('        for(s=count;s-->first+1u;) {', '        for(s=count;s-->0u;) {'),
    'sai-point-any-scene': ('RET_SAI_POINT || !mapmaker_grid.stack || !mapmaker_grid_editor()) return;',
                            'RET_SAI_POINT || !mapmaker_grid.stack) return;'),
    'sai-point-default-on': ('RET_SAI_POINT || !mapmaker_grid.stack || !mapmaker_grid_editor()) return;',
                             'RET_SAI_POINT || !mapmaker_grid_editor()) return;'),
    'sai-point-counts-empty': ('    if((int32_t)g_eax<0 || ret!=', '    if(ret!='),
    'stack-any-scene': ('    if(!mapmaker_grid.stack || !mapmaker_grid_editor() || !MAPMAKER_STACK_OK(16)) { original();return; }',
                        '    if(!mapmaker_grid.stack || !MAPMAKER_STACK_OK(16)) { original();return; }'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = GRID.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / GRID.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('grid.exe' if os.name == 'nt' else 'grid')
    source = ROOT / 'tests/mapmaker/mapmaker_grid.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/wd4127',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-o', str(exe), '-lm']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_grid(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_OVERLAP',
                        'TSFP_MAPMAKER_STACK_ITEMS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker grid ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


# ---- guest-code table ------------------------------------------------------
def config_addresses():
    text = HEADER.read_text(encoding='utf-8')
    base = int(re.search(r'#define MAPMAKER_GRID_CONFIG (0x[0-9A-F]+)u', text).group(1), 16)
    out = {}
    for name, off in re.findall(r'#define MAPMAKER_GRID_CFG_(\w+) \(MAPMAKER_GRID_CONFIG\+(0x[0-9A-F]+)u\)', text):
        out[name] = base + int(off, 16)
    return base, out


# site: (function, original constant, settings field)
REDIRECTS = {
    0x0030BEDD: (0x0030BE60, 0x004760CC, 'FLOORS'),
    0x002F8ED2: (0x002F8DE0, 0x004788B8, 'EXTENT'),
    0x002F9FBC: (0x002F9F70, 0x004788B8, 'EXTENT'),
    0x002F9FE3: (0x002F9F70, 0x004788B8, 'EXTENT'),
    0x0030A974: (0x0030A550, 0x004788B8, 'EXTENT'),
    0x0030AB90: (0x0030A550, 0x004788B8, 'EXTENT'),
    0x003075E2: (0x00306460, 0x00478394, 'ZOOM_INV'),
    0x0030764D: (0x00306460, 0x004760AC, 'ZOOM'),
}


def test_table_matches_settings_block():
    base, fields = config_addresses()
    assert set(fields) == {'FLOORS', 'EXTENT', 'ZOOM', 'ZOOM_INV'}
    # free extension range given to the feature work (actors end at 0x009034FF)
    assert 0x00903600 <= base and base + 0x10 <= 0x00920000
    patches = json.loads(TABLE.read_text(encoding='utf-8'))['patches']
    seen = set()
    for e in patches:
        a = int(e['address'], 16)
        assert e['kind'] == 'operand'
        if e['id'] == 'grid-floor-bars':
            assert a == 0x0030BF3F and e['original_hex'] == 'c744241805000000' and int(e['value'], 16) == 7
            continue
        func, old, field = REDIRECTS[a]
        assert int(e['function'], 16) == func, e['id']
        assert old.to_bytes(4, 'little').hex() in e['original_hex'], e['id']
        assert int(e['value'], 16) == fields[field], e['id']
        seen.add(a)
    assert seen == set(REDIRECTS)
    text = GRID.read_text(encoding='utf-8')
    # the stock values come from the very constants the table redirects away from
    for _, old, field in REDIRECTS.values():
        assert f'MAPMAKER_GRID_CFG_{field})=w?' in text
        assert f'FLOAT_AT(0x{old:08X}u)' in text


def test_redirects_cover_every_editor_read():
    """Every decoded read of 40.0 in the editor's grid code, and of the 5.0 /
    60.0 / 1/60 constants in the functions changed, is redirected; story and
    UI code keeps the image constants."""
    from mapmaker_decoded import decoded
    sites = {}
    for insn in decoded():
        for kind, value in insn.values():
            if kind == 'disp' and value in (0x004788B8, 0x004760CC, 0x004760AC, 0x00478394):
                sites.setdefault(value, []).append(insn)
    # editor code: 0x002F0000..0x0033FFFF (0x002E03A4/0x002E06DC are front-end layout)
    forty = {i.address for i in sites[0x004788B8] if 0x002F0000 <= i.address < 0x00340000}
    # 0x00334F82 (sub_00334EB0) scales a UI layout by 40.0, not the grid
    assert forty - {0x00334F82} == {a for a, r in REDIRECTS.items() if r[2] == 'EXTENT'}
    in_menu = {i.address for i in sites[0x004760CC] if 0x0030BE60 <= i.address < 0x0030C3C0}
    assert in_menu == {0x0030BEDD}
    zoom = {i.address for i in sites[0x004760AC] + sites[0x00478394] if 0x00306460 <= i.address < 0x00307C20}
    assert zoom == {0x003075E2, 0x0030764D}
