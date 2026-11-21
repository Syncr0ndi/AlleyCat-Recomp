"""NEC V30 (8086 + 80186 + NEC extensions) instruction decoder.

decode(mem, cs, ip) -> Insn. `mem` is a bytes-like 1 MB address space.
The decoder is shared by the disassembler and (later) the static recompiler, so every
instruction carries structured operands, not just text.
"""
from dataclasses import dataclass, field

R8 = ["AL", "CL", "DL", "BL", "AH", "CH", "DH", "BH"]
R16 = ["AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI"]
SREG = ["ES", "CS", "SS", "DS"]
EA16 = ["BX+SI", "BX+DI", "BP+SI", "BP+DI", "SI", "DI", "BP", "BX"]
ALU = ["ADD", "OR", "ADC", "SBB", "AND", "SUB", "XOR", "CMP"]
SHF = ["ROL", "ROR", "RCL", "RCR", "SHL", "SHR", "SHL", "SAR"]
JCC = ["JO", "JNO", "JC", "JNC", "JZ", "JNZ", "JBE", "JA", "JS", "JNS", "JPE", "JPO", "JL", "JGE", "JLE", "JG"]

# flow kinds
SEQ, JMP, JCC_, CALL, RET, JMPF, CALLF, RETF, IRET, IJMP, ICALL, IJMPF, ICALLF, HALT, BAD = (
    "seq", "jmp", "jcc", "call", "ret", "jmpf", "callf", "retf", "iret",
    "ijmp", "icall", "ijmpf", "icallf", "halt", "bad")


@dataclass
class Op:
    kind: str                 # reg8 reg16 sreg imm mem rel far
    val: object = None        # reg index / imm value / (seg, off) for far
    size: int = 0             # 1 or 2 (mem operands), 0 if n/a
    base: str = None          # mem: "BX+SI" etc. or None for direct
    disp: int = 0
    seg: str = None           # explicit segment override

    def text(self):
        k = self.kind
        if k == "reg8": return R8[self.val]
        if k == "reg16": return R16[self.val]
        if k == "sreg": return SREG[self.val]
        if k == "imm": return f"0x{self.val:x}"
        if k in ("rel", "near"): return f"0x{self.val:04x}"
        if k == "far": return f"{self.val[0]:04X}:{self.val[1]:04X}"
        if k == "mem":
            sz = {1: "byte ", 2: "word ", 4: "dword "}.get(self.size, "")
            seg = f"{self.seg}:" if self.seg else ""
            if self.base is None:
                inner = f"0x{self.disp:04x}"
            elif self.disp:
                d = self.disp if self.disp < 0x8000 else self.disp - 0x10000
                inner = f"{self.base}{'+' if d >= 0 else '-'}0x{abs(d):x}"
            else:
                inner = self.base
            return f"{sz}{seg}[{inner}]"
        return "?"


@dataclass
class Insn:
    cs: int
    ip: int
    length: int
    mnem: str
    ops: list = field(default_factory=list)
    flow: str = SEQ
    target: tuple = None      # (cs, ip) for direct flow
    prefix: list = field(default_factory=list)
    raw: bytes = b""

    @property
    def linear(self):
        return ((self.cs << 4) + self.ip) & 0xFFFFF

    def text(self):
        p = " ".join(self.prefix)
        s = self.mnem + (" " + ",".join(o.text() for o in self.ops) if self.ops else "")
        return (p + " " + s) if p else s


class _Rd:
    def __init__(self, mem, cs, ip):
        self.mem, self.cs, self.ip0, self.ip = mem, cs, ip, ip

    def b(self):
        v = self.mem[((self.cs << 4) + (self.ip & 0xFFFF)) & 0xFFFFF]
        self.ip += 1
        return v

    def w(self):
        lo = self.b(); return lo | (self.b() << 8)

    def sb(self):
        v = self.b(); return v - 256 if v & 0x80 else v


def _modrm(rd, size, seg):
    m = rd.b()
    mod, reg, rm = m >> 6, (m >> 3) & 7, m & 7
    if mod == 3:
        op = Op("reg8" if size == 1 else "reg16", rm)
    else:
        if mod == 0 and rm == 6:
            op = Op("mem", size=size, base=None, disp=rd.w(), seg=seg)
        else:
            disp = 0
            if mod == 1: disp = rd.sb() & 0xFFFF
            elif mod == 2: disp = rd.w()
            op = Op("mem", size=size, base=EA16[rm], disp=disp, seg=seg)
    return reg, op, mod


