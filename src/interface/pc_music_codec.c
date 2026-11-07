/* Host decoder adapter for imported music (pc_custom_music.h,
 * PC_MUSIC_KIND_CODEC): MP3 through FFmpeg's libavcodec, loaded at run time
 * like the XMV movie decoder (runtime src/video/xmv_decode.c) and from the
 * same shipped libraries (avutil, swresample, avcodec). No libavformat: the
 * MPEG audio parser in libavcodec splits the elementary stream, after any
 * leading ID3v2 tags are skipped here. Decoding streams one packet at a time
 * through a 16 KiB input buffer; nothing is loaded whole.
 *
 * Built with FFmpeg's headers when PC_MUSIC_HAVE_FFMPEG is defined (the title
 * CMake adds them where third_party/ffmpeg exists, as for movies); otherwise
 * every entry reports no decoder and only WAV is offered. Libraries are
 * looked for beside the executable / on the loader path, then in
 * RECOMP_FFMPEG_DIR, then in PC_MUSIC_FFMPEG_BIN (this checkout's copy). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_custom_music.h"

#if defined(PC_MUSIC_HAVE_FFMPEG)

#include <errno.h>
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>

#if defined(_WIN32)
#include <windows.h>
typedef HMODULE pmc_lib;
static pmc_lib pmc_open_lib(const char *dir, const char *name)
{
    char path[MAX_PATH * 2];
    if (!dir) return LoadLibraryA(name);
    snprintf(path, sizeof path, "%s\\%s", dir, name);
    return LoadLibraryExA(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
}
static void *pmc_sym(pmc_lib l, const char *s) { return (void *)GetProcAddress(l, s); }
#define PMC_STR2(x) #x
#define PMC_STR(x) PMC_STR2(x)
#define PMC_AVCODEC "avcodec-" PMC_STR(LIBAVCODEC_VERSION_MAJOR) ".dll"
#define PMC_AVUTIL "avutil-" PMC_STR(LIBAVUTIL_VERSION_MAJOR) ".dll"
#define PMC_SWRESAMPLE "swresample-6.dll"
#else
#include <dlfcn.h>
typedef void *pmc_lib;
static pmc_lib pmc_open_lib(const char *dir, const char *name)
{
    char path[4096];
    if (!dir) return dlopen(name, RTLD_NOW | RTLD_GLOBAL);
    snprintf(path, sizeof path, "%s/%s", dir, name);
    return dlopen(path, RTLD_NOW | RTLD_GLOBAL);
}
static void *pmc_sym(pmc_lib l, const char *s) { return dlsym(l, s); }
#define PMC_STR2(x) #x
#define PMC_STR(x) PMC_STR2(x)
#if defined(__APPLE__)
#define PMC_AVCODEC "libavcodec." PMC_STR(LIBAVCODEC_VERSION_MAJOR) ".dylib"
#define PMC_AVUTIL "libavutil." PMC_STR(LIBAVUTIL_VERSION_MAJOR) ".dylib"
#else
#define PMC_AVCODEC "libavcodec.so." PMC_STR(LIBAVCODEC_VERSION_MAJOR)
#define PMC_AVUTIL "libavutil.so." PMC_STR(LIBAVUTIL_VERSION_MAJOR)
#endif
#define PMC_SWRESAMPLE NULL
#endif

#define PMC_INPUT 16384u
#define PMC_PROBE (1u << 20)          /* the first frame must start within 1 MiB of the audio */
#define PMC_BAD_LIMIT 256u            /* consecutive refused packets before the track ends */

static struct {
    int tried, ok;
    const AVCodec *(*find_decoder)(enum AVCodecID);
    AVCodecContext *(*alloc_context3)(const AVCodec *);
    int (*open2)(AVCodecContext *, const AVCodec *, AVDictionary **);
    void (*free_context)(AVCodecContext **);
    void (*flush_buffers)(AVCodecContext *);
    int (*send_packet)(AVCodecContext *, const AVPacket *);
    int (*receive_frame)(AVCodecContext *, AVFrame *);
    AVCodecParserContext *(*parser_init)(int);
    int (*parser_parse2)(AVCodecParserContext *, AVCodecContext *, uint8_t **, int *,
                         const uint8_t *, int, int64_t, int64_t, int64_t);
    void (*parser_close)(AVCodecParserContext *);
    AVPacket *(*packet_alloc)(void);
    void (*packet_free)(AVPacket **);
    AVFrame *(*frame_alloc)(void);
    void (*frame_free)(AVFrame **);
    void (*frame_unref)(AVFrame *);
} pmc;

