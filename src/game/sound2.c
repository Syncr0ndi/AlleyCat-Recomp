// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
/* Alley Cat, hand-written: more sound effects, the level-up picture, the display check. */
#include "hand.h"

#define SOUND_ON (G8(0x0000) != 0)

static void tone_start(V30 *c, uint16_t divisor) {
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

/* 5B21 speaker_off */
void hand_5b21(V30 *c) {
    AL = (uint8_t)v30_in(c, 0x61, 0);
    AL = and8(c, AL, 0xFC);
    v30_out(c, 0x61, AL, 0);
    V30_TICK(40);
}

/* 5B28 note: play [592A] for [592E] timer counts (busy wait on the PIT), then silence. */
void hand_5b28(V30 *c) {
    tone_start(c, g16(c, 0x592A));
    CALL(0x13B7);
    CX = AX;
    do {
        CALL(0x13B7);
        DX = (uint16_t)(CX - AX);
    } while (DX < g16(c, 0x592E));
    AL = (uint8_t)v30_in(c, 0x61, 0);
    AL &= 0xFC;
    v30_out(c, 0x61, AL, 0);
}

/* 59A3 beep: tone BX for a CPU-timed CX loop turns (halved on a PCjr). */
void hand_59a3(V30 *c) {
    V30_TICK(30);
    if (!SOUND_ON) return;
    tone_start(c, BX);
    if (G8(0x0697) == 0xFD) CX >>= 1;
    V30_TICK(CX * 17 - 12);                               /* LOOP $ */
    CX = 0;
    CALL(0x5B21);
}

/* 58F8 / 590E / 591F: short beeps (landing on a line item, on a sill, the fence call) */
void hand_58f8(V30 *c) {
    if (!G8(0x1CBF)) { BX = 0x390; CX = 0x1800; CALL(0x59A3); }
    G8(0x127C) = 0;
}
void hand_590e(V30 *c) {
    if (!G8(0x1CBF)) { BX = 0x400; CX = 0x1800; CALL(0x59A3); }
}
void hand_591f(V30 *c) {
    BX = 0x7D0; CX = 0x1800; CALL(0x59A3);
    BX = 0xA6E; CX = 0x1800; CALL(0x59A3);
    BX = 0xDEC; CX = 0x1800; CALL(0x59A3);
}

/* 593B effect: tone AX now, then BX for the next 2 ticks (played by 546D). */
void hand_593b(V30 *c) {
    CMP8(G8(0x0000), 0);
    if (c->zf) return;
    sg16(c, 0x5923, BX);
    uint16_t ax = AX;
    AL = 0xB6;
    v30_out(c, 0x43, AL, 0);
    AX = ax;
    CALL(0x5889);
    G8(0x5920) = 2;
    AH = 0;
    INT(0x1A);
    sg16(c, 0x5921, DX);
    c->cf = 0;
}

/* 595D chirp: a random chirp (bird), unless an effect is playing. */
void hand_595d(V30 *c) {
    V30_TICK(30);
    if (!SOUND_ON || G8(0x5920)) return;
    CALL(0x2DFD);
    AX = DX;
    AX &= 0x7F;
    AX = (uint16_t)(AX + 0xAA);
    BX = AX;
    AX = (uint16_t)(AX + 0x1E);
    CALL(0x593B);
}

/* 597F bubble: rising bubble sound (fishbowl, out of air). */
void hand_597f(V30 *c) {
    V30_TICK(30);
    if (!SOUND_ON) return;
    AX = (uint16_t)(0x1200 + g16(c, 0x5B08));
    BX = (uint16_t)(0x1312 + g16(c, 0x5B08));
    sg16(c, 0x5B08, (uint16_t)(g16(c, 0x5B08) + 0x15E));
    CALL(0x593B);
    G8(0x5B07) = 0x18;
}

/* 59CB fall_noise: the cat falling into a hole: clicks getting slower until 7 ticks. */
void hand_59cb(V30 *c) {
    AL = (uint8_t)(v30_in(c, 0x61, 0) & 0xFE);
    v30_out(c, 0x61, AL, 0);
    AH = 0;
    INT(0x1A);
    sg16(c, 0x5A40, DX);
    sg16(c, 0x5A42, 0);
    for (;;) {
        AX = g16(c, 0x5A42);
        CL = 6;
        AX >>= 6;
        if (!AX) AX++;
        CX = AX;
        for (;;) {
            uint16_t cx = CX;
            AH = 0;
            INT(0x1A);
            CX = cx;
            DX = (uint16_t)(DX - g16(c, 0x5A40));
            if (DX < 2) continue;
            if (DX >= 7) { CALL(0x5B21); return; }
            if (--CX == 0) break;
        }
        CALL(0x2DFD);
        DL &= 2;
        DL &= G8(0x0000);
        AL = (uint8_t)(v30_in(c, 0x61, 0) ^ DL);
        v30_out(c, 0x61, AL, 0);
        sg16(c, 0x5A42, (uint16_t)(g16(c, 0x5A42) + 7));
    }
}

/* 5A1C splash_step: a warbling splash tone, changed when the PIT has moved on enough. */
void hand_5a1c(V30 *c) {
    V30_TICK(30);
    if (!SOUND_ON) return;
    CALL(0x13B7);
    BX = (uint16_t)(g16(c, 0x5A16) - AX);
    if (!(g16(c, 0x5A16) < AX) && !(BX > 0x260)) return;
    sg16(c, 0x5A16, AX);
    AL = 0xB6;
    v30_out(c, 0x43, AL, 0);
    sg16(c, 0x5A18, (uint16_t)(g16(c, 0x5A18) + 1));
    BX = g16(c, 0x5A18) & 0x1E;
    AX = g16(c, 0x5A3C) & 0x3FF;
    if (AX >= 0x180) { CX = (uint16_t)(0x180 - AX); uint16_t t = AX; AX = CX; CX = t; }
    AX >>= 2;
    AX = (uint16_t)(AX + g16(c, (uint16_t)(BX + 0x5A1A)));
    BX = 1;
    if (G8(0x0697) == 0xFD) BL = (uint8_t)(BL << 1);
    sg16(c, 0x5A3E, (uint16_t)(g16(c, 0x5A3E) + BX));
    BX = (uint16_t)(BX << 2);
    sg16(c, 0x5A3C, (uint16_t)(g16(c, 0x5A3C) + BX));
    DX = g16(c, 0x5A3E);
    CL = 3;
    DX >>= 3;
    AX = (uint16_t)(AX + DX);
    v30_out(c, 0x42, AL, 0);
    AL = AH;
    v30_out(c, 0x42, AL, 0);
    AL = (uint8_t)(v30_in(c, 0x61, 0) | 3);
    v30_out(c, 0x61, AL, 0);
}

/* 5A90 squeak: a random squeak once per tick (mouse bite). */
void hand_5a90(V30 *c) {
    V30_TICK(30);
    if (!SOUND_ON) return;
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, 0x5A14)) return;
    sg16(c, 0x5A14, DX);
    AL = 0xB6;
    v30_out(c, 0x43, AL, 0);
    CALL(0x2DFD);
    AX = DX;
    AX &= 0x70;
    AX = (uint16_t)(AX + 0x200);
    v30_out(c, 0x42, AL, 0);
    AL = AH;
    v30_out(c, 0x42, AL, 0);
    AL = (uint8_t)(v30_in(c, 0x61, 0) | 3);
    v30_out(c, 0x61, AL, 0);
}

