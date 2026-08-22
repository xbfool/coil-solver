#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""相位 A：不用解知识的图哈密顿环搜索（度=2 + 子回路割迭代 MILP）

找到环 → 调 arc_scan 逻辑验缝隙 → 报告：环与真环一致？缝隙集是什么？
"""
import re
import sys
import numpy as np
from scipy.optimize import milp, LinearConstraint, Bounds
from scipy.sparse import lil_matrix

LV = sys.argv[1] if len(sys.argv) > 1 else "414"

raw = open(f"/mnt/d/workspace/coilbench/levels_all/{LV}").read()
w = int(re.search(r"x=(\d+)", raw).group(1))
h = int(re.search(r"y=(\d+)", raw).group(1))
b = re.search(r"board=([.X]+)", raw).group(1)
free = sorted((x, y) for y in range(h) for x in range(w) if b[y * w + x] == ".")
ci = {c: i for i, c in enumerate(free)}
n = len(free)
edges = []
for (x, y) in free:
    for dx, dy in ((1, 0), (0, 1)):
        if (x + dx, y + dy) in ci:
            edges.append((ci[(x, y)], ci[(x + dx, y + dy)]))
m = len(edges)
print(f"L{LV}: n={n} m={m}", flush=True)

A_deg = lil_matrix((n, m))
for ei, (u, v) in enumerate(edges):
    A_deg[u, ei] = 1
    A_deg[v, ei] = 1
A_deg = A_deg.tocsr()

def solve(cuts):
    cons = [LinearConstraint(A_deg, np.full(n, 2), np.full(n, 2))]
    for S in cuts:
        row = np.zeros((1, m))
        cnt = 0
        for ei, (u, v) in enumerate(edges):
            if u in S and v in S:
                row[0, ei] = 1
                cnt += 1
        cons.append(LinearConstraint(row, [-np.inf], [len(S) - 1]))
    r = milp(np.zeros(m), constraints=cons, bounds=Bounds(0, 1),
             integrality=np.ones(m), options={"time_limit": 1200})
    return r

def components(x):
    import collections
    adj = collections.defaultdict(list)
    for ei, (u, v) in enumerate(edges):
        if x[ei] > 0.5:
            adj[u].append(v)
            adj[v].append(u)
    seen = set()
    comps = []
    for s0 in range(n):
        if s0 in seen:
            continue
        comp = {s0}
        q = [s0]
        seen.add(s0)
        while q:
            u = q.pop()
            for v in adj[u]:
                if v not in seen:
                    seen.add(v)
                    comp.add(v)
                    q.append(v)
        comps.append(comp)
    return comps

cuts = []
for it in range(60):
    r = solve(cuts)
    if r.status != 0:
        print(f"迭代{it}: MILP status={r.status}（无环或超时）")
        break
    comps = components(r.x)
    print(f"迭代{it}: 分量数 {len(comps)}", flush=True)
    if len(comps) == 1:
        used = set(ei for ei in range(m) if r.x[ei] > 0.5)
        # 和真环比对
        sol = open(f"solutions/{LV}.sol").read()
        sx = int(re.search(r"x=(\d+)", sol).group(1))
        sy = int(re.search(r"y=(\d+)", sol).group(1))
        path = re.search(r"path=([UDLR]+)", sol).group(1)
        DD = {"L": (-1, 0), "U": (0, -1), "R": (1, 0), "D": (0, 1)}
        cur = (sx, sy)
        vset = {cur}
        tru = set()
        for ch in path:
            d = DD[ch]
            while (cur[0] + d[0], cur[1] + d[1]) in ci and (cur[0] + d[0], cur[1] + d[1]) not in vset:
                nxt = (cur[0] + d[0], cur[1] + d[1])
                a2, b2 = ci[cur], ci[nxt]
                for ei, (u, v) in enumerate(edges):
                    pass
                tru.add((min(a2, b2), max(a2, b2)))
                cur = nxt
                vset.add(cur)
        # 闭合边
        e_close = (min(ci[(sx, sy)], ci[cur]), max(ci[(sx, sy)], ci[cur]))
        tru.add(e_close)
        used_pairs = set((min(edges[ei]), max(edges[ei])) for ei in used)
        inter = len(used_pairs & tru)
        print(f"找到图环！与真环边重合 {inter}/{n}（真环边数 {len(tru)}）")
        print("SAME" if used_pairs == tru else "DIFFERENT-CYCLE")
        break
    comps.sort(key=len)
    for csub in comps[:-1][:20]:
        cuts.append(frozenset(csub))
