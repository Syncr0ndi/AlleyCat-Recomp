/* Alley Cat, hand-written: winning room 7 (the sweetheart), hearts, level-up tunes, and the
 * graffiti on the fence. Tunes are lists of PIT divisors played one per BIOS tick. */
#include "hand.h"

static void speaker_on(V30 *c, uint16_t divisor) {
    AL = 0xB6;
    v30_out(c, 0x43, AL, 0);
    AX = divisor;
    v30_out(c, 0x42, AL, 0);
    AL = AH;
    v30_out(c, 0x42, AL, 0);
    AL = (uint8_t)v30_in(c, 0x61, 0);
    AL |= 3;
    v30_out(c, 0x61, AL, 0);
}

/* 5450 sound_reset (state of the 5B.. effects) */
void hand_5450(V30 *c) {
    G8(0x5B0F) = 0x0C;
    sg16(c, 0x5B0C, 1);
    sg16(c, 0x5B12, 0x1FF);
    sg16(c, 0x5B0A, 0x0F);
    G8(0x5B0E) = 1;
    V30_TICK(80);
}

/* 52D0 tune_step: next note of the level-up tune (list at 52CA), once per tick. */
void hand_52d0(V30 *c) {
    V30_TICK(30);
    if (!G8(0x0000)) return;
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, 0x52C4)) return;
    sg16(c, 0x52C4, DX);
    BX = g16(c, 0x52C6);
    sg16(c, 0x52C6, (uint16_t)(g16(c, 0x52C6) + 2));
    AX = g16(c, (uint16_t)(BX + 0x52CA));
    if (AX == g16(c, 0x52C8)) { CALL(0x5B21); return; }
    sg16(c, 0x52C8, AX);
    speaker_on(c, g16(c, 0x52C8));
}

/* 53B0 tune2_step: next note of the title tune (note numbers at 538C, 66h ends). */
void hand_53b0(V30 *c) {
    V30_TICK(30);
    if (!G8(0x0000)) return;
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, 0x5322)) return;
    sg16(c, 0x5322, DX);
    BX = g16(c, 0x5320);
    BL = G8((uint16_t)(BX + 0x538C));
    if (BL == 0x66) { CALL(0x5B21); return; }
    BH = 0;
    sg16(c, 0x5320, (uint16_t)(g16(c, 0x5320) + 1));
    if (BX == 0) { CALL(0x5B21); return; }
    speaker_on(c, g16(c, (uint16_t)(BX + 0x5324)));
}

/* 5368 dancers: draw this level's dancing cats (frame [5016]) with the tune going. */
void hand_5368(V30 *c) {
    AX = CGA_SEG; ES = AX;
    BX = (uint16_t)(g16(c, 0x0008) << 1);
    AX = g16(c, (uint16_t)(BX + 0x52AE));
    sg16(c, 0x5010, AX);
    for (;;) {
        BX = g16(c, 0x5010);
        DI = g16(c, BX);
        if (DI == 0) return;
        BX = g16(c, (uint16_t)(BX + 2));
        BX ^= g16(c, 0x5016);
        BX &= 2;
        SI = g16(c, (uint16_t)(BX + 0x5012));
        CX = 0x2304;
        CALL(0x2D9D);
        CALL(0x52D0);
        sg16(c, 0x5010, (uint16_t)(g16(c, 0x5010) + 4));
    }
}

/* 5313 level_up: from level 2 on, dancing cats for 40 ticks with the tune. */
void hand_5313(V30 *c) {
    V30_TICK(30);
    if (g16(c, 0x0008) < 2) return;
    sg16(c, 0x5016, 0);
    AH = 0;
    INT(0x1A);
    sg16(c, 0x52C0, DX); sg16(c, 0x52C2, DX); sg16(c, 0x52C4, DX);
    sg16(c, 0x52C6, 0); sg16(c, 0x52C8, 0);
    for (;;) {
        CALL(0x5368);
        sg16(c, 0x5016, g16(c, 0x5016) ^ 2);
        for (;;) {
            CALL(0x52D0);
            AH = 0;
            INT(0x1A);
            AX = (uint16_t)(DX - g16(c, 0x52C0));
            if (AX >= 5) break;
        }
        sg16(c, 0x52C0, DX);
        DX = (uint16_t)(DX - g16(c, 0x52C2));
        if (DX >= 0x28) break;
    }
    CALL(0x5B21);
}

