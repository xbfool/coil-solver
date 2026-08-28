#!/usr/bin/env python3
"""分片产物唯一合法读取器（2026-08-28 切片截断陷阱根治,第二层防御）。

验三样,缺一即非零退出、零输出:
  1. 份数: prefix.0..jobs-1 全部在场(.tmp 不算——那是被杀 shard 的尸体)
  2. 哨兵: 每份末行必须是 '# EOF <n>'
  3. 行数: n == 该份数据行数

用法:
  python3 tools/survread.py <prefix> --jobs N              # 交错序(各片第1名先出,配合针位前三成信号)
  python3 tools/survread.py <prefix> --jobs N --concat     # 片序拼接
  python3 tools/survread.py <prefix> --jobs N --startord   # 直接输出 STARTORD 格式(OK <id> 0)
  python3 tools/survread.py <prefix> --jobs N --check      # 只验不出
"""
import argparse
import sys
from pathlib import Path


def read_validated(prefix: str, jobs: int):
    shards = []
    missing = []
    for i in range(jobs):
        p = Path(f"{prefix}.{i}")
        if not p.is_file():
            missing.append(i)
            continue
        lines = p.read_text().splitlines()
        if not lines or not lines[-1].startswith("# EOF "):
            sys.exit(f"REJECT: {p} 缺 EOF 哨兵(写入被截断?)")
        n = int(lines[-1].split()[-1])
        data = [int(x) for x in lines[:-1] if x.strip() and not x.startswith("#")]
        if len(data) != n:
            sys.exit(f"REJECT: {p} 哨兵计数 {n} != 数据行 {len(data)}")
        shards.append(data)
    if missing:
        sys.exit(f"REJECT: 缺份 {missing}（{jobs - len(missing)}/{jobs}）——半成品名单一律拒读")
    return shards


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("prefix")
    ap.add_argument("--jobs", type=int, required=True)
    ap.add_argument("--concat", action="store_true")
    ap.add_argument("--startord", action="store_true")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    shards = read_validated(args.prefix, args.jobs)
    total = sum(len(s) for s in shards)
    if args.check:
        print(f"OK {args.jobs}/{args.jobs} 份, {total} 条", file=sys.stderr)
        return
    if args.concat:
        seq = [x for s in shards for x in s]
    else:  # 轮转交错: 各片第 1 名先出(针位信号: 真起点集中在前三成)
        seq = []
        for r in range(max(len(s) for s in shards)):
            for s in shards:
                if r < len(s):
                    seq.append(s[r])
    seen = set()
    for x in seq:
        if x in seen:
            continue
        seen.add(x)
        print(f"OK {x} 0" if args.startord else x)
    print(f"survread: {args.jobs}份 {total}条 去重后{len(seen)}", file=sys.stderr)


if __name__ == "__main__":
    main()