static int pmc_load(void)
{
    const char *dirs[3];
    pmc_lib codec = NULL, util = NULL;
    int i;
    if (pmc.tried) return pmc.ok;
    pmc.tried = 1;
    dirs[0] = NULL;
    dirs[1] = getenv("RECOMP_FFMPEG_DIR");
#ifdef PC_MUSIC_FFMPEG_BIN
    dirs[2] = PC_MUSIC_FFMPEG_BIN;
#else
    dirs[2] = NULL;
#endif
    for (i = 0; i < 3 && !codec; i++) {
        if (i > 0 && (!dirs[i] || !*dirs[i])) continue;
        util = pmc_open_lib(dirs[i], PMC_AVUTIL);
        if (!util) continue;
        if (PMC_SWRESAMPLE) pmc_open_lib(dirs[i], PMC_SWRESAMPLE);
        codec = pmc_open_lib(dirs[i], PMC_AVCODEC);
    }
    if (!codec || !util) {
        fprintf(stderr, "[music] no FFmpeg (%s, %s): MP3 tracks are not offered\n", PMC_AVCODEC, PMC_AVUTIL);
        fflush(stderr);
        return 0;
    }
#define PMC_LOAD(field, lib, name) \
    if (!(*(void **)&pmc.field = pmc_sym(lib, name))) { \
        fprintf(stderr, "[music] FFmpeg is missing %s: MP3 tracks are not offered\n", name); \
        fflush(stderr); return 0; }
    PMC_LOAD(find_decoder, codec, "avcodec_find_decoder");
    PMC_LOAD(alloc_context3, codec, "avcodec_alloc_context3");
    PMC_LOAD(open2, codec, "avcodec_open2");
    PMC_LOAD(free_context, codec, "avcodec_free_context");
    PMC_LOAD(flush_buffers, codec, "avcodec_flush_buffers");
    PMC_LOAD(send_packet, codec, "avcodec_send_packet");
    PMC_LOAD(receive_frame, codec, "avcodec_receive_frame");
    PMC_LOAD(parser_init, codec, "av_parser_init");
    PMC_LOAD(parser_parse2, codec, "av_parser_parse2");
    PMC_LOAD(parser_close, codec, "av_parser_close");
    PMC_LOAD(packet_alloc, codec, "av_packet_alloc");
    PMC_LOAD(packet_free, codec, "av_packet_free");
    PMC_LOAD(frame_alloc, util, "av_frame_alloc");
    PMC_LOAD(frame_free, util, "av_frame_free");
    PMC_LOAD(frame_unref, util, "av_frame_unref");
#undef PMC_LOAD
    if (!pmc.find_decoder(AV_CODEC_ID_MP3)) {
        fprintf(stderr, "[music] this FFmpeg has no MP3 decoder: MP3 tracks are not offered\n");
        fflush(stderr);
        return 0;
    }
    pmc.ok = 1;
    return 1;
}

struct PcMusicCodec {
    FILE *file;
    long start;                       /* first byte after the ID3v2 tags */
    const AVCodec *codec;
    AVCodecContext *ctx;
    AVCodecParserContext *parser;
    AVPacket *pkt;
    AVFrame *frame;
    int have_frame, frame_pos;        /* a decoded frame partly copied out */
    int input_end, parser_flushed, decoder_flushed;
    unsigned bad;
    size_t in_len, in_pos;
    uint32_t rate, channels;          /* channels: the output's, 1 or 2 */
    uint8_t in[PMC_INPUT + AV_INPUT_BUFFER_PADDING_SIZE];
};

/* Bytes of ID3v2 tags at the start (several may follow each other). */
static long pmc_id3_skip(FILE *file)
{
    long at = 0;
    unsigned steps;
    for (steps = 0; steps < 8; steps++) {
        uint8_t h[10];
        uint32_t size;
        if (fseek(file, at, SEEK_SET) || fread(h, 1, 10, file) != 10) break;
        if (memcmp(h, "ID3", 3) || h[3] == 0xFF || h[4] == 0xFF ||
            (h[6] | h[7] | h[8] | h[9]) & 0x80) break;
        size = (uint32_t)h[6] << 21 | (uint32_t)h[7] << 14 | (uint32_t)h[8] << 7 | h[9];
        at += 10L + (long)size + ((h[5] & 0x10) ? 10L : 0L);
    }
    return at;
}

