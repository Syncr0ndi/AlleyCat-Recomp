// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
/* Alley Cat, hand-written: room 6, the sleeping dogs and the bowls of milk.
 *
 * 12 dog places ([4441+2n] occupied, sleep state [4459+2n]: 0 asleep .. 2 awake, x [43E1+2n],
 * y [43F9+2n], screen [4411+2n], picture [4429+2n]) and 12 bowls (milk left [44C4+n], x
 * [4481+2n], y [4499+n], picture [44A5+2n]). The cat laps a bowl with the button (lapping
 * picture at [43DC], [44BD] = lapping, [44D0] animation); emptying all ([44D6]) finishes the
 * room. A dog that wakes up near the cat catches it (2136). */
#include "hand.h"

/* 4C00: nothing */
void hand_4c00(V30 *c) { V30_TICK(20); }

/* 4BE8 bowl_addr: AX = screen address of bowl BX (BX kept). */
void hand_4be8(V30 *c) {
    uint16_t bx = BX;
    DL = G8((uint16_t)(BX + 0x4499));
    BL = (uint8_t)(BL << 1);
    CX = g16(c, (uint16_t)(BX + 0x4481));
    CALL(0x2CB0);
    BX = bx;
}

/* 4BC8 bowl_draw: bowl BX with its milk level. */
void hand_4bc8(V30 *c) {
    CALL(0x4BE8);
    DI = AX;
    AL = G8((uint16_t)(BX + 0x44C4));
    AH = 0;
    CL = 5;
    AX = (uint16_t)(AX << 5);
    AX = (uint16_t)(AX + 0x41FC);
    SI = AX;
    CX = 0x0802;
    AX = CGA_SEG; ES = AX;
    CALL(0x2D9D);
}

/* 4B03 lap_erase / 4B1D lap_draw: the lapping cat picture (two frames). */
void hand_4b03(V30 *c) {
    CMP8(G8(0x43E0), 0);
    V30_TICK(40);
    if (!c->zf) return;
    DI = g16(c, 0x43DE);
    SI = 0x43A0;
    AX = CGA_SEG; ES = AX;
    CX = 0x0A03;
    CALL(0x2D9D);
}
void hand_4b1d(V30 *c) {
    G8(0x43E0) = 0;
    AX = CGA_SEG; ES = AX;
    DI = g16(c, 0x43DC);
    sg16(c, 0x43DE, DI);
    BP = 0x43A0;
    SI = g16(c, 0x44D1);
    if (G8(0x44D0) >= 0x80) SI = (uint16_t)(SI + 0x3C);
    CX = 0x0A03;
    V30_TICK(60);
    CALL(0x2D35);
}

/* 4B47 room6_reset: dogs at random places (count by level), bowls full. */
void hand_4b47(V30 *c) {
    ES = DS;
    AX = 0;
    DI = 0x4441;
    CX = 0x0C;
    rep_stosw(c);
    AX = CGA_SEG; ES = AX;
    BX = g16(c, 0x0008);
    CL = G8((uint16_t)(BX + 0x4471));
    CH = 0;
    do {
        do {
            CALL(0x2DFD);
            BL = DL;
            BX &= 0x1E;
        } while (BL >= 0x18 || g16(c, (uint16_t)(BX + 0x4441)));
        sg16(c, (uint16_t)(BX + 0x4459), 0);
        sg16(c, (uint16_t)(BX + 0x4441), 1);
        uint16_t cx = CX;
        SI = g16(c, (uint16_t)(BX + 0x4429));
        DI = g16(c, (uint16_t)(BX + 0x4411));
        CX = 0x0D05;
        CALL(0x2D9D);
        CX = cx;
    } while (--CX);
    CX = 0x0C;
    do {
        BX = (uint16_t)(CX - 1);
        SI = g16(c, 0x0008);
        DL = G8((uint16_t)(SI + 0x4479));
        G8((uint16_t)(BX + 0x44C4)) = DL;
        uint16_t cx = CX;
        CALL(0x4BC8);
        CX = cx;
    } while (--CX);
    G8(0x44D0) = 0; G8(0x44BD) = 0; G8(0x43E0) = 1; G8(0x44D6) = 0x0C; G8(0x44BE) = 0;
}

/* 488D: remove the lapping picture, or the cat */
void hand_488d(V30 *c) {
    if (G8(0x44BD)) { CALL(0x4B03); G8(0x44BD) = 0; return; }
    CALL(0x11E3);
}

