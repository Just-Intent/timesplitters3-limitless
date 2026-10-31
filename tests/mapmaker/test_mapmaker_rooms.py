"""MapMaker rooms 200 -> 241: C policy fixture with mutations, plus the guest-code table's shape."""
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
    assert len(by_value(0xFE)) == 98          # global room (+ light row width, list cap)
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
