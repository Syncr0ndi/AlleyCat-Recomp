#!/usr/bin/env python3
"""Interprocedural register/flag liveness for the Alley Cat code.

For every routine it computes
  inputs  = registers/flags read before being written (live at entry),
  outputs = registers/flags some caller reads after the routine returns (live-out).
Check mode compares only the outputs, so hand-written C need not reproduce scratch
registers or flags that nothing reads.

The CFG is global and context-insensitive: RET has an edge to every instruction that follows
a CALL of any routine that can reach that RET; CALL has an edge to the callee only. That is
conservative (a register live after one call site counts for all). Interrupt handlers keep
everything live. Byte registers are tracked separately (AL and AH are different bits).

Usage: python tools/liveness.py CAT.EXE            prints the table
As a module: analyse(map) -> {routine_offset: (in_mask, out_mask)}
"""
import collections, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from v30dec import SEQ, JMP, JCC_, CALL, RET, JMPF, RETF, IRET, IJMP

NAMES = ["AL", "AH", "CL", "CH", "DL", "DH", "BL", "BH", "SP", "BP", "SI", "DI",
         "ES", "CS", "SS", "DS", "CF", "ZF", "SF", "OF", "PF", "AF", "DF"]
B = {n: 1 << i for i, n in enumerate(NAMES)}
R16 = [B["AL"] | B["AH"], B["CL"] | B["CH"], B["DL"] | B["DH"], B["BL"] | B["BH"], B["SP"], B["BP"], B["SI"], B["DI"]]
R8 = [B["AL"], B["CL"], B["DL"], B["BL"], B["AH"], B["CH"], B["DH"], B["BH"]]
SREG = [B["ES"], B["CS"], B["SS"], B["DS"]]
AX, CX, DX, BX, SI, DI, BP = R16[0], R16[1], R16[2], R16[3], B["SI"], B["DI"], B["BP"]
ARITH = B["CF"] | B["ZF"] | B["SF"] | B["OF"] | B["PF"] | B["AF"]
SZPAO = ARITH & ~B["CF"]
ALL = (1 << len(NAMES)) - 1
SEGOF = {"ES": B["ES"], "CS": B["CS"], "SS": B["SS"], "DS": B["DS"]}
JCC_USE = {"JO": "OF", "JNO": "OF", "JC": "CF", "JNC": "CF", "JZ": "ZF", "JNZ": "ZF", "JBE": "CF ZF", "JA": "CF ZF",
           "JS": "SF", "JNS": "SF", "JPE": "PF", "JPO": "PF", "JL": "SF OF", "JGE": "SF OF", "JLE": "ZF SF OF", "JG": "ZF SF OF"}


def mask(names): return sum(B[n] for n in names.split())


def op_regs(op):
    """(register bits the operand names, address bits it uses)"""
    if op.kind == "reg8": return R8[op.val], 0
    if op.kind == "reg16": return R16[op.val], 0
    if op.kind == "sreg": return SREG[op.val], 0
    if op.kind == "mem":
        a = 0
        if op.base:
            for r in op.base.split("+"): a |= {"BX": BX, "SI": SI, "DI": DI, "BP": BP}[r]
        seg = op.seg or ("SS" if op.base and "BP" in op.base else "DS")
        return 0, a | SEGOF[seg]
    return 0, 0