/* 48A1 dog_blank: blank the dog picture at [44DA]. */
void hand_48a1(V30 *c) {
    ES = DS;
    c->df = 0;
    DI = 0x0E;
    SI = DI;
    AX = 0xAAAA;
    CX = 0x41;
    rep_stosw(c);
    AX = CGA_SEG; ES = AX;
    DI = g16(c, 0x44DA);
    CX = 0x0D05;
    CALL(0x2D9D);
}

/* 48C1: redraw the cat (or the lapping picture) after a dog was redrawn over it */
void hand_48c1(V30 *c) {
    V30_TICK(30);
    if (!G8(0x44D9)) return;
    if (!G8(0x44BD)) { CALL(0x1145); return; }
    CALL(0x4B1D);
}

/* 48D7 dog_over_cat: if dog BX overlaps the cat, remove the cat (redrawn by 48C1). */
void hand_48d7(V30 *c) {
    G8(0x44D9) = 0;
    AX = g16(c, (uint16_t)(BX + 0x43E1));
    DX = g16(c, (uint16_t)(BX + 0x43F9));
    AX = (uint16_t)(AX - 0x14);
    SI = 0x28;
    BX = g16(c, 0x0579);
    DH = G8(0x057B);
    CX = 0x0E06;
    DI = 0x18;
    CALL(0x2E29);
    if (!c->cf) return;
    G8(0x44D9) = 1;
    do CALL(0x13D8); while (c->zf);
    if (G8(0x44BD)) { CALL(0x4B03); return; }
    CALL(0x11E3);
}

/* 4916 dog_zzz: the sleep-state picture of dog BX. */
void hand_4916(V30 *c) {
    AX = g16(c, (uint16_t)(BX + 0x4411));
    SI = (uint16_t)((g16(c, (uint16_t)(BX + 0x4459)) << 1) + 0x4100);
    AX = (uint16_t)(AX + 0xA7);
    if (g16(c, (uint16_t)(BX + 0x4429)) != 0x429C) { AX = (uint16_t)(AX - 6); SI = (uint16_t)(SI + 6); }
    DI = AX;
    AX = CGA_SEG; ES = AX;
    CX = 0x0101;
    CALL(0x2D9D);
}

/* 47D6 dogs_update: dogs on the cat's level wake up when it is near; awake next to the
 * cat they catch it; others doze off again now and then. */
void hand_47d6(V30 *c) {
    AH = 0;
    INT(0x1A);
    AX = (uint16_t)(DX - g16(c, 0x44D7));
    SI = (uint16_t)(g16(c, 0x0008) << 1);
    if (AX <= g16(c, (uint16_t)(SI + 0x44DC))) return;
    sg16(c, 0x44D7, DX);
    if (G8(0x1CB8)) return;
    G8(0x44FC) = 0;
    CX = 0x0C;
    do {
        BX = (uint16_t)(CX - 1);
        BL = (uint8_t)(BL << 1);
        if (!g16(c, (uint16_t)(BX + 0x4441))) continue;
        AX = g16(c, (uint16_t)(BX + 0x43F9));
        int near = 0;
        if (AL == G8(0x057B)) {
            uint16_t d = g16(c, (uint16_t)(BX + 0x43E1)), x = g16(c, 0x0579);
            AX = (uint16_t)(d - x);
            if (d < x) AX = (uint16_t)~AX;
            SI = (uint16_t)(g16(c, 0x0008) << 1);
            near = !(AX > g16(c, (uint16_t)(SI + 0x44EC)));
        }
        if (near) {
            if (g16(c, (uint16_t)(BX + 0x4459)) >= 2) {      /* awake: gets the cat */
                AX = g16(c, (uint16_t)(BX + 0x4411));
                sg16(c, 0x44DA, AX);
                CALL(0x488D);
                CALL(0x48A1);
                CALL(0x3339);
                CALL(0x1145);
                CALL(0x2136);
                return;
            }
            sg16(c, (uint16_t)(BX + 0x4459), (uint16_t)(g16(c, (uint16_t)(BX + 0x4459)) + 1));
            if (g16(c, (uint16_t)(BX + 0x4459)) >= 2) G8(0x44FC)++;
        } else {
            if (!g16(c, (uint16_t)(BX + 0x4459))) continue;
            CALL(0x2DFD);
            if (DL <= 0x38) sg16(c, (uint16_t)(BX + 0x4459), (uint16_t)(g16(c, (uint16_t)(BX + 0x4459)) - 1));
        }
        uint16_t cx = CX, bx = BX;
        CALL(0x48D7);
        BX = bx;
        CALL(0x4916);
        CALL(0x48C1);
        CX = cx;
    } while (--CX);
    if (G8(0x44FC)) CALL(0x5691);
}

