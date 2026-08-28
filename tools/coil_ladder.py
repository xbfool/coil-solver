#!/usr/bin/env python3
"""hacker.org Mortal Coil 打榜客户端 —— 用离线解的银行喂榜单。

为什么这个文件必须进仓库：家里那台机器上的打榜循环没提交，
换一台机器就得从零重新逆向一遍协议（这次花了 5 次请求）。

=== 三条硬性约束（曾把 hacker.org 打挂过，见 notes.md「在线全面停火」）===

1. **一律 POST。** 官方 coil.js 里那行是坏的：
       '<form action="./" method"' + method + '" ...'
                          ^^^^^^ 漏了等号
   属性名成了 `method"POST"`，浏览器只好退回默认的 GET，长路径直接撞 414 ——
   那次事故的真凶。我们不复现这个 bug。
2. **限速**：每次请求之间至少 --rate 秒（默认 2.0）。
3. **指数退避 + 熔断**：出错 1→2→4…封顶 60 秒；连续失败到 --max-fail 次就停。

=== 关卡编号对不上，所以按盘面内容匹配 ===

线上页面显示的 "Level: N" 与本地 coilbench/levels_all 的编号不是一回事
（实测线上 "Level: 499" 给的盘面 == 本地 501 关）。靠偏移量硬套迟早错关，
所以建一张 盘面内容 -> 本地关号 的索引，按内容精确匹配 —— 对任何编号偏移免疫。
"""

import argparse
import hashlib
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

BASE = "https://www.hacker.org/coil/index.php"
UA = "coil-solver/1.0 (personal research; contact via hacker.org user xbfool)"

# 当前盘面是页面里第一个 curLevel/boardStr 脚本块（后面那些 0/1/25 是首页演示用的小盘）
BOARD_RE = re.compile(
    r'var\s+curLevel\s*=\s*(\d+);\s*var\s+width\s*=\s*(\d+);\s*'
    r'var\s+height\s*=\s*(\d+);\s*var\s+boardStr\s*=\s*"([^"]*)"'
)
SOL_RE = re.compile(r'x=(\d+)&y=(\d+)&path=([UDLR]*)')


def board_key(w, h, board):
    return hashlib.md5(f"{w}x{h}:{board}".encode()).hexdigest()


def build_index(levels_dir):
    """本地关卡 -> {盘面指纹: 关号}。文件格式 x=<w>&y=<h>&board=<...>"""
    idx = {}
    for name in os.listdir(levels_dir):
        if not name.isdigit():
            continue
        try:
            with open(os.path.join(levels_dir, name)) as f:
                txt = f.read().strip()
        except OSError:
            continue
        m = re.match(r'x=(\d+)&y=(\d+)&board=(.*)', txt, re.S)
        if m:
            idx[board_key(m.group(1), m.group(2), m.group(3).strip())] = int(name)
    return idx


class Client:
    def __init__(self, user, pw, rate, max_fail):
        self.auth = urllib.parse.urlencode({"name": user, "password": pw})
        self.rate = rate
        self.max_fail = max_fail
        self.last = 0.0
        self.fails = 0

    def _throttle(self):
        wait = self.rate - (time.time() - self.last)
        if wait > 0:
            time.sleep(wait)
        self.last = time.time()

    def _request(self, data=None):
        """限速 + 指数退避。data 为 None 走 GET(取盘面)，否则 POST(交解)。"""
        backoff = 1.0
        while True:
            self._throttle()
            req = urllib.request.Request(
                f"{BASE}?{self.auth}",
                data=urllib.parse.urlencode(data).encode() if data else None,
                headers={"User-Agent": UA},
            )
            try:
                with urllib.request.urlopen(req, timeout=60) as r:
                    self.fails = 0
                    return r.read().decode("utf-8", "replace")
            except (urllib.error.URLError, OSError, TimeoutError) as e:
                self.fails += 1
                if self.fails >= self.max_fail:
                    raise SystemExit(f"连续失败 {self.fails} 次，熔断退出：{e}")
                print(f"  ! 请求失败({e})，退避 {backoff:.0f}s "
                      f"[{self.fails}/{self.max_fail}]", flush=True)
                time.sleep(backoff)
                backoff = min(backoff * 2, 60.0)

    def fetch_board(self):
        html = self._request()
        m = BOARD_RE.search(html)
        if not m:
            return None, html
        return (int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4)), html

    def submit(self, x, y, path):
        return self._request({"x": x, "y": y, "path": path})


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--levels", default=str(ROOT.parent / "coilbench" / "levels_all"))
    ap.add_argument("--solutions", default=str(ROOT.parent / "coil-solutions" / "solutions"))
    ap.add_argument("--rate", type=float, default=2.0, help="请求最小间隔秒数")
    ap.add_argument("--max-fail", type=int, default=5, help="连续失败多少次熔断")
    ap.add_argument("--max", type=int, default=0, help="本次最多提交多少关(0=不限)")
    ap.add_argument("--dry-run", action="store_true", help="只取盘面和查解，不提交")
    args = ap.parse_args()

    user = os.environ.get("COIL_USER")
    pw = os.environ.get("COIL_PASS")
    if not (user and pw):
        raise SystemExit("需要 COIL_USER / COIL_PASS 环境变量（从 1Password 现取，别写进仓库）")

    idx = build_index(args.levels)
    print(f"本地关卡索引 {len(idx)} 个盘面；限速 {args.rate}s"
          f"{'；DRY-RUN（不提交）' if args.dry_run else ''}", flush=True)

    cli = Client(user, pw, args.rate, args.max_fail)
    done = 0
    last_key = None       # 上一轮的盘面：提交后盘面没变 => 服务器没收 => 必须停，
    while True:           # 否则会在同一关上无限重试打服务器（那正是上次事故的形态）
        board, html = cli.fetch_board()
        if board is None:
            shown = re.search(r'Level:\s*(\d+)', html)
            print(f"页面里没有盘面了（Level: {shown.group(1) if shown else '?'}）—— 停。")
            break
        cur, w, h, bstr = board
        key = board_key(w, h, bstr)
        if key == last_key:
            print(f"提交后盘面没变 —— 服务器没收这个解，停（别在同一关上反复打服务器）。")
            break
        last_key = key
        lvl = idx.get(key)
        shown = re.search(r'Level:\s*(\d+)', html)
        tag = f"线上Level {shown.group(1) if shown else '?'} / 盘面 {w}x{h}"

        if lvl is None:
            print(f"{tag}: **本地找不到这个盘面** —— 停（需要先离线解出来）。")
            break

        sol_path = os.path.join(args.solutions, f"{lvl}.sol")
        if not os.path.exists(sol_path):
            print(f"{tag} = 本地第 {lvl} 关: **银行里没有解** —— 停。")
            break
        m = SOL_RE.search(open(sol_path).read().strip())
        if not m:
            print(f"{tag} = 本地第 {lvl} 关: 解文件格式不对 —— 停。")
            break
        x, y, path = m.group(1), m.group(2), m.group(3)

        if args.dry_run:
            print(f"{tag} = 本地第 {lvl} 关 -> 有解(起点 {x},{y}，{len(path)} 步)，DRY-RUN 不提交")
            break

        cli.submit(x, y, path)
        done += 1
        print(f"{tag} = 本地第 {lvl} 关 -> 已提交({len(path)} 步) [本次第 {done} 关]", flush=True)

        if args.max and done >= args.max:
            print(f"到达本次上限 {args.max} 关，停。")
            break

    print(f"本次提交 {done} 关。")


if __name__ == "__main__":
    main()
