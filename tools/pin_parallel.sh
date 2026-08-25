#!/usr/bin/env bash
# 并行钉一批候选：从 cand 文件读格号，J 路并行，谁出解全停。
#   tools/pin_parallel.sh <关号> [并行度] [每候选上限秒]
set -uo pipefail
LV=${1:?}; J=${2:-10}; LIMIT=${3:-1200}
BIN=${COIL_ROOT:-/mnt/d/works}/coil-solver/bin/v73-dirlayer
BOARD=${COIL_ROOT:-/mnt/d/works}/coilbench/levels_all/$LV
BANK=${COIL_ROOT:-/mnt/d/works}/coil-solutions/solutions
CHECK=${COIL_ROOT:-/mnt/d/works}/coilbench/coil_check/check
W=$(( $(grep -oE "x=[0-9]+" "$BOARD" | head -1 | cut -d= -f2) + 2 ))
LOG=/tmp/pin_${LV}.log; : > "$LOG"
WIN=/tmp/win_${LV}.flag; rm -f "$WIN"
pin_one() {
  local cell=$1 x y S T out
  x=$(( cell % W - 1 )); y=$(( cell / W - 1 ))
  out=/tmp/pin_${LV}_${cell}.txt
  S=$(date +%s)
  PINSTART=$x,$y JOBS=1 timeout "$LIMIT" "$BIN" "$BOARD" > "$out" 2>/dev/null
  T=$(( $(date +%s) - S ))
  if [ -s "$out" ] && "$CHECK" "$BOARD" "$out" >/dev/null 2>&1; then
    cp "$out" "$BANK/$LV.sol"
    echo "钉($x,$y): ${T}s **出解✅已入库**" >> "$LOG"
    touch "$WIN"
  else
    echo "钉($x,$y): ${T}s 证伪/未解" >> "$LOG"
  fi
}
export -f pin_one 2>/dev/null || true
n=0
while read -r _ cell _; do
  [ -f "$WIN" ] && break
  pin_one "$cell" &
  n=$((n+1))
  while [ "$(jobs -r | wc -l)" -ge "$J" ]; do
    sleep 3
    [ -f "$WIN" ] && { kill $(jobs -p) 2>/dev/null; break 2; }
  done
done < "/tmp/cand_$LV.txt"
wait 2>/dev/null
cat "$LOG"
[ -f "$WIN" ] && echo "== 关 $LV 攻陷 ==" || echo "== 关 $LV 全部候选试完未解 =="
