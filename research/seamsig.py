#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import re
from collections import Counter

rows = []
for line in open("/tmp/seamscore.err"):
    m = re.match(r"T3 shard(\d+) r0 #(\d+) cell=\((\d+),(\d+)\) sc=(\d+) nodes=(\d+) r=(-?\d+)", line)
    if m:
        sh, pos, x, y, sc, nodes, r = (int(v) for v in m.groups())
        rows.append(((x, y), nodes, r))

rc = Counter(r for (_, _, r) in rows)
print("r 分布:", dict(rc), " 总树", len(rows))
caps = [c for (c, n, r) in rows if r == -1]
print(f"撞帽树 {len(caps)} 棵（{100.0 * len(caps) / len(rows):.1f}%）")
for seam in [(112, 115), (112, 116), (113, 116)]:
    hit = [(c, n, r) for (c, n, r) in rows if c == seam]
    print(f"真缝 {seam}:", hit if hit else "not-in-completed")
# 若撞帽树是少数，它们就是「缝约束下活得下来」的候选集——统计真缝是否在其中
