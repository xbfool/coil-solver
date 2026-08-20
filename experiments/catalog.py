#!/usr/bin/env python3
"""穿越目录 -> 必用边/禁用边。分区法的完整机制，但出口换掉了。

ericgopak/Mortal-Coil 把分区当**主搜索框架**：离线穷举每个 component 的全部穿越方式，
主搜索只在 component 层面走。实测那条路在中盘慢两个数量级，而 visit_pattern.py 量出了原因：
段总数只比格子级的滑行步数少 25%（L215 是 1495 对 1986）——**搜索深度根本没降下来**，
却把格子级的所有剪枝都丢了。

所以这里把同一套机制当**信息源**用：对每个区域枚举**全部**可行走法，
  · 所有走法都用到的边  => **必用边**
  · 所有走法都不用的边  => **禁用边**
再喂给 v20 已有的 estate 剪枝。区域榨不出信息就直接跳过，榨得出的就是白捡的 —— 只赚不亏。
「零穿越解 => 必含端点」只是这套东西的一个极端情形（可行走法数为 0）。

枚举必须**穷尽**才 sound，所以三件事都得考虑：
  · 路径可能**多次**进出同一个区域（实测 13% 的区域会，最多 7 次）；
  · 路径可能在区域内**开始或结束**（端点在里面）；
  · 滑行撞到区域外时**外面的状态未知** —— 保守地两种都试
    （撞到已用过的口：外侧格必然已访问，必须停；撞到没用过的口：可停可出去）。
枚举量超上限就直接放弃这个区域（放弃 = 不提取信息，仍然 sound）。
"""
import os, sys
# MODE 决定枚举哪些走法，直接决定结论 sound 到什么程度：
#   all     —— 所有走法。端点模式每种都少用一条割边，交集必然被打成空集（实测 0%）。
#   pure    —— 纯穿越（区域内不含端点）。最强（58~64%），但对真正含端点的那 2 个区域是错的。
#   nostart —— 禁止起点、允许终点。**求解器本来就在枚举起点，所以「本区域不含起点」是已知的**，
#              这一档给定起点后完全 sound。
MODE = os.environ.get("MODE", "all")
PURE = MODE == "pure"
NOSTART = MODE == "nostart"
from collections import deque, Counter

LEVELS = r"D:\workspace\coilbench\levels_all"
if not os.path.isdir(LEVELS):
    LEVELS = "/mnt/d/workspace/coilbench/levels_all"
SOLS = r"C:\Users\xbfoo\AppData\Local\Temp\claude\D--workspace\a9a27850-3ba8-4b8e-8061-a644473312f2\scratchpad\sols"

DIRS = [(-1, 0), (0, -1), (1, 0), (0, 1)]           # L U R D
LEFT = {(-1, 0): (0, 1), (0, -1): (-1, 0), (1, 0): (0, -1), (0, 1): (1, 0)}
RIGHT = {d: (-LEFT[d][0], -LEFT[d][1]) for d in DIRS}
MAX_CELLS = int(os.environ.get("MAX_CELLS", 24))    # 超过这么大的区域不枚举
MAX_NODES = int(os.environ.get("MAX_NODES", 200000))# 单个区域的枚举预算
# PURE=1: 只枚举纯穿越走法（区域内不含端点）。全盘只有 2 个端点，几百个区域里绝大多数
# 确实不含端点——端点模式每种都少用一条割边，会把交集直接打成空集。


def load(lv):
    t = open(os.path.join(LEVELS, str(lv))).read().strip().split("&")
    w = int(t[0].split("=")[1]); h = int(t[1].split("=")[1]); b = t[2].split("=")[1]
    return w, h, [[b[y * w + x] == "." for x in range(w)] for y in range(h)]


