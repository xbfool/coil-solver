#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import re
import os

DD = {"L": (-1, 0), "U": (0, -1), "R": (1, 0), "D": (0, 1)}
for fn in sorted(os.listdir("solutions"), key=lambda s: int(s.split(".")[0])):
    L = fn.split(".")[0]
    if int(L) > 140:
        continue
    raw = open(f"/mnt/d/workspace/coilbench/levels_all/{L}").read()
    w = int(re.search(r"x=(\d+)", raw).group(1))
    h = int(re.search(r"y=(\d+)", raw).group(1))
    b = re.search(r"board=([.X]+)", raw).group(1)
    free = set((x, y) for y in range(h) for x in range(w) if b[y * w + x] == ".")
    sol = open(f"solutions/{fn}").read()
    sx = int(re.search(r"x=(\d+)", sol).group(1))
    sy = int(re.search(r"y=(\d+)", sol).group(1))
    path = re.search(r"path=([UDLR]+)", sol).group(1)
    cur = (sx, sy)
    vset = {cur}
    for ch in path:
        d = DD[ch]
        while (cur[0] + d[0], cur[1] + d[1]) in free and (cur[0] + d[0], cur[1] + d[1]) not in vset:
            cur = (cur[0] + d[0], cur[1] + d[1])
            vset.add(cur)
    dist = abs(cur[0] - sx) + abs(cur[1] - sy)
    tag = "  [CYCLE!]" if dist == 1 else ""
    print(f"L{L}: start=({sx},{sy}) end={cur} L1={dist}{tag}")
