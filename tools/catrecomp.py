#!/usr/bin/env python3
"""Static recompiler: Alley Cat 8086 program -> C (adapted from LOHT-Recomp's v30recomp.py).

Reads the user's CAT.EXE, maps it with tools/catmap.py and writes C into OUTDIR (a build
directory: the output is derived from the game and is never committed):
  cat_gen.h          prototypes
  cat_gen_NN.c       generated functions
  cat_gen_entries.c  linear address -> function table for the dispatcher

Usage: python tools/catrecomp.py CAT.EXE OUTDIR [--hand SRC_DIR] [--per-file N] [--no-comments]

--hand SRC_DIR: routines rewritten by hand (`void hand_XXXX(V30 *c)` in SRC_DIR/*.c) are
called through cat_hand_call(), which runs the C version, the recompiled one, or both and
compares them (src/game/hand.c).
"""
import collections, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import catmap                                            # noqa: E402
import liveness                                          # noqa: E402
from v30dec import (SEQ, JMP, JCC_, CALL, RET, JMPF, CALLF, RETF, IRET,  # noqa: E402
                    IJMP, ICALL, IJMPF, ICALLF, HALT, R8, R16, SREG)

SREG_IDX = {"ES": 0, "CS": 1, "SS": 2, "DS": 3}
REG_EXPR = {"AX": "c->r[0]", "CX": "c->r[1]", "DX": "c->r[2]", "BX": "c->r[3]",
            "SP": "c->r[4]", "BP": "c->r[5]", "SI": "c->r[6]", "DI": "c->r[7]"}
SHIFT_OP = {"ROL": "SH_ROL", "ROR": "SH_ROR", "RCL": "SH_RCL", "RCR": "SH_RCR",
            "SHL": "SH_SHL", "SHR": "SH_SHR", "SAR": "SH_SAR"}
JCC_COND = {
    "JO": "c->of", "JNO": "!c->of", "JC": "c->cf", "JNC": "!c->cf", "JZ": "c->zf", "JNZ": "!c->zf",
    "JBE": "(c->cf | c->zf)", "JA": "!(c->cf | c->zf)", "JS": "c->sf", "JNS": "!c->sf",
    "JPE": "c->pf", "JPO": "!c->pf", "JL": "(c->sf != c->of)", "JGE": "(c->sf == c->of)",
    "JLE": "(c->zf || c->sf != c->of)", "JG": "(!c->zf && c->sf == c->of)",
    "LOOP": "(--c->r[1] != 0)", "LOOPZ": "(--c->r[1] != 0 && c->zf)",
    "LOOPNZ": "(--c->r[1] != 0 && !c->zf)", "JCXZ": "(c->r[1] == 0)",
}
CALL_LIKE = (CALL, CALLF, ICALL, ICALLF)


def r8slot(i): return (i & 3) * 2 + (i >> 2)


