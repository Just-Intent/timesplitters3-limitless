"""Asset-free checks for the TEST-ONLY local release folder builder."""
import contextlib
import hashlib
import importlib.util
import io
from pathlib import Path
import re
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('make_local_release', ROOT / 'tools/make-local-release.py')
local = importlib.util.module_from_spec(spec)
spec.loader.exec_module(local)


class LocalRelease(unittest.TestCase):
    def test_repository_constants_match_the_launcher(self):
        client = (ROOT / 'tools/launcher/ReleaseClient.cs').read_text(encoding='utf-8')
        self.assertIn(f'public const string Repository = "{local.REPOSITORY}";', client)
        self.assertIn('LocalReleaseSource.FromEnvironment()', client)
        config = (ROOT / 'config/project-repository.json').read_text(encoding='utf-8')
        self.assertIn(local.REPOSITORY, config)
        worker = (ROOT / 'tools/compatibility-worker.py').read_text(encoding='utf-8')
        self.assertIn(f"REPO = '{local.REPOSITORY}'", worker)

    def test_default_version_is_a_prerelease(self):
        version = local.default_version()
        self.assertRegex(version, r'^v\d+\.\d+\.\d+-mapmaker\.1$')

    def test_stable_versions_are_refused(self):
        with mock.patch.object(sys, 'argv', ['make-local-release.py', '--version', 'v1.2.3']), \
                mock.patch.object(local.os, 'name', 'nt'), \
                contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            local.main()

    def test_assets_use_this_repositorys_download_urls_and_digests(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'TimeSplitters-Future-Perfect-PC-v1.0.0-x.1-windows-x64.zip'
            path.write_bytes(b'synthetic')
            entry = local.asset('v1.0.0-x.1', path)
            self.assertEqual(entry['state'], 'uploaded')
            self.assertEqual(entry['size'], 9)
            self.assertEqual(entry['digest'], 'sha256:' + hashlib.sha256(b'synthetic').hexdigest())
            self.assertTrue(entry['browser_download_url'].startswith(
                'https://github.com/' + local.REPOSITORY + '/releases/download/v1.0.0-x.1/'))
            record = local.release('v1.0.0-x.1', 'n', 'b', True, [entry], '2026-10-31T00:00:00Z')
            self.assertFalse(record['draft'])
            self.assertTrue(record['prerelease'])

    def test_lfs_pointers_are_reported_not_packaged(self):
        with tempfile.TemporaryDirectory() as directory:
            pack = Path(directory)
            self.assertFalse(local.textures_hydrated(pack)[0])
            (pack / '0123456789abcdef.dds').write_bytes(local.LFS_POINTER + b'\noid sha256:00\n')
            hydrated, detail = local.textures_hydrated(pack)
            self.assertFalse(hydrated)
            self.assertIn('LFS', detail)
            (pack / '0123456789abcdef.dds').write_bytes(b'DDS |' + b'\0' * 120)
            self.assertTrue(local.textures_hydrated(pack)[0])

    def test_private_paths_are_rejected(self):
        for name in ['Data/default.xbe', 'Saves/profile', 'Logs/game.log', 'private/game/x', 'Bin/a.pdb', 'disc.iso']:
            self.assertTrue(local.FORBIDDEN.search(name), name)
        for name in ['Bin/futureperfect_recomp.exe', 'Textures/0123456789abcdef.dds', 'Support/Python/python.exe']:
            self.assertFalse(local.FORBIDDEN.search(name), name)


if __name__ == '__main__':
    unittest.main()
