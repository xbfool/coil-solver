#!/usr/bin/env bash
# 一条命令把开发环境备齐：拉 coilbench、编译校验器、装全部 1207 关（奇+偶）。
#
#   wsl -e bash -lc "cd /path/to/coil-solver && ./setup.sh"
#
# 为什么关卡不进 git：奇 148MB + 偶 150MB ≈ 300MB，单个文件最大 3.9MB，
# 每次 clone 都要拖一遍；而上游 release 本来就有 47MB 的压缩包，就地重建更划算。
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
BENCH="${COILBENCH_DIR:-$HERE/../coilbench}"
PASS="${COIL_FULL_PASSWORD:-local}"      # 偶数关归档的口令，本机自用，随便设

echo "==> coilbench: $BENCH"
if [ ! -d "$BENCH" ]; then
  git clone https://github.com/adum/coilbench.git "$BENCH"
fi
# Windows 上 clone 出来的 CRLF 会把 shebang 弄坏（/usr/bin/env: 'python3\r'）
git -C "$BENCH" config core.autocrlf false
git -C "$BENCH" rm --cached -r -q . >/dev/null 2>&1 || true
git -C "$BENCH" reset --hard -q

echo "==> 编译校验器"
[ -x "$BENCH/coil_check/check" ] || make -C "$BENCH/coil_check"

echo "==> 关卡（奇数明文 + 偶数加密归档）"
if [ "$(ls "$BENCH/levels_public" 2>/dev/null | wc -l)" -lt 600 ] || [ ! -f "$BENCH/levels_secret_even.tar.enc" ]; then
  ( cd "$BENCH" && printf '%s\n%s\n' "$PASS" "$PASS" | ./download_full_levels.sh )
fi

echo "==> 偶数关解到 levels_all/（我们自己写求解器，不看答案，泄漏那条顾虑对我们不适用）"
mkdir -p "$BENCH/levels_all"
if [ "$(ls "$BENCH/levels_all" | wc -l)" -lt 1200 ]; then
  cp -a "$BENCH"/levels_public/* "$BENCH/levels_all/"
  tmp="$(mktemp -d)"
  printf '%s\n' "$PASS" | openssl enc -d -aes-256-cbc -pbkdf2 -pass stdin \
      -in "$BENCH/levels_secret_even.tar.enc" -out "$tmp/even.tar"
  tar -xf "$tmp/even.tar" -C "$tmp"
  find "$tmp" -type f -regex '.*/[0-9]+' -exec cp -a {} "$BENCH/levels_all/" \;
  rm -rf "$tmp"
fi

echo
echo "奇数关 $(ls "$BENCH/levels_public" | wc -l) 个，全部关卡 $(ls "$BENCH/levels_all" | wc -l) 个"
echo "口令（跑 evaluate_full.py 要用）：COIL_FULL_PASSWORD=$PASS"
echo
echo "跑法："
echo "  ./bench v23-adaptive --start 1 --timeout 600            # 官方奇数关口径"
echo "  COIL_FULL_PASSWORD=$PASS ./bench-full v23-adaptive      # 全量口径（奇+偶）"
