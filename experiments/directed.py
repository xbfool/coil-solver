#!/usr/bin/env python3
"""有向传播原型：把「从某侧进入某格」当变量，这才是 Tron 消除的对象。

之前的传播全是**无向**的（每格挂 1 或 2 条边 + 不成环 + 两色交替 + probing），
格子口径只能定死 36~47%，而 Tron 的预处理能压到「只剩 0.3% 的格子不确定用哪两条边」。
差距的根子是：**无向边模型装不下 Coil 的滑行规则**，而滑行规则极强 ——
实测真解里只有 3.5~4.6% 的格子「前方是自由格且更早访问」，也就是说
**95%+ 的情况下，一旦知道进入方向 d，要么 c+d 是墙（必须转弯），要么必须直行下去**。

有向模型的变量是 in[c] ⊆ {L,U,R,D,START}：路径是从哪一侧进入 c 的。out 由邻居的 in 派生
（out[c] 含 d ⟺ in[c+d] 含 d）。规则：

  1. 每格恰好一个 in（起点除外）、恰好一个 out（终点除外）；
  2. **2 环排除**：in(c)=d ⟹ in(c−d) ≠ −d（否则 c 和 c−d 互为前驱）；
  3. **互斥（关键，之前完全没用）**：in(c)=d ⟹ in(c+d) ≠ −d。
     推导：两边的入边都被别人占了 ⇒ 边 (c, c+d) 一定不被使用 ⇒ c 处转弯要求 t(c+d)<t(c)、
     c+d 处转弯要求 t(c)<t(c+d) ⇒ 矛盾。**无条件成立**，在 5 个真解上验过 0 次违反。
  4. in(c)={d} ⟹ c−d 的出边就定死是 d ⟹ c−d 的其它邻居都不能从对应方向被进入；
  5. out(c)={d} 且 c 不可能是终点 ⟹ in(c+d) 直接塌成 {d}；
  6. out(c)=∅ ⟹ c 就是终点；终点唯一、起点唯一，颜色由两色交替定死。
"""
import os, sys
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LEVELS = str(ROOT.parent / "coilbench" / "levels_all")
SOLS = r"C:\Users\xbfoo\AppData\Local\Temp\claude\D--workspace\a9a27850-3ba8-4b8e-8061-a644473312f2\scratchpad\sols"

DIRS = [(-1, 0), (0, -1), (1, 0), (0, 1)]        # L U R D；opp(d) = d ^ 2
START = 4


def load(lv):
    t = open(os.path.join(LEVELS, str(lv))).read().strip().split("&")
    w = int(t[0].split("=")[1]); h = int(t[1].split("=")[1]); b = t[2].split("=")[1]
    return w, h, [[b[y * w + x] == "." for x in range(w)] for y in range(h)]


