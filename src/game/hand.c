/* Running and checking hand-written routines against the recompiled originals. */
#include "hand.h"
#include "cat_gen.h"
#include <stdlib.h>
#include <string.h>

int cat_hand_mode = HAND_ON;
uint32_t cat_cov[0x10000];

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

/* differing register / flag bits, in tools/liveness.py order:
 * AL AH CL CH DL DH BL BH SP BP SI DI ES CS SS DS CF ZF SF OF PF AF DF */
static uint32_t state_diff(const V30 *a, const V30 *b) {
    uint32_t d = 0;
    for (int i = 0; i < 8; i++) if (a->b[i] != b->b[i]) d |= 1u << i;
    for (int i = 4; i < 8; i++) if (a->r[i] != b->r[i]) d |= 1u << (i + 4);
    for (int i = 0; i < 4; i++) if (a->sreg[i] != b->sreg[i]) d |= 1u << (12 + i);
    const uint8_t fa[7] = {a->cf, a->zf, a->sf, a->of, a->pf, a->af, a->df};
    const uint8_t fb[7] = {b->cf, b->zf, b->sf, b->of, b->pf, b->af, b->df};
    for (int i = 0; i < 7; i++) if (fa[i] != fb[i]) d |= 1u << (16 + i);
    return d;
}
static const char *const bit_name[23] = {"AL", "AH", "CL", "CH", "DL", "DH", "BL", "BH", "SP", "BP", "SI", "DI",
                                         "ES", "CS", "SS", "DS", "CF", "ZF", "SF", "OF", "PF", "AF", "DF"};
static uint32_t live_out(unsigned off) {
    static uint32_t *tab;
    if (!tab) {
        tab = calloc(0x10000, sizeof *tab);
        for (unsigned i = 0; i < 0x10000; i++) tab[i] = 0x7FFFFF;          /* unknown: everything */
        for (unsigned i = 0; i < cat_live_out_count; i++) tab[cat_live_out[i][0]] = cat_live_out[i][1];
    }
    return tab[off] | (1u << 8) | (1u << 14);                                /* SP and SS always */
}
/* memory differences, ignoring the dead stack below SP */
static int mem_diff(const Snap *g, const Snap *h, unsigned *first) {
    uint32_t dead_lo = (uint32_t)g->cpu.sreg[V30_SS] << 4, dead_hi = dead_lo + g->cpu.r[V30_SP];
    for (unsigned k = 0; k < SNAP; k++)
        if (g->mem[k] != h->mem[k]) {
            uint32_t a = snap_addr(k);
            if (a >= dead_lo && a < dead_hi) continue;
            if (first) *first = k;
            return 1;
        }
    return 0;
}

typedef struct { unsigned calls, bad, flag_bad; } Stat;
static Stat stats[0x10000];
static unsigned printed;
static Snap *pool[8];
static int depth;
int cat_hand_busy(void) { return depth != 0; }

static void report_mismatch(unsigned off, const Snap *in, const Snap *g, const Snap *h, int flags_only) {
    if (printed++ >= 20) return;
    const V30 *i = &in->cpu, *a = &g->cpu, *b = &h->cpu;
    fprintf(stderr, "MISMATCH hand_%04x%s  in: AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X BP=%04X DS=%04X ES=%04X\n",
            off, flags_only ? " (flags only)" : "", i->r[0], i->r[3], i->r[1], i->r[2], i->r[6], i->r[7], i->r[5], i->sreg[3], i->sreg[0]);
    uint32_t d = state_diff(a, b) & live_out(off);
    for (int k = 0; k < 23; k++)
        if (d >> k & 1) {
            unsigned va = k < 8 ? a->b[k] : k < 12 ? a->r[k - 4] : k < 16 ? a->sreg[k - 12] : 0;
            unsigned vb = k < 8 ? b->b[k] : k < 12 ? b->r[k - 4] : k < 16 ? b->sreg[k - 12] : 0;
            if (k < 16) fprintf(stderr, "   %s gen=%04X hand=%04X\n", bit_name[k], va, vb);
            else fprintf(stderr, "   flag %s differs (gen C%d Z%d S%d O%d, hand C%d Z%d S%d O%d)\n", bit_name[k],
                         a->cf, a->zf, a->sf, a->of, b->cf, b->zf, b->sf, b->of);
        }
    if (a->ip != b->ip) fprintf(stderr, "   IP gen=%04X hand=%04X\n", a->ip, b->ip);
    int shown = 0;
    uint32_t dead_lo = (uint32_t)a->sreg[V30_SS] << 4, dead_hi = dead_lo + a->r[V30_SP];
    for (unsigned k = 0; k < SNAP && shown < 8; k++)
        if (g->mem[k] != h->mem[k] && !(snap_addr(k) >= dead_lo && snap_addr(k) < dead_hi)) {
            fprintf(stderr, "   mem %05X: in=%02X gen=%02X hand=%02X\n", snap_addr(k), in->mem[k], g->mem[k], h->mem[k]);
            shown++;
        }
}

/* ---- I/O and BIOS record / replay while checking ----
 * The recompiled run records every port access and every BIOS call; the hand-written run
 * gets the recorded port reads back, has its port writes compared instead of performed,
 * gets the recorded INT 1Ah results (time) and runs other BIOS calls for real (they only
 * touch memory, which is restored in between). The machine does not advance while the
 * hand-written version runs. */
