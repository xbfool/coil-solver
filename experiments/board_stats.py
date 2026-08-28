#!/usr/bin/env python3
"""看关卡的生成结构：墙密度、度分布、2x2 空块比例、连通性。

动机：空盘上 Coil 解只有 32 个（常数），说明解几乎被强制。
那大盘能不能构造性求解，取决于这些盘面本身长什么样——是开阔场地还是走廊迷宫。
用法: board_stats.py <关号> [关号...]
"""
import sys
from collections import Counter

LV = "/mnt/d/works/coil/coilbench/levels_public/%s"


def load(n):
    s = open(LV % n).read().strip()
    w = int(s.split("x=")[1].split("&")[0])
    h = int(s.split("y=")[1].split("&")[0])
    b = s.split("board=")[1]
    return w, h, b


def stats(n):
    w, h, b = load(n)
    free = [(x, y) for y in range(h) for x in range(w) if b[y * w + x] == "."]
    fs = set(free)
    nf = len(free)

    deg = Counter()
    for (x, y) in free:
        d = sum(1 for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)) if (x + dx, y + dy) in fs)
        deg[d] += 1

    # 2x2 全空的块数：衡量「开阔度」。全是走廊的话这个数会很低
    open2 = sum(1 for y in range(h - 1) for x in range(w - 1)
                if all((x + i, y + j) in fs for i in (0, 1) for j in (0, 1)))

    # 连通分量
    seen, comps = set(), 0
    for s0 in free:
        if s0 in seen:
            continue
        comps += 1
        st = [s0]; seen.add(s0)
        while st:
            x, y = st.pop()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                p = (x + dx, y + dy)
                if p in fs and p not in seen:
                    seen.add(p); st.append(p)

    degtxt = " ".join(f"deg{k}={deg[k] * 100 / nf:.0f}%" for k in range(5) if deg[k])
    print(f"L{n:<5} {w}x{h}  自由格 {nf:>8,} ({nf * 100 / (w * h):.0f}%)  连通块 {comps}")
    print(f"        {degtxt}   2x2全空块 {open2:>8,} ({open2 * 100 / max(1, nf):.0f}% of free)")


if __name__ == "__main__":
    for a in sys.argv[1:]:
        stats(a)
