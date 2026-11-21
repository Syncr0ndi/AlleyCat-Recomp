// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
/* NEC V30 runtime core: non-inline instruction semantics, interrupts and dispatch. */
#include "v30.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define CODE_SPACE 0x100000          /* code may live anywhere in the 1 MB space */
static uint32_t recent[32];          /* last dispatched CS:IP pairs, for crash reports */
static unsigned recent_pos;


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

/* ---------------------------------------------------------------- shifts */
uint8_t v30_shift8(V30 *c, int op, uint8_t v, unsigned n) {
    if (!n) return v;
    unsigned r = v;
    for (unsigned i = 0; i < n; i++) {
        unsigned o = c->cf;
        switch (op) {
        case SH_ROL: c->cf = (r >> 7) & 1; r = ((r << 1) | c->cf) & 0xFF; break;
        case SH_ROR: c->cf = r & 1; r = (r >> 1) | (c->cf << 7); break;
        case SH_RCL: c->cf = (r >> 7) & 1; r = ((r << 1) | o) & 0xFF; break;
        case SH_RCR: c->cf = r & 1; r = (r >> 1) | (o << 7); break;
        case SH_SHL: case SH_SAL: c->cf = (r >> 7) & 1; r = (r << 1) & 0xFF; break;
        case SH_SHR: c->cf = r & 1; r >>= 1; break;
        case SH_SAR: c->cf = r & 1; r = (r >> 1) | (r & 0x80); break;
        }
    }
    switch (op) {
    case SH_ROL: case SH_RCL: case SH_SHL: case SH_SAL: c->of = ((r >> 7) & 1) ^ c->cf; break;
    case SH_ROR: case SH_RCR: c->of = ((r >> 7) ^ (r >> 6)) & 1; break;
    case SH_SHR: c->of = (v >> 7) & 1; break;
    case SH_SAR: c->of = 0; break;
    }
    if (op >= SH_SHL) { szp8(c, (uint8_t)r); c->af = 0; }
    return (uint8_t)r;
}

uint16_t v30_shift16(V30 *c, int op, uint16_t v, unsigned n) {
    if (!n) return v;
    unsigned r = v;
    for (unsigned i = 0; i < n; i++) {
        unsigned o = c->cf;
        switch (op) {
        case SH_ROL: c->cf = (r >> 15) & 1; r = ((r << 1) | c->cf) & 0xFFFF; break;
        case SH_ROR: c->cf = r & 1; r = (r >> 1) | (c->cf << 15); break;
        case SH_RCL: c->cf = (r >> 15) & 1; r = ((r << 1) | o) & 0xFFFF; break;
        case SH_RCR: c->cf = r & 1; r = (r >> 1) | (o << 15); break;
        case SH_SHL: case SH_SAL: c->cf = (r >> 15) & 1; r = (r << 1) & 0xFFFF; break;
        case SH_SHR: c->cf = r & 1; r >>= 1; break;
        case SH_SAR: c->cf = r & 1; r = (r >> 1) | (r & 0x8000); break;
        }
    }
    switch (op) {
    case SH_ROL: case SH_RCL: case SH_SHL: case SH_SAL: c->of = ((r >> 15) & 1) ^ c->cf; break;
    case SH_ROR: case SH_RCR: c->of = ((r >> 15) ^ (r >> 14)) & 1; break;
    case SH_SHR: c->of = (v >> 15) & 1; break;
    case SH_SAR: c->of = 0; break;
    }
    if (op >= SH_SHL) { szp16(c, (uint16_t)r); c->af = 0; }
    return (uint16_t)r;
}

