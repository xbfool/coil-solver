#!/usr/bin/env python3
"""EVOLVE-PLAN P0: 跑一组盘产 performance vector（JSONL，每盘一行）。

用法（WSL）:
    python3 tools/evalvec.py bin/v78stats 50 101 139 --timeout 30
    python3 tools/evalvec.py bin/v78stats --list bench/calib/list.txt --out results/vec/v78.jsonl
    python3 tools/evalvec.py bin/v78stats 50 --env BJ=500 --env RESTART=1

约定:
  - 强制 JOBS=1 STATS=1（swarm 多进程时 STATS 每 shard 一行，不可比；
    校准盘要的是确定性单进程向量）。
  - 关号解析为 coilbench levels_all/<n>（或 --levels-dir 指定）；也接受直接给文件路径。
  - 每盘输出: level / w / h / solved / verified(官方 check) / wall_ms / timeout /
    STATS 里的全部字段 / env 配置 / 二进制路径。
  - solved 但 verified=false 是最高级警报（unsound！），进程退出码置 2。
"""
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

STATS_RE = re.compile(r"^STATS (\{.*\})$", re.M)
SOL_RE = re.compile(r"^x=-?\d+&y=-?\d+&path=[UDLR]+$", re.M)


def find_level(spec: str, levels_dir: Path) -> Path:
    p = Path(spec)
    if p.is_file():
        return p
    p = levels_dir / spec
    if p.is_file():
        return p
    sys.exit(f"找不到关卡: {spec}（levels_dir={levels_dir}）")


def check_solution(check_bin: Path, level: Path, solution: str) -> bool:
    fd, tmp = tempfile.mkstemp(prefix="evalvec_sol_", suffix=".txt")
    os.close(fd)
    try:
        Path(tmp).write_text(solution, encoding="utf-8")
        r = subprocess.run([str(check_bin), str(level), tmp], capture_output=True, text=True)
        return r.returncode == 0
    finally:
        os.unlink(tmp)


def run_one(binary: Path, level: Path, timeout: float, env_extra: dict, check_bin) -> dict:
    env = dict(os.environ)
    env.update(env_extra)
    env["STATS"] = "1"
    env["JOBS"] = "1"
    board = level.read_text(encoding="utf-8").strip()
    m = dict(kv.split("=", 1) for kv in board.split("&")[:2])
    rec = {
        "level": level.name,
        "w": int(m.get("x", -1)),
        "h": int(m.get("y", -1)),
        "solved": 0,
        "verified": None,
        "timeout": 0,
        "wall_ms": None,
        "env": env_extra,
        "binary": str(binary),
    }
    t0 = time.monotonic()
    try:
        r = subprocess.run(
            [str(binary)], input=board, capture_output=True, text=True,
            timeout=timeout, env=env,
        )
        out, err = r.stdout, r.stderr
        # 验尸: 崩溃(负退出码=信号)必须显式记账,绝不能静默当"没解出"(767 案教训)
        if r.returncode < 0:
            rec["signal"] = -r.returncode
            rec["tainted"] = 1
    except subprocess.TimeoutExpired as e:
        rec["timeout"] = 1
        out = e.stdout or ""
        err = e.stderr or ""
        if isinstance(out, bytes):
            out = out.decode("utf-8", "replace")
        if isinstance(err, bytes):
            err = err.decode("utf-8", "replace")
    rec["wall_ms"] = int((time.monotonic() - t0) * 1000)

    ms = STATS_RE.findall(err)
    if ms:
        rec.update(json.loads(ms[-1]))
    sol = SOL_RE.findall(out)
    if sol:
        rec["solved"] = 1
        if check_bin:
            rec["verified"] = 1 if check_solution(check_bin, level, sol[-1]) else 0
    return rec


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("binary")
    ap.add_argument("levels", nargs="*", help="关号或文件路径")
    ap.add_argument("--list", help="关号清单文件（每行一个，# 注释）")
    ap.add_argument("--timeout", type=float, default=30.0)
    ap.add_argument("--out", help="JSONL 输出（默认 stdout）")
    ap.add_argument("--env", action="append", default=[], help="K=V，可多次")
    ap.add_argument("--levels-dir", default=None)
    ap.add_argument("--coilbench", default=None)
    args = ap.parse_args()

    here = Path(__file__).resolve().parent.parent
    cb = Path(args.coilbench or os.environ.get("COILBENCH_DIR") or here.parent / "coilbench")
    levels_dir = Path(args.levels_dir) if args.levels_dir else cb / "levels_all"
    check_bin = cb / "coil_check" / "check"
    if not check_bin.is_file():
        print(f"警告: 找不到官方 check（{check_bin}），跳过验证", file=sys.stderr)
        check_bin = None

    specs = list(args.levels)
    if args.list:
        for line in Path(args.list).read_text(encoding="utf-8").splitlines():
            line = line.split("#")[0].strip()
            if line:
                specs.append(line)
    if not specs:
        sys.exit("没有指定关卡")

    env_extra = dict(kv.split("=", 1) for kv in args.env)
    binary = Path(args.binary)
    if not binary.is_file():
        sys.exit(f"找不到二进制: {binary}")

    sink = open(args.out, "w", encoding="utf-8") if args.out else sys.stdout
    unsound = False
    for spec in specs:
        level = find_level(spec, levels_dir)
        rec = run_one(binary, level, args.timeout, env_extra, check_bin)
        sink.write(json.dumps(rec, ensure_ascii=False) + "\n")
        sink.flush()
        if rec["solved"] and rec["verified"] == 0:
            unsound = True
            print(f"🚨 UNSOUND: {level.name} 声称解出但官方 check 拒绝!", file=sys.stderr)
        tag = "PASS" if rec["solved"] else ("TIMEOUT" if rec["timeout"] else "FAIL")
        print(f"  L{level.name} {tag} {rec['wall_ms']}ms nodes={rec.get('nodes_total')}", file=sys.stderr)
    if args.out:
        sink.close()
    sys.exit(2 if unsound else 0)


if __name__ == "__main__":
    main()
