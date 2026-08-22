#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""L101 差集分析：真可解集 vs 探针幸存集，找「假幸存者」的画像

输入：/tmp/l101census.log（x,y SOLVABLE|no）+ /tmp/l101probe.log（x,y PSURV|pdead）
输出：
  1. 盘面 ASCII 图：#墙 .普通 S=真可解 F=假幸存(探针过但不可解) k=探针即死
  2. 计数与几何统计
注意：census 的 no 含 15s 超时的未知样本（下界真值：SOLVABLE 标签是铁的）。
"""
import re
import sys

W = H = 36

def load_board():
    raw = open("/mnt/d/workspace/coilbench/levels_all/101").read()
    b = re.search(r"board=([.X]+)", raw).group(1)
    return b

def load_log(path, yes_word):
    yes, no = set(), set()
    for line in open(path):
        m = re.match(r"(\d+),(\d+) (\S+)", line)
        if not m:
            continue
        cell = (int(m.group(1)), int(m.group(2)))
        (yes if m.group(3) == yes_word else no).add(cell)
    return yes, no

def main():
    b = load_board()
    solv, unsolv = load_log("/tmp/l101census.log", "SOLVABLE")
    try:
        psurv, pdead = load_log("/tmp/l101probe.log", "PSURV")
    except FileNotFoundError:
        psurv, pdead = set(), set()
    false_surv = psurv & unsolv
    print(f"真可解 {len(solv)}｜不可解(或超时) {len(unsolv)}｜探针幸存 {len(psurv)}｜假幸存 {len(false_surv)}")
    rows = []
    for y in range(H):
        row = []
        for x in range(W):
            ch = b[y * W + x]
            if ch == "X":
                row.append("#")
            elif (x, y) in solv:
                row.append("S")
            elif (x, y) in false_surv:
                row.append("F")
            elif (x, y) in pdead:
                row.append("k")
            else:
                row.append(".")
        rows.append("".join(row))
    print("\n".join(rows))
    if solv:
        xs = [c[0] for c in solv]; ys = [c[1] for c in solv]
        print(f"\n真可解起点包围盒: x∈[{min(xs)},{max(xs)}] y∈[{min(ys)},{max(ys)}]，数量 {len(solv)}")

if __name__ == "__main__":
    main()
