/* Alley Cat, hand-written: the cat's position and motion state.
 *
 * Cat variables (data segment): x [0579] (word, pixels), y [057B] (byte), y + 50 [057C],
 * screen address [055F], direction [056E] (1 right, FF left), vertical direction [0571],
 * motion kind [055B], speed / step counters [0572] [0576] [0578], sprite [0561]. */
#include "hand.h"

#define V_CAT_X   0x0579
#define V_CAT_Y   0x057B
#define V_CAT_Y2  0x057C
#define V_SCRADDR 0x055F

/* the flags cleared whenever the cat is (re)placed */
static void clear_cat_flags(V30 *c) {
    G8(0x0550) = 0; G8(0x055C) = 0; G8(0x055A) = 0; G8(0x0583) = 0; G8(0x0698) = 0; G8(0x0699) = 0;
    G8(0x0551) = 0; G8(0x0584) = 0; G8(0x0552) = 0; sg16(c, 0x0554, 0); G8(0x0553) = 0; G8(0x127C) = 0;
}

/* 070D cat_start_alley: put the cat on the ground at the left or right edge (whichever is
 * nearer), facing inwards, and draw it. */
void hand_070d(V30 *c) {
    CX = 0; AH = 1;
    if (g16(c, V_CAT_X) >= 0xA0) { CX = 0x128; AH = 0xFF; }
    G8(0x056E) = AH;
    G8(0x0558) = 3; G8(0x0559) = 0x0C;
    DL = 0xB4;
    sg16(c, V_CAT_X, CX);
    G8(V_CAT_Y) = DL;
    G8(V_CAT_Y2) = 0xE6;
    V30_TICK(120);
    CALL(0x2CB0);
    sg16(c, V_SCRADDR, AX);
    sg16(c, 0x0561, 0x0B03);
    CALL(0x1124);
    G8(0x0571) = 0;
    sg16(c, 0x0572, 2);
    G8(0x0576) = 1;
    G8(0x055B) = 0;
    clear_cat_flags(c);
    V30_TICK(250);
    CALL(0x0700);
}

/* 07A1 cat_place: put the cat where it was in the alley ([0001]/[0003], state 0) or at the
 * entry point of room state [0004], dropping in, and draw it. */
void hand_07a1(V30 *c) {
    BX = g16(c, 0x0004);
    if (BX == 0) {
        CX = g16(c, 0x0001);
        DL = G8(0x0003);
    } else {
        DL = G8((uint16_t)(BX + 0x05E9));
        BL = (uint8_t)(BL << 1);
        CX = g16(c, (uint16_t)(BX + 0x05D9));
    }
    sg16(c, V_CAT_X, CX);
    G8(V_CAT_Y) = DL;
    AL = (uint8_t)(DL + 0x32);
    G8(V_CAT_Y2) = AL;
    V30_TICK(100);
    CALL(0x2CB0);
    sg16(c, V_SCRADDR, AX);
    AX = g16(c, 0x0FB2);
    sg16(c, 0x0569, AX);
    AX = g16(c, 0x0FBE);
    sg16(c, 0x0567, AX);
    sg16(c, 0x0561, AX);
    CALL(0x1124);
    G8(0x0571) = 1;
    G8(0x056E) = 0;
    G8(0x0576) = 1;
    G8(0x0578) = 0x40;
    AL = g16(c, 0x0004) == 7 ? 0 : 0x0A;
    G8(0x055B) = AL;
    clear_cat_flags(c);
    V30_TICK(250);
    CALL(0x0700);
    if (g16(c, 0x0004) == 2) {
        G8(0x0576) = 0x10;
        sg16(c, 0x0574, 0x10);
        AH = 0;
        INT(0x1A);
        sg16(c, 0x05F1, DX);
        G8(0x05F3) = 0; G8(0x05F4) = 5; G8(0x05F5) = 1;
    }
}

/* 0872 cat_knocked: throw the cat away from the point [2650]/[2652] (unless already
 * thrown): direction away from it, strength falling with distance. */
void hand_0872(V30 *c) {
    sg16(c, 0x592A, 0x0400);
    V30_TICK(40);
    if (G8(0x0584)) return;
    G8(0x0576) = 8;
    DL = G8(V_CAT_Y) < G8(0x2652) ? 1 : 0xFF;
    G8(0x0571) = DL;
    uint16_t x = g16(c, V_CAT_X), px = g16(c, 0x2650);
    AX = (uint16_t)(x - px);
    DL = 0xFF;
    if (x <= px) { DL = 1; AX = (uint16_t)~AX; }        /* JA not taken: x <= px */
    G8(0x056E) = DL;
    if (AH) AX = 0xFF;
    AL = (uint8_t)~AL;                                   /* nearer -> stronger */
    if (AL < 0x30) AL = 0x30;
    BL = (uint8_t)(AL >> 2);
    AL = (uint8_t)(AL - BL);
    G8(0x0578) = AL;
    CL = 5;
    AL = (uint8_t)(AL >> 5);
    sg16(c, 0x0572, AX);
    G8(0x055C) = 0; G8(0x39E0) = 0; G8(0x0577) = 1; G8(0x055B) = 0x10; G8(0x0584) = 1;
    V30_TICK(300);
}
