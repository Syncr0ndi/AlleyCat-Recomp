/* Alley Cat, hand-written: room 3's spider and room 4's holes.
 *
 * Spider: x [3964], y [3966] (8 = up at the ceiling), vertical direction [3967], frame
 * [396B], screen [39CA]. It walks along the ceiling to above the cat, drops, climbs back.
 * Touching it ends the room ([0552]); it stops at the broom (3E52). */
#include "hand.h"

/* 3E14 spider_draw (saving the screen at 396F) */
void hand_3e14(V30 *c) {
    AX = CGA_SEG; ES = AX;
    DI = g16(c, 0x39CA);
    sg16(c, 0x3968, DI);
    BP = 0x396F;
    G8(0x396A) = 0;
    SI = (uint16_t)(g16(c, 0x396B) + 0x38BC);
    CX = 0x0E03;
    V30_TICK(60);
    CALL(0x2CCC);
}

/* 3E38 spider_erase */
void hand_3e38(V30 *c) {
    AX = CGA_SEG; ES = AX;
    CMP8(G8(0x396A), 0);
    V30_TICK(40);
    if (!c->zf) return;
    DI = g16(c, 0x3968);
    SI = 0x396F;
    CX = 0x0E03;
    CALL(0x2D9D);
}

/* 3E52 spider_touches_broom / 3E6E spider_touches_cat: CF=1 on overlap. */
void hand_3e52(V30 *c) {
    AX = g16(c, 0x3964); DL = G8(0x3966);
    SI = 0x18;
    BX = g16(c, 0x327D); DH = G8(0x327F);
    DI = 0x10; CX = 0x1E0E;
    CALL(0x2E29);
}
void hand_3e6e(V30 *c) {
    AX = g16(c, 0x3964); DL = G8(0x3966);
    SI = 0x18; DI = SI;
    BX = g16(c, 0x0579); DH = G8(0x057B);
    CX = 0x0E0E;
    CALL(0x2E29);
}

/* 3CB1 spider_update: every 2 ticks. */
void hand_3cb1(V30 *c) {
    AH = 0;
    INT(0x1A);
    AX = (uint16_t)(DX - g16(c, 0x39C8));
    if (AX < 2) return;
    sg16(c, 0x39C8, DX);
    CALL(0x3E52);
    if (c->cf) return;
    CALL(0x3E6E);
    if (c->cf) goto bite;
    BX = g16(c, 0x0008);
    BL = (uint8_t)(BL << 1);
    AX = g16(c, (uint16_t)(BX + 0x39CC));
    sg16(c, 0x39C6, AX);                                  /* speed by level */
    AX = g16(c, 0x3964);
    sg16(c, 0x39C3, AX);
    DL = G8(0x3966);
    G8(0x39C5) = DL;
    if (DL == 8) {                                        /* at the ceiling: walk to the cat */
        AX &= 0xFFF8;
        DX = g16(c, 0x0579) & 0xFFF8;
        if (AX == DX) {
            G8(0x3967) = 1;
            G8(0x396E) = 1;
        } else {
            int below = AX < DX;
            AX = g16(c, 0x3964);
            if (below) AX = (uint16_t)(AX + g16(c, 0x39C6));
            else AX = AX < g16(c, 0x39C6) ? 0 : (uint16_t)(AX - g16(c, 0x39C6));
            sg16(c, 0x3964, AX);
            goto moved;
        }
    }
    AL = G8(0x3966);
    G8(0x396E)++;
    DL = (uint8_t)(((G8(0x396E) >> 2) & 3) + 2);
    if (G8(0x3967) == 1) {                                /* dropping */
        AL = (uint8_t)(AL + DL);
        int up = 0;
        if (AL > G8(0x057B)) up = 1;
        else {
            uint16_t sx = g16(c, 0x3964), cx = g16(c, 0x0579);
            BX = (uint16_t)(sx - cx);
            if (sx < cx) BX = (uint16_t)~BX;
            if (BX > 0x30) up = 1;
            else if (AL >= 0xA0) { AL = 0x9F; up = 1; }
        }
        if (up) G8(0x3967) = 0xFF;
    } else {                                              /* climbing */
        uint8_t a = AL;
        AL = (uint8_t)(AL - DL);
        if (a < DL || AL < 9) { AL = 8; G8(0x3967) = 0; }
    }
    G8(0x3966) = AL;
moved:
    CALL(0x3E52);
    if (c->cf) {                                          /* the broom is there: undo */
        AX = g16(c, 0x39C3); sg16(c, 0x3964, AX);
        AL = G8(0x39C5); G8(0x3966) = AL;
        return;
    }
    CALL(0x3E6E);
    if (!c->cf) {
        CX = g16(c, 0x3964);
        DL = G8(0x3966);
        CALL(0x2CB0);
        sg16(c, 0x39CA, AX);
        CALL(0x3E38);
        if (--G8(0x396D) == 0) { G8(0x396D) = 2; sg16(c, 0x396B, g16(c, 0x396B) ^ 0x54); }
        CALL(0x3E14);
        return;
    }
bite:
    if (G8(0x0553)) return;
    CX = g16(c, 0x0579);
    CX = CX < 0x0C ? 0 : (uint16_t)(CX - 0x0C);
    if (CX >= 0x10F) CX = 0x10E;
    DL = G8(0x057B);
    DL = DL < 4 ? 0 : (uint8_t)(DL - 4);
    CALL(0x2CB0);
    DI = AX;
    AX = CGA_SEG; ES = AX;
    SI = 0x37C0; BP = 0x0E; CX = 0x1506;
    CALL(0x2CCC);
    CALL(0x56F4);
    AH = 0;
    INT(0x1A);
    sg16(c, 0x39C8, DX);
    do {
        CALL(0x5704);
        AH = 0;
        INT(0x1A);
        DX = (uint16_t)(DX - g16(c, 0x39C8));
    } while (DX < 9);
    G8(0x0552) = 1;
}

