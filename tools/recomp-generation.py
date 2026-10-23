"""Track which generator sources produced the local, ignored game C files."""
import argparse
import hashlib
import json
import re
from pathlib import Path


def fingerprint(runtime):
    paths = [runtime / 'scripts/recompile.py',
             runtime / 'templates/runtime/recomp_types.h',
             runtime / 'titles/futureperfect/src/recomp_manual.c']
    paths.extend(p for p in (runtime / 'tools/recomp').rglob('*')
                 if p.is_file() and p.suffix in ('.py', '.json', '.txt')
                 and '__pycache__' not in p.parts and not p.name.startswith('test_'))
    paths.extend((runtime / 'config/extra_symbols').glob('*.json'))
    result = {p.relative_to(runtime).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in sorted(paths)}
    # The lift scans these declarations to emit wrappers and original-body
    # pointers. Function-body edits only require compiling, but changing the
    # exported/imported names requires regenerating the guest C first.
    declarations = set()
    pattern = re.compile(r"^\s*(HLE_EXPORT|HLE_ORIGINAL|HLE_IMPORT_VAR)\(\s*([A-Za-z_]\w*)\s*\)", re.M)
    for source in (runtime / 'src/hle').rglob('*.c'):
        declarations.update(pattern.findall(source.read_text(encoding='utf-8')))
    result['hle-declarations'] = hashlib.sha256(
        json.dumps(sorted(declarations)).encode('utf-8')).hexdigest()
    # Verified guest-code patches are applied by the lift, so editing the
    # project's table must regenerate the guest C.
    tables = guest_patch_table(runtime)
    digest = hashlib.sha256()
    for table in sorted(tables.glob('*.json')) if tables.is_dir() else ():
        digest.update(table.name.encode('utf-8') + b'\0' + table.read_bytes() + b'\0')
    result['guest-code-patches'] = digest.hexdigest() if tables.is_dir() else 'absent'
    return result


def guest_patch_table(runtime):
    """The project's tables folder, two levels above the runtime checkout (external/<runtime>)."""
    return Path(runtime).resolve().parents[1] / 'config/guest-code-patches'


def marker(runtime):
    return runtime / 'titles/futureperfect/src/recomp/gen/tsfp-generator.json'


def current(runtime):
    try:
        return json.loads(marker(runtime).read_text(encoding='utf-8')) == fingerprint(runtime)
    except (OSError, ValueError):
        return False


def record(runtime):
    path = marker(runtime)
    if not (path.parent / 'recomp_funcs.h').is_file():
        raise RuntimeError('Generate the game sources before recording their generator.')
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(fingerprint(runtime), indent=2) + '\n', encoding='utf-8')
    temporary.replace(path)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runtime', type=Path)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    runtime = args.runtime.resolve()
    if args.record:
        record(runtime)
    else:
        raise SystemExit(0 if current(runtime) else 1)
