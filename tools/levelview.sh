#!/bin/bash
# 本地关卡浏览器：静态服务三仓库父目录, 打开 levelviewer.html
# 用法: tools/levelview.sh [端口=8814]
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${1:-8814}"
echo "== coil 关卡浏览器: http://localhost:$PORT/coil-solver/tools/levelviewer.html"
cd "$ROOT/.."
exec python3 -m http.server "$PORT"
