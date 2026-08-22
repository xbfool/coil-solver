#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成器指纹检验（2026-08-24，用户直觉驱动）

假设：关卡由「空盘上按滑动规则随机走到卡死，未访问格变墙」生成。
可证明该构造的走法在成品盘上仍是合法解，且其**第一步滑行发生在空盘上**，
必然一路滑到外边界——那条直线全是路径格（永不变墙）。

预言（必要条件）：每关至少存在一个 (自由格 c, 方向 d)，使 c 沿 d 到边界的
整条射线无墙。30% 墙密度下随机射线全程无墙概率 ~0.7^L（L~85 时 ≈1e-13），
所以此类格子应当稀少——若成立，就是把上万起点候选压到几百的静态过滤器，
且保证生成器起点在其中。

输出：每关的 (射线对数量, 覆盖的起点格数量, 自由格总数)。任何一关为 0 => 假设证伪。
"""
import re
import sys

def load(path):
    raw = open(path).read()
    w = int(re.search(r"x=(\d+)", raw).group(1))
    h = int(re.search(r"y=(\d+)", raw).group(1))
    b = re.search(r"board=([.X]+)", raw).group(1)
    return w, h, b

def analyze(path):
    w, h, b = load(path)
    free = [[b[y * w + x] == "." for x in range(w)] for y in range(h)]
    nfree = sum(map(sum, free))
    # 每行/每列做前缀墙扫描：射线 (c,d) 无墙 ⟺ c 到该方向边界之间没有 X
    npairs = 0
    cells = set()
    for y in range(h):
        # 左右方向：找每行的「无墙前缀/后缀」
        for x in range(w):
            if not free[y][x]:
                continue
            ok_l = all(free[y][k] for k in range(0, x))
            ok_r = all(free[y][k] for k in range(x + 1, w))
            ok_u = all(free[k][x] for k in range(0, y))
            ok_d = all(free[k][x] for k in range(y + 1, h))
            cnt = ok_l + ok_r + ok_u + ok_d
            if cnt:
                npairs += cnt
                cells.add((x, y))
    return w, h, nfree, npairs, len(cells)

def main():
    for p in sys.argv[1:]:
        try:
            w, h, nfree, npairs, ncells = analyze(p)
            lvl = p.rsplit("/", 1)[-1]
            print(f"L{lvl}: {w}x{h} free={nfree} rays={npairs} ray-cells={ncells} ({100.0*ncells/nfree:.2f}%)")
        except Exception as e:
            print(f"{p}: ERR {e}")

if __name__ == "__main__":
    main()
