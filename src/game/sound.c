// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
/* Alley Cat, hand-written: PC speaker sound effects and background sounds.
 *
 * [0000] is the sound switch (Ctrl-S). Tones go through timer channel 2 (port 42h, mode 3
 * set by B6h to port 43h) gated by port 61h bits 0-1; some noises toggle bit 1 directly. */
#include "hand.h"

#define SOUND_ON (G8(0x0000) != 0)

/* 5889 tone: start a tone with divisor AX (channel 2 mode already set). */
void hand_5889(V30 *c) {
    v30_out(c, 0x42, AL, 0);
    AL = AH;
    v30_out(c, 0x42, AL, 0);
    AL = (uint8_t)v30_in(c, 0x61, 0);
    AL = or8(c, AL, 3);
    v30_out(c, 0x61, AL, 0);
    V30_TICK(60);
}

static void tone(V30 *c, uint16_t divisor) {
    AL = 0xB6;
    v30_out(c, 0x43, AL, 0);
    AX = divisor;
    CALL(0x5889);
}

/* 5896: nothing (a sound hook left empty) */
void hand_5896(V30 *c) { V30_TICK(20); }

/* 5897 wipe_tone: next of 4 tones for the iris wipe (AX, CX, DX kept). */
void hand_5897(V30 *c) {
    V30_TICK(30);
    if (!SOUND_ON) return;
    uint16_t ax = AX, cx = CX, dx = DX;
    AL = 0xB6;
    v30_out(c, 0x43, AL, 0);
    BX = g16(c, 0x5A56) & 6;
    sg16(c, 0x5A56, (uint16_t)(g16(c, 0x5A56) + 2));
    AX = g16(c, (uint16_t)(BX + 0x5A5A));
    CALL(0x5889);
    DX = dx; CX = cx; AX = ax;
}

/* 5829 / 5835 / 5869: the bonus banner sound (two alternating tones per tick, or the next
 * tone of the list). */
void hand_5829(V30 *c) { sg16(c, 0x5A62, 0); G8(0x5A82) = 0; V30_TICK(40); }
void hand_5835(V30 *c) {
    V30_TICK(30);
    if (!SOUND_ON) return;
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, 0x5A80)) return;
    sg16(c, 0x5A80, DX);
    G8(0x5A82)++;
    AL = 0xB6;
    v30_out(c, 0x43, AL, 0);
    BX = g16(c, 0x5A62);
    if (!(G8(0x5A82) & 1)) BX += 2;
    AX = g16(c, (uint16_t)(BX + 0x5A64));
    CALL(0x5889);
}
void hand_5869(V30 *c) {
    V30_TICK(30);
    if (!SOUND_ON) return;
    uint16_t bx = BX, ax = AX;
    AL = 0xB6;
    v30_out(c, 0x43, AL, 0);
    BX = g16(c, 0x5A62);
    sg16(c, 0x5A62, (uint16_t)(g16(c, 0x5A62) + 2));
    AX = g16(c, (uint16_t)(BX + 0x5A64));
    CALL(0x5889);
    AX = ax; BX = bx;
}

/* 57D5 / 57E4: the tune of the between-scenes picture, one note per 2 ticks (a different
 * one after losing a life, [0552]). */
void hand_57d5(V30 *c) {
    sg16(c, 0x5A85, 0);
    AH = 0;
    INT(0x1A);
    sg16(c, 0x5A83, DX);
}
void hand_57e4(V30 *c) {
    V30_TICK(30);
    if (!SOUND_ON) return;
    AH = 0;
    INT(0x1A);
    AX = (uint16_t)(DX - g16(c, 0x5A83));
    if (AX < 2) return;
    sg16(c, 0x5A83, DX);
    BX = g16(c, 0x5A85);
    sg16(c, 0x5A85, (uint16_t)(g16(c, 0x5A85) + 2));
    if (G8(0x0552)) {
        AX = g16(c, (uint16_t)(BX + 0x5AA3));
        if (AX == 0) { CALL(0x5B21); return; }
    } else AX = g16(c, (uint16_t)(BX + 0x5A87));
    tone(c, AX);
}

