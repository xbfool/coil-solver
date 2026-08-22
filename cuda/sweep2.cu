// Coil GPU v2：warp-per-tree + 共享显存位棋盘 + CPU 品质剪枝
//
// 和 v1 的本质区别：
//   * 一个 warp（32 lane）磨一棵候选树；每块 warp 数按 shared 预算自适应（L501 为 5，
//     2 块/SM = 10 warp/SM）。
//   * 每树全部热状态放 shared memory：free 位棋盘 + seen 位棋盘 + BFS 环形队列 + 侧带掩码
//     （L501: 4.1KB + 4.1KB + 1KB + 64B ≈ 9.1KB/树）。DFS 栈在全局显存，但栈顶帧缓存在
//     寄存器里，每个节点只有 push/pop 各一次 12B 的全局访问。
//   * 剪枝集和 CPU v16-localreach **逐位对齐**（可用 cuda/oracle16 做逐起点节点数
//     差分验证）：
//       a. 奇偶染色精确计数：cnt[col[落点]^1] != (rem+1)/2 => 剪（比 |cnt0-cnt1|>1 强）
//       b. 落点死路：freedeg(落点)==0 且 rem>0 => 剪（reach_local 首行）
//       c. 全局死端/孤格计数：low_cnt/zero_cnt 增量维护，含「紧邻落点豁免」
//          （cheap_ok 的 adj_low/adj_zero 修正项——这是 v1 误杀真解那个坑的系统化修法）
//       d. 局部连通性 reach_local：滑行线两侧自由邻居两两连通 <=> 剩余区域仍连通。
//          warp 32 lane 协作 BFS（8 格 x 4 向打包展开、ballot + popc 前缀和入队）。
//          侧带源压成 run（连续段天然连通）：run 总数 1 直接判真；碰到 run 任一格
//          整段 collapse。B 探针（末 run 起、带帽）先试，把「失败洪水洗整块大区」
//          压到口袋大小；没判定再上 A 全洪水；队近满退化成位棋盘整窗扫荡（保 sound）。
//       e. Warnsdorff：候选按滑行落点自由度升序（用 5 步比较交换网络排，
//          键 = deg*4+dir，天然稳定 => 和 CPU 插入排序同序）。
//   * 统一节点预算 + 全局工作队列（warp 干完一棵领下一棵，消灭长尾）。
//
// 节点语义 = CPU 的 dfs() 调用数（每次落点展开记 1，预算判定同 CPU 的
// `nodes++ >= limit`），所以这里报告的节点/秒就是「CPU 品质节点」速率。
//
// 编译：nvcc -O3 -arch=compute_90 -code=compute_90 -o sweep2 sweep2.cu
//      （WSL 的 nvcc 12.4 最高 sm_90，出 PTX 让驱动在 sm_120 上 JIT——已验证可行）
// 运行：./sweep2 <levelfile> [budget-per-tree] [maxtrees] [--dump] [--nostop] [--sol <file>]
//   --dump   逐树打印「x y result nodes」（和 oracle16 输出格式一致，差分测试用）
//   --nostop 有树解出后其它树不收工（差分测试必开，否则节点数不可比）
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cuda_runtime.h>

#define CK(x) do { cudaError_t e_ = (x); if (e_) { \
    printf("CUDA err %s @%d\n", cudaGetErrorString(e_), __LINE__); exit(1); } } while (0)

#define WPB   4        // warps (trees) per block
#define QCAP  512     // BFS 环形队列条目数（u16），须为 2 的幂
#define QMASK (QCAP - 1)

struct TreeOut {                 // 每树 32B 结果
    unsigned nodes;              // 消耗节点数（CPU 语义）
    int result;                  // 1=解出 0=搜穷 -1=预算尽 -2=被 found_flag 叫停 -3=没轮到
    unsigned rcalls, rcells;     // reach_local 调用数 / BFS 展开格数（profile 用）
    unsigned sweeps, maxsp;      // 队满退化次数 / 最大栈深
    unsigned kcyc_reach, kcyc_tree;   // reach_local 累计周期 / 全树周期（>>10）
    unsigned nfail, cfail;       // reach_local 失败次数 / 失败洪水的格数
};

__constant__ int cR, cC, cWpr, cNW, cTotalFree;
__constant__ unsigned cBudget;
__constant__ int cStop;          // 1 = 有解全员收工

// 方向：0=L 1=U 2=R 3=D（和 CPU 的 delta {-1,-W,+1,+W}、DCH "LURD" 对齐）
__device__ __forceinline__ int DRow(int d) { return (d & 1) ? (d - 2) : 0; }
__device__ __forceinline__ int DCol(int d) { return (d & 1) ? 0 : (d - 1); }

__device__ __forceinline__ int bget(const unsigned *bs, int r, int c) {
    return (bs[r * cWpr + (c >> 5)] >> (c & 31)) & 1;
}
// 只准对「自由格或其邻居可安全落在 [0,R)x[0,C) 的格」调用（自由格必在内圈，成立）
__device__ __forceinline__ int freedeg4(const unsigned *F, int r, int c) {
    return bget(F, r, c - 1) + bget(F, r, c + 1) + bget(F, r - 1, c) + bget(F, r + 1, c);
}

// 滑行：从 (pr,pc) 沿 d 直到撞非自由格，返回 len（调用前提：第一格自由 => len>=1）
__device__ __forceinline__ int wslide(const unsigned *F, int pr, int pc, int d, int lane) {
    int dr = DRow(d), dc = DCol(d), base = 0;
    for (;;) {
        int k = base + lane + 1;
        int r = pr + dr * k, c = pc + dc * k;
        int blocked = 1;
        if ((unsigned)r < (unsigned)cR && (unsigned)c < (unsigned)cC) blocked = !bget(F, r, c);
        unsigned bal = __ballot_sync(0xffffffffu, blocked);
        if (bal) return base + __ffs(bal) - 1;
        base += 32;
    }
}

