#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""圆弧扫描验证（L414，严格环实例）

输入：已知解（起终点相邻 ⇒ 解+闭合边 = 图哈密顿环）。
只用环的几何：对每个「软垫背转弯」(i 需 j 更早) 标记禁止弧 [j+1..i]；
两个方向各扫；被弧覆盖但覆盖弧的转弯格恰为断点两端者豁免（断环处转弯被切除）。
输出：合法缝位置集合——预言：极小，且含真实断点。
"""
import re
import sys

LV = sys.argv[1] if len(sys.argv) > 1 else "414"
DD = {"L": (-1, 0), "U": (0, -1), "R": (1, 0), "D": (0, 1)}

raw = open(f"/mnt/d/workspace/coilbench/levels_all/{LV}").read()
w = int(re.search(r"x=(\d+)", raw).group(1))
h = int(re.search(r"y=(\d+)", raw).group(1))
b = re.search(r"board=([.X]+)", raw).group(1)
free = set((x, y) for y in range(h) for x in range(w) if b[y * w + x] == ".")

sol = open(f"solutions/{LV}.sol").read()
sx = int(re.search(r"x=(\d+)", sol).group(1))
sy = int(re.search(r"y=(\d+)", sol).group(1))
path = re.search(r"path=([UDLR]+)", sol).group(1)
cur = (sx, sy)
order = [cur]
vset = {cur}
for ch in path:
    d = DD[ch]
    while (cur[0] + d[0], cur[1] + d[1]) in free and (cur[0] + d[0], cur[1] + d[1]) not in vset:
        cur = (cur[0] + d[0], cur[1] + d[1])
        order.append(cur)
        vset.add(cur)
end = cur
assert abs(end[0] - sx) + abs(end[1] - sy) == 1, "非严格环实例"
n = len(order)
pos = {c: i for i, c in enumerate(order)}
print(f"L{LV}: 环长 {n}（真实断点在 位置0={order[0]} 与 位置{n-1}={order[-1]} 之间）")

def scan(seq):
    """seq = 环的一个方向。返回 valid seam 位置集合。
    seam=s 表示：断开 seq[s-1]—seq[s] 之间的环边，从 seq[s] 出发走到 seq[s-1]。"""
    constraints = []          # (i, j): 转弯格 seq[i] 需要 seq[j] 更早
    m = len(seq)
    for i in range(m):
        prev = seq[(i - 1) % m]
        nxt = seq[(i + 1) % m]
        di = (seq[i][0] - prev[0], seq[i][1] - prev[1])
        do = (nxt[0] - seq[i][0], nxt[1] - seq[i][1])
        if di == do:
            continue                                  # 直穿
        beyond = (seq[i][0] + di[0], seq[i][1] + di[1])
        if beyond not in free:
            continue                                  # 墙垫背，无约束
        j = pos2[beyond]
        constraints.append((i, j))
    print(f"  方向扫描：转弯软垫背约束 {len(constraints)} 条")
    forb = [0] * m
    arcs = {}
    for (i, j) in constraints:
        arcs.setdefault(i, []).append(j)
    valid = []
    for s in range(m):
        ok = True
        for (i, j) in constraints:
            if i == s or i == (s - 1) % m:
                continue                              # 断点两端的转弯被切除
            # s 违反 (i,j) ⟺ 从 s 走到 i 比走到 j 近 ⟺ (i-s)%m < (j-s)%m
            if (i - s) % m < (j - s) % m:
                ok = False
                break
        if ok:
            valid.append(s)
    return valid

pos2 = pos
fwd = scan(order)
rev_order = list(reversed(order))
pos2 = {c: i for i, c in enumerate(rev_order)}
rev = scan(rev_order)
print(f"正向合法缝：{len(fwd)} 个 -> {[order[s] for s in fwd[:8]]}")
print(f"反向合法缝：{len(rev)} 个 -> {[rev_order[s] for s in rev[:8]]}")
print(f"真实断点：正向 s=0（起点 {order[0]}）应在正向集合中：{'YES' if 0 in fwd else 'NO?!'}")
