"""Check a saved MapMaker map without starting the game.

Reads a portable .tsmap file, an anicemap.mkr save file, a save directory
holding one, or a whole save folder (every UserData/45410066/<map> below it),
and prints the same kind of report the game logs at level start
(src/interface/pc_mapmaker_report.inc, docs/mapmaker/mapmaker-report.md):
counts against caps and the problems that can be found from the file alone.

The game's report also checks navigation (pads, links, separate areas and
Story AI reachability); pads come from the tile templates inside the game,
so this offline check cannot do that part.

    python tools/testing/map_report.py <file-or-folder> [...]

Exit status: 0 no problems, 1 problems found, 2 a file could not be read.
Never writes anything.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib

# Shared with the in-game report (tests/mapmaker/test_mapmaker_report.py checks them).
CAPS_RAISED = {'tiles': 1023, 'items': 500, 'Story AI': 511, 'logic entries': 400}
CAPS_STOCK = {'tiles': 200, 'items': 50, 'Story AI': 50, 'logic entries': 100}
OBJECTS_CAP, OBJECT_RESERVE = 2047, 128
MAX_PROBLEMS, NEAR_PERCENT, NEAR_MIN, FIRST_ITEM = 20, 90, 10, 10
TELEPORT_FIRST, TELEPORT_LAST = 14, 18
CATEGORY_NAMES = {
    8: 'weapons', 9: 'ammo', 10: 'red keycards', 11: 'blue keycards', 12: 'yellow keycards', 13: 'green keycards',
    14: 'teleporters (colour 1)', 15: 'teleporters (colour 2)', 16: 'teleporters (colour 3)', 17: 'teleporters (colour 4)',
    18: 'teleporters (colour 5)', 19: 'health', 20: 'armour', 21: 'power-ups', 25: 'movers', 26: 'props',
    27: 'turrets (type 1)', 28: 'turrets (type 2)', 29: 'turrets (type 3)', 31: 'special pickups', 32: 'markers',
    33: 'cover props', 35: 'vehicles',
}
# Item id -> category (catalogue 0x00524F58 + id * 0xC4 + 2, ids 0..79) and the
# category maxima (0x00524BE0 + category * 24 + 4) of the US game; raised
# values as pc_mapmaker_limits.inc applies them with TSFP_MAPMAKER_LIMITS=1.
ITEM_CATEGORY = (0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 14, 15, 16, 17, 18, 4, 5, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 10, 11, 12,
                 13, 22, 23, 19, 19, 20, 20, 21, 21, 21, 21, 35, 33, 33, 33, 33, 33, 33, 29, 29, 29, 27, 27, 28, 30, 30,
                 30, 30, 30, 30, 30, 30, 30, 30, 31, 25, 6, 32, 34, 34, 36, 26, 26, 26, 26, 31, 31, 31)
CATEGORY_STOCK = (32, 32, 16, 1, 1, 1, 4, 1, 32, 32, 2, 2, 2, 2, 2, 2, 2, 2, 2, 16, 16, 12, 1, 16, 50, 20, 20, 8, 8, 8,
                  10, 32, 20, 66, 66, 4, 4)
CATEGORY_RAISED = {8: 128, 9: 128, 10: 16, 11: 16, 12: 16, 13: 16, 14: 8, 15: 8, 16: 8, 17: 8, 18: 8, 19: 64, 20: 64,
                   21: 48, 25: 160, 26: 160, 27: 24, 28: 24, 29: 24, 31: 128, 32: 160, 33: 132, 36: 8}

MAGIC, VERSION, END = 0x00C77667, 0x12D, 0x00C7766A
SIGNATURE = 20
TIERS = (0x7FE0, 0xFFE0, 0x1FFE0, 0x3FFE0, 0x7FFE0)
TILES, ITEM_IDS, TILESETS = 45, 80, 7
TITLE_FOLDER = 'UserData/45410066'


def tag(n):
    return 0x00C77600 + n


class MapFileError(Exception):
    pass


def unpack_tsmap(data):
    if len(data) <= 48 or data[:8] != b'TSFPMAP1':
        raise MapFileError('not a .tsmap file')
    if hashlib.sha256(data[:-32]).digest() != data[-32:]:
        raise MapFileError('the .tsmap checksum does not match (damaged file)')
    header = struct.unpack_from('<I', data, 8)[0]
    if not 0 < header <= 65536 or 12 + header + 4 > len(data) - 32:
        raise MapFileError('damaged .tsmap header')
    meta = json.loads(data[12:12 + header].decode('utf-8'))
    size = struct.unpack_from('<I', data, 12 + header)[0]
    if 16 + header + size + 32 != len(data):
        raise MapFileError('damaged .tsmap length')
    return meta, data[16 + header:16 + header + size]


def body_of(save):
    """The map container of an anicemap.mkr file (body + signature)."""
    body = len(save) - SIGNATURE
    if body not in TIERS:
        raise MapFileError(f'the file size ({len(save)} bytes) is not a MapMaker size tier')
    if struct.unpack_from('<II', save, 0) != (MAGIC, VERSION):
        raise MapFileError('not a supported MapMaker map (unknown header or version)')
    used = struct.unpack_from('<I', save, 16 + 0x248)[0]
    if not 24 <= used <= body - 4:
        raise MapFileError("the map's stored size does not fit its file")
    if struct.unpack_from('<I', save, used)[0] != zlib.crc32(save[:used]):
        raise MapFileError("the map's CRC check failed; the file is damaged")
    return save[:body], used


class Report:
    def __init__(self):
        self.problems = []
        self.near = []
        self.info = []

    def problem(self, text, heuristic=False):
        self.problems.append(('possible problem: ' if heuristic else 'problem: ') + text)

    def cap(self, what, used, cap):
        if used > cap:
            self.problem(f'{what} {used} is over the cap of {cap}')
        elif used * 100 > cap * NEAR_PERCENT and cap >= NEAR_MIN:
            self.near.append(f'{what} {used}/{cap} ({used * 100 // cap}%)')


def u16(b, o):
    return b[o] | b[o + 1] << 8


def s8(v):
    return v - 256 if v > 127 else v


def scan(body, used, report):
    """Walk the chunks as the loaders do; collect what the report needs."""
    m = {'name': '', 'tileset': None, 'pieces': [], 'items': 0, 'category': [0] * len(CATEGORY_STOCK), 'ai': [],
         'logic': [], 'open': [0, 0, 0, 0], 'header_pieces': None}
    offset, end = 8, used
    for _ in range(0x40000):
        if offset + 8 > end:
            report.problem('the chunk list ends without an end marker')
            break
        t, size = struct.unpack_from('<II', body, offset)
        d = offset + 8
        if t == END:
            break
        if size > end - d:
            report.problem(f'a chunk at byte {offset} is larger than the map')
            break
        chunk = body[d:d + size]
        if t == tag(0x68) and size >= 0x62 + 32:
            name = chunk[0x62:0x62 + 32].split(b'\0', 1)[0]
            m['name'] = name.decode('ascii', 'replace')
            m['header_pieces'] = struct.unpack_from('<i', chunk, 4)[0]
            m['tileset'] = u16(chunk, 0xA)
        elif t == tag(0x69):
            pieces(chunk, m, report)
        elif t == tag(0x6C) and size >= 0x44:
            count = u16(chunk, 0x2C)
            if count > 8:
                report.problem(f'Story AI #{len(m["ai"]) + 1} has {count} points; the game reads at most 8')
            count = min(count, 8, (size - 0x44) // 4)
            name = chunk[:32].split(b'\0', 1)[0].decode('ascii', 'replace')
            m['ai'].append({'name': name, 'boss': chunk[0x29],
                            'pads': [u16(chunk, 0x44 + 4 * k) for k in range(count)]})
        elif t == tag(0x6F) and size >= 0x8C:
            m['logic'].append(chunk)
        elif t in (tag(0x73), tag(0x74), tag(0x75)):
            limit = {tag(0x73): 0x4B00, tag(0x74): 2000, tag(0x75): 3000}[t]
            if size > limit:
                report.problem(f'chunk 0x{t & 0xFF:02X} is {size} bytes; the editor reads at most {limit} (refused at load)')
        offset = d + size
    return m


def pieces(chunk, m, report):
    r, end = 0, len(chunk)
    while end - r >= 0x28:
        length, items, mark = u16(chunk, r + 0x24), u16(chunk, r + 0x1E), u16(chunk, r + 0x26)
        if length < 0x28 or length > end - r:
            report.problem(f'tile record #{len(m["pieces"]) + 1} is damaged; later tiles are not checked')
            return
        tile = u16(chunk, r)
        mode = mark & 0xFF if (mark & 0xFF00) == 0x5200 and (mark & 0xFF) <= 3 else 0
        piece = {'index': len(m['pieces']), 'tile': tile, 'x': s8(chunk[r + 4]), 'z': s8(chunk[r + 5]),
                 'floor': s8(chunk[r + 6]), 'mode': mode}
        m['pieces'].append(piece)
        m['open'][mode] += 1
        if tile >= TILES:
            report.problem(f'tile #{piece["index"] + 1} has tile id {tile}; the game knows {TILES} (refused at load)')
        at = r + length
        for _ in range(items):
            if end - at < 0x18:
                report.problem(f'the items of tile #{piece["index"] + 1} are damaged')
                return
            size = (chunk[at + 0x14] * 4 + 0x1B) & ~3
            if size > end - at:
                report.problem(f'the items of tile #{piece["index"] + 1} are damaged')
                return
            item = struct.unpack_from('<h', chunk, at + 0xE)[0]
            if not 0 <= item < ITEM_IDS:
                report.problem(f'an item on tile #{piece["index"] + 1} at grid {piece["x"]},{piece["z"]} floor '
                               f'{piece["floor"]} has id {item}; the game knows {ITEM_IDS} (refused at load)')
            else:
                if item >= FIRST_ITEM:
                    m['items'] += 1
                m['category'][ITEM_CATEGORY[item]] += 1
            at += size
        r = at


def check(body, used, label='map'):
    report = Report()
    m = scan(body, used, report)
    total = len(m['pieces'])
    ai, logic = m['ai'], m['logic']
    bosses = sum(1 for a in ai if a['boss'] & 0xF8 in (0xB0, 0xB4))
    if m['tileset'] is None:
        report.problem('the map information chunk is missing')
    elif m['tileset'] >= TILESETS:
        report.problem(f'tileset {m["tileset"]} is unknown (refused at load)')
    if m['header_pieces'] is not None and m['header_pieces'] > total:
        report.problem(f'the map says {m["header_pieces"]} tiles but holds {total} (refused at load)')
    # 1. caps (this build, raised limits) and the original game's limits
    counts = {'tiles': total, 'items': m['items'], 'Story AI': len(ai), 'logic entries': len(logic)}
    for what, value in counts.items():
        report.cap(what, value, CAPS_RAISED[what])
    report.cap('engine objects (estimate)', total + m['items'] + OBJECT_RESERVE, OBJECTS_CAP)
    stock = [f'{what} {value}/{CAPS_STOCK[what]}' for what, value in counts.items() if value > CAPS_STOCK[what]]
    for c, n in enumerate(m['category']):
        if not n:
            continue
        what = CATEGORY_NAMES.get(c, f'category {c} items')
        report.cap(what, n, CATEGORY_RAISED.get(c, CATEGORY_STOCK[c]))
        if n > CATEGORY_STOCK[c]:
            stock.append(f'{what} {n}/{CATEGORY_STOCK[c]}')
    # 3. Story AI without a position / on no pad (pad 0xFFFF; other ids need the game's pad tables)
    for a in ai:
        if not a['pads']:
            report.problem(f'Story AI "{a["name"]}" has no position (shown with ! in the editor list): it is not placed')
        for k, pad in enumerate(a['pads']):
            if pad == 0xFFFF:
                report.problem(f'Story AI "{a["name"]}" point {k + 1} is on no pad (pad id 65535): move it in the editor')
    # 4. logic entries naming tiles past the placed ones (the saver's "not found")
    for record, chunk in enumerate(logic):
        kind = struct.unpack_from('<I', chunk, 0x80)[0]
        if kind == 3:
            n, base, most = struct.unpack_from('<i', chunk, 0x20)[0], 0, 13
        elif kind == 0xA:
            n, base, most = struct.unpack_from('<i', chunk, 0x70)[0], 1, 10
        else:
            continue
        if n > most:
            report.problem(f'logic entry {record + 1} has {n} links; the game reads at most {most} (refused at load)')
        for k in range(max(0, min(n, 13))):
            idx = struct.unpack_from('<i', chunk, 0x24 + 8 * k)[0] - base
            if idx >= total:
                report.problem(f'logic entry {record + 1} (link {k + 1}) names tile #{idx + 1}, which is not on the map (deleted?)')
    # 6. teleporters
    for c in range(TELEPORT_FIRST, TELEPORT_LAST + 1):
        n = m['category'][c]
        if n == 1:
            report.problem(f'teleporter colour {c - TELEPORT_FIRST + 1} has a single teleporter: it needs a partner of the same colour')
        elif n % 2:
            report.problem(f'teleporter colour {c - TELEPORT_FIRST + 1} has an odd number ({n}): one is left without a partner', True)
    # 7. overlapping tiles (the editor allows them only with OVERLAP)
    seen = {}
    for p in m['pieces']:
        key = (p['x'], p['z'], p['floor'])
        if key in seen:
            a = seen[key]
            report.problem(f'tiles #{a["index"] + 1} (tile {a["tile"] + 1}) and #{p["index"] + 1} (tile {p["tile"] + 1}) are both '
                           f'at grid {p["x"]},{p["z"]} floor {p["floor"]} (overlap; fine only if you placed it with OVERLAP)')
        else:
            seen[key] = p

    lines = [f'"{m["name"] or "unnamed"}" ({label}), {total} tiles, {m["items"]} items, {len(ai)} Story AI '
             f'({bosses} boss{"" if bosses == 1 else "es"}), {len(logic)} logic entries']
    lines.append('navigation: checked only in the game (pads come from the tile templates)')
    if any(m['open'][1:]):
        lines.append(f'open tiles: {m["open"][1]} no roof, {m["open"][2]} open air (unbounded: falling off the map kills), '
                     f'{m["open"][3]} open air (bounded)')
    if stock:
        lines.append('past the original game\'s limits (needs this build with Raised MapMaker limits): ' + ', '.join(stock))
    if report.near:
        lines.append('near limit: ' + ', '.join(report.near))
    if not report.problems:
        lines.append('no problems found')
    else:
        n = len(report.problems)
        lines.append(f'{n} problem{"" if n == 1 else "s"} ("possible" = a heuristic check):')
        lines += ['  ' + p for p in report.problems[:MAX_PROBLEMS]]
        if n > MAX_PROBLEMS:
            lines.append(f'  ... and {n - MAX_PROBLEMS} more not listed')
    lines.append('end')
    return lines, len(report.problems)


def report_file(path):
    data = Path(path).read_bytes()
    if data[:8] == b'TSFPMAP1':
        _, data = unpack_tsmap(data)
    body, used = body_of(data)
    return check(body, used, Path(path).name)


def files(target):
    target = Path(target)
    if target.is_file():
        return [target]
    if (target / 'anicemap.mkr').is_file():
        return [target / 'anicemap.mkr']
    title = target / TITLE_FOLDER if (target / TITLE_FOLDER).is_dir() else target
    found = sorted(title.glob('*/anicemap.mkr')) + sorted(target.glob('*.tsmap'))
    return found


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('paths', nargs='+', help='.tsmap or anicemap.mkr file, a save directory or a save folder')
    args = parser.parse_args(argv)
    status = 0
    for target in args.paths:
        found = files(target)
        if not found:
            print(f'{target}: no MapMaker map found')
            status = 2
        for path in found:
            try:
                lines, problems = report_file(path)
            except (OSError, MapFileError, ValueError, struct.error) as error:
                print(f'{path}: cannot be read: {error}')
                status = 2
                continue
            print(f'{path}:')
            for line in lines:
                print('  map report: ' + line)
            if problems and status == 0:
                status = 1
    return status


if __name__ == '__main__':
    sys.exit(main())