// 把滑行线整体置 0（mark）或置 1（unmark）。first=(fr,fc) 是 p+delta 那格。
// 横线：每 lane 负责一个 32 位字（字内合并掩码，无原子无冲突）；
// 竖线：每 lane 负责一格（不同字，无冲突）。
__device__ __forceinline__ void apply_line(unsigned *F, int fr, int fc, int d, int len,
                                           int setbits, int lane) {
    if (d & 1) {                                   // 竖线
        int r0 = (d == 1) ? fr - (len - 1) : fr;
        int wcol = fc >> 5; unsigned m = 1u << (fc & 31);
        for (int k = lane; k < len; k += 32) {
            int w = (r0 + k) * cWpr + wcol;
            if (setbits) F[w] |= m; else F[w] &= ~m;
        }
    } else {                                       // 横线
        int c0 = (d == 0) ? fc - (len - 1) : fc;
        int w0 = c0 >> 5, w1 = (c0 + len - 1) >> 5;
        int wi = w0 + lane;
        if (wi <= w1) {
            int lo = max(c0, wi << 5), hi = min(c0 + len, (wi << 5) + 32);
            unsigned m = (hi - lo >= 32) ? 0xffffffffu : ((1u << (hi - lo)) - 1u) << (lo & 31);
            int w = fr * cWpr + wi;
            if (setbits) F[w] |= m; else F[w] &= ~m;
        }
    }
    __syncwarp();
}

// mark 一条滑行线并增量维护 (cnt0,cnt1,low,zero)。批量公式和 CPU 逐格 mark 的
// 终态严格相等（low' = low - #{线上格 deg<=1} + #{侧邻自由格 deg==2}，zero 同理；
// 侧邻格和线的邻接恰好一对一：直线的垂向邻居唯一，轴向两端是 p/挡格，都不在剩余集里）。
__device__ __forceinline__ void mark_line_counts(unsigned *F, int fr, int fc, int d, int len,
                                                 int lane, int colp,
                                                 int *cnt0, int *cnt1, int *low, int *zero) {
    int dr = DRow(d), dc = DCol(d);
    int rm = 0, add = 0;                                  // 打包：low | zero<<16
    for (int k = lane; k < len; k += 32) {                // 线上格（mark 前的 deg）
        int r = fr + k * dr, c = fc + k * dc;
        int dg = freedeg4(F, r, c);
        rm += (dg <= 1) + ((dg == 0) << 16);
    }
    int len2 = len * 2;
    for (int i = lane; i < len2; i += 32) {               // 两条侧带（mark 前的 deg）
        int k = (i >= len) ? i - len : i, t = (i >= len) ? 1 : -1;
        int r = fr + k * dr + ((d & 1) ? 0 : t);
        int c = fc + k * dc + ((d & 1) ? t : 0);
        if (bget(F, r, c)) {
            int dg = freedeg4(F, r, c);
            add += (dg == 2) + ((dg == 1) << 16);
        }
    }
    rm  = __reduce_add_sync(0xffffffffu, rm);
    add = __reduce_add_sync(0xffffffffu, add);
    __syncwarp();
    apply_line(F, fr, fc, d, len, 0, lane);
    *low  += (add & 0xffff) - (rm & 0xffff);
    *zero += (add >> 16)    - (rm >> 16);
    int c0line = ((colp ^ 1) == 0) ? (len + 1) >> 1 : len >> 1;   // 线上颜色交替，首格色 = colp^1
    *cnt0 -= c0line;
    *cnt1 -= len - c0line;
}

// O(1) 剪枝（CPU cheap_ok 逐位对齐）：奇偶精确计数 + 死端/孤格全局计数（含落点邻居豁免）
__device__ __forceinline__ bool cheap_ok(const unsigned *F, int cr2, int cc2, int rem2,
                                         int colc, int cnt0, int cnt1, int low, int zero) {
    int other = colc ? cnt0 : cnt1;                       // cnt[col^1]
    if (other != (rem2 + 1) >> 1) return false;
    int adj_low = 0, adj_zero = 0;
    #pragma unroll
    for (int d = 0; d < 4; ++d) {
        int r = cr2 + DRow(d), c = cc2 + DCol(d);
        if (bget(F, r, c)) {
            int dg = freedeg4(F, r, c);
            adj_low += (dg <= 1); adj_zero += (dg == 0);
        }
    }
    if (zero - adj_zero > 0) return false;                // 有进不去的孤格
    return low - adj_low + adj_zero <= 1;                 // 死端最多 1 个（未来的终点）
}

// 位棋盘整窗扫荡洪水（BFS 队满时的保底路径）：S 内的 1 逐层向邻位扩散到不动点。
// 行按 32 位对齐补齐、边框/补齐列恒为墙，所以跨字/跨行的移位杂散位会被 F 掩掉。
__device__ __forceinline__ void sweep_flood(const unsigned *F, unsigned *S,
                                            int *pwlo, int *pwhi, int lane) {
    int wlo = *pwlo, whi = *pwhi;
    for (;;) {
        wlo = max(0, wlo - cWpr); whi = min(cNW - 1, whi + cWpr);
        int changed = 0;
        for (int base = wlo; base <= whi; base += 32) {
            int w = base + lane;
            if (w <= whi) {
                unsigned cur = S[w];
                unsigned up  = (w >= cWpr) ? S[w - cWpr] : 0u;
                unsigned dn  = (w + cWpr < cNW) ? S[w + cWpr] : 0u;
                unsigned lf  = (w > 0) ? S[w - 1] : 0u;
                unsigned rg  = (w + 1 < cNW) ? S[w + 1] : 0u;
                unsigned nb  = (up | dn | (cur << 1) | (lf >> 31) | (cur >> 1) | (rg << 31))
                               & F[w] & ~cur;
                if (nb) { S[w] |= nb; changed = 1; }
            }
        }
        __syncwarp();
        if (!__any_sync(0xffffffffu, changed)) break;
    }
    *pwlo = wlo; *pwhi = whi;
}

__device__ __forceinline__ void clear_window(unsigned *S, int wlo, int whi, int lane) {
    for (int w = wlo + lane; w <= whi; w += 32) S[w] = 0u;
    __syncwarp();
}

