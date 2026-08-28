#!/usr/bin/env bash
# 猎凶钉法：把消融名单相对基线 87 的"多出来的幸存者"逐个钉住搜索。
# ⚠ 钉的时候必须带上同样的消融环境（嫌疑规则保持关闭），否则它会把真起点再杀一次。
#   tools/pin_extras.sh <消融文件> <环境串> [并行度] [每个上限秒]
set -uo pipefail
ABL=${1:?消融 OK 文件}; ENVS=${2:?环境串如 CHAINSTRICT=0}; J=${3:-12}; LIMIT=${4:-900}
BASE=/tmp/cand_767.txt
BIN=/mnt/d/works/coil/coil-solver/bin/v77b
BOARD=/mnt/d/works/coil/coilbench/levels_all/767
CHECK=/mnt/d/works/coil/coilbench/coil_check/check
BANK=/mnt/d/works/coil/coil-solutions/solutions
W=260
LOG=/tmp/pinex_$(basename "$ABL").log; : > "$LOG"
# 差集：消融幸存 − 基线87
awk '{print $2}' "$BASE" | sort > /tmp/_base87.txt
awk '{print $2}' "$ABL" | sort | comm -23 - /tmp/_base87.txt > /tmp/_extras.txt
NE=$(wc -l < /tmp/_extras.txt)
echo "差集 $NE 个（$ENVS）" | tee -a "$LOG"
n=0
while read C; do
  X=$(( C % W - 1 )); Y=$(( C / W - 1 ))
  (
    S=$(date +%s)
    env $ENVS PINSTART=$X,$Y PDEVERY=200 BJ=40 JOBS=1 timeout "$LIMIT" "$BIN" "$BOARD" > "/tmp/px_$C.txt" 2>/dev/null
    T=$(( $(date +%s) - S ))
    if [ -s "/tmp/px_$C.txt" ] && "$CHECK" "$BOARD" "/tmp/px_$C.txt" >/dev/null 2>&1; then
      cp "/tmp/px_$C.txt" "$BANK/767.sol"
      echo "($X,$Y): ${T}s **出解!! 凶手=$ENVS 那条规则**" >> "$LOG"
      echo won > /tmp/c767.done
    else
      echo "($X,$Y): ${T}s 证伪" >> "$LOG"
    fi
  ) &
  n=$((n+1))
  while [ "$(jobs -r | wc -l)" -ge "$J" ]; do
    sleep 3
    [ -f /tmp/c767.done ] && { kill $(jobs -p) 2>/dev/null; exit 0; }
  done
done < /tmp/_extras.txt
wait 2>/dev/null
grep -c 证伪 "$LOG" || true
tail -2 "$LOG"
