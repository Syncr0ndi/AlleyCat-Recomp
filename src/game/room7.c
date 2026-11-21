/* Alley Cat, hand-written: rectangle overlap test, and room 7 (the wall of bricks).
 *
 * Room 7: a wall of brick rows (y at 2BD4, 7 rows; first index per row at 2BDB; brick
 * present = 0 in 2BE2). Up to 8 pieces ([2B72+n] on/off, x [2B5A+2n], y [2B6A+n]) can be
 * picked up by the cat ([2E8D] = carried piece) and put back with the button. */
#include "hand.h"

/* 2E29 overlap: CF=1 if box A (x AX, width SI... measured from the left by SI and DI, y DL
 * with heights CL / CH) touches point-box B (x BX, y DH). AX and DL are changed. */
void hand_2e29(V30 *c) {
    V30_TICK(90);
    AX = (uint16_t)(AX + SI);
    CMP16(AX, BX);
    if (c->cf) { c->cf = 0; return; }
    AX = (uint16_t)(AX - SI);
    { uint16_t a = AX; AX = (uint16_t)(AX - DI); if (a < DI) AX = 0; }
    CMP16(AX, BX);
    if (!c->cf && !c->zf) { c->cf = 0; return; }
    DL = (uint8_t)(DL + CL);
    CMP8(DL, DH);
    if (c->cf) { c->cf = 0; return; }
    DL = (uint8_t)(DL - CL);
    { uint8_t d = DL; DL = (uint8_t)(DL - CH); if (d < CH) DL = 0; }
    CMP8(DL, DH);
    if (!c->cf && !c->zf) { c->cf = 0; return; }
    c->cf = 1;
}

/* brick index for x AX (pixels) in row BX */
static void brick_index(V30 *c) {
    CL = 4;
    AX = (uint16_t)(AX >> 4);
    AX = AX < 2 ? 0 : (uint16_t)(AX - 2);
    if (AX >= 0x12) AX = 0x11;
    DL = G8((uint16_t)(BX + 0x2BDB));
    DH = 0;
    AX = (uint16_t)(AX + DX);
    SI = AX;
}

/* 30E3 brick_draw: brick picture BX (0 or 2) at x CX, y DL. */
void hand_30e3(V30 *c) {
    uint16_t bx = BX;
    CALL(0x2CB0);
    DI = AX;
    AX = CGA_SEG; ES = AX;
    BX = bx;
    SI = g16(c, (uint16_t)(BX + 0x2E20));
    CX = 0x0802;
    CALL(0x2D9D);
}

/* 30FA room7_on_brick: CF=1 (cat placed on it) if there is a brick under the cat. */
void hand_30fa(V30 *c) {
    AL = (uint8_t)((G8(0x057B) - 5) & 0xF8);
    CX = 7;
    int found = 0;
    do {
        BX = (uint16_t)(CX - 1);
        V30_TICK(40);
        if (AL == G8((uint16_t)(BX + 0x2BD4))) { found = 1; break; }
    } while (--CX);
    if (!found) { c->cf = 0; return; }
    CH = AL;
    AX = (uint16_t)(g16(c, 0x0579) + 7);
    brick_index(c);
    if (G8((uint16_t)(SI + 0x2BE2))) { c->cf = 0; return; }
    CH = (uint8_t)(CH + 5);
    G8(0x057B) = CH;
    CH = (uint8_t)(CH + 0x32);
    G8(0x057C) = CH;
    c->cf = 1;
}

/* 300F room7_draw: the wall (random brick shades), the pieces in their start places. */
void hand_300f(V30 *c) {
    AX = 0;
    BX = 0x2E24;
    CALL(0x2B24);
    G8(0x2E8A) = 0xBF;
    sg16(c, 0x2E8B, 0);
    do {
        sg16(c, 0x2E88, 0x20);
        do {
            BX = 0;
            if (G8(0x2E8A) != 0xBF) { CALL(0x2DFD); BL = DL; BL &= 2; }
            CX = g16(c, 0x2E88);
            DL = G8(0x2E8A);
            uint16_t bx = BX;
            CALL(0x30E3);
            BX = bx;
            SI = g16(c, 0x2E8B);
            AX = g16(c, 0x2E88);
            CL = 4;
            AX = (uint16_t)(AX >> 4);
            AX = AX < 2 ? 0 : (uint16_t)(AX - 2);
            if (AX >= 0x12) AX = 0x11;
            DL = G8((uint16_t)(SI + 0x2BDB));
            DH = 0;
            AX = (uint16_t)(AX + DX);
            SI = AX;
            G8((uint16_t)(SI + 0x2BE2)) = BL;
            sg16(c, 0x2E88, (uint16_t)(g16(c, 0x2E88) + 0x10));
        } while (g16(c, 0x2E88) < 0x111);
        sg16(c, 0x2E8B, (uint16_t)(g16(c, 0x2E8B) + 1));
        G8(0x2E8A) = (uint8_t)(G8(0x2E8A) - 0x18);
    } while (G8(0x2E8A) >= 0x2F);
    AX = 0xFFFF;
    sg16(c, 0x2E8D, AX);
    sg16(c, 0x2E94, AX);
    AX = 0;
    sg16(c, 0x2B72, AX); sg16(c, 0x2B74, AX); sg16(c, 0x2B76, AX); sg16(c, 0x2B78, AX);
    CX = g16(c, 0x0414);
    if (CX == 0) { CX++; sg16(c, 0x0414, CX); }
    if (CX > 8) CX = 8;
    do {
        BX = (uint16_t)(CX - 1);
        G8((uint16_t)(BX + 0x2B72)) = 1;
        DL = 0xB0;
        G8((uint16_t)(BX + 0x2B6A)) = DL;
        uint16_t cx = CX;
        BL = (uint8_t)(BL << 1);
        CX = g16(c, (uint16_t)(BX + 0x2B4A));
        sg16(c, (uint16_t)(BX + 0x2B5A), CX);
        CALL(0x2CB0);
        DI = AX;
        SI = 0x2AF0;
        CX = 0x0F03;
        CALL(0x2D9D);
        CX = cx;
    } while (--CX);
}