/* 522A heart_move: move heart BX by its direction ([4D6F+BX], [4D7F+BX]) within the
 * limits; [4D6E] counts hearts that reached an edge. */
void hand_522a(V30 *c) {
    AX = g16(c, (uint16_t)(BX + 0x4D4A));
    uint16_t d = g16(c, (uint16_t)(BX + 0x4D6F));
    if (d >= 1) {
        if (d == 1) {
            AX = (uint16_t)(AX + g16(c, 0x4D6A));
            if (AX > g16(c, 0x4DD0)) { AX = g16(c, 0x4DD0); G8(0x4D6E)++; }
        } else {
            if (AX < g16(c, 0x4D6A)) { AX = 0; G8(0x4D6E)++; } else AX = (uint16_t)(AX - g16(c, 0x4D6A));
        }
        sg16(c, (uint16_t)(BX + 0x4D4A), AX);
    }
    AX = g16(c, (uint16_t)(BX + 0x4D5A));
    d = g16(c, (uint16_t)(BX + 0x4D7F));
    if (d >= 1) {
        if (d == 1) {
            AX = (uint16_t)(AX + g16(c, 0x4D6C));
            if (AX > g16(c, 0x4DD2)) { AX = g16(c, 0x4DD2); G8(0x4D6E)++; }
        } else {
            if (AX < g16(c, 0x4D6C)) { AX = 0; G8(0x4D6E)++; } else AX = (uint16_t)(AX - g16(c, 0x4D6C));
        }
        sg16(c, (uint16_t)(BX + 0x4D5A), AX);
    }
    V30_TICK(150);
}

/* 519B hearts_burst: 8 hearts fly out from one point until one reaches an edge. */
void hand_519b(V30 *c) {
    CX = 8;
    G8(0x4D91) = 1;
    do {
        uint16_t cx = CX;
        CALL(0x5B63);
        CX = cx;
        BX = (uint16_t)((CX - 1) << 1);
        sg16(c, (uint16_t)(BX + 0x4D4A), g16(c, 0x4DCC));
        sg16(c, (uint16_t)(BX + 0x4D5A), g16(c, 0x4DCE));
    } while (--CX);
    AX = CGA_SEG; ES = AX;
    G8(0x4D6E) = 0;
    do {
        AH = 0;
        INT(0x1A);
        sg16(c, 0x4A80, DX);
        CX = 8;
        do {
            uint16_t cx = CX;
            CALL(0x5B63);
            CX = cx;
            BX = (uint16_t)((CX - 1) << 1);
            uint16_t bx = BX;
            if (!G8(0x4D91) || CX == 8) {
                CX = g16(c, (uint16_t)(BX + 0x4D4A));
                DX = g16(c, (uint16_t)(BX + 0x4D5A));
                CALL(0x2CB0);
                DI = AX;
                SI = g16(c, 0x4DCA);
                BP = 0x0E;
                CX = g16(c, 0x4DD4);
                CALL(0x2CCC);
            }
            BX = bx; CX = cx;
            CALL(0x522A);
        } while (--CX);
        do {
            CALL(0x5B63);
            AH = 0;
            INT(0x1A);
            DX = (uint16_t)(DX - g16(c, 0x4A80));
        } while (DX < g16(c, 0x4DC8));
        G8(0x4D91) = 0;
    } while (!G8(0x4D6E));
}

/* 514A hearts: three bursts (parameter tables at 4D92-4DC2). */
void hand_514a(V30 *c) {
    CX = 3;
    do {
        BX = (uint16_t)((3 - CX) << 1);
        sg16(c, 0x4D6A, g16(c, (uint16_t)(BX + 0x4DA4)));
        sg16(c, 0x4D6C, g16(c, (uint16_t)(BX + 0x4DAA)));
        sg16(c, 0x4DCC, g16(c, (uint16_t)(BX + 0x4D98)));
        sg16(c, 0x4DCE, g16(c, (uint16_t)(BX + 0x4D9E)));
        sg16(c, 0x4DD0, g16(c, (uint16_t)(BX + 0x4DB0)));
        sg16(c, 0x4DD2, g16(c, (uint16_t)(BX + 0x4DB6)));
        sg16(c, 0x4DD4, g16(c, (uint16_t)(BX + 0x4DBC)));
        sg16(c, 0x4DCA, g16(c, (uint16_t)(BX + 0x4D92)));
        AX = g16(c, (uint16_t)(BX + 0x4DC2));
        sg16(c, 0x4DC8, AX);
        uint16_t cx = CX;
        CALL(0x519B);
        CX = cx;
    } while (--CX);
}

