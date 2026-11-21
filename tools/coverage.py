#!/usr/bin/env python3
"""Merge --coverage files and list routines never entered. Usage: coverage.py ROUTINES.txt COV..."""
import collections, re, sys
c = collections.Counter()
for f in sys.argv[2:]:
    for l in open(f):
        o, n = l.split(); c[int(o, 16)] += int(n)
allr = {int(m.group(1), 16) for m in re.finditer(r"L\d+ ([0-9A-F]{4})", open(sys.argv[1]).read())}
print("entered", len(set(c) & allr), "of", len(allr))
print("never:", " ".join(f"{r:04X}" for r in sorted(allr - set(c))))
