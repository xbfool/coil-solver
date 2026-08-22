#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""梯子 v3：C 传播 + 割集记账反驳器（开阔盘的无锚规则）

割集记账：对区域 A，B = 2|A| − endp_A − 2·I，其中
  B = 跨界用边数，I = A 内部用边数，endp_A ∈ {0,1,2}（若起点已知在 A/非 A 可收紧为
  {1} ∪ 终点归属——per-start 下起点固定，endp_A = 1[start∈A] + 1[end∈A]，end 未知 ⇒
  endp_A ∈ {s_in, s_in+1}）。
用 estate 的边界 [B_min,B_max]、[I_min,I_max] 检查是否存在可行 (B,I,endp)；
不可行 ⇒ 该假设态被反驳。区域族：所有行前缀、列前缀（2(W+H) 个割）。
接进一级探针：C 的 PROPOK=1 后再过记账器；记账死也算死。
"""
import re
import subprocess
import sys
import os

LV = sys.argv[1] if len(sys.argv) > 1 else "22"

raw = open(f"/mnt/d/workspace/coilbench/levels_all/{LV}").read()
w = int(re.search(r"x=(\d+)", raw).group(1))
h = int(re.search(r"y=(\d+)", raw).group(1))
b = re.search(r"board=([.X]+)", raw).group(1)
W = w + 2
free = set()
for y in range(h):
    for x in range(w):
        if b[y * w + x] == ".":
            free.add((x + 1) + (y + 1) * W)
DELTA = {2: 1, 3: W}
edges = [(c, d) for c in sorted(free) for d in (2, 3) if c + DELTA[d] in free]
print(f"L{LV}: {len(free)} 格 {len(edges)} 边")

SOL = open(f"solutions/{LV}.sol").read()
sx = int(re.search(r"x=(\d+)", SOL).group(1))
sy = int(re.search(r"y=(\d+)", SOL).group(1))
START = (sx + 1) + (sy + 1) * W
spath = re.search(r"path=([UDLR]+)", SOL).group(1)
DD2 = {"L": -1, "U": -W, "R": 1, "D": W}
cur = START
vis = {cur}
used = set()
for ch in spath:
    dd = DD2[ch]
    while cur + dd in free and cur + dd not in vis:
        a2, b2 = cur, cur + dd
        used.add((min(a2, b2), 2 if abs(dd) == 1 else 3))
        cur += dd
        vis.add(cur)
truth = {(c, d): (1 if (c, d) in used else 2) for (c, d) in edges}

# 区域族：列前缀 A = {格: x <= k}、行前缀 A = {格: y <= k}
REGIONS = []
for k in range(1, w):
    REGIONS.append(frozenset(c for c in free if (c % W) <= k))
for k in range(1, h):
    REGIONS.append(frozenset(c for c in free if (c // W) <= k))

def cut_feasible(state):
    """state: {(c,d): 0未定/1必用/2禁用}。逐区域检查记账可行性。"""
    for A in REGIONS:
        nA = len(A)
        if nA == 0 or nA == len(free):
            continue
        Bmin = Bmax = Imin = Imax = 0
        for (c, d) in edges:
            inA = c in A
            inB = (c + DELTA[d]) in A
            v = state.get((c, d), 0)
            if inA and inB:
                if v == 1:
                    Imin += 1; Imax += 1
                elif v == 0:
                    Imax += 1
            elif inA != inB:
                if v == 1:
                    Bmin += 1; Bmax += 1
                elif v == 0:
                    Bmax += 1
        s_in = 1 if START in A else 0
        ok = False
        for endp in (s_in, s_in + 1):
            # B = 2nA - endp - 2I，需存在 I∈[Imin,Imax] 使 B∈[Bmin,Bmax]
            # B 随 I 单调减：I 的可行区间求交
            # B>=Bmin => I <= (2nA-endp-Bmin)/2；B<=Bmax => I >= (2nA-endp-Bmax)/2
            hi = (2 * nA - endp - Bmin) / 2.0
            lo = (2 * nA - endp - Bmax) / 2.0
            import math
            ilo = max(Imin, math.ceil(lo - 1e-9))
            ihi = min(Imax, math.floor(hi + 1e-9))
            if ilo <= ihi:
                ok = True
                break
        if not ok:
            return False
    return True

def run(forced):
    fe = ";".join(f"{c}:{d}:{v}" for ((c, d), v) in forced.items())
    env = {"STRONG": "1", "PROBEROUNDS": "3", "FIXDUMP": "1",
           "XCHAIN": os.environ.get("XCHAIN", "0"), "FORCEEDGES": fe, "PATH": "/usr/bin:/bin"}
    r = subprocess.run(["./bin/v58-probezero", f"/mnt/d/workspace/coilbench/levels_all/{LV}",
                        "--verify", f"solutions/{LV}.sol"],
                       capture_output=True, text=True, timeout=120, env=env,
                       cwd="/mnt/d/workspace/coil-solver")
    if not re.search(r"PROPOK 1", r.stderr):
        return 0
    D = dict(forced)
    for m in re.finditer(r"FIX (\d+) (\d+) (\d+)", r.stderr):
        e = (int(m.group(1)), int(m.group(2)))
        if e in truth:
            D[e] = int(m.group(3))
    return 1 if cut_feasible(D) else 0        # 记账死也算死

F = {}
calls = 0
rounds = 0
while True:
    rounds += 1
    newly = 0
    for e in edges:
        if e in F:
            continue
        for v in (1, 2):
            hyp = dict(F)
            hyp[e] = v
            calls += 1
            if run(hyp) == 0:
                F[e] = 3 - v
                if truth[e] != F[e]:
                    print(f"!!! 不可靠：边 {e} 定 {F[e]} 真值 {truth[e]}")
                    sys.exit(1)
                newly += 1
                break
    print(f"第{rounds}轮：+{newly}，累计 {len(F)}/{len(edges)}（{100.0*len(F)/len(edges):.0f}%），调用 {calls}", flush=True)
    if newly == 0:
        break
print(f"最终确定率 {100.0*len(F)/len(edges):.1f}%，可靠性全程对拍通过")
