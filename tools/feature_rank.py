#!/usr/bin/env python3
"""在已知解的关卡上比较各候选排序特征：谁能把「针」排到最前面。

背景：needle_rank.py 量出现用的 F 信号百分位平均 46%（13 关），与均匀随机无法区分 ——
排序是假的，这才是高段硬墙的病灶（L500 要处理 16532 个幸存者才够到针）。
银行里 800 个已知解是现成的标注集，于是"找针"从玄学变成统计问题。

用 bin/v72-featdump 的 PROBEDUMP（STRONG=1）逐起点吐：
    OK <格号> <F> <度数> <起点必用边数> <终点候选数> <到最近终点候选的L1>
对每个特征（升序/降序都试）算真起点的百分位。**越小越好**，50% = 随机 = 没用。
"""

import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

SOLVER = str(ROOT / "bin" / "v72-featdump")
LEVELS = str(ROOT.parent / "coilbench" / "levels_all")
BANK = str(ROOT.parent / "coil-solutions" / "solutions")

CHECK = str(ROOT.parent / "coilbench" / "coil_check" / "check")


def label_ok(level):
    """标签闸：银行里有脏解（direction is blocked / start position is blocked /
    残缺条目 / 低关号 1-indexed）。拿它当标注集之前必须逐个过官方校验器 ——
    我就是没做这一步，13 个样本里混进了 4 个假针。"""
    r = subprocess.run([CHECK, f"{LEVELS}/{level}", f"{BANK}/{level}.sol"],
                       capture_output=True, text=True)
    return not (r.stdout.strip() or r.stderr.strip())

FEATS = ["F", "度数", "起点必用边", "终点候选数", "到候选L1"]


def run(level, timeout):
    if not label_ok(level):
        return None, f"关 {level}: 银行里的解不合法，跳过（不能当标注用）"
    head = open(f"{LEVELS}/{level}").read(64)
    w = int(re.search(r'x=(\d+)', head).group(1))
    m = re.search(r'x=(\d+)&y=(\d+)', open(f"{BANK}/{level}.sol").read())
    cell = (int(m.group(2)) + 1) * (w + 2) + int(m.group(1)) + 1

    env = {**os.environ, "PROBEDUMP": "1", "STRONG": "1"}
    try:
        out = subprocess.run([SOLVER, f"{LEVELS}/{level}"], env=env,
                             capture_output=True, text=True, timeout=timeout).stdout
    except subprocess.TimeoutExpired:
        return None, f"关 {level}: 超时(>{timeout}s)"

    rows = []
    for line in out.splitlines():
        p = line.split()
        if p and p[0] == "OK" and len(p) >= 7:
            rows.append((int(p[1]), [float(v) for v in p[2:7]]))
    if not rows:
        return None, f"关 {level}: 没有幸存者"
    if not any(c == cell for c, _ in rows):
        return None, f"关 {level}: 针不在幸存者里（脏解或被证伪）"

    n = len(rows)
    best = {}
    for i, name in enumerate(FEATS):
        vals = sorted(rows, key=lambda r: r[1][i])
        asc = next(k for k, (c, _) in enumerate(vals) if c == cell) + 1
        # 降序名次可由升序推出（同值并列时略有出入，够用）
        pct = min(100.0 * asc / n, 100.0 * (n - asc + 1) / n)
        best[name] = pct
    return best, f"关 {level}: n={n} " + " ".join(f"{k}={v:.1f}%" for k, v in best.items())


if __name__ == "__main__":
    tmo = int(os.environ.get("TIMEOUT", "3600"))
    agg = {f: [] for f in FEATS}
    for lv in sys.argv[1:]:
        best, line = run(lv, tmo)
        print(line, flush=True)
        if best:
            for k, v in best.items():
                agg[k].append(v)
    if agg[FEATS[0]]:
        print("=== 平均百分位（越小越好，50%=随机） ===")
        for k, v in agg.items():
            print(f"  {k}: {sum(v)/len(v):.1f}%  (n={len(v)})")