/* ---------------------------------------------------------------- mul / div */
void v30_mul8(V30 *c, uint8_t v) { AX = (uint16_t)(AL * v); c->cf = c->of = AH != 0; }
void v30_imul8(V30 *c, uint8_t v) {
    int16_t r = (int16_t)((int8_t)AL * (int8_t)v); AX = (uint16_t)r; c->cf = c->of = r != (int8_t)r;
}
void v30_mul16(V30 *c, uint16_t v) {
    uint32_t r = (uint32_t)AX * v; AX = (uint16_t)r; DX = (uint16_t)(r >> 16); c->cf = c->of = DX != 0;
}
void v30_imul16(V30 *c, uint16_t v) {
    int32_t r = (int32_t)(int16_t)AX * (int16_t)v; AX = (uint16_t)r; DX = (uint16_t)((uint32_t)r >> 16);
    c->cf = c->of = r != (int16_t)r;
}
uint16_t v30_imul3(V30 *c, uint16_t a, uint16_t b) {
    int32_t r = (int32_t)(int16_t)a * (int16_t)b; c->cf = c->of = r != (int16_t)r; return (uint16_t)r;
}
bool v30_div8(V30 *c, uint8_t v) {
    if (!v) return false;
    unsigned q = AX / v; if (q > 0xFF) return false;
    AH = (uint8_t)(AX % v); AL = (uint8_t)q; return true;
}
bool v30_idiv8(V30 *c, uint8_t v) {
    if (!v) return false;
    int n = (int16_t)AX, d = (int8_t)v, q = n / d;
    if (q > 127 || q < -128) return false;
    AH = (uint8_t)(n % d); AL = (uint8_t)q; return true;
}
bool v30_div16(V30 *c, uint16_t v) {
    if (!v) return false;
    uint32_t n = ((uint32_t)DX << 16) | AX, q = n / v; if (q > 0xFFFF) return false;
    DX = (uint16_t)(n % v); AX = (uint16_t)q; return true;
}
bool v30_idiv16(V30 *c, uint16_t v) {
    if (!v) return false;
    int32_t n = (int32_t)(((uint32_t)DX << 16) | AX), d = (int16_t)v, q = n / d;
    if (q > 32767 || q < -32768) return false;
    DX = (uint16_t)(n % d); AX = (uint16_t)q; return true;
}

/* ---------------------------------------------------------------- BCD */
void v30_daa(V30 *c) {
    uint8_t old = AL; unsigned ocf = c->cf;
    if ((AL & 0xF) > 9 || c->af) { c->cf = ocf | (AL > 0xF9); AL += 6; c->af = 1; } else c->af = 0;
    if (old > 0x99 || ocf) { AL += 0x60; c->cf = 1; } else c->cf = 0;
    szp8(c, AL);
}
void v30_das(V30 *c) {
    uint8_t old = AL; unsigned ocf = c->cf;
    if ((AL & 0xF) > 9 || c->af) { c->cf = ocf | (AL < 6); AL -= 6; c->af = 1; } else c->af = 0;
    if (old > 0x99 || ocf) { AL -= 0x60; c->cf = 1; }
    szp8(c, AL);
}
void v30_aaa(V30 *c) {
    if ((AL & 0xF) > 9 || c->af) { AX += 0x106; c->af = c->cf = 1; } else c->af = c->cf = 0;
    AL &= 0xF;
}
void v30_aas(V30 *c) {
    if ((AL & 0xF) > 9 || c->af) { AL -= 6; AH -= 1; c->af = c->cf = 1; } else c->af = c->cf = 0;
    AL &= 0xF;
}
bool v30_aam(V30 *c, uint8_t base) {
    if (!base) return false;
    AH = AL / base; AL = AL % base; szp8(c, AL); return true;
}
void v30_aad(V30 *c, uint8_t base) { AL = (uint8_t)(AH * base + AL); AH = 0; szp8(c, AL); }