// 求「包含位置 k（该位已置 1）的连续 1 段」的边界 [k0,k1]。mask 超出 len 的位恒 0。
__device__ __forceinline__ void run_bounds(const unsigned *M, int len, int k, int *k0, int *k1) {
    int j = k >> 5, b = k & 31;
    unsigned mj = M[j];
    unsigned below = ~mj & ((b == 0) ? 0u : ((1u << b) - 1u));
    if (below) *k0 = (j << 5) + (32 - __clz(below));
    else {
        int jj = j - 1;
        while (jj >= 0 && M[jj] == 0xffffffffu) --jj;
        *k0 = (jj < 0) ? 0 : (jj << 5) + (32 - __clz(~M[jj]));
    }
    unsigned above = (b == 31) ? 0u : (~mj & ~((2u << b) - 1u));
    if (above) *k1 = (j << 5) + __ffs(above) - 2;
    else {
        int nw = (len + 31) >> 5, jj = j + 1;
        while (jj < nw && M[jj] == 0xffffffffu) ++jj;
        *k1 = (jj >= nw) ? len - 1 : ((jj << 5) + __ffs(~M[jj]) - 2);
    }
}

// collapse：把 strip s 里包含位置 k 的整段 run 置 seen + 入队，count/window 同步更新。
// 调用方保证 uniform（全 warp 同参）。
__device__ __forceinline__ void collapse_run(const unsigned *STRIP, unsigned *S, unsigned short *Q,
                                             int fr, int fc, int d, int len, int s, int k,
                                             int lane, int *tail, int *left, int *wmin, int *wmax) {
    int dr = DRow(d), dc = DCol(d), vert = d & 1;
    int k0, k1;
    run_bounds(STRIP + s * 8, len, k, &k0, &k1);
    int size = k1 - k0 + 1, t = s ? 1 : -1;
    if (vert) {
        int cx = fc + t, ra = fr + k0 * dr, rb = fr + k1 * dr;
        if (ra > rb) { int tmp = ra; ra = rb; rb = tmp; }
        int wcol = cx >> 5; unsigned mm = 1u << (cx & 31);
        for (int i = lane; i < size; i += 32) S[(ra + i) * cWpr + wcol] |= mm;
        *wmin = min(*wmin, ra * cWpr + wcol); *wmax = max(*wmax, rb * cWpr + wcol);
        for (int i = lane; i < size; i += 32)
            Q[(*tail + i) & QMASK] = (unsigned short)(((ra + i) << 8) | cx);
    } else {
        int rx = fr + t, ca = fc + k0 * dc, cb = fc + k1 * dc;
        if (ca > cb) { int tmp = ca; ca = cb; cb = tmp; }
        int w0 = ca >> 5, w1 = cb >> 5, wb = rx * cWpr;
        int wi = w0 + lane;
        if (wi <= w1) {
            int lo = max(ca, wi << 5), hi = min(cb + 1, (wi << 5) + 32);
            S[wb + wi] |= ((hi - lo >= 32) ? 0xffffffffu : ((1u << (hi - lo)) - 1u) << (lo & 31));
        }
        *wmin = min(*wmin, wb + w0); *wmax = max(*wmax, wb + w1);
        for (int i = lane; i < size; i += 32)
            Q[(*tail + i) & QMASK] = (unsigned short)((rx << 8) | (ca + i));
    }
    *tail += size; *left -= size;
    __syncwarp();
}

// 从某个 run 起洪水，直到判定或放弃。
// 返回 1=全部 run 连通  0=洪水灌尽仍有 run 未接（断开）  -1=没判定（enqueue 超 cap / 队近满）。
// S 的置位窗口通过 wmin/wmax 传出，由调用方清。
__device__ int flood_runs(const unsigned *F, unsigned *S, unsigned short *Q, const unsigned *STRIP,
                          int fr, int fc, int d, int len, int lr0, int lc0, int nsrc,
                          int seeds, int seedk, int cap, int lane,
                          int *pwmin, int *pwmax, unsigned *rcells) {
    const int vert = d & 1;
    const int margin = len + 40;                          // 一次 collapse <= len，一轮普通入队 <= 32
    int head = 0, tail = 0, left = nsrc;
    int wmin = 0x7fffffff, wmax = -1;
    collapse_run(STRIP, S, Q, fr, fc, d, len, seeds, seedk, lane, &tail, &left, &wmin, &wmax);
    int ret = -2;
    while (ret == -2) {
        if (left == 0) { ret = 1; break; }
        int avail = tail - head;
        if (avail == 0) { ret = 0; break; }
        if (tail >= cap || avail > QCAP - margin) { ret = -1; break; }
        int take = min(avail, 8);
        unsigned e = Q[(head + min(lane >> 2, take - 1)) & QMASK];
        head += take;
        int er = (int)(e >> 8), ec = (int)(e & 255), d2 = lane & 3;
        int nr = er + DRow(d2), nc = ec + DCol(d2);
        bool active = (lane >> 2) < take;
        int w = nr * cWpr + (nc >> 5); unsigned m = 1u << (nc & 31);
        bool isfree = active && (F[w] & m);
        bool s_geom;
        if (vert) s_geom = (nc == fc - 1 || nc == fc + 1) && (unsigned)(nr - lr0) < (unsigned)len;
        else      s_geom = (nr == fr - 1 || nr == fr + 1) && (unsigned)(nc - lc0) < (unsigned)len;
        bool seen0 = isfree && (S[w] & m);
        bool newly = isfree && !s_geom && !seen0;
        if (newly) {
            unsigned old = atomicOr(&S[w], m);
            newly = !(old & m);
            if (newly) { wmin = min(wmin, w); wmax = max(wmax, w); }
        }
        unsigned bn = __ballot_sync(0xffffffffu, newly);
        if (newly) Q[(tail + __popc(bn & ((1u << lane) - 1u))) & QMASK] = (unsigned short)((nr << 8) | nc);
        tail += __popc(bn);
        unsigned bs = __ballot_sync(0xffffffffu, isfree && s_geom && !seen0);
        while (bs) {
            int l = __ffs(bs) - 1; bs &= bs - 1;
            int hr = __shfl_sync(0xffffffffu, nr, l), hc = __shfl_sync(0xffffffffu, nc, l);
            int s, k;
            if (vert) { s = (hc == fc + 1); k = (hr - fr) * ((d == 1) ? -1 : 1); }
            else      { s = (hr == fr + 1); k = (hc - fc) * ((d == 0) ? -1 : 1); }
            // canonical 去重：只有 collapse 会置源格 seen 位；段首位已置 => 本 run 已收
            int k0, k1;
            run_bounds(STRIP + s * 8, len, k, &k0, &k1);
            int t = s ? 1 : -1;
            int r0c = fr + k0 * DRow(d) + (vert ? 0 : t), c0c = fc + k0 * DCol(d) + (vert ? t : 0);
            if (bget(S, r0c, c0c)) continue;
            if (tail - head > QCAP - margin + 32) { ret = -1; break; }
            collapse_run(STRIP, S, Q, fr, fc, d, len, s, k, lane, &tail, &left, &wmin, &wmax);
        }
        __syncwarp();                                     // 队列写 -> 下一轮读
    }
    *rcells += (unsigned)tail;
    *pwmin = __reduce_min_sync(0xffffffffu, wmin);
    *pwmax = __reduce_max_sync(0xffffffffu, wmax);
    return ret;
}