typedef struct { uint8_t kind; uint16_t port; uint16_t val; uint16_t ax, cx, dx; } IoEv;   /* kind: 1 in, 2 out, 3 bios */
static IoEv io_log[8192];
static unsigned io_n, io_pos, io_bad;
static char io_why[160];
static uint16_t (*real_in)(V30 *, uint16_t, int);
static void (*real_out)(V30 *, uint16_t, uint16_t, int);
static void (*real_bios)(V30 *, unsigned);
static void (*real_sync)(V30 *);

static void io_fail(const char *fmt, unsigned a, unsigned b) {
    if (!io_bad++) snprintf(io_why, sizeof io_why, fmt, a, b);
}
static uint16_t rec_in(V30 *c, uint16_t port, int w) {
    uint16_t v = real_in(c, port, w);
    if (io_n < 8192) io_log[io_n++] = (IoEv){1, port, v, 0, 0, 0};
    return v;
}
static void rec_out(V30 *c, uint16_t port, uint16_t v, int w) {
    real_out(c, port, v, w);
    if (io_n < 8192) io_log[io_n++] = (IoEv){2, port, v, 0, 0, 0};
}
static void rec_bios(V30 *c, unsigned vec) {
    real_bios(c, vec);
    if (io_n < 8192) io_log[io_n++] = (IoEv){3, (uint16_t)vec, 0, AX, CX, DX};
}
static uint16_t rep_in(V30 *c, uint16_t port, int w) {
    (void)c; (void)w;
    if (io_pos >= io_n || io_log[io_pos].kind != 1 || io_log[io_pos].port != port) { io_fail("unexpected IN %03X (event %u)", port, io_pos); return 0xFF; }
    return io_log[io_pos++].val;
}
static void rep_out(V30 *c, uint16_t port, uint16_t v, int w) {
    (void)c; (void)w;
    if (io_pos >= io_n || io_log[io_pos].kind != 2 || io_log[io_pos].port != port || io_log[io_pos].val != v) {
        io_fail("unexpected OUT %03X,%02X", port, v); return;
    }
    io_pos++;
}
static void rep_bios(V30 *c, unsigned vec) {
    if (io_pos >= io_n || io_log[io_pos].kind != 3 || io_log[io_pos].port != vec) { io_fail("unexpected INT %02X (event %u)", vec, io_pos); return; }
    if (vec == 0x1A) { AX = io_log[io_pos].ax; CX = io_log[io_pos].cx; DX = io_log[io_pos].dx; }
    else real_bios(c, vec);
    io_pos++;
}
static void rep_sync(V30 *c) { c->cycles = 400; }

static int in_hand_run;

void cat_hand_call(V30 *c, unsigned off, cat_fn hand, cat_fn gen) {
    if (cat_hand_mode == HAND_GEN) { gen(c); return; }
    if (cat_hand_mode == HAND_ON || (depth && in_hand_run)) { hand(c); c->ip = pop16(c); return; }
    if (depth) { gen(c); return; }        /* inside the recompiled run of an outer check */
    /* check: run both from the same state, compare, keep the recompiled result */
    Stat *st = &stats[off];
    st->calls++;
    if (!pool[0]) pool[0] = malloc(sizeof(Snap) * 3);
    Snap *in = &pool[0][0], *g = &pool[0][1], *h = &pool[0][2];
    uint8_t hold0 = c->irq_hold;
    real_in = c->io_in; real_out = c->io_out; real_bios = c->bios; real_sync = c->sync;
    depth++;
    c->irq_hold++;
    take(in, c);
    io_n = 0; io_pos = 0; io_bad = 0;
    c->io_in = rec_in; c->io_out = rec_out; c->bios = rec_bios;
    gen(c);
    c->io_in = real_in; c->io_out = real_out; c->bios = real_bios;
    take(g, c);
    put(in, c);
    c->io_in = rep_in; c->io_out = rep_out; c->bios = rep_bios; c->sync = rep_sync;
    in_hand_run = 1;
    hand(c); c->ip = pop16(c);
    in_hand_run = 0;
    c->io_in = real_in; c->io_out = real_out; c->bios = real_bios; c->sync = real_sync;
    if (io_pos != io_n) io_fail("hand version did %u of %u recorded I/O events", io_pos, io_n);
    take(h, c);
    depth--;
    uint32_t d = state_diff(&g->cpu, &h->cpu), live = live_out(off);
    int regs_bad = (d & live & 0xFFFF) || g->cpu.ip != h->cpu.ip || (d & live & (1u << 22));
    int mem_bad = mem_diff(g, h, NULL);
    int flags_bad = (d & live & 0x3F0000) != 0;
    if (flags_bad) regs_bad = 1;          /* a flag a caller reads is a real difference */
    if (regs_bad || mem_bad || io_bad) {
        st->bad++;
        if (io_bad && printed < 20) fprintf(stderr, "I/O MISMATCH hand_%04x: %s\n", off, io_why);
        report_mismatch(off, in, g, h, 0);
    } else if (flags_bad) { st->flag_bad++; }
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