def decode(mem, cs, ip):
    rd = _Rd(mem, cs, ip)
    prefix, seg = [], None
    while True:
        o = rd.b()
        if o in (0x26, 0x2E, 0x36, 0x3E):
            seg = SREG[(o >> 3) & 3]; prefix_seg = True; continue
        if o in (0xF2, 0xF3, 0x64, 0x65, 0xF0):
            prefix.append({0xF2: "REPNZ", 0xF3: "REP", 0x64: "REPNC", 0x65: "REPC", 0xF0: "LOCK"}[o]); continue
        break
    I = lambda m, ops=(), flow=SEQ, target=None: Insn(cs, ip, 0, m, list(ops), flow, target, prefix)
    r8 = lambda i: Op("reg8", i)
    r16 = lambda i: Op("reg16", i)
    imm = lambda v: Op("imm", v)
    ins = None

    if o < 0x40 and (o & 7) < 6:
        sz = 1 if (o & 1) == 0 else 2
        mn = ALU[o >> 3]
        f = o & 7
        if f < 4:
            reg, e, _ = _modrm(rd, sz, seg)
            g = r8(reg) if sz == 1 else r16(reg)
            ins = I(mn, (e, g) if f < 2 else (g, e))
        elif f == 4: ins = I(mn, (r8(0), imm(rd.b())))
        else: ins = I(mn, (r16(0), imm(rd.w())))
    elif o in (0x06, 0x0E, 0x16, 0x1E): ins = I("PUSH", (Op("sreg", o >> 3),))
    elif o in (0x07, 0x17, 0x1F): ins = I("POP", (Op("sreg", o >> 3),))
    elif o == 0x0F: ins = _nec(rd, seg, I, r8, imm)
    elif o in (0x27, 0x2F, 0x37, 0x3F): ins = I({0x27: "DAA", 0x2F: "DAS", 0x37: "AAA", 0x3F: "AAS"}[o])
    elif 0x40 <= o < 0x60: ins = I(["INC", "DEC", "PUSH", "POP"][(o - 0x40) >> 3], (r16(o & 7),))
    elif o == 0x60: ins = I("PUSHA")
    elif o == 0x61: ins = I("POPA")
    elif o == 0x62:
        reg, e, _ = _modrm(rd, 4, seg); ins = I("BOUND", (r16(reg), e))
    elif o == 0x68: ins = I("PUSH", (imm(rd.w()),))
    elif o == 0x6A: ins = I("PUSH", (imm(rd.sb() & 0xFFFF),))
    elif o in (0x69, 0x6B):
        reg, e, _ = _modrm(rd, 2, seg)
        v = rd.w() if o == 0x69 else rd.sb() & 0xFFFF
        ins = I("IMUL", (r16(reg), e, imm(v)))
    elif 0x6C <= o <= 0x6F: ins = I(["INSB", "INSW", "OUTSB", "OUTSW"][o - 0x6C])
    elif 0x70 <= o <= 0x7F:
        d = rd.sb(); t = (rd.ip + d) & 0xFFFF
        ins = I(JCC[o - 0x70], (Op("rel", t),), JCC_, (cs, t))
    elif 0x80 <= o <= 0x83:
        sz = 1 if o in (0x80, 0x82) else 2
        reg, e, _ = _modrm(rd, sz, seg)
        v = rd.w() if o == 0x81 else (rd.sb() & 0xFFFF if o == 0x83 else rd.b())
        ins = I(ALU[reg], (e, imm(v)))
    elif o in (0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B):
        sz = 1 if (o & 1) == 0 else 2
        reg, e, _ = _modrm(rd, sz, seg)
        g = r8(reg) if sz == 1 else r16(reg)
        mn = {0x84: "TEST", 0x86: "XCHG", 0x88: "MOV", 0x8A: "MOV"}[o & 0xFE]
        ins = I(mn, (g, e) if o >= 0x8A else (e, g))
    elif o == 0x8C:
        reg, e, _ = _modrm(rd, 2, seg); ins = I("MOV", (e, Op("sreg", reg & 3)))
    elif o == 0x8D:
        reg, e, _ = _modrm(rd, 2, seg); e.size = 0; ins = I("LEA", (r16(reg), e))
    elif o == 0x8E:
        reg, e, _ = _modrm(rd, 2, seg); ins = I("MOV", (Op("sreg", reg & 3), e))
    elif o == 0x8F:
        reg, e, _ = _modrm(rd, 2, seg); ins = I("POP", (e,))
    elif o == 0x90: ins = I("NOP")
    elif 0x91 <= o <= 0x97: ins = I("XCHG", (r16(0), r16(o & 7)))
    elif o == 0x98: ins = I("CBW")
    elif o == 0x99: ins = I("CWD")
    elif o == 0x9A:
        off = rd.w(); s = rd.w(); ins = I("CALLF", (Op("far", (s, off)),), CALLF, (s, off))
    elif o == 0x9B: ins = I("WAIT")
    elif 0x9C <= o <= 0x9F: ins = I(["PUSHF", "POPF", "SAHF", "LAHF"][o - 0x9C])
    elif 0xA0 <= o <= 0xA3:
        sz = 1 if (o & 1) == 0 else 2
        m = Op("mem", size=sz, disp=rd.w(), seg=seg)
        a = r8(0) if sz == 1 else r16(0)
        ins = I("MOV", (a, m) if o < 0xA2 else (m, a))
    elif 0xA4 <= o <= 0xAF and o not in (0xA8, 0xA9):
        ins = I(["MOVSB", "MOVSW", "CMPSB", "CMPSW", None, None, "STOSB", "STOSW",
                 "LODSB", "LODSW", "SCASB", "SCASW"][o - 0xA4])
        if seg: ins.ops = [Op("sreg", SREG.index(seg))]
    elif o == 0xA8: ins = I("TEST", (r8(0), imm(rd.b())))
    elif o == 0xA9: ins = I("TEST", (r16(0), imm(rd.w())))
    elif 0xB0 <= o <= 0xB7: ins = I("MOV", (r8(o & 7), imm(rd.b())))
    elif 0xB8 <= o <= 0xBF: ins = I("MOV", (r16(o & 7), imm(rd.w())))
    elif o in (0xC0, 0xC1, 0xD0, 0xD1, 0xD2, 0xD3):
        sz = 1 if (o & 1) == 0 else 2
        reg, e, _ = _modrm(rd, sz, seg)
        cnt = imm(rd.b()) if o < 0xD0 else (imm(1) if o < 0xD2 else r8(1))
        ins = I(SHF[reg], (e, cnt))
    elif o == 0xC2: ins = I("RET", (imm(rd.w()),), RET)
    elif o == 0xC3: ins = I("RET", (), RET)
    elif o in (0xC4, 0xC5):
        reg, e, _ = _modrm(rd, 4, seg); ins = I("LES" if o == 0xC4 else "LDS", (r16(reg), e))
    elif o in (0xC6, 0xC7):
        sz = 1 if o == 0xC6 else 2
        reg, e, _ = _modrm(rd, sz, seg)
        ins = I("MOV", (e, imm(rd.b() if sz == 1 else rd.w())))
    elif o == 0xC8: a = rd.w(); b = rd.b(); ins = I("ENTER", (imm(a), imm(b)))
    elif o == 0xC9: ins = I("LEAVE")
    elif o == 0xCA: ins = I("RETF", (imm(rd.w()),), RETF)
    elif o == 0xCB: ins = I("RETF", (), RETF)
    elif o == 0xCC: ins = I("INT", (imm(3),))
    elif o == 0xCD: ins = I("INT", (imm(rd.b()),))
    elif o == 0xCE: ins = I("INTO")
    elif o == 0xCF: ins = I("IRET", (), IRET)
    elif o in (0xD4, 0xD5): ins = I("AAM" if o == 0xD4 else "AAD", (imm(rd.b()),))
    elif o == 0xD7: ins = I("XLAT")
    elif 0xD8 <= o <= 0xDF:
        reg, e, _ = _modrm(rd, 2, seg); ins = I("ESC", (imm(((o & 7) << 3) | reg), e))
    elif 0xE0 <= o <= 0xE3:
        d = rd.sb(); t = (rd.ip + d) & 0xFFFF
        ins = I(["LOOPNZ", "LOOPZ", "LOOP", "JCXZ"][o - 0xE0], (Op("rel", t),), JCC_, (cs, t))
    elif 0xE4 <= o <= 0xE7:
        p = imm(rd.b()); a = r8(0) if (o & 1) == 0 else r16(0)
        ins = I("IN", (a, p)) if o < 0xE6 else I("OUT", (p, a))
    elif o == 0xE8:
        d = rd.w(); t = (rd.ip + d) & 0xFFFF; ins = I("CALL", (Op("rel", t),), CALL, (cs, t))
    elif o == 0xE9:
        d = rd.w(); t = (rd.ip + d) & 0xFFFF; ins = I("JMP", (Op("rel", t),), JMP, (cs, t))
    elif o == 0xEA:
        off = rd.w(); s = rd.w(); ins = I("JMPF", (Op("far", (s, off)),), JMPF, (s, off))
    elif o == 0xEB:
        d = rd.sb(); t = (rd.ip + d) & 0xFFFF; ins = I("JMP", (Op("rel", t),), JMP, (cs, t))
    elif 0xEC <= o <= 0xEF:
        a = r8(0) if (o & 1) == 0 else r16(0); dx = r16(2)
        ins = I("IN", (a, dx)) if o < 0xEE else I("OUT", (dx, a))
    elif o == 0xF4: ins = I("HLT", (), HALT)
    elif o == 0xF5: ins = I("CMC")
    elif o in (0xF6, 0xF7):
        sz = 1 if o == 0xF6 else 2
        reg, e, _ = _modrm(rd, sz, seg)
        mn = ["TEST", "TEST", "NOT", "NEG", "MUL", "IMUL", "DIV", "IDIV"][reg]
        if reg < 2: ins = I(mn, (e, imm(rd.b() if sz == 1 else rd.w())))
        else: ins = I(mn, (e,))
    elif 0xF8 <= o <= 0xFD: ins = I(["CLC", "STC", "CLI", "STI", "CLD", "STD"][o - 0xF8])
    elif o == 0xFE:
        reg, e, _ = _modrm(rd, 1, seg)
        ins = I(["INC", "DEC"][reg], (e,)) if reg < 2 else I("BAD", (), BAD)
    elif o == 0xFF:
        reg, e, mod = _modrm(rd, 2, seg)
        if reg == 0: ins = I("INC", (e,))
        elif reg == 1: ins = I("DEC", (e,))
        elif reg == 2: ins = I("CALL", (e,), ICALL)
        elif reg == 3: e.size = 4; ins = I("CALLF", (e,), ICALLF)
        elif reg == 4: ins = I("JMP", (e,), IJMP)
        elif reg == 5: e.size = 4; ins = I("JMPF", (e,), IJMPF)
        elif reg == 6: ins = I("PUSH", (e,))
        else: ins = I("BAD", (), BAD)
    else:
        ins = I("BAD", (), BAD)

    ins.length = (rd.ip - ip) & 0xFFFF
    ins.raw = bytes(mem[((cs << 4) + ((ip + i) & 0xFFFF)) & 0xFFFFF] for i in range(ins.length))
    if seg and not any(op.kind == "mem" for op in ins.ops) and not ins.mnem.endswith(("SB", "SW")):
        ins.prefix = ins.prefix + [seg + ":"]
    return ins


