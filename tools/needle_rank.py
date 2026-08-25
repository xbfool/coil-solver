#!/usr/bin/env python3
"""量「针排第几」：已知解的关卡里，真起点在漏斗 F 排序中的名次。

出发点（家里那台的结论 + 664 的纠错都指向同一件事）：
    赢家树很便宜（664 实测仅 29 万节点），贵的是**找到它**。
    "让小树针早点被够到" 才是真问题。
但"针排第几"这件事**从来没被量过** —— 而我们手里有 800 个已知解的关卡，
每个 .sol 里的 x=&y= 就是那一关的真起点，这是一份现成的标注数据集。

做法：对每关跑 PROBEDUMP（逐起点传播普查，输出 `OK <格号> <F>` / `REF <格号>`），
按 F 升序排名，找出真起点的名次和百分位。

看什么：
  · 百分位若随盘面增大而稳定 => 针的绝对名次随规模线性增长，这就是高段变慢的直接原因；
  · 百分位若在 HARD 关附近急剧恶化 => F 信号在那里失效，需要换排序特征。
"""

import os
import re
import subprocess
import sys

SOLVER = "/mnt/d/works/coil-solver/bin/v58-probezero"
LEVELS = "/mnt/d/works/coilbench/levels_all"
BANK = "/mnt/d/works/coil-solutions/solutions"


def needle_cell(level):
    """真起点的内部格号。W = 盘宽 + 2（四周补墙），格号 = (y+1)*W + (x+1)。"""
    head = open(f"{LEVELS}/{level}").read(64)
    w = int(re.search(r'x=(\d+)', head).group(1))
    m = re.search(r'x=(\d+)&y=(\d+)', open(f"{BANK}/{level}.sol").read())
    x, y = int(m.group(1)), int(m.group(2))
    return (y + 1) * (w + 2) + (x + 1), (x, y)


def measure(level, timeout):
    cell, xy = needle_cell(level)
    try:
        out = subprocess.run([SOLVER, f"{LEVELS}/{level}"], env={**os.environ, "PROBEDUMP": "1"},
                             capture_output=True, text=True, timeout=timeout).stdout
    except subprocess.TimeoutExpired:
        return f"关 {level}: PROBEDUMP 超时（>{timeout}s）"

    survivors, refuted = [], set()
    for line in out.splitlines():
        p = line.split()
        if p and p[0] == "OK":
            survivors.append((int(p[2]), int(p[1])))     # (F, 格号)
        elif p and p[0] == "REF":
            refuted.add(int(p[1]))

    if not survivors:
        return f"关 {level}: 普查没有幸存者（异常）"
    if cell in refuted:
        return f"关 {level}: **真起点被证伪了** —— 传播不可靠，这是重大发现"

    survivors.sort()                                      # F 升序 = 现在的排序信号
    rank = next((i for i, (_, c) in enumerate(survivors) if c == cell), None)
    if rank is None:
        return f"关 {level}: 真起点 {xy} 既不在幸存者也不在证伪表里（普查未覆盖？）"
    n = len(survivors)
    return (f"关 {level}: 针 {xy} 排 {rank + 1}/{n} = 前 {100.0 * (rank + 1) / n:.2f}%"
            f"（证伪 {len(refuted)}）")


if __name__ == "__main__":
    tmo = int(os.environ.get("TIMEOUT", "3600"))
    for lv in sys.argv[1:]:
        print(measure(lv, tmo), flush=True)
