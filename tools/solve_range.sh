#!/usr/bin/env bash
# 批量解一段关卡，解出来先过官方校验器再入银行。
#
#   tools/solve_range.sh 502 510 [每关上限秒数] [JOBS]
#
# 为什么要先校验再入库：银行里已经出现过残缺条目（502~510 每个只有 30 步，
# 而盘面有近 2 万个自由格），直接进库会在打榜时卡死整条梯子。
set -uo pipefail

FROM=${1:?起始关号}; TO=${2:?结束关号}; LIMIT=${3:-1800}; JOBS=${4:-24}
HERE="$(cd "$(dirname "$0")/.." && pwd)"
BENCH="${COILBENCH_DIR:-$HERE/../coilbench}"
BANK="${COIL_BANK:-$HERE/../coil-solutions/solutions}"
SOLVER="${SOLVER:-$HERE/bin/v73-dirlayer}"   # v73=有向层默认开(660: 21s->2s)；可用 SOLVER= 覆盖

[ -x "$SOLVER" ] || cc -std=gnu99 -O3 -march=native -funroll-loops \
    "$HERE/versions/v58-probezero.c" -o "$SOLVER" || exit 1

for n in $(seq "$FROM" "$TO"); do
    lvl="$BENCH/levels_all/$n"
    [ -f "$lvl" ] || { echo "关 $n: 本地没有盘面，跳过"; continue; }
    if [ -f "$BANK/$n.sol" ] && [ -z "$("$BENCH/coil_check/check" "$lvl" "$BANK/$n.sol" 2>&1)" ]; then
        echo "关 $n: 银行里已有合法解，跳过"; continue
    fi
    echo "关 $n: 开跑（上限 ${LIMIT}s, JOBS=$JOBS）..."
    S=$(date +%s)
    JOBS=$JOBS timeout "$LIMIT" "$SOLVER" "$lvl" > "/tmp/sol_$n.txt" 2>/dev/null
    T=$(( $(date +%s) - S ))
    if [ ! -s "/tmp/sol_$n.txt" ]; then
        echo "关 $n: ${T}s 未解出 —— 继续下一关"; continue
    fi
    if msg=$("$BENCH/coil_check/check" "$lvl" "/tmp/sol_$n.txt" 2>&1); [ -n "$msg" ]; then
        echo "关 $n: ${T}s 出解但**校验不过**（$msg）—— 不入库"; continue
    fi
    cp "/tmp/sol_$n.txt" "$BANK/$n.sol"
    echo "关 $n: ${T}s 解出并通过校验 ✅ 已入库"
done
