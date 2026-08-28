#!/usr/bin/env bash
# 组合攻击一个硬关的单个候选起点：10 个单核进程，身份横跨全部 4 个策略档。
#
#   tools/portfolio703.sh <x,y> [上限秒] [关号]
#
# 为什么这么打：
# · 回溯耗时是重尾的 —— 同一起点换分支顺序可能差几个数量级，多样性 > 单点算力；
# · 策略档由 shard 身份决定：0=纯 Warnsdorff, 1..7=同分抖动, 8..21=大抖动, 22..31=纯哈希序。
#   之前 JOBS=3 的"多策略"其实只碰到 0/1 两档，2/3 档一次没跑过 —— 假多样性。
# · FORCESHARD=i 单进程指定身份即可激活 swarm 顺序（代码里 force_shard_identity>=0 分支），
#   所以 10 个单核进程选 {0,3,6, 10,14,18, 22,25,28,31} 就覆盖四档，且总占用只有 10 核。
# 任何一个进程出解，其余立刻停。
set -uo pipefail
XY=${1:?候选起点 x,y}; LIMIT=${2:-900}; LV=${3:-703}
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN=${BIN:-$ROOT/bin/v73-dirlayer}
BOARD="$ROOT/../coilbench/levels_all/$LV"
IDS="0 3 6 10 14 18 22 25 28 31"
LOG=/tmp/pf_${LV}_${XY}.log
: > "$LOG"
pids=()
for id in $IDS; do
  (
    S=$(date +%s)
    PINSTART=$XY FORCESHARD=$id JOBS=1 timeout "$LIMIT" "$BIN" "$BOARD" > "/tmp/pf_${LV}_${XY}_$id.txt" 2>/dev/null
    if [ -s "/tmp/pf_${LV}_${XY}_$id.txt" ]; then
      echo "身份 $id: $(( $(date +%s) - S ))s **出解**" >> "$LOG"
    else
      echo "身份 $id: $(( $(date +%s) - S ))s 无" >> "$LOG"
    fi
  ) &
  pids+=($!)
done
# 有人出解就掐掉其他人
while :; do
  alive=0
  for p in "${pids[@]}"; do kill -0 "$p" 2>/dev/null && alive=$((alive+1)); done
  [ "$alive" -eq 0 ] && break
  if grep -q "出解" "$LOG" 2>/dev/null; then
    for p in "${pids[@]}"; do kill "$p" 2>/dev/null; done
    pkill -P $$ 2>/dev/null
    break
  fi
  sleep 5
done
wait 2>/dev/null
cat "$LOG"
for id in $IDS; do
  f="/tmp/pf_${LV}_${XY}_$id.txt"
  [ -s "$f" ] && { echo "WINNER=$f"; break; }
done
