#ifndef PC_CUSTOM_MUSIC_H
#define PC_CUSTOM_MUSIC_H
/* Imported music: the player's own WAV files, played on a host audio slot in
 * place of a level's music (docs/mapmaker/mapmaker-custom-music.md).
 *
 * Shared Windows/macOS code. The folder is TSFP_MUSIC_DIR (the launcher sets
 * it to the install's Music folder) or private/music under the working
 * directory. Only files ending in .wav are listed: RIFF/WAVE PCM 8/16/24/32-bit
 * integer or 32/64-bit float (also WAVE_FORMAT_EXTENSIBLE), 1..8 channels
 * (more than two keep the front pair), 8000..192000 Hz. WAV needs no codec,
 * so both hosts read it with this file alone; other formats are converted
 * before import (the launcher's FFmpeg). Files are streamed, never loaded whole.
 *
 * The player runs one host thread that keeps about a quarter second queued on
 * RECOMP_AUDIO_SLOT_MUSIC (0059-custom-music-voice.patch) and asks a watch
 * callback, on that thread, for the gain and pause state; the callback ends
 * the track by returning 0. Define PC_CUSTOM_MUSIC_IMPLEMENTATION in exactly
 * one translation unit. PC_CUSTOM_MUSIC_TEST replaces the thread and the
 * output with calls a fixture drives (pc_music_pump). */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define PC_MUSIC_MAX_TRACKS 64u
#define PC_MUSIC_NAME_MAX 64u        /* file name bytes with the NUL: what a map stores */
#define PC_MUSIC_LABEL_MAX 40u
#define PC_MUSIC_SLOT 273u           /* RECOMP_AUDIO_SLOT_MUSIC */
#define PC_MUSIC_LEVEL 0.65f         /* the stock tracks' own level (0x00535858: 60..70 of 100) */

typedef struct PcMusicList {
    unsigned count;
    char name[PC_MUSIC_MAX_TRACKS][PC_MUSIC_NAME_MAX];
    unsigned char present[PC_MUSIC_MAX_TRACKS];
} PcMusicList;

typedef struct PcWavInfo {
    uint32_t rate, channels, bits, block;
    int is_float;
    uint64_t data_offset, data_bytes;
} PcWavInfo;

/* 1 when the folder path fits. */
int pc_music_dir(char *out, size_t bytes);
/* Entries stay at their index for the process (a map or menu value names an
 * index): a rescan marks every entry missing, then marks or appends what the
 * folder holds now. Returns the number present. */
unsigned pc_music_scan(PcMusicList *list);
unsigned pc_music_scan_dir(PcMusicList *list, const char *dir);
/* Index of a present entry with this name (case-insensitive), or -1. */
int pc_music_find(const PcMusicList *list, const char *name);
/* A name a map may store: a .wav file name, no path, printable, short enough. */
int pc_music_name_ok(const char *name);
/* Menu text: the name without .wav, '_' as spaces, ASCII only, at most
 * PC_MUSIC_LABEL_MAX - 1 bytes. */
void pc_music_label(const char *name, char *out, size_t bytes);

/* 1 when the stream holds playable WAV data; the file is left anywhere. */
int pc_wav_parse(FILE *file, PcWavInfo *info);
/* Converts whole frames to signed 16-bit (one or two channels); returns the
 * channels written per frame. */
unsigned pc_wav_to_s16(const PcWavInfo *info, const uint8_t *in, size_t frames, int16_t *out);
/* Linear gain (0..1) to the output's hundredths of a dB. */
int32_t pc_music_db(float gain);

/* The watch runs on the player thread about every 15 ms: return 0 to end the
 * track, else set *gain (0..1) and *paused. It may read guest memory only. */
typedef int (*PcMusicWatch)(void *context, float *gain, int *paused);
int pc_music_start(const char *name, PcMusicWatch watch, void *context);
void pc_music_stop(void);
int pc_music_playing(void);

/* The Arcade choice (a track name, "" for none) beside the display settings
 * (pc-music.ini, like pc-audio.ini). */
int pc_music_arcade_load(char *name, size_t bytes);
int pc_music_arcade_save(const char *name);

#ifdef PC_CUSTOM_MUSIC_TEST
/* Test hooks: one player step at the given milliseconds. */
void pc_music_pump(uint64_t now_ms);
#endif

#ifdef PC_CUSTOM_MUSIC_IMPLEMENTATION
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif

