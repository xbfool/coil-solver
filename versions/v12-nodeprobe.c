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

static int in_probe = 0;                 // 探针内部不再套探针
static int probe_depth = 14;             // 只在前 N 个决策点做（早期错误的子树最大最贵）
static long long probe_nodes = 30000;    // 每个候选给多少节点（PDEPTH/PNODES 可调，用来调参）

static int dfs(int p, int remaining, int depth) {
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
        if (rem2 == 0 || (cheap_ok(c, rem2) && reach_ok(c, rem2))) {
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

    // 浅层分支点：给每个候选跑一次限预算探针。**被搜穷仍无解的候选就此被证伪**，
    // 二选一直接塌成强制走 —— 早期一步走错的代价是整棵子树，这里花小钱买大保险。
    if (!in_probe && nc > 1 && depth < probe_depth) {
        int keep = 0;
        for (int i = 0; i < nc; ++i) {
            int dd = delta[cand[i].dir], len = cand[i].len, c = cand[i].endp;
            for (int k = 0, q = p; k < len; ++k) { q += dd; mark(q); }
            path[path_len++] = DCH[cand[i].dir];
            long long sn = nodes, sl = node_limit;
            nodes = 0; node_limit = probe_nodes; in_probe = 1;
            int r = dfs(c, remaining - len, depth + 1);
            in_probe = 0; nodes = sn; node_limit = sl;
            if (r == 1) return 1;                       // 探针顺手把整题解了
            --path_len;
            for (int k = 0, back = c; k < len; ++k, back -= dd) unmark(back);
            if (r != 0) cand[keep++] = cand[i];         // r=-1 预算用尽=未被证伪，留下
        }
        nc = keep;
        if (nc == 0) return 0;                          // 全被证伪，这个状态死了
    }

    for (int i = 0; i < nc; ++i) {
        int dd = delta[cand[i].dir], len = cand[i].len, c = cand[i].endp;
        for (int k = 0, q = p; k < len; ++k) { q += dd; mark(q); }
        path[path_len++] = DCH[cand[i].dir];
        int r = dfs(c, remaining - len, depth + 1);
        if (r == 1) return 1;
        // 注意：-1（预算用尽）也必须先撤销再上抛。v5 里可以不撤，因为每换起点都会 memcpy 重置盘面；
        // 但 v12 把 -1 当探针结果用在搜索**内部**，返回后还要接着搜，不撤销盘面就脏了。
        --path_len;
        for (int k = 0, back = c; k < len; ++k, back -= dd) unmark(back);
        if (r == -1) return -1;
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
                for (int j = 0; j <= k; ++j) close(pfd[j * 2]);
                shard = k; out_fd = pfd[k * 2 + 1];
                goto child_work;
            }
            kids[k] = pid;
            close(pfd[k * 2 + 1]);
        }
        // 父进程：等第一个成功退出的孩子
        for (int done = 0; done < nshard; ++done) {
            int st = 0;
            pid_t p2 = wait(&st);
            if (p2 <= 0) break;
            int k = -1;
            for (int j = 0; j < nshard; ++j) if (kids[j] == p2) k = j;
            if (k >= 0 && WIFEXITED(st) && WEXITSTATUS(st) == 0) {
                char buf[1 << 22];
                ssize_t n2 = read(pfd[k * 2], buf, sizeof buf - 1);
                if (n2 > 0) {
                    buf[n2] = 0;
                    fputs(buf, stdout);
                    for (int j = 0; j < nshard; ++j) if (kids[j] != p2) kill(kids[j], 9);
                    return 0;
                }
            }
        }
        fprintf(stderr, "no solution found\n");
        return 1;
    }
  child_work:;

    // ---- 第一段：探针，证伪 + 打分 ----
    long long probe = getenv("PROBE") ? atoll(getenv("PROBE")) : 2000;
    if (getenv("PDEPTH")) probe_depth = atoi(getenv("PDEPTH"));
    if (getenv("PNODES")) probe_nodes = atoll(getenv("PNODES"));
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
        mark(s);
        path_len = 0; nodes = 0; node_limit = probe; best_rem = total_free;
        int r = dfs(s, total_free - 1, 0);
        if (r == 1) { path[path_len] = 0;
            printf("x=%d&y=%d&path=%s\n", (s % W) - 1, (s / W) - 1, path); return 0; }
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
        mark(s);

        path_len = 0; nodes = 0;
        if (dfs(s, total_free - 1, 0) == 1) {
            path[path_len] = 0;
            printf("x=%d&y=%d&path=%s\n", (s % W) - 1, (s / W) - 1, path);
            return 0;
        }
    }
    fprintf(stderr, "no solution found\n");
    return 1;
}
