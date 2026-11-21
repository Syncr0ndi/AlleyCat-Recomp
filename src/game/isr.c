/* Alley Cat, hand-written: interrupt vectors and the keyboard interrupt handler.
 *
 * The game replaces INT 09h (and INT 48h on the PCjr) with its own handler, which keeps the
 * state of the 22 game keys (table of scan codes at 06A1, states at 06B7: 00 down, 80 up)
 * and counts key presses in [0693]. The PCjr handlers (14FB, 1554) cannot run on the
 * emulated IBM PC and stay recompiled. */
#include "hand.h"

#define CODE_SEG (PC_LOAD_SEG + 0x723)
#define IS_PCJR (G8(0x0697) == 0xFD)

/* RETF/IRET from a hand-written routine: the wrapper does the final POP IP */
static void iret(V30 *c) {
    uint16_t ip = pop16(c), cs = pop16(c), fl = pop16(c);
    c->sreg[V30_CS] = cs;
    v30_set_flags(c, fl);
    push16(c, ip);
}

/* 1419 vectors_install: save the old INT 09h / 48h vectors (in the code segment) and point
 * them at the game's handlers. */
void hand_1419(V30 *c) {
    AX = 0; ES = AX;
    AX = m16(c, 0, 0x24); BX = m16(c, 0, 0x26);
    CX = m16(c, 0, 0x120); DX = m16(c, 0, 0x122);
    w16(c, CODE_SEG, 0x13DF, AX); w16(c, CODE_SEG, 0x13E1, BX);
    w16(c, CODE_SEG, 0x13E3, CX); w16(c, CODE_SEG, 0x13E5, DX);
    BX = IS_PCJR ? 0x14FB : 0x14B3;
    c->ifl = 0;
    w16(c, 0, 0x24, BX); w16(c, 0, 0x26, CODE_SEG);
    CMP8(G8(0x0697), 0xFD);
    if (IS_PCJR) {
        w16(c, 0, 0x120, 0x1554); w16(c, 0, 0x122, CODE_SEG);
        AX = 0x40; ES = AX;
        AL = (uint8_t)(m8(c, ES, 0x18) | 1);
        w8(c, ES, 0x18, AL);
    }
    c->ifl = 1;
    V30_TICK(250);
}

/* 147F vectors_restore: put the saved INT 09h / 48h vectors back. */
void hand_147f(V30 *c) {
    AX = 0; ES = AX;
    AX = m16(c, CODE_SEG, 0x13DF); BX = m16(c, CODE_SEG, 0x13E1);
    CX = m16(c, CODE_SEG, 0x13E3); DX = m16(c, CODE_SEG, 0x13E5);
    c->ifl = 0;
    w16(c, 0, 0x24, AX); w16(c, 0, 0x26, BX);
    CMP8(G8(0x0697), 0xFD);
    if (c->zf) { w16(c, 0, 0x120, CX); w16(c, 0, 0x122, DX); }
    c->ifl = 1;
    V30_TICK(200);
}

/* 1572 hotkeys: with Ctrl+Alt held: Del reboots (here: ends the program), Right / Left move
 * the picture (CRTC register 2, value [0690] + 27h). ES = game data segment. */
void hand_1572(V30 *c) {
    AL = (uint8_t)(m8(c, ES, 0x06C9) | m8(c, ES, 0x06B7));
    CMP8(AL, 0);
    V30_TICK(40);
    if (AL) return;
    TEST8(m8(c, ES, 0x06CA), 0x80);
    if (c->zf) {                                          /* Ctrl+Alt+Del */
        AL = 0x20;
        v30_out(c, 0x20, AL, 0);
        AX = 0xF000; c->sreg[V30_SS] = AX;
        AX = 0x40; DS = AX;
        BX = 0x72;
        w16(c, DS, BX, 0x1234);                           /* BIOS "warm boot" flag */
        AX = 0; ES = AX;
        c->bios(c, 0xE05B);                               /* JMP F000:E05B: reboot -> quit */
        return;
    }
    TEST8(m8(c, ES, 0x06B9), 0x80);
    if (c->zf) {                                          /* Right */
        CMP8(m8(c, ES, 0x0690), 1);
        if (c->cf) return;
        w8(c, ES, 0x0690, dec8(c, m8(c, ES, 0x0690)));
    } else {
        TEST8(m8(c, ES, 0x06BB), 0x80);
        if (!c->zf) return;                               /* neither */
        CMP8(m8(c, ES, 0x0690), 7);
        if (!c->cf) return;
        w8(c, ES, 0x0690, inc8(c, m8(c, ES, 0x0690)));
    }
    uint16_t dx = DX;
    AL = 2;
    DX = 0x3D4;
    v30_out(c, DX, AL, 0);
    AL = add8(c, m8(c, ES, 0x0690), 0x27, 0);
    DX = inc16(c, DX);
    v30_out(c, DX, AL, 0);
    DX = dx;
}

/* 14B3 keyboard_irq (IBM PC): read the scan code, count presses, update the key's state,
 * acknowledge the keyboard and the interrupt controller. */
void hand_14b3(V30 *c) {
    uint16_t ax = AX, es = ES, di = DI, cx = CX;
    DI = GAME_DS; ES = DI;
    AL = (uint8_t)v30_in(c, 0x60, 0);
    AH = AL;
    AL &= 0x7F;
    if (!(AH & 0x80)) w16(c, ES, 0x0693, (uint16_t)(m16(c, ES, 0x0693) + 1));
    c->df = 0;
    int found = 0;
    for (DI = 0x06A1, CX = 0x16; CX; ) {                 /* REPNE SCASB */
        uint8_t k = m8(c, ES, DI);
        DI++; CX--;
        if (k == AL) { found = 1; break; }
    }
    if (found) {
        DI = (uint16_t)(DI - 0x06A2);
        AH &= 0x80;
        w8(c, ES, (uint16_t)(DI + 0x06B7), AH);
    }
    AL = (uint8_t)v30_in(c, 0x61, 0);
    AH = AL;
    v30_out(c, 0x61, (uint8_t)(AL | 0x80), 0);
    v30_out(c, 0x61, AH, 0);
    CALL(0x1572);
    CX = cx; DI = di; ES = es;
    AL = 0x20;
    v30_out(c, 0x20, AL, 0);
    AX = ax;
    V30_TICK(400);
    iret(c);
}
