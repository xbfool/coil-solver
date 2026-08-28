#!/bin/bash
# 814 逐钉长磨（767 终局配方,半算力版）—— EVOLVE/CASE-814 第三枪
# 用法: pingrind.sh <board> <survdir前缀> <outdir> [并行=13] [每钉秒=1800] [bin=bin/champ1]
# 幸存者格号 = FUNNELDUMP 落盘的内部索引, x=s%(W+2)-1, y=s/(W+2)-1
set -u
BOARD="$1"; SURV="$2"; OUT="$3"; PAR="${4:-13}"; BOX="${5:-1800}"; BIN="${6:-bin/champ1}"
ROOT=/mnt/d/works/coil/coil-solver
CK=/mnt/d/works/coil/coilbench/coil_check/check
cd "$ROOT"; mkdir -p "$OUT"
W=$(head -c 30 "$BOARD" | grep -oE 'x=[0-9]+' | grep -oE '[0-9]+'); WW=$((W+2))
ulimit -s unlimited 2>/dev/null

cat "$SURV".* | sort -un | while read -r s; do
    [ -n "$s" ] && echo "$((s % WW - 1)),$((s / WW - 1))"
done > "$OUT/pins.txt"
N=$(wc -l < "$OUT/pins.txt")
echo "== $N 钉, 并行 $PAR, 每钉 ${BOX}s, 二进制 $BIN"

grind_one() {
    pin="$1"
    [ -f "$OUT/FOUND" ] && return 0
    logf="$OUT/pin_${pin//,/_}.log"
    timeout "$BOX" env PINSTART="$pin" STATS=1 RESTART=1 SWEEP=50000 JOBS=1 \
        "$BIN" "$BOARD" > "$OUT/sol_${pin//,/_}.txt" 2> "$logf"
    rc=$?
    if [ $rc -ge 128 ] && [ $rc -ne 124 ] && [ $((rc-128)) -ne 15 ] && [ $((rc-128)) -ne 13 ]; then
        echo "TAINTED $pin rc=$rc" >> "$OUT/tainted.txt"   # 崩溃必须记账(767教训)
    fi
    if [ -s "$OUT/sol_${pin//,/_}.txt" ] && "$CK" "$BOARD" "$OUT/sol_${pin//,/_}.txt" >/dev/null 2>&1; then
        cp "$OUT/sol_${pin//,/_}.txt" "$OUT/SOLUTION.txt"
        echo "$pin" > "$OUT/FOUND"
        echo "🎯 SOLVED at pin $pin"
    fi
}
export -f grind_one; export OUT BOX BOARD BIN CK
xargs -P "$PAR" -I{} bash -c 'grind_one {}' < "$OUT/pins.txt"
[ -f "$OUT/FOUND" ] && { echo "DONE: $(cat "$OUT/FOUND")"; exit 0; }
echo "ALL PINS EXHAUSTED (unsolved in box)"; exit 1