/* 5AC2 jump_sound: random falling chirps for 2 ticks (the cat landing after a jump). */
void hand_5ac2(V30 *c) {
    sg16(c, 0x5A12, 0x338);
    AH = 0;
    INT(0x1A);
    sg16(c, 0x5A10, DX);
    CALL(0x13B7);
    sg16(c, 0x5A0E, AX);
    do {
        CALL(0x13B7);
        DX = AX;
        AX = (uint16_t)(AX - g16(c, 0x5A0E));
        if (AX >= 0x9C40) {
            sg16(c, 0x5A0E, DX);
            if (SOUND_ON) {
                AL = 0xB6;
                v30_out(c, 0x43, AL, 0);
                CALL(0x2DFD);
                AX = DX;
                AX &= 0x7FF;
                AX = (uint16_t)(AX + g16(c, 0x5A12));
                sg16(c, 0x5A12, (uint16_t)(g16(c, 0x5A12) - 2));
                v30_out(c, 0x42, AL, 0);
                AL = AH;
                v30_out(c, 0x42, AL, 0);
                AL = (uint8_t)(v30_in(c, 0x61, 0) | 3);
                v30_out(c, 0x61, AL, 0);
            }
        }
        AH = 0;
        INT(0x1A);
        DX = (uint16_t)(DX - g16(c, 0x5A10));
    } while (DX < 2);
    CALL(0x5B21);
}

