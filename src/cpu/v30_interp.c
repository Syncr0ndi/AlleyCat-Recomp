/* NEC V30 interpreter: fallback for code with no recompiled entry (e.g. code the i8751 writes
 * into the B0000 shared RAM). Instruction bytes are fetched from memory every time, so code
 * patched at run time is seen. Returns to the dispatcher as soon as CS:IP reaches an address
 * that has a recompiled entry. Semantics come from the same helpers as the recompiled code. */
#include "v30.h"
#include <stdio.h>
#include <stdlib.h>

#define AX c->r[V30_AX]
#define CX c->r[V30_CX]
#define DX c->r[V30_DX]
#define BX c->r[V30_BX]
#define SP c->r[V30_SP]
#define BP c->r[V30_BP]
#define SI c->r[V30_SI]
#define DI c->r[V30_DI]
#define AL c->b[0]
#define CL c->b[2]
#define CS_ c->sreg[V30_CS]

typedef struct {
    int seg;        /* segment override (-1 none) */
    int rep;        /* REP_* */
    int mod, reg, rm;
    uint16_t ea; int eseg;
} X;

static inline uint8_t f8(V30 *c) { return rd8(c, CS_, c->ip++); }
static inline uint16_t f16(V30 *c) { uint16_t v = rd16(c, CS_, c->ip); c->ip += 2; return v; }

static void modrm(V30 *c, X *x) {
    uint8_t m = f8(c);
    x->mod = m >> 6; x->reg = (m >> 3) & 7; x->rm = m & 7;
    if (x->mod == 3) return;
    uint16_t ea = 0; int defss = 0;
    switch (x->rm) {
    case 0: ea = BX + SI; break;          case 1: ea = BX + DI; break;
    case 2: ea = BP + SI; defss = 1; break; case 3: ea = BP + DI; defss = 1; break;
    case 4: ea = SI; break;               case 5: ea = DI; break;
    case 6: if (x->mod == 0) ea = 0; else { ea = BP; defss = 1; } break;
    case 7: ea = BX; break;
    }
    if (x->mod == 0 && x->rm == 6) ea = f16(c);
    else if (x->mod == 1) ea += (uint16_t)(int8_t)f8(c);
    else if (x->mod == 2) ea += f16(c);
    x->ea = ea;
    x->eseg = x->seg >= 0 ? x->seg : (defss ? V30_SS : V30_DS);
    V30_TICK(7);
}
static inline uint8_t *r8p(V30 *c, int i) { return &c->b[V30_R8(i)]; }
static inline uint8_t get8(V30 *c, X *x) { return x->mod == 3 ? *r8p(c, x->rm) : rd8(c, c->sreg[x->eseg], x->ea); }
static inline uint16_t get16(V30 *c, X *x) { return x->mod == 3 ? c->r[x->rm] : rd16(c, c->sreg[x->eseg], x->ea); }
static inline void set8(V30 *c, X *x, uint8_t v) { if (x->mod == 3) *r8p(c, x->rm) = v; else wr8(c, c->sreg[x->eseg], x->ea, v); }
static inline void set16(V30 *c, X *x, uint16_t v) { if (x->mod == 3) c->r[x->rm] = v; else wr16(c, c->sreg[x->eseg], x->ea, v); }

