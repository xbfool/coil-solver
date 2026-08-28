#!/usr/bin/env bash
# 破硬关的标准打法（703 实测 79s 攻陷）：
#   1) PROBEDUMP STRONG=1 全盘强筛选（传播+flow+链+有向层），拿到极小候选集（703 是 8 个）；
#   2) 逐个 PINSTART 真钉住跑——绝大多数候选秒级被完整搜索证伪，真起点很快出解；
#   3) 出解立即过官方校验并入库。
#
#   tools/crack_hard.sh <关号> [每候选上限秒] [最多候选数]
#
# ⚠ 前提：二进制必须真解析 PINSTART（看 stderr 有没有"命中，只跑这一个起点"）。
#   v58->v73 谱系曾静默忽略它，所有"钉住"测试实际是全盘跑，"703 树价"的诊断因此错了一轮。
set -uo pipefail
LV=${1:?关号}; LIMIT=${2:-1200}; MAXC=${3:-40}
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN=${BIN:-${COIL_ROOT:-$ROOT/..}/coil-solver/bin/v73-dirlayer}
BOARD=${COIL_ROOT:-$ROOT/..}/coilbench/levels_all/$LV
BANK=${COIL_ROOT:-$ROOT/..}/coil-solutions/solutions
CHECK=${COIL_ROOT:-$ROOT/..}/coilbench/coil_check/check

W=$(( $(grep -oE "x=[0-9]+" "$BOARD" | head -1 | cut -d= -f2) + 2 ))
echo "== 关 $LV：强筛选找候选 =="
PROBEDUMP=1 STRONG=1 JOBS=1 timeout 3600 "$BIN" "$BOARD" 2>/dev/null | grep "^OK" > "/tmp/cand_$LV.txt"
NC=$(wc -l < "/tmp/cand_$LV.txt")
echo "候选 $NC 个"
[ "$NC" -eq 0 ] && { echo "候选为 0（异常，强筛选可能证伪了全部）"; exit 1; }
[ "$NC" -gt "$MAXC" ] && { echo "候选太多（>$MAXC），这关不属于'针尖类'，另行处理"; exit 1; }

while read -r _ cell _; do
  x=$(( cell % W - 1 )); y=$(( cell / W - 1 ))
  printf "钉 (%s,%s): " "$x" "$y"
  S=$(date +%s)
  PINSTART=$x,$y JOBS=1 timeout "$LIMIT" "$BIN" "$BOARD" > "/tmp/hd_${LV}.txt" 2>/dev/null
  T=$(( $(date +%s) - S ))
  if [ -s "/tmp/hd_${LV}.txt" ]; then
    if "$CHECK" "$BOARD" "/tmp/hd_${LV}.txt" >/dev/null 2>&1; then
      cp "/tmp/hd_${LV}.txt" "$BANK/$LV.sol"
      echo "${T}s 出解 ✅ 过校验，已入库"
      exit 0
    fi
    echo "${T}s 出解但校验不过 ⚠（不入库，继续）"
  else
    echo "${T}s 证伪/未解"
  fi
done < "/tmp/cand_$LV.txt"
echo "关 $LV：所有候选试完未出解"
exit 2