def build(free, w, h):
    def wall(px, py):
        return not (0 <= px < w and 0 <= py < h and free[py][px])

    def bn(x, y, d):
        lx, ly = LEFT[d]; rx, ry = RIGHT[d]; dx, dy = d
        return ((wall(x + lx, y + ly) or wall(x + dx + lx, y + dy + ly)) and
                (wall(x + rx, y + ry) or wall(x + dx + rx, y + dy + ry)))

    comp = [[-1] * w for _ in range(h)]
    cells = []
    for y0 in range(h):
        for x0 in range(w):
            if not free[y0][x0] or comp[y0][x0] >= 0:
                continue
            cid = len(cells); mine = []
            q = deque([(x0, y0)]); comp[y0][x0] = cid
            while q:
                x, y = q.popleft(); mine.append((x, y))
                for d in DIRS:
                    nx, ny = x + d[0], y + d[1]
                    if not (0 <= nx < w and 0 <= ny < h) or not free[ny][nx]:
                        continue
                    if bn(x, y, d):
                        continue
                    if comp[ny][nx] < 0:
                        comp[ny][nx] = cid; q.append((nx, ny))
            cells.append(mine)
    return wall, comp, cells


class Region:
    """一个 component 的枚举器。返回 (可行走法数, 必用边集, 禁用边集)。"""

    def __init__(self, mine, ports, wall, comp, cid):
        self.cells = mine
        self.idx = {c: i for i, c in enumerate(mine)}
        self.k = len(mine)
        self.full = (1 << self.k) - 1
        self.wall = wall
        self.comp = comp
        self.cid = cid
        self.ports = ports                       # [(内格, 外格)]
        self.pidx = {p: i for i, p in enumerate(ports)}
        # 边编号：区域内部边 + 割边。用 (格子, 方向索引) 的规范形式
        self.edges = {}
        for c in mine:
            for di, d in enumerate(DIRS):
                n = (c[0] + d[0], c[1] + d[1])
                if wall(n[0], n[1]):
                    continue
                key = (c, n) if c < n else (n, c)
                if key not in self.edges:
                    self.edges[key] = len(self.edges)
        self.nedges = len(self.edges)
        self.nodes = 0
        self.count = 0
        self.and_mask = (1 << self.nedges) - 1
        self.or_mask = 0

    def ebit(self, a, b):
        return 1 << self.edges[(a, b) if a < b else (b, a)]

    def inside(self, p):
        return p in self.idx

    def slide(self, cur, d, vis, ed, used_ports):
        """从 cur 沿 d 滑（cur 已访问）。返回若干 (结局, 位置, vis, ed) 分支。
        结局 'stop' = 停在区域内；'exit' = 从某个口出去（附带口号）。"""
        out = []
        while True:
            q = (cur[0] + d[0], cur[1] + d[1])
            if self.wall(q[0], q[1]):
                out.append(('stop', cur, vis, ed)); return out
            if not self.inside(q):
                pi = self.pidx.get((cur, q))
                if pi is None:                       # 不该发生：跨区域的边都是口
                    out.append(('stop', cur, vis, ed)); return out
                if used_ports >> pi & 1:
                    # 这个口用过了 => 外侧格必然已访问 => 必须停
                    out.append(('stop', cur, vis, ed)); return out
                # 没用过的口：外面走没走过不知道，两种都要试
                out.append(('exit', pi, vis, ed | self.ebit(cur, q)))
                out.append(('stop', cur, vis, ed))
                return out
            qi = self.idx[q]
            if vis >> qi & 1:
                out.append(('stop', cur, vis, ed)); return out
            vis |= 1 << qi
            ed |= self.ebit(cur, q)
            cur = q

    def run_segment(self, cur, d, vis, ed, used_ports, has_end, cont):
        """在区域内走一段：反复滑行，直到从某个口出去，或者在区域内收尾（终点）。"""
        self.nodes += 1
        if self.nodes > MAX_NODES:
            raise OverflowError
        for res in self.slide(cur, d, vis, ed, used_ports):
            if res[0] == 'exit':
                _, pi, v2, e2 = res
                cont(v2, e2, used_ports | (1 << pi), has_end)
                continue
            _, p2, v2, e2 = res
            # 停在 p2：可以在这里收尾（这一格是整条路径的终点），也可以换方向继续
            if not has_end and not PURE:
                cont(v2, e2, used_ports, True)
            for d2 in DIRS:
                if d2 == d:
                    continue                          # 前方刚被挡住
                n = (p2[0] + d2[0], p2[1] + d2[1])
                if self.wall(n[0], n[1]):
                    continue
                if not self.inside(n):
                    pi2 = self.pidx.get((p2, n))
                    if pi2 is None or (used_ports >> pi2 & 1):
                        continue                      # 用过的口出不去（外侧已访问）
                    cont(v2, e2 | self.ebit(p2, n), used_ports | (1 << pi2), has_end)
                    continue
                if v2 >> self.idx[n] & 1:
                    continue
                self.run_segment(p2, d2, v2, e2, used_ports, has_end, cont)

    def enumerate(self):
        try:
            self._explore(0, 0, 0, False, False)
        except OverflowError:
            return None                                # 预算耗尽，放弃这个区域
        if self.count == 0:
            return (0, 0, 0)
        return (self.count, self.and_mask, ((1 << self.nedges) - 1) & ~self.or_mask)

    def _explore(self, vis, ed, used_ports, has_start, has_end):
        self.nodes += 1
        if self.nodes > MAX_NODES:
            raise OverflowError
        if vis == self.full:
            if PURE and (has_start or has_end): return
            if NOSTART and has_start: return
            # 全覆盖 => 一个可行走法。路径不会再进来（进来就得重复走格子）
            self.count += 1
            self.and_mask &= ed
            self.or_mask |= ed
            return
        cont = lambda v, e, up, he: self._explore(v, e, up, has_start, he)
        # 情形一：从某个还没用过的口进来
        for pi, (b, a) in enumerate(self.ports):
            if used_ports >> pi & 1:
                continue
            bi = self.idx[b]
            if vis >> bi & 1:
                continue                               # 入口那一格走过了，进不来
            d = (b[0] - a[0], b[1] - a[1])
            self.run_segment(b, d, vis | (1 << bi), ed | self.ebit(a, b),
                             used_ports | (1 << pi), has_end,
                             lambda v, e, up, he: self._explore(v, e, up, has_start, he))
        # 情形二：整条路径的起点就在这个区域里
        if not has_start and not PURE and not NOSTART:
            for i, c in enumerate(self.cells):
                if vis >> i & 1:
                    continue
                for d in DIRS:
                    n = (c[0] + d[0], c[1] + d[1])
                    if self.wall(n[0], n[1]):
                        continue
                    if not self.inside(n):
                        pi2 = self.pidx.get((c, n))
                        if pi2 is None or (used_ports >> pi2 & 1):
                            continue
                        self._explore(vis | (1 << i), ed | self.ebit(c, n),
                                      used_ports | (1 << pi2), True, has_end)
                        continue
                    if vis >> self.idx[n] & 1:
                        continue
                    self.run_segment(c, d, vis | (1 << i), ed, used_ports, has_end,
                                     lambda v, e, up, he: self._explore(v, e, up, True, he))


