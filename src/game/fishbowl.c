/* Alley Cat, hand-written: room 2, the fishbowl.
 *
 * 24 swimmers: 0-11 fish (eat them all to finish the room), 12-23 eels (touching one gives
 * a shock and costs the room). Per swimmer n: x [3447+2n], y [3477+n] (home row [34F1+n]),
 * directions [3417+n] / [342F+n], gone [34A7+n], not yet drawn [348F+n], screen address
 * [34BF+2n]. One swimmer moves per BIOS tick ([3415]); fish left [3410]. */
#include "hand.h"

/* 37C1 swimmer_erase: blank swimmer BX (if it was drawn). */
void hand_37c1(V30 *c) {
    CMP8(G8((uint16_t)(BX + 0x348F)), 0);
    V30_TICK(30);
    if (!c->zf) return;
    BX = (uint16_t)(BX << 1);
    SI = 0x3404;
    DI = g16(c, (uint16_t)(BX + 0x34BF));
    AX = CGA_SEG; ES = AX;
    CX = BX < 0x18 ? 0x0601 : 0x0202;
    CALL(0x2D9D);
}

static uint8_t rnd_dir(V30 *c) {
    CALL(0x2DFD);
    DL &= 1;
    if (!DL) DL = (uint8_t)~DL;
    return DL;
}

/* 35C9 fishbowl_reset: everyone at home, random directions and x; some eels absent
 * (count by level). */
void hand_35c9(V30 *c) {
    sg16(c, 0x3411, 0);
    sg16(c, 0x3415, 0);
    G8(0x3410) = 0x0C;
    CX = 0x18;
    do {
        BX = (uint16_t)(CX - 1);
        G8((uint16_t)(BX + 0x348F)) = 1;
        G8((uint16_t)(BX + 0x34A7)) = 0;
        AL = G8((uint16_t)(BX + 0x34F1));
        G8((uint16_t)(BX + 0x3477)) = AL;
        G8((uint16_t)(BX + 0x342F)) = 1;
        G8((uint16_t)(BX + 0x3417)) = rnd_dir(c);
        BX = (uint16_t)(BX << 1);
        CALL(0x2DFD);
        DH = 0;
        sg16(c, (uint16_t)(BX + 0x3447), DX);
    } while (--CX);
    BX = g16(c, 0x0008);
    CL = G8((uint16_t)(BX + 0x351C));
    CH = 0;
    do {
        do {
            do { CALL(0x2DFD); DL &= 0x0F; } while (DL >= 0x0C);
            BL = (uint8_t)(DL + 0x0C);
            BH = 0;
        } while (G8((uint16_t)(BX + 0x34A7)));
        G8((uint16_t)(BX + 0x34A7)) = 1;
    } while (--CX);
}

/* 363D fish_return: bring back eels (as many as fish were eaten, [351B]) at the side away
 * from the cat. */
void hand_363d(V30 *c) {
again:
    CX = 0x0C;
    do {
        BX = (uint16_t)(CX + 0x0B);
        if (G8((uint16_t)(BX + 0x34A7))) {
            AX = 0;
            DL = 1;
            G8((uint16_t)(BX + 0x34A7)) = AL;
            if (!(g16(c, 0x0579) > 0xA0)) { AX = 0x12E; DL = 0xFF; }
            G8((uint16_t)(BX + 0x3417)) = DL;
            BL = (uint8_t)(BL << 1);
            sg16(c, (uint16_t)(BX + 0x3447), AX);
            if (--G8(0x351B)) goto again;
            return;
        }
        V30_TICK(40);
    } while (--CX);
}

/* 34A0 fishbowl_touch: check every swimmer against the cat: fish are eaten (sound,
 * points via [0553] when all are gone), an eel shocks the cat (flashing screen) and ends
 * the room ([0552]). CF=1 if anything was eaten. */