#ifndef PC_CUSTOM_MUSIC_TEST
int recomp_audio_output_submit(uint32_t slot, const uint8_t *pcm, uint32_t bytes,
    uint32_t sample_rate, uint32_t channels, uint32_t bits_per_sample, int32_t volume_hundredth_db);
int recomp_audio_output_position(uint32_t slot, uint64_t *played_bytes, uint32_t *queued_buffers);
int recomp_audio_output_set_volume(uint32_t slot, int32_t volume_hundredth_db);
void recomp_audio_output_reset_voice(uint32_t slot);
int recomp_audio_output_take_discontinuity(uint32_t slot);
#endif

static int pc_music_lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int pc_music_casecmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = pc_music_lower((unsigned char)*a), y = pc_music_lower((unsigned char)*b);
        if (x != y || !x) return x - y;
    }
}

int pc_music_dir(char *out, size_t bytes)
{
    const char *dir = getenv("TSFP_MUSIC_DIR");
    if (!dir || !*dir) dir = "private/music";
    if (!out || strlen(dir) >= bytes) return 0;
    strcpy(out, dir);
    return 1;
}

int pc_music_name_ok(const char *name)
{
    size_t n = name ? strlen(name) : 0, k;
    if (n < 5 || n >= PC_MUSIC_NAME_MAX || name[0] == '.') return 0;
    if (pc_music_casecmp(name + n - 4, ".wav")) return 0;
    for (k = 0; k < n; k++) {
        unsigned char c = (unsigned char)name[k];
        if (c < 0x20 || c == 0x7F || c == '/' || c == '\\' || c == ':') return 0;
    }
    return 1;
}

static void pc_music_add(PcMusicList *list, const char *name)
{
    unsigned k;
    if (!pc_music_name_ok(name)) return;
    for (k = 0; k < list->count; k++)
        if (!pc_music_casecmp(list->name[k], name)) { list->present[k] = 1; return; }
    if (list->count >= PC_MUSIC_MAX_TRACKS) return;
    strcpy(list->name[list->count], name);
    list->present[list->count++] = 1;
}

unsigned pc_music_scan_dir(PcMusicList *list, const char *dir)
{
    unsigned k, n = 0, first;
    if (!list) return 0;
    for (k = 0; k < list->count; k++) list->present[k] = 0;
    first = list->count;
    if (dir && *dir) {
#ifdef _WIN32
        wchar_t pattern[MAX_PATH + 8];
        WIN32_FIND_DATAW found;
        HANDLE h;
        int w = MultiByteToWideChar(CP_UTF8, 0, dir, -1, pattern, MAX_PATH);
        if (w > 0 && w < MAX_PATH - 3) {
            wcscat(pattern, L"\\*");
            if ((h = FindFirstFileW(pattern, &found)) != INVALID_HANDLE_VALUE) {
                do {
                    char name[PC_MUSIC_NAME_MAX * 4];
                    if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                    if (WideCharToMultiByte(CP_UTF8, 0, found.cFileName, -1, name, (int)sizeof name, NULL, NULL) > 0)
                        pc_music_add(list, name);
                } while (FindNextFileW(h, &found));
                FindClose(h);
            }
        }
#else
        DIR *d = opendir(dir);
        struct dirent *e;
        if (d) {
            while ((e = readdir(d)) != NULL) {
                char path[4096 + 256];
                struct stat s;
                snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
                if (!stat(path, &s) && S_ISREG(s.st_mode)) pc_music_add(list, e->d_name);
            }
            closedir(d);
        }
#endif
    }
    /* New entries in name order, so a folder lists the same way on both hosts. */
    for (k = first + 1; k < list->count; k++) {
        unsigned j = k;
        while (j > first && pc_music_casecmp(list->name[j - 1], list->name[j]) > 0) {
            char t[PC_MUSIC_NAME_MAX];
            memcpy(t, list->name[j], sizeof t);
            memcpy(list->name[j], list->name[j - 1], sizeof t);
            memcpy(list->name[j - 1], t, sizeof t);
            j--;
        }
    }
    for (k = 0; k < list->count; k++) n += list->present[k];
    return n;
}

unsigned pc_music_scan(PcMusicList *list)
{
    char dir[4096];
    return pc_music_scan_dir(list, pc_music_dir(dir, sizeof dir) ? dir : NULL);
}

