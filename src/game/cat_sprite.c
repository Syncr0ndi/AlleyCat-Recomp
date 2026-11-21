// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
/* Alley Cat, hand-written: drawing the cat and its horizontal movement.
 *
 * The cat is drawn with sprite_and (2D35) at its screen address [055F]; the screen under it
 * is kept at 05FA (size [0561] = rows:words) and put back by cat_erase. [055D] is the sprite
 * data, [0565] its size, [056B] the walk frame, [0572] the speed. */
#include "hand.h"

#define V_SPRITE  0x055D
#define V_SPRSIZE 0x0565
#define V_SAVSIZE 0x0561
#define V_SCRADDR 0x055F
#define V_CAT_X   0x0579

/* 0F87 cat_slide_in: the first frames of entering from the edge: copy the visible part
 * (AL words cut off, AH = side) of the sprite to a buffer at 000E and use that. */
void hand_0f87(V30 *c) {
    CX = 0x0B03;
    CL = (uint8_t)(CL - AL);
    sg16(c, V_SPRSIZE, CX);
    if (AH == 0xFF) {
        AH = 0;
        AL = (uint8_t)(AL << 3);
        AX = (uint16_t)(AX + 0x128);
        sg16(c, V_CAT_X, AX);
    } else {
        AH = 0;
        AL = (uint8_t)(AL << 1);
        sg16(c, V_SPRITE, (uint16_t)(g16(c, V_SPRITE) + AX));
        sg16(c, V_CAT_X, 0);
    }
    ES = DS;
    SI = g16(c, V_SPRITE);
    DI = 0x0E;
    AL = 3;
    V30_TICK(150);
    CALL(0x2D70);
    sg16(c, V_SPRITE, 0x0E);
}

/* 0FC9 cat_move_x: move by the speed in the facing direction, within the room's limits
 * ([05F6]..[05F8]); CF=1 when stopped at a limit. */
void hand_0fc9(V30 *c) {
    sg16(c, 0x05F6, 0x08);
    sg16(c, 0x05F8, 0x123);
    if (g16(c, 0x0004) == 7) { sg16(c, 0x05F6, 0x24); sg16(c, 0x05F8, 0x10F); }
    AX = g16(c, V_CAT_X);
    uint8_t dir = G8(0x056E);
    V30_TICK(120);
    c->cf = 0;
    if (dir == 0) return;
    if (dir == 1) {
        AX = (uint16_t)(AX + g16(c, 0x0572));
        if (AX < g16(c, 0x05F8)) { sg16(c, V_CAT_X, AX); return; }
        AX = (uint16_t)(g16(c, 0x05F8) - 1);
        sg16(c, V_CAT_X, AX);
        c->cf = 1;
        return;
    }
    uint16_t x = AX;
    AX = (uint16_t)(AX - g16(c, 0x0572));
    if (x < g16(c, 0x0572) || AX < g16(c, 0x05F6)) {
        AX = g16(c, 0x05F6);
        sg16(c, V_CAT_X, AX);
        c->cf = 1;
        return;
    }
    sg16(c, V_CAT_X, AX);
}

/* 1020 cat_walk_frame: speed up while walking the same way, advance the walk animation;
 * BX = sprite of the frame. */
void hand_1020(V30 *c) {
    AL = G8(0x056E);
    if (AL != G8(0x056F)) sg16(c, 0x0572, 2);
    if (g16(c, 0x0572) < 8) {
        AL = --G8(0x0577);
        AL &= 3;
        if (!AL) sg16(c, 0x0572, (uint16_t)(g16(c, 0x0572) + 1));
    }
    BL = (uint8_t)(G8(0x056B) + 1);
    if (BL >= 6) BL = 0;
    G8(0x056B) = BL;
    if (G8(0x056E) == 0xFF) BL = (uint8_t)(BL + 6);
    BL = (uint8_t)(BL << 1);
    BH = 0;
    c->cf = 0;
    BX = g16(c, (uint16_t)(BX + 0x0F7A));
    V30_TICK(150);
}

/* 1069 cat_sit: standing still: every 8th call draw a random sitting pose (body and
 * tail chosen separately). */
