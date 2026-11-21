#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Syncr0ndi
"""Routine table for the rewrite: size, callees, I/O, and a bottom-up order.
Usage: python tools/callgraph.py CAT.EXE"""
import sys, collections, re
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import catmap
from v30dec import CALL, JMP, JCC_

m = catmap.main(sys.argv[1], quiet=True)
CS = catmap.CODE_SEG * 16
size, callees, io, root_of = collections.Counter(), collections.defaultdict(set), collections.defaultdict(set), {}
for l, ins in m["code"].items():
    r = m["code_root"][l] - CS
    size[r] += ins.length
    if ins.flow == CALL: callees[r].add(ins.target[1])
    elif ins.flow in (JMP, JCC_) and ins.target is not None:
        t = m["canon"](*ins.target)
        if t in m["code"] and m["code_root"][t] - CS != r: callees[r].add(("jmp", m["code_root"][t] - CS))
    if ins.mnem in ("IN", "OUT"): io[r].add("io")
    if ins.mnem == "INT": io[r].add(f"int{ins.ops[0].val:x}")
    if ins.mnem in ("CLI", "STI", "IRET"): io[r].add(ins.mnem.lower())
done = set()
for f in Path(__file__).resolve().parents[1].glob("src/game/*.c"):
    done |= {int(h, 16) for h in re.findall(r"^void hand_([0-9a-f]{4})\(V30 \*c\)", f.read_text(), re.M)}
def deps(r): return {c if isinstance(c, int) else c[1] for c in callees[r]}
level = {}
def lv(r, seen=()):
    if r in level: return level[r]
    if r in seen: return 0
    level[r] = 0
    level[r] = 1 + max([lv(d, seen + (r,)) for d in deps(r)] or [-1])
    return level[r]
for r in size: lv(r)
for r in sorted(size, key=lambda r: (level[r], r)):
    cl = " ".join(f"{c:04X}" if isinstance(c, int) else f"j{c[1]:04X}" for c in sorted(callees[r], key=str))
    print(f"{'*' if r in done else ' '} L{level[r]} {r:04X} {size[r]:4d}B {','.join(sorted(io[r])):12s} {cl}")
print(f"done {len(done & set(size))}/{len(size)}", file=sys.stderr)
