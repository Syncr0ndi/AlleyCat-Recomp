#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Syncr0ndi
"""Line coverage of the hand-written C (src/game) under check-mode autoplay runs.

Builds build/cov (clang source-based coverage, no SDL app) if needed, runs the check seeds,
and prints, per file, the percentage of lines executed and the line ranges never executed.
Code that never ran has not been compared with the original yet.

Usage: python tools/hand_coverage.py CAT.EXE [--seeds N] [--frames N] [--files lines.c,cat.c]"""
import argparse, concurrent.futures, os, re, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
B = ROOT / "build" / "cov"
ap = argparse.ArgumentParser()
ap.add_argument("cat_exe")
ap.add_argument("--seeds", type=int, default=12)
ap.add_argument("--frames", type=int, default=10000)
ap.add_argument("--files", default="")
ap.add_argument("--room", type=int, action="append", default=[], help="also run seeds pinned to room N")
a = ap.parse_args()

flags = "-fprofile-instr-generate -fcoverage-mapping"
if not (B / "build.ninja").exists():
    subprocess.run(["cmake", "-S", str(ROOT), "-B", str(B), "-G", "Ninja", "-DCMAKE_C_COMPILER=clang",
                    "-DCAT_BUILD_APP=OFF", f"-DCAT_EXE={a.cat_exe}", f"-DCMAKE_C_FLAGS={flags}",
                    f"-DCMAKE_EXE_LINKER_FLAGS={flags}"], check=True, stdout=subprocess.DEVNULL)
subprocess.run(["cmake", "--build", str(B), "--target", "cat_test"], check=True, stdout=subprocess.DEVNULL)
exe = B / "cat_test.exe"
prof = B / "prof"
prof.mkdir(exist_ok=True)
for f in prof.glob("*.profraw"): f.unlink()


def run(job):
    seed, room = job
    env = dict(os.environ, LLVM_PROFILE_FILE=str(prof / f"s{seed}_{room}.profraw"))
    extra = ["--room", str(room)] if room >= 0 else ["--rooms"]
    subprocess.run([str(exe), a.cat_exe, "--hand-mode", "check", "--autoplay", str(seed), "--frames", str(a.frames)] + extra,
                   env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


jobs = [(s, -1) for s in range(1, a.seeds + 1)] + [(s, r) for r in a.room for s in range(1, 5)]
with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
    list(ex.map(run, jobs))
subprocess.run(["llvm-profdata", "merge", "-o", str(prof / "all.profdata")] + [str(p) for p in prof.glob("*.profraw")], check=True)
srcs = sorted(p.relative_to(ROOT).as_posix() for p in (ROOT / "src" / "game").glob("*.c"))
if a.files: srcs = [s for s in srcs if Path(s).name in a.files.split(",")]
out = subprocess.run(["llvm-cov", "show", str(exe), f"-instr-profile={prof / 'all.profdata'}", "-show-line-counts", f"-compilation-dir={ROOT}"]
                     + srcs, capture_output=True, text=True, cwd=ROOT).stdout
cur, lines = None, {}
for ln in out.splitlines():
    m = re.match(r"^(\S.*\.c):$", ln)
    if m: cur = Path(m.group(1)).name; lines[cur] = []; continue
    m = re.match(r"^\s*(\d+)\|\s*([0-9.kM]*)\|(.*)$", ln)
    if m and cur: lines[cur].append((int(m.group(1)), m.group(2), m.group(3)))
for name, ls in lines.items():
    counted = [(n, cnt, t) for n, cnt, t in ls if cnt != ""]
    zero = [n for n, cnt, t in counted if cnt == "0"]
    pct = 100 * (len(counted) - len(zero)) / max(1, len(counted))
    print(f"{name}: {pct:.1f}% of {len(counted)} lines")
    ranges, start, prev = [], None, None
    for n in zero:
        if start is None: start = prev = n
        elif n == prev + 1: prev = n
        else: ranges.append((start, prev)); start = prev = n
    if start is not None: ranges.append((start, prev))
    if ranges: print("   never ran: " + ", ".join(f"{s}" if s == e else f"{s}-{e}" for s, e in ranges))
