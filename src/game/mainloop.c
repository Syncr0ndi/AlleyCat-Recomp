// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
/* Alley Cat, hand-written: the program's entry and main loop (never returns).
 *
 * Start-up -> title -> menu -> new game -> alley loop; jumping through a window picks a
 * room (state [0004], 1-7) whose loop runs until the cat leaves, loses, wins ([0553]) or the
 * player restarts (Ctrl-R, [041B]) or goes back to the menu (Ctrl-M, [041C]). Labels keep
 * the original addresses. The room loops (the jump table at CS:0250) are folded in here. */
// absorbs: 0260 02AA 02FE 0349 0394 03E2 0459
#include "hand.h"

/* one turn of a room loop; returns nonzero when the room ends */
static int room_turn(V30 *c, const uint16_t *calls, int n, const uint16_t *flags, int nflags) {
    CALL(0x1338);
    CALL(0x1200);
    CALL(0x546D);
    for (int i = 0; i < n; i++) CALL(calls[i]);
    AL = 0;
    for (int i = 0; i < nflags; i++) AL |= G8(flags[i]);
    return AL != 0;
}

static void room_setup(V30 *c, uint16_t state, const uint16_t *calls, int n) {
    sg16(c, 0x0004, state);
    CALL(0x1BF0);
    CALL(0x2790);
    for (int i = 0; i < n; i++) CALL(calls[i]);
}

