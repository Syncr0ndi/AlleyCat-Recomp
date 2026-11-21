#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Syncr0ndi
"""Assemble the player release in dist/AlleyCat/ and zip it.

Contents: alleycat.exe (the build made only from the hand-written C), README.txt, and
LICENSES/ with this port's MIT licence and SDL2's zlib licence. The game file itself is never included; as a
safety check the program is scanned for any 32-byte run of the original CAT.EXE.

Usage: python tools/make_release.py CAT.EXE [--build build/rel]"""
import argparse, shutil, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ap = argparse.ArgumentParser()
ap.add_argument("cat_exe")
ap.add_argument("--build", default=str(ROOT / "build" / "rel"))
a = ap.parse_args()
b = Path(a.build)
exe = b / "alleycat_release.exe"
if not exe.exists(): sys.exit(f"{exe} not built (cmake --build {b} --target alleycat_release)")

game = Path(a.cat_exe).read_bytes()
prog = exe.read_bytes()
grams = {game[i:i + 32] for i in range(len(game) - 32) if len(set(game[i:i + 32])) > 4}
hits = [i for i in range(len(prog) - 32) if prog[i:i + 32] in grams]
if hits: sys.exit(f"refusing: {exe.name} contains {len(hits)} 32-byte runs of CAT.EXE (first at {hits[0]:#x})")

out = ROOT / "dist" / "AlleyCat"
if out.exists(): shutil.rmtree(out)
out.mkdir(parents=True)
shutil.copy2(exe, out / "alleycat.exe")
shutil.copy2(ROOT / "release" / "README.txt", out / "README.txt")
lic = out / "LICENSES"
lic.mkdir()
shutil.copy2(ROOT / "LICENSE", lic / "AlleyCat-port-LICENSE.txt")
sdl = next(iter((b / "_deps" / "sdl2-src").glob("LICENSE*")), None)
if not sdl: sys.exit("SDL2 licence not found in the build folder")
shutil.copy2(sdl, lic / "SDL2-LICENSE.txt")
z = ROOT / "dist" / "AlleyCat-Windows.zip"
with zipfile.ZipFile(z, "w", zipfile.ZIP_DEFLATED) as zf:
    for f in sorted(p for p in out.rglob("*") if p.is_file()):
        zf.write(f, "AlleyCat/" + f.relative_to(out).as_posix())
print(f"OK: {out} and {z} ({z.stat().st_size // 1024} KB); no CAT.EXE content found in the program")
