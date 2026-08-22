#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""LP 边固定探针（研究原型，2026-08-24）

假设链：零信息类的病根是 estate 全空（流松弛下所有边都可行）。比流更强的下一层
松弛是「度约束 LP + 子回路消除割（+ 将来 blossom 割）」。对每条边 e 做两次探针：
  强制 x_e=1 -> LP 不可行 => e 全局禁用；强制 x_e=0 -> 不可行 => e 全局必用。
固定率 >0 就是零信息类的破壁信号（v48 教训：estate 密度是深传播威力之源）。

哈密顿路径的 LP 松弛（无向边变量 x_e ∈ [0,1]）：
  Σ_{e∋v} x_e = 2  对内部格；路径两端点未知 => 引入端点松弛：
  Σ_{e∋v} x_e ≥ 1 且 ≤ 2 对所有格，Σ_v (2 - deg_v) = 2（恰好两个端点）
  等价形式：Σ_e x_e = n - 1（边总数）+ 每格度 ∈ [1,2]。
子回路消除（对分数支持图迭代分离）：对割集 S（1<|S|<n）：x(δ(S)) ≥ ... 路径版：
  x(E(S)) ≤ |S| - 1（S 内部边数至多 |S|-1，防止 S 内成环/成满圈）。
分离：在分数支持图上找 x(E(S)) > |S|-1 的 S —— 用最小割/连通分量启发式。

用 scipy.optimize.linprog(HiGHS)。探针只判可行性（目标 0）。
先抽样 SAMPLE 条边测固定率，不全量跑。
"""
import re
import sys
import random
import numpy as np
from scipy.optimize import linprog
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

def solve_feasible(n, edges, fixed, cuts):
    """度约束 LP + 已加的割；fixed: {edge_idx: 0/1}。返回 (feasible, x)."""
    m = len(edges)
    # 变量：x_e。约束：
    #  每格度 >= 1（-deg <= -1)、度 <= 2；总边数 = n-1；割：x(E(S)) <= |S|-1
    A_ub = lil_matrix((2 * n + len(cuts), m))
    b_ub = np.zeros(2 * n + len(cuts))
    for ei, (u, v) in enumerate(edges):
        A_ub[u, ei] = 1; A_ub[v, ei] = 1          # deg <= 2
        A_ub[n + u, ei] = -1; A_ub[n + v, ei] = -1  # -deg <= -1
    b_ub[:n] = 2
    b_ub[n:2 * n] = -1
    for k, S in enumerate(cuts):
        Sset = S
        for ei, (u, v) in enumerate(edges):
            if u in Sset and v in Sset:
                A_ub[2 * n + k, ei] = 1
        b_ub[2 * n + k] = len(S) - 1
    A_eq = lil_matrix((1, m)); A_eq[0, :] = 1
    b_eq = [n - 1]
    bounds = []
    for ei in range(m):
        if ei in fixed:
            bounds.append((fixed[ei], fixed[ei]))
        else:
            bounds.append((0, 1))
    r = linprog(np.zeros(m), A_ub=A_ub.tocsr(), b_ub=b_ub,
                A_eq=A_eq.tocsr(), b_eq=b_eq, bounds=bounds, method="highs")
    return r.status == 0, (r.x if r.status == 0 else None)

def separate_subtour(n, edges, x, thresh=1e-6):
    """启发式分离：在支持图（x_e>eps）上找连通分量 S，若 x(E(S)) > |S|-1+eps 返回 S。
    再对『分数环』做一个廉价检测：支持图里 x 值和超过 |S|-1 的小环。"""
    import collections
    adj = collections.defaultdict(list)
    for ei, (u, v) in enumerate(edges):
        if x[ei] > thresh:
            adj[u].append((v, ei)); adj[v].append((u, ei))
    seen = set()
    comps = []
    for s0 in adj:
        if s0 in seen:
            continue
        comp = set([s0]); q = [s0]; seen.add(s0)
        while q:
            u = q.pop()
            for v, _ in adj[u]:
                if v not in seen:
                    seen.add(v); comp.add(v); q.append(v)
        comps.append(comp)
    viol = []
    for comp in comps:
        if len(comp) == n:
            continue
        val = sum(x[ei] for ei, (u, v) in enumerate(edges) if u in comp and v in comp)
        if val > len(comp) - 1 + 1e-6:
            viol.append(frozenset(comp))
    return viol

def probe_edge(n, edges, ei, val, cuts, max_rounds=8):
    fixed = {ei: val}
    cs = list(cuts)
    for _ in range(max_rounds):
        ok, x = solve_feasible(n, edges, fixed, cs)
        if not ok:
            return False, cs                    # 不可行 => 该假设被证伪
        v = separate_subtour(n, edges, x)
        if not v:
            return True, cs                     # 割加完仍可行 => 探不出矛盾
        cs.extend(v)
    return True, cs

def main():
    path = sys.argv[1]
    sample = int(sys.argv[2]) if len(sys.argv) > 2 else 60
    seed = int(sys.argv[3]) if len(sys.argv) > 3 else 42
    w, h, free = load(path)
    cells, ci, edges = build(free)
    n, m = len(cells), len(edges)
    print(f"board {w}x{h} n={n} m={m}")
    ok, x = solve_feasible(n, edges, {}, [])
    print("base LP feasible:", ok)
    if not ok:
        print("!! 基础 LP 不可行——整关无解（不该发生）")
        return
    random.seed(seed)
    idxs = random.sample(range(m), min(sample, m))
    cuts = []
    nban = nforce = 0
    for k, ei in enumerate(idxs):
        okb, cuts = probe_edge(n, edges, ei, 1, cuts)
        if not okb:
            nban += 1
            continue
        okf, cuts = probe_edge(n, edges, ei, 0, cuts)
        if not okf:
            nforce += 1
        if (k + 1) % 10 == 0:
            print(f"  {k+1}/{len(idxs)} probed, ban={nban} force={nforce} cuts={len(cuts)}")
    print(f"RESULT sample={len(idxs)} banned={nban} forced={nforce} fixrate={100.0*(nban+nforce)/len(idxs):.1f}% cuts={len(cuts)}")

if __name__ == "__main__":
    main()
