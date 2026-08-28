#!/usr/bin/env python3
"""进化战役 #2 评测器：双标的 fitness（EVOLVE-PLAN，用户定义的科研目标）。

标的A 磨的加速: 零信息阶梯钉真起点 → 针树节点数（越少越好）
标的B 名单的加速: 中型盘漏斗 → 时间×幸存者数，硬约束"真起点必须存活"（解库裁判）

soundness: T0 REFSOL 快门 + B 的真起点存活 + 官方 check。
"""
import json
import math
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
CB = Path(os.environ.get("COILBENCH_DIR", ROOT.parent / "coilbench"))
LEVELS = CB / "levels_all"
CHECK = CB / "coil_check" / "check"
SOLBANK = Path(os.environ.get("COIL_SOLBANK", ROOT.parent / "coil-solutions" / "solutions"))

STATS_RE = re.compile(r"^STATS (\{.*\})$", re.M)
SOL_RE = re.compile(r"^x=-?\d+&y=-?\d+&path=[UDLR]+$", re.M)

# 标的A: (关, 真起点, champ1 基线节点) —— needlebench-champ1.tsv 2026-08-28
NEEDLES = [("580", "142,79", 71491), ("582", "142,130", 224832), ("590", "74,97", 22892),
           ("598", "18,60", 79736), ("628", "134,0", 617054), ("634", "78,121", 191217),
           ("736", "231,140", 616501)]
NEEDLE_TMO = 150
# 标的B: 漏斗盘（中型零信息）; 基线由第一次运行时自标定
FUNNEL_LEVEL = "634"
FUNNEL_TRUE = (78, 121)
FUNNEL_TMO = 420
FUNNEL_BASE_COST = None  # 惰性: 第一次eval时用 wall_s*surv 直接比意义即可,用固定参考:
FUNNEL_REF = 120.0 * 200  # 参考成本(s×幸存者),首测后可校正


def build(program_path):
    tmp = Path(tempfile.mkdtemp(prefix="coil_ev2_"))
    shutil.copy(HERE / "solver.c", tmp / "solver.c")
    shutil.copy(program_path, tmp / "policy.h")
    r = subprocess.run(["cc", "-std=gnu99", "-O2", "-march=native",
                        str(tmp / "solver.c"), "-o", str(tmp / "solver")],
                       capture_output=True, text=True, timeout=180)
    if r.returncode != 0:
        return None, (tmp, r.stderr[-1500:])
    return tmp / "solver", (tmp, "")


def _run(binp, lv, timeout, env2):
    env = dict(os.environ, STATS="1", **env2)
    board = (LEVELS / lv).read_text(encoding="utf-8").strip()
    try:
        r = subprocess.run([str(binp)], input=board, capture_output=True, text=True,
                           timeout=timeout, env=env)
        out, err, rc = r.stdout, r.stderr, r.returncode
    except subprocess.TimeoutExpired as e:
        out = e.stdout or ""
        err = e.stderr or ""
        if isinstance(out, bytes): out = out.decode("utf-8", "replace")
        if isinstance(err, bytes): err = err.decode("utf-8", "replace")
        rc = -999
    rec = {"tainted": 1 if (rc < 0 and rc != -999) else 0, "timeout": 1 if rc == -999 else 0}
    m = STATS_RE.findall(err)
    if m: rec.update(json.loads(m[-1]))
    sols = SOL_RE.findall(out)
    rec["sol"] = sols[-1] if sols else None
    return rec


def _fail(reason, extra=None):
    m = {"combined_score": 0.0, "t0_pass": 0.0, "error": reason,
         "needle_score": 0.0, "funnel_score": 0.0}
    if extra: m["error_detail"] = str(extra)[:1200]
    return m


def _verify(lv, sol):
    import tempfile as tf
    with tf.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
        f.write(sol)
    ok = subprocess.run([str(CHECK), str(LEVELS / lv), f.name], capture_output=True).returncode == 0
    os.unlink(f.name)
    return ok


