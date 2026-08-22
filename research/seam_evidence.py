#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""缝合线理论证据表：重放 solutions/ 下全部解，输出起终点 L1 距离与盘面对角线的对比"""
import os
import re

DD = {"L": (-1, 0), "R": (1, 0), "U": (0, -1), "D": (0, 1)}

def endpoint(levelpath, soltext):
    raw = open(levelpath).read()
    W = int(re.search(r"x=(\d+)", raw).group(1))
    H = int(re.search(r"y=(\d+)", raw).group(1))
    b = re.search(r"board=([.X]+)", raw).group(1)
    free = set((x, y) for y in range(H) for x in range(W) if b[y * W + x] == ".")
    sx = int(re.search(r"x=(\d+)", soltext).group(1))
    sy = int(re.search(r"y=(\d+)", soltext).group(1))
    path = re.search(r"path=([UDLR]+)", soltext).group(1)
    vis = {(sx, sy)}
    cur = (sx, sy)
    for ch in path:
        dx, dy = DD[ch]
        while (cur[0] + dx, cur[1] + dy) in free and (cur[0] + dx, cur[1] + dy) not in vis:
            cur = (cur[0] + dx, cur[1] + dy)
            vis.add(cur)
    full = len(vis) == len(free)
    d = abs(cur[0] - sx) + abs(cur[1] - sy)
    return (sx, sy), cur, d, W + H, full

def main():
    rows = []
    for fn in sorted(os.listdir("solutions"), key=lambda s: int(s.split(".")[0])):
        L = fn.split(".")[0]
        sol = open(f"solutions/{fn}").read()
        if "path=" not in sol:
            continue
        s, e, d, diag, full = endpoint(f"/mnt/d/workspace/coilbench/levels_all/{L}", sol)
        rows.append((int(L), s, e, d, diag, full))
    print(f"{'关':>5} {'起点':>10} {'终点':>10} {'L1距离':>6} {'盘面W+H':>7} 覆盖完整")
    near = 0
    for (L, s, e, d, diag, full) in rows:
        print(f"{L:>5} {str(s):>10} {str(e):>10} {d:>6} {diag:>7} {'✓' if full else '✗!'}")
        if d <= 8:
            near += 1
    print(f"\n起终点 L1≤8 的比例：{near}/{len(rows)}")

if __name__ == "__main__":
    main()
