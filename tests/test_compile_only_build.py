"""tsfp.ps1 -Action Build -CompileOnly: compile everything, execute no built programs.

Hosts with application control (for example Smart App Control) can block the
locally compiled test executables. Compile-only mode must still enforce game
validation, patching, generation and compilation, must name every skipped
check, and must never seal the executable as a tested build.
"""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = (ROOT / 'tsfp.ps1').read_bytes().decode('utf-8')
TEXT = SCRIPT.replace('\r\n', '\n')


def build_case():
    start = TEXT.index("                'Build' {\n")
    end = TEXT.index("{ $_ -in 'Run', 'SmokeTest' }", start)
    return TEXT[start:end]


def block_after(text, opener):
    """Return the brace-balanced body that follows `opener`."""
    start = text.index(opener) + len(opener)
    depth, i = 1, start
    while depth:
        depth += {'{': 1, '}': -1}.get(text[i], 0)
        i += 1
    return text[start:i - 1]


BUILD = build_case()
BRANCH = '                    if ($CompileOnly) {\n                        # Every target'
TAIL = BUILD[BUILD.index(BRANCH):]
COMPILE_ONLY = block_after(TAIL, 'if ($CompileOnly) {')
FULL = block_after(TAIL[len(COMPILE_ONLY):], 'else {')
BEFORE = BUILD[:BUILD.index(BRANCH)]
EXECUTABLE = re.compile(r'\b(tsfp_\w+|d3d8_replay)\.exe\b')


def test_switch_is_explicit_and_build_only():
    assert re.search(r'^\s*\[switch\]\$CompileOnly,$', TEXT, re.M)
    guard = "if ($CompileOnly -and $Action -ne 'Build') { throw '-CompileOnly applies only to -Action Build.' }"
    assert guard in TEXT
    # The guard runs before the script creates folders or settings.
    assert TEXT.index(guard) < TEXT.index('New-Item -ItemType Directory -Force -Path $privateRoot')


def test_required_steps_run_before_and_regardless_of_mode():
    for step in ('Assert-Game', 'tools\\build_textures.py', 'Apply-LocalPatches', "throw 'Run Generate first.'",
                 "recomp-generation.py') $toolRoot", "'-m', 'tools.xdk_symbols'", "'scripts/recompile.py'",
                 "'--record')", "tools\\network-identity.py'))", "'-S', 'titles/futureperfect'",
                 "@('--build', $buildRoot"):
        assert step in BEFORE, step
    # Regeneration is only annotated in compile-only mode, never skipped.
    note = block_after(BEFORE, 'if ($CompileOnly) {')
    assert 'Invoke-Checked' not in note and 'return' not in note and 'continue' not in note
    # No other conditional on the mode precedes compilation.
    assert BEFORE.count('$CompileOnly') == 1


def test_compile_only_targets_match_full_build():
    targets = re.search(r"'--target', (.*?), '--parallel'", BEFORE, re.S)[1]
    built = set(re.findall(r"'(\w+)'", targets))
    assert 'futureperfect_recomp' in built
    executed = set(EXECUTABLE.findall(FULL))
    assert executed and executed <= built  # compile-only still compiles every test program


def test_compile_only_branch_executes_nothing():
    assert 'Write-CompileOnlyReport' in COMPILE_ONLY
    assert 'Invoke-Checked' not in COMPILE_ONLY and '& ' not in COMPILE_ONLY
    assert 'Start-Process' not in COMPILE_ONLY and 'Invoke-Expression' not in COMPILE_ONLY


def test_every_skipped_check_is_named():
    skipped = set(EXECUTABLE.findall(COMPILE_ONLY))
    executed = set(EXECUTABLE.findall(FULL))
    assert skipped == executed, f'unlisted: {executed - skipped}, stale: {skipped - executed}'
    assert 'ghost_targets.py' in COMPILE_ONLY and 'ghost_targets.py' in FULL
    assert '--seal-executable' in COMPILE_ONLY and "'--seal-executable', $exe)" in FULL
    texture_modes = re.search(r"foreach \(\$mode in @\(([^)]*)\)\)", FULL)[1]
    for mode in re.findall(r"'(\w+)'", texture_modes):
        assert mode in COMPILE_ONLY


