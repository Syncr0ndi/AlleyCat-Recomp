/* Alley Cat, hand-written: room 4, the cheese room with mouse holes.
 *
 * Four mice (n = 0-3, [3EDA] the one moved this tick) each live in a hole ([3EBA+2n], one
 * of 16; position [3ECC+2n] / [3ED4+n]); [3EB6+n] is a timer: counting down they peek out,
 * sit, and go back; the cat catches one that is out (all four: room done, [3ED8] left).
 * Hidden [3EAE+n], caught [3EB2+n]. The room is a wall of cheese tiles with holes. */
#include "hand.h"

/* 4065 broom_touches_cat */
void hand_4065(V30 *c) {
    AX = g16(c, 0x327D); DL = G8(0x327F);
    SI = 0x10;
    BX = g16(c, 0x0579); DH = G8(0x057B);
    DI = 0x18; CX = 0x0E1E;
    CALL(0x2E29);
}

/* 431C near_cat: CF=1 if mouse (BX, SI) is within BP (x + y distance) of the cat. */
void hand_431c(V30 *c) {
    { uint16_t m = g16(c, (uint16_t)(SI + 0x3ECC)), x = g16(c, 0x0579);
      AX = (uint16_t)(m - x); if (m < x) AX = (uint16_t)~AX; }
    { uint8_t m = G8((uint16_t)(BX + 0x3ED4)), y = G8(0x057B);
      DL = (uint8_t)(m - y); if (m < y) DL = (uint8_t)~DL; }
    DH = 0;
    AX = (uint16_t)(AX + DX);
    c->cf = AX < BP;
    V30_TICK(90);
}

/* 42B4 mouse_at_hole: position of mouse BX at its hole. */
void hand_42b4(V30 *c) {
    DI = g16(c, (uint16_t)(SI + 0x3EBA));
    AL = G8((uint16_t)(DI + 0x1050));
    DL = DI < 3 ? 0 : 0x0A;
    AL = (uint8_t)(AL - DL);
    AL = (uint8_t)(AL + 3);
    G8((uint16_t)(BX + 0x3ED4)) = AL;
    DI = (uint16_t)(DI << 1);
    AX = (uint16_t)(g16(c, (uint16_t)(DI + 0x1137)) + 8);
    sg16(c, (uint16_t)(SI + 0x3ECC), AX);
    V30_TICK(90);
}

/* 4277 mouse_new_hole: a random free hole, preferably not next to the cat (32 tries). */
void hand_4277(V30 *c) {
    G8(0x3ED9) = 0x20;
    for (;;) {
        int taken;
        do {
            CALL(0x2DFD);
            DX &= 0x0F;
            taken = 0;
            for (DI = 0; DI < 8; DI += 2)
                if (DI != SI && DX == g16(c, (uint16_t)(DI + 0x3EBA))) { taken = 1; break; }
        } while (taken);
        sg16(c, (uint16_t)(SI + 0x3EBA), DX);
        CALL(0x42B4);
        if (!G8(0x3ED9)) return;
        BP = 0x32;
        CALL(0x431C);
        if (!c->cf) return;
        G8(0x3ED9)--;
    }
}

/* 42DB mouse_touches_cat / 42FC mouse_touches_broom (BX, SI kept) */
void hand_42db(V30 *c) {
    uint16_t si = SI, bx = BX;
    AX = g16(c, (uint16_t)(SI + 0x3ECC)); DL = G8((uint16_t)(BX + 0x3ED4));
    SI = 0x10;
    BX = g16(c, 0x0579); DH = G8(0x057B);
    DI = 0x18; CX = 0x0E0C;
    CALL(0x2E29);
    BX = bx; SI = si;
}
void hand_42fc(V30 *c) {
    uint16_t si = SI, bx = BX;
    AX = g16(c, (uint16_t)(SI + 0x3ECC)); DL = G8((uint16_t)(BX + 0x3ED4));
    SI = 0x10;
    BX = g16(c, 0x327D); DH = G8(0x327F);
    DI = SI; CX = 0x1E0C;
    CALL(0x2E29);
    BX = bx; SI = si;
}

