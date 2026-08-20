#!/usr/bin/env python3
"""无向层 + 有向层的联合传播 —— 两层互相喂，这才是 Tron 那条路。

单独跑哪一层都不行，实测过：
  · 只有无向层（度约束 + 不成环 + 两色交替 + probing）：格子口径只到 36~47%；
  · 只有有向层（in/out + 互斥 + 2 环排除）：**几乎定不死任何东西**，平均每格还剩 2.81 个
    in 候选 —— 因为没有种子，互斥和 2 环排除都要求 in[c] 先收敛成单值。

但两层能互相喂：
  · 无向层定死的**必用边**，只要一端的方向被排除，另一端的 in 就直接塌成单值 —— 有向层有了种子；
  · 有向层 in 一收敛，互斥（in(c)=d ⟹ in(c+d)≠−d）和 2 环排除就能开火，消除更多方向；
  · 方向消完，边又能被判成禁用/必用，回喂无向层。

耦合规则（u = c−d，即边 (u,c) 沿方向 d）：
  R1  in[c] 不含 d 且 in[u] 不含 −d      => 这条边没人用，判**禁用**
  R2  边禁用                              => in[c] 去掉 d，in[u] 去掉 −d
  R3  边必用 且 in[u] 不含 −d             => 方向只能是 u→c => **in[c] 塌成 {d}**
  R3' 边必用 且 in[c] 不含 d              => 方向只能是 c→u => **in[u] 塌成 {−d}**
  R4  in[c] = {d}                         => 边 (u,c) 判**必用**
有向层自己的规则：2 环排除、互斥、in 单值 ⟹ 上游出边定死、out 单值 ⟹ 下游 in 定死、
终点/起点唯一。无向层：每格 1 或 2 条边、强制边不成环。
"""
import os, sys
from collections import deque

LEVELS = r"D:\workspace\coilbench\levels_all"
if not os.path.isdir(LEVELS):
    LEVELS = "/mnt/d/workspace/coilbench/levels_all"
SOLS = r"C:\Users\xbfoo\AppData\Local\Temp\claude\D--workspace\a9a27850-3ba8-4b8e-8061-a644473312f2\scratchpad\sols"

DIRS = [(-1, 0), (0, -1), (1, 0), (0, 1)]
START = 4
UNDEC, FORCED, BANNED = 0, 1, 2


def load(lv):
    t = open(os.path.join(LEVELS, str(lv))).read().strip().split("&")
    w = int(t[0].split("=")[1]); h = int(t[1].split("=")[1]); b = t[2].split("=")[1]
    return w, h, [[b[y * w + x] == "." for x in range(w)] for y in range(h)]


