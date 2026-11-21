// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
/* Alley Cat, hand-written: the picture shown between scenes (cat face / lives), faded in
 * by masking it with a growing bit pattern while a sound plays. */
#include "hand.h"

/* 1E17 lives_draw: picture [1C2E] AND mask [1C1B] -> buffer 000E -> screen at 0ED0. */
void hand_1e17(V30 *c) {
    c->df = 0;
    ES = DS;
    SI = g16(c, 0x1C2E);
    DI = 0x0E;
    CX = 0x60;
    do {
        AX = m16(c, DS, SI); SI += 2;
        AX = and16(c, AX, g16(c, 0x1C1B));
        w16(c, ES, DI, AX); DI += 2;
        V30_TICK(45);
    } while (--CX);
    AX = CGA_SEG; ES = AX;
    SI = 0x0E;
    DI = 0x0ED0;
    CX = 0x0C08;
    CALL(0x2D9D);
}

/* 1D76 lives_screen: the in-between picture: after losing a life one of four faces (and,
 * with lives left and a level done, the bonus tune instead); fade it in over 28 ticks. */
void hand_1d76(V30 *c) {
    if (g16(c, 0x0006) == 7) { CALL(0x6040); return; }
    CALL(0x57D5);
    AX = 0x185B;
    if (G8(0x0552)) {
        BX = g16(c, 0x1C30);
        sg16(c, 0x1C30, (uint16_t)(g16(c, 0x1C30) + 2));
        BX &= 6;
        AX = g16(c, (uint16_t)(BX + 0x1C26));
        if (G8(0x1F80)) G8(0x1F80)--;
        if (G8(0x0552) == 0xDD && g16(c, 0x0008) && G8(0x1F80) >= 1) {
            CALL(0x5BE0);
            CALL(0x5B21);
            return;
        }
    }
    sg16(c, 0x1C2E, AX);
    sg16(c, 0x1C1B, 0x8080);
    G8(0x1C1D) = 0x1C;
    do {
        CALL(0x1E17);
        AH = 0;
        INT(0x1A);
        sg16(c, 0x1830, DX);
        do {
            CALL(0x57E4);
            AH = 0;
            INT(0x1A);
        } while (DX == g16(c, 0x1830));
        if (G8(0x1C1D) <= 0x14) {
            BH = 0;
            BL = G8(0x1C1D) & 6;
            AX = g16(c, (uint16_t)(BX + 0x1C1E));
        } else {
            AX = g16(c, 0x1C1B);
            c->cf = AL & 1;
            AL = (uint8_t)((AL >> 1) | 0x80);           /* STC; RCR AL,1 */
            AH = AL;
        }
        sg16(c, 0x1C1B, AX);
    } while (--G8(0x1C1D));
    CALL(0x5B21);
}
