#!/usr/bin/env bash
# 767 可靠性猎凶：逐层消融强筛选，看哪一层关掉后幸存者变多（多出来的=被那层误杀的嫌疑人）
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
LV=${1:-767}
run_one() {  # $1=名字 $2=环境串
  env $2 PROBEDUMP=1 STRONG=1 JOBS=1 timeout 3600 bin/v77-deepprune \
    "$ROOT/../coilbench/levels_all/$LV" 2>/dev/null | grep "^OK" > /tmp/abl_$1.txt
  echo done > /tmp/abl_$1.done
}
run_one dir0  "DIRLAYER=0" &
run_one chain0 "CHAIN=0" &
run_one flow0 "FLOW=0" &
run_one subt0 "SUBTOUR=0" &
wait
