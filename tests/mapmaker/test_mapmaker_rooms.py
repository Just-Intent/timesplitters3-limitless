"""MapMaker rooms 200 -> 255: C policy fixture with mutations, plus the guest-code table's shape."""
import json
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
ROOMS = ROOT / 'src/interface/pc_mapmaker_rooms.inc'
TABLE = ROOT / 'config/guest-code-patches/rooms.json'
SUB_RANGE = (0x008C0000, 0x008E0000)

MUTATIONS = {
    'cap-stock': ('    return MAPMAKER_ROOMS_MAX;\n}', '    return MAPMAKER_ROOMS_STOCK;\n}'),
    'cap-raised': ('#define MAPMAKER_ROOMS_MAX 255u', '#define MAPMAKER_ROOMS_MAX 256u'),
    'cap-241': ('#define MAPMAKER_ROOMS_MAX 255u', '#define MAPMAKER_ROOMS_MAX 241u'),
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
    # light rooms past 0xFF (stage 3)
    'light-link-byte': ('        g_eax=mapmaker_rooms_light.pending;\n', '        ;\n'),
    'light-unlink-byte-head': ('        if(!prev) {                      /* the original', '        if(0) {                      /* the original'),
    'light-unlink-keeps-id': ('        mapmaker_rooms_light_set(light,0);\n        mapmaker_rooms_light.unlinked++;',
                              '        mapmaker_rooms_light.unlinked++;'),
    'light-colour-byte': ('    if(!mapmaker_rooms_light_wide(light) || !guest_writable(light,0xB8u)) {',
                          '    if(1) {'),
    'light-colour-not-restored': ('    original();\n    *byte=keep;\n', '    original();\n'),
    'light-step-byte-group-kept': ('        WORD_AT(byte_group)=saved;\n', ''),
    'light-step-room-group-missing': ('            WORD_AT(MAPMAKER_ROOMS_LIGHT_GROUPS+g*4u)=(uint32_t)', '            (void)(uint32_t)'),
    'light-failed-create-forgets': ('    if(!light && picked) mapmaker_rooms_light_set(picked,room);', ''),
    'light-reset-keeps-ids': ('    memset(mapmaker_rooms_light.room,0,sizeof mapmaker_rooms_light.room);\n', ''),
    'light-reset-keeps-others': ('    memset(mapmaker_rooms_light.others,0,sizeof mapmaker_rooms_light.others);\n', ''),
    'light-pool-only': ('    if(!light) return NULL;\n    for(i=0;', '    return NULL;\n    for(i=0;'),
    'light-slot-unaligned': ('    if(off%MAPMAKER_ROOMS_LIGHT_STRIDE || off/', '    if(off/'),
    'light-slot-past-count': ('off/MAPMAKER_ROOMS_LIGHT_STRIDE>=n ||', '0 ||'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = ROOMS.read_text(encoding='utf-8')
        assert old in text, mutation
        (tmp_path / ROOMS.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('rooms.exe' if os.name == 'nt' else 'rooms')
    source = ROOT / 'tests/mapmaker/mapmaker_rooms.c'
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


ID_SPACE = 0x10D                      # room ids 0..0x10C: real 1..0xFF, spare 0x100, players, views, global
GLOBAL = 0x10C


def by_id(patches):
    return {p['id']: p for p in patches}


def op_at(ops, address):
    return next(p for p in ops if p['address'] == address)


def test_table_shape():
    patches = table()
    ids = [p['id'] for p in patches]
    assert len(ids) == len(set(ids)) and all(i.startswith('rooms-') for i in ids)
    ops = [p for p in patches if p['kind'] == 'operand']
    relocs = [p for p in patches if p['kind'] == 'relocate']
    # code entries: only the two stage-6 word loads (test_mapmaker_rooms_buckets.py decodes them)
    assert sorted(p['address'] for p in patches if p['kind'] not in ('operand', 'relocate')) == ['0x000864F4', '0x00086644']
    # special ids move by 0x37 to 0x100..0x10C (above every byte), nothing else in between
    specials = [p for p in ops if 'room special id' in p['reason']]
    for p in specials:
        old, new = p['reason'].split('room special id ')[1].split(' (')[0].split(' -> ')
        assert int(new, 16) - int(old, 16) == 0x37 and int(p['value'], 16) == int(new, 16), p
    by_value = lambda v: [p for p in specials if int(p['value'], 16) == v]
    assert len(by_value(GLOBAL)) == 98        # global room (+ light row width, list cap)
    assert len(by_value(0x101)) == 19 and len(by_value(0x105)) == 11 and len(by_value(0x106)) == 3
    assert len(by_value(ID_SPACE)) == 12      # id-space counts
    assert len(specials) == 98 + 19 + 11 + 3 + 12
    players = [p for p in ops if p['reason'].endswith('player index = room - first player id')]
    assert len(players) == 9 and {int(p['value'], 16) for p in players} == {(-0x101) & 0xFFFFFFFF}
    real = [p for p in ops if 'last real room 200 -> 255' in p['reason']]
    assert len(real) == 21 and all(int(p['value'], 16) == define('MAPMAKER_ROOMS_IDS_REAL') == 0xFF for p in real)
    assert define('MAPMAKER_ROOMS_MAX') <= define('MAPMAKER_ROOMS_IDS_REAL') < 0x100
    assert not [p for p in ops if '241' in p['reason'] or ' -> 0xFE' in p['reason'] or ' -> 0xF3' in p['reason']]
    # every new table lies in this area's sub-range, without overlap
    spans = sorted((int(r['new'], 16), int(r['new'], 16) + int(r['new_size'], 16), r['id']) for r in relocs)
    for lo, hi, rid in spans:
        assert SUB_RANGE[0] <= lo < hi <= SUB_RANGE[1], rid
    for (a0, a1, n0), (b0, b1, n1) in zip(spans, spans[1:]):
        assert a1 <= b0, (n0, n1)
    # the model table (moved by operand entries) and the query buffers fit around them
    model, model_end = define('MAPMAKER_ROOMS_MODEL_TABLE'), define('MAPMAKER_ROOMS_MODEL_TABLE') + 0xA08 * 2
    model_ops = [p for p in ops if 'm-0x78b860' in p['reason']]
    assert model_ops and all(model <= int(p['value'], 16) < model_end for p in model_ops)
    query = define('MAPMAKER_ROOMS_QUERY_BUFFER')
    query_end = query + define('MAPMAKER_ROOMS_QUERY_BYTES') * define('MAPMAKER_ROOMS_QUERY_LEVELS')
    for lo, hi, rid in spans:
        assert hi <= model or lo >= model_end, rid
    for lo, hi, rid in spans + [(model, model_end, 'model')]:
        assert hi <= query or lo >= query_end, rid
    assert SUB_RANGE[0] <= query and query_end <= SUB_RANGE[1]
    # the view array grows by the shadow at +0x6000 and is cleared in full
    alloc = op_at(ops, '0x000C61D3')
    clear = op_at(ops, '0x000C61E7')
    assert int(alloc['value'], 16) == 0x6000 + 6 * 0xFFC + 8 == 4 * int(clear['value'], 16)
    # portal flags: frame and buffer grown together, and the C limit leaves the flag for portal 0
    frame = op_at(ops, '0x0008A786')
    flags = op_at(ops, '0x0008ADF0')
    assert int(frame['value'], 16) - 0x1270 == 4 * int(flags['value'], 16) == define('MAPMAKER_ROOMS_PORTALS') + 1


def id_space_problems(patches):
    """Stage 4: every table and count indexed by a room id covers ids 0..0x10C (0x10D); [] when right."""
    out = []

    def need(cond, why):
        if not cond:
            out.append(why)
    ops = [p for p in patches if p['kind'] == 'operand']
    r = by_id(patches)
    v = lambda a: int(op_at(ops, a)['value'], 16)
    # per-id prop tables (step 0x440): p1/p4 plus the directly named global entry 0x10C
    for t, glob in (('rooms-reloc-p1-0x7b54e0', 'rooms-reloc-p1-global'), ('rooms-reloc-p4-0x7b5f00', 'rooms-reloc-p4-global')):
        base, size = int(r[t]['new'], 16), int(r[t]['new_size'], 16)
        need(size == GLOBAL * 4 and int(r[glob]['new'], 16) == base + GLOBAL * 4 and int(r[glob]['new_size'], 16) == 8, t)
    for t in ('rooms-reloc-p2-0x7b5840', 'rooms-reloc-p3-0x7b5ba0', 'rooms-reloc-p5-0x7b6260'):
        need(int(r[t]['new_size'], 16) >= ID_SPACE * 4, t)
    bases = [int(r[t]['new'], 16) for t in ('rooms-reloc-p1-0x7b54e0', 'rooms-reloc-p2-0x7b5840', 'rooms-reloc-p3-0x7b5ba0',
                                            'rooms-reloc-p4-0x7b5f00', 'rooms-reloc-p5-0x7b6260')]
    need(all(b - a >= ID_SPACE * 4 for a, b in zip(bases, bases[1:])), 'per-id table step')
    need(v('0x0009E4E6') == ID_SPACE * 4 and v('0x000A1ED5') == GLOBAL * 4, 'per-id loop ends')
    # per-id flag bytes [0x7B977C]: allocation, clear (dwords + a stosw) and the special displacements
    need(v('0x0008A6FD') == v('0x0008BA63') == 0x110 and ID_SPACE <= 4 * v('0x00089734') + 2 <= 0x110, 'id flag bytes')
    need((v('0x00089746'), v('0x00089756'), v('0x00089776')) == (GLOBAL, 0x101, 0x106), 'id flag displacements')
    # per-id drawn flag (stage 2)
    need(int(r['rooms-reloc-f-0x7b5180']['new_size'], 16) >= ID_SPACE * 4, 'drawn flag table')
    # light heads [3][0x10C]: base, the clear loop (count, stride, start at row 0's last entry, end)
    heads, row = define('MAPMAKER_ROOMS_LIGHT_HEADS'), define('MAPMAKER_ROOMS_LIGHT_ROW')
    head, rest = r['rooms-reloc-l1-head'], r['rooms-reloc-l1-rest']
    need(row == GLOBAL and int(head['new'], 16) == heads, 'light head base / row')
    need(int(rest['new'], 16) == heads + int(head['new_size'], 16), 'light head pieces')
    need(int(rest['new'], 16) + int(rest['new_size'], 16) == heads + 3 * row * 4, 'light head size')
    start = int(rest['new'], 16) + (0x0073C6A0 - int(rest['old'], 16))
    need(start == heads + (row - 1) * 4 and v('0x00112272') == row, 'light head clear start / count')
    need(v('0x0011227F') == row * 4 and v('0x00112285') == start + 3 * row * 4, 'light head clear stride / end')
    # room light colour float[0x10C][3] (its fill loop runs from +4 while below the end)
    need(int(r['rooms-reloc-l2-0x7ac880']['new_size'], 16) == GLOBAL * 12 + 4, 'light colour')
    # light round robin room/16 covers the highest special id
    need(int(r['rooms-reloc-lb-0x73d910']['new_size'], 16) // 4 > GLOBAL // 16, 'round robin')
    # view shadow per view (0xFFC apart): rects 0x10D x 12, flags 0x110, list 0x10D words
    flag_ops = [p for p in ops if 'view per-room visible flag' in p['reason']]
    special_flags = [p for p in ops if 'view flag of special id' in p['reason']]
    list_ops = [p for p in ops if 'view sorted room list' in p['reason']]
    rect_ops = [p for p in ops if 'view per-room rect field' in p['reason'] or 'view rect' in p['reason']]
    flags_at = min(int(p['value'], 16) for p in flag_ops)
    list_at = v('0x000899A8')
    need({int(p['value'], 16) for p in rect_ops} <= set(range(0x6000, 0x600D)) | {0x1801}, 'rects at +0x6000')
    need(0x6000 + ID_SPACE * 12 <= flags_at and flags_at + 0x110 <= list_at, 'shadow flags')
    need(list_at + ID_SPACE * 2 <= 0x6000 + 0xFFC, 'shadow list below the next view')
    need(all(int(p['value'], 16) == list_at for p in list_ops if p['address'] not in ('0x00160A90', '0x0025F2E7')), 'list sites')
    need(v('0x00160A90') == list_at - 0xE08 and v('0x0025F2E7') == list_at - 0xE0A, 'list from its counts')
    for p in flag_ops:                     # real rooms keep their offset from the flags' base (fixed rooms 0x25..0x36)
        need(int(p['value'], 16) - flags_at in (0, 0x25, 0x35, 0x36), p['address'])
    for p in special_flags:                # mov [esi+0xD2C+stock id] -> flags + new id
        stock = int(p['reason'].split('special id ')[1].split()[0], 16)
        need(int(p['value'], 16) == flags_at + stock + 0x37, p['address'])
    need(len(special_flags) == 12 and v('0x000899CA') == GLOBAL, 'special flags / list cap')
    need(5 * 0xFFC + list_at + ID_SPACE * 2 <= v('0x000C61D3'), 'view array allocation')
    need(list_at + ID_SPACE * 2 <= int(r['rooms-reloc-v-0x749890']['new_size'], 16), 'front-end static view')
    # model table: room models 0x908..0x907+255 (0xA06), 0x18-byte per-model array, bounds
    need(v('0x002427C3') * 2 == 0xA08 > 0x907 + define('MAPMAKER_ROOMS_IDS_REAL'), 'model table clear')
    need(all(v(a) == 0xA08 for a in ('0x000A5E4E', '0x001B59B8', '0x001B5C09', '0x001B618E')), 'model bounds')
    need(v('0x0008392D') == 0xA08 * 0x18 and v('0x00083947') * 4 == v('0x0008392D'), 'per-model array')
    need(define('MAPMAKER_ROOMS_MODEL_TABLE') + 0xA08 * 2 <= int(r['rooms-reloc-lb-0x73d910']['new'], 16), 'model table end')
    return out


def _mutate(entry_id, **fields):
    return [dict(p, **fields) if p['id'] == entry_id or p.get('address') == entry_id else p for p in table()]


ID_SPACE_MUTATIONS = {
    'p2-256-entries': lambda: _mutate('rooms-reloc-p2-0x7b5840', new_size='0x400'),
    'p1-global-stale': lambda: _mutate('rooms-reloc-p1-global', new='0x008C73F8'),
    'p-step-0x400': lambda: _mutate('rooms-reloc-p3-0x7b5ba0', new='0x008C7800'),
    'loop-end-0xFF': lambda: _mutate('0x0009E4E6', value='0x3FC'),
    'id-flags-clear-short': lambda: _mutate('0x00089734', value='0x40'),
    'id-flag-global-stale': lambda: _mutate('0x00089746', value='0xFE'),
    'drawn-flag-small': lambda: _mutate('rooms-reloc-f-0x7b5180', new_size='0x400'),
    'light-head-start': lambda: _mutate('rooms-reloc-l1-head', new_size='0xD0'),
    'light-head-stride': lambda: _mutate('0x0011227F', value='0x3F8'),
    'light-head-end': lambda: _mutate('0x00112285', value='0x8C93DC'),
    'light-colour-small': lambda: _mutate('rooms-reloc-l2-0x7ac880', new_size='0xBEC'),
    'round-robin-16': lambda: _mutate('rooms-reloc-lb-0x73d910', new_size='0x40'),
    'shadow-flags-stock': lambda: _mutate('0x000F7F54', value='0x6C00'),
    'shadow-special-flag': lambda: _mutate('0x000CC9FB', value='0x6CFE'),
    'shadow-list-stock': lambda: _mutate('0x00116F4B', value='0x6D00'),
    'shadow-list-count': lambda: _mutate('0x00160A90', value='0x5EF8'),
    'list-cap': lambda: _mutate('0x000899CA', value='0xFE'),
    'model-bound': lambda: _mutate('0x001B5C09', value='0xA00'),
    'model-array': lambda: _mutate('0x0008392D', value='0xF000'),
}


def test_id_space_tables():
    assert not id_space_problems(table())


@pytest.mark.parametrize('mutation', sorted(ID_SPACE_MUTATIONS))
def test_id_space_mutations_fail(mutation):
    assert id_space_problems(ID_SPACE_MUTATIONS[mutation]()), mutation


def c_define(path, name):
    for line in (ROOT / path).read_text(encoding='utf-8').splitlines():
        if line.startswith(f'#define {name} '):
            return int(line.split()[2].rstrip('u'), 0)
    raise AssertionError(name)


def test_other_areas_use_the_moved_tables():
    """The objects pool clear and the probe read the per-id tables where rooms.json puts them."""
    r = by_id(table())
    new = lambda t: int(r[t]['new'], 16)
    objects = (ROOT / 'src/interface/pc_mapmaker_objects.inc').read_text(encoding='utf-8')
    for t in ('rooms-reloc-p1-0x7b54e0', 'rooms-reloc-p2-0x7b5840', 'rooms-reloc-p5-0x7b6260'):
        assert f'    mapmaker_objects_clear(0x{new(t):08X}u,0x{ID_SPACE:X}u*4u);' in objects, t
    assert objects.count('mapmaker_objects_clear(0x008C') == 3
    probe = 'src/interface/pc_mapmaker_probe.inc'
    assert c_define(probe, 'MAPMAKER_PROBE_ROOM_COUNTS') == new('rooms-reloc-p1-0x7b54e0')
    assert c_define(probe, 'MAPMAKER_PROBE_ROOM_LISTS') == new('rooms-reloc-p4-0x7b5f00')
    assert c_define(probe, 'MAPMAKER_PROBE_ROOM_IDS') == ID_SPACE and c_define(probe, 'MAPMAKER_PROBE_MODEL_ROOMS_END') == 0xA08
    assert c_define('src/interface/pc_mapmaker_skymask.inc', 'MAPMAKER_SKYMASK_TABLE') == new('rooms-reloc-t3-0x703460')
    assert c_define('src/interface/pc_mapmaker_roof.inc', 'MAPMAKER_ROOF_ROOMS') == 256 > define('MAPMAKER_ROOMS_IDS_REAL')
    assert '#define OPENVIEW_RECT 0x6000u' in (ROOT / 'src/interface/pc_mapmaker_openview.inc').read_text(encoding='utf-8')


def rooms_regions():
    """Every block of the rooms sub-range: relocated tables, model table, query buffers."""
    regions = [(int(r['new'], 16), int(r['new'], 16) + int(r['new_size'], 16), r['id'])
               for r in table() if r['kind'] == 'relocate']
    model = define('MAPMAKER_ROOMS_MODEL_TABLE')
    query = define('MAPMAKER_ROOMS_QUERY_BUFFER')
    regions += [(model, model + 0xA08 * 2, 'model table'),
                (query, query + define('MAPMAKER_ROOMS_QUERY_BYTES') * define('MAPMAKER_ROOMS_QUERY_LEVELS'), 'query')]
    return sorted(regions)


def test_extension_map_rooms_sub_range():
    """No other user of 0x008C0000..0x008E0000 in tracked code: the gaps the 255 layout uses are free."""
    import re
    regions = rooms_regions()
    for (a0, a1, n0), (b0, b1, n1) in zip(regions, regions[1:]):
        assert a1 <= b0, (n0, n1)
    inside = lambda x: any(lo <= x < hi for lo, hi, _ in regions)
    # constants of other areas (C sources and their guest-code tables) never point into the rooms blocks' gaps
    found = []
    for path in sorted((ROOT / 'src').rglob('*')):
        if path.suffix not in ('.c', '.h', '.inc') or path.name == 'pc_mapmaker_rooms.inc':
            continue
        for m in re.finditer(r'0x00(8[CDcd][0-9A-Fa-f]{4})', path.read_text(encoding='utf-8', errors='replace')):
            found.append((int(m.group(1), 16), path.name))
    for path in sorted((ROOT / 'config/guest-code-patches').glob('*.json')):
        if path.name in ('rooms.json', '00-header.json'):
            continue
        for e in json.loads(path.read_text(encoding='utf-8'))['patches']:
            for key in ('value', 'new'):
                if isinstance(e.get(key), str):
                    found.append((int(e[key], 16), path.name))
    stray = [(hex(x), n) for x, n in found if 0x008C0000 <= x < 0x008E0000 and not inside(x)]
    assert not stray, stray
    # the rooms area's own operand values into the sub-range are inside its blocks (the head clear end is a bound)
    for p in table():
        if p['kind'] == 'operand' and 0x008C0000 <= int(p['value'], 16) < 0x008E0000:
            assert inside(int(p['value'], 16)) or p['address'] == '0x00112285', p
    # the colour fill writes entries 0..0x10B from +4: inside the table it is declared with
    colour = by_id(table())['rooms-reloc-l2-0x7ac880']
    assert 12 * GLOBAL <= int(colour['new_size'], 16)          # last entry: +4+12*0x10B-4 .. +12*0x10C
    gaps = [(a1, b0) for (a0, a1, _), (b0, b1, _) in zip(regions, regions[1:]) if b0 > a1]
    assert regions[0][0] == 0x008C0000 and regions[-1][1] == 0x008D4000
    assert sum(b - a for a, b in gaps) < 0x2000, [(hex(a), hex(b)) for a, b in gaps]


def test_header_extension_range():
    header = json.loads((ROOT / 'config/guest-code-patches/00-header.json').read_text(encoding='utf-8'))
    va, size = int(header['extension']['va'], 16), int(header['extension']['size'], 16)
    assert va <= SUB_RANGE[0] and SUB_RANGE[1] <= va + size
    assert '0x008C0000 rooms' in header['extension']['note']


# Every immediate in the game's code (not the XDK libraries) that could be a room
# special id or the 200-room bound, reviewed by hand: patched in rooms.json, or one of
# these (stack frames, struct strides, sound/text/menu/event ids, model ids, jump-table
# bytes decoded as code). A site in neither list fails: 0x000BBA05 was missed once
# because the text listing elides 11-byte instructions, and a story level crashed.
NOT_ROOM = {int(a, 16) for a in """
FF0F2
2098A 27774 277FB 278C0 29261 292DF 2937B 2940F 29470 294BB 2DE83 30D70 361FF 36206 36386 3761D
37626 3F050 3F2D2 59D0E 59EDF 5A2CE 5A30E 5BC89 5BDBE 5BF4A 5FD39 5FD50 6275B 9116F 91209 92A10
92CA0 92F27 93690 93920 93BA7 96410 964E2 965DA 98413 9F860 9F866 A1B80 A1D96 B1F9F C01CA C3C50
C3E4C C3E58 CFB56 CFB91 CFBA7 CFD82 CFE21 CFE8E CFEEA CFEFC D39F2 DB106 DB115 DB124 DB133 F2C3E
FCE1E FEEC2 1099B7 109B91 109C54 109D1A 109DE0 109EC2 10A1FE 11DA03 11E5B0 121F50 122F04 12B81F
12B8BF 13596D 135979 135987 1364C4 1364EA 138590 138F4F 138F69 146E6F 146E8C 14DA46 14DA55
157CD0 157CEB 158079 1580C4 163F10 163FF1 163FFE 166E70 1671C2 1671CF 16D380 16DA85 17AEE0
17AF42 17BAEB 17BB03 17C4E2 195B7F 1962DC 1972C0 19787B 199CE6 19A26A 19FC73 19FCD1 19FCEF
1A1694 1A16B4 1A5D9D 1A5E83 1A5EBA 1A5EEC 1A5F4D 1A6033 1A606A 1A609C 1B38A2 1BF774 1BF7CC
1C4E93 1C51FC 1C530D 1C56B8 1C5EAF 1C5EB9 1C5EC3 1C650E 1C6552 1C6765 1C6A17 1C719B 1C752B
1C8042 1C9D08 1C9D22 1C9D3B 1C9D58 1C9D72 1C9D8B 1C9DA5 1C9DC2 1C9DDB 1C9DF5 1C9E0F 1C9E2B
1C9E45 1C9E5F 1CBD4E 1CBDC2 1D7BCE 1D7BD9 1DE6E7 1E1710 1E3A4A 1F2C40 1F3353 20162E 20F2B0
20F69D 214DD8 215B56 22435C 225AF8 226F6F 22CCF8 22CE20 2310D2 234EE1 23CABA 23DC45 23DC8F
23DCD9 23DD23 23DD67 23E51C 2540B9 25445D 254679 254683 256229 2574A0 2575C0 2578C1 25DDA8
298D50 298F52 2A0D24 2A0E34 2C6955 2CD7D0 2CDFC0 2D0F24 2D1B69 2E72E0 2E7935 2EDDC0 2EE225
2EE2B8 2EE2CD 2FAEE0 303466 309F10 31FE7A 322C70 322CA0 32322C 323321 32F8E5 32F96C 32F9B6
330DB2 330DD3 33A907 349F35 350C48 353536 35DD13 377C2F 37C57E 391966 39A9A1 39A9C3 3AA826
3AA8DA 3AAB7C 3AABDC 3B06FA 3B0797 3B07DD 3B08BF 3B090A 3B1509 3B1528 3B1C29 3B1C31 3B1FE7
3B1FF1 3B1FFB 3B3497 3BB7CE 3BB831 3BFDB7 3CFA42 3CFBC2 3D0C25 3D0C29 3D0C2D 3D0C31 3D0C35
3D0C39 3D0C3D 3D0C41 3D0C45 3D0C49
""".split()}
GAME_CODE_END = 0x003D3380            # .text; D3D, XGRPH, XONLINE, ... follow


def candidate_sites():
    import sys
    private = ROOT / 'private'
    if not (private / 'game/default.xbe').is_file() and ROOT.parent.name == 'worktrees':
        private = ROOT.parents[2] / 'private'
    xbe = private / 'game/default.xbe'
    functions = private / 'pipeline/disasm/functions.json'
    if not (xbe.is_file() and functions.is_file()):
        pytest.skip('needs the private XBE and its disassembly')
    sys.path.insert(0, str(ROOT / 'tools/compiler'))
    import translation_layout as config
    from translation_decode import Disassembler
    from guest_code_patches import _decode
    config.configure_from_xbe(str(xbe))
    data = xbe.read_bytes()
    specials = set(range(0xC9, 0xD7))
    found = {}
    dis = Disassembler()
    for f in json.loads(functions.read_text(encoding='utf-8')):
        if 'end' not in f:
            continue
        f['_addr'], f['end'] = int(f['start'], 16), int(f['end'], 16)
        if f['_addr'] >= GAME_CODE_END or not config.is_code_address(f['_addr']):
            continue
        for insn in _decode(dis, data, f):
            if insn.is_branch or insn.is_call:
                continue
            for op in insn.operands:
                if op.type == 'imm':
                    v = op.imm & 0xFFFFFFFF
                    if v in specials or (-v) & 0xFFFFFFFF in specials or (v == 0xC8 and insn.mnemonic == 'cmp'):
                        found[insn.address] = f'{insn.mnemonic} {insn.op_str}'
                elif (op.type == 'mem' and insn.mnemonic == 'lea' and isinstance(op.mem_disp, int)
                      and -op.mem_disp in specials and 'esp' not in insn.op_str and 'ebp' not in insn.op_str):
                    found[insn.address] = f'{insn.mnemonic} {insn.op_str}'
    return found


def test_every_room_constant_is_classified():
    patched = {int(p['address'], 16) for p in table() if p['kind'] == 'operand'}
    found = candidate_sites()
    unclassified = {a: s for a, s in found.items() if a not in patched and a not in NOT_ROOM}
    assert not unclassified, {f'0x{a:08X}': s for a, s in sorted(unclassified.items())}
    assert 0x000BBA05 in patched and 0x000BBA05 not in NOT_ROOM
