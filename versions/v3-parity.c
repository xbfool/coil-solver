// Coil solver — v3: 在 v2 基础上加奇偶染色剪枝（O(1)，最狠的一条）
//
// 棋盘按国际象棋染色，相邻格必异色。路径从当前位置 p 出发，下一格必是 ~col(p)，再下一格是 col(p)…
// 严格交替。所以剩余 r 格里，两色数量被**精确锁死**：#~col(p) == ceil(r/2)，#col(p) == floor(r/2)。
// 差一个就注定走不完 —— 这条比连通性剪枝廉价得多（两次比较），却能在极浅处砍掉大片子树。
// 两色计数随滑动增量维护。
//
// v1 的时间曲线极不稳定（L131 46x46 只要 0.04s，L129 46x45 要 9s），说明成败取决于头几步
// 猜得准不准，而不是盘面本身难。确定性 DFS 一旦扎进坏子树，加大节点上限只是把同一片坏区域
// 再犁一遍。v2 改成：小预算搜一会儿 → 换随机种子重来，预算按 Luby 序列涨。
// Warnsdorff 仍是主排序，只在打平时随机，保证启发式不被随机性冲掉。
//
// 规则回顾：选一个空格出发，每次选方向后一直滑到撞墙/撞边界/撞已访问才停，要求每个空格恰好走一次。
// 所以路径本质是空格集合上的哈密顿路径，只是「拐弯点」被滑动规则限死了。
// 哈密顿路径的必要条件都能拿来剪枝（连通性、死端数），这是 v1 的主要武器。
//
// 输入（stdin 或文件参数）：x=<w>&y=<h>&board=<w*h 个 . 或 X>
// 输出：x=<起点x>&y=<起点y>&path=<UDLR 序列>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int W, H, N;              // 加了一圈哨兵墙后的宽高
static unsigned char *g;         // 1 = 空且未访问，0 = 墙或已访问
static unsigned char *g0;        // 原始棋盘副本，每次换起点时还原
static int delta[4];
static const char DCH[4] = { 'L', 'U', 'R', 'D' };
static char *path;
static int path_len;
static int total_free;
static unsigned char *col;       // 棋盘染色，相邻异色
static int cnt[2];               // 未访问格子的两色计数，随滑动增量维护
static int *fstack;              // 洪水填充用的显式栈
static int32_t *seen;            // 时间戳法，免去每次清零
static int32_t seen_id;
static long long nodes, node_limit;

static uint64_t rng_s = 0x243F6A8885A308D3ULL;
static inline uint64_t rnd(void) {
    rng_s ^= rng_s << 13; rng_s ^= rng_s >> 7; rng_s ^= rng_s << 17;
    return rng_s;
}

// Luby 重启序列 1,1,2,1,1,2,4,1,1,2,1,1,2,4,8,... —— 不需要预知「多久算久」就有好的期望表现
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

// 从当前位置 p 出发做一次洪水填充，同时统计死端数量。
// 剪枝依据：1) 剩余空格必须全部还能从 p 连通到达；2) 只允许存在 1 个死端（路径终点）。
static int prune_ok(int p, int remaining) {
    int top = 0, count = 0, deadends = 0;
    ++seen_id;
    for (int d = 0; d < 4; ++d) {
        int c = p + delta[d];
        if (g[c] && seen[c] != seen_id) { seen[c] = seen_id; fstack[top++] = c; }
    }
    while (top) {
        int c = fstack[--top];
        ++count;
        int deg = 0;
        for (int d = 0; d < 4; ++d) {
            int nb = c + delta[d];
            if (g[nb]) {
                ++deg;
                if (seen[nb] != seen_id) { seen[nb] = seen_id; fstack[top++] = nb; }
            } else if (nb == p) {
                ++deg;                        // 还能从当前位置进来
            }
        }
        if (deg <= 1 && ++deadends > 1) return 0;
    }
    return count == remaining;
}

