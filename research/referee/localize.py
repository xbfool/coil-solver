#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
针尖定位器（localize）—— 营救区域演算的自动化管线（2026-08-26）

流程：撒样本起点 → 裁判验尸 → 收集杀手链（双杀模体）→ 逐模体算营救区域 → 求交 →
     （有真解时）真起点必须在区域里 = 地面真值对账。

可靠性语义：区域 = 真候选集的**超集**（R3：裸流营救 ⊇ 加紧后营救；模体发现是抽样的，
漏掉模体只会让区域偏大，不会漏真起点）。

用法：localize.py <board> [解文件(对账用)] [样本网格边=4]
"""
import sys, itertools
from collections import deque
import numpy as np
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from referee import Board, Propagator, sol_edges
from rescue import rescue_sets


def nearest_free(bd, x, y):
    best, bd2 = None, 10 ** 9
    for c in bd.cells:
        cx, cy = bd.xy(c)
        d = abs(cx - x) + abs(cy - y)
        if d < bd2: bd2, best = d, c
    return best


def motif_edges_of(bd, seq):
    out = []
    for a, b in zip(seq, seq[1:]):
        d = {1: 2, -1: 2, bd.W: 3, -bd.W: 3}[b - a]
        c = a if b - a > 0 else b
        x, y = bd.xy(c)
        out.append((x, y, d))
    return out


def main():
    bd = Board(sys.argv[1])
    solp = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] != '-' else None
    grid = int(sys.argv[3]) if len(sys.argv) > 3 else 4
    W2, H2 = bd.W - 2, bd.H - 2
    motifs = {}
    other_kills = alive = 0
    for i, j in itertools.product(range(grid), range(grid)):
        x = int(W2 * (i + 0.5) / grid); y = int(H2 * (j + 0.5) / grid)
        s = nearest_free(bd, x, y)
        p = Propagator(bd, s)
        ok, reason = p.run()
        if ok: alive += 1; continue
        seq = getattr(p, 'kill_chain', None)
        if seq is None:
            other_kills += 1
            continue
        motifs[frozenset(seq)] = list(seq)
    print(f"样本 {grid * grid}: 存活 {alive}, 非模体死 {other_kills}, 去重模体 {len(motifs)} 个")
    if not motifs:
        print("== 无双杀模体：该关不适用定位器（或需加密采样）=="); return
    region = None
    for k, seq in enumerate(motifs.values()):
        me = motif_edges_of(bd, seq)
        x0, y0 = bd.xy(seq[0]); x1, y1 = bd.xy(seq[-1])
        print(f"模体{k}: 长{len(seq)} ({x0},{y0})->({x1},{y1})")
        R = rescue_sets(bd, me)
        R |= set(seq)                       # 保守：模体自身格计入（链端起点的 strict 边界情形）
        region = R if region is None else (region & R)
        print(f"  该模体营救集 {len(R)}，累计交集 {len(region)}")
    xs = sorted(bd.xy(c)[0] for c in region); ys = sorted(bd.xy(c)[1] for c in region)
    print(f"\n定位结果: {len(region)} / {bd.total_free} 格 ({100.0*len(region)/bd.total_free:.2f}%)"
          f"  bbox x∈[{xs[0]},{xs[-1]}] y∈[{ys[0]},{ys[-1]}]")
    if solp:
        s, _, _ = sol_edges(bd, solp)
        inr = s in region
        print(f"真起点 {bd.xy(s)} ∈ 区域？ {'✓ PASS' if inr else '‼ FAIL —— 定位器不可靠!'}")
        sys.exit(0 if inr else 2)


if __name__ == "__main__":
    main()
