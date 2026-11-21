// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
/* Alley Cat, hand-written: 08E5 cat_update, the cat's per-step movement.
 *
 * Runs at most once per BIOS tick (every [0684] calls otherwise). In the fishbowl room
 * (state 2) the cat swims: depth sets the background colour, the air timer [05F1] runs out,
 * the arrow keys ([0698] horizontal, [0699] vertical) accelerate it. Everywhere else it
 * walks, jumps ([055C]), falls ([0571] = 1 down, FF up, [0576] speed), rides lines and can be
 * thrown ([0584]). Labels keep the original addresses so the control flow can be compared
 * with the listing. */
#include "hand.h"

static void wait_retrace_then_delay(V30 *c) {
    do CALL(0x13D8); while (!c->zf);         /* until retrace ends */
    do CALL(0x13D8); while (c->zf);          /* until the next one starts */
    V30_TICK(CX * 17);                       /* LOOP $ */
    CX = 0;
}

/* sprite selection, drawing and redraw for the swimming cat */
static void swim_draw(V30 *c) {
    AX = g16(c, (uint16_t)(BX + 0x09A6)); sg16(c, 0x055D, AX);
    AX = g16(c, (uint16_t)(BX + 0x09C0)); sg16(c, 0x0565, AX);
    AL = 0x30; CX = 0x2BC;
    CMP8(G8(0x0697), 0xFD);
    int wait = 1;
    if (c->cf) wait = 0;
    else if (!c->zf) { AL = 8; CX = 0x3E8; }
    if (wait && !(G8(0x057B) > AL)) wait_retrace_then_delay(c);
    CALL(0x11E3);
    AX = g16(c, 0x0563); sg16(c, 0x055F, AX);
    CALL(0x1145);
    CALL(0x34A0);
    if (c->cf) CALL(0x1145);
}

