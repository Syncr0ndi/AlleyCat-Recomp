/* Alley Cat, hand-written: what the cat can stand on.
 *
 * Alley: the clothesline item maps (1016, see lines.c), the window sills (table at 0FF0:
 * per sill bit 7 = row 88h else 90h, bits 0-6 = x / 4; groups per level start at [1006+level]).
 * Rooms: per-room platform lists at [1269 + 2*room] (y in 1029, flag in 1089, x range in
 * 10E9 / 11A9). Each test returns CF=1 and snaps the cat onto the surface. */
#include "hand.h"

#define V_CAT_X  0x0579
#define V_CAT_Y  0x057B
#define V_CAT_Y2 0x057C

/* 15D0 pick_sill: a random window sill of this level, not the last one used:
 * DL = its row (88h / 90h), CX = its x. */
void hand_15d0(V30 *c) {
    BX = g16(c, 0x0008);
    CL = G8((uint16_t)(BX + 0x100E));
    for (;;) {
        CALL(0x2DFD);
        DL &= 7;
        if (DL > CL) continue;
        DL = (uint8_t)(DL + G8((uint16_t)(BX + 0x1006)));
        if (DL != G8(0x1028)) break;
    }
    G8(0x1028) = DL;
    BL = DL;
    CL = G8((uint16_t)(BX + 0x0FF0));
    DL = (CL & 0x80) ? 0x88 : 0x90;
    CX &= 0x7F;
    CX = (uint16_t)(CX << 2);
    V30_TICK(120);
}

/* 1657 on_sill: is the cat at a window sill of this level? */
void hand_1657(V30 *c) {
    CL = (uint8_t)((G8(V_CAT_Y) + 2) & 0xF8);
    BX = g16(c, 0x0008);
    BL = G8((uint16_t)(BX + 0x1006));
    for (;;) {
        AL = G8((uint16_t)(BX + 0x0FF0));
        V30_TICK(90);
        if (AL == 0) { G8(0x127C) = 0; c->cf = 0; return; }
        BX++;
        CH = (AL & 0x80) ? 0x88 : 0x90;
        if (CL != CH) continue;
        AX &= 0x7F;
        AX = (uint16_t)(AX << 2);
        DX = g16(c, V_CAT_X) & 0xFFF8;
        if (DX < AX) continue;
        DX = (uint16_t)((g16(c, V_CAT_X) - 0x0F) & 0xFFF8);
        if (DX > AX) continue;
        break;
    }
    CH = (uint8_t)(CH - 2);
    G8(V_CAT_Y) = CH;
    CH = (uint8_t)(CH + 0x32);
    G8(V_CAT_Y2) = CH;
    if (!G8(0x127C)) { G8(0x127C) = 1; CALL(0x590E); }
    c->cf = 1;
}

/* 1799 bit_of: BX = x (pixels) -> BX = x / 64 (byte), CH = bit for x / 8 in that byte. */
void hand_1799(V30 *c) {
    CL = 3;
    BX >>= 3;
    CH = BL;
    BX >>= 3;
    CL = CH & 7;
    CH = (uint8_t)(0x80 >> CL);
    V30_TICK(60);
}

/* 17AD on_line_item: is the cat on a solid item hanging on one of the three lines? */
void hand_17ad(V30 *c) {
    DL = G8(V_CAT_Y) & 0xF8;
    BX = 0;
    V30_TICK(60);
    if (DL != 0x08) {
        BL++;
        if (DL != 0x28) {
            BL++;
            if (DL != 0x48) { c->cf = 0; return; }
        }
    }
    AX = g16(c, V_CAT_X);
    if (BX == g16(c, 0x052F) && G8(0x0525) <= 3) {     /* that line is scrolling: allow for it */
        if (BL == 1) {
            CH = 0;
            CL = (uint8_t)(G8(0x0525) + 1);
            CL = (uint8_t)(CL << 2);
            AX = (uint16_t)(AX - CX);
        } else {
            CX = 4;
            CL = (uint8_t)(CL - G8(0x0525));
            CL = (uint8_t)(CL << 2);
            AX = (uint16_t)(AX + CX);
        }
    }
    BL = G8((uint16_t)(BX + 0x1025));
    SI = BX;
    BX = AX;
    BX += 0x0A;
    CALL(0x1799);
    if (!(G8((uint16_t)(BX + SI + 0x1016)) & CH)) { c->cf = 0; return; }
    G8(V_CAT_Y) = DL;
    DL = (uint8_t)(DL + 0x32);
    G8(V_CAT_Y2) = DL;
    sg16(c, V_CAT_X, g16(c, V_CAT_X) & 0xFFF8);
    G8(0x055C) = 1;
    c->cf = 1;
}