int pc_music_find(const PcMusicList *list, const char *name)
{
    unsigned k;
    if (!list || !name || !*name) return -1;
    for (k = 0; k < list->count; k++)
        if (list->present[k] && !pc_music_casecmp(list->name[k], name)) return (int)k;
    return -1;
}

void pc_music_label(const char *name, char *out, size_t bytes)
{
    size_t n = strlen(name), k, o = 0, cap = bytes < PC_MUSIC_LABEL_MAX ? bytes : PC_MUSIC_LABEL_MAX;
    if (!cap) return;
    if (n >= 4 && !pc_music_casecmp(name + n - 4, ".wav")) n -= 4;
    for (k = 0; k < n && o + 1 < cap; k++) {
        unsigned char c = (unsigned char)name[k];
        if (c >= 0x80) {                       /* one '?' per UTF-8 character */
            if ((c & 0xC0) == 0x80) continue;
            c = '?';
        }
        out[o++] = c == '_' ? ' ' : (char)c;
    }
    out[o] = 0;
}

static uint32_t pc_wav_u32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t pc_wav_u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

int pc_wav_parse(FILE *file, PcWavInfo *info)
{
    uint8_t head[12], chunk[8], fmt[40];
    uint64_t pos = 12;
    int have_fmt = 0;
    unsigned steps;
    memset(info, 0, sizeof *info);
    if (!file || fseek(file, 0, SEEK_SET) || fread(head, 1, 12, file) != 12) return 0;
    if (memcmp(head, "RIFF", 4) || memcmp(head + 8, "WAVE", 4)) return 0;
    for (steps = 0; steps < 64; steps++) {
        uint32_t size;
        if (fseek(file, (long)pos, SEEK_SET) || fread(chunk, 1, 8, file) != 8) return 0;
        size = pc_wav_u32(chunk + 4);
        if (!memcmp(chunk, "fmt ", 4)) {
            uint16_t tag;
            if (size < 16 || fread(fmt, 1, size < sizeof fmt ? size : sizeof fmt, file) < 16) return 0;
            tag = pc_wav_u16(fmt);
            info->channels = pc_wav_u16(fmt + 2);
            info->rate = pc_wav_u32(fmt + 4);
            info->block = pc_wav_u16(fmt + 12);
            info->bits = pc_wav_u16(fmt + 14);
            if (tag == 0xFFFE && size >= 40) tag = pc_wav_u16(fmt + 24);   /* extensible: sub-format */
            if (tag == 3) info->is_float = 1;
            else if (tag != 1) return 0;
            have_fmt = 1;
        } else if (!memcmp(chunk, "data", 4)) {
            if (!have_fmt) return 0;
            info->data_offset = pos + 8;
            info->data_bytes = size;
            break;
        }
        pos += 8u + (uint64_t)size + (size & 1u);
    }
    if (!info->data_offset || info->channels < 1 || info->channels > 8 ||
        info->rate < 8000 || info->rate > 192000) return 0;
    if (info->is_float ? (info->bits != 32 && info->bits != 64)
                       : (info->bits != 8 && info->bits != 16 && info->bits != 24 && info->bits != 32)) return 0;
    if (info->block != info->channels * (info->bits / 8)) return 0;
    if (fseek(file, 0, SEEK_END) == 0) {         /* a data size past the file (streamed WAVs say 0xFFFFFFFF) */
        long end = ftell(file);
        if (end > 0 && (uint64_t)end < info->data_offset + info->data_bytes)
            info->data_bytes = (uint64_t)end > info->data_offset ? (uint64_t)end - info->data_offset : 0;
    }
    info->data_bytes -= info->data_bytes % info->block;
    return info->data_bytes >= info->block;
}

static int16_t pc_wav_sample(const PcWavInfo *info, const uint8_t *p)
{
    double f;
    int32_t v;
    if (info->is_float) {
        if (info->bits == 32) { float x; memcpy(&x, p, 4); f = x; }
        else memcpy(&f, p, 8);
        if (!(f == f)) f = 0;
        if (f > 1.0) f = 1.0;
        if (f < -1.0) f = -1.0;
        return (int16_t)lrint(f * 32767.0);
    }
    switch (info->bits) {
    case 8: return (int16_t)(((int)p[0] - 128) * 256);
    case 16: return (int16_t)pc_wav_u16(p);
    case 24: v = (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24); return (int16_t)(v >> 16);
    default: v = (int32_t)pc_wav_u32(p); return (int16_t)(v >> 16);
    }
}