static void swim(V30 *c) {
    SI = (uint16_t)(g16(c, 0x0008) << 1);
    AX = (uint16_t)(g16(c, 0x057D) - g16(c, 0x05F1));          /* time under water */
    if (AX >= g16(c, (uint16_t)(SI + 0x0589))) {
        /* out of air */
        if (AX >= g16(c, (uint16_t)(SI + 0x0599))) G8(0x0552) = 1;
        if (--G8(0x05F5) == 0) {
            CALL(0x597F);
            G8(0x05F5) = 6;
            AL = G8(0x05F4);
            if (G8(0x057B) >= 0xB3 && AL < 0xC8) { AL = (uint8_t)(AL + 0x1E); G8(0x05F4) = AL; }
            DL = G8(0x057B);
            uint8_t d = DL;
            DL = (uint8_t)(DL - AL);
            if (d < AL) DL = 0;
            CX = g16(c, 0x0579);
            DL &= 0xF8;
            CALL(0x2CB0);
            DI = AX; SI = 0x064E;
            AX = CGA_SEG; ES = AX;
            BP = 0x0E; CX = 0x0503;
            CALL(0x2D35);
        }
        G8(0x056E) = 0; G8(0x0571) = 1; G8(0x05F3) = 1; G8(0x0576) = 0x20;
        BX = 0; AH = 0x0B; INT(0x10);
    } else {
        SI = (uint16_t)(g16(c, 0x0008) << 1);
        BX = 0;                                                    /* background by depth of breath */
        if (AX >= g16(c, (uint16_t)(SI + 0x05A9))) {
            BL++;
            if (AX >= g16(c, (uint16_t)(SI + 0x05B9))) {
                BL = 5;
                if (AX >= g16(c, (uint16_t)(SI + 0x05C9))) BL--;
            }
        }
        AH = 0x0B; INT(0x10);
        G8(0x056F) = G8(0x056E);
        G8(0x0570) = G8(0x0571);
        /* horizontal: accelerate while the key is held the same way */
        AL = G8(0x0698);
        int reset = 0;
        if (AL == 0) {
            if (g16(c, 0x0574) < 0x10) reset = 1;
            else sg16(c, 0x0574, (uint16_t)(g16(c, 0x0574) - 1));
        } else if (AL != G8(0x056E)) reset = 1;
        else if (g16(c, 0x0574) < 0x30) sg16(c, 0x0574, (uint16_t)(g16(c, 0x0574) + 3));
        if (reset) { G8(0x056E) = AL; sg16(c, 0x0574, 0x20); }
        AX = (uint16_t)(g16(c, 0x0574) >> 3);
        CL = 3;
        BX = g16(c, 0x0008);
        BL = (uint8_t)(BL << 1);
        if (AX > g16(c, (uint16_t)(BX + 0x066C))) AX = g16(c, (uint16_t)(BX + 0x066C));
        sg16(c, 0x0572, AX);
        CALL(0x0FC9);
        /* vertical */
        AL = G8(0x0699);
        reset = 0;
        if (AL == 0) {
            AL = (uint8_t)~AL;
            if (G8(0x0576) < 0x10) reset = 1;
            else G8(0x0576)--;
        } else if (AL != G8(0x0571)) reset = 1;
        else if (G8(0x0576) < 0x40) G8(0x0576) += 4;
        if (reset) { G8(0x0571) = AL; G8(0x0576) = 0x20; }
    }
    /* 0A86: move vertically */
    SI = g16(c, 0x0008);
    DL = G8(0x057B);
    CL = 4;
    BL = (uint8_t)(G8(0x0576) >> 4);
    if (BL > G8((uint16_t)(SI + 0x067C))) BL = G8((uint16_t)(SI + 0x067C));
    AL = G8(0x0571);
    if (AL == 1) {
        DL = (uint8_t)(DL + BL);
        if (DL >= 0xB4) DL = 0xB3;
    } else if (AL > 1) {
        uint8_t d = DL;
        DL = (uint8_t)(DL - BL);
        if (d < BL || DL <= 3) {                                   /* at the surface: breathe */
            AX = g16(c, 0x09B8);
            if (AX == g16(c, 0x055D)) { AX = g16(c, 0x057D); sg16(c, 0x05F1, AX); }
            DL = 2;
        }
    }
    G8(0x057B) = DL;
    CX = g16(c, 0x0579);
    CALL(0x2CB0);
    sg16(c, 0x0563, AX);
    if (G8(0x05F3)) { BX = 0x10; swim_draw(c); return; }
    if (G8(0x056E) != G8(0x056F) || G8(0x0571) != G8(0x0570)) { AL = G8(0x0571); BX = 0x18; swim_draw(c); return; }
    AL = G8(0x0571);
    sg16(c, 0x0587, (uint16_t)(g16(c, 0x0587) + 1));               /* animation frame */
    BX = g16(c, 0x0587);
    AL = G8(0x0698) | G8(0x0699);
    if (!AL) BL = (uint8_t)(BL >> 1);
    int sideways;
    if (G8(0x057B) >= 0xB3 && G8(0x0571) == 1) sideways = 1;
    else if (G8(0x057B) <= 4 && G8(0x0699)) sideways = 0;
    else {
        AL = G8(0x0576); AH = 0; AX >>= 1;
        sideways = AX < g16(c, 0x0574);
    }
    if (sideways && G8(0x056E)) {
        BX &= 6;
        if (G8(0x056E) != 1) BL |= 8;
    } else {
        BX &= 2;
        BL |= 0x10;
        if (G8(0x0571) == 1) BL = (uint8_t)(BL + 4);
    }
    swim_draw(c);
}