/* 2E60 room7_drop: with the button up, put the carried piece down at the nearest row
 * under the cat (if it does not overlap another piece). */
void hand_2e60(V30 *c) {
    V30_TICK(40);
    if (g16(c, 0x2E8D) >= 8 || G8(0x069A)) return;
    sg16(c, 0x2E92, 0xFFFF);
    G8(0x2E91) = 0xFF;
    CX = 7;
    do {
        BX = (uint16_t)(CX - 1);
        uint8_t y = G8(0x057B), r = G8((uint16_t)(BX + 0x2BD4));
        AL = (uint8_t)(y - r);
        if (y < r) AL = (uint8_t)~AL;
        if (!(AL > G8(0x2E91))) { G8(0x2E91) = AL; sg16(c, 0x2E92, BX); }
        V30_TICK(60);
    } while (--CX);
    if (g16(c, 0x2E92) == 0xFFFF) sg16(c, 0x2E92, 0);
    BX = g16(c, 0x2E8D);
    SI = g16(c, 0x2E92);
    AL = G8((uint16_t)(SI + 0x2BD4));
    G8((uint16_t)(BX + 0x2B6A)) = AL;
    G8(0x2E98) = AL;
    AX = g16(c, 0x0579);
    BL = (uint8_t)(BL << 1);
    if (AX >= 0x108) AX = 0x107;
    AX &= 0x0FFC;
    sg16(c, (uint16_t)(BX + 0x2B5A), AX);
    sg16(c, 0x2E96, AX);
    CX = 8;
    do {
        BX = (uint16_t)(CX - 1);
        if (BX == g16(c, 0x2E8D) || !G8((uint16_t)(BX + 0x2B72))) continue;
        uint16_t cx = CX;
        DL = G8((uint16_t)(BX + 0x2B6A));
        BL = (uint8_t)(BL << 1);
        AX = g16(c, (uint16_t)(BX + 0x2B5A));
        BX = g16(c, 0x2E96);
        DH = G8(0x2E98);
        SI = 0x18;
        DI = SI;
        CX = 0x0F0F;
        CALL(0x2E29);
        CX = cx;
        if (c->cf) return;
    } while (--CX);
    CALL(0x11E3);
    if (G8(0x70F2)) CALL(0x622B);
    BX = g16(c, 0x2E8D);
    sg16(c, 0x2E94, BX);
    G8((uint16_t)(BX + 0x2B72)) = 1;
    DL = G8((uint16_t)(BX + 0x2B6A));
    BL = (uint8_t)(BL << 1);
    CX = g16(c, (uint16_t)(BX + 0x2B5A));
    CALL(0x2CB0);
    DI = AX;
    SI = 0x2AF0;
    AX = CGA_SEG; ES = AX;
    CX = 0x0F03;
    CALL(0x2D9D);
    sg16(c, 0x2E8D, 0xFFFF);
    BX = 0;
    AH = 0x0B;
    INT(0x10);
    CALL(0x4E3E);
    if (G8(0x70F2)) CALL(0x61FA);
    CALL(0x1145);
    AX = 0x03E8; BX = 0x04A5;
    CALL(0x593B);
}

/* 2F66 room7_pickup: once per BIOS tick: the cat touching a piece picks it up (not the
 * one just put down). */
void hand_2f66(V30 *c) {
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, 0x2E8F)) return;
    sg16(c, 0x2E8F, DX);
    if (g16(c, 0x2E8D) < 8) { sg16(c, 0x2E94, 0xFFFF); return; }
    CX = 8;
    int hit = 0;
    do {
        BX = (uint16_t)(CX - 1);
        if (!G8((uint16_t)(BX + 0x2B72))) continue;
        uint16_t cx = CX;
        DL = G8((uint16_t)(BX + 0x2B6A));
        BL = (uint8_t)(BL << 1);
        AX = g16(c, (uint16_t)(BX + 0x2B5A));
        SI = 0x18;
        DI = SI;
        BX = g16(c, 0x0579);
        DH = G8(0x057B);
        CX = 0x0E0F;
        CALL(0x2E29);
        CX = cx;
        if (c->cf) { hit = 1; break; }
    } while (--CX);
    if (!hit) { sg16(c, 0x2E94, 0xFFFF); return; }
    BX = (uint16_t)(CX - 1);
    if (BX == g16(c, 0x2E94)) return;
    uint16_t bx = BX;
    CALL(0x11E3);
    if (G8(0x70F2)) CALL(0x622B);
    BX = bx;
    G8((uint16_t)(BX + 0x2B72)) = 0;
    DL = G8((uint16_t)(BX + 0x2B6A));
    sg16(c, 0x2E8D, BX);
    BL = (uint8_t)(BL << 1);
    CX = g16(c, (uint16_t)(BX + 0x2B5A));
    CALL(0x2CB0);
    DI = AX;
    SI = 0x2B7A;
    AX = CGA_SEG; ES = AX;
    CX = 0x0F03;
    CALL(0x2D9D);
    if (G8(0x70F2)) CALL(0x61FA);
    CALL(0x1145);
    BX = 1;
    AH = 0x0B;
    INT(0x10);
    AX = 0x03E8; BX = 0x0349;
    CALL(0x593B);
}
