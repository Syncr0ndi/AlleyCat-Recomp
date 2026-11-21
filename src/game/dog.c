/* Alley Cat, hand-written: the bulldog.
 *
 * The dog runs in from a side when the cat is on the ground ([1CC0] entering steps,
 * [1CBF] on screen), chases it ([1CD0] direction, x [1CC6], y [1CC8]) and, when it catches
 * the cat ([1CB8] = side it was knocked to, [1CB9] steps left), throws it out of the room /
 * costs a life. [1CC1] counts the leaving steps. Sprites: frames at 15C8, saved screen
 * at 1C40. */
#include "hand.h"

#define V_DOG_X   0x1CC6
#define V_DOG_Y   0x1CC8
#define V_DOG_DIR 0x1CD0
#define V_CAUGHT  0x1CB8

/* 2210 dog_flag_clear */
void hand_2210(V30 *c) { G8(0x1D59) = 0; V30_TICK(20); }

/* 1E40 dog_reset */
void hand_1e40(V30 *c) {
    G8(0x1CBF) = 0; sg16(c, 0x1CE1, 0); G8(0x1CC0) = 0; G8(0x1CC1) = 0; G8(V_CAUGHT) = 0;
    G8(V_DOG_Y) = 0xB1;
    V30_TICK(100);
    CALL(0x5450);
}

/* 2022 dog_frame: next running frame (fighting frames when it has caught the cat). */
void hand_2022(V30 *c) {
    BH = 0;
    if (G8(V_CAUGHT)) {
        G8(0x1CCF)++;
        BL = (uint8_t)((G8(0x1CCF) & 6) | 8);
    } else {
        G8(0x1CCF) = (uint8_t)(G8(0x1CCF) + 2);
        BL = G8(0x1CCF) & 2;
        if (G8(V_DOG_DIR) == 1) BL |= 4;
    }
    AX = g16(c, (uint16_t)(BX + 0x15C8));
    sg16(c, 0x1CBB, AX);
    V30_TICK(90);
}

/* 2059 dog_edge_frame: the dog partly off screen (AL words hidden, AH = side): copy the
 * visible part of the frame to 000E. */
void hand_2059(V30 *c) {
    CX = 0x0F04;
    CL = (uint8_t)(CL - AL);
    sg16(c, 0x1CC4, CX);
    if (AH == 0xFF) {
        AH = 0;
        AL = (uint8_t)(AL << 3);
        AX = (uint16_t)(AX + 0x120);
        sg16(c, V_DOG_X, AX);
    } else {
        AH = 0;
        AL = (uint8_t)(AL << 1);
        sg16(c, 0x1CBB, (uint16_t)(g16(c, 0x1CBB) + AX));
        sg16(c, V_DOG_X, 0);
    }
    ES = DS;
    SI = g16(c, 0x1CBB);
    DI = 0x0E;
    AL = 4;
    V30_TICK(150);
    CALL(0x2D70);
    sg16(c, 0x1CBB, 0x0E);
}

/* 209B dog_draw: draw frame [1CBB] at [1CBD], saving the screen (when it has the cat,
 * drawn opaque). */
void hand_209b(V30 *c) {
    CX = g16(c, 0x1CC4);
    sg16(c, 0x1CC2, CX);
    AX = CGA_SEG;
    V30_TICK(80);
    if (!G8(V_CAUGHT)) {
        ES = AX;
        DI = g16(c, 0x1CBD);
        SI = g16(c, 0x1CBB);
        BP = 0x1C40;
        CALL(0x2CCC);
        return;
    }
    uint16_t ds = DS;
    DS = AX;
    ES = ds;
    SI = m16(c, ES, 0x1CBD);
    DI = 0x1C40;
    CALL(0x2DCA);
    ES = CGA_SEG;                                        /* POP ES: the screen; AX stays B800 */
    DS = ds;
    SI = g16(c, 0x1CBB);
    DI = g16(c, 0x1CBD);
    CX = g16(c, 0x1CC4);
    CALL(0x2D9D);
}

