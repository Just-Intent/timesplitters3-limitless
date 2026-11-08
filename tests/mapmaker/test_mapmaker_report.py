"""Compile the MapMaker map report (src/interface/pc_mapmaker_report.inc) with the
validator that calls it against fake guest memory, with mutation cases, and
check its caps and hook placement against the rest of the source."""
import os
from pathlib import Path
import re
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'src/interface/pc_mapmaker_report.inc'
VALIDATE = ROOT / 'src/interface/pc_mapmaker_validate.inc'

MUTATIONS = {
    'problems-21': ('#define MAPMAKER_REPORT_PROBLEMS 20u', '#define MAPMAKER_REPORT_PROBLEMS 21u'),
    'near-80': ('#define MAPMAKER_REPORT_NEAR 90u', '#define MAPMAKER_REPORT_NEAR 80u'),
    'near-at-cap-only': ('else if((unsigned long long)used*100u>(unsigned long long)cap*MAPMAKER_REPORT_NEAR && ', 'else if(0 && '),
    'over-not-problem': ('if(used>cap) mapmaker_report_problem(0,', 'if(0) mapmaker_report_problem(0,'),
    'stock-caps-ignored': ('on?MAPMAKER_REPORT_CAP_PIECES:MAPMAKER_REPORT_STOCK_PIECES', 'MAPMAKER_REPORT_CAP_PIECES'),
    'raised-caps-ignored': ('on?MAPMAKER_REPORT_CAP_PIECES:MAPMAKER_REPORT_STOCK_PIECES', 'MAPMAKER_REPORT_STOCK_PIECES'),
    'category-caps-ignored': ('            mapmaker_report_cap(what,mapmaker_report.category[i],max,nearby,sizeof nearby);\n', ''),
    'items-from-zero': ('#define MAPMAKER_REPORT_FIRST_ITEM 10u', '#define MAPMAKER_REPORT_FIRST_ITEM 0u'),
    'pads-length-ignored': ('uint32_t length=mapmaker_report_u16(r+0x24u),', 'uint32_t length=MAPMAKER_REPORT_RECORD+0u*mapmaker_report_u16(r+0x24u),'),
    'item-size-fixed': ('bytes=(MAPMAKER_REPORT_B(at+0x14u)*4u+0x1Bu)&~3u;', 'bytes=0x18u;'),
    'no-link-flag': ('if((WORD_AT(l)&MAPMAKER_REPORT_LINK_GROUPS) && a<pads && b<pads)', 'if(a<pads && b<pads)'),
    'link-bound': ('if((WORD_AT(l)&MAPMAKER_REPORT_LINK_GROUPS) && a<pads && b<pads)', 'if((WORD_AT(l)&MAPMAKER_REPORT_LINK_GROUPS) && a<pads)'),
    'no-group-join': ('if(first[g]<0) first[g]=(int32_t)i; else mapmaker_report_join((uint32_t)first[g],i);', 'first[g]=(int32_t)i;'),
    'main-is-first': ('    mapmaker_report.main=best;', '    mapmaker_report.main=mapmaker_report_root(0);'),
    'islands-4': ('#define MAPMAKER_REPORT_ISLANDS 3u', '#define MAPMAKER_REPORT_ISLANDS 4u'),
    'pad-unverified': ('if(h>=mapmaker_report.pads || WORD_AT(mapmaker_report.pool+h*0x20u)!=id) return -1;', 'if(h>=mapmaker_report.pads) return -1;'),
    'high-ids-unknown': ('else if(id>=MAPMAKER_REPORT_HIGH_ID && id<MAPMAKER_REPORT_HIGH_END) h=', 'else if(0) h='),
    'cover-not-traced': ('    if(!mapmaker_report.cover_pool) return -2;', '    return -2;'),
    'cover-unverified': ('if(h>=mapmaker_report.cover || WORD_AT(mapmaker_report.cover_pool+h*0x20u)!=id) return -1;', 'if(h>=mapmaker_report.cover) return -1;'),
    'cover-as-regular': ('if(id<MAPMAKER_REPORT_COVER_ID || id>=MAPMAKER_REPORT_HIGH_ID) return mapmaker_report_regular(id);', 'return mapmaker_report_regular(id);'),
    'near-small-caps': ('&& cap>=MAPMAKER_REPORT_NEAR_MIN) {', ') {'),
    'patrol-unchecked': ('} else if(mapmaker_report_root((uint32_t)h)!=mapmaker_report_root((uint32_t)start)) {', '} else if(0) {'),
    'unplaced-unchecked': ('        if(!mapmaker_report.ai[i].points)\n', '        if(0)\n'),
    'boss-is-problem': ("if((b&0xF8u)==0xB0u) mapmaker_report.bosses++;",
                        "if((b&0xF8u)==0xB0u) { mapmaker_report.bosses++;mapmaker_report_problem(0,\"boss\"); }"),
    'logic-one-based-3': ('if(type==3u) { n=WORD_AT(d+0x20u);base=0; }', 'if(type==3u) { n=WORD_AT(d+0x20u);base=1; }'),
    'logic-zero-based-A': ('else if(type==0xAu) { n=WORD_AT(d+0x70u);base=1; }', 'else if(type==0xAu) { n=WORD_AT(d+0x70u);base=0; }'),
    'logic-unchecked': ('    mapmaker_report_scan(container,span,1);\n', ''),
    'teleport-single': ('if(n==1u) mapmaker_report_problem(0,', 'if(0) mapmaker_report_problem(0,'),
    'overlap-env-ignored': ('            if(!mapmaker_report.overlap)\n', '            if(1)\n'),
    'heap-unsigned': ('if(heap<0) mapmaker_report_problem(0,', 'if(0) mapmaker_report_problem(0,'),
    'hidden-ignored': ('    if(hidden) mapmaker_report_problem(0,', '    if(0) mapmaker_report_problem(0,'),
    'writes-guest': ('    mapmaker_report_nav();\n    nearby[0]=0;', '    mapmaker_report_nav();\n    WORD_AT(MAPMAKER_REPORT_COVER_COUNT)++;\n    nearby[0]=0;'),
    'floor-truncated': ('*f=(int)floorf(fy/MAPMAKER_REPORT_FLOOR+0.5f);', '*f=(int)(fy/MAPMAKER_REPORT_FLOOR);'),
}
VALIDATE_MUTATIONS = {
    'report-after-restore': (
        '    original();\n#ifdef PC_MAPMAKER_REPORT_INC\n    /* The map report (pc_mapmaker_report.inc) reads the map while the\n'
        '     * refused chunks are still hidden. */\n    mapmaker_report_level(container,span,problems);\n#else\n    (void)problems;\n#endif\n'
        '    while(n--) if(WORD_AT(edits[n].address)==edits[n].value) WORD_AT(edits[n].address)=edits[n].old;\n',
        '    original();\n    while(n--) if(WORD_AT(edits[n].address)==edits[n].value) WORD_AT(edits[n].address)=edits[n].old;\n'
        '    mapmaker_report_level(container,span,problems);\n'),
    'report-no-hidden-count': ('    mapmaker_report_level(container,span,problems);', '    mapmaker_report_level(container,span,0u);'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    for source, table in ((SOURCE, MUTATIONS), (VALIDATE, VALIDATE_MUTATIONS)):
        if mutation in table:
            old, new = table[mutation]
            text = source.read_text(encoding='utf-8')
            assert old in text, mutation
            (tmp_path / source.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('report.exe' if os.name == 'nt' else 'report')
    source = ROOT / 'tests/mapmaker/mapmaker_report.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), str(source), '-o', str(exe), '-lm']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS, *VALIDATE_MUTATIONS])
def test_mapmaker_report(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_')}
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=120, env=env)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr[-4000:]
        assert 'mapmaker report ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def _define(path, name):
    for line in path.read_text(encoding='utf-8').splitlines():
        m = re.match(rf'#define {name} (\S+)', line)
        if m:
            value = m.group(1).rstrip('u')
            return int(value, 0) if re.fullmatch(r'(0x[0-9A-Fa-f]+|\d+)', value) else value
    raise AssertionError(name)


def test_caps_match_the_limits():
    interface = ROOT / 'src/interface'
    limits, editor, pads = interface / 'pc_mapmaker_limits.inc', interface / 'pc_mapmaker_editor.inc', interface / 'pc_mapmaker_pads.inc'
    pairs = {
        'MAPMAKER_REPORT_CAP_PIECES': (limits, 'MAPMAKER_LIMIT_PIECES'),
        'MAPMAKER_REPORT_CAP_ITEMS': (limits, 'MAPMAKER_LIMIT_SUB'),
        'MAPMAKER_REPORT_CAP_OBJECTS': (limits, 'MAPMAKER_LIMIT_OBJECTS'),
        'MAPMAKER_REPORT_CAP_SAI': (editor, 'MAPMAKER_EDITOR_SAI_LIMIT'),
        'MAPMAKER_REPORT_STOCK_SAI': (editor, 'MAPMAKER_EDITOR_SAI_STOCK'),
        'MAPMAKER_REPORT_CAP_LOGIC': (editor, 'MAPMAKER_EDITOR_RECORDS_CAP'),
        'MAPMAKER_REPORT_POOL_PADS': (limits, 'MAPMAKER_POOL_PADS'),
        'MAPMAKER_REPORT_POOL_LINKS': (limits, 'MAPMAKER_POOL_LINKS'),
        'MAPMAKER_REPORT_POOL_GROUPS': (limits, 'MAPMAKER_LIMIT_GROUPS'),
        'MAPMAKER_REPORT_POOL_COVER': (pads, 'MAPMAKER_PADS_COVER_RANGE'),
        'MAPMAKER_REPORT_REGULAR_ID': (pads, 'MAPMAKER_PADS_REGULAR_ID'),
        'MAPMAKER_REPORT_COVER_ID': (pads, 'MAPMAKER_PADS_COVER_ID'),
        'MAPMAKER_REPORT_HIGH_END': (pads, 'MAPMAKER_PADS_HIGH_END'),
        'MAPMAKER_REPORT_LOW_PADS': (pads, 'MAPMAKER_PADS_LOW_PADS'),
        'MAPMAKER_REPORT_SPAN': (VALIDATE, 'MAPMAKER_VALIDATE_SPAN'),
    }
    for ours, (path, theirs) in pairs.items():
        assert _define(SOURCE, ours) == _define(path, theirs), (ours, theirs)
    # high ids start right after the cover range
    assert _define(SOURCE, 'MAPMAKER_REPORT_HIGH_ID') == _define(pads, 'MAPMAKER_PADS_COVER_ID') + _define(pads, 'MAPMAKER_PADS_COVER_RANGE')
    reserve = _define(limits, 'MAPMAKER_OBJECTS_CHARACTERS') + _define(limits, 'MAPMAKER_OBJECTS_EFFECTS')
    assert _define(SOURCE, 'MAPMAKER_REPORT_OBJECT_RESERVE') == reserve


def test_report_is_wired_into_level_setup():
    guest = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    assert guest.index('#include "pc_mapmaker_limits.inc"') < guest.index('#include "pc_mapmaker_report.inc"') \
        < guest.index('#include "pc_mapmaker_validate.inc"')
    validate = VALIDATE.read_text(encoding='utf-8')
    hook = validate[validate.index('void pc_mapmaker_validate_level('):]
    assert hook.index('original();') < hook.index('mapmaker_report_level(container,span,problems);') \
        < hook.index('while(n--)')
    # the report never writes guest memory: no WORD_AT/B store in it
    text = SOURCE.read_text(encoding='utf-8')
    assert not re.search(r'(WORD_AT|MAPMAKER_REPORT_B)\([^;]*\)\s*(\+|-|\||&)?=(?!=)', text)


def test_tool_matches_the_report():
    """The offline checker (tools/testing/map_report.py) uses the same caps and record layout."""
    import importlib.util
    spec = importlib.util.spec_from_file_location('map_report', ROOT / 'tools/testing/map_report.py')
    tool = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(tool)
    assert tool.CAPS_RAISED == {'tiles': _define(SOURCE, 'MAPMAKER_REPORT_CAP_PIECES'),
                                'items': _define(SOURCE, 'MAPMAKER_REPORT_CAP_ITEMS'),
                                'Story AI': _define(SOURCE, 'MAPMAKER_REPORT_CAP_SAI'),
                                'logic entries': _define(SOURCE, 'MAPMAKER_REPORT_CAP_LOGIC')}
    assert tool.CAPS_STOCK == {'tiles': _define(SOURCE, 'MAPMAKER_REPORT_STOCK_PIECES'),
                               'items': _define(SOURCE, 'MAPMAKER_REPORT_STOCK_ITEMS'),
                               'Story AI': _define(SOURCE, 'MAPMAKER_REPORT_STOCK_SAI'),
                               'logic entries': _define(SOURCE, 'MAPMAKER_REPORT_STOCK_LOGIC')}
    assert tool.OBJECTS_CAP == _define(SOURCE, 'MAPMAKER_REPORT_CAP_OBJECTS')
    assert tool.OBJECT_RESERVE == _define(SOURCE, 'MAPMAKER_REPORT_OBJECT_RESERVE')
    assert tool.MAX_PROBLEMS == _define(SOURCE, 'MAPMAKER_REPORT_PROBLEMS')
    assert tool.NEAR_PERCENT == _define(SOURCE, 'MAPMAKER_REPORT_NEAR')
    assert tool.NEAR_MIN == _define(SOURCE, 'MAPMAKER_REPORT_NEAR_MIN')
    assert tool.FIRST_ITEM == _define(SOURCE, 'MAPMAKER_REPORT_FIRST_ITEM')
    assert (tool.TELEPORT_FIRST, tool.TELEPORT_LAST) == (_define(SOURCE, 'MAPMAKER_REPORT_TELEPORT_FIRST'),
                                                         _define(SOURCE, 'MAPMAKER_REPORT_TELEPORT_LAST'))
    names = dict(re.findall(r'\[(\d+)\]="([^"]+)"', SOURCE.read_text(encoding='utf-8')))
    assert {str(k): v for k, v in tool.CATEGORY_NAMES.items()} == names
    # raised category maxima follow pc_mapmaker_limits.inc's table
    limits = (ROOT / 'src/interface/pc_mapmaker_limits.inc').read_text(encoding='utf-8')
    for address, stock, raised in re.findall(r'\{0x([0-9A-F]{8})u,(\d+)u,(\d+)u\}', limits):
        category = (int(address, 16) - 0x00524BE0 - 4) // 24
        assert tool.CATEGORY_STOCK[category] == int(stock), category
        assert tool.CATEGORY_RAISED.get(category, tool.CATEGORY_STOCK[category]) == int(raised), category