class Gen:
    def __init__(self, m, comments=True):
        self.m = m
        self.comments = comments
        self.mem, self.code, self.root = m["mem"], m["code"], m["code_root"]
        self.canon, self.label = m["canon"], m["label"]
        # group instructions into C functions by (root, cs)
        self.group_of = {}
        groups = collections.defaultdict(list)
        for l, ins in self.code.items():
            key = (self.root[l], ins.cs)
            groups[key].append(l)
            self.group_of[l] = key
        self.groups = {k: sorted(v) for k, v in groups.items()}
        per_root = collections.Counter(k[0] for k in self.groups)
        self.fname = {k: f"f_{k[0]:05x}" + (f"_{k[1]:04x}" if per_root[k[0]] > 1 else "") for k in self.groups}
        self.hand = set()
        # entry labels: function entries, flow targets, return points, resume points
        self.labels = set(m["funcs"]) & set(self.code)
        for l, ins in self.code.items():
            if ins.target is not None:
                t = self.canon(*ins.target)
                if t in self.code: self.labels.add(t)
            if ins.flow in CALL_LIKE or ins.mnem in ("INT", "INTO", "BOUND", "DIV", "IDIV", "AAM", "STI", "POPF", "HLT"):
                n = self.canon(ins.cs, (ins.ip + ins.length) & 0xFFFF)
                if n in self.code: self.labels.add(n)

    # ------------------------------------------------------------ operands
    def seg_index(self, op):
        if op.seg: return SREG_IDX[op.seg]
        return 2 if op.base and "BP" in op.base else 3

    def ea_expr(self, op):
        parts = [REG_EXPR[r] for r in op.base.split("+")] if op.base else []
        if op.disp or not parts: parts.append(f"0x{op.disp:x}")
        return f"(uint16_t)({' + '.join(parts)})"

    @staticmethod
    def size_of(ops):
        for o in ops:
            if o.kind == "reg8": return 1
            if o.kind in ("reg16", "sreg"): return 2
            if o.kind == "mem" and o.size in (1, 2): return o.size
        return 2

    def rd(self, op, sz):
        k = op.kind
        if k == "reg16": return f"c->r[{op.val}]"
        if k == "reg8": return f"c->b[{r8slot(op.val)}]"
        if k == "sreg": return f"c->sreg[{op.val}]"
        if k == "imm": return f"0x{op.val & (0xFF if sz == 1 else 0xFFFF):x}"
        if k == "mem": return f"rd{8 if sz == 1 else 16}(c, c->sreg[{self.seg_index(op)}], ea)"
        raise ValueError(k)

    def wr(self, op, val, sz):
        k = op.kind
        if k == "reg16": return f"c->r[{op.val}] = (uint16_t)({val});"
        if k == "reg8": return f"c->b[{r8slot(op.val)}] = (uint8_t)({val});"
        if k == "sreg": return f"c->sreg[{op.val}] = (uint16_t)({val});"
        if k == "mem":
            return f"wr{8 if sz == 1 else 16}(c, c->sreg[{self.seg_index(op)}], ea, (uint{8 if sz == 1 else 16}_t)({val}));"
        raise ValueError(k)

    # ------------------------------------------------------------ control transfer helpers
    def fn_for(self, cs, ip):
        t = self.canon(cs, ip)
        if t in self.code and self.code[t].cs == cs:
            return self.fname[self.group_of[t]], t
        return None, t

    def transfer(self, cur_group, cs, ip, backward=False):
        """Code that continues execution at cs:ip (never falls through)."""
        fn, t = self.fn_for(cs, ip)
        if fn and self.group_of[t] == cur_group:
            pre = ""
            if backward:
                pre = (f"if (c->cycles <= 0) {{ c->ip = 0x{ip:04x}; v30_sync_point(c); "
                       f"if (c->ip != 0x{ip:04x} || c->sreg[1] != 0x{cs:04x}) return; }} ")
            return pre + f"goto L_{t:05x};"
        setcs = f"c->sreg[1] = 0x{cs:04x}; " if cur_group[1] != cs else ""
        if fn:
            return f"{setcs}c->ip = 0x{ip:04x}; V30_TAIL({fn});"
        return f"{setcs}c->ip = 0x{ip:04x}; V30_TAIL(v30_dispatch);"

    @staticmethod
    def check(cs, nxt):
        return f"if (c->ip != 0x{nxt:04x} || c->sreg[1] != 0x{cs:04x}) return;"

    # ------------------------------------------------------------ instruction
    def insn(self, ins, grp):
        cs, ip, mn, ops = ins.cs, ins.ip, ins.mnem, ins.ops
        nxt = (ip + ins.length) & 0xFFFF
        sz = self.size_of(ops)
        out = []
        mem_op = next((o for o in ops if o.kind == "mem"), None)
        if mem_op is not None and mn not in ("POP",):
            out.append(f"const uint16_t ea = {self.ea_expr(mem_op)};")
        cyc = 2 + (7 if mem_op is not None else 0)
        rep = REP_KIND.get(next((p for p in ins.prefix if p in REP_KIND), None), "REP_NONE")
        segpfx = next((p[:-1] for p in ins.prefix if p.endswith(":")), None)
        A = lambda: self.rd(ops[0], sz)
        B = lambda: self.rd(ops[1], sz)
        W = lambda v: self.wr(ops[0], v, sz)
        w = 8 if sz == 1 else 16
        f = ins.flow

        if mn in ("ADD", "ADC", "SUB", "SBB", "CMP"):
            fn = "add" if mn in ("ADD", "ADC") else "sub"
            carry = "c->cf" if mn in ("ADC", "SBB") else "0"
            e = f"{fn}{w}(c, {A()}, {B()}, {carry})"
            out.append(f"{e};" if mn == "CMP" else W(e))
        elif mn in ("AND", "OR", "XOR", "TEST"):
            fn = {"AND": "and", "OR": "or", "XOR": "xor", "TEST": "and"}[mn]
            e = f"{fn}{w}(c, {A()}, {B()})"
            out.append(f"{e};" if mn == "TEST" else W(e))
        elif mn in ("INC", "DEC", "NEG"):
            out.append(W(f"{mn.lower()}{w}(c, {A()})"))
        elif mn == "NOT":
            out.append(W(f"~{A()}"))
        elif mn in ("MUL", "IMUL") and len(ops) == 1:
            out.append(f"v30_{mn.lower()}{w}(c, {A()});"); cyc += 20
        elif mn == "IMUL":
            out.append(self.wr(ops[0], f"v30_imul3(c, {self.rd(ops[1], 2)}, {self.rd(ops[2], 2)})", 2)); cyc += 22
        elif mn in ("DIV", "IDIV"):
            out.append(f"if (!v30_{mn.lower()}{w}(c, {A()})) {{ c->ip = 0x{nxt:04x}; v30_int(c, 0); {self.check(cs, nxt)} }}")
            cyc += 35
        elif mn in SHIFT_OP:
            cnt = ops[1]
            n = "(c->b[2] & 0x1f)" if cnt.kind == "reg8" else f"{cnt.val & 0x1f}"
            out.append(W(f"v30_shift{w}(c, {SHIFT_OP[mn]}, {A()}, {n})"))
        elif mn == "MOV":
            out.append(self.wr(ops[0], self.rd(ops[1], sz), sz))
        elif mn == "XCHG":
            out.append(f"uint{w}_t t = {A()}; {W(B())} {self.wr(ops[1], 't', sz)}")
        elif mn == "LEA":
            out.append(self.wr(ops[0], "ea", 2))
        elif mn in ("LES", "LDS"):
            s = self.seg_index(ops[1])
            out.append(f"uint16_t o = rd16(c, c->sreg[{s}], ea), s = rd16(c, c->sreg[{s}], (uint16_t)(ea + 2)); "
                       f"c->r[{ops[0].val}] = o; c->sreg[{0 if mn == 'LES' else 3}] = s;")
        elif mn == "PUSH":
            out.append(f"uint16_t t = {self.rd(ops[0], 2)}; push16(c, t);"); cyc += 6
        elif mn == "POP":
            if ops[0].kind == "mem":
                out.append(f"uint16_t t = pop16(c); const uint16_t ea = {self.ea_expr(ops[0])}; {self.wr(ops[0], 't', 2)}")
            else:
                out.append(self.wr(ops[0], "pop16(c)", 2))
            cyc += 6
        elif mn == "PUSHA":
            out.append("uint16_t t = c->r[4]; push16(c, c->r[0]); push16(c, c->r[1]); push16(c, c->r[2]); "
                       "push16(c, c->r[3]); push16(c, t); push16(c, c->r[5]); push16(c, c->r[6]); push16(c, c->r[7]);")
            cyc += 30
        elif mn == "POPA":
            out.append("c->r[7] = pop16(c); c->r[6] = pop16(c); c->r[5] = pop16(c); c->r[4] += 2; "
                       "c->r[3] = pop16(c); c->r[2] = pop16(c); c->r[1] = pop16(c); c->r[0] = pop16(c);")
            cyc += 30
        elif mn == "PUSHF":
            out.append("push16(c, v30_get_flags(c));")
        elif mn == "POPF":
            out.append(f"v30_set_flags(c, pop16(c)); if (c->irq_pending && c->ifl) {{ c->ip = 0x{nxt:04x}; "
                       f"v30_sync_point(c); {self.check(cs, nxt)} }}")
        elif mn == "SAHF":
            out.append("{ uint8_t f = c->b[1]; c->cf = f & 1; c->pf = (f >> 2) & 1; c->af = (f >> 4) & 1; "
                       "c->zf = (f >> 6) & 1; c->sf = (f >> 7) & 1; }")
        elif mn == "LAHF":
            out.append("c->b[1] = (uint8_t)v30_get_flags(c);")
        elif mn == "CBW":
            out.append("c->r[0] = (uint16_t)(int16_t)(int8_t)c->b[0];")
        elif mn == "CWD":
            out.append("c->r[2] = (c->r[0] & 0x8000) ? 0xFFFF : 0;")
        elif mn in ("CLC", "STC", "CMC", "CLD", "STD", "CLI"):
            out.append({"CLC": "c->cf = 0;", "STC": "c->cf = 1;", "CMC": "c->cf ^= 1;",
                        "CLD": "c->df = 0;", "STD": "c->df = 1;", "CLI": "c->ifl = 0;"}[mn])
        elif mn == "STI":
            out.append(f"c->ifl = 1; if (c->irq_pending) {{ c->ip = 0x{nxt:04x}; v30_sync_point(c); {self.check(cs, nxt)} }}")
        elif mn in STRING_OPS:
            base, wd = mn[:-1], 1 if mn.endswith("W") else 0
            seg = SREG_IDX[SREG[ops[0].val]] if ops else 3
            out.append({
                "MOVS": f"v30_movs(c, {wd}, {seg}, {rep});", "CMPS": f"v30_cmps(c, {wd}, {seg}, {rep});",
                "STOS": f"v30_stos(c, {wd}, {rep});", "LODS": f"v30_lods(c, {wd}, {seg}, {rep});",
                "SCAS": f"v30_scas(c, {wd}, {rep});", "INS": f"v30_ins_io(c, {wd}, {rep});",
                "OUTS": f"v30_outs_io(c, {wd}, {seg}, {rep});"}[base])
        elif mn == "IN":
            port = f"0x{ops[1].val:x}" if ops[1].kind == "imm" else "c->r[2]"
            wd = 1 if ops[0].kind == "reg16" else 0
            out.append(f"c->r[0] = v30_in(c, {port}, 1);" if wd else f"c->b[0] = (uint8_t)v30_in(c, {port}, 0);")
            cyc += 8
        elif mn == "OUT":
            port = f"0x{ops[0].val:x}" if ops[0].kind == "imm" else "c->r[2]"
            wd = 1 if ops[1].kind == "reg16" else 0
            out.append(f"v30_out(c, {port}, {'c->r[0]' if wd else 'c->b[0]'}, {wd});")
            cyc += 8
        elif mn == "XLAT":
            s = SREG_IDX[segpfx] if segpfx else 3
            out.append(f"c->b[0] = rd8(c, c->sreg[{s}], (uint16_t)(c->r[3] + c->b[0]));")
        elif mn in ("NOP", "WAIT", "ESC"):
            pass
        elif mn == "HLT":
            out.append(f"c->ip = 0x{nxt:04x}; v30_halt(c); {self.check(cs, nxt)}")
        elif mn == "INT":
            out.append(f"c->ip = 0x{nxt:04x}; v30_int(c, 0x{ops[0].val:x}); {self.check(cs, nxt)}")
        elif mn == "INTO":
            out.append(f"if (c->of) {{ c->ip = 0x{nxt:04x}; v30_int(c, 4); {self.check(cs, nxt)} }}")
        elif mn == "BOUND":
            s = self.seg_index(ops[1])
            out.append(f"int16_t lo = (int16_t)rd16(c, c->sreg[{s}], ea), hi = (int16_t)rd16(c, c->sreg[{s}], (uint16_t)(ea + 2)); "
                       f"if ((int16_t)c->r[{ops[0].val}] < lo || (int16_t)c->r[{ops[0].val}] > hi) "
                       f"{{ c->ip = 0x{nxt:04x}; v30_int(c, 5); {self.check(cs, nxt)} }}")
        elif mn == "ENTER":
            out.append(f"v30_enter(c, 0x{ops[0].val:x}, 0x{ops[1].val:x});"); cyc += 12
        elif mn == "LEAVE":
            out.append("c->r[4] = c->r[5]; c->r[5] = pop16(c);")
        elif mn in ("DAA", "DAS", "AAA", "AAS"):
            out.append(f"v30_{mn.lower()}(c);")
        elif mn == "AAM":
            out.append(f"if (!v30_aam(c, 0x{ops[0].val:x})) {{ c->ip = 0x{nxt:04x}; v30_int(c, 0); {self.check(cs, nxt)} }}")
        elif mn == "AAD":
            out.append(f"v30_aad(c, 0x{ops[0].val:x});")
        elif mn in ("TEST1", "SET1", "CLR1", "NOT1"):
            e, bit = ops
            bsz = 1 if (e.kind == "reg8" or (e.kind == "mem" and e.size == 1)) else 2
            bw = 8 if bsz == 1 else 16
            b = "c->b[2]" if bit.kind == "reg8" else f"{bit.val}"
            if mn == "TEST1":
                out.append(f"test1_{bw}(c, {self.rd(e, bsz)}, {b});")
            else:
                out.append(self.wr(e, f"{mn}_{bw}({self.rd(e, bsz)}, {b})", bsz))
        elif mn in ("INS", "EXT"):
            a, l2 = ops
            ln = f"c->b[{r8slot(l2.val)}]" if l2.kind == "reg8" else f"{l2.val}"
            out.append(f"v30_{mn.lower()}(c, {r8slot(a.val)}, {ln});"); cyc += 30
        elif mn in ("ADD4S", "SUB4S", "CMP4S"):
            out.append(f"v30_{mn.lower()}(c, {SREG_IDX[segpfx] if segpfx else 3});")
        elif mn in ("ROL4", "ROR4"):
            out.append(self.wr(ops[0], f"v30_{mn.lower()}(c, {self.rd(ops[0], 1)})", 1)); cyc += 20
        # ------------------------------------------------ flow
        elif f == JMP:
            tcs, tip = ins.target
            out.append(self.transfer(grp, tcs, tip, backward=self.canon(tcs, tip) <= self.canon(cs, ip))); cyc += 12
        elif f == JCC_:
            tcs, tip = ins.target
            out.append(f"if ({JCC_COND[mn]}) {{ V30_TICK({taken_extra(ins)}); {self.transfer(grp, tcs, tip, backward=self.canon(tcs, tip) <= self.canon(cs, ip))} }}")
            cyc += 2
        elif f == JMPF:
            tcs, tip = ins.target
            out.append(self.transfer(grp, tcs, tip)); cyc += 11
        elif f == CALL:
            tcs, tip = ins.target
            fn, _ = self.fn_for(tcs, tip)
            out.append(f"push16(c, 0x{nxt:04x}); c->ip = 0x{tip:04x}; {fn or 'v30_dispatch'}(c); {self.check(cs, nxt)}")
            cyc += 14
        elif f == CALLF:
            tcs, tip = ins.target
            fn, _ = self.fn_for(tcs, tip)
            out.append(f"push16(c, 0x{cs:04x}); push16(c, 0x{nxt:04x}); c->sreg[1] = 0x{tcs:04x}; c->ip = 0x{tip:04x}; "
                       f"{fn or 'v30_dispatch'}(c); {self.check(cs, nxt)}")
            cyc += 20
        elif f == ICALL:
            out.append(f"uint16_t t = {self.rd(ops[0], 2)}; push16(c, 0x{nxt:04x}); c->ip = t; v30_dispatch(c); {self.check(cs, nxt)}")
            cyc += 14
        elif f == ICALLF:
            s = self.seg_index(ops[0])
            out.append(f"uint16_t o = rd16(c, c->sreg[{s}], ea), s = rd16(c, c->sreg[{s}], (uint16_t)(ea + 2)); "
                       f"push16(c, 0x{cs:04x}); push16(c, 0x{nxt:04x}); c->sreg[1] = s; c->ip = o; v30_dispatch(c); {self.check(cs, nxt)}")
            cyc += 24
        elif f == IJMP:
            out.append(f"c->ip = {self.rd(ops[0], 2)}; V30_TAIL(v30_dispatch);"); cyc += 11
        elif f == IJMPF:
            s = self.seg_index(ops[0])
            out.append(f"uint16_t o = rd16(c, c->sreg[{s}], ea), s = rd16(c, c->sreg[{s}], (uint16_t)(ea + 2)); "
                       f"c->sreg[1] = s; c->ip = o; V30_TAIL(v30_dispatch);")
        elif f == RET:
            n = ops[0].val if ops else 0
            out.append(f"cat_seq++; c->ip = pop16(c);{f' c->r[4] += 0x{n:x};' if n else ''} return;"); cyc += 13
        elif f == RETF:
            n = ops[0].val if ops else 0
            out.append(f"cat_seq++; c->ip = pop16(c); c->sreg[1] = pop16(c);{f' c->r[4] += 0x{n:x};' if n else ''} return;"); cyc += 20
        elif f == IRET:
            out.append("cat_seq++; c->ip = pop16(c); c->sreg[1] = pop16(c); v30_set_flags(c, pop16(c)); return;"); cyc += 25
        else:
            out.append(f'c->ip = 0x{ip:04x}; v30_unimplemented(c, "{mn}"); return;')
        cyc = cost8088(ins)
        body = " ".join(out)
        return f"V30_TICK({cyc}); {{ {body} }}" if body else f"V30_TICK({cyc});"

    # ------------------------------------------------------------ function
    def function(self, key):
        ls = self.groups[key]
        fn = self.fname[key]
        cs = key[1]
        lab = [l for l in ls if l in self.labels]
        if ls[0] not in lab: lab.insert(0, ls[0])
        root_off = key[0] - cs * 16
        hand = root_off in self.hand
        body_fn = f"{fn}_gen" if hand else fn
        lines = ([f"/* {self.label(key[0])} */"] if self.comments else []) + [f"void {body_fn}(V30 *c) {{", "    switch (c->ip) {"]
        for l in lab:
            if l == key[0]:   # count real entries only (not loops back to the first instruction)
                lines.append(f"    case 0x{self.code[l].ip:04x}: cat_cov[0x{root_off:04x}]++; cat_seq++; goto L_{l:05x};")
            else:
                lines.append(f"    case 0x{self.code[l].ip:04x}: goto L_{l:05x};")
        lines.append(f'    default: v30_bad_entry(c, "{fn}"); return;')
        lines.append("    }")
        ends = (JMP, JMPF, RET, RETF, IRET, IJMP, IJMPF)
        for i, l in enumerate(ls):
            ins = self.code[l]
            if l in lab:
                lines.append(f"L_{l:05x}:")
            if self.comments:
                lines.append(f"    /* {ins.cs:04X}:{ins.ip:04X} {ins.text()} */")
            lines.append("    " + self.insn(ins, key))
            if ins.flow in ends:
                continue
            nxt_ip = (ins.ip + ins.length) & 0xFFFF
            nl = self.canon(ins.cs, nxt_ip)
            if i + 1 < len(ls) and ls[i + 1] == nl:
                continue
            lines.append("    " + self.transfer(key, ins.cs, nxt_ip))
        lines.append("}")
        if hand:   # entry at the start runs the hand-written C (or both, in check mode)
            lines.append(f"void {fn}(V30 *c) {{ cat_hand_call(c, 0x{root_off:04x}, hand_{root_off:04x}, {body_fn}); }}")
            return "\n".join(lines), [(l, self.code[l].cs, self.code[l].ip, fn if l == key[0] else body_fn) for l in lab]
        return "\n".join(lines), [(l, self.code[l].cs, self.code[l].ip, fn) for l in lab]


