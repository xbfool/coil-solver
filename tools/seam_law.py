#!/usr/bin/env python3
"""缝定律普查：真解的两个端点到底有多近？

笔记里记的是"真解是近环（82/82 实证，L1 中位 7 最大 41）"。
但那是 82 个样本，而且集中在低关号。银行里有 800 个解，其中一批能过官方校验 ——
拿它们全量验一遍，看这条定律在高段还成不成立、有没有反例。

为什么值得验：703 的 8 个候选起点，"到最近终点候选的 L1"**全部等于 1**。
如果缝定律普遍成立，它就能当硬约束用（钉住起点的同时把终点也锁进一个小球里），
而不只是个观察。

⚠️ 只用过官方 coil_check 的解 —— 银行里有脏条目（残缺/坐标 1-indexed/起点在墙上），
拿脏解当标注会得出假结论（我已经在针位测量上栽过一次）。
"""

import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

LEVELS = str(ROOT.parent / "coilbench" / "levels_all")
BANK = str(ROOT.parent / "coil-solutions" / "solutions")
CHECK = str(ROOT.parent / "coilbench" / "coil_check" / "check")
DIRS = {"U": (0, -1), "D": (0, 1), "L": (-1, 0), "R": (1, 0)}


def endpoints(level):
    """跑一遍滑行，返回 (起点, 终点, 自由格数)。不合法就返回 None。"""
    txt = open(f"{LEVELS}/{level}").read().strip()
    m = re.match(r'x=(\d+)&y=(\d+)&board=(.*)', txt, re.S)
    w, h, b = int(m.group(1)), int(m.group(2)), m.group(3).strip()
    s = re.search(r'x=(\d+)&y=(\d+)&path=([UDLR]*)', open(f"{BANK}/{level}.sol").read())
    x, y, path = int(s.group(1)), int(s.group(2)), s.group(3)

    free = b.count(".")
    seen = bytearray(w * h)
    if b[y * w + x] != ".":
        return None
    seen[y * w + x] = 1
    n = 1
    for mv in path:                      # 每个字母 = 滑到撞墙/撞到已访问为止
        dx, dy = DIRS[mv]
        moved = False
        while True:
            nx, ny = x + dx, y + dy
            if not (0 <= nx < w and 0 <= ny < h):
                break
            if b[ny * w + nx] != "." or seen[ny * w + nx]:
                break
            x, y = nx, ny
            seen[y * w + x] = 1
            n += 1
            moved = True
        if not moved:
            return None
    return (int(s.group(1)), int(s.group(2))), (x, y), free, n


def label_ok(level):
    r = subprocess.run([CHECK, f"{LEVELS}/{level}", f"{BANK}/{level}.sol"],
                       capture_output=True, text=True)
    return not (r.stdout.strip() or r.stderr.strip())


if __name__ == "__main__":
    rows = []
    for f in sorted(os.listdir(BANK)):
        lv = f[:-4]
        if not lv.isdigit() or not os.path.exists(f"{LEVELS}/{lv}"):
            continue
        if not label_ok(lv):
            continue
        r = endpoints(lv)
        if not r:
            continue
        (sx, sy), (ex, ey), free, n = r
        rows.append((int(lv), abs(ex - sx) + abs(ey - sy), free))

    if not rows:
        sys.exit("没有可用的合法解")
    rows.sort()
    l1s = sorted(r[1] for r in rows)
    k = len(l1s)
    print(f"合法解样本 {k} 个")
    print(f"L1(起点,终点): 中位 {l1s[k // 2]}  最大 {l1s[-1]}  最小 {l1s[0]}")
    for t in (1, 2, 5, 10, 20, 50):
        c = sum(1 for v in l1s if v <= t)
        print(f"  L1 <= {t:2d}: {c}/{k} = {100.0 * c / k:.1f}%")
    print("最大的 10 个反例（关号, L1, 自由格）:")
    for r in sorted(rows, key=lambda r: -r[1])[:10]:
        print(f"  关 {r[0]}: L1={r[1]} (自由格 {r[2]})")
