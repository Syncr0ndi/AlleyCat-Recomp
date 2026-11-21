/* Alley Cat, hand-written: the bonus after a room, and the fishbowl in room 1.
 *
 * Bonus: points for the time left (binary [3697] -> BCD at 368D, added to the score
 * 1F82), shown in BIOS text over a banner; in room 7 times a multiplier. */
#include "hand.h"

/* 3850 room1_bowl: animate the fishbowl on the table (4 frames, every 6 ticks); jumping
 * into it sets [0554] (-> the fishbowl state). */
void hand_3850(V30 *c) {
    AH = 0;
    INT(0x1A);
    AX = (uint16_t)(DX - g16(c, 0x35DA));
    if (AX < 6) return;
    sg16(c, 0x35DA, DX);
    sg16(c, 0x35D8, (uint16_t)(g16(c, 0x35D8) + 2));
    BX = g16(c, 0x35D8) & 6;
    SI = g16(c, (uint16_t)(BX + 0x35D0));
    DI = 0x15C9;
    AX = CGA_SEG; ES = AX;
    CX = 0x0A02;
    CALL(0x2D9D);
    AX = 0xE4; DL = 0x8A; SI = 0x10;
    BX = g16(c, 0x0579);
    DH = G8(0x057B);
    DI = 0x18; CX = 0x0E0A;
    CALL(0x2E29);
    if (c->cf) G8(0x0554) = 1;
}

/* 3A96 banner_mask: banner picture 35E0 AND DX -> 000E (30 words). */
void hand_3a96(V30 *c) {
    c->df = 0;
    AX = GAME_DS; ES = AX;
    DI = 0x0E; SI = 0x35E0; CX = 0x1E;
    do {
        AX = (uint16_t)(m16(c, DS, SI) & DX); SI += 2;
        w16(c, ES, DI, AX); DI += 2;
        V30_TICK(40);
    } while (--CX);
}

/* 3AAC banner_scroll: draw the banner list (361C) from 1B80 upwards in steps of 280h down to
 * AX, with a sound and 2 ticks per step when [369F] is set. */
void hand_3aac(V30 *c) {
    sg16(c, 0x369A, AX);
    AX = CGA_SEG; ES = AX;
    CALL(0x5829);
    AX = 0x1B80;
    for (;;) {
        BX = 0x361C;
        sg16(c, 0x369C, AX);
        CALL(0x2B24);
        if (G8(0x369F)) {
            CALL(0x5869);
            AH = 0;
            INT(0x1A);
            sg16(c, 0x3695, DX);
            do { AH = 0; INT(0x1A); DX = (uint16_t)(DX - g16(c, 0x3695)); } while (DX < 2);
        }
        AX = g16(c, 0x369C);
        if (AX < 0x280) break;
        AX = (uint16_t)(AX - 0x280);
        if (AX < g16(c, 0x369A)) break;
    }
    CALL(0x5B21);
}

/* 3AF4 to_bcd: [368B] = AX in binary -> BCD at 368D (sum of the digit tables at 3684-7n). */
void hand_3af4(V30 *c) {
    sg16(c, 0x368B, AX);
    AX = 0;
    sg16(c, 0x368D, AX); sg16(c, 0x368F, AX); sg16(c, 0x3691, AX); sg16(c, 0x3693, AX);
    BX = 0x3684;
    DX = 0x1000;
    for (;;) {
        if (g16(c, 0x368B) & DX) { SI = BX; DI = 0x368D; CALL(0x271E); }
        BX = (uint16_t)(BX - 7);
        unsigned out = DX & 1;
        DX >>= 1;
        if (out) break;
    }
}

/* 39FA bonus_save: keep the screen under the two text areas (room 7). */
void hand_39fa(V30 *c) {
    ES = DS;
    AX = CGA_SEG; DS = AX;
    CX = 0x0804; DI = 0x0E; SI = 0x08E4;
    CALL(0x2DCA);
    CX = 0x0814; DI = 0x4E; SI = 0x0C94;
    CALL(0x2DCA);
    DS = ES;
}

/* 3A1C bonus_flash: wait for the retrace, flash the background with [3699] (DX kept). */
void hand_3a1c(V30 *c) {
    AH = 0;
    INT(0x1A);
    uint16_t dx = DX;
    do CALL(0x13D8); while (c->zf);
    DX = dx;
    BX = 0;
    if (!(DX & 4)) BL = G8(0x3699);
    AH = 0x0B;
    INT(0x10);
    DX = dx;
}

