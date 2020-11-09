#!/usr/bin/env python3

import os.path

lines = list(open("make.units").readlines())

n = min([len(l) for l in lines])
for i in range(n):
    prefixes = set([ l[:i] for l in lines ])
    if len(prefixes)==1:
        longest = i
        prefix = lines[0][:longest]

stems = [ l[longest:] for l in lines ]
units = []
for s in stems:
    words = s.split(" ")
    for w in words:
        if w[-2:]=='.c':
            units.append(w)

dirs = set([ os.path.dirname(u) for u in units ])
for d in sorted(dirs):
    print(f"mkdir -p ../units/{d}")
for u in units:
    print(f"clang-11 {prefix[4:]} -g -S -emit-llvm {u} -o ../units/{u[:-2]}.ll")