unsigned pc_wav_to_s16(const PcWavInfo *info, const uint8_t *in, size_t frames, int16_t *out)
{
    unsigned out_channels = info->channels >= 2 ? 2u : 1u, bytes = info->bits / 8u;
    size_t f;
    for (f = 0; f < frames; f++) {
        const uint8_t *frame = in + f * info->block;
        out[f * out_channels] = pc_wav_sample(info, frame);
        if (out_channels == 2) out[f * 2 + 1] = pc_wav_sample(info, frame + bytes);
    }
    return out_channels;
}

int32_t pc_music_db(float gain)
{
    double db;
    if (!(gain > 0.0001f)) return -10000;
    if (gain >= 1.0f) return 0;
    db = 2000.0 * log10((double)gain);
    return db < -10000.0 ? -10000 : (int32_t)lrint(db);
}

/* ---- the player -------------------------------------------------------- */
#define PC_MUSIC_CHUNK_FRAMES 4096u   /* 93 ms at 44.1 kHz */
#define PC_MUSIC_LEAD_MS 250u
#define PC_MUSIC_TICK_MS 15u

typedef struct PcMusicPlayer {
    FILE *file;
    PcWavInfo info;
    uint64_t read_bytes;               /* of the data chunk, this pass */
    uint64_t submitted_frames, base_ms;
    int32_t volume;
    int paused, pending_frames;
    unsigned out_channels;
    PcMusicWatch watch;
    void *context;
    uint8_t raw[PC_MUSIC_CHUNK_FRAMES * 8u * 8u];
    int16_t pcm[PC_MUSIC_CHUNK_FRAMES * 2u];
} PcMusicPlayer;

static PcMusicPlayer *pc_music_player;
static volatile int pc_music_run;

#ifdef PC_CUSTOM_MUSIC_TEST
int pc_music_test_submit(const uint8_t *pcm, uint32_t bytes, uint32_t rate, uint32_t channels, int32_t volume);
int pc_music_test_position(uint64_t *played, uint32_t *queued);
void pc_music_test_volume(int32_t volume);
void pc_music_test_reset(void);
static uint64_t pc_music_test_now;
static uint64_t pc_music_now_ms(void) { return pc_music_test_now; }
#define PC_MUSIC_SUBMIT(p, b, r, c, v) pc_music_test_submit(p, b, r, c, v)
#define PC_MUSIC_POSITION(p, q) pc_music_test_position(p, q)
#define PC_MUSIC_VOLUME(v) pc_music_test_volume(v)
#define PC_MUSIC_RESET() pc_music_test_reset()
#define PC_MUSIC_DISCONTINUITY() 0
#else
static uint64_t pc_music_now_ms(void)
{
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000u + (uint64_t)t.tv_nsec / 1000000u;
#endif
}
#define PC_MUSIC_SUBMIT(p, b, r, c, v) recomp_audio_output_submit(PC_MUSIC_SLOT, p, b, r, c, 16u, v)
#define PC_MUSIC_POSITION(p, q) recomp_audio_output_position(PC_MUSIC_SLOT, p, q)
#define PC_MUSIC_VOLUME(v) recomp_audio_output_set_volume(PC_MUSIC_SLOT, v)
#define PC_MUSIC_RESET() recomp_audio_output_reset_voice(PC_MUSIC_SLOT)
#define PC_MUSIC_DISCONTINUITY() recomp_audio_output_take_discontinuity(PC_MUSIC_SLOT)
#endif

static FILE *pc_music_open(const char *dir, const char *name)
{
    char path[4096 + PC_MUSIC_NAME_MAX + 2];
    if (strlen(dir) + strlen(name) + 2 > sizeof path) return NULL;
    snprintf(path, sizeof path, "%s/%s", dir, name);
#ifdef _WIN32
    {
        wchar_t wide[4096 + PC_MUSIC_NAME_MAX + 2];
        if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, (int)(sizeof wide / sizeof wide[0])) <= 0) return NULL;
        return _wfopen(wide, L"rb");
    }
#else
    return fopen(path, "rb");
#endif
}