def analyse(lv, verbose=False):
    w, h, free = load(lv)
    wall, comp, cells = build(free, w, h)
    ports = [[] for _ in cells]
    for y in range(h):
        for x in range(w):
            if not free[y][x]:
                continue
            for d in DIRS:
                nx, ny = x + d[0], y + d[1]
                if 0 <= nx < w and 0 <= ny < h and free[ny][nx] and comp[ny][nx] != comp[y][x]:
                    ports[comp[y][x]].append(((x, y), (nx, ny)))

    forced, banned, dead, skipped, done = 0, 0, 0, 0, 0
    from collections import Counter as _C
    cntdist = _C(); sizedist = _C()
    forced_edges, banned_edges = set(), set()
    for cid, mine in enumerate(cells):
        if len(mine) > MAX_CELLS:
            skipped += 1; continue
        R = Region(mine, ports[cid], wall, comp, cid)
        r = R.enumerate()
        if r is None:
            skipped += 1; continue
        done += 1
        cnt, am, bm = r
        cntdist[min(cnt, 1000)] += 1; sizedist[len(mine)] += 1
        if cnt == 0:
            dead += 1; continue
        inv = {v: k for k, v in R.edges.items()}
        for i in range(R.nedges):
            if am >> i & 1:
                forced += 1; forced_edges.add(inv[i])
            if bm >> i & 1:
                banned += 1; banned_edges.add(inv[i])
    total_edges = 0
    for y in range(h):
        for x in range(w):
            if free[y][x]:
                for d in (DIRS[2], DIRS[3]):
                    nx, ny = x + d[0], y + d[1]
                    if 0 <= nx < w and 0 <= ny < h and free[ny][nx]:
                        total_edges += 1
    print(f"L{lv}: 区域 {len(cells)} (枚举了 {done}, 跳过 {skipped}), 网格边 {total_edges}")
    top = sorted(cntdist.items())[:12]
    print(f"    走法数分布(前12档) {dict(top)}  中位区域大小 {sorted(sizedist.elements())[len(list(sizedist.elements()))//2]}")
    print(f"    零走法区域(必含端点) {dead} 个 | 必用边 {forced} ({100.0*forced/total_edges:.1f}%) "
          f"| 禁用边 {banned} ({100.0*banned/total_edges:.1f}%)")
    return forced_edges, banned_edges, wall, free, w, h


