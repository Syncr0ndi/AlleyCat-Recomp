/* 8086 CPU state and instruction semantics shared by recompiled code and the interpreter.
 * Taken from LOHT-Recomp (NEC V30, a superset of the 8086); the IBM PC memory map replaces the
 * M72 one: 1 MB of RAM, F0000-FFFFF is read-only and calls into F000 run the C BIOS.
 *
 * Execution model (see docs/RECOMP.md):
 *  - Every recompiled function is `void f(V30 *c)` and starts by switching on c->ip, so it can
 *    be entered at any of its labelled addresses.
 *  - The V30 stack is emulated faithfully. A recompiled CALL pushes the return IP, calls the
 *    target's C function and then checks that CS:IP equals the expected return address; if not
 *    (code that manipulates return addresses), it returns and lets the caller / dispatcher
 *    continue at CS:IP. RET pops IP and returns from the C function.
 *  - Jumps to code owned by another C function are tail calls (clang musttail).
 *  - Time advances through c->cycles; when it runs out the runtime's sync hook runs the rest
 *    of the machine and may deliver an interrupt.
 */
#pragma once
#ifndef V30_NO_LIBC
#include <stdint.h>
#include <stdbool.h>
#endif

typedef struct V30 V30;
typedef void (*v30_fn)(V30 *c);

enum { V30_ES = 0, V30_CS = 1, V30_SS = 2, V30_DS = 3 };
enum { V30_AX, V30_CX, V30_DX, V30_BX, V30_SP, V30_BP, V30_SI, V30_DI };

/* REP prefix kinds for string helpers */
enum { REP_NONE = 0, REP_Z = 1 /* F3 */, REP_NZ = 2 /* F2 */, REP_C = 3 /* 65 */, REP_NC = 4 /* 64 */ };

struct V30 {
    union { uint16_t r[8]; uint8_t b[16]; };   /* b[]: AL AH CL CH DL DH BL BH (little endian) */
    uint16_t sreg[4];
    uint16_t ip;
    uint8_t cf, pf, af, zf, sf, of, df, ifl, tf;
    uint8_t halted;
    int32_t cycles;                 /* budget left before the next sync */
    uint64_t total_cycles;          /* cycles retired at the last sync */
    uint8_t *mem;                   /* 1 MB linear address space */
    void *host;                     /* runtime / machine context */
    uint16_t (*io_in)(V30 *c, uint16_t port, int word);
    void (*io_out)(V30 *c, uint16_t port, uint16_t val, int word);
    void (*bios)(V30 *c, unsigned vec);  /* code at F000:vec (BIOS entry points) */
    void (*sync)(V30 *c);           /* advance the machine; may set irq_pending */
    int  (*irq_ack)(V30 *c);        /* returns interrupt vector number, or -1 */
    volatile uint8_t irq_pending;
    uint8_t irq_hold;               /* >0: interrupts wait (used while comparing routines) */
    uint8_t trace;
    int tail_depth;                 /* nested tail calls (compilers without musttail) */
};

/* 8-bit register index (AL CL DL BL AH CH DH BH) -> b[] slot */
#define V30_R8(i) (((i) & 3) * 2 + ((i) >> 2))

void v30_init(void);                       /* builds the dispatch table */

/* ---------------------------------------------------------------- memory */
static inline uint32_t v30_lin(uint16_t s, uint16_t o) { return (((uint32_t)s << 4) + o) & 0xFFFFF; }
static inline uint8_t rd8(V30 *c, uint16_t s, uint16_t o) { return c->mem[v30_lin(s, o)]; }
static inline uint16_t rd16(V30 *c, uint16_t s, uint16_t o) {
    return (uint16_t)(rd8(c, s, o) | (rd8(c, s, (uint16_t)(o + 1)) << 8));
}
static inline void wr8(V30 *c, uint16_t s, uint16_t o, uint8_t v) {
    uint32_t a = v30_lin(s, o);
    if (a >= 0xF0000) return;                             /* BIOS ROM */
    c->mem[a] = v;
}
static inline void wr16(V30 *c, uint16_t s, uint16_t o, uint16_t v) {
    wr8(c, s, o, (uint8_t)v); wr8(c, s, (uint16_t)(o + 1), (uint8_t)(v >> 8));
}
static inline void push16(V30 *c, uint16_t v) { c->r[V30_SP] -= 2; wr16(c, c->sreg[V30_SS], c->r[V30_SP], v); }
static inline uint16_t pop16(V30 *c) { uint16_t v = rd16(c, c->sreg[V30_SS], c->r[V30_SP]); c->r[V30_SP] += 2; return v; }

