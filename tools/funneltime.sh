#!/bin/bash
# 漏斗计时: 起跑到 FUNNELDUMP 全落盘的墙钟 + 幸存者数 —— 科研标的B 的度量
# 用法: funneltime.sh <关号> [JOBS=8] [超时=900] [bin=bin/champ1]
set -u
LV="$1"; J="${2:-8}"; TMO="${3:-900}"; BIN="${4:-bin/champ1}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"; ulimit -s unlimited 2>/dev/null
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
start=$(date +%s)
timeout "$TMO" env FUNNELDUMP="$D/surv" STATS=1 RESTART=1 SWEEP=50000 JOBS="$J" \
    "$BIN" "$ROOT/../coilbench/levels_all/$LV" >/dev/null 2>"$D/log" &
pid=$!
while kill -0 "$pid" 2>/dev/null; do
    n=$(ls "$D"/surv.* 2>/dev/null | wc -l)
    [ "$n" -ge "$J" ] && break
    [ $(( $(date +%s) - start )) -gt "$TMO" ] && break
    sleep 2
done
wall=$(( $(date +%s) - start ))
sleep 1
surv=$(cat "$D"/surv.* 2>/dev/null | sort -un | wc -l)
dumps=$(ls "$D"/surv.* 2>/dev/null | wc -l)
kill -- -"$pid" 2>/dev/null; kill "$pid" 2>/dev/null
pkill -P "$pid" 2>/dev/null
echo -e "level=$LV\tjobs=$J\twall=${wall}s\tdumps=$dumps\tsurv=$surv"
