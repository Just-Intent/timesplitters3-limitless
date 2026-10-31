"""The build must regenerate guest wrappers when runtime hooks change."""


import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('generation', ROOT / 'tools/build/recomp-generation.py')
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


def test_parser_sources_and_bridge_invalidate_generation(tmp_path, monkeypatch):
    project = tmp_path / 'project'
    runtime = tmp_path / 'runtime'
    for name in ('scripts/recompile.py', 'templates/runtime/recomp_types.h',
                 'titles/futureperfect/src/recomp_manual.c',
                 'titles/futureperfect/src/recomp/gen/recomp_funcs.h',
                 'tools/xbe_parser/xbe_parser.py', 'tools/disasm/engine.py', 'tools/xdk_symbols/__main__.py', 'tools/func_id/__main__.py'):
        path = runtime / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('original')
    for name in ('xbe_image.py', 'xbe_kernel_exports.def', 'recomp_image.py', 'x86_decode.py', 'x86_flow.py', 'x86_recovery.py', 'x86_functions.py', 'recomp_symbols.py', 'analysis_cache.py', 'analysis_config.py', 'analysis_output.py', 'analysis_runner.py', 'xdk_catalog.py', 'compile_pipeline.py', 'function_evidence.py', 'function_categories.py', 'function_catalog.py', 'c_source_scan.py', 'generation_output.py', 'translation_decode.py', 'translation_layout.py', 'translation_hooks.py', 'branch_feedback.py', 'unresolved_report.py', 'translation_operands.py', 'translation_moves.py', 'translation_integer.py', 'translation_bits.py', 'translation_product.py', 'translation_bitops.py', 'translation_exchange.py', 'translation_compare.py', 'translation_strings.py', 'translation_conditions.py', 'translation_sse.py', 'translation_mmx.py', 'translation_x87.py', 'translation_control.py', 'translation_lifting.py', 'translation_context.py', 'translation_helpers.py', 'translation_names.py', 'translation_host_symbols.txt', 'translation_host_symbols.json', 'translation_batch.py', 'translation_discovery.py', 'translation_function.py', 'translation_cli.py', 'guest_code_patches.py'):
        path = project / 'tools/compiler' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('original')
    header = project / 'src/runtime/recomp_types.h'
    header.parent.mkdir(parents=True)
    header.write_text('original')
    support = header.with_name('tsfp_branch_feedback.h')
    support.write_text('original')
    monkeypatch.setattr(generation, '__file__', str(project / 'tools/build/recomp-generation.py'))
    for path in (header, support, project / 'tools/compiler/xbe_image.py', project / 'tools/compiler/xbe_kernel_exports.def',
                 project / 'tools/compiler/recomp_image.py', project / 'tools/compiler/x86_decode.py', project / 'tools/compiler/x86_flow.py', project / 'tools/compiler/x86_recovery.py', project / 'tools/compiler/x86_functions.py', project / 'tools/compiler/recomp_symbols.py',
                 project / 'tools/compiler/analysis_cache.py', project / 'tools/compiler/analysis_config.py', project / 'tools/compiler/analysis_output.py', project / 'tools/compiler/analysis_runner.py',
                 project / 'tools/compiler/xdk_catalog.py', project / 'tools/compiler/compile_pipeline.py',
                 project / 'tools/compiler/function_evidence.py', project / 'tools/compiler/function_categories.py', project / 'tools/compiler/function_catalog.py', project / 'tools/compiler/c_source_scan.py', project / 'tools/compiler/generation_output.py', project / 'tools/compiler/translation_decode.py', project / 'tools/compiler/translation_layout.py', project / 'tools/compiler/translation_hooks.py', project / 'tools/compiler/branch_feedback.py', project / 'tools/compiler/unresolved_report.py', project / 'tools/compiler/translation_operands.py', project / 'tools/compiler/translation_moves.py', project / 'tools/compiler/translation_integer.py', project / 'tools/compiler/translation_bits.py', project / 'tools/compiler/translation_product.py', project / 'tools/compiler/guest_code_patches.py',
                 runtime / 'tools/xbe_parser/xbe_parser.py', runtime / 'tools/disasm/engine.py', runtime / 'tools/xdk_symbols/__main__.py', runtime / 'tools/func_id/__main__.py'):
        generation.record(runtime)
        assert generation.current(runtime)
        path.write_text('changed')
        assert not generation.current(runtime)
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
    for script, count in (('tsfp.ps1', 2), ('tools/build/build-native-mac.py', 1)):
        assert (ROOT / script).read_text(encoding='utf-8').count('--guest-patches') == count, script
    header = json.loads((ROOT / 'config/guest-code-patches/00-header.json').read_text(encoding='utf-8'))
    assert header['title_id'] == '45410066'
    assert header['xbe_sha256'] == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    assert header['extension']['va'] == '0x008A0000' and header['extension']['size'] == '0x00080000'
    for table in (ROOT / 'config/guest-code-patches').glob('*.json'):
        if table.name != '00-header.json':
            part = json.loads(table.read_text(encoding='utf-8'))
            assert set(part) <= {'patches'}, f'{table.name}: only the header carries title/XBE/extension'