/* ---------------------------------------------------------------- flags */
static inline uint16_t v30_get_flags(V30 *c) {
    return (uint16_t)(0xF002 | c->cf | (c->pf << 2) | (c->af << 4) | (c->zf << 6) | (c->sf << 7) |
                      (c->tf << 8) | (c->ifl << 9) | (c->df << 10) | (c->of << 11));
}
static inline void v30_set_flags(V30 *c, uint16_t f) {
    c->cf = f & 1; c->pf = (f >> 2) & 1; c->af = (f >> 4) & 1; c->zf = (f >> 6) & 1; c->sf = (f >> 7) & 1;
    c->tf = (f >> 8) & 1; c->ifl = (f >> 9) & 1; c->df = (f >> 10) & 1; c->of = (f >> 11) & 1;
}
/* PF = 1 when the low byte has an even number of set bits (0x6996 = odd-parity bitmap of a nibble) */
static inline uint8_t v30_pf(uint8_t r) { return (uint8_t)(((0x6996u >> ((r ^ (r >> 4)) & 0xF)) & 1) ^ 1); }
static inline void szp8(V30 *c, uint8_t r) { c->zf = r == 0; c->sf = r >> 7; c->pf = v30_pf(r); }
static inline void szp16(V30 *c, uint16_t r) { c->zf = r == 0; c->sf = r >> 15; c->pf = v30_pf((uint8_t)r); }

/* ---------------------------------------------------------------- ALU */
static inline uint8_t add8(V30 *c, uint8_t a, uint8_t b, unsigned ci) {
    unsigned r = a + b + ci; uint8_t r8 = (uint8_t)r;
    c->cf = (r >> 8) & 1; c->of = (((a ^ r8) & (b ^ r8)) >> 7) & 1; c->af = ((a ^ b ^ r8) >> 4) & 1; szp8(c, r8); return r8;
}
static inline uint16_t add16(V30 *c, uint16_t a, uint16_t b, unsigned ci) {
    uint32_t r = (uint32_t)a + b + ci; uint16_t r16 = (uint16_t)r;
    c->cf = (r >> 16) & 1; c->of = (((a ^ r16) & (b ^ r16)) >> 15) & 1; c->af = ((a ^ b ^ r16) >> 4) & 1; szp16(c, r16); return r16;
}
static inline uint8_t sub8(V30 *c, uint8_t a, uint8_t b, unsigned bi) {
    unsigned r = (unsigned)a - b - bi; uint8_t r8 = (uint8_t)r;
    c->cf = (r >> 8) & 1; c->of = (((a ^ b) & (a ^ r8)) >> 7) & 1; c->af = ((a ^ b ^ r8) >> 4) & 1; szp8(c, r8); return r8;
}
static inline uint16_t sub16(V30 *c, uint16_t a, uint16_t b, unsigned bi) {
    uint32_t r = (uint32_t)a - b - bi; uint16_t r16 = (uint16_t)r;
    c->cf = (r >> 16) & 1; c->of = (((a ^ b) & (a ^ r16)) >> 15) & 1; c->af = ((a ^ b ^ r16) >> 4) & 1; szp16(c, r16); return r16;
}
#define V30_LOGIC(name, op, T, szp)                                              \
    static inline T name(V30 *c, T a, T b) { T r = (T)(a op b); c->cf = 0; c->of = 0; c->af = 0; szp(c, r); return r; }
V30_LOGIC(and8, &, uint8_t, szp8)  V30_LOGIC(or8, |, uint8_t, szp8)  V30_LOGIC(xor8, ^, uint8_t, szp8)
V30_LOGIC(and16, &, uint16_t, szp16) V30_LOGIC(or16, |, uint16_t, szp16) V30_LOGIC(xor16, ^, uint16_t, szp16)

static inline uint8_t inc8(V30 *c, uint8_t a) { uint8_t r = a + 1; c->of = r == 0x80; c->af = (r & 0xF) == 0; szp8(c, r); return r; }
static inline uint8_t dec8(V30 *c, uint8_t a) { uint8_t r = a - 1; c->of = r == 0x7F; c->af = (r & 0xF) == 0xF; szp8(c, r); return r; }
static inline uint16_t inc16(V30 *c, uint16_t a) { uint16_t r = a + 1; c->of = r == 0x8000; c->af = (r & 0xF) == 0; szp16(c, r); return r; }
static inline uint16_t dec16(V30 *c, uint16_t a) { uint16_t r = a - 1; c->of = r == 0x7FFF; c->af = (r & 0xF) == 0xF; szp16(c, r); return r; }
static inline uint8_t neg8(V30 *c, uint8_t a) { return sub8(c, 0, a, 0); }
static inline uint16_t neg16(V30 *c, uint16_t a) { return sub16(c, 0, a, 0); }

/* shifts/rotates (count already masked by the caller where appropriate) */
uint8_t  v30_shift8(V30 *c, int op, uint8_t v, unsigned n);
uint16_t v30_shift16(V30 *c, int op, uint16_t v, unsigned n);
enum { SH_ROL, SH_ROR, SH_RCL, SH_RCR, SH_SHL, SH_SHR, SH_SAL, SH_SAR };

