/* Alley Cat, hand-written: CGA drawing primitives and the random number generator.
 *
 * Screen memory is CGA 320x200x4: byte = 4 pixels (bits 7-6 leftmost), even rows at B800:0000,
 * odd rows at B800:2000, 80 bytes per row. Sprites are stored as rows of whole words
 * (8 pixels each). Pixel value 0 is transparent in sprite_draw. */
#include "hand.h"

/* Shared scratch variables of the drawing routines (data segment). */
#define V_WIDTH   0x2AE0   /* byte: words per row (read as a word when stepping rows) */
#define V_ROWS    0x2AE2   /* byte: rows left */
#define V_SPRWORD 0x2AE3   /* word: sprite word being drawn */
#define V_SEED    0x2AE5   /* word: random number generator state */
#define V_SRCROW  0x2AE9   /* word: source row start (rect_copy) */
#define V_STRIDE  0x2AEB   /* byte: source stride in bytes (rect_copy) */

/* Next row of the same bitmap in interlaced CGA memory, after having advanced `bytes`. */
static uint8_t row_cf;                                /* CF left by the original's row step */
static uint16_t next_row(uint16_t p, uint16_t bytes) {
    p = (uint16_t)(p - bytes);
    p ^= 0x2000;
    row_cf = 0;
    if (!(p & 0x2000)) { row_cf = p > 0xFFFF - 0x50; p = (uint16_t)(p + 0x50); }
    return p;
}

static void flags_after_dec(V30 *c, uint8_t r) {   /* DEC byte: Z, S, O, A, P (C unchanged) */
    c->zf = r == 0; c->sf = r >> 7; c->of = r == 0x7F; c->af = (r & 0xF) == 0xF; c->pf = v30_pf(r);
}

/* 2CB0 screen_address: pixel (x = CX, y = DL) -> AX = offset in CGA memory,
 * CL = left shift of the pixel inside its byte * 2 ... i.e. (x & 3) * 2, DX = x / 4. */
void hand_2cb0(V30 *c) {
    uint16_t x = CX;
    uint8_t y = DL;
    AX = (uint16_t)(y * 40 + ((y & 1) ? 0x1FD8 : 0) + (x >> 2));    /* = (y/2)*80 + (y&1)*0x2000 + x/4 */
    DX = (uint16_t)(x >> 2);
    CL = (uint8_t)((x & 3) << 1);
    c->cf = 0; c->of = 0; c->sf = 0; c->zf = CL == 0; c->pf = v30_pf(CL);
    V30_TICK(150);
}

/* 2CCC sprite_draw: draw CH rows of CL words from DS:SI at ES:DI; sprite pixels that are 0
 * leave the screen pixel. The screen words underneath are saved to DS:BP onwards. */
void hand_2ccc(V30 *c) {
    c->df = 0;
    G8(V_WIDTH) = CL; G8(V_ROWS) = CH;
    CH = 0;
    uint16_t ax = AX, bx = BX;
    uint8_t rows;
    do {
        CX = G8(V_WIDTH);
        do {
            bx = m16(c, ES, DI);
            w16(c, DS, BP, bx);
            ax = m16(c, DS, SI); SI += 2;
            sg16(c, V_SPRWORD, ax);
            /* mask: 11 where the sprite pixel is 0, else the sprite pixel itself */
            uint16_t m = ax;
            for (int px = 0; px < 8; px++) {
                uint16_t bits = (uint16_t)(3u << (px * 2));
                if (!(m & bits)) m |= bits;
            }
            ax = (uint16_t)((m & bx) | g16(c, V_SPRWORD));
            w16(c, ES, DI, ax); DI += 2;
            BP += 2;
            V30_TICK(280);
        } while (--CX);
        DI = next_row(DI, (uint16_t)(g16(c, V_WIDTH) * 2));
        rows = --G8(V_ROWS);
        V30_TICK(110);
    } while (rows);
    AX = ax; BX = bx; DX = 0x30C0;
    c->cf = row_cf; flags_after_dec(c, 0);
}

/* 2D35 sprite_and: AND CH rows of CL words from DS:SI into the screen at ES:DI (punches a
 * shape mask), saving the screen words to DS:BP onwards. */