# ------------------------------------------------------------ 8088 timing
# Approximate 4.77 MHz 8088 cycle costs (Intel 8086 tables, +4 cycles per word memory transfer
# on the 8-bit bus, and never faster than fetching the instruction bytes at 4 cycles each).
# String instructions charge their per-element cost in src/cpu/v30.c.
def ea_cost(op):
    if op.base is None: return 6
    b = op.base
    c = 5 if "+" not in b else (7 if b in ("BP+DI", "BX+SI") else 8)
    return c + (4 if op.disp else 0)


def cost8088(ins):
    mn, ops, f = ins.mnem, ins.ops, ins.flow
    mem = next((o for o in ops if o.kind == "mem"), None)
    w = 2
    for o in ops:
        if o.kind == "reg8" or (o.kind == "mem" and o.size == 1): w = 1; break
    ea = ea_cost(mem) if mem else 0
    wp = 4 if (mem is not None and w == 2) else 0       # one word transfer on the 8-bit bus
    dst_mem = bool(ops) and ops[0].kind == "mem"
    src_imm = len(ops) > 1 and ops[1].kind == "imm"
    if mn == "MOV":
        if ops[0].kind == "sreg" or (len(ops) > 1 and ops[1].kind == "sreg"): x = 2 if not mem else 9 + ea + wp
        elif not mem: x = 4 if src_imm else 2
        elif dst_mem: x = (10 if src_imm else 9) + ea + wp
        else: x = 8 + ea + wp
    elif mn in ("ADD", "ADC", "SUB", "SBB", "AND", "OR", "XOR"):
        if not mem: x = 4 if src_imm else 3
        elif dst_mem: x = (17 if src_imm else 16) + ea + 2 * wp
        else: x = 9 + ea + wp
    elif mn in ("CMP", "TEST"):
        if not mem: x = 4 if src_imm else 3
        else: x = (10 if src_imm else 9) + ea + wp
    elif mn in ("INC", "DEC"):
        x = 15 + ea + 2 * wp if mem else (2 if w == 2 else 3)
    elif mn in ("NEG", "NOT"):
        x = 16 + ea + 2 * wp if mem else 3
    elif mn in ("SHL", "SHR", "SAR", "ROL", "ROR", "RCL", "RCR"):
        by_cl = ops[1].kind == "reg8"
        n = 0 if by_cl else 1
        x = (20 + ea + 2 * wp if mem else 8) if by_cl else (15 + ea + 2 * wp if mem else 2)
        if by_cl: x += 4 * 4        # typical small counts; the exact count is not known statically
    elif mn in ("MUL", "IMUL"): x = (73 if w == 1 else 128) + ea + wp
    elif mn in ("DIV", "IDIV"): x = (85 if w == 1 else 153) + ea + wp
    elif mn == "XCHG": x = 17 + ea + 2 * wp if mem else 4
    elif mn == "LEA": x = 2 + ea
    elif mn in ("LES", "LDS"): x = 16 + ea + 8
    elif mn == "PUSH": x = (16 + ea + 8) if mem else 15
    elif mn == "POP": x = (17 + ea + 8) if mem else 12
    elif mn == "PUSHF": x = 14
    elif mn == "POPF": x = 12
    elif mn in ("CBW", "CLC", "STC", "CMC", "CLD", "STD", "CLI", "STI", "NOP", "SAHF", "LAHF"): x = 2
    elif mn == "CWD": x = 5
    elif mn == "XLAT": x = 11
    elif mn == "IN": x = 14 if ops[1].kind == "imm" else 12
    elif mn == "OUT": x = 14 if ops[0].kind == "imm" else 12
    elif mn == "INT": x = 71
    elif mn in ("DAA", "DAS", "AAA", "AAS"): x = 4
    elif mn == "AAM": x = 83
    elif mn == "AAD": x = 60
    elif mn in STRING_OPS: x = 2          # per-element cost is charged by the string helper
    elif f == JMP: x = 15
    elif f == JCC_: x = 6 if mn == "JCXZ" else (5 if mn.startswith("LOOP") else 4)
    elif f == JMPF: x = 15
    elif f == CALL: x = 23
    elif f == CALLF: x = 36
    elif f == ICALL: x = (29 + ea + 4) if mem else 24
    elif f == ICALLF: x = 53 + ea
    elif f == IJMP: x = (18 + ea + 4) if mem else 11
    elif f == IJMPF: x = 24 + ea + 8
    elif f == RET: x = 24 if ops else 20
    elif f == RETF: x = 33 if ops else 34
    elif f == IRET: x = 32
    else: x = 4
    return max(x, 4 * ins.length)


