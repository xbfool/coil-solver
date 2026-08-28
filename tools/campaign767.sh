#!/usr/bin/env bash
# 767 战役（栈溢出修复后的第一枪）—— 先控制组后开火，全程账本。
#
# 阶段 1（控制组）：768 全漏斗（非钉），与 767 完全同构的调用形态。必须出解且无 TAINTED，
#                   否则脚手架红灯，禁止进入阶段 2。
# 阶段 2（开火）：767 同配置，时间盒 ${T767}s。三种结局都有明确语义：
#     出解         => 破案收官（顺手过官方 check 入库）
#     TAINTED/CRASH => 还有崩溃形态，回脚手架
#     干净无解     => 传播层把真起点杀了(与 SOLWALK 结论矛盾) => 重启独立裁判(Phase 2)审计
#
# 配置 = 747 攻陷配方（PDEVERY/BJ/RESTART + SWEEP=400000）+ 全默认规则链。
# 用法：tools/campaign767.sh [JOBS=20] [T767=7200]
set -uo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$HERE/bin/v77b"
LV="$HERE/../coilbench/levels_all"
CHECK="$HERE/../coilbench/coil_check/check"
OUT="$HERE/research/campaign767"
mkdir -p "$OUT"
J=${1:-20}; T767=${2:-7200}
ulimit -s unlimited

run() {  # run <关号> <超时> —— 返回：0 出解 / 2 TAINTED / 3 崩 / 1 干净无解 / 4 超时
  local L=$1 T=$2
  local o="$OUT/$L.out" e="$OUT/$L.err"
  local t0=$(date +%s)
  SWEEP=400000 PDEVERY=20 BJ=20 RESTART=1 JOBS=$J TREELOG=1 \
    timeout "$T" "$BIN" "$LV/$L" >"$o" 2>"$e"
  local rc=$? t1=$(date +%s)
  local dt=$((t1-t0))
  if [ -s "$o" ] && "$CHECK" "$LV/$L" "$o" >/dev/null 2>&1; then
    echo "L$L: **出解** ${dt}s，官方 check 通过（$o）"; return 0
  elif [ -s "$o" ]; then echo "L$L: 有输出但 check 不过（${dt}s）—— BADSOL，查 $o"; return 2
  elif [ $rc -eq 3 ] || grep -q TAINTED "$e"; then echo "L$L: TAINTED（${dt}s）—— 有 shard 崩溃，回脚手架"; return 2
  elif [ $rc -ge 128 ]; then echo "L$L: CRASH sig$((rc-128))（${dt}s）"; return 3
  elif [ $rc -eq 124 ]; then echo "L$L: 时间盒到（${dt}s），无结论"; return 4
  else echo "L$L: 干净无解（${dt}s, rc=$rc）"; return 1
  fi
}

echo "=== 阶段 1：控制组 768 全漏斗（同构调用）==="
if ! run 768 1800; then
  echo "控制组未出解 —— 脚手架红灯，不打 767。查 $OUT/768.err"; exit 1
fi

echo "=== 阶段 2：767 开火（时间盒 ${T767}s）==="
run 767 "$T767"
rc=$?
if [ $rc -eq 0 ]; then
  cp "$OUT/767.out" "$HERE/solutions/767.sol"
  echo "== 767 攻陷，解已存 solutions/767.sol =="
fi
exit $rc
