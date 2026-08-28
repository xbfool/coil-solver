#!/usr/bin/env python3
"""EVOLVE-PLAN P1: 盘面结构特征提取（分型用）。

用法（WSL）:
    python3 tools/boardfeat.py 1 1050 --out results/boardfeat.jsonl
    python3 tools/boardfeat.py --files board1 board2   # 直接给文件（如合成盘）

特征（全部对自由格归一化，尺寸无关，便于跨大小聚类）：
  n_free / wall_density / deg1..deg4 占比 / open2x2 占比（开阔度）/
  corridor_frac（走廊=deg2）/ junction_frac（deg3+deg4）/
  chain_mean, chain_max_frac（deg<=2 走廊段长度：均值、最长段占比）/
  color_balance（双色墙数差 / 墙数，官方盘恒 0）
"""
import argparse
import json
import sys
from pathlib import Path

import numpy as np


def load(path: Path):
    s = path.read_text(encoding="utf-8").strip()
    w = int(s.split("x=")[1].split("&")[0])
    h = int(s.split("y=")[1].split("&")[0])
    b = s.split("board=")[1].strip()[: w * h]   # 容忍生成器在盘面后附加 SOL/STAT 行
    if len(b) < w * h or set(b) - {".", "X"}:
        raise ValueError(f"盘面长度/字符不合法 ({len(b)} vs {w*h})")
    g = np.frombuffer(b.encode(), dtype=np.uint8).reshape(h, w) == ord(".")
    return g


def features(g: np.ndarray) -> dict:
    h, w = g.shape
    n = int(g.sum())
    # 度数：四方向平移求和
    deg = np.zeros_like(g, dtype=np.int8)
    deg[:, :-1] += g[:, 1:]
    deg[:, 1:] += g[:, :-1]
    deg[:-1, :] += g[1:, :]
    deg[1:, :] += g[:-1, :]
    deg = np.where(g, deg, -1)
    dc = [int((deg == d).sum()) for d in range(5)]
    open2 = int((g[:-1, :-1] & g[:-1, 1:] & g[1:, :-1] & g[1:, 1:]).sum())

    # 走廊段：deg<=2 的自由格子图的连通分量长度（洪水，栈实现）
    corr = g & (deg <= 2)
    seen = np.zeros_like(corr)
    sizes = []
    idx = np.argwhere(corr)
    corr_set = corr  # alias
    for y0, x0 in idx:
        if seen[y0, x0]:
            continue
        st = [(int(y0), int(x0))]
        seen[y0, x0] = True
        sz = 0
        while st:
            y, x = st.pop()
            sz += 1
            for yy, xx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
                if 0 <= yy < h and 0 <= xx < w and corr_set[yy, xx] and not seen[yy, xx]:
                    seen[yy, xx] = True
                    st.append((yy, xx))
        sizes.append(sz)
    sizes = np.array(sizes) if sizes else np.array([0])

    # 双色平衡：棋盘染色下墙的数量差
    yy, xx = np.mgrid[0:h, 0:w]
    color = (yy + xx) & 1
    walls = ~g
    cb = int(walls[color == 0].sum()) - int(walls[color == 1].sum())
    nw = int(walls.sum())

    return {
        "w": w, "h": h, "n_free": n,
        "wall_density": round(nw / (w * h), 4),
        "deg1": round(dc[1] / n, 4), "deg2": round(dc[2] / n, 4),
        "deg3": round(dc[3] / n, 4), "deg4": round(dc[4] / n, 4),
        "open2x2": round(open2 / n, 4),
        "junction_frac": round((dc[3] + dc[4]) / n, 4),
        "chain_mean": round(float(sizes.mean()), 2),
        "chain_max_frac": round(float(sizes.max()) / n, 4),
        "color_balance": round(cb / nw, 4) if nw else 0.0,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("range", nargs="*", help="起 止 关号")
    ap.add_argument("--files", nargs="*", help="直接给盘面文件")
    ap.add_argument("--levels-dir", default="/mnt/d/works/coil/coilbench/levels_all")
    ap.add_argument("--out")
    args = ap.parse_args()

    jobs = []
    if args.files:
        jobs = [(Path(f).name, Path(f)) for f in args.files]
    elif len(args.range) == 2:
        a, b = int(args.range[0]), int(args.range[1])
        d = Path(args.levels_dir)
        jobs = [(str(i), d / str(i)) for i in range(a, b + 1) if (d / str(i)).is_file()]
    else:
        sys.exit("给 <起 止> 或 --files")

    sink = open(args.out, "w", encoding="utf-8") if args.out else sys.stdout
    for name, p in jobs:
        try:
            rec = {"level": name}
            rec.update(features(load(p)))
            sink.write(json.dumps(rec, ensure_ascii=False) + "\n")
        except Exception as e:
            print(f"跳过 {name}: {e}", file=sys.stderr)
    if args.out:
        sink.close()
        print(f"写入 {args.out}: {len(jobs)} 盘", file=sys.stderr)


if __name__ == "__main__":
    main()
