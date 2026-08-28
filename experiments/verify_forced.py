#!/usr/bin/env python3
"""校验：传播推出的强制边/排除边，是否真的与唯一解一致？

传播用的是「每格恰好 2 条边」的环模型，但真解是路径——有 2 个端点只用 1 条边。
所以传播可能推错。这个脚本拿真解当标准答案逐条核对，决定 v6 能不能拿它当硬剪枝。

用法: verify_forced.py <关号> <solver 可执行文件>
"""
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

LV = str(ROOT.parent / "coilbench" / "levels_public" / "%s")
D = {"L": (-1, 0), "R": (1, 0), "U": (0, -1), "D": (0, 1)}


def propagate(fs):
    inc = defaultdict(set)
    for (x, y) in fs:
        for dx, dy in ((1, 0), (0, 1)):
            q = (x + dx, y + dy)
            if q in fs:
                e = ((x, y), q)
                inc[(x, y)].add(e); inc[q].add(e)
    par = {c: c for c in fs}

    def find(a):
        while par[a] != a:
            par[a] = par[par[a]]; a = par[a]
        return a

    forced, excluded = set(), set()
    changed = True
    while changed:
        changed = False
        for c in fs:
            for e in inc[c]:
                if e not in forced and e not in excluded and find(e[0]) == find(e[1]):
                    excluded.add(e); changed = True
        for c in fs:
            f = [e for e in inc[c] if e in forced]
            u = [e for e in inc[c] if e not in forced and e not in excluded]
            if len(f) == 2 and u:
                excluded.update(u); changed = True
            elif len(f) + len(u) == 2 and u:
                for e in u:
                    forced.add(e)
                    ra, rb = find(e[0]), find(e[1])
                    if ra != rb:
                        par[ra] = rb
                changed = True
    return forced, excluded


def main():
    n, solver = sys.argv[1], sys.argv[2]
    s = open(LV % n).read().strip()
    w = int(s.split("x=")[1].split("&")[0])
    h = int(s.split("y=")[1].split("&")[0])
    b = s.split("board=")[1]
    fs = {(x, y) for y in range(h) for x in range(w) if b[y * w + x] == "."}

    out = subprocess.run([solver, LV % n], capture_output=True, text=True, timeout=600).stdout.strip()
    sx = int(out.split("x=")[1].split("&")[0]); sy = int(out.split("y=")[1].split("&")[0])
    path = out.split("path=")[1]

    # 真解用到的边
    cur = (sx, sy); seq = [cur]; used = set()
    for mv in path:
        dx, dy = D[mv]
        while True:
            nxt = (cur[0] + dx, cur[1] + dy)
            if nxt not in fs or nxt in seq:
                break
            used.add((min(cur, nxt), max(cur, nxt)))
            seq.append(nxt); cur = nxt
    assert len(seq) == len(fs), f"解没覆盖全部格子 {len(seq)} != {len(fs)}"

    forced, excluded = propagate(fs)
    norm = lambda e: (min(e), max(e))
    forced = {norm(e) for e in forced}
    excluded = {norm(e) for e in excluded}

    bad_f = forced - used              # 推成必选、真解却没用 → 硬剪枝会把真解剪掉
    bad_x = excluded & used            # 推成排除、真解却用了 → 同样致命
    print(f"L{n}: 自由格 {len(fs)}  真解用边 {len(used)}")
    print(f"  强制边 {len(forced)} 条，其中真解没用的：{len(bad_f)}")
    print(f"  排除边 {len(excluded)} 条，其中真解用了的：{len(bad_x)}")
    if bad_f:
        print("  反例(强制但没用):", list(bad_f)[:5])
    if bad_x:
        print("  反例(排除但用了):", list(bad_x)[:5])
    print("  结论:", "可以当硬剪枝 ✓" if not bad_f and not bad_x else "不能直接当硬剪枝 ✗")


if __name__ == "__main__":
    main()
