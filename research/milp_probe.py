#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""整数边固定探针 = ground truth（2026-08-24）

关键观察：度∈[1,2] + 边数=n-1 + 连通（子回路割迭代到整数解连通为止）的整数解
恰好就是哈密顿路径。所以「强制 x_e=1 的 MILP 可行性」= 「存在过 e 的哈密顿路径」。
这是边固定问题的 ground truth：若抽样边几乎全部可行，说明零信息类的边**真的自由**，
任何割平面（comb/blossom 也不例外）都不可能有固定率——矿脉判死；
若有可观比例不可行，则说明割平面有肉，值得上 blossom 分离。
注意：这里不含滑动规则（滑动只会让可行集更小、禁边更多——所以「图层面自由」是
禁边率的上界判决，足够判死矿脉；反向不成立时才需要更细的分析）。
"""
import re
import sys
import random
import numpy as np
from scipy.optimize import milp, LinearConstraint, Bounds
from scipy.sparse import lil_matrix

def load(path):
    raw = open(path).read()
    w = int(re.search(r"x=(\d+)", raw).group(1))
    h = int(re.search(r"y=(\d+)", raw).group(1))
    b = re.search(r"board=([.X]+)", raw).group(1)
    free = set()
    for y in range(h):
        for x in range(w):
            if b[y * w + x] == ".":
                free.add((x, y))
    return w, h, free

def build(free):
    cells = sorted(free)
    ci = {c: i for i, c in enumerate(cells)}
    edges = []
    for (x, y) in cells:
        for dx, dy in ((1, 0), (0, 1)):
            if (x + dx, y + dy) in free:
                edges.append((ci[(x, y)], ci[(x + dx, y + dy)]))
    return cells, ci, edges

def components(n, edges, x):
    import collections
    adj = collections.defaultdict(list)
    used = set()
    for ei, (u, v) in enumerate(edges):
        if x[ei] > 0.5:
            adj[u].append(v); adj[v].append(u); used.add(u); used.add(v)
    seen = set(); comps = []
    for s0 in range(n):
        if s0 in seen:
            continue
        comp = {s0}; q = [s0]; seen.add(s0)
        while q:
            u = q.pop()
            for v in adj[u]:
                if v not in seen:
                    seen.add(v); comp.add(v); q.append(v)
        comps.append(comp)
    return comps

def probe(n, edges, ei_fix, cuts, max_rounds=40):
    m = len(edges)
    for _ in range(max_rounds):
        cons = []
        A = lil_matrix((n, m))
        for ei, (u, v) in enumerate(edges):
            A[u, ei] = 1; A[v, ei] = 1
        cons.append(LinearConstraint(A.tocsr(), np.ones(n), np.full(n, 2)))
        cons.append(LinearConstraint(np.ones((1, m)), [n - 1], [n - 1]))
        for S in cuts:
            row = np.zeros((1, m))
            for ei, (u, v) in enumerate(edges):
                if u in S and v in S:
                    row[0, ei] = 1
            cons.append(LinearConstraint(row, [-np.inf], [len(S) - 1]))
        lb = np.zeros(m); ub = np.ones(m)
        lb[ei_fix] = 1
        r = milp(np.zeros(m), constraints=cons, bounds=Bounds(lb, ub),
                 integrality=np.ones(m), options={"time_limit": 60})
        if r.status != 0:
            return False, cuts                    # 不可行（或超时算存疑，status 区分）
        comps = components(n, edges, r.x)
        big = [c for c in comps if len(c) > 1 or True]
        if len(comps) == 1:
            return True, cuts                     # 连通整数解 = 真哈密顿路径，e 可行
        # 加最大分量以外全部分量的子回路割
        comps.sort(key=len)
        for c in comps[:-1]:
            cuts.append(frozenset(c))
    return True, cuts                             # 轮数耗尽按可行保守处理

def main():
    path = sys.argv[1]
    sample = int(sys.argv[2]) if len(sys.argv) > 2 else 12
    w, h, free = load(path)
    cells, ci, edges = build(free)
    n, m = len(cells), len(edges)
    print(f"board {w}x{h} n={n} m={m}", flush=True)
    random.seed(7)
    idxs = random.sample(range(m), min(sample, m))
    cuts = []
    nban = 0
    for k, ei in enumerate(idxs):
        ok, cuts = probe(n, edges, ei, cuts)
        if not ok:
            nban += 1
        print(f"  edge {k+1}/{len(idxs)} -> {'BAN' if not ok else 'ok'} (cuts={len(cuts)})", flush=True)
    print(f"RESULT sample={len(idxs)} banned={nban} groundtruth-banrate={100.0*nban/len(idxs):.1f}%")

if __name__ == "__main__":
    main()