/* 3A3A bonus_digits: print digits 3-6 of the bonus at text row [369E]/8, column 18. */
void hand_3a3a(V30 *c) {
    AH = 2;
    DH = G8(0x369E);
    CL = 3;
    DH = (uint8_t)(DH >> 3);
    DL = 0x12;
    BH = 0;
    INT(0x10);
    sg16(c, 0x36A0, 3);
    do {
        BX = g16(c, 0x36A0);
        AL = (uint8_t)(G8((uint16_t)(BX + 0x368D)) + 0x30);
        AH = 0x0E;
        BL = 3;
        INT(0x10);
        sg16(c, 0x36A0, (uint16_t)(g16(c, 0x36A0) + 1));
    } while (g16(c, 0x36A0) < 7);
}

/* 3A6C bonus_multiplier_text: "BONUS MULTIPLIER: nn" at row 10, column 10. */
void hand_3a6c(V30 *c) {
    AH = 2;
    DL = 0x0A;
    DH = DL;
    BX = 0;
    INT(0x10);
    BX = g16(c, 0x370C);
    AX = g16(c, (uint16_t)(BX + 0x36EC));
    sg16(c, 0x3720, AX);                                  /* the two digits in the text */
    BX = 0;
    do {
        AH = 0x0E;
        AL = G8((uint16_t)(BX + 0x370E));
        uint16_t bx = BX;
        BL = 3;
        INT(0x10);
        BX = bx;
        BX++;
    } while (BX < 0x14);
}

/* 38B0 bonus: compute and show the time bonus, add it to the score (room 7: multiplied,
 * more for a full wall), with sound and flashing. */
void hand_38b0(V30 *c) {
    if (g16(c, 0x0006) != 7) {
        sg16(c, 0x0414, (uint16_t)(g16(c, 0x0414) + 1));
        G8(0x0418) = 1;
        DX = 0xAAAA;
        CALL(0x3A96);
        AX = 0;
        G8(0x369F) = 0;
        CALL(0x3AAC);
    }
    AH = 0;
    INT(0x1A);
    if (g16(c, 0x0006) == 7) {
        DX = (uint16_t)(DX - g16(c, 0x0412));
        AX = DX > 0x2A30 ? 0 : (uint16_t)(0x2A30 - DX);
        AX >>= 1;
    } else {
        DX = (uint16_t)(DX - g16(c, 0x0410));
        AX = 0x546;
        if (g16(c, 0x0006) == 6) AX = (uint16_t)(AX << 1);
        AX = DX > AX ? 0 : (uint16_t)(AX - DX);
        if (g16(c, 0x0006) != 6) AX = (uint16_t)(AX << 1);
    }
    sg16(c, 0x3697, AX);
    CALL(0x3AF4);
    BX = g16(c, 0x0006);
    BL = (uint8_t)(BL << 1);
    SI = g16(c, (uint16_t)(BX + 0x36CC));
    DI = 0x368D;
    CALL(0x271E);                                        /* + the room's base bonus */
    if (g16(c, 0x0006) == 7) {
        BX = g16(c, 0x0008);
        BL = (uint8_t)(BL << 1);
        AX = BX;
        CX = g16(c, (uint16_t)(BX + 0x36DC));
        if (g16(c, 0x2E8D) < 8) { CX = (uint16_t)(CX << 1); AX = (uint16_t)(AX + 0x10); }
        sg16(c, 0x370C, AX);
        do {
            SI = 0x368D; DI = 0x1F82;
            uint16_t cx = CX;
            CALL(0x271E);
            CX = cx;
        } while (--CX);
        CALL(0x39FA);
        G8(0x369E) = 0x38;
        G8(0x3699) = 1;
        sg16(c, 0x3722, 0x44);
        CALL(0x3A3A);
        CALL(0x3A6C);
    } else {
        SI = 0x368D; DI = 0x1F82;
        CALL(0x271E);
        G8(0x3699) = 2;
        sg16(c, 0x3722, 0x1E);
        DX = 0xFFFF;
        CALL(0x3A96);
        AX = (uint16_t)(0x0A8C - g16(c, 0x3697));
        CL = 4;
        AX >>= 4;
        AL &= 0xF0;
        G8(0x369E) = AL;
        AH = 0x28;
        AX = (uint16_t)(AL * AH);
        G8(0x369F) = 1;
        CALL(0x3AAC);
        CALL(0x3A3A);
    }
    AH = 0;
    INT(0x1A);
    sg16(c, 0x3695, DX);
    do {
        if (g16(c, 0x0006) == 7) CALL(0x5B63); else CALL(0x5835);
        CALL(0x3A1C);
        DX = (uint16_t)(DX - g16(c, 0x3695));
    } while (DX < g16(c, 0x3722));
    BX = 0;
    AH = 0x0B;
    INT(0x10);
    if (g16(c, 0x0006) != 7) { CALL(0x5B21); return; }
    AX = CGA_SEG; ES = AX;
    SI = 0x0E; DI = 0x08E4; CX = 0x0804;
    CALL(0x2D9D);
    SI = 0x4E; DI = 0x0C94; CX = 0x0814;
    CALL(0x2D9D);
}