// 局部连通性（CPU reach_local 语义严格等价，布尔结果逐位一致）。
//   * 侧带压位掩码；同带连续源天然连通 => 只需「每个 run 被碰到」。truns==1 直接判真。
//   * 洪水碰到 run 的任一格 => collapse 整段（置位 + 入队 + 计数一次减完）。
//   * 三段式：B 探针（从「最后一个 run」起，带 ~REACH_BCAP 格帽）先跑——
//     被切下的小口袋无论含不含种子 run，两段里总有一段便宜地灌尽它；
//     失败洪水从「平均洗 7700 格」降到「口袋大小」。B 没判定再上 A 全洪水，
//     队满则 sweep 保底。每段都是精确的连通性事实，剪枝布尔值不变。
#ifndef REACH_BCAP
#define REACH_BCAP 256
#endif
__device__ bool reach_local(const unsigned *F, unsigned *S, unsigned short *Q, unsigned *STRIP,
                            int fr, int fc, int d, int len, int cr2, int cc2,
                            int lane, unsigned *rcells, unsigned *sweeps,
                            unsigned *nfail, unsigned *cfail) {
    if (freedeg4(F, cr2, cc2) == 0) return false;         // 滑进死胡同
    const int dr = DRow(d), dc = DCol(d), vert = d & 1;
    const int lr0 = (d == 1) ? fr - (len - 1) : fr;       // 线的最小角
    const int lc0 = (d == 0) ? fc - (len - 1) : fc;
    // 侧带位掩码（位置 k = 线上第 k 格的垂向邻居），顺带数 nsrc
    int nsrc = 0;
    #pragma unroll
    for (int s = 0; s < 2; ++s) {
        int t = s ? 1 : -1;
        for (int base = 0; base < len; base += 32) {
            int k = base + lane;
            bool fb = (k < len) && bget(F, fr + k * dr + (vert ? 0 : t), fc + k * dc + (vert ? t : 0));
            unsigned m = __ballot_sync(0xffffffffu, fb);
            if (lane == 0) STRIP[s * 8 + (base >> 5)] = m;
            nsrc += __popc(m);
        }
    }
    __syncwarp();
    if (nsrc == 0) return false;                          // rem2>0 却没格挨着线
    int nwm = (len + 31) >> 5, truns = 0;
    #pragma unroll
    for (int s = 0; s < 2; ++s) {
        unsigned carry = 0;
        for (int j = 0; j < nwm; ++j) {
            unsigned m = STRIP[s * 8 + j];
            truns += __popc(m & ~((m << 1) | carry));
            carry = m >> 31;
        }
    }
    if (truns == 1) return true;                          // 全部源在同一条连续段里

    // 首 run 与末 run（B 探针从末 run 起，离首 run 最远）
    int sA = 0, kA = -1, sB = 0, kB = -1;
    for (int s = 0; s < 2 && kA < 0; ++s)
        for (int j = 0; j < nwm; ++j) {
            unsigned m = STRIP[s * 8 + j];
            if (m) { sA = s; kA = (j << 5) + __ffs(m) - 1; break; }
        }
    for (int s = 1; s >= 0 && kB < 0; --s)
        for (int j = nwm - 1; j >= 0; --j) {
            unsigned m = STRIP[s * 8 + j];
            if (m) { sB = s; kB = (j << 5) + 32 - __clz(m) - 1; break; }
        }
    int wmin, wmax;
    unsigned rc0 = *rcells;
    int r = flood_runs(F, S, Q, STRIP, fr, fc, d, len, lr0, lc0, nsrc, sB, kB, REACH_BCAP,
                       lane, &wmin, &wmax, rcells);
    if (wmax >= 0) clear_window(S, wmin, wmax, lane);
    bool ok;
    if (r >= 0) ok = (r == 1);
    else {
        r = flood_runs(F, S, Q, STRIP, fr, fc, d, len, lr0, lc0, nsrc, sA, kA, 0x7fffffff,
                       lane, &wmin, &wmax, rcells);
        if (r >= 0) ok = (r == 1);
        else {                                            // 队近满：sweep 保底
            ++*sweeps;
            sweep_flood(F, S, &wmin, &wmax, lane);
            int missing = 0, len2 = len * 2;
            for (int i = lane; i < len2; i += 32) {
                int k = (i >= len) ? i - len : i, t = (i >= len) ? 1 : -1;
                int rr2 = fr + k * dr + (vert ? 0 : t);
                int cc3 = fc + k * dc + (vert ? t : 0);
                if (bget(F, rr2, cc3) && !bget(S, rr2, cc3)) missing = 1;
            }
            ok = !__any_sync(0xffffffffu, missing);
        }
        if (wmax >= 0) clear_window(S, wmin, wmax, lane);
    }
    if (!ok) { ++*nfail; *cfail += *rcells - rc0; }
    return ok;
}

