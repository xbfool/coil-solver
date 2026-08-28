#!/usr/bin/env bash
# 基石钉法：767 的 87 个强筛候选，用"无菌 estate"（CHAIN=0 FLOW=0 DIRLAYER=0）逐个完整搜索。
# 逻辑：87 里必有真起点（可解盘 + 筛选自称 sound）；此前它死在"全链条 estate"喂养的搜索里。
# 拔掉全部可疑 estate 生产者后只剩基石（度数/奇偶/成环）——无毒可下。出解 = 毒源实锤在被拔的三层里。
set -uo pipefail
BIN=/mnt/d/works/coil/coil-solver/bin/v77b
BOARD=/mnt/d/works/coil/coilbench/levels_all/767
CHECK=/mnt/d/works/coil/coilbench/coil_check/check
BANK=/mnt/d/works/coil/coil-solutions/solutions
W=260
LOG=/tmp/bed767.log; : > "$LOG"
WIN=/tmp/bed767.win; rm -f "$WIN"
J=${1:-18}; LIMIT=${2:-2400}
while read -r _ cell _; do
  [ -f "$WIN" ] && break
  X=$(( cell % W - 1 )); Y=$(( cell / W - 1 ))
  (
    S=$(date +%s)
    CHAIN=0 FLOW=0 DIRLAYER=0 PDEVERY=200 BJ=40 PINSTART=$X,$Y JOBS=1 \
      timeout "$LIMIT" "$BIN" "$BOARD" > "/tmp/bp_$cell.txt" 2>/dev/null
    T=$(( $(date +%s) - S ))
    if [ -s "/tmp/bp_$cell.txt" ] && "$CHECK" "$BOARD" "/tmp/bp_$cell.txt" >/dev/null 2>&1; then
      cp "/tmp/bp_$cell.txt" "$BANK/767.sol"
      echo "($X,$Y): ${T}s **出解!! 毒源实锤=chain/flow/dir 三层之一**" >> "$LOG"
      touch "$WIN"
    else
      echo "($X,$Y): ${T}s 无" >> "$LOG"
    fi
  ) &
  while [ "$(jobs -r | wc -l)" -ge "$J" ]; do
    sleep 4
    [ -f "$WIN" ] && { kill $(jobs -p) 2>/dev/null; break 2; }
  done
done < /tmp/cand_767.txt
wait 2>/dev/null
sort "$LOG"
[ -f "$WIN" ] && echo "== 767 攻陷(基石) ==" || echo "== 87 个基石钉全空 => 强筛探针本身误杀了真起点 =="