static uint8_t alu8(V30 *c, int op, uint8_t a, uint8_t b) {
    switch (op) {
    case 0: return add8(c, a, b, 0);      case 1: return or8(c, a, b);
    case 2: return add8(c, a, b, c->cf);  case 3: return sub8(c, a, b, c->cf);
    case 4: return and8(c, a, b);         case 5: return sub8(c, a, b, 0);
    case 6: return xor8(c, a, b);         default: sub8(c, a, b, 0); return a;
    }
}
static uint16_t alu16(V30 *c, int op, uint16_t a, uint16_t b) {
    switch (op) {
    case 0: return add16(c, a, b, 0);     case 1: return or16(c, a, b);
    case 2: return add16(c, a, b, c->cf); case 3: return sub16(c, a, b, c->cf);
    case 4: return and16(c, a, b);        case 5: return sub16(c, a, b, 0);
    case 6: return xor16(c, a, b);        default: sub16(c, a, b, 0); return a;
    }
}
static int cond(V30 *c, int n) {
    switch (n) {
    case 0: return c->of;  case 1: return !c->of; case 2: return c->cf; case 3: return !c->cf;
    case 4: return c->zf;  case 5: return !c->zf; case 6: return c->cf | c->zf; case 7: return !(c->cf | c->zf);
    case 8: return c->sf;  case 9: return !c->sf; case 10: return c->pf; case 11: return !c->pf;
    case 12: return c->sf != c->of; case 13: return c->sf == c->of;
    case 14: return c->zf || c->sf != c->of; default: return !c->zf && c->sf == c->of;
    }
}
/* software interrupt from interpreted code: enter the handler directly */
static void soft_int(V30 *c, unsigned vec) {
    push16(c, v30_get_flags(c)); c->ifl = 0; c->tf = 0;
    push16(c, CS_); push16(c, c->ip);
    c->ip = rd16(c, 0, (uint16_t)(vec * 4)); CS_ = rd16(c, 0, (uint16_t)(vec * 4 + 2));
    V30_TICK(50);
}

static void bad(V30 *c, uint16_t ip0, uint8_t op) {
    v30_fatal(c, "Undefined opcode %02X at %04X:%04X (interpreted code).", op, CS_, ip0);
}

static void nec0f(V30 *c, X *x, uint16_t ip0) {
    uint8_t o = f8(c);
    if (o >= 0x10 && o <= 0x1F) {
        int w = o & 1, kind = (o >> 1) & 3;
        modrm(c, x);
        unsigned bit = o >= 0x18 ? f8(c) : CL;
        if (w) {
            uint16_t v = get16(c, x);
            if (kind == 0) test1_16(c, v, bit);
            else set16(c, x, kind == 1 ? CLR1_16(v, bit) : kind == 2 ? SET1_16(v, bit) : NOT1_16(v, bit));
        } else {
            uint8_t v = get8(c, x);
            if (kind == 0) test1_8(c, v, bit);
            else set8(c, x, kind == 1 ? CLR1_8(v, bit) : kind == 2 ? SET1_8(v, bit) : NOT1_8(v, bit));
        }
        V30_TICK(4);
        return;
    }
    switch (o) {
    case 0x20: v30_add4s(c, x->seg >= 0 ? x->seg : V30_DS); return;
    case 0x22: v30_sub4s(c, x->seg >= 0 ? x->seg : V30_DS); return;
    case 0x26: v30_cmp4s(c, x->seg >= 0 ? x->seg : V30_DS); return;
    case 0x28: modrm(c, x); set8(c, x, v30_rol4(c, get8(c, x))); V30_TICK(20); return;
    case 0x2A: modrm(c, x); set8(c, x, v30_ror4(c, get8(c, x))); V30_TICK(20); return;
    case 0x31: case 0x33: { uint8_t m = f8(c); unsigned len = c->b[V30_R8((m >> 3) & 7)];
        if (o == 0x31) v30_ins(c, V30_R8(m & 7), len); else v30_ext(c, V30_R8(m & 7), len); V30_TICK(30); return; }
    case 0x39: case 0x3B: { uint8_t m = f8(c); unsigned len = f8(c);
        if (o == 0x39) v30_ins(c, V30_R8(m & 7), len); else v30_ext(c, V30_R8(m & 7), len); V30_TICK(30); return; }
    default: bad(c, ip0, 0x0F);
    }
}

