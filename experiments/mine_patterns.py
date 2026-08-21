#!/usr/bin/env python3
"""从真解反推「局部模式 => 某个进入方向不可能」的规则。

动机：Tron（唯一通关者）说他的核心是消除「the field at y,x will not be entered from the left side」，
预处理后只剩 0.3% 的格子不确定用哪两条边；我们只判定 ~48%。他管这叫
local patterns / recursive patterns / bounded brute force，但从未展开。

思路：我们有真解，那就**从答案反推**——统计每个局部墙形下，各个进入方向实际出现的频次。
某个方向在大量样本里**一次都没出现**，就是一条候选规则（再人工验证它是不是可证的）。

窗口取以格子为中心的 5x5 里的「墙/空」布局（中心必为空），按 8 种对称归一化。
**必须做留出验证**：19314 种形状 x 4 个方向 ≈ 7 万次检验，光靠挖掘集上「0 次」很可能是
多重检验的噪声。所以按关号奇偶把关卡分成挖掘集/留出集，只有在**没参与挖掘**的关上也 0 次，
才算候选规则活下来。

用法: mine_patterns.py [窗口边长 3|5] [最少支持数]
"""
import os
import sys
from collections import defaultdict

LV = "/mnt/d/works/coilbench/levels_public/%s"
SOL = "/tmp/sols/%s"
D = {"L": (-1, 0), "R": (1, 0), "U": (0, -1), "D": (0, 1)}
DIRS = [(-1, 0), (0, -1), (1, 0), (0, 1)]        # L U R D，与求解器一致
DNAME = "LURD"


def load(n):
    s = open(LV % n).read().strip()
    w = int(s.split("x=")[1].split("&")[0])
    h = int(s.split("y=")[1].split("&")[0])
    b = s.split("board=")[1]
    return w, h, b


def walk(n):
    """回放真解，产出 (格子, 进入方向idx, 离开方向idx)。起点没有进入方向，终点没有离开方向。"""
    w, h, b = load(n)
    free = lambda x, y: 0 <= x < w and 0 <= y < h and b[y * w + x] == "."
    sol = open(SOL % n).read().strip()
    if "path=" not in sol:
        return None
    x = int(sol.split("x=")[1].split("&")[0])
    y = int(sol.split("y=")[1].split("&")[0])
    path = sol.split("path=")[1]
    seq = [(x, y)]
    seen = {(x, y)}
    for mv in path:
        dx, dy = D[mv]
        while free(x + dx, y + dy) and (x + dx, y + dy) not in seen:
            x, y = x + dx, y + dy
            seq.append((x, y)); seen.add((x, y))
    out = []
    for i, c in enumerate(seq):
        din = DIRS.index((c[0] - seq[i - 1][0], c[1] - seq[i - 1][1])) if i > 0 else -1
        dout = DIRS.index((seq[i + 1][0] - c[0], seq[i + 1][1] - c[1])) if i + 1 < len(seq) else -1
        out.append((c, din, dout))
    return w, h, b, out


def rot(dx, dy):        # 顺时针 90 度
    return (-dy, dx)


def canon(pat, k):
    """pat: k*k 的 0/1 元组（1=空）。返回 (归一化后的模式, 把原方向映射到归一化坐标系的置换)。"""
    best = None
    m = k // 2
    for flip in (False, True):
        cur = [[pat[(y * k) + x] for x in range(k)] for y in range(k)]
        if flip:
            cur = [row[::-1] for row in cur]
        for r in range(4):
            flat = tuple(v for row in cur for v in row)
            # 方向映射：原方向 (dx,dy) 先按 flip 再按 r 次旋转
            perm = []
            for (dx, dy) in DIRS:
                nx, ny = (-dx, dy) if flip else (dx, dy)
                for _ in range(r):
                    nx, ny = rot(nx, ny)
                perm.append(DIRS.index((nx, ny)))
            cand = (flat, tuple(perm))
            if best is None or cand[0] < best[0]:
                best = cand
            cur = [[cur[k - 1 - x][y] for x in range(k)] for y in range(k)]   # 转 90 度
    return best


