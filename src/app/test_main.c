/* Headless runner: plays scripted keys as fast as possible and writes screenshots / audio.
 *
 *   cat_test CAT.EXE [--frames N] [--key FRAME:SC[:LEN]]... [--shot FRAME:FILE.png]...
 *            [--wav FILE.wav] [--scale X] [--status N]
 *
 * SC is an XT scancode in hex (1C Enter, 39 Space, 48/50/4B/4D arrows, 38 Alt, 15 Y, 31 N...).
 * The key is held for LEN frames (default 3). Each status line prints a hash of video RAM. */
#include "pc.h"
#include "cat_gen.h"
#include "game_file.h"
#include "png_write.h"
#include "hand.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { long frame; uint8_t sc; int len; } KeyEv;
typedef struct { long frame; const char *file; } Shot;
static KeyEv keys[512]; static int nkeys;
static Shot shots[256]; static int nshots;
static long max_frames = 600, status_every = 0;
static FILE *wav; static long wav_samples;
static PC pc;

static void wav_header(FILE *f, long n) {
    uint32_t data = (uint32_t)n * 2;
    uint8_t h[44] = {'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,
                     0x44,0xAC,0,0,0x88,0x58,1,0,2,0,16,0,'d','a','t','a'};
    uint32_t riff = 36 + data;
    memcpy(h + 4, &riff, 4); memcpy(h + 40, &data, 4);
    fseek(f, 0, SEEK_SET); fwrite(h, 1, 44, f);
}

static void save_png(const char *file) {
    static uint8_t ix[320 * 200]; static uint32_t argb[320 * 200];
    pc_render(&pc, ix);
    for (int i = 0; i < 320 * 200; i++) argb[i] = 0xFF000000u | pc_rgbi_palette[ix[i]];
    if (!png_write_argb(file, argb, 320, 200, 2)) fprintf(stderr, "cannot write %s\n", file);
}

static uint32_t vram_hash(void) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < 0x4000; i++) h = (h ^ pc.mem[0xB8000 + i]) * 16777619u;
    return h;
}

static void finish(int code) {
    if (wav) { wav_header(wav, wav_samples); fclose(wav); }
    if (cat_hand_mode == HAND_CHECK) {
        printf("hand-written routines checked against the recompiled code:\n");
        unsigned bad = cat_hand_report(stdout);
        printf("total mismatches: %u\n", bad);
        if (bad && !code) code = 4;
    }
    printf("end: frame %llu, vram %08x, CS:IP %04X:%04X\n", (unsigned long long)pc.frames, vram_hash(),
           pc.cpu.sreg[V30_CS], pc.cpu.ip);
    fflush(stdout);
    exit(code);
}

static void on_frame(PC *p) {
    long f = (long)p->frames;
    for (int i = 0; i < nkeys; i++) {
        if (keys[i].frame == f) pc_key(p, keys[i].sc, true);
        if (keys[i].frame + keys[i].len == f) pc_key(p, keys[i].sc, false);
    }
    for (int i = 0; i < nshots; i++) if (shots[i].frame == f) save_png(shots[i].file);
    if (wav) { fwrite(p->audio, 2, (size_t)p->audio_len, wav); wav_samples += p->audio_len; }
    p->audio_len = 0;
    if (status_every && f % status_every == 0) {
        printf("frame %5ld  vram %08x  CS:IP %04X:%04X  timer polls/frame %.1f\n", f, vram_hash(),
               p->cpu.sreg[V30_CS], p->cpu.ip, (double)p->stat_polls / (double)status_every);
        p->stat_polls = 0;
    }
    if (f >= max_frames) finish(0);
}

static void on_exit_(PC *p, int code) { (void)p; printf("game exited (%d)\n", code); finish(0); }

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: cat_test CAT.EXE [options]\n"); return 2; }
    double scale = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc) max_frames = atol(argv[++i]);
        else if (!strcmp(argv[i], "--status") && i + 1 < argc) status_every = atol(argv[++i]);
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atof(argv[++i]);
        else if (!strcmp(argv[i], "--hand-mode") && i + 1 < argc) {
            const char *m = argv[++i];
            cat_hand_mode = !strcmp(m, "gen") ? HAND_GEN : !strcmp(m, "check") ? HAND_CHECK : HAND_ON;
        }
        else if (!strcmp(argv[i], "--key") && i + 1 < argc && nkeys < 512) {
            unsigned sc; int len = 3; long fr;
            if (sscanf(argv[++i], "%ld:%x:%d", &fr, &sc, &len) >= 2) keys[nkeys++] = (KeyEv){fr, (uint8_t)sc, len};
        } else if (!strcmp(argv[i], "--shot") && i + 1 < argc && nshots < 256) {
            char *s = argv[++i], *c = strchr(s, ':');
            if (c) { *c = 0; shots[nshots++] = (Shot){atol(s), c + 1}; }
        } else if (!strcmp(argv[i], "--wav") && i + 1 < argc) {
            wav = fopen(argv[++i], "wb");
            if (wav) { uint8_t z[44] = {0}; fwrite(z, 1, 44, wav); }
        } else { fprintf(stderr, "unknown option %s\n", argv[i]); return 2; }
    }
    uint8_t *exe; size_t n;
    int rc = game_file_load(argv[1], &exe, &n);
    if (rc) { fprintf(stderr, "%s: %s\n", argv[1], rc == GAME_FILE_WRONG ? "not the known CAT.EXE" : "cannot read"); return 1; }
    pc.mem = calloc(1, 0x100000);
    pc.cycle_scale = scale;
    pc.on_frame = on_frame; pc.on_exit = on_exit_;
    pc.sound_on = true;
    pc_set_tick_base(0);
    if (!pc_init(&pc, exe, n, cat_entries, cat_entry_count)) { fprintf(stderr, "bad EXE\n"); return 1; }
    pc_run(&pc);
    return 0;
}
