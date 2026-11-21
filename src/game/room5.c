// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
/* Alley Cat, hand-written: room 5, the bird cage.
 *
 * Cage: x [40A8], y [40AA] (it falls when pushed off the table, with a falling tone, and
 * breaks at A4h), pushed state [40AF] / direction [40B0], fallen [40B1]. Bird (free once
 * the cage broke): x [40B2], y [40B4], directions [40B7] / [40B8], target perch [40C8].
 * Catching the bird finishes the room. */
#include "hand.h"

/* 4759 cage_draw / 4773 cage_erase */
void hand_4759(V30 *c) {
    AX = CGA_SEG; ES = AX;
    BP = 0x401E; SI = 0x3FBE;
    DI = g16(c, 0x40AB);
    sg16(c, 0x40A6, DI);
    CX = 0x1003;
    V30_TICK(60);
    CALL(0x2D35);
}
void hand_4773(V30 *c) {
    AX = CGA_SEG; ES = AX;
    SI = 0x401E;
    DI = g16(c, 0x40A6);
    CX = 0x1003;
    V30_TICK(50);
    CALL(0x2D9D);
}

/* 473E cage_touches_cat / 4786 broom_at_cage / 452D bird_touches_cat / 4557 bird_touches_broom */
void hand_473e(V30 *c) {
    AX = g16(c, 0x40A8); DL = G8(0x40AA);
    SI = 0x18;
    BX = g16(c, 0x0579); DH = G8(0x057B);
    DI = SI; CX = 0x0E10;
    CALL(0x2E29);
}
void hand_4786(V30 *c) {
    V30_TICK(60);
    if (G8(0x327F) < 0x66) { c->cf = 0; return; }
    AX = (uint16_t)(g16(c, 0x40A8) - 0x14);
    if (AX > g16(c, 0x327D)) { c->cf = 0; return; }
    AX = (uint16_t)(AX + 0x30);
    c->cf = !(AX < g16(c, 0x327D));
}
void hand_452d(V30 *c) {
    AX = g16(c, 0x40B2); DL = G8(0x40B4);
    SI = 8;
    BX = g16(c, 0x0579); DH = G8(0x057B);
    DI = 0x18; CX = 0x0E05;
    CALL(0x2E29);
    if (c->cf && !G8(0x0552)) G8(0x0553) = 1;
}
void hand_4557(V30 *c) {
    AX = g16(c, 0x40B2); DL = G8(0x40B4);
    SI = 8;
    BX = g16(c, 0x327D); DH = G8(0x327F);
    DI = 0x10; CX = 0x1E05;
    CALL(0x2E29);
    if (c->cf) G8(0x40B8) = 0xFF;
}

/* 44E7 cat_on_table: CF=1 if the cat stands at table height (88h). */
void hand_44e7(V30 *c) {
    V30_TICK(40);
    c->cf = !G8(0x0571) && (G8(0x057B) & 0xF8) == 0x88;
}

/* 44FB bird_flee_dir: direction away from the cat ([40CA], [40CB]) and distance [40CC]. */
void hand_44fb(V30 *c) {
    { uint16_t b = g16(c, 0x40B2), x = g16(c, 0x0579);
      AX = (uint16_t)(b - x); DL = 1; if (b < x) { AX = (uint16_t)~AX; DL = 0xFF; } }
    G8(0x40CA) = DL;
    sg16(c, 0x40CC, AX);
    { uint8_t b = G8(0x40B4), y = G8(0x057B);
      AL = (uint8_t)(b - y); DL = 1; if (b < y) { AL = (uint8_t)~AL; DL = 0xFF; } }
    G8(0x40CB) = DL;
    AH = 0;
    AX = (uint16_t)(AX << 1);
    sg16(c, 0x40CC, (uint16_t)(g16(c, 0x40CC) + AX));
    V30_TICK(120);
}

/* 457A cage_reset: cage on the table. */
void hand_457a(V30 *c) {
    CX = 0x90; DL = 0x86;
    sg16(c, 0x40A8, CX);
    G8(0x40AA) = DL;
    CALL(0x2CB0);
    sg16(c, 0x40AB, AX);
    CALL(0x4759);
    G8(0x40AF) = 0; G8(0x40B1) = 0; G8(0x40B9) = 1; G8(0x40B8) = 0;
    sg16(c, 0x40C8, 0xFF);
}

static uint8_t rnd_dir(V30 *c) {
    CALL(0x2DFD);
    DL &= 1;
    if (!DL) DL = 0xFF;
    return DL;
}

/* 4340 bird_update: flies around: away from the cat when it is close, to a perch now and
 * then, at random otherwise. */