void hand_1069(V30 *c) {
    sg16(c, 0x0572, 2);
    G8(0x0577) = 8;
    V30_TICK(60);
    if (g16(c, V_SAVSIZE) == 0x0C02) {
        G8(0x056D)++;
        if (G8(0x056D) & 7) return;
    }
    CALL(0x11E3);
    CALL(0x1B7A);
    if (c->cf) return;
    CALL(0x20F5);
    if (c->cf) return;
    CALL(0x2DFD);
    BL = DL;
    BX &= 0x0E;
    SI = g16(c, (uint16_t)(BX + 0x0F92));
    AX = CGA_SEG; ES = AX;
    DI = g16(c, V_SCRADDR);
    BP = 0x05FA;
    sg16(c, V_SAVSIZE, 0x0C02);
    CX = 0x0602;
    CALL(0x2D35);
    CALL(0x2DFD);
    BL = DL;
    BX &= 0x06;
    SI = g16(c, (uint16_t)(BX + 0x0FA2));
    DI = (uint16_t)(g16(c, V_SCRADDR) + 0xF0);
    BP = 0x0612;
    CX = 0x0602;
    CALL(0x2D35);
    G8(0x0583) = 0;
}

/* 10DD cat_drop: start falling straight down (from a hole: [0550] = 2). */
void hand_10dd(V30 *c) {
    G8(0x055C) = 0; G8(0x0571) = 1; G8(0x0576) = 2; G8(0x0578) = 1; G8(0x0577) = 0xFF; G8(0x056E) = 0;
    G8(0x055A) = 1;
    AX = g16(c, 0x0FAC); sg16(c, 0x0569, AX);
    AX = g16(c, 0x0FB8); sg16(c, 0x0567, AX);
    G8(0x0550) = 2;
    V30_TICK(180);
}

/* 1112 cat_save_at: compute the cat's screen address and save the screen under it. */
void hand_1112(V30 *c) {
    CX = g16(c, V_CAT_X);
    DL = G8(0x057B);
    CALL(0x2CB0);
    sg16(c, V_SCRADDR, AX);
    CALL(0x1124);
}

/* 1124 cat_save_bg: save the screen under the cat to 05FA. */
void hand_1124(V30 *c) {
    AX = GAME_DS; ES = AX;
    DI = 0x05FA;
    uint16_t ds = DS;
    SI = g16(c, V_SCRADDR);
    AX = CGA_SEG; DS = AX;
    CX = m16(c, ES, V_SAVSIZE);
    CALL(0x2DCA);
    DS = ds;
    G8(0x0583) = 0;
    V30_TICK(80);
}

/* 1145 cat_draw: draw sprite [055D] (size [0565]) at the cat's address, saving the screen. */
void hand_1145(V30 *c) {
    AX = CGA_SEG; ES = AX;
    DI = g16(c, V_SCRADDR);
    BP = 0x05FA;
    SI = g16(c, V_SPRITE);
    CX = g16(c, V_SPRSIZE);
    sg16(c, V_SAVSIZE, CX);
    G8(0x0583) = 0;
    V30_TICK(90);
    CALL(0x2D35);
}

/* 1166 cat_splash: draw the splash at the cat, play the splash sound for 10 ticks, remove it. */
void hand_1166(V30 *c) {
    DL = G8(0x057B);
    CX = g16(c, V_CAT_X);
    if (CX < 0x0C) CX = 0; else CX -= 0x0C;
    if (CX >= 0x10F) CX = 0x10E;
    CALL(0x2CB0);
    sg16(c, 0x0581, AX);
    DI = AX;
    AX = CGA_SEG; ES = AX;
    BP = 0x0E; SI = 0x1679; CX = 0x1205;
    CALL(0x2CCC);
    AH = 0;
    INT(0x1A);
    sg16(c, 0x057F, DX);
    sg16(c, 0x5A3C, 0);
    sg16(c, 0x5A3E, 0);
    do {
        CALL(0x5A1C);
        AH = 0;
        INT(0x1A);
        DX = (uint16_t)(DX - g16(c, 0x057F));
    } while (DX < 0x0A);
    CALL(0x5B21);
    DI = g16(c, 0x0581);
    SI = 0x0E; CX = 0x1205;
    G8(0x0583) = 0;
    CALL(0x2D9D);
    if (G8(0x1678) && G8(0x1F80)) G8(0x1F80)--;
}

/* 11E3 cat_erase: put back the screen under the cat. */
void hand_11e3(V30 *c) {
    AX = CGA_SEG; ES = AX;
    DI = g16(c, V_SCRADDR);
    SI = 0x05FA;
    CX = g16(c, V_SAVSIZE);
    V30_TICK(70);
    CALL(0x2D9D);
}