/* 16C6 on_room_surface: in a room: caught by the thing at [2650] (-> leave, [0551]),
 * the special test of room 3, or one of the room's platforms. */
void hand_16c6(V30 *c) {
    G8(0x39E0) = 0;
    if (G8(0x0571) == 1) {
        AX = (uint16_t)(g16(c, 0x2650) - 4);
        DL = (uint8_t)(G8(0x2652) - 8);
        SI = 0x0C;
        BX = g16(c, V_CAT_X);
        DH = G8(V_CAT_Y);
        DI = 0x18;
        CX = 0x0E10;
        CALL(0x2E29);
        if (c->cf) { G8(0x0551) = 1; c->cf = 0; return; }
    }
    if (g16(c, 0x0004) == 3) {
        CALL(0x3C43);
        if (c->cf) { G8(0x055C) = 1; return; }
    }
    CL = G8(V_CAT_Y) & 0xF8;
    BX = (uint16_t)(g16(c, 0x0004) << 1);
    BX = g16(c, (uint16_t)(BX + 0x1269));
    for (;;) {
        CH = G8((uint16_t)(BX + 0x1029));
        V30_TICK(110);
        if (CH == 0) { c->cf = 0; return; }
        AL = G8((uint16_t)(BX + 0x1089));
        G8(0x127B) = AL;
        BL = (uint8_t)(BL << 1);
        AX = g16(c, (uint16_t)(BX + 0x11A9));
        sg16(c, 0x1279, AX);
        AX = g16(c, (uint16_t)(BX + 0x10E9));
        BL = (uint8_t)(BL >> 1);
        BX++;
        if (CL != CH) continue;
        DX = g16(c, V_CAT_X) & 0xFFF8;
        if (DX < AX) continue;
        uint16_t x = g16(c, V_CAT_X), w = g16(c, 0x1279);
        DX = x < w ? 0 : (uint16_t)(x - w);
        DX &= 0xFFFC;
        if (DX > AX) continue;
        break;
    }
    G8(V_CAT_Y) = CH;
    CH = (uint8_t)(CH + 0x32);
    G8(V_CAT_Y2) = CH;
    AL = G8(0x127B);
    G8(0x055C) = AL;
    if (AL) sg16(c, V_CAT_X, g16(c, V_CAT_X) & 0xFFFC);
    if (g16(c, 0x0004) == 4) {                          /* room 4: remember which shelf */
        BX--;
        uint16_t b = BX;
        BX = (uint16_t)(BX - 0x27);
        if (b >= 0x27 && BX < 0x10) { BX++; G8(0x39E0) = BL; }
    }
    c->cf = 1;
}

/* 1608 cat_on_ground: CF=1 if the cat stands on something (and is placed on it). */
void hand_1608(V30 *c) {
    if (g16(c, 0x0004) == 7) { CALL(0x30FA); return; }
    if (g16(c, 0x0004) != 0) { CALL(0x16C6); return; }
    AL = G8(V_CAT_Y) & 0xF8;
    if (AL != 0x60) {
        CALL(0x1657);
        if (c->cf) return;
        CALL(0x17AD);
        return;
    }
    /* y = 60h: the fence top */
    if (G8(0x0550) >= 2) { c->cf = 0; return; }
    G8(V_CAT_Y) = AL;
    AL = (uint8_t)(AL + 0x32);
    G8(V_CAT_Y2) = AL;
    if (G8(0x0550) != 1) {
        G8(0x0550) = 1;
        AH = 0;
        INT(0x1A);
        sg16(c, 0x0556, DX);
    }
    c->cf = 1;
}