/* ---------------------------------------------------------------- NEC extensions */
void v30_ext(V30 *c, int slot, unsigned len) {
    unsigned off = c->b[slot] & 0xF; len = (len & 0xF) + 1;
    uint32_t w = rd16(c, c->sreg[V30_DS], SI) | ((uint32_t)rd16(c, c->sreg[V30_DS], (uint16_t)(SI + 2)) << 16);
    AX = (uint16_t)((w >> off) & ((1u << len) - 1));
    off += len; if (off > 15) { SI += 2; off -= 16; }
    c->b[slot] = (uint8_t)off;
}
void v30_ins(V30 *c, int slot, unsigned len) {
    unsigned off = c->b[slot] & 0xF; len = (len & 0xF) + 1;
    uint16_t es = c->sreg[V30_ES];
    uint32_t w = rd16(c, es, DI) | ((uint32_t)rd16(c, es, (uint16_t)(DI + 2)) << 16);
    uint32_t mask = ((1u << len) - 1) << off;
    w = (w & ~mask) | (((uint32_t)AX << off) & mask);
    wr16(c, es, DI, (uint16_t)w); wr16(c, es, (uint16_t)(DI + 2), (uint16_t)(w >> 16));
    off += len; if (off > 15) { DI += 2; off -= 16; }
    c->b[slot] = (uint8_t)off;
}
static void bcd_str(V30 *c, int seg, int mode /*0 add,1 sub,2 cmp*/) {
    unsigned n = (CL + 1u) / 2, carry = 0, zero = 1;
    for (unsigned i = 0; i < n; i++) {
        uint8_t s = rd8(c, c->sreg[seg], (uint16_t)(SI + i)), d = rd8(c, c->sreg[V30_ES], (uint16_t)(DI + i));
        int lo, hi;
        if (mode == 0) {
            lo = (s & 0xF) + (d & 0xF) + carry; carry = lo > 9; if (carry) lo -= 10;
            hi = (s >> 4) + (d >> 4) + carry; carry = hi > 9; if (carry) hi -= 10;
        } else {
            lo = (d & 0xF) - (s & 0xF) - (int)carry; carry = lo < 0; if (carry) lo += 10;
            hi = (d >> 4) - (s >> 4) - (int)carry; carry = hi < 0; if (carry) hi += 10;
        }
        uint8_t r = (uint8_t)((hi << 4) | lo);
        if (r) zero = 0;
        if (mode != 2) wr8(c, c->sreg[V30_ES], (uint16_t)(DI + i), r);
    }
    c->cf = carry; c->zf = zero;
    V30_TICK(n * 19);
}
void v30_add4s(V30 *c, int seg) { bcd_str(c, seg, 0); }
void v30_sub4s(V30 *c, int seg) { bcd_str(c, seg, 1); }
void v30_cmp4s(V30 *c, int seg) { bcd_str(c, seg, 2); }
uint8_t v30_rol4(V30 *c, uint8_t m) { uint8_t r = (uint8_t)((m << 4) | (AL & 0xF)); AL = (uint8_t)((AL & 0xF0) | (m >> 4)); return r; }
uint8_t v30_ror4(V30 *c, uint8_t m) { uint8_t r = (uint8_t)(((AL & 0xF) << 4) | (m >> 4)); AL = (uint8_t)((AL & 0xF0) | (m & 0xF)); return r; }

/* ---------------------------------------------------------------- strings
 * Per-element costs are 4.77 MHz 8088 cycles (8086 timings + 4 per word on the 8-bit bus). */
#define STEP(w) ((uint16_t)(c->df ? -(1 + (w)) : (1 + (w))))
#define REPEAT(body, per)                                                      \
    do {                                                                       \
        if (rep == REP_NONE) { body; V30_TICK(per - 2); break; }               \
        V30_TICK(9);               \
        while (CX) { body; CX--; V30_TICK(per); COND }                         \
    } while (0)
