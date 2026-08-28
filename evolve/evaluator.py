#!/usr/bin/env python3
"""EVOLVE-PLAN P3: OpenEvolve 评测器（冻结 harness，AI 不可改）。

被进化的 program = policy.h。本文件：
  1. 把候选 policy.h 和冻结的 solver.c 一起编译
  2. T0 soundness 硬门（REFSOL 回放：断言违例/崩溃/假解 → 0 分淘汰）
  3. T1 校准套件 → fitness + 行为特征（MAP-Elites 描述符）
  4. stage3 加 T1H 深树探针（271/287 是 v78 解不动的现成靶子）

红线：绝不把解文件内容放进 metrics/artifacts（解库私有，防 LLM 训练集污染）。
"""
import json
import math
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent          # evolve/
ROOT = HERE.parent                              # coil-solver/
CB = Path(os.environ.get("COILBENCH_DIR", ROOT.parent / "coilbench"))
SOLBANK = Path(os.environ.get("COIL_SOLBANK", ROOT.parent / "coil-solutions" / "solutions"))
LEVELS = CB / "levels_all"
CHECK = CB / "coil_check" / "check"

STATS_RE = re.compile(r"^STATS (\{.*\})$", re.M)
SOL_RE = re.compile(r"^x=-?\d+&y=-?\d+&path=[UDLR]+$", re.M)

# v78 基线（results/vec/v78-baseline2*.jsonl，节点数确定性可复现）
BASELINE_T1_NODES = 2_547_262
BASELINE_HARD = {"195": 3162454, "223": 2544140, "271": None, "287": None,
                 "319": 22585743, "383": 2095962}

# 分型（results/boardtypes.tsv 的规则）：校准盘的 A/B 标签
TYPE_B = {"84", "102", "126", "137", "155", "174", "230", "263", "323", "353", "413"}


def _read_list(name):
    out = []
    for line in (ROOT / "calib" / name).read_text(encoding="utf-8").splitlines():
        line = line.split("#")[0].strip()
        if line:
            out.append(line)
    return out


def build(program_path: str):
    """候选 policy.h + 冻结 solver.c → 二进制。失败返回 (None, 错误尾巴)。"""
    tmp = Path(tempfile.mkdtemp(prefix="coil_evolve_"))
    shutil.copy(HERE / "solver.c", tmp / "solver.c")
    shutil.copy(program_path, tmp / "policy.h")
    binp = tmp / "solver"
    r = subprocess.run(
        ["cc", "-std=gnu99", "-O2", "-march=native", str(tmp / "solver.c"), "-o", str(binp)],
        capture_output=True, text=True, timeout=180)
    if r.returncode != 0 or not binp.is_file():
        return None, (tmp, r.stderr[-2000:])
    return binp, (tmp, "")


def run_level(binp, lv, timeout, refsol=False):
    env = dict(os.environ, STATS="1", JOBS="1")
    if refsol:
        env["REFSOL"] = str(SOLBANK / f"{lv}.sol")
    board = (LEVELS / lv).read_text(encoding="utf-8").strip()
    rec = {"level": lv, "solved": 0, "tainted": 0, "violation": 0, "timeout": 0}
    try:
        r = subprocess.run([str(binp)], input=board, capture_output=True, text=True,
                           timeout=timeout, env=env)
        out, err = r.stdout, r.stderr
        if r.returncode < 0:
            rec["tainted"] = 1          # 崩溃 = TAINTED（767 教训：验尸，不许静默）
    except subprocess.TimeoutExpired as e:
        rec["timeout"] = 1
        out = (e.stdout or b"") if isinstance(e.stdout, bytes) else (e.stdout or "")
        err = (e.stderr or b"") if isinstance(e.stderr, bytes) else (e.stderr or "")
        if isinstance(out, bytes):
            out = out.decode("utf-8", "replace")
        if isinstance(err, bytes):
            err = err.decode("utf-8", "replace")
    if refsol and "!!" in err:
        rec["violation"] = 1            # REFSOL 断言违例 = unsound
    m = STATS_RE.findall(err)
    if m:
        rec.update(json.loads(m[-1]))
    sols = SOL_RE.findall(out)
    if sols:
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
            f.write(sols[-1])
        ok = subprocess.run([str(CHECK), str(LEVELS / lv), f.name],
                            capture_output=True).returncode == 0
        os.unlink(f.name)
        if ok:
            rec["solved"] = 1
        else:
            rec["violation"] = 1        # 声称解出但官方 check 拒绝 = 假解
    return rec


