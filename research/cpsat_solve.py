#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CP-SAT 完整编码：AddCircuit（图半边）+ 位置变量 + 滑动蕴含子句（时序半边）

编码：
- 节点 = 自由格 + 1 个虚拟格（环过虚拟格 = 路径，虚拟格的出/入弧即起点/终点）
- AddCircuit 保证单一闭环覆盖全部格
- pos[c] ∈ [0, n-1]：弧 (u→v) 选中 ⇒ pos[v] = pos[u]+1；虚拟出弧 ⇒ pos=0
- 滑动规则（本项目的私货）：进入 c 沿方向 d（弧 prev→c 选中）且不直行（弧 c→b 未选）
  且 b=c+d 是自由格 ⇒ pos[b] < pos[c]（弯要靠山：垫背必须更早）
用法：python3 cpsat_solve.py <level> [timelimit] [workers]
输出：x=..&y=..&path=... 并自检滑动合法性。"""
import re
import sys
from ortools.sat.python import cp_model

LV = sys.argv[1] if len(sys.argv) > 1 else "73"
TL = float(sys.argv[2]) if len(sys.argv) > 2 else 300.0
NW = int(sys.argv[3]) if len(sys.argv) > 3 else 28

raw = open(f"/mnt/d/workspace/coilbench/levels_all/{LV}").read()
w = int(re.search(r"x=(\d+)", raw).group(1))
h = int(re.search(r"y=(\d+)", raw).group(1))
b = re.search(r"board=([.X]+)", raw).group(1)
free = sorted((x, y) for y in range(h) for x in range(w) if b[y * w + x] == ".")
ci = {c: i for i, c in enumerate(free)}
n = len(free)
V = n                                  # 虚拟节点
DD = [(1, 0), (-1, 0), (0, 1), (0, -1)]
print(f"L{LV}: n={n}", flush=True)

m = cp_model.CpModel()
arc = {}
arcs = []
for (x, y) in free:
    u = ci[(x, y)]
    for (dx, dy) in DD:
        vxy = (x + dx, y + dy)
        if vxy in ci:
            v = ci[vxy]
            lit = m.NewBoolVar(f"a{u}_{v}")
            arc[(u, v)] = lit
            arcs.append((u, v, lit))
for c in range(n):
    lit1 = m.NewBoolVar(f"aV_{c}")
    arc[(V, c)] = lit1
    arcs.append((V, c, lit1))
    lit2 = m.NewBoolVar(f"a{c}_V")
    arc[(c, V)] = lit2
    arcs.append((c, V, lit2))
m.AddCircuit(arcs)

pos = [m.NewIntVar(0, n - 1, f"p{c}") for c in range(n)]
for (u, v), lit in arc.items():
    if u == V:
        m.Add(pos[v] == 0).OnlyEnforceIf(lit)
    elif v == V:
        m.Add(pos[u] == n - 1).OnlyEnforceIf(lit)
    else:
        m.Add(pos[v] == pos[u] + 1).OnlyEnforceIf(lit)

# 滑动子句：弯要靠山
nslide = 0
for (x, y) in free:
    c = ci[(x, y)]
    for (dx, dy) in DD:
        prev = (x - dx, y - dy)
        if prev not in ci:
            continue
        in_lit = arc[(ci[prev], c)]
        bxy = (x + dx, y + dy)
        if bxy not in ci:
            continue                    # 垫背是墙/边界：停步自由
        bb = ci[bxy]
        straight = arc[(c, bb)]
        m.Add(pos[bb] < pos[c]).OnlyEnforceIf([in_lit, straight.Not()])
        nslide += 1
print(f"滑动子句 {nslide} 条", flush=True)

# v2：缝合线约束（起终点 L1<=25，19/19 实证）+ 方向对称破除
import os
if os.environ.get("SEAMCON", "1") == "1":
    xs = m.NewIntVar(0, w - 1, "xs"); ys = m.NewIntVar(0, h - 1, "ys")
    xe = m.NewIntVar(0, w - 1, "xe"); ye = m.NewIntVar(0, h - 1, "ye")
    m.Add(xs == sum(free[c][0] * arc[(V, c)] for c in range(n)))
    m.Add(ys == sum(free[c][1] * arc[(V, c)] for c in range(n)))
    m.Add(xe == sum(free[c][0] * arc[(c, V)] for c in range(n)))
    m.Add(ye == sum(free[c][1] * arc[(c, V)] for c in range(n)))
    dx_ = m.NewIntVar(-w, w, "dx"); dy_ = m.NewIntVar(-h, h, "dy")
    m.Add(dx_ == xs - xe); m.Add(dy_ == ys - ye)
    adx = m.NewIntVar(0, w, "adx"); ady = m.NewIntVar(0, h, "ady")
    m.AddAbsEquality(adx, dx_); m.AddAbsEquality(ady, dy_)
    m.Add(adx + ady <= int(os.environ.get("SEAMR2", "25")))
    print("缝合线约束已加", flush=True)

solver = cp_model.CpSolver()
solver.parameters.max_time_in_seconds = TL
solver.parameters.num_workers = NW
solver.parameters.log_search_progress = False
st = solver.Solve(m)
print("status:", solver.StatusName(st), f"wall={solver.WallTime():.1f}s", flush=True)
if st not in (cp_model.OPTIMAL, cp_model.FEASIBLE):
    sys.exit(1)

order = sorted(range(n), key=lambda c: solver.Value(pos[c]))
cellseq = [free[c] for c in order]
# 生成 path 字符串（滑行合并）并自检滑动合法性
sx, sy = cellseq[0]
moves = []
i = 0
vis = set()
ok = True
while i < len(cellseq) - 1:
    (x0, y0), (x1, y1) = cellseq[i], cellseq[i + 1]
    dx, dy = x1 - x0, y1 - y0
    ch = "R" if (dx, dy) == (1, 0) else "L" if (dx, dy) == (-1, 0) else "D" if (dx, dy) == (0, 1) else "U"
    # 沿该方向吃到不能吃
    j = i
    while j + 1 < len(cellseq) and (cellseq[j + 1][0] - cellseq[j][0], cellseq[j + 1][1] - cellseq[j][1]) == (dx, dy):
        j += 1
    for k in range(i, j + 1):
        vis.add(cellseq[k])
    # 滑动自检：停点 cellseq[j] 的前方必须 墙/已访问
    beyond = (cellseq[j][0] + dx, cellseq[j][1] + dy)
    if beyond in ci and beyond not in vis:
        ok = False
    moves.append(ch)
    i = j
print(("SLIDE-VALID " if ok else "SLIDE-INVALID! ") + f"x={sx}&y={sy}&path=" + "".join(moves))
