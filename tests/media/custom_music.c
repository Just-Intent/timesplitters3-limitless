/* Asset-free contract for src/interface/pc_custom_music.h: the folder listing
 * (stable indexes, name order, rescans), labels, every WAV form the importer
 * accepts and the ones it refuses, 16-bit conversion, the player's pacing,
 * looping, retry of a refused submission, pause, the watch ending a track,
 * the routing of .mp3 files to the decoder adapter (a fake one here, reading
 * "FAKE" files; the real one is tests/media/music_codec.c) and the Arcade
 * choice file. Run by tests/media/test_custom_music.py, which writes the WAV
 * and FAKE files into argv[1] first. */
#define PC_CUSTOM_MUSIC_TEST 1
#define PC_CUSTOM_MUSIC_IMPLEMENTATION 1
#include "pc_custom_music.h"
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

/* ---- fake output ---- */
static uint64_t submitted_bytes, played_bytes;
static unsigned submits, refuse_next, resets, volumes;
static uint32_t refused_bytes, retried_bytes;
static int32_t last_volume = 12345, submit_volume;
static int16_t first_sample[2];
static uint32_t last_rate, last_channels;
int pc_music_test_submit(const uint8_t *pcm, uint32_t bytes, uint32_t rate, uint32_t channels, int32_t volume)
{
    if (refuse_next) { refuse_next--; refused_bytes = bytes; return 0; }
    if (refused_bytes && !retried_bytes) retried_bytes = bytes;
    if (!submits) memcpy(first_sample, pcm, sizeof first_sample);
    submits++; submitted_bytes += bytes; last_rate = rate; last_channels = channels; submit_volume = volume;
    return 1;
}
int pc_music_test_position(uint64_t *played, uint32_t *queued) { *played = played_bytes; *queued = 0; return 1; }
void pc_music_test_volume(int32_t volume) { last_volume = volume; volumes++; }
void pc_music_test_reset(void) { resets++; played_bytes = submitted_bytes = 0; }

/* ---- fake decoder adapter: "FAKE" u32 rate, u32 channels, u32 frames, u32 fail_at ---- */
struct PcMusicCodec { FILE *file; uint32_t rate, channels, frames, fail_at, at; };
static int codec_available, codec_streams, codec_rewinds;
static int fake_close(FILE *f) { if (f) { codec_streams--; return fclose(f); } return 0; }
int pc_music_codec_available(void) { return codec_available; }
PcMusicCodec *pc_music_codec_open(FILE *file, uint32_t *rate, uint32_t *channels)
{
    uint8_t h[20];
    PcMusicCodec *c;
    if (!file) return NULL;
    codec_streams++;
    if (!codec_available || fread(h, 1, 20, file) != 20 || memcmp(h, "FAKE", 4) || !(c = calloc(1, sizeof *c))) {
        fake_close(file);
        return NULL;
    }
    c->file = file;
    memcpy(&c->rate, h + 4, 4); memcpy(&c->channels, h + 8, 4); memcpy(&c->frames, h + 12, 4); memcpy(&c->fail_at, h + 16, 4);
    *rate = c->rate; *channels = c->channels;
    return c;
}
long pc_music_codec_read(PcMusicCodec *c, int16_t *out, unsigned frames)
{
    unsigned k, j;
    if (c->fail_at && c->at >= c->fail_at) return -1;
    if (frames > c->frames - c->at) frames = c->frames - c->at;
    for (k = 0; k < frames; k++)
        for (j = 0; j < c->channels; j++) out[k * c->channels + j] = (int16_t)((c->at + k) * (j ? -1 : 1));
    c->at += frames;
    return (long)frames;
}
int pc_music_codec_rewind(PcMusicCodec *c) { codec_rewinds++; c->at = 0; return 1; }
void pc_music_codec_close(PcMusicCodec *c) { if (c) { fake_close(c->file); free(c); } }

static int watch_result = 1, watch_paused, watch_calls;
static float watch_gain = 1.0f;
static int watch(void *context, float *gain, int *paused)
{
    (void)context; watch_calls++;
    *gain = watch_gain; *paused = watch_paused;
    return watch_result;
}

