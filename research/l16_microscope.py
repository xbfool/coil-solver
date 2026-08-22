#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""L16 显微镜：81 条边的完整账本——真解用/不用 vs 传播已定/未定，画出漏网边地图"""
import re
import subprocess

DD = {"R": (1, 0), "L": (-1, 0), "D": (0, 1), "U": (0, -1)}

def load(L):
    raw = open(f"/mnt/d/workspace/coilbench/levels_all/{L}").read()
    w = int(re.search(r"x=(\d+)", raw).group(1))
    h = int(re.search(r"y=(\d+)", raw).group(1))
    b = re.search(r"board=([.X]+)", raw).group(1)
    free = set((x, y) for y in range(h) for x in range(w) if b[y * w + x] == ".")
    return w, h, free

def solution_edges(free, soltext):
    sx = int(re.search(r"x=(\d+)", soltext).group(1))
    sy = int(re.search(r"y=(\d+)", soltext).group(1))
    path = re.search(r"path=([UDLR]+)", soltext).group(1)
    cur = (sx, sy)
    vis = {cur}
    used = set()
    for ch in path:
        d = DD[ch]
        while (cur[0] + d[0], cur[1] + d[1]) in free and (cur[0] + d[0], cur[1] + d[1]) not in vis:
            nxt = (cur[0] + d[0], cur[1] + d[1])
            used.add(frozenset((cur, nxt)))
            cur = nxt
            vis.add(cur)
    return (sx, sy), cur, used

def all_edges(free):
    E = set()
    for (x, y) in free:
        for dx, dy in ((1, 0), (0, 1)):
            if (x + dx, y + dy) in free:
                E.add(frozenset(((x, y), (x + dx, y + dy))))
    return E

def main():
    L = 16
    w, h, free = load(L)
    sol = open(f"solutions/{L}.sol").read()
    start, end, used = solution_edges(free, sol)
    E = all_edges(free)
    banned_truth = E - used
    print(f"L16 {w}x{h} free={len(free)} 边={len(E)} 真解用={len(used)} 真禁={len(banned_truth)}")
    print(f"起点 {start} 终点 {end}")
    # 边地图：横边画在两格之间。用字符网格 2w-1 × 2h-1
    G = [[" "] * (2 * w - 1) for _ in range(2 * h - 1)]
    for (x, y) in free:
        deg = sum(1 for dx, dy in ((1,0),(-1,0),(0,1),(0,-1)) if (x+dx, y+dy) in free)
        ch = "o"
        if (x, y) == start:
            ch = "S"
        elif (x, y) == end:
            ch = "E"
        G[2 * y][2 * x] = ch
    for e in E:
        (a, b) = sorted(e)
        mx, my = a[0] + b[0], a[1] + b[1]
        G[my][mx] = "=" if e in used else "."   # = 真解用，. 真解不用
    for row in G:
        print("".join(row))
    print("图例：o格 S起点 E终点 =真解用边 .真解不用边")
    print("（对照求解器 --verify 的判定：必用18/禁用5——其余 58 条边全是漏网）")

if __name__ == "__main__":
    main()
