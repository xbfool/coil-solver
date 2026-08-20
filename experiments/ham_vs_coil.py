#!/usr/bin/env python3
"""哈密顿路径里有多大比例是合法 Coil 解？

动机：插头 DP / 匹配 这类「换空间」的办法都是构造**几何对象**（哈密顿路径），
而 Coil 多一条纯时序约束——转向时前方格子必须已经走过。如果合法比例极低，
那「先枚举哈密顿路径再过滤」的路子就是死的，得另找结构。
"""
import sys
from itertools import product

D = {'L': (-1, 0), 'R': (1, 0), 'U': (0, -1), 'D': (0, 1)}


def solve_counts(w, h, walls=()):
    free = [(x, y) for y in range(h) for x in range(w) if (x, y) not in walls]
    fs = set(free)
    n = len(free)
    ham = 0      # 有向哈密顿路径数
    coil = 0     # 其中满足 Coil 时序条件的

    def nbrs(c):
        x, y = c
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            p = (x + dx, y + dy)
            if p in fs:
                yield p

    order = {}
    seq = []

    def rec(cur, depth):
        nonlocal ham, coil
        if depth == n:
            ham += 1
            if coil_ok():
                coil += 1
            return
        for nb in nbrs(cur):
            if nb in order:
                continue
            order[nb] = depth
            seq.append(nb)
            rec(nb, depth + 1)
            seq.pop()
            del order[nb]

    def coil_ok():
        # 走到 seq[i] 时若发生转向，前方格子必须是墙或更早访问过
        for i in range(1, len(seq) - 1):
            prev, cur, nxt = seq[i - 1], seq[i], seq[i + 1]
            din = (cur[0] - prev[0], cur[1] - prev[1])
            dout = (nxt[0] - cur[0], nxt[1] - cur[1])
            if din == dout:
                continue                      # 直行，无需检查
            ahead = (cur[0] + din[0], cur[1] + din[1])
            if ahead not in fs:
                continue                      # 前方是墙/边界，合法停下
            if order[ahead] < order[cur]:
                continue                      # 前方已经走过，合法停下
            return False                      # 前方还空着 —— 按规则你不能停，非法
        return True

    for s in free:
        order.clear(); seq.clear()
        order[s] = 0; seq.append(s)
        rec(s, 1)
    return n, ham, coil


def main():
    cases = [("3x3 空", 3, 3, ()),
             ("4x4 空", 4, 4, ()),
             ("5x5 空", 5, 5, ()),
             ("4x4 带2墙", 4, 4, {(1, 1), (2, 2)}),
             ("5x5 带6墙", 5, 5, {(1, 1), (3, 1), (2, 2), (0, 3), (4, 3), (1, 4)})]
    print(f"{'盘面':<12} {'格数':>4} {'哈密顿路径':>12} {'合法Coil解':>12} {'占比':>10}")
    for name, w, h, walls in cases:
        n, ham, coil = solve_counts(w, h, walls)
        pct = (coil / ham * 100) if ham else 0
        print(f"{name:<12} {n:>4} {ham:>12,} {coil:>12,} {pct:>9.3f}%")


if __name__ == "__main__":
    main()