static void set_env(const char *name, const char *value)
{
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

static int parse(const char *dir, const char *name, PcWavInfo *info)
{
    char path[4096];
    FILE *f;
    int ok;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    if (!(f = fopen(path, "rb"))) return -1;
    ok = pc_wav_parse(f, info);
    fclose(f);
    return ok;
}

int main(int argc, char **argv)
{
    PcMusicList list;
    PcWavInfo info;
    char label[64], ini[4200], name[PC_MUSIC_NAME_MAX];
    const char *dir;
    unsigned n, k;
    if (argc < 2) return 2;
    dir = argv[1];
    memset(&list, 0, sizeof list);

    /* names */
    CHECK(pc_music_name_ok("a.wav") && pc_music_name_ok("Song Two.WAV"));
    CHECK(pc_music_name_ok("x.mp3") && pc_music_name_ok("Y.MP3") && pc_music_kind("x.Mp3") == PC_MUSIC_KIND_CODEC &&
          pc_music_kind("x.wav") == PC_MUSIC_KIND_WAV && pc_music_kind("x.ogg") == PC_MUSIC_KIND_NONE);
    CHECK(!pc_music_name_ok(".wav") && !pc_music_name_ok("x.ogg") && !pc_music_name_ok(".mp3") && !pc_music_name_ok("a/b.wav") &&
          !pc_music_name_ok("a\\b.wav") && !pc_music_name_ok(".hidden.wav") && !pc_music_name_ok(NULL));
    {
        char longname[80];
        memset(longname, 'a', 59); strcpy(longname + 59, ".wav");     /* 63 bytes: fits */
        CHECK(pc_music_name_ok(longname));
        memset(longname, 'a', 60); strcpy(longname + 60, ".wav");     /* 64: does not */
        CHECK(!pc_music_name_ok(longname));
    }
    pc_music_label("My_Song.wav", label, sizeof label); CHECK(!strcmp(label, "My Song"));
    pc_music_label("Other_Song.MP3", label, sizeof label); CHECK(!strcmp(label, "Other Song"));
    pc_music_label("caf\xC3\xA9.WAV", label, sizeof label); CHECK(!strcmp(label, "caf?"));
    pc_music_label("0123456789012345678901234567890123456789012345.wav", label, sizeof label);
    CHECK(strlen(label) == PC_MUSIC_LABEL_MAX - 1u);
    CHECK(pc_music_db(1.0f) == 0 && pc_music_db(0.0f) == -10000 && pc_music_db(0.5f) == -602 && pc_music_db(0.1f) == -2000);

    /* listing: the test wrote b16.wav, a8.wav, ..., notes.txt, a sub folder */
    n = pc_music_scan_dir(&list, dir);
    CHECK(n == 9u && list.count == 9u);
    CHECK(!strcmp(list.name[0], "a8.wav") && !strcmp(list.name[1], "B16.wav"));
    for (k = 1; k < list.count; k++) {
        char x[PC_MUSIC_NAME_MAX], y[PC_MUSIC_NAME_MAX]; unsigned j;
        for (j = 0; list.name[k - 1][j]; j++) x[j] = (char)pc_music_lower(list.name[k - 1][j]); x[j] = 0;
        for (j = 0; list.name[k][j]; j++) y[j] = (char)pc_music_lower(list.name[k][j]); y[j] = 0;
        CHECK(strcmp(x, y) < 0);
    }
    CHECK(pc_music_find(&list, "b16.WAV") == 1 && pc_music_find(&list, "notes.txt") == -1 && pc_music_find(&list, "") == -1);
    /* a rescan of another folder: old entries keep their index but are missing */
    n = pc_music_scan_dir(&list, argv[2]);
    CHECK(n == 1u && list.count == 10u && !list.present[0] && pc_music_find(&list, "a8.wav") == -1);
    CHECK(!strcmp(list.name[9], "aaa.wav") && pc_music_find(&list, "aaa.wav") == 9);
    n = pc_music_scan_dir(&list, dir);
    CHECK(n == 9u && list.count == 10u && pc_music_find(&list, "a8.wav") == 0 && pc_music_find(&list, "aaa.wav") == -1);
    CHECK(pc_music_scan_dir(&list, NULL) == 0u && list.count == 10u);
    /* .mp3 files (z_*.mp3) are listed only while the host has a decoder */
    codec_available = 1;
    n = pc_music_scan_dir(&list, dir);
    CHECK(n == 14u && list.count == 15u && pc_music_find(&list, "z_fake.mp3") >= 10 && pc_music_find(&list, "a8.wav") == 0);
    codec_available = 0;
    n = pc_music_scan_dir(&list, dir);
    CHECK(n == 9u && list.count == 15u && pc_music_find(&list, "z_fake.mp3") == -1);

    /* formats */
    CHECK(parse(dir, "a8.wav", &info) == 1 && info.bits == 8 && info.channels == 1 && info.rate == 22050);
    CHECK(parse(dir, "B16.wav", &info) == 1 && info.bits == 16 && info.channels == 2 && info.rate == 44100);
    CHECK(info.data_bytes == 4410u * 4u);
    CHECK(parse(dir, "c24.wav", &info) == 1 && info.bits == 24);
    CHECK(parse(dir, "d32.wav", &info) == 1 && info.bits == 32 && !info.is_float);
    CHECK(parse(dir, "e_float.wav", &info) == 1 && info.is_float && info.bits == 32);
    CHECK(parse(dir, "f_ext.wav", &info) == 1 && info.channels == 6 && info.bits == 16);
    CHECK(parse(dir, "g_list.wav", &info) == 1 && info.data_bytes == 400u * 4u);   /* LIST chunk before data, odd pad */
    CHECK(parse(dir, "h_adpcm.wav", &info) == 0);
    CHECK(parse(dir, "i_truncated.wav", &info) == 1 && info.data_bytes == 1000u);  /* size past the end */
    {
        uint8_t raw[24];
        int16_t out[4];
        float f[2] = {1.5f, -0.5f};
        PcWavInfo w = {0};
        w.channels = 2; w.bits = 24; w.block = 6;
        raw[0] = 0x12; raw[1] = 0x34; raw[2] = 0x56;         /* 0x563412 -> 0x5634 */
        raw[3] = 0; raw[4] = 0x00; raw[5] = 0xC0;            /* -0x400000 -> -0x4000 */
        CHECK(pc_wav_to_s16(&w, raw, 1, out) == 2u && out[0] == 0x5634 && out[1] == -0x4000);
        w.bits = 32; w.is_float = 1; w.block = 8;
        memcpy(raw, f, 8);
        CHECK(pc_wav_to_s16(&w, raw, 1, out) == 2u && out[0] == 32767 && out[1] == -16384);
        w.channels = 1; w.bits = 8; w.block = 1; w.is_float = 0; raw[0] = 0; raw[1] = 255;
        CHECK(pc_wav_to_s16(&w, raw, 2, out) == 1u && out[0] == -32768 && out[1] == 32512);
        w.channels = 6; w.bits = 16; w.block = 12;
        for (k = 0; k < 6; k++) { raw[2 * k] = (uint8_t)(k + 1); raw[2 * k + 1] = 0; }
        CHECK(pc_wav_to_s16(&w, raw, 1, out) == 2u && out[0] == 1 && out[1] == 2);
    }

    /* player */
    set_env("TSFP_MUSIC_DIR", dir);
    CHECK(!pc_music_start("h_adpcm.wav", watch, NULL) && !pc_music_playing());
    CHECK(!pc_music_start("missing.wav", watch, NULL));
    CHECK(!pc_music_start("B16.wav", NULL, NULL));
    CHECK(pc_music_start("B16.wav", watch, NULL) && pc_music_playing());     /* 4410 frames: 100 ms */
    pc_music_pump(0);
    CHECK(last_volume == pc_music_db(1.0f) && last_rate == 44100u && last_channels == 2u);   /* full slider: 0 dB */
    CHECK(first_sample[0] == 1000 && first_sample[1] == -1000);              /* the file's first frame */
    /* 250 ms lead at 44.1 kHz = 11025 frames: the 100 ms file loops to fill it */
    CHECK(submitted_bytes / 4u >= 11025u && submitted_bytes / 4u < 11025u + 4410u);
    k = submits;
    pc_music_pump(15);                                                      /* nothing heard yet: no more */
    CHECK(submits == k);
    played_bytes = submitted_bytes;                                         /* all heard: refill */
    refuse_next = 1;
    pc_music_pump(30);
    CHECK(submits == k);                                                    /* refused, stopped this step */
    pc_music_pump(45);
    CHECK(submits > k && refused_bytes && retried_bytes == refused_bytes);   /* the same chunk offered again */
    watch_gain = 0.0f;
    pc_music_pump(60);
    CHECK(last_volume == -10000);
    watch_gain = 1.0f; watch_paused = 1; k = submits; played_bytes = submitted_bytes;
    pc_music_pump(75);
    CHECK(last_volume == -10000 && submits == k && pc_music_playing());
    watch_paused = 0; n = resets;
    pc_music_pump(90);
    CHECK(resets == n + 1u && submits > k && last_volume == pc_music_db(1.0f));
    watch_result = 0; n = resets;
    pc_music_pump(105);
    CHECK(!pc_music_playing() && resets == n + 1u);
    watch_result = 1;
    CHECK(pc_music_start("a8.wav", watch, NULL));
    pc_music_pump(200);
    CHECK(last_channels == 1u && last_rate == 22050u);
    n = resets;
    pc_music_stop();
    CHECK(!pc_music_playing() && resets == n + 1u);
    pc_music_stop();                                                        /* twice is harmless */

    /* compressed tracks: the decoder adapter, never the WAV reader */
    CHECK(!pc_music_start("z_fake.mp3", watch, NULL) && codec_streams == 0); /* no decoder */
    codec_available = 1;
    CHECK(!pc_music_start("z_bad.mp3", watch, NULL) && codec_streams == 0);  /* decoder refuses: stream closed */
    CHECK(!pc_music_start("z_slow.mp3", watch, NULL) && codec_streams == 0); /* 4 kHz: out of range */
    submits = 0; submitted_bytes = played_bytes = 0; codec_rewinds = 0;
    CHECK(pc_music_start("z_fake.mp3", watch, NULL) && codec_streams == 1);  /* 32 kHz stereo, 1000 frames */
    pc_music_pump(300);
    CHECK(last_rate == 32000u && last_channels == 2u);
    CHECK(first_sample[0] == 0 && first_sample[1] == 0);
    /* 250 ms at 32 kHz = 8000 frames from a 1000-frame track: it loops */
    CHECK(submitted_bytes / 4u >= 8000u && submitted_bytes / 4u < 8000u + 4096u && codec_rewinds >= 7);
    pc_music_stop();
    CHECK(codec_streams == 0);
    CHECK(pc_music_start("z_broken.mp3", watch, NULL));                     /* an error after 600 frames */
    pc_music_pump(400);
    CHECK(!pc_music_playing() && codec_streams == 0);
    pc_music_stop();
    CHECK(pc_music_start("z_empty.mp3", watch, NULL));                      /* no frames at all: ends, no spin */
    pc_music_pump(500);
    CHECK(!pc_music_playing() && codec_streams == 0 && codec_rewinds < 1000);
    pc_music_stop();
    codec_available = 0;

    /* Arcade choice beside the display settings */
    snprintf(ini, sizeof ini, "%s/pc-display.ini", argv[2]);
    set_env("TSFP_DISPLAY_CONFIG", ini);
    CHECK(!pc_music_arcade_load(name, sizeof name) && !name[0]);
    CHECK(pc_music_arcade_save("B16.wav") && pc_music_arcade_load(name, sizeof name) && !strcmp(name, "B16.wav"));
    {
        FILE *f;
        snprintf(ini, sizeof ini, "%s/pc-music.ini", argv[2]);
        CHECK((f = fopen(ini, "rb")) != NULL);
        if (f) fclose(f);
    }
    CHECK(!pc_music_arcade_save("bad/name.wav"));
    CHECK(pc_music_arcade_load(name, sizeof name) && !strcmp(name, "B16.wav"));
    CHECK(pc_music_arcade_save("") && !pc_music_arcade_load(name, sizeof name) && !name[0]);
    CHECK(pc_music_dir(name, 4) == 0);

    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("custom music ok\n");
    return 0;
}
