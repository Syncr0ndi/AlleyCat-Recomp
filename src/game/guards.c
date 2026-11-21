/* Alley Cat, hand-written: room 7's wall guards, one per brick row.
 *
 * Each guard has a 12-byte record (copied to 4548 while it is processed, tables at
 * [45A8+2n]): x [4548], direction [454A], y [454B], screen [454C], hidden [454E], waiting
 * since [454F] (0 = active), frame [4551], knocked-back steps [4553]. They patrol, walk at
 * the cat on their row, knock it down, and push out pieces the cat has put in the wall. */
#include "hand.h"

#define CUR 0x45B6

/* 4FBB guard_store / 4FCD guard_load (record BX) */
void hand_4fbb(V30 *c) {
    ES = DS;
    BL = (uint8_t)(BL << 1);
    c->df = 0;
    DI = g16(c, (uint16_t)(BX + 0x45A8));
    SI = 0x4548;
    CX = 0x0C;
    rep_movsb(c);
}
void hand_4fcd(V30 *c) {
    ES = DS;
    BL = (uint8_t)(BL << 1);
    c->df = 0;
    SI = g16(c, (uint16_t)(BX + 0x45A8));
    DI = 0x4548;
    CX = 0x0C;
    rep_movsb(c);
}

/* 4FDF guard_or: OR a 12-row, 3-word picture DS:SI onto the screen at DI. */
void hand_4fdf(V30 *c) {
    AX = CGA_SEG; ES = AX;
    c->df = 0;
    DH = 0x0C;
    do {
        CX = 3;
        do {
            BX = m16(c, ES, DI);
            AX = (uint16_t)(m16(c, DS, SI) | BX); SI += 2;
            w16(c, ES, DI, AX); DI += 2;
            V30_TICK(60);
        } while (--CX);
        DI = (uint16_t)(DI - 6);
        DI ^= 0x2000;
        if (!(DI & 0x2000)) DI = (uint16_t)(DI + 0x50);
    } while (--DH);
}

/* 5008 guard_clear: 12 rows of 3 words of the brick-gap colour (5555h) at DI. */
void hand_5008(V30 *c) {
    AX = CGA_SEG; ES = AX;
    c->df = 0;
    DH = 0x0C;
    AX = 0x5555;
    do {
        CX = 3;
        rep_stosw(c);
        DI = (uint16_t)(DI - 6);
        DI ^= 0x2000;
        if (!(DI & 0x2000)) DI = (uint16_t)(DI + 0x50);
    } while (--DH);
}

/* 4F4A guard_erase / 4F10 guard_draw */
void hand_4f4a(V30 *c) {
    V30_TICK(30);
    if (G8(0x454E)) return;
    DI = g16(c, 0x454C);
    CALL(0x5008);
}
void hand_4f10(V30 *c) {
    G8(0x454E) = 0;
    SI = 0x4500;
    if (G8(0x454A)) {
        BX = g16(c, 0x4551);
        if (G8(0x4553)) { BL &= 2; BL = (uint8_t)(BL + 0x0C); }
        if (G8(0x454A) == 0xFF) BX = (uint16_t)(BX + 0x10);
        SI = g16(c, (uint16_t)(BX + 0x4A60));
    }
    DI = g16(c, 0x45BA);
    sg16(c, 0x454C, DI);
    CALL(0x4FDF);
}

/* 502D guard_touches_flyer: CF=1 if the flying thing (6106) touches the guard. */
void hand_502d(V30 *c) {
    V30_TICK(30);
    if (!G8(0x70F2)) { c->cf = 0; return; }
    AX = g16(c, 0x70F3); DL = G8(0x70F5);
    SI = 0x10;
    BX = g16(c, 0x4548); DH = G8(0x454B);
    DI = 0x18; CX = 0x0C08;
    CALL(0x2E29);
}

/* 4DD0 guard_hits_cat: CF=1 if the guard touches the cat: knocks it down (bottom row: the
 * room ends). */
