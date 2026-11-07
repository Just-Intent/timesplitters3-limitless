/* Contract for src/interface/pc_music_codec.c, the imported-music MP3 decoder
 * adapter. Run by tests/media/test_music_codec.py:
 *   music_codec stub <file>    built without FFmpeg: no decoder, the stream is closed
 *   music_codec ffmpeg <dir>   built with FFmpeg: the MP3 files the test wrote */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_custom_music.h"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

static FILE *open_in(const char *dir, const char *name)
{
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    return fopen(path, "rb");
}

static int16_t pcm[4096 * 2];

/* Decodes to the end in odd-sized pieces; returns the frames or -1 on an error.
 * *peak gets the largest |sample|, *anti the share of frames whose two
 * channels are opposite (the test writes the right channel inverted). */
static long drain(PcMusicCodec *c, unsigned channels, int *peak, double *anti)
{
    long total = 0, got, opposite = 0, loud = 0;
    unsigned piece = 1;
    *peak = 0;
    for (;;) {
        long i;
        piece = piece * 7u % 4093u + 1u;             /* 1..4093 frames a call */
        got = pc_music_codec_read(c, pcm, piece);
        if (got < 0) return -1;
        if (!got) break;
        if ((unsigned long)got > piece) return -1;
        for (i = 0; i < got * (long)channels; i++) {
            int v = pcm[i] < 0 ? -pcm[i] : pcm[i];
            if (v > *peak) *peak = v;
        }
        if (channels == 2)
            for (i = 0; i < got; i++) {
                int l = pcm[2 * i], r = pcm[2 * i + 1];
                if (l > 500 || l < -500) { loud++; if ((l > 0) != (r > 0)) opposite++; }
            }
        total += got;
        if (total > 10L * 48000L) return -1;          /* runaway */
    }
    *anti = loud ? (double)opposite / (double)loud : 0.0;
    return total;
}

int main(int argc, char **argv)
{
    uint32_t rate = 0, channels = 0;
    PcMusicCodec *c;
    int peak;
    double anti;
    long a, b;
    if (argc < 3) return 2;
    CHECK(pc_music_codec_open(NULL, &rate, &channels) == NULL);
    CHECK(pc_music_codec_read(NULL, pcm, 16) == -1);
    pc_music_codec_close(NULL);
    if (!strcmp(argv[1], "stub")) {
        FILE *f = fopen(argv[2], "rb");
        CHECK(f != NULL);
        CHECK(!pc_music_codec_available());
        CHECK(pc_music_codec_open(f, &rate, &channels) == NULL);     /* and closed it */
        if (failures) return 1;
        printf("music codec ok\n");
        return 0;
    }
    CHECK(pc_music_codec_available());

    /* 1 s stereo 44.1 kHz, right channel inverted, ID3v2 tag in front */
    CHECK((c = pc_music_codec_open(open_in(argv[2], "stereo.mp3"), &rate, &channels)) != NULL);
    if (c) {
        CHECK(rate == 44100u && channels == 2u);
        a = drain(c, 2, &peak, &anti);
        /* encoder delay and padding are not trimmed: up to four frames more */
        CHECK(a >= 44100 && a <= 44100 + 4 * 1152);
        CHECK(peak > 2000 && peak < 8000);           /* lavfi sine: amplitude 1/8 */
        CHECK(anti > 0.9);
        CHECK(pc_music_codec_read(c, pcm, 64) == 0); /* stays at the end */
        CHECK(pc_music_codec_rewind(c));
        b = drain(c, 2, &peak, &anti);
        CHECK(b == a && anti > 0.9);
        pc_music_codec_close(c);
    }
    /* the same audio behind a 2 MiB ID3v2 tag: past the 1 MiB first-frame probe */
    CHECK((c = pc_music_codec_open(open_in(argv[2], "big_tag.mp3"), &rate, &channels)) != NULL);
    if (c) {
        CHECK(rate == 44100u && channels == 2u);
        a = drain(c, 2, &peak, &anti);
        CHECK(a >= 44100 && a <= 44100 + 4 * 1152 && anti > 0.9);
        CHECK(pc_music_codec_rewind(c) && drain(c, 2, &peak, &anti) == a);
        pc_music_codec_close(c);
    }
    /* mono 22.05 kHz, no tag */
    CHECK((c = pc_music_codec_open(open_in(argv[2], "mono.mp3"), &rate, &channels)) != NULL);
    if (c) {
        CHECK(rate == 22050u && channels == 1u);
        a = drain(c, 1, &peak, &anti);
        CHECK(a >= 11025 && a <= 11025 + 4 * 576 && peak > 2000);
        pc_music_codec_close(c);
    }
    /* the first half of the stereo file: decodes what is there, then ends */
    CHECK((c = pc_music_codec_open(open_in(argv[2], "truncated.mp3"), &rate, &channels)) != NULL);
    if (c) {
        a = drain(c, 2, &peak, &anti);
        CHECK(a > 10000 && a < 40000);
        pc_music_codec_close(c);
    }
    /* mono 22.05 kHz followed by stereo 44.1 kHz: the change of rate ends it */
    CHECK((c = pc_music_codec_open(open_in(argv[2], "rate_change.mp3"), &rate, &channels)) != NULL);
    if (c) {
        CHECK(rate == 22050u && channels == 1u);
        CHECK(drain(c, 1, &peak, &anti) == -1);
        pc_music_codec_close(c);
    }
    /* no audio at all */
    CHECK(pc_music_codec_open(open_in(argv[2], "empty.mp3"), &rate, &channels) == NULL);
    CHECK(pc_music_codec_open(open_in(argv[2], "text.mp3"), &rate, &channels) == NULL);
    CHECK(pc_music_codec_open(open_in(argv[2], "tag_only.mp3"), &rate, &channels) == NULL);
    CHECK(pc_music_codec_open(open_in(argv[2], "late_audio.mp3"), &rate, &channels) == NULL);

    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("music codec ok\n");
    return 0;
}
