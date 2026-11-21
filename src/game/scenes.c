/* Alley Cat, hand-written: drawing the alley and the rooms.
 *
 * Most scenery is a "picture list": a word (rows:bytes of each piece) followed by
 * (source, screen offset) pairs, ended by FFFF; draw_list adds AX to every screen offset. */
#include "hand.h"

/* 2B24 draw_list: draw picture list DS:BX at offset AX. */
void hand_2b24(V30 *c) {
    CX = m16(c, DS, BX);
    sg16(c, 0x2AC7, CX);
    sg16(c, 0x2ACC, AX);
    BX += 2;
    for (;;) {
        SI = m16(c, DS, BX);
        CMP16(SI, 0xFFFF);
        if (c->zf) return;
        DI = (uint16_t)(m16(c, DS, (uint16_t)(BX + 2)) + g16(c, 0x2ACC));
        c->df = 0;
        G8(0x2AD0) = CH;
        CH = 0;
        sg16(c, 0x2ACE, CX);
        do {
            CX = g16(c, 0x2ACE);
            rep_movsb(c);
            DI = (uint16_t)(DI - g16(c, 0x2ACE));
            DI ^= 0x2000;
            if (!(DI & 0x2000)) DI = (uint16_t)(DI + 0x50);
        } while (--G8(0x2AD0));
        BX += 4;
        CX = g16(c, 0x2AC7);
    }
}

/* 29E1 vline: a vertical line of 5Fh scan lines of the byte AL from ES:DI down. */
void hand_29e1(V30 *c) {
    CX = 0x5F;
    do {
        w8(c, ES, DI, AL);
        DI ^= 0x2000;
        if (!(DI & 0x2000)) DI = (uint16_t)(DI + 0x50);
        V30_TICK(50);
    } while (--CX);
}

/* 29A0 room_box: the walls of a room at offset AX: outline, floor / ceiling lines, sides. */
void hand_29a0(V30 *c) {
    sg16(c, 0x267E, AX);
    BX = 0x251C;
    CALL(0x2B24);
    AX = 0;
    c->df = 0;
    DI = (uint16_t)(g16(c, 0x267E) + 0x0284); CX = 0x24; rep_stosw(c);
    DI = (uint16_t)(g16(c, 0x267E) + 0x1184); CX = 0x24; rep_stosw(c);
    DI = (uint16_t)(g16(c, 0x267E) + 0x2284); AL = 0x2A; CALL(0x29E1);
    DI = (uint16_t)(g16(c, 0x267E) + 0x22CB); AL = 0xA8; CALL(0x29E1);
}

/* the same picture list repeated at several offsets (tables at 2634) */
static void repeat_list(V30 *c, uint16_t table, uint16_t count) {
    sg16(c, 0x2634, AX);
    SI = count;
    do {
        AX = g16(c, 0x2634);
        BX = g16(c, (uint16_t)(SI + table));
        uint16_t si = SI;
        CALL(0x2B24);
        SI = si;
        SI -= 2;
    } while (SI);
}
void hand_2958(V30 *c) { repeat_list(c, 0x2634, 8); }
void hand_2970(V30 *c) { repeat_list(c, 0x263C, 0x0A); }
void hand_2988(V30 *c) { repeat_list(c, 0x2646, 8); }

/* 2945: two pieces (lists 2384, 238C) at AX */
void hand_2945(V30 *c) {
    sg16(c, 0x2634, AX);
    BX = 0x2384; CALL(0x2B24);
    AX = g16(c, 0x2634);
    BX = 0x238C; CALL(0x2B24);
}

