"""MapMaker rooms 200 -> 241: C policy fixture with mutations, plus the guest-code table's shape."""
import json
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]
ROOMS = ROOT / 'src/pc_mapmaker_rooms.inc'
TABLE = ROOT / 'config/guest-code-patches/rooms.json'
SUB_RANGE = (0x008C0000, 0x008E0000)

MUTATIONS = {
    'cap-stock': ('    return MAPMAKER_ROOMS_MAX;\n}', '    return MAPMAKER_ROOMS_STOCK;\n}'),
    'cap-raised': ('#define MAPMAKER_ROOMS_MAX 241u', '#define MAPMAKER_ROOMS_MAX 242u'),
    'query-flags-kept': ('&=0xEFu; /* clear "listed" */', '&=0xFFu;'),
    'query-ignores-box-bit': ('if(word&take) {', 'if(take) {'),
    'query-ignores-max': ('if(++found==max) take=0;', '++found;'),
    'query-no-depth-guard': ('if(mapmaker_rooms.query_depth>=MAPMAKER_ROOMS_QUERY_LEVELS) { original(); return; }', ''),
    'query-one-buffer': ('+mapmaker_rooms.query_depth*MAPMAKER_ROOMS_QUERY_BYTES;', ';'),
    'query-wrong-box-end': ('box,box+0xCu,0x00087260u', 'box,box+0x10u,0x00087260u'),
    'totals-not-reset': ('mapmaker_rooms.rooms=mapmaker_rooms.portals=mapmaker_rooms.pieces=mapmaker_rooms.dropped=0;',
                         'mapmaker_rooms.pieces=mapmaker_rooms.dropped=0;'),
    'drop-keeps-records': ('WORD_AT(entry+0x18u)=0;WORD_AT(entry+0x24u)=0;', 'WORD_AT(entry+0x18u)=0;'),
    'no-portal-limit': ('portals>MAPMAKER_ROOMS_PORTALS-mapmaker_rooms.portals', '0'),
    'guard-outside-compile': ('if(!mapmaker_rooms.compiling || !guest_writable(entry,0x2Cu)) return;',
                              'if(!guest_writable(entry,0x2Cu)) return;'),
    'props-any-scene': ('WORD_AT(0x0079094Cu)!=100u) return;', 'WORD_AT(0x0079094Cu)==0xFFFFu) return;'),
    'props-partial-table': ('if(!mapmaker_rooms_slot_ok(model)) return;          /* not the MapMaker table */', ''),
    'props-keep-geometry': ('WORD_AT(slot+4u)=0;WORD_AT(slot+8u)=0;', ';'),
    'props-count-not-raised': ('WORD_AT(0x00791D4Cu)=fresh;WORD_AT(0x0075EAA8u)=n;', 'WORD_AT(0x00791D4Cu)=fresh;'),
    'props-replace-defined': ('MAPMAKER_ROOMS_MODEL_TABLE+model*2u)>=0) continue;', 'MAPMAKER_ROOMS_MODEL_TABLE+model*2u)<-1) continue;'),
    'props-wrong-template': ('(first+MAPMAKER_ROOMS_STOCK-1u)*2u', '(first)*2u'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = ROOMS.read_text(encoding='utf-8')
        assert old in text, mutation
        (tmp_path / ROOMS.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('rooms.exe' if os.name == 'nt' else 'rooms')
    source = ROOT / 'tests/mapmaker_rooms.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_rooms(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker rooms ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def table():
    return json.loads(TABLE.read_text(encoding='utf-8'))['patches']


def define(name):
    for line in ROOMS.read_text(encoding='utf-8').splitlines():
        if line.startswith(f'#define {name} '):
            return int(line.split()[2].rstrip('u'), 0)
    raise AssertionError(name)


def test_table_shape():
    patches = table()
    ids = [p['id'] for p in patches]
    assert len(ids) == len(set(ids)) and all(i.startswith('rooms-') for i in ids)
    ops = [p for p in patches if p['kind'] == 'operand']
    relocs = [p for p in patches if p['kind'] == 'relocate']
    assert not [p for p in patches if p['kind'] not in ('operand', 'relocate')]
    # special ids move by 0x29 to the top of the byte range, nothing else in between
    specials = [p for p in ops if 'room special id' in p['reason']]
    for p in specials:
        old, new = p['reason'].split('room special id ')[1].split(' (')[0].split(' -> ')
        assert int(new, 16) - int(old, 16) == 0x29 and int(p['value'], 16) == int(new, 16), p
    by_value = lambda v: [p for p in specials if int(p['value'], 16) == v]
    assert len(by_value(0xFE)) == 97          # global room (+ light row width, list cap)
    assert len(by_value(0xF3)) == 19 and len(by_value(0xF7)) == 11 and len(by_value(0xF8)) == 3
    assert len(by_value(0xFF)) == 12          # id-space counts
    real = [p for p in ops if 'last real room 200 -> 241' in p['reason']]
    assert len(real) == 21 and all(int(p['value'], 16) == define('MAPMAKER_ROOMS_MAX') for p in real)
    # every new table lies in this area's sub-range, without overlap
    spans = sorted((int(r['new'], 16), int(r['new'], 16) + int(r['new_size'], 16), r['id']) for r in relocs)
    for lo, hi, rid in spans:
        assert SUB_RANGE[0] <= lo < hi <= SUB_RANGE[1], rid
    for (a0, a1, n0), (b0, b1, n1) in zip(spans, spans[1:]):
        assert a1 <= b0, (n0, n1)
    # the model table (moved by operand entries) and the query buffers fit around them
    model = define('MAPMAKER_ROOMS_MODEL_TABLE')
    model_ops = [p for p in ops if 'm-0x78b860' in p['reason']]
    assert model_ops and all(model <= int(p['value'], 16) < model + 0x1400 for p in model_ops)
    query = define('MAPMAKER_ROOMS_QUERY_BUFFER')
    query_end = query + define('MAPMAKER_ROOMS_QUERY_BYTES') * define('MAPMAKER_ROOMS_QUERY_LEVELS')
    for lo, hi, rid in spans + [(model, model + 0x1400, 'model')]:
        assert hi <= query or lo >= query_end, rid
    assert SUB_RANGE[0] <= query and query_end <= SUB_RANGE[1]
    # the view array grows by the shadow at +0x6000 and is cleared in full
    alloc = next(p for p in ops if p['address'] == '0x000C61D3')
    clear = next(p for p in ops if p['address'] == '0x000C61E7')
    assert int(alloc['value'], 16) == 0x6000 + 6 * 0xFFC + 8 == 4 * int(clear['value'], 16)
    # portal flags: frame and buffer grown together, and the C limit leaves the flag for portal 0
    frame = next(p for p in ops if p['address'] == '0x0008A786')
    flags = next(p for p in ops if p['address'] == '0x0008ADF0')
    assert int(frame['value'], 16) - 0x1270 == 4 * int(flags['value'], 16) == define('MAPMAKER_ROOMS_PORTALS') + 1


def test_header_extension_range():
    header = json.loads((ROOT / 'config/guest-code-patches/00-header.json').read_text(encoding='utf-8'))
    va, size = int(header['extension']['va'], 16), int(header['extension']['size'], 16)
    assert va <= SUB_RANGE[0] and SUB_RANGE[1] <= va + size
    assert '0x008C0000 rooms' in header['extension']['note']
