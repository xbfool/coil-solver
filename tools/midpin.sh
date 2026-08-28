#!/usr/bin/env bash
# 基石深钉：无菌 estate（CHAIN=0 FLOW=0 DIRLAYER=0）+ 全深度树内传播（PROPDEPTH=999999 SEEDMODE=0）。
# 树内传播是基石级的（剩余图度数/奇偶/成环，PROOFS §3.6），从零播种零外层依赖——
# 它才是"死树 10M->15K"的那把刀；上一轮只给了 PDEVERY=200 的稀疏出场，慢 100 倍是自找的。
set -uo pipefail
BIN=/mnt/d/works/coil/coil-solver/bin/v77b
BOARD=/mnt/d/works/coil/coilbench/levels_all/767
CHECK=/mnt/d/works/coil/coilbench/coil_check/check
BANK=/mnt/d/works/coil/coil-solutions/solutions
W=260
LOG=/tmp/mid767.log; : > "$LOG"
WIN=/tmp/mid767.win; rm -f "$WIN"
J=${1:-18}; LIMIT=${2:-3600}
DONE=/tmp/done87.txt
while read -r _ cell _; do
  [ -f "$WIN" ] && break
  X=$(( cell % W - 1 )); Y=$(( cell / W - 1 ))
  
  (
    S=$(date +%s)
    CHAIN=0 FLOW=1 DIRLAYER=0 PDEVERY=20 BJ=20 PINSTART=$X,$Y JOBS=1 \
      timeout "$LIMIT" "$BIN" "$BOARD" > "/tmp/mid_$cell.txt" 2>/dev/null
    T=$(( $(date +%s) - S ))
    if [ -s "/tmp/mid_$cell.txt" ] && "$CHECK" "$BOARD" "/tmp/mid_$cell.txt" >/dev/null 2>&1; then
      cp "/tmp/mid_$cell.txt" "$BANK/767.sol"
      echo "($X,$Y): ${T}s **出解!!**" >> "$LOG"
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
[ -f "$WIN" ] && echo "== 767 攻陷 ==" || echo "== 剩余 40 深钉全空 =="
