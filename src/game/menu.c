/* Alley Cat, hand-written: title screen, menus, pause, BIOS text, random seed.
 *
 * Text goes through the BIOS (INT 10h AH=02h cursor, AH=0Eh teletype). The menu lines are a
 * list of (cursor, text) pairs at 6D63 / 6D37, [6D8F] = next line. [0693] counts key presses;
 * waiting for a key means waiting for it to change. */
#include "hand.h"

#define KEYCOUNT 0x0693
#define KEY(i) G8(0x06B7 + (i))

/* 2E10 seed_random: random seed from the timer counter. */
void hand_2e10(V30 *c) {
    AL = 0;
    v30_out(c, 0x43, AL, 0);
    AL = (uint8_t)v30_in(c, 0x40, 0);
    AH = AL;
    AL = (uint8_t)v30_in(c, 0x40, 0);
    CMP16(AX, 0);
    if (c->zf) AX = 0xFA59;
    sg16(c, 0x2AE5, AX);
    V30_TICK(80);
}

/* 5E2B print: the zero-ended text at DS:SI in colour 2. */
void hand_5e2b(V30 *c) {
    int16_t d = c->df ? -1 : 1;
    for (;;) {
        AL = m8(c, DS, SI); SI = (uint16_t)(SI + d);
        CMP8(AL, 0);
        if (c->zf) return;
        uint16_t si = SI;
        BL = 2;
        AH = 0x0E;
        INT(0x10);
        SI = si;
    }
}

/* 5E5B cursor: to row DH, column 0. */
void hand_5e5b(V30 *c) {
    DL = 0;
    BH = DL;
    AH = 2;
    INT(0x10);
}

/* 5FB1 menu_line: print the next menu line. */
void hand_5fb1(V30 *c) {
    BX = g16(c, 0x6D8F);
    DX = g16(c, (uint16_t)(BX + 0x6D63));
    CALL(0x5E5B);
    BX = g16(c, 0x6D8F);
    sg16(c, 0x6D8F, (uint16_t)(g16(c, 0x6D8F) + 2));
    SI = g16(c, (uint16_t)(BX + 0x6D37));
    CALL(0x5E2B);
}

/* 5FCD cls: clear CGA memory. */
void hand_5fcd(V30 *c) {
    c->df = 0;
    AX = CGA_SEG; ES = AX;
    AX = 0;
    DI = AX;
    CX = 0xFA0; rep_stosw(c);
    DI = 0x2000;
    CX = 0xFA0; rep_stosw(c);
}

static void wait_key(V30 *c) {
    AX = g16(c, KEYCOUNT);
    do idle(c, 40); while (AX == g16(c, KEYCOUNT));
}

/* 5F97 wait_any: a key (or the joystick button, in joystick mode). */
void hand_5f97(V30 *c) {
    if (G8(0x069B)) {
        do {
            DX = 0x201;
            AL = (uint8_t)(v30_in(c, DX, 0) & 0x10);
            idle(c, 30);
        } while (AL);
        return;
    }
    wait_key(c);
}

/* 600F joystick_present: CF=0 if an axis of the game port settles within 18 ticks. */
void hand_600f(V30 *c) {
    DX = 0x201;
    v30_out(c, DX, AL, 0);
    AH = 0;
    INT(0x1A);
    sg16(c, 0x6DFA, DX);
    for (;;) {
        DX = 0x201;
        AL = (uint8_t)v30_in(c, DX, 0);
        TEST8(AL, 3);
        if (c->zf) { c->cf = 0; return; }
        AH = 0;
        INT(0x1A);
        DX = (uint16_t)(DX - g16(c, 0x6DFA));
        if (DX >= 0x12) { c->cf = 1; return; }
    }
}