// 起点连通性（CPU reach_ok）：从起点四邻洪水，数到的格数 == remaining 才行。
__device__ bool reach_root(const unsigned *F, unsigned *S, unsigned short *Q,
                           int sr, int sc, int remaining, int lane,
                           unsigned *rcells, unsigned *sweeps) {
    int head = 0, tail = 0, count = 0;
    int wmin = 0x7fffffff, wmax = -1;
    #pragma unroll
    for (int d = 0; d < 4; ++d) {
        int r = sr + DRow(d), c = sc + DCol(d);
        if (bget(F, r, c)) {
            int w = r * cWpr + (c >> 5);
            if (lane == 0) { S[w] |= 1u << (c & 31); Q[tail & QMASK] = (unsigned short)((r << 8) | c); }
            wmin = min(wmin, w); wmax = max(wmax, w);
            ++tail; ++count;
        }
    }
    __syncwarp();
    bool fellback = false, ok = false;
    for (;;) {
        int avail = tail - head;
        if (avail == 0) break;
        if (avail > QCAP - 132) { fellback = true; break; }
        int take = min(avail, 32);
        unsigned e = Q[(head + min(lane, take - 1)) & QMASK];
        int er = (int)(e >> 8), ec = (int)(e & 255);
        head += take;
        #pragma unroll
        for (int d2 = 0; d2 < 4; ++d2) {
            int nr = er + DRow(d2), nc = ec + DCol(d2);
            bool newly = false;
            int w = nr * cWpr + (nc >> 5);
            if (lane < take) {
                unsigned m = 1u << (nc & 31);
                if (F[w] & m) {
                    unsigned old = atomicOr(&S[w], m);
                    if (!(old & m)) { newly = true; wmin = min(wmin, w); wmax = max(wmax, w); }
                }
            }
            unsigned bn = __ballot_sync(0xffffffffu, newly);
            if (newly) {
                int rank = __popc(bn & ((1u << lane) - 1u));
                Q[(tail + rank) & QMASK] = (unsigned short)((nr << 8) | nc);
            }
            count += __popc(bn);
            tail += __popc(bn);
        }
        __syncwarp();
        if (count >= remaining) { ok = true; break; }
    }
    *rcells += (unsigned)count;
    wmin = __reduce_min_sync(0xffffffffu, wmin);
    wmax = __reduce_max_sync(0xffffffffu, wmax);
    if (wmax < 0) return remaining == 0;                  // 起点没有自由邻居
    if (fellback) {
        ++*sweeps;
        sweep_flood(F, S, &wmin, &wmax, lane);
        int cl = 0;
        for (int w = wmin + lane; w <= wmax; w += 32) cl += __popc(S[w]);
        count = __reduce_add_sync(0xffffffffu, cl);
        ok = (count == remaining);
    } else if (!ok) ok = (count == remaining);
    clear_window(S, wmin, wmax, lane);
    return ok;
}

// ==== 帧打包：3 个 u32 ====
// a: indir(2) | inlen<<2(8) | cands<<10(8) | ncand<<18(3) | next<<21(3)
// b: prev_low | prev_zero<<16
// c: prev_cnt0
__device__ __forceinline__ void frame_store(unsigned *fp, unsigned a, unsigned b, unsigned c, int lane) {
    if (lane == 0) { fp[0] = a; fp[1] = b; fp[2] = c; }
    __syncwarp();
}
__device__ __forceinline__ void frame_load(const unsigned *fp, unsigned *a, unsigned *b, unsigned *c, int lane) {
    unsigned v = fp[min(lane, 2)];
    *a = __shfl_sync(0xffffffffu, v, 0);
    *b = __shfl_sync(0xffffffffu, v, 1);
    *c = __shfl_sync(0xffffffffu, v, 2);
}

