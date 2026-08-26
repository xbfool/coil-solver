#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
独立裁判 v1（2026-08-26，信任重建 Phase 2）

从 PROOFS.md 的数学规范**独立**重实现筛选规则（刻意不照抄 C 实现——独立性即价值），
用于跨实现对拍：求解器版本随便迭代，裁判不动，谁错谁现形。

实现的规则面（= per-start 强筛链，除 dirlayer/subtour/probing——767 消融证明其贡献为零）：
  §1.1 度数窗口   §1.2 必用边不成环(DSU)   §1.3/1.4 端点色/唯一终点
  §3.4 首滑直冲到墙（0 向死 / 1 向强制整线）
  §2.1/2.2 流层 Régin GAC（scipy maximum_flow + 强连通分量），哑点按奇偶挂边
  §3.5 链定向（内部拐点 tf>ti / strict / 首滑直线 / 含 s 链的方向淘汰）

坐标系与 C 完全一致：W=盘宽+2（哨兵边框），cell=(y+1)*W+(x+1)，
delta=[-1,-W,+1,+W]=[L,U,R,D]，estate: 0=未定 1=必用 2=禁用，规范方向 d∈{2,3}。

模式：
  selftest <board> <sol>        真起点必须存活 + 裁判结论与真解逐边一致（裁判自己的 REFSOL）
  refute   <board> <x> <y>      独立尝试证伪一个起点，输出判决与理由
  certify  <board> <oklist> <n> [seed]  从"求解器杀掉的起点"里抽 n 个，裁判逐个独立复核