/* 5797 / 57A6 shock_noise: the eel's shock, a pulse-width buzz on port 61h bit 1. */
void hand_5797(V30 *c) {
    CALL(0x5B21);
    G8(0x5ACF) = 0;
    sg16(c, 0x5ACD, 8);
}
void hand_57a6(V30 *c) {
    G8(0x5ACF)++;
    DL = 0;
    AL = G8(0x5ACF) & 0x3F;
    if (AL == 0) sg16(c, 0x5ACD, (uint16_t)(g16(c, 0x5ACD) + 1));
    BX = g16(c, 0x5ACD);
    CL = 2;
    BX >>= 2;
    BL &= 0x1F;
    if (AL >= BL) DL = 2;
    DL &= G8(0x0000);
    AL = (uint8_t)v30_in(c, 0x61, 0);
    AL &= 0xFD;
    AL = or8(c, AL, DL);
    v30_out(c, 0x61, AL, 0);
    V30_TICK(150);
}

/* 56F4 / 5704 bite_noise: the spider's bite, bits of a noise table on port 61h. */
void hand_56f4(V30 *c) {
    AX = 0x200;
    if (G8(0x0697) == 0xFD) AX = (uint16_t)(AX << 1);
    sg16(c, 0x5AD0, AX);
    V30_TICK(40);
}
void hand_5704(V30 *c) {
    sg16(c, 0x5AD0, (uint16_t)(g16(c, 0x5AD0) + 1));
    BX = g16(c, 0x5AD0);
    DX = BX;
    CL = 9;
    DX >>= 9;
    CL = DL & 0x0F;
    BX >>= CL;
    BX &= 0x0F;
    DL = (uint8_t)(G8((uint16_t)(BX + 0x5AD2)) & G8(0x0000));
    AL = (uint8_t)v30_in(c, 0x61, 0);
    AL &= 0xFC;
    AL |= DL;
    v30_out(c, 0x61, AL, 0);
    V30_TICK(150);
}

/* 576E sweep_step: a CPU-timed pause (LOOP $, 4096 times; half on a PCjr), then the tone
 * [5ACB]. 572E sweep: down, down again, up. */
void hand_576e(V30 *c) {
    CX = 0x1000;
    if (G8(0x0697) == 0xFD) CX >>= 1;
    V30_TICK(CX * 17 - 12);                              /* LOOP $: 17 cycles a turn, 5 the last */
    CX = 0;
    if (!SOUND_ON) return;
    tone(c, g16(c, 0x5ACB));
}
void hand_572e(V30 *c) {
    sg16(c, 0x5ACB, 0x1F4);
    do { CALL(0x576E); sg16(c, 0x5ACB, (uint16_t)(g16(c, 0x5ACB) - 0x1E)); } while (g16(c, 0x5ACB) > 0xC8);
    sg16(c, 0x5ACB, 0x1F4);
    do { CALL(0x576E); sg16(c, 0x5ACB, (uint16_t)(g16(c, 0x5ACB) - 0x14)); } while (g16(c, 0x5ACB) > 0x12C);
    do { CALL(0x576E); sg16(c, 0x5ACB, (uint16_t)(g16(c, 0x5ACB) + 0x1E)); } while (g16(c, 0x5ACB) < 0x320);
    CALL(0x5B21);
}

