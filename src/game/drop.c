// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
/* Alley Cat, hand-written: the object thrown from a window in the alley.
 *
 * Position x [1671] (word), y [1673] (0 = none), horizontal direction [1674] (1 right,
 * FF left, 0 straight down), speed [17EA], fall time [17E9], floor [1676], sprite [17DD],
 * size [17DF]. Drawn with sprite_draw, background kept at 17EE. */
#include "hand.h"

/* 1830 drop_reset: no object in the air. */
void hand_1830(V30 *c) {
    G8(0x1665) = 0; G8(0x1673) = 0; G8(0x1677) = 0; G8(0x1678) = 0;
    sg16(c, 0x166C, 9);
    V30_TICK(80);
}

/* 1922 drop_erase: put back the screen under the object. */
void hand_1922(V30 *c) {
    AX = CGA_SEG; ES = AX;
    DI = g16(c, 0x17E5);
    SI = 0x17EE;
    CX = g16(c, 0x17E3);
    V30_TICK(70);
    CALL(0x2D9D);
}

/* 184B drop_update: once per BIOS tick: drift sideways (slowing down; aimed objects stop
 * above the cat), fall faster and faster, stop at the floor, redraw. */
void hand_184b(V30 *c) {
    V30_TICK(40);
    if (!G8(0x1673)) return;
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, 0x17EC)) return;
    sg16(c, 0x17EC, DX);
    if (G8(0x1677)) {
        AX = g16(c, 0x1671) & 0xFFF8;
        BX = g16(c, 0x0579) & 0xFFF8;
        if (AX == BX) G8(0x1674) = 0;
    }
    G8(0x17E9)++;
    if (g16(c, 0x17EA) <= 1) sg16(c, 0x17EA, (uint16_t)(g16(c, 0x17EA) - 1));
    AX = g16(c, 0x1671);
    DX = g16(c, 0x17EA);
    CL = 3;
    DL = (uint8_t)(DL >> 3);
    uint8_t dir = G8(0x1674);
    if (dir == 1) {
        AX = (uint16_t)(AX + DX);
        if (AX >= 0x12F) AX = 0x12E;
    } else if (dir > 1) {
        AX = AX < DX ? 0 : (uint16_t)(AX - DX);
    }
    sg16(c, 0x1671, AX);
    BX = g16(c, 0x17DF);
    AL = (uint8_t)(G8(0x17E9) >> 1);
    AL = (uint8_t)(AL + G8(0x1673));
    DL = AL;
    int landed;
    if (AL < G8(0x1676)) landed = 0;
    else {
        AL = (uint8_t)(AL - G8(0x1676));                /* how far into the floor: clip */
        uint8_t bh = BH;
        BH = (uint8_t)(BH - AL);
        landed = BH == 0 || bh < AL;
    }
    if (landed) {
        G8(0x1673) = 0;
        G8(0x1678) = 0;
        CALL(0x1922);
        return;
    }
    G8(0x1673) = DL;
    CX = g16(c, 0x1671);
    sg16(c, 0x17E1, BX);
    CALL(0x2CB0);
    sg16(c, 0x17E7, AX);
    if (G8(0x17E9) != 2) CALL(0x1922);
    CALL(0x1B7A);
    if (c->cf) return;
    DI = g16(c, 0x17E7);
    sg16(c, 0x17E5, DI);
    CX = g16(c, 0x17E1);
    sg16(c, 0x17E3, CX);
    AX = CGA_SEG; ES = AX;
    SI = g16(c, 0x17DD);
    BP = 0x17EE;
    CALL(0x2CCC);
}
