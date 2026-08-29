#!/usr/bin/env python3
"""公理蒸馏一键攻关（814 终杀配方的流水线化, 2026-08-29）

流程: 冠军漏斗(FUNNELDUMP原子交卷) -> 幸存并集(哨兵门) -> 逐钉审讯(STRONG传播)
      -> 场景交集蒸馏公理 -> king-ax 公理舰队(16M短阶梯) -> 官方check
任一阶段出解即短路到 check。产物入 research/<lv>/。

用法(WSL): python3 tools/axcrack.py <lv> [--jobs 26] [--funnel-tmo 3600] [--fleet-tmo 28800]
依赖: bin/king(冠军), /tmp/pz3(带FORCEEDGEFILE的probezero), /tmp/king-ax(带FORCEEDGEFILE的冠军)
      —— 后两者若缺, 从 versions/v58-probezero.c 与 evolve/solver.c+冠军policy 重编。
"""
import argparse
import glob
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BENCH = ROOT.parent / "coilbench"
CHECK = BENCH / "coil_check" / "check"
SOLBANK = ROOT.parent / "coil-solutions" / "solutions"


def sh(cmd, **kw):
    return subprocess.run(cmd, shell=isinstance(cmd, str), **kw)


def read_board(lv):
    b = (BENCH / "levels_all" / str(lv)).read_text()
    w = int(b.split("x=")[1].split("&")[0])
    return w + 2  # WW


def official_check(lv, solfile):
    r = sh([str(CHECK), str(BENCH / "levels_all" / str(lv)), str(solfile)],
           capture_output=True)
    return r.returncode == 0


def bank(lv, solfile, note):
    dst = SOLBANK / f"{lv}.sol"
    dst.write_text(Path(solfile).read_text())
    print(f"[{lv}] ✅ 官方check通过, 已入解库 {dst} ({note})", flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("lv")
    ap.add_argument("--jobs", type=int, default=26)
    ap.add_argument("--funnel-tmo", type=int, default=3600)
    ap.add_argument("--fleet-tmo", type=int, default=28800)
    a = ap.parse_args()
    lv = a.lv
    WW = read_board(lv)
    rd = ROOT / "research" / lv
    rd.mkdir(parents=True, exist_ok=True)
    board = str(BENCH / "levels_all" / lv)
    env0 = dict(os.environ)
    env0["PATH"] = "/usr/bin:/bin"

    # ---- 1. 冠军漏斗 ----
    for f in glob.glob(f"{rd}/surv.*"):
        os.unlink(f)
    sol1 = rd / "funnel.sol"
    print(f"[{lv}] 漏斗开火 JOBS={a.jobs}", flush=True)
    env = dict(env0, STATS="1", FUNNELDUMP=str(rd / "surv"), JOBS=str(a.jobs))
    with open(sol1, "w") as so, open(rd / "funnel.log", "w") as se:
        sh(f"ulimit -s unlimited; timeout {a.funnel_tmo} {ROOT}/bin/king {board}",
           stdout=so, stderr=se, env=env)
    if sol1.stat().st_size > 0 and official_check(lv, sol1):
        bank(lv, sol1, "漏斗直接出解")
        return 0

    # ---- 2. 幸存并集（哨兵门）----
    union = set()
    good = 0
    for f in glob.glob(f"{rd}/surv.*"):
        if f.endswith(".tmp"):
            continue
        lines = Path(f).read_text().splitlines()
        sent = [l for l in lines if l.startswith("# EOF")]
        ids = [int(l) for l in lines if l.strip() and not l.startswith("#")]
        if not sent or int(sent[0].split()[-1]) != len(ids):
            print(f"[{lv}] ⚠ {f} 半成品拒读", flush=True)
            continue
        good += 1
        union |= set(ids)
    pins = sorted(union)
    print(f"[{lv}] 并集候选 {len(pins)} ({good} 份合格切片)", flush=True)
    if not pins:
        print(f"[{lv}] ❌ 无幸存候选(全灭或漏斗失败), 人工介入", flush=True)
        return 1

    # ---- 3. 审讯 + 蒸馏 ----
    print(f"[{lv}] 审讯 {len(pins)} 场景", flush=True)
    inter = [None]
    refuted = set()

    def interrogate(cid):
        x, y = cid % WW - 1, cid // WW - 1
        fake = f"/tmp/ax_{lv}_{x}_{y}.sol"
        Path(fake).write_text(f"x={x}&y={y}&path=")
        env = dict(env0, STRONG="1", PROBEROUNDS="3", FIXDUMP="1")
        r = subprocess.run(["/tmp/pz3", board, "--verify", fake],
                           capture_output=True, text=True, timeout=300, env=env)
        os.unlink(fake)
        if "PROPOK 1" not in r.stderr:
            return cid, None
        return cid, frozenset(l for l in r.stderr.splitlines() if l.startswith("FIX "))

    with ThreadPoolExecutor(max_workers=a.jobs) as ex:
        for cid, fixes in ex.map(interrogate, pins):
            if fixes is None:
                refuted.add(cid)
            else:
                inter[0] = set(fixes) if inter[0] is None else (inter[0] & fixes)
    surv = [c for c in pins if c not in refuted]
    ax = rd / "axioms.txt"
    with open(ax, "w") as out:
        for l in sorted(inter[0] or []):
            _, c, d, v = l.split()
            out.write(f"{c} {d} {v}\n")
    axl = f"/tmp/axioms_{lv}.txt"
    sh(f"cp {ax} {axl}")
    with open(rd / "pins.startord", "w") as out:
        for i, c in enumerate(surv):
            out.write(f"OK {c} {i}\n")
    print(f"[{lv}] 审讯灭 {len(refuted)}, 幸存 {len(surv)}, 公理 {len(inter[0] or [])}", flush=True)
    if not surv:
        print(f"[{lv}] ❌ 审讯全灭 => unsound警报或无解, 人工介入", flush=True)
        return 1

    # ---- 4. 公理舰队（16M 短阶梯）----
    sol2 = rd / "fleet.sol"
    print(f"[{lv}] 公理舰队起飞 {len(surv)} 候选", flush=True)
    env = dict(env0, STARTORD=str(rd / "pins.startord"), STARTONLY="1", STATS="1",
               TREELOG="1", DEATHSTAT="1", RESTART="1", RESTARTK="12",
               SWEEP="16000000", JOBS=str(a.jobs), FORCEEDGEFILE=axl)
    with open(sol2, "w") as so, open(rd / "fleet.log", "w") as se:
        sh(f"ulimit -s unlimited; timeout {a.fleet_tmo} /tmp/king-ax {board}",
           stdout=so, stderr=se, env=env)
    if sol2.stat().st_size > 0 and official_check(lv, sol2):
        bank(lv, sol2, "公理舰队")
        return 0
    nos = Path(rd / "fleet.log").read_text(errors="replace").count("no solution")
    print(f"[{lv}] ❌ 舰队未出解 (no-solution {nos}/{a.jobs}), 人工介入", flush=True)
    return 1


if __name__ == "__main__":
    sys.exit(main())
