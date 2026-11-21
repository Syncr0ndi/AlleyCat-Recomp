// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
/* Alley Cat, hand-written: keyboard / joystick input and small hardware helpers.
 *
 * The keyboard interrupt handler keeps one byte per game key at 06B7 (00 = held, 80 = up),
 * in this order: Alt, Up, Right, Down, Left, PgUp, PgDn, End, Home, Esc, Y, N, K, H, T, A, S,
 * R, Ctrl, Del, M, 9. [0693] counts key presses. The game reads directions into [0698]
 * (x: 1 right, FF left) and [0699] (y: 1 down, FF up), and the action button into [069A]. */
#include "hand.h"

#define KEY(i) G8(0x06B7 + (i))
enum { K_ALT, K_UP, K_RIGHT, K_DOWN, K_LEFT, K_PGUP, K_PGDN, K_END, K_HOME, K_ESC, K_Y, K_N, K_K, K_H,
       K_T, K_A, K_S, K_R, K_CTRL, K_DEL, K_M, K_9 };
#define IS_PCJR (G8(0x0697) == 0xFD)

/* 13AA machine_id: [0697] = BIOS machine ID byte (FF PC, FE XT, FD PCjr). */
void hand_13aa(V30 *c) {
    AX = 0xF000; ES = AX;
    AL = m8(c, ES, 0xFFFE);
    G8(0x0697) = AL;
    V30_TICK(50);
}

/* 13B7 pit_read: AX = current count of timer channel 0. */
void hand_13b7(V30 *c) {
    AL = 0;
    v30_out(c, 0x43, AL, 0);
    uint8_t lo = (uint8_t)v30_in(c, 0x40, 0);
    uint8_t hi = (uint8_t)v30_in(c, 0x40, 0);
    AL = lo; AH = hi;
    V30_TICK(70);
}

/* 13D8 in_retrace: AL = 8 and ZF=0 during the vertical retrace, else AL = 0, ZF=1. */
void hand_13d8(V30 *c) {
    DX = 0x3DA;
    AL = and8(c, (uint8_t)v30_in(c, DX, 0), 8);
    V30_TICK(35);
}

/* 13E8 keys_init: all keys up; remember the BIOS keyboard byte (PCjr) in the code segment. */
void hand_13e8(V30 *c) {
    for (int i = 0; i < 0x16; i++) KEY(i) = 0x80;
    c->df = 0;
    sg16(c, 0x0691, sub16(c, g16(c, 0x0693), 0x70, 0));
    w8(c, PC_LOAD_SEG + 0x723, 0x13E7, m8(c, 0x40, 0x12));
    V30_TICK(300);                                       /* AX, CX, DI, ES are saved and restored */
}

/* 12A1 joy_axis: BL = -1 / 0 / 1 from the time an axis took (AX unchanged). */
void hand_12a1(V30 *c) {
    uint16_t ax = AX;
    CALL(0x13B7);
    BX = (uint16_t)(AX - g16(c, 0x069C));
    AX = ax;
    if (BX < 0xF5E6) BL = 1;
    else if (BX < 0xFAFA) BL = 0;
    else BL = 0xFF;
    V30_TICK(60);
}

/* 12C1 keys_to_dirs: directions from the keypad (on the PC the diagonal keys count too). */
void hand_12c1(V30 *c) {
    AL = KEY(K_DOWN);
    if (!IS_PCJR) AL = (uint8_t)(AL & KEY(K_PGDN) & KEY(K_END));
    AL ^= 0x80; if (AL) AL = 1;
    G8(0x0699) = AL;
    AL = KEY(K_UP);
    if (!IS_PCJR) AL = (uint8_t)(AL & KEY(K_PGUP) & KEY(K_HOME));
    AL ^= 0x80; if (AL) G8(0x0699) = 0xFF;
    AL = KEY(K_RIGHT);
    if (!IS_PCJR) AL = (uint8_t)(AL & KEY(K_PGUP) & KEY(K_PGDN));
    AL ^= 0x80; if (AL) AL = 1;
    G8(0x0698) = AL;
    AL = KEY(K_LEFT);
    if (!IS_PCJR) AL = (uint8_t)(AL & KEY(K_END) & KEY(K_HOME));
    AL ^= 0x80; if (AL) G8(0x0698) = 0xFF;
    AL = KEY(K_ALT);
    CL = 3;
    AL = (uint8_t)(AL >> 3);
    G8(0x069A) = AL;
    V30_TICK(250);
}

