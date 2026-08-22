#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""多项式梯子驱动器：以 C 求解器为可注入传播内核，在外面搭 N 级探针。

用法：python3 ladder.py <level> [level2]
一级：对每条未定边 e、每个取值 v，注入 (已知集F + e:v)，PROPOK=0 ⇒ 定为反值。滚到不动点。
二级（level2 参数开启）：一级不动点后，对每个存活假设 (e,v)：
  在 (F + e:v) 下扫其它未定边 e2——若某 e2 两个取值都 PROPOK=0 ⇒ (e,v) 证伪 ⇒ e 定为反值。
知识集 F 全程累积并注入。真解对拍：F 不得与真解冲突（冲突=不可靠，立刻报警）。
"""
import re
import subprocess
import sys

LV = sys.argv[1] if len(sys.argv) > 1 else "16"
DO_L2 = len(sys.argv) > 2

raw = open(f"/mnt/d/workspace/coilbench/levels_all/{LV}").read()
w = int(re.search(r"x=(\d+)", raw).group(1))
h = int(re.search(r"y=(\d+)", raw).group(1))
b = re.search(r"board=([.X]+)", raw).group(1)
W = w + 2
free = set()
for y in range(h):
    for x in range(w):
        if b[y * w + x] == ".":
            free.add((x + 1) + (y + 1) * W)
DELTA = {2: 1, 3: W}          # R, D（无向边只枚举这两个方向）
edges = []
for c in sorted(free):
    for d in (2, 3):
        if c + DELTA[d] in free:
            edges.append((c, d))
print(f"L{LV}: {len(free)} 格 {len(edges)} 边")

SOL = open(f"solutions/{LV}.sol").read()
sx = int(re.search(r"x=(\d+)", SOL).group(1))
sy = int(re.search(r"y=(\d+)", SOL).group(1))
spath = re.search(r"path=([UDLR]+)", SOL).group(1)
DD2 = {"L": -1, "U": -W, "R": 1, "D": W}
cur = (sx + 1) + (sy + 1) * W
vis = {cur}
used_edges = set()
for ch in spath:
    dd = DD2[ch]
    while cur + dd in free and cur + dd not in vis:
        a, bb = cur, cur + dd
        e = (min(a, bb), 2 if abs(dd) == 1 else 3) if dd > 0 else (min(a, bb), 2 if abs(dd) == 1 else 3)
        used_edges.add((min(a, bb), 2 if abs(dd) == 1 else 3))
        cur = cur + dd
        vis.add(cur)
truth = {}
for (c, d) in edges:
    truth[(c, d)] = 1 if (c, d) in used_edges else 2

def run(forced):
    fe = ";".join(f"{c}:{d}:{v}" for ((c, d), v) in forced.items())
    env = {"STRONG": "1", "PROBEROUNDS": "3", "XCHAIN": __import__("os").environ.get("XCHAIN", "0"), "FORCEEDGES": fe, "PATH": "/usr/bin:/bin"}
    r = subprocess.run(["./bin/v58-probezero", f"/mnt/d/workspace/coilbench/levels_all/{LV}",
                        "--verify", f"solutions/{LV}.sol"],
                       capture_output=True, text=True, timeout=120, env=env,
                       cwd="/mnt/d/workspace/coil-solver")
    m = re.search(r"PROPOK (\d)", r.stderr)
    return int(m.group(1)) if m else 0

F = {}
calls = 0

def probe_round(l2=False):
    global calls
    newly = 0
    for e in edges:
        if e in F:
            continue
        for v in (1, 2):
            hyp = dict(F)
            hyp[e] = v
            calls += 1
            if run(hyp) == 0:
                F[e] = 3 - v
                if truth[e] != F[e]:
                    print(f"!!! 不可靠：边 {e} 被定为 {F[e]} 但真解是 {truth[e]}")
                    sys.exit(1)
                newly += 1
                break
            elif l2:
                # 二级：在 (F+e:v) 下找「某边两值皆死」
                refuted = False
                for e2 in edges:
                    if e2 in hyp or e2 == e:
                        continue
                    dead = 0
                    for v2 in (1, 2):
                        h2 = dict(hyp)
                        h2[e2] = v2
                        calls += 1
                        if run(h2) == 0:
                            dead += 1
                    if dead == 2:
                        refuted = True
                        break
                if refuted:
                    F[e] = 3 - v
                    if truth[e] != F[e]:
                        print(f"!!! 二级不可靠：边 {e} 定 {F[e]} 真值 {truth[e]}")
                        sys.exit(1)
                    newly += 1
                    break
    return newly

rounds = 0
while True:
    rounds += 1
    n1 = probe_round(l2=False)
    print(f"一级第{rounds}轮：+{n1}，累计 {len(F)}/{len(edges)}（{100.0*len(F)/len(edges):.0f}%），调用 {calls}")
    if n1 == 0:
        break
if DO_L2:
    while True:
        rounds += 1
        n2 = probe_round(l2=True)
        print(f"二级第{rounds}轮：+{n2}，累计 {len(F)}/{len(edges)}（{100.0*len(F)/len(edges):.0f}%），调用 {calls}")
        if n2 == 0:
            break
print(f"最终确定率 {100.0*len(F)/len(edges):.1f}%（{len(F)}/{len(edges)}），可靠性全程对拍通过")