void hand_34a0(V30 *c) {
    sg16(c, 0x3511, 0);
    G8(0x351B) = 0;
    do {
        BX = g16(c, 0x3511);
        if (G8((uint16_t)(BX + 0x34A7))) continue;
        SI = (uint16_t)(BX << 1);
        AX = g16(c, (uint16_t)(SI + 0x3447));
        DL = G8((uint16_t)(BX + 0x3477));
        DI = BX < 0x0C ? 0 : 2;
        SI = g16(c, (uint16_t)(DI + 0x3513));
        CX = g16(c, (uint16_t)(DI + 0x3517));
        BX = g16(c, 0x0579);
        DH = G8(0x057B);
        DI = 0x18;
        CH = 0x0E;
        CALL(0x2E29);
        if (!c->cf) continue;
        BX = g16(c, 0x3511);
        if (BX >= 0x0C && !G8(0x0553) && !G8(0x05F3)) {     /* eel: shock */
            G8(0x0552) = 1;
            CX = g16(c, 0x0579);
            CX = CX < 8 ? 0 : (uint16_t)(CX - 8);
            if (CX >= 0x117) CX = 0x116;
            DL = G8(0x057B);
            if (DL >= 0xB5) DL = 0xB4;
            CALL(0x2CB0);
            DI = AX;
            SI = 0x3350;
            AX = CGA_SEG; ES = AX;
            CX = 0x1205;
            CALL(0x2D9D);
            CALL(0x5797);
            AH = 0;
            INT(0x1A);
            sg16(c, 0x3509, DX);
            do {
                uint16_t dx = DX;
                do { CALL(0x57A6); CALL(0x13D8); } while (c->zf);
                CALL(0x57A6);
                DX = dx;
                BX = (DL & 1) ? 1 : 0x0F;                    /* flash the background */
                AH = 0x0B;
                INT(0x10);
                CALL(0x57A6);
                AH = 0;
                INT(0x1A);
                DX = (uint16_t)(DX - g16(c, 0x3509));
                CMP16(DX, 0x0D);
            } while (c->cf);
            return;
        }
        G8(0x351B)++;                                         /* eaten */
        AX = 0x05DC; BX = 0x0425;
        CALL(0x593B);
        if (G8(0x351B) == 1) CALL(0x11E3);
        BX = g16(c, 0x3511);
        CALL(0x37C1);
        BX = g16(c, 0x3511);
        G8((uint16_t)(BX + 0x34A7)) = 1;
        if (BX < 0x0C) {
            if (--G8(0x3410) == 0 && !G8(0x05F3)) G8(0x0553) = 1;
        }
    } while (sg16(c, 0x3511, (uint16_t)(g16(c, 0x3511) + 1)), g16(c, 0x3511) < 0x18);
    if (G8(0x351B)) { CALL(0x363D); c->cf = 1; return; }
    c->cf = 0;
}

/* 3675 fishbowl_update: once per BIOS tick (fish more slowly on a PCjr) move the next
 * swimmer: random turns, bounce at the sides and its band, redraw. */