/* 20E1 dog_erase */
void hand_20e1(V30 *c) {
    AX = CGA_SEG; ES = AX;
    DI = g16(c, 0x1CBD);
    SI = 0x1C40;
    CX = g16(c, 0x1CC2);
    V30_TICK(70);
    CALL(0x2D9D);
}

/* 2136 dog_catch: the dog meets the cat: fight between them, then the cat flies off to
 * the far side ([1CB8] direction, [1CB9] steps). CF=1. */
void hand_2136(V30 *c) {
    if (g16(c, 0x0004) == 6) {
        AL = G8(0x057B);
        G8(V_DOG_Y) = AL;
        AX = g16(c, 0x0579);
        sg16(c, V_DOG_X, AX);
    }
    AX = (uint16_t)((uint16_t)(g16(c, V_DOG_X) + g16(c, 0x0579)) >> 1);
    if (AX >= 0x118) AX = 0x117;
    sg16(c, V_DOG_X, AX);
    if (AX > 0xA0) { BL = 1; AX = (uint16_t)(AX - 0x9F); DX = AX; }
    else { BL = 0xFF; DX = (uint16_t)(0xA1 - AX); }
    G8(V_CAUGHT) = BL;
    G8(0x1CBF) = 1;
    G8(0x1CC1) = 0;
    CL = 3;
    DX >>= 3;
    sg16(c, 0x1CB9, DX);
    V30_TICK(200);
    if (g16(c, 0x0004) == 6) {
        CALL(0x11E3);
        AL = G8(V_CAUGHT);
        uint16_t ax = AX;
        G8(V_CAUGHT) = 0;
        sg16(c, 0x1CC4, 0x0F04);
        AX = g16(c, 0x15C8);
        sg16(c, 0x1CBB, AX);
        CX = g16(c, V_DOG_X);
        DL = G8(V_DOG_Y);
        CALL(0x2CB0);
        sg16(c, 0x1CBD, AX);
        CALL(0x209B);
        AX = ax;
        G8(V_CAUGHT) = AL;
    }
    CALL(0x20E1);
    CALL(0x11E3);
    AX = g16(c, 0x0579) >= 0xA0 ? 0 : 0x122;
    sg16(c, 0x0579, AX);
    if (g16(c, 0x0004) == 0) CALL(0x070D);
    c->cf = 1;
}

/* 20F5 dog_meets_cat: CF=1 if the dog (on screen, not busy) is at the cat on the ground. */
void hand_20f5(V30 *c) {
    V30_TICK(80);
    if (G8(V_CAUGHT)) { c->cf = 0; return; }
    AL = (uint8_t)(G8(0x1CBF) | G8(0x1CC0) | G8(0x1CC1));
    if (!AL) { c->cf = 0; return; }
    if (G8(0x057B) < 0xA3 || G8(0x0558)) { c->cf = 0; return; }
    AX = (uint16_t)(g16(c, V_DOG_X) + 0x20);
    if (AX < g16(c, 0x0579)) { c->cf = 0; return; }
    AX = AX < 0x38 ? 0 : (uint16_t)(AX - 0x38);
    if (AX > g16(c, 0x0579)) { c->cf = 0; return; }
    CALL(0x2136);
}

/* 21E0 dog_touch: CF=1 if the dog overlaps the thing at [327D]/[327F]. */
void hand_21e0(V30 *c) {
    AL = (uint8_t)(G8(0x1CBF) | G8(0x1CC0) | G8(0x1CC1));
    V30_TICK(50);
    if (!AL) { c->cf = 0; return; }
    AX = g16(c, 0x327D);
    DL = G8(0x327F);
    SI = 0x10;
    BX = g16(c, V_DOG_X);
    DH = G8(V_DOG_Y);
    DI = 0x20;
    CX = 0x0F1E;
    CALL(0x2E29);
}

