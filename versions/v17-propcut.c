// Coil solver — v17: v16 + 约束传播（每个起点开搜前跑一次，定死的边拿来剪枝）
//
// 以下是 v16 的说明。
// Coil solver — v16: 把每节点的全盘洪水填充换成等价的「局部」连通性检查
//
// v13 的每个搜索节点，对每个候选方向都做一次 reach_ok —— 从落点洪水填充整个剩余区域，
// 数够不够 remaining 个。这是最强的一条剪枝，但也是 O(剩余格数)：L195 开局就是三千格，
// 一个节点最多四个候选，光这一项就是每节点上万次访存。
//
// 但它可以严格等价地做成局部的。进入某节点时剩余区域 R 已知连通（上一层验过），
// 本次滑行吃掉的只是一条直线 S，新剩余 R' = R \ S。R' 要被切开，切口必然紧贴 S；
// 又因为 R 连通，R' 里任何一点都能在 R' 内走到某个「S 的自由邻居」。于是
//
//     R' 连通  ⟺  S 的自由邻居两两连通
//
// 这些邻居一旦连上就能立刻收工，不用把剩余区域数完。滑行终点 c 也在 S 里，
// 它的自由邻居同属这个集合，所以「从 c 出发能到全部剩余格」一并被保证。
//
// 归纳的底座要单独打：起点处剩余区域是「全盘去掉起点」，起点若是割点就不连通了，
// 所以每个起点开搜前照旧做一次完整的 reach_ok（顺带白捡一条起点剪枝）。
//
// 以下是 v13 的说明。
// Coil solver — v13: 修掉 v11 的并行失效 bug（emit() 写了却从未被调用）
//
// v11 的意图是「谁先解出就把答案写进管道退出，父进程收第一个成功的并杀掉其余」，为此写了
// emit()——但两处解出的地方走的都是 printf + return 0，emit 一次都没被调用（编译警告
// 'emit' defined but not used 就是证据）。后果：
//   · 子进程把答案直接打进继承来的 stdout，所以校验能过，成绩看着是对的；
//   · 父进程从管道只读到 0 字节，于是**不 kill 其余子进程**；
//   · evaluate.py 用 capture_output 抓 stdout，必须等所有持有该 fd 的进程退出才返回。
// 所以 v11 的实测耗时 = 最慢的那个 shard 把自己那片起点全部搜穷，而不是「最快找到解」。
// 28 核并行的收益基本被这一个 bug 吃掉了。
//
// 本版只改这一件事，好单独量出它值多少分：
//   (a) 两处解出都改走 emit()（写管道 + _exit(0)）；
//   (b) 子进程 fork 后关掉所有不属于自己的管道端；
//   (c) 父进程从 wait() 改成 poll()，第一个写出答案的孩子赢，立刻 kill 其余。
//
// 以下是 v11 的原始说明。
// Coil solver — v11: v10 + 起点分片并行（fork），32 核直接打在「起点彩票」这个瓶颈上
//
// 实测打脸：真实关卡的解极少（常常唯一），穷举整棵树也不大——L139 全枚举才 1680 万节点，
// 几秒跑完；而 v2~v4 的 Luby 重启到 4 万节点就换起点，等于把搜过的反复扔掉，同一关 30 秒超时。
// 所以这版删掉重启和随机化，老老实实系统搜索；连通性检查恢复成每节点都做（最强的一条，代价撑得住）。
//
// v3 的瓶颈：每个候选都做一次 O(剩余格数) 的洪水填充。盘子越大越贵，而大盘恰恰需要探更多节点。
// v4 把两条剪枝都降成 O(1)：
//   · 奇偶染色（v3 那条）——两色计数增量维护
//   · 死端/孤格——给每个未访问格维护「未访问邻居数」deg，标记访问时只更新 4 个邻居，
//     同时维护 deg<=1 的格数 low_cnt 和 deg==0 的格数 zero_cnt。当前位置 p 的相邻格能从 p 进来，
//     实际度要 +1，检查时扫 p 的 4 个邻居修正即可。
// 精确连通性（洪水填充）仍然保留，但只在剩余格数 <= FF_MAX 时做：残局时它便宜且最能砍，
// 开局时它最贵而 O(1) 那两条已经够用。
//
// 输入（stdin 或文件参数）：x=<w>&y=<h>&board=<w*h 个 . 或 X>
// 输出：x=<起点x>&y=<起点y>&path=<UDLR 序列>