void hand_4dd0(V30 *c) {
    AX = g16(c, 0x0579); DL = G8(0x057B);
    SI = 0x18; DI = SI;
    BX = g16(c, 0x4548); DH = G8(0x454B);
    CX = 0x0C0E;
    CALL(0x2E29);
    if (!c->cf) return;
    if (g16(c, CUR) == 6) {
        G8(0x0553) = 1;
        CALL(0x11E3);
        CALL(0x4F4A);
        c->cf = 1;
        return;
    }
    CALL(0x11E3);
    CALL(0x4F4A);
    CALL(0x1145);
    G8(0x055B) = 4; G8(0x0571) = 1; G8(0x0576) = 4; G8(0x0578) = 8; G8(0x4553) = 4;
    DL = g16(c, 0x4548) > g16(c, 0x0579) ? 1 : 0xFF;
    G8(0x454A) = DL;
    AX = 0x0CE4; BX = 0x123B;
    CALL(0x593B);
    c->cf = 1;
}

/* 4E75 guard_pushes_piece: a placed piece touching the guard is pushed out again. */
void hand_4e75(V30 *c) {
    CX = 8;
    do {
        BX = (uint16_t)(CX - 1);
        if (!G8((uint16_t)(BX + 0x2B72))) continue;
        uint16_t cx = CX;
        DL = G8((uint16_t)(BX + 0x2B6A));
        BL = (uint8_t)(BL << 1);
        AX = g16(c, (uint16_t)(BX + 0x2B5A));
        SI = 0x18; DI = SI;
        BX = g16(c, 0x4548); DH = G8(0x454B);
        CX = 0x0C0F;
        CALL(0x2E29);
        CX = cx;
        if (!c->cf) continue;
        if (!G8(0x45BE)) { CALL(0x11E3); if (G8(0x70F2)) CALL(0x622B); }
        CALL(0x4F4A);
        CX = cx;
        BX = (uint16_t)(CX - 1);
        G8((uint16_t)(BX + 0x2B72)) = 0;
        DL = G8((uint16_t)(BX + 0x2B6A));
        BL = (uint8_t)(BL << 1);
        CX = g16(c, (uint16_t)(BX + 0x2B5A));
        CALL(0x2CB0);
        DI = AX;
        SI = 0x2B7A;
        AX = CGA_SEG; ES = AX;
        CX = 0x0F03;
        CALL(0x2D9D);
        if (!G8(0x45BE)) { if (G8(0x70F2)) CALL(0x61FA); CALL(0x1145); }
        DX = 0;
        if (g16(c, CUR) != 6) {                           /* the guard waits a while */
            AH = 0;
            INT(0x1A);
            if (DX == 0) DX--;
        }
        sg16(c, 0x454F, DX);
        return;
    } while (--CX);
}

/* 4E3E guards_check_pieces: after a piece was put down, every active guard checks it. */
void hand_4e3e(V30 *c) {
    G8(0x45BE) = 1;
    uint16_t cur = g16(c, CUR);
    sg16(c, CUR, 0);
    do {
        BX = g16(c, CUR);
        CALL(0x4FCD);
        if (!g16(c, 0x454F)) {
            CALL(0x4E75);
            BX = g16(c, CUR);
            CALL(0x4FBB);
        }
        sg16(c, CUR, (uint16_t)(g16(c, CUR) + 1));
    } while (g16(c, CUR) < 7);
    AX = cur;
    sg16(c, CUR, AX);
}

/* 4F59 guards_reset: random x on each row; all but the top one start waiting. */
void hand_4f59(V30 *c) {
    sg16(c, CUR, 0);
    do {
        CALL(0x2DFD);
        DX &= 0x7F;
        DX = (uint16_t)(DX + 0x60);
        sg16(c, 0x4548, DX);
        G8(0x454A) = 0; G8(0x454E) = 1;
        sg16(c, 0x4551, 0);
        G8(0x4553) = 0;
        DX = 0;
        if (g16(c, CUR) == 0) { AH = 0; INT(0x1A); if (DX == 0) DX--; }
        sg16(c, 0x454F, DX);
        BX = g16(c, CUR);
        AL = (uint8_t)(G8((uint16_t)(BX + 0x2BD4)) + 3);
        G8(0x454B) = AL;
        CALL(0x4FBB);
        sg16(c, CUR, (uint16_t)(g16(c, CUR) + 1));
    } while (g16(c, CUR) < 7);
    sg16(c, CUR, 0);
}

