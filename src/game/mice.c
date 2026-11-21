/* Alley Cat, hand-written: the three mice in the alley, and the creature on the sills.
 *
 * Mouse n (0-2, one moved per BIOS tick, [1F6C]): x [1F30+2n], y [1F36+n], direction
 * [1F3C+n] (1 right, FF left, 0 stopped), previous direction [1F3F+n], caught [1F50+n]
 * (time [1F53+2n]), screen address [1F42+2n], saved screen at [1F59+2n], points [1F39+n].
 * They run along the fence and the lines; they run away from a jumping cat; the cat catches
 * one by landing on it (a bonus, then it reappears at the far side after 36h ticks) unless
 * it is coming down from above (then it bites and the cat drops). */
#include "hand.h"

#define V_CUR 0x1F6C

/* 2330 mice_reset: all three at the side away from the cat. */
void hand_2330(V30 *c) {
    sg16(c, V_CUR, 0);
    AX = 0;
    DL = 1;
    if (!(g16(c, 0x0579) > 0xA0)) { AX = 0x12C; DL = 0xFF; }
    sg16(c, 0x1F30, AX); sg16(c, 0x1F32, AX); sg16(c, 0x1F34, AX);
    G8(0x1F3C) = DL; G8(0x1F3D) = DL; G8(0x1F3E) = DL;
    G8(0x1F48) = 1; G8(0x1F49) = 1; G8(0x1F4A) = 1;
    G8(0x1F50) = 0; G8(0x1F51) = 0; G8(0x1F52) = 0;
    V30_TICK(200);
}

/* 265E mouse_hit_by_object: CF=1 if the thrown object touches the current mouse. */
void hand_265e(V30 *c) {
    V30_TICK(30);
    if (!G8(0x1673)) { c->cf = 0; return; }
    BX = g16(c, V_CUR);
    DL = G8((uint16_t)(BX + 0x1F36));
    BL = (uint8_t)(BL << 1);
    AX = g16(c, (uint16_t)(BX + 0x1F30));
    SI = 0x10;
    DI = SI;
    BX = g16(c, 0x1671);
    DH = G8(0x1673);
    CX = 0x0C08;
    CALL(0x2E29);
}

/* 254D mouse_erase */
void hand_254d(V30 *c) {
    BX = g16(c, V_CUR);
    BL = (uint8_t)(BL << 1);
    AX = CGA_SEG; ES = AX;
    DI = g16(c, (uint16_t)(BX + 0x1F42));
    SI = g16(c, (uint16_t)(BX + 0x1F59));
    CX = 0x0802;
    V30_TICK(60);
    CALL(0x2D9D);
}

/* 2567 mouse_meets_cat: CF=1 if the current mouse touches the cat (and handle it). */
void hand_2567(V30 *c) {
    BX = g16(c, V_CUR);
    DL = G8((uint16_t)(BX + 0x1F36));
    BL = (uint8_t)(BL << 1);
    AX = g16(c, (uint16_t)(BX + 0x1F30));
    SI = 0x10;
    BX = g16(c, 0x0579);
    DH = G8(0x057B);
    DI = 0x18;
    CX = 0x0E08;
    CALL(0x2E29);
    if (!c->cf) return;
    BX = g16(c, V_CUR);
    if (G8((uint16_t)(BX + 0x1F50)) || G8(0x057C) < 0x26) { c->cf = 1; return; }
    if (G8(0x055C)) {                                    /* landing on it from a jump: bitten */
        G8(0x055C) = 0; G8(0x055B) = 0x11; G8(0x0571) = 1; G8(0x056E) = 0;
        BX = g16(c, V_CUR);
        BL = (uint8_t)(BL << 1);
        DI = g16(c, (uint16_t)(BX + 0x1F42));
        if (g16(c, (uint16_t)(BX + 0x1F30)) >= 0x10) DI -= 4;
        sg16(c, 0x1F67, DI);
        AX = CGA_SEG; ES = AX;
        SI = 0x1D70; BP = 0x0E; CX = 0x0806;
        CALL(0x2CCC);
        AH = 0;
        INT(0x1A);
        sg16(c, 0x1F65, DX);
        do {
            CALL(0x5A90);
            AH = 0;
            INT(0x1A);
            DX = (uint16_t)(DX - g16(c, 0x1F65));
        } while (DX < 8);
        CALL(0x5B21);
        DI = g16(c, 0x1F67);
        SI = 0x0E; CX = 0x0806;
        CALL(0x2D9D);
        c->cf = 1;
        return;
    }
    /* caught: points and a sound */
    AH = 0;
    INT(0x1A);
    BX = g16(c, V_CUR);
    G8((uint16_t)(BX + 0x1F50)) = 1;
    G8((uint16_t)(BX + 0x1F3C)) = 1;
    BL = (uint8_t)(BL << 1);
    sg16(c, (uint16_t)(BX + 0x1F53), DX);
    CALL(0x11E3);
    BX = g16(c, V_CUR);
    BL = (uint8_t)(BL << 1);
    SI = g16(c, (uint16_t)(BX + 0x1F5F));
    DI = g16(c, (uint16_t)(BX + 0x1F42));
    AX = CGA_SEG; ES = AX;
    BP = 0x0E; CX = 0x0802;
    CALL(0x2CCC);
    CALL(0x1124);
    BX = g16(c, V_CUR);
    AL = G8((uint16_t)(BX + 0x1F39));
    CALL(0x2706);
    AX = 0x03E8; BX = 0x02EE;
    CALL(0x593B);
    c->cf = 1;
}

