#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import re

def rays(L):
    raw = open(f"/mnt/d/workspace/coilbench/levels_all/{L}").read()
    w = int(re.search(r"x=(\d+)", raw).group(1))
    h = int(re.search(r"y=(\d+)", raw).group(1))
    b = re.search(r"board=([.X]+)", raw).group(1)
    free = [[b[y * w + x] == "." for x in range(w)] for y in range(h)]
    cells = set()
    for y in range(h):
        for x in range(w):
            if not free[y][x]:
                continue
            if (all(free[y][k] for k in range(0, x)) or all(free[y][k] for k in range(x + 1, w))
                    or all(free[k][x] for k in range(0, y)) or all(free[k][x] for k in range(y + 1, h))):
                cells.add((x, y))
    return cells

for L, winners in [(414, [(112, 115), (112, 116)]), (475, [(2, 106)])]:
    rc = rays(L)
    for wxy in winners:
        tag = "RAY-CELL!" if wxy in rc else "not-ray"
        print(f"L{L} winner {wxy}: {tag}  (ray cells={len(rc)})")
