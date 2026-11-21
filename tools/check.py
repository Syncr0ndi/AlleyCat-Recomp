#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Syncr0ndi
"""Check every hand-written routine against the recompiled code over several autoplay runs.

Usage: python tools/check.py CAT.EXE [--seeds N] [--frames N] [--exe build/rel/cat_test.exe]
Runs the seeds in parallel, merges the per-routine counts and prints the first mismatches.
Exit code 0 only if there were no mismatches."""
import argparse, collections, concurrent.futures, os, re, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ap = argparse.ArgumentParser()
ap.add_argument("cat_exe")
ap.add_argument("--seeds", type=int, default=8)
ap.add_argument("--frames", type=int, default=6000)
ap.add_argument("--exe", default=str(ROOT / "build" / "rel" / "cat_test.exe"))
ap.add_argument("--room", type=int, action="append", default=[], help="also run seeds pinned to room N (repeatable)")
ap.add_argument("--all-rooms", action="store_true", help="same as --room 1 ... --room 7")
a = ap.parse_args()


if a.all_rooms: a.room = list(range(1, 8))


def run(job):
    seed, room = job
    extra = ["--room", str(room)] if room >= 0 else ["--rooms"] + (["--joystick"] if room == -2 else [])
    play = ["--autoplay", str(seed)]
    if room == -3:                                       # start a game, then Ctrl-Y: quit to DOS
        play, extra = [], ["--key", "100:39", "--key", "160:31", "--key", "240:25", "--key", "320:39",
                           "--key", "600:1d:20", "--key", "605:15:5"]
    p = subprocess.run([a.exe, a.cat_exe, "--hand-mode", "check", "--frames", str(a.frames)] + play + extra,
                       capture_output=True, text=True)
    return f"{seed}" + (f" room {room}" if room >= 0 else " joystick" if room == -2 else " quit" if room == -3 else ""), p.stdout, p.stderr


calls, bad, flags, nested = collections.Counter(), collections.Counter(), collections.Counter(), collections.Counter()
errors = []
with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
    jobs = [(s, -1) for s in range(1, a.seeds + 1)] + [(s, r) for r in a.room for s in range(1, 5)] + [(s, -2) for s in range(1, 5)] + [(1, -3)]
    for seed, out, err in ex.map(run, jobs):
        for m in re.finditer(r"hand_([0-9a-f]{4}):\s+(\d+) calls checked, (\d+) mismatches, (\d+) flag-only differences, (\d+) nested", out):
            o = m.group(1); calls[o] += int(m.group(2)); bad[o] += int(m.group(3)); flags[o] += int(m.group(4)); nested[o] += int(m.group(5))
        if err.strip(): errors.append(f"--- seed {seed}\n" + "\n".join(err.strip().splitlines()[:30]))
        if "end: frame" not in out: errors.append(f"--- seed {seed}: run did not finish\n{out[-500:]}")

done = set()
for f in (ROOT / "src" / "game").glob("*.c"):
    done |= set(re.findall(r"^void hand_([0-9a-f]{4})\(V30 \*c\)", f.read_text(), re.M))
for o in sorted(done):
    tag = ("NEVER CALLED" if not nested[o] else "OK (inside callers)") if not calls[o] else ("OK" if not bad[o] else "MISMATCH")
    print(f"  {o.upper()}  {calls[o]:8d} calls  {nested[o]:8d} nested  {bad[o]:5d} bad  {tag}")
total = sum(bad.values())
print(f"{len(done)} routines, {sum(calls.values())} calls checked, {total} mismatches, "
      f"{sum(1 for o in done if not calls[o] and not nested[o])} never called")
for e in errors[:4]: print(e)
sys.exit(1 if total or errors else 0)