/* execute one instruction */
static void step(V30 *c) {
    X x = { .seg = -1, .rep = REP_NONE };
    uint16_t ip0 = c->ip;
    uint8_t o;
    for (;;) {
        o = f8(c);
        if (o == 0x26 || o == 0x2E || o == 0x36 || o == 0x3E) { x.seg = (o >> 3) & 3; continue; }
        if (o == 0xF3) { x.rep = REP_Z; continue; }
        if (o == 0xF2) { x.rep = REP_NZ; continue; }
        if (o == 0x65) { x.rep = REP_C; continue; }
        if (o == 0x64) { x.rep = REP_NC; continue; }
        if (o == 0xF0) continue;
        break;
    }
    V30_TICK(2);
    int srcseg = x.seg >= 0 ? x.seg : V30_DS;

    if (o < 0x40 && (o & 7) < 6) {
        int op = o >> 3;
        switch (o & 7) {
        case 0: modrm(&*c, &x); { uint8_t r = alu8(c, op, get8(c, &x), *r8p(c, x.reg)); if (op != 7) set8(c, &x, r); } return;
        case 1: modrm(c, &x); { uint16_t r = alu16(c, op, get16(c, &x), c->r[x.reg]); if (op != 7) set16(c, &x, r); } return;
        case 2: modrm(c, &x); { uint8_t r = alu8(c, op, *r8p(c, x.reg), get8(c, &x)); if (op != 7) *r8p(c, x.reg) = r; } return;
        case 3: modrm(c, &x); { uint16_t r = alu16(c, op, c->r[x.reg], get16(c, &x)); if (op != 7) c->r[x.reg] = r; } return;
        case 4: { uint8_t r = alu8(c, op, AL, f8(c)); if (op != 7) AL = r; } return;
        case 5: { uint16_t r = alu16(c, op, AX, f16(c)); if (op != 7) AX = r; } return;
        }
    }
    switch (o) {
    case 0x06: case 0x0E: case 0x16: case 0x1E: push16(c, c->sreg[o >> 3]); V30_TICK(6); return;
    case 0x07: case 0x17: case 0x1F: c->sreg[o >> 3] = pop16(c); V30_TICK(6); return;
    case 0x0F: nec0f(c, &x, ip0); return;
    case 0x27: v30_daa(c); return;  case 0x2F: v30_das(c); return;
    case 0x37: v30_aaa(c); return;  case 0x3F: v30_aas(c); return;
    case 0x60: { uint16_t t = SP; push16(c, AX); push16(c, CX); push16(c, DX); push16(c, BX);
                 push16(c, t); push16(c, BP); push16(c, SI); push16(c, DI); V30_TICK(30); return; }
    case 0x61: DI = pop16(c); SI = pop16(c); BP = pop16(c); SP += 2; BX = pop16(c); DX = pop16(c);
               CX = pop16(c); AX = pop16(c); V30_TICK(30); return;
    case 0x62: { modrm(c, &x); int16_t lo = (int16_t)rd16(c, c->sreg[x.eseg], x.ea), hi = (int16_t)rd16(c, c->sreg[x.eseg], (uint16_t)(x.ea + 2));
                 int16_t v = (int16_t)c->r[x.reg]; if (v < lo || v > hi) soft_int(c, 5); return; }
    case 0x68: { uint16_t v = f16(c); push16(c, v); V30_TICK(6); return; }
    case 0x6A: { uint16_t v = (uint16_t)(int8_t)f8(c); push16(c, v); V30_TICK(6); return; }
    case 0x69: case 0x6B: { modrm(c, &x); uint16_t a = get16(c, &x);
                 uint16_t b = o == 0x69 ? f16(c) : (uint16_t)(int8_t)f8(c);
                 c->r[x.reg] = v30_imul3(c, a, b); V30_TICK(22); return; }
    case 0x6C: v30_ins_io(c, 0, x.rep); return;  case 0x6D: v30_ins_io(c, 1, x.rep); return;
    case 0x6E: v30_outs_io(c, 0, srcseg, x.rep); return;  case 0x6F: v30_outs_io(c, 1, srcseg, x.rep); return;
    case 0x80: case 0x82: { modrm(c, &x); uint8_t i = f8(c); uint8_t r = alu8(c, x.reg, get8(c, &x), i); if (x.reg != 7) set8(c, &x, r); return; }
    case 0x81: { modrm(c, &x); uint16_t i = f16(c); uint16_t r = alu16(c, x.reg, get16(c, &x), i); if (x.reg != 7) set16(c, &x, r); return; }
    case 0x83: { modrm(c, &x); uint16_t i = (uint16_t)(int8_t)f8(c); uint16_t r = alu16(c, x.reg, get16(c, &x), i); if (x.reg != 7) set16(c, &x, r); return; }
    case 0x84: modrm(c, &x); and8(c, get8(c, &x), *r8p(c, x.reg)); return;
    case 0x85: modrm(c, &x); and16(c, get16(c, &x), c->r[x.reg]); return;
    case 0x86: { modrm(c, &x); uint8_t t = get8(c, &x); set8(c, &x, *r8p(c, x.reg)); *r8p(c, x.reg) = t; return; }
    case 0x87: { modrm(c, &x); uint16_t t = get16(c, &x); set16(c, &x, c->r[x.reg]); c->r[x.reg] = t; return; }
    case 0x88: modrm(c, &x); set8(c, &x, *r8p(c, x.reg)); return;
    case 0x89: modrm(c, &x); set16(c, &x, c->r[x.reg]); return;
    case 0x8A: modrm(c, &x); *r8p(c, x.reg) = get8(c, &x); return;
    case 0x8B: modrm(c, &x); c->r[x.reg] = get16(c, &x); return;
    case 0x8C: modrm(c, &x); set16(c, &x, c->sreg[x.reg & 3]); return;
    case 0x8D: modrm(c, &x); c->r[x.reg] = x.ea; return;
    case 0x8E: modrm(c, &x); c->sreg[x.reg & 3] = get16(c, &x); return;
    case 0x8F: { uint16_t v = pop16(c); modrm(c, &x); set16(c, &x, v); return; }
    case 0x90: return;
    case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: {
        uint16_t t = AX; AX = c->r[o & 7]; c->r[o & 7] = t; return; }
    case 0x98: AX = (uint16_t)(int16_t)(int8_t)AL; return;
    case 0x99: DX = (AX & 0x8000) ? 0xFFFF : 0; return;
    case 0x9A: { uint16_t off = f16(c), seg = f16(c); push16(c, CS_); push16(c, c->ip); CS_ = seg; c->ip = off; V30_TICK(20); return; }
    case 0x9B: return;
    case 0x9C: push16(c, v30_get_flags(c)); return;
    case 0x9D: v30_set_flags(c, pop16(c)); return;
    case 0x9E: { uint8_t f = c->b[1]; c->cf = f & 1; c->pf = (f >> 2) & 1; c->af = (f >> 4) & 1; c->zf = (f >> 6) & 1; c->sf = (f >> 7) & 1; return; }
    case 0x9F: c->b[1] = (uint8_t)v30_get_flags(c); return;
    case 0xA0: AL = rd8(c, c->sreg[srcseg], f16(c)); return;
    case 0xA1: AX = rd16(c, c->sreg[srcseg], f16(c)); return;
    case 0xA2: wr8(c, c->sreg[srcseg], f16(c), AL); return;
    case 0xA3: wr16(c, c->sreg[srcseg], f16(c), AX); return;
    case 0xA4: v30_movs(c, 0, srcseg, x.rep); return;  case 0xA5: v30_movs(c, 1, srcseg, x.rep); return;
    case 0xA6: v30_cmps(c, 0, srcseg, x.rep); return;  case 0xA7: v30_cmps(c, 1, srcseg, x.rep); return;
    case 0xA8: and8(c, AL, f8(c)); return;
    case 0xA9: and16(c, AX, f16(c)); return;
    case 0xAA: v30_stos(c, 0, x.rep); return;  case 0xAB: v30_stos(c, 1, x.rep); return;
    case 0xAC: v30_lods(c, 0, srcseg, x.rep); return;  case 0xAD: v30_lods(c, 1, srcseg, x.rep); return;
    case 0xAE: v30_scas(c, 0, x.rep); return;  case 0xAF: v30_scas(c, 1, x.rep); return;
    case 0xC0: case 0xC1: case 0xD0: case 0xD1: case 0xD2: case 0xD3: {
        modrm(c, &x);
        unsigned n = (o <= 0xC1) ? (f8(c) & 0x1F) : (o <= 0xD1 ? 1 : (CL & 0x1F));
        if (o & 1) set16(c, &x, v30_shift16(c, x.reg, get16(c, &x), n));
        else set8(c, &x, v30_shift8(c, x.reg, get8(c, &x), n));
        V30_TICK(n); return; }
    case 0xC2: { uint16_t n = f16(c); c->ip = pop16(c); SP += n; V30_TICK(13); return; }
    case 0xC3: c->ip = pop16(c); V30_TICK(13); return;
    case 0xC4: case 0xC5: { modrm(c, &x); uint16_t off = rd16(c, c->sreg[x.eseg], x.ea), seg = rd16(c, c->sreg[x.eseg], (uint16_t)(x.ea + 2));
                c->r[x.reg] = off; c->sreg[o == 0xC4 ? V30_ES : V30_DS] = seg; return; }
    case 0xC6: modrm(c, &x); set8(c, &x, f8(c)); return;
    case 0xC7: modrm(c, &x); set16(c, &x, f16(c)); return;
    case 0xC8: { uint16_t size = f16(c); uint8_t lvl = f8(c); v30_enter(c, size, lvl); return; }
    case 0xC9: SP = BP; BP = pop16(c); return;
    case 0xCA: { uint16_t n = f16(c); c->ip = pop16(c); CS_ = pop16(c); SP += n; V30_TICK(20); return; }
    case 0xCB: c->ip = pop16(c); CS_ = pop16(c); V30_TICK(20); return;
    case 0xCC: soft_int(c, 3); return;
    case 0xCD: soft_int(c, f8(c)); return;
    case 0xCE: if (c->of) soft_int(c, 4); return;
    case 0xCF: c->ip = pop16(c); CS_ = pop16(c); v30_set_flags(c, pop16(c)); V30_TICK(25); return;
    case 0xD4: if (!v30_aam(c, f8(c))) soft_int(c, 0); return;
    case 0xD5: v30_aad(c, f8(c)); return;
    case 0xD7: AL = rd8(c, c->sreg[srcseg], (uint16_t)(BX + AL)); return;
    case 0xD8: case 0xD9: case 0xDA: case 0xDB: case 0xDC: case 0xDD: case 0xDE: case 0xDF: modrm(c, &x); return;
    case 0xE0: case 0xE1: case 0xE2: case 0xE3: {
        int8_t d = (int8_t)f8(c); int take;
        if (o == 0xE3) take = CX == 0;
        else { CX--; take = CX != 0 && (o == 0xE2 || (o == 0xE1 ? c->zf : !c->zf)); }
        if (take) { c->ip += (uint16_t)d; V30_TICK(11); }
        return; }
    case 0xE4: AL = (uint8_t)v30_in(c, f8(c), 0); V30_TICK(8); return;
    case 0xE5: AX = v30_in(c, f8(c), 1); V30_TICK(8); return;
    case 0xE6: v30_out(c, f8(c), AL, 0); V30_TICK(8); return;
    case 0xE7: v30_out(c, f8(c), AX, 1); V30_TICK(8); return;
    case 0xE8: { uint16_t d = f16(c); push16(c, c->ip); c->ip += d; V30_TICK(14); return; }
    case 0xE9: { uint16_t d = f16(c); c->ip += d; V30_TICK(11); return; }
    case 0xEA: { uint16_t off = f16(c), seg = f16(c); CS_ = seg; c->ip = off; V30_TICK(11); return; }
    case 0xEB: { int8_t d = (int8_t)f8(c); c->ip += (uint16_t)d; V30_TICK(10); return; }
    case 0xEC: AL = (uint8_t)v30_in(c, DX, 0); V30_TICK(6); return;
    case 0xED: AX = v30_in(c, DX, 1); V30_TICK(6); return;
    case 0xEE: v30_out(c, DX, AL, 0); V30_TICK(6); return;
    case 0xEF: v30_out(c, DX, AX, 1); V30_TICK(6); return;
    case 0xF4: v30_halt(c); return;
    case 0xF5: c->cf ^= 1; return;
    case 0xF6: case 0xF7: {
        modrm(c, &x);
        int w = o & 1;
        switch (x.reg) {
        case 0: case 1: if (w) and16(c, get16(c, &x), f16(c)); else and8(c, get8(c, &x), f8(c)); return;
        case 2: if (w) set16(c, &x, (uint16_t)~get16(c, &x)); else set8(c, &x, (uint8_t)~get8(c, &x)); return;
        case 3: if (w) set16(c, &x, neg16(c, get16(c, &x))); else set8(c, &x, neg8(c, get8(c, &x))); return;
        case 4: if (w) v30_mul16(c, get16(c, &x)); else v30_mul8(c, get8(c, &x)); V30_TICK(20); return;
        case 5: if (w) v30_imul16(c, get16(c, &x)); else v30_imul8(c, get8(c, &x)); V30_TICK(22); return;
        case 6: if (!(w ? v30_div16(c, get16(c, &x)) : v30_div8(c, get8(c, &x)))) soft_int(c, 0); V30_TICK(30); return;
        default: if (!(w ? v30_idiv16(c, get16(c, &x)) : v30_idiv8(c, get8(c, &x)))) soft_int(c, 0); V30_TICK(35); return;
        } }
    case 0xF8: c->cf = 0; return;  case 0xF9: c->cf = 1; return;
    case 0xFA: c->ifl = 0; return; case 0xFB: c->ifl = 1; return;
    case 0xFC: c->df = 0; return;  case 0xFD: c->df = 1; return;
    case 0xFE: modrm(c, &x);
        if (x.reg == 0) set8(c, &x, inc8(c, get8(c, &x)));
        else if (x.reg == 1) set8(c, &x, dec8(c, get8(c, &x)));
        else bad(c, ip0, o);
        return;
    case 0xFF: modrm(c, &x);
        switch (x.reg) {
        case 0: set16(c, &x, inc16(c, get16(c, &x))); return;
        case 1: set16(c, &x, dec16(c, get16(c, &x))); return;
        case 2: { uint16_t t = get16(c, &x); push16(c, c->ip); c->ip = t; V30_TICK(14); return; }
        case 3: { uint16_t off = rd16(c, c->sreg[x.eseg], x.ea), seg = rd16(c, c->sreg[x.eseg], (uint16_t)(x.ea + 2));
                  push16(c, CS_); push16(c, c->ip); CS_ = seg; c->ip = off; V30_TICK(24); return; }
        case 4: c->ip = get16(c, &x); V30_TICK(11); return;
        case 5: { uint16_t off = rd16(c, c->sreg[x.eseg], x.ea), seg = rd16(c, c->sreg[x.eseg], (uint16_t)(x.ea + 2));
                  CS_ = seg; c->ip = off; V30_TICK(15); return; }
        case 6: { uint16_t t = get16(c, &x); push16(c, t); V30_TICK(6); return; }
        default: bad(c, ip0, o);
        }
    default:
        if (o >= 0x40 && o < 0x48) { c->r[o & 7] = inc16(c, c->r[o & 7]); return; }
        if (o >= 0x48 && o < 0x50) { c->r[o & 7] = dec16(c, c->r[o & 7]); return; }
        if (o >= 0x50 && o < 0x58) { uint16_t t = c->r[o & 7]; push16(c, t); V30_TICK(6); return; }
        if (o >= 0x58 && o < 0x60) { c->r[o & 7] = pop16(c); V30_TICK(6); return; }
        if (o >= 0x70 && o < 0x80) { int8_t d = (int8_t)f8(c); if (cond(c, o & 15)) { c->ip += (uint16_t)d; V30_TICK(10); } return; }
        if (o >= 0xB0 && o < 0xB8) { *r8p(c, o & 7) = f8(c); return; }
        if (o >= 0xB8 && o < 0xC0) { c->r[o & 7] = f16(c); return; }
        bad(c, ip0, o);
    }
}

uint64_t v30_interp_count;

void v30_interpret(V30 *c) {
    do {
        if (c->trace) fprintf(stderr, "[interp] %04X:%04X\n", CS_, c->ip);
        step(c);
        v30_interp_count++;
        if (c->cycles <= 0 || (c->irq_pending && c->ifl)) v30_sync_point(c);
    } while (!v30_lookup(c) && c->sreg[V30_CS] != 0xF000);   /* F000: the C BIOS (dispatcher) */
}