def verify(lv):
    """soundness：必用边真解必须用，禁用边真解必须不用。"""
    fe, be, wall, free, w, h = analyse(lv)
    sp = os.path.join(SOLS, f"{lv}.sol")
    if not os.path.exists(sp):
        print("    （没有解文件，跳过 soundness 检查）"); return
    t = open(sp).read().strip().split("&")
    sx = int(t[0].split("=")[1]); sy = int(t[1].split("=")[1])
    path = "".join(c for c in t[2].split("=")[1] if c in "LURD")
    used = set(); cur = (sx, sy); vis = {cur}
    for ch in path:
        d = DIRS["LURD".index(ch)]
        while True:
            q = (cur[0] + d[0], cur[1] + d[1])
            if wall(q[0], q[1]) or q in vis:
                break
            used.add((cur, q) if cur < q else (q, cur))
            cur = q; vis.add(cur)
    bad_f = [e for e in fe if e not in used]
    bad_b = [e for e in be if e in used]
    print(f"    soundness: " + ("OK" if not bad_f and not bad_b else
          f"**必用边错 {len(bad_f)} 条, 禁用边错 {len(bad_b)} 条**"))





def hybrid(lv):
    """按区域挑档：不含终点色格子的区域用 pure（强），可能含终点的用 nostart（弱但 sound）。

    终点的颜色是**已知**的：路径在棋盘两色间严格交替，起点一定，终点色就定死了
    （自由格数 n 为偶 => 异色，n 为奇 => 同色）。所以一个区域里要是根本没有终点色的格子，
    它就绝不可能含终点；再加上「本区域不是起点区域」（求解器本来就在枚举起点），
    这个区域就一定是纯穿越 —— pure 那一档的强结论对它完全成立。
    """
    global PURE, NOSTART
    w, h, free = load(lv)
    wall, comp, cells = build(free, w, h)
    ports = [[] for _ in cells]
    for y in range(h):
        for x in range(w):
            if not free[y][x]:
                continue
            for d in DIRS:
                nx, ny = x + d[0], y + d[1]
                if 0 <= nx < w and 0 <= ny < h and free[ny][nx] and comp[ny][nx] != comp[y][x]:
                    ports[comp[y][x]].append(((x, y), (nx, ny)))

    total_free = sum(r.count(True) for r in free)
    total_edges = 0
    for y in range(h):
        for x in range(w):
            if free[y][x]:
                for d in (DIRS[2], DIRS[3]):
                    nx, ny = x + d[0], y + d[1]
                    if 0 <= nx < w and 0 <= ny < h and free[ny][nx]:
                        total_edges += 1

    for tc in (0, 1):
        forced = set()
        pure_regions = 0
        for cid, mine in enumerate(cells):
            if len(mine) > MAX_CELLS:
                continue
            can_hold_end = any((x + y) % 2 == tc for (x, y) in mine)
            PURE, NOSTART = (not can_hold_end), can_hold_end
            if can_hold_end:
                pass
            else:
                pure_regions += 1
            R = Region(mine, ports[cid], wall, comp, cid)
            r = R.enumerate()
            if r is None or r[0] == 0:
                continue
            inv = {v: k for k, v in R.edges.items()}
            for i in range(R.nedges):
                if r[1] >> i & 1:
                    forced.add(inv[i])
        print(f"L{lv} 终点色={tc}: 能用 pure 的区域 {pure_regions}/{len(cells)} "
              f"({100.0*pure_regions/len(cells):.0f}%), 必用边 {len(forced)} "
              f"({100.0*len(forced)/total_edges:.1f}%)")
    PURE = NOSTART = False


if __name__ == "__main__":
    lvs = [int(a) for a in sys.argv[1:]] or [139, 165, 195]
    if os.environ.get("HYBRID") == "1":
        for lv in lvs: hybrid(lv)
    else:
        for lv in lvs: verify(lv)
