#!/bin/bash
# ============================================================================
# 路由求解器 solve.sh —— 族路由定位器 + C funnel 兜底
# ----------------------------------------------------------------------------
# 对**任何关卡都正确**：correctness 由兜底(全 C funnel 全起点搜索)保证。
# flow 族：定位器秒级出紧候选 → STARTONLY 跳过全 funnel → swarm 求解（加速）。
# 有向族 / 定位失败 / flow 路径超时：自动退回全 C funnel（退化成"没用"，但仍正确）。
# 定位器区域已证是真候选**超集**(R3)，flow 族赢家必在其中；即便有 bug，兜底也兜住。
#
# 用法: solve.sh <board_file> <out_file> [flow路径超时秒=600]
# 退出: 0=解出(打印 ROUTED/FUNNEL 说明走了哪条路), 1=未解
# ============================================================================
set -u
BOARD="$1"; OUT="$2"; FT="${3:-600}"
ROOT=/mnt/d/works/coil/coil-solver
CK=/mnt/d/works/coil/coilbench/coil_check/check
ulimit -s unlimited 2>/dev/null
W=$(head -c 40 "$BOARD" | grep -oE 'x=[0-9]+' | grep -oE '[0-9]+')
WW=$((W + 2))
CAND="/tmp/rt_cand_$$.txt"; SO="/tmp/rt_so_$$.txt"
rm -f "$CAND" "$SO" "$OUT"

# --- Step 1: 族路由 + 定位（Python 裁判撒 16 样本判族, ~15-20s；big board 超 120s 则跳过）---
DUMPCAND="$CAND" timeout 120 python3 "$ROOT/research/referee/localize.py" "$BOARD" - >/dev/null 2>&1

# --- Step 2: flow 族快路径（有候选文件 = 找到模体）---
if [ -s "$CAND" ]; then
    awk -F, -v w=$WW '{print "OK", ($2 + 1) * w + ($1 + 1), 0}' "$CAND" > "$SO"
    timeout "$FT" env STARTORD="$SO" STARTONLY=1 BJ=500 JOBS=26 RESTART=1 SWEEP=3000000 \
        "$ROOT/bin/v78" "$BOARD" 999999 > "$OUT" 2>/dev/null
    if [ -s "$OUT" ] && "$CK" "$BOARD" "$OUT" >/dev/null 2>&1; then
        echo "ROUTED flow族定位器($(wc -l < "$CAND")候选)"; rm -f "$CAND" "$SO"; exit 0
    fi
fi

# --- Step 3: 兜底 —— 有向族 / 定位失败 / flow 超时 → 全 C funnel（对任何关卡正确）---
timeout 1780 env NESTPROBE=1 BJ=500 JOBS=26 RESTART=1 SWEEP=3000000 \
    "$ROOT/bin/v78" "$BOARD" 1780 > "$OUT" 2>/dev/null
rm -f "$CAND" "$SO"
if [ -s "$OUT" ] && "$CK" "$BOARD" "$OUT" >/dev/null 2>&1; then echo "FUNNEL兜底"; exit 0; fi
echo "UNSOLVED"; exit 1
