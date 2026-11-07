"""Imported music host layer (src/interface/pc_custom_music.h): the fixture
tests/media/custom_music.c with WAV files written here, plus mutations that the
fixture must catch. No game data and no audio device are used."""
import os
import struct
import subprocess
from pathlib import Path

import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / 'src/interface/pc_custom_music.h'
FIXTURE = ROOT / 'tests/media/custom_music.c'

MUTATIONS = {
    'list-rescan-reorders': ('    for (k = first + 1; k < list->count; k++) {\n        unsigned j = k;\n        while (j > first &&',
                             '    for (k = 1; k < list->count; k++) {\n        unsigned j = k;\n        while (j > 0 &&'),
    'list-missing-kept': ('    for (k = 0; k < list->count; k++) list->present[k] = 0;\n', ''),
    'find-missing': ('        if (list->present[k] && !pc_music_casecmp(list->name[k], name)) return (int)k;',
                     '        if (!pc_music_casecmp(list->name[k], name)) return (int)k;'),
    'name-any-extension': ('    if (pc_music_casecmp(name + n - 4, ".wav")) return 0;\n', ''),
    'name-path': ("        if (c < 0x20 || c == 0x7F || c == '/' || c == '\\\\' || c == ':') return 0;",
                  '        if (c < 0x20 || c == 0x7F) return 0;'),
    'name-too-long': ('    if (n < 5 || n >= PC_MUSIC_NAME_MAX ||', '    if (n < 5 || n > PC_MUSIC_NAME_MAX ||'),
    'label-extension': ('    if (n >= 4 && !pc_music_casecmp(name + n - 4, ".wav")) n -= 4;\n', ''),
    'wav-any-codec': ('            else if (tag != 1) return 0;\n', ''),
    'wav-no-extensible': ('            if (tag == 0xFFFE && size >= 40) tag = pc_wav_u16(fmt + 24);   /* extensible: sub-format */\n', ''),
    'wav-no-pad': ('        pos += 8u + (uint64_t)size + (size & 1u);', '        pos += 8u + (uint64_t)size;'),
    'wav-trust-size': ('        if (end > 0 && (uint64_t)end < info->data_offset + info->data_bytes)', '        if (0)'),
    'convert-24': ('    case 24: v = (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24); return (int16_t)(v >> 16);',
                   '    case 24: v = (int32_t)((uint32_t)p[2] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[0] << 24); return (int16_t)(v >> 16);'),
    'convert-8-signed': ('    case 8: return (int16_t)(((int)p[0] - 128) * 256);', '    case 8: return (int16_t)((int)p[0] * 256);'),
    'convert-float-unclamped': ('        if (f > 1.0) f = 1.0;\n', ''),
    'convert-surround-second': ('        if (out_channels == 2) out[f * 2 + 1] = pc_wav_sample(info, frame + bytes);',
                                '        if (out_channels == 2) out[f * 2 + 1] = pc_wav_sample(info, frame + 2u * bytes);'),
    'db-scale': ('    db = 2000.0 * log10((double)gain);', '    db = 1000.0 * log10((double)gain);'),
    'player-no-lead': ('        if (ahead_frames * 1000u >= (uint64_t)PC_MUSIC_LEAD_MS * p->info.rate || queued >= 6u) break;',
                       '        if (queued >= 6u) break;'),
    'player-no-loop': ('                p->read_bytes = 0;\n                left = p->info.data_bytes;\n', '                return 0;\n'),
    'player-skips-refused': ('            break;                     /* offered again next step, never skipped */',
                             '            { p->pending_frames = 0; break; }'),
    'player-pause-plays': ('        if (!p->paused) p->paused = 1;\n        return 1;', '        if (!p->paused) p->paused = 1;'),
    'player-resume-no-reset': ('    if (p->paused) { p->paused = 0; p->submitted_frames = 0; p->base_ms = now; PC_MUSIC_RESET(); }',
                               '    if (p->paused) { p->paused = 0; }'),
    'player-watch-ignored': ('    if (!p->watch(p->context, &gain, &paused)) return 0;', '    p->watch(p->context, &gain, &paused);'),
    'player-level': ('#define PC_MUSIC_LEVEL 0.65f', '#define PC_MUSIC_LEVEL 1.0f'),
    'player-bad-file': ('    if (!(p->file = pc_music_open(dir, name)) || !pc_wav_parse(p->file, &p->info)) {',
                        '    if (!(p->file = pc_music_open(dir, name))) {'),
    'ini-any-name': ('    if (*name && !pc_music_name_ok(name)) return 0;\n', ''),
    'ini-other-folder': ('    strcpy(out + prefix, "pc-music.ini");', '    strcpy(out, "pc-music.ini");'),
}


def wav(path, rate, channels, bits, frames, tag=1, extensible=False, extra=b'', data=None, size=None):
    block = channels * bits // 8
    if data is None:
        if bits == 16:
            data = b''.join(struct.pack('<' + 'h' * channels, *([1000, -1000] * 4)[:channels]) for _ in range(frames))
        else:
            data = bytes(block * frames)
    if extensible:
        fmt = struct.pack('<HHIIHHHHI16s', 0xFFFE, channels, rate, rate * block, block, bits, 22, bits, 0x3F,
                          struct.pack('<H', tag) + bytes(14))
    else:
        fmt = struct.pack('<HHIIHH', tag, channels, rate, rate * block, block, bits)
    body = b'WAVE' + b'fmt ' + struct.pack('<I', len(fmt)) + fmt + extra
    body += b'data' + struct.pack('<I', len(data) if size is None else size) + data
    path.write_bytes(b'RIFF' + struct.pack('<I', len(body)) + body)


def write_folder(folder, other):
    folder.mkdir()
    other.mkdir()
    wav(folder / 'B16.wav', 44100, 2, 16, 4410)
    wav(folder / 'a8.wav', 22050, 1, 8, 2205)
    wav(folder / 'c24.wav', 48000, 2, 24, 480)
    wav(folder / 'd32.wav', 48000, 2, 32, 480)
    wav(folder / 'e_float.wav', 48000, 2, 32, 480, tag=3)
    wav(folder / 'f_ext.wav', 48000, 6, 16, 480, extensible=True)
    wav(folder / 'g_list.wav', 44100, 2, 16, 400, extra=b'LIST' + struct.pack('<I', 3) + b'abc\0')
    wav(folder / 'h_adpcm.wav', 44100, 2, 16, 400, tag=0x69)
    wav(folder / 'i_truncated.wav', 44100, 2, 16, 250, size=0xFFFFFFF0)
    (folder / 'notes.txt').write_text('not music')
    (folder / 'sub.wav').mkdir()                      # a folder named like a track
    wav(other / 'aaa.wav', 44100, 2, 16, 100)


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    include = ROOT / 'src/interface'
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = HEADER.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / 'inc').mkdir()
        (tmp_path / 'inc' / HEADER.name).write_text(text.replace(old, new), encoding='utf-8')
        include = tmp_path / 'inc'
    exe = tmp_path / ('custom_music.exe' if os.name == 'nt' else 'custom_music')
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/I' + str(include), str(FIXTURE), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L', '-D_DEFAULT_SOURCE',
                '-I' + str(include), str(FIXTURE), '-o', str(exe), '-lm']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_custom_music(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    folder, other = tmp_path / 'music', tmp_path / 'other'
    write_folder(folder, other)
    env = {k: v for k, v in os.environ.items() if not k.startswith(('TSFP_', 'RECOMP_'))}
    result = subprocess.run([str(exe), str(folder), str(other)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'custom music ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'
