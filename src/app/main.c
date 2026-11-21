/* Alley Cat for Windows: SDL2 front end around the recompiled game.
 *
 * Finds the player's CAT.EXE (command line, alleycat.ini, next to the program, or a file
 * picker), checks it, and runs it on the PC model at the speed of a 4.77 MHz IBM PC.
 * Keys: the game's own (arrows, Alt, Esc, Ctrl-S/R/M, letters). F11 or Alt+Enter: full screen.
 * F12: screenshot (PNG next to the program). Window close: quit. */
#include "pc.h"
#include "cat_gen.h"
#include "game_file.h"
#include "png_write.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#endif

#define PRESCALE 4                        /* crisp integer upscale, then smooth to the window */

static PC pc;
static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex, *big;
static SDL_AudioDeviceID audio;
static uint64_t perf_freq, next_deadline;
static double frame_ticks;
static char base_dir[1024], ini_path[1100];
static uint8_t held[128];

/* ---------------------------------------------------------------- keyboard: SDL -> XT set 1 */
static uint8_t xt_code(SDL_Scancode s) {
    if (s >= SDL_SCANCODE_A && s <= SDL_SCANCODE_Z) {
        static const uint8_t az[26] = {0x1E,0x30,0x2E,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,
                                       0x31,0x18,0x19,0x10,0x13,0x1F,0x14,0x16,0x2F,0x11,0x2D,0x15,0x2C};
        return az[s - SDL_SCANCODE_A];
    }
    if (s >= SDL_SCANCODE_1 && s <= SDL_SCANCODE_9) return (uint8_t)(0x02 + (s - SDL_SCANCODE_1));
    if (s >= SDL_SCANCODE_F1 && s <= SDL_SCANCODE_F10) return (uint8_t)(0x3B + (s - SDL_SCANCODE_F1));
    switch (s) {
    case SDL_SCANCODE_0: return 0x0B;          case SDL_SCANCODE_MINUS: return 0x0C;
    case SDL_SCANCODE_EQUALS: return 0x0D;     case SDL_SCANCODE_BACKSPACE: return 0x0E;
    case SDL_SCANCODE_TAB: return 0x0F;        case SDL_SCANCODE_LEFTBRACKET: return 0x1A;
    case SDL_SCANCODE_RIGHTBRACKET: return 0x1B; case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: return 0x1C;
    case SDL_SCANCODE_LCTRL: case SDL_SCANCODE_RCTRL: return 0x1D;
    case SDL_SCANCODE_SEMICOLON: return 0x27;  case SDL_SCANCODE_APOSTROPHE: return 0x28;
    case SDL_SCANCODE_GRAVE: return 0x29;      case SDL_SCANCODE_LSHIFT: return 0x2A;
    case SDL_SCANCODE_BACKSLASH: return 0x2B;  case SDL_SCANCODE_COMMA: return 0x33;
    case SDL_SCANCODE_PERIOD: return 0x34;     case SDL_SCANCODE_SLASH: return 0x35;
    case SDL_SCANCODE_RSHIFT: return 0x36;     case SDL_SCANCODE_KP_MULTIPLY: return 0x37;
    case SDL_SCANCODE_LALT: case SDL_SCANCODE_RALT: return 0x38;
    case SDL_SCANCODE_SPACE: return 0x39;      case SDL_SCANCODE_ESCAPE: return 0x01;
    case SDL_SCANCODE_HOME: case SDL_SCANCODE_KP_7: return 0x47;
    case SDL_SCANCODE_UP: case SDL_SCANCODE_KP_8: return 0x48;
    case SDL_SCANCODE_PAGEUP: case SDL_SCANCODE_KP_9: return 0x49;
    case SDL_SCANCODE_KP_MINUS: return 0x4A;
    case SDL_SCANCODE_LEFT: case SDL_SCANCODE_KP_4: return 0x4B;
    case SDL_SCANCODE_KP_5: return 0x4C;
    case SDL_SCANCODE_RIGHT: case SDL_SCANCODE_KP_6: return 0x4D;
    case SDL_SCANCODE_KP_PLUS: return 0x4E;
    case SDL_SCANCODE_END: case SDL_SCANCODE_KP_1: return 0x4F;
    case SDL_SCANCODE_DOWN: case SDL_SCANCODE_KP_2: return 0x50;
    case SDL_SCANCODE_PAGEDOWN: case SDL_SCANCODE_KP_3: return 0x51;
    case SDL_SCANCODE_INSERT: case SDL_SCANCODE_KP_0: return 0x52;
    case SDL_SCANCODE_DELETE: case SDL_SCANCODE_KP_PERIOD: return 0x53;
    default: return 0;
    }
}

