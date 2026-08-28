#!/usr/bin/env python3
"""沿着真解走一遍，看每个决策点有几个候选能活过剪枝。

动机：如果绝大多数步是「只有一条路」，那束搜索（宽度受限的宽搜）就能一路推到底，
不需要回溯——这对 1990x1990 这种回溯根本跑不完的规模是决定性的。
反之如果分支点很多且分散，束搜索也会走丢。

用法: branching.py <关号> <solver>
"""
import subprocess
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

LV = str(ROOT.parent / "coilbench" / "levels_public" / "%s")
D = {"L": (-1, 0), "R": (1, 0), "U": (0, -1), "D": (0, 1)}


def main():
    n, solver = sys.argv[1], sys.argv[2]
    s = open(LV % n).read().strip()
    w = int(s.split("x=")[1].split("&")[0])
    h = int(s.split("y=")[1].split("&")[0])
    b = s.split("board=")[1]
    fs = {(x, y) for y in range(h) for x in range(w) if b[y * w + x] == "."}

    out = subprocess.run([solver, LV % n], capture_output=True, text=True, timeout=900).stdout.strip()
    cur = (int(out.split("x=")[1].split("&")[0]), int(out.split("y=")[1].split("&")[0]))
    path = out.split("path=")[1]

    un = set(fs); un.discard(cur)
    col = lambda c: (c[0] + c[1]) & 1
    hist = Counter()
    forced_run = 0

    def slide(p, d, unv):
        dx, dy = D[d]; q = p; got = []
        while (q[0] + dx, q[1] + dy) in unv:
            q = (q[0] + dx, q[1] + dy); got.append(q)
        return q, got

    def viable(p, unv):
        """返回能活过 奇偶+孤格+连通 三条剪枝的方向数"""
        ok = 0
        for d in D:
            q, got = slide(p, d, unv)
            if not got:
                continue
            rest = unv - set(got)
            r = len(rest)
            if r:
                # 奇偶
                if sum(1 for c in rest if col(c) != col(q)) != (r + 1) // 2:
                    continue
                # 连通（从 q 出发能否走到所有剩余格）
                seen = set(); st = [x for x in
                                    [(q[0]+1, q[1]), (q[0]-1, q[1]), (q[0], q[1]+1), (q[0], q[1]-1)]
                                    if x in rest]
                seen.update(st)
                while st:
                    v = st.pop()
                    for nb in ((v[0]+1, v[1]), (v[0]-1, v[1]), (v[0], v[1]+1), (v[0], v[1]-1)):
                        if nb in rest and nb not in seen:
                            seen.add(nb); st.append(nb)
                if len(seen) != r:
                    continue
            ok += 1
        return ok

    for mv in path:
        k = viable(cur, un)
        hist[k] += 1
        if k <= 1:
            forced_run += 1
        q, got = slide(cur, mv, un)
        un -= set(got); cur = q

    tot = sum(hist.values())
    print(f"L{n}: 决策点 {tot}  分布 " +
          "  ".join(f"{k}个候选:{v}({v*100/tot:.0f}%)" for k, v in sorted(hist.items())))
    print(f"     其中被完全逼死（<=1 个候选）的占 {forced_run*100/tot:.0f}%")


if __name__ == "__main__":
    main()