/* 2790 room_draw: draw room [0004] (2 = fishbowl water surface, 7 = the special room). */
void hand_2790(V30 *c) {
    AX = CGA_SEG; ES = AX;
    uint16_t room = g16(c, 0x0004);
    if (room == 2) {
        c->df = 0;
        DI = 0; AX = 0xAAAA; CX = 0x50; rep_stosw(c);
        DI = 0x2000; CX = 0x50; rep_stosw(c);
        sg16(c, 0x2654, 0);
        do {                                             /* a row of random wave tiles */
            do { CALL(0x2DFD); DX &= 0x18; } while (DL == G8(0x2653));
            G8(0x2653) = DL;
            BX = g16(c, 0x2654);
            G8((uint16_t)(BX + 0x2656)) = DL;
            DX = (uint16_t)(DX + 0x2020);
            SI = DX;
            DI = BX;
            DI = (uint16_t)(DI << 1);
            DI = (uint16_t)(DI + 0xA0);
            CX = 0x0401;
            CALL(0x2D9D);
            sg16(c, 0x2654, (uint16_t)(g16(c, 0x2654) + 1));
        } while (g16(c, 0x2654) < 0x28);
        return;
    }
    if (room == 7) { CALL(0x300F); return; }
    if (room == 6) {
        AX = 0; CALL(0x29A0);
        BX = 0x2570; AX = 0x064A; CALL(0x2B24);
        sg16(c, 0x2650, 0x48); G8(0x2652) = 0x38;
        AX = 0x0DD2; CALL(0x2958);
        AX = 0x0DF6; CALL(0x2970);
        SI = 0x1FA0; DI = 0x067E; CX = 0x1002; CALL(0x2D9D);
        BX = 0x2344; AX = 0x0B84; CALL(0x2B24);
        CALL(0x4B47);
        return;
    }
    if (room == 5) {
        AX = 0x0640; CALL(0x29A0);
        BX = 0x2570; AX = 0x0CB6; CALL(0x2B24);
        sg16(c, 0x2650, 0xF8); G8(0x2652) = 0x60;
        AX = 0x140E; CALL(0x2958);
        AX = 0x1434; CALL(0x2970);
        AX = 0x143E; CALL(0x2970);
        AX = 0x16A0; CALL(0x2988);
        BX = 0x2344; AX = 0x1184; CALL(0x2B24);
        SI = 0x1FE0; DI = 0x0DD6; CX = 0x1002; CALL(0x2D9D);
        return;
    }
    if (room == 4) {
        AX = 0x0640; CALL(0x29A0);
        BX = 0x2570; AX = 0x0CBA; CALL(0x2B24);
        sg16(c, 0x2650, 0x108); G8(0x2652) = 0x60;
        AX = 0x1439; CALL(0x2958);
        AX = 0x16C0; CALL(0x2945);
        CALL(0x3F9E);
        return;
    }
    if (room == 3) {
        AX = 0x0640; CALL(0x29A0);
        BX = 0x2570; AX = 0x0C90; CALL(0x2B24);
        sg16(c, 0x2650, 0x60); G8(0x2652) = 0x60;
        AX = 0x140C; CALL(0x2958);
        AX = 0x1418; CALL(0x2970);
        BX = 0x2344; AX = 0x1184; CALL(0x2B24);
        BX = 0x2344; AX = 0x11A2; CALL(0x2B24);
        BX = 0x2624; AX = 0; CALL(0x2B24);
        CALL(0x3BDB);
        return;
    }
    AX = 0x0640; CALL(0x29A0);                           /* room 1 */
    BX = 0x2570; AX = 0x0CA0; CALL(0x2B24);
    sg16(c, 0x2650, 0xA0); G8(0x2652) = 0x60;
    AX = 0x1406; CALL(0x2958);
    BX = 0x2344; AX = 0x11C4; CALL(0x2B24);
    AX = 0x1422; CALL(0x2970);
    AX = 0x1690; CALL(0x2988);
    AX = 0x16B6; CALL(0x2945);
}

/* 2A68 alley_sign: one of four pictures by [6DF8] at 1902. */
void hand_2a68(V30 *c) {
    BX = g16(c, 0x6DF8) & 3;
    BL = (uint8_t)(BL << 1);
    SI = g16(c, (uint16_t)(BX + 0x2AD1));
    DI = 0x1902; CX = 0x0801;
    V30_TICK(60);
    CALL(0x2D9D);
}

/* 2AC6 line_fill: draw 20 patterns of a clothesline at DI (odds BH), filling its item map
 * at 1016 + [2ACA]. */
void hand_2ac6(V30 *c) {
    G8(0x2AC9) = BH;
    G8(0x2AC4) = 0;
    do {
        uint16_t di = DI, es = ES;
        BX = g16(c, 0x0008);
        BL = G8((uint16_t)(BX + 0x2ABA));
        BH = G8(0x2AC9);
        AX = GAME_DS; ES = AX;
        DI = 0x04D7;
        CALL(0x067D);
        ES = es; DI = di;
        SI = 0x04D7; CX = 0x1002;
        CALL(0x2D9D);
        BH = 0;
        BL = G8(0x2AC4);
        CL = BL;
        BL = (uint8_t)(BL >> 2);
        CL = (uint8_t)~CL;
        CL &= 3;
        CL = (uint8_t)(CL << 1);
        AL = (uint8_t)(G8(0x0540) << CL);
        SI = g16(c, 0x2ACA);
        G8((uint16_t)(BX + SI + 0x1016)) |= AL;
        DI = (uint16_t)(di + 4);
        G8(0x2AC4)++;
    } while (G8(0x2AC4) < 0x14);
}

/* 2A80 lines_fill: the three clotheslines, and the scrolling state. */
void hand_2a80(V30 *c) {
    for (BX = 0x0F; BX; BX--) G8((uint16_t)(BX + 0x1015)) = 0;
    DI = 0x0140; BH = 0x80; sg16(c, 0x2ACA, 0x00); CALL(0x2AC6);
    DI = 0x0640; BH = 0x30; sg16(c, 0x2ACA, 0x05); CALL(0x2AC6);
    DI = 0x0B40; BH = 0x00; sg16(c, 0x2ACA, 0x0A); CALL(0x2AC6);
    G8(0x0525) = 0x10;
    sg16(c, 0x052F, 0);
    G8(0x0531) = 1;
}