/* 5691 dogs_awake_noise: a short noise burst with a flash of the background. */
void hand_5691(V30 *c) {
    CALL(0x5B21);
    AH = 0x0B;
    BX = 4;
    INT(0x10);
    AH = 0;
    INT(0x1A);
    sg16(c, 0x5AE2, DX);
    sg16(c, 0x5AE4, 0);
    AL = 2;
    if (G8(0x0697) == 0xFD) AL >>= 1;
    G8(0x5B06) = AL;
    do {
        if (SOUND_ON) {
            sg16(c, 0x5AE4, (uint16_t)(g16(c, 0x5AE4) + 1));
            BX = g16(c, 0x5AE4);
            CL = G8(0x5B06);
            BX >>= CL;
            BX &= 0x1F;
            AL = (uint8_t)v30_in(c, 0x61, 0);
            AL ^= G8((uint16_t)(BX + 0x5AE6));
            v30_out(c, 0x61, AL, 0);
        }
        AH = 0;
        INT(0x1A);
        DX = (uint16_t)(DX - g16(c, 0x5AE2));
    } while (DX < 2);
    AH = 0x0B;
    BX = 0;
    INT(0x10);
    G8(0x5B07) = 0x0C;
    CALL(0x5B21);
}

/* 58BD background_reset */
void hand_58bd(V30 *c) {
    G8(0x5927) = 0x80; G8(0x5928) = 0; G8(0x5929) = 0;
    sg16(c, 0x592A, 0x500);
    G8(0x592C) = 0xFF; G8(0x592D) = 0; G8(0x5920) = 0; G8(0x5B07) = 0;
    sg16(c, 0x5B08, 0);
    sg16(c, 0x5B0C, 1);
    G8(0x5B0E) = 1;
    V30_TICK(150);
}

/* 546D background_sound: called every main-loop turn: the dog fight noise, a pending short
 * effect ([5920] ticks of [5923]), or the background music of the scene (alley, room,
 * falling object), a little random melody built from the tables at 59C2-5A54. */
