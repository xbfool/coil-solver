#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""零类关卡结构对比：L501（未破）vs 已破零类关"""
import re
from collections import Counter

def load(L):
    raw = open(f"/mnt/d/workspace/coilbench/levels_all/{L}").read()
    w = int(re.search(r"x=(\d+)", raw).group(1))
    h = int(re.search(r"y=(\d+)", raw).group(1))
    b = re.search(r"board=([.X]+)", raw).group(1)
    return w, h, b

def stats(L):
    w, h, b = load(L)
    free = set((x, y) for y in range(h) for x in range(w) if b[y * w + x] == ".")
    nfree = len(free)
    deg = Counter()
    for (x, y) in free:
        d = sum(1 for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)) if (x + dx, y + dy) in free)
        deg[d] += 1
    # 墙连通块（4 邻接，不含边界外）
    walls = set((x, y) for y in range(h) for x in range(w) if b[y * w + x] == "X")
    seen = set()
    comps = []
    for c in walls:
        if c in seen:
            continue
        q = [c]; seen.add(c); n = 0
        while q:
            u = q.pop(); n += 1
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                v = (u[0] + dx, u[1] + dy)
                if v in walls and v not in seen:
                    seen.add(v); q.append(v)
        comps.append(n)
    comps.sort(reverse=True)
    # 每行/列的墙数最小值（“空走廊”指标）
    roww = [sum(1 for x in range(w) if b[y * w + x] == "X") for y in range(h)]
    colw = [sum(1 for y in range(h) if b[y * w + x] == "X") for x in range(w)]
    zrow = sum(1 for v in roww if v == 0)
    zcol = sum(1 for v in colw if v == 0)
    # 最大内接无墙矩形（粗略：直方图法求最大全自由子矩形面积）
    hh = [0] * w
    best = 0
    for y in range(h):
        for x in range(w):
            hh[x] = hh[x] + 1 if (x, y) in free else 0
        st = []
        for x in range(w + 1):
            cur = hh[x] if x < w else 0
            while st and hh[st[-1]] >= cur:
                ht = hh[st.pop()]
                left = st[-1] + 1 if st else 0
                best = max(best, ht * (x - left))
            st.append(x)
    d2 = 100.0 * deg[2] / nfree; d3 = 100.0 * deg[3] / nfree; d4 = 100.0 * deg[4] / nfree
    return (f"L{L}: {w}x{h} free={nfree}({100.0*nfree/(w*h):.0f}%) "
            f"deg2/3/4={d2:.0f}/{d3:.0f}/{d4:.0f}% "
            f"墙块数={len(comps)} 最大墙块={comps[0] if comps else 0} 单格墙比={100.0*sum(1 for c in comps if c==1)/max(1,len(comps)):.0f}% "
            f"空行/空列={zrow}/{zcol} 最大无墙矩形={best}")

for L in [414, 458, 475, 496, 501, 502, 503]:
    try:
        print(stats(L))
    except Exception as e:
        print(L, "ERR", e)