// 瓶颈是「解唯一 => 起点唯一，但要试 n 个起点」。这件事天然并行：起点之间完全独立。
// 用 fork 而不是线程，是因为所有工作数组都是全局的——fork 之后每个子进程自带一份副本，
// 零重构。子进程 i 只处理下标 ≡ i (mod P) 的起点（探针和正式搜索都只做自己那份），
// 谁先解出就把答案写进自己的管道并退出 0，父进程收到第一个成功的就打印并杀掉其余。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>

static int shard = 0, nshard = 1;      // 本进程负责 starts 里下标 % nshard == shard 的那些
static int out_fd = -1;                // 子进程把答案写这里
static void emit(int s, const char *pathstr);

#ifndef FF_MAX
#define FF_MAX 1200                  // 剩余格数 <= 此值才做精确连通性检查
#endif

static int W, H, N;
static unsigned char *g;             // 1 = 空且未访问，0 = 墙或已访问
static unsigned char *g0;
static unsigned char *col;           // 棋盘染色
static int *deg;                     // 未访问格的未访问邻居数
static int cnt[2];                   // 未访问格的两色计数
static int low_cnt, zero_cnt;        // deg<=1 / deg==0 的未访问格数
static int delta[4];
static const char DCH[4] = { 'L', 'U', 'R', 'D' };
static char *path;
static int path_len;
static int total_free;
static int *fstack;
static int32_t *seen;
static int32_t *srcmark;   // 标记「S 的自由邻居」，配 src_id 使用
static int32_t src_id;
static int *srcbuf;
static int32_t seen_id;
static long long nodes, node_limit;
static int best_rem;

// 找到解就写进管道（子进程）或直接打印（单进程模式）
static void emit(int s, const char *pathstr) {
    static char obuf[1 << 22];
    int n = snprintf(obuf, sizeof obuf, "x=%d&y=%d&path=%s\n", (s % W) - 1, (s / W) - 1, pathstr);
    if (out_fd >= 0) { ssize_t wr = write(out_fd, obuf, (size_t)n); (void)wr; _exit(0); }
    fputs(obuf, stdout);
    exit(0);
}

static uint64_t rng_s = 0x243F6A8885A308D3ULL;
static inline uint64_t rnd(void) {
    rng_s ^= rng_s << 13; rng_s ^= rng_s >> 7; rng_s ^= rng_s << 17;
    return rng_s;
}

static long long luby(long long i) {
    for (long long k = 1; k < 62; ++k) {
        if (i == (1LL << k) - 1) return 1LL << (k - 1);
        if (i >= (1LL << (k - 1)) && i < (1LL << k) - 1)
            return luby(i - (1LL << (k - 1)) + 1);
    }
    return 1;
}

static inline int freedeg(int c) {
    return g[c + delta[0]] + g[c + delta[1]] + g[c + delta[2]] + g[c + delta[3]];
}

static inline void mark(int c) {                 // 未访问 -> 已访问
    if (deg[c] <= 1) --low_cnt;
    if (deg[c] == 0) --zero_cnt;
    g[c] = 0; --cnt[col[c]];
    for (int d = 0; d < 4; ++d) {
        int n = c + delta[d];
        if (g[n]) {
            int o = deg[n]--;
            if (o == 2) ++low_cnt;
            if (o == 1) ++zero_cnt;
        }
    }
}

static inline void unmark(int c) {               // 已访问 -> 未访问（严格 LIFO 回滚）
    g[c] = 1; ++cnt[col[c]];
    for (int d = 0; d < 4; ++d) {
        int n = c + delta[d];
        if (n != c && g[n]) {
            int o = deg[n]++;
            if (o == 1) --low_cnt;
            if (o == 0) --zero_cnt;
        }
    }
    deg[c] = freedeg(c);
    if (deg[c] <= 1) ++low_cnt;
    if (deg[c] == 0) ++zero_cnt;
}