void hand_546d(V30 *c) {
    V30_TICK(30);
    if (!SOUND_ON) return;
    if (G8(0x1CB8)) {                                     /* dog and cat fighting */
        AH = 0;
        INT(0x1A);
        if (!G8(0x5B0F)) {
            if (DX == g16(c, 0x5B10)) return;
            sg16(c, 0x5B10, DX);
            AL = 0xB6;
            v30_out(c, 0x43, AL, 0);
            AX = (uint16_t)((g16(c, 0x5B12) & 0x1FF) + 0xC8);
            CALL(0x5889);
            sg16(c, 0x5B12, (uint16_t)(g16(c, 0x5B12) - 0x4B));
            return;
        }
        if (DX != g16(c, 0x5B10)) { sg16(c, 0x5B10, DX); G8(0x5B0F)--; }
        if (--G8(0x5B0E)) return;
        AL = 1;
        if (G8(0x0697) != 0xFD) AL = (uint8_t)(AL << 1);
        G8(0x5B0E) = AL;
        CALL(0x2DFD);
        if (DL <= 4) sg16(c, 0x5B0C, (uint16_t)(g16(c, 0x5B0C) + 1));
        if (g16(c, 0x5B0C) & 1) sg16(c, 0x5B0A, (uint16_t)(g16(c, 0x5B0A) + 7));
        AL = 0xB6;
        v30_out(c, 0x43, AL, 0);
        CALL(0x2DFD);
        AX = DX;
        AX &= g16(c, 0x5B0A);
        AX &= 0x1FF;
        AX = (uint16_t)(AX + 0x190);
        CALL(0x5889);
        return;
    }
    AH = 0;
    INT(0x1A);
    if (G8(0x5920)) {                                     /* a short effect */
        if (DX == g16(c, 0x5921)) return;
        sg16(c, 0x5921, DX);
        if (--G8(0x5920) == 0) { CALL(0x5B21); return; }
        AL = 0xB6;
        v30_out(c, 0x43, AL, 0);
        AX = g16(c, 0x5923);
        CALL(0x5889);
        return;
    }
    if (DX == g16(c, 0x5925)) return;
    SI = 3;                                               /* 3 dog, 1 room, 0 alley, 2 object */
    AL = (uint8_t)(G8(0x1CBF) | G8(0x5B07));
    if (!AL) {
        SI = 1;
        if (g16(c, 0x0004) == 0) { SI--; if (G8(0x1673)) SI = 2; }
    }
    DI = (uint16_t)(SI << 1);
    AL = (uint8_t)(G8(0x0584) | G8(0x5B07));
    if (!AL) {
        AX = (uint16_t)(DX - g16(c, 0x5925));
        if (AX < g16(c, (uint16_t)(DI + 0x59F2))) return;
    }
    sg16(c, 0x5925, DX);
    if (G8(0x1CBF) || G8(0x5B07)) {                       /* the dog's tune */
        if (!G8(0x1CBF)) G8(0x5B07)--;
        sg16(c, 0x592E, 0x1200);
        BX = g16(c, 0x59BA);
        if (BX >= 6) { BX = 0; sg16(c, 0x59BA, BX); }
        sg16(c, 0x59BA, (uint16_t)(g16(c, 0x59BA) + 2));
        AX = g16(c, (uint16_t)(BX + 0x5A44));
        sg16(c, 0x592A, AX);
        CALL(0x5B28);
        return;
    }
    if (SI == 2) {                                        /* falling object: pitch by height */
        AL = G8(0x1673);
        AH = 0;
        CL = 4;
        AX = (uint16_t)(AX << 4);
        AX = (uint16_t)(AX + 0x200);
        sg16(c, 0x592A, AX);
        sg16(c, 0x592E, 0x1800);
        CALL(0x5B28);
        return;
    }
    AX = g16(c, (uint16_t)(DI + 0x5A02));
    sg16(c, 0x592E, AX);
    { unsigned out = G8(0x5927) & 1; G8(0x5927) >>= 1;
      if (out) {                                          /* next bar: maybe a new phrase */
          sg16(c, 0x592E, 0x1000);
          G8(0x5927) = 0x80;
          G8(0x5928)++;
          AL = (uint8_t)(G8(0x5928) & G8((uint16_t)(SI + 0x59FA)));
          if (!AL) {
              DL = G8((uint16_t)(SI + 0x5A0A));
              G8(0x5929) = (uint8_t)(G8(0x5929) + DL);
              CALL(0x2DFD);
              if (DL <= G8((uint16_t)(SI + 0x5A0C))) { DL &= 7; G8(0x592D) = DL; }
              CALL(0x2DFD);
              DX &= 0xFF;
              DX = (uint16_t)(DX << 1);
              CL = 1;
              if (DL & 2) { CL = 0xFF; DX = (uint16_t)(DX + 0x300); }
              sg16(c, 0x592A, DX);
              G8(0x592C) = CL;
          }
          AH = (uint8_t)(G8(0x5929) & G8((uint16_t)(SI + 0x59FC)));
          AL |= AH;
          G8(0x5928) = AL;
      } }
    if (G8(0x592C) != 0xFF) {                             /* melody from the scale */
        sg16(c, 0x5A54, (uint16_t)(g16(c, 0x5A54) + 2));
        BX = g16(c, 0x5A54) & 0x0E;
        AX = g16(c, (uint16_t)(BX + 0x5A44));
        sg16(c, 0x592A, AX);
    } else {                                              /* falling glide */
        if (g16(c, 0x592A) <= 0xC8) sg16(c, 0x592A, 0x500);
        sg16(c, 0x592A, (uint16_t)(g16(c, 0x592A) - 0x19));
    }
    if (G8(0x0584)) {
        sg16(c, 0x592E, 0x2000);
        G8(0x592C) = 0xFF;
        CALL(0x5B28);
        return;
    }
    BL = G8(0x5928);
    BH = 0;
    BX = (uint16_t)(BX + g16(c, (uint16_t)(DI + 0x59FE)));
    AL = (uint8_t)(G8((uint16_t)(BX + 0x59C2)) & G8(0x5927));
    if (!AL) {                                            /* rest, unless notes are pending */
        if (!G8(0x592D)) return;
        G8(0x592D)--;
        AX = g16(c, (uint16_t)(DI + 0x5A06));
        sg16(c, 0x592E, AX);
    }
    CALL(0x5B28);
}