/* 4254 mouse_erase: put back the screen under mouse [3EDA] if it is out. */
void hand_4254(V30 *c) {
    BX = g16(c, 0x3EDA);
    SI = (uint16_t)(BX << 1);
    CMP8(G8((uint16_t)(BX + 0x3EAE)), 0);
    V30_TICK(40);
    if (!c->zf) return;
    DI = g16(c, (uint16_t)(SI + 0x3EA6));
    CX = 0x0C02;
    SI = g16(c, (uint16_t)(SI + 0x3EC2));
    AX = CGA_SEG; ES = AX;
    CALL(0x2D9D);
}

/* 4090 room4_reset */
void hand_4090(V30 *c) {
    CX = 4;
    do {
        BX = (uint16_t)(CX - 1);
        SI = (uint16_t)(BX << 1);
        G8((uint16_t)(BX + 0x3EAE)) = 1;
        G8((uint16_t)(BX + 0x3EB2)) = 0;
        CALL(0x4277);
        CALL(0x2DFD);
        DL &= 0x0F;
        DL = (uint8_t)(DL + 0x14);
        G8((uint16_t)(BX + 0x3EB6)) = DL;
    } while (--CX);
    sg16(c, 0x3EDA, 0);
    G8(0x3ED8) = 4;
}

/* 40C2 room4_update: one mouse per BIOS tick (the 4th shares the 3rd's tick). */
void hand_40c2(V30 *c) {
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, 0x3EDC)) return;
    sg16(c, 0x3EDA, (uint16_t)(g16(c, 0x3EDA) + 1));
    BX = g16(c, 0x3EDA);
    if (BX <= 2) sg16(c, 0x3EDC, DX);
    else if (BX >= 4) { BX = 0; sg16(c, 0x3EDA, BX); sg16(c, 0x3EDC, DX); }
    SI = (uint16_t)(BX << 1);
    if (G8((uint16_t)(BX + 0x3EB2))) return;
    CALL(0x42DB);
    if (!c->cf) {
        CALL(0x42FC);
        if (c->cf) return;
        if (!G8((uint16_t)(BX + 0x3EB6))) {               /* back in: choose a new hole */
            CALL(0x4277);
            CALL(0x2DFD);
            DL &= 7;
            DL = (uint8_t)(DL + 0x14);
            G8((uint16_t)(BX + 0x3EB6)) = DL;
        }
        G8((uint16_t)(BX + 0x3EB6))--;
        CALL(0x42DB);
        if (!c->cf) goto animate;
    }
    /* 4124: caught by the cat? */
    if (G8((uint16_t)(BX + 0x3EAE)) || G8((uint16_t)(BX + 0x3EB6)) >= 0x14) return;
    CALL(0x11E3);
    CALL(0x4254);
    BX = g16(c, 0x3EDA);
    G8((uint16_t)(BX + 0x3EB2)) = 1;
    CALL(0x1124);
    G8(0x055C) = 0;
    if (--G8(0x3ED8) == 0) G8(0x0553) = 1;
    AL = (uint8_t)(4 - G8(0x3ED8));
    CL = 2;
    AL = (uint8_t)(AL << 2);
    AH = 0;
    AX = (uint16_t)(AX + 0x51);
    DI = AX;                                              /* trophy at the top */
    BP = 0x0E; SI = 0x3D20;
    AX = CGA_SEG; ES = AX;
    CX = 0x0C02;
    CALL(0x2D35);
    AX = 0x03E8; BX = 0x02EE;
    CALL(0x593B);
    return;

