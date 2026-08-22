#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""时序松紧解剖：对每个已知解，找出全部软垫背转弯，量 slack = pos(转弯) - pos(垫背)。
问题：小 slack（刚好赶上）的荷载依赖在地理上是否聚在缝（起终点）附近？"""
import re
import os
import sys

DD = {"L": (-1, 0), "U": (0, -1), "R": (1, 0), "D": (0, 1)}

def anatomize(L):
    raw = open(f"/mnt/d/workspace/coilbench/levels_all/{L}").read()
    w = int(re.search(r"x=(\d+)", raw).group(1))
    h = int(re.search(r"y=(\d+)", raw).group(1))
    b = re.search(r"board=([.X]+)", raw).group(1)
    free = set((x, y) for y in range(h) for x in range(w) if b[y * w + x] == ".")
    sol = open(f"solutions/{L}.sol").read()
    sx = int(re.search(r"x=(\d+)", sol).group(1))
    sy = int(re.search(r"y=(\d+)", sol).group(1))
    path = re.search(r"path=([UDLR]+)", sol).group(1)
    cur = (sx, sy)
    pos = {cur: 0}
    order = [cur]
    stops = []          # (停格, 方向, 垫背格 or None墙)
    for ch in path:
        d = DD[ch]
        while (cur[0] + d[0], cur[1] + d[1]) in free and (cur[0] + d[0], cur[1] + d[1]) not in pos:
            cur = (cur[0] + d[0], cur[1] + d[1])
            pos[cur] = len(order)
            order.append(cur)
        beyond = (cur[0] + d[0], cur[1] + d[1])
        stops.append((cur, ch, beyond if beyond in free else None))
    n = len(order)
    soft = [(q, bk) for (q, ch, bk) in stops if bk is not None]
    rows = []
    for (q, bk) in soft:
        slack = pos[q] - pos[bk]
        dend = min(abs(q[0] - sx) + abs(q[1] - sy),
                   abs(q[0] - order[-1][0]) + abs(q[1] - order[-1][1]))
        rows.append((slack, q, dend))
    rows.sort()
    print(f"L{L}: n={n} 停步={len(stops)} 软垫背={len(soft)}（{100.0*len(soft)/max(1,len(stops)):.0f}%）")
    for (slack, q, dend) in rows[:6]:
        print(f"   slack={slack:4d} 停在{q} 距缝端={dend}")
    if rows:
        tight = [r for r in rows if r[0] <= max(5, n // 20)]
        near = sum(1 for r in tight if r[2] <= max(4, (w + h) // 8))
        print(f"   紧依赖 {len(tight)} 个，其中贴缝 {near} 个")

for L in (sys.argv[1:] or ["13", "16", "18", "22", "27", "30", "38"]):
    anatomize(L)
