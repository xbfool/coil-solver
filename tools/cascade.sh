#!/usr/bin/env bash
# EVOLVE-PLAN P0: 级联评审 —— 候选先过 soundness 硬门，再逐级花钱。
#
#   tools/cascade.sh versions/v78-rpbj.c            # 全流程 T0→T1
#   tools/cascade.sh bin/v78stats --t0-only         # 只跑硬门
#   tools/cascade.sh bin/v78stats --tag exp42       # vec 归档名
#
# T0 soundness 硬门（一票否决，767 案教训固化）:
#   - 编译（若给 .c）
#   - REFSOL 回放集 calib/refsol.txt: 每关带真解跑，
#     出现 "!!"（断言违例）/ 崩溃（信号）/ 未解出 / 官方 check 拒绝 => FAIL
# T1 校准套件: calib/list.txt，evalvec 产 fingerprint 向量，
#   全 solved+verified 才 PASS；向量归档 results/vec/<tag>.jsonl
# T2/T3（中档 600s / frontier）尚未接：先人工用 ./bench 与 tools/solve.sh。
set -uo pipefail

HERE="$(cd "$(dirname "$0")/.." && pwd)"
CB="${COILBENCH_DIR:-$HERE/../coilbench}"
SOLBANK="${COIL_SOLBANK:-$HERE/../coil-solutions/solutions}"
LEVELS="$CB/levels_all"
CHECK="$CB/coil_check/check"

TARGET="${1:?用法: cascade.sh <versions/xxx.c|bin/xxx> [--t0-only] [--tag NAME]}"; shift
T0_ONLY=0; TAG="cascade-$(date +%m%d-%H%M%S)"
while [ $# -gt 0 ]; do case "$1" in
  --t0-only) T0_ONLY=1;; --tag) TAG="$2"; shift;;
  *) echo "未知参数 $1" >&2; exit 1;; esac; shift; done

# ---- 编译（如需）----
BIN="$TARGET"
if [[ "$TARGET" == *.c ]]; then
  NAME="$(basename "$TARGET" .c)"
  BIN="$HERE/bin/cascade-$NAME"
  echo "== T0.compile $TARGET"
  cc -std=gnu99 -O3 -march=native -funroll-loops "$TARGET" -o "$BIN" 2> >(tail -5 >&2) \
    || { echo "T0 FAIL: 编译失败"; exit 3; }
fi
[ -x "$BIN" ] || { echo "T0 FAIL: 没有可执行文件 $BIN"; exit 3; }

# ---- T0: REFSOL 回放硬门 ----
T0_TIMEOUT="${T0_TIMEOUT:-90}"
echo "== T0.refsol（每关 ${T0_TIMEOUT}s，违例/崩溃/未解/假解 一票否决）"
TMPD="$(mktemp -d)"; trap 'rm -rf "$TMPD"' EXIT
while read -r lv; do
  lv="${lv%%#*}"; lv="$(echo "$lv" | tr -d '[:space:]')"; [ -n "$lv" ] || continue
  SOL="$SOLBANK/$lv.sol"
  [ -f "$SOL" ] || { echo "T0 FAIL: 解库缺 $SOL（硬门不许静默跳关）"; exit 3; }
  ERR="$TMPD/$lv.err"; OUT="$TMPD/$lv.out"
  STATS=1 JOBS=1 REFSOL="$SOL" timeout "$T0_TIMEOUT" "$BIN" < "$LEVELS/$lv" > "$OUT" 2> "$ERR"
  rc=$?
  if [ $rc -ge 128 ]; then echo "T0 FAIL: L$lv 崩溃（信号 $((rc-128))）—— TAINTED"; exit 3; fi
  if grep -q '!!' "$ERR"; then echo "T0 FAIL: L$lv REFSOL 断言违例:"; grep '!!' "$ERR" | head -3; exit 3; fi
  if ! grep -q '^x=.*&path=' "$OUT"; then echo "T0 FAIL: L$lv 带真解都解不出（rc=$rc）"; exit 3; fi
  if ! "$CHECK" "$LEVELS/$lv" <(grep '^x=.*&path=' "$OUT" | tail -1) >/dev/null 2>&1; then
    echo "T0 FAIL: L$lv 🚨 输出假解（官方 check 拒绝）"; exit 3
  fi
  echo "  L$lv ok"
done < "$HERE/calib/refsol.txt"
echo "== T0 PASS"
[ "$T0_ONLY" = 1 ] && exit 0

# ---- T1: 校准套件 fingerprint ----
T1_TIMEOUT="${T1_TIMEOUT:-15}"
VEC="$HERE/results/vec/$TAG.jsonl"
mkdir -p "$HERE/results/vec"
echo "== T1.calib（每关 ${T1_TIMEOUT}s → $VEC）"
python3 "$HERE/tools/evalvec.py" "$BIN" --list "$HERE/calib/list.txt" \
  --timeout "$T1_TIMEOUT" --out "$VEC" --coilbench "$CB"
rcv=$?
[ $rcv -eq 2 ] && { echo "T1 FAIL: 🚨 UNSOUND（假解）"; exit 3; }
NFAIL=$(python3 -c "
import json,sys
n=0
for l in open('$VEC',encoding='utf-8'):
    r=json.loads(l)
    if not r['solved'] or r.get('verified')==0: n+=1; print('  FAIL L'+r['level'],file=sys.stderr)
print(n)")
if [ "$NFAIL" != "0" ]; then echo "== T1 FAIL: $NFAIL 关未过"; exit 4; fi
echo "== T1 PASS，向量在 $VEC"
python3 -c "
import json
rs=[json.loads(l) for l in open('$VEC',encoding='utf-8')]
tn=sum(r['nodes_total'] for r in rs); tm=sum(r['wall_ms'] for r in rs)
print(f'== 汇总: {len(rs)} 关, nodes_total={tn:,}, wall={tm}ms')"
