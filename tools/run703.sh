#!/usr/bin/env bash
# 703 的 8 个候选起点各钉一次，比较有向层档位。
#   tools/run703.sh <DIRLAYER档位> <上限秒> <日志>
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN=${BIN:-$ROOT/bin/v73-dirlayer}
LV="$ROOT/../coilbench/levels_all/703"
D=${1:-4}; LIMIT=${2:-2400}; LOG=${3:-/tmp/p703b.log}
: > "$LOG"
for xy in 28,150 29,151 31,151 28,152 30,152 31,152 30,153 31,153; do
  (
    S=$(date +%s)
    PINSTART=$xy DIRLAYER=$D JOBS=1 timeout "$LIMIT" "$BIN" "$LV" > "/tmp/pb_$xy.txt" 2>/dev/null
    echo "起点 $xy: $(( $(date +%s) - S ))s 出=$(wc -c < "/tmp/pb_$xy.txt")字节" >> "$LOG"
  ) &
done
wait
cat "$LOG"