class Joint:
    def __init__(self, w, h, free, start):
        self.w, self.h, self.free = w, h, free
        self.cells = [(x, y) for y in range(h) for x in range(w) if free[y][x]]
        self.n = len(self.cells)
        self.start = start
        sc = (start[0] + start[1]) % 2
        self.endcol = sc if (self.n % 2) else 1 - sc
        self.bad = False
        self.is_end = None

        self.inm = {}
        for c in self.cells:
            m = 0
            for d, (dx, dy) in enumerate(DIRS):
                if self.ok((c[0] - dx, c[1] - dy)):
                    m |= 1 << d
            self.inm[c] = m
        self.inm[start] = 1 << START
        self.not_end = {c: ((c[0] + c[1]) % 2 != self.endcol or c == start) for c in self.cells}

        self.est = {}
        for c in self.cells:
            for d in (2, 3):
                a = (c[0] + DIRS[d][0], c[1] + DIRS[d][1])
                if self.ok(a):
                    self.est[(c, a)] = UNDEC
        self.parent = {}
        self.q = deque(self.cells); self.inq = set(self.cells)

    def ok(self, p):
        return 0 <= p[0] < self.w and 0 <= p[1] < self.h and self.free[p[1]][p[0]]

    def key(self, a, b):
        return (a, b) if (a, b) in self.est else (b, a)

    def find(self, x):
        while self.parent.setdefault(x, x) != x:
            self.parent[x] = self.parent[self.parent[x]]
            x = self.parent[x]
        return x

    def push(self, c):
        if c in self.inm and c not in self.inq:
            self.inq.add(c); self.q.append(c)

    def set_edge(self, a, b, v):
        k = self.key(a, b)
        cur = self.est[k]
        if cur == v:
            return
        if cur != UNDEC:
            self.bad = True; return
        if v == FORCED:
            ra, rb = self.find(a), self.find(b)
            if ra == rb:
                self.bad = True; return
            self.parent[ra] = rb
        self.est[k] = v
        if v == BANNED:                                   # R2
            for (p, qq) in ((a, b), (b, a)):
                d = self.dirof(p, qq)
                self.rm_in(qq, d)
        self.push(a); self.push(b)

    def dirof(self, p, q):
        v = (q[0] - p[0], q[1] - p[1])
        return DIRS.index(v)

    def rm_in(self, c, d):
        if not self.ok(c):
            return
        if self.inm[c] >> d & 1:
            self.inm[c] &= ~(1 << d)
            if self.inm[c] == 0:
                self.bad = True
            self.push(c)
            self.push((c[0] - DIRS[d][0], c[1] - DIRS[d][1]))

    def fix_in(self, c, d):
        """把 in[c] 钉成单值 d"""
        if not (self.inm[c] >> d & 1):
            self.bad = True; return
        if self.inm[c] != (1 << d):
            self.inm[c] = 1 << d
            self.push(c)
            for e in range(4):
                self.push((c[0] - DIRS[e][0], c[1] - DIRS[e][1]))
                self.push((c[0] + DIRS[e][0], c[1] + DIRS[e][1]))

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
        if self.bad:
            return
        I = self.inm[c]
        if I == 0:
            self.bad = True; return
        O = self.outm(c)
        if O == 0 and self.not_end[c]:
            self.bad = True; return
        if O == 0:
            self.set_end(c)

        # ---- 有向层 ----
        if I and (I & (I - 1)) == 0 and I != (1 << START):
            d = I.bit_length() - 1
            dx, dy = DIRS[d]
            u = (c[0] - dx, c[1] - dy)
            a = (c[0] + dx, c[1] + dy)
            self.rm_in(u, d ^ 2)                          # 2 环排除
            if self.ok(a):
                self.rm_in(a, d ^ 2)                      # 互斥（关键）
            self.set_edge(u, c, FORCED)                   # R4
            for e in range(4):                            # u 的出边定死是 d
                if e == d:
                    continue
                v = (u[0] + DIRS[e][0], u[1] + DIRS[e][1])
                self.rm_in(v, e)
            if not self.not_end[u]:
                self.not_end[u] = True; self.push(u)
        if O and (O & (O - 1)) == 0 and self.not_end[c]:
            d = O.bit_length() - 1
            a = (c[0] + DIRS[d][0], c[1] + DIRS[d][1])
            self.fix_in(a, d)
            self.set_edge(c, a, FORCED)

        # ---- 耦合 R1 / R3 / R3' ----
        for d, (dx, dy) in enumerate(DIRS):
            a = (c[0] + dx, c[1] + dy)
            if not self.ok(a):
                continue
            k = self.key(c, a)
            st = self.est[k]
            ca = self.inm[a] >> d & 1                     # c -> a 可行？
            ac = self.inm[c] >> (d ^ 2) & 1               # a -> c 可行？
            if st == UNDEC and not ca and not ac:
                self.set_edge(c, a, BANNED)               # R1
            elif st == FORCED:
                if ca and not ac:
                    self.fix_in(a, d)                     # R3
                elif ac and not ca:
                    self.fix_in(c, d ^ 2)                 # R3'
                elif not ca and not ac:
                    self.bad = True; return

        # ---- 无向层：每格 1 或 2 条边 + 不成环 ----
        nb = []
        for d, (dx, dy) in enumerate(DIRS):
            a = (c[0] + dx, c[1] + dy)
            if self.ok(a):
                nb.append((a, self.key(c, a)))
        for a, k in nb:
            if self.est[k] == UNDEC and self.find(c) == self.find(a):
                self.set_edge(c, a, BANNED)
        used = [a for a, k in nb if self.est[k] == FORCED]
        undec = [a for a, k in nb if self.est[k] == UNDEC]
        lo = 1 if (c == self.start or not self.not_end[c]) else 2
        hi = 1 if (c == self.start or self.is_end == c) else 2
        if len(used) > hi or len(used) + len(undec) < lo:
            self.bad = True; return
        if len(used) == hi:
            for a in undec:
                self.set_edge(c, a, BANNED)
        elif len(used) + len(undec) == lo:
            for a in undec:
                self.set_edge(c, a, FORCED)

    def seed_first_move(self, d):
        """种子：起点的第一次滑行是在**空盘面**上做的，撞不到任何已访问格，必然一路滑到墙。
        所以那整条线上的格子，进入方向全是同一个 d —— 一次就钉死一大串 in，
        互斥规则和 2 环排除这才有得开火。单个格子的 in 天然最少也有 2 个候选（度最小是 2），
        所以不给这个种子的话，有向层永远收敛不了（实测：只能定死 1 个格子）。"""
        cur = self.start
        nxt = (cur[0] + DIRS[d][0], cur[1] + DIRS[d][1])
        if not self.ok(nxt):
            self.bad = True; return
        while self.ok(nxt):
            self.fix_in(nxt, d)
            self.set_edge(cur, nxt, FORCED)
            if self.bad: return
            cur = nxt
            nxt = (cur[0] + DIRS[d][0], cur[1] + DIRS[d][1])

    def run(self):
        while self.q and not self.bad:
            c = self.q.popleft(); self.inq.discard(c)
            self.process(c)
        return not self.bad

    def stats(self):
        det = sum(1 for c in self.cells if bin(self.inm[c]).count("1") == 1)
        both = 0
        for c in self.cells:
            I, O = self.inm[c], self.outm(c)
            if I and (I & (I - 1)) == 0 and O and (O & (O - 1)) == 0:
                both += 1
        ef = sum(1 for v in self.est.values() if v == FORCED)
        return det, both, ef, len(self.est)