/* 5B54 / 5B63 / 5BBF: the love tune (room 7), one note per 2 ticks; 5BBF plays it to the end. */
void hand_5b54(V30 *c) {
    sg16(c, 0x59BE, 0);
    AH = 0;
    INT(0x1A);
    sg16(c, 0x59C0, DX);
}
void hand_5b63(V30 *c) {
    V30_TICK(30);
    if (!SOUND_ON) return;
    AH = 0;
    INT(0x1A);
    AX = (uint16_t)(DX - g16(c, 0x59C0));
    if (AX < 2) return;
    sg16(c, 0x59C0, DX);
    BX = g16(c, 0x59BE) & 0xFE;
    if (BX >= 0x86) { BX = 0; sg16(c, 0x59BE, BX); }
    sg16(c, 0x59BE, (uint16_t)(g16(c, 0x59BE) + 2));
    AX = g16(c, (uint16_t)(BX + 0x5934));
    CX = g16(c, 0x59BC);
    sg16(c, 0x59BC, AX);
    if (AX == CX) { CALL(0x5B21); return; }
    CX = AX;
    AL = 0xB6;
    v30_out(c, 0x43, AL, 0);
    AX = CX;
    v30_out(c, 0x42, AL, 0);
    AL = AH;
    v30_out(c, 0x42, AL, 0);
    AL = (uint8_t)(v30_in(c, 0x61, 0) | 3);
    v30_out(c, 0x61, AL, 0);
}
void hand_5bbf(V30 *c) {
    while (SOUND_ON) {
        CALL(0x5B63);
        if (g16(c, 0x59BE) >= 0x7C) return;
    }
}

/* 5BE0 level_picture: the "next level" animation (two frames, an extra piece, the cats),
 * 4 ticks per step with the picture tune. */
void hand_5be0(V30 *c) {
    AH = 0;
    INT(0x1A);
    sg16(c, 0x5F66, DX);
    sg16(c, 0x5F60, 0);
    do {
        AX = CGA_SEG; ES = AX;
        BX = g16(c, 0x5F60);
        sg16(c, 0x5F60, (uint16_t)(g16(c, 0x5F60) + 2));
        BX &= 2;
        SI = g16(c, (uint16_t)(BX + 0x5F62));
        DI = 0x0A74;
        CX = 0x4404;
        CALL(0x2D9D);
        do {
            CALL(0x57E4);
            AH = 0;
            INT(0x1A);
            AX = (uint16_t)(DX - g16(c, 0x5F66));
        } while (AX < 4);
        sg16(c, 0x5F66, DX);
        if (g16(c, 0x5F60) == 4) { SI = 0x5F68; DI = 0x0668; CX = 0x1004; CALL(0x2D9D); }
        BX = g16(c, 0x5F60);
        if (BX >= 8 && (uint16_t)(BX - 8) < 6) {
            BX = (uint16_t)(BX - 8);
            SI = 0x5FE8;
            DI = g16(c, (uint16_t)(BX + 0x60E4));
            CX = 0x1506;
            CALL(0x2D9D);
        } else BX = (uint16_t)(BX - 8);
    } while (g16(c, 0x5F60) < 0x10);
    CALL(0x5B21);
}

/* 5C9E message: print the text at game DS:SI. */
void hand_5c9e(V30 *c) {
    AX = GAME_DS; DS = AX;
    CALL(0x5E2B);
}

/* 5C60 display_check (runs with DS = PSP): on a monochrome card, if CGA memory works say so
 * and switch the BIOS to colour; if not, say a colour card is needed and stop (JMP $). */
void hand_5c60(V30 *c) {
    INT(0x11);
    AL &= 0x30;
    CMP8(AL, 0x30);
    if (!c->zf) return;
    AX = CGA_SEG; DS = AX;
    AX = 0x55AA;
    w16(c, DS, 0, AX);
    AX = m16(c, DS, 0);
    if (AX != 0x55AA) {
        SI = 0x6112;
        CALL(0x5C9E);
        for (;;) {                                        /* JMP $: the machine keeps running */
            V30_TICK(15);
            if (c->cycles <= 0) v30_sync_point(c);
        }
    }
    SI = 0x60F0;
    CALL(0x5C9E);
    AX = 0x40; DS = AX;
    AX = m16(c, DS, 0x10);
    AL &= 0xCF;
    AL |= 0x10;
    w16(c, DS, 0x10, AX);
    AX = 4;
    INT(0x10);
}
