/* Alley Cat, hand-written: the windows in the alley that open, and what is thrown out.
 *
 * One window at a time: [1665] is its animation step (1Dh opening ... 0Fh open, waiting
 * [166C] ticks ... closing ... 0), position [1666] (x) / [1668] (y), window index [1669].
 * While open, an object may be thrown once ([1670]); it is aimed at the cat ([1677]) when the
 * cat has been sitting on the fence for a while. Touching the open window = entering it. */
#include "hand.h"

#define V_STEP 0x1665

/* 1AEA window_pos: window number DL -> CX = x, DL = y. */
void hand_1aea(V30 *c) {
    BH = 0;
    BL = DL & 3;
    BL = (uint8_t)(BL << 1);
    CX = g16(c, (uint16_t)(BX + 0x1658));
    BL = DL;
    BL = (uint8_t)(BL >> 2);
    BL &= 3;
    DL = G8((uint16_t)(BX + 0x1660));
    V30_TICK(90);
}

/* 1B05 window_touch: CF=1 if the cat overlaps the window; jumping up into an open window
 * (mid-animation steps 5-18h) enters it ([0551]=1). */
void hand_1b05(V30 *c) {
    AX = g16(c, 0x1666);
    DL = G8(0x1668);
    BX = g16(c, 0x0579);
    DH = G8(0x057B);
    SI = 0x20; DI = 0x18; CX = 0x0E0F;
    CALL(0x2E29);
    if (!c->cf) return;
    if (G8(0x0571) == 1 && !G8(0x055A) && G8(0x057B) < 0x60 && G8(V_STEP) >= 5 && G8(V_STEP) < 0x19)
        G8(0x0551) = 1;
    c->cf = 1;
}

/* 1B4C window_blocked: CF=1 if window [1669] (one of the first 8) is next to the thing at
 * [1F32]/[1F34] (then it must not open). */
void hand_1b4c(V30 *c) {
    AL = G8(0x1669);
    V30_TICK(60);
    if (AL >= 8) { c->cf = 0; return; }
    BX = 2;
    if (AL & 4) BL = (uint8_t)(BL << 1);
    AX = (uint16_t)(g16(c, (uint16_t)(BX + 0x1F30)) + 0x10);
    if (AX < g16(c, 0x1666)) { c->cf = 0; return; }
    AX = AX < 0x30 ? 0 : (uint16_t)(AX - 0x30);
    c->cf = !(AX > g16(c, 0x1666));
}

/* 1B7A hit_by_object: in the alley, CF=1 if the thrown object hits the cat: the cat drops,
 * the first hit splashes and bounces the object back. */
void hand_1b7a(V30 *c) {
    V30_TICK(40);
    if (g16(c, 0x0004) != 0) { c->cf = 0; return; }
    DL = G8(0x1673);
    if (DL == 0) { c->cf = 0; return; }
    CX = g16(c, 0x17DF);
    uint8_t t = CH; CH = CL; CL = t;
    SI = 0x10;
    AX = g16(c, 0x1671);
    BX = g16(c, 0x0579);
    DH = G8(0x057B);
    DI = 0x18;
    CH = 0x0E;
    CALL(0x2E29);
    if (!c->cf) return;
    CALL(0x11E3);
    CALL(0x1922);
    CALL(0x10DD);
    if (!G8(0x1675)) {
        G8(0x1675) = 1;
        CALL(0x1166);
        DL = G8(0x1674) == 0xFF ? 1 : 0xFF;
        G8(0x1674) = DL;
        sg16(c, 0x17EA, 0x60);
        G8(0x17E9) = 1;
        G8(0x055C) = 0;
    }
    c->cf = 1;
}

