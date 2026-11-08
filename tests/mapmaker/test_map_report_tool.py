"""The offline MapMaker map checker (tools/testing/map_report.py): synthetic
anicemap.mkr / .tsmap files in the launcher's format, one problem each, and
the read-only and exit-status contract."""
import hashlib
import hmac
import importlib.util
import json
from pathlib import Path
import struct
import zlib

import pytest

ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location('map_report', ROOT / 'tools/testing/map_report.py')
tool = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(tool)


def chunk(n, data):
    return struct.pack('<II', 0x00C77600 + n, len(data)) + data


def piece(tile=0, x=0, z=0, floor=0, items=(), pads=2, mark=0):
    record = bytearray(0x28)
    struct.pack_into('<H', record, 0, tile)
    record[4:8] = bytes([x & 0xFF, z & 0xFF, floor & 0xFF, 0])
    struct.pack_into('<HHH', record, 0x1E - 0, len(items), 0, 0)
    struct.pack_into('<H', record, 0x24, 0x28 + pads * 8)
    struct.pack_into('<H', record, 0x26, mark)
    record += b'\x11' * (pads * 8)
    for k, item in enumerate(items):
        extra = k % 2
        sub = bytearray(0x18 + 4 * extra)
        struct.pack_into('<h', sub, 0xE, item)
        sub[0x14] = extra
        record += sub
    return bytes(record)


def ai(name, pads=(1001,), boss=0):
    data = bytearray(0x44 + 4 * len(pads))
    data[:len(name)] = name.encode()
    data[0x29] = boss
    struct.pack_into('<H', data, 0x2C, len(pads))
    for k, pad in enumerate(pads):
        struct.pack_into('<HBB', data, 0x44 + 4 * k, pad, 0, 0)
    return bytes(data)


def logic(kind, links):
    data = bytearray(0x8C)
    struct.pack_into('<I', data, 0x80, kind)
    struct.pack_into('<I', data, 0x20 if kind == 3 else 0x70, len(links))
    for k, idx in enumerate(links[:13]):
        struct.pack_into('<i', data, 0x24 + 8 * k, idx)
    return bytes(data)


def mkr(name='Test Lab', pieces=None, ais=(), logics=(), tileset=3, extra=b''):
    if pieces is None:
        pieces = [piece(tile=k % 45, x=k, items=(0x11,) if k == 0 else ()) for k in range(6)]
    info = bytearray(0x24C)
    struct.pack_into('<i', info, 4, len(pieces))
    struct.pack_into('<H', info, 0xA, tileset)
    info[0x62:0x62 + len(name)] = name.encode()
    body = chunk(0x68, bytes(info)) + chunk(0x69, b''.join(pieces))
    body += b''.join(chunk(0x6C, a) for a in ais) + b''.join(chunk(0x6F, entry) for entry in logics) + extra
    used = 8 + len(body) + 8
    end = struct.pack('<II', 0x00C7766A, used - 8)
    container = bytearray(struct.pack('<II', 0x00C77667, 0x12D) + body + end)
    struct.pack_into('<I', container, 16 + 0x248, used)
    container += struct.pack('<I', zlib.crc32(bytes(container)))
    tier = next(t for t in tool.TIERS if t >= len(container))
    container += b'\0' * (tier - len(container))
    return bytes(container) + hmac.new(bytes(16), bytes(container), hashlib.sha1).digest()


def tsmap(save, name='Test Lab'):
    meta = json.dumps({'format': 1, 'title_id': '45410066', 'name': name}).encode()
    content = b'TSFPMAP1' + struct.pack('<I', len(meta)) + meta + struct.pack('<I', len(save)) + save
    return content + hashlib.sha256(content).digest()


def run(save):
    body, used = tool.body_of(save)
    lines, problems = tool.check(body, used)
    return '\n'.join(lines), problems


def test_clean_map():
    text, problems = run(mkr(ais=[ai('Guard', (1001, 1004), boss=0xB1), ai('Twin', boss=0xB1), ai('Third', boss=0xB5)],
                             logics=[logic(3, [0, 5])]))
    assert problems == 0, text
    assert '"Test Lab" (map), 6 tiles, 1 items, 3 Story AI (3 bosses), 1 logic entries' in text
    assert 'no problems found' in text and 'past the original' not in text and 'near limit' not in text
    assert 'navigation: checked only in the game' in text