/* 4943 lap_update: once per tick: with the button down at a bowl of milk at the cat's
 * height, lap from it (walking up to it first); otherwise stop lapping. */
void hand_4943(V30 *c) {
    if (G8(0x1CB8)) return;
    if (G8(0x44BE)) { AL = G8(0x44BE); G8(0x0698) = AL; G8(0x0699) = 0; }
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, 0x44D3)) return;
    sg16(c, 0x44D3, DX);
    if (G8(0x0584)) {                                    /* thrown: stop lapping */
        if (G8(0x44BD)) {
            CALL(0x4B03);
            CALL(0x33A0);
            CALL(0x1124);
            CALL(0x3339);
            G8(0x44BD) = 0; G8(0x43E0) = 1; G8(0x44BE) = 0;
        }
        return;
    }
    if (!G8(0x069A)) {
        AX = 0xFFFF;
        sg16(c, 0x44C1, AX);
        sg16(c, 0x44BF, AX);
        CX = 0x0C;
        SI = g16(c, 0x0579);
        DL = (uint8_t)(G8(0x057B) + 8);
        do {                                              /* the nearest bowl with milk */
            BX = (uint16_t)(CX - 1);
            if (G8((uint16_t)(BX + 0x44C4)) < 1) continue;
            if (DL != G8((uint16_t)(BX + 0x4499))) continue;
            AX = SI;
            BL = (uint8_t)(BL << 1);
            DH = 0xFF;
            uint16_t bx = g16(c, (uint16_t)(BX + 0x4481));
            if (AX < bx) { AX = (uint16_t)~(uint16_t)(AX - bx); DH = 1; } else AX = (uint16_t)(AX - bx);
            if (AX > g16(c, 0x44BF)) continue;
            sg16(c, 0x44BF, AX);
            AX = g16(c, (uint16_t)(BX + 0x44A5));
            sg16(c, 0x44D1, AX);
            BL = (uint8_t)(BL >> 1);
            sg16(c, 0x44C1, BX);
            G8(0x44C3) = DH;
        } while (--CX);
        if (g16(c, 0x44C1) < 0x0C) {
            if (g16(c, 0x44BF) >= 4) {                    /* walk to it */
                if (g16(c, 0x44BF) <= 8) G8(0x0572) = 4;
                AL = G8(0x44C3);
                G8(0x0698) = AL; G8(0x056E) = AL; G8(0x44BE) = AL;
                G8(0x0699) = 0; G8(0x0571) = 0;
            } else {
                /* 4A4B: lap */
                G8(0x44BE) = 0;
                if (!G8(0x44BD)) { CALL(0x11E3); CALL(0x1124); }
                G8(0x44BD) = 1;
                AL = 0;
                { unsigned s = (unsigned)G8(0x44D0) + 0x30; G8(0x44D0) = (uint8_t)s; if (s > 0xFF) AL++; }
                G8(0x44D5) = AL;
                CX = g16(c, 0x0579) & 0x0FFC;
                DL = (uint8_t)(G8(0x057B) + 3);
                if (g16(c, 0x44D1) == 0x410C) { CX = CX < 8 ? 0 : (uint16_t)(CX - 8); }
                else { CX = (uint16_t)(CX + 8); if (CX >= 0x127) CX = 0x126; }
                CALL(0x2CB0);
                sg16(c, 0x43DC, AX);
                do CALL(0x13D8); while (c->zf);
                CALL(0x4B03);
                if (G8(0x44D5)) {
                    BX = g16(c, 0x44C1);
                    if (G8((uint16_t)(BX + 0x44C4))) {
                        if (--G8((uint16_t)(BX + 0x44C4)) == 0) {   /* bowl empty */
                            uint16_t bx = BX;
                            AX = 0x08FD; BX = 0x0723;
                            CALL(0x593B);
                            BX = bx;
                            G8(0x0698) = 0; G8(0x44BE) = 0; G8(0x069A) = 0x10;
                            if (--G8(0x44D6) == 0) G8(0x0553) = 1;
                        }
                        uint16_t bx = BX;
                        CALL(0x47B0);
                        BX = bx;
                        if (c->cf) {
                            CALL(0x33A0);
                            BX = bx;
                            CALL(0x4BC8);
                            CALL(0x3339);
                        } else CALL(0x4BC8);
                    }
                }
                CALL(0x4B1D);
                return;
            }
        }
    }
    /* 49F9: not lapping */
    if (G8(0x44BD)) { CALL(0x4B03); CALL(0x1145); G8(0x069A) = 0x10; }
    G8(0x44BD) = 0; G8(0x43E0) = 1; G8(0x44D0) = 0; G8(0x44BE) = 0;
}