/* 3E90 room4_hole: on a shelf with the button pressed, the cat goes into the hole there and
 * comes out of the connected one (7 steps of a 2-frame animation per hole). */
void hand_3e90(V30 *c) {
    if (G8(0x39E1)) {
        AH = 0;
        INT(0x1A);
        if (DX == g16(c, 0x3D16)) return;
    } else {
        if (G8(0x0584) || G8(0x069A) || !G8(0x39E0)) return;
        CALL(0x4065);
        if (c->cf) return;
        AH = 0;
        INT(0x1A);
        AX = (uint16_t)(DX - g16(c, 0x3D18));
        if (AX < 0x0C) return;
        sg16(c, 0x3D18, DX);
        G8(0x055C) = 0;
        BL = (uint8_t)(G8(0x39E0) - 1);
        BH = 0;
        SI = BX;
        CL = 2;
        SI = (uint16_t)(SI << 2);
        AX = g16(c, (uint16_t)(SI + 0x3C5A));
        sg16(c, 0x39E2, AX);
        AX = 0;
        if (BL < 3) AL = 0x80;
        sg16(c, 0x39E4, AX);
        BL = G8((uint16_t)(BX + 0x3CE3));                 /* the hole it leads to */
        SI = BX;
        SI = (uint16_t)(SI << 2);
        AX = g16(c, (uint16_t)(SI + 0x3C5A));
        sg16(c, 0x39E6, AX);
        AX = 0;
        if (BL < 3) AL = 0x80;
        sg16(c, 0x39E8, AX);
        AL = G8((uint16_t)(BX + 0x1050));
        G8(0x3D05) = AL;
        BL = (uint8_t)(BL << 1);
        AX = (uint16_t)(g16(c, (uint16_t)(BX + 0x1137)) + 8);
        sg16(c, 0x3D03, AX);
        CALL(0x11E3);
        G8(0x39E1) = 0x0E;
        G8(0x069A) = 0x10;
    }
    if (!G8(0x1CBF)) CALL(0x33A0);
    G8(0x39E1) = (uint8_t)(G8(0x39E1) - 2);
    BH = 0;
    BL = G8(0x39E1);
    if (BL >= 8) {
        DI = g16(c, 0x39E2);
        AX = g16(c, 0x39E4);
    } else {
        DI = g16(c, 0x39E6);
        AL = G8(0x3D05);
        G8(0x057B) = AL;
        AL = (uint8_t)(AL + 0x32);
        G8(0x057C) = AL;
        AX = g16(c, 0x3D03);
        sg16(c, 0x0579, AX);
        AX = g16(c, 0x39E8);
    }
    AX = (uint16_t)(AX + g16(c, (uint16_t)(BX + 0x3D06)));
    SI = AX;
    AX = CGA_SEG; ES = AX;
    CX = 0x1002;
    CALL(0x2D9D);
    if (!G8(0x1CBF)) CALL(0x3339);
    AH = 0;
    INT(0x1A);
    sg16(c, 0x3D16, DX);
    if (!G8(0x39E1)) CALL(0x1112);
}