// O(1) 剪枝：奇偶 + 死端/孤格
static inline int cheap_ok(int p, int remaining) {
    if (cnt[col[p] ^ 1] != (remaining + 1) / 2) return 0;      // 奇偶被锁死
    int adj_low = 0, adj_zero = 0;
    for (int d = 0; d < 4; ++d) {
        int n = p + delta[d];
        if (g[n]) { if (deg[n] <= 1) ++adj_low; if (deg[n] == 0) ++adj_zero; }
    }
    if (zero_cnt - adj_zero > 0) return 0;                     // 有进不去的孤格
    return low_cnt - adj_low + adj_zero <= 1;                  // 死端最多 1 个（路径终点）
}

// 精确连通性：剩余格必须都能从 p 走到
static int reach_ok(int p, int remaining) {
    int top = 0, count = 0;
    ++seen_id;
    for (int d = 0; d < 4; ++d) {
        int c = p + delta[d];
        if (g[c] && seen[c] != seen_id) { seen[c] = seen_id; fstack[top++] = c; }
    }
    while (top) {
        int c = fstack[--top];
        ++count;
        for (int d = 0; d < 4; ++d) {
            int n = c + delta[d];
            if (g[n] && seen[n] != seen_id) { seen[n] = seen_id; fstack[top++] = n; }
        }
    }
    return count == remaining;
}

// ===== 约束传播层（每个起点开搜前跑一次，结果整局有效）=====
//
// v9 当年空转的真正原因不是「环模型 vs 路径模型」，而是**传播没有起爆点**：
// 度 2 规则要「这一格恰好用 2 条边」才能锁死两条边，可路径有两个端点只用 1 条边，
// 端点是谁又不知道，于是每格都得留「我可能是端点」的活口，谁也锁不死谁。
//
// 种子：路径在两色间严格交替，**起点一旦假定，终点的颜色就被完全确定**
//   （自由格数 n 为偶 => 终点异色于起点；n 为奇 => 同色）。
// 于是一半的格子当场被证明「不可能是端点」，它们里的度 2 格必须用满两条边，
// 传播起爆，再沿强制边连锁。我们本来就在逐个枚举起点，这个假定是免费的。
//
// 传播出来的东西整局有效——它说的是**最终解**的性质，跟搜索走到哪一步无关。
// 已在 11 个已知真解上验过 soundness：真解用到的边一条也没被禁用。
static unsigned char *estate;      // estate[c*4+d]: 0 未定 1 必用 2 禁用，与反向边同步
static int *dsu;
static int *pq;
static unsigned char *inq;
static int qhead, qtail;
static int prop_bad, prop_start, prop_endcol, prop_forced_end;

static int dfind(int x) { while (dsu[x] != x) { dsu[x] = dsu[dsu[x]]; x = dsu[x]; } return x; }

static void ppush(int c) {
    if (inq[c]) return;
    inq[c] = 1; pq[qtail++] = c;
    if (qtail == N + 1) qtail = 0;
}

static void set_edge(int c, int d, int v) {
    int n = c + delta[d];
    unsigned char *a = &estate[c * 4 + d], *b = &estate[n * 4 + (d ^ 2)];
    if (*a == v) return;
    if (*a != 0) { prop_bad = 1; return; }
    if (v == 1) {
        int ra = dfind(c), rb = dfind(n);
        if (ra == rb) { prop_bad = 1; return; }   // 闭成环，而 Coil 的解是路径
        dsu[ra] = rb;
    }
    *a = *b = (unsigned char)v;
    ppush(c); ppush(n);
}