/* jump / fall step and landing (0C6A-0E22) */
static void fall_or_jump(V30 *c) {
    CALL(0x0FC9);
    if (c->cf) {                                         /* hit something sideways: drop */
        G8(0x056E) = 0; G8(0x0576) = 2; G8(0x0571) = 1; G8(0x055B) = 0;
    } else {
        AL = G8(0x0578);
        uint8_t a = G8(0x0577);
        G8(0x0577) = (uint8_t)(a - AL);
        if (a < AL) {                                    /* gravity */
            if (G8(0x0571) == 1) { if (G8(0x0576) < 4) G8(0x0576)++; }
            else if (G8(0x0576) <= 1) G8(0x0571) = 1;
            else G8(0x0576)--;
        }
    }
    /* 0CC1: landing on something? */
    int landed = 0;
    if (!G8(0x055A)) {
        int test = 1;
        if (G8(0x055B)) test = --G8(0x055B) == 0;
        if (test && G8(0x0571) == 1) {
            CALL(0x1608);
            if (c->cf) { AL = G8(0x057C); landed = 1; }
        }
    }
    if (!landed) {
        AL = G8(0x057C);
        if (G8(0x0571) != 1) {
            uint8_t a = AL;
            AL = (uint8_t)(AL - G8(0x0576));
            if (a < G8(0x0576)) { AL = 0; G8(0x0571) = 1; G8(0x0576) = 1; }
            goto store;
        }
        AL = (uint8_t)(AL + G8(0x0576));
        if (AL <= 0xE6) goto store;
        if (g16(c, 0x0004) == 7) {
            if (AL >= 0xF8) { AL = 0xF8; G8(0x0551) = 1; }
            goto store;
        }
        AL = 0xE6;
        G8(0x0550) = 0;
    }
    /* 0D29: standing */
    G8(0x0571) = 0; G8(0x0584) = 0; sg16(c, 0x0572, 2); G8(0x055B) = 0; G8(0x055A) = 0;
    if (G8(0x055C)) { uint16_t ax = AX; CALL(0x5AC2); AX = ax; }
store:
    G8(0x057C) = AL;
    if (AL < 0x32) AL = 0; else AL = (uint8_t)(AL - 0x32);
    G8(0x057B) = AL;
    DL = G8(0x057B);
    CX = g16(c, 0x0579);
    CALL(0x2CB0);
    sg16(c, 0x0563, AX);
    if (!G8(0x0583)) CALL(0x11E3);
    CALL(0x1B7A);
    if (c->cf) { G8(0x0583) = 1; return; }
    CALL(0x20F5);
    if (c->cf) { G8(0x0583) = 1; return; }
    AX = g16(c, 0x0563); sg16(c, 0x055F, AX);
    if (G8(0x0584)) {                                    /* tumbling */
        sg16(c, 0x0585, (uint16_t)(g16(c, 0x0585) + 2));
        BX = g16(c, 0x0585) & 0x0E;
        AX = g16(c, (uint16_t)(BX + 0x0FC2));
        BX = g16(c, (uint16_t)(BX + 0x0FD2));
    } else {
        AX = g16(c, 0x0569);
        BX = g16(c, 0x0567);
    }
    sg16(c, 0x055D, AX);
    sg16(c, 0x0565, BX);
    uint8_t y2 = G8(0x057C);
    AL = (uint8_t)(0x32 - y2);
    if (y2 < 0x32) {                                     /* partly above the screen: clip */
        V30_TICK(0x168 * 17);
        CX = 0;
        uint8_t bh = BH;
        BH = (uint8_t)(BH - AL);
        if (BH == 0 || bh < AL) { G8(0x0583) = 1; return; }
        sg16(c, 0x0565, BX);
        AH = (uint8_t)(BL << 1);
        AX = (uint16_t)(AL * AH);
        AX = (uint16_t)(AX + g16(c, 0x0569));
        sg16(c, 0x055D, AX);
        CALL(0x1145);
        return;
    }
    /* 0DDE: sinking into the ground (room 7) or a hole (state [0550] = 2) */
    if (g16(c, 0x0004) == 7) {
        uint8_t y = G8(0x057B);
        AL = (uint8_t)(y - 0xBB);
        if (y < 0xBB) { CALL(0x1145); return; }
    } else {
        if (G8(0x0550) != 2) { CALL(0x1145); return; }
        uint8_t y = G8(0x057B);
        AL = (uint8_t)(y - 0x5E);
        if (y < 0x5E) { CALL(0x1145); return; }
    }
    {
        uint8_t bh = BH;
        BH = (uint8_t)(BH - AL);
        if (BH == 0 || bh < AL) {
            if (g16(c, 0x0004) == 7) { G8(0x0551) = 1; return; }
            CALL(0x070D);
            CALL(0x59CB);
            return;
        }
        sg16(c, 0x0565, BX);
        G8(0x0576) = 2;
    }
    CALL(0x1145);
}