static int pmc_reset(PcMusicCodec *c)
{
    if (c->parser) pmc.parser_close(c->parser);
    if (!(c->parser = pmc.parser_init(AV_CODEC_ID_MP3))) return 0;
    if (c->ctx) pmc.flush_buffers(c->ctx);
    if (c->frame) pmc.frame_unref(c->frame);
    c->have_frame = c->frame_pos = 0;
    c->input_end = c->parser_flushed = c->decoder_flushed = 0;
    c->bad = 0;
    c->in_len = c->in_pos = 0;
    return fseek(c->file, c->start, SEEK_SET) == 0;
}

static int pmc_planar(int format)
{
    return format == AV_SAMPLE_FMT_U8P || format == AV_SAMPLE_FMT_S16P || format == AV_SAMPLE_FMT_S32P ||
           format == AV_SAMPLE_FMT_FLTP || format == AV_SAMPLE_FMT_DBLP;
}

/* One sample of channel ch, frame i, as s16 (float clamped to -1..1). */
static int16_t pmc_s16(const AVFrame *f, int ch, int i)
{
    const int planar = pmc_planar(f->format);
    const int n = f->ch_layout.nb_channels;
    const uint8_t *base = planar ? f->extended_data[ch] : f->extended_data[0];
    const int at = planar ? i : i * n + ch;
    double x;
    int v;
    switch (f->format) {
    case AV_SAMPLE_FMT_S16: case AV_SAMPLE_FMT_S16P: return ((const int16_t *)base)[at];
    case AV_SAMPLE_FMT_S32: case AV_SAMPLE_FMT_S32P: return (int16_t)(((const int32_t *)base)[at] >> 16);
    case AV_SAMPLE_FMT_U8: case AV_SAMPLE_FMT_U8P: return (int16_t)(((int)base[at] - 128) * 256);
    case AV_SAMPLE_FMT_FLT: case AV_SAMPLE_FMT_FLTP: x = ((const float *)base)[at]; break;
    case AV_SAMPLE_FMT_DBL: case AV_SAMPLE_FMT_DBLP: x = ((const double *)base)[at]; break;
    default: return 0;
    }
    if (!(x == x)) x = 0;
    if (x > 1.0) x = 1.0;
    if (x < -1.0) x = -1.0;
    v = (int)(x * 32767.0 + (x >= 0 ? 0.5 : -0.5));
    return (int16_t)v;
}