static void release_all(void) {
    for (int i = 1; i < 128; i++) if (held[i]) { held[i] = 0; pc_key(&pc, (uint8_t)i, false); }
}

/* ---------------------------------------------------------------- video */
static void present(void) {
    static uint8_t ix[320 * 200];
    void *px; int pitch;
    pc_render(&pc, ix);
    if (SDL_LockTexture(tex, NULL, &px, &pitch) == 0) {
        for (int y = 0; y < 200; y++) {
            uint32_t *row = (uint32_t *)((uint8_t *)px + y * pitch);
            for (int x = 0; x < 320; x++) row[x] = 0xFF000000u | pc_rgbi_palette[ix[y * 320 + x]];
        }
        SDL_UnlockTexture(tex);
    }
    SDL_SetRenderTarget(ren, big);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_SetRenderTarget(ren, NULL);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    int w, h;
    SDL_GetRendererOutputSize(ren, &w, &h);
    int dw = w, dh = w * 3 / 4;                         /* 4:3 picture, as on a CRT */
    if (dh > h) { dh = h; dw = h * 4 / 3; }
    SDL_Rect dst = {(w - dw) / 2, (h - dh) / 2, dw, dh};
    SDL_RenderCopy(ren, big, NULL, &dst);
    SDL_RenderPresent(ren);
}

static void screenshot(void) {
    static uint8_t ix[320 * 200]; static uint32_t argb[320 * 200];
    char path[1200];
    pc_render(&pc, ix);
    for (int i = 0; i < 320 * 200; i++) argb[i] = 0xFF000000u | pc_rgbi_palette[ix[i]];
    for (int i = 1; i < 10000; i++) {
        snprintf(path, sizeof path, "%salleycat_%04d.png", base_dir, i);
        FILE *f = fopen(path, "rb");
        if (f) { fclose(f); continue; }
        png_write_argb(path, argb, 320, 200, 3);
        break;
    }
}

/* ---------------------------------------------------------------- per-frame host work */
static void quit_now(int code) {
    if (audio) SDL_CloseAudioDevice(audio);
    SDL_Quit();
    exit(code);
}

static void poll_events(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT: quit_now(0); break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) release_all();
            break;
        case SDL_KEYDOWN: case SDL_KEYUP: {
            bool down = e.type == SDL_KEYDOWN;
            SDL_Scancode s = e.key.keysym.scancode;
            if (down && (s == SDL_SCANCODE_F11 || (s == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT)))) {
                bool fs = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                SDL_SetWindowFullscreen(win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                SDL_ShowCursor(fs ? SDL_ENABLE : SDL_DISABLE);
                break;
            }
            if (down && s == SDL_SCANCODE_F12) { screenshot(); break; }
            uint8_t sc = xt_code(s);
            if (!sc) break;
            if (down) { held[sc] = 1; pc_key(&pc, sc, true); }    /* repeats act like typematic */
            else if (held[sc]) { held[sc] = 0; pc_key(&pc, sc, false); }
            break;
        }
        default: break;
        }
    }
}

static void on_frame(PC *p) {
    poll_events();
    present();
    /* audio: keep 40-120 ms queued; trim or pad if the clocks drift apart */
    if (audio) {
        uint32_t q = SDL_GetQueuedAudioSize(audio) / 2;
        if (q < PC_AUDIO_RATE / 50) {
            static int16_t pad[PC_AUDIO_RATE / 25];
            SDL_QueueAudio(audio, pad, sizeof pad);
        }
        if (q < PC_AUDIO_RATE / 8) SDL_QueueAudio(audio, p->audio, (uint32_t)p->audio_len * 2);
    }
    p->audio_len = 0;
    /* pace to real time: one CGA frame = 1/59.92 s */
    uint64_t now = SDL_GetPerformanceCounter();
    next_deadline += (uint64_t)frame_ticks;
    if (now > next_deadline + perf_freq / 4) next_deadline = now;          /* fell behind (e.g. window drag) */
    while ((now = SDL_GetPerformanceCounter()) < next_deadline) {
        uint64_t left_ms = (next_deadline - now) * 1000 / perf_freq;
        if (left_ms > 2) SDL_Delay((uint32_t)(left_ms - 1));
    }
}

static void on_exit_(PC *p, int code) { (void)p; quit_now(code); }

static void on_fatal(const char *report) {
    char path[1200];
    snprintf(path, sizeof path, "%salleycat_crash.log", base_dir);
    FILE *f = fopen(path, "w");
    if (f) { fputs(report, f); fclose(f); }
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Alley Cat stopped",
                             "The game hit an internal error. Details were saved to alleycat_crash.log.", win);
}

