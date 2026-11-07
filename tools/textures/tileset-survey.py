"""Read-only survey of the textures used by level or MapMaker tileset models.

Model files list their textures as 16-byte rows at ``+[file+0]`` (24-bit id
twice, terminated by 0xFFFFFFFF); each id is the pak entry
``textures/%06x.xbt`` (CRC32 name). This tool resolves those ids in the given
paks, reports format/size/mips/alpha/tiling hints plus the models that use
each texture, and can decode base levels to PNG with a labelled contact sheet.

Game data is read in place. Decoded images are derived from the user's game
files: write them only under ignored ``private/`` (the default output root).
"""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths
    _configure_paths()

import argparse
import importlib.util
import io
import json
from pathlib import Path
import struct
import zlib

_spec = importlib.util.spec_from_file_location('texture_formats', Path(__file__).with_name('texture-formats.py'))
formats = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(formats)
_spec = importlib.util.spec_from_file_location('texture_catalog', Path(__file__).with_name('texture-catalog.py'))
catalog = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(catalog)

KIND_NAMES = {0: 'DXT1', 1: 'DXT3', 2: 'A8R8G8B8', 4: 'DXT5', 5: 'A8', 6: 'L8', 7: 'A8L8'}
DXT = {0: (b'DXT1', 8), 1: (b'DXT3', 16), 4: (b'DXT5', 16)}
TILESET_PAK = 'stream/lv100/tileset/tileset{0}.pak'
PRESETS = {
    'disco': dict(models=['arcade/l_56_ar.pak'], textures=['arcademem/l_56_arm.pak'],
                  exclude=['arcademem/l_100m.pak', 'arcade/l_100.pak']),
    # Abstract's tile models are only in the editor pak; their textures only in its memory pak.
    'tileset1': dict(models=[TILESET_PAK.format(1), 'arcade/l_102.pak'],
                     textures=[TILESET_PAK.format(1), 'arcademem/l_102m.pak'], tile_set=1),
}
for _n in range(2, 7):
    PRESETS[f'tileset{_n}'] = dict(models=[TILESET_PAK.format(_n)], textures=[TILESET_PAK.format(_n)], tile_set=_n)
TILESET_NAMES = {1: 'Abstract', 2: 'Lab', 3: 'Egyptian', 4: 'Military', 5: 'Horror', 6: 'Virtual'}


def texture_entry_name(texture_id):
    return 'textures/%06x.xbt' % texture_id


def entry_crc(name):
    return zlib.crc32(name.encode()) & 0xffffffff


def read_pak(path):
    data = path.read_bytes()
    if data[:4] != b'P5CK':
        raise ValueError(f'Not P5CK: {path}')
    offset, length = struct.unpack_from('<II', data, 4)
    if length % 16 or offset + length > len(data):
        raise ValueError(f'Invalid archive index: {path}')
    rows = []
    for crc, pos, size, compressed in struct.iter_unpack('<4I', data[offset:offset + length]):
        if pos + (compressed or size) > len(data):
            raise ValueError(f'Out-of-bounds entry: {path}')
        rows.append((crc, pos, size, compressed))
    return data, rows


def texture_header(blob):
    if len(blob) < 128:
        return None
    w, h, w2, h2, _unknown, kind, sentinel = struct.unpack_from('<7I', blob)
    if not (w and h and w <= 4096 and h <= 4096 and not w & (w - 1) and not h & (h - 1)
            and w == w2 and h == h2 and sentinel == 0xffffffff):
        return None
    return w, h, kind


def model_textures(blob):
    """Return the texture-id list of a model file, or None when it has none."""
    if len(blob) < 16:
        return None
    start = struct.unpack_from('<I', blob, 0)[0]
    if not 12 <= start <= len(blob) - 4:
        return None
    ids = []
    for pos in range(start, len(blob) - 15, 16):
        first, second = struct.unpack_from('<II', blob, pos)
        if first == 0xffffffff:
            return ids or None
        if first != second or first >> 24:
            return None
        ids.append(first)
    return None


