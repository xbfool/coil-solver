#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
营救区域演算（rescue region calculus）—— 针尖定位器原型（2026-08-26）

理论：U 形死结模体的 5 条边若被 flow 全部强制，链定向双杀该起点。
  ⇒ 幸存起点必须"营救"至少一条模体边（使其不被 Régin 强制）。
  ⇒ Régin-强制(e|s) ⟺ s-模型删 e 后不可行。
关键化简（供应过剩 1 单位的对偶）：
  色 X 起点的模型 = SRC→每个X格 cap2（起点本应 cap1）+ 哑点(¬X侧) cap1 + ¬X格→SNK cap2。
  供应 2|X|+1 > 需求 2|X|。"起点 s 处减 1 供应仍饱和" ⟺ 任取删 e 后的最大流 F：
     flow(SRC→s) ≤ 1，或残量图中 SRC ⇝ s 可达（把富余单位换给 s）。
  ⇒ **每条模体边一次 max-flow + 一次残量 BFS，即得全体该色起点的营救集**。
注意：此处不含逐起点基础传播（首滑强制等），故营救集是真幸存集的**超集** —— 定位器的
安全方向（只多不漏）。多模体时对各自营救集**求交**。

用法：rescue.py <board> <oklist(87候选,用来对账)> <模体边列表 x,y,d ...>
"""
import sys, re
from collections import deque
import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import maximum_flow
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from referee import Board


def rescue_sets(bd: Board, motif_edges):
    """返回 {起点cell} 的并集：对至少一条模体边 e，s-模型删 e 仍可行。"""
    rescued = set()
    for X in (0, 1):                      # 起点颜色两类
        cells = bd.cells
        idx = {c: i + 3 for i, c in enumerate(cells)}   # 0=SRC 1=SNK 2=DUM
        nn = len(cells) + 3
        base = []                          # (u,v,cap)
        for c in cells:
            if bd.col[c] == X: base.append((0, idx[c], 2))
            else: base.append((idx[c], 1, 2))
        base.append((0, 2, 1))             # 哑点：¬X 侧终点自由度
        for c in cells:
            if bd.col[c] != X: base.append((2, idx[c], 1))
        grid = {}                          # (c,d规范) -> (u,v)
        for c in cells:
            if bd.col[c] != X: continue
            for d in range(4):
                n = c + bd.delta[d]
                if not bd.free[n]: continue
                cd = (c, d) if d in (2, 3) else (n, d ^ 2)
                grid[cd] = (idx[c], idx[n])
                base.append((idx[c], idx[n], 1))
        target = 2 * sum(1 for c in cells if bd.col[c] == X)

        for (ex, ey, ed) in motif_edges:
            e_cd = (bd.cell(ex, ey), ed)
            if e_cd not in grid:           # 该色模型里此边方向不存在（属于另一色）
                continue
            eu, ev = grid[e_cd]
            rows, cols, caps = [], [], []
            for (u, v, cap) in base:
                if (u, v) == (eu, ev): continue          # 删掉模体边
                rows.append(u); cols.append(v); caps.append(cap)
            capm = coo_matrix((caps, (rows, cols)), shape=(nn, nn)).tocsr()
            capm.sum_duplicates()
            res = maximum_flow(capm, 0, 1)
            if res.flow_value < target:
                print(f"  边({ex},{ey})d{ed} × 色{X}: 删边即不可行 —— 该色起点无人能营救此边")
                continue
            F = res.flow.tocsr()
            R = (capm - F)
            Rb = (R > 0).tocsr()
            seen = np.zeros(nn, dtype=bool)
            dq = deque([0]); seen[0] = True
            while dq:
                u = dq.popleft()
                for v in Rb.indices[Rb.indptr[u]:Rb.indptr[u + 1]]:
                    if not seen[v]: seen[v] = True; dq.append(v)
            Fd = F.todok()
            got = 0
            for c in cells:
                if bd.col[c] != X: continue
                if Fd.get((0, idx[c]), 0) <= 1 or seen[idx[c]]:
                    rescued.add(c); got += 1
            print(f"  边({ex},{ey})d{ed} × 色{X}: 营救集 {got} 格")
    return rescued


def main():
    bd = Board(sys.argv[1])
    ok = set()
    for line in open(sys.argv[2]):
        mm = re.match(r"OK (\d+)", line)
        if mm: ok.add(int(mm.group(1)))
    motif = [tuple(map(int, t.split(','))) for t in sys.argv[3:]]
    print(f"模体边 {len(motif)} 条，87 候选对账集 {len(ok)} 个")
    R = rescue_sets(bd, motif)
    print(f"\n营救并集: {len(R)} / {bd.total_free} 格 ({100.0*len(R)/bd.total_free:.2f}%)")
    if R:
        xs = sorted(bd.xy(c)[0] for c in R); ys = sorted(bd.xy(c)[1] for c in R)
        print(f"bbox: x∈[{xs[0]},{xs[-1]}] y∈[{ys[0]},{ys[-1]}]")
    missed = ok - R
    print(f"87 候选 ⊆ 营救并集？ {'是 ✓（超集方向正确）' if not missed else f'否!! 漏 {len(missed)} 个: ' + str([bd.xy(c) for c in sorted(missed)][:5])}")
    extra = R - ok
    print(f"营救并集里非候选(被其他规则/模体杀掉的): {len(extra)} 个")


if __name__ == "__main__":
    main()
