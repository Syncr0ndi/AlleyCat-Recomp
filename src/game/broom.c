// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
/* Alley Cat, hand-written: the broom in the rooms, and the cat's paw prints.
 *
 * Walking on the floor the cat leaves prints (up to 4 per 8-pixel column, [328E+col]).
 * The broom (x [327D], y [327F], directions [3280] / [3281], frame [327A]) wanders,
 * hunts the cat more as time goes on, sweeps prints away, and knocks the cat away when it
 * touches it. Drawn OR-ed over the background (kept at 31E8). */
#include "hand.h"

#define V_BX   0x327D
#define V_BY   0x327F
#define V_DX   0x3280
#define V_DY   0x3281

/* 347F print_draw: print picture AL (0-4) in column BX. */
void hand_347f(V30 *c) {
    AH = 0x0A;
    AX = (uint16_t)(AL * AH);
    AX = (uint16_t)(AX + 0x32B8);
    SI = AX;
    DI = (uint16_t)((BX << 1) + 0x1E00);
    AX = CGA_SEG; ES = AX;
    CX = 0x0501;
    V30_TICK(150);
    CALL(0x2D9D);
}

/* 3445 cat_prints: a walking cat leaves a print in its column (each column once in turn). */
void hand_3445(V30 *c) {
    V30_TICK(40);
    if (G8(0x057B) < 0xB4 || !G8(0x056E)) return;
    AX = (uint16_t)(g16(c, 0x0579) + 0x0C);
    CL = 3;
    AX >>= 3;
    if (AX > 0x27 || AX == g16(c, 0x32B6)) return;
    sg16(c, 0x32B6, AX);
    BX = AX;
    AL = G8((uint16_t)(BX + 0x328E));
    if (AL >= 4) return;
    AL++;
    G8((uint16_t)(BX + 0x328E)) = AL;
    CALL(0x347F);
}

/* 3405 broom_reset */
void hand_3405(V30 *c) {
    c->df = 0;
    AX = 0;
    ES = DS;
    DI = 0x328E;
    CX = 0x14;
    rep_stosw(c);
    sg16(c, 0x32B6, 0xFF);
    sg16(c, 0x327A, 0);
    sg16(c, V_BX, 0);
    G8(V_BY) = 0xA0;
    G8(0x3286) = 1;
    G8(V_DX) = 0; G8(V_DY) = 0;
    CALL(0x2DFD);
    G8(0x32EB) = DL;
    G8(0x32EA) = 0x6C;
}

/* 33A0 broom_erase */
void hand_33a0(V30 *c) {
    CMP8(G8(0x3286), 0);
    V30_TICK(30);
    if (!c->zf) return;
    AX = CGA_SEG; ES = AX;
    SI = 0x31E8;
    DI = g16(c, 0x3282);
    CX = 0x1E02;
    CALL(0x2D9D);
}

/* 3339 broom_draw: next frame at [3284], OR-ed onto the screen. */
void hand_3339(V30 *c) {
    for (;;) {
        BX = g16(c, 0x327A);
        AX = g16(c, (uint16_t)(BX + 0x3260));
        if (AX) break;
        sg16(c, 0x327A, AX);
    }
    SI = AX;
    DI = g16(c, 0x3284);
    sg16(c, 0x3282, DI);
    BP = 0x31E8;
    AX = CGA_SEG; ES = AX;
    CX = 0x1E02;
    G8(0x3286) = 0;
    c->df = 0;
    G8(0x3289) = CH;
    CH = 0;
    sg16(c, 0x3287, CX);
    do {
        CX = g16(c, 0x3287);
        do {
            BX = m16(c, ES, DI);
            w16(c, DS, BP, BX);
            AX = m16(c, DS, SI); SI += 2;
            AX |= BX;
            w16(c, ES, DI, AX); DI += 2;
            BP += 2;
            V30_TICK(90);
        } while (--CX);
        DI = (uint16_t)(DI - g16(c, 0x3287));
        DI = (uint16_t)(DI - g16(c, 0x3287));
        DI ^= 0x2000;
        if (!(DI & 0x2000)) DI = (uint16_t)(DI + 0x50);
    } while (--G8(0x3289));
}

/* 33BA broom_hits_cat: CF=1 if the broom touches the cat (which is knocked away). */
void hand_33ba(V30 *c) {
    V30_TICK(40);
    if (G8(0x1CB8)) { c->cf = 0; return; }
    if (g16(c, 0x0004) == 6 && G8(0x44BD)) { CALL(0x47B0); return; }
    AX = g16(c, V_BX);
    DL = G8(V_BY);
    SI = 0x10;
    BX = g16(c, 0x0579);
    DH = G8(0x057B);
    DI = 0x18;
    CX = 0x0E1E;
    CALL(0x2E29);
    if (!c->cf) { c->cf = 0; return; }
    if (!(g16(c, 0x0004) == 4 && G8(0x39E1))) CALL(0x0872);
    c->cf = 1;
}

static uint8_t random_dir(V30 *c) {
    CALL(0x2DFD);
    DL &= 1;
    if (!DL) DL = 0xFF;
    return DL;
}

/* 3150 broom_update: every [32F2+room] ticks: choose a direction (towards prints to sweep,
 * at the cat, or at random), move 8 / 2 pixels, redraw. */
