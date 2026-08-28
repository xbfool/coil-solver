#!/usr/bin/env python3
"""瓶颈分区的核心判据：穿越解数为 0 的区域必含端点。

推理（只对**恰好 2 个口**的区域，它们占 70~80%）：
割边是「边」，路径走过一次就不能再走。2 口区域只有两条割边，所以路径进出它最多各一次。
于是只有两种可能：
  · **恰好穿越一次**：从 e1 进、从 e2 出（或反过来），而且必须把区域内的格子全覆盖；
  · 或者**端点在这个区域里**（进去出不来 / 从里面出发）。
所以：两个方向都不存在穿越解 => **这个区域必然含起点或终点**。
全盘只有 2 个端点，所以这种区域最多 2 个 —— 一旦找到，起点候选就从几千格塌到几十格。
这正是传播干不了的那件事（偶数关起点证伪率 0%）。

穿越解的判定必须**保守**（宁可判有解），否则「无解」的结论就不 sound。唯一需要小心的
是滑行会撞到区域外的格子，而外面的状态我们不知道：
  · 撞到**进来那条割边**的外侧格：那格是我们进来前走过的，必然已访问 => 必须停；
  · 撞到**出去那条割边**的外侧格：不知道它走没走过 => 两种都试（停下 / 滑出去），
    「滑出去」只有在区域已被全覆盖时才算一个穿越解；
  · 撞到区域内的格子：走没走过我们自己算得出来，精确判定。
区域最大才 57 格，用一个 uint64 位掩码就能精确穷举。
"""
import os, sys
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LEVELS = str(ROOT.parent / "coilbench" / "levels_all")
SOLS = r"C:\Users\xbfoo\AppData\Local\Temp\claude\D--workspace\a9a27850-3ba8-4b8e-8061-a644473312f2\scratchpad\sols"

DIRS = [(-1, 0), (0, -1), (1, 0), (0, 1)]
LEFT = {(-1, 0): (0, 1), (0, -1): (-1, 0), (1, 0): (0, -1), (0, 1): (1, 0)}
RIGHT = {d: (-LEFT[d][0], -LEFT[d][1]) for d in DIRS}
sys.setrecursionlimit(10000)


def load(lv):
    t = open(os.path.join(LEVELS, str(lv))).read().strip().split("&")
    w = int(t[0].split("=")[1]); h = int(t[1].split("=")[1]); b = t[2].split("=")[1]
    return w, h, [[b[y * w + x] == "." for x in range(w)] for y in range(h)]


def make_bn(free, w, h):
    def wall(px, py):
        return not (0 <= px < w and 0 <= py < h and free[py][px])
    def bn(x, y, d):
        lx, ly = LEFT[d]; rx, ry = RIGHT[d]; dx, dy = d
        return ((wall(x + lx, y + ly) or wall(x + dx + lx, y + dy + ly)) and
                (wall(x + rx, y + ry) or wall(x + dx + rx, y + dy + ry)))
    return wall, bn


def partition(free, w, h, bn):
    comp = [[-1] * w for _ in range(h)]
    cells = []
    for y0 in range(h):
        for x0 in range(w):
            if not free[y0][x0] or comp[y0][x0] >= 0:
                continue
            cid = len(cells); mine = []
            q = deque([(x0, y0)]); comp[y0][x0] = cid
            while q:
                x, y = q.popleft(); mine.append((x, y))
                for d in DIRS:
                    nx, ny = x + d[0], y + d[1]
                    if not (0 <= nx < w and 0 <= ny < h) or not free[ny][nx]:
                        continue
                    if bn(x, y, d):
                        continue
                    if comp[ny][nx] < 0:
                        comp[ny][nx] = cid; q.append((nx, ny))
            cells.append(mine)
    return comp, cells


