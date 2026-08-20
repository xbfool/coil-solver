#!/usr/bin/env python3
"""用区域的 pure 必用边去筛「(起点区域, 终点区域)」对 —— 直打起点彩票。

思路链条：
  · 每个区域的**纯穿越**走法交集能定死 58~67% 的边（catalog.py 量的），非常强；
  · 但它只对「本区域不含端点」成立 —— 全盘 540~1092 个区域里，**只有 2 个**是错的
    （起点所在的那个、终点所在的那个）；
  · 所以反过来枚举那 2 个区域：假设端点区域是 (S, T)，就把**除 S、T 之外**所有区域的
    pure 必用边全部应用上 —— 这一步是完全 sound 的 —— 然后传播。
    传播一矛盾，这对 (S, T) 就被排除。

一对区域被排除是**证明**不是启发式。剩下的候选对直接把起点候选从几千格压到几个区域里，
而起点彩票正是我们最大的瓶颈（而且已经证明约束传播本身干不掉它：偶数关证伪率 0%）。

这个脚本先验证威力：真解那一对必须活下来（soundness），随机的其它对应该大量被排除。
"""
import os, sys, random
from collections import deque

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import catalog as C

DIRS = C.DIRS


def edges_of(free, w, h):
    ed = {}
    for y in range(h):
        for x in range(w):
            if not free[y][x]:
                continue
            for d in (DIRS[2], DIRS[3]):
                nx, ny = x + d[0], y + d[1]
                if 0 <= nx < w and 0 <= ny < h and free[ny][nx]:
                    ed[((x, y), (nx, ny))] = len(ed)
    return ed


def propagate(free, w, h, cellidx, adj, forced, banned, ep_cells):
    """极简传播：非端点格恰好挂 2 条边、端点格挂 1 条，强制边不许闭成环。
    forced/banned 是初始给定的边集。ep_cells = 允许当端点的格子集合。
    返回 False 表示矛盾。"""
    state = {}
    for e in forced:
        state[e] = 1
    for e in banned:
        if state.get(e) == 1:
            return False
        state[e] = 2
    parent = {}

    def find(x):
        while parent.setdefault(x, x) != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for e, v in list(state.items()):
        if v == 1:
            a, b = find(e[0]), find(e[1])
            if a == b:
                return False
            parent[a] = b

    q = deque(cellidx)
    inq = set(cellidx)
    while q:
        c = q.popleft(); inq.discard(c)
        nb = adj[c]
        used = [e for e in nb if state.get(e) == 1]
        undec = [e for e in nb if e not in state]
        for e in list(undec):                       # 会闭成环的待定边先禁掉
            if find(e[0]) == find(e[1]):
                state[e] = 2; undec.remove(e)
                for p in e:
                    if p not in inq: q.append(p); inq.add(p)
        lo, hi = (1, 2) if c in ep_cells else (2, 2)
        if len(used) > hi or len(used) + len(undec) < lo:
            return False
        newly = []
        if len(used) == hi:
            for e in undec: state[e] = 2; newly.append(e)
        elif len(used) + len(undec) == lo:
            for e in undec:
                a, b = find(e[0]), find(e[1])
                if a == b: return False
                parent[a] = b
                state[e] = 1; newly.append(e)
        for e in newly:
            for p in e:
                if p not in inq: q.append(p); inq.add(p)
    return True


def run(lv, samples=60):
    w, h, free = C.load(lv)
    wall, comp, cells = C.build(free, w, h)
    ports = [[] for _ in cells]
    for y in range(h):
        for x in range(w):
            if not free[y][x]: continue
            for d in DIRS:
                nx, ny = x + d[0], y + d[1]
                if 0 <= nx < w and 0 <= ny < h and free[ny][nx] and comp[ny][nx] != comp[y][x]:
                    ports[comp[y][x]].append(((x, y), (nx, ny)))

    # 每个区域的 pure 必用边 / 禁用边
    C.PURE, C.NOSTART = True, False
    reg_forced, reg_banned = {}, {}
    for cid, mine in enumerate(cells):
        if len(mine) > C.MAX_CELLS: continue
        R = C.Region(mine, ports[cid], wall, comp, cid)
        r = R.enumerate()
        if r is None or r[0] == 0: continue
        inv = {v: k for k, v in R.edges.items()}
        reg_forced[cid] = {inv[i] for i in range(R.nedges) if r[1] >> i & 1}
        reg_banned[cid] = {inv[i] for i in range(R.nedges) if r[2] >> i & 1}

    cellidx = [(x, y) for y in range(h) for x in range(w) if free[y][x]]
    adj = {c: [] for c in cellidx}
    for (a, b) in edges_of(free, w, h):
        adj[a].append((a, b)); adj[b].append((a, b))

    # 真解的端点落在哪两个区域
    sp = os.path.join(C.SOLS, f"{lv}.sol")
    t = open(sp).read().strip().split("&")
    sx = int(t[0].split("=")[1]); sy = int(t[1].split("=")[1])
    path = "".join(ch for ch in t[2].split("=")[1] if ch in "LURD")
    cur = (sx, sy); vis = {cur}
    for ch in path:
        d = DIRS["LURD".index(ch)]
        while True:
            q = (cur[0] + d[0], cur[1] + d[1])
            if wall(q[0], q[1]) or q in vis: break
            cur = q; vis.add(cur)
    S_true, T_true = comp[sy][sx], comp[cur[1]][cur[0]]

    def test(S, T):
        f, b = set(), set()
        for cid in reg_forced:
            if cid == S or cid == T: continue
            f |= reg_forced[cid]; b |= reg_banned[cid]
        if f & b: return False
        ep = {c for c in cellidx if comp[c[1]][c[0]] in (S, T)}
        return propagate(free, w, h, cellidx, adj, f, b, ep)

    ok_true = test(S_true, T_true)
    ids = [c for c in reg_forced]
    random.seed(12345)
    pairs = [(random.choice(ids), random.choice(ids)) for _ in range(samples)]
    survive = sum(1 for S, T in pairs if test(S, T))
    print(f"L{lv}: 区域 {len(cells)}（算出 pure 目录的 {len(reg_forced)} 个）")
    print(f"    真解那一对 (S={S_true}, T={T_true}): "
          f"{'活下来 (对)' if ok_true else '**被排除 —— 判据错了**'}")
    print(f"    随机 {samples} 对里活下来 {survive} 个 "
          f"({100.0*survive/samples:.0f}%)  <- 越低说明筛得越狠")