def use_def(ins):
    """(use, def) bit masks of one instruction, calls/returns excluded."""
    mn, ops = ins.mnem, ins.ops
    u = d = 0
    regs = [op_regs(o) for o in ops]
    for _, a in regs: u |= a
    r0 = regs[0][0] if regs else 0
    r1 = regs[1][0] if len(regs) > 1 else 0
    w = 1 if any(o.kind == "reg8" or (o.kind == "mem" and o.size == 1) for o in ops) else 2
    rep = bool(ins.prefix) and ins.prefix[0].startswith("REP")
    if mn == "MOV": u |= r1; d |= r0
    elif mn in ("ADD", "SUB", "AND", "OR", "XOR"):
        u |= r0 | r1; d |= r0 | ARITH
        if mn in ("SUB", "XOR") and len(ops) == 2 and ops[0].kind in ("reg8", "reg16") and ops[0].kind == ops[1].kind and ops[0].val == ops[1].val:
            u &= ~r0                                     # SUB r,r / XOR r,r: no real read
    elif mn in ("ADC", "SBB"): u |= r0 | r1 | B["CF"]; d |= r0 | ARITH
    elif mn in ("CMP", "TEST"): u |= r0 | r1; d |= ARITH
    elif mn in ("INC", "DEC"): u |= r0; d |= r0 | SZPAO
    elif mn == "NOT": u |= r0; d |= r0
    elif mn == "NEG": u |= r0; d |= r0 | ARITH
    elif mn in ("SHL", "SHR", "SAR", "ROL", "ROR", "RCL", "RCR"):
        u |= r0 | r1; d |= r0 | ARITH
        if mn in ("RCL", "RCR"): u |= B["CF"]
    elif mn == "MUL":
        u |= r0 | (B["AL"] if w == 1 else AX); d |= AX | ARITH | (0 if w == 1 else DX)
    elif mn == "XCHG": u |= r0 | r1; d |= r0 | r1
    elif mn == "PUSH": u |= r0 | B["SP"] | B["SS"]; d |= B["SP"]
    elif mn == "POP": u |= B["SP"] | B["SS"]; d |= r0 | B["SP"]
    elif mn == "PUSHF": u |= ARITH | B["DF"] | B["SP"] | B["SS"]; d |= B["SP"]
    elif mn == "POPF": u |= B["SP"] | B["SS"]; d |= ARITH | B["DF"] | B["SP"]
    elif mn == "LAHF": u |= ARITH; d |= B["AH"]
    elif mn == "SAHF": u |= B["AH"]; d |= ARITH
    elif mn in ("CLC", "STC"): d |= B["CF"]
    elif mn in ("CLD", "STD"): d |= B["DF"]
    elif mn in ("CLI", "STI", "NOP"): pass
    elif mn == "AAA": u |= AX | B["AF"]; d |= AX | ARITH
    elif mn == "IN": u |= (DX if ops[1].kind == "reg16" else 0); d |= (B["AL"] if ops[0].kind == "reg8" else AX)
    elif mn == "OUT": u |= (DX if ops[0].kind == "reg16" else 0) | (B["AL"] if ops[1].kind == "reg8" else AX)
    elif mn in ("LODSB", "LODSW"):
        u |= SI | B["DF"] | SEGOF[(ins.ops[0].text() if False else "DS")]
        d |= SI | (B["AL"] if mn == "LODSB" else AX)
    elif mn in ("STOSB", "STOSW"):
        u |= DI | B["ES"] | B["DF"] | (B["AL"] if mn == "STOSB" else AX); d |= DI
    elif mn in ("MOVSB", "MOVSW"): u |= SI | DI | B["DS"] | B["ES"] | B["DF"]; d |= SI | DI
    elif mn in ("SCASB", "SCASW"):
        u |= DI | B["ES"] | B["DF"] | (B["AL"] if mn == "SCASB" else AX); d |= DI | ARITH
    elif mn == "INT":
        v = ops[0].val
        if v == 0x1A: u |= B["AH"]; d |= CX | DX | B["AL"]
        else: u |= AX | BX | CX | DX | B["DS"] | B["ES"]; d |= AX     # INT 10h/11h: AX may change
    elif mn in ("LOOP", "LOOPZ", "LOOPNZ"):
        u |= CX | (B["ZF"] if mn != "LOOP" else 0); d |= CX
    elif ins.flow == JCC_: u |= mask(JCC_USE[mn])
    elif ins.flow == IJMP: u |= r0
    if rep and mn in ("MOVSB", "MOVSW", "STOSB", "STOSW", "SCASB", "SCASW", "LODSB", "LODSW"):
        u |= CX; d |= CX
    return u, d