def level_sizes(kind, w, h):
    sizes = []
    while True:
        if kind in DXT:
            sizes.append(((w + 3) // 4) * ((h + 3) // 4) * DXT[kind][1])
        elif kind in formats.UNCOMPRESSED:
            sizes.append(w * h * formats.UNCOMPRESSED[kind][1])
        else:
            return None
        if w == 1 and h == 1:
            return sizes
        w, h = max(1, w // 2), max(1, h // 2)


def mip_count(kind, w, h, payload):
    sizes = level_sizes(kind, w, h)
    if not sizes:
        return None
    total, count = 0, 0
    for size in sizes:
        if total + size > payload:
            break
        total, count = total + size, count + 1
    return count


def decode(kind, w, h, raw):
    from PIL import Image
    if kind in DXT:
        fourcc, block = DXT[kind]
        n = ((w + 3) // 4) * ((h + 3) // 4) * block
        return Image.open(io.BytesIO(catalog.dds_header(w, h, fourcc) + raw[:n])).convert('RGBA')
    if kind in formats.UNCOMPRESSED:
        return formats.decode(kind, w, h, raw)
    return None


def alpha_class(kind, image):
    if kind == 5:
        return 'alpha-only mask'
    lo, hi = image.getchannel('A').getextrema()
    if lo == 255:
        return 'opaque'
    hist = image.getchannel('A').histogram()
    if sum(hist[1:255]) == 0:
        return 'cutout (1-bit)'
    return 'blended'


def seam_ratio(image, axis):
    """Wrap-seam difference over the mean neighbour difference (about 1 = tiles)."""
    rgb = image.convert('L')
    w, h = rgb.size
    px = rgb.load()
    if axis == 'x':
        if w < 4:
            return None
        seam = sum(abs(px[0, y] - px[w - 1, y]) for y in range(h)) / h
        inner = sum(abs(px[x, y] - px[x + 1, y]) for y in range(0, h, max(1, h // 64))
                    for x in range(w - 1)) / ((w - 1) * len(range(0, h, max(1, h // 64))))
    else:
        if h < 4:
            return None
        seam = sum(abs(px[x, 0] - px[x, h - 1]) for x in range(w)) / w
        inner = sum(abs(px[x, y] - px[x, y + 1]) for x in range(0, w, max(1, w // 64))
                    for y in range(h - 1)) / ((h - 1) * len(range(0, w, max(1, w // 64))))
    return round(seam / inner, 2) if inner else (0.0 if seam == 0 else 99.0)


def tiling_hint(rx, ry):
    flags = [axis for axis, r in (('x', rx), ('y', ry)) if r is not None and r <= 2.0]
    return 'repeat ' + '+'.join(flags) if flags else 'no seamless edge (atlas/decal?)'


def model_name(crc, tile_set):
    for t in ([tile_set] if tile_set else []):
        for m in range(1, 30):
            if entry_crc(f'assets/ts/models/tileset{t}/tile{t}_{m}/xbox/output/tile{t}_{m}.xbr') == crc:
                return f'tile{t}_{m}'
    return f'model_{crc:08x}'


def survey(game, preset, decode_dir=None):
    tile_set = preset.get('tile_set')
    texture_paks = {}
    for rel in preset['textures']:
        data, rows = read_pak(game / 'pak' / rel)
        for crc, pos, size, compressed in rows:
            if not compressed and texture_header(data[pos:pos + 128]):
                texture_paks.setdefault(crc, (rel, data, pos, size))
    excluded = set()
    for rel in preset.get('exclude', []):
        data, rows = read_pak(game / 'pak' / rel)
        excluded.update(crc for crc, pos, size, c in rows if not c and texture_header(data[pos:pos + 128]))
    users = {}
    for rel in preset['models']:
        data, rows = read_pak(game / 'pak' / rel)
        for crc, pos, size, compressed in rows:
            blob = data[pos:pos + size]
            if compressed or texture_header(blob):
                continue
            ids = model_textures(blob)
            if not ids:
                continue
            name = model_name(crc, tile_set)
            if tile_set and not name.startswith('tile'):
                if rel != TILESET_PAK.format(tile_set):
                    continue  # other models of a shared pak (the editor's) are not tileset props
                name = f'prop_{crc:08x}'
            for tid in ids:
                users.setdefault(tid, set()).add(name)
    rows = []
    for tid, models in sorted(users.items()):
        crc = entry_crc(texture_entry_name(tid))
        if crc not in texture_paks or crc in excluded:
            continue
        rel, data, pos, size = texture_paks[crc]
        w, h, kind = texture_header(data[pos:pos + 128])
        raw = data[pos + 128:pos + size]
        info = catalog.classify(kind, w, h, raw)
        row = dict(id='%06x' % tid, key=info.get('key'), pak=rel, archive_format=kind,
                   format=KIND_NAMES.get(kind, f'kind{kind}'), width=w, height=h,
                   aspect=f'{max(w, h) // min(w, h)}:1' if w != h else '1:1',
                   mips=mip_count(kind, w, h, len(raw)), models=sorted(models))
        image = decode(kind, w, h, raw)
        if image is not None:
            row['alpha'] = alpha_class(kind, image)
            row['seam_x'], row['seam_y'] = seam_ratio(image, 'x'), seam_ratio(image, 'y')
            row['tiling'] = tiling_hint(row['seam_x'], row['seam_y'])
            r, g, b = image.convert('RGB').resize((1, 1), 1).getpixel((0, 0))
            row['mean_rgb'] = '#%02x%02x%02x' % (r, g, b)
            if decode_dir:
                decode_dir.mkdir(parents=True, exist_ok=True)
                image.save(decode_dir / f"{row['id']}.png")
        rows.append(row)
    tile_models = sorted({m for r in rows for m in r['models'] if m.startswith('tile')})
    return dict(schema=1, textures=rows, texture_count=len(rows), tile_models=tile_models,
                unresolved=sorted('%06x' % t for t in users
                                  if entry_crc(texture_entry_name(t)) not in texture_paks
                                  and entry_crc(texture_entry_name(t)) not in excluded))


def contact_sheet(rows, image_dir, dest, title, cell=168, columns=8):
    from PIL import Image, ImageDraw
    label_h = 46
    count = len(rows)
    lines = (count + columns - 1) // columns
    sheet = Image.new('RGB', (columns * cell, 24 + lines * (cell + label_h)), (32, 32, 32))
    draw = ImageDraw.Draw(sheet)
    draw.text((6, 6), title, fill=(255, 255, 255))
    checker = Image.new('RGB', (cell - 8, cell - 8), (90, 90, 90))
    cd = ImageDraw.Draw(checker)
    for y in range(0, cell, 12):
        for x in range(0, cell, 12):
            if (x // 12 + y // 12) % 2:
                cd.rectangle([x, y, x + 11, y + 11], fill=(150, 150, 150))
    for i, row in enumerate(rows):
        x, y = (i % columns) * cell, 24 + (i // columns) * (cell + label_h)
        path = image_dir / f"{row['id']}.png"
        if path.is_file():
            image = Image.open(path).convert('RGBA')
            image.thumbnail((cell - 8, cell - 8))
            back = checker.crop((0, 0) + image.size)
            back.paste(image, (0, 0), image)
            sheet.paste(back, (x + 4, y + 4))
        alpha = {'opaque': 'op', 'cutout (1-bit)': 'cut', 'blended': 'blend', 'alpha-only mask': 'A8'}.get(row.get('alpha'), '?')
        tiling = row.get('tiling', '?').replace('repeat ', 'rep ').replace('no seamless edge (atlas/decal?)', 'atlas?')
        draw.text((x + 4, y + cell - 2), f"{row['id']} {row['width']}x{row['height']}", fill=(255, 255, 160))
        draw.text((x + 4, y + cell + 11), f"{row['format']} m{row['mips']} {alpha}", fill=(200, 220, 255))
        draw.text((x + 4, y + cell + 24), f"{tiling} u{len(row['models'])}", fill=(200, 255, 200))
    dest.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(dest)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, default=Path('private/game'))
    parser.add_argument('--output', type=Path, default=Path('private/tileset-survey'))
    parser.add_argument('--preset', choices=sorted(PRESETS), action='append', required=True)
    parser.add_argument('--decode', action='store_true', help='Write base-level PNGs and a contact sheet')
    args = parser.parse_args()
    if args.decode and 'private' not in args.output.resolve().parts:
        parser.error('Decoded game images must stay under an ignored private/ folder')
    formats.verify_mapping(args.game / 'default.xbe')
    for name in args.preset:
        out = args.output / name
        result = survey(args.game, PRESETS[name], out / 'png' if args.decode else None)
        out.mkdir(parents=True, exist_ok=True)
        (out / 'survey.json').write_text(json.dumps(result, indent=1) + '\n', encoding='utf-8')
        if args.decode:
            label = TILESET_NAMES.get(PRESETS[name].get('tile_set'), name)
            contact_sheet(result['textures'], out / 'png', args.output / f'contact-{name}.png',
                          f"{name} ({label}): {result['texture_count']} textures; id WxH / format mips alpha / tiling users")
        print(name, result['texture_count'], 'textures,', len(result['tile_models']), 'tile models,',
              len(result['unresolved']), 'unresolved ids')


if __name__ == '__main__':
    main()
