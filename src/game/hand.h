/* Shared definitions for the hand-written C version of Alley Cat (phase 2).
 *
 * A routine rewritten by hand is `void hand_XXXX(V30 *c)`, where XXXX is the offset of the
 * original routine in the code segment. The generator finds these functions and routes every
 * call of the original routine through cat_hand_call(), which, depending on cat_hand_mode,
 * runs the recompiled version, the hand-written one, or both and compares their effects.
 *
 * At this stage a hand routine keeps the original register interface (inputs and outputs in
 * the 8086 registers, game variables at fixed offsets of the data segment). The RET is done by
 * cat_hand_call; a hand routine only charges its approximate 8088 time with V30_TICK. */
#pragma once
#include "v30.h"
#include "pc.h"
#include <stdint.h>
#include <stdio.h>

#define AX c->r[V30_AX]
#define CX c->r[V30_CX]
#define DX c->r[V30_DX]
#define BX c->r[V30_BX]
#define SP c->r[V30_SP]
#define BP c->r[V30_BP]
#define SI c->r[V30_SI]
#define DI c->r[V30_DI]
#define AL c->b[0]
#define AH c->b[1]
#define CL c->b[2]
#define CH c->b[3]
#define DL c->b[4]
#define DH c->b[5]
#define BL c->b[6]
#define BH c->b[7]
#define ES c->sreg[V30_ES]
#define DS c->sreg[V30_DS]

#define GAME_DS   (PC_LOAD_SEG + 0x10)            /* the game's data segment */
#define CGA_SEG   0xB800

/* byte / word at seg:off (with 16-bit wrap of the offset, as on the 8086) */
static inline uint8_t  m8(V30 *c, uint16_t s, uint16_t o) { return rd8(c, s, o); }
static inline uint16_t m16(V30 *c, uint16_t s, uint16_t o) { return rd16(c, s, o); }
static inline void     w8(V30 *c, uint16_t s, uint16_t o, uint8_t v) { wr8(c, s, o, v); }
static inline void     w16(V30 *c, uint16_t s, uint16_t o, uint16_t v) { wr16(c, s, o, v); }
/* game variables (data segment) */
#define G8(off)  (c->mem[(GAME_DS << 4) + (off)])
static inline uint16_t g16(V30 *c, uint16_t off) { return m16(c, GAME_DS, off); }
static inline void sg16(V30 *c, uint16_t off, uint16_t v) { w16(c, GAME_DS, off, v); }

#define CALL(off) cat_call(c, (off))
#define INT(n)    v30_int(c, (n))           /* BIOS call through the interrupt vector */
/* 8086 compare / test: set the flags exactly as CMP / TEST would */
#define CMP8(a, b)  ((void)sub8(c, (uint8_t)(a), (uint8_t)(b), 0))
#define CMP16(a, b) ((void)sub16(c, (uint16_t)(a), (uint16_t)(b), 0))
#define TEST8(a, b) ((void)and8(c, (uint8_t)(a), (uint8_t)(b)))
#define TEST16(a, b) ((void)and16(c, (uint16_t)(a), (uint16_t)(b)))

enum { HAND_GEN = 0, HAND_ON = 1, HAND_CHECK = 2 };
extern int cat_hand_mode;
int cat_hand_busy(void);                 /* inside a check-mode comparison */
/* Calls the routine at code-segment offset `off` (hand-written or recompiled), like CALL. */
void cat_call(V30 *c, uint16_t off);
/* Check-mode summary: per routine, calls compared and mismatches. Returns total mismatches. */
unsigned cat_hand_report(FILE *f);