void hand_3150(V30 *c) {
    AH = 0;
    INT(0x1A);
    BX = g16(c, 0x0004);
    BL = (uint8_t)(BL << 1);
    CX = g16(c, (uint16_t)(BX + 0x32F2));
    AX = (uint16_t)(DX - g16(c, 0x328C));
    if (AX < CX) return;
    sg16(c, 0x328C, DX);
    CALL(0x33BA);
    if (c->cf) return;
    CALL(0x21E0);
    if (c->cf) return;
    G8(0x32EA)++;
    CALL(0x2DFD);
    AL = G8(0x32EA) & DL;
    G8(0x32EB) ^= AL;
    /* where is the cat */
    { uint16_t bx = g16(c, V_BX), cx = g16(c, 0x0579);
      AX = (uint16_t)(bx - cx); DL = 0xFF;
      if (bx < cx) { AX = (uint16_t)~AX; DL = 1; } }
    G8(0x32ED) = DL;
    BL = (uint8_t)(G8(V_BY) + 0x14);
    { uint8_t b = BL, y = G8(0x057B);
      BL = (uint8_t)(BL - y); DL = 0xFF;
      if (b < y) { BL = (uint8_t)~BL; DL = 1; } }
    G8(0x32EE) = DL;
    AX >>= 2;
    BL = (uint8_t)(BL >> 1);
    AL = (uint8_t)(AL + BL);
    G8(0x32EC) = AL;                                     /* distance */
    BX = g16(c, 0x328A);
    if (BX >= 0x27) { BX = 0x26; sg16(c, 0x328A, BX); }
    if (G8((uint16_t)(BX + 0x328E)) == 0) {
        sg16(c, 0x328A, (uint16_t)(g16(c, 0x328A) - 1));   /* look at the next column next time */
        AH = 0;
        INT(0x1A);
        DX = (uint16_t)(DX - g16(c, 0x0410));
        CL = 3;
        DX >>= 3;
        { uint8_t a = G8(0x32EC); AL = (uint8_t)(a - DL); if (a < DL) AL = 0; }
        if (AL >= G8(0x32EB)) {                          /* far away: wander */
            G8(V_DY) = 1;
            CALL(0x2DFD);
            if (DL != 0) {
                if (DL > 7) goto move;
                DL &= 1;
                if (!DL) DL = 0xFF;
            }
            G8(V_DX) = DL;
        } else {
            AL = G8(0x32EB) & 0x2F;
            if (!AL) {
                G8(V_DX) = random_dir(c);
                G8(V_DY) = random_dir(c);
            } else {
                AL &= 7;
                if (!AL) { AL = G8(0x32ED); G8(V_DX) = AL; AL = G8(0x32EE); G8(V_DY) = AL; }
            }
        }
    } else {                                             /* prints in column BX: go sweep */
        G8(V_DY) = 1;
        AX = BX;
        CL = 3;
        AX = (uint16_t)(AX << 3);
        if (g16(c, V_BX) != AX) {
            G8(V_DX) = g16(c, V_BX) < AX ? 1 : 0xFF;
        } else {
            G8(V_DX) = 0;
            if (G8(V_BY) == 0xA5) {
                G8(V_DY) = 0;
                if (g16(c, 0x327A) == 6 || g16(c, 0x327A) == 0x12) {
                    uint16_t bx = BX;
                    SI = 0x31E8;
                    DI = g16(c, 0x3282);
                    CX = 0x1E02;
                    AX = CGA_SEG; ES = AX;
                    CALL(0x2D9D);
                    BX = bx;
                    G8((uint16_t)(BX + 0x328E))--;
                    AL = G8((uint16_t)(BX + 0x328E));
                    CALL(0x347F);
                    G8(0x3286) = 1;
                }
            }
        }
    }
move:
    CX = g16(c, V_BX);
    DL = G8(V_BY);
    sg16(c, 0x32EF, CX);
    G8(0x32F1) = DL;
    if (G8(V_DX) == 1) { CX = (uint16_t)(CX + 8); if (CX >= 0x131) CX = 0x130; }
    else if (G8(V_DX) > 1) { CX = CX < 8 ? 0 : (uint16_t)(CX - 8); }
    CX &= 0xFFF8;
    sg16(c, V_BX, CX);
    if (G8(V_DY) == 1) { DL = (uint8_t)(DL + 2); if (DL >= 0xA6) DL = 0xA5; }
    else if (G8(V_DY) > 1) { DL = DL < 2 ? 0 : (uint8_t)(DL - 2); }
    G8(V_BY) = DL;
    CALL(0x2CB0);
    sg16(c, 0x3284, AX);
    CALL(0x33BA);
    if (!c->cf) {
        CALL(0x21E0);
        if (!c->cf) {
            CALL(0x33A0);
            sg16(c, 0x327A, (uint16_t)(g16(c, 0x327A) + 2));
            CALL(0x3339);
            return;
        }
    }
    G8(V_DX) = 0; G8(V_DY) = 0;                          /* blocked: stay */
    CX = g16(c, 0x32EF); sg16(c, V_BX, CX);
    DL = G8(0x32F1); G8(V_BY) = DL;
}
