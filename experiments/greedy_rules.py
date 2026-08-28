#!/usr/bin/env python3
"""固定转向规则能覆盖多少？

动机：盘面被证实是「照着有解」构造的（无度1格、墙的奇偶完全平衡）。
如果生成器用的是某种简单确定性走法（比如一直右转的螺旋），那固定规则应该能走出很高覆盖率。
覆盖率接近 100% → 生成结构可利用；只有 60~70% → 说明解不是简单规则能复现的。

用法: greedy_rules.py <关号>...
"""
import sys

DIRS = [(1, 0), (0, 1), (-1, 0), (0, -1)]      # R D L U，顺时针


def load(n):
    s = open("/mnt/d/works/coil/coilbench/levels_public/%s" % n).read().strip()
    w = int(s.split("x=")[1].split("&")[0])
    h = int(s.split("y=")[1].split("&")[0])
    return w, h, s.split("board=")[1]


def run(w, h, b, start, d0, turn):
    """turn=+1 优先顺时针转，-1 优先逆时针。返回覆盖格数。"""
    free = lambda x, y: 0 <= x < w and 0 <= y < h and b[y * w + x] == "."
    vis = set([start])
    x, y = start
    d = d0
    while True:
        dx, dy = DIRS[d]
        moved = False
        while free(x + dx, y + dy) and (x + dx, y + dy) not in vis:
            x, y = x + dx, y + dy
            vis.add((x, y))
            moved = True
        # 停下了，按偏好挑一个垂直方向
        nxt = None
        for cand in ((d + turn) % 4, (d - turn) % 4):
            cdx, cdy = DIRS[cand]
            if free(x + cdx, y + cdy) and (x + cdx, y + cdy) not in vis:
                nxt = cand
                break
        if nxt is None:
            return len(vis), moved
        d = nxt


def main():
    for n in sys.argv[1:]:
        w, h, b = load(n)
        total = b.count(".")
        best = (0, None)
        # 试几个起点 × 两种转向偏好 × 四个初始方向
        corners = [(0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)]
        starts = [c for c in corners if b[c[1] * w + c[0]] == "."]
        starts += [(x, y) for y in range(h) for x in range(w) if b[y * w + x] == "."][:1]
        for s in starts:
            for turn in (1, -1):
                for d0 in range(4):
                    cov, _ = run(w, h, b, s, d0, turn)
                    if cov > best[0]:
                        best = (cov, (s, d0, turn))
        print(f"L{n:<5} {w}x{h}  自由格 {total:>7,}  固定规则最好覆盖 {best[0]:>7,} "
              f"({best[0] * 100 / total:5.1f}%)")


if __name__ == "__main__":
    main()