void hand_3675(V30 *c) {
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, 0x3509)) return;
    sg16(c, 0x350B, DX);
    sg16(c, 0x3415, (uint16_t)(g16(c, 0x3415) + 1));
    BX = g16(c, 0x3415);
    int stamp = 0;
    if (BX >= 0x18) {
        BX = 0;
        sg16(c, 0x3415, BX);
        sg16(c, 0x3411, g16(c, 0x3411) ^ 0x0C);
        sg16(c, 0x3413, (uint16_t)(g16(c, 0x3413) + 8));
        stamp = 1;
    } else if (BX == 0x0C) {
        stamp = !(G8(0x0697) == 0xFD && G8(0x057B) < 0x30);
    }
    if (stamp) { AX = g16(c, 0x350B); sg16(c, 0x3509, AX); }
    SI = (uint16_t)(BX << 1);
    if (G8((uint16_t)(BX + 0x34A7))) return;
    CALL(0x2DFD);
    if (DL <= 0x10) {                                    /* turn: this draw gives x, a new one y */
        DL &= 1;
        if (!DL) DL = (uint8_t)~DL;
        G8((uint16_t)(BX + 0x3417)) = DL;
        G8((uint16_t)(BX + 0x342F)) = rnd_dir(c);
    }
    CX = 4;
    if (BX >= 0x0C) CL = (uint8_t)(CL >> 1);
    AX = g16(c, (uint16_t)(SI + 0x3447));
    if (G8((uint16_t)(BX + 0x3417)) == 1) {
        AX = (uint16_t)(AX + CX);
        if (AX >= 0x12F) { AX = 0x12E; G8((uint16_t)(BX + 0x3417)) = 0xFF; }
    } else {
        if (AX < CX) { AX = 0; G8((uint16_t)(BX + 0x3417)) = 1; }
        else AX = (uint16_t)(AX - CX);
    }
    sg16(c, (uint16_t)(SI + 0x3447), AX);
    AL = G8((uint16_t)(BX + 0x3477));
    if (G8((uint16_t)(BX + 0x342F)) == 1) {
        AL++;
        DL = (uint8_t)(G8((uint16_t)(BX + 0x34F1)) + 0x18);
        if (AL > DL) { AL = DL; G8((uint16_t)(BX + 0x342F)) = 0xFF; }
    } else {
        AL--;
        if (AL < G8((uint16_t)(BX + 0x34F1))) { AL = G8((uint16_t)(BX + 0x34F1)); G8((uint16_t)(BX + 0x342F)) = 1; }
    }
    G8((uint16_t)(BX + 0x3477)) = AL;
    DL = AL;
    CX = g16(c, (uint16_t)(SI + 0x3447));
    CALL(0x2CB0);
    sg16(c, 0x34EF, AX);
    BX = g16(c, 0x3415);
    CALL(0x37C1);
    BX = g16(c, 0x3415);
    SI = (uint16_t)(BX << 1);
    DI = g16(c, 0x34EF);
    sg16(c, (uint16_t)(SI + 0x34BF), DI);
    G8((uint16_t)(BX + 0x348F)) = 0;
    AX = CGA_SEG; ES = AX;
    if (BX >= 0x0C) {                                    /* eel: 4 frames */
        SI = BX;
        CL = 3;
        SI = (uint16_t)(SI << 3);
        SI = (uint16_t)(SI + g16(c, 0x3413));
        SI &= 0x18;
        SI = (uint16_t)(SI + 0x3330);
        CX = 0x0202;
        CALL(0x2D9D);
        return;
    }
    SI = g16(c, 0x3411);                                 /* fish: 2 frames, 2 directions */
    if (!(BL & 1)) SI ^= 0x0C;
    if (G8((uint16_t)(BX + 0x3417)) != 1) SI = (uint16_t)(SI + 0x18);
    SI = (uint16_t)(SI + 0x3300);
    CX = 0x0601;
    CALL(0x2D9D);
}

/* 37E5 waves: every 8 ticks, change the next tile of the water surface (not right under
 * the cat when it is at the top). */
void hand_37e5(V30 *c) {
    AH = 0;
    INT(0x1A);
    AX = (uint16_t)(DX - g16(c, 0x350F));
    if (AX < 8) return;
    sg16(c, 0x350D, (uint16_t)(g16(c, 0x350D) + 1));
    BX = g16(c, 0x350D);
    if (BX >= 0x28) { BX = 0; sg16(c, 0x350D, BX); sg16(c, 0x350F, DX); }
    DI = (uint16_t)(BX << 1);
    if (G8(0x057B) <= 7) {
        AX = g16(c, 0x0579);
        CL = 2;
        AX >>= 2;
        AX++;
        uint16_t a = AX;
        AX = (uint16_t)(AX - DI);
        if (a < DI) AX = (uint16_t)~AX;
        if (AX < 4) return;
    }
    DI = (uint16_t)(DI + 0xA0);
    AL = (uint8_t)(G8((uint16_t)(BX + 0x2656)) + 8);
    G8((uint16_t)(BX + 0x2656)) = AL;
    AX &= 0x18;
    AX = (uint16_t)(AX + 0x2020);
    SI = AX;
    AX = CGA_SEG; ES = AX;
    CX = 0x0401;
    CALL(0x2D9D);
}
