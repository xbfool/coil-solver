#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""批量弧扫描：/tmp/lkh/s*/L.tour 全部过一遍，报告每个环的合法缝数"""
import re
import sys
import glob

LV = sys.argv[1] if len(sys.argv) > 1 else "414"
raw = open(f"/mnt/d/workspace/coilbench/levels_all/{LV}").read()
w = int(re.search(r"x=(\d+)", raw).group(1))
h = int(re.search(r"y=(\d+)", raw).group(1))
b = re.search(r"board=([.X]+)", raw).group(1)
free = sorted((x, y) for y in range(h) for x in range(w) if b[y * w + x] == ".")
freeset = set(free)
n = len(free)

def scan(seq):
    m = len(seq)
    cons = []
    p2 = {c: i for i, c in enumerate(seq)}
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

for tf in sorted(glob.glob(f"/tmp/lkh/s*/{LV}.tour")):
    tour = []
    in_sec = False
    for line in open(tf):
        t = line.strip()
        if t == "TOUR_SECTION":
            in_sec = True
            continue
        if not in_sec:
            continue
        if t in ("-1", "EOF"):
            break
        tour.append(int(t))
    if len(tour) != n:
        print(f"{tf}: 无效（{len(tour)}）")
        continue
    order = [free[t - 1] for t in tour]
    ok = all(abs(order[i][0] - order[(i + 1) % n][0]) + abs(order[i][1] - order[(i + 1) % n][1]) == 1 for i in range(n))
    if not ok:
        print(f"{tf}: 环断裂")
        continue
    fwd = scan(order)
    rev = scan(list(reversed(order)))
    hit = "  <<< 有缝！可解" if (fwd or rev) else ""
    print(f"{tf}: 正向缝 {len(fwd)} 反向缝 {len(rev)}{hit}")
    if fwd or rev:
        print("   缝位:", (fwd or rev)[:6])
