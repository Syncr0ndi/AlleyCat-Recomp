// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
/* Alley Cat, hand-written: switching scenes and the screen palette.
 *
 * Going through a window, the picture is wiped by a rectangle growing from the cat
 * (iris_wipe, with a rising sound), the next scene is drawn, then the wipe runs again in
 * the new scene's fill pattern. */
#include "hand.h"

/* 1D6E pcjr_palette: INT 10h AX=1000h (set one palette register, PCjr only). */
void hand_1d6e(V30 *c) {
    AX = 0x1000;
    uint16_t si = SI;
    INT(0x10);
    SI = si;
}

/* 1D31 set_palette: CGA palette of scene [0004] (PCjr: three palette registers), black
 * border/background. */
void hand_1d31(V30 *c) {
    if (G8(0x0697) != 0xFD) {
        AH = 0x0B;
        BH = 1;
        SI = g16(c, 0x0004);
        BL = G8((uint16_t)(SI + 0x1853));
        INT(0x10);
    } else {
        SI = g16(c, 0x0004);
        BL = 1; BH = G8((uint16_t)(SI + 0x183B)); CALL(0x1D6E);
        BL = 2; BH = G8((uint16_t)(SI + 0x1843)); CALL(0x1D6E);
        BL = 3; BH = G8((uint16_t)(SI + 0x184B)); CALL(0x1D6E);
    }
    AH = 0x0B;
    BX = 0;
    INT(0x10);
}

/* 1C67 iris_wipe: fill a rectangle around the cat with the word [1839], growing by 16
 * pixels each way per step (with a sound step) until it covers the screen. */
void hand_1c67(V30 *c) {
    CALL(0x5896);
    sg16(c, 0x1835, 1);                                  /* width (pixels) */
    G8(0x1837) = 8;                                      /* height */
    CX = (uint16_t)((g16(c, 0x0579) + 0x0C) & 0xFFF0);
    DL = (uint8_t)(G8(0x057B) + 8);
    G8(0x1838) = 0;                                      /* edges reached: 1 left 2 right 4 top 8 bottom */
    for (;;) {
        CALL(0x5897);
        sg16(c, 0x1832, CX);
        G8(0x1834) = DL;
        CALL(0x2CB0);
        DI = AX;
        BL = G8(0x1837);
        do {
            AX = g16(c, 0x1839);
            CX = (uint16_t)(g16(c, 0x1835) >> 3);
            rep_stosw(c);
            CX = (uint16_t)((g16(c, 0x1835) >> 2) & 0xFE);
            DI = (uint16_t)(DI - CX);
            DI ^= 0x2000;
            if (!(DI & 0x2000)) DI = (uint16_t)(DI + 0x50);
            V30_TICK(80);
        } while (--BL);
        if (G8(0x1838) == 0x0F) return;
        sg16(c, 0x1835, (uint16_t)(g16(c, 0x1835) + 0x20));
        G8(0x1837) = (uint8_t)(G8(0x1837) + 0x10);
        CX = g16(c, 0x1832);
        DL = G8(0x1834);
        if (CX < 0x10) { CX = 0; G8(0x1838) |= 1; } else CX -= 0x10;
        AX = (uint16_t)(g16(c, 0x1835) + CX);
        if (AX >= 0x140) {
            AX = (uint16_t)(0x140 - CX);
            sg16(c, 0x1835, AX);
            G8(0x1838) |= 2;
        }
        if (DL < 8) { DL = 0; G8(0x1838) |= 4; } else DL = (uint8_t)(DL - 8);
        unsigned sum = (unsigned)G8(0x1837) + DL;
        AL = (uint8_t)sum;
        if (sum > 0xFF || AL >= 0xC8) {
            AL = (uint8_t)(0xC8 - DL);
            G8(0x1837) = AL;
            G8(0x1838) |= 8;
        }
    }
}

/* 1BF0 scene_change: wipe to black, set the palette, draw scene [0004] (alley, or the
 * win / bonus screens), wipe again in the scene's fill. */
void hand_1bf0(V30 *c) {
    BX = 0;
    AH = 0x0B;
    INT(0x10);
    if (g16(c, 0x0006) == 7 && G8(0x0553)) {
        CALL(0x528B);
        sg16(c, 0x0579, 0x98);
        G8(0x057B) = 0x5F;
    }
    AX = CGA_SEG; ES = AX;
    c->df = 0;
    sg16(c, 0x1839, 0);
    CALL(0x1C67);
    CALL(0x5B21);
    CALL(0x1D31);
    if (g16(c, 0x0004) == 0) {
        if (G8(0x0553)) {
            if (g16(c, 0x0006) == 7) CALL(0x5313);
            else CALL(0x38B0);
        } else CALL(0x1D76);
    }
    AX = (g16(c, 0x0004) == 7 || g16(c, 0x0004) == 2) ? 0x5555 : 0xAAAA;
    sg16(c, 0x1839, AX);
    CALL(0x1C67);
    CALL(0x5B21);
}