def run(lv):
    w, h, free = load(lv)
    s = open(os.path.join(SOLS, f"{lv}.sol")).read().strip().split("&")
    sx = int(s[0].split("=")[1]); sy = int(s[1].split("=")[1])
    path = "".join(ch for ch in s[2].split("=")[1] if ch in "LURD")
    true_d = "LURD".index(path[0])
    best = None
    for d in range(4):
        J = Joint(w, h, free, (sx, sy))
        J.seed_first_move(d)
        ok = J.run()
        det, both, ef, te = J.stats()
        tag = " <- 真解的首步方向" if d == true_d else ""
        print(f"L{lv} 首步={'LURD'[d]}: {'无矛盾' if ok else '矛盾(此方向被证伪)'} | "
              f"in 定死 {100.0*det/J.n:.1f}% | [Tron口径] 用哪两条边已确定 {100.0*both/J.n:.1f}% "
              f"| 必用边 {100.0*ef/te:.1f}%{tag}")
        if d == true_d: best = J
    if best is None: return
    cur = (sx, sy); vis = {cur}; bad = 0
    for ch in path:
        di = "LURD".index(ch); dd = DIRS[di]
        while True:
            q = (cur[0] + dd[0], cur[1] + dd[1])
            if not best.ok(q) or q in vis: break
            if not (best.inm[q] >> di & 1): bad += 1
            if best.est.get(best.key(cur, q)) == BANNED: bad += 1
            cur = q; vis.add(cur)
    print(f"    真解首步那一档的 soundness: {'OK' if bad == 0 else f'**真解被误杀 {bad} 处**'}")

if __name__ == "__main__":
    for lv in [int(a) for a in sys.argv[1:]] or [139, 165, 195, 215]:
        run(lv)