@pytest.mark.parametrize('case, expected', [
    (dict(ais=[ai('Ghost', ())]), 'problem: Story AI "Ghost" has no position'),
    (dict(ais=[ai('Lost', (0xFFFF,))]), 'problem: Story AI "Lost" point 1 is on no pad (pad id 65535)'),
    (dict(logics=[logic(3, [0, 6])]), 'problem: logic entry 1 (link 2) names tile #7, which is not on the map'),
    (dict(logics=[logic(0xA, [1, 7])]), 'names tile #7'),
    (dict(logics=[logic(3, list(range(14)))]), 'has 14 links; the game reads at most 13'),
    (dict(pieces=[piece(x=1, items=(0x0A,)), piece(x=2)]), 'problem: teleporter colour 1 has a single teleporter'),
    (dict(pieces=[piece(x=1, items=(0x0C, 0x0C, 0x0C))]), 'possible problem: teleporter colour 3 has an odd number (3)'),
    (dict(pieces=[piece(tile=3, x=4, z=2), piece(tile=9, x=4, z=2)]),
     'problem: tiles #1 (tile 4) and #2 (tile 10) are both at grid 4,2 floor 0 (overlap'),
    (dict(pieces=[piece(tile=45)]), 'tile id 45'),
    (dict(pieces=[piece(x=1, z=3, floor=2, items=(80,))]), 'an item on tile #1 at grid 1,3 floor 2 has id 80'),
    (dict(tileset=7), 'tileset 7 is unknown'),
    (dict(pieces=[piece(x=1, items=(0x2B,) * 5)]), 'problem: vehicles 5 is over the cap of 4'),
    (dict(extra=chunk(0x74, bytes(2001))), 'chunk 0x74 is 2001 bytes'),
])
def test_problems(case, expected):
    text, problems = run(mkr(**case))
    assert expected in text, text
    assert problems >= 1


def test_floor_and_open_tiles_and_stock_limits():
    pieces = [piece(tile=0, x=k % 40, z=k // 40, mark=0x5202 if k == 3 else 0) for k in range(230)]
    pieces.append(piece(tile=0, x=0, z=0, floor=1))                         # same cell, other floor: fine
    text, problems = run(mkr(pieces=pieces))
    assert problems == 0, text
    assert 'open tiles: 0 no roof, 1 open air (unbounded' in text
    assert "past the original game's limits (needs this build with Raised MapMaker limits): tiles 231/200" in text
    text, _ = run(mkr(pieces=[piece(x=k % 40, z=k // 40) for k in range(940)]))
    assert 'near limit: tiles 940/1023 (91%)' in text


def test_at_most_20_problem_lines():
    text, problems = run(mkr(ais=[ai(f'N{k}', ()) for k in range(25)]))
    assert problems == 25 and '25 problems' in text
    assert sum(1 for line in text.splitlines() if line.startswith('  problem: ')) == 20
    assert '  ... and 5 more not listed' in text


def test_files_and_exit_status(tmp_path, capsys):
    good, bad = mkr(), mkr(ais=[ai('Ghost', ())])
    saves = tmp_path / 'saves/UserData/45410066'
    for folder, data in (('AAAAAAAAAAAA', good), ('BBBBBBBBBBBB', bad)):
        (saves / folder).mkdir(parents=True)
        (saves / folder / 'anicemap.mkr').write_bytes(data)
    exported = tmp_path / 'Map.tsmap'
    exported.write_bytes(tsmap(good))
    before = {p: p.read_bytes() for p in tmp_path.rglob('*') if p.is_file()}
    assert tool.main([str(exported)]) == 0
    assert tool.main([str(saves / 'AAAAAAAAAAAA')]) == 0
    assert tool.main([str(tmp_path / 'saves')]) == 1
    out = capsys.readouterr().out
    assert 'Map.tsmap:' in out and 'BBBBBBBBBBBB' in out and 'problem: Story AI "Ghost"' in out
    damaged = bytearray(tsmap(good))
    damaged[40] ^= 1
    (tmp_path / 'Damaged.tsmap').write_bytes(bytes(damaged))
    assert tool.main([str(tmp_path / 'Damaged.tsmap')]) == 2
    assert 'checksum does not match' in capsys.readouterr().out
    crc = bytearray(good)
    crc[0x40] ^= 1
    (tmp_path / 'crc.mkr').write_bytes(bytes(crc))
    assert tool.main([str(tmp_path / 'crc.mkr')]) == 2
    assert {p: p.read_bytes() for p in tmp_path.rglob('*') if p.is_file() and p.name not in ('Damaged.tsmap', 'crc.mkr')} == before


def test_category_table_matches_the_game():
    """Item categories and stock maxima against the private XBE (skipped without it)."""
    import sys
    from mapmaker_decoded import _private
    xbe = _private() / 'game/default.xbe'
    recomp = _private().parent / 'external/ts3FPrecomp'
    if not xbe.is_file() or not (recomp / 'tools/recomp/config.py').is_file():
        pytest.skip('needs the private XBE and external/ts3FPrecomp')
    sys.path.insert(0, str(recomp))
    from tools.recomp import config
    config.configure_from_xbe(str(xbe))
    data = xbe.read_bytes()
    word = lambda va, fmt: struct.unpack_from(fmt, data, config.va_to_file_offset(va))[0]
    assert tuple(word(0x00524F58 + i * 0xC4 + 2, '<h') for i in range(80)) == tool.ITEM_CATEGORY
    assert tuple(word(0x00524BE0 + c * 24 + 4, '<i') for c in range(37)) == tool.CATEGORY_STOCK
    assert word(0x00475D30, '<f') == 6.0 and word(0x004760CC, '<f') == 5.0   # grid cell and floor height
