/* IBM PC machine model for Alley Cat. See pc.h. */
#include "pc.h"
#include "font8x8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLICE 400                       /* generated-code cycles between sync points */
#define TICK_CYC (65536.0 * PC_PIT_DIV) /* one BIOS tick (18.2 Hz) */
#define VSYNC_LINE 224                  /* mode 4: CRTC R7 = 0x70 character rows of 2 lines */
#define VSYNC_LINES 16
#define ACTIVE_CYC 213                  /* 640 of 912 hdots per line carry picture */

static inline PC *PCOF(V30 *c) { return (PC *)c->host; }
static inline uint8_t *bda(PC *pc, unsigned off) { return pc->mem + 0x400 + off; }
static inline void bda_w16(PC *pc, unsigned off, uint16_t v) { bda(pc, off)[0] = (uint8_t)v; bda(pc, off)[1] = (uint8_t)(v >> 8); }
static inline uint16_t bda_r16(PC *pc, unsigned off) { return (uint16_t)(bda(pc, off)[0] | bda(pc, off)[1] << 8); }

/* IBM RGBI as shown by a 5153 monitor (colour 6 is brown, not dark yellow) */
const uint32_t pc_rgbi_palette[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

/* ---------------------------------------------------------------- speaker audio */
static int pit2_level(const PC *pc, uint64_t t) {
    if (!(pc->port61 & 1)) return 1;                    /* gate low: mode 3 output stays high */
    uint64_t period = (uint64_t)(pc->pit_reload[2] ? pc->pit_reload[2] : 65536) * PC_PIT_DIV;
    uint64_t ph = (t - pc->pit2_start) % period;
    return ph < period / 2;
}

static void audio_render(PC *pc, uint64_t until) {
    const double step = PC_CPU_HZ / PC_AUDIO_RATE;
    while (pc->audio_pos + step <= (double)until) {
        uint64_t a = (uint64_t)pc->audio_pos, b = (uint64_t)(pc->audio_pos + step);
        int on = 0, n = 0;
        if (pc->port61 & 2) {
            for (uint64_t t = a; t < b; t += PC_PIT_DIV, n++) on += pit2_level(pc, t);
        } else n = 1;
        double x = n ? (double)on / n : 0.0;
        /* DC blocker + a soft one-pole low-pass (a small speaker cone) */
        double y = x - pc->hp_prev_in + 0.995 * pc->hp_prev_out;
        pc->hp_prev_in = x; pc->hp_prev_out = y;
        pc->lp += 0.45 * (y - pc->lp);
        if (pc->audio_len < PC_AUDIO_MAX) {
            double v = pc->sound_on ? pc->lp * 9000.0 : 0.0;
            if (v > 32767) v = 32767; if (v < -32768) v = -32768;
            pc->audio[pc->audio_len++] = (int16_t)v;
        }
        pc->audio_pos += step;
    }
}

/* ---------------------------------------------------------------- time keeping */
static uint32_t tick_base;

static void update_ticks(PC *pc, uint64_t t) {
    uint64_t ticks = tick_base + (uint64_t)(t / TICK_CYC);
    uint32_t day = (uint32_t)(ticks % 0x1800B0);
    uint32_t old = (uint32_t)(bda_r16(pc, 0x6C) | (uint32_t)bda_r16(pc, 0x6E) << 16);
    if (day < old) *bda(pc, 0x70) = 1;                  /* passed midnight */
    bda_w16(pc, 0x6C, (uint16_t)day); bda_w16(pc, 0x6E, (uint16_t)(day >> 16));
}

static void pc_sync(V30 *c) {
    PC *pc = PCOF(c);
    uint64_t t = pc_time(pc);
    pc->now = t;
    pc->slice = SLICE;
    c->cycles = SLICE;
    c->total_cycles = t;
    update_ticks(pc, t);
    while (t >= pc->next_frame) {
        audio_render(pc, pc->next_frame);
        pc->next_frame += PC_FRAME_CYC;
        pc->frames++;
        if (pc->on_frame) pc->on_frame(pc);
    }
    if (!pc->kbd_in_service && pc->kq_head != pc->kq_tail && t >= pc->kbd_next) c->irq_pending = 1;
}

static int pc_irq_ack(V30 *c) {
    PC *pc = PCOF(c);
    c->irq_pending = 0;
    if (pc->kbd_in_service || pc->kq_head == pc->kq_tail) return -1;
    pc->kbd_data = pc->kq[pc->kq_head];
    pc->kq_head = (pc->kq_head + 1) & 63;
    pc->kbd_in_service = true;
    return 9;
}

void pc_key(PC *pc, uint8_t sc, bool down) {
    int nt = (pc->kq_tail + 1) & 63;
    if (nt == pc->kq_head) return;                      /* buffer full: drop */
    pc->kq[pc->kq_tail] = down ? sc : (uint8_t)(sc | 0x80);
    pc->kq_tail = nt;
}

/* ---------------------------------------------------------------- I/O ports */
static uint16_t pc_in(V30 *c, uint16_t port, int word) {
    PC *pc = PCOF(c);
    (void)word;
    uint64_t t = pc_time(pc);
    switch (port) {
    case 0x40: {                                        /* channel 0, BIOS mode 3, reload 65536 */
        if (!pc->pit_latched) { pc->pit_latch = (uint16_t)(0x10000 - (((t / PC_PIT_DIV) * 2) & 0xFFFF)); pc->pit_latch_hi = 0; }
        uint8_t v = pc->pit_latch_hi ? (uint8_t)(pc->pit_latch >> 8) : (uint8_t)pc->pit_latch;
        if (pc->pit_latch_hi) pc->pit_latched = 0;
        pc->pit_latch_hi ^= 1;
        return v;
    }
    case 0x60: return pc->kbd_data;
    case 0x61: return (uint8_t)(pc->port61 | (pit2_level(pc, t) ? 0x20 : 0));
    case 0x201: {                                       /* game port */
        if (!pc->joy_present) return 0xFF;
        uint8_t v = (uint8_t)(0xF0 & ~((pc->joy_buttons & 3) << 4));
        /* each axis one-shot stays high ~24 us + up to ~1.1 ms depending on the stick */
        for (int i = 0; i < 2; i++)
            if (t - pc->joy_fired < (uint64_t)(115 + pc->joy_axis[i] * 20)) v |= (uint8_t)(1 << i);
        return v;
    }
    case 0x3D5: return pc->crtc_index < 18 ? pc->crtc[pc->crtc_index] : 0xFF;
    case 0x3DA: {
        uint64_t ph = t % PC_FRAME_CYC;
        unsigned line = (unsigned)(ph / PC_LINE_CYC), hpos = (unsigned)(ph % PC_LINE_CYC);
        uint8_t v = 0;
        if (line >= 200 || hpos >= ACTIVE_CYC) v |= 1;
        if (line >= VSYNC_LINE && line < VSYNC_LINE + VSYNC_LINES) v |= 8;
        return v;
    }
    default: return 0xFF;
    }
}

static void pc_out(V30 *c, uint16_t port, uint16_t val, int word) {
    PC *pc = PCOF(c);
    uint8_t v = (uint8_t)val;
    (void)word;
    uint64_t t = pc_time(pc);
    switch (port) {
    case 0x20: if (v == 0x20) { pc->kbd_in_service = false; pc->kbd_next = t + 1500; } break;
    case 0x43: {
        unsigned ch = v >> 6;
        if (ch == 3) break;
        if (((v >> 4) & 3) == 0) {                      /* counter latch */
            if (ch == 0) { pc->pit_latch = (uint16_t)(0x10000 - (((t / PC_PIT_DIV) * 2) & 0xFFFF)); pc->pit_latched = 1; pc->pit_latch_hi = 0; }
            break;
        }
        pc->pit_rw[ch] = (v >> 4) & 3; pc->pit_lohi_next[ch] = 0;
        break;
    }
    case 0x42: {
        audio_render(pc, t);
        uint16_t r = pc->pit_reload[2];
        switch (pc->pit_rw[2]) {
        case 1: r = (uint16_t)((r & 0xFF00) | v); break;
        case 2: r = (uint16_t)((r & 0x00FF) | v << 8); break;
        default:
            if (!pc->pit_lohi_next[2]) { r = (uint16_t)((r & 0xFF00) | v); pc->pit_lohi_next[2] = 1; }
            else { r = (uint16_t)((r & 0x00FF) | v << 8); pc->pit_lohi_next[2] = 0; }
        }
        if (pc->pit_reload[2] != r || !pc->pit_lohi_next[2]) {
            /* a new count takes effect at the end of the current half cycle; restarting
             * the phase here is close enough for a speaker */
            if (pc->pit_reload[2] != r) pc->pit2_start = t;
            pc->pit_reload[2] = r;
        }
        break;
    }
    case 0x61:
        audio_render(pc, t);
        if ((v & 1) && !(pc->port61 & 1)) pc->pit2_start = t;   /* gate rising edge restarts */
        pc->port61 = v & 0x0F;
        break;
    case 0x201: pc->joy_fired = t; break;
    case 0x3D4: pc->crtc_index = v & 0x1F; break;
    case 0x3D5: if (pc->crtc_index < 18) pc->crtc[pc->crtc_index] = v; break;
    case 0x3D8: pc->cga_mode = v; break;
    case 0x3D9: pc->cga_color = v; break;
    default: break;
    }
}

/* ---------------------------------------------------------------- BIOS */
#define AX c->r[V30_AX]
#define BX c->r[V30_BX]
#define CX c->r[V30_CX]
#define DX c->r[V30_DX]
#define AL c->b[0]
#define AH c->b[1]
#define BL c->b[6]
#define BH c->b[7]
#define DL c->b[4]
#define DH c->b[5]

static uint8_t *cga_byte(PC *pc, unsigned x, unsigned y) {   /* mode 4 pixel x (0-319) */
    return pc->mem + 0xB8000 + (y & 1) * 0x2000 + (y >> 1) * 80 + (x >> 2);
}

static void put_pixel(PC *pc, unsigned x, unsigned y, uint8_t col) {
    if (x >= 320 || y >= 200) return;
    uint8_t *p = cga_byte(pc, x, y);
    unsigned sh = (3 - (x & 3)) * 2;
    if (col & 0x80) *p ^= (uint8_t)((col & 3) << sh);
    else *p = (uint8_t)((*p & ~(3u << sh)) | ((col & 3u) << sh));
}

static void scroll_up_8(PC *pc) {
    for (unsigned y = 0; y < 200; y++)
        for (unsigned x = 0; x < 80; x++) {
            uint8_t v = y + 8 < 200 ? *cga_byte(pc, x * 4, y + 8) : 0;
            *cga_byte(pc, x * 4, y) = v;
        }
}

static void teletype(PC *pc, uint8_t ch, uint8_t col) {
    unsigned cx = bda(pc, 0x50)[0], cy = bda(pc, 0x50)[1];
    switch (ch) {
    case 7: return;
    case 8: if (cx) cx--; break;
    case 10: cy++; break;
    case 13: cx = 0; break;
    default: {
        const uint8_t *g = font8x8_glyph(ch);
        for (unsigned r = 0; r < 8; r++)
            for (unsigned b = 0; b < 8; b++) {
                unsigned x = cx * 8 + b, y = cy * 8 + r;
                if (g[r] & (0x80 >> b)) put_pixel(pc, x, y, col);
                else if (!(col & 0x80)) put_pixel(pc, x, y, 0);
            }
        if (++cx >= 40) { cx = 0; cy++; }
    }
    }
    if (cy >= 25) { scroll_up_8(pc); cy = 24; }
    bda(pc, 0x50)[0] = (uint8_t)cx; bda(pc, 0x50)[1] = (uint8_t)cy;
}

static void int10(PC *pc, V30 *c) {
    switch (AH) {
    case 0x00: {
        uint8_t m = AL & 0x7F;
        static const uint8_t modereg[8] = {0x2C, 0x28, 0x2D, 0x29, 0x2A, 0x2E, 0x1E, 0x29};
        pc->cga_mode = modereg[m & 7];
        pc->cga_color = m == 6 ? 0x3F : 0x30;
        *bda(pc, 0x49) = m; *bda(pc, 0x65) = pc->cga_mode; *bda(pc, 0x66) = pc->cga_color;
        bda_w16(pc, 0x4A, m <= 1 || m == 4 || m == 5 ? 40 : 80);
        if (!(AL & 0x80)) memset(pc->mem + 0xB8000, 0, 0x4000);
        memset(bda(pc, 0x50), 0, 16);
        break;
    }
    case 0x02: bda(pc, 0x50 + 2 * (BH & 7))[0] = DL; bda(pc, 0x50 + 2 * (BH & 7))[1] = DH; break;
    case 0x03: DL = bda(pc, 0x50 + 2 * (BH & 7))[0]; DH = bda(pc, 0x50 + 2 * (BH & 7))[1]; CX = 0x0607; break;
    case 0x0B:
        if (BH == 0) pc->cga_color = (uint8_t)((pc->cga_color & 0xE0) | (BL & 0x1F));
        else pc->cga_color = (uint8_t)((pc->cga_color & 0xDF) | ((BL & 1) ? 0x20 : 0));
        *bda(pc, 0x66) = pc->cga_color;
        break;
    case 0x0C: put_pixel(pc, CX, DX, AL); break;
    case 0x0D: AL = (uint8_t)(CX < 320 && DX < 200 ? (*cga_byte(pc, CX, DX) >> ((3 - (CX & 3)) * 2)) & 3 : 0); break;
    case 0x0E: teletype(pc, AL, BL); break;
    case 0x0F: AL = *bda(pc, 0x49); AH = (uint8_t)bda_r16(pc, 0x4A); BH = 0; break;
    default: break;                                     /* AH=10h is PCjr-only */
    }
}

static void pc_bios(V30 *c, unsigned vec) {
    PC *pc = PCOF(c);
    switch (vec) {
    case 0x09: (void)pc_in(c, 0x60, 0); pc->kbd_in_service = false; break;   /* before the game hooks it */
    case 0x10: int10(pc, c); break;
    case 0x11: AX = bda_r16(pc, 0x10); break;
    case 0x12: AX = bda_r16(pc, 0x13); break;
    case 0x16: AX = 0; break;
    case 0x1A:
        pc->stat_polls++;
        if (AH == 0) {
            update_ticks(pc, pc_time(pc));
            DX = bda_r16(pc, 0x6C); CX = bda_r16(pc, 0x6E); AL = *bda(pc, 0x70); *bda(pc, 0x70) = 0;
        }
        break;
    case 0x20: pc->on_exit(pc, 0); break;
    case 0x21: if (AH == 0x4C || AH == 0x00) pc->on_exit(pc, AH == 0x4C ? AL : 0); break;
    case 0x19: case 0xE05B: pc->on_exit(pc, 0); break;  /* Ctrl-Alt-Del in the game reboots */
    default: break;
    }
}

/* ---------------------------------------------------------------- video output */
void pc_render(const PC *pc, uint8_t *pix) {
    const uint8_t *vram = pc->mem + 0xB8000;
    uint8_t bg = pc->cga_color & 0x0F;
    if (!(pc->cga_mode & 0x08)) { memset(pix, 0, 320 * 200); return; }   /* video disabled */
    if (pc->cga_mode & 0x10) {                          /* 640x200 mono: show every other pixel pair as one */
        uint8_t fg = pc->cga_color & 0x0F;
        for (unsigned y = 0; y < 200; y++)
            for (unsigned x = 0; x < 320; x++) {
                uint8_t b = vram[(y & 1) * 0x2000 + (y >> 1) * 80 + (x >> 2)];
                unsigned bits = (b >> ((3 - (x & 3)) * 2)) & 3;
                pix[y * 320 + x] = bits ? fg : 0;
            }
        return;
    }
    uint8_t pal[4];
    unsigned inten = (pc->cga_color & 0x10) ? 8 : 0;
    if (pc->cga_mode & 0x04) {                          /* mode 5: cyan / red / white */
        pal[1] = (uint8_t)(3 + inten); pal[2] = (uint8_t)(4 + inten); pal[3] = (uint8_t)(7 + inten);
    } else if (pc->cga_color & 0x20) {
        pal[1] = (uint8_t)(3 + inten); pal[2] = (uint8_t)(5 + inten); pal[3] = (uint8_t)(7 + inten);
    } else {
        pal[1] = (uint8_t)(2 + inten); pal[2] = (uint8_t)(4 + inten); pal[3] = (uint8_t)(6 + inten);
    }
    pal[0] = bg;
    for (unsigned y = 0; y < 200; y++) {
        const uint8_t *row = vram + (y & 1) * 0x2000 + (y >> 1) * 80;
        uint8_t *o = pix + y * 320;
        for (unsigned x = 0; x < 80; x++) {
            uint8_t b = row[x];
            o[0] = pal[b >> 6]; o[1] = pal[(b >> 4) & 3]; o[2] = pal[(b >> 2) & 3]; o[3] = pal[b & 3];
            o += 4;
        }
    }
}

/* ---------------------------------------------------------------- setup */
bool pc_init(PC *pc, const uint8_t *d, size_t n, const v30_entry *entries, unsigned count) {
    V30 *c = &pc->cpu;
    uint8_t *mem = pc->mem;
    if (n < 28 || d[0] != 'M' || d[1] != 'Z') return false;
#define H(i) ((unsigned)(d[2 * (i)] | d[2 * (i) + 1] << 8))
    unsigned last = H(1), pages = H(2), nrel = H(3), hdr = H(4), ss = H(7), sp = H(8), ip = H(10), cs = H(11), roff = H(12);
#undef H
    size_t size = (size_t)pages * 512 - (last ? 512 - last : 0) - hdr * 16;
    if (hdr * 16 + size > n) return false;
    memset(mem, 0, 0x100000);
    uint32_t base = PC_LOAD_SEG * 16;
    memcpy(mem + base, d + hdr * 16, size);
    for (unsigned i = 0; i < nrel; i++) {
        unsigned o = d[roff + 4 * i] | d[roff + 4 * i + 1] << 8, s = d[roff + 4 * i + 2] | d[roff + 4 * i + 3] << 8;
        uint32_t p = base + s * 16 + o;
        unsigned v = (mem[p] | mem[p + 1] << 8) + PC_LOAD_SEG;
        mem[p] = (uint8_t)v; mem[p + 1] = (uint8_t)(v >> 8);
    }
    /* interrupt vectors -> F000:vec, run by pc_bios */
    for (unsigned v = 0; v < 256; v++) {
        mem[v * 4] = (uint8_t)v; mem[v * 4 + 1] = 0; mem[v * 4 + 2] = 0x00; mem[v * 4 + 3] = 0xF0;
    }
    bda_w16(pc, 0x10, pc->joy_present ? 0x1021 : 0x0021);   /* floppy, 80x25 colour, game port? */
    pc->joy_axis[0] = pc->joy_axis[1] = 128;
    bda_w16(pc, 0x13, 640);
    mem[0xFFFFE] = 0xFF;                                /* machine ID: IBM PC */
    mem[PC_PSP_SEG * 16] = 0xCD; mem[PC_PSP_SEG * 16 + 1] = 0x20;   /* PSP:0 INT 20h */
    mem[PC_PSP_SEG * 16 + 2] = 0x00; mem[PC_PSP_SEG * 16 + 3] = 0xA0;   /* top of memory A000 */

    pc->cga_mode = 0x29; pc->cga_color = 0x30;
    pc->pit_reload[2] = 0x0533; pc->pit_rw[2] = 3;      /* BIOS beep setting */
    pc->next_frame = (uint64_t)VSYNC_LINE * PC_LINE_CYC;
    if (pc->cycle_scale <= 0) pc->cycle_scale = 1.0;

    memset(c, 0, sizeof *c);
    c->mem = mem; c->host = pc;
    c->io_in = pc_in; c->io_out = pc_out; c->sync = pc_sync; c->irq_ack = pc_irq_ack; c->bios = pc_bios;
    v30_set_flags(c, 0x0200);
    c->sreg[V30_CS] = (uint16_t)(cs + PC_LOAD_SEG); c->ip = (uint16_t)ip;
    c->sreg[V30_SS] = (uint16_t)(ss + PC_LOAD_SEG); c->r[V30_SP] = (uint16_t)sp;
    c->sreg[V30_DS] = c->sreg[V30_ES] = PC_PSP_SEG;
    pc->slice = SLICE; c->cycles = SLICE;
    v30_set_entries(entries, count);
    v30_init();
    return true;
}

void pc_set_tick_base(uint32_t ticks) { tick_base = ticks; }

void pc_run(PC *pc) { v30_run(&pc->cpu); }