void hand_4340(V30 *c) {
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, 0x40B5)) return;
    G8(0x40FF)++;
    if (G8(0x40FF) & 3) sg16(c, 0x40B5, DX);
    if (G8(0x40AA) < 0xA4) return;                       /* still in the cage */
    CALL(0x452D);
    CALL(0x4557);
    if (c->cf) return;
    CALL(0x2DFD);
    if (DL <= 0x30) {
        CALL(0x44FB);
        SI = (uint16_t)(g16(c, 0x0008) << 1);
        AX = g16(c, (uint16_t)(SI + 0x40CE));
        if (g16(c, 0x40CC) <= AX) {                      /* too close: flee */
            CALL(0x595D);
            sg16(c, 0x40C8, 0xFF);
            AL = G8(0x40CA); G8(0x40B7) = AL;
            AL = G8(0x40CB); G8(0x40B8) = AL;
            goto fly;
        }
    }
    if (g16(c, 0x40C8) <= 0x0A) {                        /* heading for perch [40C8] */
        CALL(0x2DFD);
        if (DL > 6) {
            BX = g16(c, 0x40C8);
            SI = (uint16_t)(BX << 1);
            AX = g16(c, 0x40B2) & 0x0FFC;
            DL = AX == g16(c, (uint16_t)(SI + 0x40DE)) ? 0 : AX < g16(c, (uint16_t)(SI + 0x40DE)) ? 1 : 0xFF;
            G8(0x40B7) = DL;
            AL = G8(0x40B4) & 0xFE;
            DL = AL == G8((uint16_t)(BX + 0x40F4)) ? 0 : AL < G8((uint16_t)(BX + 0x40F4)) ? 1 : 0xFF;
            G8(0x40B8) = DL;
            DL |= G8(0x40B7);
            if (DL) goto fly;
            CALL(0x2DFD);                                /* sitting there: maybe leave */
            if (DL > 0x10) goto fly;
            sg16(c, 0x40C8, 0xFF);
            CALL(0x595D);
        } else sg16(c, 0x40C8, 0xFF);
    }
    CALL(0x2DFD);                                        /* 4402: random flight, new target */
    if (DL <= 0x30) {
        DL &= 1;
        if (!DL) DL = 0xFF;
        G8(0x40B7) = DL;
        G8(0x40B8) = rnd_dir(c);
    }
    CALL(0x2DFD);
    DX &= 0xFF;
    sg16(c, 0x40C8, DX);
fly:
    AL = G8(0x40B4);
    if (G8(0x40B8) == 1) {
        AL = (uint8_t)(AL + 2);
        if (AL >= 0xA8) { AL = 0xA7; G8(0x40B8) = 0xFF; }
        G8(0x40B4) = AL;
    } else if (G8(0x40B8) > 1) {
        AL = (uint8_t)(AL - 2);
        if (AL < 0x30) { AL = 0x30; G8(0x40B8) = 1; }
        G8(0x40B4) = AL;
    }
    AX = g16(c, 0x40B2);
    if (G8(0x40B7) == 1) {
        AX = (uint16_t)(AX + 4);
        if (AX >= 0x136) { AX = 0x135; G8(0x40B7) = 0xFF; }
        sg16(c, 0x40B2, AX);
    } else if (G8(0x40B7) > 1) {
        if (AX < 4) { AX = 0; G8(0x40B7) = 1; } else AX -= 4;
        sg16(c, 0x40B2, AX);
    }
    CALL(0x452D);
    CX = g16(c, 0x40B2);
    DL = G8(0x40B4);
    CALL(0x2CB0);
    sg16(c, 0x40BC, AX);
    AX = CGA_SEG; ES = AX;
    if (!G8(0x40B9)) {
        SI = 0x3F2C;
        DI = g16(c, 0x40BA);
        CX = 0x0501;
        CALL(0x2D9D);
    }
    CALL(0x4557);
    if (c->cf) return;
    G8(0x40B9) = 0;
    sg16(c, 0x40BE, (uint16_t)(g16(c, 0x40BE) + 2));
    BX = g16(c, 0x40BE) & 6;
    SI = g16(c, (uint16_t)(BX + 0x40C0));
    if (G8(0x40B7) == 0xFF) SI = (uint16_t)(SI + 0x1E);
    DI = g16(c, 0x40BC);
    sg16(c, 0x40BA, DI);
    BP = 0x3F2C;
    CX = 0x0501;
    CALL(0x2CCC);
}

/* 45AB cage_update: the cat pushes the cage along the table; off the edge it falls
 * (rising tone) and breaks, freeing the bird. */