animate:
    CALL(0x42B4);
    DI = (uint16_t)(g16(c, 0x0008) << 1);
    BP = g16(c, (uint16_t)(DI + 0x3EDE));
    CALL(0x431C);
    if (c->cf && G8((uint16_t)(BX + 0x3EB6)) >= 2) {      /* the cat is near: hide */
        uint8_t t = G8((uint16_t)(BX + 0x3EB6));
        AL = 1;
        if (t > 0x11) { if (t < 0x14) { AL--; G8((uint16_t)(BX + 0x3EB6)) = AL; } }
        else G8((uint16_t)(BX + 0x3EB6)) = AL;
    }
    AL = G8((uint16_t)(BX + 0x3EB6));
    if (AL <= 1 || AL >= 0x12) {                          /* coming out / going in */
        AL = g16(c, (uint16_t)(SI + 0x3EBA)) < 3 ? 3 : 1;
        G8((uint16_t)(BX + 0x3ED4)) = (uint8_t)(G8((uint16_t)(BX + 0x3ED4)) + AL);
        uint8_t t = G8((uint16_t)(BX + 0x3EB6));
        if (t <= 1) AX = t >= 1 ? 0x3D80 : 0x3DB0;
        else if (t < 0x13) AX = 0x3D80;
        else if (t == 0x13) AX = 0x3DB0;
        else AX = 0;
    } else {                                              /* sitting: two frames */
        AL = (uint8_t)(AL << 1);
        DI = AX;
        DI &= 2;
        AX = g16(c, (uint16_t)(DI + 0x3DE0));
    }
    sg16(c, 0x3ECA, AX);
    DL = G8((uint16_t)(BX + 0x3ED4));
    CX = g16(c, (uint16_t)(SI + 0x3ECC));
    CALL(0x2CB0);
    sg16(c, 0x3DE4, AX);
    CALL(0x4254);
    BX = g16(c, 0x3EDA);
    SI = (uint16_t)(BX << 1);
    CALL(0x42FC);
    if (c->cf) return;
    if (!g16(c, 0x3ECA)) { G8((uint16_t)(BX + 0x3EAE)) = 1; return; }
    G8((uint16_t)(BX + 0x3EAE)) = 0;
    DI = g16(c, 0x3DE4);
    sg16(c, (uint16_t)(SI + 0x3EA6), DI);
    AX = CGA_SEG; ES = AX;
    BP = g16(c, (uint16_t)(SI + 0x3EC2));
    CX = 0x0C02;
    SI = g16(c, 0x3ECA);
    CALL(0x2D35);
}

/* 3F9E room4_draw: cheese wall (random tiles with holes), shelves and pictures, and the
 * hole connections of this level. */
void hand_3f9e(V30 *c) {
    AX = CGA_SEG; ES = AX;
    G8(0x39E0) = 0;
    G8(0x39E1) = 0;
    sg16(c, 0x3CBF, 0x0506);
    sg16(c, 0x3CC1, 0);
    do {
        BX = g16(c, 0x3CC1);
        CL = G8((uint16_t)(BX + 0x3CAE));
        BX = 0;
        CH = BL;
        do {
            SI = 0x3AEA;
            CALL(0x2DFD);
            if (DL <= 0x30) { SI = 0x3AF8; if (!(DL & 4)) SI = 0x3B02; }
            DI = (uint16_t)(g16(c, 0x3CBF) + BX);
            uint16_t cx = CX, bx = BX;
            CX = 0x0801;
            CALL(0x2D9D);
            BX = bx; CX = cx;
            BX += 2;
        } while (--CX);
        sg16(c, 0x3CBF, (uint16_t)(g16(c, 0x3CBF) + 0x140));
        sg16(c, 0x3CC1, (uint16_t)(g16(c, 0x3CC1) + 1));
    } while (g16(c, 0x3CC1) < 0x11);
    BX = 0x3C22; AX = 0; CALL(0x2B24);
    BX = 0x3C3E; AX = 0; CALL(0x2B24);
    BX = 0x3C9A; AX = 0; CALL(0x2B24);
    BX = 0x3C56; AX = 0; CALL(0x2B24);
    SI = 0x3CAA; DI = 0x08EC; CX = 0x0102; BP = 0x0E;
    CALL(0x2D35);
    SI = 0;
    BX = g16(c, 0x0008);
    CL = 3;
    BL &= CL;
    BL = (uint8_t)(BL << CL);
    do {
        AL = G8((uint16_t)(BX + 0x3CC3));
        AH = AL;
        CL = 4;
        AL = (uint8_t)(AL >> 4);
        G8((uint16_t)(SI + 0x3CE3)) = AL;
        G8((uint16_t)(SI + 0x3CF3)) = 0;
        AH &= 0x0F;
        G8((uint16_t)(SI + 0x3CE4)) = AH;
        G8((uint16_t)(SI + 0x3CF4)) = 0;
        SI += 2;
        BX++;
    } while (SI < 0x10);
}
