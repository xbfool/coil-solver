#!/usr/bin/env bash
# 767 终局攻坚：6 个活口候选逐格 swarm（747 攻陷配方）。
#
# 推理链（2026-08-26 晚，见 notes.md）：必有解 + 真起点∈87(双独立背书) + 52 个探针层真穷尽
# (650~32k 节点整树走完) + 16 个 shard 无限预算穷尽其份额 ⇒ 真起点极可能 ∈ 6 个活口
# (research/767/alive_hard.txt)。逐格上全部核心 swarm 磨，谁破谁收官。
# 用法：tools/hard6.sh [JOBS=26] [每格超时=14400]
set -uo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$HERE/bin/v77b"
BOARD=/mnt/d/works/coilbench/levels_all/767
CHECK=/mnt/d/works/coilbench/coil_check/check
LIST="$HERE/research/767/alive_hard.txt"
OUT="$HERE/research/767/hard6"
mkdir -p "$OUT"
J=${1:-26}; T=${2:-14400}
ulimit -s unlimited

while read -r X Y; do
  echo "=== 攻 ($X,$Y)  swarm x $J 核, 时间盒 ${T}s ==="
  t0=$(date +%s)
  PINSTART=$X,$Y JOBS=$J RESTART=1 RESTARTK=48 SWEEP=400000 PDEVERY=20 BJ=20 TREELOG=1 \
    timeout "$T" "$BIN" "$BOARD" > "$OUT/$X-$Y.out" 2> "$OUT/$X-$Y.err"
  rc=$?; dt=$(( $(date +%s) - t0 ))
  if [ -s "$OUT/$X-$Y.out" ] && "$CHECK" "$BOARD" "$OUT/$X-$Y.out" >/dev/null 2>&1; then
    cp "$OUT/$X-$Y.out" "$HERE/solutions/767.sol"
    echo "★★ ($X,$Y) ${dt}s 出解，官方 check 通过 —— 767 攻陷 ★★"; exit 0
  fi
  if [ $rc -ge 128 ] && [ $rc -ne 124 ] || grep -q TAINTED "$OUT/$X-$Y.err"; then
    echo "‼ ($X,$Y) 异常(rc=$rc)/TAINTED —— 该格无结论，查 $OUT/$X-$Y.err"
  elif [ $rc -eq 124 ]; then echo "($X,$Y) ${dt}s 时间盒到，树未穷尽（可加时续磨）"
  else echo "($X,$Y) ${dt}s 干净穷尽（该格排除）"
  fi
done < "$LIST"
echo "== 6 活口全过一轮未破：谁被'干净穷尽'谁进裁判复核；全穷尽则矛盾升级，开审搜索段 =="