def evaluate_stage1(program_path):
    binp, (tmp, err) = build(program_path)
    try:
        if binp is None:
            return _fail("compile", err)
        # T0 快门: 2 关带真解
        for lv in ("13", "61"):
            r = _run(binp, lv, 25, {"JOBS": "1", "REFSOL": str(SOLBANK / f"{lv}.sol")})
            if r["tainted"] or not r["sol"] or not _verify(lv, r["sol"]):
                return _fail(f"t0@{lv}", r)
        # 针快检: 2 个便宜针
        for lv, pin, base in NEEDLES[:2]:
            r = _run(binp, lv, 60, {"JOBS": "1", "PINSTART": pin, "RESTART": "1", "SWEEP": "50000"})
            if r["tainted"]:
                return _fail(f"tainted@{lv}", r)
        return {"combined_score": 0.5, "t0_pass": 1.0}
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def evaluate_stage2(program_path):
    binp, (tmp, err) = build(program_path)
    try:
        if binp is None:
            return _fail("compile", err)
        for lv in ("13", "61", "139", "263"):
            r = _run(binp, lv, 120, {"JOBS": "1", "REFSOL": str(SOLBANK / f"{lv}.sol")})
            if r["tainted"] or not r["sol"] or not _verify(lv, r["sol"]):
                return _fail(f"t0@{lv}", r)
        # ---- 标的A: 针成本 ----
        logs = []
        solved = 0
        for lv, pin, base in NEEDLES:
            r = _run(binp, lv, NEEDLE_TMO, {"JOBS": "1", "PINSTART": pin, "RESTART": "1", "SWEEP": "50000"})
            if r["tainted"]:
                return _fail(f"tainted@{lv}", r)
            if r["sol"]:
                if not _verify(lv, r["sol"]):
                    return _fail(f"fakesol@{lv}")
                solved += 1
                logs.append(math.log10(max(1, r.get("nodes_total", 10**9)) / base))
            else:
                logs.append(1.5)  # 超时罚: 当作 30 倍劣化
        # needle_score: 基线=0.5; 每 10 倍加速 +0.25
        mean_log = sum(logs) / len(logs)
        needle_score = max(0.0, min(1.0, 0.5 - 0.25 * mean_log))
        frac = solved / len(NEEDLES)
        m = {"t0_pass": 1.0, "needle_score": needle_score, "needle_solved": frac,
             "combined_score": 0.15 + 0.5 * needle_score * frac}
        return m
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def evaluate_stage3(program_path):
    """标的B(端到端口径): 628 无钉 JOBS=8 从开跑到解出的墙钟。
    基线 champ1 = 117s。涵盖名单成本+磨成本+590型'漏斗自解'的良性情况。"""
    m = evaluate_stage2(program_path)
    if m.get("t0_pass", 0) < 1.0:
        return m
    binp, (tmp, _) = build(program_path)
    try:
        import time
        t0 = time.monotonic()
        r = _run(binp, "628", 300, {"JOBS": "8", "RESTART": "1", "SWEEP": "50000"})
        wall = time.monotonic() - t0
        if r["tainted"]:
            return _fail("tainted@e2e")
        if r["sol"] and not _verify("628", r["sol"]):
            return _fail("fakesol@e2e")
        if r["sol"]:
            e2e = max(0.0, min(1.0, 0.5 - 0.25 * math.log10(max(wall, 1) / 117.0)))
            m["e2e_wall_s"] = round(wall, 1)
        else:
            e2e = 0.0
            m["e2e_note"] = "timeout300"
        m["funnel_score"] = e2e   # 字段名沿用,语义=端到端分
        # 2026-08-28 16:25 降权: h&7 vs h&15 证明单盘 e2e 被顺序彩票主导(19s vs 300s超时),
        # needle(确定性节点数)才是抗彩票信号。e2e 降为 tie-breaker。
        m["combined_score"] = (0.1 * m.get("t0_pass", 0)
                               + 0.65 * m["needle_score"] * m.get("needle_solved", 0)
                               + 0.25 * e2e)
        return m
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def _dead_stage3(program_path):
    m = {}
    binp, (tmp, _) = build(program_path)
    try:
        # ---- 标的B: 漏斗成本（真起点必须活着）----
        dump = tmp / "surv"
        env = dict(os.environ, STATS="1", JOBS="4", RESTART="1", SWEEP="50000",
                   FUNNELDUMP=str(dump))
        import signal
        import time
        board = (LEVELS / FUNNEL_LEVEL).read_text(encoding="utf-8").strip()
        t0 = time.monotonic()
        p = subprocess.Popen([str(binp)], stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL, env=env, text=True,
                             preexec_fn=os.setsid)
        p.stdin.write(board); p.stdin.close()
        wall = None
        while time.monotonic() - t0 < FUNNEL_TMO:
            # 4 个 shard 的 dump 全落盘 = 漏斗完成,计时并掐掉(后面的扫荡不计入名单成本)
            if len(list(tmp.glob("surv.*"))) >= 4:
                time.sleep(0.3)
                wall = time.monotonic() - t0
                break
            if p.poll() is not None:
                wall = time.monotonic() - t0
                break
            time.sleep(0.5)
        try:
            os.killpg(p.pid, signal.SIGKILL)
        except Exception:
            pass
        if wall is None:
            m["funnel_score"] = 0.0
            m["funnel_note"] = "funnel_timeout"
            m["combined_score"] = 0.1 + 0.55 * m["needle_score"] * m.get("needle_solved", 0)
            return m
        surv = set()
        for f in tmp.glob("surv.*"):
            surv.update(int(x) for x in f.read_text().split())
        board = (LEVELS / FUNNEL_LEVEL).read_text(encoding="utf-8")
        W = int(board.split("x=")[1].split("&")[0])
        tx, ty = FUNNEL_TRUE
        true_id = (ty + 1) * (W + 2) + (tx + 1)
        if surv and true_id not in surv:
            return _fail("funnel_killed_true_start")   # unsound 一票否决
        if not surv:
            m["funnel_score"] = 0.0
            m["funnel_note"] = "no_dump"
        else:
            cost = wall * len(surv)
            m["funnel_wall_s"] = round(wall, 1)
            m["funnel_surv"] = len(surv)
            m["funnel_score"] = max(0.0, min(1.0, 0.5 - 0.25 * math.log10(cost / FUNNEL_REF)))
        m["combined_score"] = (0.1 * m.get("t0_pass", 0) + 0.55 * m["needle_score"] * m.get("needle_solved", 0)
                               + 0.35 * m.get("funnel_score", 0))
        return m
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def evaluate(program_path):
    return evaluate_stage3(program_path)


if __name__ == "__main__":
    import sys
    print(json.dumps(evaluate(sys.argv[1] if len(sys.argv) > 1 else str(HERE / "champions" / "2026-08-28-adapt-z10-warm16.h")), ensure_ascii=False, indent=1))