/* 5FE5 joystick_check: CF=1 (after a message and a key) if there is no usable joystick. */
void hand_5fe5(V30 *c) {
    INT(0x11);
    if (AX & 0x1000) {
        CALL(0x600F);
        if (!c->cf) return;
        CALL(0x600F);
        if (!c->cf) return;
    }
    sg16(c, 0x6D8F, 0x24);
    CX = 4;
    do { CALL(0x5FB1); } while (--CX);
    wait_key(c);
    c->cf = 1;
}

/* 5EE5 menu: joystick? skill level? instructions; [069B] joystick, [6DF8] skill 0-3. */
void hand_5ee5(V30 *c) {
    CALL(0x5B21);
    for (;;) {
        CALL(0x5FCD);
        sg16(c, 0x6D8F, 0);
        CALL(0x5FB1);
        for (;;) {
            wait_key(c);
            if (!(KEY(10) & 0x80)) {                      /* Y */
                CALL(0x5FE5);
                if (c->cf) break;
                G8(0x069B) = 1;
                goto skill;
            }
            if (!(KEY(11) & 0x80)) { G8(0x069B) = 0; goto skill; }   /* N */
        }
    }
skill:
    CX = 5;
    do { uint16_t cx = CX; CALL(0x5FB1); CX = cx; } while (--CX);
    for (;;) {
        wait_key(c);
        AX = 0;
        if (!(KEY(12) & 0x80)) break;                     /* K */
        AX++;
        if (!(KEY(13) & 0x80)) break;                     /* H */
        AX++;
        if (!(KEY(14) & 0x80)) break;                     /* T */
        AX++;
        if (!(KEY(15) & 0x80)) break;                     /* A */
    }
    sg16(c, 0x6DF8, AX);
    CX = 5;
    do { uint16_t cx = CX; CALL(0x5FB1); CX = cx; } while (--CX);
    if (G8(0x069B)) {
        sg16(c, 0x6D8F, 0x20); CALL(0x5FB1); CALL(0x5FB1);
        sg16(c, 0x6D8F, 0x18); CALL(0x5FB1); CALL(0x5FB1);
    } else {
        sg16(c, 0x6D8F, 0x1C); CALL(0x5FB1); CALL(0x5FB1);
        sg16(c, 0x6D8F, 0x16); CALL(0x5FB1);
    }
    CALL(0x5F97);
}

/* 5E70 pause: "paws" message over the alley, wait for a key, put the picture and the clock
 * back (so the pause does not count). */
void hand_5e70(V30 *c) {
    CALL(0x5B21);
    AH = 0;
    INT(0x1A);
    sg16(c, 0x6DFC, DX);
    sg16(c, 0x6DFE, CX);
    uint16_t ds = DS;
    ES = DS;
    AX = CGA_SEG; DS = AX;
    SI = 0x0DCA; DI = 0x0E; CX = 0x1020;
    CALL(0x2DCA);
    DS = ds;
    DX = 0x0B05; BH = 0; AH = 2;
    INT(0x10);
    SI = 0x6D91;
    c->df = 0;
    CALL(0x5E2B);
    DX = 0x0C05; BH = 0; AH = 2;
    INT(0x10);
    SI = G8(0x069B) ? 0x6DD3 : 0x6DB2;
    c->df = 0;
    CALL(0x5E2B);
    CALL(0x5F97);
    AX = CGA_SEG; ES = AX;
    SI = 0x0E; DI = 0x0DCA; CX = 0x1020;
    CALL(0x2D9D);
    AH = 1;
    CX = g16(c, 0x6DFE);
    DX = g16(c, 0x6DFC);
    INT(0x1A);
    AX = g16(c, KEYCOUNT);
    sg16(c, 0x6E00, AX);
}

/* 5E3B title_flash: alternate the two "press a key" pictures. */
void hand_5e3b(V30 *c) {
    AX = CGA_SEG; ES = AX;
    sg16(c, 0x6A8D, (uint16_t)(g16(c, 0x6A8D) + 2));
    BX = g16(c, 0x6A8D) & 2;
    SI = g16(c, (uint16_t)(BX + 0x6A8F));
    CX = 0x0C0A; DI = 0x1D38;
    CALL(0x2D9D);
}

