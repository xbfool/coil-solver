#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""赢家起点统计分析（对 /tmp/harvest.log）

问题：生成器政策是否在起点位置留下可利用的空间偏置？
对比量（每个都带均匀基线）：
  1. 起点到最近边界的距离分布 vs 该关自由格的均匀基线
  2. 首步方向
  3. 首滑长度（replay 得到）
  4. 起点的局部墙型（8 邻域墙数）vs 基线
分零信息类（gfix=0）和其余类分别统计——零类才是要打的目标。
"""
import re
import sys
import random

def load(L):
    raw = open(f"/mnt/d/workspace/coilbench/levels_all/{L}").read()
    w = int(re.search(r"x=(\d+)", raw).group(1))
    h = int(re.search(r"y=(\d+)", raw).group(1))
    b = re.search(r"board=([.X]+)", raw).group(1)
    free = set((x, y) for y in range(h) for x in range(w) if b[y * w + x] == ".")
    return w, h, free

def border_dist(x, y, w, h):
    return min(x, y, w - 1 - x, h - 1 - y)

def main():
    logpath = sys.argv[1] if len(sys.argv) > 1 else "/tmp/harvest.log"
    rows = []
    for line in open(logpath):
        m = re.match(r"(\d+) gfix=(\d+) start=(\d+),(\d+) first=(\w)", line)
        if m:
            rows.append(tuple(int(v) if v.isdigit() else v for v in m.groups()))
    zero = [r for r in rows if r[1] == 0]
    other = [r for r in rows if r[1] != 0]
    print(f"样本：零类 {len(zero)} 关，其余 {len(other)} 关")
    random.seed(1)
    for name, rs in (("零信息类", zero), ("其余类", other)):
        if not rs:
            continue
        wd, bd = [], []
        dirs = {}
        wall8_w, wall8_b = [], []
        for (L, g, sx, sy, fd) in rs:
            w, h, free = load(L)
            wd.append(border_dist(sx, sy, w, h) / (min(w, h) / 2))
            dirs[fd] = dirs.get(fd, 0) + 1
            n8 = sum(1 for dx in (-1, 0, 1) for dy in (-1, 0, 1)
                     if (dx or dy) and (sx + dx, sy + dy) not in free)
            wall8_w.append(n8)
            base = random.sample(sorted(free), min(200, len(free)))
            for (bx, by) in base:
                bd.append(border_dist(bx, by, w, h) / (min(w, h) / 2))
                wall8_b.append(sum(1 for dx in (-1, 0, 1) for dy in (-1, 0, 1)
                                   if (dx or dy) and (bx + dx, by + dy) not in free))
        wd.sort(); bd.sort()
        med = lambda a: a[len(a) // 2] if a else 0
        avg = lambda a: sum(a) / len(a) if a else 0
        print(f"[{name}]")
        print(f"  归一化离边距离：赢家 median={med(wd):.3f} mean={avg(wd):.3f}  基线 median={med(bd):.3f} mean={avg(bd):.3f}")
        print(f"  首步方向: {dirs}")
        print(f"  8邻域墙数：赢家 mean={avg(wall8_w):.2f}  基线 mean={avg(wall8_b):.2f}")
        near = sum(1 for v in wd if v < 0.1) / len(wd)
        nearb = sum(1 for v in bd if v < 0.1) / len(bd)
        print(f"  『贴边』(归一距<0.1) 比例：赢家 {100*near:.0f}% vs 基线 {100*nearb:.0f}%")

if __name__ == "__main__":
    main()