def test_full_build_behaviour_is_preserved():
    passed = FULL.index("Write-Output 'Build tests passed;")
    assert FULL.index('Remove-Item -LiteralPath $compileOnlyRecord') > passed
    assert FULL.index("'--seal-executable', $exe)") > passed
    # Crash symbols only archive files and run in both modes.
    after = TAIL[len(COMPILE_ONLY):]
    assert "tools\\crash-symbols.py'), '--build-dir'" in after[after.index(FULL) + len(FULL):]


def test_report_never_claims_success():
    report = block_after(TEXT, 'function Write-CompileOnlyReport {')
    assert 'tests_run = $false' in report and 'network_sealed = $false' in report
    assert 'Write-Warning' in report and 'NOT run' in report and 'SKIPPED (not run)' in report
    assert 'passed' not in report.lower()
    assert 'throw "Compile-only build did not produce' in report


def test_script_keeps_crlf():
    assert '\r\n' in SCRIPT and '\n' not in SCRIPT.replace('\r\n', '')


def powershell(script):
    return subprocess.run(['powershell.exe', '-NoProfile', '-Command', script],
                          capture_output=True, text=True, timeout=120)


def quote(value):
    return "'" + str(value).replace("'", "''") + "'"


@pytest.mark.skipif(sys.platform != 'win32', reason='Windows PowerShell')
def test_report_function_on_windows(tmp_path):
    exe = tmp_path / 'futureperfect_recomp.exe'
    exe.write_bytes(b'not executed')
    record = tmp_path / 'compile-only-build.json'
    script = f"""
$ErrorActionPreference = 'Stop'
$ast = [System.Management.Automation.Language.Parser]::ParseFile({quote(ROOT / 'tsfp.ps1')}, [ref]$null, [ref]$null)
$function = $ast.Find({{ param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Write-CompileOnlyReport'
}}, $true)
. ([scriptblock]::Create($function.Extent.Text))
Write-CompileOnlyReport -Root {quote(ROOT)} -Executable {quote(exe)} -Record {quote(record)} -Skipped @('one.exe', 'two.exe') 3>&1
try {{
    Write-CompileOnlyReport -Root {quote(ROOT)} -Executable {quote(tmp_path / 'missing.exe')} -Record {quote(tmp_path / 'x.json')} -Skipped @('one.exe')
    throw 'Missing executable was accepted'
}} catch {{ if ($_.Exception.Message -notmatch 'did not produce') {{ throw }} }}
"""
    result = powershell(script)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'COMPILE-ONLY BUILD: 2 build checks were NOT run' in result.stdout
    assert 'SKIPPED (not run): one.exe' in result.stdout and 'SKIPPED (not run): two.exe' in result.stdout
    details = json.loads(record.read_text(encoding='utf-8'))
    assert details['tests_run'] is False and details['network_sealed'] is False
    assert details['skipped'] == ['one.exe', 'two.exe']
    assert details['executable_sha256'] == hashlib.sha256(b'not executed').hexdigest()
    assert not (tmp_path / 'x.json').exists()


@pytest.mark.skipif(sys.platform != 'win32', reason='Windows PowerShell')
def test_switch_rejected_outside_build_on_windows(tmp_path):
    # An inert copy: $PSScriptRoot points at an empty folder.
    copy = tmp_path / 'tsfp.ps1'
    copy.write_bytes((ROOT / 'tsfp.ps1').read_bytes())
    result = subprocess.run(['powershell.exe', '-NoProfile', '-File', str(copy), '-Action', 'Status', '-CompileOnly'],
                            capture_output=True, text=True, timeout=120)
    assert result.returncode != 0
    assert 'applies only to -Action Build' in result.stdout + result.stderr
    assert sorted(p.name for p in tmp_path.iterdir()) == ['tsfp.ps1']
