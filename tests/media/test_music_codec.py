"""Imported-music MP3 adapter (src/interface/pc_music_codec.c), fixture
tests/media/music_codec.c. The stub build (no FFmpeg headers, the macOS case
today) always runs. The FFmpeg build runs where the pinned FFmpeg package is
unpacked (tools/build/fetch-video.ps1: external/ts3FPrecomp/third_party/ffmpeg,
or TSFP_FFMPEG_DIR): its ffmpeg executable writes the MP3 files here, and
mutations of the adapter must be caught. No game data or audio device."""
import os
import subprocess
from pathlib import Path

import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'src/interface/pc_music_codec.c'
FIXTURE = ROOT / 'tests/media/music_codec.c'

MUTATIONS = {
    'id3-not-skipped': ('    c->start = pmc_id3_skip(file);', '    c->start = 0;'),
    'id3-footer-ignored': ("        at += 10L + (long)size + ((h[5] & 0x10) ? 10L : 0L);", '        at += (long)size;'),
    'probe-unbounded': ('                if (probe_limit && fed > probe_limit) return -1;\n', ''),
    'rate-change-accepted': ('            if ((uint32_t)c->frame->sample_rate != c->rate) return done ? (long)done : -1;\n', ''),
    'planar-first-only': ('    const uint8_t *base = planar ? f->extended_data[ch] : f->extended_data[0];',
                          '    const uint8_t *base = f->extended_data[0];'),
    'right-from-left': ('            if (c->channels == 2) o[1] = pmc_s16(c->frame, src >= 2 ? 1 : 0, at);',
                        '            if (c->channels == 2) o[1] = pmc_s16(c->frame, 0, at);'),
    'rewind-keeps-position': ('    return fseek(c->file, c->start, SEEK_SET) == 0;', '    return 1;'),
    'partial-frame-dropped': ('        c->frame_pos += take;\n        if (c->frame_pos >= c->frame->nb_samples)',
                              '        c->frame_pos += take;\n        if (1)'),
}


def ffmpeg_dir():
    configured = os.environ.get('TSFP_FFMPEG_DIR')
    path = Path(configured) if configured else ROOT / 'external/ts3FPrecomp/third_party/ffmpeg'
    return path if (path / 'include/libavcodec/avcodec.h').is_file() else None


def build(tmp_path, mutation, ffmpeg):
    compiler, env = native_compiler(tmp_path)
    source = SOURCE
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = SOURCE.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        source = tmp_path / SOURCE.name
        source.write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('music_codec.exe' if os.name == 'nt' else 'music_codec')
    include = ROOT / 'src/interface'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/I' + str(include)]
        if ffmpeg:
            args += ['/I' + str(ffmpeg / 'include'), '/DPC_MUSIC_HAVE_FFMPEG',
                     '/DPC_MUSIC_FFMPEG_BIN="' + (ffmpeg / 'bin').as_posix() + '"']
        args += [str(FIXTURE), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L', '-I' + str(include)]
        if ffmpeg:
            args += ['-I' + str(ffmpeg / 'include'), '-DPC_MUSIC_HAVE_FFMPEG',
                     '-DPC_MUSIC_FFMPEG_BIN="' + str(ffmpeg / 'lib') + '"']
        args += [str(FIXTURE), str(source), '-o', str(exe), '-ldl']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=180)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


def encode(ffmpeg, out, *args):
    tool = ffmpeg / 'bin' / ('ffmpeg.exe' if os.name == 'nt' else 'ffmpeg')
    subprocess.run([str(tool), '-hide_banner', '-loglevel', 'error', '-y', *args, str(out)],
                   check=True, capture_output=True, timeout=120)


def id3(size, footer=False, fill=b''):
    syncsafe = bytes([(size >> 21) & 0x7F, (size >> 14) & 0x7F, (size >> 7) & 0x7F, size & 0x7F])
    body = b'PRIV' + (size - 10).to_bytes(4, 'big') + bytes(2) + fill + bytes(size - 10 - len(fill))
    tag = b'ID3' + bytes([4, 0, 0x10 if footer else 0]) + syncsafe + body
    return tag + (b'3DI' + bytes([4, 0, 0x10]) + syncsafe if footer else b'')


def write_files(folder, ffmpeg):
    folder.mkdir()
    sine = ['-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=44100:duration=1']
    encode(ffmpeg, folder / 'stereo.mp3', *sine, '-af', 'pan=stereo|c0=c0|c1=-1*c0', '-c:a', 'libmp3lame',
           '-b:a', '128k', '-metadata', 'title=Test tone', '-id3v2_version', '3')
    plain = folder / 'plain.mp3'
    encode(ffmpeg, plain, *sine, '-af', 'pan=stereo|c0=c0|c1=-1*c0', '-c:a', 'libmp3lame', '-b:a', '128k',
           '-id3v2_version', '0', '-write_xing', '0')
    encode(ffmpeg, folder / 'mono.mp3', '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=22050:duration=0.5',
           '-c:a', 'libmp3lame', '-b:a', '64k', '-id3v2_version', '0', '-write_xing', '0')
    mono = (folder / 'mono.mp3').read_bytes()
    # 2 MiB of tag, then a tag with a footer whose body holds 22.05 kHz frames
    # (a wrong skip decodes those first), then the 44.1 kHz audio
    (folder / 'big_tag.mp3').write_bytes(id3(2 << 20) + id3(len(mono) + 64, footer=True, fill=mono) + plain.read_bytes())
    # audio that starts past the 1 MiB first-frame probe is refused
    (folder / 'late_audio.mp3').write_bytes(bytes(3 << 19) + plain.read_bytes())
    data = (folder / 'stereo.mp3').read_bytes()
    (folder / 'truncated.mp3').write_bytes(data[:len(data) // 2])
    (folder / 'rate_change.mp3').write_bytes((folder / 'mono.mp3').read_bytes() + plain.read_bytes())
    (folder / 'empty.mp3').write_bytes(b'')
    (folder / 'text.mp3').write_bytes(bytes(65536) + b'not an mp3 file\n' * 4096)
    (folder / 'tag_only.mp3').write_bytes(id3(8192))


def run(exe, *args):
    env = {k: v for k, v in os.environ.items() if not k.startswith(('TSFP_', 'RECOMP_'))}
    return subprocess.run([str(exe), *args], env=env, capture_output=True, text=True, timeout=120)


def test_music_codec_stub(tmp_path):
    exe = build(tmp_path, 'fixed', None)
    track = tmp_path / 'song.mp3'
    track.write_bytes(bytes([0xFF, 0xFB]) + bytes(1000))
    result = run(exe, 'stub', str(track))
    assert result.returncode == 0 and 'music codec ok' in result.stdout, result.stdout + result.stderr


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_music_codec_ffmpeg(tmp_path, mutation):
    ffmpeg = ffmpeg_dir()
    if not ffmpeg:
        pytest.skip('pinned FFmpeg package not unpacked (tools/build/fetch-video.ps1)')
    if os.name != 'nt':
        pytest.skip('the pinned FFmpeg package is the Windows build')
    exe = build(tmp_path, mutation, ffmpeg)
    folder = tmp_path / 'music'
    write_files(folder, ffmpeg)
    result = run(exe, 'ffmpeg', str(folder))
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'music codec ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'