__global__ void __launch_bounds__(32 * 8)
k_sweep2(const unsigned * __restrict__ g0bits, const unsigned short * __restrict__ starts,
         int ns, TreeOut * __restrict__ out, unsigned * __restrict__ frames, int framestride,
         int *next_ctr, int *found_flag, int *sol_meta, unsigned char *sol_dirs)
{
    extern __shared__ unsigned char smem[];
    const int lane = threadIdx.x, slot = threadIdx.y;
    size_t slotbytes = ((size_t)cNW * 8 + QCAP * 2 + 64 + 127) & ~(size_t)127;
    unsigned *F = (unsigned *)(smem + slot * slotbytes);
    unsigned *S = F + cNW;
    unsigned short *Q = (unsigned short *)(S + cNW);
    unsigned *STRIP = (unsigned *)(Q + QCAP);             // 2 x 8 words 侧带掩码
    const int gwarp = blockIdx.x * blockDim.y + slot;
    unsigned *myframes = frames + (size_t)gwarp * framestride;

    for (;;) {
        int t;
        if (lane == 0) t = atomicAdd(next_ctr, 1);
        t = __shfl_sync(0xffffffffu, t, 0);
        if (t >= ns) return;

        unsigned spk = starts[t];
        int sr = (int)(spk >> 8), sc = (int)(spk & 255);
        unsigned nodes = 0, rcalls = 0, rcells = 0, sweeps = 0, maxsp = 0, nfail = 0, cfail = 0;
        long long cyc_reach = 0, cyc_t0 = clock64();
        int result;

        if (cStop && *(volatile int *)found_flag) { result = -2; goto record; }

        // ---- 每树初始化：拷位棋盘、清 seen、起点落子、全盘扫计数 ----
        for (int w = lane; w < cNW; w += 32) { F[w] = g0bits[w]; S[w] = 0u; }
        __syncwarp();
        if (lane == 0) F[sr * cWpr + (sc >> 5)] &= ~(1u << (sc & 31));
        __syncwarp();
        {
            int a = 0, b = 0;                             // a: cnt0|cnt1<<16  b: low|zero<<16
            for (int r = 1; r < cR - 1; ++r)
                for (int c = 1 + lane; c < cC - 1; c += 32)
                    if (bget(F, r, c)) {
                        int col = (r + c) & 1;
                        a += col ? 0x10000 : 1;
                        int dg = freedeg4(F, r, c);
                        b += (dg <= 1) + ((dg == 0) << 16);
                    }
            a = __reduce_add_sync(0xffffffffu, a);
            b = __reduce_add_sync(0xffffffffu, b);
            int cnt0 = a & 0xffff, cnt1 = a >> 16;
            int low = b & 0xffff, zero = b >> 16;
            int remaining = cTotalFree - 1;
            int colp = (sr + sc) & 1;
            int pr = sr, pc = sc;

            if (remaining > 0 && !reach_root(F, S, Q, sr, sc, remaining, lane, &rcells, &sweeps)) {
                result = 0; goto record;                  // 起点是割点，直接证伪
            }

            // ---- 迭代 DFS ----
            int sp = 0;
            unsigned fa = 0, fb = 0, fc2 = 0;             // 栈顶帧（寄存器缓存）
            bool expanding = true;
            for (;;) {
                if (expanding) {
                    if (remaining == 0) { result = 1; break; }
                    if (cStop && (nodes & 255u) == 0u && *(volatile int *)found_flag) { result = -2; break; }
                    ++nodes;
                    if (nodes > cBudget) { result = -1; break; }
                    // 候选生成（逐方向：滑、mark、验、unmark）
                    int k0 = 63, k1 = 63, k2 = 63, k3 = 63;   // 键 = freedeg(落点)*4 + dir，63=空
                    int ncand = 0;
                    for (int d = 0; d < 4; ++d) {
                        int fr = pr + DRow(d), fcc = pc + DCol(d);
                        if (!bget(F, fr, fcc)) continue;
                        int len = wslide(F, pr, pc, d, lane);
                        int er = pr + DRow(d) * len, ec = pc + DCol(d) * len;
                        int rem2 = remaining - len;
                        if (rem2 > 0) {                   // 奇偶预筛（纯算术）：注定被 cheap_ok
                            int colc = colp ^ (len & 1);  // 的奇偶项拒掉的候选连 mark 都省了
                            int c0l = ((colp ^ 1) == 0) ? (len + 1) >> 1 : len >> 1;
                            int other = colc ? (cnt0 - c0l) : (cnt1 - (len - c0l));
                            if (other != (rem2 + 1) >> 1) continue;
                        }
                        int sv0 = cnt0, sv1 = cnt1, svl = low, svz = zero;
                        mark_line_counts(F, fr, fcc, d, len, lane, colp, &cnt0, &cnt1, &low, &zero);
                        bool okc;
                        if (rem2 == 0) okc = true;
                        else {
                            okc = cheap_ok(F, er, ec, rem2, colp ^ (len & 1), cnt0, cnt1, low, zero);
                            if (okc) {
                                ++rcalls;
                                long long c0_ = clock64();
                                okc = reach_local(F, S, Q, STRIP, fr, fcc, d, len, er, ec, lane, &rcells, &sweeps, &nfail, &cfail);
                                cyc_reach += clock64() - c0_;
                            }
                        }
                        if (okc) {
                            int key = freedeg4(F, er, ec) * 4 + d;
                            if (ncand == 0) k0 = key; else if (ncand == 1) k1 = key;
                            else if (ncand == 2) k2 = key; else k3 = key;
                            ++ncand;
                        }
                        apply_line(F, fr, fcc, d, len, 1, lane);      // unmark
                        cnt0 = sv0; cnt1 = sv1; low = svl; zero = svz;
                    }
                    // 5 步比较交换网络（键含 dir，全不等 => 稳定序 == CPU 插入排序）
                    #define CSWP(x, y) { int t_ = min(x, y); y = max(x, y); x = t_; }
                    CSWP(k0, k1) CSWP(k2, k3) CSWP(k0, k2) CSWP(k1, k3) CSWP(k1, k2)
                    #undef CSWP
                    unsigned cands = (unsigned)((k0 & 3) | ((k1 & 3) << 2) | ((k2 & 3) << 4) | ((k3 & 3) << 6));
                    fa = (fa & 0x3ffu) | (cands << 10) | ((unsigned)ncand << 18);   // 保留 indir/inlen；next=0
                    expanding = false;
                }
                int ncand = (int)((fa >> 18) & 7), next = (int)((fa >> 21) & 7);
                if (next < ncand) {
                    int d = (int)((fa >> (10 + next * 2)) & 3);
                    fa += 1u << 21;                                   // next++
                    int len = wslide(F, pr, pc, d, lane);
                    frame_store(myframes + (size_t)sp * 3, fa, fb, fc2, lane);
                    int fr = pr + DRow(d), fcc = pc + DCol(d);
                    unsigned pl = (unsigned)low | ((unsigned)zero << 16), pc0 = (unsigned)cnt0;
                    mark_line_counts(F, fr, fcc, d, len, lane, colp, &cnt0, &cnt1, &low, &zero);
                    ++sp; if ((unsigned)sp > maxsp) maxsp = sp;
                    fa = (unsigned)d | ((unsigned)len << 2);          // 新帧：indir/inlen，cands 待展开填
                    fb = pl; fc2 = pc0;
                    pr += DRow(d) * len; pc += DCol(d) * len;
                    colp ^= (len & 1); remaining -= len;
                    expanding = true;
                    continue;
                }
                // 回溯
                if (sp == 0) { result = 0; break; }                   // 整树搜穷
                {
                    int d = (int)(fa & 3), len = (int)((fa >> 2) & 255);
                    int fr = pr - DRow(d) * (len - 1), fcc = pc - DCol(d) * (len - 1);
                    apply_line(F, fr, fcc, d, len, 1, lane);          // 恢复线
                    remaining += len;
                    low = (int)(fb & 0xffff); zero = (int)(fb >> 16);
                    cnt0 = (int)fc2; cnt1 = remaining - cnt0;
                    colp ^= (len & 1);
                    pr -= DRow(d) * len; pc -= DCol(d) * len;
                    --sp;
                    frame_load(myframes + (size_t)sp * 3, &fa, &fb, &fc2, lane);
                    expanding = false;
                }
            }
            // 解出：写路径（帧 1..sp 的 indir 就是方向序列）
            if (result == 1) {
                frame_store(myframes + (size_t)sp * 3, fa, fb, fc2, lane);
                int win = 0;
                if (lane == 0) win = (atomicCAS(found_flag, 0, 1) == 0);
                win = __shfl_sync(0xffffffffu, win, 0);
                if (win) {
                    if (lane == 0) { sol_meta[0] = t; sol_meta[1] = (int)spk; sol_meta[2] = sp; }
                    for (int k = 1 + lane; k <= sp; k += 32)
                        sol_dirs[k - 1] = (unsigned char)(myframes[(size_t)k * 3] & 3u);
                    __threadfence();
                }
            }
        }
record:
        if (lane == 0) {
            TreeOut o; o.nodes = nodes; o.result = result;
            o.rcalls = rcalls; o.rcells = rcells; o.sweeps = sweeps; o.maxsp = maxsp;
            o.kcyc_reach = (unsigned)(cyc_reach >> 10);
            o.kcyc_tree = (unsigned)((clock64() - cyc_t0) >> 10);
            o.nfail = nfail; o.cfail = cfail;
            out[t] = o;
        }
        __syncwarp();
    }
}