def main():
    k = int(sys.argv[1]) if len(sys.argv) > 1 else 3
    minsup = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    m = k // 2
    stats = defaultdict(lambda: [0, 0, 0, 0])     # 挖掘集：归一化模式 -> 各进入方向计数
    hold = defaultdict(lambda: [0, 0, 0, 0])      # 留出集：同上
    total = 0
    levels = ntrain = nhold = 0
    names = sorted((z for z in os.listdir("/tmp/sols") if os.path.getsize("/tmp/sols/" + z) > 0),
                   key=lambda z: int(z))
    for idx, n in enumerate(names):
        if not os.path.exists(LV % n):
            continue
        r = walk(n)
        if not r:
            continue
        w, h, b, cells = r
        # SPLIT=<关号>：小于它的当挖掘集，其余当留出集 —— 用来检验规则能否**跨规模**泛化
        split = int(os.environ.get("SPLIT", "0"))
        is_train = (int(n) < split) if split else (idx % 2 == 0)
        tgt = stats if is_train else hold
        levels += 1
        if is_train: ntrain += 1
        else: nhold += 1
        free = lambda x, y: 0 <= x < w and 0 <= y < h and b[y * w + x] == "."
        for (c, din, dout) in cells:
            if din < 0:
                continue
            x, y = c
            pat = tuple(1 if free(x + dx, y + dy) else 0
                        for dy in range(-m, m + 1) for dx in range(-m, m + 1))
            cp, perm = canon(pat, k)
            tgt[cp][perm[din]] += 1
            total += 1

    print(f"样本：{levels} 关（挖掘 {ntrain} / 留出 {nhold}），{total} 个「进入」事件，"
          f"{len(stats)} 种 {k}x{k} 归一化局部形状")
    print()
    hits = []
    for pat, cnt in stats.items():
        sup = sum(cnt)
        if sup < minsup:
            continue
        # 这个形状下，哪些方向的**格子是空的**（物理上能进来）却一次都没出现过
        for d in range(4):
            dx, dy = DIRS[d]
            src = ((m - dx) + (m - dy) * k)          # 进入方向 d 的前驱在中心的反方向
            if not pat[src]:
                continue                              # 前驱是墙，物理上就进不来，不算发现
            if cnt[d] == 0:
                hits.append((sup, pat, d, cnt))
    hits.sort(reverse=True)
    print(f"挖掘集上的候选规则（前驱是空格、{minsup}+ 样本里 0 次）：{len(hits)} 条")
    print()
    survived = killed = nodata = 0
    for sup, pat, d, cnt in hits:
        hc = hold.get(pat)
        if hc is None or sum(hc) == 0:
            nodata += 1
            verdict = "留出集无样本，判不了"
        elif hc[d] == 0:
            survived += 1
            verdict = f"**留出集也 0 次**（留出支持 {sum(hc)}）—— 活下来"
        else:
            killed += 1
            verdict = f"留出集出现 {hc[d]} 次（留出支持 {sum(hc)}）—— 证伪"
        if hc is not None and sum(hc) and hc[d] == 0 and survived <= 6:
            print(f"\n  [规则 {survived}] 挖掘支持 {sup} + 留出支持 {sum(hc)}，"
                  f"**从 {DNAME[d]} 侧进入 0 次**")
            print(f"       挖掘集各方向 {cnt}，留出集 {hc}")
            for yy in range(k):
                row = "".join("##" if not pat[yy * k + xx] else
                              (" ●" if (xx == m and yy == m) else " ·") for xx in range(k))
                print("       " + row)
    print()
    print(f"留出验证：活下来 {survived}，被证伪 {killed}，留出集没样本 {nodata}")


if __name__ == "__main__":
    main()
