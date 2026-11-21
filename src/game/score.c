// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
/* Alley Cat, hand-written: score, high score and lives.
 *
 * Scores are 7 unpacked BCD digits, most significant first: score at 1F82, high score at
 * 1F89. Lives in [1F80] ([1F81] = the value on screen). Digits are 8x8 pictures at
 * 2720 + 16 * digit. */
#include "hand.h"

#define SCORE 0x1F82
#define HISCORE 0x1F89

/* 26E8 bcd_clear: 7 zero digits at DS:DI. */
void hand_26e8(V30 *c) {
    ES = DS;
    CX = 7;
    AL = 0;
    int16_t d = c->df ? -1 : 1;
    for (; CX; CX--) { w8(c, ES, DI, AL); DI = (uint16_t)(DI + d); }
    V30_TICK(9 + 7 * 10);
}

/* 26DA score_clear / 26E1 hiscore_clear */
void hand_26da(V30 *c) { DI = SCORE; CALL(0x26E8); }
void hand_26e1(V30 *c) { DI = HISCORE; CALL(0x26E8); }

/* 2690 hiscore_update: copy the score to the high score if it is higher. */
void hand_2690(V30 *c) {
    ES = DS;
    CX = 7;
    SI = SCORE;
    int16_t d = c->df ? -1 : 1;
    do {
        AL = G8(SI); SI = (uint16_t)(SI + d);
        BX = (uint16_t)(7 - CX);
        CMP8(AL, G8((uint16_t)(BX + HISCORE)));
        V30_TICK(40);
    } while (--CX && c->zf);                             /* LOOPZ */
    if (!(!c->cf && !c->zf)) return;                     /* JA */
    SI = SCORE; DI = HISCORE; CX = 7;
    rep_movsb(c);
}

/* 2739 draw_number: the 7 digits at DS:BX on screen at DI (a gap after the third). */
void hand_2739(V30 *c) {
    AX = CGA_SEG; ES = AX;
    sg16(c, 0x1F90, DI);
    sg16(c, 0x1F93, BX);
    G8(0x1F92) = 0;
    for (;;) {
        BX = g16(c, 0x1F93);
        AL = G8(BX);
        AH = 0;
        CL = 4;
        AX = (uint16_t)(AX << 4);
        AX = (uint16_t)(AX + 0x2720);
        SI = AX;
        DI = g16(c, 0x1F90);
        CX = 0x0801;
        CALL(0x2D9D);
        sg16(c, 0x1F90, (uint16_t)(g16(c, 0x1F90) + 2));
        sg16(c, 0x1F93, (uint16_t)(g16(c, 0x1F93) + 1));
        G8(0x1F92)++;
        if (G8(0x1F92) == 7) return;
        if (G8(0x1F92) == 3) sg16(c, 0x1F90, (uint16_t)(g16(c, 0x1F90) + 2));
    }
}

/* 26F2 hiscore_draw / 26FC score_draw */
void hand_26f2(V30 *c) { BX = HISCORE; DI = 0x12CA; CALL(0x2739); }
void hand_26fc(V30 *c) { BX = SCORE; DI = 0x143C; CALL(0x2739); }

/* 2706 score_add: add AL tens to the score (digits 1F82-1F87; the last digit stays 0),
 * with BCD carries, and redraw it. */
void hand_2706(V30 *c) {
    CX = 6;
    do {
        BX = CX;
        AH = 0;
        AL = add8(c, AL, G8((uint16_t)(BX + 0x1F81)), 0);
        v30_aaa(c);
        G8((uint16_t)(BX + 0x1F81)) = AL;
        AL = AH;
        V30_TICK(50);
    } while (--CX);
    CALL(0x26FC);
}

/* 271E bcd_add: DS:DI += DS:SI, 7 digits (AX, BX, CX kept). */
void hand_271e(V30 *c) {
    uint16_t cx = CX, ax = AX, bx = BX;
    c->cf = 0;
    CX = 7;
    do {
        BX = (uint16_t)(CX - 1);
        AL = G8((uint16_t)(BX + DI));
        AL = add8(c, AL, G8((uint16_t)(BX + SI)), c->cf);
        v30_aaa(c);
        G8((uint16_t)(BX + DI)) = AL;
        V30_TICK(70);
    } while (--CX);
    BX = bx; AX = ax; CX = cx;
}

/* 26B3 lives_draw: show the number of lives if it changed. */
void hand_26b3(V30 *c) {
    AL = G8(0x1F80);
    V30_TICK(30);
    if (AL == G8(0x1F81)) return;
    G8(0x1F81) = AL;
    AH = 0;
    CL = 4;
    AX = (uint16_t)(AX << 4);
    AX = (uint16_t)(AX + 0x2720);
    SI = AX;
    AX = CGA_SEG; ES = AX;
    DI = 0x1260;
    CX = 0x0801;
    CALL(0x2D9D);
}