void hand_0000(V30 *c) {
    /* DOS start: far return address to the PSP (INT 20h) for the quit key */
    push16(c, DS);
    AX = 0;
    push16(c, AX);
    CALL(0x5C60);
    AX = GAME_DS; DS = AX;
    CALL(0x13AA);
    G8(0x0690) = 4;
    sg16(c, 0x6DF8, 0);
    G8(0x069B) = 0;
    CALL(0x1419);
    CALL(0x13E8);
    AX = (uint16_t)(g16(c, 0x0693) + 0x240);
    sg16(c, 0x6E00, AX);
    AX = 4;
    INT(0x10);                                           /* 320x200 4-colour */
    AL = G8(0x0697) == 0xFD ? 4 : 6;
    G8(0x0690) = AL;                                     /* picture position (CRTC R2 - 27h) */
    AH = 0x0B; BX = 0x0101;
    INT(0x10);
    sg16(c, 0x0416, 0);
    sg16(c, 0x0004, 0);
    CALL(0x1D31);
    if (G8(0x0697) != 0xFD) { DX = 0x3D9; AL = 0x20; v30_out(c, DX, AL, 0); }
    CALL(0x2E10);
    CALL(0x26E1);
    CALL(0x26DA);
    G8(0x041A) = 0;
    AX = 0xFFFF;
    sg16(c, 0x041D, AX);
    sg16(c, 0x041F, AX);

L0081:                                                   /* title screen */
    G8(0x0000) = 0xFF;                                   /* sound on */
    CALL(0x2690);
    sg16(c, 0x0008, 0);
    sg16(c, 0x0004, 0);
    CALL(0x1D31);
    CALL(0x5B21);
    CALL(0x5CB0);
    CALL(0x5B21);
    if (G8(0x041A)) goto L00AE;
L00A3:                                                   /* menu */
    CALL(0x5B21);
    CALL(0x5EE5);
    G8(0x041A) = 1;
L00AE:                                                   /* new game */
    AX = g16(c, 0x6DF8);
    sg16(c, 0x0008, AX);                                 /* level = skill */
    G8(0x1F80) = 3;                                      /* lives */
    CALL(0x26DA);
    sg16(c, 0x0004, 0);
    CALL(0x1D31);
    CALL(0x5B21);
    sg16(c, 0x1C30, 0);
    AH = 0;
    INT(0x1A);
    sg16(c, 0x0412, DX);
    sg16(c, 0x0414, 0);
    G8(0x0418) = 0; G8(0x0419) = 0; G8(0x041C) = 0; G8(0x041B) = 0;
    CALL(0x5B21);

L00F3:                                                   /* back in the alley */
    if (!G8(0x1F80)) goto L0081;
    if (G8(0x041B)) goto L00AE;
    if (G8(0x041C)) goto L00A3;
    CALL(0x2A00);
    CALL(0x5400);
    G8(0x1F81) = 0xFF;
    CALL(0x5B21);
    sg16(c, 0x0004, 0);
    if (G8(0x0419)) {                                    /* came out of a room: drop back in place */
        CALL(0x07A1);
        G8(0x0550) = 2; G8(0x0576) = 1; G8(0x0578) = 0x20;
    } else {
        sg16(c, 0x0579, 0);
        CALL(0x070D);
    }
    CALL(0x1E40);
    CALL(0x1830);
    CALL(0x2210);
    CALL(0x2330);
    CALL(0x26F2);
    CALL(0x26FC);
    CALL(0x58BD);
    for (;;) {                                           /* 0155: the alley loop */
        if (!G8(0x1F80)) goto L0081;
        CALL(0x1338);
        if (G8(0x041C)) goto L00A3;
        if (G8(0x041B)) goto L00AE;
        CALL(0x1200);
        CALL(0x08E5);
        CALL(0x1E63);
        if (!G8(0x1CB8)) {
            G8(0x040F)++;
            if (G8(0x040F) & 3) continue;                /* the rest runs every 4th turn */
        }
        CALL(0x546D);
        CALL(0x04A0);
        CALL(0x1936);
        CALL(0x184B);
        CALL(0x2216);
        CALL(0x237B);
        CALL(0x26B3);
        if (!G8(0x0551)) continue;
        if (!G8(0x1F80)) goto L0081;
        break;
    }
    /* 01B7: into a window: remember the spot, pick a room */
    AH = 0;
    INT(0x1A);
    sg16(c, 0x0410, DX);
    AX = g16(c, 0x0579);
    sg16(c, 0x0001, AX);
    AL = G8(0x057B);
    G8(0x0003) = AL;
    G8(0x0419) = 1;
    if (G8(0x0418)) {
        G8(0x0418) = 0;
        sg16(c, 0x0004, 7);
    } else {
        for (;;) {
            CALL(0x2DFD);
            if ((DL & 0xA0) && (g16(c, 0x0008) & 3) != 3) {
                BX = g16(c, 0x0008) & 3;
                CL = 2;
                BX = (uint16_t)(BX << 2);
                DX &= 3;
                BX = (uint16_t)(BX + DX);
                AL = G8((uint16_t)(BX + 0x0421));        /* rooms of this level */
            } else {
                do { CALL(0x2DFD); DX &= 7; } while (DX >= 5);
                BX = DX;
                AL = G8((uint16_t)(BX + 0x042D));        /* any room */
            }
            AH = 0;
            if (AX == g16(c, 0x041D) && AX == g16(c, 0x041F)) continue;   /* not 3 times running */
            break;
        }
        sg16(c, 0x0004, AX);
        CX = g16(c, 0x041D);
        sg16(c, 0x041F, CX);
        sg16(c, 0x041D, AX);
    }
    sg16(c, 0x0006, 0);
    BX = g16(c, 0x0004);
    if (BX > 7) BX = 0;
    switch (BX) {
    case 7: {                                            /* 0260: the wall (room 7) */
        static const uint16_t setup[] = {0x07A1, 0x1E40, 0x3405, 0x6100, 0x4F59, 0x58BD};
        static const uint16_t turn[] = {0x08E5, 0x6106, 0x2F66, 0x2E60, 0x4C10};
        static const uint16_t fl[] = {0x0551, 0x0553, 0x041C, 0x041B};
        room_setup(c, 7, setup, 6);
        while (!room_turn(c, turn, 5, fl, 4)) {}
        break;
    }
    case 6: {                                            /* 02AA: dogs and milk */
        static const uint16_t setup[] = {0x4C00, 0x07A1, 0x3405, 0x1E40, 0x58BD};
        static const uint16_t turn[] = {0x4943, 0x47D6, 0x08E5};
        static const uint16_t fl[] = {0x0551, 0x0552, 0x0553, 0x041B, 0x041C};
        room_setup(c, 6, setup, 5);
        for (;;) {
            CALL(0x1338); CALL(0x1200); CALL(0x546D);
            for (int i = 0; i < 3; i++) CALL(turn[i]);
            if (G8(0x1CB8)) CALL(0x1E63); else CALL(0x3150);
            AL = 0;
            for (int i = 0; i < 5; i++) AL |= G8(fl[i]);
            if (AL) break;
        }
        break;
    }
    case 5: {                                            /* 02FE: bird cage */
        static const uint16_t setup[] = {0x457A, 0x07A1, 0x3405, 0x1E40, 0x58BD};
        static const uint16_t turn[] = {0x45AB, 0x4340, 0x08E5, 0x3150, 0x1E63};
        static const uint16_t fl[] = {0x0552, 0x0553, 0x0551, 0x041C, 0x041B};
        room_setup(c, 5, setup, 5);
        while (!room_turn(c, turn, 5, fl, 5)) {}
        break;
    }
    case 4: {                                            /* 0349: mouse holes */
        static const uint16_t setup[] = {0x07A1, 0x3405, 0x1E40, 0x4090, 0x58BD};
        static const uint16_t turn[] = {0x08E5, 0x3E90, 0x40C2, 0x3150, 0x1E63};
        static const uint16_t fl[] = {0x0552, 0x0553, 0x0551, 0x041C, 0x041B};
        room_setup(c, 4, setup, 5);
        while (!room_turn(c, turn, 5, fl, 5)) {}
        break;
    }
    case 3: {                                            /* 0394: bookshelves and spider */
        static const uint16_t setup[] = {0x07A1, 0x3405, 0x1E40, 0x3B30, 0x3C90, 0x58BD};
        static const uint16_t turn[] = {0x08E5, 0x3CB1, 0x3B42, 0x3150, 0x1E63};
        static const uint16_t fl[] = {0x0552, 0x0553, 0x0551, 0x041C, 0x041B};
        room_setup(c, 3, setup, 6);
        while (!room_turn(c, turn, 5, fl, 5)) {}
        break;
    }
    case 2: fishbowl: {                                  /* 0459: inside the fishbowl */
        static const uint16_t setup[] = {0x35C9, 0x07A1};
        static const uint16_t turn[] = {0x08E5, 0x3675, 0x37E5};
        static const uint16_t fl[] = {0x0552, 0x0553, 0x041C, 0x041B};
        room_setup(c, 2, setup, 2);
        G8(0x1CBF) = 0; G8(0x1CB8) = 0;
        CALL(0x58BD);
        while (!room_turn(c, turn, 3, fl, 4)) {}
        break;
    }
    default: {                                           /* 03E2: room 1, the fishbowl on the table */
        static const uint16_t setup[] = {0x07A1, 0x3405, 0x1E40, 0x58BD};
        room_setup(c, 1, setup, 4);
        for (;;) {
            CALL(0x1338); CALL(0x1200); CALL(0x546D);
            CALL(0x08E5); CALL(0x3150); CALL(0x1E63); CALL(0x3850);
            if (G8(0x0554)) goto fishbowl;               /* jumped into the bowl */
            AL = (uint8_t)(G8(0x0552) | G8(0x0551) | G8(0x041B) | G8(0x041C));
            if (AL) break;
        }
        break;
    }
    }
    /* 0427: the room is over */
    if (G8(0x041B)) goto L00AE;
    if (G8(0x041C)) goto L00A3;
    if (G8(0x0552)) G8(0x0419) = 0;                      /* lost: start the alley afresh */
    AX = g16(c, 0x0004);
    sg16(c, 0x0006, AX);
    sg16(c, 0x0004, 0);
    CALL(0x1BF0);
    goto L00F3;
}
