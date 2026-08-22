#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""WLOG 挖矿实验台（2026-08-23 凌晨）

三件事：
1. 小关全解穷举（免费 ground truth：解的全体，不止一个）
2. 属性表：候选 WLOG 引理（「若有解，必有一个解满足 P」）在全解集上的检验
   —— P 是合格 WLOG ⟺ 每个有解关卡至少一个解满足 P
3. 可确定性上限地图：每格的局部形态（边对）在全部解中是否一致
   —— 一致格比例 = 静态规则理论上限（Tron 的 99.7% 对标）
"""
import re
import sys
from collections import Counter

DD = {"R": (1, 0), "L": (-1, 0), "D": (0, 1), "U": (0, -1)}
DIRS = [(1, 0), (-1, 0), (0, 1), (0, -1)]

def load(L):
    raw = open(f"/mnt/d/workspace/coilbench/levels_all/{L}").read()
    w = int(re.search(r"x=(\d+)", raw).group(1))
    h = int(re.search(r"y=(\d+)", raw).group(1))
    b = re.search(r"board=([.X]+)", raw).group(1)
    free = frozenset((x, y) for y in range(h) for x in range(w) if b[y * w + x] == ".")
    return w, h, free

def enumerate_solutions(free, cap=20000):
    """穷举全部 (start, moves) 解。返回解列表，每解 = (start, [(dir,len,stop,backing)...])"""
    sols = []
    n = len(free)
    for start in sorted(free):
        vis = {start}
        moves = []
        def dfs(p):
            if len(sols) >= cap:
                return
            if len(vis) == n:
                sols.append((start, list(moves)))
                return
            for d in DIRS:
                q = p
                ln = 0
                while (q[0] + d[0], q[1] + d[1]) in free and (q[0] + d[0], q[1] + d[1]) not in vis:
                    q = (q[0] + d[0], q[1] + d[1])
                    ln += 1
                if ln == 0:
                    continue
                cells = []
                c = p
                for _ in range(ln):
                    c = (c[0] + d[0], c[1] + d[1])
                    cells.append(c)
                    vis.add(c)
                beyond = (q[0] + d[0], q[1] + d[1])
                backing = "W" if beyond not in free else ("V" if beyond in vis else "?")
                moves.append((d, ln, q, backing))
                dfs(q)
                moves.pop()
                for c in cells:
                    vis.discard(c)
        dfs(start)
        if len(sols) >= cap:
            break
    return sols

def props(free, w, h, sol):
    """一个解的属性向量"""
    start, moves = sol
    out = {}
    d0, l0, q0, b0 = moves[0]
    # 首滑可选集
    lens = {}
    for d in DIRS:
        q = start
        ln = 0
        while (q[0] + d[0], q[1] + d[1]) in free:
            q = (q[0] + d[0], q[1] + d[1])
            ln += 1
        if ln:
            lens[d] = ln
    out["P2_first_longest"] = l0 == max(lens.values())
    out["P3_first_shortest"] = l0 == min(lens.values())
    out["P5_first3_wall"] = all(m[3] == "W" for m in moves[:3])
    out["P6_first5_wall"] = all(m[3] == "W" for m in moves[:5])
    out["P7_last_len1"] = moves[-1][1] == 1
    end = moves[-1][2]
    out["P9_seam8"] = abs(end[0] - start[0]) + abs(end[1] - start[1]) <= 8
    out["P10_last_wall"] = moves[-1][3] == "W"
    out["P11_first_len1"] = l0 == 1
    return out

def cell_config(free, sol):
    """每格的边对形态（无序对，用相邻格集合表示）"""
    start, moves = sol
    order = [start]
    cur = start
    for (d, ln, q, b) in moves:
        for _ in range(ln):
            cur = (cur[0] + d[0], cur[1] + d[1])
            order.append(cur)
    cfg = {}
    for i, c in enumerate(order):
        nb = []
        if i > 0:
            nb.append(order[i - 1])
        if i < len(order) - 1:
            nb.append(order[i + 1])
        cfg[c] = frozenset(nb)
    return cfg

def main():
    levels = [int(v) for v in sys.argv[1:]] or [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13]
    agg = Counter()
    nlv = 0
    for L in levels:
        w, h, free = load(L)
        sols = enumerate_solutions(free)
        if not sols:
            print(f"L{L}: 无解?!")
            continue
        nlv += 1
        pv = [props(free, w, h, s) for s in sols]
        keys = pv[0].keys()
        # WLOG 合格 = 至少一个解满足；强度 = 满足解比例
        line = [f"L{L}({len(sols)}解)"]
        for k in keys:
            some = any(p[k] for p in pv)
            frac = sum(1 for p in pv if p[k]) / len(pv)
            if some:
                agg[k] += 1
            line.append(f"{k.split('_')[0]}:{'Y' if some else 'N'}({frac:.0%})")
        print(" ".join(line))
        # 可确定性上限
        cfgs = [cell_config(free, s) for s in sols]
        same = sum(1 for c in free if len(set(cf[c] for cf in cfgs)) == 1)
        print(f"    可确定性上限：{same}/{len(free)} 格（{100.0*same/len(free):.0f}%）在全部解中形态一致")
    print("\nWLOG 合格榜（在几关成立/共几关）：")
    for k, v in sorted(agg.items(), key=lambda kv: -kv[1]):
        print(f"  {k}: {v}/{nlv}")

if __name__ == "__main__":
    main()
