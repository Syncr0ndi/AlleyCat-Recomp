/* Running and checking hand-written routines against the recompiled originals. */
#include "hand.h"
#include "cat_gen.h"
#include <stdlib.h>
#include <string.h>

int cat_hand_mode = HAND_ON;

/* memory that a routine may change: PSP + program image + data segment, and CGA memory */
#define R0_LO 0x10000u
#define R0_HI 0x20300u
#define R1_LO 0xB8000u
#define R1_HI 0xBC000u
#define SNAP  ((R0_HI - R0_LO) + (R1_HI - R1_LO))

typedef struct { V30 cpu; uint8_t mem[SNAP]; } Snap;

static void take(Snap *s, const V30 *c) {
    s->cpu = *c;
    memcpy(s->mem, c->mem + R0_LO, R0_HI - R0_LO);
    memcpy(s->mem + (R0_HI - R0_LO), c->mem + R1_LO, R1_HI - R1_LO);
}
static void put(const Snap *s, V30 *c) {
    *c = s->cpu;
    memcpy(c->mem + R0_LO, s->mem, R0_HI - R0_LO);
    memcpy(c->mem + R1_LO, s->mem + (R0_HI - R0_LO), R1_HI - R1_LO);
}
static uint32_t snap_addr(unsigned i) { return i < R0_HI - R0_LO ? R0_LO + i : R1_LO + (i - (R0_HI - R0_LO)); }

typedef struct { unsigned calls, bad, flag_bad; } Stat;
static Stat stats[0x10000];
static unsigned printed;
static Snap *pool[8];
static int depth;

static void report_mismatch(unsigned off, const Snap *in, const Snap *g, const Snap *h, int flags_only) {
    if (printed++ >= 20) return;
    const V30 *i = &in->cpu, *a = &g->cpu, *b = &h->cpu;
    fprintf(stderr, "MISMATCH hand_%04x%s  in: AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X BP=%04X DS=%04X ES=%04X\n",
            off, flags_only ? " (flags only)" : "", i->r[0], i->r[3], i->r[1], i->r[2], i->r[6], i->r[7], i->r[5], i->sreg[3], i->sreg[0]);
    static const char *rn[8] = {"AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI"};
    for (int r = 0; r < 8; r++)
        if (a->r[r] != b->r[r]) fprintf(stderr, "   %s gen=%04X hand=%04X\n", rn[r], a->r[r], b->r[r]);
    for (int r = 0; r < 4; r++)
        if (a->sreg[r] != b->sreg[r]) fprintf(stderr, "   sreg%d gen=%04X hand=%04X\n", r, a->sreg[r], b->sreg[r]);
    if (a->ip != b->ip) fprintf(stderr, "   IP gen=%04X hand=%04X\n", a->ip, b->ip);
    if (a->cf != b->cf || a->zf != b->zf || a->sf != b->sf || a->of != b->of)
        fprintf(stderr, "   flags gen C%d Z%d S%d O%d  hand C%d Z%d S%d O%d\n", a->cf, a->zf, a->sf, a->of, b->cf, b->zf, b->sf, b->of);
    int shown = 0;
    for (unsigned k = 0; k < SNAP && shown < 8; k++)
        if (g->mem[k] != h->mem[k]) {
            fprintf(stderr, "   mem %05X: in=%02X gen=%02X hand=%02X\n", snap_addr(k), in->mem[k], g->mem[k], h->mem[k]);
            shown++;
        }
}

void cat_hand_call(V30 *c, unsigned off, cat_fn hand, cat_fn gen) {
    if (cat_hand_mode == HAND_GEN || depth >= 8) { gen(c); return; }
    if (cat_hand_mode == HAND_ON) { hand(c); c->ip = pop16(c); return; }
    /* check: run both from the same state, compare, keep the recompiled result */
    Stat *st = &stats[off];
    st->calls++;
    if (!pool[depth]) pool[depth] = malloc(sizeof(Snap) * 3);
    Snap *in = &pool[depth][0], *g = &pool[depth][1], *h = &pool[depth][2];
    uint8_t hold0 = c->irq_hold;
    depth++;
    c->irq_hold++;
    take(in, c);
    gen(c);
    take(g, c);
    put(in, c);
    hand(c); c->ip = pop16(c);
    take(h, c);
    depth--;
    int regs_bad = memcmp(g->cpu.r, h->cpu.r, sizeof g->cpu.r) || memcmp(g->cpu.sreg, h->cpu.sreg, sizeof g->cpu.sreg) ||
                   g->cpu.ip != h->cpu.ip || g->cpu.df != h->cpu.df;
    int mem_bad = memcmp(g->mem, h->mem, SNAP) != 0;
    int flags_bad = g->cpu.cf != h->cpu.cf || g->cpu.zf != h->cpu.zf || g->cpu.sf != h->cpu.sf || g->cpu.of != h->cpu.of;
    if (regs_bad || mem_bad) { st->bad++; report_mismatch(off, in, g, h, 0); }
    else if (flags_bad) { st->flag_bad++; }
    put(g, c);
    c->irq_hold = hold0;
}

void cat_call(V30 *c, uint16_t off) {
    uint16_t ret_sp = SP;
    push16(c, 0xFFFF);                     /* return address no routine uses */
    c->ip = off;
    c->sreg[V30_CS] = PC_LOAD_SEG + 0x723;
    v30_fn f = v30_lookup(c);
    if (!f) v30_fatal(c, "cat_call: no routine at %04X", off);
    f(c);
    while (!(c->ip == 0xFFFF && SP == ret_sp)) v30_dispatch(c);   /* unusual returns */
}

unsigned cat_hand_report(FILE *f) {
    unsigned total = 0;
    for (unsigned o = 0; o < 0x10000; o++) {
        Stat *s = &stats[o];
        if (!s->calls) continue;
        fprintf(f, "  hand_%04x: %7u calls checked, %u mismatches, %u flag-only differences\n", o, s->calls, s->bad, s->flag_bad);
        total += s->bad;
    }
    return total;
}
