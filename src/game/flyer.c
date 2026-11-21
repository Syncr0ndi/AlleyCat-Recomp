// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
/* Alley Cat, hand-written: room 7's flying thing, and the sweetheart picture.
 *
 * Flyer: active [70F2], x [70F3], y [70F5] (it falls 2 per step), direction [70F6], wing
 * frame [70F0], screen [70FA] / saved at 70CC ([70F8], [70F7] = nothing saved). It flips the
 * brick it flies over and knocks the cat down. */
#include "hand.h"

/* 6100 flyer_reset */
void hand_6100(V30 *c) { G8(0x70F2) = 0; V30_TICK(20); }

/* 622B flyer_erase */
void hand_622b(V30 *c) {
    CMP8(G8(0x70F7), 0);
    V30_TICK(30);
    if (!c->zf) return;
    AX = CGA_SEG; ES = AX;
    SI = 0x70CC;
    DI = g16(c, 0x70F8);
    CX = 0x0802;
    CALL(0x2D9D);
}

/* 61FA flyer_draw */
void hand_61fa(V30 *c) {
    AX = CGA_SEG; ES = AX;
    G8(0x70F7) = 0;
    AX = (uint16_t)((g16(c, 0x70F0) & 0x1E0) + 0x6F30);
    if (G8(0x70F6) != 0xFF) AX = (uint16_t)(AX + 0xC0);
    SI = AX;
    DI = g16(c, 0x70FA);
    sg16(c, 0x70F8, DI);
    BP = 0x70CC;
    CX = 0x0802;
    CALL(0x2CCC);
}

/* 62A6 flyer_hits_cat: CF=1 if it touches the cat (which falls, with a sound). */
void hand_62a6(V30 *c) {
    V30_TICK(30);
    if (!G8(0x70F2)) { c->cf = 0; return; }
    AX = g16(c, 0x70F3); DL = G8(0x70F5);
    SI = 0x10;
    BX = g16(c, 0x0579); DH = G8(0x057B);
    DI = 0x18; CX = 0x0E08;
    CALL(0x2E29);
    if (!c->cf) return;
    G8(0x0571) = 1; G8(0x0576) = 2; G8(0x0578) = 0x20; G8(0x055B) = 8;
    AX = 0x091D; BX = 0x0CE4;
    CALL(0x593B);
    c->cf = 1;
}

/* 6245 flyer_flip_brick: passing a brick row, flip the brick under it (once per brick). */
void hand_6245(V30 *c) {
    AL = (uint8_t)((G8(0x70F5) - 8) & 0xF8);
    CX = 7;
    int found = 0;
    do {
        BX = (uint16_t)(CX - 1);
        if (AL == G8((uint16_t)(BX + 0x2BD4))) { found = 1; break; }
        V30_TICK(40);
    } while (--CX);
    if (!found) return;
    AX = g16(c, 0x70F3);
    CL = 4;
    AX >>= 4;
    if (AX < 2) return;
    AX = (uint16_t)(AX - 2);
    if (AX >= 0x10) return;
    DI = AX;
    DL = G8((uint16_t)(BX + 0x2BDB));
    DH = 0;
    AX = (uint16_t)(AX + DX);
    if (AX == g16(c, 0x70EC)) return;
    sg16(c, 0x70EC, AX);
    SI = AX;
    G8((uint16_t)(SI + 0x2BE2)) ^= 2;
    AL = G8((uint16_t)(SI + 0x2BE2));
    AH = 0;
    DI = (uint16_t)(DI << 1);
    CX = g16(c, (uint16_t)(DI + 0x70FC));
    DL = G8((uint16_t)(BX + 0x7120));
    uint16_t ax = AX, cx = CX, dx = DX;
    CALL(0x622B);
    DX = dx; CX = cx; BX = ax;
    CALL(0x30E3);
    CALL(0x61FA);
}