/* 2B71 window_row: 4 windows from DI, 14h bytes apart. */
void hand_2b71(V30 *c) {
    G8(0x2AC4) = 4;
    do {
        SI = 0x2680; CX = 0x1005;
        uint16_t di = DI;
        CALL(0x2D9D);
        DI = (uint16_t)(di + 0x14);
    } while (--G8(0x2AC4));
}

/* 2B8B windows: three rows of windows */
void hand_2b8b(V30 *c) {
    DI = 0x03C5; CALL(0x2B71);
    DI = 0x08C5; CALL(0x2B71);
    DI = 0x0DC5; CALL(0x2B71);
}

/* 2B9E fence: the fence top (random boards), the fence, random marks and scraps. */
void hand_2b9e(V30 *c) {
    sg16(c, 0x2AC2, 0x103E);
    for (;;) {
        sg16(c, 0x2AC2, (uint16_t)(g16(c, 0x2AC2) + 2));
        DI = g16(c, 0x2AC2);
        if (DI >= 0x1090) break;
        do { CALL(0x2DFD); DX &= 0x30; } while (DL == G8(0x2AC4));
        G8(0x2AC4) = DL;
        DX = (uint16_t)(DX + 0x2904);
        SI = DX; CX = 0x0801;
        CALL(0x2D9D);
    }
    DI = 0x1180; AX = 0x5655; CX = 0x500;
    c->df = 0;
    rep_stosw(c);
    DI = 0x3180; CX = 0x500;
    rep_stosw(c);
    sg16(c, 0x2AC2, 0x2944);
    do {
        G8(0x2AC4) = 9;
        do {
            CALL(0x2DFD);
            DX &= 0x0776;
            DX = (uint16_t)(DX + 0x12C0);
            DI = DX;
            SI = g16(c, 0x2AC2);
            CX = 0x0501;
            CALL(0x2D9D);
        } while (--G8(0x2AC4));
        sg16(c, 0x2AC2, (uint16_t)(g16(c, 0x2AC2) + 0x0A));
    } while (g16(c, 0x2AC2) < 0x296C);
    G8(0x2AC4) = 5;
    do {
        CALL(0x2DFD);
        DX &= 0x3E;
        DX = (uint16_t)(DX + 0x3A98);
        DI = DX;
        SI = 0x296C; CX = 0x0501;
        CALL(0x2D9D);
    } while (--G8(0x2AC4));
}

/* 2C3D trash_can: a can at DI (one ring less from 1720 on). */
void hand_2c3d(V30 *c) {
    sg16(c, 0x2AC2, DI);
    AL = 3;
    if (DI >= 0x1720) AL--;
    G8(0x2AC4) = AL;
    sg16(c, 0x2AC2, (uint16_t)(g16(c, 0x2AC2) + 0x1E0));
    SI = 0x2976; CX = 0x0C05;
    CALL(0x2D9D);
    do {
        DI = g16(c, 0x2AC2);
        sg16(c, 0x2AC2, (uint16_t)(g16(c, 0x2AC2) + 0x140));
        SI = 0x29EE; CX = 0x0804;
        CALL(0x2D9D);
    } while (--G8(0x2AC4));
    DI = g16(c, 0x2AC2);
    SI = 0x2A2E; CX = 0x0B04;
    CALL(0x2D9D);
}

/* 2C84 trash_cans: the cans of this level (list at 2A86 + [2AB2 + level]). */
void hand_2c84(V30 *c) {
    BX = g16(c, 0x0008);
    BL = G8((uint16_t)(BX + 0x2AB2));
    for (;;) {
        sg16(c, 0x2AC5, BX);
        DI = g16(c, (uint16_t)(BX + 0x2A86));
        CMP16(DI, 0);
        if (c->zf) return;
        CALL(0x2C3D);
        BX = (uint16_t)(g16(c, 0x2AC5) + 2);
    }
}

static void alley_base(V30 *c) {
    AX = CGA_SEG; ES = AX;
    c->df = 0;
    DI = 0; AX = 0xAAAA; CX = 0xFA0; rep_stosw(c);
    DI = 0x2000; CX = 0xFA0; rep_stosw(c);
    CALL(0x2B9E);
    BX = 0x28A0; AX = 0;
    CALL(0x2B24);
    CALL(0x2A68);
}

/* 2A00 alley_draw */
void hand_2a00(V30 *c) {
    alley_base(c);
    CALL(0x2C84);
    CALL(0x2B8B);
    CALL(0x2A80);
}

/* 2A30 title_draw: the alley without windows / lines, with level 1's cans. */
void hand_2a30(V30 *c) {
    alley_base(c);
    AX = g16(c, 0x0008);
    uint16_t level = AX;
    sg16(c, 0x0008, 1);
    CALL(0x2C84);
    AX = level;
    sg16(c, 0x0008, AX);
}