static void pprocess(int c) {
    if (prop_bad) return;

    for (int d = 0; d < 4; ++d) {                  // 连上去就成环的待定边先禁掉
        int n = c + delta[d];
        if (!g0[n] || estate[c * 4 + d] != 0) continue;
        if (dfind(c) == dfind(n)) { set_edge(c, d, 2); if (prop_bad) return; }
    }

    int used = 0, avail = 0;
    for (int d = 0; d < 4; ++d) {
        if (!g0[c + delta[d]]) continue;
        if (estate[c * 4 + d] == 1) ++used;
        else if (estate[c * 4 + d] == 0) ++avail;
    }

    int lo, hi;
    if (c == prop_start)             lo = hi = 1;        // 起点只挂 1 条边
    else if (prop_forced_end == c)   lo = hi = 1;        // 已认定的终点
    else if (prop_forced_end >= 0)   lo = hi = 2;        // 终点定在别处，这里必是中间格
    else if (col[c] == prop_endcol) { lo = 1; hi = 2; }  // 颜色对得上，还可能是终点
    else                             lo = hi = 2;        // 颜色不对，绝无可能是端点

    if (used > hi) { prop_bad = 1; return; }
    if (used + avail < lo) { prop_bad = 1; return; }

    if (used == hi && avail > 0) {
        for (int d = 0; d < 4; ++d)
            if (g0[c + delta[d]] && estate[c * 4 + d] == 0) { set_edge(c, d, 2); if (prop_bad) return; }
    } else if (used + avail == lo && avail > 0) {
        for (int d = 0; d < 4; ++d)
            if (g0[c + delta[d]] && estate[c * 4 + d] == 0) { set_edge(c, d, 1); if (prop_bad) return; }
    }

    if (c != prop_start && used + avail == 1 && hi == 2) {   // 最多挂 1 条边 => 只能是终点
        if (col[c] != prop_endcol) { prop_bad = 1; return; }
        if (prop_forced_end >= 0 && prop_forced_end != c) { prop_bad = 1; return; }
        if (prop_forced_end < 0) {
            prop_forced_end = c;
            for (int q = 0; q < N; ++q) if (g0[q] && col[q] == prop_endcol) ppush(q);
        }
    }
}

// 返回 0 表示传播出矛盾 => 这个起点被证伪（是证明，不是启发式）
static int propagate(int s) {
    memset(estate, 0, (size_t)N * 4);
    for (int c = 0; c < N; ++c) dsu[c] = c;
    memset(inq, 0, (size_t)N);
    qhead = qtail = 0;
    prop_bad = 0; prop_forced_end = -1; prop_start = s;
    prop_endcol = (total_free & 1) ? col[s] : (col[s] ^ 1);

    for (int c = 0; c < N; ++c) if (g0[c]) ppush(c);
    while (qhead != qtail && !prop_bad) {
        int c = pq[qhead++];
        if (qhead == N + 1) qhead = 0;
        inq[c] = 0;
        pprocess(c);
    }
    return !prop_bad;
}

// 用传播结果卡一次滑行。滑行从 p 出发沿 dd 走 len 格到 c，它对最终解的边意味着：
//   · run 上那 len 条边**全都要用** —— 撞上任何一条禁用边就不可能；
//   · run 的中间格是直穿的，两条边都沿 dd，所以它的两条**垂直边一条都不用**
//     —— 那里若有必用边，矛盾；
//   · 落点 c 处路径要么转弯要么收尾，反正不会再往 dd 走一格，
//     所以边 (c, c+dd) 不用 —— 它若是必用边，矛盾。
//     （c+dd 此刻要么是墙，要么是更早访问过的格子；后者也不可能与 c 在路径上相邻，
//       因为 c 的前驱已经是 c-dd 了。）
static inline int estate_ok(int p, int d, int dd, int len, int c) {
    int od = d ^ 2;
    for (int k = 0, q = p; k < len; ++k, q += dd)
        if (estate[q * 4 + d] == 2) return 0;                 // 禁用边挡在 run 里
    for (int k = 1, q = p + dd; k < len; ++k, q += dd) {      // 中间格：垂直边必须不用
        int e1 = (d + 1) & 3, e2 = (d + 3) & 3;
        if (estate[q * 4 + e1] == 1 || estate[q * 4 + e2] == 1) return 0;
    }
    if (g0[c + dd] && estate[c * 4 + d] == 1) return 0;        // 落点不会再往前一格
    (void)od;
    return 1;
}