"""
import sys, re, random
from collections import deque
import numpy as np
from scipy.sparse import coo_matrix, csr_matrix
from scipy.sparse.csgraph import maximum_flow, connected_components


class Dead(Exception):
    def __init__(self, reason): self.reason = reason


class Board:
    def __init__(self, path):
        raw = open(path).read()
        m = re.search(r"x=(\d+)&y=(\d+)&board=([.X]+)", raw)
        bw, bh, b = int(m.group(1)), int(m.group(2)), m.group(3)
        assert len(b) == bw * bh, "board 长度与声明尺寸不符"
        self.W = bw + 2
        self.H = bh + 2
        self.N = self.W * self.H
        self.free = bytearray(self.N)          # g0
        for y in range(bh):
            for x in range(bw):
                if b[y * bw + x] == '.':
                    self.free[(y + 1) * self.W + (x + 1)] = 1
        self.delta = (-1, -self.W, +1, +self.W)
        self.col = bytearray(self.N)           # (x+y) 奇偶二染色（用内部坐标，奇偶不变）
        for c in range(self.N):
            self.col[c] = (c % self.W + c // self.W) & 1
        self.total_free = sum(self.free)
        self.cells = [c for c in range(self.N) if self.free[c]]

    def xy(self, c):  # 内部 cell -> 盘面坐标
        return c % self.W - 1, c // self.W - 1

    def cell(self, x, y):
        return (y + 1) * self.W + (x + 1)


class Propagator:
    """一个起点的独立传播（§1 + §3.4 + §2 流 + §3.5 链）。"""

    def __init__(self, bd: Board, s: int):
        self.bd = bd; self.s = s
        self.estate = bytearray(bd.N * 4)
        self.dsu = list(range(bd.N))
        n = bd.total_free
        self.endcol = bd.col[s] if (n & 1) else (bd.col[s] ^ 1)
        self.forced_end = -1
        self.q = deque(); self.inq = bytearray(bd.N)

    # ---------- 基础设施 ----------
    def find(self, c):
        d = self.dsu
        while d[c] != c:
            d[c] = d[d[c]]; c = d[c]
        return c

    def push(self, c):
        if not self.inq[c]:
            self.inq[c] = 1; self.q.append(c)

    def set_edge(self, c, d, v):
        bd = self.bd
        n = c + bd.delta[d]
        a = self.estate[c * 4 + d]
        if a == v: return
        if a != 0: raise Dead(f"边({c},{d})冲突 {a}->{v}")
        if v == 1:
            ra, rb = self.find(c), self.find(n)
            if ra == rb: raise Dead(f"必用边({c},{d})闭环")   # §1.2
            self.dsu[ra] = rb
        self.estate[c * 4 + d] = v
        self.estate[n * 4 + (d ^ 2)] = v
        self.push(c); self.push(n)

    def lo_hi(self, c):
        if c == self.s: return 1, 1
        if self.forced_end >= 0:
            return (1, 1) if c == self.forced_end else (2, 2)
        return (1, 2) if self.bd.col[c] == self.endcol else (2, 2)

    # ---------- §1.1/1.2/1.3/1.4 ----------
    def process(self, c):
        bd = self.bd
        used, avail, av = 0, 0, []
        for d in range(4):
            n = c + bd.delta[d]
            if not bd.free[n]: continue
            e = self.estate[c * 4 + d]
            if e == 1: used += 1
            elif e == 0:
                if self.find(c) == self.find(n):        # §1.2 连上即成环 -> 禁
                    self.set_edge(c, d, 2)
                else:
                    avail += 1; av.append(d)
        lo, hi = self.lo_hi(c)
        if used > hi: raise Dead(f"格{c} used={used}>hi={hi}")
        if used + avail < lo: raise Dead(f"格{c} used+avail={used+avail}<lo={lo}")
        if used + avail == 1 and c != self.s and self.forced_end < 0:   # §1.4
            if bd.col[c] != self.endcol: raise Dead(f"格{c}只挂1边但颜色≠终点色")
            self.forced_end = c
            for c2 in bd.cells:                          # 其余终点色格收紧为 [2,2]
                if bd.col[c2] == self.endcol: self.push(c2)
        if used == hi:
            for d in av: self.set_edge(c, d, 2)
        elif used + avail == lo:
            for d in av: self.set_edge(c, d, 1)

    def drain(self):
        while self.q:
            c = self.q.popleft(); self.inq[c] = 0
            self.process(c)

    # ---------- §3.4 首滑 ----------
    def first_dir_ok(self, d):
        bd = self.bd; dd = bd.delta[d]
        c = self.s
        if not bd.free[c + dd]: return False
        while bd.free[c + dd]:
            if self.estate[c * 4 + d] == 2: return False
            c += dd
            if bd.free[c + dd]:
                e1, e2 = (d + 1) & 3, (d + 3) & 3
                if self.estate[c * 4 + e1] == 1 or self.estate[c * 4 + e2] == 1:
                    return False
        return True

    def first_move_rounds(self):
        bd = self.bd
        for _ in range(8):
            ok = [d for d in range(4) if self.first_dir_ok(d)]
            if not ok: raise Dead("首滑四向皆不可行")
            if len(ok) > 1: return
            d = ok[0]; dd = bd.delta[d]; c = self.s; changed = False
            while bd.free[c + dd]:
                if self.estate[c * 4 + d] != 1:
                    self.set_edge(c, d, 1); changed = True
                c += dd
            if not changed: return
            self.drain()

    # ---------- §2.1/2.2 流层 Régin ----------
    def flow_regin(self):
        bd = self.bd
        X = bd.col[self.s]
        idx = {}
        for c in bd.cells: idx[c] = len(idx) + 3        # 0=SRC 1=SNK 2=DUM
        nn = len(idx) + 3
        rows, cols, caps = [], [], []
        edge_arcs = []                                   # (c,d,row,col) 待定网格边

        def arc(u, v, cap):
            rows.append(u); cols.append(v); caps.append(cap)

        needL = needR = 0
        for c in bd.cells:
            b = 1 if c == self.s else 2
            nfix = sum(1 for d in range(4)
                       if bd.free[c + bd.delta[d]] and self.estate[c * 4 + d] == 1)
            rb = b - nfix
            if rb < 0: raise Dead(f"格{c}必用边超配额")
            if bd.col[c] == X:
                if rb: arc(0, idx[c], rb)
                needL += rb
            else:
                if rb: arc(idx[c], 1, rb)
                needR += rb
        if self.endcol != X:                             # 终点在 ~X 侧：哑点挂源侧
            arc(0, 2, 1); needL += 1
            for c in bd.cells:
                if bd.col[c] != X: arc(2, idx[c], 1)
        else:                                            # 终点在 X 侧（≠起点）：哑点挂汇侧
            arc(2, 1, 1); needR += 1
            for c in bd.cells:
                if bd.col[c] == X and c != self.s: arc(idx[c], 2, 1)
        for c in bd.cells:
            if bd.col[c] != X: continue
            for d in range(4):
                n = c + bd.delta[d]
                if not bd.free[n] or self.estate[c * 4 + d] != 0: continue
                edge_arcs.append((c, d, idx[c], idx[n]))
                arc(idx[c], idx[n], 1)
        if needL != needR: raise Dead(f"流配平失败 needL={needL} needR={needR}")

        cap = coo_matrix((caps, (rows, cols)), shape=(nn, nn)).tocsr()
        cap.sum_duplicates()
        res = maximum_flow(cap, 0, 1)
        if res.flow_value != needL:
            raise Dead(f"度约束松弛无解 flow={res.flow_value}<{needL}")
        F = res.flow.tocsr()

        # 残量图：cap-F>0 给正向弧，F>0 给反向弧；求强连通分量
        R = cap - F
        rr, rc = (R > 0).nonzero()
        fr, fc = (F > 0).nonzero()
        radj = coo_matrix((np.ones(len(rr) + len(fr), dtype=np.int8),
                           (np.concatenate([rr, fc]), np.concatenate([rc, fr]))),
                          shape=(nn, nn)).tocsr()
        _, scc = connected_components(radj, directed=True, connection='strong')

        newly = 0
        Fd = F.todok()
        for (c, d, u, v) in edge_arcs:
            if self.estate[c * 4 + d] != 0: continue     # 可能已被本轮更早的结论级联定死
            if scc[u] == scc[v]: continue
            if Fd.get((u, v), 0) > 0:
                self.set_edge(c, d, 1)
            else:
                self.set_edge(c, d, 2)
            newly += 1
        if newly: self.drain()
        return newly

    # ---------- §3.5 链定向 ----------
    def chains_check(self):
        bd = self.bd
        chain_id = {}; chain_pos = {}
        chains = []
        mdeg = {}
        for c in bd.cells:
            k = sum(1 for d in range(4)
                    if bd.free[c + bd.delta[d]] and self.estate[c * 4 + d] == 1)
            mdeg[c] = k
        for c0 in bd.cells:
            if mdeg[c0] != 1 or c0 in chain_id: continue
            seq = []; cur, prev = c0, -1
            while True:
                chain_id[cur] = len(chains); chain_pos[cur] = len(seq); seq.append(cur)
                nxt = -1
                for d in range(4):
                    n = cur + bd.delta[d]
                    if bd.free[n] and self.estate[cur * 4 + d] == 1 and n != prev:
                        nxt = n; break
                if nxt < 0: break
                prev, cur = cur, nxt
            chains.append(seq)

        def dir_ok(seq, rev, cid, strict):
            # 返回 None=可行，否则 (死因, 拐点格, 前方格)
            k = len(seq)
            for i in range(1, k - 1):
                prev = seq[i + 1] if rev else seq[i - 1]
                nxt = seq[i - 1] if rev else seq[i + 1]
                if nxt - seq[i] == seq[i] - prev: continue
                f = seq[i] + (seq[i] - prev)
                if not bd.free[f]: continue
                if chain_id.get(f) != cid:
                    if strict: return ("strict链外拐点", seq[i], f)
                    continue
                tf = (k - 1 - chain_pos[f]) if rev else chain_pos[f]
                ti = (k - 1 - i) if rev else i
                if tf > ti: return ("同链拐点tf>ti", seq[i], f)
            return None

        def first_run_ok(seq):
            k = len(seq)
            if k < 2: return True
            e = seq[1] - seq[0]; limit = 1
            while bd.free[seq[0] + (limit + 1) * e]: limit += 1
            limit = min(limit, k - 1)
            return all(seq[i + 1] - seq[i] == e for i in range(1, limit))

        for cid, seq in enumerate(chains):
            if len(seq) < 3: continue
            head_s, tail_s = seq[0] == self.s, seq[-1] == self.s
            bad0 = dir_ok(seq, False, cid, head_s)
            bad1 = dir_ok(seq, True, cid, tail_s)
            if head_s: bad1 = ("s在链头,反向被淘汰", None, None)
            if tail_s: bad0 = ("s在链尾,正向被淘汰", None, None)
            if head_s and not first_run_ok(seq): raise Dead(f"链{cid}首滑非直线(head)")
            if tail_s and not first_run_ok(seq[::-1]): raise Dead(f"链{cid}首滑非直线(tail)")
            if bad0 and bad1:
                xy = self.bd.xy
                def fmt(b):
                    return b[0] if b[1] is None else f"{b[0]}@拐点{xy(b[1])}前方{xy(b[2])}"
                raise Dead(f"链{cid}(长{len(seq)},{xy(seq[0])}->{xy(seq[-1])})双杀: "
                           f"正向[{fmt(bad0)}] 反向[{fmt(bad1)}]")

    # ---------- 总流程 ----------
    def run(self):
        try:
            for c in self.bd.cells: self.push(c)
            self.drain()
            self.first_move_rounds()
            self.flow_regin()
            self.first_move_rounds()
            self.flow_regin()                            # 第二轮流（吃前轮级联的新结论）
            self.chains_check()
            return True, "存活"
        except Dead as e:
            return False, e.reason


# ---------- 真解工具 ----------
def sol_edges(bd: Board, sol_path):
    raw = open(sol_path).read()
    sx = int(re.search(r"x=(\d+)", raw).group(1))
    sy = int(re.search(r"y=(\d+)", raw).group(1))
    path = re.search(r"path=([LURD]+)", raw).group(1)
    s = bd.cell(sx, sy)
    g = bytearray(bd.free)
    g[s] = 0
    edges = set(); cur = s
    DIRS = {'L': 0, 'U': 1, 'R': 2, 'D': 3}
    for ch in path:
        d = DIRS[ch]; dd = bd.delta[d]
        moved = False
        while g[cur + dd]:
            n = cur + dd
            edges.add((cur, d) if d in (2, 3) else (n, d ^ 2))   # 规范方向
            g[n] = 0; cur = n; moved = True
        assert moved, "解走不动，文件或解析有问题"
    assert sum(g) == 0, f"解未覆盖全部自由格,剩{sum(g)}"
    return s, edges, cur


def cmd_selftest(board_path, sol_path):
    bd = Board(board_path)
    s, sedges, term = sol_edges(bd, sol_path)
    p = Propagator(bd, s)
    alive, reason = p.run()
    print(f"真起点 {bd.xy(s)} 裁判判决: {'存活' if alive else '证伪!!'} ({reason})")
    bad = 0
    if alive:
        nm = nb = 0
        for c in bd.cells:
            for d in (2, 3):
                if not bd.free[c + bd.delta[d]]: continue
                e = p.estate[c * 4 + d]
                if e == 1:
                    nm += 1
                    if (c, d) not in sedges:
                        bad += 1
                        if bad <= 10: print(f"‼ 裁判必用边({bd.xy(c)},d{d})不在真解里")
                elif e == 2:
                    nb += 1
                    if (c, d) in sedges:
                        bad += 1
                        if bad <= 10: print(f"‼ 裁判禁用边({bd.xy(c)},d{d})被真解使用")
        print(f"裁判结论: 必用{nm} 禁用{nb}; 与真解冲突 {bad}")
    ok = alive and bad == 0
    print(f"SELFTEST {'PASS' if ok else '**FAIL**'}")
    return 0 if ok else 2


def cmd_refute(board_path, x, y):
    bd = Board(board_path)
    s = bd.cell(x, y)
    if not bd.free[s]:
        print(f"({x},{y}): 是墙，不是合法起点"); return 1
    p = Propagator(bd, s)
    alive, reason = p.run()
    print(f"({x},{y}): {'存活' if alive else '证伪'} —— {reason}")
    if len(sys.argv) > 5 and sys.argv[5] == "--edges":   # 探针：报告指定边的判定
        for spec in sys.argv[6:]:
            ex, ey, ed = map(int, spec.split(','))
            c = bd.cell(ex, ey)
            v = p.estate[c * 4 + ed]
            print(f"  边({ex},{ey})d{ed}: {'未定' if v == 0 else '必用' if v == 1 else '禁用'}")
    return 0


def cmd_certify(board_path, oklist_path, n, seed=42):
    bd = Board(board_path)
    ok = set()
    for line in open(oklist_path):
        mm = re.match(r"OK (\d+)", line)
        if mm: ok.add(int(mm.group(1)))
    killed = [c for c in bd.cells if c not in ok]
    random.seed(seed)
    sample = random.sample(killed, min(n, len(killed)))
    cert = unv = 0
    for c in sample:
        p = Propagator(bd, c)
        alive, reason = p.run()
        if alive:
            unv += 1
            print(f"UNVERIFIED {bd.xy(c)} 裁判无法复核该击杀（求解器更强or不可靠,存疑）")
        else:
            cert += 1
    print(f"CERTIFY: 抽样{len(sample)} 独立复核击杀 {cert} ({100*cert//max(1,len(sample))}%) 未复核 {unv}")
    return 0


if __name__ == "__main__":
    mode = sys.argv[1]
    if mode == "selftest": sys.exit(cmd_selftest(sys.argv[2], sys.argv[3]))
    elif mode == "refute": sys.exit(cmd_refute(sys.argv[2], int(sys.argv[3]), int(sys.argv[4])))
    elif mode == "certify":
        sd = int(sys.argv[5]) if len(sys.argv) > 5 else 42
        sys.exit(cmd_certify(sys.argv[2], sys.argv[3], int(sys.argv[4]), sd))
    else: print(__doc__); sys.exit(1)