/* 5060 climb_down: the cat climbs down the wall to its sweetheart (bonus after the first
 * step), drawn with the wall piece behind it. */
void hand_5060(V30 *c) {
    AX = CGA_SEG; ES = AX;
    AX = g16(c, 0x0579);
    if (AX >= 0x117) AX = 0x116;
    AX = AX < 0x10 ? 0 : (uint16_t)(AX - 0x10);
    AX &= 0x0FF0;
    sg16(c, 0x0579, AX);
    G8(0x057B) = 0x14;
    { uint16_t a = AX; AX = (uint16_t)(AX - 0x80); if (a < 0x80) AX = (uint16_t)~AX; }
    CL = 3;
    AX >>= 3;
    if (AX > 0x0D) AX = 0x0D;
    AX = (uint16_t)(AX + 2);
    sg16(c, 0x4D6A, AX);                                  /* sideways step towards the middle */
    sg16(c, 0x4DD6, 0x0A);
    for (;;) {
        if (g16(c, 0x4DD6) != 0x0A) CALL(0x5B63);
        AH = 0;
        INT(0x1A);
        sg16(c, 0x4A80, DX);
        AX = g16(c, 0x0579);
        CX = AX & 0x0FF0;
        if (CX == 0x80) AX = CX;
        else if (CX < 0x80) AX = (uint16_t)(AX + g16(c, 0x4D6A));
        else AX = (uint16_t)(AX - g16(c, 0x4D6A));
        sg16(c, 0x0579, AX);
        if (G8(0x057B) >= 0x54) return;
        G8(0x057B) = (uint8_t)(G8(0x057B) + 8);
        CX = (uint16_t)(g16(c, 0x0579) + 4);
        DL = G8(0x057B);
        CALL(0x2CB0);
        DI = AX;
        sg16(c, 0x4DD8, DI);
        SI = 0x4B8A; BP = 0x0E; CX = 0x2007;
        CALL(0x2CCC);
        DI = (uint16_t)(g16(c, 0x4DD8) + 0xF3);
        SI = 0x4A82; CX = 0x0D04;
        CALL(0x2D9D);
        if (g16(c, 0x4DD6) == 0x0A) { CALL(0x572E); CALL(0x5B54); }
        do {
            CALL(0x5B63);
            AH = 0;
            INT(0x1A);
            DX = (uint16_t)(DX - g16(c, 0x4A80));
        } while (DX < g16(c, 0x4DD6));
        if (g16(c, 0x4DD6) == 0x0A) {
            CALL(0x38B0);
            BX = 0;
            AH = 0x0B;
            INT(0x10);
        }
        sg16(c, 0x4DD6, 2);
    }
}

/* 528B room7_won: palette, climb down, hearts, a life and a level more. */
void hand_528b(V30 *c) {
    CALL(0x5B54);
    if (G8(0x0697) != 0xFD) { AH = 0x0B; BX = 0x0101; INT(0x10); }
    CALL(0x5060);
    CALL(0x514A);
    CALL(0x5BBF);
    if (G8(0x1F80) < 9) G8(0x1F80)++;
    if (g16(c, 0x0008) < 7) sg16(c, 0x0008, (uint16_t)(g16(c, 0x0008) + 1));
    sg16(c, 0x0414, 0);
    AH = 0;
    INT(0x1A);
    sg16(c, 0x0412, DX);
    CALL(0x5B21);
}

/* 5400 graffiti: random scribbles of this level on the fence. */
void hand_5400(V30 *c) {
    AX = CGA_SEG; ES = AX;
    BX = g16(c, 0x0008) & 7;
    BX = (uint16_t)(BX << 1);
    AX = BX;
    BX = g16(c, (uint16_t)(BX + 0x5908));
    CL = 3;
    AX = (uint16_t)(AX << 3);
    sg16(c, 0x5918, AX);
    for (;;) {
        DI = g16(c, BX);
        if (DI == 0xFFFF) return;
        CALL(0x2DFD);
        DX &= 0x0E;
        DX = (uint16_t)(DX + g16(c, 0x5918));
        SI = DX;
        SI = g16(c, (uint16_t)(SI + 0x5888));
        CX = g16(c, (uint16_t)(SI + 0x5858));
        SI = g16(c, (uint16_t)(SI + 0x584C));
        uint16_t bx = BX;
        CALL(0x2D9D);
        BX = bx;
        BX += 2;
    }
}
