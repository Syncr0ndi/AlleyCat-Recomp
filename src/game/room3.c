/* Alley Cat, hand-written: room 3, the bookshelves.
 *
 * Three things to knock off the shelves ([37B0+2n] still there, x [37A3+2n], screen
 * [37A9+2n], [37AF] left). The shelves are 8 pixels apart; the cat can stand on them. */
#include "hand.h"

/* 3B30 room3_reset */
void hand_3b30(V30 *c) {
    G8(0x37AF) = 3;
    AX = 1;
    sg16(c, 0x37B0, AX); sg16(c, 0x37B2, AX); sg16(c, 0x37B4, AX);
    V30_TICK(80);
}

/* 3BA3 room3_take: remove thing BX (blank it); the last one finishes the room. */
void hand_3ba3(V30 *c) {
    sg16(c, (uint16_t)(BX + 0x37B0), 0);
    ES = DS;
    c->df = 0;
    AX = 0xAAAA;
    DI = 0x0E;
    SI = DI;
    CX = 0x20;
    rep_stosw(c);
    DI = g16(c, (uint16_t)(BX + 0x37A9));
    AX = CGA_SEG; ES = AX;
    CX = 0x1002;
    CALL(0x2D9D);
    if (--G8(0x37AF) == 0 && !G8(0x0552)) G8(0x0553) = 1;
}

/* 3B42 room3_update: once per BIOS tick: the cat touching a thing takes it. */
void hand_3b42(V30 *c) {
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, 0x37B8)) return;
    sg16(c, 0x37B8, DX);
    sg16(c, 0x37B6, 4);
    for (;;) {
        BX = g16(c, 0x37B6);
        if (g16(c, (uint16_t)(BX + 0x37B0))) {
            AX = g16(c, (uint16_t)(BX + 0x37A3));
            DL = 0x18; SI = 0x10;
            BX = g16(c, 0x0579);
            DH = G8(0x057B);
            DI = 0x18; CX = 0x0E10;
            CALL(0x2E29);
            if (c->cf) {
                AX = 0x0C00; BX = 0x08FD;
                CALL(0x593B);
                CALL(0x11E3);
                CALL(0x3E38);
                BX = g16(c, 0x37B6);
                CALL(0x3BA3);
                CALL(0x1124);
                CALL(0x3E14);
                return;
            }
        }
        uint16_t n = g16(c, 0x37B6);
        sg16(c, 0x37B6, (uint16_t)(n - 2));
        if (n < 2) return;
    }
}

/* 3C43 on_shelf: CF=1 (cat placed) if the cat is on a shelf. */
void hand_3c43(V30 *c) {
    AX = g16(c, 0x0579) & 0xFFFC;
    V30_TICK(80);
    if (AX < 0xA4 || AX > 0x118) { c->cf = 0; return; }
    DL = (uint8_t)((G8(0x057B) - 2) & 0xF8);
    if (!(DL & 8) || DL < 0x28 || DL > 0xA0) { c->cf = 0; return; }
    sg16(c, 0x0579, AX);
    DL = (uint8_t)(DL + 2);
    G8(0x057B) = DL;
    DL = (uint8_t)(DL + 0x32);
    G8(0x057C) = DL;
    c->cf = 1;
}

/* 3BDB shelves_draw: 16 rows of 16 book tiles (random spines, gaps, end pieces). */
void hand_3bdb(V30 *c) {
    AX = CGA_SEG; ES = AX;
    sg16(c, 0x37A0, 0x066A);
    CX = 0x10;
    for (;;) {
        AX = 0;
        BX = AX;
        for (;;) {
            G8(0x37A2) = AL;
            AH = 0;
            AX = (uint16_t)(AX + 0x3730);
            SI = AX;
            DI = (uint16_t)(g16(c, 0x37A0) + BX);
            uint16_t cx = CX, bx = BX;
            CX = 0x0801;
            CALL(0x2D9D);
            BX = bx; CX = cx;
            BX += 2;
            if (BX < 0x1E) {
                if (G8(0x37A2) != 0x50 && !(CL & 1)) {
                    CALL(0x2DFD);
                    if (DL >= 0x40) { AL = 0x10; continue; }
                }
                CALL(0x2DFD);
                AL = (uint8_t)(DL - BL);
                AL &= 0x30;
                AL = (uint8_t)(AL + 0x30);
                continue;
            }
            if (BX == 0x1E) { AL = 0x20; continue; }
            break;
        }
        sg16(c, 0x37A0, (uint16_t)(g16(c, 0x37A0) + 0x140));
        if (!--CX) return;
    }
}

/* 3C90 room3_dog_reset (the thing at [3964]) */
void hand_3c90(V30 *c) {
    G8(0x3966) = 8; G8(0x396A) = 1; G8(0x3967) = 0; G8(0x396D) = 2;
    sg16(c, 0x3964, 0x118);
    sg16(c, 0x396B, 0);
    V30_TICK(100);
}