class Dir:
    def __init__(self, w, h, free, start):
        self.w, self.h, self.free = w, h, free
        self.cells = [(x, y) for y in range(h) for x in range(w) if free[y][x]]
        self.n = len(self.cells)
        self.start = start
        # 终点颜色：路径两色交替，起点一定，终点色就定死了
        sc = (start[0] + start[1]) % 2
        self.endcol = sc if (self.n % 2) else 1 - sc
        self.inm = {}
        for c in self.cells:
            m = 0
            for d, (dx, dy) in enumerate(DIRS):
                u = (c[0] - dx, c[1] - dy)
                if self.ok(u):
                    m |= 1 << d
            self.inm[c] = m
        self.inm[start] = 1 << START
        self.not_end = {c: ((c[0] + c[1]) % 2 != self.endcol or c == start) for c in self.cells}
        self.is_end = None
        self.bad = False
        self.q = deque(self.cells)
        self.inq = set(self.cells)

    def ok(self, p):
        return 0 <= p[0] < self.w and 0 <= p[1] < self.h and self.free[p[1]][p[0]]

    def push(self, c):
        if c in self.inm and c not in self.inq:
            self.inq.add(c); self.q.append(c)

    def rm_in(self, c, d):
        if not self.ok(c):
            return
        if self.inm[c] >> d & 1:
            self.inm[c] &= ~(1 << d)
            if self.inm[c] == 0:
                self.bad = True
            self.push(c)
            u = (c[0] - DIRS[d][0], c[1] - DIRS[d][1])
            self.push(u)

    def outm(self, c):
        m = 0
        for d, (dx, dy) in enumerate(DIRS):
            a = (c[0] + dx, c[1] + dy)
            if self.ok(a) and (self.inm[a] >> d & 1):
                m |= 1 << d
        return m

    def set_end(self, c):
        if self.not_end[c]:
            self.bad = True; return
        if self.is_end is None:
            self.is_end = c
            for q in self.cells:
                if q != c and not self.not_end[q]:
                    self.not_end[q] = True; self.push(q)
        elif self.is_end != c:
            self.bad = True

    def process(self, c):
        I = self.inm[c]
        if I == 0:
            self.bad = True; return
        O = self.outm(c)
        if O == 0 and self.not_end[c]:
            self.bad = True; return
        if O == 0:
            self.set_end(c); return

        if I == (1 << START):
            pass
        elif I and (I & (I - 1)) == 0:                      # in 是单值
            d = I.bit_length() - 1
            if d < 4:
                dx, dy = DIRS[d]
                u = (c[0] - dx, c[1] - dy)
                a = (c[0] + dx, c[1] + dy)
                self.rm_in(u, d ^ 2)                        # 规则 2：2 环排除
                self.rm_in(a, d ^ 2)                        # 规则 3：互斥（关键）
                # 规则 4：u 的出边定死是 d，它的其它邻居都不能从对应方向被进入
                for e in range(4):
                    if e == d:
                        continue
                    v = (u[0] + DIRS[e][0], u[1] + DIRS[e][1])
                    self.rm_in(v, e)
                if not self.not_end[u]:
                    self.not_end[u] = True; self.push(u)    # u 有出边 => 不是终点

        if O and (O & (O - 1)) == 0 and self.not_end[c]:    # 规则 5：out 单值且不可能收尾
            d = O.bit_length() - 1
            a = (c[0] + DIRS[d][0], c[1] + DIRS[d][1])
            if self.inm[a] != (1 << d):
                self.inm[a] = 1 << d
                if self.inm[a] == 0:
                    self.bad = True
                self.push(a)
                for e in range(4):
                    self.push((a[0] - DIRS[e][0], a[1] - DIRS[e][1]))

    def run(self):
        while self.q and not self.bad:
            c = self.q.popleft(); self.inq.discard(c)
            self.process(c)
        return not self.bad

    def stats(self):
        det = sum(1 for c in self.cells if bin(self.inm[c]).count("1") == 1)
        cand = sum(bin(self.inm[c]).count("1") for c in self.cells)
        # 「用哪两条边」确定 = in 定死 且 out 定死
        both = 0
        for c in self.cells:
            I, O = self.inm[c], self.outm(c)
            if I and (I & (I - 1)) == 0 and O and (O & (O - 1)) == 0:
                both += 1
        return det, both, cand


def run(lv):
    w, h, free = load(lv)
    s = open(os.path.join(SOLS, f"{lv}.sol")).read().strip().split("&")
    sx = int(s[0].split("=")[1]); sy = int(s[1].split("=")[1])
    path = "".join(ch for ch in s[2].split("=")[1] if ch in "LURD")

    D = Dir(w, h, free, (sx, sy))
    ok = D.run()
    det, both, cand = D.stats()
    print(f"L{lv}: 自由格 {D.n} | 传播 {'无矛盾' if ok else '**矛盾（对真起点不该发生）**'}")
    print(f"    in 方向定死 {det}/{D.n} = {100.0*det/D.n:.1f}%  |  "
          f"[Tron 口径] 用哪两条边已确定 {both}/{D.n} = {100.0*both/D.n:.1f}% "
          f"(还不确定 {100.0*(D.n-both)/D.n:.1f}%)")
    print(f"    平均每格还剩 {cand/D.n:.2f} 个 in 候选")

    # soundness：真解的 in 方向不许被消除
    cur = (sx, sy); vis = {cur}; bad = 0
    for ch in path:
        d = DIRS["LURD".index(ch)]
        while True:
            q = (cur[0] + d[0], cur[1] + d[1])
            if not D.ok(q) or q in vis:
                break
            di = "LURD".index(ch)
            if not (D.inm[q] >> di & 1):
                bad += 1
            cur = q; vis.add(cur)
    print(f"    soundness: {'OK' if bad == 0 else f'**真解的 {bad} 个进入方向被误杀**'}")


if __name__ == "__main__":
    for lv in [int(a) for a in sys.argv[1:]] or [139, 165, 195, 215]:
        run(lv)
