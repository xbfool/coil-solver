#!/bin/bash
# 针基准（磨成本的纯度量）：钉真起点,测解出时间/节点 —— 科研标的A
# 用法: needlebench.sh <outfile.tsv> [并行=11] [超时=600] [bin=bin/champ1]
# 盘单: 零信息阶梯(真起点来自解库)。输出 TSV: 关 钉 wall_s nodes_total
set -u
OUTF="$1"; PAR="${2:-11}"; TMO="${3:-600}"; BIN="${4:-bin/champ1}"
ROOT=/mnt/d/works/coil/coil-solver
LV=/mnt/d/works/coil/coilbench/levels_all
cd "$ROOT"; ulimit -s unlimited 2>/dev/null
PINS="570 33,183 580 142,79 582 142,130 590 74,97 598 18,60 628 134,0 630 126,27 634 78,121 736 231,140 744 204,77 748 236,111"

run_one() {
    lv="$1"; pin="$2"
    line=$(timeout "$TMO" env PINSTART="$pin" STATS=1 RESTART=1 SWEEP=50000 JOBS=1 \
        "$BIN" "$LV/$lv" 2>&1 >/dev/null | grep -m1 '^STATS')
    ws=$(echo "$line" | grep -oE '"wall_s":[0-9]+' | cut -d: -f2)
    nt=$(echo "$line" | grep -oE '"nodes_total":[0-9]+' | cut -d: -f2)
    sv=$(echo "$line" | grep -oE '"solved":[0-9]' | cut -d: -f2)
    echo -e "$lv\t$pin\t${sv:-crash}\t${ws:--}\t${nt:--}"
}
export -f run_one; export TMO BIN LV

echo "$PINS" | tr ' ' '\n' | paste - - | \
  xargs -P "$PAR" -n2 bash -c 'run_one "$0" "$1"' >> "$OUTF"
sort -n -o "$OUTF" "$OUTF"
echo "== needlebench -> $OUTF"; cat "$OUTF"