/* 237B mice_update: once per BIOS tick, move the next mouse. */
void hand_237b(V30 *c) {
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, 0x1F65)) return;
    sg16(c, 0x1F65, DX);
    if (G8(0x055A)) return;
    BX = (uint16_t)(g16(c, V_CUR) + 1);
    if (BX >= 3) BX = 0;
    sg16(c, V_CUR, BX);
    CALL(0x265E);
    if (c->cf) return;
    CALL(0x2567);
    if (c->cf) return;
    BX = g16(c, V_CUR);
    if (G8((uint16_t)(BX + 0x1F50))) {                   /* caught one: comes back later */
        AH = 0;
        INT(0x1A);
        BX = g16(c, V_CUR);
        BL = (uint8_t)(BL << 1);
        DX = (uint16_t)(DX - g16(c, (uint16_t)(BX + 0x1F53)));
        if (DX < 0x36) return;
        DL = 1;
        AX = 0;
        if (!(g16(c, 0x0579) > 0xA0)) { AX = 0x12C; DL = 0xFF; }
        sg16(c, (uint16_t)(BX + 0x1F30), AX);
        BL = (uint8_t)(BL >> 1);
        G8((uint16_t)(BX + 0x1F50)) = 0;
        G8((uint16_t)(BX + 0x1F3C)) = DL;
    }
    DL = G8((uint16_t)(BX + 0x1F3C));
    G8((uint16_t)(BX + 0x1F3F)) = DL;
    int random_dir = 0;
    if (G8(0x1664)) {
        sg16(c, 0x1F69, 0x0C);
        random_dir = 1;
    } else {
        AX = 8;
        if (G8(0x057B) > 0x60) AL = (uint8_t)(AL >> 1);
        sg16(c, 0x1F69, AX);
        random_dir = BX == g16(c, 0x052F);
    }
    if (random_dir && G8((uint16_t)(BX + 0x1F3C)) == 0) {
        CALL(0x2DFD);
        goto pick;                                       /* 248A */
    }
    /* 2425: flee from a jumping cat at its height */
    if (G8(0x055C)) {
        AL = G8((uint16_t)(BX + 0x1F36));
        if (AL <= G8(0x057B)) {
            AL = (uint8_t)(AL + 0x10);
            if (AL >= G8(0x057B)) {
                CALL(0x2DFD);
                SI = g16(c, 0x0008);
                if (DL <= G8((uint16_t)(SI + 0x1F6E))) {
                    sg16(c, 0x1F69, 0x0C);
                    AL = 1;
                    BL = (uint8_t)(BL << 1);
                    CX = g16(c, (uint16_t)(BX + 0x1F30));
                    BL = (uint8_t)(BL >> 1);
                    if (CX >= g16(c, 0x0579)) AL = 0xFF;
                    goto set;
                }
            }
        }
    }
    /* 2466: now and then change direction / stop */
    CL = 0x18;
    if (G8(0x057B) > 0x60) {
        CL = 0x28;
        if (G8((uint16_t)(BX + 0x1F3C)) == 0) CL = 0x10;
    }
    CALL(0x2DFD);
    if (DL > CL) goto move;
    AL = 0;
    if (G8((uint16_t)(BX + 0x1F3C))) goto set;
pick:
    AL = DL & 1;
    if (!AL) AL = 0xFF;
set:
    G8((uint16_t)(BX + 0x1F3C)) = AL;
