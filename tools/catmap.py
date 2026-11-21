#!/usr/bin/env python3
"""Recursive-descent code map of Alley Cat (CAT.EXE, 1984).

CAT.EXE is an unpacked MZ executable of hand-written 8086 code: one code segment (image
0x7230..end) and one data segment (image 0x0100..0x7230). The runtime loads it at the fixed
segment LOAD_SEG (src/pc/pc.h must agree), so code addresses are known at build time.

Seeds: the entry point, the interrupt handlers the game installs, and the jump table at
CS:0250. Usage as a module: main(exe_path) -> map dict for catrecomp.py. As a script it
prints a report and writes build/map/listing.asm.
"""
import collections, hashlib, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from v30dec import decode, SEQ, JMP, JCC_, CALL, RET, JMPF, CALLF, RETF, IRET, IJMP, ICALL, IJMPF, ICALLF, HALT, BAD

CAT_SHA256 = "4979c8867826e08881d1d4bec95c95d6bc1ac70fd21050ccdb4f733a50d7bfdd"
PSP_SEG = 0x1000
LOAD_SEG = PSP_SEG + 0x10           # first paragraph of the load image
CODE_SEG = LOAD_SEG + 0x723         # entry CS after relocation

# (offset in the code segment, reason)
SEEDS = [
    (0x0000, "entry"),
    (0x14B3, "int 09 keyboard handler (PC), installed at 1443"),
    (0x14FB, "int 09 keyboard handler (PCjr), installed at 144D"),
    (0x1554, "int 48 PCjr keyboard translation, installed at 1462"),
]
TABLES = [(0x0250, 8, "game-state dispatch, JMP CS:[BX+0250] at 024B")]


def lin(cs, ip): return ((cs << 4) + (ip & 0xFFFF)) & 0xFFFFF
def canon(cs, ip): return lin(cs, ip)


def load_exe(path):
    """Return (1 MB memory image with CAT.EXE loaded and relocated, header dict)."""
    d = Path(path).read_bytes()
    if hashlib.sha256(d).hexdigest() != CAT_SHA256:
        raise SystemExit(f"{path}: not the known CAT.EXE (SHA-256 mismatch)")
    (sig, last, pages, nrel, hdr, _mn, _mx, ss, sp, _ck, ip, cs, roff, _ov) = struct.unpack("<14H", d[:28])
    size = pages * 512 - (512 - last if last else 0) - hdr * 16
    img = d[hdr * 16: hdr * 16 + size]
    mem = bytearray(0x100000)
    base = LOAD_SEG * 16
    mem[base: base + len(img)] = img
    for i in range(nrel):
        o, s = struct.unpack_from("<HH", d, roff + 4 * i)
        p = base + s * 16 + o
        v = (mem[p] | mem[p + 1] << 8) + LOAD_SEG
        mem[p], mem[p + 1] = v & 0xFF, (v >> 8) & 0xFF
    return mem, {"cs": cs + LOAD_SEG, "ip": ip, "ss": ss + LOAD_SEG, "sp": sp, "size": len(img)}


def main(exe, quiet=False):
    mem, hdr = load_exe(exe)
    assert hdr["cs"] == CODE_SEG and hdr["ip"] == 0
    code, owner, funcs, code_root = {}, {}, {}, {}
    refs = collections.defaultdict(set)
    work = collections.deque()
    indirect, problems = [], []

    def add_func(ip, origin):
        l = canon(CODE_SEG, ip)
        if l not in funcs:
            funcs[l] = (CODE_SEG, ip, origin)
            work.append((CODE_SEG, ip, l))
        return l

    for ip, why in SEEDS: add_func(ip, why)
    for off, cnt, why in TABLES:
        for i in range(cnt):
            p = lin(CODE_SEG, off + 2 * i)
            add_func(mem[p] | mem[p + 1] << 8, f"table CS:{off:04X}[{i}] ({why})")

    while work:
        cs, ip, root = work.popleft()
        while True:
            l = canon(cs, ip)
            if cs != CODE_SEG:
                problems.append(f"{cs:04X}:{ip:04X} flow leaves the code segment"); break
            if l in code: break
            if l in owner:
                problems.append(f"{cs:04X}:{ip:04X} jumps into the middle of the insn at {owner[l]:05x}"); break
            ins = decode(mem, cs, ip)
            if ins.flow == BAD:
                problems.append(f"{cs:04X}:{ip:04X} undecodable bytes {ins.raw.hex()}"); break
            clash = [canon(cs, ip + i) for i in range(1, ins.length) if canon(cs, ip + i) in code]
            if clash:
                problems.append(f"{cs:04X}:{ip:04X} overlaps insn at {clash[0]:05x}"); break
            code[l] = ins; code_root[l] = root
            for i in range(ins.length): owner.setdefault(canon(cs, ip + i), l)
            f = ins.flow
            if ins.target is not None: refs[canon(*ins.target)].add(l)
            if f == CALL:
                add_func(ins.target[1], f"call from {ip:04X}")
            elif f == JMP:
                if ins.target == (cs, ip): break
                work.appendleft((*ins.target, root)); break
            elif f == JCC_:
                work.append((*ins.target, root))
            elif f in (IJMP, ICALL, IJMPF, ICALLF, JMPF, CALLF):
                indirect.append(ins)
                if f in (IJMP, IJMPF, JMPF): break
            elif f in (RET, RETF, IRET, HALT):
                break
            ip = (ip + ins.length) & 0xFFFF

    def label(l):
        return f"sub_{l - CODE_SEG * 16:04x}" if l in funcs else f"loc_{l - CODE_SEG * 16:04x}"

    seg_len = hdr["size"] - 0x7230
    covered = sum(i.length for i in code.values())
    report = [f"instructions {len(code)}, functions {len(funcs)}, code bytes {covered}/{seg_len} "
              f"({covered * 100 / seg_len:.1f}%)", f"indirect/far flow ({len(indirect)}):"]
    report += [f"  {i.cs:04X}:{i.ip:04X} {i.text()}" for i in indirect]
    report += [f"problems ({len(problems)}):"] + [f"  {p}" for p in problems]
    if not quiet:
        print("\n".join(report))
        out = Path(__file__).resolve().parents[1] / "build" / "map"
        out.mkdir(parents=True, exist_ok=True)
        with open(out / "listing.asm", "w") as w:
            prev = None
            for l in sorted(code):
                ins = code[l]
                if prev is not None and l != prev: w.write("\n")
                if l in funcs: w.write(f"\n;---------------- {label(l)}  ({funcs[l][2]})\n")
                elif l in refs: w.write(f"{label(l)}:\n")
                w.write(f"{ins.ip:04X}  {ins.raw.hex():<14} {ins.text()}\n")
                prev = l + ins.length
    if problems: raise SystemExit("code map has problems")
    return {"mem": mem, "code": code, "code_root": code_root, "funcs": funcs, "refs": refs,
            "label": label, "canon": canon, "hdr": hdr}


if __name__ == "__main__":
    main(sys.argv[1])
