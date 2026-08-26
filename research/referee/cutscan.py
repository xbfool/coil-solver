#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
割集奇偶演算（cutscan）—— G1'/G2 定理草案的实验器（2026-08-26）

问：87 候选挤在 16×22 小盒里，是否存在小点割集 S 在数学上把"路径端点"逼进那片区域？

法：
  1) Menger：点拆分最大流求 [盒区 A] 与 [远区 B] 的最小点割 S（k=|S|）；
  2) G1'：Σ max(|b_i-w_i|,1) ≤ k+1（割集奇偶不等式，必要条件）；
  3) G2 枚举：端点放置(分量/割格×颜色) × 割-割路径边子集 × 各割格出边分配 →
     每分量线性可行性（2d_i = ΔA_i+Δep_i；s_i 定值且 ≥1；∃n_i 分解；Σs_i = T = k+1-ep_cut-cc）
     → 汇总所有可行放置：若全部放置都在目标分量里放了 ≥1 端点 ⇒ 证书成立。

用法：cutscan.py <board> <x0> <x1> <y0> <y1> [远区距离序列,默认 40,60,80,100]
校准：在已解关上跑，真起点所在区域必须与证书相容（不许出现"端点不可在该区"）。
"""
import sys, itertools
from collections import deque
import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import maximum_flow
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from referee import Board

INF = 1 << 20


def min_vertex_cut(bd: Board, A, B):
    """点拆分最大流：cell c -> in=2i, out=2i+1（i=紧凑编号）；A/B 内部容量 INF（割不落在 A/B）。"""
    cells = bd.cells
    idx = {c: i for i, c in enumerate(cells)}
    n = len(cells)
    SRC, SNK = 2 * n, 2 * n + 1
    rows, cols, caps = [], [], []

    def arc(u, v, cap):
        rows.append(u); cols.append(v); caps.append(cap)

    Aset, Bset = set(A), set(B)
    for c in cells:
        i = idx[c]
        arc(2 * i, 2 * i + 1, INF if (c in Aset or c in Bset) else 1)
        for d in (2, 3):
            nb = c + bd.delta[d]
            if bd.free[nb]:
                j = idx[nb]
                arc(2 * i + 1, 2 * j, INF)
                arc(2 * j + 1, 2 * i, INF)
    for a in A: arc(SRC, 2 * idx[a], INF)
    for b in B: arc(2 * idx[b] + 1, SNK, INF)

    cap = coo_matrix((caps, (rows, cols)), shape=(2 * n + 2, 2 * n + 2)).tocsr()
    cap.sum_duplicates()
    res = maximum_flow(cap, SRC, SNK)
    k = res.flow_value
    if k >= INF: return None, k
    R = (cap - res.flow.tocsr())            # 残量
    Rb = (R > 0).tocsr()
    seen = np.zeros(2 * n + 2, dtype=bool)
    dq = deque([SRC]); seen[SRC] = True
    while dq:
        u = dq.popleft()
        for v in Rb.indices[Rb.indptr[u]:Rb.indptr[u + 1]]:
            if not seen[v]: seen[v] = True; dq.append(v)
    S = [c for c in cells if seen[2 * idx[c]] and not seen[2 * idx[c] + 1]]
    assert len(S) == k, f"割集抽取不一致 {len(S)} != {k}"
    return S, k


def components(bd: Board, S):
    Sset = set(S)
    comp = {}; comps = []
    for c0 in bd.cells:
        if c0 in Sset or c0 in comp: continue
        cur = [c0]; comp[c0] = len(comps); dq = deque([c0])
        while dq:
            c = dq.popleft()
            for d in range(4):
                nb = c + bd.delta[d]
                if bd.free[nb] and nb not in Sset and nb not in comp:
                    comp[nb] = len(comps); cur.append(nb); dq.append(nb)
        comps.append(cur)
    return comp, comps


def g2_enumerate(bd, S, comp, comps, verbose=True):
    """枚举可行的端点放置。返回 feasible 放置集合 {(locA,cA,locB,cB)}，loc = ('C',i)或('S',v)。"""
    m = len(comps)
    k = len(S)
    di = []
    for cells in comps:
        b = sum(1 for c in cells if bd.col[c] == 0)
        di.append(b - (len(cells) - b))
    # 割格 -> 各分量的邻居数(边容量 min(2,·))、割-割相邻对
    vadj = {}
    for v in S:
        cnt = {}
        for d in range(4):
            nb = v + bd.delta[d]
            if bd.free[nb] and nb in comp:
                cnt[comp[nb]] = cnt.get(comp[nb], 0) + 1
        vadj[v] = cnt
    pairs = [(u, v) for i2, u in enumerate(S) for v in S[i2 + 1:]
             if abs(u - v) in (1, bd.W)]
    if len(pairs) > 12:
        print(f"  ⚠ 割-割相邻对过多({len(pairs)})，枚举截断到前 12 对")
        pairs = pairs[:12]
    total_even = (bd.total_free % 2 == 0)

    locs = [('C', i2) for i2 in range(m)] + [('S', v) for v in S]

    def loc_color_ok(loc, c):
        if loc[0] == 'S': return bd.col[loc[1]] == c
        cells = comps[loc[1]]
        b = sum(1 for x in cells if bd.col[x] == 0)
        return (b > 0) if c == 0 else (len(cells) - b > 0)

    feasible = set()
    # 端点放置：偶盘两端异色；奇盘同为多数色（这批盘偶为主，奇分支从简：两端同色=多数色）
    maj = 0
    if not total_even:
        b0 = sum(1 for c in bd.cells if bd.col[c] == 0)
        maj = 0 if b0 > bd.total_free - b0 else 1
    placements = []
    for la, lb in itertools.combinations_with_replacement(locs, 2):
        if total_even:
            for ca in (0, 1):
                cb = ca ^ 1
                if la == lb and la[0] == 'S': continue
                if loc_color_ok(la, ca) and loc_color_ok(lb, cb):
                    placements.append((la, ca, lb, cb))
        else:
            if la == lb and la[0] == 'S': continue
            if loc_color_ok(la, maj) and loc_color_ok(lb, maj):
                placements.append((la, maj, lb, maj))

    for (la, ca, lb, cb) in placements:
        ep_comp = [[0, 0] for _ in range(m)]     # ep_i[color]
        ep_cut = set()
        for loc, c in ((la, ca), (lb, cb)):
            if loc[0] == 'C': ep_comp[loc[1]][c] += 1
            else: ep_cut.add(loc[1])
        degp = {v: (1 if v in ep_cut else 2) for v in S}
        ok_found = False
        for cc_bits in range(1 << len(pairs)):
            cc = [pairs[i2] for i2 in range(len(pairs)) if cc_bits >> i2 & 1]
            use = {v: 0 for v in S}
            bad = False
            for (u, v) in cc:
                use[u] += 1; use[v] += 1
                if use[u] > degp[u] or use[v] > degp[v]: bad = True; break
            if bad: continue
            T = k + 1 - len(ep_cut) - len(cc)
            if T < m: continue                    # 每分量至少 1 段
            # DFS 分配各割格剩余出边到相邻分量，累计 A_i[color]
            vs = list(S)

            def dfs(i2, A):
                nonlocal ok_found
                if ok_found: return
                if i2 == len(vs):
                    ssum = 0
                    for ci in range(m):
                        d0 = A[ci][0] + ep_comp[ci][0]
                        d1 = A[ci][1] + ep_comp[ci][1]
                        if d0 - d1 != 2 * di[ci]: return
                        si = d1 + di[ci]
                        if si < 1: return
                        lo_n = max(0, -di[ci])
                        if 2 * lo_n > d1: return  # ∃n_i 分解失败
                        ssum += si
                    if ssum == T: ok_found = True
                    return
                v = vs[i2]
                r = degp[v] - use[v]
                items = list(vadj[v].items())
                colv = bd.col[v] ^ 1              # v 的出边落点颜色
                # 枚举把 r 条边分到各相邻分量（每分量 ≤ min(2,邻居数)）
                def go(j2, left, A2):
                    if ok_found: return
                    if j2 == len(items):
                        if left == 0: dfs(i2 + 1, A2)
                        return
                    ci, cap2 = items[j2]
                    for t in range(min(left, min(2, cap2)) + 1):
                        A3 = [row[:] for row in A2]
                        A3[ci][colv] += t
                        go(j2 + 1, left - t, A3)
                go(0, r, A)

            dfs(0, [[0, 0] for _ in range(m)])
            if ok_found: break
        if ok_found:
            feasible.add((la, ca, lb, cb))
    return feasible, di


def main():
    board_path = sys.argv[1]
    x0, x1, y0, y1 = map(int, sys.argv[2:6])
    dists = [int(t) for t in (sys.argv[6].split(',') if len(sys.argv) > 6 else ['40', '60', '80', '100'])]
    bd = Board(board_path)
    box = [c for c in bd.cells if x0 <= bd.xy(c)[0] <= x1 and y0 <= bd.xy(c)[1] <= y1]
    print(f"盒区 [{x0},{x1}]x[{y0},{y1}]: {len(box)} 自由格 / 全盘 {bd.total_free}")
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    best = None
    for D in dists:
        far = [c for c in bd.cells
               if max(abs(bd.xy(c)[0] - cx), abs(bd.xy(c)[1] - cy)) > D]
        if not far: continue
        S, k = min_vertex_cut(bd, box, far)
        print(f"距离>{D}: 最小点割 k={k}" + (" (太大,跳过)" if S is None or k > 9 else ""))
        if S is not None and k <= 9 and (best is None or k < best[1]):
            best = (S, k, D)
    if not best:
        print("== 未找到 k<=9 的割集：该簇不是小瓶颈奇偶所致（或需换区域参数）==")
        return
    S, k, D = best
    print(f"\n=== 采用 D={D} 的割集 k={k}: {[bd.xy(v) for v in S]} ===")
    comp, comps = components(bd, S)
    boxcomp = comp[box[len(box) // 2]]
    ledger = []
    for i2, cells in enumerate(comps):
        b = sum(1 for c in cells if bd.col[c] == 0)
        d = b - (len(cells) - b)
        ledger.append(d)
        tag = " <-- 盒区所在" if i2 == boxcomp else ""
        print(f"  分量{i2}: {len(cells)}格 色差 d={d:+d}{tag}")
    g1 = sum(max(abs(d), 1) for d in ledger)
    print(f"G1' 账本: Σmax(|d|,1)={g1}  vs  k+1={k + 1}  => {'满足(必要条件过)' if g1 <= k + 1 else '‼ 违反 => 无解?!'}")
    feas, di = g2_enumerate(bd, S, comp, comps)
    if not feas:
        print("‼ G2: 无任何可行端点放置 —— 盘面无解?!（若为已解关即 bug）")
        return
    incomp = sum(1 for (la, ca, lb, cb) in feas
                 if (la[0] == 'C' and la[1] == boxcomp) or (lb[0] == 'C' and lb[1] == boxcomp))
    print(f"G2: 可行端点放置 {len(feas)} 种，其中含[盒区分量端点]的 {incomp} 种")
    if incomp == len(feas):
        cols_in = set()
        for (la, ca, lb, cb) in feas:
            if la[0] == 'C' and la[1] == boxcomp: cols_in.add(ca)
            if lb[0] == 'C' and lb[1] == boxcomp: cols_in.add(cb)
        print(f"★★ 证书成立：任何解必有一个端点落在盒区分量！允许端点色: {sorted(cols_in)}")
    else:
        others = set()
        for (la, ca, lb, cb) in feas:
            for loc, c in ((la, ca), (lb, cb)):
                others.add(f"{loc}:{c}")
        print(f"证书不成立（存在把两端点都放在别处的可行方案）。可行位置样本: {sorted(others)[:8]}")


if __name__ == "__main__":
    main()
