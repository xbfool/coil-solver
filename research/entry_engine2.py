#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""滑动感知进入引擎 v2：时序递归 + 环模式 = 缝合线探测器（2026-08-22 夜）

两条 v1 没有的真规则：
1. 【时序递归】停在 q（沿 d）的软垫背 b=q+d（自由格）要求 b 更早被访问。
   递归验证「b 存在可行的更早进入」：feasible_past(b, forbidden, depth)——
   forbidden = 此刻尚未访问的格子集合（至少含当前滑行线），b 的更早进入
   (b,d') 要求 b-d' 自由且 ∉ forbidden，其停步的软垫背再递归（depth 递减）。
   禁集随递归增长 ⇒ 时序环被抓住。这就是 Tron 的 bounded brute force。
2. 【环模式】不给「终点豁免」：假设解是纯环（每格一进一出）。真解在缝处
   本来就断 ⇒ 引擎在缝附近推出矛盾（某格 0 个可行进入）= 缝合线探测器。

验证协议：
- L101 已知缝 (14,15)-(11,13)：矛盾格应落在缝附近
- 真解走查：环模式下真解的缝邻域允许报警（理论预期！），远离缝的报警=bug
"""
import re
import sys

DD = {0: (1, 0), 1: (-1, 0), 2: (0, 1), 3: (0, -1)}   # R L D U

def load(path):
    raw = open(path).read()
    w = int(re.search(r"x=(\d+)", raw).group(1))
    h = int(re.search(r"y=(\d+)", raw).group(1))
    b = re.search(r"board=([.X]+)", raw).group(1)
    free = set((x, y) for y in range(h) for x in range(w) if b[y * w + x] == ".")
    return w, h, free

class Engine2:
    def __init__(self, free, past_depth=2):
        self.free = free
        self.ban = set()          # (cell, dir) 从 dir 滑入 cell 不可能（环模式下）
        self.past_depth = past_depth

    def ray(self, c, d):
        """从 c 沿 d 的自由射线（含 c）"""
        dx, dy = DD[d]
        out = [c]
        q = c
        while (q[0] + dx, q[1] + dy) in self.free:
            q = (q[0] + dx, q[1] + dy)
            out.append(q)
        return out

    def feasible_past(self, b, forbidden, depth):
        """b 是否存在「早于 forbidden 全体」的可行进入。保守：depth 耗尽即放行（True）。"""
        if depth <= 0:
            return True
        for d in range(4):
            dx, dy = DD[d]
            prev = (b[0] - dx, b[1] - dy)
            if prev not in self.free or prev in forbidden:
                continue
            if (b, d) in self.ban:
                continue
            # b 的这次进入要停在某处（b 本身或更远——但停点在 b 之后不影响 b 的访问时刻；
            # 只需这次滑行本身可行：它的停步垫背若是软的且 ∈ forbidden ⇒ 不行，
            # 若软且 ∉ forbidden ⇒ 递归。保守起见只查「停在 b」和「穿过 b」两类中存在其一：
            #   穿过：b+d 自由且 ∉ forbidden 且 (b+d,d) 未 ban —— 后续交给迭代
            nxt = (b[0] + dx, b[1] + dy)
            if nxt in self.free and nxt not in forbidden and (nxt, d) not in self.ban:
                return True
            # 停在 b：垫背 bb = b+d
            if nxt not in self.free:
                return True                        # 硬垫背，成立
            if nxt in forbidden:
                continue                           # 软垫背还没被访问 ⇒ 此案不行
            if self.feasible_past(nxt, forbidden | {b}, depth - 1):
                return True
        return False

    def entry_ok(self, c, d):
        """环模式：从 d 滑入 c 是否存在自洽图景。滑行 = 从 c 沿 d 到某停点 q。"""
        dx, dy = DD[d]
        prev = (c[0] - dx, c[1] - dy)
        if prev not in self.free:
            return False
        # 来路：prev 沿 d 更早滑入未被 ban，或 prev 是转弯点（垂直进入未 ban）
        ok_prev = (prev, d) not in self.ban
        if not ok_prev:
            for q in range(4):
                if DD[q][0] == dx and DD[q][1] == dy:
                    continue
                if DD[q][0] == -dx and DD[q][1] == -dy:
                    continue
                pp = (prev[0] - DD[q][0], prev[1] - DD[q][1])
                if pp in self.free and (prev, q) not in self.ban:
                    ok_prev = True
                    break
        if not ok_prev:
            return False
        # 本滑行的停点：沿射线每个可停位置
        rayc = self.ray(c, d)
        slidecells = set()
        for q in rayc:
            slidecells.add(q)
            bq = (q[0] + dx, q[1] + dy)
            hard = bq not in self.free
            # 停在 q：垫背可行？
            if not hard:
                # 软垫背：bq 必须更早——递归时序验证（forbidden = 本滑行已含格）
                if not self.feasible_past(bq, frozenset(slidecells), self.past_depth):
                    # 这个停点不行，但可以继续滑（穿过 q）
                    continue
            # 停点成立 ⇒ 环模式下 q 处必须转弯（无终点豁免）：存在垂直续路
            turn_ok = False
            for p in range(4):
                px, py = DD[p]
                if (px, py) == (dx, dy) or (px, py) == (-dx, -dy):
                    continue
                t = (q[0] + px, q[1] + py)
                if t in self.free and (t, p) not in self.ban:
                    turn_ok = True
                    break
            if turn_ok:
                return True
            # 停点成立但无处可转 ⇒ 只能当终点——环模式不豁免 ⇒ 此停点作废，继续滑
        return False

    def run(self, max_rounds=50):
        for r in range(max_rounds):
            changed = False
            for c in self.free:
                for d in range(4):
                    if (c, d) in self.ban:
                        continue
                    if not self.entry_ok(c, d):
                        self.ban.add((c, d))
                        changed = True
            if not changed:
                return r + 1
        return max_rounds

def main():
    L = sys.argv[1] if len(sys.argv) > 1 else "101"
    depth = int(sys.argv[2]) if len(sys.argv) > 2 else 2
    w, h, free = load(f"/mnt/d/workspace/coilbench/levels_all/{L}")
    eng = Engine2(free, past_depth=depth)
    rounds = eng.run()
    possible = sum(1 for c in free for d in range(4)
                   if (c[0] - DD[d][0], c[1] - DD[d][1]) in free)
    nban = len(eng.ban)
    # 环矛盾格：0 个可行进入
    dead = [c for c in free if all((c, d) in eng.ban or
            (c[0] - DD[d][0], c[1] - DD[d][1]) not in free for d in range(4))]
    print(f"L{L} depth={depth}: 可能进入 {possible}，ban {nban}（{100.0*nban/possible:.1f}%），迭代 {rounds} 轮")
    print(f"环矛盾格（0 可行进入）：{len(dead)} 个 -> {dead[:12]}")

if __name__ == "__main__":
    main()

def verify_and_map(L="101", depth=2):
    w, h, free = load(f"/mnt/d/workspace/coilbench/levels_all/{L}")
    eng = Engine2(free, past_depth=depth)
    eng.run()
    sol = open(f"solutions/{L}.sol").read()
    sx = int(re.search(r"x=(\d+)", sol).group(1)); sy = int(re.search(r"y=(\d+)", sol).group(1))
    path = re.search(r"path=([UDLR]+)", sol).group(1)
    D2 = {"R": 0, "L": 1, "D": 2, "U": 3}
    vis = {(sx, sy)}; cur = (sx, sy); alarms = []
    for ch in path:
        d = D2[ch]; dx, dy = DD[d]
        while (cur[0]+dx, cur[1]+dy) in free and (cur[0]+dx, cur[1]+dy) not in vis:
            cur = (cur[0]+dx, cur[1]+dy); vis.add(cur)
            if (cur, d) in eng.ban:
                alarms.append((cur, "RLDU"[d]))
    end = cur
    print(f"真解报警 {len(alarms)} 处")
    for (c, dn) in alarms[:20]:
        ds = abs(c[0]-sx)+abs(c[1]-sy); de = abs(c[0]-end[0])+abs(c[1]-end[1])
        print(f"  {c} 进入{dn} 距起点{ds} 距终点{de}")
    # 逐格幸存进入数分布 + 最少的格子在哪
    cnt = {}
    for c in free:
        n = sum(1 for d in range(4) if (c[0]-DD[d][0], c[1]-DD[d][1]) in free and (c, d) not in eng.ban)
        cnt[c] = n
    from collections import Counter
    print("每格幸存进入数分布:", dict(sorted(Counter(cnt.values()).items())))
    low = sorted(free, key=lambda c: cnt[c])[:10]
    for c in low[:6]:
        ds = abs(c[0]-sx)+abs(c[1]-sy)
        print(f"  最少进入格 {c}: {cnt[c]} 个（距起点 {ds}）")

if len(sys.argv) > 3 and sys.argv[3] == "verify":
    verify_and_map(sys.argv[1], int(sys.argv[2]))

def shape_metric(L="101", depth=2):
    """Tron 对表指标：每格可行 (入,出) 形状数。入=幸存进入方向；出=邻格的幸存进入
    （c 沿 d 的出 = c+d 的「从 d 进入」幸存）。形状 = {入边, 出边} 无序对，入≠出反向。
    唯一形状格比例 = 静态确定率。"""
    w, h, free = load(f"/mnt/d/workspace/coilbench/levels_all/{L}")
    eng = Engine2(free, past_depth=depth)
    rounds = eng.run()
    from collections import Counter
    shapes = {}
    for c in free:
        ins = [d for d in range(4) if (c[0]-DD[d][0], c[1]-DD[d][1]) in free and (c, d) not in eng.ban]
        outs = [d for d in range(4) if (c[0]+DD[d][0], c[1]+DD[d][1]) in free and ((c[0]+DD[d][0], c[1]+DD[d][1]), d) not in eng.ban]
        # 形状 = (入方向 din, 出方向 dout)，不能原路返回：dout != 反(din)
        ns = 0
        for din in ins:
            for dout in outs:
                if DD[dout][0] == -DD[din][0] and DD[dout][1] == -DD[din][1]:
                    continue
                ns += 1
        shapes[c] = ns
    dist = Counter(shapes.values())
    uniq = sum(1 for v in shapes.values() if v == 1)
    print(f"L{L} depth={depth}: 形状数分布 {dict(sorted(dist.items()))}")
    print(f"唯一形状格：{uniq}/{len(free)}（{100.0*uniq/len(free):.1f}%）——Tron 口径 99.7%")

if len(sys.argv) > 3 and sys.argv[3] == "shape":
    shape_metric(sys.argv[1], int(sys.argv[2]))
