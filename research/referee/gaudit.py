#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
g_estate 全局底座审计（gaudit）—— 767 案头号嫌疑的跨实现对拍（2026-08-26）

模型（偶盘全局相，§2.2）：SRC→色0格 cap2；色1格→SNK cap2；
  哑点1: SRC→D1 cap1, D1→每个色1格 cap1（色1侧端点少收 1 条真边）；
  哑点2: 每个色0格→D2 cap1, D2→SNK cap1（色0侧端点少发 1 条真边）。
审计两步：
  1) 批量核销：裁判自己跑一次 Régin（maxflow+强SCC），凡 C 声明与裁判结论一致者 → 核销；
  2) 残余逐边反证：C 声明"禁用"→ 测「存在用它的可行子图」（强制 1 单位过边）；
     声明"必用"→ 测「存在避开它的可行子图」（删边）。可行 ⇒ 该声明**流层面无法辩护**
     （可能由 probing/分治/subtour/有向层辩护，也可能就是毒 —— 进嫌疑清单人工/深挖）。
  end_ok=0 声明（E 行）：测「c 当色相应端点」可行性（哑弧钉到 c）。

用法：gaudit.py <board> <gdump文件> [残余逐边上限=400]
"""
import sys, re
from collections import deque
import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import maximum_flow, connected_components
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from referee import Board


def build(bd, skip_edge=None, force_edge=None, pin_end=None):
    """全局双哑点网络。返回 (cap, needL, idx, nn, edge_map)。
    skip_edge=(c,d) 删边；force_edge=(c,d) 强制用边(两端各扣1容量+删边,目标-1)；
    pin_end=(cell,) 把该格钉成端点（其颜色侧哑弧只连它）。"""
    cells = bd.cells
    idx = {c: i + 4 for i, c in enumerate(cells)}     # 0=SRC 1=SNK 2=D1 3=D2
    nn = len(cells) + 4
    rows, cols, caps = [], [], []
    def arc(u, v, cp):
        rows.append(u); cols.append(v); caps.append(cp)
    dec0 = dec1 = None
    if force_edge is not None:
        c, d = force_edge
        n = c + bd.delta[d]
        c0 = c if bd.col[c] == 0 else n
        c1 = n if bd.col[c] == 0 else c
        dec0, dec1 = c0, c1
    n0 = sum(1 for c in cells if bd.col[c] == 0)
    needL = 2 * n0 + 1
    for c in cells:
        cap2 = 2 - (1 if c == dec0 or c == dec1 else 0)
        if bd.col[c] == 0: arc(0, idx[c], cap2)
        else: arc(idx[c], 1, cap2)
    if force_edge is not None: needL -= 1
    arc(0, 2, 1)
    for c in cells:
        if bd.col[c] == 1 and (pin_end is None or bd.col[pin_end] != 1 or c == pin_end):
            arc(2, idx[c], 1)
    arc(3, 1, 1)
    for c in cells:
        if bd.col[c] == 0 and (pin_end is None or bd.col[pin_end] != 0 or c == pin_end):
            arc(idx[c], 3, 1)
    emap = {}
    for c in cells:
        if bd.col[c] != 0: continue
        for d in range(4):
            n = c + bd.delta[d]
            if not bd.free[n]: continue
            cd = (c, d) if d in (2, 3) else (n, d ^ 2)
            if skip_edge is not None and cd == skip_edge: continue
            if force_edge is not None and cd == (force_edge if force_edge[1] in (2, 3) else force_edge): pass
            emap[cd] = (idx[c], idx[n])
            arc(idx[c], idx[n], 1)
    cap = coo_matrix((caps, (rows, cols)), shape=(nn, nn)).tocsr()
    cap.sum_duplicates()
    return cap, needL, idx, nn, emap


def canon(bd, c, d):
    return (c, d) if d in (2, 3) else (c + bd.delta[d], d ^ 2)


def main():
    bd = Board(sys.argv[1])
    cap_lim = int(sys.argv[3]) if len(sys.argv) > 3 else 400
    claims_e, claims_end = [], []
    for line in open(sys.argv[2]):
        m = re.match(r"G (\d+) (\d+) (\d+)", line)
        if m: claims_e.append((int(m.group(1)), int(m.group(2)), int(m.group(3))))
        m = re.match(r"E (\d+)", line)
        if m: claims_end.append(int(m.group(1)))
    print(f"C 声明: 边 {len(claims_e)} 条, end_ok=0 {len(claims_end)} 格")

    # 第 1 步：裁判自己的全局 Régin
    cap, needL, idx, nn, emap = build(bd)
    res = maximum_flow(cap, 0, 1)
    assert res.flow_value == needL, f"全局松弛不可行?! {res.flow_value}<{needL}"
    F = res.flow.tocsr()
    R = (cap - F)
    rr, rc = (R > 0).nonzero()
    radj = coo_matrix((np.ones(len(rr), dtype=np.int8), (rr, rc)), shape=(nn, nn)).tocsr()
    _, scc = connected_components(radj, directed=True, connection='strong')
    Fd = F.todok()
    my = {}
    for cd, (u, v) in emap.items():
        if scc[u] != scc[v]:
            my[cd] = 1 if Fd.get((u, v), 0) > 0 else 2
    print(f"裁判全局 Régin 自推: {sum(1 for v in my.values() if v==1)} 必用 + "
          f"{sum(1 for v in my.values() if v==2)} 禁用")

    agree = residue = conflict = 0
    res_list = []
    for (c, d, v) in claims_e:
        cd = canon(bd, c, d)
        mv = my.get(cd)
        if mv == v: agree += 1
        elif mv is not None and mv != v: conflict += 1; print(f"‼ 直接冲突: 边{bd.xy(cd[0])}d{cd[1]} C={v} 裁判={mv}")
        else: residue += 1; res_list.append((cd, v))
    print(f"批量核销 {agree}, 直接冲突 {conflict}, 残余待逐边反证 {residue}")

    unjust = 0
    for i, (cd, v) in enumerate(res_list[:cap_lim]):
        if v == 2:
            cap2, needL2, *_ = build(bd, force_edge=cd)
            ok = maximum_flow(cap2, 0, 1).flow_value == needL2
        else:
            cap2, needL2, *_ = build(bd, skip_edge=cd)
            ok = maximum_flow(cap2, 0, 1).flow_value == needL2
        if ok:
            unjust += 1
            if unjust <= 15:
                print(f"⚠ 流层面无法辩护: 边{bd.xy(cd[0])}d{cd[1]} 声明={'必用' if v==1 else '禁用'}")
    n_test = min(len(res_list), cap_lim)
    print(f"残余逐边: 测 {n_test}, 流层面无法辩护 {unjust} "
          f"(这些由更高层规则产生——或就是毒,进嫌疑清单)")

    bad_end = 0
    for c in claims_end[:cap_lim]:
        cap2, needL2, *_ = build(bd, pin_end=c)
        if maximum_flow(cap2, 0, 1).flow_value == needL2:
            bad_end += 1
            if bad_end <= 10: print(f"⚠ end_ok=0 流层面无法辩护: 格{bd.xy(c)}")
    print(f"end_ok 声明: 测 {min(len(claims_end),cap_lim)}, 无法辩护 {bad_end}")
    print("== 审计完（'直接冲突'=实锤毒; '无法辩护'=嫌疑清单,需比对更高层规则）==")


if __name__ == "__main__":
    main()
