#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""梯子 v2：数独式交集推理（either-way common outcomes）

对每条未定边 e：
  分支1 = 传播(F + e:必用) → (活?, 确定集 D1)
  分支2 = 传播(F + e:禁用) → (活?, 确定集 D2)
  两支都死 ⇒ 矛盾（真起点下不应发生）
  一支死   ⇒ e 定为另一支，且**那一支的整个确定集 D 无条件并入 F**（老版只收 e 本身）
  两支都活 ⇒ **D1∩D2 并入 F**（不管 e 取哪边都成立——数独 either-way 的移植）
可靠性：每条并入 F 的边即时对拍真解，冲突即报错退出。
"""
import re
import subprocess
import sys

LV = sys.argv[1] if len(sys.argv) > 1 else "22"

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
DELTA = {2: 1, 3: W}
edges = [(c, d) for c in sorted(free) for d in (2, 3) if c + DELTA[d] in free]
print(f"L{LV}: {len(free)} 格 {len(edges)} 边", flush=True)

SOL = open(f"solutions/{LV}.sol").read()
sx = int(re.search(r"x=(\d+)", SOL).group(1))
sy = int(re.search(r"y=(\d+)", SOL).group(1))
spath = re.search(r"path=([UDLR]+)", SOL).group(1)
DD2 = {"L": -1, "U": -W, "R": 1, "D": W}
cur = (sx + 1) + (sy + 1) * W
vis = {cur}
used = set()
for ch in spath:
    dd = DD2[ch]
    while cur + dd in free and cur + dd not in vis:
        a2, b2 = cur, cur + dd
        used.add((min(a2, b2), 2 if abs(dd) == 1 else 3))
        cur += dd
        vis.add(cur)
truth = {(c, d): (1 if (c, d) in used else 2) for (c, d) in edges}

def run(forced):
    fe = ";".join(f"{c}:{d}:{v}" for ((c, d), v) in forced.items())
    env = {"STRONG": "1", "PROBEROUNDS": "3", "XCHAIN": __import__("os").environ.get("XCHAIN", "0"), "FIXDUMP": "1", "FORCEEDGES": fe, "PATH": "/usr/bin:/bin"}
    r = subprocess.run(["./bin/v58-probezero", f"/mnt/d/workspace/coilbench/levels_all/{LV}",
                        "--verify", f"solutions/{LV}.sol"],
                       capture_output=True, text=True, timeout=180, env=env,
                       cwd="/mnt/d/workspace/coil-solver")
    ok = bool(re.search(r"PROPOK 1", r.stderr))
    D = {}
    if ok:
        for m in re.finditer(r"FIX (\d+) (\d+) (\d+)", r.stderr):
            D[(int(m.group(1)), int(m.group(2)))] = int(m.group(3))
    return ok, D

def adopt(e, v, why):
    if truth.get(e) != v:
        print(f"!!! 不可靠（{why}）：边 {e} 定 {v} 真值 {truth.get(e)}")
        sys.exit(1)
    F[e] = v

F = {}
calls = 0
rounds = 0
while True:
    rounds += 1
    gained = 0
    for e in list(edges):
        if e in F:
            continue
        h1 = dict(F); h1[e] = 1
        h2 = dict(F); h2[e] = 2
        ok1, D1 = run(h1)
        ok2, D2 = run(h2)
        calls += 2
        if not ok1 and not ok2:
            print(f"!!! 双死于 {e}——起点被证伪?!")
            sys.exit(1)
        if not ok1:
            for (ee, vv) in D2.items():
                if ee in edges and ee not in F:
                    adopt(ee, vv, "单死全收")
                    gained += 1
        elif not ok2:
            for (ee, vv) in D1.items():
                if ee in edges and ee not in F:
                    adopt(ee, vv, "单死全收")
                    gained += 1
        else:
            for ee in D1:
                if ee == e or ee not in edges or ee in F:
                    continue
                if ee in D2 and D1[ee] == D2[ee]:
                    adopt(ee, D1[ee], "交集")
                    gained += 1
    print(f"第{rounds}轮：+{gained}，累计 {len(F)}/{len(edges)}（{100.0*len(F)/len(edges):.0f}%），调用 {calls}", flush=True)
    if gained == 0:
        break
print(f"最终确定率 {100.0*len(F)/len(edges):.1f}%，调用 {calls}，可靠性全程对拍通过")