def synergy(lv):
    """区域信息和现有传播是不是互补 —— 这决定了要不要把它 C 化接进求解器。

    对照三组，都用同一套传播（v20 的模型：假定起点、终点色由两色交替定死、
    非端点格恰好挂 2 条边、强制边不许成环）：
      A. 只有传播（这就是 v20 现在的水平）
      B. 只有区域的 pure 必用边（只用在「不含终点色格子」的区域上，这样才 sound）
      C. 两个一起喂进去，看传播的连锁能把它推到哪
    """
    w, h, free = C.load(lv)
    wall, comp, cells = C.build(free, w, h)
    ports = [[] for _ in cells]
    for y in range(h):
        for x in range(w):
            if not free[y][x]: continue
            for d in DIRS:
                nx, ny = x + d[0], y + d[1]
                if 0 <= nx < w and 0 <= ny < h and free[ny][nx] and comp[ny][nx] != comp[y][x]:
                    ports[comp[y][x]].append(((x, y), (nx, ny)))

    sp = os.path.join(C.SOLS, f"{lv}.sol")
    t = open(sp).read().strip().split("&")
    sx = int(t[0].split("=")[1]); sy = int(t[1].split("=")[1])
    start = (sx, sy)
    cellidx = [(x, y) for y in range(h) for x in range(w) if free[y][x]]
    n = len(cellidx)
    tc = ((sx + sy) % 2) if (n % 2) else (1 - (sx + sy) % 2)   # 终点色
    ep = {c for c in cellidx if (c[0] + c[1]) % 2 == tc} | {start}

    C.PURE, C.NOSTART = True, False
    rf, rb = set(), set()
    usable = 0
    for cid, mine in enumerate(cells):
        if len(mine) > C.MAX_CELLS: continue
        if any((x + y) % 2 == tc for (x, y) in mine): continue   # 可能含终点，pure 不 sound
        if comp[sy][sx] == cid: continue                          # 起点区域也排除
        usable += 1
        R = C.Region(mine, ports[cid], wall, comp, cid)
        r = R.enumerate()
        if r is None or r[0] == 0: continue
        inv = {v: k for k, v in R.edges.items()}
        rf |= {inv[i] for i in range(R.nedges) if r[1] >> i & 1}
        rb |= {inv[i] for i in range(R.nedges) if r[2] >> i & 1}

    ed = edges_of(free, w, h)
    adj = {c: [] for c in cellidx}
    for (a, b) in ed:
        adj[a].append((a, b)); adj[b].append((a, b))

    def measure(f0, b0):
        st = {}
        ok = propagate2(free, w, h, cellidx, adj, set(f0), set(b0), ep, st)
        nf = sum(1 for v in st.values() if v == 1)
        return ok, nf

    total = len(ed)
    okA, nA = measure(set(), set())
    okB, nB = measure(rf, rb)
    print(f"L{lv}: 网格边 {total}, 能安全用 pure 的区域 {usable}/{len(cells)}")
    print(f"    A 只有传播          : 必用边 {nA} ({100.0*nA/total:.1f}%)")
    print(f"    B 传播 + 区域 pure  : 必用边 {nB} ({100.0*nB/total:.1f}%)   "
          f"净增 {nB-nA} 条 ({100.0*(nB-nA)/total:+.1f}%)")


def propagate2(free, w, h, cellidx, adj, forced, banned, ep_cells, out_state):
    state = out_state
    for e in forced: state[e] = 1
    for e in banned:
        if state.get(e) == 1: return False
        state[e] = 2
    parent = {}
    def find(x):
        while parent.setdefault(x, x) != x:
            parent[x] = parent[parent[x]]; x = parent[x]
        return x
    for e, v in list(state.items()):
        if v == 1:
            a, b = find(e[0]), find(e[1])
            if a == b: return False
            parent[a] = b
    q = deque(cellidx); inq = set(cellidx)
    while q:
        c = q.popleft(); inq.discard(c)
        nb = adj[c]
        used = [e for e in nb if state.get(e) == 1]
        undec = [e for e in nb if e not in state]
        for e in list(undec):
            if find(e[0]) == find(e[1]):
                state[e] = 2; undec.remove(e)
                for p in e:
                    if p not in inq: q.append(p); inq.add(p)
        lo, hi = (1, 2) if c in ep_cells else (2, 2)
        if len(used) > hi or len(used) + len(undec) < lo: return False
        newly = []
        if len(used) == hi:
            for e in undec: state[e] = 2; newly.append(e)
        elif len(used) + len(undec) == lo:
            for e in undec:
                a, b = find(e[0]), find(e[1])
                if a == b: return False
                parent[a] = b; state[e] = 1; newly.append(e)
        for e in newly:
            for p in e:
                if p not in inq: q.append(p); inq.add(p)
    return True


if __name__ == "__main__":
    fn = synergy if os.environ.get("SYN") == "1" else run
    for lv in [int(a) for a in sys.argv[1:]] or [139]:
        fn(lv)