void hand_45ab(V30 *c) {
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, 0x40AD)) return;
    sg16(c, 0x40AD, DX);
    if (G8(0x40AA) >= 0xA4) return;
    CALL(0x4786);
    if (c->cf) {                                         /* the broom knocks the cat off */
        CALL(0x44E7);
        if (!c->cf) return;
        G8(0x0571) = 1; G8(0x055B) = 0x10;
        return;
    }
    CALL(0x473E);
    if (c->cf) {                                         /* pushing: move the cat out */
        if (!G8(0x40AF)) {
            AL = G8(0x056E);
            if (AL == 0) {
                AL++;
                if (!(g16(c, 0x40A8) > g16(c, 0x0579))) AL = 0xFF;
            }
            G8(0x40B0) = AL;
        }
        G8(0x40AF) = 1;
        CX = 0x20;
        do {
            AX = g16(c, 0x0579);
            if (G8(0x40B0) == 1) { AX = (uint16_t)(AX - 8); DL = 0xFF; }
            else { AX = (uint16_t)(AX + 8); DL = 1; }
            sg16(c, 0x0579, AX);
            G8(0x056E) = DL;
            AL = G8(0x057B);
            if (G8(0x0571) >= 1) {
                AL = G8(0x0571) == 1 ? (uint8_t)(AL - 3) : (uint8_t)(AL + 3);
                G8(0x057B) = AL;
                AL = (uint8_t)(AL + 0x32);
                G8(0x057C) = AL;
            }
            uint16_t cx = CX;
            CALL(0x473E);
            CX = cx;
            if (!c->cf) break;
        } while (--CX);
        CALL(0x11E3);
        CALL(0x1112);
        return;
    }
    if (!G8(0x40B1)) {
        if (!G8(0x40AF)) return;
        AX = g16(c, 0x40A8);
        AX = G8(0x40B0) == 1 ? (uint16_t)(AX + 8) : (uint16_t)(AX - 8);
        sg16(c, 0x40A8, AX);
        CALL(0x473E);
        if (c->cf) return;
        AX = 0x0C00; BX = 0x0B54;
        CALL(0x593B);
        G8(0x40AF) = 0;
        CX = g16(c, 0x40A8);
        DL = G8(0x40AA);
        CALL(0x2CB0);
        sg16(c, 0x40AB, AX);
        CALL(0x4773);
        CALL(0x4759);
        AX = g16(c, 0x40A8);
        if (AX >= 0x78 && AX <= 0xA8) return;            /* still on the table */
    }
    for (;;) {                                           /* 46A2: falling */
        G8(0x40B1) = 1;
        if (G8(0x1CBF)) {
            CALL(0x44E7);
            if (c->cf) { G8(0x0571) = 1; G8(0x055B) = 0x10; }
            return;
        }
        AH = 0;
        INT(0x1A);
        if (DX == g16(c, 0x40AD)) continue;
        sg16(c, 0x40AD, DX);
        if (G8(0x0000)) {
            AL = 0xB6;
            v30_out(c, 0x43, AL, 0);
            AL = G8(0x40AA);
            AH = 0;
            AX = (uint16_t)(AX << 2);
            v30_out(c, 0x42, AL, 0);
            AL = AH;
            v30_out(c, 0x42, AL, 0);
            AL = (uint8_t)v30_in(c, 0x61, 0);
            AL |= 3;
            v30_out(c, 0x61, AL, 0);
        }
        DL = G8(0x40AA);
        if (DL >= 0xA4) break;
        DL = (uint8_t)(DL + 5);
        G8(0x40AA) = DL;
        CX = g16(c, 0x40A8);
        CALL(0x2CB0);
        sg16(c, 0x40AB, AX);
        CALL(0x4773);
        CALL(0x4759);
    }
    CALL(0x5B21);                                        /* broken cage, bird free */
    CALL(0x4773);
    BP = 0x401E;
    sg16(c, 0x40A6, (uint16_t)(g16(c, 0x40A6) - 1));
    DI = g16(c, 0x40A6);
    SI = 0x3F36; CX = 0x1104;
    CALL(0x2D35);
    AX = g16(c, 0x40A8); sg16(c, 0x40B2, AX);
    AL = G8(0x40AA); G8(0x40B4) = AL;
    CALL(0x44FB);
    AL = G8(0x40CA);
    G8(0x40B7) = AL;
}

/* 47B0 broom_touches_cat (room 6 variant, wider) */
void hand_47b0(V30 *c) {
    AX = g16(c, 0x327D); DL = G8(0x327F);
    SI = 0x10;
    BX = g16(c, 0x0579);
    BX = BX < 8 ? 0 : (uint16_t)(BX - 8);
    DH = (uint8_t)(G8(0x057B) + 3);
    DI = 0x28; CX = 0x0E1E;
    CALL(0x2E29);
}
