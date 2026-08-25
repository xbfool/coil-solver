#!/usr/bin/env bash
# 703 的 8 个候选，每个用 swarm 多策略并行啃（而不是单一确定策略）。
#
# 为什么：swarm 的激活条件是 ns <= nshard*SWARMX 且 nshard > 1。
# 之前测 703 全用 PINSTART + JOBS=1，nshard=1 直接把 swarm 关掉了 ——
# 等于单起点 + 单一确定策略 + 零多样性，正是最差的配置。
# 回溯搜索的耗时是重尾的：同一实例换个分支顺序可能几秒出解，
# 所以多策略并行才是对的形态（代码里本来就有 4 档策略梯队）。
set -uo pipefail
BIN=${BIN:-/mnt/d/works/coil-solver/bin/v73-dirlayer}
LV=${LV:-/mnt/d/works/coilbench/levels_all/703}
J=${1:-3}; LIMIT=${2:-600}; LOG=${3:-/tmp/swarm703.log}
: > "$LOG"
for xy in 28,150 29,151 31,151 28,152 30,152 31,152 30,153 31,153; do
  (
    S=$(date +%s)
    PINSTART=$xy JOBS=$J timeout "$LIMIT" "$BIN" "$LV" > "/tmp/sw_$xy.txt" 2>/dev/null
    echo "起点 $xy: $(( $(date +%s) - S ))s 出=$(wc -c < "/tmp/sw_$xy.txt")字节" >> "$LOG"
  ) &
done
wait
cat "$LOG"
