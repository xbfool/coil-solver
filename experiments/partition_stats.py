#!/usr/bin/env python3
"""瓶颈分区的可行性检查 —— 先量一量，再决定要不要实现。

用的是 ericgopak/Mortal-Coil 的切割判据（src/Simulator.cpp:8，一共 5 行）：
站在格子 c 朝方向 d 看，如果

    (左边是墙 或 左前是墙) 且 (右边是墙 或 右前是墙)

那么 c -> c+d 这一步就是个「宽度为 1 的瓶颈」，把它当割边。
砍掉所有割边之后的连通分量就是 component。

分区法值不值得，全看切出来的区域有多小：区域够小才能离线穷举它的全部穿越方式
（那才是这套方法的本钱），要是切完还剩一个巨型区域，这条路直接就没用。
它那个仓库跑到了第 332 关，但同时警告只改善尾部——我们 2 秒能过的 L215 它要两小时。
"""
import sys, os
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LEVELS = os.environ.get("LEVELS", str(ROOT.parent / "coilbench" / "levels_all"))

DIRS = [(-1, 0), (0, -1), (1, 0), (0, 1)]          # L U R D
LEFT = {(-1, 0): (0, 1), (0, -1): (-1, 0), (1, 0): (0, -1), (0, 1): (1, 0)}
RIGHT = {d: (-LEFT[d][0], -LEFT[d][1]) for d in DIRS}


def load(level):
    with open(os.path.join(LEVELS, str(level))) as f:
        txt = f.read().strip()
    parts = txt.split("&")
    w = int(parts[0].split("=")[1])
    h = int(parts[1].split("=")[1])
    board = parts[2].split("=")[1]
    free = [[board[y * w + x] == "." for x in range(w)] for y in range(h)]
    return w, h, free


def is_bottleneck(free, w, h, x, y, d):
    """c=(x,y) 朝 d 走这一步是不是宽度为 1 的瓶颈"""
    def wall(px, py):
        return not (0 <= px < w and 0 <= py < h and free[py][px])
    lx, ly = LEFT[d]
    rx, ry = RIGHT[d]
    dx, dy = d
    left       = wall(x + lx,      y + ly)
    front_left = wall(x + dx + lx, y + dy + ly)
    right      = wall(x + rx,      y + ry)
    front_right= wall(x + dx + rx, y + dy + ry)
    return (left or front_left) and (right or front_right)


def analyse(level):
    w, h, free = load(level)
    total = sum(r.count(True) for r in free)

    # 砍掉割边后求连通分量
    comp = [[-1] * w for _ in range(h)]
    sizes = []
    ncut = 0
    for y0 in range(h):
        for x0 in range(w):
            if not free[y0][x0] or comp[y0][x0] >= 0:
                continue
            cid = len(sizes)
            n = 0
            q = deque([(x0, y0)])
            comp[y0][x0] = cid
            while q:
                x, y = q.popleft()
                n += 1
                for d in DIRS:
                    nx, ny = x + d[0], y + d[1]
                    if not (0 <= nx < w and 0 <= ny < h) or not free[ny][nx]:
                        continue
                    if is_bottleneck(free, w, h, x, y, d):
                        continue                       # 割边，不跨过去
                    if comp[ny][nx] < 0:
                        comp[ny][nx] = cid
                        q.append((nx, ny))
            sizes.append(n)

    for y in range(h):
        for x in range(w):
            if free[y][x]:
                for d in (DIRS[2], DIRS[3]):           # 每条边只数一次
                    nx, ny = x + d[0], y + d[1]
                    if 0 <= nx < w and 0 <= ny < h and free[ny][nx] \
                       and is_bottleneck(free, w, h, x, y, d):
                        ncut += 1

    sizes.sort(reverse=True)
    big = sizes[0] if sizes else 0
    small = sum(1 for s in sizes if s <= 20)
    return dict(level=level, wh=f"{w}x{h}", free=total, cuts=ncut,
                comps=len(sizes), biggest=big, biggest_pct=100.0 * big / total,
                small_comps=small, top5=sizes[:5])


if __name__ == "__main__":
    levels = [int(a) for a in sys.argv[1:]] or [99, 139, 195, 215, 223, 301, 401]
    print(f"{'关':>5} {'尺寸':>9} {'自由格':>7} {'割边':>6} {'区域数':>7} "
          f"{'最大区域':>9} {'占比':>7} {'<=20格':>7}  最大的5块")
    for lv in levels:
        try:
            r = analyse(lv)
        except FileNotFoundError:
            continue
        print(f"{r['level']:>5} {r['wh']:>9} {r['free']:>7} {r['cuts']:>6} "
              f"{r['comps']:>7} {r['biggest']:>9} {r['biggest_pct']:>6.1f}% "
              f"{r['small_comps']:>7}  {r['top5']}")