/* ---------------------------------------------------------------- finding CAT.EXE */
static void read_ini(char *game, size_t n) {
    game[0] = 0;
    FILE *f = fopen(ini_path, "r");
    if (!f) return;
    char line[1200];
    while (fgets(line, sizeof line, f)) {
        if (!strncmp(line, "game=", 5)) {
            snprintf(game, n, "%s", line + 5);
            game[strcspn(game, "\r\n")] = 0;
        }
    }
    fclose(f);
}

static void write_ini(const char *game) {
    FILE *f = fopen(ini_path, "w");
    if (!f) return;
    fprintf(f, "; Alley Cat for Windows\n; game = path to your original CAT.EXE (it is read at every start)\ngame=%s\n", game);
    fclose(f);
}

static bool pick_file(char *out, size_t n) {
#ifdef _WIN32
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof ofn);
    out[0] = 0;
    ofn.lStructSize = sizeof ofn;
    ofn.lpstrFilter = "Alley Cat (CAT.EXE)\0CAT.EXE\0All files\0*.*\0";
    ofn.lpstrFile = out; ofn.nMaxFile = (DWORD)n;
    ofn.lpstrTitle = "Where is your Alley Cat CAT.EXE?";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameA(&ofn) != 0;
#else
    (void)out; (void)n; return false;
#endif
}

static void msg(const char *text) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Alley Cat", text, NULL); }

static bool find_game(int argc, char **argv, uint8_t **exe, size_t *len) {
    char cand[1200];
    if (argc > 1 && argv[1][0] != '-') return game_file_load(argv[1], exe, len) == GAME_FILE_OK;
    read_ini(cand, sizeof cand);
    if (cand[0] && game_file_load(cand, exe, len) == GAME_FILE_OK) return true;
    snprintf(cand, sizeof cand, "%sCAT.EXE", base_dir);
    if (game_file_load(cand, exe, len) == GAME_FILE_OK) { write_ini(cand); return true; }
    msg("This program needs the original Alley Cat game file, CAT.EXE (1984).\n"
        "It is not included. Please choose your copy in the next window.\n"
        "Its location is saved in alleycat.ini.");
    for (;;) {
        if (!pick_file(cand, sizeof cand)) return false;
        int rc = game_file_load(cand, exe, len);
        if (rc == GAME_FILE_OK) { write_ini(cand); return true; }
        msg(rc == GAME_FILE_WRONG ? "That file is not the known CAT.EXE (55,067 bytes, August 1984). Please try another."
                                  : "That file could not be read. Please try another.");
    }
}

int main(int argc, char **argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    char *bp = SDL_GetBasePath();
    snprintf(base_dir, sizeof base_dir, "%s", bp ? bp : "");
    SDL_free(bp);
    snprintf(ini_path, sizeof ini_path, "%salleycat.ini", base_dir);

    uint8_t *exe; size_t len;
    if (!find_game(argc, argv, &exe, &len)) { SDL_Quit(); return 1; }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    win = SDL_CreateWindow("Alley Cat", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 960,
                           SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    if (!win || !ren) { msg(SDL_GetError()); return 1; }
    SDL_SetWindowMinimumSize(win, 320, 240);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 320, 200);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    big = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 320 * PRESCALE, 200 * PRESCALE);

    SDL_AudioSpec want = {0}, have;
    want.freq = PC_AUDIO_RATE; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 512;
    audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio) SDL_PauseAudioDevice(audio, 0);

    perf_freq = SDL_GetPerformanceFrequency();
    frame_ticks = (double)perf_freq * PC_FRAME_CYC / PC_CPU_HZ;
    next_deadline = SDL_GetPerformanceCounter();

    pc.mem = calloc(1, 0x100000);
    pc.on_frame = on_frame; pc.on_exit = on_exit_;
    pc.sound_on = true;
    {   /* BIOS clock starts at the current time of day, as on a real PC after DOS set it */
        time_t t = time(NULL); struct tm *lt = localtime(&t);
        double secs = lt ? lt->tm_hour * 3600.0 + lt->tm_min * 60.0 + lt->tm_sec : 0;
        pc_set_tick_base((uint32_t)(secs * 1193182.0 / 65536.0));
    }
    if (!pc_init(&pc, exe, len, cat_entries, cat_entry_count)) { msg("CAT.EXE could not be loaded."); return 1; }
    free(exe);
    v30_fatal_hook = on_fatal;
    pc_run(&pc);
    return 0;
}