void hand_2d35(V30 *c) {
    c->df = 0;
    G8(V_WIDTH) = CL; G8(V_ROWS) = CH;
    CH = 0;
    uint16_t ax = AX, bx = BX;
    uint8_t rows;
    do {
        CX = G8(V_WIDTH);
        do {
            bx = m16(c, ES, DI);
            w16(c, DS, BP, bx);
            ax = (uint16_t)(m16(c, DS, SI) & bx); SI += 2;
            w16(c, ES, DI, ax); DI += 2;
            BP += 2;
            V30_TICK(95);
        } while (--CX);
        DI = next_row(DI, (uint16_t)(g16(c, V_WIDTH) * 2));
        rows = --G8(V_ROWS);
        V30_TICK(110);
    } while (rows);
    AX = ax; BX = bx;
    c->cf = row_cf; flags_after_dec(c, 0);
}

/* 2D70 rect_copy: copy CH rows of CL words from DS:SI (source rows AL words apart) to
 * consecutive memory at ES:DI. */
void hand_2d70(V30 *c) {
    c->df = 0;
    sg16(c, V_SRCROW, SI);
    G8(V_WIDTH) = CL; G8(V_ROWS) = CH;
    AL = (uint8_t)(AL << 1);
    G8(V_STRIDE) = AL;
    CH = 0;
    uint8_t rows;
    do {
        for (CX = G8(V_WIDTH); CX; CX--) {
            w16(c, ES, DI, m16(c, DS, SI)); SI += 2; DI += 2;
            V30_TICK(25);
        }
        CL = G8(V_STRIDE);
        c->cf = (uint32_t)g16(c, V_SRCROW) + CX > 0xFFFF;
        sg16(c, V_SRCROW, (uint16_t)(g16(c, V_SRCROW) + CX));
        SI = g16(c, V_SRCROW);
        rows = --G8(V_ROWS);
        V30_TICK(120);
    } while (rows);
    flags_after_dec(c, 0);
}

/* 2D9D blit: copy CH rows of CL words from consecutive memory DS:SI to the screen at ES:DI. */
void hand_2d9d(V30 *c) {
    c->df = 0;
    G8(V_WIDTH) = CL; G8(V_ROWS) = CH;
    CH = 0;
    uint8_t rows;
    do {
        for (CX = G8(V_WIDTH); CX; CX--) {
            w16(c, ES, DI, m16(c, DS, SI)); SI += 2; DI += 2;
            V30_TICK(25);
        }
        DI = next_row(DI, (uint16_t)(g16(c, V_WIDTH) * 2));
        rows = --G8(V_ROWS);
        V30_TICK(110);
    } while (rows);
    c->cf = row_cf; flags_after_dec(c, 0);
}

/* 2DCA grab: copy CH rows of CL words from the screen at DS:SI to consecutive memory ES:DI.
 * Here DS is the screen, so the scratch variables are addressed through ES. */
void hand_2dca(V30 *c) {
    c->df = 0;
    w8(c, ES, V_WIDTH, CL); w8(c, ES, V_ROWS, CH);
    CH = 0;
    uint8_t rows;
    do {
        for (CX = m8(c, ES, V_WIDTH); CX; CX--) {
            w16(c, ES, DI, m16(c, DS, SI)); SI += 2; DI += 2;
            V30_TICK(25);
        }
        SI = next_row(SI, (uint16_t)(m16(c, ES, V_WIDTH) * 2));
        rows = (uint8_t)(m8(c, ES, V_ROWS) - 1);
        w8(c, ES, V_ROWS, rows);
        V30_TICK(115);
    } while (rows);
    c->cf = row_cf; flags_after_dec(c, 0);
}

/* 2DFD random: 16-bit shift register; DX = new value. Feedback bit = bit 1 of (low ^ high). */
void hand_2dfd(V30 *c) {
    uint16_t s = g16(c, V_SEED);
    unsigned fb = (((s & 0xFF) ^ (s >> 8)) >> 1) & 1;
    uint8_t low = (uint8_t)((((s & 0xFF) ^ (s >> 8)) & 0xFF) >> 2);
    c->zf = low == 0; c->sf = 0; c->pf = v30_pf(low);
    c->cf = s & 1;
    s = (uint16_t)((s >> 1) | (fb << 15));
    c->of = ((s >> 15) ^ (s >> 14)) & 1;
    sg16(c, V_SEED, s);
    DX = s;
    V30_TICK(75);
}