/* 0E78-0F33: start moving according to the keys ([0698], [0699]) */
static int start_move(V30 *c) {
    G8(0x056F) = G8(0x056E);
    AL = G8(0x0698); G8(0x056E) = AL;
    AL = G8(0x0699); G8(0x0571) = AL;
    if (AL == 0) return 0;                               /* -> 0F34 */
    if (G8(0x0571) == 1) {
        if (G8(0x057B) >= 0xB4) { G8(0x0571) = 0; G8(0x0584) = 0; G8(0x0699) = 0; return 0; }
        goto jump_down;
    }
    /* 0EC9: jump up, strength from the run-up [0572] */
    G8(0x055B) = 0;
    AX = g16(c, 0x0572);
    BL = AL;
    if (AL > 2) AL = (uint8_t)(AL - 2);
    sg16(c, 0x0572, AX);
    AH = 8;
    AL = BL;
    AL ^= 0x0F;
    CL = 4;
    AL = (uint8_t)(AL << 4);
    if (G8(0x0550) == 1) G8(0x0550)++;
    goto go;
jump_down:
    AH = 1; AL = 0x20;
    G8(0x055B) = 8;
    if (G8(0x0550) == 1) G8(0x0550) = 0;
go:
    G8(0x0578) = AL;
    G8(0x0576) = AH;
    G8(0x0577) = 1;
    G8(0x055C) = 0;
    BL = (uint8_t)(G8(0x056E) + 1);
    BL = (uint8_t)(BL << 1);
    if (G8(0x0571) != 0xFF) BL = (uint8_t)(BL + 6);
    BH = 0;
    AX = g16(c, (uint16_t)(BX + 0x0FAA)); sg16(c, 0x0569, AX);
    AX = g16(c, (uint16_t)(BX + 0x0FB6)); sg16(c, 0x0567, AX);
    G8(0x39E0) = 0;
    if (G8(0x127C)) CALL(0x58F8);
    return 1;
}

/* 0F34: walking on the ground */
static void walk(V30 *c) {
    if (g16(c, 0x0004) != 0 && g16(c, 0x0004) != 7) CALL(0x3445);
    CALL(0x0FC9);
    DL = G8(0x057B);
    CX = g16(c, 0x0579);
    CALL(0x2CB0);
    sg16(c, 0x0563, AX);
    AL = G8(0x056E) | G8(0x0571);
    if (!AL) { CALL(0x1069); return; }
    CALL(0x1020);
    sg16(c, 0x055D, BX);
    CALL(0x11E3);
    CALL(0x1B7A);
    if (c->cf) return;
    CALL(0x20F5);
    if (c->cf) return;
    AX = g16(c, 0x0563); sg16(c, 0x055F, AX);
    sg16(c, 0x0565, 0x0B03);
    CALL(0x1145);
}

