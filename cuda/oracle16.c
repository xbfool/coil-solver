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
        while (g[c + dd]) { c += dd; mark(c); ++len; }
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

    int *starts = malloc(sizeof(int) * (size_t)total_free);
    int ns = 0;
    memcpy(g, g0, N);
    for (int c = 0; c < N; ++c) if (g0[c]) starts[ns++] = c;
    for (int i = 1; i < ns; ++i) {
        int v = starts[i], dv = freedeg(v), j = i;
        while (j > 0 && freedeg(starts[j - 1]) > dv) { starts[j] = starts[j - 1]; --j; }
        starts[j] = v;
    }


    // ==== ORACLE 尾巴（替换 v16 的分片+两段式搜索）====
    // 用途：给 GPU v2 内核当差分测试基准。逐起点跑固定预算 dfs，
    // 打印「x y result nodes」。result: 1=解出 0=搜穷 -1=预算尽。
    // 单进程单线程；起点顺序 = freedeg 升序（上面排过，打印后外部按坐标排序对比）。
    long long probe = argc > 2 ? atoll(argv[2]) : 2000;
    long long tot_nodes = 0; int nsolve = 0, nexh = 0, ncap = 0;
    for (int i = 0; i < ns; ++i) {
        int s = starts[i];
        memcpy(g, g0, N);
        cnt[0] = cnt[1] = 0; low_cnt = zero_cnt = 0;
        for (int c = 0; c < N; ++c) if (g[c]) ++cnt[col[c]];
        for (int c = 0; c < N; ++c) if (g[c]) { deg[c] = freedeg(c);
            if (deg[c] <= 1) ++low_cnt; if (deg[c] == 0) ++zero_cnt; }
        mark(s);
        path_len = 0; nodes = 0; node_limit = probe; best_rem = total_free;
        int r;
        if (!reach_ok(s, total_free - 1)) r = 0;
        else r = dfs(s, total_free - 1);
        printf("%d %d %d %lld\n", (s % W) - 1, (s / W) - 1, r, nodes);
        tot_nodes += nodes;
        if (r == 1) ++nsolve; else if (r == 0) ++nexh; else ++ncap;
    }
    fprintf(stderr, "oracle: starts=%d solved=%d exhausted=%d capped=%d nodes=%lld\n",
            ns, nsolve, nexh, ncap, tot_nodes);
    return 0;
}