/* The next decoded frame into c->frame: 1, 0 at the end, -1 on an error. */
static int pmc_next(PcMusicCodec *c, size_t probe_limit)
{
    size_t fed = 0;
    for (;;) {
        int r = pmc.receive_frame(c->ctx, c->frame);
        if (r == 0) {
            if (c->frame->nb_samples <= 0 || c->frame->ch_layout.nb_channels < 1) { pmc.frame_unref(c->frame); continue; }
            c->bad = 0;
            return 1;
        }
        if (r == AVERROR_EOF) return 0;
        if (r != AVERROR(EAGAIN)) { if (++c->bad > PMC_BAD_LIMIT) return -1; continue; }
        if (c->decoder_flushed) return 0;
        /* the decoder wants a packet: parse one from the input */
        for (;;) {
            uint8_t *data = NULL;
            int size = 0, used;
            if (c->in_pos == c->in_len && !c->input_end) {
                c->in_len = fread(c->in, 1, PMC_INPUT, c->file);
                c->in_pos = 0;
                fed += c->in_len;
                memset(c->in + c->in_len, 0, AV_INPUT_BUFFER_PADDING_SIZE);
                if (!c->in_len) c->input_end = 1;
                if (probe_limit && fed > probe_limit) return -1;
            }
            if (c->in_pos < c->in_len) {
                used = pmc.parser_parse2(c->parser, c->ctx, &data, &size, c->in + c->in_pos,
                                         (int)(c->in_len - c->in_pos), AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
                if (used < 0) return -1;
                c->in_pos += (size_t)used;
            } else if (!c->parser_flushed) {
                pmc.parser_parse2(c->parser, c->ctx, &data, &size, NULL, 0, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
                c->parser_flushed = 1;
            } else {
                pmc.send_packet(c->ctx, NULL);       /* drain what the decoder holds */
                c->decoder_flushed = 1;
                break;
            }
            if (size > 0) {
                c->pkt->data = data;
                c->pkt->size = size;
                r = pmc.send_packet(c->ctx, c->pkt);
                c->pkt->data = NULL;
                c->pkt->size = 0;
                if (r < 0 && r != AVERROR(EAGAIN) && ++c->bad > PMC_BAD_LIMIT) return -1;
                break;
            }
        }
    }
}

int pc_music_codec_available(void) { return pmc_load(); }

PcMusicCodec *pc_music_codec_open(FILE *file, uint32_t *rate, uint32_t *channels)
{
    PcMusicCodec *c;
    if (!file) return NULL;
    if (!pmc_load() || !(c = (PcMusicCodec *)calloc(1, sizeof *c))) { fclose(file); return NULL; }
    c->file = file;
    c->start = pmc_id3_skip(file);
    if (!(c->codec = pmc.find_decoder(AV_CODEC_ID_MP3)) || !(c->ctx = pmc.alloc_context3(c->codec)) ||
        !(c->pkt = pmc.packet_alloc()) || !(c->frame = pmc.frame_alloc()) ||
        pmc.open2(c->ctx, c->codec, NULL) < 0 || !pmc_reset(c) || pmc_next(c, PMC_PROBE) != 1 ||
        c->frame->sample_rate <= 0) {
        pc_music_codec_close(c);
        return NULL;
    }
    c->have_frame = 1;
    c->rate = (uint32_t)c->frame->sample_rate;
    c->channels = c->frame->ch_layout.nb_channels >= 2 ? 2u : 1u;
    if (rate) *rate = c->rate;
    if (channels) *channels = c->channels;
    return c;
}

long pc_music_codec_read(PcMusicCodec *c, int16_t *out, unsigned frames)
{
    unsigned done = 0;
    if (!c || !out) return -1;
    while (done < frames) {
        int i, take, src;
        if (!c->have_frame) {
            int r = pmc_next(c, 0);
            if (r < 0) return done ? (long)done : -1;
            if (r == 0) break;
            if ((uint32_t)c->frame->sample_rate != c->rate) return done ? (long)done : -1;
            c->have_frame = 1;
            c->frame_pos = 0;
        }
        take = c->frame->nb_samples - c->frame_pos;
        if ((unsigned)take > frames - done) take = (int)(frames - done);
        src = c->frame->ch_layout.nb_channels;
        for (i = 0; i < take; i++) {
            int at = c->frame_pos + i;
            int16_t *o = out + (size_t)(done + (unsigned)i) * c->channels;
            o[0] = pmc_s16(c->frame, 0, at);
            if (c->channels == 2) o[1] = pmc_s16(c->frame, src >= 2 ? 1 : 0, at);
        }
        done += (unsigned)take;
        c->frame_pos += take;
        if (c->frame_pos >= c->frame->nb_samples) { pmc.frame_unref(c->frame); c->have_frame = 0; }
    }
    return (long)done;
}

int pc_music_codec_rewind(PcMusicCodec *c) { return c && pmc_reset(c); }

void pc_music_codec_close(PcMusicCodec *c)
{
    if (!c) return;
    if (c->parser) pmc.parser_close(c->parser);
    if (c->ctx) pmc.free_context(&c->ctx);
    if (c->pkt) pmc.packet_free(&c->pkt);
    if (c->frame) pmc.frame_free(&c->frame);
    if (c->file) fclose(c->file);
    free(c);
}

#else /* no FFmpeg headers at build time: WAV only */

int pc_music_codec_available(void) { return 0; }
PcMusicCodec *pc_music_codec_open(FILE *file, uint32_t *rate, uint32_t *channels)
{
    (void)rate; (void)channels;
    if (file) fclose(file);
    return NULL;
}
long pc_music_codec_read(PcMusicCodec *codec, int16_t *out, unsigned frames) { (void)codec; (void)out; (void)frames; return -1; }
int pc_music_codec_rewind(PcMusicCodec *codec) { (void)codec; return 0; }
void pc_music_codec_close(PcMusicCodec *codec) { (void)codec; }

#endif
