"""The build must regenerate guest wrappers when runtime hooks change."""
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('generation', ROOT / 'tools/recomp-generation.py')
generation = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generation)


def test_runtime_declarations_invalidate_generated_wrappers(tmp_path):
    for name in ('scripts/recompile.py', 'templates/runtime/recomp_types.h',
                 'titles/futureperfect/src/recomp_manual.c',
                 'titles/futureperfect/src/recomp/gen/recomp_funcs.h',
                 'src/hle/stream.c'):
        p = tmp_path / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text('/* original */')
    source = tmp_path / 'src/hle/stream.c'
    generation.record(tmp_path)
    for declaration in ('HLE_EXPORT(Release)', 'HLE_ORIGINAL(Release)', 'HLE_IMPORT_VAR(Clock)'):
        source.write_text(source.read_text() + '\n' + declaration + '\n')
        assert not generation.current(tmp_path)
        generation.record(tmp_path)
        assert generation.current(tmp_path)
    source.write_text(source.read_text() + '\n/* body-only edit */\n')
    assert generation.current(tmp_path)
    source.unlink()
    assert not generation.current(tmp_path)
    generation.record(tmp_path)
    symbols = tmp_path / 'config/extra_symbols/45410066.json'
    symbols.parent.mkdir(parents=True)
    symbols.write_text('{"symbols": []}')
    assert not generation.current(tmp_path)
    generation.record(tmp_path)
    symbols.write_text('{"symbols": [{"name": "XMVPlaybackStop"}]}')
    assert not generation.current(tmp_path)


def test_guest_code_patch_table_invalidates_generated_code(tmp_path):
    runtime = tmp_path / 'external/ts3FPrecomp'
    for name in ('scripts/recompile.py', 'templates/runtime/recomp_types.h',
                 'titles/futureperfect/src/recomp_manual.c',
                 'titles/futureperfect/src/recomp/gen/recomp_funcs.h'):
        (runtime / name).parent.mkdir(parents=True, exist_ok=True)
        (runtime / name).write_text('/* original */')
    tables = tmp_path / 'config/guest-code-patches'
    assert generation.guest_patch_table(runtime) == tables.resolve()
    generation.record(runtime)
    assert generation.current(runtime)
    tables.mkdir(parents=True)
    (tables / 'a.json').write_text('{"patches": []}')
    assert not generation.current(runtime)
    generation.record(runtime)
    (tables / 'a.json').write_text('{"patches": [{"id": "x"}]}')
    assert not generation.current(runtime)
    generation.record(runtime)
    (tables / 'b.json').write_text('{"patches": []}')            # a new area file
    assert not generation.current(runtime)
    generation.record(runtime)
    (tables / 'notes.txt').write_text('ignored')
    assert generation.current(runtime)


def test_builds_pass_the_project_guest_patch_table():
    import json
    for script, count in (('tsfp.ps1', 2), ('tools/build-native-mac.py', 1)):
        assert (ROOT / script).read_text(encoding='utf-8').count('--guest-patches') == count, script
    header = json.loads((ROOT / 'config/guest-code-patches/00-header.json').read_text(encoding='utf-8'))
    assert header['title_id'] == '45410066'
    assert header['xbe_sha256'] == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    assert header['extension']['va'] == '0x008A0000' and header['extension']['size'] == '0x00080000'
    for table in (ROOT / 'config/guest-code-patches').glob('*.json'):
        if table.name != '00-header.json':
            part = json.loads(table.read_text(encoding='utf-8'))
            assert set(part) <= {'patches'}, f'{table.name}: only the header carries title/XBE/extension'
