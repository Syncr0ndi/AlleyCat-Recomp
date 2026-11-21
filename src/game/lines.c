// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
/* Alley Cat, hand-written: the moving clotheslines in the alley.
 *
 * Each of the lines (index 0-2 in [052F]) scrolls one byte (4 pixels) per update, left or
 * right, by moving its two screen rows in CGA memory. A fresh 4-pixel column is fed in from
 * a 16-byte strip at 04D7 (one byte per scan line, every 4th byte). New patterns (bare line,
 * shirts/socks/boots) are generated into that strip by line_pattern, and bits of [0540]
 * mark which hanging items are solid enough to stand on (shifted into a 40-bit map per line
 * at 1016 by item_bits). */
#include "hand.h"

#define V_LINE      0x052F   /* word: line being scrolled (0-2) */
#define V_FILL      0x0525   /* byte: strip column being fed in */
#define V_ITEMBITS  0x0540   /* byte: item bits of the new pattern */
#define V_ONLINE    0x04D6   /* byte: 1 = the cat rides the line being scrolled */
#define V_DELAY     0x0531   /* byte: updates until the next scroll step */
#define V_LASTTICK  0x0544   /* word: BIOS tick of the last step */
#define V_CAT_X     0x0579   /* word: cat x */
#define V_CAT_Y     0x057B   /* byte: cat y */

/* 0700: clear two counters ([057D], [0684]) */
void hand_0700(V30 *c) {
    sg16(c, 0x057D, 0);
    sg16(c, 0x0684, 0);
    V30_TICK(45);
}

/* 0633 item_bits: shift CF into the 40-bit item map of the current line, rightwards
 * (lines 0, 1) or leftwards (map index 9, line 2), one byte at a time through carry. */
void hand_0633(V30 *c) {
    uint8_t flags = (uint8_t)v30_get_flags(c);           /* LAHF */
    BX = g16(c, V_LINE);
    BL = G8((uint16_t)(BX + 0x541));
    unsigned cf = c->cf;
    CX = 5;
    if (BL != 9) {
        do {
            uint16_t a = (uint16_t)(BX + 0x1016);
            uint8_t v = G8(a);
            unsigned out = v & 1;
            G8(a) = (uint8_t)((v >> 1) | (cf << 7));
            cf = out;
            BX++;
        } while (--CX);
    } else {
        do {
            uint16_t a = (uint16_t)(BX + 0x1016);
            uint8_t v = G8(a);
            unsigned out = v >> 7;
            G8(a) = (uint8_t)((v << 1) | cf);
            cf = out;
            BX--;
        } while (--CX);
    }
    AH = (uint8_t)((flags & 0xFE) | cf);
    c->cf = cf;
    V30_TICK(40 + 5 * 45);
}

/* 0658 cat_on_line: ZF=1 if the cat's height [057C] is not within line BX's band, or it is
 * and [055C] says it rides the line (then [04D6]=1). ZF=0, CF=1: on the band, not riding. */
void hand_0658(V30 *c) {
    G8(V_ONLINE) = 0;
    AL = G8(0x057C);
    V30_TICK(60);
    if (AL >= G8((uint16_t)(BX + 0x053D)) && AL < G8((uint16_t)(BX + 0x053A))) {
        CMP8(G8(0x055C), 1);
        if (c->cf) return;
        G8(V_ONLINE) = 1;
    }
    CMP8(AL, AL);
}

/* 06DE item: copy one random 8-word item (or nothing, AL=0) to ES:DI, every other word. */
void hand_06de(V30 *c) {
    CALL(0x2DFD);
    DX = and16(c, DX, 6);
    if (DL == 6) { AL = sub8(c, AL, AL, 0); V30_TICK(30); return; }
    BX = DX;
    SI = g16(c, (uint16_t)(BX + 0x04D0));
    int16_t step = c->df ? -2 : 2;
    CX = 8;
    do {
        AX = m16(c, DS, SI); SI += step;
        w16(c, ES, DI, AX); DI += step;
        DI += 2;
    } while (--CX);
    AL = 1;
    V30_TICK(60 + 8 * 55);
}

/* 067D line_pattern: build the next 16-byte strip at ES:DI: bare line, then maybe (random,
 * odds BL/BH) one of two fixed patterns or two random items; [0540] gets the item bits. */
void hand_067d(V30 *c) {
    G8(V_ITEMBITS) = 0;
    c->df = 0;
    AX = 0xAAAA;
    for (CX = 0x20; CX; CX--) { w16(c, ES, DI, AX); DI += 2; }
    DI -= 0x40;
    AX = 0x4444;
    w16(c, ES, (uint16_t)(DI + 4), AX);
    w16(c, ES, (uint16_t)(DI + 6), AX);
    V30_TICK(9 + 0x20 * 14 + 80);
    CALL(0x2DFD);
    if (DL < BL || DH <= BH) { CMP8(DH, BH); return; }
    CALL(0x2DFD);
    if (DL < 0x18 || DL < 0x60) {
        CX = DL < 0x18 ? 0x20 : 0x10;
        SI = DL < 0x18 ? 0x0490 : 0x0460;
        for (; CX; CX--) { w16(c, ES, DI, m16(c, DS, SI)); SI += 2; DI += 2; }
        G8(V_ITEMBITS) = 3;
        V30_TICK(9 + 0x20 * 25);
        return;
    }
    uint16_t di = DI;
    CALL(0x06DE);
    G8(V_ITEMBITS) = (uint8_t)(AL << 1);
    DI = (uint16_t)(di + 2);
    CALL(0x06DE);
    G8(V_ITEMBITS) |= AL;
}