/* 5DD4 title_cat: the cat on the title screen walks about by itself. */
void hand_5dd4(V30 *c) {
    if (!(g16(c, 0x0579) > 0x20)) G8(0x0698) = 1;
    else if (g16(c, 0x0579) >= 0x120) G8(0x0698) = 0xFF;
    else {
        AH = 0;
        INT(0x1A);
        AX = (uint16_t)(DX - g16(c, 0x6A88));
        if (AX >= 0x12) {
            sg16(c, 0x6A88, DX);
            CALL(0x2DFD);
            G8(0x0698) = 0;
            if (DL <= 0xA0) { DL &= 1; if (!DL) DL = 0xFF; G8(0x0698) = DL; }
        }
    }
    CALL(0x13D8);
    if (c->zf) return;
    sg16(c, 0x0572, 4);
    CALL(0x08E5);
}

/* 5CB0 title: draw the title screen and run it (tune, walking cat) until a key (or the
 * joystick button) is pressed or the demo time is up. */
void hand_5cb0(V30 *c) {
    c->df = 0;
    sg16(c, 0x0004, 0);
    CALL(0x1830);
    CALL(0x2330);
    CALL(0x2A30);
    AX = CGA_SEG; ES = AX;
    static const uint16_t pics[6][3] = {{0x6152, 0x1D0B, 0x00BD}, {0x63D0, 0x160E, 0x069E}, {0x6638, 0x0C03, 0x0A78},
                                        {0x6680, 0x080E, 0x0CA8}, {0x6760, 0x0B0C, 0x1D6E}, {0x6868, 0x0804, 0x1DEC}};
    for (int i = 0; i < 6; i++) { SI = pics[i][0]; CX = pics[i][1]; DI = pics[i][2]; CALL(0x2D9D); }
    sg16(c, 0x6A8D, 0);
    CALL(0x5E3B);
    sg16(c, 0x0579, 0);
    CALL(0x070D);
    G8(0x057B) = 0x60;
    G8(0x057C) = 0x92;
    CALL(0x26F2);
    CALL(0x26FC);
    G8(0x1F80) = 9;
    G8(0x1F81) = 0xFF;
    CALL(0x26B3);
    CALL(0x1E40);
    G8(0x0698) = 0; G8(0x0699) = 0; G8(0x6A8A) = 0;
    AX = g16(c, KEYCOUNT);
    sg16(c, 0x6150, AX);
    for (;;) {
        AH = 0;
        INT(0x1A);
        sg16(c, 0x6A8B, DX);
        sg16(c, 0x5322, DX);
        sg16(c, 0x6A93, DX);
        DX = (uint16_t)(DX - 0x30);
        sg16(c, 0x6A88, DX);
        sg16(c, 0x5320, 0);
        for (;;) {
            AH = 0;
            INT(0x1A);
            AX = (uint16_t)(DX - g16(c, 0x6A93));
            if (AX >= 0x24) {
                sg16(c, 0x6A93, DX);
                uint16_t dx = DX;
                CALL(0x5E3B);
                DX = dx;
            }
            DX = (uint16_t)(DX - g16(c, 0x6A8B));
            AX = g16(c, 0x56DA);
            if (G8(0x041A)) {
                AX = (uint16_t)(AX + 0x48);
                if (DX >= AX) break;                          /* restart the tune */
            } else {
                AX = (uint16_t)(AX + 6);
                if (DX > AX) return;                          /* first time: on to the menu */
            }
            CALL(0x53B0);
            CALL(0x5DD4);
            if (G8(0x069B)) {
                DX = 0x201;
                AL = (uint8_t)(v30_in(c, DX, 0) & 0x10);
                if (AL) G8(0x6A8A) = 1;
                else if (G8(0x6A8A)) return;                  /* button pressed */
            }
            AX = g16(c, 0x6150);
            if (AX != g16(c, KEYCOUNT)) return;
        }
    }
}