// 返回 1 = 解出，0 = 此分支无解，-1 = 触到节点上限，放弃
static int dfs(int p, int remaining) {
    if (remaining == 0) return 1;
    if (nodes++ >= node_limit) return -1;

    struct move { int dir, endp, len, score; } cand[4];
    int nc = 0;

    for (int d = 0; d < 4; ++d) {
        int dd = delta[d];
        if (!g[p + dd]) continue;
        int c = p, len = 0;
        while (g[c + dd]) { c += dd; g[c] = 0; --cnt[col[c]]; ++len; }   // 滑到底
        int rem2 = remaining - len;
        // 奇偶：从 c 继续走，下一格必为 ~col(c)，故该色须恰好占 ceil(rem2/2)
        if (rem2 == 0 || (cnt[col[c] ^ 1] == (rem2 + 1) / 2 && prune_ok(c, rem2))) {
            cand[nc].dir = d; cand[nc].endp = c; cand[nc].len = len;
            // Warnsdorff 主序（×4 拉开档位），低位塞随机数只在同分时打乱顺序
            cand[nc].score = freedeg(c) * 4 + (int)(rnd() & 3);
            ++nc;
        }
        for (int i = 0, back = c; i < len; ++i, back -= dd) { g[back] = 1; ++cnt[col[back]]; }
    }

    for (int i = 1; i < nc; ++i) {                        // 插入排序，n<=4
        for (int j = i; j > 0 && cand[j].score < cand[j - 1].score; --j) {
            struct move t = cand[j];
            cand[j] = cand[j - 1]; cand[j - 1] = t;
        }
    }

    for (int i = 0; i < nc; ++i) {
        int dd = delta[cand[i].dir], len = cand[i].len, c = cand[i].endp;
        for (int k = 0, q = p; k < len; ++k) { q += dd; g[q] = 0; --cnt[col[q]]; }
        path[path_len++] = DCH[cand[i].dir];
        int r = dfs(c, remaining - len);
        if (r) return r;                                  // 1 解出 / -1 放弃，都直接上抛
        --path_len;
        for (int k = 0, back = c; k < len; ++k, back -= dd) { g[back] = 1; ++cnt[col[back]]; }
    }
    return 0;
}

int main(int argc, char **argv) {
    // ---- 读入 ----
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

    int w = 0, h = 0;
    char *px = strstr(buf, "x="), *py = strstr(buf, "y="), *pb = strstr(buf, "board=");
    if (!px || !py || !pb) { fprintf(stderr, "bad input\n"); return 1; }
    w = atoi(px + 2); h = atoi(py + 2);
    pb += 6;
    if (w <= 0 || h <= 0) { fprintf(stderr, "bad dims\n"); return 1; }

    // ---- 建带哨兵边框的棋盘 ----
    W = w + 2; H = h + 2; N = W * H;
    g = calloc(N, 1); g0 = calloc(N, 1); col = calloc(N, 1);
    delta[0] = -1; delta[1] = -W; delta[2] = 1; delta[3] = W;
    for (int c = 0; c < N; ++c) col[c] = (unsigned char)(((c % W) + (c / W)) & 1);
    total_free = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            char ch = pb[(size_t)y * w + x];
            if (ch == '.') { g0[(y + 1) * W + (x + 1)] = 1; ++total_free; }
        }
    if (total_free == 0) { printf("x=0&y=0&path=\n"); return 0; }

    path = malloc(total_free + 8);
    fstack = malloc(sizeof(int) * (size_t)N);
    seen = calloc(N, sizeof(int32_t));

    // ---- 起点候选：度数小的优先（度 1 的格子必然是路径端点）----
    int *starts = malloc(sizeof(int) * (size_t)total_free);
    int ns = 0;
    memcpy(g, g0, N);
    for (int c = 0; c < N; ++c) if (g0[c]) starts[ns++] = c;
    for (int i = 1; i < ns; ++i) {                        // 按度数插入排序
        int v = starts[i], dv = freedeg(v), j = i;
        while (j > 0 && freedeg(starts[j - 1]) > dv) { starts[j] = starts[j - 1]; --j; }
        starts[j] = v;
    }

    // ---- 随机重启：轮流换起点 + 换随机种子，单次预算按 Luby 涨 ----
    const long long UNIT = 40000;                         // Luby 一个单位 = 多少节点
    for (long long it = 1; it < (1LL << 40); ++it) {
        int s = starts[(it - 1) % ns];
        rng_s = 0x9E3779B97F4A7C15ULL * (uint64_t)it + 0x243F6A8885A308D3ULL;
        memcpy(g, g0, N);
        cnt[0] = cnt[1] = 0;
        for (int c = 0; c < N; ++c) if (g0[c]) ++cnt[col[c]];
        g[s] = 0; --cnt[col[s]];
        path_len = 0; nodes = 0; node_limit = UNIT * luby(it);
        if (dfs(s, total_free - 1) == 1) {
            path[path_len] = 0;
            printf("x=%d&y=%d&path=%s\n", (s % W) - 1, (s / W) - 1, path);
            return 0;
        }
    }
    fprintf(stderr, "no solution found\n");
    return 1;
}
