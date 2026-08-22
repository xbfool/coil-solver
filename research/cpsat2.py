#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CP-SAT v2：二进制加法器编码（CP2020 胜者编码移植）
把朴素版的 n 个大域 IntVar 位置变量换成 B=ceil(log2 n) 个布尔位/格：
- AddCircuit 照旧（连通/度传播）
- succ(u) = bits(u)+1 由涟漪进位加法器定义（每格一份，弧间共享）
- 弧 (u,v) 选中 => bits(v) == succ(u)（每位 2 子句）
- 滑动子句 => 一侧 MSB 比较器 bits(垫背) < bits(转弯格)（等位链按无序对共享）
- 虚拟弧锚定 bits=0 / n-1；缝合线约束（起终点 L1<=SEAMR2）可选保留
用法：python3 cpsat2.py <level> [timelimit] [workers]"""
import re
import sys
import os
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
V = n
B = max(2, n.bit_length())            # bits(u)+1 最大 = n，n < 2^B 保证不溢出
DD = [(1, 0), (-1, 0), (0, 1), (0, -1)]
print(f"L{LV}: n={n} B={B}", flush=True)

m = cp_model.CpModel()
ncl = 0
def clause(*lits):
    global ncl
    m.AddBoolOr(list(lits))
    ncl += 1

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
    l1 = m.NewBoolVar(f"aV{c}"); arc[(V, c)] = l1; arcs.append((V, c, l1))
    l2 = m.NewBoolVar(f"a{c}V"); arc[(c, V)] = l2; arcs.append((c, V, l2))
m.AddCircuit(arcs)

# 位置的二进制位 + 每格一份加法器 succ = bits+1
bits = [[m.NewBoolVar(f"b{c}_{k}") for k in range(B)] for c in range(n)]
succ = [[m.NewBoolVar(f"s{c}_{k}") for k in range(B)] for c in range(n)]
for c in range(n):
    bc, sc = bits[c], succ[c]
    clause(sc[0], bc[0]); clause(sc[0].Not(), bc[0].Not())     # s0 <=> !b0
    car = bc[0]                                                # carry1 = b0
    for k in range(1, B):
        # s_k <=> b_k XOR car
        clause(sc[k].Not(), bc[k], car); clause(sc[k].Not(), bc[k].Not(), car.Not())
        clause(sc[k], bc[k].Not(), car); clause(sc[k], bc[k], car.Not())
        if k < B - 1:                                          # carry_{k+1} <=> b_k AND car
            nc = m.NewBoolVar(f"c{c}_{k}")
            clause(nc.Not(), bc[k]); clause(nc.Not(), car); clause(nc, bc[k].Not(), car.Not())
            car = nc

# 弧链接：真弧 (u,v) => bits(v)==succ(u)；虚拟弧锚定
end_bits = [(n - 1) >> k & 1 for k in range(B)]
for (u, v), lit in arc.items():
    if u == V:
        for k in range(B):
            clause(lit.Not(), bits[v][k].Not())
    elif v == V:
        for k in range(B):
            clause(lit.Not(), bits[u][k] if end_bits[k] else bits[u][k].Not())
    else:
        for k in range(B):
            clause(lit.Not(), succ[u][k].Not(), bits[v][k])
            clause(lit.Not(), succ[u][k], bits[v][k].Not())

# 等位链缓存（无序对共享）：E[k] => 两格第 j>k 位全等
eqcache = {}
def eqchain(p, q):
    key = (p, q) if p < q else (q, p)
    if key in eqcache:
        return eqcache[key]
    bp, bq = bits[key[0]], bits[key[1]]
    E = [None] * B                     # E[k] 对 k=0..B-2 有效
    up = None
    for k in range(B - 2, -1, -1):
        e = m.NewBoolVar(f"e{key[0]}_{key[1]}_{k}")
        clause(e.Not(), bp[k + 1].Not(), bq[k + 1])
        clause(e.Not(), bp[k + 1], bq[k + 1].Not())
        if up is not None:
            clause(e.Not(), up)
        E[k] = e; up = e
    # 注意：上面循环从高位往低位建，up 链方向要求 E[k] => E[k+1]
    eqcache[key] = E
    return E

# 滑动子句：进 c 沿 d 且不直行 且垫背 bb 自由 => bits(bb) < bits(c)
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
            continue
        bb = ci[bxy]
        straight = arc[(c, bb)]
        E = eqchain(bb, c)
        ss = []
        for k in range(B):
            s = m.NewBoolVar(f"lt{bb}_{c}_{k}")
            clause(s.Not(), bits[bb][k].Not())
            clause(s.Not(), bits[c][k])
            if k < B - 1:
                clause(s.Not(), E[k])
            ss.append(s)
        clause(in_lit.Not(), straight, *ss)
        nslide += 1
print(f"滑动比较器 {nslide} 个，总子句 {ncl}", flush=True)

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
st = solver.Solve(m)
print("status:", solver.StatusName(st), f"wall={solver.WallTime():.1f}s", flush=True)
if st not in (cp_model.OPTIMAL, cp_model.FEASIBLE):
    sys.exit(1)

posval = [sum(solver.Value(bits[c][k]) << k for k in range(B)) for c in range(n)]
order = sorted(range(n), key=lambda c: posval[c])
cellseq = [free[c] for c in order]
sx, sy = cellseq[0]
moves = []
i = 0
vis = set()
ok = True
while i < len(cellseq) - 1:
    (x0, y0), (x1, y1) = cellseq[i], cellseq[i + 1]
    dx, dy = x1 - x0, y1 - y0
    ch = "R" if (dx, dy) == (1, 0) else "L" if (dx, dy) == (-1, 0) else "D" if (dx, dy) == (0, 1) else "U"
    j = i
    while j + 1 < len(cellseq) and (cellseq[j + 1][0] - cellseq[j][0], cellseq[j + 1][1] - cellseq[j][1]) == (dx, dy):
        j += 1
    for k in range(i, j + 1):
        vis.add(cellseq[k])
    beyond = (cellseq[j][0] + dx, cellseq[j][1] + dy)
    if beyond in ci and beyond not in vis:
        ok = False
    moves.append(ch)
    i = j
print(("SLIDE-VALID " if ok else "SLIDE-INVALID! ") + f"x={sx}&y={sy}&path=" + "".join(moves))
