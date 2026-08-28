#!/usr/bin/env bash
# 阳性对照（信任重建 Phase 1）：把已知真起点钉进 767 战役用过的每一种配置 —— 必须出解。
#
# midpin.sh 的教训全部修掉：
#   · stderr 保留（PINSTART 命中/剔除 那行就在里面）
#   · 判决四分类：SOLVED / FILTERED(全局过滤剔除,未搜索) / NOSOL(声称搜穷,真起点上=假证伪现行!)
#     / TIMEOUT(墙钟,无结论) —— 绝不把它们混成一个"无"
#   · TREELOG=1 常开，每起点的 r 码/节点数留档
#
# 用法：tools/postrust.sh [timeout秒=1200] [并发=20]
# 产出：research/postrust/ledger.tsv + 每次运行的 .out/.err 全保留
set -uo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$HERE/bin/v77b"
LV=/mnt/d/works/coil/coilbench/levels_all
CHECK=/mnt/d/works/coil/coilbench/coil_check/check
BANK=/mnt/d/works/coil/coil-solutions/solutions
OUT="$HERE/research/postrust"
mkdir -p "$OUT"
T=${1:-1200}; JMAX=${2:-20}

LEVELS="747 761 762 763 764 765 766 768"

# 配置矩阵：名字|环境变量串。全部是 767 战役实际用过(或修正后)的配置。
CFGS=(
  "allon|"
  "midpin|CHAIN=0 FLOW=1 DIRLAYER=0 PDEVERY=20 BJ=20"
  "verdict_fixed|CHAINTURN=0 CHAINSTRICT=0 CHAINFRO=0 FLOW=1 DIRLAYER=0 SWEEP=400000 PDEVERY=20 BJ=20"
  "verdict_orig|CHAIN=0 DIRLAYER=0 SWEEP=400000 PDEVERY=20 BJ=20"
)

sol_path() {  # 747 在本仓库 solutions/（force-add 的），其余在解银行
  local L=$1
  if [ -f "$HERE/solutions/$L.sol" ]; then echo "$HERE/solutions/$L.sol"; else echo "$BANK/$L.sol"; fi
}

run_one() {
  local L=$1 name=$2 envs=$3
  local sol; sol=$(sol_path "$L")
  local xy; xy=$(grep -o 'x=[0-9]*&y=[0-9]*' "$sol" | head -1)
  local X=${xy#x=}; X=${X%%&*}; local Y=${xy#*y=}
  local o="$OUT/$L.$name.out" e="$OUT/$L.$name.err"
  local t0=$(date +%s)
  ulimit -s unlimited 2>/dev/null   # dfs 递归 2万+ 层，8MB 默认栈必炸（二进制里也有 setrlimit，双保险）
  env $envs PINSTART=$X,$Y TREELOG=1 timeout "$T" "$BIN" "$LV/$L" >"$o" 2>"$e"
  local rc=$? t1=$(date +%s)
  local verdict
  if grep -q "该起点已被全局过滤剔除" "$e"; then verdict=FILTERED
  elif [ -s "$o" ]; then
    if "$CHECK" "$LV/$L" "$o" >/dev/null 2>&1; then verdict=SOLVED; else verdict=BADSOL; fi
  elif [ $rc -eq 124 ]; then verdict=TIMEOUT
  elif [ $rc -ge 128 ]; then verdict="CRASH(sig$((rc-128)))"
  elif [ $rc -eq 3 ] || grep -q "TAINTED" "$e"; then verdict=TAINTED
  elif grep -q "no solution found" "$e"; then verdict=NOSOL
  else verdict="ERROR(rc=$rc)"; fi
  printf '%s\t%s\t%s\t%ss\t%s\n' "$L" "$name" "$verdict" "$((t1-t0))" "start=($X,$Y)" >> "$OUT/ledger.tsv"
}

: > "$OUT/ledger.tsv"
for cfg in "${CFGS[@]}"; do
  name=${cfg%%|*}; envs=${cfg#*|}
  for L in $LEVELS; do
    run_one "$L" "$name" "$envs" &
    while [ "$(jobs -r | wc -l)" -ge "$JMAX" ]; do sleep 2; done
  done
done
wait
echo "=== 阳性对照账本（真起点上只有 SOLVED 是合格；NOSOL=假证伪现行；FILTERED=过滤误杀现行）==="
sort "$OUT/ledger.tsv" | column -t
NBAD=$(grep -cE 'NOSOL|FILTERED|BADSOL' "$OUT/ledger.tsv" || true)
echo "不合格数: $NBAD"
