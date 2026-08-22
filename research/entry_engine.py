#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""滑动感知的定向进入一致性引擎（原型 v1，2026-08-22 夜）

对象：ban[c][d] = 「路径从 d 侧滑入格 c」不可能。
判定：窗口一致性——以 c 为中心的 (2R+1)² 窗口内，穷举「穿过 c 的局部滑动片段」，
片段必须满足：
  · 进入 c 的方向是 d（即 c 的前驱是 c-d，且 c-d..c 沿 d 连续）
  · 片段在窗口内的每次停步（转弯点）q（沿 dir 停）满足：q+dir 是墙/边界，
    或 q+dir 在窗口外（保守放行），或 q+dir 是自由格（轨迹垫背——时序保守放行）
    ——v1 先只做「结构可行性」：转弯的直角几何 + 已知 ban 的递归消费：
    片段里每一步「从 dir 进入格 q」若已被 ban ⇒ 此图景非法。
  · 片段两端伸到窗口边界或在窗口内合法终止（终止=端点，全局至多 2 个——v1 放行）
迭代：ban 集扩大 → 邻居图景减少 → 重扫，到不动点。
可靠性(soundness)校验：真解的每次进入 (q,dir) 不得被 ban。
产出指标：ban 率、每格「唯一进入结构」率——向 Tron 的 99.7% 逼近的第一步。

v1 的诚实预期：不带时序记账时可能杀伤有限，先把管线+验尸建好，
规则逐条往里加（这是从数据爬向定理的脚手架）。
"""
import re
import sys
from collections import deque

DD = {0: (1, 0), 1: (-1, 0), 2: (0, 1), 3: (0, -1)}   # R L D U
DNAME = "RLDU"

def load(path):
    raw = open(path).read()
    w = int(re.search(r"x=(\d+)", raw).group(1))
    h = int(re.search(r"y=(\d+)", raw).group(1))
    b = re.search(r"board=([.X]+)", raw).group(1)
    free = set((x, y) for y in range(h) for x in range(w) if b[y * w + x] == ".")
    return w, h, free

class Engine:
    def __init__(self, w, h, free, radius=3):
        self.w, self.h, self.free = w, h, free
        self.R = radius
        # ban[(c,d)]：从 d 方向滑入 c 不可能（d 是位移编号）
        self.ban = set()

    def inb(self, c):
        return c in self.free

    def entry_ok_local(self, c, d):
        """窗口一致性：存在一个合法的局部图景使「从 d 滑入 c」发生。
        v1 图景 = 三元组（进入滑行的入段长, 停在哪, 停后转向哪）在窗口内自洽：
          1) 入段：c-d 必须自由（滑入需要来路），且进入 c-d 的方向若全被 ban 则需要
             c-d 本身是「更早滑行的转弯起点」——v1 简化：入段只查一步（c-d 自由 +
             进入 c-d 沿 d 未被 ban 或 c-d 可以是转弯/起点）。
          2) c 的去向：要么继续沿 d（则 c+d 必须自由且 (c+d, d) 未被 ban），
             要么在 c 停（则 c+d 是墙/边界【结构硬backing】，或 c+d 自由【轨迹垫背，
             时序放行】），停后转垂直方向 p 或终止：
             转 p 需要 c+p 自由且 (c+p, p) 未被 ban。
        返回 True = 至少一个图景成立。"""
        dx, dy = DD[d]
        prev = (c[0] - dx, c[1] - dy)
        if not self.inb(prev):
            return False                       # 从 d 滑入需要来路自由
        # 来路合法性：prev 可以是（a）沿 d 更早滑入（(prev,d) 未 ban），或
        # （b）prev 是转弯点/起点（有某个垂直方向 q 使 (prev,q) 未 ban，或 prev 可为起点——放行）
        ok_prev = ((prev, d) not in self.ban)
        if not ok_prev:
            for q in range(4):
                qx, qy = DD[q]
                if (qx, qy) == (dx, dy) or (qx, qy) == (-dx, -dy):
                    continue
                if self.inb((prev[0] - qx, prev[1] - qy)) and (prev, q) not in self.ban:
                    ok_prev = True
                    break
            # prev 也可能是全局起点（每关只有一个）——v1 放行由调用端统计
            # 这里不放行：起点情形单独枚举（否则 ban 恒空）。
        if not ok_prev:
            return False
        nxt = (c[0] + dx, c[1] + dy)
        # 图景 A：穿过 c 继续滑
        if self.inb(nxt) and (nxt, d) not in self.ban:
            return True
        # 图景 B：停在 c（结构上总可停：墙硬 backing 或自由格软 backing）→ 转垂直或终止
        for p in range(4):
            px, py = DD[p]
            if (px, py) == (dx, dy) or (px, py) == (-dx, -dy):
                continue
            t = (c[0] + px, c[1] + py)
            if self.inb(t) and (t, p) not in self.ban:
                return True
        # 终止（c 是端点）：全局至多 2 个——v1 放行（统计时另计）
        return True

    def run(self):
        changed = True
        rounds = 0
        while changed:
            changed = False
            rounds += 1
            for c in self.free:
                for d in range(4):
                    if (c, d) in self.ban:
                        continue
                    if not self.entry_ok_local(c, d):
                        self.ban.add((c, d))
                        changed = True
        return rounds

def verify(engine, levelpath, soltext):
    sx = int(re.search(r"x=(\d+)", soltext).group(1))
    sy = int(re.search(r"y=(\d+)", soltext).group(1))
    path = re.search(r"path=([UDLR]+)", soltext).group(1)
    D2 = {"R": 0, "L": 1, "D": 2, "U": 3}
    vis = {(sx, sy)}
    cur = (sx, sy)
    alarms = 0
    for ch in path:
        d = D2[ch]
        dx, dy = DD[d]
        while (cur[0] + dx, cur[1] + dy) in engine.free and (cur[0] + dx, cur[1] + dy) not in vis:
            cur = (cur[0] + dx, cur[1] + dy)
            vis.add(cur)
            if (cur, d) in engine.ban:
                alarms += 1
    return alarms

def main():
    L = sys.argv[1] if len(sys.argv) > 1 else "101"
    w, h, free = load(f"/mnt/d/workspace/coilbench/levels_all/{L}")
    eng = Engine(w, h, free)
    rounds = eng.run()
    total = len(free) * 4
    # 只统计「几何上可能」的进入（来路自由）
    possible = sum(1 for c in free for d in range(4)
                   if (c[0] - DD[d][0], c[1] - DD[d][1]) in free)
    nban = len(eng.ban & set((c, d) for c in free for d in range(4)
                             if (c[0] - DD[d][0], c[1] - DD[d][1]) in free))
    print(f"L{L}: 自由格 {len(free)}，几何可能进入 {possible}，ban {nban}（{100.0*nban/possible:.1f}%），迭代 {rounds} 轮")
    solpath = f"solutions/{L}.sol"
    try:
        alarms = verify(eng, f"/mnt/d/workspace/coilbench/levels_all/{L}", open(solpath).read())
        print(f"可靠性(soundness)：真解 {alarms} 次报警 —— {'PASS' if alarms == 0 else '**FAIL**'}")
    except FileNotFoundError:
        print("（无缓存解，跳过验尸）")

if __name__ == "__main__":
    main()