/* 6106 flyer_update: once per BIOS tick: maybe start from a side or a window, glide down. */
void hand_6106(V30 *c) {
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, 0x70EE)) return;
    sg16(c, 0x70EE, DX);
    CALL(0x62A6);
    if (c->cf) {
        CALL(0x11E3);
        CALL(0x622B);
        CALL(0x1145);
        G8(0x70F2) = 0;
        return;
    }
    if (!G8(0x70F2)) {
        for (;;) {
            CALL(0x2DFD);
            BX = DX;
            BX &= 0x1F;
            if (BL >= 0x10) {
                BL = (uint8_t)(BL - 0x10);
                if (BL > 9) continue;
                DL = BL < 5 ? 1 : 0xFF;
                G8(0x70F6) = DL;
                G8(0x70F5) = 6;
                BL = (uint8_t)(BL << 1);
                AX = (uint16_t)(g16(c, (uint16_t)(BX + 0x70B8)) + 4);
                sg16(c, 0x70F3, AX);
            } else {
                AX = 0x0C; DL = 1;
                if (BL & 8) { AX = 0x120; DL = 0xFF; }
                sg16(c, 0x70F3, AX);
                G8(0x70F6) = DL;
                BL &= 7;
                AL = (uint8_t)(G8((uint16_t)(BX + 0x70B0)) + 8);
                G8(0x70F5) = AL;
            }
            break;
        }
        G8(0x70F2) = 1;
        G8(0x70F7) = 1;
        sg16(c, 0x70F0, 0);
        sg16(c, 0x70EC, 0xFFFF);
    }
    if (g16(c, 0x70F0) < 0xA0) sg16(c, 0x70F0, (uint16_t)(g16(c, 0x70F0) + 4));
    G8(0x70F5) = (uint8_t)(G8(0x70F5) + 2);
    int gone = G8(0x70F5) > 0xBF;
    if (!gone) {
        if (G8(0x70F6) != 1) {
            uint16_t x = g16(c, 0x70F3);
            sg16(c, 0x70F3, (uint16_t)(x - 5));
            gone = x < 5;
        } else {
            sg16(c, 0x70F3, (uint16_t)(g16(c, 0x70F3) + 5));
            gone = g16(c, 0x70F3) >= 0x12C;
        }
    }
    if (!gone) {
        CX = g16(c, 0x70F3);
        DL = G8(0x70F5);
        CALL(0x2CB0);
        sg16(c, 0x70FA, AX);
        CALL(0x62A6);
        if (!c->cf) {
            CALL(0x6245);
            CALL(0x622B);
            CALL(0x61FA);
            return;
        }
    }
    G8(0x70F2) = 0;
    CALL(0x622B);
}

/* 6040 sweetheart_screen: after room 7: her picture slides down to the cat with a falling
 * tone, then a two-tone call for 18 ticks. */
void hand_6040(V30 *c) {
    c->df = 0;
    ES = DS;
    DI = 0x0E;
    CX = 0x24;
    AX = 0;
    rep_stosw(c);
    sg16(c, 0x6F24, 0x25);
    AX = CGA_SEG; ES = AX;
    do {
        do CALL(0x13D8); while (c->zf);
        SI = 0x0E;
        DI = g16(c, 0x6F24);
        CX = 0x0C03;
        CALL(0x2D9D);
        sg16(c, 0x6F24, (uint16_t)(g16(c, 0x6F24) + 0x1E0));
        SI = 0x6E10;
        DI = g16(c, 0x6F24);
        CX = 0x0C03;
        CALL(0x2D9D);
        do { AH = 0; INT(0x1A); } while (DX == g16(c, 0x6F26));
        sg16(c, 0x6F26, DX);
        if (G8(0x0000)) {
            AL = 0xB6;
            v30_out(c, 0x43, AL, 0);
            AX = (uint16_t)(g16(c, 0x6F24) >> 1);
            v30_out(c, 0x42, AL, 0);
            AL = AH;
            v30_out(c, 0x42, AL, 0);
            AL = (uint8_t)(v30_in(c, 0x61, 0) | 3);
            v30_out(c, 0x61, AL, 0);
        }
    } while (g16(c, 0x6F24) < 0x1A40);
    SI = 0x6E58;
    DI = g16(c, 0x6F24);
    CX = 0x1106;
    CALL(0x2D9D);
    do {
        do { AH = 0; INT(0x1A); } while (DX == g16(c, 0x6F28));
        sg16(c, 0x6F28, DX);
        if (G8(0x0000)) {
            AL = 0xB6;
            v30_out(c, 0x43, AL, 0);
            AX = (DL & 1) ? 0x0B54 : 0x0C00;
            v30_out(c, 0x42, AL, 0);
            AL = AH;
            v30_out(c, 0x42, AL, 0);
        }
        DX = (uint16_t)(DX - g16(c, 0x6F26));
    } while (DX < 0x12);
    CALL(0x5B21);
}
