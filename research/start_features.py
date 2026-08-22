#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""可解起点的特征挖掘（对 research/unistart_2_40.log 的全量标注）

问题：什么便宜的局部/半局部特征能区分「可解起点」和普通格子？
候选特征（全部 O(1)~O(N) 可算，能直接进求解器的 sc 排序）：
  f1 颜色（含奇偶约束：奇数自由格时端点色被定死——作为基线校准）
  f2 自由度（deg）
  f3 距最近墙/边界的 L1 距离
  f4 3x3 邻域墙数
  f5 「墙贴脸数」：四个方向中撞墙(相邻即墙)的方向数
  f6 首滑可停方向数：从该格出发四个方向中，滑行终点存在（撞墙停下）的方向数
     （起点第一步必须撞墙——终点是撞已访问不可能，因为没有已访问）
     注：滑行必然停（边界兜底），真正要挖的是首滑长度等
  f7 最短首滑长度 / 最长首滑长度
对每个特征输出：可解起点上的分布 vs 全体自由格分布，以及一个简单组合评分的
排序命中率（可解起点在排序前 5%/10%/20% 的比例）。
"""
import re
from collections import Counter

def load(L):
    raw = open(f"/mnt/d/workspace/coilbench/levels_all/{L}").read()
    w = int(re.search(r"x=(\d+)", raw).group(1))
    h = int(re.search(r"y=(\d+)", raw).group(1))
    b = re.search(r"board=([.X]+)", raw).group(1)
    free = set((x, y) for y in range(h) for x in range(w) if b[y * w + x] == ".")
    return w, h, free

def feats(w, h, free, x, y):
    def isfree(a, b2):
        return (a, b2) in free
    deg = sum(1 for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)) if isfree(x + dx, y + dy))
    wall_face = 4 - deg
    n8 = sum(1 for dx in (-1, 0, 1) for dy in (-1, 0, 1)
             if (dx or dy) and not isfree(x + dx, y + dy))
    # 距最近非自由格（墙或边界外）的 L1 距离：0=贴墙
    d = 0
    while d < max(w, h):
        ring = False
        for dx in range(-d, d + 1):
            dy = d - abs(dx)
            for sy in ({dy, -dy}):
                if not isfree(x + dx, y + sy):
                    ring = True
        if ring:
            break
        d += 1
    # 首滑长度（四方向）
    lens = []
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        if not isfree(x + dx, y + dy):
            continue
        q, r = x, y
        L2 = 0
        while isfree(q + dx, r + dy):
            q += dx; r += dy; L2 += 1
        lens.append(L2)
    col = (x + y) & 1
    return dict(deg=deg, wallface=wall_face, n8=n8, dwall=d,
                minslide=min(lens) if lens else 0, maxslide=max(lens) if lens else 0,
                col=col)

def main():
    rows = []
    for line in open("research/unistart_2_40.log"):
        m = re.match(r"L(\d+) (\d+)x(\d+) solvable-starts=(\d+):(.*)", line)
        if not m:
            continue
        L = int(m.group(1)); starts = []
        for tok in m.group(5).split():
            sx, sy = tok.split(",")
            starts.append((int(sx), int(sy)))
        rows.append((L, starts))
    pos_stats = Counter(); neg_stats = Counter()
    pos_all = {k: [] for k in ("deg", "wallface", "n8", "dwall", "minslide", "maxslide")}
    neg_all = {k: [] for k in pos_all}
    hit5 = hit10 = hit20 = tot = 0
    for (L, starts) in rows:
        w, h, free = load(L)
        sset = set(starts)
        scored = []
        for (x, y) in free:
            f = feats(w, h, free, x, y)
            is_start = (x, y) in sset
            tgt = pos_all if is_start else neg_all
            for k in pos_all:
                tgt[k].append(f[k])
            # 简单评分假设：可解起点偏好 贴墙(dwall小)、墙贴脸多、短首滑
            score = -f["dwall"] * 2 + f["wallface"] * 2 - f["minslide"] * 0.5 + f["n8"] * 0.3
            scored.append((score, (x, y), is_start))
        scored.sort(reverse=True)
        n = len(scored)
        for rank, (_, cell, is_start) in enumerate(scored):
            if is_start:
                tot += 1
                if rank < n * 0.05: hit5 += 1
                if rank < n * 0.10: hit10 += 1
                if rank < n * 0.20: hit20 += 1
    avg = lambda a: sum(a) / len(a) if a else 0
    print("特征          可解起点均值   普通格均值")
    for k in pos_all:
        print(f"  {k:9s}  {avg(pos_all[k]):8.2f}   {avg(neg_all[k]):8.2f}")
    print(f"简单评分排序命中：前5% {100*hit5/tot:.0f}%  前10% {100*hit10/tot:.0f}%  前20% {100*hit20/tot:.0f}%  (n={tot})")

if __name__ == "__main__":
    main()