/* 1E63 dog_update: every 1-2 BIOS ticks (outside the retrace). */
void hand_1e63(V30 *c) {
    AH = 0;
    INT(0x1A);
    CX = DX;
    DX = (uint16_t)(DX - g16(c, 0x1CC9));
    AX = (uint16_t)((g16(c, 0x1CE1) & 1) + 1);
    if (DX < AX) return;
    CALL(0x13D8);
    if (c->zf) return;
    sg16(c, 0x1CC9, CX);
    sg16(c, 0x1CE1, (uint16_t)(g16(c, 0x1CE1) + 1));

    if (G8(0x1CC1)) {                                    /* leaving */
        if (--G8(0x1CC1) == 0) {
            CALL(0x5B21);
            if (G8(V_CAUGHT)) {
                if (g16(c, 0x0004) != 0) {               /* in a room: thrown out */
                    G8(0x0552) = 0xDD; sg16(c, 0x0579, 0xA0); G8(0x057B) = 0x60;
                    return;
                }
                if (G8(0x1F80)) G8(0x1F80)--;            /* in the alley: a life lost */
            }
            CALL(0x20E1);
            CALL(0x1E40);
            return;
        }
        CALL(0x2022);
        AX = 0x0104;
        AL = (uint8_t)(AL - G8(0x1CC1));
        if (G8(V_DOG_DIR) != 0xFF) AH = 0xFF;
        CALL(0x2059);
        goto draw;
    }
    if (G8(V_CAUGHT)) {                                  /* fighting with the cat */
        DL = G8(V_CAUGHT);
        if (g16(c, 0x1CB9)) {
            sg16(c, 0x1CB9, (uint16_t)(g16(c, 0x1CB9) - 1));
            CALL(0x2DFD);
            DL &= 1;
            if (!DL) DL = 0xFF;
        }
        G8(V_DOG_DIR) = DL;
        AX = g16(c, V_DOG_X);
        goto move;
    }
    if (!G8(0x1CBF)) {
        if (!G8(0x1CC0)) {                               /* not here: maybe come in */
            if (!G8(0x1D58)) {
                if (G8(0x057B) < 0xB4) return;
                if (G8(0x0558)) return;
                CALL(0x2DFD);
                BX = g16(c, 0x0008);
                if (!(DL < G8((uint16_t)(BX + 0x1CD1)))) return;
            }
            AL = 1;
            sg16(c, 0x59BA, 0);
            if (g16(c, 0x0579) < 0xA0) AL = 0xFF;
            G8(V_DOG_DIR) = AL;
            G8(0x1CC0) = 4;
        }
        if (--G8(0x1CC0)) {                              /* entering from the edge */
            CALL(0x2022);
            AL = G8(0x1CC0);
            AH = G8(V_DOG_DIR);
            CALL(0x2059);
            goto draw;
        }
        G8(0x1CBF) = 1;
    }
    /* 1F75: chase */
    G8(0x1D58) = 0;
    AX = g16(c, V_DOG_X);
    if (G8(0x057B) >= 0xB4 && !G8(0x0558)) {
        CALL(0x2DFD);
        BX = g16(c, 0x0008);
        if (DL <= G8((uint16_t)(BX + 0x1CD9)))
            G8(V_DOG_DIR) = AX > g16(c, 0x0579) ? 0xFF : 1;
    }
move:
    if (G8(V_DOG_DIR) == 0) goto store;
    if (G8(V_DOG_DIR) != 1) {
        if (AX >= 8) { AX -= 8; goto store; }
        AX = 0;
    } else {
        AX = (uint16_t)(AX + 8);
        if (AX < 0x11E) goto store;
        AX = 0x11E;
    }
    /* 1FBB: at the edge: leave when done */
    if (G8(V_CAUGHT)) { if (g16(c, 0x1CB9)) goto store; }
    else if (G8(0x057B) >= 0xB4 && !G8(0x0558)) goto store;
    G8(0x1CC1) = 4;
store:
    sg16(c, V_DOG_X, AX);
    CALL(0x2022);
    sg16(c, 0x1CC4, 0x0F04);
draw:
    CX = g16(c, V_DOG_X);
    DL = G8(V_DOG_Y);
    CALL(0x2CB0);
    sg16(c, 0x1CCD, AX);
    if (G8(0x1CC0) != 3) CALL(0x20E1);
    CALL(0x20F5);
    if (c->cf) return;
    AX = g16(c, 0x1CCD);
    sg16(c, 0x1CBD, AX);
    CALL(0x209B);
}