// 局部连通性：跟 reach_ok 严格等价，但通常只探一小片就能收工。
//
// 归纳前提：进入本节点时剩余区域 R 是连通的（每个节点都验过，起点处另做一次完整验证）。
// 本次滑行吃掉的是一条直线 S，新剩余 R' = R \ S。R' 若被切开，切口必然紧贴 S；
// 而 R 连通意味着 R' 里任一点 x 都能在 R' 内走到某个「S 的自由邻居」
// （x 到 S-邻居的 R-路径若碰到 S，进入 S 之前的最后一点就是一个 S-邻居）。
// 所以 **R' 连通 ⟺ S 的自由邻居两两连通**——它们一连上就能立刻返回，
// 不必像 reach_ok 那样把整个剩余区域数一遍。滑行终点 c 属于 S，它的自由邻居也在这个集合里，
// 所以「从 c 出发全部可达」同时被保证。
//
// 代价：成功时 = 把这些邻居连起来所需探索的范围（通常就在 S 附近）；
// reach_ok 则不管怎样都是 O(剩余格数)，开局就是三千格。
static int reach_local(int first, int dd, int len, int c, int rem2) {
    if (rem2 == 0) return 1;
    if (freedeg(c) == 0) return 0;                 // 滑到死胡同，后面没得走

    ++src_id;
    int nsrc = 0;
    for (int k = 0, q = first; k < len; ++k, q += dd)
        for (int d = 0; d < 4; ++d) {
            int n = q + delta[d];
            if (g[n] && srcmark[n] != src_id) { srcmark[n] = src_id; srcbuf[nsrc++] = n; }
        }
    if (nsrc == 0) return 0;                       // 还有格子没走，却没有一个挨着 S => 到不了
    if (nsrc == 1) return 1;                       // 只有一个邻居，两两连通平凡成立

    ++seen_id;
    int top = 0, found = 1;
    fstack[top++] = srcbuf[0]; seen[srcbuf[0]] = seen_id;
    while (top) {
        int q = fstack[--top];
        for (int d = 0; d < 4; ++d) {
            int n = q + delta[d];
            if (g[n] && seen[n] != seen_id) {
                seen[n] = seen_id;
                if (srcmark[n] == src_id && ++found == nsrc) return 1;
                fstack[top++] = n;
            }
        }
    }
    return 0;
}

static int dfs(int p, int remaining) {
    if (remaining < best_rem) best_rem = remaining;
    if (remaining == 0) return 1;
    if (nodes++ >= node_limit) return -1;

    struct move { int dir, endp, len, score; } cand[4];
    int nc = 0;

    for (int d = 0; d < 4; ++d) {
        int dd = delta[d];
        if (!g[p + dd]) continue;
        int c = p, len = 0;
        while (g[c + dd]) { c += dd; ++len; }              // 先只量出滑到哪，不动盘面
        if (!estate_ok(p, d, dd, len, c)) continue;        // 传播定死的边直接否掉这个方向
        for (int k = 0, q = p; k < len; ++k) { q += dd; mark(q); }
        int rem2 = remaining - len;
        if (rem2 == 0 || (cheap_ok(c, rem2) && reach_local(p + dd, dd, len, c, rem2))) {
            cand[nc].dir = d; cand[nc].endp = c; cand[nc].len = len;
            cand[nc].score = freedeg(c);
            ++nc;
        }
        for (int i = 0, back = c; i < len; ++i, back -= dd) unmark(back);
    }

    for (int i = 1; i < nc; ++i)
        for (int j = i; j > 0 && cand[j].score < cand[j - 1].score; --j) {
            struct move t = cand[j]; cand[j] = cand[j - 1]; cand[j - 1] = t;
        }

    for (int i = 0; i < nc; ++i) {
        int dd = delta[cand[i].dir], len = cand[i].len, c = cand[i].endp;
        for (int k = 0, q = p; k < len; ++k) { q += dd; mark(q); }
        path[path_len++] = DCH[cand[i].dir];
        int r = dfs(c, remaining - len);
        if (r) return r;
        --path_len;
        for (int k = 0, back = c; k < len; ++k, back -= dd) unmark(back);
    }
    return 0;
}