def taken_extra(ins):
    """Extra cycles when a conditional branch / LOOP is taken."""
    return {"LOOP": 12, "LOOPZ": 13, "LOOPNZ": 14, "JCXZ": 12}.get(ins.mnem, 12)


REP_KIND = {"REP": "REP_Z", "REPNZ": "REP_NZ", "REPC": "REP_C", "REPNC": "REP_NC"}
STRING_OPS = {"MOVSB", "MOVSW", "CMPSB", "CMPSW", "STOSB", "STOSW", "LODSB", "LODSW",
              "SCASB", "SCASW", "INSB", "INSW", "OUTSB", "OUTSW"}

PRELUDE = """/* Generated by tools/catrecomp.py from CAT.EXE. Do not edit or commit. */
#include "cat_gen.h"
#if defined(__clang__)
#define V30_TAIL(f) __attribute__((musttail)) return f(c)
#else
/* no guaranteed tail calls: nest a bounded number, then return and let the dispatcher continue
 * at c->ip (correct because the emulated stack is exact) */
#define V30_TAIL(f) do { if (c->tail_depth < 200) { c->tail_depth++; f(c); c->tail_depth--; } return; } while (0)
#endif
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunused-label"
#pragma clang diagnostic ignored "-Wpedantic"
#endif
"""


