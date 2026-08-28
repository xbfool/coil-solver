#!/usr/bin/env python3
"""约束传播能把路径定死多少？

思路（回答「能不能拆成子问题」）：不猜中间状态，而是推。
把解看成边的选择：每个自由格恰好用 2 条相邻边（两个端点用 1 条）。于是
  · 度=2 的格子 → 它那两条边必然入选（实测这类格子占 30%）
  · 某格已定 2 条边 → 其余相邻边全部排除
  · 某格「已定 + 待定」正好 = 2 → 待定的全部入选
反复迭代到不动点。定死的边会连成一段段路径骨架，把棋盘切成互相独立的小缺口 —— 那就是拆分点。

（近似：端点只用 1 条边，这里按「全都用 2 条」传播，全盘至多 2 个格子会被过度约束，
对量级判断无影响。）

用法: forced_edges.py <关号>...
"""
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

LV = str(ROOT.parent / "coilbench" / "levels_public" / "%s")


def analyse(n):
    s = open(LV % n).read().strip()
    w = int(s.split("x=")[1].split("&")[0])
    h = int(s.split("y=")[1].split("&")[0])
    b = s.split("board=")[1]
    fs = {(x, y) for y in range(h) for x in range(w) if b[y * w + x] == "."}

    inc = defaultdict(set)                       # 格 -> 相邻边集合
    for (x, y) in fs:
        for dx, dy in ((1, 0), (0, 1)):
            q = (x + dx, y + dy)
            if q in fs:
                e = ((x, y), q)
                inc[(x, y)].add(e); inc[q].add(e)

    # 并查集：定过的边把格子连成片，用来禁止提前闭环（最终是一条路径，任何环都非法）
    par = {c: c for c in fs}

    def find(a):
        while par[a] != a:
            par[a] = par[par[a]]; a = par[a]
        return a

    forced, excluded = set(), set()
    changed = True
    while changed:
        changed = False
        # 规则3：两端已被强制边连通的边，用了就成环 —— 直接排除
        for c in fs:
            for e in inc[c]:
                if e in forced or e in excluded:
                    continue
                if find(e[0]) == find(e[1]):
                    excluded.add(e); changed = True
        for c in fs:
            f = [e for e in inc[c] if e in forced]
            u = [e for e in inc[c] if e not in forced and e not in excluded]
            if len(f) > 2:
                return None                       # 矛盾
            if len(f) == 2 and u:
                excluded.update(u); changed = True
            elif len(f) + len(u) == 2 and u:
                for e in u:
                    forced.add(e)
                    ra, rb = find(e[0]), find(e[1])
                    if ra != rb:
                        par[ra] = rb
                changed = True
            elif len(f) + len(u) < 2:
                return None

    # 骨架片段：把已定边连成链，看被切成几段
    adj = defaultdict(list)
    for (a, c) in forced:
        adj[a].append(c); adj[c].append(a)
    seen, frags, longest = set(), 0, 0
    for c in fs:
        if c in seen or not adj[c]:
            continue
        stack, size = [c], 0
        seen.add(c)
        while stack:
            v = stack.pop(); size += 1
            for u2 in adj[v]:
                if u2 not in seen:
                    seen.add(u2); stack.append(u2)
        frags += 1; longest = max(longest, size)

    nf = len(fs)
    print(f"L{n:<5} 自由格 {nf:>7,}  边 {sum(len(v) for v in inc.values())//2:>7,}  "
          f"强制边 {len(forced):>7,} ({len(forced)*100/nf:5.1f}% of 需要的 ~{nf} 条)")
    print(f"        排除边 {len(excluded):>7,}   骨架片段 {frags:>6,} 段  最长片段 {longest:>6,} 格  "
          f"未被骨架覆盖的格 {nf - sum(1 for c in fs if adj[c]):>6,}")


if __name__ == "__main__":
    for a in sys.argv[1:]:
        analyse(a)
