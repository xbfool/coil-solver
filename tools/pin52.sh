#!/usr/bin/env bash
# 审讯实验一（767 案，首选）：GBASE=0 逐钉 52 疑犯 —— 绕开嫌疑层（全局底座）直接搜。
#
# 背景：87 候选中 52 个死在"g_estate 之上"的漏斗第 1/2 层，从未被搜索过；
#       g_estate 是唯一没有裁判冗余的层。空底座下这 52 个全部存活（PROBEDUMP 语义）。
# 结局语义：
#   出解        => 767 破案收官 + 全局底座定罪（那 52 个不该死）
#   全部干净穷尽 => 空底座语义下 87 全灭 —— 与裁判/营救演算矛盾升级，回炉审 §3.5/流证明本身
#   CRASH/超时  => 记账，别混成"无"
# 用法：tools/pin52.sh [并发=20] [每钉超时=1800]
set -uo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$HERE/bin/v77b"
BOARD=/mnt/d/works/coilbench/levels_all/767
CHECK=/mnt/d/works/coilbench/coil_check/check
LIST="$HERE/research/767/suspects52.txt"
OUT="$HERE/research/767/pin52"
mkdir -p "$OUT"
J=${1:-20}; T=${2:-1800}
WIN="$OUT/WIN"; rm -f "$WIN"
: > "$OUT/ledger.tsv"
ulimit -s unlimited

while read -r X Y; do
  [ -f "$WIN" ] && break
  (
    t0=$(date +%s)
    GBASE=0 DIRLAYER=0 PDEVERY=20 BJ=20 PINSTART=$X,$Y TREELOG=1 \
      timeout "$T" "$BIN" "$BOARD" > "$OUT/$X-$Y.out" 2> "$OUT/$X-$Y.err"
    rc=$?; dt=$(( $(date +%s) - t0 ))
    if [ -s "$OUT/$X-$Y.out" ] && "$CHECK" "$BOARD" "$OUT/$X-$Y.out" >/dev/null 2>&1; then
      v="**SOLVED**"; cp "$OUT/$X-$Y.out" "$HERE/solutions/767.sol"; touch "$WIN"
    elif [ $rc -eq 124 ]; then v=TIMEOUT
    elif [ $rc -ge 128 ]; then v="CRASH(sig$((rc-128)))"
    elif grep -q "该起点已被全局过滤剔除" "$OUT/$X-$Y.err"; then v=FILTERED
    elif grep -q "no solution found" "$OUT/$X-$Y.err"; then v=NOSOL
    else v="ERROR(rc=$rc)"; fi
    printf '%s,%s\t%s\t%ss\n' "$X" "$Y" "$v" "$dt" >> "$OUT/ledger.tsv"
  ) &
  while [ "$(jobs -r | wc -l)" -ge "$J" ]; do
    sleep 3
    [ -f "$WIN" ] && { kill $(jobs -p) 2>/dev/null; break 2; }
  done
done < "$LIST"
wait 2>/dev/null
sort "$OUT/ledger.tsv"
[ -f "$WIN" ] && echo "== 767 攻陷 + 全局底座定罪（解已存 solutions/767.sol）==" \
             || echo "== 未出解：账本见 $OUT/ledger.tsv（NOSOL=空底座下也搜穷,矛盾升级）=="
