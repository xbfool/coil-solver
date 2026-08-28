#!/usr/bin/env python3
"""分区法的地基测量：真解里每个 component 到底被进出几次。

这决定了「穿越目录」的规模，也就决定了整套分区法的架构：
  · 如果绝大多数 component 只被穿越一次，那目录就是 O(口数^2) 条，小得可以离线全枚举；
  · 如果经常被进出三四次，目录条数是段序列的排列组合，会爆炸 ——
    那就必须靠「按出入口序列等价归并」把它压回去（Mortal-Coil 说那是让目录不爆炸的关键）。

顺便量两件事：
  · 每个 component 的口数分布（掩码的位宽）；
  · 真解经过的「段」总数 —— 那是主搜索在 component 层面走的深度，
    要跟现在的格子级搜索深度（滑行步数）比一比，才知道分区法到底省了多少。
"""
import os, sys
from collections import deque, Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LEVELS = str(ROOT.parent / "coilbench" / "levels_all")
SOLS = r"C:\Users\xbfoo\AppData\Local\Temp\claude\D--workspace\a9a27850-3ba8-4b8e-8061-a644473312f2\scratchpad\sols"

DIRS = [(-1, 0), (0, -1), (1, 0), (0, 1)]          # L U R D
LEFT = {(-1, 0): (0, 1), (0, -1): (-1, 0), (1, 0): (0, -1), (0, 1): (1, 0)}
RIGHT = {d: (-LEFT[d][0], -LEFT[d][1]) for d in DIRS}


def load(lv):
    t = open(os.path.join(LEVELS, str(lv))).read().strip().split("&")
    w = int(t[0].split("=")[1]); h = int(t[1].split("=")[1]); b = t[2].split("=")[1]
    return w, h, [[b[y * w + x] == "." for x in range(w)] for y in range(h)]


def build(free, w, h):
    def wall(px, py):
        return not (0 <= px < w and 0 <= py < h and free[py][px])

    def bn(x, y, d):
        lx, ly = LEFT[d]; rx, ry = RIGHT[d]; dx, dy = d
        return ((wall(x + lx, y + ly) or wall(x + dx + lx, y + dy + ly)) and
                (wall(x + rx, y + ry) or wall(x + dx + rx, y + dy + ry)))

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
    return wall, comp, cells


def analyse(lv):
    w, h, free = load(lv)
    wall, comp, cells = build(free, w, h)

    ports = [set() for _ in cells]
    for y in range(h):
        for x in range(w):
            if not free[y][x]:
                continue
            for d in DIRS:
                nx, ny = x + d[0], y + d[1]
                if 0 <= nx < w and 0 <= ny < h and free[ny][nx] and comp[ny][nx] != comp[y][x]:
                    ports[comp[y][x]].add(((x, y), (nx, ny)))

    sp = os.path.join(SOLS, f"{lv}.sol")
    if not os.path.exists(sp):
        print(f"L{lv}: 没有解文件，跳过")
        return
    t = open(sp).read().strip().split("&")
    sx = int(t[0].split("=")[1]); sy = int(t[1].split("=")[1])
    path = "".join(ch for ch in t[2].split("=")[1] if ch in "LURD")

    # 沿真解走出完整的格子序列
    seq = [(sx, sy)]
    vis = {(sx, sy)}
    cur = (sx, sy)
    for ch in path:
        d = DIRS["LURD".index(ch)]
        while True:
            q = (cur[0] + d[0], cur[1] + d[1])
            if wall(q[0], q[1]) or q in vis:
                break
            cur = q; vis.add(cur); seq.append(cur)

    total_free = sum(r.count(True) for r in free)
    assert len(seq) == total_free, f"解不完整 {len(seq)}/{total_free}"

    # 把格子序列切成「段」：连续待在同一个 component 里算一段
    segs = []
    run_c = comp[seq[0][1]][seq[0][0]]; run_n = 1
    for p in seq[1:]:
        c = comp[p[1]][p[0]]
        if c == run_c:
            run_n += 1
        else:
            segs.append((run_c, run_n)); run_c = c; run_n = 1
    segs.append((run_c, run_n))

    visits = Counter(c for c, _ in segs)
    dist = Counter(visits.values())
    pdist = Counter(len(p) for p in ports)
    # 从没被单独进出、只有一次的比例
    once = sum(1 for c in range(len(cells)) if visits.get(c, 0) == 1)

    print(f"L{lv}: 自由格 {total_free}, 区域 {len(cells)}, 滑行步数 {len(path)}")
    print(f"    区域口数分布 {dict(sorted(pdist.items()))}")
    print(f"    真解把区域进出几次: {dict(sorted(dist.items()))}  "
          f"(只进出 1 次的区域 {once}/{len(cells)} = {100.0*once/len(cells):.0f}%)")
    print(f"    段总数 {len(segs)}  <- 这就是主搜索在 component 层面的深度"
          f"（对比格子级的滑行步数 {len(path)}）")
    mx = max(visits.values())
    worst = [c for c in visits if visits[c] == mx][0]
    print(f"    最多被进出 {mx} 次（区域 {worst}，{len(cells[worst])} 格，{len(ports[worst])} 个口）")


if __name__ == "__main__":
    for lv in [int(a) for a in sys.argv[1:]] or [99, 139, 165, 195, 215]:
        analyse(lv)
