#!/usr/bin/env python3
"""针位信号实验（EVOLVE 标的B 科研）：真起点在漏斗 dump 排名里的名次分布。

F 信号已证≈随机(针位测量 2026-08-25)。本实验测的是**漏斗自身的输出序**
（探针存活分档+frank 排序,FUNNELDUMP 的落盘顺序）。
若真起点系统性靠前 → 名单可流式产出(筛出前 X% 即开磨),磨的期望成本÷排名分位。

用法(WSL): python3 tools/needle_signal.py 634 736 748 [--jobs 8] [--bin bin/champ1] [--tmo 900]
"""
import argparse
import os
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LEVELS = Path("/mnt/d/works/coil/coilbench/levels_all")
SOLBANK = ROOT.parent / "coil-solutions" / "solutions"


def true_id(lv):
    sol = (SOLBANK / f"{lv}.sol").read_text(encoding="utf-8")
    x = int(sol.split("x=")[1].split("&")[0])
    y = int(sol.split("y=")[1].split("&")[0])
    board = (LEVELS / lv).read_text(encoding="utf-8")
    w = int(board.split("x=")[1].split("&")[0])
    return (y + 1) * (w + 2) + (x + 1), (x, y)


def funnel_dump(lv, binp, jobs, tmo):
    d = Path(tempfile.mkdtemp(prefix=f"nsig_{lv}_"))
    env = dict(os.environ, FUNNELDUMP=str(d / "surv"), STATS="1",
               RESTART="1", SWEEP="50000", JOBS=str(jobs))
    board = (LEVELS / lv).read_text(encoding="utf-8").strip()
    t0 = time.monotonic()
    p = subprocess.Popen([str(binp)], stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL, env=env, text=True, preexec_fn=os.setsid)
    p.stdin.write(board)
    p.stdin.close()
    while time.monotonic() - t0 < tmo:
        if len(list(d.glob("surv.*"))) >= jobs or p.poll() is not None:
            time.sleep(0.5)
            break
        time.sleep(1)
    wall = time.monotonic() - t0
    try:
        os.killpg(p.pid, signal.SIGKILL)
    except Exception:
        pass
    return d, wall


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("levels", nargs="+")
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--bin", default=str(ROOT / "bin" / "champ1"))
    ap.add_argument("--tmo", type=int, default=900)
    args = ap.parse_args()

    print("关\t真起点\t落在shard\t名次/该片长\t片内分位\t全局(名次估计)\t漏斗wall")
    for lv in args.levels:
        tid, (x, y) = true_id(lv)
        d, wall = funnel_dump(lv, args.bin, args.jobs, args.tmo)
        files = sorted(d.glob("surv.*"))
        hit = None
        total = 0
        for f in files:
            ids = [int(t) for t in f.read_text().split()]
            total += len(ids)
            if tid in ids:
                hit = (f.name, ids.index(tid) + 1, len(ids))
        if not files:
            print(f"{lv}\t({x},{y})\t漏斗未完成({wall:.0f}s)")
        elif hit is None:
            # ⚠ 份数不全时不许下 unsound 结论(2026-08-28 名单切片事故同款陷阱)
            tag = "🚨 真起点被漏斗杀掉(unsound!)" if len(files) >= args.jobs else \
                  f"⚠ 真起点不在已交卷切片({len(files)}/{args.jobs}份,不可判)"
            print(f"{lv}\t({x},{y})\t{tag} 幸存{total} wall={wall:.0f}s")
        else:
            fn, rank, n = hit
            print(f"{lv}\t({x},{y})\t{fn}\t{rank}/{n}\t{100*rank/n:.0f}%\t~{100*rank/n:.0f}%\t{wall:.0f}s")
        for f in files:
            f.unlink()
        d.rmdir()


if __name__ == "__main__":
    main()