/* One player step; 0 when the track has ended. */
static int pc_music_step(PcMusicPlayer *p, uint64_t now)
{
    float gain = 0;
    int paused = 0, rounds;
    uint64_t played = 0, ahead_frames;
    uint32_t queued = 0;
    if (!p->watch(p->context, &gain, &paused)) return 0;
    if (PC_MUSIC_DISCONTINUITY()) { p->submitted_frames = 0; p->base_ms = now; }
    {
        int32_t volume = paused ? -10000 : pc_music_db(gain * PC_MUSIC_LEVEL);
        if (volume != p->volume) { p->volume = volume; PC_MUSIC_VOLUME(volume); }
    }
    if (paused) {                      /* hold the file position; what is queued plays out silently */
        if (!p->paused) p->paused = 1;
        return 1;
    }
    if (p->paused) { p->paused = 0; p->submitted_frames = 0; p->base_ms = now; PC_MUSIC_RESET(); }
    for (rounds = 0; rounds < 8; rounds++) {
        uint64_t heard;
        if (PC_MUSIC_POSITION(&played, &queued))
            heard = played / (2u * p->out_channels);
        else                           /* no device clock: the wall clock paces the file */
            heard = (now - p->base_ms) * p->info.rate / 1000u;
        ahead_frames = p->submitted_frames > heard ? p->submitted_frames - heard : 0;
        if (ahead_frames * 1000u >= (uint64_t)PC_MUSIC_LEAD_MS * p->info.rate || queued >= 6u) break;
        if (!p->pending_frames) {
            uint64_t left = p->info.data_bytes - p->read_bytes;
            size_t want, got;
            if (!left) {               /* loop, as the stock level music does */
                p->read_bytes = 0;
                left = p->info.data_bytes;
            }
            want = (size_t)(left < (uint64_t)PC_MUSIC_CHUNK_FRAMES * p->info.block ? left : (uint64_t)PC_MUSIC_CHUNK_FRAMES * p->info.block);
            if (fseek(p->file, (long)(p->info.data_offset + p->read_bytes), SEEK_SET)) return 0;
            got = fread(p->raw, 1, want, p->file);
            got -= got % p->info.block;
            if (!got) return 0;        /* the file shrank or went away */
            p->read_bytes += got;
            p->pending_frames = (int)(got / p->info.block);
            p->out_channels = pc_wav_to_s16(&p->info, p->raw, (size_t)p->pending_frames, p->pcm);
        }
        if (!PC_MUSIC_SUBMIT((const uint8_t *)p->pcm, (uint32_t)p->pending_frames * 2u * p->out_channels,
                             p->info.rate, p->out_channels, p->volume))
            break;                     /* offered again next step, never skipped */
        p->submitted_frames += (uint64_t)p->pending_frames;
        p->pending_frames = 0;
    }
    return 1;
}

static void pc_music_free(PcMusicPlayer *p)
{
    if (!p) return;
    if (p->file) fclose(p->file);
    free(p);
}

#ifdef PC_CUSTOM_MUSIC_TEST
void pc_music_pump(uint64_t now_ms)
{
    pc_music_test_now = now_ms;
    if (pc_music_player && !pc_music_step(pc_music_player, now_ms)) {
        PC_MUSIC_RESET();
        pc_music_free(pc_music_player);
        pc_music_player = NULL;
        pc_music_run = 0;
    }
}
static int pc_music_thread_start(void) { return 1; }
static void pc_music_thread_join(void) {}
#else
static void pc_music_sleep(unsigned ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000u);
#endif
}
static void pc_music_loop(void)
{
    PcMusicPlayer *p = pc_music_player;
    while (pc_music_run && pc_music_step(p, pc_music_now_ms())) pc_music_sleep(PC_MUSIC_TICK_MS);
    PC_MUSIC_RESET();
    pc_music_run = 0;
}
#ifdef _WIN32
static HANDLE pc_music_handle;
static DWORD WINAPI pc_music_main(LPVOID unused) { (void)unused; pc_music_loop(); return 0; }
static int pc_music_thread_start(void)
{
    pc_music_handle = CreateThread(NULL, 0, pc_music_main, NULL, 0, NULL);
    return pc_music_handle != NULL;
}
static void pc_music_thread_join(void)
{
    if (!pc_music_handle) return;
    WaitForSingleObject(pc_music_handle, INFINITE);
    CloseHandle(pc_music_handle);
    pc_music_handle = NULL;
}
#else
static pthread_t pc_music_handle;
static int pc_music_started;
static void *pc_music_main(void *unused) { (void)unused; pc_music_loop(); return NULL; }
static int pc_music_thread_start(void)
{
    pc_music_started = pthread_create(&pc_music_handle, NULL, pc_music_main, NULL) == 0;
    return pc_music_started;
}
static void pc_music_thread_join(void)
{
    if (!pc_music_started) return;
    pthread_join(pc_music_handle, NULL);
    pc_music_started = 0;
}
#endif
#endif