/* 04A0 lines_update: once per BIOS tick (outside vertical retrace, when nothing else is
 * busy), every [0531] calls: scroll the current line one byte, feeding in the strip, and
 * start a new pattern / switch to another line when the strip is used up. A cat riding the
 * line moves with it (and falls off at the ends). */
void hand_04a0(V30 *c) {
    V30_TICK(25);
    if (--G8(V_DELAY)) return;
    G8(V_DELAY)++;
    CALL(0x13D8);
    if (!c->zf) return;
    if (G8(0x055A) || G8(0x1673)) return;
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, V_LASTTICK)) return;
    sg16(c, V_LASTTICK, DX);
    BX = g16(c, 0x0008);
    AL = G8((uint16_t)(BX + 0x0532));
    if (G8(V_CAT_Y) <= 0x60) AL = (uint8_t)(AL >> 2);
    G8(V_DELAY) = AL;
    BX = g16(c, V_LINE);
    CALL(0x0658);
    int new_pattern = 0;
    if (!c->zf) {
        AL = (uint8_t)(G8(V_FILL) + G8((uint16_t)(BX + 0x0529)));
        if (AL < 4) return;
        do { CALL(0x2DFD); DL &= 3; } while (DL == G8(V_LINE) || DL == 3);
        BL = DL;
        sg16(c, V_LINE, BX);
        new_pattern = 1;
    } else {
        AL = G8((uint16_t)(BX + 0x0529));
        G8(V_FILL) = (uint8_t)(G8(V_FILL) + AL);
        if (G8(V_FILL) >= 4) {
            CALL(0x2DFD);
            if (DL <= 0x40) {
                for (;;) {
                    do { CALL(0x2DFD); DL &= 3; } while (DL == 3);
                    BL = DL;
                    CALL(0x0658);
                    if (c->zf) break;
                }
                sg16(c, V_LINE, BX);
            }
            new_pattern = 1;
        }
    }
    if (new_pattern) {
        AL = G8((uint16_t)(BX + 0x0526));
        G8(V_FILL) = AL;
        ES = GAME_DS;
        DI = 0x04D7;
        AH = G8((uint16_t)(BX + 0x052C));
        BX = g16(c, 0x0008);
        BL = G8((uint16_t)(BX + 0x2ABA));
        BH = AH;
        CALL(0x067D);
        /* feed the item bits into the line's map: 2 bits, or 1 bit and a gap (line 1) */
        if (g16(c, V_LINE) != 1) {
            c->cf = G8(V_ITEMBITS) & 1; G8(V_ITEMBITS) >>= 1;
            CALL(0x0633);
            c->cf = G8(V_ITEMBITS) & 1; G8(V_ITEMBITS) >>= 1;
        } else {
            AL = G8(V_ITEMBITS);
            AL >>= 1;
            c->cf = AL & 1; AL >>= 1;
            CALL(0x0633);
            c->cf = G8(V_ITEMBITS) & 1; G8(V_ITEMBITS) >>= 1;
        }
        CALL(0x0633);
        BX = g16(c, V_LINE);
    }
    if (G8(V_ONLINE)) {                                 /* carry the cat along */
        AX = g16(c, V_CAT_X);
        int fall = 0;
        if (BL != 1) {
            sg16(c, 0x055F, (uint16_t)(g16(c, 0x055F) + 1));
            AX += 4;
            if (AX >= 0x123) fall = 1;
        } else {
            sg16(c, 0x055F, (uint16_t)(g16(c, 0x055F) - 1));
            if (AX < 4 || (uint16_t)(AX - 4) < 8) fall = 1;
            AX -= 4;
        }
        if (fall) {
            G8(0x055B) = 0x11; G8(0x0571) = 1; G8(0x0576) = 1; G8(0x0578) = 0x18; G8(0x0572) = 1; G8(0x055C) = 0;
        } else sg16(c, V_CAT_X, AX);
    }
    /* scroll both scan-line banks of the line one byte */
    uint16_t ds = DS;
    BX = (uint16_t)(BX << 1);
    AX = g16(c, (uint16_t)(BX + 0x051D));
    sg16(c, 0x0523, AX);
    SI = g16(c, (uint16_t)(BX + 0x0517));
    AX = CGA_SEG;
    DS = ES = AX;
    DI = SI;
    int step;
    if (BX == 2) { c->df = 0; DI--; step = 1; } else { c->df = 1; DI++; step = -1; }
    for (int bank = 0; bank < 2; bank++) {
        uint16_t si = SI, di = DI;
        for (CX = bank ? 0x280 : 0x27F; CX; CX--) {
            w8(c, ES, DI, m8(c, DS, SI));
            SI = (uint16_t)(SI + step); DI = (uint16_t)(DI + step);
        }
        V30_TICK(9 + 0x280 * 17);
        if (!bank) { SI = (uint16_t)(si + 0x2000); DI = (uint16_t)(di + 0x2000); }
    }
    DS = ds;
    DI = g16(c, 0x0523);
    BL = G8(V_FILL);
    BH = 0;
    BX += 0x04D7;
    for (CX = 0x10; CX; CX--) {                        /* feed in the new column */
        AL = m8(c, DS, BX);
        w8(c, ES, DI, AL);
        BX += 4;
        DI ^= 0x2000;
        if (!(DI & 0x2000)) DI += 0x50;
        V30_TICK(60);
    }
}