def _fail(reason, extra=None):
    m = {"combined_score": 0.0, "t0_pass": 0.0, "error": reason}
    if extra:
        m["error_detail"] = str(extra)[:1500]
    return m


def _score_t1(recs):
    solved = [r for r in recs if r["solved"]]
    frac = len(solved) / len(recs)
    nodes = sum(r.get("nodes_total", 0) for r in solved) or 1
    node_score = BASELINE_T1_NODES / (BASELINE_T1_NODES + nodes)  # 0.5=基线持平
    a = [r for r in solved if r["level"] not in TYPE_B]
    b = [r for r in solved if r["level"] in TYPE_B]
    na = sum(r.get("nodes_total", 0) for r in a) or 1
    nb = sum(r.get("nodes_total", 0) for r in b) or 1
    # 行为描述符（MAP-Elites 维度，0..1）
    deaths = {k: sum(r.get(k, 0) for r in solved)
              for k in ("dth_estate", "dth_reach", "dth_dyn", "dth_flow", "dth_geom", "dth_liveend")}
    tot_d = sum(deaths.values()) or 1
    deduction_frac = (deaths["dth_estate"] + deaths["dth_dyn"] + deaths["dth_flow"]) / tot_d
    b_over_a = max(0.0, min(1.0, 0.5 + 0.25 * math.log10(nb / na)))  # <0.5 偏B型特化
    return frac, node_score, deduction_frac, b_over_a


def evaluate_stage1(program_path):
    """便宜闸门：编译 + 2 关 REFSOL + 3 关小盘。"""
    binp, (tmp, errtail) = build(program_path)
    try:
        if binp is None:
            return _fail("compile", errtail)
        for lv in ("13", "61"):
            r = run_level(binp, lv, 20, refsol=True)
            if r["violation"] or r["tainted"]:
                return _fail(f"unsound@L{lv}", r)
            if not r["solved"]:
                return _fail(f"refsol_unsolved@L{lv}", r)
        recs = [run_level(binp, lv, 10) for lv in ("50", "102", "150")]
        if any(r["violation"] or r["tainted"] for r in recs):
            return _fail("unsound@mini", recs)
        frac = sum(r["solved"] for r in recs) / len(recs)
        return {"combined_score": 0.1 + 0.4 * frac, "t0_pass": 1.0}
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def evaluate_stage2(program_path):
    """完整 T0 硬门 + T1 校准套件。"""
    binp, (tmp, errtail) = build(program_path)
    try:
        if binp is None:
            return _fail("compile", errtail)
        for lv in _read_list("refsol.txt"):
            r = run_level(binp, lv, 90, refsol=True)
            if r["violation"] or r["tainted"]:
                return _fail(f"unsound@L{lv}", r)      # 一票否决
            if not r["solved"]:
                return _fail(f"refsol_unsolved@L{lv}", r)
        recs = [run_level(binp, lv, 20) for lv in _read_list("list.txt")]
        if any(r["violation"] or r["tainted"] for r in recs):
            return _fail("unsound@t1")
        frac, node_score, ded, boa = _score_t1(recs)
        return {
            "combined_score": 0.5 * frac + 0.3 * node_score + 0.05,
            "t0_pass": 1.0, "t1_frac": frac, "t1_node_score": node_score,
            "deduction_frac": ded, "b_over_a": boa,
        }
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def evaluate(program_path):
    """全量：stage2 + T1H 深树探针（真正的进化靶子在这里）。"""
    m = evaluate_stage2(program_path)
    if m.get("t0_pass", 0.0) < 1.0:
        return m
    binp, (tmp, _) = build(program_path)
    try:
        hard = [run_level(binp, lv, 45) for lv in _read_list("hard.txt")]
        if any(r["violation"] or r["tainted"] for r in hard):
            return _fail("unsound@hard")
        hs = []
        for r in hard:
            base = BASELINE_HARD.get(r["level"])
            if not r["solved"]:
                hs.append(0.0)
            elif base is None:
                hs.append(1.0)          # 解出 v78 解不动的关 = 满分
            else:
                hs.append(base / (base + (r.get("nodes_total") or base)))
        hard_score = sum(hs) / len(hs)
        m["hard_score"] = hard_score
        m["hard_solved"] = sum(r["solved"] for r in hard)
        m["combined_score"] = (0.35 * m["t1_frac"] + 0.2 * m["t1_node_score"]
                               + 0.45 * hard_score)
        return m
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    import sys
    print(json.dumps(evaluate(sys.argv[1] if len(sys.argv) > 1 else str(HERE / "policy.h")),
                     ensure_ascii=False, indent=1))