def analyse(m):
    code, canon = m["code"], m["canon"]
    cs = next(iter(code.values())).cs
    base = cs * 16
    funcs = {l - base for l in m["funcs"]}
    succ = collections.defaultdict(set)
    calls_to = collections.defaultdict(list)          # routine linear -> return points
    for l, ins in code.items():
        nxt = canon(cs, ins.ip + ins.length)
        f = ins.flow
        if f in (SEQ, JCC_) and nxt in code: succ[l].add(nxt)
        if f in (JMP, JCC_) and ins.target and canon(*ins.target) in code: succ[l].add(canon(*ins.target))
        if f == CALL:
            t = canon(*ins.target)
            if nxt in code: calls_to[t].append(nxt)
        if f == IJMP:                                  # the state table at CS:0250
            for i in range(8):
                p = base + 0x250 + 2 * i
                succ[l].add(base + (m["mem"][p] | m["mem"][p + 1] << 8))
    # which routines reach each RET without going through a call
    reaches = collections.defaultdict(set)
    for r in m["funcs"]:
        seen, st = set(), [r]
        while st:
            l = st.pop()
            if l in seen or l not in code: continue
            seen.add(l)
            ins = code[l]
            if ins.flow in (RET, RETF, IRET): reaches[l].add(r); continue
            if ins.flow == CALL:
                n = canon(cs, ins.ip + ins.length)
                if n in code: st.append(n)
                continue
            st.extend(succ[l])
    isr = {base + o for o in (0x14B3, 0x14FB, 0x1554)}
    use, dfn = {}, {}
    for l, ins in code.items(): use[l], dfn[l] = use_def(ins)
    ret_succ = {l: set() for l in code if code[l].flow in (RET, RETF, IRET)}
    ret_all = {l for l in ret_succ if reaches[l] & isr or code[l].flow in (IRET, RETF)}
    for l in ret_succ:
        for r in reaches[l]: ret_succ[l].update(calls_to[r])
    order = sorted(code, reverse=True)
    gen = {r: 0 for r in m["funcs"]}           # routine inputs (live at entry if nothing is live after)

    def solve(context):
        live_in = {l: 0 for l in code}
        changed = True
        while changed:
            changed = False
            for l in order:
                ins = code[l]
                if l in ret_succ:
                    new = 0
                    if context:
                        new = ALL if l in ret_all else 0
                        for s in ret_succ[l]: new |= live_in[s]
                elif ins.flow == CALL:
                    n = canon(cs, ins.ip + ins.length)
                    new = gen.get(canon(*ins.target), ALL) | (live_in.get(n, 0)) | B["SP"] | B["SS"]
                else:
                    out = 0
                    for s in succ[l]: out |= live_in[s]
                    new = use[l] | (out & ~dfn[l])
                if new != live_in[l]:
                    live_in[l] = new; changed = True
        return live_in

    while True:                                # routine summaries to a fixpoint
        li = solve(False)
        ng = {r: li.get(r, 0) for r in m["funcs"]}
        if ng == gen: break
        gen = ng
    live_in = solve(True)
    res = {}
    for r in m["funcs"]:
        out = 0
        for l, rs in reaches.items():
            if r in rs:
                out |= ALL if l in ret_all else 0
                for s in ret_succ.get(l, ()): out |= live_in[s]
        res[r - base] = (gen.get(r, 0) & ~(B["SP"] | B["SS"] | B["CS"]),
                         out & ~(B["SP"] | B["SS"] | B["CS"]))
    return res


def names(msk):
    out = []
    for i16, nm in ((R16[0], "AX"), (R16[1], "CX"), (R16[2], "DX"), (R16[3], "BX")):
        if msk & i16 == i16: out.append(nm); msk &= ~i16
    return " ".join(out + [n for i, n in enumerate(NAMES) if msk >> i & 1])


if __name__ == "__main__":
    import catmap
    m = catmap.main(sys.argv[1], quiet=True)
    for r, (i, o) in sorted(analyse(m).items()):
        print(f"{r:04X}  in: {names(i):40s} out: {names(o)}")