void hand_08e5(V30 *c) {
    AH = 0;
    INT(0x1A);
    if (DX == g16(c, 0x057D)) {
        if (g16(c, 0x0684) == 0) return;
        uint16_t n = (uint16_t)(g16(c, 0x0684) - 1);
        sg16(c, 0x0684, n);
        if (n) return;
    } else {
        AX = 0x20;
        if (G8(0x0697) == 0xFD) AX >>= 1;
        sg16(c, 0x0684, AX);
    }
    /* 090C: when the cat moves, wait for the vertical retrace */
    int wait = 1;
    if (g16(c, 0x0004) != 2) {
        CL = G8(0x0571) | G8(0x056E);
        if (CL) wait = 0;
    }
    if (wait) {
        uint16_t dx = DX, ax = AX;
        CALL(0x13D8);
        AX = ax; DX = dx;
        if (c->zf) return;
    }
    sg16(c, 0x057D, DX);
    sg16(c, 0x057F, AX);
    if (g16(c, 0x0004) == 4 && G8(0x39E1)) return;
    if (g16(c, 0x0004) == 6 && G8(0x44BD)) return;
    if (g16(c, 0x0004) == 2) { swim(c); return; }

    /* 0BAC */
    CALL(0x1B7A);
    if (c->cf) return;
    if (G8(0x1CB8)) return;
    if (G8(0x0558)) {                                    /* entering: short delay, then steps */
        if (G8(0x0559)) { if (!G8(0x1CBF)) G8(0x0559)--; return; }
        if (--G8(0x0558)) {
            CALL(0x1020);
            sg16(c, 0x055D, BX);
            AL = G8(0x0558);
            AH = G8(0x056E);
            CALL(0x0F87);
            if (G8(0x0558) != 2) CALL(0x11E3);
            CALL(0x1B7A);
            if (c->cf) return;
            CALL(0x20F5);
            if (c->cf) return;
            DL = G8(0x057B);
            CX = g16(c, 0x0579);
            CALL(0x2CB0);
            sg16(c, 0x055F, AX);
            CALL(0x1145);
            return;
        }
        sg16(c, 0x0572, 8);
        CALL(0x0FC9);
    }
    /* 0C1C */
    if (G8(0x055C) >= 1) {
        if (G8(0x055C) == 1) {                           /* crouch before the jump */
            G8(0x055C)++;
            sg16(c, 0x0572, 6);
            DL = G8(0x057B);
            CX = g16(c, 0x0579);
            CALL(0x2CB0);
            sg16(c, 0x0563, AX);
            CALL(0x11E3);
            CALL(0x1B7A);
            if (c->cf) return;
            CALL(0x20F5);
            if (c->cf) return;
            AX = g16(c, 0x0563); sg16(c, 0x055F, AX);
            sg16(c, 0x0565, 0x0E03);
            sg16(c, 0x055D, 0x09DA);
            CALL(0x1145);
        }
        if (!G8(0x0699)) return;
        if (!start_move(c)) walk(c);
        return;
    }
    if (G8(0x0571)) { fall_or_jump(c); return; }

    /* 0E23: on the ground or a line */
    if (g16(c, 0x0004) == 7 || G8(0x057B) < 0xB4) {
        CALL(0x1608);
        if (!c->cf) {                                    /* nothing under the cat: fall */
            G8(0x056E) = 0; G8(0x0571) = 1;
            /* continue at 0EB1 via start_move's jump_down path */
            AH = 1; AL = 0x20;
            G8(0x055B) = 8;
            if (G8(0x0550) == 1) G8(0x0550) = 0;
            G8(0x0578) = AL; G8(0x0576) = AH; G8(0x0577) = 1; G8(0x055C) = 0;
            BL = (uint8_t)(G8(0x056E) + 1);
            BL = (uint8_t)(BL << 1);
            if (G8(0x0571) != 0xFF) BL = (uint8_t)(BL + 6);
            BH = 0;
            AX = g16(c, (uint16_t)(BX + 0x0FAA)); sg16(c, 0x0569, AX);
            AX = g16(c, (uint16_t)(BX + 0x0FB6)); sg16(c, 0x0567, AX);
            G8(0x39E0) = 0;
            if (G8(0x127C)) CALL(0x58F8);
            return;
        }
        if (g16(c, 0x0004) == 0) {                       /* alley: on a fence top? */
            CALL(0x22F7);
            if (!c->cf) G8(0x056C) = 0;
            else {
                if (!G8(0x056C)) CALL(0x591F);
                G8(0x0699) = 1;
                G8(0x056C) = 1;
                CALL(0x2DFD);
                DL &= 1;
                if (!DL) DL = 0xFF;
                G8(0x0698) = DL;
            }
        }
    }
    if (!start_move(c)) walk(c);
}