def has_transit(cellset, idx, full, wall, comp, cid, in_edge, out_edge):
    """in_edge/out_edge 都是 (内格, 外格)。返回「存在一次穿越就把区域覆盖完」的走法与否。"""
    (bi, ai) = in_edge
    d0 = (bi[0] - ai[0], bi[1] - ai[1])

    def inside(p):
        return p in idx

    def go(cur, d, vis):
        # 沿 d 滑到底（cur 已访问）
        while True:
            q = (cur[0] + d[0], cur[1] + d[1])
            if wall(q[0], q[1]):
                break                                   # 撞墙，停
            if not inside(q):
                if (cur, q) == out_edge:
                    if vis == full:
                        return True                     # 全覆盖了还能滑出去 => 穿越解
                    break                               # 外侧格若已访问就停在这儿
                break                                   # 进来那条边的外侧格必然已访问 => 停
            qi = idx[q]
            if vis >> qi & 1:
                break                                   # 撞自己走过的，停
            vis |= 1 << qi
            cur = q
        # 停在 cur，换个方向再滑
        for d2 in DIRS:
            if d2 == d:
                continue                                # 前方刚被挡住
            n = (cur[0] + d2[0], cur[1] + d2[1])
            if wall(n[0], n[1]):
                continue
            if not inside(n):
                if (cur, n) == out_edge and vis == full:
                    return True                         # 直接从出口走人
                continue
            if vis >> idx[n] & 1:
                continue                                # 那格走过了，这个方向动不了
            if go(cur, d2, vis):
                return True
        return False

    return go(bi, d0, 1 << idx[bi])


def analyse(lv, verbose=False):
    w, h, free = load(lv)
    wall, bn = make_bn(free, w, h)
    comp, cells = partition(free, w, h, bn)

    # 每个区域的割边（内格, 外格）
    ports = [[] for _ in cells]
    for y in range(h):
        for x in range(w):
            if not free[y][x]:
                continue
            for d in DIRS:
                nx, ny = x + d[0], y + d[1]
                if not (0 <= nx < w and 0 <= ny < h) or not free[ny][nx]:
                    continue
                if comp[ny][nx] != comp[y][x]:
                    ports[comp[y][x]].append(((x, y), (nx, ny)))

    must = []                       # 必含端点的区域
    n2 = 0
    for cid, mine in enumerate(cells):
        p = ports[cid]
        if len(p) != 2 or len(mine) > 60:
            continue
        n2 += 1
        idx = {c: i for i, c in enumerate(mine)}
        full = (1 << len(mine)) - 1
        ok = (has_transit(mine, idx, full, wall, comp, cid, p[0], p[1]) or
              has_transit(mine, idx, full, wall, comp, cid, p[1], p[0]))
        if not ok:
            must.append(cid)

    total = sum(len(c) for c in cells)
    must_cells = sum(len(cells[c]) for c in must)
    line = (f"L{lv}: 自由格 {total}, 区域 {len(cells)}, 2口区域 {n2}, "
            f"**零穿越解区域 {len(must)} 个**（共 {must_cells} 格, 占 {100.0*must_cells/total:.2f}%）")

    # soundness：真解的起点/终点必须落在这些区域里
    sp = os.path.join(SOLS, f"{lv}.sol")
    if os.path.exists(sp) and must:
        t = open(sp).read().strip().split("&")
        sx = int(t[0].split("=")[1]); sy = int(t[1].split("=")[1])
        path = t[2].split("=")[1]
        cur = (sx, sy); vis = {cur}
        for ch in path:
            d = DIRS["LURD".index(ch)]
            while True:
                q = (cur[0] + d[0], cur[1] + d[1])
                if wall(q[0], q[1]) or q in vis:
                    break
                cur = q; vis.add(cur)
        ends = {(sx, sy), cur}
        mset = set(must)
        bad = [c for c in must if not any(comp[y][x] == c for (x, y) in ends)]
        line += f" | 真端点 {sorted(ends)} 落在区域 {sorted({comp[y][x] for (x,y) in ends})}"
        line += "  soundness: " + ("OK" if not bad else f"**{len(bad)} 个区域不含真端点 —— 判据错了**")
    print(line)
    return must


if __name__ == "__main__":
    for lv in [int(a) for a in sys.argv[1:]] or [99, 139, 165, 195, 215]:
        analyse(lv)
