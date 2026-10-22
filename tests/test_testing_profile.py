"""Test save integrity, collectible preservation and isolation without game data."""
import importlib.util
from pathlib import Path
import struct
import hashlib

import pytest

spec=importlib.util.spec_from_file_location('testing_profile',Path(__file__).parents[1]/'tools/testing_profile.py')
profile=importlib.util.module_from_spec(spec)
spec.loader.exec_module(profile)


def fresh():
    body=bytearray((i*17+3)%256 for i in range(profile.BODY_SIZE))
    struct.pack_into('<I',body,0x1EE0,2)
    return bytes(body)+profile.signature(body)


def test_collectibles_and_settings_preserved():
    original=fresh()
    unlocked=profile.unlock_profile(original)
    allowed=set(range(profile.PROGRESS+0xBF4,profile.PROGRESS+0xC4C))
    for i in range(48):allowed.update(range(profile.PROGRESS+0x49C+i*0x18,profile.PROGRESS+0x4A0+i*0x18))
    assert all(original[i]==unlocked[i] for i in range(profile.BODY_SIZE) if i not in allowed)
    assert unlocked[profile.PROGRESS+0xBF0:profile.PROGRESS+0xBF4]==original[profile.PROGRESS+0xBF0:profile.PROGRESS+0xBF4]
    assert profile.signature(unlocked[:-20])==unlocked[-20:]
    assert profile.unlock_profile(unlocked)==unlocked


@pytest.mark.parametrize('raw',[b'',fresh()[:-1],fresh()[:-20]+bytes(20)])
def test_corrupt_save_rejected(raw):
    with pytest.raises(ValueError):profile.unlock_profile(raw)


def test_unsupported_version_rejected():
    body=bytearray(fresh()[:-20]);struct.pack_into('<I',body,0x1EE0,3)
    with pytest.raises(ValueError,match='version'):profile.unlock_profile(bytes(body)+profile.signature(body))


def test_personal_seed_refused(tmp_path):
    with pytest.raises(ValueError,match='Personal'):
        profile.create_fixture(tmp_path,tmp_path/'private/saves')


def test_paths_cannot_escape_test_root(tmp_path):
    with pytest.raises(ValueError):
        profile.contained(tmp_path,tmp_path/'private/test-saves/../../personal','private/test-saves')


def test_unowned_run_not_overwritten(tmp_path,monkeypatch):
    run=tmp_path/'private/test-saves/session';run.mkdir(parents=True)
    note=run/'keep.txt';note.write_text('keep')
    monkeypatch.setattr(profile,'check_fixture',lambda root:(tmp_path/profile.FIXTURE,{'files':{}}))
    with pytest.raises(ValueError,match='not owned'):profile.prepare_run(tmp_path,run)
    assert note.read_text()=='keep'


def test_fixture_and_disposable_copy_lifecycle(tmp_path,monkeypatch):
    game=tmp_path/'private/game';game.mkdir(parents=True)
    (game/'default.xbe').write_bytes(b'test executable')
    monkeypatch.setattr(profile,'XBE_SHA256',hashlib.sha256(b'test executable').hexdigest())
    seed=tmp_path/'private/test-saves/fresh/saves'
    title=seed/'UserData/45410066';slot=title/'16D15BD8D404';slot.mkdir(parents=True)
    body=bytearray(fresh()[:-20]);struct.pack_into('<I',body,profile.PROGRESS+0xBF0,0)
    (slot/'tsftrpft').write_bytes(bytes(body)+profile.signature(body))
    for name in ('SaveImage.xbx','TitleImage.xbx','TitleMeta.xbx'):(title/name).write_bytes(b'fixture')
    controls=tmp_path/'tests/fixtures/pc-controls.json';controls.parent.mkdir(parents=True)
    controls.write_text('{}')
    canonical=profile.create_fixture(tmp_path,seed)
    with pytest.raises(ValueError,match='exists'):profile.create_fixture(tmp_path,seed)
    run=tmp_path/'private/test-saves/session'
    monkeypatch.setenv('TSFP_TEXTURE_PACK_DIR',str(tmp_path/'personal-pack'))
    monkeypatch.setenv('TSFP_TEXTURE_MODE','4k')
    monkeypatch.setenv('TSFP_TEXTURE_DUMP_DIR',str(tmp_path/'personal-dumps'))
    monkeypatch.setenv('TSFP_MAPMAKER_PROBE','7')  # inherited probes stay off
    env=profile.prepare_run(tmp_path,run)
    relative=Path('UserData/45410066/16D15BD8D404/tsftrpft')
    saved=(canonical/relative).read_bytes()
    (run/'saves'/relative).write_bytes(b'changed during gameplay')
    assert (canonical/relative).read_bytes()==saved
    profile.prepare_run(tmp_path,run)
    assert (run/'saves'/relative).read_bytes()==saved
    assert env['RECOMP_XMV_PLAY']=='0'
    assert env['TSFP_TEXTURE_PACK_DIR']==str(tmp_path/'assets/textures')
    assert env['TSFP_TEXTURE_MODE']=='classic'
    assert 'TSFP_TEXTURE_DUMP_DIR' not in env
    assert 'TSFP_MAPMAKER_PROBE' not in env
    assert all(str(run) in env[key] for key in ('RECOMP_SAVE_DIR','RECOMP_INPUT_CONFIG','RECOMP_DISPLAY_CONFIG','TSFP_DISPLAY_CONFIG','TSFP_CRASH_DIR'))
    (run/'run.lock').write_text('in use')
    with pytest.raises(ValueError,match='in use'):profile.prepare_run(tmp_path,run)


def test_mapmaker_probe_environment(tmp_path):
    """The meter probe is explicit per run and logs inside the disposable test root."""
    run=tmp_path/'private/test-saves/quick-test'
    assert profile.mapmaker_probe_environment(tmp_path,run,None)=={}
    assert profile.mapmaker_probe_environment(tmp_path,run,0)=={}
    env=profile.mapmaker_probe_environment(tmp_path,run,3)
    assert env=={'TSFP_MAPMAKER_PROBE':'3','TSFP_MAPMAKER_PROBE_LOG':str((run/'mapmaker-probe.log').resolve())}
    assert profile.mapmaker_probe_environment(tmp_path,run,11)['TSFP_MAPMAKER_PROBE']=='11'
    for flags in (-1,16):
        with pytest.raises(ValueError,match='between 1 and 15'):profile.mapmaker_probe_environment(tmp_path,run,flags)
    with pytest.raises(ValueError):profile.mapmaker_probe_environment(tmp_path,tmp_path/'private/saves',1)
