#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""梯子 v4：形状探针（假设原子 = 格子的路径形状）

每个自由格的形状 ∈ {方向对 (da,db)}（直穿/拐角）∪ {端点}。
形状假设 (da,db)：edges(c,da)=(c,db)=必用，c 的其余自由边=禁用——一次 3~4 条边。
反驳掉一个形状 ⇒ 从该格形状集删除；只剩一个对形状且格不可为端点 ⇒ 直接定边。
所有对形状全灭 ⇒ 该格必为端点（END 候选，全局至多 1 个非起点端点——2 个即矛盾）。
产出并入 F（真解对拍守护）。"""
import re
import subprocess
import sys
import os
from itertools import combinations

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
DL = {0: -1, 1: -W, 2: 1, 3: W}
edges = [(c, d) for c in sorted(free) for d in (2, 3) if c + DL[d] in free]
eset = set(edges)
print(f"L{LV}: {len(free)} 格 {len(edges)} 边", flush=True)

SOL = open(f"solutions/{LV}.sol").read()
sx = int(re.search(r"x=(\d+)", SOL).group(1)); sy = int(re.search(r"y=(\d+)", SOL).group(1))
START = (sx + 1) + (sy + 1) * W
spath = re.search(r"path=([UDLR]+)", SOL).group(1)
DD2 = {"L": -1, "U": -W, "R": 1, "D": W}
cur = START; vis = {cur}; used = set()
for ch in spath:
    dd = DD2[ch]
    while cur + dd in free and cur + dd not in vis:
        a2, b2 = cur, cur + dd
        used.add((min(a2, b2), 2 if abs(dd) == 1 else 3))
        cur += dd; vis.add(cur)
END = cur
truth = {(c, d): (1 if (c, d) in used else 2) for (c, d) in edges}

def canon(c, d):
    """边 (c,d) 的规范无向表示（d∈0..3 -> 基准格+方向2/3）"""
    if d in (2, 3):
        return (c, d)
    n = c + DL[d]
    return (n, 2 if d == 0 else 3)

def cell_dirs(c):
    return [d for d in range(4) if c + DL[d] in free]

def shape_edges(c, da, db):
    out = {}
    for d in cell_dirs(c):
        e = canon(c, d)
        out[e] = 1 if d in (da, db) else 2
    return out

def run(forced):
    fe = ";".join(f"{c}:{d}:{v}" for ((c, d), v) in forced.items())
    env = {"STRONG": "1", "PROBEROUNDS": "3", "FIXDUMP": "1",
           "XCHAIN": os.environ.get("XCHAIN", "0"), "FORCEEDGES": fe, "PATH": "/usr/bin:/bin"}
    r = subprocess.run(["./bin/v58-probezero", f"/mnt/d/workspace/coilbench/levels_all/{LV}",
                        "--verify", f"solutions/{LV}.sol"],
                       capture_output=True, text=True, timeout=120, env=env,
                       cwd="/mnt/d/workspace/coil-solver")
    return 1 if re.search(r"PROPOK 1", r.stderr) else 0

F = {}
shapes = {}
for c in free:
    ds = cell_dirs(c)
    if c == START:
        shapes[c] = [("S", d) for d in ds]      # 起点：单边形状
    else:
        shapes[c] = list(combinations(ds, 2))
end_cands = set(c for c in free if c != START)

def adopt(e, v, why):
    if truth.get(e) != v:
        print(f"!!! 不可靠（{why}）：{e} 定 {v} 真值 {truth.get(e)}")
        sys.exit(1)
    F[e] = v

calls = 0
rounds = 0
while True:
    rounds += 1
    gained = 0
    for c in sorted(free):
        alive = []
        for sh in shapes[c]:
            if sh[0] == "S":
                hyp = dict(F)
                e = canon(c, sh[1])
                if F.get(e) == 2:
                    continue
                for d in cell_dirs(c):
                    ee = canon(c, d)
                    vv = 1 if d == sh[1] else 2
                    if F.get(ee, vv) != vv:
                        break
                    hyp[ee] = vv
                else:
                    calls += 1
                    if run(hyp):
                        alive.append(sh)
                continue
            da, db = sh
            se = shape_edges(c, da, db)
            if any(F.get(e2, v2) != v2 for (e2, v2) in se.items()):
                continue
            hyp = dict(F); hyp.update(se)
            calls += 1
            if run(hyp):
                alive.append(sh)
        old = shapes[c]
        shapes[c] = alive
        if c != START:
            pair_alive = [s for s in alive]
            if len(pair_alive) == 0:
                if c in end_cands:
                    pass                                    # 必为端点
                else:
                    print(f"!!! 格 {c} 无形状且非端点候选——矛盾")
                    sys.exit(1)
            elif len(pair_alive) == 1 and c not in (END,):
                # 只剩一个对形状：若 c 确非终点才可定——保守：仅当 c 已被排除出端点候选
                if c not in end_cands:
                    (da, db) = pair_alive[0]
                    for (e2, v2) in shape_edges(c, da, db).items():
                        if e2 in eset and e2 not in F:
                            adopt(e2, v2, "唯一形状")
                            gained += 1
        # 端点排除：若存在活的对形状（能作中间格）——不能排除；
        # 排除端点候选的条件：假设「c 的度=1」（每个单边形状）全部被反驳
        if c != START and c in end_cands:
            single_ok = False
            for d in cell_dirs(c):
                hyp = dict(F)
                bad = False
                for d2 in cell_dirs(c):
                    ee = canon(c, d2)
                    vv = 1 if d2 == d else 2
                    if F.get(ee, vv) != vv:
                        bad = True
                        break
                    hyp[ee] = vv
                if bad:
                    continue
                calls += 1
                if run(hyp):
                    single_ok = True
                    break
            if not single_ok:
                end_cands.discard(c)
    print(f"第{rounds}轮：+{gained}，F={len(F)}/{len(edges)}（{100.0*len(F)/len(edges):.0f}%），端点候选剩 {len(end_cands)}，调用 {calls}", flush=True)
    if gained == 0 and rounds > 1:
        break
print(f"最终确定率 {100.0*len(F)/len(edges):.1f}%，端点候选 {len(end_cands)}（真终点 {END} {'在内' if END in end_cands else '不在?!'}），调用 {calls}")
