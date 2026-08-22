#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""LKH 相位A + 圆弧扫描相位B 一条龙：
1. 关卡 → TSPLIB HCP 编码 → LKH-3 找哈密顿环
2. 找到的环 → 圆弧扫描 → 合法缝集合
3. 对照真解（若有）：环是否为真环；缝集是否含真缝
用法：python3 lkh_pipeline.py <level> [timelimit_sec]"""
import re
import subprocess
import sys
import os

LV = sys.argv[1] if len(sys.argv) > 1 else "414"
TL = int(sys.argv[2]) if len(sys.argv) > 2 else 300

raw = open(f"/mnt/d/workspace/coilbench/levels_all/{LV}").read()
w = int(re.search(r"x=(\d+)", raw).group(1))
h = int(re.search(r"y=(\d+)", raw).group(1))
b = re.search(r"board=([.X]+)", raw).group(1)
free = sorted((x, y) for y in range(h) for x in range(w) if b[y * w + x] == ".")
ci = {c: i + 1 for i, c in enumerate(free)}          # 1-indexed for TSPLIB
n = len(free)
edges = []
for (x, y) in free:
    for dx, dy in ((1, 0), (0, 1)):
        if (x + dx, y + dy) in ci:
            edges.append((ci[(x, y)], ci[(x + dx, y + dy)]))
print(f"L{LV}: n={n} m={len(edges)}", flush=True)

os.makedirs("/tmp/lkh", exist_ok=True)
with open(f"/tmp/lkh/{LV}.hcp", "w") as f:
    f.write(f"NAME : coil{LV}\nTYPE : HCP\nDIMENSION : {n}\n")
    f.write("EDGE_DATA_FORMAT : EDGE_LIST\nEDGE_DATA_SECTION\n")
    for (u, v) in edges:
        f.write(f"{u} {v}\n")
    f.write("-1\nEOF\n")
with open(f"/tmp/lkh/{LV}.par", "w") as f:
    f.write(f"PROBLEM_FILE = /tmp/lkh/{LV}.hcp\n")
    f.write(f"TOUR_FILE = /tmp/lkh/{LV}.tour\n")
    f.write("RUNS = 1\nMAX_TRIALS = 100000\n")
    f.write(f"TIME_LIMIT = {TL}\nSEED = 7\nTRACE_LEVEL = 1\n")

r = subprocess.run(["/tmp/LKH-3.0.13/LKH", f"/tmp/lkh/{LV}.par"],
                   capture_output=True, text=True, timeout=TL + 60)
tail = "\n".join(r.stdout.splitlines()[-6:])
print(tail, flush=True)
if not os.path.exists(f"/tmp/lkh/{LV}.tour"):
    print("LKH 未产出环")
    sys.exit(1)

tour = []
in_sec = False
for line in open(f"/tmp/lkh/{LV}.tour"):
    t = line.strip()
    if t == "TOUR_SECTION":
        in_sec = True
        continue
    if not in_sec:
        continue
    if t in ("-1", "EOF"):
        break
    tour.append(int(t))
assert len(tour) == n, len(tour)
order = [free[t - 1] for t in tour]
# 校验环合法（图相邻）
for i in range(n):
    a, b2 = order[i], order[(i + 1) % n]
    assert abs(a[0] - b2[0]) + abs(a[1] - b2[1]) == 1, f"环断裂于 {i}"
print("LKH 环合法 ✓", flush=True)

# 对照真环（若解存在且相邻成环）
freeset = set(free)
try:
    sol = open(f"solutions/{LV}.sol").read()
    sx = int(re.search(r"x=(\d+)", sol).group(1))
    sy = int(re.search(r"y=(\d+)", sol).group(1))
    path = re.search(r"path=([UDLR]+)", sol).group(1)
    DD = {"L": (-1, 0), "U": (0, -1), "R": (1, 0), "D": (0, 1)}
    cur = (sx, sy)
    vset = {cur}
    tru = set()
    for ch in path:
        d = DD[ch]
        while (cur[0] + d[0], cur[1] + d[1]) in freeset and (cur[0] + d[0], cur[1] + d[1]) not in vset:
            nxt = (cur[0] + d[0], cur[1] + d[1])
            tru.add(frozenset((cur, nxt)))
            cur = nxt
            vset.add(cur)
    if abs(cur[0] - sx) + abs(cur[1] - sy) == 1:
        tru.add(frozenset(((sx, sy), cur)))
        got = set(frozenset((order[i], order[(i + 1) % n])) for i in range(n))
        inter = len(got & tru)
        print(f"与真环边重合 {inter}/{n} —— {'完全一致!' if inter == n else '不同的环'}")
except FileNotFoundError:
    print("（无缓存解，跳过对照）")

# 相位 B：圆弧扫描
def scan(seq):
    m = len(seq)
    p2 = {c: i for i, c in enumerate(seq)}
    cons = []
    for i in range(m):
        prev = seq[(i - 1) % m]
        nxt = seq[(i + 1) % m]
        di = (seq[i][0] - prev[0], seq[i][1] - prev[1])
        do = (nxt[0] - seq[i][0], nxt[1] - seq[i][1])
        if di == do:
            continue
        beyond = (seq[i][0] + di[0], seq[i][1] + di[1])
        if beyond not in freeset:
            continue
        cons.append((i, p2[beyond]))
    valid = []
    for s in range(m):
        ok = True
        for (i, j) in cons:
            if i == s or i == (s - 1) % m:
                continue
            if (i - s) % m < (j - s) % m:
                ok = False
                break
        if ok:
            valid.append(seq[s])
    return valid

fwd = scan(order)
rev = scan(list(reversed(order)))
print(f"弧扫描：正向合法缝 {len(fwd)} 个 {fwd[:6]}；反向 {len(rev)} 个 {rev[:6]}")