/* draw one 4-word row of the window animation (1A76-1AE9) */
static void animate(V30 *c) {
    G8(V_STEP)--;
    CX = g16(c, 0x1666);
    DL = G8(0x1668);
    if (G8(V_STEP) > 0x0E) { DL = (uint8_t)(DL + G8(V_STEP)); DL = (uint8_t)(DL - 0x0E); }
    else { DL = (uint8_t)(DL + 0x0E); DL = (uint8_t)(DL - G8(V_STEP)); }
    G8(0x166B) = DL;
    CALL(0x2CB0);
    DI = AX;
    AX = CGA_SEG; ES = AX;
    c->df = 0;
    CX = 4;
    if (G8(V_STEP) > 0x0E) {                             /* closing: blank (or the special scene) */
        if (!G8(0x0418)) { AX = 0; rep_stosw(c); return; }
        AL = (uint8_t)(G8(0x166B) - G8(0x1668));
        AH = 0;
        CL = 3;
        AX = (uint16_t)(AX << 3);
        AX = (uint16_t)(AX + 0x15E0);
        SI = AX;
        CX = 4;
        rep_movsw(c);
        return;
    }
    AL = (uint8_t)(G8(0x166B) - G8(0x1668));          /* opening: rows of the window picture */
    AH = 0x0A;
    AX = (uint16_t)(AL * AH);
    AX = (uint16_t)(AX + 0x2681);
    SI = AX;
    rep_movsw(c);
}

/* 1936 window_update: every 13th call (outside the retrace): maybe open a random window
 * (or one near the cat), run its animation, throw something while it is open. */
void hand_1936(V30 *c) {
    V30_TICK(30);
    if (--G8(0x166A)) return;
    G8(0x166A) = 0x0D;
    CALL(0x13D8);
    if (!c->zf) return;
    if (G8(V_STEP)) CALL(0x1B05);
    if (G8(0x1673)) return;
    if (!G8(V_STEP)) {
        if (G8(0x057B) > 0x60) return;
        G8(0x1677) = 0;
        if (G8(0x0550) == 1 && !G8(0x0418)) {           /* sitting on the fence: aim at the cat */
            AH = 0;
            INT(0x1A);
            DX = (uint16_t)(DX - g16(c, 0x0556));
            if (DX >= 0x48) G8(0x1677)++;
        }
        for (;;) {
            CALL(0x2DFD);
            if (G8(0x1677)) DL &= 3;
            else { DL &= 0x0F; if (DL >= 0x0C) return; }
            G8(0x1669) = DL;
            CALL(0x1AEA);
            sg16(c, 0x1666, CX);
            G8(0x1668) = DL;
            CALL(0x1B05);
            if (!c->cf) break;
        }
        G8(V_STEP) = 0x1D;
        BX = g16(c, 0x0008);
        BL = (uint8_t)(BL << 1);
        AX = g16(c, (uint16_t)(BX + 0x181E));
        sg16(c, 0x166C, AX);
        G8(0x1670) = 1;
    }
    /* 19CD */
    CALL(0x1B05);
    if (c->cf) return;
    G8(0x1664) = 0;
    CALL(0x1B4C);
    if (c->cf) { G8(0x1664)++; return; }
    if (G8(V_STEP) == 0x10) { AH = 0; INT(0x1A); sg16(c, 0x166E, DX); }
    if (G8(V_STEP) != 0x0F) { animate(c); return; }
    AH = 0;
    INT(0x1A);
    DX = (uint16_t)(DX - g16(c, 0x166E));
    if (DX >= g16(c, 0x166C)) { animate(c); return; }  /* open long enough: start closing */
    if (!G8(0x1670) || G8(0x1673) || G8(0x0418)) return;
    G8(0x1670)--;                                        /* throw something */
    G8(0x1678) = 1;
    AL = G8(0x1668);
    G8(0x1673) = AL;
    CALL(0x2DFD);
    DX &= 0x0F;
    DX = (uint16_t)(DX + g16(c, 0x1666));
    sg16(c, 0x1671, DX);
    AL = DX < g16(c, 0x0579) ? 1 : 0xFF;
    G8(0x1674) = AL;
    CALL(0x2DFD);
    BL = DL;
    BX &= 6;
    AX = g16(c, (uint16_t)(BX + 0x17C9)); sg16(c, 0x17DD, AX);
    AX = g16(c, (uint16_t)(BX + 0x17D1)); sg16(c, 0x17DF, AX);
    BL = (uint8_t)(BL >> 1);
    AL = G8((uint16_t)(BX + 0x17D9));
    G8(0x1676) = AL;
    sg16(c, 0x17EA, 0x20);
    G8(0x17E9) = 1;
    G8(0x1675) = 0;
}