/* Game thread only: start, stop and the playing test are never concurrent. */
void pc_music_stop(void)
{
    pc_music_run = 0;
    pc_music_thread_join();
    if (pc_music_player) {
#ifdef PC_CUSTOM_MUSIC_TEST
        PC_MUSIC_RESET();
#endif
        pc_music_free(pc_music_player);
        pc_music_player = NULL;
    }
}

int pc_music_playing(void) { return pc_music_player && pc_music_run; }

int pc_music_start(const char *name, PcMusicWatch watch, void *context)
{
    char dir[4096];
    PcMusicPlayer *p;
    pc_music_stop();
    if (!watch || !pc_music_name_ok(name) || !pc_music_dir(dir, sizeof dir)) return 0;
    if (!(p = (PcMusicPlayer *)calloc(1, sizeof *p))) return 0;
    if (!(p->file = pc_music_open(dir, name)) || !pc_wav_parse(p->file, &p->info)) {
        pc_music_free(p);
        return 0;
    }
    p->watch = watch;
    p->context = context;
    p->volume = 1;                     /* not a level pc_music_db returns: the first step sets it */
    p->out_channels = p->info.channels >= 2 ? 2u : 1u;
    p->base_ms = pc_music_now_ms();
    pc_music_player = p;
    pc_music_run = 1;
    if (!pc_music_thread_start()) { pc_music_run = 0; pc_music_free(p); pc_music_player = NULL; return 0; }
    return 1;
}

/* ---- the Arcade choice ------------------------------------------------- */
static int pc_music_ini(char *out, size_t bytes)
{
    const char *source = getenv("TSFP_DISPLAY_CONFIG");
    char *a, *b, *slash;
    size_t prefix;
    if (!source || !*source) source = getenv("RECOMP_INPUT_CONFIG");
    if (!source || !*source) source = "config/pc-display.ini";
    if (strlen(source) >= bytes) return 0;
    strcpy(out, source);
    a = strrchr(out, '/'); b = strrchr(out, '\\');
    slash = a && b ? (a > b ? a : b) : a ? a : b;
    prefix = slash ? (size_t)(slash + 1 - out) : 0;
    if (prefix + strlen("pc-music.ini") >= bytes) return 0;
    strcpy(out + prefix, "pc-music.ini");
    return 1;
}

int pc_music_arcade_load(char *name, size_t bytes)
{
    char path[4096], line[PC_MUSIC_NAME_MAX + 16];
    FILE *file;
    int ok = 0;
    if (!name || !bytes) return 0;
    name[0] = 0;
    if (!pc_music_ini(path, sizeof path) || !(file = fopen(path, "rb"))) return 0;
    if (fgets(line, sizeof line, file) && !strncmp(line, "TSFP_MUSIC_V1", 13) &&
        fgets(line, sizeof line, file) && !strncmp(line, "arcade=", 7)) {
        char *v = line + 7;
        v[strcspn(v, "\r\n")] = 0;
        if (pc_music_name_ok(v) && strlen(v) < bytes) { strcpy(name, v); ok = 1; }
    }
    fclose(file);
    return ok;
}

int pc_music_arcade_save(const char *name)
{
    char path[4096], temporary[4096 + 8];
    FILE *file;
    int ok;
    if (!name) name = "";
    if (*name && !pc_music_name_ok(name)) return 0;
    if (!pc_music_ini(path, sizeof path)) return 0;
    snprintf(temporary, sizeof temporary, "%s.tmp", path);
    if (!(file = fopen(temporary, "wb"))) return 0;
    ok = fprintf(file, "TSFP_MUSIC_V1\narcade=%s\n", name) > 0;
    if (fflush(file)) ok = 0;
    if (fclose(file)) ok = 0;
#ifdef _WIN32
    if (ok) ok = MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) DeleteFileA(temporary);
#else
    if (ok) ok = rename(temporary, path) == 0;
    if (!ok) remove(temporary);
#endif
    return ok;
}
#endif /* PC_CUSTOM_MUSIC_IMPLEMENTATION */
#endif /* PC_CUSTOM_MUSIC_H */