/* 4C10 guards_update: one guard per tick (some share ticks). */
void hand_4c10(V30 *c) {
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, 0x45B8)) return;
    sg16(c, CUR, (uint16_t)(g16(c, CUR) + 1));
    BX = g16(c, CUR);
    if (BX == 1 || BX == 4) sg16(c, 0x45B8, DX);
    else if (BX >= 7) { BX = 0; sg16(c, CUR, BX); sg16(c, 0x45B8, DX); }
    CALL(0x4FCD);
    CALL(0x502D);
    if (c->cf) return;
    if (g16(c, 0x454F)) {                                 /* waiting to (re)appear */
        AH = 0;
        INT(0x1A);
        DX = (uint16_t)(DX - g16(c, 0x454F));
        BX = g16(c, 0x0008);
        BL = (uint8_t)(BL << 1);
        AX = g16(c, (uint16_t)(BX + 0x45C7));
        if (g16(c, CUR) == 0) AX = (uint16_t)(AX << 1);
        if (DX < AX) return;
        sg16(c, 0x454F, 0);
        G8(0x454E) = 1;
        AX = g16(c, 0x0579) > 0xA0 ? 0x24 : 0x108;
        sg16(c, 0x4548, AX);
        G8(0x454A) = 0;
    }
    CALL(0x4DD0);
    if (c->cf) { BX = g16(c, CUR); CALL(0x4FBB); return; }
    if (G8(0x4553)) {
        if (--G8(0x4553) == 0) G8(0x454A) = G8(0x454A) == 0xFF ? 1 : 0xFF;
    } else if (!(G8(0x454B) > G8(0x057B))) {              /* the cat is level or below */
        int chase = G8(0x057B) < 0x28 && g16(c, CUR) == 6;
        if (!chase) {
            CALL(0x2DFD);
            BX = g16(c, 0x0008);
            chase = !(DL > G8((uint16_t)(BX + 0x45BF)));
        }
        if (chase) {
            DL = 0;
            AX = g16(c, 0x4548) & 0x0FF8;
            CX = g16(c, 0x0579) & 0x0FF8;
            if (AX != CX) DL = AX < CX ? 1 : 0xFF;
            G8(0x454A) = DL;
            if (G8(0x057B) >= 0x28 && g16(c, CUR) == 6) {
                AL = DL == 0xFF ? 1 : 0xFF;
                G8(0x454A) = AL;
            }
        }
    }
    sg16(c, 0x45BC, G8(0x4553) ? 4 : 8);
    AX = g16(c, 0x4548);
    if (G8(0x454A) == 0) {
        CALL(0x2DFD);
        if (DL <= 0x10) { DL &= 1; if (!DL) DL = 0xFF; G8(0x454A) = DL; }
    } else {
        if (G8(0x454A) == 1) {
            AX = (uint16_t)(AX + g16(c, 0x45BC));
            if (AX >= 0x10B) { AX = 0x10A; G8(0x454A) = 0xFF; G8(0x4553) = 0; }
        } else {
            uint16_t a = AX;
            AX = (uint16_t)(AX - g16(c, 0x45BC));
            if (a < g16(c, 0x45BC) || AX <= 0x24) { AX = 0x25; G8(0x454A) = 1; G8(0x4553) = 0; }
        }
        sg16(c, 0x4548, AX);
        sg16(c, 0x4551, (uint16_t)(g16(c, 0x4551) + 2));
        if (g16(c, 0x4551) >= 0x0C) sg16(c, 0x4551, 0);
        if (!G8(0x4553)) { CALL(0x2DFD); if (DL <= 8) G8(0x454A) = 0; }
    }
    CX = g16(c, 0x4548);
    DL = G8(0x454B);
    CALL(0x2CB0);
    sg16(c, 0x45BA, AX);
    CALL(0x502D);
    if (c->cf) return;
    CALL(0x4DD0);
    if (!c->cf) {
        CALL(0x4F4A);
        CALL(0x4F10);
        G8(0x45BE) = 0;
        CALL(0x4E75);
    }
    BX = g16(c, CUR);
    CALL(0x4FBB);
}