def _nec(rd, seg, I, r8, imm):
    o2 = rd.b()
    if 0x10 <= o2 <= 0x1F:
        mn = ["TEST1", "CLR1", "SET1", "NOT1"][(o2 >> 1) & 3]
        sz = 1 if (o2 & 1) == 0 else 2
        _, e, _ = _modrm(rd, sz, seg)
        bit = imm(rd.b()) if o2 >= 0x18 else r8(1)
        return I(mn, (e, bit))
    if o2 in (0x20, 0x22, 0x26): return I({0x20: "ADD4S", 0x22: "SUB4S", 0x26: "CMP4S"}[o2])
    if o2 in (0x28, 0x2A):
        _, e, _ = _modrm(rd, 1, seg); return I("ROL4" if o2 == 0x28 else "ROR4", (e,))
    if o2 in (0x31, 0x33):
        m = rd.b(); return I("INS" if o2 == 0x31 else "EXT", (r8(m & 7), r8((m >> 3) & 7)))
    if o2 in (0x39, 0x3B):
        m = rd.b(); return I("INS" if o2 == 0x39 else "EXT", (r8(m & 7), imm(rd.b())))
    if o2 == 0xFF: return I("BRKEM", (imm(rd.b()),), BAD)
    return I("BAD", (), BAD)