/* 1200 read_input: every 2 BIOS ticks, read the keys (with a short timer-based pause) or
 * the joystick: trigger the one-shots, time both axes (x -> [0698], y -> [0699]). */
void hand_1200(V30 *c) {
    AH = 0;
    INT(0x1A);
    AX = DX;
    AX = (uint16_t)(AX - g16(c, 0x069F));
    if (AX < 2) return;
    sg16(c, 0x069F, DX);
    if (!G8(0x069B)) {
        CALL(0x12C1);
        CALL(0x13B7);
        DX = AX;
        do { CALL(0x13B7); AX = (uint16_t)(AX - DX); } while (AX < 0xF8ED);
        return;
    }
    DX = 0x201;
    AL = (uint8_t)(v30_in(c, DX, 0) & 0x10);
    G8(0x069A) = AL;
    G8(0x069E) = 3;
    CALL(0x13B7);
    sg16(c, 0x069C, AX);
    v30_out(c, DX, AL, 0);
    CX = 0x7D0;
    for (;;) {
        AL = (uint8_t)v30_in(c, DX, 0);
        if (!(AL & 1) && (G8(0x069E) & 1)) { G8(0x069E) &= 0xFE; CALL(0x12A1); G8(0x0698) = BL; }
        if (!(AL & 2) && (G8(0x069E) & 2)) { G8(0x069E) &= 0xFD; CALL(0x12A1); G8(0x0699) = BL; }
        if (!(G8(0x069E) & 3)) return;
        CALL(0x13B7);
        AX = (uint16_t)(AX - g16(c, 0x069C));
        V30_TICK(80);
        if (--CX == 0 || AX == 0x1964) break;             /* LOOPNZ after CMP AX,1964h */
    }
    if (G8(0x069E) & 1) G8(0x0698) = 0xFF;
    if (G8(0x069E) & 2) G8(0x0699) = 0xFF;
}

/* 1338 control_keys: on a new key press: Esc pauses; Ctrl-9 gives 9 lives, Ctrl-M back to the
 * menu, Ctrl-R restart, Ctrl-S sound on/off, Ctrl-Y quits to DOS. */
void hand_1338(V30 *c) {
    AX = g16(c, 0x0693);
    CMP16(AX, g16(c, 0x0691));
    if (c->zf) return;
    sg16(c, 0x0691, AX);
    TEST8(KEY(K_ESC), 0x80);
    if (c->zf) {                                          /* Esc */
        AX = g16(c, 0x0693);
        CMP16(AX, g16(c, 0x6E00));
        if (!c->zf) CALL(0x5E70);
        return;
    }
    TEST8(KEY(K_CTRL), 0x80);
    if (!c->zf) return;
    TEST8(KEY(K_9), 0x80);
    if (c->zf) { G8(0x1F80) = 9; return; }
    TEST8(KEY(K_Y), 0x80);
    if (c->zf) {                                          /* quit: far return to the PSP (INT 20h) */
        CALL(0x147F);
        AX = pop16(c);                                    /* drop our caller's return address */
        uint16_t ip = pop16(c), cs = pop16(c);
        c->sreg[V30_CS] = cs;
        push16(c, ip);                                    /* the wrapper's RET pops this */
        return;
    }
    TEST8(KEY(K_M), 0x80);
    if (c->zf) { G8(0x041C) = 0xFF; return; }
    TEST8(KEY(K_R), 0x80);
    if (c->zf) { G8(0x041B) = 0xFF; return; }
    TEST8(KEY(K_S), 0x80);
    if (!c->zf) return;
    G8(0x0000) = (uint8_t)~G8(0x0000);
    CMP8(G8(0x0000), 0);
    if (!c->zf) return;
    CALL(0x5B21);
}
