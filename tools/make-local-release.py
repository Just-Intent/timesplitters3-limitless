#!/usr/bin/env python3
"""Build a TEST-ONLY local release folder for the launcher's hidden local source.

The folder mirrors the GitHub release layout the launcher reads:

    releases.json                      GitHub "list releases" fields the launcher uses
    <game tag>/TimeSplitters-Future-Perfect-PC-<tag>-windows-x64.zip (+ .sha256)
    launcher-v<version>/TimeSplitters-Launcher-v<version>-windows-x64.zip (+ .sha256)
    launcher-v<version>/launcher-update.json
    RELEASE-NOTES.md, SHA256SUMS.txt, local-release.json
    Use-local-release-source.cmd       starts a launcher with the variable set

Point a launcher at it with TSFP_LAUNCHER_RELEASE_SOURCE=<folder>; download,
checksum verification, extraction, install, repair and launch then use the
same code path as GitHub releases. Nothing is uploaded or published.

The game ZIP comes from tools/package-release.py --local-preview: the runtime
build from build/windows/Release keeps its compile-only provenance (BUILD.json
"preview"), it is never relabelled as validated, and it never contains game
files, saves, personal settings, logs or private fixtures. Windows only.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from project_repository import REPOSITORY, UPSTREAM_REPOSITORY  # noqa: E402

LFS_POINTER = b'version https://git-lfs.github.com/spec/v1'
FORBIDDEN = re.compile(r'(^|/)(Data|Saves|Logs|private|test-fixtures)/|\.(iso|xbe|pak|img|pdb|log)$', re.I)


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def default_version():
    base = (ROOT / 'VERSION').read_text(encoding='utf-8').strip().split('-', 1)[0]
    return 'v' + base + '-mapmaker-preview.1'


def textures_hydrated(pack):
    files = sorted(pack.glob('*.dds')) if pack.is_dir() else []
    if not files:
        return False, 'assets/textures has no DDS files'
    for path in files:
        with path.open('rb') as stream:
            if stream.read(len(LFS_POINTER)) == LFS_POINTER:
                return False, 'assets/textures contains Git LFS pointers (run git lfs pull)'
    return True, f'{len(files)} DDS files'


def asset(tag, path):
    return {'name': path.name, 'state': 'uploaded', 'size': path.stat().st_size,
            'digest': 'sha256:' + sha256(path), 'content_type': 'application/octet-stream',
            'browser_download_url': f'https://github.com/{REPOSITORY}/releases/download/{tag}/{path.name}'}


def release(tag, name, body, prerelease, assets, published):
    return {'tag_name': tag, 'name': name, 'body': body, 'draft': False, 'prerelease': prerelease,
            'published_at': published, 'html_url': f'https://github.com/{REPOSITORY}/releases/tag/{tag}',
            'assets': assets}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--version', default=default_version(), help='Game prerelease tag (default: %(default)s)')
    parser.add_argument('--build-root', type=Path, default=ROOT,
                        help='Checkout holding build/windows/Release and external/ts3FPrecomp (default: this checkout)')
    parser.add_argument('--output', type=Path, help='Release folder (default: <build-root>/build/local-release/<version>)')
    parser.add_argument('--with-textures', action='store_true', help='Bundle the hydrated repository texture pack (off by default: textures are optional)')
    parser.add_argument('--replace', action='store_true', help='Replace an existing output folder made by this script')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('The Windows game runtime package can only be assembled on Windows.')
    if not re.fullmatch(r'v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)-[0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*', args.version):
        parser.error('Use a prerelease tag such as v0.3.1-mapmaker-preview.1; a local test build is never a stable release.')
    build_root = args.build_root.resolve()
    exe = build_root / 'build/windows/Release/futureperfect_recomp.exe'
    if not exe.is_file():
        parser.error(f'{exe} is missing. Build the game first (tsfp.ps1 -Action Build).')
    output = (args.output or build_root / 'build/local-release' / args.version).resolve()
    marker = output / 'local-release.json'
    if output.exists():
        if not (args.replace and marker.is_file()):
            parser.error(f'{output} exists. Pass --replace to rebuild a folder made by this script.')
        shutil.rmtree(output)
    work = output.parent / (output.name + '.work')
    if work.exists():
        shutil.rmtree(work)

    gaps = []
    textures = args.with_textures
    if textures:
        hydrated, detail = textures_hydrated(ROOT / 'assets/textures')
        if not hydrated:
            textures = False
            gaps.append('Enhanced textures NOT bundled: ' + detail + '. Classic textures only.')
    else:
        gaps.append('Enhanced textures not bundled (optional; pass --with-textures). Classic textures only.')

    command = [sys.executable, str(ROOT / 'tools/package-release.py'), '--local-preview', '--version', args.version,
               '--build-root', str(build_root), '--output', str(work)]
    if not textures:
        command.append('--without-textures')
    subprocess.run(command, check=True)

    game_name = f'TimeSplitters-Future-Perfect-PC-{args.version}-windows-x64.zip'
    game_zip = work / game_name
    size = game_zip.stat().st_size
    if size > 2 * 1024 ** 3:
        raise SystemExit(f'{game_name} is {size} bytes; the launcher accepts at most 2 GiB per download.')
    with zipfile.ZipFile(game_zip) as archive:
        prefix = game_zip.stem + '/'
        names = archive.namelist()
        bad = [n for n in names if FORBIDDEN.search(n[len(prefix):] if n.startswith(prefix) else n)]
        if bad:
            raise SystemExit('Refusing to publish locally: package contains private/game files: ' + ', '.join(bad[:5]))
        build = json.loads(archive.read(prefix + 'BUILD.json'))
    preview = build.get('preview') or {}
    if preview.get('validated') is not False:
        raise SystemExit('The package is missing its local-preview provenance; refusing to label it.')
    launcher_version = build['launcher_version']
    launcher_dir = ROOT / 'build/launcher'
    launcher_name = f'TimeSplitters-Launcher-v{launcher_version}-windows-x64.zip'
    update = json.loads((launcher_dir / 'launcher-update.json').read_text(encoding='utf-8-sig'))
    if update.get('asset_name') != launcher_name or update.get('sha256') != sha256(launcher_dir / launcher_name):
        raise SystemExit('build/launcher does not match its launcher-update.json; rebuild the launcher.')

    game_tag, launcher_tag = args.version, 'launcher-v' + launcher_version
    (output / game_tag).mkdir(parents=True)
    (output / launcher_tag).mkdir()
    shutil.move(str(game_zip), output / game_tag / game_name)
    shutil.move(str(work / (game_name + '.sha256')), output / game_tag / (game_name + '.sha256'))
    for name in (launcher_name, launcher_name + '.sha256', 'launcher-update.json'):
        shutil.copy2(launcher_dir / name, output / launcher_tag / name)

    commit = build.get('source_commit', '')
    dirty = build.get('source_changes') or []
    notes = '\n'.join([
        f'# {game_tag} (TEST-ONLY local preview)', '',
        '**Not a published or validated release.** Served only through the launcher\'s hidden',
        '`TSFP_LAUNCHER_RELEASE_SOURCE` test source; nothing was uploaded to GitHub.', '',
        '## Build provenance',
        f'- Runtime build mode: {preview.get("build_mode")}; tests run: {preview.get("tests_run")}; '
        f'network sealed: {preview.get("network_sealed")}',
        f'- Runtime built from: `{preview.get("build_source_commit")}`'
        + (' with uncommitted changes' if preview.get('build_source_changes') else ''),
        f'- Package source: `{commit}`' + (f' with {len(dirty)} uncommitted change(s)' if dirty else ''),
        f'- Skipped checks: {len(preview.get("skipped_checks") or [])} (listed in BUILD.json "preview")',
        f'- Launcher: v{launcher_version}',
        '- Enhanced textures: ' + ('bundled (repository pack, audited by the packager)' if textures else 'not bundled'),
        '', '## Known gaps', *(['- ' + gap for gap in gaps] or ['- None recorded beyond the unvalidated build.']),
        '- Gameplay, MapMaker capacity behaviour and online play are untested in this build.',
        '- Use Preview releases in the launcher channel picker to see this game build.', '',
        f'Based on {UPSTREAM_REPOSITORY}; releases for this fork: {REPOSITORY}.', ''])
    launcher_notes = (f'Launcher v{launcher_version} built locally from `{commit}` for the TEST-ONLY local '
                      'release source. Built without private launcher artwork. Not published.\n')
    stamp = int(subprocess.check_output(['git', 'log', '-1', '--format=%ct', 'HEAD'], cwd=ROOT, text=True).strip())
    published = datetime.datetime.fromtimestamp(stamp, datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
    releases = [
        release(game_tag, f'{game_tag} - MapMaker local preview (untested)', notes, True,
                [asset(game_tag, output / game_tag / game_name),
                 asset(game_tag, output / game_tag / (game_name + '.sha256'))], published),
        release(launcher_tag, f'Launcher v{launcher_version} (local build)', launcher_notes, False,
                [asset(launcher_tag, output / launcher_tag / n)
                 for n in (launcher_name, launcher_name + '.sha256', 'launcher-update.json')], published),
    ]
    (output / 'releases.json').write_text(json.dumps(releases, indent=2) + '\n', encoding='utf-8')
    (output / 'RELEASE-NOTES.md').write_text(notes, encoding='utf-8')
    (output / 'Use-local-release-source.cmd').write_bytes((
        '@echo off\r\n'
        'rem TEST ONLY: start a TimeSplitters launcher that reads releases from this folder.\r\n'
        'rem Drag "TimeSplitters Launcher.exe" onto this file, or pass its path.\r\n'
        'if "%~1"=="" (echo Drag "TimeSplitters Launcher.exe" onto this file.& pause & exit /b 1)\r\n'
        'set "TSFP_LAUNCHER_RELEASE_SOURCE=%~dp0."\r\n'
        'start "" /D "%~dp1" "%~1"\r\n').encode('ascii'))
    record = {'schema': 1, 'kind': 'tsfp-local-test-release', 'repository': REPOSITORY,
              'game_tag': game_tag, 'launcher_tag': launcher_tag, 'textures_bundled': textures,
              'gaps': gaps, 'preview': preview, 'package_source_commit': commit}
    marker.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
    sums = [f'{sha256(p)}  {p.relative_to(output).as_posix()}' for p in sorted(output.rglob('*'))
            if p.is_file() and p.name != 'SHA256SUMS.txt']
    (output / 'SHA256SUMS.txt').write_text('\n'.join(sums) + '\n', encoding='ascii')
    # The staging tree is disposable; keep matching crash symbols beside the folder.
    symbols = work / (game_zip.stem + '-symbols.zip')
    if symbols.is_file():
        shutil.move(str(symbols), output.parent / symbols.name)
    shutil.rmtree(work)
    print(json.dumps({'release_source': str(output), 'game_tag': game_tag, 'launcher_tag': launcher_tag,
                      'game_zip_bytes': size, 'textures_bundled': textures, 'gaps': gaps,
                      'environment': f'TSFP_LAUNCHER_RELEASE_SOURCE={output}'}, indent=2))
    for gap in gaps:
        print('GAP:', gap, file=sys.stderr)


if __name__ == '__main__':
    main()
