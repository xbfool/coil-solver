#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""缝约束打分分析：解析 TREELOG（SEAMR 强制 + SWEEP 小预算），
按 bestrem 升序全局排名，报告已知真缝格的名次 vs sc 基线名次。
用法：python3 seamscore_analyze.py /tmp/seamscore.err 112,115 112,116 113,116"""
import re
import sys

def main():
    path = sys.argv[1]
    seams = set()
    for tok in sys.argv[2:]:
        x, y = tok.split(",")
        seams.add((int(x), int(y)))
    rows = []
    for line in open(path):
        m = re.match(r"T3 shard(\d+) r(\d+) #(\d+) cell=\((\d+),(\d+)\) sc=(\d+) nodes=(\d+) r=(-?\d+) bestrem=(\d+)", line)
        if not m:
            m2 = re.match(r"T3 shard(\d+) r(\d+) #(\d+) cell=\((\d+),(\d+)\) sc=(\d+) nodes=(\d+) r=(-?\d+)", line)
            if not m2:
                continue
            sh, rnd, pos, x, y, sc, nodes, r = (int(v) for v in m2.groups())
            rows.append(((x, y), sh, pos, sc, nodes, r, None))
            continue
        sh, rnd, pos, x, y, sc, nodes, r, br = (int(v) for v in m.groups())
        if rnd != 0:
            continue
        rows.append(((x, y), sh, pos, sc, nodes, r, br))
    if not rows:
        print("无 T3 数据")
        return
    have_br = all(r[6] is not None for r in rows)
    key = (lambda r: r[6]) if have_br else (lambda r: r[3])
    ranked = sorted(rows, key=key)
    print(f"树数 {len(rows)}，排序键 = {'bestrem' if have_br else 'sc'}")
    for i, r in enumerate(ranked):
        if r[0] in seams:
            print(f"真缝 {r[0]}: 全局第 {i+1}/{len(rows)} 名（{100.0*(i+1)/len(rows):.1f}%），"
                  f"sc={r[3]} bestrem={r[6]} 分片内 #{r[2]}")
    scs = sorted(rows, key=lambda r: r[3])
    for i, r in enumerate(scs):
        if r[0] in seams:
            print(f"  （sc 基线名次：{r[0]} 第 {i+1} 名）")

if __name__ == "__main__":
    main()
