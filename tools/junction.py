#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 结构分析：度数分布(每个自由格的自由邻居数) = coil 分叉复杂度指纹
# deg2 = 走廊(被迫直穿/单转) ; deg3/4 = 潜在 junction(分叉点) ; deg1 = 死端(必为端点)
# 用法: junction.py <board1> <board2> ...
import sys, re

def analyze(path):
    t = open(path).read().strip()
    m = re.match(r'x=(\d+)&y=(\d+)&board=(.*)$', t, re.S)
    w, h, b = int(m.group(1)), int(m.group(2)), m.group(3).strip()
    deg = [0, 0, 0, 0, 0]
    total = 0
    for y in range(h):
        row = y * w
        for x in range(w):
            if b[row + x] != '.':
                continue
            total += 1
            d = 0
            if x + 1 < w and b[row + x + 1] == '.': d += 1
            if x - 1 >= 0 and b[row + x - 1] == '.': d += 1
            if y + 1 < h and b[row + w + x] == '.': d += 1
            if y - 1 >= 0 and b[row - w + x] == '.': d += 1
            deg[d] += 1
    return w, h, total, deg

print(f"{'关':>6} {'长×宽':>11} {'自由格':>9} {'deg1死端':>9} {'deg2走廊':>11} {'deg3':>11} {'deg4':>10}")
for lv in sys.argv[1:]:
    name = lv.split('/')[-1]
    w, h, total, deg = analyze(lv)
    p = lambda n: f"{n}({100*n//total}%)"
    print(f"{name:>6} {f'{w}×{h}':>11} {total:>9} {p(deg[1]):>9} {p(deg[2]):>11} {p(deg[3]):>11} {p(deg[4]):>10}")