/* multiply / divide (divide returns false on divide error) */
void v30_mul8(V30 *c, uint8_t v);   void v30_imul8(V30 *c, uint8_t v);
void v30_mul16(V30 *c, uint16_t v); void v30_imul16(V30 *c, uint16_t v);
uint16_t v30_imul3(V30 *c, uint16_t a, uint16_t b);
bool v30_div8(V30 *c, uint8_t v);   bool v30_idiv8(V30 *c, uint8_t v);
bool v30_div16(V30 *c, uint16_t v); bool v30_idiv16(V30 *c, uint16_t v);

/* decimal adjust */
void v30_daa(V30 *c); void v30_das(V30 *c); void v30_aaa(V30 *c); void v30_aas(V30 *c);
bool v30_aam(V30 *c, uint8_t base); void v30_aad(V30 *c, uint8_t base);

/* NEC extensions */
static inline void test1_8(V30 *c, uint8_t v, unsigned bit) { c->zf = ((v >> (bit & 7)) & 1) == 0; c->cf = 0; c->of = 0; }
static inline void test1_16(V30 *c, uint16_t v, unsigned bit) { c->zf = ((v >> (bit & 15)) & 1) == 0; c->cf = 0; c->of = 0; }
#define SET1_8(v, bit)  ((uint8_t)((v) | (1u << ((bit) & 7))))
#define CLR1_8(v, bit)  ((uint8_t)((v) & ~(1u << ((bit) & 7))))
#define NOT1_8(v, bit)  ((uint8_t)((v) ^ (1u << ((bit) & 7))))
#define SET1_16(v, bit) ((uint16_t)((v) | (1u << ((bit) & 15))))
#define CLR1_16(v, bit) ((uint16_t)((v) & ~(1u << ((bit) & 15))))
#define NOT1_16(v, bit) ((uint16_t)((v) ^ (1u << ((bit) & 15))))
void v30_ins(V30 *c, int reg_off, unsigned len);   /* reg_off = b[] slot holding the bit offset */
void v30_ext(V30 *c, int reg_off, unsigned len);
void v30_add4s(V30 *c, int seg); void v30_sub4s(V30 *c, int seg); void v30_cmp4s(V30 *c, int seg);
uint8_t v30_rol4(V30 *c, uint8_t m); uint8_t v30_ror4(V30 *c, uint8_t m);

/* string instructions: seg = source segment register index (override or DS) */
void v30_movs(V30 *c, int w, int seg, int rep); void v30_cmps(V30 *c, int w, int seg, int rep);
void v30_stos(V30 *c, int w, int rep);          void v30_lods(V30 *c, int w, int seg, int rep);
void v30_scas(V30 *c, int w, int rep);
void v30_ins_io(V30 *c, int w, int rep);        void v30_outs_io(V30 *c, int w, int seg, int rep);

void v30_enter(V30 *c, uint16_t size, uint8_t level);

/* ---------------------------------------------------------------- control */
static inline uint16_t v30_in(V30 *c, uint16_t port, int w) { return c->io_in(c, port, w); }
static inline void v30_out(V30 *c, uint16_t port, uint16_t v, int w) { c->io_out(c, port, v, w); }

void v30_dispatch(V30 *c);                 /* run code at CS:IP until its C function returns */
void v30_sync_point(V30 *c);               /* cycles exhausted / STI: advance machine, take IRQs */
void v30_int(V30 *c, unsigned vec);        /* software/exception interrupt; returns after IRET */
void v30_halt(V30 *c);
void v30_bad_entry(V30 *c, const char *fn);
void v30_unimplemented(V30 *c, const char *what);
#ifndef V30_NO_LIBC
/* Unrecoverable error: builds a report (message, registers, recent addresses), calls
 * v30_fatal_hook if set (the game writes loht_crash.log and shows a message), then exits. */
void v30_fatal(V30 *c, const char *fmt, ...);
extern void (*v30_fatal_hook)(const char *report);
#endif
void v30_run(V30 *c);                      /* top-level loop, never returns */
void v30_reset(V30 *c);

/* entry table emitted by the recompiler (linked in, or produced by the run-time compiler) */
typedef struct { uint32_t lin; uint16_t cs, ip; v30_fn fn; } v30_entry;
void v30_set_entries(const v30_entry *entries, unsigned count);   /* call before v30_init */

/* fallback for code that was not recompiled (e.g. code the MCU writes into shared RAM).
 * Executes from CS:IP until it reaches an address with a recompiled entry. */
void v30_interpret(V30 *c);
v30_fn v30_lookup(V30 *c);                /* recompiled function for CS:IP, or NULL */

#define V30_TICK(n) (c->cycles -= (n))