#define COND
void v30_movs(V30 *c, int w, int seg, int rep) {
    REPEAT({ if (w) wr16(c, c->sreg[V30_ES], DI, rd16(c, c->sreg[seg], SI));
             else wr8(c, c->sreg[V30_ES], DI, rd8(c, c->sreg[seg], SI));
             SI += STEP(w); DI += STEP(w); }, w ? 25 : 17);
}
void v30_stos(V30 *c, int w, int rep) {
    REPEAT({ if (w) wr16(c, c->sreg[V30_ES], DI, AX); else wr8(c, c->sreg[V30_ES], DI, AL); DI += STEP(w); }, w ? 14 : 10);
}
void v30_lods(V30 *c, int w, int seg, int rep) {
    REPEAT({ if (w) AX = rd16(c, c->sreg[seg], SI); else AL = rd8(c, c->sreg[seg], SI); SI += STEP(w); }, w ? 17 : 13);
}
void v30_ins_io(V30 *c, int w, int rep) {
    REPEAT({ uint16_t v = v30_in(c, DX, w); if (w) wr16(c, c->sreg[V30_ES], DI, v); else wr8(c, c->sreg[V30_ES], DI, (uint8_t)v);
             DI += STEP(w); }, 8);
}
void v30_outs_io(V30 *c, int w, int seg, int rep) {
    REPEAT({ v30_out(c, DX, w ? rd16(c, c->sreg[seg], SI) : rd8(c, c->sreg[seg], SI), w); SI += STEP(w); }, 8);
}
#undef COND
#define COND                                                                   \
    if ((rep == REP_Z && !c->zf) || (rep == REP_NZ && c->zf) ||                \
        (rep == REP_C && !c->cf) || (rep == REP_NC && c->cf)) break;
void v30_cmps(V30 *c, int w, int seg, int rep) {
    REPEAT({ if (w) sub16(c, rd16(c, c->sreg[seg], SI), rd16(c, c->sreg[V30_ES], DI), 0);
             else sub8(c, rd8(c, c->sreg[seg], SI), rd8(c, c->sreg[V30_ES], DI), 0);
             SI += STEP(w); DI += STEP(w); }, w ? 30 : 22);
}
void v30_scas(V30 *c, int w, int rep) {
    REPEAT({ if (w) sub16(c, AX, rd16(c, c->sreg[V30_ES], DI), 0); else sub8(c, AL, rd8(c, c->sreg[V30_ES], DI), 0);
             DI += STEP(w); }, w ? 19 : 15);
}
#undef COND

void v30_enter(V30 *c, uint16_t size, uint8_t level) {
    push16(c, BP);
    uint16_t frame = SP;
    level &= 0x1F;
    for (unsigned i = 1; i < level; i++) { BP -= 2; push16(c, rd16(c, c->sreg[V30_SS], BP)); }
    if (level) push16(c, frame);
    BP = frame; SP -= size;
}

/* ---------------------------------------------------------------- dispatch */
static v30_fn *fn_tab;
static uint16_t *cs_tab;
static const v30_entry *ent_tab;
static unsigned ent_count;

void v30_set_entries(const v30_entry *entries, unsigned count) { ent_tab = entries; ent_count = count; }

void v30_init(void) {
    fn_tab = calloc(CODE_SPACE, sizeof *fn_tab);
    cs_tab = calloc(CODE_SPACE, sizeof *cs_tab);
    for (unsigned i = 0; i < ent_count; i++) {
        fn_tab[ent_tab[i].lin] = ent_tab[i].fn;
        cs_tab[ent_tab[i].lin] = ent_tab[i].cs;
    }
}

static inline uint32_t canon(uint32_t l) { return l; }

v30_fn v30_lookup(V30 *c) {
    uint32_t l = canon(v30_lin(c->sreg[V30_CS], c->ip));
    return l < CODE_SPACE ? fn_tab[l] : NULL;
}