ENTRIES_HEAD = "/* Generated. Do not edit. */\n#include \"cat_gen.h\"\n"


NL = chr(10)


def hand_functions(src_dir):
    """Offsets of routines rewritten by hand: every `void hand_XXXX(V30 *c)` in src/game/*.c."""
    import re
    found = set()
    for f in sorted(Path(src_dir).glob("*.c")):
        found |= {int(h, 16) for h in re.findall(r"^void hand_([0-9a-f]{4})\(V30 \*c\)", f.read_text(), re.M)}
    return found


def check_hand(g, hand):
    roots = {k[0] - k[1] * 16: k for k in g.groups}
    for off in sorted(hand):
        if off not in roots: raise SystemExit(f"hand_{off:04x}: no routine starts at {off:04X}")
        key = roots[off]
        for l, ins in g.code.items():
            if ins.target is None or g.group_of[l] == key: continue
            t = g.canon(*ins.target)
            if t in g.code and g.group_of[t] == key and t != key[0]:
                raise SystemExit(f"hand_{off:04x}: {ins.ip:04X} enters the routine in the middle at {t - key[1] * 16:04X}")


def main():
    argv = sys.argv[1:]
    args = [a for i, a in enumerate(argv) if not a.startswith("--") and (i == 0 or argv[i - 1] not in ("--hand", "--per-file"))]
    if len(args) != 2: raise SystemExit(__doc__)
    exe, out = args[0], Path(args[1])
    per_file = 60
    if "--per-file" in sys.argv: per_file = int(sys.argv[sys.argv.index("--per-file") + 1])
    comments = "--no-comments" not in sys.argv
    g = Gen(catmap.main(exe, quiet=True), comments)
    if "--hand" in sys.argv:
        g.hand = hand_functions(sys.argv[sys.argv.index("--hand") + 1])
        check_hand(g, g.hand)
    keys = sorted(g.groups)
    entries, chunks = {}, []
    for fi in range(0, len(keys), per_file):
        parts = [PRELUDE]
        for k in keys[fi:fi + per_file]:
            src, ents = g.function(k)
            parts.append(src)
            for l, cs, ip, fn in ents: entries.setdefault(l, (cs, ip, fn))
        chunks.append((NL + NL).join(parts) + NL)
    ent = [ENTRIES_HEAD, "const v30_entry cat_entries[] = {" + NL]
    for l in sorted(entries):
        cs, ip, fn = entries[l]
        ent.append(f"    {{0x{l:05x}, 0x{cs:04x}, 0x{ip:04x}, {fn}}}," + NL)
    ent.append("};" + NL + f"const unsigned cat_entry_count = {len(entries)};" + NL)
    live = liveness.analyse(g.m)
    ent.append("/* registers / flags callers read after each routine returns (tools/liveness.py) */" + NL)
    ent.append("const uint32_t cat_live_out[][2] = {" + NL)
    for off, (_i, o) in sorted(live.items()): ent.append(f"    {{0x{off:04x}, 0x{o:06x}}}," + NL)
    ent.append("};" + NL + f"const unsigned cat_live_out_count = {len(live)};" + NL)
    hdr = ["/* Generated. Do not edit. */", "#pragma once", '#include "v30.h"',
           "extern const v30_entry cat_entries[];", "extern const unsigned cat_entry_count;"]
    hdr += [f"void {g.fname[k]}(V30 *c);" for k in keys]
    hdr += ["extern uint32_t cat_cov[0x10000];   /* times each routine was entered (test coverage) */",
            "extern uint32_t cat_seq;            /* routine entries + returns (orders interrupts in checks) */",
            "extern const uint32_t cat_live_out[][2];", "extern const unsigned cat_live_out_count;",
            "typedef void (*cat_fn)(V30 *c);",
            "void cat_hand_call(V30 *c, unsigned off, cat_fn hand, cat_fn gen);"]
    hdr += [f"void hand_{o:04x}(V30 *c);" for o in sorted(g.hand)]
    hdr += [f"void {g.fname[k]}_gen(V30 *c);" for k in keys if k[0] - k[1] * 16 in g.hand]
    files = {"cat_gen.h": NL.join(hdr) + NL, "cat_gen_entries.c": "".join(ent)}
    for i, text in enumerate(chunks): files[f"cat_gen_{i:02d}.c"] = text
    out.mkdir(parents=True, exist_ok=True)
    for old in out.glob("cat_gen_*.c"):
        if old.name not in files: old.unlink()
    for name, text in files.items():       # rewrite only what changed (keeps rebuilds fast)
        p = out / name
        if not p.exists() or p.read_text() != text: p.write_text(text, newline=NL)
    print(f"generated {len(keys)} C functions in {len(chunks)} files, {len(entries)} entry points -> {out}")


if __name__ == "__main__":
    main()