move:
    DL = G8((uint16_t)(BX + 0x1F3C));
    BL = (uint8_t)(BL << 1);
    AX = g16(c, (uint16_t)(BX + 0x1F30));
    if (DL == 1) {
        AX = (uint16_t)(AX + g16(c, 0x1F69));
        if (AX >= 0x12F) { AX = 0x12E; DL = 0xFF; }
    } else if (DL > 1) {
        uint16_t s = g16(c, 0x1F69);
        if (AX < s) { AX = 0; DL = 1; } else AX = (uint16_t)(AX - s);
    }
    sg16(c, (uint16_t)(BX + 0x1F30), AX);
    BL = (uint8_t)(BL >> 1);
    G8((uint16_t)(BX + 0x1F3C)) = DL;
    DL = G8((uint16_t)(BX + 0x1F36));
    CX = AX;
    CALL(0x2CB0);
    sg16(c, 0x1F4B, AX);
    BX = g16(c, V_CUR);
    if (!G8((uint16_t)(BX + 0x1F48))) {
        AL = (uint8_t)(G8((uint16_t)(BX + 0x1F3C)) | G8((uint16_t)(BX + 0x1F3F)));
        if (AL) CALL(0x254D);
    }
    CALL(0x265E);
    if (c->cf) return;
    CALL(0x2567);
    if (c->cf) return;
    BX = g16(c, V_CUR);
    G8((uint16_t)(BX + 0x1F48)) = 0;
    if (G8((uint16_t)(BX + 0x1F3C)) == 0) {
        if (G8((uint16_t)(BX + 0x1F3F)) == 0) return;
        SI = 0x1E30;                                     /* sitting */
    } else {
        SI = 0x1E50;                                     /* running frames */
        G8((uint16_t)(BX + 0x1F4D))++;
        if (!(G8((uint16_t)(BX + 0x1F4D)) & 1)) SI += 0x20;
        if (G8((uint16_t)(BX + 0x1F3C)) != 1) SI += 0x40;
    }
    BL = (uint8_t)(BL << 1);
    DI = g16(c, 0x1F4B);
    sg16(c, (uint16_t)(BX + 0x1F42), DI);
    AX = CGA_SEG; ES = AX;
    BP = g16(c, (uint16_t)(BX + 0x1F59));
    CX = 0x0802;
    CALL(0x2D35);
}

/* ---- the creature that pops out on a window sill ([1D59] animation step, 1Bh..0) ---- */

/* 22F7 sill_touch: CF=1 (and [1D58]=1) if it touches the cat. */
void hand_22f7(V30 *c) {
    V30_TICK(30);
    if (!G8(0x1D59)) { c->cf = 0; return; }
    CX = g16(c, 0x1D64);
    uint8_t t = CL; CL = CH; CH = t;
    AX = g16(c, 0x1D5C);
    DL = G8(0x1D5F);
    SI = 0x10;
    BX = g16(c, 0x0579);
    DH = G8(0x057B);
    DI = 0x18;
    CH = 0x0E;
    CALL(0x2E29);
    if (c->cf) G8(0x1D58) = 1;
}

/* 22DC sill_erase */
void hand_22dc(V30 *c) {
    CMP8(G8(0x1D59), 0x1A);
    V30_TICK(30);
    if (c->zf) return;
    AX = CGA_SEG; ES = AX;
    DI = g16(c, 0x1D60);
    SI = 0x1D24;
    CX = g16(c, 0x1D66);
    CALL(0x2D9D);
}

/* 2216 sill_update: once per BIOS tick (outside the retrace): maybe start at a random sill
 * (always when the cat is at sill height), rise, wait, sink. */
void hand_2216(V30 *c) {
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, 0x1D5A)) return;
    CX = DX;
    CALL(0x13D8);
    if (c->zf) return;
    sg16(c, 0x1D5A, CX);
    CALL(0x22F7);
    if (c->cf) return;
    if (!G8(0x1D59)) {
        if (G8(0x057B) != 0x86 && G8(0x057B) != 0x8E) {
            CALL(0x2DFD);
            if (DL > 5) return;
        }
        CALL(0x15D0);
        DL = (uint8_t)(DL + 3);
        G8(0x1D5E) = DL;
        CALL(0x2DFD);
        DX &= 7;
        CX = (uint16_t)(CX + DX);
        CX = (uint16_t)(CX + 6);
        sg16(c, 0x1D5C, CX);
        G8(0x1D59) = 0x1B;
    }
    G8(0x1D59)--;
    CX = g16(c, 0x1D5C);
    DL = G8(0x1D5E);
    if (G8(0x1D59) > 0x0D) {                             /* sinking */
        DL = (uint8_t)(DL + G8(0x1D59)); DL = (uint8_t)(DL - 0x0F);
        BX = 0x1B02; BH = (uint8_t)(BH - G8(0x1D59));
    } else {                                             /* rising */
        DL = (uint8_t)(DL + 0x0C); DL = (uint8_t)(DL - G8(0x1D59));
        BX = 2; BH = (uint8_t)(BH + G8(0x1D59));
    }
    sg16(c, 0x1D64, BX);
    G8(0x1D5F) = DL;
    CALL(0x2CB0);
    sg16(c, 0x1D62, AX);
    CALL(0x22DC);
    CALL(0x22F7);
    if (c->cf || !G8(0x1D59)) return;
    AX = CGA_SEG; ES = AX;
    DI = g16(c, 0x1D62);
    SI = 0x1CF0;
    sg16(c, 0x1D60, DI);
    CX = g16(c, 0x1D64);
    sg16(c, 0x1D66, CX);
    BP = 0x1D24;
    CALL(0x2CCC);
}