void v30_dispatch(V30 *c) {
    if (c->cycles <= 0 || (c->irq_pending && c->ifl)) v30_sync_point(c);
    if (c->sreg[V30_CS] == 0xF000) {            /* BIOS: run the C version, then IRET */
        c->bios(c, c->ip);
        c->ip = pop16(c); c->sreg[V30_CS] = pop16(c); v30_set_flags(c, pop16(c));
        return;
    }
    uint32_t l = canon(v30_lin(c->sreg[V30_CS], c->ip));
    v30_fn f = l < CODE_SPACE ? fn_tab[l] : NULL;
    if (!f) { v30_interpret(c); return; }
    uint16_t cs = cs_tab[l];
    if (cs != c->sreg[V30_CS]) { c->sreg[V30_CS] = cs; c->ip = (uint16_t)(l - ((uint32_t)cs << 4)); }
    recent[recent_pos++ & 31] = ((uint32_t)c->sreg[V30_CS] << 16) | c->ip;
    if (c->trace) fprintf(stderr, "[v30] %04X:%04X\n", c->sreg[V30_CS], c->ip);
    f(c);
}

void v30_int(V30 *c, unsigned vec) {
    uint16_t rcs = c->sreg[V30_CS], rip = c->ip, sp0 = SP;
    push16(c, v30_get_flags(c));
    c->ifl = 0; c->tf = 0;
    push16(c, rcs); push16(c, rip);
    c->ip = rd16(c, 0, (uint16_t)(vec * 4));
    c->sreg[V30_CS] = rd16(c, 0, (uint16_t)(vec * 4 + 2));
    V30_TICK(50);
    while (!(c->sreg[V30_CS] == rcs && c->ip == rip && SP == sp0)) v30_dispatch(c);
}

void v30_sync_point(V30 *c) {
    if (c->cycles <= 0) c->sync(c);
    if (c->irq_pending && c->ifl && !c->irq_hold) {
        int v = c->irq_ack(c);
        if (v >= 0) { c->halted = 0; v30_int(c, (unsigned)v); }
    }
}

void v30_halt(V30 *c) {
    c->halted = 1;
    while (c->halted) { c->cycles = 0; v30_sync_point(c); }
}

/* ---------------------------------------------------------------- fatal errors */
void (*v30_fatal_hook)(const char *report);

void v30_fatal(V30 *c, const char *fmt, ...) {
    char report[4096];
    size_t n = 0;
    va_list ap;
    va_start(ap, fmt);
    n += (size_t)vsnprintf(report + n, sizeof report - n, fmt, ap);
    va_end(ap);
    n += (size_t)snprintf(report + n, sizeof report - n,
        "\n\nCS:IP %04X:%04X  AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X BP=%04X SP=%04X\n"
        "DS=%04X ES=%04X SS=%04X  flags=%04X  cycles=%llu\nrecent dispatches (oldest first):",
        c->sreg[V30_CS], c->ip, AX, BX, CX, DX, SI, DI, BP, SP, c->sreg[V30_DS], c->sreg[V30_ES], c->sreg[V30_SS],
        v30_get_flags(c), (unsigned long long)c->total_cycles);
    for (unsigned i = 0; i < 32 && n < sizeof report - 16; i++) {
        uint32_t r = recent[(recent_pos + i) & 31];
        if (r) n += (size_t)snprintf(report + n, sizeof report - n, "%s%04X:%04X", i % 8 ? " " : "\n  ", r >> 16, r & 0xFFFF);
    }
    fprintf(stderr, "v30 fatal: %s\n", report);
    if (v30_fatal_hook) v30_fatal_hook(report);
    exit(3);
}

void v30_bad_entry(V30 *c, const char *fn) {
    v30_fatal(c, "Recompiled function %s was entered at an unknown address %04X:%04X.", fn, c->sreg[V30_CS], c->ip);
}
void v30_unimplemented(V30 *c, const char *what) {
    v30_fatal(c, "Unimplemented instruction %s at %04X:%04X.", what, c->sreg[V30_CS], c->ip);
}

void v30_reset(V30 *c) {
    for (int i = 0; i < 8; i++) c->r[i] = 0;
    for (int i = 0; i < 4; i++) c->sreg[i] = 0;
    v30_set_flags(c, 0);
    c->sreg[V30_CS] = 0xFFFF; c->ip = 0;
    c->halted = 0; c->irq_pending = 0;
}

void v30_run(V30 *c) { for (;;) v30_dispatch(c); }