int main(int argc, char **argv) {
    FILE *in = stdin;
    if (argc > 1 && !(in = fopen(argv[1], "r"))) { fprintf(stderr, "open %s failed\n", argv[1]); return 1; }
    size_t cap = 1 << 20, n = 0;
    char *buf = malloc(cap);
    for (;;) {
        if (n + 65536 > cap) buf = realloc(buf, cap *= 2);
        size_t r = fread(buf + n, 1, 65536, in);
        if (!r) break;
        n += r;
    }
    buf[n] = 0;
    if (in != stdin) fclose(in);

    char *px = strstr(buf, "x="), *py = strstr(buf, "y="), *pb = strstr(buf, "board=");
    if (!px || !py || !pb) { fprintf(stderr, "bad input\n"); return 1; }
    int w = atoi(px + 2), h = atoi(py + 2);
    pb += 6;
    if (w <= 0 || h <= 0) { fprintf(stderr, "bad dims\n"); return 1; }

    W = w + 2; H = h + 2; N = W * H;
    g = calloc(N, 1); g0 = calloc(N, 1); col = calloc(N, 1);
    deg = malloc(sizeof(int) * (size_t)N);
    delta[0] = -1; delta[1] = -W; delta[2] = 1; delta[3] = W;
    for (int c = 0; c < N; ++c) col[c] = (unsigned char)(((c % W) + (c / W)) & 1);

    total_free = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (pb[(size_t)y * w + x] == '.') { g0[(y + 1) * W + (x + 1)] = 1; ++total_free; }
    if (total_free == 0) { printf("x=0&y=0&path=\n"); return 0; }

    path = malloc(total_free + 8);
    fstack = malloc(sizeof(int) * (size_t)N);
    seen = calloc(N, sizeof(int32_t));
    srcmark = calloc(N, sizeof(int32_t));
    srcbuf = malloc(sizeof(int) * (size_t)N);
    estate = malloc((size_t)N * 4);
    dsu = malloc(sizeof(int) * (size_t)N);
    pq = malloc(sizeof(int) * (size_t)(N + 1));
    inq = malloc((size_t)N);

    int *starts = malloc(sizeof(int) * (size_t)total_free);
    int ns = 0;
    memcpy(g, g0, N);
    for (int c = 0; c < N; ++c) if (g0[c]) starts[ns++] = c;
    for (int i = 1; i < ns; ++i) {
        int v = starts[i], dv = freedeg(v), j = i;
        while (j > 0 && freedeg(starts[j - 1]) > dv) { starts[j] = starts[j - 1]; --j; }
        starts[j] = v;
    }

    // ---- 分叉：起点分片，各干各的 ----
    nshard = getenv("JOBS") ? atoi(getenv("JOBS")) : (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (nshard < 1) nshard = 1;
    if (nshard > ns) nshard = ns;
    int *pfd = malloc(sizeof(int) * (size_t)nshard * 2);
    pid_t *kids = malloc(sizeof(pid_t) * (size_t)nshard);
    if (nshard > 1) {
        for (int k = 0; k < nshard; ++k) {
            if (pipe(&pfd[k * 2]) != 0) { nshard = 1; break; }
        }
    }
    if (nshard > 1) {
        for (int k = 0; k < nshard; ++k) {
            pid_t pid = fork();
            if (pid == 0) {                                 // 子进程
                // 管道在 fork 之前就全建好了，这个孩子继承了「所有」管道的两端。
                // 不属于自己的必须全关，否则别人的写端被我攥着，父进程永远收不到那条管道的 EOF。
                for (int j = 0; j < nshard; ++j) {
                    close(pfd[j * 2]);
                    if (j != k) close(pfd[j * 2 + 1]);
                }
                shard = k; out_fd = pfd[k * 2 + 1];
                goto child_work;
            }
            kids[k] = pid;
            close(pfd[k * 2 + 1]);
        }
        // 父进程：poll 所有读端。孩子有解就写管道；无解就自己退出，读端报 EOF。
        // 用 poll 而不是 wait，是因为大盘的解可能撑爆管道 64KB 缓冲——孩子会阻塞在 write 上
        // 等人来读，而 wait() 里的父进程要等它退出才读，那就互相等死了。
        struct pollfd *pf = malloc(sizeof(struct pollfd) * (size_t)nshard);
        for (int k = 0; k < nshard; ++k) { pf[k].fd = pfd[k * 2]; pf[k].events = POLLIN; }
        static char rbuf[1 << 22];
        int alive = nshard;
        while (alive > 0) {
            if (poll(pf, (nfds_t)nshard, -1) < 0) break;
            for (int k = 0; k < nshard; ++k) {
                if (pf[k].fd < 0 || pf[k].revents == 0) continue;
                size_t len = 0;
                ssize_t r;
                while ((r = read(pf[k].fd, rbuf + len, sizeof rbuf - 1 - len)) > 0) len += (size_t)r;
                if (len > 0) {                              // 这个 shard 赢了
                    rbuf[len] = 0;
                    fputs(rbuf, stdout);
                    fflush(stdout);
                    for (int j2 = 0; j2 < nshard; ++j2) if (kids[j2] > 0) kill(kids[j2], SIGKILL);
                    return 0;
                }
                close(pf[k].fd); pf[k].fd = -1; --alive;    // 这片搜穷了，无解
            }
        }
        for (int j2 = 0; j2 < nshard; ++j2) if (kids[j2] > 0) kill(kids[j2], SIGKILL);
        fprintf(stderr, "no solution found\n");
        return 1;
    }
  child_work:;

    // ---- 第一段：探针，证伪 + 打分 ----
    long long probe = getenv("PROBE") ? atoll(getenv("PROBE")) : 2000;
    int *sc = malloc(sizeof(int) * (size_t)ns);
    int keep = 0;
    for (int i = 0; i < ns; ++i) {
        if (i % nshard != shard) continue;                 // 只做自己这一片
        int s = starts[i];
        memcpy(g, g0, N);
        cnt[0] = cnt[1] = 0; low_cnt = zero_cnt = 0;
        for (int c = 0; c < N; ++c) if (g[c]) ++cnt[col[c]];
        for (int c = 0; c < N; ++c) if (g[c]) { deg[c] = freedeg(c);
            if (deg[c] <= 1) ++low_cnt; if (deg[c] == 0) ++zero_cnt; }
        if (!propagate(s)) continue;   // 传播直接矛盾 => 这个起点被证伪
        mark(s);
        // 归纳的底座：局部检查靠「上一层的剩余区域连通」，起点这一层得自己验。
        // 起点若是割点，去掉它剩余区域就断了。顺带白捡一条起点剪枝。
        if (!reach_ok(s, total_free - 1)) continue;
        path_len = 0; nodes = 0; node_limit = probe; best_rem = total_free;
        int r = dfs(s, total_free - 1);
        if (r == 1) { path[path_len] = 0; emit(s, path); }
        if (r == 0) continue;                          // 搜穷了仍无解 => 此起点被证伪，永久剔除
        starts[keep] = s; sc[keep] = best_rem; ++keep;
    }
    // 幸存者按探到的深度排序。对照实验：关掉排序反而更慢（L137 4.1s→23.5s，L171 26s→92s），
    // 说明排序是有信息量的；NORANK=1 可关掉做对照。
    if (!getenv("NORANK")) for (int i = 1; i < keep; ++i) {
        int vs = starts[i], vc = sc[i], j = i;
        while (j > 0 && sc[j - 1] > vc) { starts[j] = starts[j - 1]; sc[j] = sc[j - 1]; --j; }
        starts[j] = vs; sc[j] = vc;
    }
    fprintf(stderr, "probe: %d/%d 起点被证伪，剩 %d\n", ns - keep, ns, keep);

    // ---- 第二段：按顺序正式搜 ----
    node_limit = (long long)1 << 62;
    for (int i = 0; i < keep; ++i) {
        int s = starts[i];
        memcpy(g, g0, N);
        cnt[0] = cnt[1] = 0; low_cnt = zero_cnt = 0;
        for (int c = 0; c < N; ++c) if (g[c]) ++cnt[col[c]];
        for (int c = 0; c < N; ++c) if (g[c]) {
            deg[c] = freedeg(c);
            if (deg[c] <= 1) ++low_cnt;
            if (deg[c] == 0) ++zero_cnt;
        }
        if (!propagate(s)) continue;   // 传播直接矛盾 => 这个起点被证伪
        mark(s);
        // 归纳的底座：局部检查靠「上一层的剩余区域连通」，起点这一层得自己验。
        // 起点若是割点，去掉它剩余区域就断了。顺带白捡一条起点剪枝。
        if (!reach_ok(s, total_free - 1)) continue;

        path_len = 0; nodes = 0;
        if (dfs(s, total_free - 1) == 1) { path[path_len] = 0; emit(s, path); }
    }
    fprintf(stderr, "no solution found\n");
    return 1;
}