// ======================= 主机侧 =======================
static int hW, hH, hR, hC, hWpr, hNW, hTotalFree;

int main(int argc, char **argv) {
    if (argc < 2) { printf("usage: %s <level> [budget] [maxtrees] [--dump] [--nostop] [--sol <f>]\n", argv[0]); return 1; }
    long long budget = 2000000; int maxtrees = 1 << 30, dump = 0, nostop = 0;
    const char *solfile = nullptr;
    int posarg = 0;
    for (int i = 2; i < argc; ++i) {
        if (!strcmp(argv[i], "--dump")) dump = 1;
        else if (!strcmp(argv[i], "--nostop")) nostop = 1;
        else if (!strcmp(argv[i], "--sol") && i + 1 < argc) solfile = argv[++i];
        else if (posarg == 0) { budget = atoll(argv[i]); posarg = 1; }
        else if (posarg == 1) { maxtrees = atoi(argv[i]); posarg = 2; }
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { printf("no level\n"); return 1; }
    static char buf[1 << 22];
    size_t nread = fread(buf, 1, sizeof buf - 1, f); buf[nread] = 0; fclose(f);
    int w = 0, h = 0; char *bp = strstr(buf, "board=");
    sscanf(strstr(buf, "x="), "x=%d", &w);
    sscanf(strstr(buf, "y="), "y=%d", &h);
    if (!bp || w <= 0 || h <= 0) { printf("bad level\n"); return 1; }
    bp += 6;
    hW = w; hH = h; hR = h + 2; hC = w + 2;
    if (hR > 255 || hC > 255) { printf("board too large for u16 packing (%dx%d)\n", w, h); return 1; }
    if (budget > 0x7fffffffLL) { printf("budget too large (u32)\n"); return 1; }
    hWpr = (hC + 31) / 32; hNW = hWpr * hR;

    unsigned *g0bits = (unsigned *)calloc(hNW, 4);
    unsigned char *g0 = (unsigned char *)calloc((size_t)hR * hC, 1);   // 主机重放校验用
    hTotalFree = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (bp[(size_t)y * w + x] == '.') {
                int r = y + 1, c = x + 1;
                g0bits[r * hWpr + (c >> 5)] |= 1u << (c & 31);
                g0[r * hC + c] = 1;
                ++hTotalFree;
            }
    if (hTotalFree == 0) { printf("x=0&y=0&path=\n"); return 0; }

    unsigned short *starts = (unsigned short *)malloc(sizeof(unsigned short) * hTotalFree);
    int ns = 0;
    for (int r = 1; r < hR - 1 && ns < maxtrees; ++r)
        for (int c = 1; c < hC - 1 && ns < maxtrees; ++c)
            if (g0[r * hC + c]) starts[ns++] = (unsigned short)((r << 8) | c);

    cudaDeviceProp prop; CK(cudaGetDeviceProperties(&prop, 0));
    size_t slotbytes = ((size_t)hNW * 8 + QCAP * 2 + 64 + 127) & ~(size_t)127;
    int wpb = (int)((size_t)prop.sharedMemPerMultiprocessor / 2 / slotbytes);   // 目标 2 块/SM
    if (wpb > 8) wpb = 8;
    if (wpb < 1) wpb = 1;
    while (wpb > 1 && slotbytes * wpb > (size_t)prop.sharedMemPerBlockOptin) --wpb;
    if (getenv("WPB")) wpb = atoi(getenv("WPB"));
    size_t shbytes = slotbytes * wpb;
    int nwarps = prop.multiProcessorCount * 16;
    if (nwarps > ns) nwarps = ns;
    int nblocks = (nwarps + wpb - 1) / wpb; nwarps = nblocks * wpb;
    int maxdepth = hTotalFree + 2, framestride = maxdepth * 3;

    printf("board %dx%d free=%d starts=%d budget=%lld | Wpr=%d NW=%d slot=%zuB block=%zuB warps=%d\n",
           w, h, hTotalFree, ns, budget, hWpr, hNW, slotbytes, shbytes, nwarps);
    if (shbytes > (size_t)prop.sharedMemPerBlockOptin) {
        printf("shared per block %zu > optin %zu — board too large\n", shbytes, (size_t)prop.sharedMemPerBlockOptin);
        return 1;
    }
    CK(cudaFuncSetAttribute(k_sweep2, cudaFuncAttributeMaxDynamicSharedMemorySize, (int)shbytes));

    unsigned bud32 = (unsigned)budget;
    int stop = nostop ? 0 : 1;
    CK(cudaMemcpyToSymbol(cR, &hR, 4)); CK(cudaMemcpyToSymbol(cC, &hC, 4));
    CK(cudaMemcpyToSymbol(cWpr, &hWpr, 4)); CK(cudaMemcpyToSymbol(cNW, &hNW, 4));
    CK(cudaMemcpyToSymbol(cTotalFree, &hTotalFree, 4));
    CK(cudaMemcpyToSymbol(cBudget, &bud32, 4));
    CK(cudaMemcpyToSymbol(cStop, &stop, 4));

    unsigned *d_g0; unsigned short *d_starts; TreeOut *d_out; unsigned *d_frames;
    int *d_next, *d_found, *d_solmeta; unsigned char *d_soldirs;
    CK(cudaMalloc(&d_g0, (size_t)hNW * 4));
    CK(cudaMalloc(&d_starts, sizeof(unsigned short) * ns));
    CK(cudaMalloc(&d_out, sizeof(TreeOut) * ns));
    CK(cudaMalloc(&d_frames, (size_t)nwarps * framestride * 4));
    CK(cudaMalloc(&d_next, 4)); CK(cudaMalloc(&d_found, 4));
    CK(cudaMalloc(&d_solmeta, 12)); CK(cudaMalloc(&d_soldirs, (size_t)hTotalFree + 8));
    CK(cudaMemcpy(d_g0, g0bits, (size_t)hNW * 4, cudaMemcpyHostToDevice));
    CK(cudaMemcpy(d_starts, starts, sizeof(unsigned short) * ns, cudaMemcpyHostToDevice));
    CK(cudaMemset(d_next, 0, 4)); CK(cudaMemset(d_found, 0, 4));
    CK(cudaMemset(d_solmeta, 0xff, 12));
    {   TreeOut init; memset(&init, 0, sizeof init); init.result = -3;
        TreeOut *tmp = (TreeOut *)malloc(sizeof(TreeOut) * ns);
        for (int i = 0; i < ns; ++i) tmp[i] = init;
        CK(cudaMemcpy(d_out, tmp, sizeof(TreeOut) * ns, cudaMemcpyHostToDevice));
        free(tmp);
    }
    printf("gpu mem: frames %.1fMB, out %.1fMB\n",
           (double)nwarps * framestride * 4 / 1e6, (double)sizeof(TreeOut) * ns / 1e6);

    cudaEvent_t t0, t1; cudaEventCreate(&t0); cudaEventCreate(&t1);
    cudaEventRecord(t0);
    k_sweep2<<<nblocks, dim3(32, wpb), shbytes>>>(d_g0, d_starts, ns, d_out, d_frames,
                                                  framestride, d_next, d_found, d_solmeta, d_soldirs);
    CK(cudaGetLastError());
    cudaEventRecord(t1);
    CK(cudaEventSynchronize(t1));
    float ms; cudaEventElapsedTime(&ms, t0, t1);

    TreeOut *outv = (TreeOut *)malloc(sizeof(TreeOut) * ns);
    CK(cudaMemcpy(outv, d_out, sizeof(TreeOut) * ns, cudaMemcpyDeviceToHost));
    int solmeta[3]; CK(cudaMemcpy(solmeta, d_solmeta, 12, cudaMemcpyDeviceToHost));

    long long tot = 0, trc = 0, trcell = 0, tsw = 0;
    int nsolve = 0, nexh = 0, ncap = 0, nabort = 0, nnorun = 0;
    for (int i = 0; i < ns; ++i) {
        tot += outv[i].nodes; trc += outv[i].rcalls; trcell += outv[i].rcells; tsw += outv[i].sweeps;
        switch (outv[i].result) {
            case 1: ++nsolve; break;
            case 0: ++nexh; break;
            case -1: ++ncap; break;
            case -2: ++nabort; break;
            default: ++nnorun; break;
        }
    }
    printf("wall %.3fs  nodes %lld  aggregate %.1fM nodes/s\n", ms / 1000.0, tot, tot / (ms / 1000.0) / 1e6);
    printf("solved=%d exhausted=%d capped=%d aborted=%d notrun=%d\n", nsolve, nexh, ncap, nabort, nnorun);
    printf("reach: calls %lld, cells %lld (%.1f cells/call), sweep-fallbacks %lld\n",
           trc, trcell, trc ? (double)trcell / trc : 0.0, tsw);
    {   long long kr = 0, kt = 0;
        for (int i = 0; i < ns; ++i) { kr += outv[i].kcyc_reach; kt += outv[i].kcyc_tree; }
        int nblk = 0;
        cudaOccupancyMaxActiveBlocksPerMultiprocessor(&nblk, k_sweep2, 32 * wpb, shbytes);
        long long tnf = 0, tcf = 0;
        for (int i = 0; i < ns; ++i) { tnf += outv[i].nfail; tcf += outv[i].cfail; }
        printf("prof: reach %.1f%% of tree cycles | occupancy %d blocks/SM = %d warps/SM x %d SMs (shared/SM=%zu)\n",
               kt ? 100.0 * kr / kt : 0.0, nblk, nblk * wpb, prop.multiProcessorCount,
               (size_t)prop.sharedMemPerMultiprocessor);
        printf("prof: reach fails %lld/%lld (%.1f%%), fail-cells %lld (%.1f/fail, %.1f%% of cells)\n",
               tnf, trc, trc ? 100.0 * tnf / trc : 0.0, tcf,
               tnf ? (double)tcf / tnf : 0.0, trcell ? 100.0 * tcf / trcell : 0.0);
    }

    if (dump)
        for (int i = 0; i < ns; ++i)
            printf("%d %d %d %u\n", (starts[i] & 255) - 1, (starts[i] >> 8) - 1,
                   outv[i].result, outv[i].nodes);

    if (solmeta[2] >= 0 && solmeta[0] >= 0) {
        int plen = solmeta[2];
        unsigned char *dirs = (unsigned char *)malloc((size_t)plen + 1);
        CK(cudaMemcpy(dirs, d_soldirs, plen, cudaMemcpyDeviceToHost));
        int ssr = solmeta[1] >> 8, ssc = solmeta[1] & 255;
        // 主机重放校验：滑行语义 + 全覆盖
        unsigned char *vis = (unsigned char *)calloc((size_t)hR * hC, 1);
        int cr = ssr, cc = ssc, visited = 1, legal = 1;
        vis[cr * hC + cc] = 1;
        const int drh[4] = { 0, -1, 0, 1 }, dch[4] = { -1, 0, 1, 0 };
        for (int k = 0; k < plen && legal; ++k) {
            int d = dirs[k], len = 0;
            for (;;) {
                int nr = cr + drh[d], nc = cc + dch[d];
                if (!g0[nr * hC + nc] || vis[nr * hC + nc]) break;
                cr = nr; cc = nc; vis[cr * hC + cc] = 1; ++len; ++visited;
            }
            if (len == 0) legal = 0;
        }
        printf("replay: %s, visited %d/%d\n",
               (legal && visited == hTotalFree) ? "PASS" : "FAIL", visited, hTotalFree);
        static const char DCH[5] = "LURD";
        char *pathstr = (char *)malloc((size_t)plen + 1);
        for (int k = 0; k < plen; ++k) pathstr[k] = DCH[dirs[k]];
        pathstr[plen] = 0;
        printf("x=%d&y=%d&path=%s\n", ssc - 1, ssr - 1, pathstr);
        if (solfile) {
            FILE *sf = fopen(solfile, "w");
            if (sf) { fprintf(sf, "x=%d&y=%d&path=%s\n", ssc - 1, ssr - 1, pathstr); fclose(sf); }
        }
    }
    return 0;
}
